# Packaging

## Linux
- `lumina.desktop` – desktop entry. Install with `cmake --install build --prefix /usr/local`
  and copy the desktop file to `share/applications/`.
- `deploy-linux.sh` – builds Release and produces a tarball in `dist/` (relies on system Qt/libVLC; not a bundled AppImage).

## Windows installer (.exe)
The Windows installer has NOT been built or run by the author: it needs a Windows machine and the Windows Qt/VLC downloads, which were not reachable from the Linux build environment.
Two routes, both untested:
- GitHub Actions: push this folder to a GitHub repo and run the *Windows installer* workflow (`.github/workflows/windows.yml`); download `Lumina-0.1.0-Setup.exe` and `Lumina-portable.zip` from the run's artifacts.
- Locally on Windows: `packaging\windows\build-windows.ps1 -QtDir C:\Qt\6.5.3\msvc2019_64 -VlcDir C:\path\vlc-3.0.20` (needs VS Build Tools, CMake, Ninja, Inno Setup).
No .msi is produced; the installer is an Inno Setup .exe. The app has no custom .ico yet (default exe icon).

## Windows (manual steps)
1. Build Release (see docs/BUILD.md).
2. `windeployqt --qmldir qml build\lumina.exe` (also deploys Svg, Sql/sqlite plugins).
3. Copy from the VLC 3.0.x zip next to `lumina.exe`: `libvlc.dll`, `libvlccore.dll`, `plugins\`.
   Run `vlc-cache-gen plugins` if startup is slow.
4. Ship `LICENSE`, Qt and VLC license notices (LGPL; keep both dynamically linked).
