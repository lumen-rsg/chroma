#include "app.hpp"
#include "config.hpp"
#include <cstdio>
#include <cstring>

static void print_usage(const char* prog) {
    std::printf("Usage: %s [options]\n", prog);
    std::printf("Options:\n");
    std::printf("  -c, --config <path>   Path to config file (default: $XDG_CONFIG_HOME/chroma/config.toml)\n");
    std::printf("  --print-default-config Print the default config to stdout and exit\n");
    std::printf("  -h, --help            Show this help and exit\n");
}

int main(int argc, char* argv[]) {
    std::string config_path;

    // Parse CLI args
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                std::fprintf(stderr, "Error: --config requires a path argument\n");
                return 1;
            }
        } else if (std::strcmp(argv[i], "--print-default-config") == 0) {
            std::printf("%s", chroma::default_config_toml().c_str());
            return 0;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            // Positional argument — treat as config path
            config_path = argv[i];
        }
    }

    std::printf("╔══════════════════════════════════╗\n");
    std::printf("║       Chroma WM  v0.1.0          ║\n");
    std::printf("║  spatial canvas window manager   ║\n");
    std::printf("╚══════════════════════════════════╝\n\n");

    chroma::ChromaApp app(config_path);

    if (!app.init()) {
        std::fprintf(stderr, "FATAL: Failed to initialize Chroma\n");
        return 1;
    }

    app.run();

    return 0;
}
