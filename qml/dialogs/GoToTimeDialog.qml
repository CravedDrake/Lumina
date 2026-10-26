import QtQuick
import QtQuick.Layouts
import Lumina.UI

AppDialog {
    id: root
    title: qsTr("Go to time")
    dialogWidth: 360
    function go() {
        var ms = App.parseTime ? App.parseTime(field.text) : -1
        if (ms < 0) { err.text = qsTr("Use a time like 1:23:45, 12:30 or 90 (seconds)."); return }
        root.close(); Player.seekTo(ms)
    }
    onOpened: { field.text = App.formatTime(Player.positionMs); field.forceActiveFocus(); field.selectAll(); err.text = "" }
    AppTextField { id: field; Layout.fillWidth: true; onAccepted: root.go(); Accessible.name: qsTr("Time") }
    Text { id: err; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; visible: text !== "" }
    footer: [ Item { Layout.fillWidth: true }, AppButton { text: qsTr("Cancel"); onClicked: root.close() }, AppButton { kind: "primary"; text: qsTr("Go"); onClicked: root.go() } ]
}
