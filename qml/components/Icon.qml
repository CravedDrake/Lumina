import QtQuick
import QtQuick.Window
import Lumina.UI

// Stroke icon from resources/icons, tinted with any colour.
Item {
    id: root
    property string name: ""
    property color color: Theme.text
    property int size: 20
    implicitWidth: size
    implicitHeight: size
    Image {
        anchors.fill: parent
        source: root.name ? Theme.iconUrl(root.name, root.color) : ""
        sourceSize.width: Math.ceil(root.width * Screen.devicePixelRatio)
        sourceSize.height: Math.ceil(root.height * Screen.devicePixelRatio)
        smooth: true
        cache: true
        asynchronous: false
    }
}
