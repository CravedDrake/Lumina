import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

PanelFrame {
    id: root
    title: qsTr("Audio")
    icon: "audio"
    property var devices: Player.audioDevices()
    onVisibleChanged: if (visible) devices = Player.audioDevices()

    PanelLabel { text: qsTr("Track") }
    TrackList { Layout.fillWidth: true; tracks: Player.audioTracks; currentId: Player.audioTrack; onChosen: function(id) { Player.setAudioTrack(id) } }

    PanelLabel { text: qsTr("Output device") }
    AppComboBox {
        Layout.fillWidth: true
        model: root.devices
        displayText: count > 0 ? currentText : qsTr("Default device")
        textRole: "name"; valueRole: "id"
        onActivated: Player.setAudioDevice(currentValue)
    }

    PanelLabel { text: qsTr("Channels") }
    AppComboBox {
        Layout.fillWidth: true
        textRole: "text"; valueRole: "value"
        model: [{ text: qsTr("Default"), value: "default" }, { text: qsTr("Stereo"), value: "stereo" }, { text: qsTr("Reverse stereo"), value: "reverse" },
                { text: qsTr("Left only"), value: "left" }, { text: qsTr("Right only"), value: "right" }, { text: qsTr("Dolby surround"), value: "dolby" }]
        onActivated: Player.setStereoMode(currentValue)
    }

    PanelLabel { text: qsTr("Audio delay") }
    RowLayout {
        Layout.fillWidth: true
        IconButton { iconName: "minus"; diameter: 32; onClicked: Player.adjustAudioDelay(-100) }
        Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: (Player.audioDelayMs > 0 ? "+" : "") + Player.audioDelayMs + " ms"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
        IconButton { iconName: "plus"; diameter: 32; onClicked: Player.adjustAudioDelay(100) }
        AppButton { kind: "ghost"; text: qsTr("Reset"); onClicked: Player.setAudioDelayMs(0) }
    }

    PanelLabel { text: qsTr("Equaliser") }
    RowLayout {
        Layout.fillWidth: true
        AppSwitch { checked: Player.eqEnabled; onToggled: Player.setEqEnabled(checked) }
        Text { text: Player.eqEnabled ? qsTr("On") : qsTr("Off"); color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
    }
    AppComboBox {
        Layout.fillWidth: true
        enabled: Player.eqEnabled
        displayText: currentIndex >= 0 ? currentText : qsTr("Custom")
        model: Player.eqPresetNames()
        currentIndex: Player.eqPreset
        onActivated: Player.applyEqPreset(currentIndex)
    }
    RowLayout {
        Layout.fillWidth: true
        enabled: Player.eqEnabled
        opacity: enabled ? 1 : 0.5
        Text { text: qsTr("Pre-amp"); color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; Layout.preferredWidth: 56 }
        AppSlider { Layout.fillWidth: true; from: -20; to: 20; stepSize: 0.5; value: Player.eqPreamp; onMoved: Player.setEqPreamp(value) }
        Text { text: Player.eqPreamp.toFixed(1) + " dB"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
    }
    Row {
        Layout.fillWidth: true
        Layout.preferredHeight: 150
        enabled: Player.eqEnabled
        opacity: enabled ? 1 : 0.5
        spacing: 0
        property var freqs: Player.eqFrequencies()
        Repeater {
            model: parent.freqs.length
            delegate: Item {
                required property int index
                width: parent.width / parent.freqs.length; height: parent.height
                Slider {
                    id: bandSlider
                    anchors { top: parent.top; bottom: lbl.top; horizontalCenter: parent.horizontalCenter }
                    orientation: Qt.Vertical
                    from: -20; to: 20; stepSize: 0.5
                    value: Player.eqBands[index] !== undefined ? Player.eqBands[index] : 0
                    onMoved: Player.setEqBand(index, value)
                    width: 22
                    Accessible.name: lbl.text + " Hz"
                    background: Rectangle { x: parent.width / 2 - 2; width: 4; height: parent.height; radius: 2; color: Theme.elevated
                        Rectangle { width: parent.width; radius: 2; color: Theme.accent; y: bandSlider.visualPosition * parent.height < parent.height / 2 ? bandSlider.visualPosition * parent.height : parent.height / 2
                                    height: Math.abs(bandSlider.visualPosition * parent.height - parent.height / 2) } }
                    handle: Rectangle { x: parent.width / 2 - 7; y: bandSlider.visualPosition * (bandSlider.height - height); width: 14; height: 14; radius: 7; color: "#FFFFFF" }
                }
                Text {
                    id: lbl
                    anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter }
                    readonly property real f: parent.parent.freqs[index]
                    text: f >= 1000 ? (f / 1000) + "k" : String(Math.round(f))
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: 10
                }
            }
        }
    }
    PanelLabel { text: qsTr("Saved profiles") }
    RowLayout {
        Layout.fillWidth: true
        AppComboBox { id: profCombo; Layout.fillWidth: true; model: Player.eqProfileNames(); displayText: count > 0 ? currentText : qsTr("No saved profiles") }
        AppButton { text: qsTr("Load"); enabled: profCombo.count > 0; onClicked: Player.loadEqProfile(profCombo.currentText) }
        IconButton { iconName: "trash"; diameter: 32; enabled: profCombo.count > 0; tip: qsTr("Delete profile"); onClicked: { Player.deleteEqProfile(profCombo.currentText); profCombo.model = Player.eqProfileNames() } }
    }
    AppButton {
        Layout.fillWidth: true; iconName: "save"; text: qsTr("Save current as profile…")
        onClicked: Actions.prompt(qsTr("Save equaliser profile"), qsTr("Profile name"), "", qsTr("Save"), function(n) { if (n.trim() !== "") { Player.saveEqProfile(n.trim()); profCombo.model = Player.eqProfileNames() } })
    }
}
