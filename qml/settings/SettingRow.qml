import QtQuick
import QtQuick.Layouts
import Lumina.UI

// Label + description on the left, control on the right.
Item {
    id: root
    property string label: ""
    property string description: ""
    default property alias control: slot.data
    Layout.fillWidth: true
    implicitHeight: Math.max(52, textCol.implicitHeight + 20)

    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.border; opacity: 0.6 }
    RowLayout {
        anchors.fill: parent
        spacing: 20
        ColumnLayout {
            id: textCol
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: root.label
                color: Theme.text
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Medium
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: root.description !== ""
                text: root.description
                color: Theme.textMuted
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                wrapMode: Text.WordWrap
            }
        }
        Row { id: slot; Layout.alignment: Qt.AlignVCenter | Qt.AlignRight; spacing: 10 }
    }
}
