// Clock widget — time + date display
import Quickshell
import QtQuick
import "../theme"

Item {
    id: root
    width: clockLayout.implicitWidth + 16
    height: Theme.barHeight

    property bool hovered: false

    // ── Hover background ────────────────────────────────────
    Rectangle {
        anchors.centerIn: parent
        width: clockLayout.implicitWidth + 16
        height: parent.height - 8
        radius: Theme.radiusSm
        color: root.hovered ? Theme.glassHover : "transparent"
        opacity: root.hovered ? 1 : 0

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: root.hovered = true
        onExited: root.hovered = false
    }

    SystemClock {
        id: clock
        precision: SystemClock.Minutes
        enabled: true
    }

    Row {
        id: clockLayout
        anchors.centerIn: parent
        spacing: 6

        // ── Time ────────────────────────────────────────────
        Text {
            id: timeText
            anchors.verticalCenter: parent.verticalCenter
            text: {
                var h = clock.hours
                var m = clock.minutes
                var ampm = h >= 12 ? "PM" : "AM"
                if (h > 12) h -= 12
                if (h === 0) h = 12
                var mm = m < 10 ? "0" + m : m
                return h + ":" + mm
            }
            color: Theme.fg
            font {
                family: Theme.fontMono
                pixelSize: Theme.fontSizeMd
                weight: Theme.fontWeightMedium
            }
            renderType: Text.NativeRendering
        }

        // ── AM/PM ───────────────────────────────────────────
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: clock.hours >= 12 ? "PM" : "AM"
            color: Theme.fgDim
            font {
                family: Theme.fontFamily
                pixelSize: Theme.fontSizeXs
                weight: Theme.fontWeightNormal
            }
            renderType: Text.NativeRendering
        }

        // ── Dot separator ───────────────────────────────────
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "·"
            color: Theme.fgFaint
            font.pixelSize: Theme.fontSizeSm
        }

        // ── Date ────────────────────────────────────────────
        Text {
            id: dateText
            anchors.verticalCenter: parent.verticalCenter
            text: {
                var d = clock.date
                var months = ["Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec"]
                return months[d.getMonth()] + " " + d.getDate()
            }
            color: Theme.fgDim
            font {
                family: Theme.fontFamily
                pixelSize: Theme.fontSizeSm
                weight: Theme.fontWeightNormal
            }
            renderType: Text.NativeRendering
        }
    }
}
