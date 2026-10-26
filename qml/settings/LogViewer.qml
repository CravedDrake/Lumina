import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

ColumnLayout {
    id: root
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        AppComboBox {
            Layout.preferredWidth: 150
            model: [qsTr("Debug and up"), qsTr("Info and up"), qsTr("Warnings"), qsTr("Errors only")]
            currentIndex: AppLog.minLevel
            onActivated: AppLog.minLevel = currentIndex
        }
        Item { Layout.fillWidth: true }
        AppButton { iconName: "copy"; text: qsTr("Copy"); onClicked: { App.copyText(AppLog.asText()); Actions.success(qsTr("Log copied.")) } }
        AppButton { iconName: "folder"; text: qsTr("Open log folder"); onClicked: App.openFolderInFileManager(AppLog.logFilePath.replace(/[\\/][^\\/]*$/, "")) }
        AppButton { iconName: "trash"; text: qsTr("Clear"); onClicked: AppLog.clear() }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 260
        radius: Theme.radiusSmall + 2
        color: Theme.surface
        border.color: Theme.border
        ListView {
            id: view
            anchors.fill: parent; anchors.margins: 8
            clip: true
            model: AppLog
            onCountChanged: if (atYEnd || count < 3) positionViewAtEnd()
            ScrollBar.vertical: ScrollBar {}
            delegate: Text {
                text: model.text
                width: ListView.view.width
                wrapMode: Text.WrapAnywhere
                color: model.level >= 3 ? Theme.danger : model.level === 2 ? Theme.warning : Theme.textDim
                font.family: "monospace"; font.pixelSize: 11
            }
        }
    }
}
