# Building Lumina

## Requirements
- CMake ≥ 3.21, a C++17 compiler
- Qt ≥ 6.4: Core, Gui, Qml, Quick, QuickControls2, Sql, Concurrent, Network, Svg
  (plus the SQLite driver plugin, `qsqlite`)
- libVLC 3.x development files (developed against 3.0.20)
- Tests only: `ffmpeg` on PATH (generates test media into `build/testdata`; without it media tests are skipped)

## Linux (Debian/Ubuntu)
```
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-svg-dev libqt6sql6-sqlite libvlc-dev vlc-plugin-base ffmpeg \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/lumina [file-or-url ...]
```
Package names vary by distribution release; the QML runtime modules must be present at run time.

## Windows
Not built or tested by the author (no Windows machine was available). Intended route:
1. Install Qt 6.4+ (MSVC or MinGW kit) and the VLC 3.0.x **zip** package (it contains `sdk/`).
2. `cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt> -DLIBVLC_ROOT=<vlc>/sdk`
   (`FindLibVLC.cmake` accepts `LIBVLC_ROOT` containing `include/vlc/vlc.h` and `lib/libvlc.lib`).
3. Build, then see `packaging/README.md` for `windeployqt` and copying `libvlc.dll`, `libvlccore.dll`, `plugins/`.

## Tests
```
cd build
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software LUMINA_AUDIO_DUMMY=1 ctest --output-on-failure
```
Suites: `tst_controller` (playback controller against real libVLC), `tst_engine`, `tst_database`,
`tst_library`, `tst_logic`, `tst_support`, and `ui_selftest` (launches the real QML window and sends key events).
Last run by the author on Linux/offscreen/software renderer: 7/7 passed.

## Environment variables (diagnostics/testing)
- `LUMINA_DATA_DIR` – use a separate data directory (database, thumbnails, logs)
- `LUMINA_AUDIO_DUMMY=1` – libVLC dummy audio output (no sound device needed)

## Command-line options
`--fullscreen`, `--ui-selftest`, `--ui-screenshot <file>` (+`--ui-screenshot-delay`, `--page`, `--panel`,
`--settings-category`), `--add-library-folder <dir>`. `tests/ui_shots.sh` renders review screenshots.

Logs: `<data dir>/logs/`.
