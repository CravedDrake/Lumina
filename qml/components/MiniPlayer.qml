import QtQuick
import QtQuick.Layouts
import Lumina.UI

// Shown over library pages while something is loaded but the player view is hidden.
Rectangle {
    id: root
    height: 64
    radius: 14
    color: "#F2141824"
    border.color: Theme.borderStrong
    visible: Player.active && !Actions.playerVisible && !Actions.pipOpen
    RowLayout {
        anchors { fill: parent; leftMargin: 12; rightMargin: 10 }
        spacing: 10
        Rectangle {
            Layout.preferredHeight: 44; Layout.preferredWidth: 78; radius: 8; color: "#000000"; clip: true
            VideoThumb { anchors.fill: parent }
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 0
            Text { Layout.fillWidth: true; text: Player.title; elide: Text.ElideRight; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Medium }
            Text { text: App.formatTime(Player.positionMs) + (Player.durationMs > 0 ? " / " + App.formatTime(Player.durationMs) : ""); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: 12 }
        }
        IconButton { iconName: "skip-prev"; tip: qsTr("Previous"); onClicked: Player.previous() }
        IconButton { iconName: Player.playing ? "pause" : "play"; filled: true; tip: Player.playing ? qsTr("Pause") : qsTr("Play"); onClicked: Player.togglePause() }
        IconButton { iconName: "skip-next"; tip: qsTr("Next"); enabled: Player.canNext; onClicked: Player.next() }
        IconButton { iconName: "fullscreen"; tip: qsTr("Open player"); onClicked: Actions.showPlayer() }
        IconButton { iconName: "close"; tip: qsTr("Stop and close"); onClicked: Player.closeMedia() }
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 14; rightMargin: 14 }
        height: 2; color: Theme.elevated
        Rectangle { width: Player.durationMs > 0 ? parent.width * Player.positionMs / Player.durationMs : 0; height: parent.height; color: Theme.accent }
    }
    TapHandler { onTapped: Actions.showPlayer() }
}
