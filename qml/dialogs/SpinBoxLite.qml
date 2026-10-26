import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

SpinBox {
    from: 0; to: 9999; editable: true
    implicitWidth: 110; implicitHeight: Theme.rowHeight
    font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
    contentItem: TextInput { text: parent.textFromValue(parent.value, parent.locale); font: parent.font; color: Theme.text; horizontalAlignment: Qt.AlignHCenter; verticalAlignment: Qt.AlignVCenter; readOnly: !parent.editable; validator: parent.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
    background: Rectangle { color: Theme.surface2; radius: 8; border.color: Theme.border }
}
