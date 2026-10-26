import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

// Right-hand panel chrome: title, close button and a scrollable content area.
Rectangle {
    id: root
    property string title: ""
    property string icon: ""
    default property alias content: col.data
    property alias headerExtra: extra.data
    signal closeRequested()
    color: "#FC0E1118"
    border.color: Theme.border
    border.width: 1
    // swallow mouse so clicks never reach the video underneath
    MouseArea { anchors.fill: parent; hoverEnabled: true; onWheel: function(w) { w.accepted = true } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Layout.leftMargin: 16; Layout.rightMargin: 8
            Icon { name: root.icon; size: 18; color: Theme.accent; visible: root.icon !== "" }
            Text { text: root.title; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody + 1; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Row { id: extra; spacing: 4 }
            IconButton { iconName: "close"; iconSize: 18; diameter: 32; tip: qsTr("Close panel"); onClicked: root.closeRequested() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: col.implicitHeight + 24
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            ColumnLayout {
                id: col
                x: 16; y: 12
                width: flick.width - 32
                spacing: 10
            }
        }
    }
}
