import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

Slider {
    id: root
    property bool showValue: false
    property string suffix: ""
    property int decimals: 0
    implicitHeight: 28
    implicitWidth: 160
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    background: Item {
        x: root.leftPadding; y: root.topPadding + root.availableHeight / 2 - 2
        width: root.availableWidth; height: 4
        Rectangle { anchors.fill: parent; radius: 2; color: Theme.elevated; border.color: Theme.border; border.width: 1 }
        Rectangle { width: root.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
    }
    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 16; height: 16; radius: 8
        color: "#FFFFFF"
        border.width: root.visualFocus ? 3 : 0
        border.color: Theme.accent
        scale: root.pressed ? 1.2 : root.hovered ? 1.1 : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.animFast } }
    }
}
