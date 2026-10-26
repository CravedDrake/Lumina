import QtQuick
import Lumina.UI

SettingRow {
    id: root
    property string key: ""
    property var choices: []          // [{text, value}]
    property int comboWidth: 200
    AppComboBox {
        width: root.comboWidth
        model: root.choices
        textRole: "text"; valueRole: "value"
        currentIndex: {
            var v = Settings.values[root.key]
            for (var i = 0; i < root.choices.length; ++i) if (root.choices[i].value === v) return i
            return -1
        }
        onActivated: Settings.set(root.key, root.choices[currentIndex].value)
        Accessible.name: root.label
    }
}
