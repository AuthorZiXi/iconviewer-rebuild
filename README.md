# IconViewer Rebuild

<p align="center"><img src="docs/icon.png" width="120" alt="IconViewer Rebuild"></p>

A small portable Windows tool that shows **every icon and device image inside a program,
library or icon file** — a from-scratch Win32 reimplementation of the classic
[IconViewer 3.02](https://www.botproductions.com/iconview/iconview.html) Explorer property
page, rebuilt as a standalone app.

No installation, no shell extension, no dependencies: one `IconViewerRebuild.exe`.

![IconViewer Rebuild](docs/screenshots/rebuild_main.png)

---

## Features

* **Look inside anything with icons** — `.exe`, `.dll`, `.ocx`, `.cpl`, `.scr`, `.ico`, `.cur`.
  Reads `RT_GROUP_ICON` resources plus stray `RT_ICON`s, and parses icon files directly
  (signature based, not extension based).
* **All device images** — every size and colour depth in an icon, labelled the way the
  original did: `256x256, 32-bit (PNG)`, `48x48, 256 colors`, …
* **Preview + magnifier flyout** — 1:1 preview; images that do not fit pop out anchored to
  the preview box.
* **Multi-select and drag out** — select many icons with Ctrl/Shift or Ctrl+A and drag them
  straight to Explorer or the desktop as real `.ico` files.
* **Copy that actually pastes** — puts `CF_HDROP` (files) *and* `CF_DIB`/`CF_BITMAP` on the
  clipboard, so Explorer pastes icon files while Paint/Word paste the bitmap.
* **Export** — write the selected icons (or all of them) to a folder in one go.
* **Save As** — whole icon → `.ico`, single device image → `.bmp` (background colour blended)
  or `.png` (alpha preserved).
* **Options** — background colour, device-image ordering (size / colour depth, asc / desc),
  multi-file behaviour, grouping by file.
* **English & 简体中文**, follows the system language and can be switched at runtime.
* **Long resource names** are ellipsised in the list and shown in full on hover.

## Download

Grab `IconViewerRebuild.exe` from the [releases](../../releases) page or from a CI build
artifact. x64, Windows 7 or newer, no runtime to install.

## Build

Requirements: Visual Studio 2022/2026 Build Tools (MSVC + Windows 10/11 SDK).

```bat
build.bat
:: -> build\IconViewerRebuild.exe
```

or with CMake:

```bat
cmake -S . -B out -A x64
cmake --build out --config Release
:: -> out\Release\IconViewerRebuild.exe
```

The `build` workflow in `.github/workflows` produces the same binary on `windows-latest`.

## Usage

```
IconViewerRebuild.exe [files...]
```

* Open files with **File > Open** (Ctrl+O), by dragging them onto the window, or on the
  command line.
* Pick an icon to see its device images; pick an image to preview it.
* **Ctrl+A** select all, **Ctrl+C** copy, **Ctrl+S** save as, **F5** refresh, **F1** help.

| Action | How |
| --- | --- |
| Look at icons | open a file / drop it on the window |
| See a bigger preview | select a large image — the flyout follows the preview box |
| Copy files | Ctrl+C, then paste in Explorer |
| Copy a bitmap | Ctrl+C, then paste in Paint/Word |
| Drag icons out | drag a selection to the desktop or a folder |
| Export many | File > Export Icons… |
| Change look/order | View > Options… |
| Change language | View > Language |

## How it works

* `LoadLibraryExW(LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)` +
  `EnumResourceNamesW(RT_GROUP_ICON)` for modules; `ICONDIR` parsing for `.ico`/`.cur`.
* Icon DIBs are decoded by hand (1/4/8/16/24/32 bpp, `BI_BITFIELDS`, the "all-zero alpha →
  AND mask" fallback, and the doubled `biHeight` of icon DIBs). PNG frames go through GDI+.
* List thumbnails are built by picking the frame closest to 32×32 with the highest colour
  depth, scaling it and calling `CreateIconIndirect`.
* Drag & drop uses a small `IDataObject`/`IDropSource` pair with `CF_HDROP` + `CF_DIB`.

More background on the original: [docs/analysis.md](docs/analysis.md).

## Relationship to the original IconViewer

This is an **independent reimplementation**, not a fork. No code, binaries, icons or help
files from the Bot Productions package are used or redistributed — the app icon is drawn by
`tools/icon`, the UI is rebuilt from scratch. "IconViewer" is used descriptively.
See [ATTRIBUTION.md](ATTRIBUTION.md).

## Layout

```
src/            C++ sources (common, strings, bitmap, icofile, dataobject, settings, preview, main)
res/            app.rc, manifest, generated app.ico
tools/icon/     icon renderer + ICO packer (PowerShell + Node)
tests/          console harnesses for loading and saving
docs/           analysis notes and screenshots
```

## Regenerating the icon

```bat
powershell -File tools\icon\render-icon.ps1
node tools\icon\make-ico.js
```

## License

MIT — see [LICENSE](LICENSE). Original IconViewer is © 2008 Bot Productions and is **not**
part of this project.
