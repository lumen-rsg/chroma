// Notification daemon — receives and displays desktop notifications
import Quickshell
import Quickshell.Services.Notifications
import QtQuick
import "../theme"

Item {
    id: daemon

    // ── Notification server ───────────────────────────────
    NotificationServer {
        id: notifServer
        bodySupported: true
        actionsSupported: true
        imageSupported: true

        onNotification: function(notification) {
            daemon.createPopup(notification)
        }
    }

    // Track active notification popups
    property var activePopups: []

    function createPopup(notification) {
        // Start tracking the notification so we can control its lifecycle
        notification.tracked = true

        var comp = Qt.createComponent("NotificationPopup.qml")
        if (comp.status === Component.Ready) {
            var popup = comp.createObject(daemon, {
                "notif": notification
            })
            if (popup) {
                activePopups.push(popup)
                repositionPopups()

                // Auto-remove when closed
                function cleanup() {
                    var idx = activePopups.indexOf(popup)
                    if (idx >= 0) activePopups.splice(idx, 1)
                    popup.destroy(500) // let exit animation play
                    repositionPopups()
                }
                notification.closed.connect(cleanup)

                // Auto-dismiss after timeout (unless resident)
                if (!notification.resident && notification.expireTimeout > 0) {
                    var timer = Qt.createQmlObject(
                        'import QtQuick; Timer { interval: ' + notification.expireTimeout + '; running: true; repeat: false }',
                        popup, "dismissTimer"
                    )
                    timer.triggered.connect(function() {
                        if (notification.tracked) {
                            notification.dismiss()
                        }
                    })
                }
            }
        }
    }

    function repositionPopups() {
        var yOffset = Theme.notifMargin
        for (var i = 0; i < activePopups.length; i++) {
            if (activePopups[i]) {
                activePopups[i].y = yOffset
                yOffset += activePopups[i].height + Theme.notifGap
            }
        }
    }
}
