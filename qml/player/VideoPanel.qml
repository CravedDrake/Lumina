import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

PanelFrame {
    id: root
    title: qsTr("Video")
    icon: "monitor"

    component AdjustRow: RowLayout {
        id: row
        property string key: ""
        property string label: ""
        property real from: 0
        property real to: 2
        property real neutral: 1
        Layout.fillWidth: true
        Text { text: row.label; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; Layout.preferredWidth: 76 }
        AppSlider { Layout.fillWidth: true; from: row.from; to: row.to; stepSize: 0.01; value: Player.adjust[row.key] !== undefined ? Player.adjust[row.key] : row.neutral; onMoved: Player.setAdjustValue(row.key, value) }
        Text { text: (Player.adjust[row.key] !== undefined ? Player.adjust[row.key] : row.neutral).toFixed(2); color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
    }

    PanelLabel { text: qsTr("Speed") }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: Player.speedChoices()
            delegate: Rectangle {
                required property real modelData
                width: 52; height: 30; radius: 8
                readonly property bool sel: Math.abs(Player.rate - modelData) < 0.001
                color: sel ? Theme.accent : sma.containsMouse ? Theme.hover : Theme.elevated
                activeFocusOnTab: true
                Keys.onReturnPressed: Player.rate = modelData
                Keys.onSpacePressed: Player.rate = modelData
                border.width: activeFocus ? 2 : 0; border.color: Theme.text
                Accessible.role: Accessible.Button; Accessible.name: modelData + "x"
                MouseArea { id: sma; anchors.fill: parent; hoverEnabled: true; onClicked: Player.rate = modelData }
                Text { anchors.centerIn: parent; text: modelData + "×"; color: parent.sel ? Theme.textOnAccent : Theme.text; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Medium }
            }
        }
    }

    PanelLabel { text: qsTr("Picture") }
    AdjustRow { key: "brightness"; label: qsTr("Brightness"); from: 0; to: 2; neutral: 1 }
    AdjustRow { key: "contrast"; label: qsTr("Contrast"); from: 0; to: 2; neutral: 1 }
    AdjustRow { key: "saturation"; label: qsTr("Saturation"); from: 0; to: 3; neutral: 1 }
    AdjustRow { key: "gamma"; label: qsTr("Gamma"); from: 0.1; to: 4; neutral: 1 }
    AdjustRow { key: "hue"; label: qsTr("Hue"); from: -180; to: 180; neutral: 0 }
    AppButton { kind: "ghost"; iconName: "rotate"; text: qsTr("Reset picture"); onClicked: Player.resetAdjust() }

    PanelLabel { text: qsTr("Aspect ratio") }
    AppComboBox {
        Layout.fillWidth: true
        textRole: "text"; valueRole: "value"
        model: [{ text: qsTr("Auto (video's own)"), value: "auto" }, { text: "16:9", value: "16:9" }, { text: "4:3", value: "4:3" }, { text: "21:9", value: "21:9" },
                { text: "16:10", value: "16:10" }, { text: "1:1", value: "1:1" }, { text: qsTr("Fill screen (crop)"), value: "fill" }, { text: qsTr("Stretch"), value: "stretch" }]
        currentIndex: { for (var i = 0; i < model.length; ++i) if (model[i].value === Player.aspectMode) return i; return 0 }
        onActivated: Player.aspectMode = currentValue
    }

    PanelLabel { text: qsTr("Zoom") }
    RowLayout {
        Layout.fillWidth: true
        AppSlider { Layout.fillWidth: true; from: 0.25; to: 4; stepSize: 0.05; value: Player.zoom; onMoved: Player.zoom = value }
        Text { text: Math.round(Player.zoom * 100) + "%"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
    }

    PanelLabel { text: qsTr("Rotation") }
    RowLayout {
        Layout.fillWidth: true
        AppButton { Layout.fillWidth: true; iconName: "rotate"; text: qsTr("Rotate 90° (%1°)").arg(Player.videoRotation); onClicked: Player.videoRotation = (Player.videoRotation + 90) % 360 }
        AppButton { kind: "ghost"; text: qsTr("Reset view"); onClicked: Player.resetVideoLayout() }
    }

    PanelLabel { text: qsTr("Deinterlacing") }
    AppComboBox {
        Layout.fillWidth: true
        textRole: "text"; valueRole: "value"
        model: [{ text: qsTr("Off"), value: "" }, { text: "Yadif", value: "yadif" }, { text: "Yadif (2×)", value: "yadif2x" }, { text: "Bob", value: "bob" },
                { text: "Blend", value: "blend" }, { text: "Linear", value: "linear" }, { text: "Mean", value: "mean" }, { text: "Phosphor", value: "phosphor" }]
        currentIndex: { for (var i = 0; i < model.length; ++i) if (model[i].value === Player.deinterlace) return i; return 0 }
        onActivated: Player.deinterlace = currentValue
    }
}
