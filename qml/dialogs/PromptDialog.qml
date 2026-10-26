import QtQuick
import QtQuick.Layouts
import Lumina.UI

AppDialog {
    id: root
    property string label: ""
    property string okLabel: qsTr("OK")
    property var callback: null
    function ask(title, lbl, initial, ok, cb) { root.title = title; label = lbl; field.text = initial; okLabel = ok; callback = cb; open(); field.forceActiveFocus(); field.selectAll() }
    function accept() { root.close(); if (callback) callback(field.text) }
    onOpened: { field.forceActiveFocus(); field.selectAll() }
    Text { text: root.label; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
    AppTextField { id: field; Layout.fillWidth: true; onAccepted: root.accept(); Accessible.name: root.label }
    footer: [
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Cancel"); onClicked: root.close() },
        AppButton { kind: "primary"; text: root.okLabel; onClicked: root.accept() }
    ]
}
