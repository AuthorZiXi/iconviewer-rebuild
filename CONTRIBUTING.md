# Contributing

Thanks for taking a look! A few ground rules keep the project tidy:

1. **Build & test first.** `build.bat` (or CMake) must produce `build\IconViewerRebuild.exe`
   without warnings, and `build\test_load.exe C:\Windows\System32\imageres.dll` must pass.
2. **Match the existing style.** 4-space indent, ```camelCase``` locals, ```PascalCase``` types,
   Win32 `W` APIs only, no exceptions across the window procedure.
3. **Keep it dependency-free.** Plain Win32 + GDI+ + COM; no third-party libraries, no ATL/MFC.
4. **UI text goes through `strings.cpp`** — add the string to both the English and the Chinese
   table, never hard-code a user-visible literal.
5. **Do not add assets from the original IconViewer package.** Icons, bitmaps and help files
   must be original work (see `ATTRIBUTION.md`).
6. Open a PR with a short description and, for UI changes, a screenshot.
