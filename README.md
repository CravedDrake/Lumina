# Lumina

A native desktop video player: C++17, Qt 6 (Qt Quick / QML), libVLC 3 for decoding,
SQLite for the library, QSettings for preferences. Video is rendered from real decoded
frames (libVLC video callbacks → Qt scene graph), not a placeholder.

Highlights: custom dark title bar, library scanner with real thumbnails, queue,
A-B loop, chapters, bookmarks, resume, subtitle and audio-track selection, EQ,
video adjustments, playlists (M3U/M3U8), network streams, picture-in-picture,
rebindable shortcuts, drag-and-drop.

See:
- [docs/BUILD.md](docs/BUILD.md) – build, run, test (Linux and Windows)
- [docs/FEATURES.md](docs/FEATURES.md) – **honest status: what is implemented, tested, partial or missing**
- [docs/QA_CHECKLIST.md](docs/QA_CHECKLIST.md) – manual QA
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) – code layout
- [packaging/README.md](packaging/README.md) – deploying

Privacy: no network calls except media streams you open yourself. No telemetry, no
online lookups, no API keys. Stream URLs containing credentials are never stored or logged.
