import QtQuick
import QtQuick.Window
import Lumina.Core
import Lumina.UI

// The mini player: its own top-level, always-on-top window that is independent of the main window
// (it stays up when the main window is minimised). While it is open the main window does not draw the
// video at all, so the picture is decoded once and rendered once.
Window {
    id: pip
    required property Window mainWindow
    signal restoreRequested()

    // Open while something is loaded and the user asked for it.
    readonly property bool wanted: Actions.pipOpen && Player.active
    readonly property real videoAspect: {
        const a = Player.frameSource.naturalAspect
        if (a > 0) return a
        const s = Player.frameSource.videoSize
        return s.height > 0 ? s.width / s.height : 16 / 9
    }

    visible: false
    // Explicitly no transient parent: a child window would be minimised/hidden together with the main window.
    transientParent: null
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#000000"
    title: "Lumina mini player"
    minimumWidth: 240
    minimumHeight: 135
    width: 440
    height: 248

    onWantedChanged: {
        if (wanted) { placeWindow(); visible = true; content.forceActiveFocus() }
        else { saveGeometry(); visible = false }
    }
    Component.onCompleted: {
        // QML makes a Window declared inside another Window its transient child (the OS then minimises it with the
        // parent). The binding above is not enough on its own, so clear it once more after construction.
        transientParent = null
        if (wanted) { placeWindow(); visible = true }
    }
    onClosing: Actions.pipOpen = false      // the system closed it (taskbar, Alt+F4): treat like the close button

    function saveGeometry() {
        if (width < 100 || height < 60) return
        Settings.set("pip/geometry", JSON.stringify({ x: x, y: y, w: width, h: height }))
    }
    function placeWindow() {
        var g = null
        try { g = JSON.parse(Settings.values["pip/geometry"] || "") } catch (e) { g = null }
        var w = 440
        var h = Math.round(w / Math.max(0.6, Math.min(2.6, videoAspect)))
        // default: bottom-right corner of the screen the main window is on
        const scr = mainWindow.screen
        var px = scr.virtualX + scr.desktopAvailableWidth - w - 24
        var py = scr.virtualY + scr.desktopAvailableHeight - h - 24
        if (g && g.w >= 240 && g.h >= 135) {
            // reuse the last spot only if it is still (mostly) on a connected screen
            var onScreen = false
            const screens = Qt.application.screens
            for (var i = 0; i < screens.length; ++i) {
                const a = screens[i]
                if (g.x + 60 < a.virtualX + a.width && g.x + g.w - 60 > a.virtualX
                        && g.y + 30 < a.virtualY + a.height && g.y + g.h - 30 > a.virtualY) onScreen = true
            }
            if (onScreen) { px = g.x; py = g.y; w = g.w; h = g.h }
        }
        x = px; y = py; width = w; height = h
    }

    FocusScope {
        id: content
        anchors.fill: parent
        focus: true

        Keys.onSpacePressed: Player.togglePause()
        Keys.onLeftPressed: Player.seekSmall(-1)
        Keys.onRightPressed: Player.seekSmall(1)
        Keys.onUpPressed: { Player.volumeStep(5); pip.flashVolume() }
        Keys.onDownPressed: { Player.volumeStep(-5); pip.flashVolume() }
        Keys.onEscapePressed: Actions.pipOpen = false

        Rectangle { anchors.fill: parent; color: "#000000" }

        VideoItem {
            id: video
            anchors.fill: parent
            source: Player.frameSource
            aspectMode: Player.aspectMode
            videoRotation: Player.videoRotation
            // the mini player's own zoom is always 1 (the main player's zoom/pan does not apply here)
        }

        // audio-only media
        Text {
            anchors.centerIn: parent
            visible: Player.active && !video.hasFrame && !Player.busy
            text: Player.title
            color: Theme.textDim
            width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: 13
        }

        // ---- input: tap = pause, double tap = back to the big player, drag = move, wheel = volume
        Item {
            id: surface
            anchors.fill: parent
            HoverHandler { id: hover }
            TapHandler {
                onSingleTapped: Player.togglePause()
                onDoubleTapped: pip.restoreRequested()
            }
            DragHandler {
                target: null
                onActiveChanged: if (active) pip.startSystemMove()
            }
            WheelHandler {
                onWheel: function(e) { Player.volumeStep(e.angleDelta.y > 0 ? 5 : -5); pip.flashVolume() }
            }
        }

        // ---- overlay (fades in while the pointer is over the window or playback is paused)
        Item {
            id: overlay
            anchors.fill: parent
            opacity: (hover.hovered || !Player.playing || seekArea.pressed) ? 1 : 0
            visible: opacity > 0.01
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

            Rectangle {
                id: topBar
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 44
                gradient: Gradient {
                    GradientStop { position: 0; color: "#B3000000" }
                    GradientStop { position: 1; color: "#00000000" }
                }
                Text {
                    anchors { left: parent.left; right: topButtons.left; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 8 }
                    text: Player.title
                    elide: Text.ElideRight
                    color: "#FFFFFF"
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                Row {
                    id: topButtons
                    anchors { right: parent.right; top: parent.top; margins: 6 }
                    spacing: 2
                    IconButton { iconName: "fullscreen"; iconColor: "#FFFFFF"; diameter: 30; iconSize: 16; tip: qsTr("Back to the big player"); onClicked: pip.restoreRequested() }
                    IconButton { iconName: "close"; iconColor: "#FFFFFF"; diameter: 30; iconSize: 16; tip: qsTr("Close mini player"); onClicked: Actions.pipOpen = false }
                }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 72
                gradient: Gradient {
                    GradientStop { position: 0; color: "#00000000" }
                    GradientStop { position: 1; color: "#CC000000" }
                }
            }

            Row {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: seekArea.top; bottomMargin: 2 }
                spacing: 6
                IconButton { visible: pip.width >= 300; iconName: "skip-prev"; iconColor: "#FFFFFF"; diameter: 32; iconSize: 17; tip: qsTr("Previous"); onClicked: Player.previous() }
                IconButton { iconName: Player.playing ? "pause" : "play"; iconColor: "#FFFFFF"; filled: true; diameter: 36; iconSize: 20; tip: Player.playing ? qsTr("Pause") : qsTr("Play"); onClicked: Player.togglePause() }
                IconButton { visible: pip.width >= 300; iconName: "skip-next"; iconColor: "#FFFFFF"; diameter: 32; iconSize: 17; tip: qsTr("Next"); enabled: Player.canNext; onClicked: Player.next() }
            }

            // thin seek bar
            Item {
                id: seekArea
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 20
                property bool dragging: false
                property real dragFrac: 0
                readonly property real frac: dragging ? dragFrac : (Player.durationMs > 0 ? Math.min(1, Player.positionMs / Player.durationMs) : 0)
                readonly property bool pressed: seekMouse.pressed

                Rectangle {
                    id: track
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 10; rightMargin: 10; bottomMargin: 7 }
                    height: seekMouse.containsMouse || seekMouse.pressed ? 5 : 3
                    radius: height / 2
                    color: "#55FFFFFF"
                    Behavior on height { NumberAnimation { duration: 90 } }
                    Rectangle { width: parent.width * seekArea.frac; height: parent.height; radius: parent.radius; color: Theme.accent }
                    Rectangle {
                        visible: seekMouse.containsMouse || seekMouse.pressed
                        x: parent.width * seekArea.frac - width / 2; y: (parent.height - height) / 2
                        width: 11; height: 11; radius: 6; color: "#FFFFFF"
                    }
                }
                MouseArea {
                    id: seekMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: Player.seekable && Player.durationMs > 0
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    function frac(mx) { return Math.max(0, Math.min(1, (mx - track.x) / track.width)) }
                    onPressed: function(m) { seekArea.dragging = true; seekArea.dragFrac = frac(m.x) }
                    onPositionChanged: function(m) { if (pressed) seekArea.dragFrac = frac(m.x) }
                    onReleased: { Player.seekTo(seekArea.dragFrac * Player.durationMs); seekArea.dragging = false }
                    onCanceled: seekArea.dragging = false
                }
            }
        }

        // volume flash
        Rectangle {
            id: volBox
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14 }
            width: volText.implicitWidth + 24; height: 28; radius: 14
            color: "#CC000000"; opacity: 0; visible: opacity > 0.01
            Behavior on opacity { NumberAnimation { duration: 120 } }
            Text { id: volText; anchors.centerIn: parent; color: "#FFFFFF"; font.family: Theme.fontFamily; font.pixelSize: 12 }
            Timer { id: volTimer; interval: 900; onTriggered: volBox.opacity = 0 }
        }

        // thin border so the frameless window reads as a window on any background
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#44FFFFFF"; border.width: 1 }
    }

    function flashVolume() {
        volText.text = Player.muted ? qsTr("Muted") : qsTr("Volume %1%").arg(Player.volume)
        volBox.opacity = 1
        volTimer.restart()
    }

    // ---- resize handles (frameless window)
    component Edge: MouseArea {
        property int edges: 0
        z: 100
        hoverEnabled: true
        cursorShape: (edges === (Qt.LeftEdge | Qt.TopEdge) || edges === (Qt.RightEdge | Qt.BottomEdge)) ? Qt.SizeFDiagCursor
                   : (edges === (Qt.RightEdge | Qt.TopEdge) || edges === (Qt.LeftEdge | Qt.BottomEdge)) ? Qt.SizeBDiagCursor
                   : (edges & (Qt.LeftEdge | Qt.RightEdge)) ? Qt.SizeHorCursor : Qt.SizeVerCursor
        onPressed: pip.startSystemResize(edges)
    }
    Edge { edges: Qt.LeftEdge; width: 5; anchors { left: parent.left; top: parent.top; bottom: parent.bottom; topMargin: 8; bottomMargin: 8 } }
    Edge { edges: Qt.RightEdge; width: 5; anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 8; bottomMargin: 8 } }
    Edge { edges: Qt.TopEdge; height: 5; anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: 8; rightMargin: 8 } }
    Edge { edges: Qt.BottomEdge; height: 5; anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: 8; rightMargin: 8 } }
    Edge { edges: Qt.LeftEdge | Qt.TopEdge; width: 9; height: 9; anchors { left: parent.left; top: parent.top } }
    Edge { edges: Qt.RightEdge | Qt.TopEdge; width: 9; height: 9; anchors { right: parent.right; top: parent.top } }
    Edge { edges: Qt.LeftEdge | Qt.BottomEdge; width: 9; height: 9; anchors { left: parent.left; bottom: parent.bottom } }
    Edge { edges: Qt.RightEdge | Qt.BottomEdge; width: 9; height: 9; anchors { right: parent.right; bottom: parent.bottom } }
}
