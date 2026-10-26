import QtQuick
import Lumina.UI

SettingRow {
    id: root
    property string key: ""
    AppSwitch {
        checked: !!Settings.values[root.key]
        onToggled: Settings.set(root.key, checked)
        Accessible.name: root.label
    }
}
