import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Lumina.UI

// Custom frameless title bar: drag to move, double-click to maximise, window buttons.
Rectangle {
    id: root
    property var win: null
    signal menuRequested()
    height: Theme.titleBarHeight
    color: Theme.surface
    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.border }

    DragHandler {
        target: null
        onActiveChanged: if (active && root.win) root.win.startSystemMove()
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: root.toggleMax()
    }
    function toggleMax() {
        if (!win) return
        win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        spacing: 10
        Icon { name: "logo"; size: 22; color: Theme.accent }
        Text { text: "Lumina"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold }
        Text {
            visible: Player.active && !Actions.playerVisible
            text: "·  " + Player.title
            color: Theme.textMuted; elide: Text.ElideRight
            font.family: Theme.fontFamily; font.pixelSize: 13
            Layout.maximumWidth: 400
        }
        Item { Layout.fillWidth: true }
        IconButton { iconName: "pin"; diameter: 30; iconSize: 15; round: false; active: Actions.alwaysOnTop; tip: qsTr("Always on top"); onClicked: Actions.alwaysOnTopToggled() }
        Repeater {
            model: [
                { icon: "minus", act: "min", tip: qsTr("Minimise") },
                { icon: root.win && root.win.visibility === Window.Maximized ? "restore" : "maximize", act: "max", tip: qsTr("Maximise") },
                { icon: "close", act: "close", tip: qsTr("Close") }]
            delegate: Rectangle {
                required property var modelData
                Layout.preferredWidth: 46; Layout.fillHeight: true
                color: wma.containsMouse ? (modelData.act === "close" ? "#E5484D" : Theme.hover) : "transparent"
                activeFocusOnTab: true
                Keys.onReturnPressed: wma.clicked(null)
                Accessible.role: Accessible.Button; Accessible.name: modelData.tip
                MouseArea {
                    id: wma; anchors.fill: parent; hoverEnabled: true
                    onClicked: {
                        if (!root.win) return
                        if (modelData.act === "min") root.win.showMinimized()
                        else if (modelData.act === "max") root.toggleMax()
                        else root.win.close()
                    }
                }
                Icon { anchors.centerIn: parent; name: modelData.icon; size: 15; color: wma.containsMouse && modelData.act === "close" ? "#FFFFFF" : Theme.text }
                ToolTip.visible: wma.containsMouse; ToolTip.delay: 600; ToolTip.text: modelData.tip
            }
        }
    }
}
