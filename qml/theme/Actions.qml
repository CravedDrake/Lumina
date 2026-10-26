pragma Singleton
import QtQuick

// App-wide UI events. Components emit/call these; Main.qml owns the dialogs and window.
QtObject {
    signal toast(string text, string level)
    signal navigate(string page, var arg)
    signal showPlayer()
    signal hidePlayer()
    signal openFilesRequested()
    signal openFolderRequested()
    signal openStreamRequested()
    signal openPlaylistRequested()
    signal addSubtitleRequested()
    signal addLibraryFolderRequested()
    signal confirmRequested(string title, string message, string label, bool danger, var callback)
    signal promptRequested(string title, string label, string initial, string okLabel, var callback)
    signal playlistPickerRequested(var mediaIds)
    signal infoRequested(var rows, string title)
    signal classifyRequested(var mediaId)
    signal goToTimeRequested()
    signal fullscreenToggled()
    signal pipToggled()
    signal alwaysOnTopToggled()
    signal exportPlaylistRequested(var playlistId)
    signal saveQueueRequested()
    signal shortcutsHelpRequested()
    signal chooseScreenshotDirRequested()

    property string page: "home"
    property var pageArg: null
    property bool playerVisible: false
    property bool fullscreen: false
    property bool alwaysOnTop: false
    property bool pipOpen: false

    function info(t) { toast(t, "info") }
    function success(t) { toast(t, "success") }
    function warn(t) { toast(t, "warning") }
    function error(t) { toast(t, "error") }
    function confirm(title, message, label, danger, callback) { confirmRequested(title, message, label, danger, callback) }
    function prompt(title, label, initial, okLabel, callback) { promptRequested(title, label, initial, okLabel, callback) }

    // Play a list of library ids starting at an index and show the player.
    function playIds(ids, index) {
        if (Player.openMediaIds(ids, index || 0)) showPlayer()
    }
    function mediaInfoRows(id) {
        var m = Library.mediaInfo(id)
        var rows = []
        if (!m || m.path === undefined) return rows
        function add(k, v) { if (v !== undefined && v !== "" && v !== 0 && v !== null) rows.push({ label: k, value: String(v) }) }
        add(qsTr("Title"), m.title)
        add(qsTr("Type"), m.kind)
        if (m.showName) add(qsTr("Show"), m.showName + (m.season ? "  S" + m.season + "E" + m.episode : ""))
        add(qsTr("Year"), m.year)
        add(qsTr("Path"), m.path)
        add(qsTr("Size"), App.formatBytes(m.size))
        add(qsTr("Duration"), m.durationMs > 0 ? App.formatTime(m.durationMs) : "")
        if (m.width > 0) add(qsTr("Resolution"), m.width + " × " + m.height)
        if (m.fps > 0) add(qsTr("Frame rate"), m.fps.toFixed(2) + " fps")
        add(qsTr("Video codec"), m.vcodec)
        add(qsTr("Audio codec"), m.acodec)
        add(qsTr("Audio tracks"), m.audioTracks)
        add(qsTr("Subtitle tracks"), m.subTracks)
        add(qsTr("Play count"), m.playCount)
        add(qsTr("Added"), App.formatDate(m.addedAt))
        return rows
    }
}
