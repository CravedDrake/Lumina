import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

AppDialog {
    id: root
    title: qsTr("Add to playlist")
    property var mediaIds: []
    function pick(ids) { mediaIds = ids; open() }
    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(Math.max(contentHeight, 40), 280)
        clip: true; spacing: 4
        model: Library.playlists
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width; height: 42; radius: 8
            color: pma.containsMouse ? Theme.hover : Theme.surface2
            MouseArea { id: pma; anchors.fill: parent; hoverEnabled: true; onClicked: { Library.addToPlaylist(modelData.id, root.mediaIds); Actions.success(qsTr("Added to “%1”").arg(modelData.name)); root.close() } }
            RowLayout { anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Icon { name: "list"; size: 16; color: Theme.accent }
                Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
                Text { text: modelData.count; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
            }
        }
        Text { anchors.centerIn: parent; visible: parent.count === 0; text: qsTr("No playlists yet."); color: Theme.textMuted; font.family: Theme.fontFamily }
    }
    footer: [
        AppButton { iconName: "plus"; text: qsTr("New playlist…"); onClicked: { root.close(); Actions.prompt(qsTr("New playlist"), qsTr("Name"), "", qsTr("Create"), function(n) { if (n.trim() === "") return; var id = Library.createPlaylist(n.trim()); if (id > 0) { Library.addToPlaylist(id, root.mediaIds); Actions.success(qsTr("Added to “%1”").arg(n.trim())) } }) } },
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Cancel"); onClicked: root.close() }
    ]
}
