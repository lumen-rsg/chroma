project('chroma', 'cpp',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'cpp_std=gnu++20',
    'warning_level=3',
    'default_library=static',
  ])

# wl_container_of on C++ classes can trigger -Winvalid-offsetof.
add_project_arguments(
  '-Wno-invalid-offsetof',
  language: 'cpp',
)

# ============================================================================
# Dependencies
# ============================================================================

wlroots_dep = dependency('wlroots-0.20')
wayland_server_dep = dependency('wayland-server')
xkbcommon_dep = dependency('xkbcommon')
pixman_dep = dependency('pixman-1')
math_dep = meson.get_compiler('cpp').find_library('m', required: false)

# ============================================================================
# Source files
# ============================================================================

chroma_sources = files(
  # Domain core
  'src/canvas.cpp',
  'src/magnetism.cpp',
  'src/stack.cpp',
  'src/input.cpp',

  # Adapters
  'src/server.cpp',
  'src/xdg_handler.cpp',
  'src/layer_shell.cpp',
  'src/renderer.cpp',
  'src/seat.cpp',

  # Application
  'src/app.cpp',
  'src/main.cpp',
)

# ============================================================================
# Headers (for dependency tracking in IDEs)
# ============================================================================

chroma_headers = files(
  'src/types.hpp',
  'src/window.hpp',
  'src/canvas.hpp',
  'src/focus.hpp',
  'src/magnetism.hpp',
  'src/stack.hpp',
  'src/input.hpp',
  'src/server.hpp',
  'src/xdg_handler.hpp',
  'src/renderer.hpp',
  'src/seat.hpp',
  'src/app.hpp',
)

# ============================================================================
# Executable
# ============================================================================

executable('chroma', chroma_sources,
  include_directories: [
    include_directories('src/compat'),    # patched wlroots headers (before system)
    include_directories('src'),
  ],
  dependencies: [
    wlroots_dep,
    wayland_server_dep,
    xkbcommon_dep,
    pixman_dep,
    math_dep,
  ],
  install: true,
)
