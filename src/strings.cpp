#include "strings.h"

namespace {

const wchar_t* kEn[S_COUNT] = {
    L"&File",
    L"&Open...\tCtrl+O",
    L"&Close",
    L"&Save As...\tCtrl+S",
    L"&Export Icons...",
    L"E&xit",
    L"&Edit",
    L"Select &All\tCtrl+A",
    L"Select N&one",
    L"&Copy\tCtrl+C",
    L"&View",
    L"&Refresh\tF5",
    L"&Group icons by file",
    L"&Options...",
    L"&Language",
    L"Follow &system",
    L"&English",
    L"&Chinese",
    L"&Help",
    L"&Help\tF1",
    L"&About IconViewer Rebuild",
    L"Device images:",
    L"Preview:",
    L"Open a file, or drop files here, to view the icons they contain.",
    L"The file %s contains the following icon(s):",
    L"The selected files contain the following icons:",
    L"The file %s does not contain any icons.",
    L"Loading %s...",
    L"IconViewer Rebuild - %d files",
    L"%ux%u, %u-bit%s",
    L"%ux%u, %u colors%s",
    L" (PNG)",
    L"Could not copy the device image to the clipboard.",
    L"Could not write to the file %s.\nTry saving under a different file name.",
    L"This file is not a program, library or icon file, so it does not contain any icons.",
    L"Could not open the file.\n%s",
    L"This file does not contain any valid icon images.",
    L"Save As",
    L"Icon (*.ico)|*.ico|Bitmap Image (*.bmp)|*.bmp|PNG Image (*.png)|*.png|",
    L"Open",
    L"All Supported Files|*.exe;*.dll;*.ocx;*.cpl;*.scr;*.ico;*.cur|"
    L"Programs and Libraries|*.exe;*.dll;*.ocx;*.cpl;*.scr|"
    L"Icons and Cursors|*.ico;*.cur|All Files|*.*|",
    L"Export Icons",
    L"Choose a folder where the icons will be saved.",
    L"Exported %d icon(s) to:\n%s",
    L"Could not write %d file(s).",
    L"There are no icons to export.",
    L"&Copy",
    L"&Save As...",
    L"Select &All",
    L"Select N&one",
    L"E&xport Icons...",
    L"IconViewer shows the icons stored inside programs, libraries and icon files.\n\n"
    L"- Use File > Open (Ctrl+O) or drop files onto the window.\n"
    L"- Select one or more icons to see their device images.\n"
    L"- Drag icons out, or use Copy (Ctrl+C) / Save As (Ctrl+S).\n"
    L"- Options changes the background color and list order.",
    L"IconViewer Rebuild Options",
    L"Background Color",
    L"Choose a color for transparent pixels when saving to a bitmap file or copying to the clipboard.",
    L"&Restore Default",
    L"Device Images List Order",
    L"List images in the order they are stored in the &icon (default)",
    L"List images &from",
    L"&then",
    L"Enable IconViewer when multiple files are selected",
    L"Group icons by file",
    L"OK",
    L"Cancel",
    L"largest size to smallest",
    L"smallest size to largest",
    L"most colors to least",
    L"least colors to most",
    L"About IconViewer Rebuild",
    L"IconViewer Rebuild 1.0.0",
    L"Copyright (C) 2008 Bot Productions.",
    L"An independent Win32 reimplementation of IconViewer 3.02.",
    L"Not affiliated with Bot Productions. IconViewer (C) 2008 Bot Productions.",
    L"https://www.botproductions.com/iconview/iconview.html",
    L"The selected files do not contain any icons.",
};

const wchar_t* kZh[S_COUNT] = {
    L"文件(&F)",
    L"打开(&O)...\tCtrl+O",
    L"关闭(&C)",
    L"另存为(&S)...\tCtrl+S",
    L"导出图标(&E)...",
    L"退出(&X)",
    L"编辑(&E)",
    L"全选(&A)\tCtrl+A",
    L"全不选(&N)",
    L"复制(&C)\tCtrl+C",
    L"查看(&V)",
    L"刷新(&R)\tF5",
    L"按文件分组(&G)",
    L"选项(&O)...",
    L"语言(&L)",
    L"跟随系统(&S)",
    L"英语(&E)",
    L"中文(&C)",
    L"帮助(&H)",
    L"帮助(&H)\tF1",
    L"关于 IconViewer Rebuild(&A)",
    L"设备图像：",
    L"预览：",
    L"打开文件，或把文件拖到这里，即可查看其中的图标。",
    L"文件 %s 包含以下图标：",
    L"所选文件包含以下图标：",
    L"文件 %s 不包含任何图标。",
    L"正在加载 %s……",
    L"IconViewer Rebuild - %d 个文件",
    L"%ux%u，%u 位%s",
    L"%ux%u，%u 色%s",
    L"（PNG）",
    L"无法把设备图像复制到剪贴板。",
    L"无法写入文件 %s。\n请换一个文件名重试。",
    L"该文件不是程序、库或图标文件，因此不包含任何图标。",
    L"无法打开文件。\n%s",
    L"该文件不包含有效的图标图像。",
    L"另存为",
    L"图标文件 (*.ico)|*.ico|位图图像 (*.bmp)|*.bmp|PNG 图像 (*.png)|*.png|",
    L"打开",
    L"所有支持的文件|*.exe;*.dll;*.ocx;*.cpl;*.scr;*.ico;*.cur|"
    L"程序和库|*.exe;*.dll;*.ocx;*.cpl;*.scr|"
    L"图标和光标|*.ico;*.cur|所有文件|*.*|",
    L"导出图标",
    L"请选择保存图标的文件夹。",
    L"已导出 %d 个图标到：\n%s",
    L"有 %d 个文件写入失败。",
    L"没有可导出的图标。",
    L"复制(&C)",
    L"另存为(&S)...",
    L"全选(&A)",
    L"全不选(&N)",
    L"导出图标(&X)...",
    L"IconViewer 可以查看程序、库和图标文件中存放的图标。\n\n"
    L"- 用“文件 > 打开”(Ctrl+O) 选择文件，或把文件拖进窗口。\n"
    L"- 选中一个或多个图标即可查看其设备图像。\n"
    L"- 可以把图标直接拖出去，或用复制(Ctrl+C)/另存为(Ctrl+S)。\n"
    L"- 在“选项”里可以改背景色和列表顺序。",
    L"IconViewer Rebuild 选项",
    L"背景色",
    L"选择保存为位图或复制到剪贴板时，用来填充透明像素的颜色。",
    L"恢复默认(&R)",
    L"设备图像列表顺序",
    L"按图标中存储的顺序列出图像（默认）(&I)",
    L"排序方式(&F)",
    L"然后(&T)",
    L"多选文件时启用 IconViewer",
    L"按文件分组显示图标",
    L"确定",
    L"取消",
    L"尺寸从大到小",
    L"尺寸从小到大",
    L"颜色从多到少",
    L"颜色从少到多",
    L"关于 IconViewer Rebuild",
    L"IconViewer Rebuild 1.0.0",
    L"版权所有 (C) 2008 Bot Productions。",
    L"IconViewer 3.02 的独立 Win32 复刻实现。",
    L"与 Bot Productions 无隶属关系；原版 IconViewer 版权归其所有。",
    L"https://www.botproductions.com/iconview/iconview.html",
    L"所选文件不包含任何图标。",
};

int g_langSetting = 0;
int g_lang = 1;

}  // namespace

void SetLanguageSetting(int lang) {
    g_langSetting = lang;
    if (lang == 1 || lang == 2) {
        g_lang = lang;
    } else {
        LANGID id = GetUserDefaultUILanguage();
        g_lang = ((id & 0x3FF) == 0x04) ? 2 : 1;
    }
}

int GetLanguageSetting() { return g_langSetting; }

int CurrentLanguage() { return g_lang; }

const wchar_t* T(StrId id) {
    if (id < 0 || id >= S_COUNT) return L"";
    return (g_lang == 2) ? kZh[id] : kEn[id];
}

std::wstring FilterFromBarString(const wchar_t* s) {
    std::wstring out;
    if (!s) return out;
    for (const wchar_t* p = s; *p; ++p) out.push_back(*p == L'|' ? L'\0' : *p);
    out.push_back(L'\0');
    out.push_back(L'\0');
    return out;
}