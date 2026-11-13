# Changelog

## 1.0.0 — first public release

Initial open-source release of IconViewer Rebuild, an independent Win32 reimplementation of
IconViewer 3.02 (Bot Productions).

### Features

* Lists the icons stored in programs, libraries, control panel applets, `.ico` and `.cur` files
  (`RT_GROUP_ICON` + orphan `RT_ICON` resources, raw ICONDIR parsing).
* Device-image list with size / colour-depth ordering, 1:1 preview and a flyout for images
  larger than the preview area (anchored to the preview box).
* Multi-select icon list, Ctrl+A / Ctrl+C, drag icons straight to Explorer or the desktop.
* Copy puts real files on the clipboard (`CF_HDROP`) *and* a bitmap (`CF_DIB`/`CF_BITMAP`).
* Save As: whole icon → `.ico`, device image → `.bmp` (background colour) or `.png` (alpha kept).
* Export selected / all icons to a folder in one go.
* Options: background colour, device-image ordering, multi-file behaviour, grouping by file.
* English and Simplified Chinese UI, follows the system language, switchable at runtime.
* Own application icon, MIT licensed, no assets from the original package.
