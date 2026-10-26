import QtQuick
import QtQuick.Layouts
import Lumina.UI

ColumnLayout {
    id: root
    property string icon: "film"
    property string title: ""
    property string subtitle: ""
    property alias actions: actionRow.data
    spacing: 10
    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        width: 72; height: 72; radius: 36
        color: Theme.accentSoft
        Icon { anchors.centerIn: parent; name: root.icon; size: 32; color: Theme.accent }
    }
    Text {
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        color: Theme.text
        font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold
    }
    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 420
        visible: text !== ""
        text: root.subtitle
        color: Theme.textDim
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
    }
    Row {
        id: actionRow
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        spacing: 10
    }
}
