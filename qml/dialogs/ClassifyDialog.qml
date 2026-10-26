import QtQuick
import QtQuick.Layouts
import Lumina.UI

// Manual correction of how a file is classified (movie / TV episode / other video).
AppDialog {
    id: root
    title: qsTr("Fix title and type")
    dialogWidth: 500
    property var mediaId: 0
    function edit(id) {
        mediaId = id
        var m = Library.mediaInfo(id)
        kindBox.currentIndex = m.kind === "episode" ? 1 : m.kind === "video" ? 2 : 0
        titleField.text = m.title || ""
        showField.text = m.showName || ""
        seasonBox.value = m.season || 1
        episodeBox.value = m.episode || 1
        open()
    }
    AppComboBox { id: kindBox; Layout.fillWidth: true; model: [qsTr("Movie"), qsTr("TV episode"), qsTr("Other video")] }
    AppTextField { id: titleField; Layout.fillWidth: true; placeholderText: qsTr("Title") }
    AppTextField { id: showField; Layout.fillWidth: true; visible: kindBox.currentIndex === 1; placeholderText: qsTr("Show name") }
    RowLayout {
        visible: kindBox.currentIndex === 1; spacing: 10
        Text { text: qsTr("Season"); color: Theme.textDim; font.family: Theme.fontFamily }
        SpinBoxLite { id: seasonBox }
        Text { text: qsTr("Episode"); color: Theme.textDim; font.family: Theme.fontFamily }
        SpinBoxLite { id: episodeBox }
    }
    footer: [
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Cancel"); onClicked: root.close() },
        AppButton {
            kind: "primary"; text: qsTr("Save")
            onClicked: {
                var kinds = ["movie", "episode", "video"]
                Library.correctClassification(root.mediaId, kinds[kindBox.currentIndex], titleField.text.trim(), showField.text.trim(), seasonBox.value, episodeBox.value)
                root.close()
            }
        }
    ]
}
