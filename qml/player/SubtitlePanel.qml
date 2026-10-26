import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

PanelFrame {
    id: root
    title: qsTr("Subtitles")
    icon: "subtitles"

    PanelLabel { text: qsTr("Track") }
    TrackList {
        Layout.fillWidth: true
        tracks: Player.subtitleTracks
        currentId: Player.subtitleTrack
        offLabel: qsTr("Off")
        onChosen: function(id) { Player.setSubtitleTrack(id) }
    }

    PanelLabel { text: qsTr("External files") }
    Repeater {
        model: Player.externalSubtitles
        delegate: AppButton {
            required property var modelData
            Layout.fillWidth: true
            kind: "ghost"; iconName: "file-video"
            text: modelData.name + (modelData.language ? "  (" + modelData.language + ")" : "")
            onClicked: Player.loadSubtitleFile(modelData.path)
        }
    }
    Text {
        visible: Player.externalSubtitles.length === 0
        text: qsTr("No subtitle files found next to this video.")
        color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    AppButton { Layout.fillWidth: true; iconName: "plus"; text: qsTr("Load subtitle file…"); onClicked: Actions.addSubtitleRequested() }

    PanelLabel { text: qsTr("Delay") }
    RowLayout {
        Layout.fillWidth: true
        IconButton { iconName: "minus"; diameter: 32; tip: qsTr("Earlier"); onClicked: Player.adjustSubtitleDelay(-Settings.values["subtitles/delayStep"]) }
        Text {
            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
            text: (Player.subtitleDelayMs > 0 ? "+" : "") + Player.subtitleDelayMs + " ms"
            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
        }
        IconButton { iconName: "plus"; diameter: 32; tip: qsTr("Later"); onClicked: Player.adjustSubtitleDelay(Settings.values["subtitles/delayStep"]) }
        AppButton { kind: "ghost"; text: qsTr("Reset"); onClicked: Player.setSubtitleDelayMs(0) }
    }
    PanelLabel { text: qsTr("Style") }
    Text {
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        text: qsTr("Style is set in Settings → Subtitles. Applying it reopens the video at the same position.")
        color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
    }
    RowLayout {
        Layout.fillWidth: true
        AppButton { Layout.fillWidth: true; iconName: "refresh"; text: qsTr("Apply style now"); onClicked: Player.applySubtitleStyle() }
        AppButton { iconName: "settings"; text: qsTr("Settings"); onClicked: { Actions.hidePlayer(); Actions.navigate("settings", 3) } }
    }
}
