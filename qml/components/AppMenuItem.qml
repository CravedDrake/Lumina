import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

MenuItem {
    id: root
    property string iconName: ""
    property bool danger: false
    implicitHeight: 34
    implicitWidth: 220
    hoverEnabled: true
    contentItem: Row {
        spacing: 10
        leftPadding: 6
        Item {
            width: 18; height: parent.height
            Icon {
                anchors.centerIn: parent
                visible: root.iconName !== ""
                name: root.iconName
                size: 16
                color: root.danger ? Theme.danger : Theme.textDim
            }
            Icon {
                anchors.centerIn: parent
                visible: root.checkable && root.checked
                name: "check"
                size: 16
                color: Theme.accent
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsBody
            color: root.danger ? Theme.danger : Theme.text
            elide: Text.ElideRight
            width: root.width - 50
        }
    }
    background: Rectangle {
        radius: 6
        color: root.highlighted || root.hovered ? Theme.hover : "transparent"
        opacity: root.enabled ? 1 : 0.5
    }
}
