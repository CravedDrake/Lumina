import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lumina.UI

Popup {
    id: root
    property string title: ""
    property int dialogWidth: 440
    default property alias body: bodyCol.data
    property alias footer: footerRow.data
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(dialogWidth, (parent ? parent.width : 800) - 40)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animMed } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animFast } }
    Overlay.modal: Rectangle { color: "#99000000" }
    background: Rectangle { color: Theme.elevated; radius: Theme.radiusLarge; border.color: Theme.borderStrong; border.width: 1 }
    contentItem: ColumnLayout {
        spacing: 0
        Text {
            Layout.margins: 22; Layout.bottomMargin: 8
            text: root.title; visible: text !== ""
            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        ColumnLayout { id: bodyCol; Layout.fillWidth: true; Layout.leftMargin: 22; Layout.rightMargin: 22; spacing: 10 }
        RowLayout {
            id: footerRow
            Layout.fillWidth: true; Layout.margins: 22; Layout.topMargin: 16
            spacing: 10
        }
    }
}
