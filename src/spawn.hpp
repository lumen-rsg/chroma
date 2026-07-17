#pragma once

/// @file spawn.hpp
/// @brief Process spawning utilities — fork+exec for companion programs
/// and exec for compositor replacement.

#include <string>
#include <vector>
#include <sys/types.h>

namespace chroma {

/// Fork and exec a program. The child process runs independently;
/// the compositor does NOT wait for it.
///
/// @param program  Path or name of the executable (resolved via $PATH).
/// @param args     Arguments (argv[0] is set to program if args is empty).
/// @param workdir  Working directory for the child (empty = inherit).
/// @return  PID of the spawned child, or -1 on failure.
pid_t spawn_process(const std::string& program,
                    const std::vector<std::string>& args = {},
                    const std::string& workdir = {});

/// Replace the compositor process with a new program.
/// This does NOT return on success. Only use during shutdown.
///
/// @param program  Path or name of the executable (resolved via $PATH).
/// @param args     Arguments (argv[0] is set to program if args is empty).
void exec_process(const std::string& program,
                  const std::vector<std::string>& args = {});

/// Send SIGTERM to all tracked spawned children.
/// Call during compositor shutdown for clean teardown.
void terminate_spawned_children();

/// Track a spawned PID so it can be terminated on shutdown.
/// (Called internally by spawn_process; also available for external use.)
void track_child_pid(pid_t pid);

/// Stop tracking a child PID (e.g. after it exits).
void untrack_child_pid(pid_t pid);

} // namespace chroma
