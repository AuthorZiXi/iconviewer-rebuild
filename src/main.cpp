#include "common.h"
#include "bitmap.h"
#include "icofile.h"
#include "settings.h"
#include "preview.h"
#include "strings.h"
#include "dataobject.h"
#include "resource.h"
#include "version.h"

#include <objbase.h>
#include <ole2.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uuid.lib")

static const wchar_t* kClassName = L"IconViewerRebuildMainWnd";
static const wchar_t* kAppName = APP_NAME;

static HWND g_hMain = nullptr;
static HWND g_hStatus = nullptr;
static HWND g_hIcons = nullptr;
static HWND g_hDevList = nullptr;
static HWND g_hPreview = nullptr;
static HWND g_hDevLabel = nullptr;
static HWND g_hPrevLabel = nullptr;
static HFONT g_hFont = nullptr;
static HIMAGELIST g_hIconImages = nullptr;
static HICON g_defaultIcon = nullptr;
static int g_iconSize = 32;

static std::vector<LoadedFile> g_files;
static std::vector<int> g_devOrder;
static Bitmap32 g_currentImage;
static int g_curFile = -1;
static int g_curIcon = -1;
static std::vector<std::wstring> g_startupFiles;

static std::wstring PickFolder(HWND owner, const wchar_t* title);
static void OnDeviceSelection();
static void OnIconSelection();

struct IconRef {
    int file = -1;
    int icon = -1;
};

// ---------------------------------------------------------------- tooltip

static HWND g_hTip = nullptr;
static TOOLINFOW g_tip = {};
static bool g_tipActive = false;

static void InitTooltip(HWND parent) {
    g_hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                             WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, parent, nullptr,
                             g_hInst, nullptr);
    if (!g_hTip) return;
    g_tip.cbSize = sizeof(g_tip);
    g_tip.uFlags = TTF_TRACK | TTF_ABSOLUTE | TTF_IDISHWND;
    g_tip.hwnd = parent;
    g_tip.uId = (UINT_PTR)parent;
    g_tip.lpszText = const_cast<LPWSTR>(L"");
    SendMessageW(g_hTip, TTM_ADDTOOLW, 0, (LPARAM)&g_tip);
    SendMessageW(g_hTip, TTM_SETMAXTIPWIDTH, 0, (LPARAM)Dpi(400));
}

static void ShowTip(const std::wstring& text, int x, int y) {
    if (!g_hTip) return;
    static std::wstring tipText;
    tipText = text;
    g_tip.lpszText = tipText.data();
    SendMessageW(g_hTip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&g_tip);
    SendMessageW(g_hTip, TTM_TRACKPOSITION, 0, MAKELPARAM(x + Dpi(16), y + Dpi(20)));
    if (!g_tipActive) {
        SendMessageW(g_hTip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&g_tip);
        g_tipActive = true;
    }
}

static void HideTip() {
    if (g_hTip && g_tipActive) {
        SendMessageW(g_hTip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&g_tip);
        g_tipActive = false;
    }
}

// ---------------------------------------------------------------- helpers

static IconRef RefFromLParam(LPARAM lp) {
    IconRef r;
    r.file = (int)(lp >> 16);
    r.icon = (int)(lp & 0xFFFF);
    return r;
}

static std::wstring SanitizeFileName(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c < 32 || wcschr(L"\\/:*?\"<>|", c))
            out.push_back(L'_');
        else
            out.push_back(c);
    }
    while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
    if (out.empty()) out = L"icon";
    return out;
}

static bool WriteAllBytes(const std::wstring& path, const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(f, data.data(), (DWORD)data.size(), &written, nullptr) &&
              written == data.size();
    CloseHandle(f);
    if (!ok) DeleteFileW(path.c_str());
    return ok;
}

static std::vector<uint8_t> IconBytes(const IconRef& r) {
    if (r.file < 0 || r.file >= (int)g_files.size()) return {};
    const LoadedFile& file = g_files[(size_t)r.file];
    if (r.icon < 0 || r.icon >= (int)file.icons.size()) return {};
    if (file.isIconFile && file.icons.size() == 1 && !file.rawBytes.empty()) return file.rawBytes;
    return BuildIcoBytes(file.icons[(size_t)r.icon]);
}

static std::wstring IconBaseName(const IconRef& r) {
    const LoadedFile& file = g_files[(size_t)r.file];
    const IconItem& icon = file.icons[(size_t)r.icon];
    std::wstring base = icon.named ? SanitizeFileName(icon.name) : Fmt(L"%u", (unsigned)icon.id);
    if (g_files.size() > 1) base = SanitizeFileName(FileStemOf(file.path)) + L"_" + base;
    return base;
}

static std::vector<IconRef> SelectedIconRefs() {
    std::vector<IconRef> out;
    int item = -1;
    while ((item = ListView_GetNextItem(g_hIcons, item, LVNI_SELECTED)) != -1) {
        LVITEMW it = {};
        it.mask = LVIF_PARAM;
        it.iItem = item;
        if (ListView_GetItem(g_hIcons, &it)) out.push_back(RefFromLParam(it.lParam));
    }
    return out;
}

static std::wstring FullLabelFor(const IconRef& r) {
    if (r.file < 0 || r.file >= (int)g_files.size()) return L"";
    const LoadedFile& file = g_files[(size_t)r.file];
    if (r.icon < 0 || r.icon >= (int)file.icons.size()) return L"";
    const IconItem& icon = file.icons[(size_t)r.icon];
    std::wstring base = icon.named ? icon.name : Fmt(L"#%u", (unsigned)icon.id);
    bool grouped = g_files.size() > 1 && GetSettings().groupFiles;
    if (g_files.size() > 1 && !grouped) {
        std::wstring fn = FileNameOf(file.path);
        return icon.named ? Fmt(L"%s %s", fn.c_str(), base.c_str())
                          : Fmt(L"%s #%u", fn.c_str(), (unsigned)icon.id);
    }
    return base;
}

static std::wstring Ellipsize(HDC dc, const std::wstring& text, int maxWidth) {
    if (text.empty()) return text;
    SIZE sz = {};
    if (!GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &sz)) return text;
    if (sz.cx <= maxWidth) return text;
    const wchar_t ell = 0x2026;
    SIZE esz = {};
    GetTextExtentPoint32W(dc, &ell, 1, &esz);
    int avail = maxWidth - esz.cx;
    if (avail <= 0) return std::wstring(1, ell);
    int lo = 0, hi = (int)text.size();
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        SIZE s2 = {};
        GetTextExtentPoint32W(dc, text.c_str(), mid, &s2);
        if (s2.cx <= avail)
            lo = mid;
        else
            hi = mid - 1;
    }
    return text.substr(0, (size_t)lo) + ell;
}

// ---------------------------------------------------------------- status / title

static void UpdateStatusText() {
    std::wstring text;
    if (g_files.empty()) {
        text = T(S_STATUS_EMPTY);
    } else if (g_files.size() == 1) {
        if (g_files[0].icons.empty())
            text = Fmt(T(S_STATUS_NOICONS), FileNameOf(g_files[0].path).c_str());
        else
            text = Fmt(T(S_STATUS_ONE), FileNameOf(g_files[0].path).c_str());
    } else {
        size_t withIcons = 0;
        for (const auto& f : g_files)
            if (!f.icons.empty()) ++withIcons;
        text = (withIcons == 0) ? T(S_STATUS_NOICONS_MULTI) : T(S_STATUS_MULTI);
    }
    if (g_hStatus) SetWindowTextW(g_hStatus, text.c_str());
}

static void UpdateTitle() {
    std::wstring t = kAppName;
    if (g_files.size() == 1)
        t = Fmt(L"%s - %s", kAppName, FileNameOf(g_files[0].path).c_str());
    else if (g_files.size() > 1)
        t = Fmt(T(S_TITLE_MULTI), (int)g_files.size());
    if (g_hMain) SetWindowTextW(g_hMain, t.c_str());
}

// ---------------------------------------------------------------- menus

static void BuildMenu(HWND hwnd) {
    HMENU bar = CreateMenu();

    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_FILE_OPEN, T(S_MENU_OPEN));
    AppendMenuW(file, MF_STRING, ID_FILE_CLOSE, T(S_MENU_CLOSE));
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_FILE_SAVEAS, T(S_MENU_SAVEAS));
    AppendMenuW(file, MF_STRING, ID_FILE_EXPORT, T(S_MENU_EXPORT));
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_FILE_EXIT, T(S_MENU_EXIT));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, T(S_MENU_FILE));

    HMENU edit = CreatePopupMenu();
    AppendMenuW(edit, MF_STRING, ID_EDIT_SELECTALL, T(S_MENU_SELECTALL));
    AppendMenuW(edit, MF_STRING, ID_EDIT_SELECTNONE, T(S_MENU_SELECTNONE));
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, ID_EDIT_COPY, T(S_MENU_COPY));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)edit, T(S_MENU_EDIT));

    HMENU view = CreatePopupMenu();
    AppendMenuW(view, MF_STRING, ID_VIEW_REFRESH, T(S_MENU_REFRESH));
    AppendMenuW(view, MF_STRING, ID_VIEW_GROUP, T(S_MENU_GROUP));
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, ID_TOOLS_OPTIONS, T(S_MENU_OPTIONS));
    HMENU lang = CreatePopupMenu();
    AppendMenuW(lang, MF_STRING, ID_LANG_SYSTEM, T(S_LANG_SYSTEM));
    AppendMenuW(lang, MF_STRING, ID_LANG_EN, T(S_LANG_EN));
    AppendMenuW(lang, MF_STRING, ID_LANG_ZH, T(S_LANG_ZH));
    AppendMenuW(view, MF_POPUP, (UINT_PTR)lang, T(S_MENU_LANGUAGE));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)view, T(S_MENU_VIEW));

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, ID_HELP, T(S_MENU_HELPTOPIC));
    AppendMenuW(help, MF_STRING, ID_APP_ABOUT, T(S_MENU_ABOUT));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, T(S_MENU_HELP));

    HMENU old = GetMenu(hwnd);
    SetMenu(hwnd, bar);
    if (old) DestroyMenu(old);
    DrawMenuBar(hwnd);
}

// ---------------------------------------------------------------- lists

static void RebuildIconList() {
    g_curFile = -1;
    g_curIcon = -1;
    ListView_DeleteAllItems(g_hIcons);
    ListView_RemoveAllGroups(g_hIcons);
    if (g_hIconImages) {
        ImageList_Destroy(g_hIconImages);
        g_hIconImages = nullptr;
    }
    g_hIconImages = ImageList_Create(g_iconSize, g_iconSize, ILC_COLOR32 | ILC_MASK, 0, 64);
    if (!g_hIconImages) return;
    ListView_SetImageList(g_hIcons, g_hIconImages, LVSIL_NORMAL);

    bool grouped = g_files.size() > 1 && GetSettings().groupFiles;
    ListView_EnableGroupView(g_hIcons, grouped ? TRUE : FALSE);
    if (grouped) {
        for (size_t f = 0; f < g_files.size(); ++f) {
            std::wstring header = FileNameOf(g_files[f].path);
            LVGROUP grp = {};
            grp.cbSize = sizeof(grp);
            grp.mask = LVGF_HEADER | LVGF_GROUPID;
            grp.pszHeader = header.data();
            grp.cchHeader = (int)header.size();
            grp.iGroupId = (int)f + 1;
            ListView_InsertGroup(g_hIcons, -1, &grp);
        }
    }

    HDC dc = GetDC(g_hIcons);
    HGDIOBJ oldFont = nullptr;
    if (g_hFont) oldFont = SelectObject(dc, g_hFont);
    DWORD spacing = ListView_GetItemSpacing(g_hIcons, TRUE);
    int cellW = (int)LOWORD(spacing);
    if (cellW <= 0) cellW = g_iconSize * 3;
    int maxTextW = cellW - Dpi(6);

    int index = 0;
    for (size_t f = 0; f < g_files.size(); ++f) {
        for (size_t i = 0; i < g_files[f].icons.size(); ++i) {
            IconItem& icon = g_files[f].icons[i];
            int img = ImageList_AddIcon(g_hIconImages, icon.hIcon ? icon.hIcon : g_defaultIcon);
            if (img < 0) img = 0;
            std::wstring text = Ellipsize(dc, FullLabelFor({(int)f, (int)i}), maxTextW);

            LVITEMW it = {};
            it.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            it.iItem = index++;
            it.iImage = img;
            it.pszText = text.data();
            it.lParam = (LPARAM)(((int64_t)f << 16) | (int64_t)i);
            if (grouped) {
                it.mask |= LVIF_GROUPID;
                it.iGroupId = (int)f + 1;
            }
            ListView_InsertItem(g_hIcons, &it);
        }
    }
    if (oldFont) SelectObject(dc, oldFont);
    ReleaseDC(g_hIcons, dc);

    if (ListView_GetItemCount(g_hIcons) > 0) {
        ListView_SetItemState(g_hIcons, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(g_hIcons, 0, FALSE);
    } else {
        ListView_DeleteAllItems(g_hDevList);
        g_devOrder.clear();
        g_currentImage = Bitmap32();
        PreviewSetImage(g_hPreview, nullptr, GetSettings().bgColor);
    }
}

static void FillDeviceList(int fileIndex, int iconIndex) {
    ListView_DeleteAllItems(g_hDevList);
    g_devOrder.clear();
    g_currentImage = Bitmap32();
    if (fileIndex < 0 || fileIndex >= (int)g_files.size()) return;
    const IconItem& icon = g_files[(size_t)fileIndex].icons[(size_t)iconIndex];
    const Settings& s = GetSettings();

    for (size_t k = 0; k < icon.images.size(); ++k) g_devOrder.push_back((int)k);

    if (!s.storedOrder && g_devOrder.size() > 1) {
        auto sizeKey = [](const DeviceImage& d) { return (int)d.width * 10000 + (int)d.height; };
        auto colorKey = [](const DeviceImage& d) { return d.Bits() * 10 + (d.isPng ? 1 : 0); };
        auto cmpInt = [](int va, int vb, bool desc) -> int {
            if (va == vb) return 0;
            bool less = va < vb;
            if (desc) return less ? 1 : -1;
            return less ? -1 : 1;
        };
        std::stable_sort(g_devOrder.begin(), g_devOrder.end(), [&](int a, int b) {
            const DeviceImage& x = icon.images[(size_t)a];
            const DeviceImage& y = icon.images[(size_t)b];
            int c = 0;
            if (s.orderPrimary == 0) {
                c = cmpInt(sizeKey(x), sizeKey(y), s.sizeDescending);
                if (c == 0) c = cmpInt(colorKey(x), colorKey(y), s.colorDescending);
            } else {
                c = cmpInt(colorKey(x), colorKey(y), s.colorDescending);
                if (c == 0) c = cmpInt(sizeKey(x), sizeKey(y), s.sizeDescending);
            }
            if (c == 0) c = a - b;
            return c < 0;
        });
    }

    for (size_t row = 0; row < g_devOrder.size(); ++row) {
        int idx = g_devOrder[row];
        std::wstring label = icon.images[(size_t)idx].Label();
        LVITEMW it = {};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = (int)row;
        it.pszText = label.data();
        it.lParam = idx;
        ListView_InsertItem(g_hDevList, &it);
    }

    if (!g_devOrder.empty()) {
        ListView_SetItemState(g_hDevList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    } else {
        OnDeviceSelection();
    }
}

static void OnIconSelection() {
    int item = ListView_GetNextItem(g_hIcons, -1, LVNI_FOCUSED);
    if (item < 0) item = ListView_GetNextItem(g_hIcons, -1, LVNI_SELECTED);
    if (item < 0) {
        g_curFile = -1;
        g_curIcon = -1;
        ListView_DeleteAllItems(g_hDevList);
        g_devOrder.clear();
        g_currentImage = Bitmap32();
        PreviewSetImage(g_hPreview, nullptr, GetSettings().bgColor);
        return;
    }
    LVITEMW it = {};
    it.mask = LVIF_PARAM;
    it.iItem = item;
    if (!ListView_GetItem(g_hIcons, &it)) return;
    IconRef r = RefFromLParam(it.lParam);
    if (r.file < 0 || r.file >= (int)g_files.size()) return;
    if (r.icon < 0 || r.icon >= (int)g_files[(size_t)r.file].icons.size()) return;
    g_curFile = r.file;
    g_curIcon = r.icon;
    FillDeviceList(r.file, r.icon);
}

static void OnDeviceSelection() {
    g_currentImage = Bitmap32();
    int row = ListView_GetNextItem(g_hDevList, -1, LVNI_SELECTED);
    if (row < 0 || g_curFile < 0 || g_curIcon < 0) {
        PreviewSetImage(g_hPreview, nullptr, GetSettings().bgColor);
        return;
    }
    LVITEMW it = {};
    it.mask = LVIF_PARAM;
    it.iItem = row;
    if (!ListView_GetItem(g_hDevList, &it)) return;
    const IconItem& icon = g_files[(size_t)g_curFile].icons[(size_t)g_curIcon];
    int idx = (int)it.lParam;
    if (idx < 0 || idx >= (int)icon.images.size()) return;
    Bitmap32 bm;
    if (icon.images[(size_t)idx].Decode(bm)) {
        g_currentImage = bm;
        PreviewSetImage(g_hPreview, &bm, GetSettings().bgColor);
    } else {
        PreviewSetImage(g_hPreview, nullptr, GetSettings().bgColor);
    }
}

static bool GetSelectedDeviceImage(Bitmap32& out) {
    if (g_curFile < 0 || g_curIcon < 0) return false;
    int row = ListView_GetNextItem(g_hDevList, -1, LVNI_SELECTED);
    if (row < 0) return false;
    LVITEMW it = {};
    it.mask = LVIF_PARAM;
    it.iItem = row;
    if (!ListView_GetItem(g_hDevList, &it)) return false;
    const IconItem& icon = g_files[(size_t)g_curFile].icons[(size_t)g_curIcon];
    int idx = (int)it.lParam;
    if (idx < 0 || idx >= (int)icon.images.size()) return false;
    return icon.images[(size_t)idx].Decode(out);
}

// ---------------------------------------------------------------- commands

static void CloseAll() {
    for (auto& f : g_files) FreeLoadedFile(f);
    g_files.clear();
    g_devOrder.clear();
    g_currentImage = Bitmap32();
    g_curFile = -1;
    g_curIcon = -1;
    if (g_hIcons) {
        ListView_DeleteAllItems(g_hIcons);
        ListView_RemoveAllGroups(g_hIcons);
    }
    if (g_hDevList) ListView_DeleteAllItems(g_hDevList);
    if (g_hIconImages) {
        ImageList_Destroy(g_hIconImages);
        g_hIconImages = nullptr;
    }
    if (g_hPreview) PreviewSetImage(g_hPreview, nullptr, GetSettings().bgColor);
}

static void LoadFiles(const std::vector<std::wstring>& inputPaths) {
    CloseAll();
    std::vector<std::wstring> paths = inputPaths;
    if (!GetSettings().multiFiles && paths.size() > 1) paths.resize(1);
    if (paths.empty()) {
        UpdateStatusText();
        UpdateTitle();
        return;
    }
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::wstring errors;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (g_hStatus) {
            SetWindowTextW(g_hStatus, Fmt(T(S_STATUS_LOADING), FileNameOf(paths[i]).c_str()).c_str());
            UpdateWindow(g_hStatus);
        }
        LoadedFile f;
        if (LoadIconFile(paths[i], f)) {
            for (auto& icon : f.icons) CreateItemIcon(icon, g_iconSize);
            g_files.push_back(std::move(f));
        } else {
            errors += FileNameOf(paths[i]) + L": " + f.error + L"\n";
        }
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    RebuildIconList();
    UpdateStatusText();
    UpdateTitle();
    if (!errors.empty()) MessageBoxW(g_hMain, errors.c_str(), kAppName, MB_ICONWARNING | MB_OK);
}

static void DoOpenDialog() {
    std::wstring filter = FilterFromBarString(T(S_OPEN_FILTER));
    std::vector<wchar_t> buffer(64 * 1024, 0);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = (DWORD)buffer.size();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    ofn.lpstrTitle = T(S_OPEN_TITLE);
    std::wstring openDir = g_files.empty() ? std::wstring() : DirNameOf(g_files[0].path);
    if (!openDir.empty()) ofn.lpstrInitialDir = openDir.c_str();
    if (!GetOpenFileNameW(&ofn)) return;

    std::vector<std::wstring> paths;
    const wchar_t* dir = buffer.data();
    const wchar_t* p = dir + wcslen(dir) + 1;
    if (*p == 0) {
        paths.push_back(dir);
    } else {
        while (*p) {
            std::wstring full = dir;
            full += L"\\";
            full += p;
            paths.push_back(full);
            p += wcslen(p) + 1;
        }
    }
    LoadFiles(paths);
}

static void DoCopy() {
    std::vector<IconRef> refs = SelectedIconRefs();
    if (refs.empty() && g_curFile >= 0) refs.push_back({g_curFile, g_curIcon});
    if (refs.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    std::wstring dir = TempExportDir();
    std::vector<std::wstring> files;
    for (const auto& r : refs) {
        std::vector<uint8_t> bytes = IconBytes(r);
        if (bytes.empty()) continue;
        std::wstring path = UniquePath(dir, IconBaseName(r), L".ico");
        if (WriteAllBytes(path, bytes)) files.push_back(path);
    }
    Bitmap32 img;
    bool hasImage = GetSelectedDeviceImage(img);
    if (!hasImage && !refs.empty()) {
        const IconItem& icon = g_files[(size_t)refs.back().file].icons[(size_t)refs.back().icon];
        if (!icon.images.empty()) hasImage = icon.images[(size_t)icon.bestIndex].Decode(img);
    }
    Bitmap32 flat;
    if (hasImage && !img.Empty()) flat = BlendOnColor(img, GetSettings().bgColor);
    if (!CopyFilesToClipboard(g_hMain, files, flat.Empty() ? nullptr : &flat)) {
        MessageBoxW(g_hMain, T(S_ERR_COPY), kAppName, MB_ICONERROR | MB_OK);
    }
}

static void DoExport() {
    std::vector<IconRef> refs = SelectedIconRefs();
    if (refs.empty()) {
        for (size_t f = 0; f < g_files.size(); ++f)
            for (size_t i = 0; i < g_files[f].icons.size(); ++i)
                refs.push_back({(int)f, (int)i});
    }
    if (refs.empty()) {
        MessageBoxW(g_hMain, T(S_EXPORT_NONE), kAppName, MB_ICONINFORMATION | MB_OK);
        return;
    }
    std::wstring dir = PickFolder(g_hMain, T(S_EXPORT_PROMPT));
    if (dir.empty()) return;
    int okCount = 0, failCount = 0;
    for (const auto& r : refs) {
        std::vector<uint8_t> bytes = IconBytes(r);
        if (bytes.empty()) {
            ++failCount;
            continue;
        }
        std::wstring path = UniquePath(dir, IconBaseName(r), L".ico");
        if (WriteAllBytes(path, bytes))
            ++okCount;
        else
            ++failCount;
    }
    if (okCount > 0) {
        std::wstring msg = Fmt(T(S_EXPORT_DONE), okCount, dir.c_str());
        MessageBoxW(g_hMain, msg.c_str(), kAppName, MB_ICONINFORMATION | MB_OK);
    }
    if (failCount > 0) {
        std::wstring msg = Fmt(T(S_EXPORT_FAIL), failCount);
        MessageBoxW(g_hMain, msg.c_str(), kAppName, MB_ICONWARNING | MB_OK);
    }
}

static void DoSaveAs() {
    if (g_curFile < 0 || g_curIcon < 0) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    bool deviceMode = (GetFocus() == g_hDevList) &&
                      ListView_GetNextItem(g_hDevList, -1, LVNI_SELECTED) >= 0;

    std::wstring stem = FileStemOf(g_files[(size_t)g_curFile].path);
    if (stem.empty()) stem = L"icon";

    wchar_t path[MAX_PATH] = {};
    wcsncpy_s(path, stem.c_str(), _TRUNCATE);

    std::wstring filter = FilterFromBarString(T(S_SAVEAS_FILTER));
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = deviceMode ? 2 : 1;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = deviceMode ? L"bmp" : L"ico";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = T(S_SAVEAS_TITLE);
    std::wstring saveDir = DirNameOf(g_files[(size_t)g_curFile].path);
    if (!saveDir.empty()) ofn.lpstrInitialDir = saveDir.c_str();
    if (!GetSaveFileNameW(&ofn)) return;

    std::wstring outPath = path;
    bool ok = false;
    UINT type = ofn.nFilterIndex;

    if (type == 1) {
        std::vector<uint8_t> bytes = IconBytes({g_curFile, g_curIcon});
        ok = WriteAllBytes(outPath, bytes);
    } else {
        Bitmap32 img;
        if (!GetSelectedDeviceImage(img)) {
            const IconItem& icon = g_files[(size_t)g_curFile].icons[(size_t)g_curIcon];
            if (icon.images.empty()) {
                MessageBeep(MB_ICONWARNING);
                return;
            }
            icon.images[(size_t)icon.bestIndex].Decode(img);
        }
        if (img.Empty()) {
            MessageBeep(MB_ICONWARNING);
            return;
        }
        if (type == 2) {
            Bitmap32 flat = BlendOnColor(img, GetSettings().bgColor);
            ok = SaveBmp(flat, outPath);
        } else {
            ok = SavePng(img, outPath);
        }
    }

    if (!ok) {
        MessageBoxW(g_hMain, Fmt(T(S_ERR_SAVE), outPath.c_str()).c_str(), kAppName,
                    MB_ICONERROR | MB_OK);
    }
}

static void DoHelp() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(g_hInst, exePath, MAX_PATH);
    std::wstring dir = DirNameOf(exePath);
    std::wstring chm = dir + L"\\iconview.chm";
    if (GetFileAttributesW(chm.c_str()) != INVALID_FILE_ATTRIBUTES) {
        ShellExecuteW(g_hMain, L"open", chm.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
        return;
    }
    MessageBoxW(g_hMain, T(S_HELP_TEXT), T(S_MENU_HELP), MB_ICONINFORMATION | MB_OK);
}

// ---------------------------------------------------------------- about

static INT_PTR CALLBACK AboutProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowTextW(hDlg, T(S_ABOUT_TITLE));
        SetDlgItemTextW(hDlg, IDC_ABOUT_TITLE, T(S_ABOUT_VERSION));
        SetDlgItemTextW(hDlg, IDC_ABOUT_COPYRIGHT, T(S_ABOUT_COPYRIGHT));
        SetDlgItemTextW(hDlg, IDC_ABOUT_DESC, T(S_ABOUT_DESC));
        SetDlgItemTextW(hDlg, IDC_ABOUT_NOTE, T(S_ABOUT_NOTE));
        SetDlgItemTextW(hDlg, IDC_ABOUT_CREDIT, T(S_ABOUT_CREDIT));
        SetDlgItemTextW(hDlg, IDC_ABOUT_LINK, T(S_ABOUT_LINK));
        SetDlgItemTextW(hDlg, IDOK, T(S_OPT_OK));
        CenterWindowOnParent(hDlg, g_hMain);
        HFONT base = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
        LOGFONTW lf = {};
        if (base) GetObjectW(base, sizeof(lf), &lf);
        lf.lfUnderline = TRUE;
        HFONT linkFont = CreateFontIndirectW(&lf);
        SendDlgItemMessageW(hDlg, IDC_ABOUT_LINK, WM_SETFONT, (WPARAM)linkFont, TRUE);
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        HWND ctl = (HWND)lp;
        HDC dc = (HDC)wp;
        if (GetDlgCtrlID(ctl) == IDC_ABOUT_LINK) {
            SetTextColor(dc, RGB(0, 0, 192));
            SetBkMode(dc, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
            EndDialog(hDlg, 0);
            return TRUE;
        }
        if (LOWORD(wp) == IDC_ABOUT_LINK && HIWORD(wp) == STN_CLICKED) {
            ShellExecuteW(hDlg, L"open", APP_URL, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void DoAbout() {
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_ABOUT), g_hMain, AboutProc, 0);
}

// ---------------------------------------------------------------- options

static const StrId kSizeOptions[2] = {S_ORDER_SIZE_DESC, S_ORDER_SIZE_ASC};
static const StrId kColorOptions[2] = {S_ORDER_COLOR_DESC, S_ORDER_COLOR_ASC};

struct OptionsState {
    int sizeSel = 0;
    int colorSel = 0;
    int primary = 0;
    bool stored = true;
    bool multi = true;
    bool group = true;
    COLORREF color = 0;
};

static void RefillCombo2(HWND hDlg, OptionsState* st) {
    HWND c1 = GetDlgItem(hDlg, IDC_OPT_COMBO1);
    HWND c2 = GetDlgItem(hDlg, IDC_OPT_COMBO2);
    int s1 = (int)SendMessageW(c1, CB_GETCURSEL, 0, 0);
    SendMessageW(c2, CB_RESETCONTENT, 0, 0);
    if (s1 < 2) {
        SendMessageW(c2, CB_ADDSTRING, 0, (LPARAM)T(kColorOptions[0]));
        SendMessageW(c2, CB_ADDSTRING, 0, (LPARAM)T(kColorOptions[1]));
        SendMessageW(c2, CB_SETCURSEL, st->colorSel, 0);
    } else {
        SendMessageW(c2, CB_ADDSTRING, 0, (LPARAM)T(kSizeOptions[0]));
        SendMessageW(c2, CB_ADDSTRING, 0, (LPARAM)T(kSizeOptions[1]));
        SendMessageW(c2, CB_SETCURSEL, st->sizeSel, 0);
    }
}

static void UpdateOptionsEnable(HWND hDlg) {
    BOOL custom = IsDlgButtonChecked(hDlg, IDC_OPT_ORDER_LIST) == BST_CHECKED;
    EnableWindow(GetDlgItem(hDlg, IDC_OPT_COMBO1), custom);
    EnableWindow(GetDlgItem(hDlg, IDC_OPT_COMBO2), custom);
}

static INT_PTR CALLBACK OptionsProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    OptionsState* st = (OptionsState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    switch (msg) {
    case WM_INITDIALOG: {
        st = (OptionsState*)lp;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)st);

        SetWindowTextW(hDlg, T(S_OPT_TITLE));
        SetDlgItemTextW(hDlg, IDC_OPT_GROUP1, T(S_OPT_GROUP1));
        SetDlgItemTextW(hDlg, IDC_OPT_DESC, T(S_OPT_DESC));
        SetDlgItemTextW(hDlg, IDC_OPT_COLOR_DEFAULT, T(S_OPT_RESTORE));
        SetDlgItemTextW(hDlg, IDC_OPT_GROUP2, T(S_OPT_GROUP2));
        SetDlgItemTextW(hDlg, IDC_OPT_ORDER_STORED, T(S_OPT_ORDER_STORED));
        SetDlgItemTextW(hDlg, IDC_OPT_ORDER_LIST, T(S_OPT_ORDER_LIST));
        SetDlgItemTextW(hDlg, IDC_OPT_THEN, T(S_OPT_THEN));
        SetDlgItemTextW(hDlg, IDC_OPT_MULTIFILES, T(S_OPT_MULTI));
        SetDlgItemTextW(hDlg, IDC_OPT_GROUPFILES, T(S_OPT_GROUPFILES));
        SetDlgItemTextW(hDlg, IDOK, T(S_OPT_OK));
        SetDlgItemTextW(hDlg, IDCANCEL, T(S_OPT_CANCEL));

        const Settings& s = GetSettings();
        st->sizeSel = s.sizeDescending ? 0 : 1;
        st->colorSel = s.colorDescending ? 0 : 1;
        st->primary = s.orderPrimary;
        st->stored = s.storedOrder;
        st->multi = s.multiFiles;
        st->group = s.groupFiles;
        st->color = s.bgColor;

        HWND c1 = GetDlgItem(hDlg, IDC_OPT_COMBO1);
        SendMessageW(c1, CB_ADDSTRING, 0, (LPARAM)T(kSizeOptions[0]));
        SendMessageW(c1, CB_ADDSTRING, 0, (LPARAM)T(kSizeOptions[1]));
        SendMessageW(c1, CB_ADDSTRING, 0, (LPARAM)T(kColorOptions[0]));
        SendMessageW(c1, CB_ADDSTRING, 0, (LPARAM)T(kColorOptions[1]));
        int sel = (st->primary == 0) ? st->sizeSel : 2 + st->colorSel;
        SendMessageW(c1, CB_SETCURSEL, sel, 0);
        RefillCombo2(hDlg, st);

        CheckRadioButton(hDlg, IDC_OPT_ORDER_STORED, IDC_OPT_ORDER_LIST,
                         st->stored ? IDC_OPT_ORDER_STORED : IDC_OPT_ORDER_LIST);
        CheckDlgButton(hDlg, IDC_OPT_MULTIFILES, st->multi ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_OPT_GROUPFILES, st->group ? BST_CHECKED : BST_UNCHECKED);
        UpdateOptionsEnable(hDlg);

        CenterWindowOnParent(hDlg, g_hMain);
        return TRUE;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        if (dis->CtlID == IDC_OPT_COLOR_BTN && st) {
            RECT r = dis->rcItem;
            HBRUSH br = CreateSolidBrush(st->color);
            FillRect(dis->hDC, &r, br);
            DeleteObject(br);
            DrawEdge(dis->hDC, &r, EDGE_SUNKEN, BF_RECT);
            InflateRect(&r, -2, -2);
            if (dis->itemState & ODS_FOCUS) DrawFocusRect(dis->hDC, &r);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case IDC_OPT_COLOR_BTN:
            if (HIWORD(wp) == BN_CLICKED && st) {
                CHOOSECOLORW cc = {};
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hDlg;
                cc.rgbResult = st->color;
                cc.lpCustColors = GetSettings().customColors;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    st->color = cc.rgbResult;
                    InvalidateRect(GetDlgItem(hDlg, IDC_OPT_COLOR_BTN), nullptr, TRUE);
                }
            }
            return TRUE;
        case IDC_OPT_COLOR_DEFAULT:
            if (st) {
                st->color = GetSysColor(COLOR_3DFACE);
                InvalidateRect(GetDlgItem(hDlg, IDC_OPT_COLOR_BTN), nullptr, TRUE);
            }
            return TRUE;
        case IDC_OPT_ORDER_STORED:
        case IDC_OPT_ORDER_LIST:
            UpdateOptionsEnable(hDlg);
            return TRUE;
        case IDC_OPT_COMBO1:
            if (HIWORD(wp) == CBN_SELCHANGE && st) {
                int s1 = (int)SendMessageW(GetDlgItem(hDlg, IDC_OPT_COMBO1), CB_GETCURSEL, 0, 0);
                st->primary = (s1 < 2) ? 0 : 1;
                RefillCombo2(hDlg, st);
            }
            return TRUE;
        case IDC_OPT_COMBO2:
            if (HIWORD(wp) == CBN_SELCHANGE && st) {
                int s2 = (int)SendMessageW(GetDlgItem(hDlg, IDC_OPT_COMBO2), CB_GETCURSEL, 0, 0);
                if (st->primary == 0) st->colorSel = s2;
                else st->sizeSel = s2;
            }
            return TRUE;
        case IDOK: {
            if (!st) { EndDialog(hDlg, IDOK); return TRUE; }
            Settings& s = GetSettings();
            s.bgColor = st->color;
            s.multiFiles = IsDlgButtonChecked(hDlg, IDC_OPT_MULTIFILES) == BST_CHECKED;
            s.groupFiles = IsDlgButtonChecked(hDlg, IDC_OPT_GROUPFILES) == BST_CHECKED;
            s.storedOrder = IsDlgButtonChecked(hDlg, IDC_OPT_ORDER_STORED) == BST_CHECKED;
            s.orderPrimary = st->primary;
            s.sizeDescending = st->sizeSel == 0;
            s.colorDescending = st->colorSel == 0;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

static void ApplySettings() {
    RebuildIconList();
    OnIconSelection();
    SaveSettings();
}

static void DoOptions() {
    Settings before = GetSettings();
    OptionsState state;
    if (DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_OPTIONS), g_hMain, OptionsProc,
                        (LPARAM)&state) == IDOK) {
        ApplySettings();
    } else {
        GetSettings() = before;
    }
}

// ---------------------------------------------------------------- layout

static void LayoutChildren(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int m = Dpi(12);
    int statusH = Dpi(17);
    int labelH = Dpi(16);
    int listH = Dpi(88);

    int y = m;
    MoveWindow(g_hStatus, m, y, W - 2 * m, statusH, TRUE);
    y += statusH + Dpi(8);

    int devY = H - m - listH;
    int labelY = devY - labelH - Dpi(2);
    int iconH = labelY - Dpi(8) - y;
    if (iconH < Dpi(48)) iconH = Dpi(48);

    MoveWindow(g_hIcons, m, y, W - 2 * m, iconH, TRUE);

    int half = (W - 2 * m - Dpi(10)) / 2;
    MoveWindow(g_hDevLabel, m, labelY, half, labelH, TRUE);
    MoveWindow(g_hPrevLabel, m + half + Dpi(10), labelY, W - (m + half + Dpi(10)) - m, labelH,
               TRUE);
    MoveWindow(g_hDevList, m, devY, half, listH, TRUE);
    MoveWindow(g_hPreview, m + half + Dpi(10), devY, W - (m + half + Dpi(10)) - m, listH, TRUE);
    ListView_SetColumnWidth(g_hDevList, 0, (half - Dpi(4)) > 0 ? half - Dpi(4) : 100);
}

// ---------------------------------------------------------------- list tooltip

static LRESULT CALLBACK IconListSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR,
                                         DWORD_PTR) {
    switch (msg) {
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        LVHITTESTINFO ht = {};
        ht.pt.x = GET_X_LPARAM(lp);
        ht.pt.y = GET_Y_LPARAM(lp);
        int item = ListView_HitTest(hwnd, &ht);
        if (item >= 0 && (ht.flags & LVHT_ONITEM)) {
            LVITEMW it = {};
            it.mask = LVIF_PARAM;
            it.iItem = item;
            if (ListView_GetItem(hwnd, &it)) {
                IconRef r = RefFromLParam(it.lParam);
                std::wstring full = FullLabelFor(r);
                wchar_t shown[512] = {};
                ListView_GetItemText(hwnd, item, 0, shown, 512);
                if (!full.empty() && full != shown) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ShowTip(full, pt.x, pt.y);
                    break;
                }
            }
        }
        HideTip();
        break;
    }
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN:
    case WM_MOUSEWHEEL:
        HideTip();
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- drag out

static void BeginIconDrag(HWND hwnd) {
    std::vector<IconRef> refs = SelectedIconRefs();
    if (refs.empty()) return;
    std::wstring dir = TempExportDir();
    std::vector<std::wstring> files;
    for (const auto& r : refs) {
        std::vector<uint8_t> bytes = IconBytes(r);
        if (bytes.empty()) continue;
        std::wstring path = UniquePath(dir, IconBaseName(r), L".ico");
        if (WriteAllBytes(path, bytes)) files.push_back(path);
    }
    if (files.empty()) return;
    Bitmap32 img;
    bool hasImage = GetSelectedDeviceImage(img);
    Bitmap32 flat;
    if (hasImage && !img.Empty()) flat = BlendOnColor(img, GetSettings().bgColor);
    StartFileDrag(hwnd, files, flat.Empty() ? nullptr : &flat);
}

// ---------------------------------------------------------------- window proc

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hMain = hwnd;
        g_hStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                    0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_STATUS, g_hInst, nullptr);
        g_hIcons = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                   WS_CHILD | WS_VISIBLE | LVS_ICON | LVS_SHOWSELALWAYS |
                                       LVS_ALIGNTOP | LVS_NOLABELWRAP,
                                   0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_ICONLIST, g_hInst,
                                   nullptr);
        g_hDevLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0,
                                      0, hwnd, (HMENU)(INT_PTR)IDC_DEVLABEL, g_hInst, nullptr);
        g_hPrevLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0,
                                       0, hwnd, (HMENU)(INT_PTR)IDC_PREVLABEL, g_hInst, nullptr);
        g_hDevList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                     WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER |
                                         LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                     0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DEVLIST, g_hInst,
                                     nullptr);
        g_hPreview = CreatePreviewWindow(hwnd, IDC_PREVIEW);
        if (!g_hStatus || !g_hIcons || !g_hDevList || !g_hPreview) return -1;

        ListView_SetExtendedListViewStyle(g_hIcons, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetExtendedListViewStyle(g_hDevList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<LPWSTR>(L"Device images");
        col.cx = 200;
        ListView_InsertColumn(g_hDevList, 0, &col);

        NONCLIENTMETRICSW ncm = {sizeof(ncm)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        if (g_hFont) {
            for (HWND h : {g_hStatus, g_hIcons, g_hDevList, g_hDevLabel, g_hPrevLabel})
                SendMessageW(h, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }

        BuildMenu(hwnd);
        InitTooltip(g_hIcons);
        SetWindowSubclass(g_hIcons, IconListSubclass, 1, 0);

        SetWindowTextW(g_hDevLabel, T(S_DEVICE_IMAGES));
        SetWindowTextW(g_hPrevLabel, T(S_PREVIEW));

        DragAcceptFiles(hwnd, TRUE);
        LayoutChildren(hwnd);
        return 0;
    }
    case WM_SIZE:
        LayoutChildren(hwnd);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = Dpi(420);
        mmi->ptMinTrackSize.y = Dpi(360);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) PreviewHideFlyout();
        return 0;
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lp;
        if (hdr->hwndFrom == g_hIcons) {
            if (hdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW nv = (LPNMLISTVIEW)lp;
                if ((nv->uNewState & LVIS_FOCUSED) && !(nv->uOldState & LVIS_FOCUSED))
                    OnIconSelection();
            } else if (hdr->code == LVN_BEGINDRAG) {
                HideTip();
                BeginIconDrag(hwnd);
                return 0;
            } else if (hdr->code == NM_RCLICK) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, ID_EDIT_COPY, T(S_CTX_COPY));
                AppendMenuW(menu, MF_STRING, ID_FILE_SAVEAS, T(S_CTX_SAVEAS));
                AppendMenuW(menu, MF_STRING, ID_FILE_EXPORT, T(S_CTX_EXPORT));
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_EDIT_SELECTALL, T(S_CTX_SELECTALL));
                AppendMenuW(menu, MF_STRING, ID_EDIT_SELECTNONE, T(S_CTX_SELECTNONE));
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                return 0;
            }
        } else if (hdr->hwndFrom == g_hDevList) {
            if (hdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW nv = (LPNMLISTVIEW)lp;
                if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED))
                    OnDeviceSelection();
            } else if (hdr->code == NM_RCLICK) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, ID_EDIT_COPY, T(S_CTX_COPY));
                AppendMenuW(menu, MF_STRING, ID_FILE_SAVEAS, T(S_CTX_SAVEAS));
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_EDIT_SELECTALL, T(S_CTX_SELECTALL));
                AppendMenuW(menu, MF_STRING, ID_EDIT_SELECTNONE, T(S_CTX_SELECTNONE));
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                return 0;
            }
        }
        break;
    }
    case WM_INITMENUPOPUP: {
        HMENU menu = (HMENU)wp;
        bool hasFiles = !g_files.empty();
        bool hasCurrent = g_curFile >= 0;
        int iconCount = ListView_GetItemCount(g_hIcons);
        EnableMenuItem(menu, ID_FILE_SAVEAS, hasCurrent ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(menu, ID_FILE_EXPORT, hasFiles ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(menu, ID_EDIT_COPY, hasFiles ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(menu, ID_EDIT_SELECTALL, iconCount > 0 ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(menu, ID_EDIT_SELECTNONE, iconCount > 0 ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(menu, ID_FILE_CLOSE, hasFiles ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(menu, ID_VIEW_REFRESH, hasFiles ? MF_ENABLED : MF_GRAYED);
        CheckMenuItem(menu, ID_VIEW_GROUP, GetSettings().groupFiles ? MF_CHECKED : MF_UNCHECKED);
        int lang = GetLanguageSetting();
        CheckMenuRadioItem(menu, ID_LANG_SYSTEM, ID_LANG_ZH,
                           ID_LANG_SYSTEM + (lang == 0 ? 0 : (lang == 1 ? 1 : 2)), MF_BYCOMMAND);
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case ID_FILE_OPEN:
            DoOpenDialog();
            return 0;
        case ID_FILE_CLOSE:
            CloseAll();
            UpdateStatusText();
            UpdateTitle();
            return 0;
        case ID_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case ID_FILE_SAVEAS:
            DoSaveAs();
            return 0;
        case ID_FILE_EXPORT:
            DoExport();
            return 0;
        case ID_EDIT_COPY:
            DoCopy();
            return 0;
        case ID_EDIT_SELECTALL: {
            HWND target = (GetFocus() == g_hDevList) ? g_hDevList : g_hIcons;
            ListView_SetItemState(target, -1, LVIS_SELECTED, LVIS_SELECTED);
            if (target == g_hIcons) OnIconSelection();
            return 0;
        }
        case ID_EDIT_SELECTNONE: {
            HWND target = (GetFocus() == g_hDevList) ? g_hDevList : g_hIcons;
            ListView_SetItemState(target, -1, 0, LVIS_SELECTED);
            if (target == g_hIcons) OnIconSelection();
            return 0;
        }
        case ID_VIEW_REFRESH: {
            std::vector<std::wstring> paths;
            for (const auto& f : g_files) paths.push_back(f.path);
            if (!paths.empty()) LoadFiles(paths);
            return 0;
        }
        case ID_VIEW_GROUP:
            GetSettings().groupFiles = !GetSettings().groupFiles;
            SaveSettings();
            RebuildIconList();
            return 0;
        case ID_TOOLS_OPTIONS:
            DoOptions();
            return 0;
        case ID_LANG_SYSTEM:
        case ID_LANG_EN:
        case ID_LANG_ZH: {
            int lang = (LOWORD(wp) == ID_LANG_SYSTEM) ? 0 : (LOWORD(wp) == ID_LANG_EN ? 1 : 2);
            GetSettings().language = lang;
            SaveSettings();
            SetLanguageSetting(lang);
            BuildMenu(hwnd);
            SetWindowTextW(g_hDevLabel, T(S_DEVICE_IMAGES));
            SetWindowTextW(g_hPrevLabel, T(S_PREVIEW));
            UpdateStatusText();
            UpdateTitle();
            if (g_curFile >= 0) FillDeviceList(g_curFile, g_curIcon);
            return 0;
        }
        case ID_HELP:
            DoHelp();
            return 0;
        case ID_APP_ABOUT:
            DoAbout();
            return 0;
        }
        break;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp;
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> paths;
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH] = {};
            if (DragQueryFileW(drop, i, path, MAX_PATH)) {
                DWORD attr = GetFileAttributesW(path);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) continue;
                paths.push_back(path);
            }
        }
        DragFinish(drop);
        if (!paths.empty()) LoadFiles(paths);
        return 0;
    }
    case WM_DESTROY:
        CloseAll();
        PreviewHideFlyout();
        CleanupTempDir();
        if (g_hIconImages) {
            ImageList_Destroy(g_hIconImages);
            g_hIconImages = nullptr;
        }
        if (g_hFont) {
            DeleteObject(g_hFont);
            g_hFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- folder picker

static std::wstring PickFolder(HWND owner, const wchar_t* title) {
    std::wstring out;
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dlg)))) {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dlg->SetTitle(title);
        if (SUCCEEDED(dlg->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    out = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dlg->Release();
    }
    if (out.empty()) {
        BROWSEINFOW bi = {};
        bi.hwndOwner = owner;
        bi.lpszTitle = title;
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            wchar_t path[MAX_PATH] = {};
            if (SHGetPathFromIDListW(pidl, path)) out = path;
            CoTaskMemFree(pidl);
        }
    }
    return out;
}

// ---------------------------------------------------------------- entry point

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    g_hInst = inst;
    SetProcessDPIAware();
    {
        HDC dc = GetDC(nullptr);
        g_dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
        if (dc) ReleaseDC(nullptr, dc);
        if (g_dpi <= 0) g_dpi = 96;
    }
    g_iconSize = Dpi(32);

    OleInitialize(nullptr);  // drag-and-drop needs full OLE initialization
    INITCOMMONCONTROLSEX icc = {sizeof(icc),
                                ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    GdiPlusStart();
    LoadSettings();
    SetLanguageSetting(GetSettings().language);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a.size() >= 2 && a.front() == L'"' && a.back() == L'"')
                a = a.substr(1, a.size() - 2);
            if (GetFileAttributesW(a.c_str()) != INVALID_FILE_ATTRIBUTES) g_startupFiles.push_back(a);
        }
        LocalFree(argv);
    }

    if (!RegisterPreviewClasses(inst)) return 0;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!RegisterClassExW(&wc)) return 0;

    g_defaultIcon = LoadIconW(nullptr, IDI_APPLICATION);

    RECT rc = {0, 0, Dpi(580), Dpi(470)};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, TRUE, 0);
    HWND hwnd = CreateWindowExW(0, kClassName, kAppName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr,
                                nullptr, inst, nullptr);
    if (!hwnd) return 0;

    UpdateStatusText();
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    if (!g_startupFiles.empty()) LoadFiles(g_startupFiles);

    HACCEL accel = LoadAcceleratorsW(inst, MAKEINTRESOURCEW(ID_ACCEL));
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (accel && TranslateAcceleratorW(hwnd, accel, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiPlusStop();
    OleUninitialize();
    return (int)msg.wParam;
}
