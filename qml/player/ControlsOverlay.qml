import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

// Bottom control bar of the player: seek bar + transport + toggles.
Item {
    id: root
    implicitHeight: 112
    property bool compact: width < 1020
    signal togglePanel(string name)
    property string openPanel: ""

    Rectangle {   // gradient scrim for legibility over bright video
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#00000000" }
            GradientStop { position: 1; color: "#E6000000" }
        }
    }

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 18; rightMargin: 18; bottomMargin: 10 }
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text { text: App.formatTime(Player.positionMs); color: "#FFFFFF"; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.Medium; Layout.preferredWidth: 58; horizontalAlignment: Text.AlignRight }
            SeekBar { Layout.fillWidth: true }
            Text {
                text: Player.durationMs > 0 ? App.formatTime(Player.durationMs) : (Player.isStream ? qsTr("LIVE") : "--:--")
                color: "#FFFFFF"; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.Medium
                Layout.preferredWidth: 58
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            IconButton { iconName: "skip-prev"; iconColor: "#FFFFFF"; tip: qsTr("Previous"); enabled: Player.canPrevious || Player.positionMs > 3000; onClicked: Player.previous() }
            IconButton { iconName: "seek-back"; iconColor: "#FFFFFF"; tip: qsTr("Back %1 s").arg(Settings.values["playback/seekSmall"]); enabled: Player.seekable; onClicked: Player.seekSmall(-1) }
            IconButton {
                iconName: Player.playing ? "pause" : "play"; filled: true; diameter: 46; iconSize: 22
                tip: Player.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: Player.togglePause()
            }
            IconButton { iconName: "seek-forward"; iconColor: "#FFFFFF"; tip: qsTr("Forward %1 s").arg(Settings.values["playback/seekSmall"]); enabled: Player.seekable; onClicked: Player.seekSmall(1) }
            IconButton { iconName: "skip-next"; iconColor: "#FFFFFF"; tip: qsTr("Next"); enabled: Player.canNext; onClicked: Player.next() }
            IconButton { iconName: "stop"; iconColor: "#FFFFFF"; iconSize: 17; tip: qsTr("Stop"); onClicked: { Player.stop(); Actions.hidePlayer() } }
            IconButton { iconName: "frame-step"; iconColor: "#FFFFFF"; iconSize: 18; tip: qsTr("Next frame"); enabled: Player.active && !Player.isStream; onClicked: Player.frameStep() }
            VolumeControl { Layout.leftMargin: 6 }

            Item { Layout.fillWidth: true }

            Rectangle {
                visible: Math.abs(Player.rate - 1.0) > 0.001
                implicitWidth: rateText.implicitWidth + 16; implicitHeight: 24; radius: 12
                color: Theme.accentSoft; border.color: Theme.accent
                Text { id: rateText; anchors.centerIn: parent; text: Player.rate + "×"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                MouseArea { anchors.fill: parent; onClicked: Player.resetSpeed(); cursorShape: Qt.PointingHandCursor }
            }
            IconButton { iconName: "ab-loop"; iconColor: "#FFFFFF"; active: Player.loopA >= 0; activeColor: Theme.warning; tip: Player.loopA < 0 ? qsTr("A–B loop: set start") : Player.loopB < 0 ? qsTr("A–B loop: set end") : qsTr("A–B loop: clear"); enabled: Player.seekable; onClicked: Player.cycleAbLoop() }
            IconButton { visible: !root.compact; iconName: Player.repeatMode === "one" ? "repeat-one" : "repeat"; iconColor: "#FFFFFF"; active: Player.repeatMode !== "off"; tip: qsTr("Repeat: %1").arg(Player.repeatMode); onClicked: Player.cycleRepeat() }
            IconButton { visible: !root.compact; iconName: "shuffle"; iconColor: "#FFFFFF"; active: Player.shuffle; tip: qsTr("Shuffle"); onClicked: Player.shuffle = !Player.shuffle }
            IconButton { iconName: "subtitles"; iconColor: "#FFFFFF"; active: root.openPanel === "subtitles"; tip: qsTr("Subtitles (Shift+S)"); onClicked: root.togglePanel("subtitles") }
            IconButton { iconName: "audio"; iconColor: "#FFFFFF"; active: root.openPanel === "audio"; tip: qsTr("Audio (Shift+A)"); onClicked: root.togglePanel("audio") }
            IconButton { iconName: "monitor"; iconColor: "#FFFFFF"; active: root.openPanel === "video"; tip: qsTr("Video (Shift+V)"); onClicked: root.togglePanel("video") }
            IconButton { visible: !root.compact; iconName: "bookmark"; iconColor: "#FFFFFF"; active: root.openPanel === "bookmarks"; tip: qsTr("Bookmarks & chapters (Shift+B)"); onClicked: root.togglePanel("bookmarks") }
            IconButton { iconName: "queue"; iconColor: "#FFFFFF"; active: root.openPanel === "queue"; tip: qsTr("Queue (Q)"); onClicked: root.togglePanel("queue") }
            IconButton { visible: !root.compact; iconName: "camera"; iconColor: "#FFFFFF"; tip: qsTr("Screenshot (C)"); enabled: Player.active; onClicked: Player.takeScreenshot() }
            IconButton { visible: !root.compact; iconName: "pip"; iconColor: "#FFFFFF"; active: Actions.pipOpen; tip: qsTr("Picture-in-picture (Ctrl+P)"); onClicked: Actions.pipToggled() }
            IconButton { iconName: "more"; iconColor: "#FFFFFF"; tip: qsTr("More"); onClicked: moreMenu.popup(0, -moreMenu.implicitHeight - 8) }
            IconButton { iconName: Actions.fullscreen ? "fullscreen-exit" : "fullscreen"; iconColor: "#FFFFFF"; tip: qsTr("Fullscreen (F)"); onClicked: Actions.fullscreenToggled() }
        }
    }

    AppMenu {
        id: moreMenu
        parent: root
        x: root.width - width - 18
        AppMenuItem { text: qsTr("Go to time…"); iconName: "clock"; onTriggered: Actions.goToTimeRequested() }
        AppMenuItem { text: qsTr("Media information"); iconName: "info"; onTriggered: Actions.infoRequested(Player.mediaInfoRows(), Player.title) }
        AppMenuItem { text: qsTr("Add bookmark"); iconName: "bookmark"; onTriggered: Player.addBookmark("") }
        AppMenuItem { text: qsTr("Take screenshot"); iconName: "camera"; onTriggered: Player.takeScreenshot() }
        AppMenuItem { text: qsTr("Picture-in-picture"); iconName: "pip"; onTriggered: Actions.pipToggled() }
        AppMenuItem { text: qsTr("Always on top"); iconName: "pin"; checkable: true; checked: Actions.alwaysOnTop; onTriggered: Actions.alwaysOnTopToggled() }
        AppMenuItem { text: qsTr("Previous chapter"); iconName: "skip-prev"; enabled: Player.chapters.length > 0; onTriggered: Player.previousChapter() }
        AppMenuItem { text: qsTr("Next chapter"); iconName: "skip-next"; enabled: Player.chapters.length > 0; onTriggered: Player.nextChapter() }
        AppMenuItem { text: qsTr("Repeat: %1").arg(Player.repeatMode); iconName: "repeat"; onTriggered: Player.cycleRepeat() }
        AppMenuItem { text: qsTr("Shuffle"); iconName: "shuffle"; checkable: true; checked: Player.shuffle; onTriggered: Player.shuffle = !Player.shuffle }
        AppMenuItem { text: qsTr("Keyboard shortcuts"); iconName: "keyboard"; onTriggered: Actions.shortcutsHelpRequested() }
    }
}
