#pragma once
#include "common.h"

// A 32-bit image, pixels stored as 0xAARRGGBB (memory order B,G,R,A).
struct Bitmap32 {
    int w = 0;
    int h = 0;
    std::vector<uint32_t> px;

    bool Empty() const { return w <= 0 || h <= 0 || px.empty(); }
    uint32_t* Row(int y) { return px.data() + (size_t)y * w; }
    const uint32_t* Row(int y) const { return px.data() + (size_t)y * w; }
};

bool IsPngData(const uint8_t* d, size_t n);
inline bool IsPngData(const std::vector<uint8_t>& d) { return IsPngData(d.data(), d.size()); }

// Decodes either a DIB (BITMAPINFOHEADER payload as stored in RT_ICON / .ico)
// or a PNG stream.
bool DecodeImageData(const std::vector<uint8_t>& data, Bitmap32& out);
bool DecodeDib(const uint8_t* d, size_t n, Bitmap32& out);
bool DecodePng(const uint8_t* d, size_t n, Bitmap32& out);

Bitmap32 ScaleImage(const Bitmap32& src, int nw, int nh);
// Flattens alpha over a solid color, producing an opaque image.
Bitmap32 BlendOnColor(const Bitmap32& src, COLORREF color);

HBITMAP CreateDib32(const Bitmap32& bm);
HICON CreateIconFromImage(const Bitmap32& bm);
bool SaveBmp(const Bitmap32& opaqueImage, const std::wstring& path);
bool SavePng(const Bitmap32& argb, const std::wstring& path);
// BITMAPINFOHEADER + 24bpp bottom-up pixels, as used by CF_DIB.
std::vector<uint8_t> MakeDib24(const Bitmap32& img);
HBITMAP CreateHBitmap32(const Bitmap32& img);

bool GdiPlusStart();
void GdiPlusStop();