import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Lumina.UI

Item {
    id: root
    property int category: 0
    readonly property var categories: [
        { name: qsTr("Appearance"), icon: "palette" }, { name: qsTr("Playback"), icon: "play-circle" },
        { name: qsTr("Audio"), icon: "audio" }, { name: qsTr("Subtitles"), icon: "subtitles" },
        { name: qsTr("Library"), icon: "folder" }, { name: qsTr("Keyboard & mouse"), icon: "keyboard" },
        { name: qsTr("Performance"), icon: "cpu" }, { name: qsTr("Privacy"), icon: "shield" },
        { name: qsTr("Advanced"), icon: "settings" } ]
    property string pendingArg: ""
    function show(c) { category = c }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        // ---- category list
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Theme.surface
            Rectangle { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: 1; color: Theme.border }
            Column {
                anchors { fill: parent; margins: 12; topMargin: 22 }
                spacing: 2
                Text { text: qsTr("Settings"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsHeading; font.weight: Font.Bold; bottomPadding: 12; leftPadding: 6 }
                Repeater {
                    model: root.categories
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: parent.width; height: 40; radius: 8
                        color: root.category === index ? Theme.accentSoft : cma.containsMouse ? Theme.hover : "transparent"
                        activeFocusOnTab: true
                        Keys.onReturnPressed: root.category = index
                        Keys.onSpacePressed: root.category = index
                        border.width: activeFocus ? 2 : 0; border.color: Theme.accent
                        Accessible.role: Accessible.PageTab; Accessible.name: modelData.name
                        MouseArea { id: cma; anchors.fill: parent; hoverEnabled: true; onClicked: root.category = index }
                        Row {
                            anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12 }
                            spacing: 12
                            Icon { name: modelData.icon; size: 18; color: root.category === index ? Theme.accent : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: modelData.name; color: root.category === index ? Theme.text : Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: root.category === index ? Font.DemiBold : Font.Normal; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }
        }

        // ---- content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.category

            // ============================================================ Appearance
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Theme") }
                SettingRow {
                    label: qsTr("Accent colour"); description: qsTr("Used for buttons, progress and highlights.")
                    Row {
                        spacing: 8
                        Repeater {
                            model: ["#7C5CFC", "#3B82F6", "#06B6D4", "#10B981", "#F59E0B", "#EF4444", "#EC4899"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 26; height: 26; radius: 13; color: modelData
                                border.width: Settings.values["appearance/accent"].toString().toLowerCase() === modelData.toLowerCase() ? 3 : 0
                                border.color: "#FFFFFF"
                                activeFocusOnTab: true
                                Keys.onReturnPressed: Settings.set("appearance/accent", modelData)
                                Keys.onSpacePressed: Settings.set("appearance/accent", modelData)
                                Accessible.role: Accessible.RadioButton; Accessible.name: modelData
                                MouseArea { anchors.fill: parent; onClicked: Settings.set("appearance/accent", modelData) }
                            }
                        }
                    }
                }
                ChoiceSetting {
                    key: "appearance/density"; label: qsTr("Density"); description: qsTr("Row heights and spacing in lists and settings.")
                    choices: [{ text: qsTr("Comfortable"), value: "comfortable" }, { text: qsTr("Compact"), value: "compact" }]
                }
                SwitchSetting { key: "appearance/animations"; label: qsTr("Animations"); description: qsTr("Turn off to remove fades and slides (also helps on slow machines).") }
                SectionTitle { text: qsTr("Layout") }
                ChoiceSetting {
                    key: "appearance/viewMode"; label: qsTr("Library view"); description: qsTr("Poster grid or compact list.")
                    choices: [{ text: qsTr("Grid"), value: "grid" }, { text: qsTr("List"), value: "list" }]
                }
                SwitchSetting { key: "appearance/sidebarCollapsed"; label: qsTr("Collapse sidebar"); description: qsTr("Show only icons. Ctrl+B toggles it.") }
                SwitchSetting { key: "appearance/sidebarAutoCollapse"; label: qsTr("Collapse sidebar while playing video") }
            }

            // ============================================================ Playback
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Behaviour") }
                SwitchSetting { key: "playback/autoPlayNext"; label: qsTr("Play next item automatically"); description: qsTr("When a video ends, continue with the next item in the queue.") }
                ChoiceSetting {
                    key: "playback/resume"; label: qsTr("Resume where you left off")
                    choices: [{ text: qsTr("Ask me"), value: "ask" }, { text: qsTr("Always resume"), value: "always" }, { text: qsTr("Never (start over)"), value: "never" }]
                }
                SwitchSetting { key: "playback/rememberPerMedia"; label: qsTr("Remember settings per video"); description: qsTr("Audio/subtitle track, delays, aspect ratio, zoom and speed are restored when you reopen a video.") }
                SectionTitle { text: qsTr("Seeking and speed") }
                SliderSetting { key: "playback/seekSmall"; label: qsTr("Short seek step"); from: 1; to: 60; step: 1; suffix: " s" }
                SliderSetting { key: "playback/seekLarge"; label: qsTr("Long seek step"); from: 5; to: 300; step: 5; suffix: " s" }
                ChoiceSetting {
                    key: "playback/defaultSpeed"; label: qsTr("Default playback speed")
                    choices: [{ text: "0.25×", value: 0.25 }, { text: "0.5×", value: 0.5 }, { text: "0.75×", value: 0.75 }, { text: "1×", value: 1.0 }, { text: "1.25×", value: 1.25 }, { text: "1.5×", value: 1.5 }, { text: "2×", value: 2.0 }]
                }
                SectionTitle { text: qsTr("Controls") }
                SliderSetting { key: "playback/autoHideMs"; label: qsTr("Hide controls after"); description: qsTr("0 keeps them visible."); from: 0; to: 10000; step: 500; suffix: " ms" }
                ChoiceSetting {
                    key: "playback/defaultAspect"; label: qsTr("Default aspect ratio")
                    choices: [{ text: qsTr("Auto"), value: "auto" }, { text: "16:9", value: "16:9" }, { text: "4:3", value: "4:3" }, { text: "21:9", value: "21:9" }, { text: qsTr("Fill (crop)"), value: "fill" }]
                }
                SectionTitle { text: qsTr("Screenshots") }
                ChoiceSetting { key: "screenshots/format"; label: qsTr("Format"); choices: [{ text: "PNG", value: "png" }, { text: "JPEG", value: "jpg" }] }
                SwitchSetting { key: "screenshots/clipboard"; label: qsTr("Also copy screenshots to the clipboard") }
                SettingRow {
                    label: qsTr("Screenshot folder"); description: Settings.values["screenshots/dir"] || qsTr("Default: Pictures/Lumina")
                    AppButton { text: qsTr("Choose…"); onClicked: Actions.chooseScreenshotDirRequested() }
                    AppButton { kind: "ghost"; text: qsTr("Reset"); visible: !!Settings.values["screenshots/dir"]; onClicked: Settings.reset("screenshots/dir") }
                }
            }

            // ============================================================ Audio
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Output") }
                SettingRow {
                    label: qsTr("Output device"); description: qsTr("Applies immediately.")
                    AppComboBox {
                        id: devCombo
                        width: 260
                        textRole: "name"; valueRole: "id"
                        model: Player.audioDevices()
                        onActivated: Player.setAudioDevice(currentValue)
                    }
                }
                SliderSetting { key: "audio/volume"; label: qsTr("Startup volume"); from: 0; to: 100; step: 1; suffix: " %" }
                SwitchSetting { key: "audio/muted"; label: qsTr("Start muted") }
                SectionTitle { text: qsTr("Equaliser") }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: qsTr("The equaliser, presets and saved profiles are available in the player's Audio panel (Shift+A) while a video is playing.")
                    color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
                }
            }

            // ============================================================ Subtitles
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Selection") }
                SwitchSetting { key: "subtitles/autoEnable"; label: qsTr("Turn on subtitles automatically"); description: qsTr("Uses the best matching subtitle file or track for your preferred languages.") }
                SettingRow {
                    label: qsTr("Preferred languages"); description: qsTr("Comma separated, most preferred first (e.g. eng, nep, spa).")
                    AppTextField {
                        width: 220
                        text: (Settings.values["subtitles/languages"] || []).join(", ")
                        onEditingFinished: Settings.set("subtitles/languages", text.split(",").map(function(s) { return s.trim() }).filter(function(s) { return s !== "" }))
                    }
                }
                SectionTitle { text: qsTr("Appearance") }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: qsTr("Style changes are read by libVLC when a video starts. Press “Apply to current video” to reopen the current video at the same position with the new style.")
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                }
                SliderSetting { key: "subtitles/size"; label: qsTr("Text size"); from: 8; to: 48; step: 1 }
                SwitchSetting { key: "subtitles/bold"; label: qsTr("Bold") }
                SettingRow {
                    label: qsTr("Text colour")
                    Row {
                        spacing: 8
                        Repeater {
                            model: ["#FFFFFF", "#FFE066", "#7CF0FF", "#9BFF9B", "#FFB3C7"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 26; height: 26; radius: 13; color: modelData
                                border.width: String(Settings.values["subtitles/color"]).toLowerCase() === modelData.toLowerCase() ? 3 : 1
                                border.color: String(Settings.values["subtitles/color"]).toLowerCase() === modelData.toLowerCase() ? Theme.accent : Theme.borderStrong
                                activeFocusOnTab: true
                                Keys.onReturnPressed: Settings.set("subtitles/color", modelData)
                                Keys.onSpacePressed: Settings.set("subtitles/color", modelData)
                                Accessible.role: Accessible.RadioButton; Accessible.name: modelData
                                MouseArea { anchors.fill: parent; onClicked: Settings.set("subtitles/color", modelData) }
                            }
                        }
                    }
                }
                ChoiceSetting {
                    key: "subtitles/outline"; label: qsTr("Outline")
                    choices: [{ text: qsTr("None"), value: 0 }, { text: qsTr("Thin"), value: 1 }, { text: qsTr("Normal"), value: 2 }, { text: qsTr("Thick"), value: 3 }]
                }
                SwitchSetting { key: "subtitles/shadow"; label: qsTr("Shadow") }
                SliderSetting { key: "subtitles/bgOpacity"; label: qsTr("Background opacity"); from: 0; to: 255; step: 5 }
                SliderSetting { key: "subtitles/marginBottom"; label: qsTr("Distance from bottom"); from: 0; to: 100; step: 1 }
                SliderSetting { key: "subtitles/delayStep"; label: qsTr("Delay step"); description: qsTr("Used by the subtitle delay shortcuts."); from: 50; to: 1000; step: 50; suffix: " ms" }
                SettingRow {
                    label: qsTr("Apply to current video")
                    AppButton { enabled: Player.active; iconName: "refresh"; text: qsTr("Apply now"); onClicked: Player.applySubtitleStyle() }
                }
                SectionTitle { text: qsTr("Online subtitle search") }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: qsTr("Online subtitle search is not part of this build. Local subtitle files next to the video, in Subs/ folders, or loaded manually are fully supported.")
                    color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
                }
            }

            // ============================================================ Library
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Folders") }
                Repeater {
                    model: Library.folders
                    delegate: SettingRow {
                        required property var modelData
                        label: modelData.path
                        description: qsTr("%n item(s)", "", modelData.count || 0)
                        IconButton { iconName: "folder"; tip: qsTr("Open folder"); onClicked: App.openFolderInFileManager(modelData.path) }
                        IconButton {
                            iconName: "trash"; tip: qsTr("Remove folder from library"); iconColor: Theme.danger
                            onClicked: Actions.confirm(qsTr("Remove folder?"), qsTr("“%1” and the items found in it will be removed from Lumina's library. No files are deleted.").arg(modelData.path), qsTr("Remove"), true, function() { Library.removeFolder(modelData.path, true) })
                        }
                    }
                }
                SettingRow {
                    label: Library.folders.length === 0 ? qsTr("No folders added") : qsTr("Add another folder")
                    AppButton { kind: "primary"; iconName: "folder-plus"; text: qsTr("Add folder…"); onClicked: Actions.addLibraryFolderRequested() }
                    AppButton { iconName: "refresh"; text: Library.scanning ? qsTr("Scanning…") : qsTr("Rescan now"); enabled: !Library.scanning && Library.folders.length > 0; onClicked: Library.rescan() }
                }
                SectionTitle { text: qsTr("Scanning") }
                SwitchSetting { key: "library/autoScan"; label: qsTr("Scan when Lumina starts") }
                SwitchSetting { key: "library/localArtwork"; label: qsTr("Use poster/folder images next to videos as thumbnails") }
                SwitchSetting { key: "library/removeMissing"; label: qsTr("Forget files that disappear from disk"); description: qsTr("Off: they are only marked as missing and come back if the drive reconnects.") }
                SectionTitle { text: qsTr("Database") }
                SettingRow {
                    label: qsTr("Library database"); description: Library.databasePath()
                    AppButton { iconName: "folder"; text: qsTr("Show"); onClicked: App.openFolderInFileManager(App.dataDir) }
                    AppButton { kind: "danger"; iconName: "trash"; text: qsTr("Clear library…"); onClicked: Actions.confirm(qsTr("Clear the library?"), qsTr("All library entries, watch progress, favorites, bookmarks and playlists will be erased. Your media files are never touched."), qsTr("Clear library"), true, function() { Library.clearLibrary() }) }
                }
            }

            // ============================================================ Keyboard & mouse
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Mouse") }
                ChoiceSetting {
                    key: "mouse/wheelAction"; label: qsTr("Mouse wheel over video")
                    choices: [{ text: qsTr("Change volume"), value: "volume" }, { text: qsTr("Seek"), value: "seek" }, { text: qsTr("Nothing"), value: "none" }]
                }
                SwitchSetting { key: "mouse/clickPause"; label: qsTr("Click video to play / pause") }
                SwitchSetting { key: "mouse/doubleClickFullscreen"; label: qsTr("Double-click video for fullscreen") }
                SwitchSetting { key: "mouse/gestures"; label: qsTr("Gestures"); description: qsTr("Drag horizontally on the video to seek; drag the right half vertically for volume.") }
                SectionTitle { text: qsTr("Keyboard shortcuts") }
                ShortcutEditor { Layout.fillWidth: true }
            }

            // ============================================================ Performance
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Decoding") }
                ChoiceSetting {
                    key: "performance/hwDecoding"; label: qsTr("Hardware decoding"); description: qsTr("“Automatic” lets libVLC use your GPU decoder when available and falls back to software. Applies to the next video you open.")
                    choices: [{ text: qsTr("Automatic"), value: "auto" }, { text: qsTr("Software only"), value: "none" }]
                }
                SliderSetting { key: "performance/networkCacheMs"; label: qsTr("Network buffer"); description: qsTr("Larger values survive slow networks but add delay."); from: 300; to: 15000; step: 100; suffix: " ms" }
                SectionTitle { text: qsTr("Thumbnails") }
                SwitchSetting { key: "performance/thumbnails"; label: qsTr("Generate thumbnails from video frames") }
                SwitchSetting { key: "performance/seekPreview"; label: qsTr("Frame preview on the seek bar"); description: qsTr("Decodes a frame when you hover the timeline (local files only; uses extra CPU, and previews take a moment to appear).") }
                SliderSetting { key: "performance/thumbCacheMB"; label: qsTr("Thumbnail cache limit"); from: 32; to: 2048; step: 32; suffix: " MB" }
                SettingRow {
                    label: qsTr("Thumbnail cache"); description: App.formatBytes(App.thumbnailCacheBytes)
                    AppButton { iconName: "trash"; text: qsTr("Clear"); onClicked: App.clearThumbnailCache() }
                }
                SectionTitle { text: qsTr("Playback engine") }
                SettingRow { label: qsTr("Engine"); description: qsTr("libVLC %1").arg(Player.engineVersion) }
            }

            // ============================================================ Privacy
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("History") }
                SwitchSetting { key: "privacy/historyEnabled"; label: qsTr("Remember what I watch"); description: qsTr("Turn off to stop recording history, resume points and recent streams.") }
                ChoiceSetting {
                    key: "privacy/historyDays"; label: qsTr("Keep history for")
                    choices: [{ text: qsTr("Forever"), value: 0 }, { text: qsTr("30 days"), value: 30 }, { text: qsTr("90 days"), value: 90 }, { text: qsTr("1 year"), value: 365 }]
                }
                SliderSetting { key: "privacy/recentStreams"; label: qsTr("Recent stream addresses to keep"); from: 0; to: 50; step: 1 }
                SettingRow {
                    label: qsTr("Clear history"); description: qsTr("Removes watch history, resume points and the recent-stream list.")
                    AppButton { kind: "danger"; iconName: "trash"; text: qsTr("Clear history…"); onClicked: Actions.confirm(qsTr("Clear history?"), qsTr("Watch history, resume points and recent stream addresses will be erased."), qsTr("Clear"), true, function() { Library.clearHistory(); App.forgetStreams() }) }
                }
                SettingRow {
                    label: qsTr("Erase all private data"); description: qsTr("History, recent streams, thumbnails and the in-app log.")
                    AppButton { kind: "danger"; iconName: "shield"; text: qsTr("Erase…"); onClicked: Actions.confirm(qsTr("Erase private data?"), qsTr("History, recent streams, cached thumbnails and the in-app log will be erased."), qsTr("Erase"), true, function() { App.clearAllPrivateData() }) }
                }
                SectionTitle { text: qsTr("Network") }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: qsTr("Lumina makes no network connections of its own. It never uploads your library, history or file names. The only traffic is the stream you choose to open. Stream credentials are used for that session only: they are not saved and are masked in logs.")
                    color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody
                }
            }

            // ============================================================ Advanced
            SettingsScroll {
                SectionTitle { first: true; text: qsTr("Diagnostics") }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: diag.implicitHeight + 24
                    radius: Theme.radiusSmall + 2; color: Theme.surface; border.color: Theme.border
                    Text { id: diag; anchors { fill: parent; margins: 12 } text: App.diagnostics(); wrapMode: Text.WrapAnywhere; color: Theme.textDim; font.family: "monospace"; font.pixelSize: 12; textFormat: Text.PlainText }
                }
                SettingRow {
                    label: qsTr("Copy diagnostics")
                    AppButton { iconName: "copy"; text: qsTr("Copy"); onClicked: { App.copyText(App.diagnostics()); Actions.success(qsTr("Copied.")) } }
                    AppButton { iconName: "folder"; text: qsTr("Open data folder"); onClicked: App.openFolderInFileManager(App.dataDir) }
                }
                SwitchSetting { key: "performance/verboseEngineLog"; label: qsTr("Verbose engine log"); description: qsTr("Includes libVLC messages in the log below. Applies to the next start.") }
                SectionTitle { text: qsTr("Log") }
                LogViewer { Layout.fillWidth: true }
                SectionTitle { text: qsTr("Reset") }
                SettingRow {
                    label: qsTr("Reset all settings"); description: qsTr("Restores every option, including shortcuts, to its default. The library is not touched.")
                    AppButton { kind: "danger"; iconName: "rotate"; text: qsTr("Reset…"); onClicked: Actions.confirm(qsTr("Reset all settings?"), qsTr("Every setting and shortcut returns to its default value."), qsTr("Reset"), true, function() { Settings.resetAll(); Shortcuts.resetAll() }) }
                }
                SectionTitle { text: qsTr("About") }
                SettingRow { label: qsTr("Lumina %1").arg(App.version); description: qsTr("Qt %1 · libVLC %2 · %3").arg(App.qtVersion).arg(Player.engineVersion).arg(App.platform) }
            }
        }
    }
}
