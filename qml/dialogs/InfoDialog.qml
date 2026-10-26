import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

// Media information: rows are {label,value} or {section,label,value}.
AppDialog {
    id: root
    dialogWidth: 640
    property var rows: []
    function show(r, t) { rows = r; title = t ? qsTr("Media information — %1").arg(t) : qsTr("Media information"); open() }
    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 440)
        contentHeight: infoCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: infoCol
            width: parent.width - 12
            spacing: 0
            Repeater {
                model: root.rows
                delegate: ColumnLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: 0
                    readonly property bool newSection: !!modelData.section && (index === 0 || root.rows[index - 1].section !== modelData.section)
                    Text { visible: parent.newSection; text: modelData.section || ""; topPadding: 12; bottomPadding: 4; color: Theme.accent; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 16
                        Text { Layout.preferredWidth: 150; Layout.alignment: Qt.AlignTop; text: modelData.label; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; topPadding: 3 }
                        Text { Layout.fillWidth: true; text: modelData.value; color: Theme.text; wrapMode: Text.WrapAnywhere; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; topPadding: 3; bottomPadding: 3 }
                    }
                }
            }
        }
    }
    footer: [
        AppButton { iconName: "copy"; text: qsTr("Copy"); onClicked: { var t = []; for (var i = 0; i < root.rows.length; ++i) t.push(root.rows[i].label + ": " + root.rows[i].value); App.copyText(t.join("\n")); Actions.success(qsTr("Copied.")) } },
        Item { Layout.fillWidth: true },
        AppButton { kind: "primary"; text: qsTr("Close"); onClicked: root.close() }
    ]
}
