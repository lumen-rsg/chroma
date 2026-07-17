// Shared application state — cross-component communication
pragma Singleton

import QtQuick

QtObject {
    // Launcher toggle
    property bool launcherOpen: false

    // Signal for external notifications
    signal launcherToggled(bool open)
}
