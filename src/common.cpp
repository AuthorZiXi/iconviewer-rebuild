#include "common.h"
#include <cstdarg>
#include <cstdio>

HINSTANCE g_hInst = nullptr;
int g_dpi = 96;

std::wstring Fmt(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    return std::wstring(buf);
}

std::wstring SysErrorText(DWORD err) {
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, (LPWSTR)&msg, 0, nullptr);
    std::wstring s = msg ? msg : L"";
    if (msg) LocalFree(msg);
    // Drop the "%1" insertion placeholder that IGNORE_INSERTS leaves behind.
    size_t pos;
    while ((pos = s.find(L"%1")) != std::wstring::npos) s.erase(pos, 2);
    while (!s.empty() && (s.front() == L' ')) s.erase(s.begin());
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' '))
        s.pop_back();
    if (s.empty()) s = Fmt(L"error %lu", (unsigned long)err);
    return s;
}

std::wstring FileNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

std::wstring FileStemOf(const std::wstring& path) {
    std::wstring name = FileNameOf(path);
    size_t p = name.find_last_of(L'.');
    if (p == std::wstring::npos || p == 0) return name;
    return name.substr(0, p);
}

std::wstring FileExtOf(const std::wstring& path) {
    std::wstring name = FileNameOf(path);
    size_t p = name.find_last_of(L'.');
    std::wstring ext = (p == std::wstring::npos) ? L"" : name.substr(p);
    for (auto& c : ext) c = (wchar_t)towlower(c);
    return ext;
}

std::wstring DirNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? std::wstring() : path.substr(0, p);
}

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) { CloseHandle(h); return false; }
    if (size.QuadPart < 0 || size.QuadPart > 256ll * 1024 * 1024) { CloseHandle(h); return false; }
    out.resize((size_t)size.QuadPart);
    DWORD total = 0;
    while (total < out.size()) {
        DWORD got = 0;
        if (!ReadFile(h, out.data() + total, (DWORD)(out.size() - total), &got, nullptr) || got == 0)
            break;
        total += got;
    }
    CloseHandle(h);
    out.resize(total);
    return total > 0;
}

void CenterWindowOnParent(HWND wnd, HWND parent) {
    RECT rc, rp;
    GetWindowRect(wnd, &rc);
    GetWindowRect(parent, &rp);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = rp.left + (rp.right - rp.left - w) / 2;
    int y = rp.top + (rp.bottom - rp.top - h) / 2;
    SetWindowPos(wnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
