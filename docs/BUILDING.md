# Building IconViewer Rebuild

## Requirements

| Tool | Why |
| --- | --- |
| Visual Studio 2022/2026 **Build Tools** with *Desktop development with C++* | MSVC, `rc.exe`, Windows SDK headers/libs |
| Windows 10/11 SDK | `gdiplus`, `shlobj`, `commctrl` |
| PowerShell 5.1+ | only for the icon / social-card generators in `tools/` |
| Node.js 16+ | only for `tools/icon/make-ico.js` (ICO packing) |

No third-party libraries are used at runtime: plain Win32, GDI+ and COM.

## Quick build (MSVC)

```bat
build.bat
:: -> build\IconViewerRebuild.exe
```

The script calls `vcvars64.bat`, compiles all sources with `/W4 /O2 /MT`, builds
`res\app.rc` and links a single self-contained exe (static CRT, no redistributable).

## CMake

```bat
cmake -S . -B out -A x64
cmake --build out --config Release
:: -> out\Release\IconViewerRebuild.exe
```

The Visual Studio generator normally embeds its own manifest; `CMakeLists.txt` passes
`/MANIFEST:NO` because `res/app.rc` already embeds ours (otherwise the linker fails with
`CVT1100: duplicate resource ... MANIFEST`).

## Tests

Two console harnesses share the production code:

```bat
build.bat                    :: builds the app
test_build.bat               :: builds build\test_load.exe
test_build2.bat              :: builds build\test_save.exe

build\test_load.exe C:\Windows\System32\imageres.dll
build\test_save.exe C:\Windows\System32\imageres.dll out
```

* `test_load` enumerates every icon in the given files and decodes each device image.
* `test_save` writes `.bmp`, `.png` and a rebuilt `.ico` for each frame and re-loads the
  `.ico` through `LoadImage` to prove it is valid.

## Regenerating assets

```bat
powershell -File tools\icon\render-icon.ps1   :: draws 16..256 px PNGs
node tools\icon\make-ico.js                   :: packs res\app.ico
powershell -File tools\social\make-social-preview.ps1   :: docs\social-preview.png
```

## Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `RC1004: unexpected end of file` | `res/app.rc` or `src/resource.h` lost its trailing newline — `rc.exe` needs one |
| `CVT1100: duplicate resource ... MANIFEST` | two manifests; build with `/MANIFEST:NO` (CMakeLists already does) |
| `cmake is not recognized` | CMake ships with the Build Tools: `...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` |
| `swprintf_s` / `_countof` not found | the code targets MSVC; MinGW/clang-cl are not supported yet |
| Icons missing in the list | the file's group icon references an `RT_ICON` that does not exist; the app skips that frame by design |

## Code map

```
src/main.cpp        window, menus, lists, dialogs, commands
src/icofile.cpp     resource enumeration + ICO/CUR parsing + ICO rebuilding
src/bitmap.cpp      DIB/PNG decoding, scaling, alpha blending, BMP/PNG/DIB output
src/dataobject.cpp  temp files, CF_HDROP clipboard, OLE drag source
src/strings.cpp     English/Chinese string tables
src/preview.cpp     preview control + flyout window
src/settings.cpp    registry persistence
```
