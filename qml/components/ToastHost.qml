import QtQuick
import QtQuick.Layouts
import Lumina.UI

// Transient messages. Call show(text, level) with level = info|success|warning|error.
Item {
    id: root
    anchors.fill: parent
    z: 1000
    function show(text, level) {
        if (!text) return
        model.append({ message: text, level: level || "info", uid: ++counter })
        if (model.count > 4) model.remove(0)
    }
    property int counter: 0
    function removeByUid(uid) {
        for (var i = 0; i < model.count; ++i)
            if (model.get(i).uid === uid) { model.remove(i); return }
    }
    ListModel { id: model }
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 96
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8
        Repeater {
            model: model
            delegate: Rectangle {
                id: toast
                required property string message
                required property string level
                required property int uid
                width: Math.min(row.implicitWidth + 32, root.width - 48)
                height: 44
                radius: 12
                color: Theme.elevated
                border.width: 1
                border.color: level === "error" ? Theme.danger : level === "warning" ? Theme.warning : level === "success" ? Theme.success : Theme.borderStrong
                opacity: 0
                Component.onCompleted: { opacity = 1; life.start() }
                Behavior on opacity { NumberAnimation { duration: Theme.animMed } }
                Timer { id: life; interval: level === "error" ? 6500 : 3500; onTriggered: { toast.opacity = 0; gone.start() } }
                Timer { id: gone; interval: Theme.animMed + 30; onTriggered: root.removeByUid(toast.uid) }
                Row {
                    id: row
                    anchors.centerIn: parent
                    spacing: 10
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 18
                        name: level === "error" || level === "warning" ? "warning" : level === "success" ? "check" : "info"
                        color: level === "error" ? Theme.danger : level === "warning" ? Theme.warning : level === "success" ? Theme.success : Theme.accent
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: toast.message
                        color: Theme.text
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, root.width - 120)
                    }
                }
            }
        }
    }
}
