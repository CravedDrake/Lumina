import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

// One library item. `listMode` switches between poster card and compact row.
Item {
    id: root
    property bool listMode: false
    property var ids: []                 // the whole view (play queue when this item is activated)
    property int itemIndex: 0
    // model roles (required properties are provided by the delegate)
    property var m: ({})
    property string contextView: ""      // view name; used for context-menu entries (e.g. history / playlist)
    signal activated()
    signal removeFromViewRequested()
    signal moveRequested(int delta)
    property real dragDy: 0
    z: dragDy !== 0 ? 10 : 0
    transform: Translate { y: root.dragDy }

    readonly property bool missing: !!m.missing
    readonly property real progress: m.progress || 0
    readonly property string subtitleLine: {
        var parts = []
        if (m.kind === "episode" && m.showName) parts.push(m.showName + "  S" + m.season + "E" + m.episode)
        else if (m.year > 0) parts.push(String(m.year))
        if (m.detail) parts.push(m.detail)
        return parts.join("  ·  ")
    }

    function play() {
        if (missing) { Actions.warn(qsTr("This file is missing from disk.")); return }
        Actions.playIds(ids.length ? ids : [m.mediaId], ids.length ? itemIndex : 0)
        activated()
    }

    HoverHandler { id: hh }
    TapHandler { acceptedButtons: Qt.LeftButton; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: root.play() }
    TapHandler { acceptedButtons: Qt.RightButton; onTapped: menu.popup() }
    TapHandler { acceptedButtons: Qt.LeftButton; onLongPressed: menu.popup() }

    Accessible.role: Accessible.Button
    Accessible.name: m.title || ""
    activeFocusOnTab: true
    Keys.onReturnPressed: play()
    Keys.onEnterPressed: play()
    Keys.onMenuPressed: menu.popup()

    // ------------------------------------------------------------- grid card
    Item {
        anchors.fill: parent
        visible: !root.listMode
        Rectangle {
            id: frame
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
            height: width * 9 / 16
            radius: Theme.radius
            color: Theme.surface2
            clip: true
            border.width: root.activeFocus ? 2 : 0
            border.color: Theme.accent
            Image {
                id: thumb
                anchors.fill: parent
                source: root.m.thumbnail || ""
                sourceSize.width: 480
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animMed } }
            }
            Icon {
                anchors.centerIn: parent
                visible: thumb.status !== Image.Ready
                name: root.m.isStream ? "globe" : "film"
                size: 34
                color: Theme.textMuted
            }
            Rectangle {   // hover scrim + play affordance
                anchors.fill: parent
                color: "#66000000"
                opacity: hh.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
                Rectangle {
                    anchors.centerIn: parent
                    width: 48; height: 48; radius: 24
                    color: Theme.accent
                    Icon { anchors.centerIn: parent; name: "play"; size: 22; color: Theme.textOnAccent }
                }
            }
            // badges
            Rectangle {
                visible: !!root.m.resolution
                anchors { left: parent.left; top: parent.top; margins: 8 }
                width: resText.implicitWidth + 12; height: 20; radius: 5
                color: "#B3000000"
                Text { id: resText; anchors.centerIn: parent; text: root.m.resolution || ""; color: Theme.text; font.pixelSize: 11; font.weight: Font.DemiBold; font.family: Theme.fontFamily }
            }
            Rectangle {
                visible: root.m.durationMs > 0
                anchors { right: parent.right; bottom: parent.bottom; margins: 8 }
                width: durText.implicitWidth + 12; height: 20; radius: 5
                color: "#B3000000"
                Text { id: durText; anchors.centerIn: parent; text: App.formatTime(root.m.durationMs || 0); color: Theme.text; font.pixelSize: 11; font.weight: Font.Medium; font.family: Theme.fontFamily }
            }
            IconButton {
                anchors { right: parent.right; top: parent.top; margins: 4 }
                iconName: "heart"
                iconSize: 18
                diameter: 30
                active: !!root.m.favorite
                activeColor: Theme.danger
                iconColor: "#FFFFFF"
                opacity: root.m.favorite || hh.hovered ? 1 : 0
                tip: root.m.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                onClicked: Library.setFavorite(root.m.mediaId, !root.m.favorite)
            }
            Rectangle {   // watch progress
                visible: root.progress > 0.005
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 4
                color: "#55FFFFFF"
                Rectangle { width: parent.width * root.progress; height: parent.height; color: root.m.completed ? Theme.success : Theme.accent }
            }
            Rectangle {
                visible: root.missing
                anchors.fill: parent
                color: "#CC0A0C11"
                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "warning"; size: 22; color: Theme.warning }
                    Text { text: qsTr("File missing"); color: Theme.warning; font.pixelSize: 12; font.family: Theme.fontFamily }
                }
            }
        }
        Column {
            anchors { left: parent.left; right: parent.right; top: frame.bottom; topMargin: 8; leftMargin: 8; rightMargin: 8 }
            spacing: 2
            Text {
                width: parent.width
                text: root.m.title || ""
                color: root.missing ? Theme.textMuted : Theme.text
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Medium
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: root.subtitleLine
                color: Theme.textMuted
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                elide: Text.ElideRight
            }
        }
    }

    // ------------------------------------------------------------- list row
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        visible: root.listMode
        radius: Theme.radiusSmall + 2
        color: hh.hovered ? Theme.hover : "transparent"
        border.width: root.activeFocus ? 2 : 0
        border.color: Theme.accent
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10; anchors.rightMargin: 10
            spacing: 14
            // drag handle (playlists only): drag vertically, drop to move by whole rows
            Item {
                id: grip
                visible: root.contextView === "playlist"
                Layout.preferredWidth: visible ? 18 : 0
                Layout.fillHeight: true
                Icon { anchors.centerIn: parent; name: "menu"; size: 16; color: gripArea.pressed ? Theme.accent : Theme.textDim }
                MouseArea {
                    id: gripArea
                    anchors.fill: parent
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real startY: 0
                    onPressed: function(e) { startY = mapToItem(root, e.x, e.y).y; root.dragDy = 0 }
                    onPositionChanged: function(e) { if (pressed) root.dragDy = mapToItem(root, e.x, e.y).y - startY }
                    onReleased: {
                        const delta = Math.round(root.dragDy / Math.max(1, root.height))
                        root.dragDy = 0
                        if (delta !== 0) root.moveRequested(delta)
                    }
                    onCanceled: root.dragDy = 0
                }
            }
            Rectangle {
                Layout.preferredWidth: height * 16 / 9
                Layout.preferredHeight: parent.height - 12
                radius: 6
                color: Theme.surface2
                clip: true
                Image {
                    anchors.fill: parent
                    source: root.listMode ? (root.m.thumbnail || "") : ""
                    sourceSize.width: 160
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
                Rectangle {
                    visible: root.progress > 0.005
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 3; color: "#55FFFFFF"
                    Rectangle { width: parent.width * root.progress; height: parent.height; color: Theme.accent }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    text: root.m.title || ""
                    color: root.missing ? Theme.textMuted : Theme.text
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: root.subtitleLine
                    color: Theme.textMuted
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                    elide: Text.ElideRight
                }
            }
            Text {
                visible: root.contextView === "history" && !!root.m.playedAt
                text: App.formatDate(root.m.playedAt || 0)
                color: Theme.textMuted
                font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
            }
            IconButton {
                iconName: "heart"; iconSize: 18; diameter: 32
                active: !!root.m.favorite; activeColor: Theme.danger
                tip: root.m.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                onClicked: Library.setFavorite(root.m.mediaId, !root.m.favorite)
            }
            IconButton { iconName: "more"; iconSize: 18; diameter: 32; tip: qsTr("More"); onClicked: menu.popup() }
        }
    }

    // ------------------------------------------------------------- context menu
    AppMenu {
        id: menu
        AppMenuItem { text: qsTr("Play"); iconName: "play"; onTriggered: root.play() }
        AppMenuItem {
            text: qsTr("Play next"); iconName: "next-up"
            onTriggered: { if (Player.active) Player.enqueueMediaIds([root.m.mediaId], true); else root.play() }
        }
        AppMenuItem {
            text: qsTr("Add to queue"); iconName: "queue"
            onTriggered: { if (Player.active) Player.enqueueMediaIds([root.m.mediaId], false); else root.play() }
        }
        AppMenuItem { text: qsTr("Add to playlist…"); iconName: "list"; onTriggered: Actions.playlistPickerRequested([root.m.mediaId]) }
        AppMenuItem {
            text: root.m.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites"); iconName: "heart"
            onTriggered: Library.setFavorite(root.m.mediaId, !root.m.favorite)
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }
        AppMenuItem { text: qsTr("Media information"); iconName: "info"; onTriggered: Actions.infoRequested(Actions.mediaInfoRows(root.m.mediaId), root.m.title) }
        AppMenuItem {
            visible: !root.m.isStream
            height: visible ? implicitHeight : 0
            text: qsTr("Show in folder"); iconName: "folder"
            onTriggered: App.revealInFileManager(root.m.path)
        }
        AppMenuItem { visible: !root.m.isStream; height: visible ? implicitHeight : 0; text: qsTr("Fix title / type…"); iconName: "edit"; onTriggered: Actions.classifyRequested(root.m.mediaId) }
        AppMenuItem {
            visible: root.contextView === "playlist" && root.itemIndex > 0
            height: visible ? implicitHeight : 0
            text: qsTr("Move up"); iconName: "chevron-up"
            onTriggered: root.moveRequested(-1)
        }
        AppMenuItem {
            visible: root.contextView === "playlist"
            height: visible ? implicitHeight : 0
            text: qsTr("Move down"); iconName: "chevron-down"
            onTriggered: root.moveRequested(1)
        }
        AppMenuItem {
            visible: root.contextView === "playlist" || root.contextView === "history"
            height: visible ? implicitHeight : 0
            text: root.contextView === "playlist" ? qsTr("Remove from playlist") : qsTr("Remove from history")
            iconName: "close"
            onTriggered: root.removeFromViewRequested()
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }
        AppMenuItem {
            text: qsTr("Remove from library"); iconName: "trash"; danger: true
            onTriggered: Actions.confirm(qsTr("Remove from library?"),
                                         qsTr("“%1” will be removed from Lumina's library. The file on disk is not touched.").arg(root.m.title),
                                         qsTr("Remove"), true, function() { Library.removeMedia(root.m.mediaId) })
        }
    }
}
