import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

AppDialog {
    id: root
    title: qsTr("Keyboard shortcuts")
    dialogWidth: 620
    property var actions: []
    onAboutToShow: actions = Shortcuts.actionList()
    ListView {
        Layout.fillWidth: true; Layout.preferredHeight: 420
        clip: true; model: root.actions
        ScrollBar.vertical: ScrollBar {}
        section.property: "category"
        section.delegate: Text { required property string section; text: section; topPadding: 12; bottomPadding: 4; color: Theme.accent; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
        delegate: RowLayout {
            required property var modelData
            width: ListView.view.width - 12; height: 30
            Text { Layout.fillWidth: true; text: modelData.title; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
            Text { text: modelData.keys.length ? modelData.keys.join("   ") : "—"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
        }
    }
    footer: [
        AppButton { iconName: "settings"; text: qsTr("Customise…"); onClicked: { root.close(); Actions.hidePlayer(); Actions.navigate("settings", 5) } },
        Item { Layout.fillWidth: true },
        AppButton { kind: "primary"; text: qsTr("Close"); onClicked: root.close() }
    ]
}
