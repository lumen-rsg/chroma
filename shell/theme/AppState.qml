// Shared application state — cross-component communication
pragma Singleton

import QtQuick

QtObject {
    // Launcher toggle
    property bool launcherOpen: false

    // Currently focused application name (set by WindowTitle)
    property string focusedAppName: ""

    // Active notification count (set by NotificationDaemon)
    property int notifCount: 0

    // Signal for external consumers
    signal launcherToggled(bool open)
}
