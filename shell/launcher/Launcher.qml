// Application launcher — overlay with search + app grid
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import Quickshell.Widgets
import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: launcherRoot

    // ── Public API ──────────────────────────────────────────
    property bool open: false

    onOpenChanged: {
        if (open) {
            openLauncher()
        } else {
            closeLauncher()
        }
        AppState.launcherOpen = open
    }

    // ── Overlay management ──────────────────────────────────
    property var overlayWindow: null
    property bool closing: false

    function openLauncher() {
        if (overlayWindow) return
        closing = false
        overlayWindow = overlayComponent.createObject(launcherRoot)
        if (overlayWindow) {
            overlayWindow.dismissed.connect(function() {
                launcherRoot.open = false
            })
            // Trigger entrance animation after creation
            overlayWindow.startEntrance()
        }
    }

    function closeLauncher() {
        if (!overlayWindow || closing) return
        closing = true
        overlayWindow.startExit()
    }

    // ── Overlay window component ───────────────────────────
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

            // ── Entrance / Exit animation state ────────────
            property real cardScale: 0.92
            property real cardOpacity: 0
            property real cardYOffset: 30
            property real backdropOpacity: 0

            function startEntrance() {
                searchField.forceActiveFocus()
                searchField.selectAll()
                backdropAnimationIn.start()
            }

            function startExit() {
                cardAnimationOut.start()
            }

            // ── Animated backdrop ──────────────────────────
            Rectangle {
                id: backdrop
                anchors.fill: parent
                color: "#80000000"
                opacity: launcher.backdropOpacity

                MouseArea {
                    anchors.fill: parent
                    onClicked: launcher.dismissed()
                }

                // Smooth opacity
                Behavior on opacity { enabled: false } // controlled manually

                SequentialAnimation {
                    id: backdropAnimationIn
                    PropertyAnimation {
                        target: launcher
                        property: "backdropOpacity"
                        to: 1
                        duration: Theme.animNormal
                        easing.type: Easing.OutCubic
                    }
                    ScriptAction { script: cardAnimationIn.start() }
                }

                SequentialAnimation {
                    id: backdropAnimationOut
                    PropertyAnimation {
                        target: launcher
                        property: "backdropOpacity"
                        to: 0
                        duration: Theme.animNormal
                        easing.type: Easing.InCubic
                    }
                    ScriptAction {
                        script: {
                            launcher.visible = false
                            launcher.destroy()
                            launcherRoot.overlayWindow = null
                            launcherRoot.closing = false
                        }
                    }
                }
            }

            // ── Launcher card (centered) ───────────────────
            Rectangle {
                id: launcherCard
                width: Theme.launcherWidth
                height: Math.min(cardContent.implicitHeight + 20, Theme.launcherMaxHeight)
                anchors.centerIn: parent
                color: Theme.glassBgStrong
                radius: Theme.radiusLg
                border { color: Theme.glassBorder; width: 1 }

                // Transform group for entrance animation
                scale: launcher.cardScale
                opacity: launcher.cardOpacity

                transform: Translate {
                    y: launcher.cardYOffset
                }

                // Entrance: scale up + fade in + slide up
                SequentialAnimation {
                    id: cardAnimationIn
                    ParallelAnimation {
                        NumberAnimation {
                            target: launcher
                            property: "cardScale"
                            to: 1.0
                            duration: Theme.animSlow
                            easing.type: Easing.OutBack
                        }
                        NumberAnimation {
                            target: launcher
                            property: "cardOpacity"
                            to: 1.0
                            duration: Theme.animNormal
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: launcher
                            property: "cardYOffset"
                            to: 0
                            duration: Theme.animSlow
                            easing.type: Easing.OutBack
                        }
                    }
                }

                // Exit: scale down + fade out
                SequentialAnimation {
                    id: cardAnimationOut
                    ParallelAnimation {
                        NumberAnimation {
                            target: launcher
                            property: "cardScale"
                            to: 0.95
                            duration: Theme.animFast
                            easing.type: Easing.InCubic
                        }
                        NumberAnimation {
                            target: launcher
                            property: "cardOpacity"
                            to: 0
                            duration: Theme.animFast
                            easing.type: Easing.InCubic
                        }
                    }
                    ScriptAction { script: backdropAnimationOut.start() }
                }

                // Chain exit: first card, then backdrop
                function startCardExit() {
                    cardAnimationOut.start()
                }

                // ── Card content ────────────────────────────
                ColumnLayout {
                    id: cardContent
                    anchors {
                        fill: parent
                        margins: 10
                    }
                    spacing: 8

                    // ── Header: Search field ────────────────
                    Rectangle {
                        id: searchBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        color: Theme.bgAlt
                        radius: Theme.radius
                        border {
                            color: searchField.activeFocus
                                ? Theme.accent
                                : Theme.borderDefault
                            width: searchField.activeFocus ? 1.5 : 1
                        }

                        Behavior on border.color {
                            ColorAnimation { duration: Theme.animFast }
                        }

                        RowLayout {
                            anchors {
                                fill: parent
                                leftMargin: 12
                                rightMargin: 8
                            }
                            spacing: 8

                            // Search icon
                            Text {
                                text: "\uD83D\uDD0D"  // magnifying glass
                                font.pixelSize: 14
                                color: searchField.activeFocus
                                    ? Theme.accent
                                    : Theme.fgDim
                                Layout.alignment: Qt.AlignVCenter

                                Behavior on color {
                                    ColorAnimation { duration: Theme.animFast }
                                }
                            }

                            // Text input
                            TextInput {
                                id: searchField
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                color: Theme.fg
                                font {
                                    family: Theme.fontFamily
                                    pixelSize: Theme.fontSizeMd
                                }
                                renderType: Text.NativeRendering
                                focus: true
                                activeFocusOnTab: true
                                selectByMouse: true

                                // Placeholder
                                Text {
                                    anchors.fill: parent
                                    text: "Search applications..."
                                    color: Theme.fgFaint
                                    font: parent.font
                                    visible: !searchField.text
                                        && !searchField.activeFocus
                                }

                                // Keyboard navigation
                                Keys.onEscapePressed: {
                                    if (searchField.text) {
                                        searchField.text = ""
                                    } else {
                                        launcher.dismissed()
                                    }
                                }
                                Keys.onDownPressed: {
                                    appList.forceActiveFocus()
                                    if (appList.count > 0) {
                                        appList.currentIndex = 0
                                    }
                                }
                                Keys.onReturnPressed: {
                                    if (appList.currentItem) {
                                        appList.currentItem.launchApp()
                                    } else if (appList.count > 0) {
                                        // Launch first visible item
                                        appList.currentIndex = 0
                                        if (appList.currentItem) {
                                            appList.currentItem.launchApp()
                                        }
                                    }
                                }
                            }

                            // Clear button
                            Rectangle {
                                width: 22; height: 22
                                radius: Theme.radiusFull
                                color: clearMouse.containsMouse
                                    ? Theme.glassHover
                                    : "transparent"
                                visible: searchField.text !== ""
                                Layout.alignment: Qt.AlignVCenter

                                Behavior on color {
                                    ColorAnimation { duration: Theme.animFast }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "✕"
                                    color: Theme.fgDim
                                    font.pixelSize: 10
                                }

                                MouseArea {
                                    id: clearMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        searchField.text = ""
                                        searchField.forceActiveFocus()
                                    }
                                }
                            }
                        }
                    }

                    // ── Separator ───────────────────────────
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.borderSubtle
                    }

                    // ── App list or empty state ─────────────
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 40

                        // Empty state
                        Column {
                            anchors.centerIn: parent
                            visible: filteredApps.length === 0
                            spacing: 6

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: searchField.text ? "No results" : "No applications"
                                color: Theme.fgFaint
                                font {
                                    family: Theme.fontFamily
                                    pixelSize: Theme.fontSizeMd
                                    weight: Theme.fontWeightMedium
                                }
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: searchField.text
                                    ? "Try a different search term"
                                    : "Install .desktop files to see apps here"
                                color: Theme.fgFaint
                                font {
                                    family: Theme.fontFamily
                                    pixelSize: Theme.fontSizeSm
                                }
                                opacity: 0.6
                            }
                        }

                        // App list
                        ListView {
                            id: appList
                            anchors.fill: parent
                            clip: true
                            spacing: 2
                            boundsBehavior: Flickable.StopAtBounds
                            visible: filteredApps.length > 0
                            focus: true

                            model: filteredApps
                            delegate: appDelegate

                            // Smooth highlight tracking
                            highlightMoveDuration: Theme.animFast
                            highlightResizeDuration: Theme.animFast

                            // Highlight component
                            highlight: Rectangle {
                                color: Theme.glassHover
                                radius: Theme.radiusSm
                                border {
                                    color: Theme.accent
                                    width: appList.activeFocus ? 1 : 0
                                }

                                Behavior on border.width {
                                    NumberAnimation { duration: Theme.animFast }
                                }
                            }

                            // Keyboard nav
                            Keys.onEscapePressed: {
                                searchField.forceActiveFocus()
                                appList.currentIndex = -1
                            }
                            Keys.onUpPressed: {
                                if (appList.currentIndex === 0) {
                                    searchField.forceActiveFocus()
                                    appList.currentIndex = -1
                                } else {
                                    appList.decrementCurrentIndex()
                                }
                            }
                            Keys.onReturnPressed: {
                                if (appList.currentItem) {
                                    appList.currentItem.launchApp()
                                }
                            }

                            // Scroll to current item
                            onCurrentIndexChanged: {
                                if (currentIndex >= 0) {
                                    positionViewAtIndex(currentIndex,
                                        ListView.Contain)
                                }
                            }
                        }
                    }
                }
            }

            // ── Filtered model ──────────────────────────────
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
                        || (app.genericName
                            && app.genericName.toLowerCase().indexOf(query) !== -1)
                        || (app.comment
                            && app.comment.toLowerCase().indexOf(query) !== -1)
                        || (app.keywords && app.keywords.some(function(k) {
                            return k.toLowerCase().indexOf(query) !== -1
                        }))
                })
            }

            // Auto-focus search on open
            Component.onCompleted: {
                // startEntrance() will be called externally after creation
            }

            // ── App delegate ────────────────────────────────
            Component {
                id: appDelegate

                Rectangle {
                    id: delegateItem
                    width: ListView.view.width
                    height: Theme.launcherItemHeight
                    color: "transparent"
                    radius: Theme.radiusSm

                    property bool isCurrent: ListView.isCurrentItem

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

                        // Sync hover with ListView current item
                        onEntered: ListView.view.currentIndex = index
                    }

                    RowLayout {
                        anchors {
                            fill: parent
                            leftMargin: 10
                            rightMargin: 10
                        }
                        spacing: 10

                        // App icon
                        Rectangle {
                            Layout.preferredWidth: Theme.launcherIconSize
                            Layout.preferredHeight: Theme.launcherIconSize
                            Layout.alignment: Qt.AlignVCenter
                            color: "transparent"
                            radius: Theme.radiusSm
                            clip: true

                            IconImage {
                                anchors.centerIn: parent
                                width: Theme.launcherIconSize
                                height: Theme.launcherIconSize
                                source: modelData.icon || ""
                                smooth: true
                                asynchronous: true
                            }
                        }

                        // App name + description
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name || ""
                                color: delegateItem.isCurrent
                                    ? Theme.accent : Theme.fg
                                font {
                                    family: Theme.fontFamily
                                    pixelSize: Theme.fontSizeMd
                                    weight: Theme.fontWeightMedium
                                }
                                renderType: Text.NativeRendering
                                elide: Text.ElideRight
                                maximumLineCount: 1

                                Behavior on color {
                                    ColorAnimation { duration: Theme.animFast }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.genericName
                                    || modelData.comment || ""
                                color: Theme.fgDim
                                font {
                                    family: Theme.fontFamily
                                    pixelSize: Theme.fontSizeSm
                                }
                                renderType: Text.NativeRendering
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                visible: text !== ""
                            }
                        }

                        // Keyboard shortcut hint (if app provides one)
                        Text {
                            visible: false // placeholder for future hotkey hints
                            text: ""
                            color: Theme.fgFaint
                            font {
                                family: Theme.fontMono
                                pixelSize: Theme.fontSizeXs
                            }
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }
            }
        }
    }

    // ── Sync with AppState ──────────────────────────────────
    Component.onCompleted: {
        AppState.launcherOpenChanged.connect(function() {
            launcherRoot.open = AppState.launcherOpen
        })
    }
}
