#include "app.hpp"
#include <cstdio>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::printf("╔══════════════════════════════════╗\n");
    std::printf("║       Chroma WM  v0.1.0          ║\n");
    std::printf("║  spatial canvas window manager    ║\n");
    std::printf("╚══════════════════════════════════╝\n\n");

    chroma::ChromaApp app;

    if (!app.init()) {
        std::fprintf(stderr, "FATAL: Failed to initialize Chroma\n");
        return 1;
    }

    app.run();

    return 0;
}
