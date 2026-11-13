#pragma once
#include "common.h"
#include "bitmap.h"

bool RegisterPreviewClasses(HINSTANCE inst);
HWND CreatePreviewWindow(HWND parent, int id);
void PreviewSetImage(HWND preview, const Bitmap32* image, COLORREF bg);
void PreviewHideFlyout();
