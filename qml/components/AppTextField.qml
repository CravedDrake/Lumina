import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

TextField {
    id: root
    property string icon: ""
    implicitHeight: Theme.rowHeight
    implicitWidth: 220
    leftPadding: icon !== "" ? 38 : 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: "#FFFFFF"
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsBody
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    background: Rectangle {
        radius: Theme.radiusSmall + 2
        color: Theme.surface2
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent : root.hovered ? Theme.borderStrong : Theme.border
        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: 16
            color: Theme.textMuted
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
