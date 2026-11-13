#pragma once
#include "common.h"

enum StrId : int {
    S_MENU_FILE = 0,
    S_MENU_OPEN,
    S_MENU_CLOSE,
    S_MENU_SAVEAS,
    S_MENU_EXPORT,
    S_MENU_EXIT,
    S_MENU_EDIT,
    S_MENU_SELECTALL,
    S_MENU_SELECTNONE,
    S_MENU_COPY,
    S_MENU_VIEW,
    S_MENU_REFRESH,
    S_MENU_GROUP,
    S_MENU_OPTIONS,
    S_MENU_LANGUAGE,
    S_LANG_SYSTEM,
    S_LANG_EN,
    S_LANG_ZH,
    S_MENU_HELP,
    S_MENU_HELPTOPIC,
    S_MENU_ABOUT,
    S_DEVICE_IMAGES,
    S_PREVIEW,
    S_STATUS_EMPTY,
    S_STATUS_ONE,
    S_STATUS_MULTI,
    S_STATUS_NOICONS,
    S_STATUS_LOADING,
    S_TITLE_MULTI,
    S_FMT_BIT,
    S_FMT_COLORS,
    S_FMT_PNG,
    S_ERR_COPY,
    S_ERR_SAVE,
    S_ERR_NOTICON,
    S_ERR_OPEN,
    S_ERR_NOICONS,
    S_SAVEAS_TITLE,
    S_SAVEAS_FILTER,
    S_OPEN_TITLE,
    S_OPEN_FILTER,
    S_EXPORT_TITLE,
    S_EXPORT_PROMPT,
    S_EXPORT_DONE,
    S_EXPORT_FAIL,
    S_EXPORT_NONE,
    S_CTX_COPY,
    S_CTX_SAVEAS,
    S_CTX_SELECTALL,
    S_CTX_SELECTNONE,
    S_CTX_EXPORT,
    S_HELP_TEXT,
    S_OPT_TITLE,
    S_OPT_GROUP1,
    S_OPT_DESC,
    S_OPT_RESTORE,
    S_OPT_GROUP2,
    S_OPT_ORDER_STORED,
    S_OPT_ORDER_LIST,
    S_OPT_THEN,
    S_OPT_MULTI,
    S_OPT_GROUPFILES,
    S_OPT_OK,
    S_OPT_CANCEL,
    S_ORDER_SIZE_DESC,
    S_ORDER_SIZE_ASC,
    S_ORDER_COLOR_DESC,
    S_ORDER_COLOR_ASC,
    S_ABOUT_TITLE,
    S_ABOUT_VERSION,
    S_ABOUT_COPYRIGHT,
    S_ABOUT_DESC,
    S_ABOUT_NOTE,
    S_ABOUT_LINK,
    S_STATUS_NOICONS_MULTI,
    S_COUNT
};

// lang: 0 = follow the system, 1 = English, 2 = Chinese
void SetLanguageSetting(int lang);
int GetLanguageSetting();
// Resolved language actually in use: 1 = English, 2 = Chinese.
int CurrentLanguage();

const wchar_t* T(StrId id);

// Converts a "a|b|c" filter description into the double-NUL form used by
// OPENFILENAME, localizing each part through the string table is up to callers.
std::wstring FilterFromBarString(const wchar_t* s);