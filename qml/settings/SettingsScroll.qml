import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable {
    id: root
    default property alias content: col.data
    contentWidth: width
    contentHeight: col.implicitHeight + 60
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    ColumnLayout {
        id: col
        x: 32; y: 26
        width: Math.min(root.width - 64, 820)
        spacing: 0
    }
}
