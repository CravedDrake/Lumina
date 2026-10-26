import QtQuick
import Lumina.UI

Item {
    id: root
    property int size: 28
    property color color: Theme.accent
    implicitWidth: size; implicitHeight: size
    visible: running
    property bool running: true
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: Math.max(2, root.size / 10)
        border.color: Theme.alpha(root.color, 0.2)
    }
    Canvas {
        id: arc
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = Math.max(2, root.size / 10)
            ctx.strokeStyle = root.color
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, width / 2 - ctx.lineWidth / 2, 0, Math.PI * 0.6)
            ctx.stroke()
        }
        RotationAnimator on rotation { from: 0; to: 360; duration: 900; loops: Animation.Infinite; running: root.running && root.visible }
    }
}
