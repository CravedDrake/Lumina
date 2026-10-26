# Feature status (honest report)

Verification level on Linux, offscreen platform, software Qt Quick renderer, dummy audio output.
"Tested" = covered by an automated test that was actually run (7/7 ctest suites passing at time of writing).

## Implemented and automatically tested
- Playback via libVLC: play/pause/seek/volume (0–200 %)/speed/frame-step, queue, repeat, auto-advance, A-B loop, errors, rapid switching
- Real decoded frames in the UI (screenshot-verified); screenshots; bookmarks; resume always/ask/never
- Audio/subtitle track selection and chapters from a real MKV; audio/subtitle delay setters
- Subtitle rendering and style changes (pixel-verified); brightness, contrast, saturation, gamma and hue adjustments (measured on decoded frames, plus reset)
- Seek-bar preview frames: decoding, 5 s bucket cache (unit-tested)
- SQLite database/migrations, library scan, thumbnails from real frames, sidecar artwork
- Settings validation/persistence, shortcut conflicts/persistence, M3U round-trip, credential redaction, time parsing
- Real-window key handling (`ui_selftest`, 16 checks)

## Implemented, verified only by running/looking (not automated)
- Single instance: a second launch passes its files to the running window (checked by launching two processes)
- Pages, dialogs, panels, settings pages (offscreen screenshots reviewed), playlist Move up/down menu items and drag handle (handle rendering screenshot-checked; the mouse drag itself was not exercised), drag-and-drop, stream dialog, shortcut editor UI

## Partial
- Subtitle style change restarts the libVLC instance and reopens media at the same position
- Deinterlace modes are passed to libVLC but not verified (no interlaced test source)
- EQ/profiles tested at engine level, not through the UI
- Seek-bar frame preview is opt-in (Settings → Performance, off by default), local files only, decodes in a separate libVLC player so previews appear after a short delay; the hover UI itself was not exercised offscreen

## Not implemented
- Online subtitle search (settings page states this; no online code, API keys or telemetry exist)

## Not verified (could not be tested here)
- Windows build and packaging
- GPU hardware decoding (preference is exposed)
- Real audio-device switching
- Real network streams / credentialed URLs end to end (only redaction logic is unit-tested)
- Picture-in-picture and always-on-top behaviour on a real window manager
- Native file dialogs; GPU (non-software) Qt rendering

## Process note
Early in development an overwrite of nine pre-existing files in the working directory occurred;
they were reconstructed from the session transcript. Review the git history/diff before relying on any file
that predated this project.
