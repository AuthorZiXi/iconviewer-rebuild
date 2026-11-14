# IconViewer Rebuild 1.0.0

A standalone Windows tool that shows **every icon and device image inside a program, library
or icon file** — an independent Win32 reimplementation of the classic
[IconViewer 3.02](https://www.botproductions.com/iconview/iconview.html) Explorer property
page, rebuilt as a normal application.

## Download

| File | Notes |
| --- | --- |
| `IconViewerRebuild.exe` | x64, single file, no installer, no runtime required. Windows 7 or newer. |

## Highlights

* Reads `RT_GROUP_ICON` resources plus stray `RT_ICON`s, and parses `.ico` / `.cur` directly.
* Lists **all device images** with the original wording: `256x256, 32-bit (PNG)`,
  `48x48, 256 colors`, …
* Preview with a magnifier flyout anchored to the preview box.
* Multi-select (Ctrl/Shift, Ctrl+A), **drag icons out** to Explorer or the desktop.
* **Copy** puts real files (`CF_HDROP`) *and* a bitmap (`CF_DIB`/`CF_BITMAP`) on the clipboard —
  Explorer pastes icon files, Paint/Word paste the bitmap.
* **Save As**: icon → `.ico`, device image → `.bmp` (background colour blended) or `.png`
  (alpha preserved). **Export** writes many icons to a folder at once.
* Options: background colour, device-image ordering, multi-file behaviour, grouping by file.
* English and Simplified Chinese UI, follows the system language, switchable at runtime.
* Long resource names are ellipsised in the list and shown in full on hover.

## Install

It is a single portable exe — put it anywhere and run it. There is nothing to install and
nothing is registered in Explorer. Settings live in
`HKCU\Software\IconViewerRebuild`.

## Checksum

```
SHA256  04418933a23a4599ca455711934b793419be2517ffdfee20a95d9ea0d7b07e2f  IconViewerRebuild.exe
```

Verify with `certutil -hashfile IconViewerRebuild.exe SHA256` (or `Get-FileHash`).

## Notes

* MIT licensed. The original IconViewer is © 2008 Bot Productions; this project is not
  affiliated with them and ships none of their code or assets.
* Built with **DSH + DeepSeek-V4.1-Flash (1017)** — see `CREDITS.md`.
* Size: 346,624 bytes. The CI build is byte-different (the PE timestamp), so re-check the hash
  if you upload the CI artifact instead of the local build.
