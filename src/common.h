#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

extern HINSTANCE g_hInst;
extern int g_dpi;

inline int Dpi(int v) { return MulDiv(v, g_dpi, 96); }

std::wstring Fmt(const wchar_t* fmt, ...);
std::wstring SysErrorText(DWORD err);
std::wstring FileNameOf(const std::wstring& path);
std::wstring FileStemOf(const std::wstring& path);
std::wstring FileExtOf(const std::wstring& path);
std::wstring DirNameOf(const std::wstring& path);
bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out);
void CenterWindowOnParent(HWND wnd, HWND parent);
