#pragma once
#include "common.h"
#include "bitmap.h"

// Temporary folder used for drag/copy payloads: %TEMP%\IconViewer\<pid>
std::wstring TempExportDir();
void CleanupTempDir();
// Returns a path inside dir that does not exist yet.
std::wstring UniquePath(const std::wstring& dir, const std::wstring& base, const wchar_t* ext);

// Puts the files on the clipboard as CF_HDROP (so Explorer can paste them) and,
// when an image is supplied, also as CF_DIB / CF_BITMAP.
bool CopyFilesToClipboard(HWND owner, const std::vector<std::wstring>& files, const Bitmap32* image);

// Starts an OLE drag of the given files (Explorer accepts them as .ico files).
bool StartFileDrag(HWND owner, const std::vector<std::wstring>& files, const Bitmap32* image);
