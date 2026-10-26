import QtQuick
import Lumina.Core

// A second view of the live frame (shares the single libVLC video output with the main player).
VideoItem {
    source: Player.frameSource
    aspectMode: "auto"
}
