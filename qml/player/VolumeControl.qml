import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Item {
    id: root
    implicitHeight: 40
    implicitWidth: expanded ? 36 + 110 : 36
    readonly property bool expanded: hh.hovered || slider.pressed || slider.activeFocus || btn.activeFocus
    Behavior on implicitWidth { NumberAnimation { duration: Theme.animFast } }
    HoverHandler { id: hh }
    IconButton {
        id: btn
        iconName: Player.muted || Player.volume === 0 ? "volume-mute" : Player.volume < 40 ? "volume-low" : "volume"
        tip: Player.muted ? qsTr("Unmute") : qsTr("Mute")
        iconColor: "#FFFFFF"
        onClicked: Player.toggleMute()
    }
    AppSlider {
        id: slider
        anchors { left: btn.right; leftMargin: 2; verticalCenter: parent.verticalCenter }
        width: 96
        visible: root.expanded
        from: 0; to: 100; stepSize: 1
        value: Player.muted ? 0 : Player.volume
        onMoved: { Player.muted = false; Player.volume = Math.round(value) }
        Accessible.name: qsTr("Volume")
    }
}
