#pragma once
#include "common.h"

struct Settings {
    COLORREF bgColor = RGB(240, 240, 240);
    int language = 0;          // 0 = follow system, 1 = English, 2 = Chinese
    bool multiFiles = true;
    bool groupFiles = true;
    bool storedOrder = true;
    int orderPrimary = 0;      // 0 = size first, 1 = colors first
    bool sizeDescending = false;
    bool colorDescending = true;
    COLORREF customColors[16] = {};
};

Settings& GetSettings();
void LoadSettings();
void SaveSettings();