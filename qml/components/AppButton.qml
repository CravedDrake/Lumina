import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Button {
    id: root
    property string iconName: ""
    property string kind: "secondary"      // primary | secondary | ghost | danger
    implicitHeight: Theme.rowHeight
    implicitWidth: Math.max(contentItem.implicitWidth + 28, 72)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.45

    background: Rectangle {
        radius: Theme.radiusSmall + 2
        color: {
            if (root.kind === "primary") return root.down ? Qt.darker(Theme.accent, 1.15) : root.hovered ? Theme.accentHover : Theme.accent
            if (root.kind === "danger") return root.down ? Qt.darker(Theme.danger, 1.3) : root.hovered ? Theme.alpha(Theme.danger, 0.3) : Theme.alpha(Theme.danger, 0.18)
            if (root.kind === "ghost") return root.down ? Theme.alpha(Theme.text, 0.16) : root.hovered ? Theme.alpha(Theme.text, 0.08) : "transparent"
            return root.down ? Theme.hover : root.hovered ? Theme.hover : Theme.elevated
        }
        border.width: root.visualFocus ? 2 : (root.kind === "secondary" ? 1 : 0)
        border.color: root.visualFocus ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }
    contentItem: RowLayout {
        spacing: 8
        Icon {
            visible: root.iconName !== ""
            name: root.iconName
            size: 18
            color: root.kind === "primary" ? Theme.textOnAccent : root.kind === "danger" ? Theme.danger : Theme.text
            Layout.alignment: Qt.AlignVCenter
        }
        Text {
            text: root.text
            visible: text !== ""
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsBody
            font.weight: Font.Medium
            color: root.kind === "primary" ? Theme.textOnAccent : root.kind === "danger" ? Theme.danger : Theme.text
            elide: Text.ElideRight
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
