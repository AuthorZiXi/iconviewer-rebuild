#include "icofile.h"
#include <cstring>
#include <set>
#include "strings.h"

static int Log2Of(int v) {
    int r = 0;
    while (v > 1) { v >>= 1; ++r; }
    return r;
}

int DeviceImage::Bits() const {
    if (bitCount) return bitCount;
    if (colorCount) return Log2Of(colorCount);
    return 0;
}

std::wstring DeviceImage::Label() const {
    wchar_t buf[128];
    const wchar_t* suffix = isPng ? T(S_FMT_PNG) : L"";
    int bits = Bits();
    if (bits >= 16) {
        swprintf_s(buf, _countof(buf), T(S_FMT_BIT), (unsigned)width, (unsigned)height,
                   (unsigned)bits, suffix);
    } else {
        unsigned colors = colorCount ? colorCount : (bits ? (1u << bits) : 0u);
        if (colors)
            swprintf_s(buf, _countof(buf), T(S_FMT_COLORS), (unsigned)width, (unsigned)height,
                       colors, suffix);
        else
            swprintf_s(buf, _countof(buf), L"%ux%u%s", (unsigned)width, (unsigned)height, suffix);
    }
    return buf;
}

bool DeviceImage::Decode(Bitmap32& out) const { return DecodeImageData(data, out); }

int ChooseBestImage(const std::vector<DeviceImage>& images) {
    int best = -1;
    long bestScore = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        const DeviceImage& im = images[i];
        int w = im.width ? im.width : 32;
        int h = im.height ? im.height : 32;
        long score = (long)abs(w - 32) + (long)abs(h - 32);
        long b = im.Bits();
        // Prefer small distance from 32x32, then deeper color, then larger image.
        long key = score * 1000 - b * 4 - (w > 32 ? 0 : 0);
        if (best < 0 || key < bestScore) { best = (int)i; bestScore = key; }
    }
    return best < 0 ? 0 : best;
}

void CreateItemIcon(IconItem& item, int size) {
    if (item.hIcon) { DestroyIcon(item.hIcon); item.hIcon = nullptr; }
    if (item.images.empty()) return;
    item.bestIndex = ChooseBestImage(item.images);
    Bitmap32 bm;
    if (!item.images[(size_t)item.bestIndex].Decode(bm)) return;
    Bitmap32 scaled = ScaleImage(bm, size, size);
    item.hIcon = CreateIconFromImage(scaled);
}

static void ParseIcoEntries(const uint8_t* d, size_t n, LoadedFile& out) {
    if (n < 6) return;
    WORD type = (WORD)(d[2] | (d[3] << 8));
    WORD count = (WORD)(d[4] | (d[5] << 8));
    if (type != 1 && type != 2) return;
    size_t need = 6 + (size_t)count * 16;
    if (need > n) return;

    IconItem item;
    item.named = true;
    item.name = FileStemOf(out.path);
    for (WORD i = 0; i < count; ++i) {
        const uint8_t* e = d + 6 + (size_t)i * 16;
        DeviceImage img;
        img.width = e[0] ? e[0] : 256;
        img.height = e[1] ? e[1] : 256;
        img.colorCount = e[2];
        WORD planes = (WORD)(e[4] | (e[5] << 8));
        WORD bits = (WORD)(e[6] | (e[7] << 8));
        uint32_t bytes = (uint32_t)e[8] | ((uint32_t)e[9] << 8) | ((uint32_t)e[10] << 16) | ((uint32_t)e[11] << 24);
        uint32_t off = (uint32_t)e[12] | ((uint32_t)e[13] << 8) | ((uint32_t)e[14] << 16) | ((uint32_t)e[15] << 24);
        if (type == 2) {
            // Cursor: planes/bitcount fields hold the hotspot.
            bits = 0;
        } else {
            img.bitCount = bits;
        }
        (void)planes;
        if (off > n || bytes > n - off || bytes == 0) continue;
        img.data.assign(d + off, d + off + bytes);
        img.isPng = IsPngData(img.data);
        item.images.push_back(std::move(img));
    }
    if (!item.images.empty()) out.icons.push_back(std::move(item));
}

static void AddIconResource(HMODULE mod, WORD id, const std::wstring* name, LoadedFile& out,
                            std::set<WORD>* usedIds) {
    HRSRC hr = name ? FindResourceW(mod, name->c_str(), RT_ICON)
                    : FindResourceW(mod, MAKEINTRESOURCEW(id), RT_ICON);
    if (!hr) return;
    DWORD size = SizeofResource(mod, hr);
    if (!size) return;
    HGLOBAL hg = LoadResource(mod, hr);
    const uint8_t* p = (const uint8_t*)LockResource(hg);
    if (!p) return;

    DeviceImage img;
    if (size >= 40) {
        uint32_t hdrSize = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        if (hdrSize >= 40 && hdrSize <= size) {
            const BITMAPINFOHEADER* bi = (const BITMAPINFOHEADER*)p;
            img.width = (WORD)(bi->biWidth > 0 ? bi->biWidth : 0);
            img.height = (WORD)(bi->biHeight > 0 ? bi->biHeight / 2 : 0);
            img.bitCount = bi->biBitCount;
        }
    }
    img.data.assign(p, p + size);
    img.isPng = IsPngData(img.data);

    IconItem item;
    if (name) {
        item.named = true;
        item.name = *name;
    } else {
        item.id = id;
    }
    item.images.push_back(std::move(img));
    out.icons.push_back(std::move(item));
    if (usedIds && id) usedIds->insert(id);
}

static bool AddGroupResource(HMODULE mod, LPCWSTR name, LoadedFile& out, std::set<WORD>& usedIds) {
    HRSRC hr = FindResourceW(mod, name, RT_GROUP_ICON);
    if (!hr) return false;
    DWORD size = SizeofResource(mod, hr);
    if (size < 6) return false;
    HGLOBAL hg = LoadResource(mod, hr);
    const uint8_t* p = (const uint8_t*)LockResource(hg);
    if (!p) return false;

    WORD count = (WORD)(p[4] | (p[5] << 8));
    if (count == 0 || 6 + (size_t)count * 14 > size) return false;

    IconItem item;
    if (IS_INTRESOURCE(name)) {
        item.id = (WORD)(UINT_PTR)name;
    } else {
        item.named = true;
        item.name = name;
    }

    for (WORD i = 0; i < count; ++i) {
        const uint8_t* e = p + 6 + (size_t)i * 14;
        WORD id = (WORD)(e[12] | (e[13] << 8));
        if (!id) continue;
        HRSRC ir = FindResourceW(mod, MAKEINTRESOURCEW(id), RT_ICON);
        if (!ir) continue;
        DWORD isize = SizeofResource(mod, ir);
        if (!isize) continue;
        HGLOBAL ih = LoadResource(mod, ir);
        const uint8_t* ip = (const uint8_t*)LockResource(ih);
        if (!ip) continue;

        DeviceImage img;
        img.width = e[0] ? e[0] : 256;
        img.height = e[1] ? e[1] : 256;
        img.colorCount = e[2];
        img.bitCount = (WORD)(e[6] | (e[7] << 8));
        img.data.assign(ip, ip + isize);
        img.isPng = IsPngData(img.data);
        if (!img.isPng && img.data.size() >= 40) {
            const BITMAPINFOHEADER* bi = (const BITMAPINFOHEADER*)img.data.data();
            if (bi->biWidth > 0 && bi->biHeight > 0) {
                img.width = (WORD)bi->biWidth;
                img.height = (WORD)(bi->biHeight / 2);
            }
        }
        item.images.push_back(std::move(img));
        usedIds.insert(id);
    }
    if (item.images.empty()) return false;
    out.icons.push_back(std::move(item));
    return true;
}

struct EnumGroupsCtx {
    HMODULE mod;
    LoadedFile* file;
    std::set<WORD>* usedIds;
};

static BOOL CALLBACK EnumGroupsProc(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR lp) {
    EnumGroupsCtx* ctx = (EnumGroupsCtx*)lp;
    AddGroupResource(ctx->mod, name, *ctx->file, *ctx->usedIds);
    return TRUE;
}

struct EnumIconsCtx {
    HMODULE mod;
    LoadedFile* file;
    std::set<WORD>* usedIds;
};

static BOOL CALLBACK EnumIconsProc(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR lp) {
    EnumIconsCtx* ctx = (EnumIconsCtx*)lp;
    if (IS_INTRESOURCE(name)) {
        WORD id = (WORD)(UINT_PTR)name;
        if (ctx->usedIds->count(id)) return TRUE;
        AddIconResource(ctx->mod, id, nullptr, *ctx->file, ctx->usedIds);
    } else {
        std::wstring nm = name;
        AddIconResource(ctx->mod, 0, &nm, *ctx->file, ctx->usedIds);
    }
    return TRUE;
}

static bool LoadModuleIcons(const std::wstring& path, LoadedFile& out) {
    HMODULE mod = LoadLibraryExW(path.c_str(), nullptr,
                                 LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!mod)
        mod = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
    if (!mod) {
        DWORD err = GetLastError();
        if (err == ERROR_BAD_EXE_FORMAT || err == ERROR_BAD_FORMAT ||
            err == ERROR_INVALID_EXE_SIGNATURE) {
            out.error = L"This file is not a program, library or icon file, so it "
                        L"does not contain any icons.";
        } else {
            out.error = Fmt(L"Could not open the file.\n%s", SysErrorText(err).c_str());
        }
        return false;
    }

    std::set<WORD> used;
    EnumGroupsCtx gctx{mod, &out, &used};
    EnumResourceNamesW(mod, RT_GROUP_ICON, EnumGroupsProc, (LONG_PTR)&gctx);
    EnumIconsCtx ictx{mod, &out, &used};
    EnumResourceNamesW(mod, RT_ICON, EnumIconsProc, (LONG_PTR)&ictx);

    FreeLibrary(mod);
    return true;
}

bool LoadIconFile(const std::wstring& path, LoadedFile& out) {
    out.path = path;
    out.ok = false;
    out.error.clear();
    out.isIconFile = false;

    std::wstring ext = FileExtOf(path);
    std::vector<uint8_t> head;
    bool signatureIco = false;
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            out.error = Fmt(L"Could not open the file.\n%s", SysErrorText(GetLastError()).c_str());
            return false;
        }
        uint8_t buf[16] = {};
        DWORD got = 0;
        ReadFile(h, buf, sizeof(buf), &got, nullptr);
        CloseHandle(h);
        if (got >= 6) {
            WORD reserved = (WORD)(buf[0] | (buf[1] << 8));
            WORD type = (WORD)(buf[2] | (buf[3] << 8));
            WORD count = (WORD)(buf[4] | (buf[5] << 8));
            if (reserved == 0 && (type == 1 || type == 2) && count > 0) signatureIco = true;
        }
    }

    bool tryIco = signatureIco || ext == L".ico" || ext == L".cur";
    if (tryIco) {
        std::vector<uint8_t> bytes;
        if (ReadWholeFile(path, bytes) && bytes.size() >= 6) {
            WORD reserved = (WORD)(bytes[0] | (bytes[1] << 8));
            WORD type = (WORD)(bytes[2] | (bytes[3] << 8));
            WORD count = (WORD)(bytes[4] | (bytes[5] << 8));
            if (reserved == 0 && (type == 1 || type == 2) && count > 0) {
                ParseIcoEntries(bytes.data(), bytes.size(), out);
                if (!out.icons.empty()) {
                    out.isIconFile = true;
                    out.rawBytes = std::move(bytes);
                    out.ok = true;
                    return true;
                }
            }
        }
        if (ext == L".ico" || ext == L".cur") {
            out.error = L"This file does not contain any valid icon images.";
            return false;
        }
    }

    if (!LoadModuleIcons(path, out)) return false;
    out.ok = true;
    return true;
}

void FreeLoadedFile(LoadedFile& file) {
    for (auto& icon : file.icons) {
        if (icon.hIcon) { DestroyIcon(icon.hIcon); icon.hIcon = nullptr; }
    }
    file.icons.clear();
}

std::vector<uint8_t> BuildIcoBytes(const IconItem& item) {
    std::vector<uint8_t> out;
    size_t count = item.images.size();
    if (!count) return out;
    out.resize(6 + count * 16);
    out[0] = 0; out[1] = 0; out[2] = 1; out[3] = 0;
    out[4] = (uint8_t)(count & 0xFF);
    out[5] = (uint8_t)((count >> 8) & 0xFF);
    uint32_t offset = (uint32_t)(6 + count * 16);
    for (size_t i = 0; i < count; ++i) {
        const DeviceImage& img = item.images[i];
        uint8_t* e = out.data() + 6 + i * 16;
        e[0] = (uint8_t)(img.width >= 256 ? 0 : img.width);
        e[1] = (uint8_t)(img.height >= 256 ? 0 : img.height);
        e[2] = (uint8_t)(img.colorCount > 255 ? 0 : img.colorCount);
        e[3] = 0;
        WORD planes = 1;
        WORD bits = (WORD)img.Bits();
        e[4] = (uint8_t)(planes & 0xFF); e[5] = (uint8_t)(planes >> 8);
        e[6] = (uint8_t)(bits & 0xFF); e[7] = (uint8_t)(bits >> 8);
        uint32_t sz = (uint32_t)img.data.size();
        e[8] = (uint8_t)(sz & 0xFF); e[9] = (uint8_t)((sz >> 8) & 0xFF);
        e[10] = (uint8_t)((sz >> 16) & 0xFF); e[11] = (uint8_t)((sz >> 24) & 0xFF);
        e[12] = (uint8_t)(offset & 0xFF); e[13] = (uint8_t)((offset >> 8) & 0xFF);
        e[14] = (uint8_t)((offset >> 16) & 0xFF); e[15] = (uint8_t)((offset >> 24) & 0xFF);
        offset += sz;
    }
    for (const auto& img : item.images)
        out.insert(out.end(), img.data.begin(), img.data.end());
    return out;
}