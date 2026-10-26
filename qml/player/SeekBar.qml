import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

// Timeline: progress, A-B loop range, chapter ticks, bookmark markers, hover time (+ chapter name).
Item {
    id: root
    implicitHeight: 28
    property real hoverFraction: -1
    readonly property real duration: Player.durationMs
    property bool dragging: false
    property real dragFraction: 0
    readonly property real shownFraction: dragging ? dragFraction : (duration > 0 ? Math.min(1, Player.positionMs / duration) : 0)
    enabled: Player.seekable && duration > 0
    opacity: enabled ? 1 : 0.55
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("Seek")
    Accessible.description: App.formatTime(Player.positionMs) + " / " + App.formatTime(duration)
    Keys.onLeftPressed: Player.seekSmall(-1)
    Keys.onRightPressed: Player.seekSmall(1)

    function chapterAt(ms) {
        var c = Player.chapters, name = ""
        for (var i = 0; i < c.length; ++i) { if (c[i].startMs <= ms) name = c[i].name; else break }
        return name
    }

    Rectangle {
        id: track
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        height: area.containsMouse || root.dragging ? 8 : 5
        radius: height / 2
        color: "#40FFFFFF"
        Behavior on height { NumberAnimation { duration: Theme.animFast } }

        // buffered/hover preview fill
        Rectangle {
            visible: root.hoverFraction >= 0
            width: parent.width * root.hoverFraction; height: parent.height; radius: parent.radius
            color: "#30FFFFFF"
        }
        // A-B range
        Rectangle {
            visible: Player.loopA >= 0 && root.duration > 0
            x: parent.width * (Player.loopA / root.duration)
            width: Player.loopB >= 0 ? parent.width * ((Player.loopB - Player.loopA) / root.duration) : 2
            height: parent.height; radius: 2
            color: Theme.alpha(Theme.warning, 0.45)
        }
        // progress
        Rectangle {
            width: parent.width * root.shownFraction; height: parent.height; radius: parent.radius
            color: Theme.accent
        }
        // chapter ticks
        Repeater {
            model: root.duration > 0 ? Player.chapters : []
            delegate: Rectangle {
                required property var modelData
                visible: modelData.startMs > 1000
                x: track.width * (modelData.startMs / root.duration) - 1
                width: 2; height: track.height; color: "#99000000"
            }
        }
        // bookmarks
        Repeater {
            model: root.duration > 0 ? Player.bookmarks : []
            delegate: Rectangle {
                required property var modelData
                x: track.width * (modelData.positionMs / root.duration) - 4
                y: -9; width: 8; height: 8; radius: 4
                color: Theme.warning
            }
        }
        // handle
        Rectangle {
            x: track.width * root.shownFraction - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: area.containsMouse || root.dragging || root.activeFocus ? 16 : 0
            height: width; radius: width / 2
            color: "#FFFFFF"
            Behavior on width { NumberAnimation { duration: Theme.animFast } }
        }
    }

    // hover tooltip
    Rectangle {
        id: tip
        visible: root.hoverFraction >= 0 && root.duration > 0
        readonly property real ms: Math.round(root.hoverFraction * root.duration)
        readonly property bool previewOn: !!Settings.values["performance/seekPreview"] && Player.currentMediaId > 0 && !Player.isStream
        property int previewBucket: -1
        Timer {   // debounce: only request a frame once the pointer rests
            id: previewDebounce; interval: 350
            onTriggered: tip.previewBucket = Math.floor(tip.ms / 5000) * 5000
        }
        onMsChanged: { if (previewOn && visible) previewDebounce.restart() }
        onVisibleChanged: if (!visible) { previewDebounce.stop(); previewBucket = -1 }
        readonly property bool havePreview: previewOn && previewImg.status === Image.Ready
        width: Math.max(tipTime.implicitWidth, tipChapter.implicitWidth, havePreview ? 168 : 0) + 20
        height: (tipChapter.text !== "" ? 44 : 28) + (havePreview ? 96 : 0)
        radius: 8
        color: "#E6141824"
        border.color: Theme.borderStrong
        x: Math.max(0, Math.min(root.width - width, root.width * root.hoverFraction - width / 2))
        y: -height - 8
        Image {
            id: previewImg
            visible: tip.havePreview
            width: 168; height: 94
            anchors { top: parent.top; topMargin: 6; horizontalCenter: parent.horizontalCenter }
            fillMode: Image.PreserveAspectFit
            asynchronous: true; cache: false
            sourceSize.width: 240
            source: tip.previewOn && tip.previewBucket >= 0 ? "image://thumb/seek/" + Player.currentMediaId + "/" + tip.previewBucket : ""
        }
        Column {
            anchors { bottom: parent.bottom; bottomMargin: 4; horizontalCenter: parent.horizontalCenter }
            Text { id: tipTime; anchors.horizontalCenter: parent.horizontalCenter; text: App.formatTime(tip.ms); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.Medium }
            Text { id: tipChapter; anchors.horizontalCenter: parent.horizontalCenter; text: root.chapterAt(tip.ms); visible: text !== ""; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: 11 }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        function frac(x) { return Math.max(0, Math.min(1, x / width)) }
        onPositionChanged: function(e) {
            root.hoverFraction = frac(e.x)
            if (root.dragging) { root.dragFraction = frac(e.x); throttle.restart() }
        }
        onExited: if (!root.dragging) root.hoverFraction = -1
        onPressed: function(e) { root.dragging = true; root.dragFraction = frac(e.x); Player.seekFraction(root.dragFraction) }
        onReleased: function(e) { Player.seekFraction(frac(e.x)); root.dragging = false; throttle.stop(); if (!containsMouse) root.hoverFraction = -1 }
        onCanceled: { root.dragging = false; throttle.stop() }
    }
    Timer { id: throttle; interval: 120; repeat: true; running: false; onTriggered: Player.seekFraction(root.dragFraction) }
}
