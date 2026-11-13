#include "common.h"
#include "bitmap.h"
#include "icofile.h"
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 1;
    g_hInst = GetModuleHandleW(nullptr);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    GdiPlusStart();
    for (int a = 1; a < argc; ++a) {
        wprintf(L"=== %s\n", argv[a]);
        LoadedFile f;
        if (!LoadIconFile(argv[a], f)) {
            wprintf(L"  FAILED: %s\n", f.error.c_str());
            continue;
        }
        wprintf(L"  ok=%d isIconFile=%d icons=%zu\n", (int)f.ok, (int)f.isIconFile, f.icons.size());
        int shown = 0;
        for (auto& icon : f.icons) {
            CreateItemIcon(icon, 32);
            if (shown < 6) {
                wprintf(L"  icon id=%u named=%d name=%s images=%zu hicon=%p\n", icon.id, (int)icon.named,
                        icon.name.c_str(), icon.images.size(), (void*)icon.hIcon);
                for (auto& im : icon.images) {
                    Bitmap32 bm;
                    bool dec = im.Decode(bm);
                    wprintf(L"     %s  decode=%d %dx%d\n", im.Label().c_str(), (int)dec, bm.w, bm.h);
                }
                ++shown;
            }
        }
        FreeLoadedFile(f);
    }
    GdiPlusStop();
    CoUninitialize();
    return 0;
}
