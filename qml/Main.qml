import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import Lumina.Core
import Lumina.UI

ApplicationWindow {
    id: win
    readonly property var saved: Settings.windowState()
    width: saved.width ? Math.max(saved.width, 900) : 1280
    height: saved.height ? Math.max(saved.height, 560) : 780
    x: saved.x !== undefined ? saved.x : Screen.width / 2 - width / 2
    y: saved.y !== undefined ? saved.y : Screen.height / 2 - height / 2
    minimumWidth: 860
    minimumHeight: 520
    visible: true
    title: Player.active ? Player.title + " — Lumina" : "Lumina"
    color: Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint | (Actions.alwaysOnTop ? Qt.WindowStaysOnTopHint : 0)
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsBody

    property int visibilityBeforeFullscreen: Window.Windowed
    readonly property string debugPanel: playerView.panel          // read by --ui-selftest
    readonly property bool debugPlayerVisible: Actions.playerVisible

    // ---- theme from settings
    Binding { target: Theme; property: "accent"; value: Settings.values["appearance/accent"] }
    Binding { target: Theme; property: "compact"; value: Settings.values["appearance/density"] === "compact" }
    Binding { target: Theme; property: "animations"; value: !!Settings.values["appearance/animations"] }
    Binding { target: Shortcuts; property: "playerActive"; value: Actions.playerVisible }
    Binding { target: Shortcuts; property: "blocked"; value: anyDialogOpen }
    readonly property bool anyDialogOpen: confirmDialog.visible || promptDialog.visible || infoDialog.visible || streamDialog.visible
                                          || pickerDialog.visible || gotoDialog.visible || classifyDialog.visible || helpDialog.visible

    Component.onCompleted: {
        if (saved.maximized) showMaximized()
        if (StartFullscreen) toggleFullscreen()
        if (StartupPage !== "") Actions.page = StartupPage
        if (StartupOpen) Actions.playerVisible = true
        if (StartupPanel !== "") playerView.panel = StartupPanel
        settingsPage.category = StartupCategory
        Player.shuffle = Player.shuffle
    }
    onClosing: {
        Player.shutdown()
        if (visibility === Window.Windowed) Settings.saveWindowState(x, y, width, height, false)
        else if (visibility === Window.Maximized) Settings.saveWindowState(x, y, width, height, true)
        pip.visible = false
    }

    function toggleFullscreen() {
        if (visibility === Window.FullScreen) {
            if (visibilityBeforeFullscreen === Window.Maximized) showMaximized(); else showNormal()
        } else {
            visibilityBeforeFullscreen = visibility
            showFullScreen()
        }
    }
    onVisibilityChanged: Actions.fullscreen = (visibility === Window.FullScreen)

    // ---------------------------------------------------------------- Actions wiring
    Connections {
        target: Actions
        function onToast(text, level) { toasts.show(text, level) }
        function onNavigate(page, arg) {
            Actions.page = page
            Actions.pageArg = arg
            if (page === "settings" && typeof arg === "number") settingsPage.category = arg
            Actions.playerVisible = false
        }
        function onShowPlayer() { Actions.playerVisible = true }
        function onHidePlayer() { if (Actions.fullscreen) win.toggleFullscreen(); Actions.playerVisible = false }
        function onOpenFilesRequested() { openFilesDialog.open() }
        function onOpenFolderRequested() { openFolderDialog.open() }
        function onOpenStreamRequested() { streamDialog.open() }
        function onOpenPlaylistRequested() { importPlaylistDialog.open() }
        function onAddSubtitleRequested() { subtitleDialog.open() }
        function onAddLibraryFolderRequested() { libraryFolderDialog.open() }
        function onChooseScreenshotDirRequested() { screenshotDirDialog.open() }
        function onConfirmRequested(t, m, l, d, cb) { confirmDialog.ask(t, m, l, d, cb) }
        function onPromptRequested(t, l, i, ok, cb) { promptDialog.ask(t, l, i, ok, cb) }
        function onPlaylistPickerRequested(ids) { pickerDialog.pick(ids) }
        function onInfoRequested(rows, t) { infoDialog.show(rows, t) }
        function onClassifyRequested(id) { classifyDialog.edit(id) }
        function onGoToTimeRequested() { gotoDialog.open() }
        function onFullscreenToggled() { win.toggleFullscreen() }
        function onPipToggled() { Actions.pipOpen = !Actions.pipOpen }
        function onAlwaysOnTopToggled() { Actions.alwaysOnTop = !Actions.alwaysOnTop }
        function onExportPlaylistRequested(id) { exportPlaylistDialog.playlistId = id; exportPlaylistDialog.open() }
        function onSaveQueueRequested() { saveQueueDialog.open() }
        function onShortcutsHelpRequested() { helpDialog.open() }
    }
    Connections { target: Player; function onMessage(t, l) { toasts.show(t, l) } function onScreenshotSaved(p) { toasts.show(qsTr("Screenshot saved: %1").arg(p), "success") } }
    Connections { target: Library; function onMessage(t, l) { toasts.show(t, l) } }
    Connections { target: App; function onMessage(t, l) { toasts.show(t, l) } }
    Connections {
        target: Player
        function onMediaChanged() { if (Player.active && !Actions.playerVisible && Player.queue.count > 0 && Player.currentUrl.toString() !== "") { /* started from elsewhere */ } }
        function onPlaybackEnded() { if (Actions.fullscreen) {} }
    }

    // ---------------------------------------------------------------- shortcuts
    Connections {
        target: Shortcuts
        function onTriggered(id) { win.runAction(id) }
    }
    function osd(t) { if (Actions.playerVisible) playerView.osd(t) }
    function runAction(id) {
        const step = Settings.values["subtitles/delayStep"]
        switch (id) {
        case "playPause": if (Player.active) Player.togglePause(); else Actions.openFilesRequested(); break
        case "stop": Player.stop(); Actions.hidePlayer(); break
        case "restart": Player.restart(); break
        case "seekBack": Player.seekSmall(-1); osd("−" + Settings.values["playback/seekSmall"] + " s"); break
        case "seekForward": Player.seekSmall(1); osd("+" + Settings.values["playback/seekSmall"] + " s"); break
        case "seekBackLarge": Player.seekLarge(-1); osd("−" + Settings.values["playback/seekLarge"] + " s"); break
        case "seekForwardLarge": Player.seekLarge(1); osd("+" + Settings.values["playback/seekLarge"] + " s"); break
        case "frameStep": Player.frameStep(); break
        case "abLoop": Player.cycleAbLoop(); osd(Player.loopA < 0 ? qsTr("A–B loop off") : Player.loopB < 0 ? qsTr("Loop start set") : qsTr("Looping A–B")); break
        case "next": Player.next(); break
        case "previous": Player.previous(); break
        case "chapterNext": Player.nextChapter(); break
        case "chapterPrevious": Player.previousChapter(); break
        case "speedUp": Player.speedStep(1); osd(qsTr("Speed %1×").arg(Player.rate)); break
        case "speedDown": Player.speedStep(-1); osd(qsTr("Speed %1×").arg(Player.rate)); break
        case "speedReset": Player.resetSpeed(); osd(qsTr("Speed 1×")); break
        case "goToTime": if (Player.active) Actions.goToTimeRequested(); break
        case "volumeUp": Player.volumeStep(5); osd(qsTr("Volume %1%").arg(Player.volume)); break
        case "volumeDown": Player.volumeStep(-5); osd(qsTr("Volume %1%").arg(Player.volume)); break
        case "mute": Player.toggleMute(); osd(Player.muted ? qsTr("Muted") : qsTr("Volume %1%").arg(Player.volume)); break
        case "cycleAudio": Player.cycleAudioTrack(); break
        case "audioDelayMinus": Player.adjustAudioDelay(-100); osd(qsTr("Audio delay %1 ms").arg(Player.audioDelayMs)); break
        case "audioDelayPlus": Player.adjustAudioDelay(100); osd(qsTr("Audio delay %1 ms").arg(Player.audioDelayMs)); break
        case "cycleSubtitle": Player.cycleSubtitleTrack(); break
        case "subDelayMinus": Player.adjustSubtitleDelay(-step); osd(qsTr("Subtitle delay %1 ms").arg(Player.subtitleDelayMs)); break
        case "subDelayPlus": Player.adjustSubtitleDelay(step); osd(qsTr("Subtitle delay %1 ms").arg(Player.subtitleDelayMs)); break
        case "fullscreen": if (Player.active) { Actions.playerVisible = true; toggleFullscreen() } break
        case "escape":
            if (Actions.playerVisible && playerView.closePanelOrBack()) break
            if (Actions.fullscreen) toggleFullscreen(); else if (Actions.playerVisible) Actions.hidePlayer()
            break
        case "alwaysOnTop": Actions.alwaysOnTop = !Actions.alwaysOnTop; break
        case "pip": if (Player.active) Actions.pipOpen = !Actions.pipOpen; break
        case "screenshot": if (Player.active) Player.takeScreenshot(); break
        case "mediaInfo": if (Player.active) Actions.infoRequested(Player.mediaInfoRows(), Player.title); break
        case "addBookmark": Player.addBookmark(""); break
        case "toggleQueue": Actions.playerVisible = Actions.playerVisible || Player.active; playerView.togglePanel("queue"); break
        case "toggleSubtitlePanel": playerView.togglePanel("subtitles"); break
        case "toggleAudioPanel": playerView.togglePanel("audio"); break
        case "toggleVideoPanel": playerView.togglePanel("video"); break
        case "toggleBookmarkPanel": playerView.togglePanel("bookmarks"); break
        case "openMenu": case "openFile": Actions.openFilesRequested(); break
        case "openFolder": Actions.openFolderRequested(); break
        case "openUrl": Actions.openStreamRequested(); break
        case "search": { const p = pageStack.currentItem; if (p && p.focusSearch) { Actions.playerVisible = false; p.focusSearch() } break }
        case "toggleSidebar": Settings.set("appearance/sidebarCollapsed", !Settings.values["appearance/sidebarCollapsed"]); break
        case "openSettings": Actions.navigate("settings", null); break
        case "quit": win.close(); break
        default:
            if (id.indexOf("seekPercent") === 0) Player.seekFraction(parseInt(id.slice(11)) / 10)
        }
    }

    // ---------------------------------------------------------------- layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        TitleBar { Layout.fillWidth: true; win: win; visible: !Actions.fullscreen }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                anchors.fill: parent
                spacing: 0
                Sidebar {
                    id: sidebar
                    Layout.fillHeight: true
                    collapsed: !!Settings.values["appearance/sidebarCollapsed"] || (!!Settings.values["appearance/sidebarAutoCollapse"] && Player.playing) || win.width < 1000
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    StackLayout {
                        id: pageStack
                        anchors.fill: parent
                        currentIndex: ["home", "movies", "tv", "all", "recent", "continue", "favorites", "playlists", "history", "streams", "settings"].indexOf(Actions.page)
                        readonly property Item currentItem: children[currentIndex]
                        HomePage {}
                        LibraryPage { view: "movies"; title: qsTr("Movies"); icon: "film"; emptyTitle: qsTr("No movies yet"); emptySubtitle: qsTr("Movies in your library folders show up here.") }
                        LibraryPage { view: "tv"; grouped: true; title: qsTr("TV Shows"); icon: "tv"; emptyTitle: qsTr("No TV shows yet"); emptySubtitle: qsTr("Files named like “Show.Name.S01E02” are grouped by show automatically. You can fix any misdetected item from its context menu.") }
                        LibraryPage { view: "all"; title: qsTr("All Videos"); icon: "grid"; emptyTitle: qsTr("No videos yet"); emptySubtitle: qsTr("Add a folder and Lumina scans it in the background.") }
                        LibraryPage { view: "recent"; title: qsTr("Recently Played"); icon: "clock"; sortable: false; emptyTitle: qsTr("Nothing played yet"); emptySubtitle: qsTr("What you watch shows up here.") }
                        LibraryPage { view: "continue"; title: qsTr("Continue Watching"); icon: "play-circle"; sortable: false; emptyTitle: qsTr("Nothing to continue"); emptySubtitle: qsTr("Videos you stop partway through are listed here.") }
                        LibraryPage { view: "favorites"; title: qsTr("Favorites"); icon: "heart"; emptyTitle: qsTr("No favorites"); emptySubtitle: qsTr("Click the heart on any video to keep it here.") }
                        PlaylistsPage {}
                        LibraryPage {
                            view: "history"; title: qsTr("History"); icon: "history"; sortable: false
                            emptyTitle: qsTr("History is empty"); emptySubtitle: qsTr("Turn history on or off in Settings → Privacy.")
                            headerActions: [ AppButton { visible: false; text: "" }, AppButton { iconName: "trash"; kind: "danger"; text: qsTr("Clear history"); onClicked: Actions.confirm(qsTr("Clear history?"), qsTr("Watch history and resume points will be erased."), qsTr("Clear"), true, function() { Library.clearHistory() }) } ]
                        }
                        StreamsPage {}
                        SettingsPage { id: settingsPage }
                    }
                    MiniPlayer {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 16 }
                        z: 5
                    }
                }
            }

            PlayerView {
                id: playerView
                anchors.fill: parent
                visible: Actions.playerVisible
                z: 10
            }
        }
    }

    // ---------------------------------------------------------------- resize handles
    component Edge: MouseArea {
        property int edges: 0
        visible: win.visibility === Window.Windowed
        hoverEnabled: true
        z: 100
        cursorShape: (edges === (Qt.LeftEdge | Qt.TopEdge) || edges === (Qt.RightEdge | Qt.BottomEdge)) ? Qt.SizeFDiagCursor
                   : (edges === (Qt.RightEdge | Qt.TopEdge) || edges === (Qt.LeftEdge | Qt.BottomEdge)) ? Qt.SizeBDiagCursor
                   : (edges & (Qt.LeftEdge | Qt.RightEdge)) ? Qt.SizeHorCursor : Qt.SizeVerCursor
        onPressed: win.startSystemResize(edges)
    }
    Edge { edges: Qt.LeftEdge; width: 5; anchors { left: parent.left; top: parent.top; bottom: parent.bottom; topMargin: 6; bottomMargin: 6 } }
    Edge { edges: Qt.RightEdge; width: 5; anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 6; bottomMargin: 6 } }
    Edge { edges: Qt.TopEdge; height: 5; anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: 6; rightMargin: 6 } }
    Edge { edges: Qt.BottomEdge; height: 5; anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: 6; rightMargin: 6 } }
    Edge { edges: Qt.LeftEdge | Qt.TopEdge; width: 8; height: 8; anchors { left: parent.left; top: parent.top } }
    Edge { edges: Qt.RightEdge | Qt.TopEdge; width: 8; height: 8; anchors { right: parent.right; top: parent.top } }
    Edge { edges: Qt.LeftEdge | Qt.BottomEdge; width: 8; height: 8; anchors { left: parent.left; bottom: parent.bottom } }
    Edge { edges: Qt.RightEdge | Qt.BottomEdge; width: 8; height: 8; anchors { right: parent.right; bottom: parent.bottom } }

    ToastHost { id: toasts }

    // ---------------------------------------------------------------- drag & drop
    DropArea {
        id: drop
        anchors.fill: parent
        z: 50
        keys: ["text/uri-list"]
        onDropped: function(e) {
            if (!e.hasUrls) return
            const items = App.classifyUrls(e.urls)
            var media = [], handled = false
            for (var i = 0; i < items.length; ++i) {
                const it = items[i]
                if (it.kind === "media" || it.kind === "stream") media.push(it.location)
                else if (it.kind === "subtitle") { if (Player.active) { Player.loadSubtitleFile(it.location); handled = true } else toasts.show(qsTr("Open a video first, then drop the subtitle file onto it."), "info") }
                else if (it.kind === "playlist") { if (Player.openPlaylistFile(it.location)) Actions.showPlayer(); handled = true }
                else if (it.kind === "folder") { if (Player.openFolder(it.location)) Actions.showPlayer(); handled = true }
            }
            if (media.length) { if (Player.open(media, 0)) Actions.showPlayer() }
            else if (!handled && items.length) toasts.show(qsTr("That file type isn't supported."), "warning")
        }
        Rectangle {
            anchors.fill: parent; visible: drop.containsDrag
            color: "#66000000"; border.color: Theme.accent; border.width: 3
            Text { anchors.centerIn: parent; text: qsTr("Drop to play"); color: "#FFFFFF"; font.family: Theme.fontFamily; font.pixelSize: 28; font.weight: Font.Bold }
        }
    }

    // ---------------------------------------------------------------- dialogs
    ConfirmDialog { id: confirmDialog }
    PromptDialog { id: promptDialog }
    InfoDialog { id: infoDialog }
    StreamDialog { id: streamDialog }
    PlaylistPickerDialog { id: pickerDialog }
    GoToTimeDialog { id: gotoDialog }
    ClassifyDialog { id: classifyDialog }
    ShortcutsHelpDialog { id: helpDialog }

    FileDialog {
        id: openFilesDialog
        title: qsTr("Open media")
        fileMode: FileDialog.OpenFiles
        nameFilters: App.mediaFilters
        currentFolder: "file://" + App.moviesFolder
        onAccepted: {
            const locs = []
            for (var i = 0; i < selectedFiles.length; ++i) locs.push(App.toLocalPath(selectedFiles[i]))
            if (Player.active && Actions.playerVisible && locs.length) { const n = Player.enqueue(locs, false); toasts.show(qsTr("Added %n item(s) to the queue", "", n), "success") }
            else if (Player.open(locs, 0)) Actions.showPlayer()
        }
    }
    FolderDialog {
        id: openFolderDialog
        title: qsTr("Open folder")
        onAccepted: if (Player.openFolder(App.toLocalPath(selectedFolder))) Actions.showPlayer()
    }
    FolderDialog {
        id: libraryFolderDialog
        title: qsTr("Add folder to library")
        onAccepted: if (Library.addFolder(App.toLocalPath(selectedFolder))) toasts.show(qsTr("Folder added. Scanning…"), "info")
    }
    FolderDialog {
        id: screenshotDirDialog
        title: qsTr("Screenshot folder")
        onAccepted: Settings.set("screenshots/dir", App.toLocalPath(selectedFolder))
    }
    FileDialog {
        id: subtitleDialog
        title: qsTr("Load subtitle file")
        nameFilters: App.subtitleFilters
        onAccepted: Player.loadSubtitleFile(App.toLocalPath(selectedFile))
    }
    FileDialog {
        id: importPlaylistDialog
        title: qsTr("Import playlist")
        nameFilters: App.playlistFilters
        onAccepted: {
            const id = Library.importPlaylistFile(App.toLocalPath(selectedFile))
            toasts.show(id > 0 ? qsTr("Playlist imported.") : qsTr("Could not import that playlist."), id > 0 ? "success" : "error")
        }
    }
    FileDialog {
        id: exportPlaylistDialog
        property var playlistId: 0
        title: qsTr("Export playlist")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "m3u8"
        nameFilters: App.playlistFilters
        onAccepted: toasts.show(Library.exportPlaylistFile(playlistId, App.toLocalPath(selectedFile)) ? qsTr("Playlist exported.") : qsTr("Could not write that file."), "info")
    }
    FileDialog {
        id: saveQueueDialog
        title: qsTr("Save queue as playlist")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "m3u8"
        nameFilters: App.playlistFilters
        onAccepted: toasts.show(Player.saveQueueAsPlaylistFile(App.toLocalPath(selectedFile)) ? qsTr("Queue saved.") : qsTr("Could not write that file."), "info")
    }

    // ---------------------------------------------------------------- picture-in-picture
    Window {
        id: pip
        visible: Actions.pipOpen && Player.active
        width: 420; height: 236
        minimumWidth: 240; minimumHeight: 135
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        color: "#000000"
        title: "Lumina PiP"
        onClosing: Actions.pipOpen = false
        Rectangle { anchors.fill: parent; color: "#000000" }
        VideoItem {
            anchors.fill: parent
            source: Player.frameSource
            aspectMode: Player.aspectMode
            zoom: Player.zoom
            videoRotation: Player.videoRotation
        }
        MouseArea {
            id: pipArea
            anchors.fill: parent
            hoverEnabled: true
            onPressed: pip.startSystemMove()
            onDoubleClicked: { Actions.pipOpen = false; Actions.showPlayer() }
            onWheel: function(w) { Player.volumeStep(w.angleDelta.y > 0 ? 5 : -5) }
        }
        Row {
            anchors { top: parent.top; right: parent.right; margins: 6 }
            spacing: 4
            opacity: pipArea.containsMouse ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
            IconButton { iconName: Player.playing ? "pause" : "play"; iconColor: "#FFFFFF"; diameter: 30; iconSize: 16; tip: qsTr("Play / pause"); onClicked: Player.togglePause() }
            IconButton { iconName: "fullscreen"; iconColor: "#FFFFFF"; diameter: 30; iconSize: 16; tip: qsTr("Back to player"); onClicked: { Actions.pipOpen = false; Actions.showPlayer() } }
            IconButton { iconName: "close"; iconColor: "#FFFFFF"; diameter: 30; iconSize: 16; tip: qsTr("Close"); onClicked: Actions.pipOpen = false }
        }
        MouseArea {
            width: 18; height: 18; anchors { right: parent.right; bottom: parent.bottom }
            cursorShape: Qt.SizeFDiagCursor
            onPressed: pip.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
    }
}
