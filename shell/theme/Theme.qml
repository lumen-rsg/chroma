// Chroma Shell Theme — design tokens & constants
pragma Singleton

import QtQuick

QtObject {
    // ═══════════════════════════════════════════════════════════
    //  Palette
    // ═══════════════════════════════════════════════════════════

    // ── Base surfaces ────────────────────────────────────────
    readonly property color bg:             "#0d1117"   // github-dark inspired
    readonly property color bgAlt:          "#161b22"
    readonly property color bgRaised:       "#1c2333"
    readonly property color bgOverlay:      "#21262d"

    // ── Accent ───────────────────────────────────────────────
    readonly property color accent:         "#58a6ff"   // blue
    readonly property color accentAlt:      "#3fb950"   // green
    readonly property color accentWarn:     "#d29922"   // amber
    readonly property color accentDanger:   "#f85149"   // red
    readonly property color accentPink:     "#db61a2"   // pink
    readonly property color accentCyan:     "#39d2c0"   // cyan
    readonly property color accentPurple:   "#a371f7"   // purple

    // ── Text ─────────────────────────────────────────────────
    readonly property color fg:             "#e6edf3"
    readonly property color fgDim:          "#8b949e"
    readonly property color fgFaint:        "#484f58"
    readonly property color fgDisabled:     "#3a3f47"

    // ── Borders ──────────────────────────────────────────────
    readonly property color borderSubtle:   "#21262d"
    readonly property color borderDefault:  "#30363d"
    readonly property color borderStrong:   "#484f58"

    // ── Glassmorphism (translucent overlays) ─────────────────
    readonly property color glassBg:        "#dd0d1117"   // bg @ ~87%
    readonly property color glassBgStrong:  "#f0161b22"   // bgAlt @ ~94%
    readonly property color glassBorder:    "#33ffffff"   // white @ 20%
    readonly property color glassHover:     "#1affffff"   // white @ 10%
    readonly property color glassActive:    "#26ffffff"   // white @ 15%

    // ── Group colors (match Chroma's group_hues) ─────────────
    readonly property var groupColors: [
        "#58a6ff",  // blue
        "#f85149",  // red
        "#3fb950",  // green
        "#a371f7",  // purple
        "#d29922",  // amber
        "#39d2c0",  // cyan
        "#db61a2",  // pink
    ]

    // ═══════════════════════════════════════════════════════════
    //  Shadows (for use with QtQuick.Effects MultiEffect)
    // ═══════════════════════════════════════════════════════════

    readonly property color shadowColor:    "#40000000"

    // Small shadow — buttons, cards
    readonly property real  shadowSmBlur:      0.15
    readonly property real  shadowSmY:         1
    readonly property real  shadowSmOpacity:   0.4

    // Medium shadow — bar, panels
    readonly property real  shadowMdBlur:      0.25
    readonly property real  shadowMdY:         3
    readonly property real  shadowMdOpacity:   0.5

    // Large shadow — launcher, modals
    readonly property real  shadowLgBlur:      0.40
    readonly property real  shadowLgY:         8
    readonly property real  shadowLgOpacity:   0.6

    // ═══════════════════════════════════════════════════════════
    //  Typography
    // ═══════════════════════════════════════════════════════════

    readonly property string fontFamily:        "system-ui, -apple-system, sans-serif"
    readonly property string fontMono:          "ui-monospace, SFMono-Regular, monospace"

    readonly property int    fontSizeXs:    10
    readonly property int    fontSizeSm:    11
    readonly property int    fontSize:      13
    readonly property int    fontSizeMd:    14
    readonly property int    fontSizeLg:    17
    readonly property int    fontSizeXl:    22

    readonly property int    fontWeightNormal:  Font.Normal
    readonly property int    fontWeightMedium:  Font.Medium
    readonly property int    fontWeightBold:    Font.DemiBold
    readonly property int    fontWeightHeavy:   Font.Bold

    // ═══════════════════════════════════════════════════════════
    //  Sizing & Spacing
    // ═══════════════════════════════════════════════════════════

    // Bar
    readonly property int    barHeight:       36
    readonly property int    barPadding:      10
    readonly property int    barItemSpacing:  6
    readonly property int    barRadius:       10

    // Icons
    readonly property int    iconSizeSm:      14
    readonly property int    iconSize:        18
    readonly property int    iconSizeLg:      22
    readonly property int    trayIconSize:    18

    // Radius
    readonly property int    radiusNone:      0
    readonly property int    radiusSm:        4
    readonly property int    radius:          8
    readonly property int    radiusLg:        12
    readonly property int    radiusXl:        16
    readonly property int    radiusFull:      999

    // Launcher
    readonly property int    launcherWidth:    520
    readonly property int    launcherMaxHeight: 400
    readonly property int    launcherItemHeight: 44
    readonly property int    launcherIconSize:  28

    // Notifications
    readonly property int    notifWidth:       360
    readonly property int    notifMaxHeight:   180
    readonly property int    notifGap:         10
    readonly property int    notifMargin:      16
    readonly property int    notifMaxVisible:  4

    // ═══════════════════════════════════════════════════════════
    //  Animation presets
    // ═══════════════════════════════════════════════════════════

    // Durations (ms)
    readonly property int    animInstant:     80
    readonly property int    animFast:        150
    readonly property int    animNormal:      250
    readonly property int    animSlow:        400
    readonly property int    animGlacial:     600

    // Easing curves (access in QML as e.g. Easing.OutCubic)
    // Documented here for reference; use QtQuick's Easing.* directly.

    // ═══════════════════════════════════════════════════════════
    //  Blur
    // ═══════════════════════════════════════════════════════════

    // Compositor-level blur via ext-background-effect protocol.
    // Set to false if your compositor doesn't support it.
    readonly property bool   blurEnabled:     true
    readonly property int    blurRegionRadius: 16
}
