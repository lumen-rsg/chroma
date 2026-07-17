// Clock widget — time + date display
import Quickshell
import QtQuick
import "../theme"

Row {
    id: root
    spacing: Theme.barPadding

    SystemClock {
        id: clock
        precision: SystemClock.Minutes
        enabled: true
    }

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
        font { family: Theme.fontFamily; pixelSize: Theme.fontSize; weight: Theme.fontWeightBold }
        renderType: Text.NativeRendering
    }

    Text {
        id: dateText
        anchors.verticalCenter: parent.verticalCenter
        text: {
            var d = clock.date
            var days = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"]
            var months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
            return days[d.getDay()] + " " + months[d.getMonth()] + " " + d.getDate()
        }
        color: Theme.fgDim
        font { family: Theme.fontFamily; pixelSize: Theme.fontSizeSm }
        renderType: Text.NativeRendering
    }
}
