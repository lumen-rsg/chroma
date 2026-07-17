/// @file spawn.cpp
/// @brief Process spawning implementation.

#include "spawn.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

namespace chroma {

// ============================================================================
// Child process tracking
// ============================================================================

static std::mutex child_mutex;
static std::set<pid_t> tracked_children;

// SIGCHLD handler — reap zombies so we don't accumulate defunct processes.
// Installed once by the first spawn.
static bool sigchld_installed = false;

static void sigchld_handler(int /*sig*/) {
    // Reap all zombies without blocking
    int saved_errno = errno;
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
    errno = saved_errno;
}

static void ensure_sigchld_handler() {
    if (sigchld_installed) return;
    struct sigaction sa{};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, nullptr) != 0) {
        std::fprintf(stderr, "[chroma] Warning: Failed to install SIGCHLD handler: %s\n",
                     std::strerror(errno));
    }
    sigchld_installed = true;
}

void track_child_pid(pid_t pid) {
    if (pid <= 0) return;
    std::lock_guard<std::mutex> lock(child_mutex);
    tracked_children.insert(pid);
}

void untrack_child_pid(pid_t pid) {
    if (pid <= 0) return;
    std::lock_guard<std::mutex> lock(child_mutex);
    tracked_children.erase(pid);
}

void terminate_spawned_children() {
    std::lock_guard<std::mutex> lock(child_mutex);
    for (pid_t pid : tracked_children) {
        // Check if process still exists
        if (kill(pid, 0) == 0) {
            kill(pid, SIGTERM);
        }
    }
    tracked_children.clear();
}

// ============================================================================
// spawn_process
// ============================================================================

pid_t spawn_process(const std::string& program,
                    const std::vector<std::string>& args,
                    const std::string& workdir) {
    ensure_sigchld_handler();

    pid_t pid = fork();

    if (pid < 0) {
        std::fprintf(stderr, "[chroma] spawn: fork failed: %s\n", std::strerror(errno));
        return -1;
    }

    if (pid == 0) {
        // --- Child process ---

        // Change working directory if specified
        if (!workdir.empty()) {
            if (chdir(workdir.c_str()) != 0) {
                std::fprintf(stderr, "[chroma] spawn: chdir(%s) failed: %s\n",
                             workdir.c_str(), std::strerror(errno));
            }
        }

        // Close all file descriptors except stdin/stdout/stderr.
        // This prevents the child from holding open Wayland sockets etc.
        // We use /proc/self/fd for efficiency — only close FDs we actually
        // have open rather than iterating over the full range.
        {
            // Fallback: close range [3, 1024) — fast enough for a compositor spawn
            for (int fd = 3; fd < 1024; fd++) {
                close(fd);
            }
        }

        // Build argv
        std::vector<const char*> argv;
        argv.push_back(program.c_str());
        for (const auto& a : args) {
            argv.push_back(a.c_str());
        }
        argv.push_back(nullptr);

        // Use execvp to search $PATH
        execvp(program.c_str(), const_cast<char* const*>(argv.data()));

        // If we get here, exec failed
        std::fprintf(stderr, "[chroma] spawn: execvp('%s') failed: %s\n",
                     program.c_str(), std::strerror(errno));
        _exit(1);
    }

    // --- Parent process ---
    track_child_pid(pid);
    std::printf("[chroma] Spawned '%s' (pid %d)\n", program.c_str(), pid);
    return pid;
}

// ============================================================================
// exec_process
// ============================================================================

void exec_process(const std::string& program,
                  const std::vector<std::string>& args) {
    // Build argv
    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);

    std::printf("[chroma] Replacing with '%s'...\n", program.c_str());

    // Use execvp to search $PATH
    execvp(program.c_str(), const_cast<char* const*>(argv.data()));

    // If we get here, exec failed
    std::fprintf(stderr, "[chroma] exec: execvp('%s') failed: %s\n",
                 program.c_str(), std::strerror(errno));
    _exit(1);
}

} // namespace chroma
