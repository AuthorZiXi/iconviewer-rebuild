#include "preview.h"

static const wchar_t* kPreviewClass = L"IconViewerPreview";
static const wchar_t* kFlyoutClass = L"IVFlyoutWndClass";

struct PreviewState {
    Bitmap32 image;   // flattened over background color
    Bitmap32 raw;     // original, for the flyout
    COLORREF bg = RGB(240, 240, 240);
    bool hasImage = false;
};
static PreviewState g_preview;
static HWND g_flyout = nullptr;
static HWND g_flyoutOwner = nullptr;

static void DrawCentered(HDC dc, const RECT& client, const Bitmap32& img) {
    if (img.Empty()) return;
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;
    int dw = img.w, dh = img.h;
    double scale = 1.0;
    if (dw > cw && cw > 0) scale = (double)cw / dw;
    if (dh > ch && ch > 0) scale = (std::min)(scale, (double)ch / dh);
    dw = (int)(img.w * scale);
    dh = (int)(img.h * scale);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    int x = client.left + (cw - dw) / 2;
    int y = client.top + (ch - dh) / 2;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = img.w;
    bi.bmiHeader.biHeight = -img.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchDIBits(dc, x, y, dw, dh, 0, 0, img.w, img.h, img.px.data(), &bi, DIB_RGB_COLORS,
                  SRCCOPY);
}

static LRESULT CALLBACK PreviewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(g_preview.hasImage ? g_preview.bg : GetSysColor(COLOR_3DFACE));
        FillRect(dc, &rc, br);
        DeleteObject(br);
        if (g_preview.hasImage) DrawCentered(dc, rc, g_preview.image);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK FlyoutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(g_preview.bg);
        FillRect(dc, &rc, br);
        DeleteObject(br);
        if (g_preview.hasImage) {
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = g_preview.image.w;
            bi.bmiHeader.biHeight = -g_preview.image.h;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetDIBitsToDevice(dc, 0, 0, (DWORD)g_preview.image.w, (DWORD)g_preview.image.h, 0, 0, 0,
                              (UINT)g_preview.image.h, g_preview.image.px.data(), &bi,
                              DIB_RGB_COLORS);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool RegisterPreviewClasses(HINSTANCE inst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PreviewProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kPreviewClass;
    if (!RegisterClassExW(&wc)) return false;

    wc.lpfnWndProc = FlyoutProc;
    wc.lpszClassName = kFlyoutClass;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    if (!RegisterClassExW(&wc)) return false;
    return true;
}

HWND CreatePreviewWindow(HWND parent, int id) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, kPreviewClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10,
                           10, parent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
}

void PreviewHideFlyout() {
    if (g_flyout) ShowWindow(g_flyout, SW_HIDE);
}

static void UpdateFlyout(HWND preview) {
    if (!g_preview.hasImage) { PreviewHideFlyout(); return; }
    RECT rc;
    GetClientRect(preview, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (g_preview.image.w <= cw && g_preview.image.h <= ch) {
        PreviewHideFlyout();
        return;
    }
    if (!g_flyout) {
        g_flyout = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
                                   kFlyoutClass, L"", WS_POPUP | WS_BORDER, 0, 0, 10, 10,
                                   GetParent(preview), nullptr, g_hInst, nullptr);
        g_flyoutOwner = preview;
    }
    if (!g_flyout) return;

    HMONITOR mon = MonitorFromWindow(preview, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    RECT work = mi.rcWork;

    int w = (std::min)(g_preview.image.w, (int)(work.right - work.left));
    int h = (std::min)(g_preview.image.h, (int)(work.bottom - work.top));
    // Anchor the flyout to the preview box: same top-left corner, kept on screen.
    RECT pr;
    GetWindowRect(preview, &pr);

    int x = pr.left;
    int y = pr.top;
    if (x + w > work.right) x = work.right - w;
    if (y + h > work.bottom) y = work.bottom - h;
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;

    SetWindowPos(g_flyout, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_flyout, nullptr, TRUE);
}

void PreviewSetImage(HWND preview, const Bitmap32* image, COLORREF bg) {
    g_preview.bg = bg;
    if (image && !image->Empty()) {
        g_preview.raw = *image;
        g_preview.image = BlendOnColor(*image, bg);
        g_preview.hasImage = true;
    } else {
        g_preview.raw = Bitmap32();
        g_preview.image = Bitmap32();
        g_preview.hasImage = false;
    }
    if (preview) {
        InvalidateRect(preview, nullptr, TRUE);
        UpdateFlyout(preview);
    }
    if (g_flyout && g_preview.hasImage) InvalidateRect(g_flyout, nullptr, TRUE);
}