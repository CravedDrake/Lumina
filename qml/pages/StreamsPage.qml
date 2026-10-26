import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Item {
    id: root
    property var recents: App.recentStreams()
    Connections { target: Settings; function onValuesChanged() { root.recents = App.recentStreams() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Icon { name: "globe"; size: 26; color: Theme.accent }
            Text { text: qsTr("Network streams"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsHeading; font.weight: Font.Bold }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 720
            implicitHeight: form.implicitHeight + 32
            radius: Theme.radius
            color: Theme.surface2
            border.color: Theme.border
            StreamForm { id: form; anchors { fill: parent; margins: 16 } }
        }

        Text {
            visible: root.recents.length > 0
            text: qsTr("Recent addresses")
            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold
        }
        ListView {
            Layout.fillWidth: true
            Layout.maximumWidth: 720
            Layout.preferredHeight: Math.min(contentHeight, 220)
            visible: root.recents.length > 0
            clip: true
            model: root.recents
            spacing: 4
            delegate: Rectangle {
                required property string modelData
                width: ListView.view.width; height: 40; radius: 8
                color: rma.containsMouse ? Theme.hover : Theme.surface2
                MouseArea { id: rma; anchors.fill: parent; hoverEnabled: true; onClicked: form.urlText = modelData }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 6
                    Icon { name: "globe"; size: 16; color: Theme.textMuted }
                    Text { Layout.fillWidth: true; text: App.safeUrl(modelData); color: Theme.textDim; elide: Text.ElideMiddle; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
                    IconButton { iconName: "play"; diameter: 30; iconSize: 16; tip: qsTr("Play"); onClicked: { form.urlText = modelData; form.submit() } }
                }
            }
        }
        AppButton {
            visible: root.recents.length > 0
            kind: "ghost"; iconName: "trash"; text: qsTr("Clear recent addresses")
            onClicked: App.forgetStreams()
        }
        Item { Layout.fillHeight: true }
    }
}
