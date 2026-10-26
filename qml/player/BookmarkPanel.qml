import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

PanelFrame {
    id: root
    title: qsTr("Bookmarks & chapters")
    icon: "bookmark"

    AppButton { Layout.fillWidth: true; kind: "primary"; iconName: "bookmark"; text: qsTr("Bookmark current position"); onClicked: Player.addBookmark("") }

    PanelLabel { text: qsTr("Bookmarks") }
    Repeater {
        model: Player.bookmarks
        delegate: Rectangle {
            required property var modelData
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: 8; color: bma.containsMouse ? Theme.hover : Theme.surface2
            MouseArea { id: bma; anchors.fill: parent; hoverEnabled: true; onClicked: Player.seekToBookmark(modelData.id) }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 4; spacing: 8
                Text { text: App.formatTime(modelData.positionMs); color: Theme.accent; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                Text { Layout.fillWidth: true; text: modelData.note || qsTr("Bookmark"); elide: Text.ElideRight; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
                IconButton { iconName: "edit"; diameter: 30; iconSize: 15; tip: qsTr("Edit note"); onClicked: Actions.prompt(qsTr("Bookmark note"), qsTr("Note"), modelData.note || "", qsTr("Save"), function(n) { Player.updateBookmark(modelData.id, n) }) }
                IconButton { iconName: "trash"; diameter: 30; iconSize: 15; tip: qsTr("Delete"); onClicked: Player.removeBookmark(modelData.id) }
            }
        }
    }
    Text {
        visible: Player.bookmarks.length === 0
        text: Player.currentMediaId > 0 ? qsTr("No bookmarks yet. Press B to add one at the current position.") : qsTr("Bookmarks are available for files in your library.")
        color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }

    PanelLabel { text: qsTr("Chapters") }
    Repeater {
        model: Player.chapters
        delegate: Rectangle {
            required property var modelData
            required property int index
            Layout.fillWidth: true; Layout.preferredHeight: 40; radius: 8
            color: Player.currentChapter === index ? Theme.accentSoft : cma.containsMouse ? Theme.hover : "transparent"
            MouseArea { id: cma; anchors.fill: parent; hoverEnabled: true; onClicked: Player.setChapter(index) }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                Text { text: App.formatTime(modelData.startMs); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                Text { Layout.fillWidth: true; text: modelData.name || qsTr("Chapter %1").arg(index + 1); elide: Text.ElideRight; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
            }
        }
    }
    Text {
        visible: Player.chapters.length === 0
        text: qsTr("This video has no chapters.")
        color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
    }
}
