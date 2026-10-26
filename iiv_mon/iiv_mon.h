#include <windows.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <wincrypt.h>

#include <string>
#include <vector>
#include <thread>

#include "../../lsMisc/GetAllClipboardFormats.h"
#include "../../lsMisc/AnyCloser.h"
#include "../../lsMisc/DebugMacro.h"
#include "../../lsMisc/stdosd/stdosd.h"
#include "../../lsMisc/SaveClipboardImageToFile.h"
#include "../../lsMisc/GetUnexistingFile.h"
#include "../../lsMisc/UTF16toUTF8.h"
#include "../../lsMisc/UrlEncode.h"
#include "../../lsMisc/IsDuplicateInstance.h"

struct ClipImageData
{
    static DWORD lastTick_;
    UINT format_ = 0;
    std::wstring imagePath_;
};

#define I18N(s) s

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_CLIPBOARD = WM_APP + 2;
constexpr UINT WM_APP_CLIPBOARDUPDATE = WM_APP + 3;
constexpr UINT ID_TRAY_OPEN = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY_SETTINGS = 1003;
constexpr UINT ID_TRAY = 2001;

constexpr const wchar_t APP_NAME[] = L"iiv_mon";
constexpr const wchar_t IIV_MON_MUTEX_NAME[] = L"iiv_mon_mutex";
constexpr const wchar_t CLASS_NAME[] = L"iiv_mon_window";