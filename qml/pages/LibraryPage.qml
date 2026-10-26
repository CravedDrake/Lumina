import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.Core
import Lumina.UI

// Generic media browser used by Movies / TV / All / Recent / Continue / Favorites / History / Streams / one playlist.
Item {
    id: root
    property string view: "all"
    property var viewArg: 0
    property string title: ""
    property string icon: "film"
    property string emptyTitle: qsTr("Nothing here yet")
    property string emptySubtitle: ""
    property bool sortable: true
    property bool grouped: false                // TV: list with a header per show
    property alias search: searchField.text
    property alias model: lib
    property alias headerActions: extraActions.data
    readonly property bool listMode: grouped || view === "playlist" || Settings.values["appearance/viewMode"] === "list"
    readonly property string contextView: view

    function focusSearch() { searchField.forceActiveFocus(); searchField.selectAll() }

    LibraryModel {
        id: lib
        view: root.view
        viewArg: root.viewArg
        search: searchField.text
        sortKey: root.sortable ? sortCombo.currentValue : ""
        sortDescending: descButton.active
        active: root.visible
    }
    Connections { target: Library; function onDataChanged() { lib.refresh() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28; anchors.rightMargin: 28; anchors.topMargin: 22
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: root.icon; size: 26; color: Theme.accent }
            Text {
                text: root.title
                color: Theme.text
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsHeading; font.weight: Font.Bold
            }
            Text {
                text: lib.count > 0 ? qsTr("%n item(s)", "", lib.count) : ""
                color: Theme.textMuted
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 4
            }
            Item { Layout.fillWidth: true }
            Row { id: extraActions; spacing: 8 }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            AppTextField {
                id: searchField
                Layout.preferredWidth: 300
                icon: "search"
                placeholderText: qsTr("Search %1").arg(root.title.toLowerCase())
                Keys.onEscapePressed: { text = ""; focus = false }
                Accessible.name: qsTr("Search")
            }
            Item { Layout.fillWidth: true }
            AppComboBox {
                id: sortCombo
                visible: root.sortable
                Layout.preferredWidth: 170
                textRole: "text"; valueRole: "value"
                model: [
                    { text: qsTr("Default order"), value: "" }, { text: qsTr("Title"), value: "title" },
                    { text: qsTr("Date added"), value: "added" }, { text: qsTr("Last played"), value: "played" },
                    { text: qsTr("Duration"), value: "duration" }, { text: qsTr("File size"), value: "size" },
                    { text: qsTr("Resolution"), value: "resolution" }]
                Accessible.name: qsTr("Sort by")
            }
            IconButton {
                id: descButton
                visible: root.sortable
                property bool active: false
                iconName: active ? "move-down" : "move-up"
                tip: active ? qsTr("Descending") : qsTr("Ascending")
                iconSize: 18
                onClicked: active = !active
            }
            IconButton {
                visible: !root.grouped
                iconName: "grid"; iconSize: 18; tip: qsTr("Grid view")
                active: !root.listMode
                onClicked: Settings.set("appearance/viewMode", "grid")
            }
            IconButton {
                visible: !root.grouped
                iconName: "list"; iconSize: 18; tip: qsTr("List view")
                active: root.listMode
                onClicked: Settings.set("appearance/viewMode", "list")
            }
            AppButton {
                visible: lib.count > 0 && root.view !== "streams"
                kind: "primary"; iconName: "play"; text: qsTr("Play all")
                onClicked: Actions.playIds(lib.allIds(), 0)
            }
            AppButton {
                visible: lib.count > 1 && root.view !== "streams"
                kind: "secondary"; iconName: "shuffle"; text: qsTr("Shuffle")
                onClicked: { Player.shuffle = true; Actions.playIds(lib.allIds(), Math.floor(Math.random() * lib.count)) }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ---- grid
            GridView {
                id: grid
                anchors.fill: parent
                visible: !root.listMode && lib.count > 0
                clip: true
                readonly property int columns: Math.max(1, Math.floor(width / 232))
                cellWidth: Math.floor(width / columns)
                cellHeight: Math.round(cellWidth * 9 / 16) + 12 + 54
                model: lib
                cacheBuffer: cellHeight * 2
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: MediaCard {
                    width: grid.cellWidth; height: grid.cellHeight
                    m: model
                    ids: lib.allIds()
                    itemIndex: index
                    contextView: root.contextView
                    onRemoveFromViewRequested: root.removeFromView(model)
                    onMoveRequested: function(d) { Library.moveInPlaylist(root.viewArg, index, index + d) }
                }
            }

            // ---- list
            ListView {
                id: list
                anchors.fill: parent
                visible: root.listMode && lib.count > 0
                clip: true
                model: lib
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                section.property: root.grouped ? "showName" : ""
                section.criteria: ViewSection.FullString
                section.delegate: Item {
                    required property string section
                    width: list.width; height: 44
                    Text {
                        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 6; bottomMargin: 6 }
                        text: parent.section || qsTr("Unsorted")
                        color: Theme.accent
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold
                    }
                }
                delegate: MediaCard {
                    width: list.width; height: 66
                    listMode: true
                    m: model
                    ids: lib.allIds()
                    itemIndex: index
                    contextView: root.contextView
                    onRemoveFromViewRequested: root.removeFromView(model)
                    onMoveRequested: function(d) { Library.moveInPlaylist(root.viewArg, index, index + d) }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: lib.count === 0 && !lib.loading
                icon: searchField.text !== "" ? "search" : root.icon
                title: searchField.text !== "" ? qsTr("No matches") : root.emptyTitle
                subtitle: searchField.text !== "" ? qsTr("Nothing matches “%1”.").arg(searchField.text) : root.emptySubtitle
                actions: [
                    AppButton {
                        visible: searchField.text === "" && Library.folders.length === 0 && root.view !== "streams" && root.view !== "history"
                        kind: "primary"; iconName: "folder-plus"; text: qsTr("Add a folder")
                        onClicked: Actions.addLibraryFolderRequested()
                    }
                ]
            }
            Spinner { anchors.centerIn: parent; running: lib.loading && lib.count === 0; visible: running }
        }
    }

    function removeFromView(row) {
        if (root.view === "playlist") Library.removeFromPlaylist(row.playlistItemId)
        else if (root.view === "history") Library.removeFromHistory(row.mediaId)
    }
}
