#include "dataobject.h"
#include <objidl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <cstring>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// ------------------------------------------------------------------ temp files

std::wstring TempExportDir() {
    wchar_t temp[MAX_PATH] = {};
    DWORD n = GetTempPathW(MAX_PATH, temp);
    std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(temp) : std::wstring(L".\\");
    if (!dir.empty() && dir.back() == L'\\') dir.pop_back();
    dir += Fmt(L"\\IconViewer\\%lu", (unsigned long)GetCurrentProcessId());
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    return dir;
}

static void DeleteTree(const std::wstring& dir) {
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring child = dir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                DeleteTree(child);
            else
                DeleteFileW(child.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

void CleanupTempDir() {
    wchar_t temp[MAX_PATH] = {};
    DWORD n = GetTempPathW(MAX_PATH, temp);
    std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(temp) : std::wstring(L".\\");
    if (!dir.empty() && dir.back() == L'\\') dir.pop_back();
    dir += Fmt(L"\\IconViewer\\%lu", (unsigned long)GetCurrentProcessId());
    DeleteTree(dir);
    std::wstring parent = dir.substr(0, dir.find_last_of(L'\\'));
    RemoveDirectoryW(parent.c_str());
}

std::wstring UniquePath(const std::wstring& dir, const std::wstring& base, const wchar_t* ext) {
    std::wstring name = base;
    if (name.empty()) name = L"icon";
    std::wstring path = dir + L"\\" + name + ext;
    int i = 1;
    while (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        path = dir + L"\\" + name + Fmt(L" (%d)", i++) + ext;
    }
    return path;
}

// ------------------------------------------------------------------ clipboard (Win32)

static HGLOBAL MakeHDrop(const std::vector<std::wstring>& files) {
    size_t chars = 1;
    for (const auto& f : files) chars += f.size() + 1;
    SIZE_T bytes = sizeof(DROPFILES) + chars * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!h) return nullptr;
    DROPFILES* df = (DROPFILES*)GlobalLock(h);
    if (!df) { GlobalFree(h); return nullptr; }
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    wchar_t* p = (wchar_t*)((BYTE*)df + sizeof(DROPFILES));
    for (const auto& f : files) {
        memcpy(p, f.c_str(), (f.size() + 1) * sizeof(wchar_t));
        p += f.size() + 1;
    }
    *p = 0;
    GlobalUnlock(h);
    return h;
}

bool CopyFilesToClipboard(HWND owner, const std::vector<std::wstring>& files, const Bitmap32* image) {
    if (files.empty() && (!image || image->Empty())) return false;
    std::vector<uint8_t> dib = (image && !image->Empty()) ? MakeDib24(*image) : std::vector<uint8_t>();
    HBITMAP hbm = (image && !image->Empty()) ? CreateHBitmap32(*image) : nullptr;

    HGLOBAL hDrop = files.empty() ? nullptr : MakeHDrop(files);
    HGLOBAL hDib = nullptr;
    if (!dib.empty()) {
        hDib = GlobalAlloc(GMEM_MOVEABLE, dib.size());
        if (hDib) {
            void* p = GlobalLock(hDib);
            if (p) {
                memcpy(p, dib.data(), dib.size());
                GlobalUnlock(hDib);
            }
        }
    }

    if (!OpenClipboard(owner)) {
        if (hDrop) GlobalFree(hDrop);
        if (hDib) GlobalFree(hDib);
        if (hbm) DeleteObject(hbm);
        return false;
    }
    EmptyClipboard();
    bool ok = false;
    if (hDib && SetClipboardData(CF_DIB, hDib)) { hDib = nullptr; ok = true; }
    if (hbm && SetClipboardData(CF_BITMAP, hbm)) { hbm = nullptr; ok = true; }
    if (hDrop && SetClipboardData(CF_HDROP, hDrop)) { hDrop = nullptr; ok = true; }
    // Any handle still owned here was rejected by the clipboard.
    if (hDib) GlobalFree(hDib);
    if (hDrop) GlobalFree(hDrop);
    if (hbm) DeleteObject(hbm);
    CloseClipboard();
    return ok;
}

// ------------------------------------------------------------------ OLE data object

namespace {

const UINT kFormatCount = 3;

HGLOBAL CloneHGlobal(const void* data, size_t bytes) {
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!h) return nullptr;
    void* p = GlobalLock(h);
    if (!p) { GlobalFree(h); return nullptr; }
    memcpy(p, data, bytes);
    GlobalUnlock(h);
    return h;
}

class FileDataObject : public IDataObject {
public:
    FileDataObject(std::vector<std::wstring> files, const Bitmap32* image)
        : files_(std::move(files)) {
        if (image && !image->Empty()) {
            image_ = *image;
            dib_ = MakeDib24(*image);
        }
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs_); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG n = InterlockedDecrement(&refs_);
        if (n == 0) delete this;
        return (ULONG)n;
    }

    // IDataObject
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* fmt, STGMEDIUM* medium) override {
        if (!fmt || !medium) return E_INVALIDARG;
        medium->pUnkForRelease = nullptr;
        if (fmt->cfFormat == CF_HDROP && files_.size()) {
            HGLOBAL h = MakeHDrop(files_);
            if (!h) return STG_E_MEDIUMFULL;
            medium->tymed = TYMED_HGLOBAL;
            medium->hGlobal = h;
            return S_OK;
        }
        if (fmt->cfFormat == CF_DIB && !dib_.empty()) {
            HGLOBAL h = CloneHGlobal(dib_.data(), dib_.size());
            if (!h) return STG_E_MEDIUMFULL;
            medium->tymed = TYMED_HGLOBAL;
            medium->hGlobal = h;
            return S_OK;
        }
        if (fmt->cfFormat == CF_BITMAP && !image_.Empty()) {
            HBITMAP hbm = CreateHBitmap32(image_);
            if (!hbm) return STG_E_MEDIUMFULL;
            medium->tymed = TYMED_GDI;
            medium->hBitmap = hbm;
            return S_OK;
        }
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* fmt) override {
        if (!fmt) return E_INVALIDARG;
        if (fmt->cfFormat == CF_HDROP && files_.size()) return S_OK;
        if (fmt->cfFormat == CF_DIB && !dib_.empty()) return S_OK;
        if (fmt->cfFormat == CF_BITMAP && !image_.Empty()) return S_OK;
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override {
        if (out) out->ptd = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** out) override {
        if (!out) return E_INVALIDARG;
        *out = nullptr;
        if (direction != DATADIR_GET) return E_NOTIMPL;
        FORMATETC fmts[kFormatCount];
        UINT count = 0;
        fmts[count].cfFormat = CF_HDROP;
        fmts[count].ptd = nullptr;
        fmts[count].dwAspect = DVASPECT_CONTENT;
        fmts[count].lindex = -1;
        fmts[count].tymed = TYMED_HGLOBAL;
        ++count;
        if (!dib_.empty()) {
            fmts[count].cfFormat = CF_DIB;
            fmts[count].ptd = nullptr;
            fmts[count].dwAspect = DVASPECT_CONTENT;
            fmts[count].lindex = -1;
            fmts[count].tymed = TYMED_HGLOBAL;
            ++count;
        }
        if (!image_.Empty()) {
            fmts[count].cfFormat = CF_BITMAP;
            fmts[count].ptd = nullptr;
            fmts[count].dwAspect = DVASPECT_CONTENT;
            fmts[count].lindex = -1;
            fmts[count].tymed = TYMED_GDI;
            ++count;
        }
        return SHCreateStdEnumFmtEtc(count, fmts, out);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    ~FileDataObject() = default;
    LONG refs_ = 1;
    std::vector<std::wstring> files_;
    std::vector<uint8_t> dib_;
    Bitmap32 image_;
};

class FileDropSource : public IDropSource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs_); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG n = InterlockedDecrement(&refs_);
        if (n == 0) delete this;
        return (ULONG)n;
    }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape) return DRAGDROP_S_CANCEL;
        if ((keys & MK_LBUTTON) == 0) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

private:
    ~FileDropSource() = default;
    LONG refs_ = 1;
};

}  // namespace

bool StartFileDrag(HWND owner, const std::vector<std::wstring>& files, const Bitmap32* image) {
    if (files.empty() && (!image || image->Empty())) return false;
    FileDataObject* obj = new FileDataObject(files, image);
    FileDropSource* src = new FileDropSource();
    DWORD effect = 0;
    HRESULT hr = DoDragDrop(obj, src, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
    src->Release();
    obj->Release();
    (void)owner;
    return hr == DRAGDROP_S_DROP;
}
