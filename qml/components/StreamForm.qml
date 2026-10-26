import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

// URL (+ optional credentials) form. Credentials are passed to the player for this session only:
// they are never written to settings, the library or the log.
ColumnLayout {
    id: root
    signal opened()
    spacing: 10

    function submit() {
        var url = urlField.text.trim()
        if (url === "") { Actions.warn(qsTr("Enter a stream address first.")); return }
        if (url.indexOf("://") < 0) { Actions.warn(qsTr("The address needs a scheme such as http://, https://, rtsp:// or udp://")); return }
        var user = credsToggle.checked ? userField.text : ""
        var pass = credsToggle.checked ? passField.text : ""
        if (Player.openStream(url, user, pass)) {
            App.rememberStream(url)
            passField.text = ""
            Actions.showPlayer()
            root.opened()
        }
    }
    function clear() { urlField.text = ""; userField.text = ""; passField.text = "" }

    AppTextField {
        id: urlField
        Layout.fillWidth: true
        icon: "link"
        placeholderText: qsTr("https://…  rtsp://…  udp://@239.0.0.1:1234  …")
        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
        onAccepted: root.submit()
        Accessible.name: qsTr("Stream address")
    }
    RowLayout {
        spacing: 10
        AppSwitch { id: credsToggle }
        Text { text: qsTr("Use credentials for this session"); color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
    }
    RowLayout {
        visible: credsToggle.checked
        Layout.fillWidth: true
        spacing: 10
        AppTextField { id: userField; Layout.fillWidth: true; placeholderText: qsTr("Username"); Accessible.name: qsTr("Username") }
        AppTextField { id: passField; Layout.fillWidth: true; placeholderText: qsTr("Password"); echoMode: TextInput.Password; onAccepted: root.submit(); Accessible.name: qsTr("Password") }
    }
    Text {
        visible: credsToggle.checked
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("Credentials are used for this session only. They are not saved and are removed from logs.")
        color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
    }
    RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        AppButton { kind: "primary"; iconName: "play"; text: qsTr("Play"); onClicked: root.submit() }
    }
    property alias urlText: urlField.text
}
