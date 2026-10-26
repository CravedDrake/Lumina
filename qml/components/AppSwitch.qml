import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

Switch {
    id: root
    implicitWidth: 44
    implicitHeight: 24
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    indicator: Rectangle {
        implicitWidth: 44; implicitHeight: 24
        radius: 12
        color: root.checked ? Theme.accent : Theme.elevated
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? Theme.text : root.checked ? Theme.accent : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Rectangle {
            width: 18; height: 18; radius: 9
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: "#FFFFFF"
            Behavior on x { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: Item {}
}
