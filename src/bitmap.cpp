#include "bitmap.h"
#include <gdiplus.h>
#include <objidl.h>
#include <algorithm>
#include <cstring>

#ifndef BI_ALPHABITFIELDS
#define BI_ALPHABITFIELDS 6
#endif

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

static ULONG_PTR g_gdiplusToken = 0;

bool GdiPlusStart() {
    if (g_gdiplusToken) return true;
    Gdiplus::GdiplusStartupInput input;
    return Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) == Gdiplus::Ok;
}
void GdiPlusStop() {
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

bool IsPngData(const uint8_t* d, size_t n) {
    static const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return n >= 8 && memcmp(d, sig, 8) == 0;
}

static inline uint32_t Rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint32_t Rd16(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }

static inline uint32_t MakePixel(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (a > 255) a = 255;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline uint32_t ExtractChannel(uint32_t v, uint32_t mask) {
    if (!mask) return 0;
    int shift = 0;
    while (shift < 32 && ((mask >> shift) & 1u) == 0) shift++;
    uint32_t m = mask >> shift;
    uint32_t val = (v & mask) >> shift;
    if (!m) return 0;
    return (val * 255u + m / 2) / m;
}

bool DecodeDib(const uint8_t* d, size_t n, Bitmap32& out) {
    if (!d || n < 40) return false;
    uint32_t biSize = Rd32(d);
    if (biSize < 40 || biSize > n) return false;

    int32_t width = (int32_t)Rd32(d + 4);
    int32_t height = (int32_t)Rd32(d + 8);
    uint32_t bpp = Rd16(d + 14);
    uint32_t comp = Rd32(d + 16);
    uint32_t clrUsed = Rd32(d + 32);

    bool bottomUp = height > 0;
    int full = height < 0 ? -height : height;
    // Icon DIBs store the XOR bitmap followed by a 1bpp AND mask, so the
    // header height is twice the real image height.
    int h = (full > 1 && (full & 1) == 0) ? full / 2 : full;
    int w = width;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) return false;
    if (comp != BI_RGB && comp != BI_BITFIELDS && comp != BI_ALPHABITFIELDS) return false;

    size_t pos = biSize;
    uint32_t rmask = 0, gmask = 0, bmask = 0, amask = 0;
    if (comp == BI_BITFIELDS || comp == BI_ALPHABITFIELDS) {
        if (biSize >= 108) {
            rmask = Rd32(d + 40);
            gmask = Rd32(d + 44);
            bmask = Rd32(d + 48);
            if (biSize >= 124) amask = Rd32(d + 52);
            if (comp == BI_ALPHABITFIELDS && !amask && biSize >= 124) amask = Rd32(d + 52);
        } else {
            if (pos + 12 > n) return false;
            rmask = Rd32(d + pos);
            gmask = Rd32(d + pos + 4);
            bmask = Rd32(d + pos + 8);
            pos += 12;
            if (comp == BI_ALPHABITFIELDS) {
                if (pos + 4 > n) return false;
                amask = Rd32(d + pos);
                pos += 4;
            }
        }
    } else if (bpp == 16) {
        rmask = 0x7C00; gmask = 0x03E0; bmask = 0x001F;
    } else if (bpp == 32) {
        rmask = 0x00FF0000; gmask = 0x0000FF00; bmask = 0x000000FF; amask = 0xFF000000;
    }

    uint32_t palCount = 0;
    if (bpp <= 8) {
        palCount = clrUsed ? clrUsed : (1u << bpp);
        if (palCount > 256) palCount = 256;
        if (pos + (size_t)palCount * 4 > n) return false;
    }
    const uint8_t* pal = d + pos;
    size_t xorOff = pos + (size_t)palCount * 4;
    size_t stride = ((size_t)w * bpp + 31) / 32 * 4;
    if (xorOff + stride * h > n) return false;
    size_t maskStride = ((size_t)w + 31) / 32 * 4;
    size_t maskOff = xorOff + stride * h;
    bool hasMask = (maskOff + maskStride * h) <= n;

    out.w = w;
    out.h = h;
    out.px.assign((size_t)w * h, 0xFF000000u);

    bool alphaUsed = false;
    if (bpp == 32 && amask) {
        int ashift = 0;
        while (ashift < 32 && ((amask >> ashift) & 1u) == 0) ashift++;
        for (int y = 0; y < h && !alphaUsed; ++y) {
            const uint8_t* row = d + xorOff + (size_t)y * stride;
            for (int x = 0; x < w; ++x) {
                uint32_t v = Rd32(row + (size_t)x * 4);
                if (((v & amask) >> ashift) != 0) { alphaUsed = true; break; }
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        int sy = bottomUp ? (h - 1 - y) : y;
        const uint8_t* row = d + xorOff + (size_t)sy * stride;
        const uint8_t* mrow = hasMask ? d + maskOff + (size_t)sy * maskStride : nullptr;
        uint32_t* orow = out.Row(y);
        for (int x = 0; x < w; ++x) {
            uint32_t r = 0, g = 0, b = 0, a = 255;
            bool transparentByMask = false;
            if (mrow) {
                uint8_t mb = mrow[x >> 3];
                transparentByMask = ((mb >> (7 - (x & 7))) & 1) != 0;
            }
            switch (bpp) {
            case 32: {
                uint32_t v = Rd32(row + (size_t)x * 4);
                r = ExtractChannel(v, rmask);
                g = ExtractChannel(v, gmask);
                b = ExtractChannel(v, bmask);
                if (alphaUsed) a = ExtractChannel(v, amask);
                else a = transparentByMask ? 0 : 255;
                break;
            }
            case 24: {
                const uint8_t* p = row + (size_t)x * 3;
                b = p[0]; g = p[1]; r = p[2];
                a = transparentByMask ? 0 : 255;
                break;
            }
            case 16: {
                uint32_t v = Rd16(row + (size_t)x * 2);
                r = ExtractChannel(v, rmask);
                g = ExtractChannel(v, gmask);
                b = ExtractChannel(v, bmask);
                a = (amask && alphaUsed) ? ExtractChannel(v, amask) : (transparentByMask ? 0 : 255);
                break;
            }
            default: {
                uint32_t idx = 0;
                if (bpp == 8) {
                    idx = row[x];
                } else if (bpp == 4) {
                    uint8_t v = row[x >> 1];
                    idx = (x & 1) ? (v & 0x0F) : (v >> 4);
                } else {
                    uint8_t v = row[x >> 3];
                    idx = (v >> (7 - (x & 7))) & 1;
                }
                if (idx < palCount) {
                    b = pal[idx * 4 + 0];
                    g = pal[idx * 4 + 1];
                    r = pal[idx * 4 + 2];
                }
                a = transparentByMask ? 0 : 255;
                break;
            }
            }
            orow[x] = MakePixel(r, g, b, a);
        }
    }
    return true;
}

namespace {
struct ComInit {
    bool needed = false;
    ComInit() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        needed = (hr == S_OK || hr == S_FALSE);
    }
    ~ComInit() {
        if (needed) CoUninitialize();
    }
};
}  // namespace

bool DecodePng(const uint8_t* d, size_t n, Bitmap32& out) {
    if (!GdiPlusStart()) return false;
    ComInit com;
    IStream* stream = SHCreateMemStream(d, (UINT)n);
    if (!stream) return false;
    bool ok = false;
    {
        Gdiplus::Bitmap bmp(stream, FALSE);
        if (bmp.GetLastStatus() == Gdiplus::Ok) {
            UINT w = bmp.GetWidth(), h = bmp.GetHeight();
            if (w > 0 && h > 0 && w <= 16384 && h <= 16384) {
                Gdiplus::Rect rc(0, 0, (INT)w, (INT)h);
                Gdiplus::BitmapData bd;
                if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) ==
                    Gdiplus::Ok) {
                    out.w = (int)w;
                    out.h = (int)h;
                    out.px.resize((size_t)w * h);
                    for (UINT y = 0; y < h; ++y) {
                        const uint8_t* src = (const uint8_t*)bd.Scan0 + (size_t)y * bd.Stride;
                        memcpy(out.Row((int)y), src, (size_t)w * 4);
                    }
                    bmp.UnlockBits(&bd);
                    ok = true;
                }
            }
        }
    }
    stream->Release();
    return ok;
}

bool DecodeImageData(const std::vector<uint8_t>& data, Bitmap32& out) {
    if (data.empty()) return false;
    if (IsPngData(data)) return DecodePng(data.data(), data.size(), out);
    return DecodeDib(data.data(), data.size(), out);
}

Bitmap32 ScaleImage(const Bitmap32& src, int nw, int nh) {
    Bitmap32 dst;
    if (src.Empty() || nw <= 0 || nh <= 0) return dst;
    if (src.w == nw && src.h == nh) return src;
    dst.w = nw;
    dst.h = nh;
    dst.px.resize((size_t)nw * nh, 0);

    if (nw <= src.w && nh <= src.h) {
        // Area average (box filter) for downscaling.
        for (int y = 0; y < nh; ++y) {
            int y0 = (int)((int64_t)y * src.h / nh);
            int y1 = (int)(((int64_t)(y + 1) * src.h + nh - 1) / nh);
            if (y1 > src.h) y1 = src.h;
            if (y1 <= y0) y1 = y0 + 1;
            for (int x = 0; x < nw; ++x) {
                int x0 = (int)((int64_t)x * src.w / nw);
                int x1 = (int)(((int64_t)(x + 1) * src.w + nw - 1) / nw);
                if (x1 > src.w) x1 = src.w;
                if (x1 <= x0) x1 = x0 + 1;
                uint64_t ar = 0, ag = 0, ab = 0, aa = 0, cnt = 0;
                for (int sy = y0; sy < y1; ++sy) {
                    const uint32_t* row = src.Row(sy);
                    for (int sx = x0; sx < x1; ++sx) {
                        uint32_t p = row[sx];
                        uint32_t a = (p >> 24) & 0xFF;
                        ar += ((p >> 16) & 0xFF) * a;
                        ag += ((p >> 8) & 0xFF) * a;
                        ab += (p & 0xFF) * a;
                        aa += a;
                        ++cnt;
                    }
                }
                uint32_t a = cnt ? (uint32_t)(aa / cnt) : 0;
                uint32_t r = aa ? (uint32_t)(ar / aa) : 0;
                uint32_t g = aa ? (uint32_t)(ag / aa) : 0;
                uint32_t b = aa ? (uint32_t)(ab / aa) : 0;
                dst.Row(y)[x] = MakePixel(r, g, b, a);
            }
        }
    } else {
        for (int y = 0; y < nh; ++y) {
            double fy = (y + 0.5) * src.h / nh - 0.5;
            int y0 = (int)floor(fy);
            double dy = fy - y0;
            int y1 = y0 + 1;
            if (y0 < 0) { y0 = 0; dy = 0; }
            if (y1 >= src.h) y1 = src.h - 1;
            if (y0 >= src.h) y0 = src.h - 1;
            for (int x = 0; x < nw; ++x) {
                double fx = (x + 0.5) * src.w / nw - 0.5;
                int x0 = (int)floor(fx);
                double dx = fx - x0;
                int x1 = x0 + 1;
                if (x0 < 0) { x0 = 0; dx = 0; }
                if (x1 >= src.w) x1 = src.w - 1;
                if (x0 >= src.w) x0 = src.w - 1;
                const uint32_t* r0 = src.Row(y0);
                const uint32_t* r1 = src.Row(y1);
                double acc[4] = {0, 0, 0, 0};
                const double wts[4] = {(1 - dx) * (1 - dy), dx * (1 - dy), (1 - dx) * dy, dx * dy};
                const uint32_t samples[4] = {r0[x0], r0[x1], r1[x0], r1[x1]};
                double asum = 0;
                for (int k = 0; k < 4; ++k) {
                    uint32_t p = samples[k];
                    double a = ((p >> 24) & 0xFF) / 255.0;
                    asum += a * wts[k];
                    acc[0] += ((p >> 16) & 0xFF) * a * wts[k];
                    acc[1] += ((p >> 8) & 0xFF) * a * wts[k];
                    acc[2] += (p & 0xFF) * a * wts[k];
                }
                double a = asum;
                double r = a > 0 ? acc[0] / a : 0;
                double g = a > 0 ? acc[1] / a : 0;
                double b = a > 0 ? acc[2] / a : 0;
                dst.Row(y)[x] = MakePixel((uint32_t)(r + 0.5), (uint32_t)(g + 0.5),
                                          (uint32_t)(b + 0.5), (uint32_t)(a * 255 + 0.5));
            }
        }
    }
    return dst;
}

Bitmap32 BlendOnColor(const Bitmap32& src, COLORREF color) {
    Bitmap32 dst;
    if (src.Empty()) return dst;
    dst.w = src.w;
    dst.h = src.h;
    dst.px.resize(src.px.size());
    uint32_t br = GetRValue(color), bg = GetGValue(color), bb = GetBValue(color);
    for (int y = 0; y < src.h; ++y) {
        const uint32_t* s = src.Row(y);
        uint32_t* d = dst.Row(y);
        for (int x = 0; x < src.w; ++x) {
            uint32_t p = s[x];
            uint32_t a = (p >> 24) & 0xFF;
            if (a == 255) {
                d[x] = 0xFF000000u | (p & 0x00FFFFFFu);
            } else if (a == 0) {
                d[x] = 0xFF000000u | (br << 16) | (bg << 8) | bb;
            } else {
                uint32_t r = ((p >> 16) & 0xFF) * a / 255 + br * (255 - a) / 255;
                uint32_t g = ((p >> 8) & 0xFF) * a / 255 + bg * (255 - a) / 255;
                uint32_t b = (p & 0xFF) * a / 255 + bb * (255 - a) / 255;
                d[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
    }
    return dst;
}

static HBITMAP CreateDib32Ex(int w, int h, uint32_t** bitsOut) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = (DWORD)((size_t)w * h * 4);
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitsOut) *bitsOut = (uint32_t*)bits;
    return hbm;
}

HBITMAP CreateDib32(const Bitmap32& bm) {
    if (bm.Empty()) return nullptr;
    uint32_t* bits = nullptr;
    HBITMAP hbm = CreateDib32Ex(bm.w, bm.h, &bits);
    if (!hbm || !bits) {
        if (hbm) DeleteObject(hbm);
        return nullptr;
    }
    memcpy(bits, bm.px.data(), (size_t)bm.w * bm.h * 4);
    return hbm;
}

HICON CreateIconFromImage(const Bitmap32& bm) {
    if (bm.Empty()) return nullptr;
    uint32_t* colorBits = nullptr;
    HBITMAP hColor = CreateDib32Ex(bm.w, bm.h, &colorBits);
    if (!hColor || !colorBits) {
        if (hColor) DeleteObject(hColor);
        return nullptr;
    }
    memcpy(colorBits, bm.px.data(), (size_t)bm.w * bm.h * 4);

    size_t maskStride = ((size_t)bm.w + 31) / 32 * 4;
    std::vector<uint8_t> mask(maskStride * bm.h, 0);
    for (int y = 0; y < bm.h; ++y) {
        const uint32_t* row = bm.Row(y);
        for (int x = 0; x < bm.w; ++x) {
            if (((row[x] >> 24) & 0xFF) == 0) mask[(size_t)y * maskStride + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
        }
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.w;
    bi.bmiHeader.biHeight = -bm.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 1;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = (DWORD)mask.size();
    void* maskBits = nullptr;
    HBITMAP hMask = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &maskBits, nullptr, 0);
    if (!hMask || !maskBits) {
        DeleteObject(hColor);
        if (hMask) DeleteObject(hMask);
        return nullptr;
    }
    memcpy(maskBits, mask.data(), mask.size());

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hColor;
    ii.hbmMask = hMask;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(hColor);
    DeleteObject(hMask);
    return icon;
}

bool SaveBmp(const Bitmap32& img, const std::wstring& path) {
    if (img.Empty()) return false;
    int w = img.w, h = img.h;
    size_t stride = ((size_t)w * 24 + 31) / 32 * 4;
    DWORD imgSize = (DWORD)(stride * h);

    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;

    BITMAPFILEHEADER fh = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + imgSize;

    BITMAPINFOHEADER ih = {};
    ih.biSize = sizeof(BITMAPINFOHEADER);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = imgSize;

    DWORD written = 0;
    bool ok = WriteFile(f, &fh, sizeof(fh), &written, nullptr) &&
              WriteFile(f, &ih, sizeof(ih), &written, nullptr);
    if (ok) {
        std::vector<uint8_t> row(stride, 0);
        for (int y = h - 1; y >= 0 && ok; --y) {
            const uint32_t* src = img.Row(y);
            for (int x = 0; x < w; ++x) {
                uint32_t p = src[x];
                row[x * 3 + 0] = (uint8_t)(p & 0xFF);
                row[x * 3 + 1] = (uint8_t)((p >> 8) & 0xFF);
                row[x * 3 + 2] = (uint8_t)((p >> 16) & 0xFF);
            }
            ok = WriteFile(f, row.data(), (DWORD)stride, &written, nullptr) != 0;
        }
    }
    CloseHandle(f);
    if (!ok) DeleteFileW(path.c_str());
    return ok;
}

static int GetEncoderClsid(const WCHAR* mime, CLSID* clsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    auto* info = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!info) return -1;
    Gdiplus::GetImageEncoders(num, size, info);
    int result = -1;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(info[i].MimeType, mime) == 0) {
            *clsid = info[i].Clsid;
            result = (int)i;
            break;
        }
    }
    free(info);
    return result;
}

bool SavePng(const Bitmap32& img, const std::wstring& path) {
    if (img.Empty() || !GdiPlusStart()) return false;
    ComInit com;
    Gdiplus::Bitmap bmp(img.w, img.h, img.w * 4, PixelFormat32bppARGB, (BYTE*)img.px.data());
    if (bmp.GetLastStatus() != Gdiplus::Ok) return false;
    CLSID clsid;
    if (GetEncoderClsid(L"image/png", &clsid) < 0) return false;
    return bmp.Save(path.c_str(), &clsid, nullptr) == Gdiplus::Ok;
}

std::vector<uint8_t> MakeDib24(const Bitmap32& img) {
    std::vector<uint8_t> out;
    if (img.Empty()) return out;
    int w = img.w, h = img.h;
    size_t stride = ((size_t)w * 24 + 31) / 32 * 4;
    size_t imgSize = stride * h;
    out.resize(sizeof(BITMAPINFOHEADER) + imgSize, 0);
    BITMAPINFOHEADER* ih = (BITMAPINFOHEADER*)out.data();
    ih->biSize = sizeof(BITMAPINFOHEADER);
    ih->biWidth = w;
    ih->biHeight = h;
    ih->biPlanes = 1;
    ih->biBitCount = 24;
    ih->biCompression = BI_RGB;
    ih->biSizeImage = (DWORD)imgSize;
    uint8_t* pixels = out.data() + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < h; ++y) {
        const uint32_t* src = img.Row(h - 1 - y);
        uint8_t* dst = pixels + (size_t)y * stride;
        for (int x = 0; x < w; ++x) {
            uint32_t v = src[x];
            dst[x * 3 + 0] = (uint8_t)(v & 0xFF);
            dst[x * 3 + 1] = (uint8_t)((v >> 8) & 0xFF);
            dst[x * 3 + 2] = (uint8_t)((v >> 16) & 0xFF);
        }
    }
    return out;
}

HBITMAP CreateHBitmap32(const Bitmap32& img) {
    if (img.Empty()) return nullptr;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = img.w;
    bi.bmiHeader.biHeight = -img.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = (DWORD)((size_t)img.w * img.h * 4);
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm || !bits) {
        if (hbm) DeleteObject(hbm);
        return nullptr;
    }
    memcpy(bits, img.px.data(), (size_t)img.w * img.h * 4);
    return hbm;
}
