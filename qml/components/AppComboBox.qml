import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

ComboBox {
    id: root
    implicitHeight: Theme.rowHeight
    implicitWidth: 200
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsBody
    delegate: ItemDelegate {
        width: root.width
        height: Theme.rowHeight
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: root.textAt(index)
            color: Theme.text
            font: root.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { color: highlighted ? Theme.hover : "transparent"; radius: 4 }
    }
    indicator: Icon { name: "chevron-down"; size: 16; color: Theme.textDim; x: root.width - width - 10; y: (root.height - height) / 2 }
    contentItem: Text {
        leftPadding: 12
        rightPadding: 32
        text: root.displayText
        font: root.font
        color: Theme.text
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.radiusSmall + 2
        color: root.hovered ? Theme.hover : Theme.elevated
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? Theme.accent : Theme.border
    }
    popup: Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle { color: Theme.elevated; radius: Theme.radius; border.color: Theme.borderStrong; border.width: 1 }
    }
}
