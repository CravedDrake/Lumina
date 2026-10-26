# Manual QA checklist
Tick on each target OS (Linux, Windows). Items marked ⚠ could NOT be verified by the author.

## Startup / window
- [ ] Launch with no args; home page shows; add a folder; scan completes with thumbnails
- [ ] Title bar drag, double-click maximize, min/max/close, edge resize
- [ ] Window size/position restored on relaunch
- [ ] Sidebar collapse/expand

## Playback
- [ ] Open file (dialog, drag-drop, double-click in library, CLI arg)
- [ ] Play/pause, seek (click, drag, arrows), volume, mute, speed, frame step
- [ ] Controls auto-hide and cursor hides in fullscreen; reappear on mouse move
- [ ] Fullscreen (F / double-click / Esc)
- [ ] Queue: next/previous, shuffle, repeat modes, reorder, remove
- [ ] A-B loop (L), chapters, bookmarks, screenshots (C)
- [ ] Resume prompt/auto/never per setting
- [ ] Corrupt/non-media file gives an error message, app stays responsive

## Tracks / appearance
- [ ] Audio track switch, audio delay, EQ presets, stereo mode
- [ ] ⚠ Audio output device switch on real hardware
- [ ] Subtitle load/select/off, delay, size/colour/outline (note brief reopen)
- [ ] Video adjustments (brightness/contrast/saturation/hue/gamma), deinterlace
- [ ] ⚠ Hardware decoding preference with a supported GPU

## Library / playlists / streams
- [ ] Search, sort, grid/list; movies/TV classification; fix title
- [ ] Create playlist, add items, drag the handle / Move up/down, remove, export/import M3U
- [ ] ⚠ Open http/https/rtsp stream; URL with credentials is not stored in history or logs
- [ ] History, clear history, clear all private data

## Misc
- [ ] Launch a second copy with a file: it plays in the existing window
- [ ] Enable Settings → Performance → Frame preview; hover the seek bar (preview appears after ~0.5 s)

## Window modes
- [ ] ⚠ Picture-in-picture window, always-on-top
- [ ] Shortcut editor: rebind, conflict warning, reset

## Settings
- [ ] Every page opens; changes persist after restart; reset to defaults
