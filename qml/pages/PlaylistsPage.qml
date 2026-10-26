import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Item {
    id: root
    property int openId: -1
    // (testing) `--panel open` opens the first playlist at startup so screenshots can show it
    Component.onCompleted: if (StartupPanel === "open" && Library.playlists.length) openId = Library.playlists[0].id                         // playlist being viewed, -1 = list of playlists
    property var current: {
        for (var i = 0; i < Library.playlists.length; ++i)
            if (Library.playlists[i].id === openId) return Library.playlists[i]
        return null
    }
    onCurrentChanged: if (openId !== -1 && current === null) openId = -1

    function createPlaylist() {
        Actions.prompt(qsTr("New playlist"), qsTr("Name"), "", qsTr("Create"), function(name) {
            if (name.trim() !== "") Library.createPlaylist(name.trim())
        })
    }

    // ---------------------------------------------------------------- playlists list
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        anchors.bottomMargin: 0
        visible: root.openId === -1
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Icon { name: "list"; size: 26; color: Theme.accent }
            Text { text: qsTr("Playlists"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsHeading; font.weight: Font.Bold }
            Item { Layout.fillWidth: true }
            AppButton { iconName: "upload"; text: qsTr("Import M3U…"); onClicked: Actions.openPlaylistRequested() }
            AppButton { kind: "primary"; iconName: "plus"; text: qsTr("New playlist"); onClicked: root.createPlaylist() }
        }
        ListView {
            id: plist
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: Library.playlists
            visible: count > 0
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                required property var modelData
                width: plist.width; height: 64
                radius: Theme.radius
                color: ma.containsMouse ? Theme.hover : Theme.surface2
                border.color: Theme.border; border.width: 1
                MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onClicked: root.openId = modelData.id }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 12; spacing: 14
                    Rectangle { width: 40; height: 40; radius: 8; color: Theme.accentSoft; Icon { anchors.centerIn: parent; name: "list"; size: 20; color: Theme.accent } }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Text { text: modelData.name; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Medium; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: qsTr("%n item(s)", "", modelData.count || 0); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    }
                    IconButton { iconName: "play"; tip: qsTr("Play"); onClicked: { var ids = Library.playlistMediaIds(modelData.id); if (ids.length) Actions.playIds(ids, 0); else Actions.info(qsTr("This playlist is empty.")) } }
                    IconButton { iconName: "chevron-right"; tip: qsTr("Open"); onClicked: root.openId = modelData.id }
                }
            }
        }
        EmptyState {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            visible: plist.count === 0
            icon: "list"
            title: qsTr("No playlists yet")
            subtitle: qsTr("Create a playlist, add items from any context menu, or import an M3U/M3U8 file.")
        }
    }

    // ---------------------------------------------------------------- one playlist
    LibraryPage {
        anchors.fill: parent
        visible: root.openId !== -1
        view: "playlist"
        viewArg: root.openId
        title: root.current ? root.current.name : ""
        icon: "list"
        sortable: false
        emptyTitle: qsTr("This playlist is empty")
        emptySubtitle: qsTr("Right-click any item in your library and choose “Add to playlist…”.")
        headerActions: [
            AppButton { kind: "ghost"; iconName: "chevron-left"; text: qsTr("All playlists"); onClicked: root.openId = -1 },
            AppButton { iconName: "edit"; text: qsTr("Rename"); onClicked: Actions.prompt(qsTr("Rename playlist"), qsTr("Name"), root.current ? root.current.name : "", qsTr("Rename"), function(n) { if (n.trim() !== "") Library.renamePlaylist(root.openId, n.trim()) }) },
            AppButton { iconName: "save"; text: qsTr("Export M3U…"); onClicked: Actions.exportPlaylistRequested(root.openId) },
            AppButton { kind: "danger"; iconName: "trash"; text: qsTr("Delete"); onClicked: Actions.confirm(qsTr("Delete playlist?"), qsTr("The playlist “%1” will be deleted. Your media files are not affected.").arg(root.current ? root.current.name : ""), qsTr("Delete"), true, function() { Library.deletePlaylist(root.openId) }) }
        ]
    }
}
