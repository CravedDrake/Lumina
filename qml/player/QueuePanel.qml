import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Rectangle {
    id: root
    signal closeRequested()
    color: "#FC0E1118"
    border.color: Theme.border; border.width: 1
    MouseArea { anchors.fill: parent; hoverEnabled: true; onWheel: function(w) { w.accepted = true } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 52; Layout.leftMargin: 16; Layout.rightMargin: 8
            Icon { name: "queue"; size: 18; color: Theme.accent }
            Text { text: qsTr("Queue"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody + 1; font.weight: Font.DemiBold }
            Text { text: Player.queue.count > 0 ? "  " + (Player.queue.currentIndex + 1) + " / " + Player.queue.count : ""; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; Layout.fillWidth: true }
            IconButton { iconName: "plus"; diameter: 32; iconSize: 18; tip: qsTr("Add files to queue"); onClicked: Actions.openFilesRequested() }
            IconButton { iconName: "save"; diameter: 32; iconSize: 17; tip: qsTr("Save queue as M3U…"); onClicked: Actions.saveQueueRequested() }
            IconButton { iconName: "trash"; diameter: 32; iconSize: 17; tip: qsTr("Clear queue (keeps current)"); onClicked: Player.queue.clearExceptCurrent() }
            IconButton { iconName: "close"; diameter: 32; iconSize: 18; tip: qsTr("Close panel"); onClicked: root.closeRequested() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 10; spacing: 6
            IconButton { iconName: Player.repeatMode === "one" ? "repeat-one" : "repeat"; active: Player.repeatMode !== "off"; tip: qsTr("Repeat: %1").arg(Player.repeatMode); onClicked: Player.cycleRepeat() }
            IconButton { iconName: "shuffle"; active: Player.shuffle; tip: Player.shuffle ? qsTr("Shuffle on") : qsTr("Shuffle off"); onClicked: Player.shuffle = !Player.shuffle }
            Item { Layout.fillWidth: true }
        }
        ListView {
            id: list
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: Player.queue
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Component.onCompleted: positionViewAtIndex(Player.queue.currentIndex, ListView.Center)
            delegate: Rectangle {
                id: row
                required property int index
                required property string title
                required property bool isCurrent
                required property bool isPast
                required property bool missing
                required property real durationMs
                width: list.width - 12; x: 6; height: 46; radius: 8
                color: isCurrent ? Theme.accentSoft : qma.containsMouse ? Theme.hover : "transparent"
                activeFocusOnTab: true
                Keys.onReturnPressed: Player.playIndex(index)
                Keys.onDeletePressed: Player.queue.removeAt(index)
                border.width: activeFocus ? 2 : 0; border.color: Theme.accent
                MouseArea { id: qma; anchors.fill: parent; hoverEnabled: true; onDoubleClicked: Player.playIndex(index); onClicked: Player.playIndex(index) }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 2; spacing: 8
                    Item {
                        Layout.preferredWidth: 22; Layout.fillHeight: true
                        Icon { anchors.centerIn: parent; visible: row.isCurrent; name: Player.playing ? "play" : "pause"; size: 14; color: Theme.accent }
                        Text { anchors.centerIn: parent; visible: !row.isCurrent; text: row.index + 1; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: 11 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Text { Layout.fillWidth: true; text: row.title; elide: Text.ElideRight; color: row.missing ? Theme.danger : row.isPast ? Theme.textMuted : Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: row.isCurrent ? Font.DemiBold : Font.Normal }
                        Text { visible: row.missing || row.durationMs > 0; text: row.missing ? qsTr("File missing") : App.formatTime(row.durationMs); color: row.missing ? Theme.danger : Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: 11 }
                    }
                    IconButton { visible: qma.containsMouse && row.index > 0; iconName: "move-up"; diameter: 26; iconSize: 14; tip: qsTr("Move up"); onClicked: Player.queue.move(row.index, row.index - 1) }
                    IconButton { visible: qma.containsMouse && row.index < list.count - 1; iconName: "move-down"; diameter: 26; iconSize: 14; tip: qsTr("Move down"); onClicked: Player.queue.move(row.index, row.index + 1) }
                    IconButton { iconName: "close"; diameter: 26; iconSize: 14; tip: qsTr("Remove"); onClicked: Player.queue.removeAt(row.index) }
                }
            }
        }
    }
}
