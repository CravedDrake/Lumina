# Architecture

- `src/playback/` – `VlcEngine` (thin libVLC wrapper; software video callbacks into `VideoFrameSource`
  triple buffer), `PlaybackController` (QML-facing state: queue, tracks, chapters, A-B loop, resume,
  bookmarks, screenshots), `VideoItem` (QQuickItem painting frames; several items may share one source,
  used by the main view and the PiP window), `QueueModel`.
- `src/library/` – scanner, probe (libVLC metadata), SQLite-backed `LibraryManager`, `LibraryModel`.
- `src/database/` – per-thread SQLite connections, migrations, corruption recovery.
- `src/settings/` – validated QSettings wrapper with defaults. `src/shortcuts/` – rebindable actions with conflict detection.
- `src/thumbnails/` – frame grabber, JPEG cache, `image://thumb`. `src/app/` – `AppController`, `image://icon`.
- `src/playlists/` – M3U/M3U8 import/export. `src/subtitles/` – local subtitle discovery.
- `qml/` – `Lumina.UI` module: theme singleton, components, pages, player overlays/panels, dialogs, `Main.qml`.

Subtitle appearance note: libVLC reads `--freetype-*` options only at instance creation, so changing
subtitle style restarts the libVLC instance and reopens the current media at the same position.
