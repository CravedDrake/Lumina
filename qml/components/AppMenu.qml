import QtQuick
import QtQuick.Controls.Basic
import Lumina.UI

Menu {
    id: root
    padding: 6
    topPadding: 6
    bottomPadding: 6
    modal: false
    implicitWidth: 230
    delegate: AppMenuItem {}
    background: Rectangle {
        color: Theme.elevated
        radius: Theme.radius
        border.color: Theme.borderStrong
        border.width: 1
    }
}
