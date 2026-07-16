# Chroma WM

**Chroma** is a Wayland compositor built on [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots/) 0.20. It implements a "spatial canvas" paradigm where windows live on an infinite 2D plane with pan/zoom navigation, magnetic window grouping, card stacking, and keyboard-driven interaction.

## Architecture

```
ChromaApp (orchestrator)
├── Domain (pure C++, no Wayland)
│   ├── Canvas         — infinite 2D space, window CRUD
│   ├── ChromaWindow   — entity: id, pos, size, group, stack
│   ├── FocusTracker   — MRU focus history
│   ├── InputRouter    — keybinding → Action translation
│   ├── StackManager   — card-stack deck operations
│   └── MagnetismEngine — snap-to-group, attract, grid-snap
└── Adapters (wlroots-aware)
    ├── WlrootsServer  — backend, renderer, scene, seat
    ├── XdgShellHandler— toplevel/popup creation & lifecycle
    ├── SceneRenderer  — Canvas state → wlr_scene updates
    └── SeatManager    — keyboard, pointer, cursor, drag/drop, touch
```

## Building

### Dependencies

- wlroots 0.20
- wayland-server
- xkbcommon
- pixman-1
- meson ≥ 1.0

On Fedora:
```sh
sudo dnf install wlroots-devel wayland-devel libxkbcommon-devel pixman-devel meson
```

### Build

```sh
meson setup build
ninja -C build
sudo ninja -C build install
```

### Run

From a TTY or nested session (e.g., `cage`):
```sh
chroma
```

## Keybindings

| Shortcut | Action |
|---|---|
| `Super+Arrows` / `Super+LMB-drag` | Pan canvas |
| `Super+=` / `Super+-` | Zoom in/out at cursor |
| `Super+Scroll` | Zoom with scroll wheel |
| `Super+Tab` / `Super+Shift+Tab` | Cycle focus forward/backward |
| `Alt+LMB` (on background) | Pan viewport |
| `LMB` (on window title) | Drag to move window |
| `LMB` (on window edge) | Drag to resize |
| `Super+S` | Cycle through window stack |
| `Super+Shift+S` | Add focused window to stack |
| `Super+G` | Group nearby windows magnetically |
| `Super+Shift+E` | Quit Chroma |

## Protocols

| Protocol | Status |
|---|---|
| `wlr_xdg_shell` | ✅ |
| `wlr_xdg_decoration_v1` | ✅ |
| `wlr_xdg_activation_v1` | ✅ |
| `wlr_data_device_manager` | ✅ |
| `wlr_subcompositor` | ✅ |
| `wlr_compositor` | ✅ |
| `wlr_layer_shell_v1` | ❌ Planned |
| `ext_foreign_toplevel_list_v1` | ❌ Planned |
| `wlr_screencopy_v1` | ❌ Planned |
| `ext_session_lock_v1` | ❌ Planned |
| XWayland | ❌ Planned |

## License

MIT — see [LICENSE](LICENSE).
