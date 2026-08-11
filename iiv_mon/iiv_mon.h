#include <windows.h>
#include <shellapi.h>
#include <Shlwapi.h>

#include <string>
#include <vector>

#include "../../lsMisc/GetAllClipboardFormats.h"
#include "../../lsMisc/AnyCloser.h"
#include "../../lsMisc/DebugMacro.h"
#include "../../lsMisc/stdosd/stdosd.h"

struct ClipImageData
{
    static DWORD lastTick_;
    UINT format_ = 0;
    std::wstring imagePath_;
};