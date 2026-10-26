import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Rectangle {
    id: root
    property bool collapsed: false
    readonly property var items: [
        { id: "home", text: qsTr("Home"), icon: "home" },
        { id: "movies", text: qsTr("Movies"), icon: "film" },
        { id: "tv", text: qsTr("TV Shows"), icon: "tv" },
        { id: "all", text: qsTr("All Videos"), icon: "grid" },
        { id: "recent", text: qsTr("Recently Played"), icon: "clock" },
        { id: "continue", text: qsTr("Continue Watching"), icon: "play-circle" },
        { id: "favorites", text: qsTr("Favorites"), icon: "heart" },
        { id: "playlists", text: qsTr("Playlists"), icon: "list" },
        { id: "history", text: qsTr("History"), icon: "history" },
        { id: "streams", text: qsTr("Network Streams"), icon: "globe" } ]
    implicitWidth: collapsed ? Theme.sidebarCollapsedWidth : Theme.sidebarWidth
    color: Theme.surface
    clip: true
    Behavior on implicitWidth { NumberAnimation { duration: Theme.animMed; easing.type: Easing.OutCubic } }
    Rectangle { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: 1; color: Theme.border }

    component NavItem: Rectangle {
        id: item
        property string pageId: ""
        property string label: ""
        property string icon: ""
        property bool selected: Actions.page === pageId
        width: parent ? parent.width : 0
        height: 40
        radius: 10
        color: selected ? Theme.accentSoft : ma.containsMouse ? Theme.hover : "transparent"
        activeFocusOnTab: true
        Keys.onReturnPressed: Actions.navigate(pageId, null)
        Keys.onSpacePressed: Actions.navigate(pageId, null)
        border.width: activeFocus ? 2 : 0; border.color: Theme.accent
        Accessible.role: Accessible.Button; Accessible.name: label; Accessible.selected: selected
        Rectangle { visible: item.selected; width: 3; height: 18; radius: 2; color: Theme.accent; anchors { left: parent.left; verticalCenter: parent.verticalCenter } }
        MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onClicked: Actions.navigate(item.pageId, null) }
        Icon { x: 20 - width / 2 + 6; anchors.verticalCenter: parent.verticalCenter; name: item.icon; size: 20; color: item.selected ? Theme.accent : Theme.textDim }
        Text {
            x: 48; anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 56
            text: item.label; elide: Text.ElideRight
            color: item.selected ? Theme.text : Theme.textDim
            font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: item.selected ? Font.DemiBold : Font.Normal
            opacity: root.collapsed ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        }
        ToolTip.visible: root.collapsed && ma.containsMouse; ToolTip.delay: 400; ToolTip.text: label
    }

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            IconButton { iconName: "menu"; tip: qsTr("Toggle sidebar (Ctrl+B)"); onClicked: Settings.set("appearance/sidebarCollapsed", !Settings.values["appearance/sidebarCollapsed"]) }
        }
        Repeater {
            model: root.items
            delegate: NavItem { required property var modelData; Layout.fillWidth: true; pageId: modelData.id; label: modelData.text; icon: modelData.icon }
        }
        Item { Layout.fillHeight: true }
        // scan status
        Rectangle {
            Layout.fillWidth: true
            visible: Library.scanning && !root.collapsed
            implicitHeight: 54; radius: 10; color: Theme.surface2
            ColumnLayout {
                anchors { fill: parent; margins: 10 }
                spacing: 4
                RowLayout {
                    Spinner { size: 14 }
                    Text { text: qsTr("Scanning library"); color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: 12 }
                }
                Rectangle {
                    Layout.fillWidth: true; height: 4; radius: 2; color: Theme.elevated
                    Rectangle { width: Library.scanTotal > 0 ? parent.width * Library.scanDone / Library.scanTotal : 0; height: parent.height; radius: 2; color: Theme.accent }
                }
            }
        }
        NavItem { Layout.fillWidth: true; pageId: "settings"; label: qsTr("Settings"); icon: "settings" }
    }
}
