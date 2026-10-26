import QtQuick
import QtQuick.Layouts
import Lumina.UI

// Radio-style list of tracks. `offLabel` adds a "-1" entry.
ColumnLayout {
    id: root
    property var tracks: []
    property int currentId: -1
    property string offLabel: ""
    signal chosen(int id)
    spacing: 2

    Repeater {
        model: {
            var l = []
            if (root.offLabel !== "") l.push({ id: -1, name: root.offLabel })
            for (var i = 0; i < root.tracks.length; ++i) if (root.tracks[i].id >= 0) l.push(root.tracks[i])
            return l
        }
        delegate: Rectangle {
            required property var modelData
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            radius: 8
            readonly property bool selected: modelData.id === root.currentId
            color: selected ? Theme.accentSoft : ma.containsMouse ? Theme.hover : "transparent"
            activeFocusOnTab: true
            Keys.onReturnPressed: root.chosen(modelData.id)
            Keys.onSpacePressed: root.chosen(modelData.id)
            border.width: activeFocus ? 2 : 0; border.color: Theme.accent
            Accessible.role: Accessible.RadioButton; Accessible.name: modelData.name; Accessible.checked: selected
            MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onClicked: root.chosen(modelData.id) }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 10
                Icon { name: "check"; size: 16; color: Theme.accent; opacity: selected ? 1 : 0 }
                Text { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight; color: selected ? Theme.text : Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
            }
        }
    }
    Text {
        visible: root.tracks.length === 0 && root.offLabel === ""
        text: qsTr("No tracks available.")
        color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
    }
}
