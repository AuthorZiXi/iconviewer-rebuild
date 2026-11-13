#include "settings.h"

static const wchar_t* kRegKey = L"Software\\IconViewerRebuild";
static const wchar_t* kColor = L"BackgroundColor";
static const wchar_t* kMulti = L"MultiFiles";
static const wchar_t* kLanguage = L"Language";
static const wchar_t* kGroup = L"GroupMultiFiles";
static const wchar_t* kOrder = L"DevImgOrder";
static const wchar_t* kCustom = L"CustomColors";

static Settings g_settings;

Settings& GetSettings() { return g_settings; }

static DWORD ReadDword(HKEY key, const wchar_t* name, DWORD def) {
    DWORD type = 0, value = 0, size = sizeof(value);
    if (RegQueryValueExW(key, name, nullptr, &type, (BYTE*)&value, &size) == ERROR_SUCCESS &&
        type == REG_DWORD)
        return value;
    return def;
}

void LoadSettings() {
    g_settings.bgColor = GetSysColor(COLOR_3DFACE);
    g_settings.language = 0;
    g_settings.multiFiles = true;
    g_settings.groupFiles = true;
    g_settings.storedOrder = true;
    g_settings.orderPrimary = 0;
    g_settings.sizeDescending = false;
    g_settings.colorDescending = true;
    for (int i = 0; i < 16; ++i) g_settings.customColors[i] = RGB(255, 255, 255);

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return;

    DWORD color = ReadDword(key, kColor, 0xFFFFFFFF);
    if (color != 0xFFFFFFFF) g_settings.bgColor = (COLORREF)color;
    g_settings.language = (int)ReadDword(key, kLanguage, 0);
    g_settings.multiFiles = ReadDword(key, kMulti, 1) != 0;
    g_settings.groupFiles = ReadDword(key, kGroup, 1) != 0;

    DWORD order = ReadDword(key, kOrder, 0xFFFFFFFF);
    if (order != 0xFFFFFFFF) {
        g_settings.storedOrder = (order & 8) != 0;
        g_settings.orderPrimary = order & 1;
        g_settings.sizeDescending = (order & 2) != 0;
        g_settings.colorDescending = (order & 4) != 0;
    }

    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, kCustom, nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        type == REG_BINARY && size == sizeof(g_settings.customColors)) {
        RegQueryValueExW(key, kCustom, nullptr, &type, (BYTE*)g_settings.customColors, &size);
    }
    RegCloseKey(key);
}

void SaveSettings() {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS)
        return;
    DWORD color = (DWORD)g_settings.bgColor;
    RegSetValueExW(key, kColor, 0, REG_DWORD, (const BYTE*)&color, sizeof(color));
    DWORD lang = (DWORD)g_settings.language;
    RegSetValueExW(key, kLanguage, 0, REG_DWORD, (const BYTE*)&lang, sizeof(lang));
    DWORD multi = g_settings.multiFiles ? 1 : 0;
    RegSetValueExW(key, kMulti, 0, REG_DWORD, (const BYTE*)&multi, sizeof(multi));
    DWORD group = g_settings.groupFiles ? 1 : 0;
    RegSetValueExW(key, kGroup, 0, REG_DWORD, (const BYTE*)&group, sizeof(group));
    DWORD order = (DWORD)(g_settings.orderPrimary & 1) | (g_settings.sizeDescending ? 2u : 0u) |
                  (g_settings.colorDescending ? 4u : 0u) | (g_settings.storedOrder ? 8u : 0u);
    RegSetValueExW(key, kOrder, 0, REG_DWORD, (const BYTE*)&order, sizeof(order));
    RegSetValueExW(key, kCustom, 0, REG_BINARY, (const BYTE*)g_settings.customColors,
                   sizeof(g_settings.customColors));
    RegCloseKey(key);
}