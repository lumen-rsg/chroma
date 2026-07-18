// Chroma Shell — entry point
// Run with: quickshell -p /home/cv2/chroma/shell

import Quickshell
import QtQuick

import "theme"
import "wallpaper"
import "bar"
import "launcher"
import "notifications"

ShellRoot {
    id: root

    // ── Background wallpaper (one per screen) ──────────────
    Variants {
        model: Quickshell.screens
        delegate: Wallpaper {
            screen: modelData
        }
    }

    // ── Top bar (one per screen) ───────────────────────────
    Variants {
        model: Quickshell.screens
        delegate: Bar {
            screen: modelData
        }
    }

    // ── Application launcher (lazy overlay, no surface until opened)
    Launcher {
        id: appLauncher
    }

    // ── Notification daemon (invisible, manages popups) ───
    NotificationDaemon {
        id: notifDaemon
    }
}
