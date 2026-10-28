import QtQuick
import Lumina.Core

// A second view of the live frame (shares the single libVLC video output with the main player).
VideoItem {
    source: Player.frameSource
    aspectMode: "auto"
    maxFps: 10          // a 78-pixel thumbnail does not need 60 texture uploads per second
}
