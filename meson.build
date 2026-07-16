project('chroma', 'cpp',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'cpp_std=gnu++20',
    'warning_level=3',
    'default_library=static',
  ])

# wl_container_of() uses offsetof() on C++ types to compute the containing
# struct pointer from a wl_listener member. This is valid C but triggers
# -Winvalid-offsetof in C++ because the standard says offsetof is only
# conditionally supported for non-standard-layout types. In practice, it
# works correctly on all compilers and platforms that wlroots supports
# (GCC, Clang; Linux; ELF ABI), and wlroots itself relies on this pattern
# pervasively. The flag is scoped project-wide because every translation
# unit that includes wlroots headers will instantiate the macro.
#
# No custom offsetof usage exists in chroma's own code — this suppression
# is exclusively for wlroots interop. If the project grows its own
# offsetof-like utilities, audit them separately.
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
  'src/foreign_toplevel.cpp',
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
  'src/animation.hpp',
  'src/server.hpp',
  'src/xdg_handler.hpp',
  'src/layer_shell.hpp',
  'src/foreign_toplevel.hpp',
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

# ============================================================================
# Tests — domain layer unit tests (no wlroots dependency)
# ============================================================================

test_sources = files(
  'tests/test_main.cpp',
  'tests/test_types.cpp',
  'tests/test_focus.cpp',
  'tests/test_stack.cpp',
  'tests/test_canvas.cpp',
)

test_domain_sources = files(
  'src/canvas.cpp',
  'src/magnetism.cpp',
  'src/stack.cpp',
  'src/input.cpp',
)

test_exe = executable('test_domain',
  test_sources + test_domain_sources,
  include_directories: [
    include_directories('src'),
    include_directories('tests'),
  ],
  dependencies: [
    math_dep,
  ],
)

test('domain', test_exe)
