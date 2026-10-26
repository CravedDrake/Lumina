import QtQuick
import Lumina.UI

SettingRow {
    id: root
    property string key: ""
    property real from: 0
    property real to: 100
    property real step: 1
    property string suffix: ""
    property int decimals: 0
    AppSlider {
        id: s
        width: 200
        from: root.from; to: root.to; stepSize: root.step
        value: Number(Settings.values[root.key])
        onMoved: Settings.set(root.key, root.decimals > 0 ? value : Math.round(value))
        Accessible.name: root.label
    }
    Text {
        width: 64
        text: Number(Settings.values[root.key]).toFixed(root.decimals) + root.suffix
        color: Theme.textDim
        font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
    }
}
