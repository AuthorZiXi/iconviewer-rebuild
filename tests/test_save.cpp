#include "common.h"
#include "bitmap.h"
#include "icofile.h"
#include <cstdio>

static bool CheckIconLoads(const wchar_t* path) {
    HICON icon = (HICON)LoadImageW(nullptr, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (!icon) return false;
    DestroyIcon(icon);
    return true;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) { wprintf(L"usage: test_save <file> <outdir>\n"); return 1; }
    g_hInst = GetModuleHandleW(nullptr);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    GdiPlusStart();

    LoadedFile f;
    if (!LoadIconFile(argv[1], f)) { wprintf(L"load failed: %s\n", f.error.c_str()); return 2; }

    // find the icon with the most images
    size_t best = 0;
    for (size_t i = 1; i < f.icons.size(); ++i)
        if (f.icons[i].images.size() > f.icons[best].images.size()) best = i;
    IconItem& icon = f.icons[best];
    wprintf(L"icon #%u images=%zu\n", icon.id, icon.images.size());

    std::wstring dir = argv[2];
    std::wstring bmp = dir + L"\\out.bmp";
    std::wstring png = dir + L"\\out.png";
    std::wstring ico = dir + L"\\out.ico";

    int failures = 0;
    for (auto& im : icon.images) {
        if (im.data.size() >= 40) {
            const BITMAPINFOHEADER* bi = (const BITMAPINFOHEADER*)im.data.data();
            wprintf(L"    hdr %s size=%zu bi=%u %dx%d bpp=%u comp=%u clrUsed=%u\n", im.Label().c_str(),
                    im.data.size(), bi->biSize, bi->biWidth, bi->biHeight, bi->biBitCount,
                    bi->biCompression, bi->biClrUsed);
        }
        Bitmap32 bm;
        if (!im.Decode(bm)) { wprintf(L"  decode FAIL %s\n", im.Label().c_str()); ++failures; continue; }
        Bitmap32 flat = BlendOnColor(bm, RGB(255, 255, 255));
        if (!SaveBmp(flat, bmp)) { wprintf(L"  bmp FAIL %s\n", im.Label().c_str()); ++failures; }
        if (!SavePng(bm, png)) { wprintf(L"  png FAIL %s\n", im.Label().c_str()); ++failures; }
        wprintf(L"  ok %s -> %dx%d\n", im.Label().c_str(), bm.w, bm.h);
    }
    // Save the largest image separately for visual inspection.
    int bigIdx = 0;
    for (size_t i = 1; i < icon.images.size(); ++i)
        if ((int)icon.images[i].width * icon.images[i].height >
            (int)icon.images[bigIdx].width * icon.images[bigIdx].height)
            bigIdx = (int)i;
    {
        Bitmap32 bm;
        if (icon.images[(size_t)bigIdx].Decode(bm)) {
            Bitmap32 flat = BlendOnColor(bm, RGB(240, 240, 240));
            SaveBmp(flat, dir + L"\\final.bmp");
            SavePng(bm, dir + L"\\final.png");
            wprintf(L"final from %s %dx%d\n", icon.images[(size_t)bigIdx].Label().c_str(), bm.w, bm.h);
        }
    }

    std::vector<uint8_t> bytes = BuildIcoBytes(icon);
    HANDLE h = CreateFileW(ico.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    DWORD wr = 0;
    WriteFile(h, bytes.data(), (DWORD)bytes.size(), &wr, nullptr);
    CloseHandle(h);
    wprintf(L"ico bytes=%zu loads=%d\n", bytes.size(), (int)CheckIconLoads(ico.c_str()));

    // Verify saved png re-decodes
    std::vector<uint8_t> pngBytes;
    if (ReadWholeFile(png, pngBytes)) {
        Bitmap32 back;
        bool ok = DecodePng(pngBytes.data(), pngBytes.size(), back);
        wprintf(L"png re-decode=%d %dx%d\n", (int)ok, back.w, back.h);
    }
    FreeLoadedFile(f);
    GdiPlusStop();
    CoUninitialize();
    wprintf(L"failures=%d\n", failures);
    return failures;
}
