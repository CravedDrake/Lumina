import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Flickable {
    id: root
    contentWidth: width
    contentHeight: col.implicitHeight + 60
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: col
        x: 28; y: 22
        width: root.width - 56
        spacing: 22

        // ---- greeting / quick actions
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            radius: Theme.radiusLarge
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Theme.alpha(Theme.accent, 0.38) }
                GradientStop { position: 1; color: Theme.surface2 }
            }
            border.width: 1
            border.color: Theme.border
            ColumnLayout {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 28 }
                spacing: 6
                Text {
                    text: qsTr("Welcome to Lumina")
                    color: Theme.text
                    font.family: Theme.fontFamily; font.pixelSize: 28; font.weight: Font.Bold
                }
                Text {
                    text: Library.mediaCount > 0 ? qsTr("%n video(s) in your library", "", Library.mediaCount)
                                                 : qsTr("Add a folder to build your library, or just open a file to play it.")
                    color: Theme.textDim
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
                }
                Row {
                    Layout.topMargin: 10
                    spacing: 10
                    AppButton { kind: "primary"; iconName: "file-video"; text: qsTr("Open file…"); onClicked: Actions.openFilesRequested() }
                    AppButton { iconName: "folder"; text: qsTr("Open folder…"); onClicked: Actions.openFolderRequested() }
                    AppButton { iconName: "link"; text: qsTr("Network stream…"); onClicked: Actions.openStreamRequested() }
                    AppButton { iconName: "folder-plus"; text: qsTr("Add library folder…"); onClicked: Actions.addLibraryFolderRequested() }
                }
            }
            Row {
                visible: Library.scanning
                anchors { right: parent.right; bottom: parent.bottom; margins: 16 }
                spacing: 8
                Spinner { size: 18; running: Library.scanning }
                Text {
                    text: Library.scanPhase + (Library.scanTotal > 0 ? "  " + Library.scanDone + " / " + Library.scanTotal : "")
                    color: Theme.textDim
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        MediaRow { Layout.fillWidth: true; view: "continue"; title: qsTr("Continue watching"); seeAllPage: "continue" }
        MediaRow { Layout.fillWidth: true; view: "recent"; title: qsTr("Recently played"); seeAllPage: "recent" }
        MediaRow { Layout.fillWidth: true; view: "added"; title: qsTr("Recently added"); seeAllPage: "all" }
        MediaRow { Layout.fillWidth: true; view: "favorites"; title: qsTr("Favorites"); seeAllPage: "favorites" }

        EmptyState {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 30
            visible: Library.mediaCount === 0
            icon: "film"
            title: qsTr("Your library is empty")
            subtitle: qsTr("Lumina scans the folders you add in the background and never moves or modifies your files.")
        }
    }
}
