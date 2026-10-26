import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.Core
import Lumina.UI

// Titled horizontal shelf of media cards (Home page).
ColumnLayout {
    id: root
    property string view: "recent"
    property string title: ""
    property string seeAllPage: ""
    property int limit: 12
    property alias model: lib
    readonly property int count: lib.count
    visible: lib.count > 0
    spacing: 8

    LibraryModel { id: lib; view: root.view; limit: root.limit; active: root.visible || true }
    Connections { target: Library; function onDataChanged() { lib.refresh() } }

    RowLayout {
        Layout.fillWidth: true
        Text {
            text: root.title
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold
        }
        Item { Layout.fillWidth: true }
        AppButton {
            visible: root.seeAllPage !== ""
            kind: "ghost"; text: qsTr("See all"); iconName: ""
            onClicked: Actions.navigate(root.seeAllPage, null)
        }
    }
    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: 214
        orientation: ListView.Horizontal
        clip: true
        model: lib
        spacing: 4
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: MediaCard {
            width: 240; height: 200
            m: model
            ids: lib.allIds()
            itemIndex: index
            contextView: root.view
        }
    }
}
