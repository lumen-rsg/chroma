# Vendored wlroots Headers

These are copies of wlroots 0.20 unstable headers with minimal patches to
make them compatible with C++20. They live in `src/compat/` which is placed
**before** the system include directories in the Meson build, so they shadow
the originals.

## Why vendoring is necessary

wlroots headers are written in C and use two constructs that are not valid
in standard C++:

### 1. C99 `static` array declarators

```c
// Valid C99, not valid C++:
void foo(const float matrix[static 9]);
```

GCC and Clang accept this as an extension in C++ mode, but the project
uses `-Wno-invalid-offsetof` which also suppresses the diagnostic. Rather
than relying on compiler extensions, the vendored headers simply remove the
`static` keyword — the array size information is still preserved.

**Affected files:**
- `wlr/render/color.h` — 3 occurrences
- `wlr/types/wlr_scene.h` — 2 occurrences

### 2. C++ keyword used as identifier

```c
// Valid C, hard error in C++:
struct wlr_layer_surface_v1 {
    char *namespace;  // 'namespace' is a C++ reserved word
};
```

This is a hard compilation error — no compiler flag can suppress it. The
vendored header renames the member to `name_space`.

**Affected file:**
- `wlr/types/wlr_layer_shell_v1.h` — 1 occurrence

### 3. Generated protocol header

`wlr-layer-shell-unstable-v1-protocol.h` is included by `wlr_layer_shell_v1.h`
with a quoted `#include`. Because the system installation may not ship this
generated file alongside the main header, a copy is vendored here to satisfy
the include.

## Upgrading wlroots

When upgrading to a new wlroots version:

1. Copy the updated system headers from `/usr/include/wlroots-0.XX/`
2. Re-apply the patches described above (search for `// PATCHED` comments)
3. Check `diff` against the originals to verify no other changes were made
4. Test compilation

Each patched location is marked with a `// PATCHED: ...` comment explaining
the change.
