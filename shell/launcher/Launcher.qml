// Application launcher — overlay with search + app grid
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import Quickshell.Widgets
import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: launcherRoot

    // ── Public API ────────────────────────────────────────
    property bool open: false

    onOpenChanged: {
        if (open) {
            openLauncher()
        } else {
            closeLauncher()
        }
        AppState.launcherOpen = open
    }

    // ── Dynamically create/destroy the overlay window ─────
    property var overlayWindow: null

    function openLauncher() {
        if (overlayWindow) return
        overlayWindow = overlayComponent.createObject(launcherRoot)
        if (overlayWindow) {
            overlayWindow.dismissed.connect(function() {
                launcherRoot.open = false
            })
        }
    }

    function closeLauncher() {
        if (overlayWindow) {
            overlayWindow.visible = false
            overlayWindow.destroy()
            overlayWindow = null
        }
    }

    // ── Overlay window component ─────────────────────────
    Component {
        id: overlayComponent

        WlrLayershell {
            id: launcher
            property alias searchText: searchField.text

            signal dismissed()

            anchors {
                top: true
                bottom: true
                left: true
                right: true
            }
            layer: WlrLayer.Overlay
            keyboardFocus: WlrKeyboardFocus.Exclusive
            visible: true

            // ── Backdrop ──────────────────────────────────
            Rectangle {
                anchors.fill: parent
                color: "#80000000"

                MouseArea {
                    anchors.fill: parent
                    onClicked: launcher.dismissed()
                }
            }

            // ── Launcher card ─────────────────────────────
            Rectangle {
                width: Theme.launcherWidth
                height: Math.min(launcherCard.contentHeight + 16, Theme.launcherMaxHeight)
                anchors.centerIn: parent
                color: Theme.glassBg
                radius: Theme.radiusLg
                border { color: Theme.glassBorder; width: 1 }

                ColumnLayout {
                    id: launcherCard
                    anchors {
                        fill: parent
                        margins: 8
                    }
                    spacing: 6

                    // ── Search field ──────────────────────
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        color: Theme.bgAlt
                        radius: Theme.radius

                        RowLayout {
                            anchors { fill: parent; margins: 8 }
                            spacing: 6

                            Text {
                                text: "\uD83D\uDD0D"
                                font.pixelSize: Theme.fontSize
                                color: Theme.fgDim
                            }

                            TextInput {
                                id: searchField
                                Layout.fillWidth: true
                                color: Theme.fg
                                font { family: Theme.fontFamily; pixelSize: Theme.fontSize }
                                renderType: Text.NativeRendering
                                focus: true
                                activeFocusOnTab: true

                                Text {
                                    anchors.fill: parent
                                    text: "Search applications..."
                                    color: Theme.fgFaint
                                    font: parent.font
                                    visible: !searchField.text && !searchField.activeFocus
                                }

                                Keys.onEscapePressed: launcher.dismissed()
                                Keys.onDownPressed: appList.incrementCurrentIndex()
                                Keys.onUpPressed: appList.decrementCurrentIndex()
                                Keys.onReturnPressed: {
                                    if (appList.currentItem) {
                                        appList.currentItem.launchApp()
                                    }
                                }
                            }
                        }
                    }

                    // ── Separator ─────────────────────────
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.glassBorder
                    }

                    // ── App list ──────────────────────────
                    ListView {
                        id: appList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds

                        model: filteredApps
                        delegate: appDelegate
                        focus: true
                    }
                }
            }

            // ── Filtered model ───────────────────────────
            property var allApps: {
                var apps = []
                var model = DesktopEntries.applications
                for (var i = 0; i < model.count; i++) {
                    var entry = model.get(i)
                    if (entry && !entry.noDisplay) {
                        apps.push(entry)
                    }
                }
                apps.sort(function(a, b) {
                    var na = a.name.toLowerCase()
                    var nb = b.name.toLowerCase()
                    if (na < nb) return -1
                    if (na > nb) return 1
                    return 0
                })
                return apps
            }

            property var filteredApps: {
                var query = searchField.text.toLowerCase().trim()
                if (!query) return allApps
                return allApps.filter(function(app) {
                    return app.name.toLowerCase().indexOf(query) !== -1
                        || (app.genericName && app.genericName.toLowerCase().indexOf(query) !== -1)
                        || (app.comment && app.comment.toLowerCase().indexOf(query) !== -1)
                        || (app.keywords && app.keywords.some(function(k) {
                            return k.toLowerCase().indexOf(query) !== -1
                        }))
                })
            }

            // ── App delegate ─────────────────────────────
            Component {
                id: appDelegate

                Rectangle {
                    id: delegateItem
                    width: ListView.view.width
                    height: Theme.launcherItemHeight
                    color: mouseArea.containsMouse || ListView.isCurrentItem
                        ? Theme.glassHover : "transparent"
                    radius: Theme.radius

                    function launchApp() {
                        launcher.dismissed()
                        try {
                            modelData.execute()
                        } catch (e) {
                            var proc = Qt.createQmlObject(
                                'import Quickshell.Io; Process { command: ["gtk-launch", "' + modelData.id + '"] }',
                                delegateItem, "launchProc"
                            )
                            if (proc) proc.running = true
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: delegateItem.launchApp()
                    }

                    RowLayout {
                        anchors {
                            fill: parent
                            leftMargin: 10
                            rightMargin: 10
                        }
                        spacing: 10

                        IconImage {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            source: modelData.icon || ""
                            smooth: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name || ""
                                color: Theme.fg
                                font { family: Theme.fontFamily; pixelSize: Theme.fontSize; weight: Theme.fontWeightBold }
                                renderType: Text.NativeRendering
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.genericName || modelData.comment || ""
                                color: Theme.fgDim
                                font { family: Theme.fontFamily; pixelSize: Theme.fontSizeSm }
                                renderType: Text.NativeRendering
                                elide: Text.ElideRight
                                visible: text !== ""
                            }
                        }
                    }
                }
            }

            // ── Initial focus ────────────────────────────
            Component.onCompleted: {
                searchField.forceActiveFocus()
            }
        }
    }

    // ── Sync with AppState ───────────────────────────────
    Component.onCompleted: {
        AppState.launcherOpenChanged.connect(function() {
            launcherRoot.open = AppState.launcherOpen
        })
    }
}
