// Chroma Shell Theme — shared constants
pragma Singleton

import QtQuick

QtObject {
    // ── Palette ──────────────────────────────────────────────
    // Base colors — harmonized with Chroma's default theme
    readonly property color bg:            "#1a1a2e"   // deep navy
    readonly property color bgAlt:         "#16213e"   // slightly lighter navy
    readonly property color surface:       "#0f3460"   // accent navy
    readonly property color surfaceAlt:    "#1a4a8a"   // lighter accent
    readonly property color accent:        "#e94560"   // vibrant red-pink
    readonly property color accentAlt:     "#0abde3"   // cyan
    readonly property color fg:            "#eaeaea"   // near-white
    readonly property color fgDim:         "#8892b0"   // muted text
    readonly property color fgFaint:       "#495670"   // very muted

    // Glassmorphism
    readonly property color glassBg:       "#d91a1a2e" // bg @ 85%
    readonly property color glassBorder:   "#33ffffff" // white @ 20%
    readonly property color glassHover:    "#26ffffff" // white @ 15%
    readonly property color glassActive:   "#40ffffff" // white @ 25%

    // Group indicator colors — match Chroma's group_hues
    readonly property var groupColors: [
        "#0abde3",  // cyan
        "#e94560",  // red-pink
        "#54a0ff",  // blue
        "#5f27cd",  // purple
        "#01a3a4",  // teal
        "#feca57",  // yellow
    ]

    // ── Typography ───────────────────────────────────────────
    readonly property string fontFamily:   "sans-serif"
    readonly property int    fontSizeSm:   11
    readonly property int    fontSize:     13
    readonly property int    fontSizeLg:   16
    readonly property int    fontSizeXl:   22
    readonly property int    fontWeightNormal: Font.Normal
    readonly property int    fontWeightBold:   Font.DemiBold

    // ── Sizing ───────────────────────────────────────────────
    readonly property int    barHeight:     32
    readonly property int    barPadding:    8
    readonly property int    iconSize:      16
    readonly property int    trayIconSize:  18
    readonly property int    radius:        8
    readonly property int    radiusSm:      4
    readonly property int    radiusLg:      12

    // Launcher
    readonly property int    launcherWidth:  480
    readonly property int    launcherMaxHeight: 360
    readonly property int    launcherItemHeight: 40

    // Notifications
    readonly property int    notifWidth:     340
    readonly property int    notifMaxHeight: 160
    readonly property int    notifGap:       8
    readonly property int    notifMargin:    12

    // ── Animations ───────────────────────────────────────────
    readonly property int    animFast:       150
    readonly property int    animNormal:     250
    readonly property int    animSlow:       400
    readonly property string easeOut:        "easeOutCubic"
    readonly property string easeIn:         "easeInCubic"
    readonly property string easeInOut:      "easeInOutCubic"
}
