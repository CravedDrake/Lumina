import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

// Flat icon button with hover/press/focus states and a tooltip.
AbstractButton {
    id: root
    property string iconName: ""
    property int iconSize: 20
    property color iconColor: Theme.text
    property color activeColor: Theme.accent
    property bool active: false
    property bool filled: false          // solid accent background (primary action)
    property string tip: ""
    property int diameter: 36
    property bool round: true
    implicitWidth: diameter
    implicitHeight: diameter
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    Accessible.role: Accessible.Button
    Accessible.name: tip !== "" ? tip : text

    background: Rectangle {
        radius: root.round ? width / 2 : Theme.radiusSmall
        color: root.filled ? (root.down ? Qt.darker(Theme.accent, 1.15) : root.hovered ? Theme.accentHover : Theme.accent)
                           : root.down ? Theme.alpha(Theme.text, 0.18)
                           : root.hovered ? Theme.alpha(Theme.text, 0.10)
                           : root.active ? Theme.accentSoft : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.iconName
            size: root.iconSize
            color: root.filled ? Theme.textOnAccent : root.active ? root.activeColor : root.iconColor
        }
    }
    ToolTip.visible: hovered && tip !== ""
    ToolTip.delay: 500
    ToolTip.text: tip
}
