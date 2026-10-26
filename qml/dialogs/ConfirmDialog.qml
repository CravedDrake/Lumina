import QtQuick
import QtQuick.Layouts
import Lumina.UI

AppDialog {
    id: root
    property string message: ""
    property string confirmLabel: qsTr("OK")
    property bool danger: false
    property var callback: null
    function ask(title, msg, label, isDanger, cb) { root.title = title; message = msg; confirmLabel = label || qsTr("OK"); danger = isDanger; callback = cb; open() }
    Text { Layout.fillWidth: true; text: root.message; wrapMode: Text.WordWrap; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
    footer: [
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Cancel"); onClicked: root.close() },
        AppButton { kind: root.danger ? "danger" : "primary"; text: root.confirmLabel; onClicked: { root.close(); if (root.callback) root.callback() } }
    ]
}
