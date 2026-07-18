// Individual notification popup
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import Quickshell.Widgets
import Quickshell.Services.Notifications
import QtQuick
import QtQuick.Layouts
import "../theme"

WlrLayershell {
    id: popup

    required property var notif

    anchors {
        top: true
        right: true
    }
    anchors.rightMargin: Theme.notifMargin
    anchors.topMargin: Theme.notifMargin

    // Smooth repositioning when other popups are added/removed
    Behavior on anchors.topMargin {
        NumberAnimation {
            duration: Theme.animNormal
            easing.type: Easing.OutCubic
        }
    }
    layer: WlrLayer.Overlay
    keyboardFocus: WlrKeyboardFocus.None
    visible: true
    implicitWidth: Theme.notifWidth
    implicitHeight: Math.min(contentLayout.implicitHeight + 20, Theme.notifMaxHeight)

    // Public dismiss function — called by NotificationDaemon
    function dismiss() {
        if (animatedContent) animatedContent.dismiss()
    }

    // ── Animated content wrapper ────────────────────────────
    // WlrLayershell doesn't support transform/opacity directly,
    // so we animate an inner Item instead.
    Item {
        id: animatedContent
        anchors.fill: parent

        // Animation state
        property real slideX: 60
        property real contentOpacity: 0
        property real contentScale: 0.9
        property bool dismissing: false

        // Entrance animation triggers on component completion
        Component.onCompleted: entranceAnim.start()

        function dismiss() {
            if (dismissing) return
            dismissing = true
            exitAnim.start()
        }

        ParallelAnimation {
            id: entranceAnim
            NumberAnimation {
                target: animatedContent; property: "slideX"; to: 0
                duration: Theme.animSlow; easing.type: Easing.OutBack
            }
            NumberAnimation {
                target: animatedContent; property: "contentOpacity"; to: 1
                duration: Theme.animNormal; easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: animatedContent; property: "contentScale"; to: 1.0
                duration: Theme.animSlow; easing.type: Easing.OutBack
            }
        }

        ParallelAnimation {
            id: exitAnim
            NumberAnimation {
                target: animatedContent; property: "slideX"; to: 80
                duration: Theme.animNormal; easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: animatedContent; property: "contentOpacity"; to: 0
                duration: Theme.animNormal; easing.type: Easing.InCubic
            }
            ScriptAction {
                script: {
                    popup.visible = false
                    popup.destroy(100)
                }
            }
        }

        transform: [
            Translate { x: animatedContent.slideX },
            Scale {
                origin.x: animatedContent.width / 2
                origin.y: animatedContent.height / 2
                xScale: animatedContent.contentScale
                yScale: animatedContent.contentScale
            }
        ]

        opacity: animatedContent.contentOpacity

        // ── Glassmorphism card ──────────────────────────────
        Rectangle {
            id: cardBg
            anchors.fill: parent
            color: Theme.glassBgStrong
            radius: Theme.radius
            border { color: Theme.glassBorder; width: 1 }

            // Urgency accent strip on left
            Rectangle {
                id: urgencyStrip
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    topMargin: Theme.radiusSm
                    bottomMargin: Theme.radiusSm
                }
                width: 3
                radius: 2
                color: {
                    if (!popup.notif) return Theme.accentAlt
                    switch (popup.notif.urgency) {
                        case NotificationUrgency.Critical: return Theme.accentDanger
                        case NotificationUrgency.Low:     return Theme.fgFaint
                        default:                          return Theme.accent
                    }
                }

                Behavior on color {
                    ColorAnimation { duration: Theme.animFast }
                }
            }
        }

        // ── Content ─────────────────────────────────────────
        ColumnLayout {
            id: contentLayout
            anchors {
                fill: parent
                margins: 12
                leftMargin: 20
            }
            spacing: 6

            // ── Header: app icon + app name + dismiss button ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                IconImage {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    source: popup.notif ? (popup.notif.appIcon || "") : ""
                    smooth: true
                    visible: source != ""
                }

                Text {
                    Layout.fillWidth: true
                    text: popup.notif ? (popup.notif.appName || "Notification") : ""
                    color: Theme.fgDim
                    font {
                        family: Theme.fontFamily
                        pixelSize: Theme.fontSizeXs
                        weight: Theme.fontWeightBold
                    }
                    renderType: Text.NativeRendering
                    elide: Text.ElideRight
                }

                // Dismiss button
                Rectangle {
                    width: 20; height: 20
                    radius: Theme.radiusFull
                    color: closeMouse.containsMouse
                        ? Theme.glassHover : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: Theme.animFast }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: closeMouse.containsMouse
                            ? Theme.fg : Theme.fgDim
                        font.pixelSize: 10

                        Behavior on color {
                            ColorAnimation { duration: Theme.animFast }
                        }
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (popup.notif && popup.notif.tracked) {
                                popup.notif.dismiss()
                            }
                        }
                    }
                }
            }

            // ── Summary (title) ─────────────────────────────
            Text {
                Layout.fillWidth: true
                text: popup.notif ? (popup.notif.summary || "") : ""
                color: Theme.fg
                font {
                    family: Theme.fontFamily
                    pixelSize: Theme.fontSizeMd
                    weight: Theme.fontWeightBold
                }
                renderType: Text.NativeRendering
                elide: Text.ElideRight
                maximumLineCount: 1
                visible: text !== ""
            }

            // ── Body ────────────────────────────────────────
            Text {
                Layout.fillWidth: true
                text: popup.notif ? (popup.notif.body || "") : ""
                color: Theme.fgDim
                font {
                    family: Theme.fontFamily
                    pixelSize: Theme.fontSizeSm
                }
                renderType: Text.NativeRendering
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                visible: text !== ""
            }

            // ── Action buttons ──────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: popup.notif && popup.notif.actions
                    && popup.notif.actions.length > 0

                Repeater {
                    model: popup.notif ? popup.notif.actions : []
                    delegate: Rectangle {
                        height: 28
                        width: actionText.implicitWidth + 20
                        radius: Theme.radiusSm
                        color: actionMouse.containsMouse
                            ? Theme.glassHover : Theme.bgAlt
                        border {
                            color: actionMouse.containsMouse
                                ? Theme.borderDefault : Theme.borderSubtle
                            width: 1
                        }

                        Behavior on color {
                            ColorAnimation { duration: Theme.animFast }
                        }
                        Behavior on border.color {
                            ColorAnimation { duration: Theme.animFast }
                        }

                        Text {
                            id: actionText
                            anchors.centerIn: parent
                            text: modelData.text || modelData.identifier || ""
                            color: actionMouse.containsMouse
                                ? Theme.fg : Theme.fgDim
                            font {
                                family: Theme.fontFamily
                                pixelSize: Theme.fontSizeSm
                                weight: Theme.fontWeightMedium
                            }
                            renderType: Text.NativeRendering

                            Behavior on color {
                                ColorAnimation { duration: Theme.animFast }
                            }
                        }

                        MouseArea {
                            id: actionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                modelData.invoke()
                                if (popup.notif && popup.notif.tracked) {
                                    popup.notif.dismiss()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
