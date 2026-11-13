> 本文是分析笔记，记录对原版 IconViewer 3.02 的静态分析结果，用于实现行为兼容的独立程序。
> 仓库中不包含原版的任何二进制、图标、位图或帮助文件；复刻版的应用图标由 `tools/icon` 自行绘制。
> 原版 IconViewer © 2008 Bot Productions，与本项目无隶属关系（见 [../ATTRIBUTION.md](../ATTRIBUTION.md)）。

# IconViewer 3.02 逆向笔记

分析对象：`D:\workspace\分析软件\IconViewer\`

```
iconview.dll     421376 B  2008-05-15  PE32+ x64  版本 3.02.147.0
IconVwLC.dll      12800 B  UI 字符串资源
IVGdip.dll        57344 B  GDI+ 封装
iconview.chm     169959 B  帮助（含截图）
Setup.exe / SetupLC.dll    Bot Productions Setup 3.5（未分析）
x86\              32 位版本
```

---

## 1. PE 结构

```
machine x64, subsystem 5.02, image base 0x180000000
sections: .text .rdata .data .pdata .rsrc(0x1D000) .reloc
exports: BMWSInstallProc, DllCanUnloadNow, DllGetClassObject,
         DllRegisterServer, DllUnregisterServer
```

导出表说明它就是标准 ATL 风格 shell extension：

* `DllGetClassObject` → 属性页 handler 的 class factory
* `DllRegisterServer` → 写
  `HKCR\*\shellex\PropertySheetHandlers\IconViewer`、`CLSID\{...}`、
  `Approved`、`IconViewer\<ver>` 等（字符串表里能看到这些键）
* `BMWSInstallProc` → Bot Productions Setup 的安装回调

## 2. 导入表（行为画像）

| DLL | 关键导入 | 推断 |
| --- | --- | --- |
| KERNEL32 | EnumResourceNamesW, FindResourceW, LoadResource, SizeofResource, LockResource | 枚举文件里的图标资源 |
| USER32 | LookupIconIdFromDirectoryEx, CreateIconFromResourceEx, DrawIconEx, GetIconInfo, DrawTextW, SetWindowsHookExW, TrackPopupMenu | 由 GRPICONDIR 造 HICON、画预览、右键菜单、子类化 |
| GDI32 | CreateDIBSection, GetDIBColorTable, GetNearestColor, CreateFontIndirectW | 设备图像解码 / 背景色合成 / 最近色 |
| USER32 | OpenClipboard, EmptyClipboard, SetClipboardData, CloseClipboard | 复制设备图像 |
| SHELL32 | ExtractIconW, ShellExecuteW, DragQueryFileW | 帮助、拖放 |
| SHLWAPI | PathFindExtensionW, PathStripPathW, PathAppendW, SHDeleteKeyW | 路径处理、卸载清理 |
| ADVAPI32 | RegCreateKeyExW/RegQueryValueExW/RegSetValueExW/RegEnumKeyExW | 设置持久化 |
| ole32 | CoCreateInstance, CreateStreamOnHGlobal, StringFromCLSID | Vista IFileDialog / PNG 流 |
| VERSION | GetFileVersionInfoW, VerQueryValueW | 关于框版本号 |
| KERNEL32 | CreateFileW, ReadFile, WriteFile | 读 .ico、写 .ico/.bmp/.png |

没有 comdlg32：保存对话框走的是 `IFileSaveDialog`（Vista 通用对话框）。
也没有 gdiplus 直接导入 —— PNG 相关都在 `IVGdip.dll` 里。

## 3. 资源

```
RT_CURSOR  11
RT_BITMAP  101 (120x24, 32bpp) 102 (4bpp) 103 (8bpp)   -> 5 个 24x24 工具栏按钮
RT_ICON    1..11                                       -> 应用图标各尺寸
RT_STRING  1 / 26 / 501 / 526 / 532                    -> 内部名字、注册表键
RT_GROUP_CURSOR 9651
RT_GROUP_ICON 1 (10 帧, 16..256) 2 (1 帧 32x32)
RT_VERSION 1
RT_MANIFEST 2  (comctl32 6.0, processorArchitecture=amd64)
```

**没有 RT_DIALOG / RT_MENU / RCDATA。** 属性页和 Options/About 对话框都是
运行期用 `DLGTEMPLATEEX` 直接拼出来的 —— 这也解释了 `IconVwLC.dll` 里为什么会
出现 `"SysListView32"`、`"List1"`、`"List2"`、`"v3"`、`"wv*"`、`"msctls_progress32"`、
`"MS Shell Dlg"` 这类模板片段（这些是动态模板里的 class / 控件名 / 样式字节）。

## 4. 字符串（IconVwLC.dll，UTF-16）

按上下文整理出的 UI 文案（节选）：

```
Icons                                  属性页标题
The file %s contains the following icon(s):
The selected files contain the following icons:
Counting icons (%u found). Please wait...
Adding icons to list. Please wait...
Device images:            Preview:
%ux%u, %u-bit%s           %ux%u, %u colors%s
 (PNG)                    (PNG format)
#%u                       %s #%u              %s                    %s %s
largest size to smallest  smallest size to largest
most colors to least      least colors to most
Background Color
Choose a &color for transparent pixels when saving to a bitmap file or copying to the clipboard.
&Restore Default
Device Images List Order
List images in the order they are stored in the &icon (default)
List images &from ... &then ...
Enable IconViewer when multiple files are selected
Group icons by file
Copy   Save As...   Options   Help   About IconViewer
Icon (*.ico)|*.ico|Bitmap Image (*.bmp)|*.bmp|PNG Image (*.png)|*.png|
Could not copy the device image to the clipboard.
Could not write to the file %s.
Image resource %d could not be found.
```

## 5. 行为还原

### 5.1 枚举

* 对每个文件：`LoadLibraryExW(..., LOAD_LIBRARY_AS_DATAFILE)`，然后
  `EnumResourceNamesW(RT_GROUP_ICON)`；每个组读 `GRPICONDIR`，按 `nID` 找 `RT_ICON`。
* `.ico`/`.cur` 直接解析 ICONDIR（原 DLL 里有 `".ico"`/`".cur"` 字符串）。
* 列表标签：无名资源 `#<id>`，有名资源用资源名；多文件且不分组时前面加文件名
  （对应 `"%s #%u"` / `"%s %s"`）。
* 列表缩略图：`32x32`（或最接近的尺寸，拉伸）+ 最高色深 —— `LookupIconIdFromDirectoryEx`
  的语义。

### 5.2 Device images 文案

* `bits >= 16`：`"%ux%u, %u-bit"`（PNG 帧追加 `" (PNG)"`）
* 否则：`"%ux%u, %u colors"`（16/256 色）
  与 CHM 截图里的 `256x256, 32-bit (PNG)`、`48x48, 256 colors` 完全一致。

### 5.3 排序

选项给出两个下拉：第一个选属性（尺寸或色深）与方向，第二个是另一个属性的方向：

```
largest size to smallest / smallest size to largest
most colors to least / least colors to most
```

注册表 `DevImgOrder` 保存该组合。

### 5.4 预览与 flyout

预览区居中绘制，透明区域用背景色填充（`GetSysColor(COLOR_3DFACE)` 默认）。
设备图像大于预览区时，用 `IVFlyoutWndClass` 弹出 1:1 的小窗口。
窗口类名、`BackgroundColor`/`CustomColors` 等键名都沿用原版。

### 5.5 保存

* 图标 → `.ico`：把 GRPICONDIR + 各 RT_ICON 重新组装成 ICONDIR。
* 设备图像 → `.bmp`：背景色合成后写 24bpp BMP。
* 设备图像 → `.png`：保留 alpha（原版通过 IVGdip 调 GDI+）。

## 6. 界面还原

CHM 里的窗口截图被切成 40+ 块 PNG（`r0.png` … `rf.png`，按 409px 宽的表格式切片）。
用脚本按列宽/heights 重新拼接得到完整截图：
`（原版帮助文档中的界面截图，未随仓库分发）`（409×489）。

由此确定：状态文字 → 大图标列表 → 左侧 Device images / 右侧 Preview → 底部 5 个 24×24
工具按钮（Copy / Save As / Options / Help / About），属性页底部是 OK/Cancel/Apply。

原版工具栏位图（RT_BITMAP #101）直接被复刻程序复用，所以按钮图标一致。

## 7. 复刻程序与原始二进制的对应关系

| 原始实现 | 复刻实现 |
| --- | --- |
| IShellExtInit/IShellPropSheetExt + 属性页 | 独立窗口（`IconViewerMainWnd`） |
| 运行期 DLGTEMPLATEEX | `res\app.rc` 中的 `IDD_OPTIONS` / `IDD_ABOUT` |
| `LookupIconIdFromDirectoryEx` | `ChooseBestImage()`（距 32 最近 → 色深最高） |
| `CreateIconFromResourceEx` | 自解码 + `ScaleImage` + `CreateIconIndirect` |
| `IVGdip.dll` | 直接链接 `gdiplus.lib` |
| `IFileSaveDialog` | `GetSaveFileNameW`（同样的过滤器字符串） |
| 注册表 `Software\IconViewer` | `HKCU\Software\IconViewerRebuild`（键名保持一致） |
## 8. 复刻版的界面调整（相对原版的取舍）

原版是属性页，复刻版是独立程序，所以交互上有几处主动调整：

* **去掉工具栏**：原版 5 个 24×24 按钮（Copy / Save As / Options / Help / About）全部收进菜单，
  快捷键 Ctrl+C / Ctrl+S / F1 保留；窗口因此更紧凑。
* **菜单重组**：`文件`（打开、关闭、另存为、导出图标、退出）、`编辑`（全选、全不选、复制）、
  `查看`（刷新、分组、选项、语言）、`帮助`。原版的 Tools 只有一个 Options，改成放进「查看」。
* **复制语义**：原版只放 CF_BITMAP，粘不进资源管理器。复刻版同时放
  `CF_HDROP`（临时 .ico 文件）、`CF_DIB`、`CF_BITMAP`，所以资源管理器能直接粘贴文件，
  画图/Word 能直接粘贴位图。
* **多选 + 拖拽 + 批量导出**：原版只能一个一个来；复刻版图标列表可多选（Ctrl+A），
  可直接拖到桌面/资源管理器（OLE `IDataObject` + `IDropSource`），也可
  `文件 > 导出图标…` 批量写入指定文件夹。
* **长资源名**：`#id` 之外的命名资源（如 `IDI_SYSTEMSETTINGSADMINFLOWS_ICON`）按单元格宽度
  省略号截断，悬停用 tooltip 显示全名——原版是直接把长名字塞进 32×32 的格子里。
* **大图预览**：原版 flyout 是随手贴在预览区右边；复刻版把 flyout 的左上角对齐预览框
  的左上角（超出屏幕时自动内缩），视觉上像是从预览框里“长出来”的。
* **多语言**：原版只有英文（`IconVwLC.dll` 是语言资源 DLL 的雏形）。复刻版内置中文/英文两套
  文案，默认跟随系统 UI 语言，可在 `查看 > 语言` 切换并记忆。