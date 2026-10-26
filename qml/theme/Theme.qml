pragma Singleton
import QtQuick

QtObject {
    // Surfaces (dark, slightly blue-tinted neutrals)
    readonly property color bg: "#0A0C11"
    readonly property color surface: "#11141B"
    readonly property color surface2: "#171B24"
    readonly property color elevated: "#1D2230"
    readonly property color hover: "#232938"
    readonly property color border: "#262C3B"
    readonly property color borderStrong: "#363E52"
    // Text
    readonly property color text: "#E9ECF3"
    readonly property color textDim: "#A3ABBD"
    readonly property color textMuted: "#6D768A"
    // Accent (user selectable)
    property color accent: "#7C5CFC"
    readonly property color accentHover: Qt.lighter(accent, 1.18)
    readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, 0.16)
    readonly property color textOnAccent: "#FFFFFF"
    // Status
    readonly property color success: "#34D399"
    readonly property color warning: "#FBBF24"
    readonly property color danger: "#F87171"
    readonly property color scrim: "#B3000000"
    readonly property color overlayTop: "#CC000000"

    // Metrics
    property bool compact: false
    property bool animations: true
    readonly property int radius: 10
    readonly property int radiusSmall: 6
    readonly property int radiusLarge: 16
    readonly property int pad: compact ? 10 : 14
    readonly property int rowHeight: compact ? 32 : 38
    readonly property int sidebarWidth: 236
    readonly property int sidebarCollapsedWidth: 64
    readonly property int titleBarHeight: 40
    readonly property int animFast: animations ? 120 : 0
    readonly property int animMed: animations ? 200 : 0

    readonly property string fontFamily: "Inter"
    readonly property int fsSmall: 12
    readonly property int fsBody: 14
    readonly property int fsTitle: 18
    readonly property int fsHeading: 26

    function iconUrl(name, color) {
        var c = color === undefined ? text : color
        return "image://icon/" + name + "?color=" + String(c).replace("#", "").slice(-6)
    }
    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
}
