import QtQuick
import QtQuick.Layouts
import Lumina.UI

AppDialog {
    id: root
    title: qsTr("Open network stream")
    dialogWidth: 560
    onOpened: form.clear()
    StreamForm { id: form; Layout.fillWidth: true; onOpened: root.close() }
    footer: [ Item { Layout.fillWidth: true }, AppButton { text: qsTr("Cancel"); onClicked: root.close() } ]
}
