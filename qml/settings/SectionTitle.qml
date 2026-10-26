import QtQuick
import Lumina.UI

Text {
    property bool first: false
    topPadding: first ? 0 : 22
    bottomPadding: 6
    color: Theme.accent
    font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold
    font.capitalization: Font.AllUppercase
    font.letterSpacing: 1
}
