import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.Core
import Lumina.UI

Item {
    id: root
    property string panel: ""                  // "" | subtitles | audio | video | bookmarks | queue
    property bool controlsVisible: true
    readonly property bool uiPinned: Player.state !== "playing" || panel !== "" || controlsHover.hovered || menuOpen
    property bool menuOpen: ctxMenu.opened
    readonly property bool showUi: controlsVisible || uiPinned

    function osd(text) { osdText.text = text; osdBox.opacity = 1; osdTimer.restart() }
    function togglePanel(name) { panel = (panel === name) ? "" : name; wake() }
    function wake() {
        controlsVisible = true
        hideTimer.restart()
    }
    function closePanelOrBack() {
        if (panel !== "") { panel = ""; return true }
        return false
    }

    Timer {
        id: hideTimer
        interval: Math.max(500, Settings.values["playback/autoHideMs"])
        running: Settings.values["playback/autoHideMs"] > 0
        onTriggered: if (!root.uiPinned) root.controlsVisible = false
    }
    Timer { id: osdTimer; interval: 1200; onTriggered: osdBox.opacity = 0 }
    Timer { id: clickTimer; interval: 230; onTriggered: if (Settings.values["mouse/clickPause"]) Player.togglePause() }

    Rectangle { anchors.fill: parent; color: "#000000" }

    VideoItem {
        id: video
        anchors.fill: parent
        source: Player.frameSource
        aspectMode: Player.aspectMode
        zoom: Player.zoom
        videoRotation: Player.videoRotation
        visible: !Actions.pipOpen            // the mini player shows the picture; do not draw it twice
    }

    // audio-only / no picture yet
    ColumnLayout {
        anchors.centerIn: parent
        visible: Player.active && !video.hasFrame && !Player.busy && Player.errorText === ""
        spacing: 12
        Icon { Layout.alignment: Qt.AlignHCenter; name: "music"; size: 64; color: Theme.textMuted }
        Text { Layout.alignment: Qt.AlignHCenter; text: Player.title; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle }
    }

    // shown instead of the picture while the mini player window is open
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        visible: Actions.pipOpen
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 14
            Icon { Layout.alignment: Qt.AlignHCenter; name: "pip"; size: 56; color: Theme.textMuted }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("Playing in mini player"); color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle }
            Text { Layout.alignment: Qt.AlignHCenter; text: Player.title; elide: Text.ElideRight; Layout.maximumWidth: 520; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
            AppButton { Layout.alignment: Qt.AlignHCenter; iconName: "fullscreen"; text: qsTr("Bring video back here"); onClicked: Actions.pipToggled() }
        }
    }

    // ------------------------------------------------------------------ input
    MouseArea {
        id: input
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: root.showUi ? Qt.ArrowCursor : Qt.BlankCursor
        property real startX: 0
        property real startY: 0
        property string mode: ""            // "" | seek | volume
        property real startPos: 0
        property real startVol: 0
        property real seekTarget: 0
        property bool moved: false

        onPositionChanged: function(e) {
            root.wake()
            if (!pressed || !(pressedButtons & Qt.LeftButton) || !Settings.values["mouse/gestures"]) return
            const dx = e.x - startX, dy = e.y - startY
            if (mode === "") {
                if (Math.abs(dx) > 14 && Math.abs(dx) > Math.abs(dy) && Player.seekable && Player.durationMs > 0) { mode = "seek"; startPos = Player.positionMs }
                else if (Math.abs(dy) > 14 && Math.abs(dy) > Math.abs(dx) && startX > width / 2) { mode = "volume"; startVol = Player.volume }
                if (mode !== "") moved = true
            }
            if (mode === "seek") {
                const span = Math.min(Player.durationMs, 180000)
                seekTarget = Math.max(0, Math.min(Player.durationMs, startPos + dx / width * span))
                root.osd(App.formatTime(seekTarget) + "  (" + (seekTarget >= startPos ? "+" : "−") + App.formatTime(Math.abs(seekTarget - startPos)) + ")")
            } else if (mode === "volume") {
                const v = Math.max(0, Math.min(100, Math.round(startVol - dy / height * 120)))
                Player.muted = false
                Player.volume = v
                root.osd(qsTr("Volume %1%").arg(v))
            }
        }
        onPressed: function(e) {
            root.wake()
            if (e.button === Qt.RightButton) { ctxMenu.popup(e.x, e.y); return }
            startX = e.x; startY = e.y; mode = ""; moved = false
        }
        onReleased: function(e) {
            if (e.button !== Qt.LeftButton) return
            if (mode === "seek") Player.seekTo(seekTarget)
            mode = ""
        }
        onClicked: function(e) {
            if (e.button !== Qt.LeftButton || moved) return
            if (root.panel !== "") { root.panel = ""; return }
            if (Settings.values["mouse/doubleClickFullscreen"]) clickTimer.restart()
            else if (Settings.values["mouse/clickPause"]) Player.togglePause()
        }
        onDoubleClicked: function(e) {
            if (e.button !== Qt.LeftButton) return
            clickTimer.stop()
            if (Settings.values["mouse/doubleClickFullscreen"]) Actions.fullscreenToggled()
        }
        onWheel: function(w) {
            root.wake()
            const act = Settings.values["mouse/wheelAction"]
            const dir = w.angleDelta.y > 0 ? 1 : -1
            if (act === "volume") { Player.volumeStep(dir * 5); root.osd(qsTr("Volume %1%").arg(Player.muted ? 0 : Player.volume)) }
            else if (act === "seek") { Player.seekSmall(dir); }
            w.accepted = true
        }
    }

    AppMenu {
        id: ctxMenu
        AppMenuItem { text: Player.playing ? qsTr("Pause") : qsTr("Play"); iconName: Player.playing ? "pause" : "play"; onTriggered: Player.togglePause() }
        AppMenuItem { text: qsTr("Fullscreen"); iconName: "fullscreen"; onTriggered: Actions.fullscreenToggled() }
        AppMenuItem { text: qsTr("Subtitles…"); iconName: "subtitles"; onTriggered: root.togglePanel("subtitles") }
        AppMenuItem { text: qsTr("Audio…"); iconName: "audio"; onTriggered: root.togglePanel("audio") }
        AppMenuItem { text: qsTr("Video…"); iconName: "monitor"; onTriggered: root.togglePanel("video") }
        AppMenuItem { text: qsTr("Queue"); iconName: "queue"; onTriggered: root.togglePanel("queue") }
        AppMenuItem { text: qsTr("Take screenshot"); iconName: "camera"; onTriggered: Player.takeScreenshot() }
        AppMenuItem { text: qsTr("Media information"); iconName: "info"; onTriggered: Actions.infoRequested(Player.mediaInfoRows(), Player.title) }
        AppMenuItem { text: qsTr("Back to library"); iconName: "chevron-left"; onTriggered: Actions.hidePlayer() }
    }

    // ------------------------------------------------------------------ overlays
    Spinner { anchors.centerIn: parent; size: 48; running: Player.busy; visible: Player.busy }
    Text {
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.verticalCenter; topMargin: 44 }
        visible: Player.busy && Player.bufferingPercent < 100
        text: qsTr("Buffering %1%").arg(Math.round(Player.bufferingPercent))
        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
    }

    Rectangle {
        id: osdBox
        anchors { left: parent.left; top: parent.top; leftMargin: 24; topMargin: 76 }
        width: osdText.implicitWidth + 28; height: 38; radius: 10
        color: "#CC0E1118"; border.color: Theme.border
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animMed } }
        Text { id: osdText; anchors.centerIn: parent; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Medium }
    }

    // resume prompt
    Rectangle {
        id: resumeCard
        visible: !!Player.resumePrompt.positionMs && Player.errorText === ""
        anchors.centerIn: parent
        width: 380; height: resumeCol.implicitHeight + 40; radius: Theme.radiusLarge
        color: "#F2141824"; border.color: Theme.borderStrong
        ColumnLayout {
            id: resumeCol
            anchors { fill: parent; margins: 20 }
            spacing: 14
            Text { text: qsTr("Resume playback?"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("You stopped “%1” at %2.").arg(Player.title).arg(App.formatTime(Player.resumePrompt.positionMs || 0))
                color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
            }
            RowLayout {
                spacing: 10
                AppButton { Layout.fillWidth: true; kind: "primary"; iconName: "play"; text: qsTr("Resume"); onClicked: Player.acceptResume() }
                AppButton { Layout.fillWidth: true; text: qsTr("Start over"); onClicked: Player.declineResume() }
            }
        }
    }

    // error card
    Rectangle {
        visible: Player.errorText !== ""
        anchors.centerIn: parent
        width: 440; height: errCol.implicitHeight + 40; radius: Theme.radiusLarge
        color: "#F2141824"; border.color: Theme.danger
        ColumnLayout {
            id: errCol
            anchors { fill: parent; margins: 20 }
            spacing: 12
            RowLayout {
                spacing: 10
                Icon { name: "warning"; size: 22; color: Theme.danger }
                Text { text: qsTr("Can't play this"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle; font.weight: Font.DemiBold }
            }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; text: Player.errorText; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody }
            RowLayout {
                spacing: 10
                AppButton { text: qsTr("Try again"); iconName: "refresh"; onClicked: Player.restart() }
                AppButton { visible: Player.canNext; text: qsTr("Skip to next"); iconName: "skip-next"; onClicked: Player.next() }
                AppButton { kind: "ghost"; text: qsTr("Close"); onClicked: { Player.closeMedia(); Actions.hidePlayer() } }
            }
        }
    }

    // ------------------------------------------------------------------ top bar
    Item {
        id: topBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 70
        opacity: root.showUi ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animMed } }
        Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "#CC000000" } GradientStop { position: 1; color: "#00000000" } } }
        RowLayout {
            anchors { fill: parent; leftMargin: 14; rightMargin: 14; topMargin: 6 }
            spacing: 10
            IconButton { iconName: "chevron-left"; iconColor: "#FFFFFF"; tip: qsTr("Back to library (Esc)"); onClicked: Actions.hidePlayer() }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 0
                Text { Layout.fillWidth: true; text: Player.title; elide: Text.ElideRight; color: "#FFFFFF"; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTitle - 1; font.weight: Font.DemiBold }
                Text {
                    Layout.fillWidth: true
                    visible: Player.queue.count > 1
                    text: qsTr("%1 of %2 in queue").arg(Player.queue.currentIndex + 1).arg(Player.queue.count)
                    color: "#B3FFFFFF"; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                }
            }
            IconButton { iconName: "pin"; iconColor: "#FFFFFF"; active: Actions.alwaysOnTop; tip: qsTr("Always on top (T)"); onClicked: Actions.alwaysOnTopToggled() }
            IconButton { iconName: "info"; iconColor: "#FFFFFF"; tip: qsTr("Media information (I)"); onClicked: Actions.infoRequested(Player.mediaInfoRows(), Player.title) }
        }
    }

    // ------------------------------------------------------------------ bottom controls
    ControlsOverlay {
        id: controls
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        openPanel: root.panel
        opacity: root.showUi ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animMed } }
        onTogglePanel: function(name) { root.togglePanel(name) }
        HoverHandler { id: controlsHover }
    }

    // ------------------------------------------------------------------ side panels
    Item {
        id: panelHost
        anchors { right: parent.right; top: parent.top; bottom: controls.top; topMargin: 0 }
        width: 380
        visible: root.panel !== ""
        clip: true
        SubtitlePanel { anchors.fill: parent; visible: root.panel === "subtitles"; onCloseRequested: root.panel = "" }
        AudioPanel { anchors.fill: parent; visible: root.panel === "audio"; onCloseRequested: root.panel = "" }
        VideoPanel { anchors.fill: parent; visible: root.panel === "video"; onCloseRequested: root.panel = "" }
        BookmarkPanel { anchors.fill: parent; visible: root.panel === "bookmarks"; onCloseRequested: root.panel = "" }
        QueuePanel { anchors.fill: parent; visible: root.panel === "queue"; onCloseRequested: root.panel = "" }
    }

    Connections {
        target: Player
        function onMediaChanged() { root.wake() }
        function onVolumeChanged() { /* osd shown by callers */ }
    }
    Component.onCompleted: wake()
}
