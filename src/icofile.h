#pragma once
#include "common.h"
#include "bitmap.h"

struct DeviceImage {
    WORD width = 0;
    WORD height = 0;
    WORD bitCount = 0;    // bits per pixel, when known
    WORD colorCount = 0;  // number of colors, when that is how it is described
    bool isPng = false;
    std::vector<uint8_t> data;

    int Bits() const;
    std::wstring Label() const;
    bool Decode(Bitmap32& out) const;
};

struct IconItem {
    bool named = false;
    WORD id = 0;
    std::wstring name;
    std::vector<DeviceImage> images;
    HICON hIcon = nullptr;
    int bestIndex = 0;
};

struct LoadedFile {
    std::wstring path;
    std::vector<IconItem> icons;
    bool isIconFile = false;
    std::vector<uint8_t> rawBytes;
    bool ok = false;
    std::wstring error;
};

bool LoadIconFile(const std::wstring& path, LoadedFile& out);
void FreeLoadedFile(LoadedFile& file);
int ChooseBestImage(const std::vector<DeviceImage>& images);
// Creates the small icon used in the icon list.
void CreateItemIcon(IconItem& item, int size);
// Builds .ico file bytes out of the icon.
std::vector<uint8_t> BuildIcoBytes(const IconItem& item);
