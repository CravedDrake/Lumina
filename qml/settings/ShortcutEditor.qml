import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

// Lists every action with its binding; click a binding to record a new one. Conflicts are rejected with a message.
ColumnLayout {
    id: root
    spacing: 8
    property var actions: Shortcuts.actionList()
    property string capturing: ""
    property string notice: ""
    Connections { target: Shortcuts; function onBindingsChanged() { root.actions = Shortcuts.actionList() } }

    RowLayout {
        Layout.fillWidth: true
        AppTextField { id: filter; Layout.preferredWidth: 280; icon: "search"; placeholderText: qsTr("Filter shortcuts") }
        Item { Layout.fillWidth: true }
        AppButton { kind: "danger"; iconName: "rotate"; text: qsTr("Reset all"); onClicked: { Shortcuts.resetAll(); root.notice = qsTr("All shortcuts reset to defaults.") } }
    }
    Text {
        visible: root.notice !== ""
        text: root.notice
        color: Theme.warning
        font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }

    Repeater {
        model: root.actions
        delegate: Item {
            required property var modelData
            readonly property bool show: filter.text === "" || (modelData.title + " " + modelData.category + " " + modelData.keys.join(" ")).toLowerCase().indexOf(filter.text.toLowerCase()) >= 0
            visible: show
            Layout.fillWidth: true
            Layout.preferredHeight: show ? 40 : 0
            RowLayout {
                anchors.fill: parent
                spacing: 12
                Text { Layout.preferredWidth: 70; text: modelData.category; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                Text { Layout.fillWidth: true; text: modelData.title; color: Theme.text; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
                Rectangle {
                    id: keyBox
                    Layout.preferredWidth: 170; Layout.preferredHeight: 30
                    radius: 6
                    color: root.capturing === modelData.id ? Theme.accentSoft : Theme.surface2
                    border.width: root.capturing === modelData.id ? 2 : 1
                    border.color: root.capturing === modelData.id ? Theme.accent : modelData.customized ? Theme.accent : Theme.border
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("%1 shortcut: %2").arg(modelData.title).arg(modelData.keys.join(", "))
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 12
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: root.capturing === modelData.id ? qsTr("Press keys…  (Esc cancels)") : (modelData.keys.length ? modelData.keys.join("  ·  ") : qsTr("Unassigned"))
                        color: root.capturing === modelData.id ? Theme.accent : modelData.keys.length ? Theme.text : Theme.textMuted
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Medium
                    }
                    MouseArea { anchors.fill: parent; onClicked: { root.notice = ""; root.capturing = modelData.id; keyBox.forceActiveFocus() } }
                    Keys.onPressed: function(e) {
                        if (root.capturing !== modelData.id) return
                        if (e.key === Qt.Key_Shift || e.key === Qt.Key_Control || e.key === Qt.Key_Alt || e.key === Qt.Key_Meta) { e.accepted = true; return }
                        e.accepted = true
                        if (e.key === Qt.Key_Escape && e.modifiers === Qt.NoModifier) { root.capturing = ""; return }
                        var seq = Shortcuts.sequenceFromKey(e.key, e.modifiers)
                        var err = seq === "" ? qsTr("That key cannot be used.") : Shortcuts.setBinding(modelData.id, seq)
                        root.notice = err
                        root.capturing = ""
                    }
                    onActiveFocusChanged: if (!activeFocus && root.capturing === modelData.id) root.capturing = ""
                }
                IconButton { iconName: "rotate"; diameter: 30; iconSize: 15; visible: modelData.customized; tip: qsTr("Reset to default"); onClicked: Shortcuts.resetBinding(modelData.id) }
                Item { Layout.preferredWidth: 30; visible: !modelData.customized }
            }
        }
    }
}
