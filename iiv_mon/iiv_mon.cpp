#include "iiv_mon.h"
#include "Settngs.h"

#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

using namespace Ambiesoft;
using namespace Ambiesoft::stdosd;

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_CLIPBOARD = WM_APP + 2;
constexpr UINT ID_TRAY_OPEN = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY_SETTINGS = 1003;
constexpr UINT ID_TRAY = 2001;

NOTIFYICONDATAW g_nid{};
HWND g_hwnd = nullptr;
DWORD ClipImageData::lastTick_ = 0;

// Compute MD5 of a file using CryptoAPI
static bool ComputeFileMD5(const std::wstring& filePath, std::wstring& outHex)
{
    HANDLE hFile;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool success = false;
    do {
        outHex.clear();
        hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
            break;

        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
            break;

        const DWORD bufSize = 64 * 1024;
        std::vector<BYTE> buffer(bufSize);
        DWORD bytesRead = 0;
        bool bBreak = false;
        while (ReadFile(hFile, buffer.data(), bufSize, &bytesRead, nullptr) && bytesRead > 0)
        {
            if (!CryptHashData(hHash, buffer.data(), bytesRead, 0))
            {
                bBreak = true;
                break;
            }
        }
        if (bBreak)
            break;

        BYTE rgbHash[16];
        DWORD cbHash = sizeof(rgbHash);
        if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
            break;

        static const wchar_t hexDigits[] = L"0123456789abcdef";
        outHex.reserve(cbHash * 2);
        for (DWORD i = 0; i < cbHash; ++i)
        {
            BYTE b = rgbHash[i];
            outHex.push_back(hexDigits[b >> 4]);
            outHex.push_back(hexDigits[b & 0x0F]);
        }

        success = true;
    } while (false);

    if (hHash)
        CryptDestroyHash(hHash);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);
    return success;
}

void ShowTrayMenu()
{
    POINT pt{};
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open with iiv_view");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"&Settings");
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu);
}

std::wstring getTempImageDirectory()
{
    std::wstring tempDir = stdCombinePath(
        stdGetParentDirectory(stdGetModuleFileName()).c_str(),
        L"temp");
    if (!CreateDirectory(tempDir.c_str(), nullptr))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            MessageBox(g_hwnd, stdFormat(L"Failed to create temp directory: %s", tempDir.c_str()).c_str(), L"Error", MB_ICONERROR);
            return L"";
        }
    }
	return tempDir;
}
std::wstring getTempImagePath()
{
	std::wstring tempDir = getTempImageDirectory();
	if (tempDir.empty())
		return L"";

    std::wstring tempPath = GetUnexistingFile(
		tempDir.c_str(),
        L"iiv-tempimage", L".bmp");
    return tempPath;
}

// Implementation plan (detailed pseudocode):
// 1. Obtain the temporary directory by calling `getTempImageDirectory()`.
//    - If the returned path is empty, fail and return false.
// 2. Construct a search pattern to enumerate all files in the directory:
//    - `stdCombinePath(tempDir.c_str(), L"*.*")`.
// 3. Call `FindFirstFileW` with the search pattern.
//    - If it returns `INVALID_HANDLE_VALUE`, treat as no files / nothing to remove and return true.
// 4. Use `do { ... } while (FindNextFileW(...));` loop to enumerate entries.
//    - For each `WIN32_FIND_DATAW` entry:
//      a. If `findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY` is set, `continue` (skip directories).
//      b. Read the file name from `findData.cFileName` into `std::wstring filename`.
//      c. Locate the last dot (`.`) character to split extension:
//         - If not found, `continue`.
//         - Let `nameNoExt = filename.substr(0, posDot)` and `ext = filename.substr(posDot)`
//           (note: `ext` includes the leading dot).
//      d. Only target files with `.bmp` extension (case-insensitive).
//         - Use `_wcsicmp(ext.c_str(), L".bmp")` to check.
//         - If not `.bmp`, `continue`.
//      e. Expect the file name (without extension) to be in the form: `<size>-<md5>-<number>`.
//         - Find `firstDash = nameNoExt.find(L'-')` and `lastDash = nameNoExt.rfind(L'-')`.
//         - If either dash not found or `firstDash == lastDash`, `continue`.
//         - Extract `partSize = nameNoExt.substr(0, firstDash)`,
//                   `partMd5 = nameNoExt.substr(firstDash + 1, lastDash - firstDash - 1)`,
//                   `partNum = nameNoExt.substr(lastDash + 1)`.
//         - If any part is empty, `continue`.
//      f. Validate each part:
//         - `partSize` must contain only digits (use `iswdigit` in a loop).
//         - `partNum` must contain only digits.
//         - `partMd5` must be exactly 32 characters and each character must be a hex digit:
//           (0-9, a-f, A-F). Check length first, then iterate characters.
//      g. If all validations pass, build the full file path using `stdCombinePath(tempDir.c_str(), filename.c_str())`
//         and call `DeleteFileW(fullPath.c_str())` to remove it.
//         - Ignore delete failures (do not abort enumeration); proceed to next file.
// 5. After enumeration completes, call `FindClose(hFind)` and return true.
// 6. Notes and caveats:
//    - The loop must be structured as `do { } while (FindNextFileW(hFind, &findData));`
//      so that `continue` statements inside the body correctly advance to the next file.
//    - Do not delete files that do not strictly match the expected pattern to avoid accidental removal.
//    - Be robust to errors: when encountering malformed entries or IO errors, skip and continue.

bool RemoveOldTempImageFiles()
{
    /*
    Implementation plan (detailed pseudocode):
    1. Get the temporary directory by calling `getTempImageDirectory()`.
       - If empty, return false.
    2. Create a search pattern `stdCombinePath(tempDir.c_str(), L"*.*")` and start file enumeration.
       - If `FindFirstFileW` returns `INVALID_HANDLE_VALUE`, treat as no target files and return true.
    3. Get current time via `GetSystemTimeAsFileTime` and convert to 100-nanosecond units (`ULARGE_INTEGER::QuadPart`).
    4. Compute threshold by subtracting 14 days in 100-nanosecond units: `threshold = now - 14days`.
    5. Enumerate with `do { ... } while (FindNextFileW(...));`. For each entry:
       a. Skip directories.
       b. Obtain filename as `std::wstring filename(findData.cFileName);`.
       c. Convert the file's last write time `findData.ftLastWriteTime` to `ULARGE_INTEGER` and get `fileTime.QuadPart`.
       d. If `fileTime.QuadPart > threshold` (i.e., newer than 14 days), skip (continue).
       e. Verify the extension exists and is `.bmp` (case-insensitive); otherwise continue.
       f. Verify the name without extension matches `<size>-<md5>-<number>`:
          - Find first and last '-' positions and ensure parts are non-empty.
          - `partSize` and `partNum` must be digits only; `partMd5` must be 32 hex characters.
       g. If all validations pass and the file is older than 14 days, build the full path with `stdCombinePath` and call `DeleteFileW` (ignore failures).
    6. After enumeration, call `FindClose(hFind)` and return true.
    Notes:
    - Use `do { } while (FindNextFileW(...));` so that `continue` advances correctly.
    - Deletion condition: the file's last write time must be older than 14 days.
    */

    std::wstring tempDir = getTempImageDirectory();
    if (tempDir.empty())
        return false;

    WIN32_FIND_DATAW findData{};
    std::wstring searchPattern = stdCombinePath(tempDir.c_str(), L"*.*");
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return true;

    // Get current time and compute threshold by subtracting 14 days expressed in 100-nanosecond units
    FILETIME ftNow{};
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER uiNow{};
    uiNow.LowPart = ftNow.dwLowDateTime;
    uiNow.HighPart = ftNow.dwHighDateTime;

    const ULONGLONG fourteenDays100ns = 14ULL * 24ULL * 60ULL * 60ULL * 10000000ULL;
    ULONGLONG threshold = 0;
    if (uiNow.QuadPart > fourteenDays100ns)
        threshold = uiNow.QuadPart - fourteenDays100ns;
    else
        threshold = 0; // Use 0 as threshold in case of overflow

    do
    {
        // Skip directories
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        // Check last write time: skip files newer than 14 days
        ULARGE_INTEGER fileTime{};
        fileTime.LowPart = findData.ftLastWriteTime.dwLowDateTime;
        fileTime.HighPart = findData.ftLastWriteTime.dwHighDateTime;

        if (fileTime.QuadPart > threshold)
            continue; // Do not delete files newer than 14 days

        std::wstring filename(findData.cFileName);

        // Separate extension
        auto posDot = filename.find_last_of(L'.');
        if (posDot == std::wstring::npos)
            continue;

        std::wstring nameNoExt = filename.substr(0, posDot);
        std::wstring ext = filename.substr(posDot); // includes '.'

        // Only target .bmp (case-insensitive)
        if (_wcsicmp(ext.c_str(), L".bmp") != 0)
            continue;

        // Format: <size>-<md5>-<number>
        size_t firstDash = nameNoExt.find(L'-');
        size_t lastDash = nameNoExt.rfind(L'-');
        if (firstDash == std::wstring::npos || lastDash == std::wstring::npos || firstDash == lastDash)
            continue;

        std::wstring partSize = nameNoExt.substr(0, firstDash);
        std::wstring partMd5 = nameNoExt.substr(firstDash + 1, lastDash - firstDash - 1);
        std::wstring partNum = nameNoExt.substr(lastDash + 1);

        if (partSize.empty() || partMd5.empty() || partNum.empty())
            continue;

        // Helpers
        auto isDigits = [](const std::wstring& s) -> bool {
            for (wchar_t c : s)
            {
                if (!iswdigit(c))
                    return false;
            }
            return true;
        };
        auto isHex32 = [](const std::wstring& s) -> bool {
            if (s.length() != 32) return false;
            for (wchar_t c : s)
            {
                if (!(iswdigit(c) ||
                      (c >= L'a' && c <= L'f') ||
                      (c >= L'A' && c <= L'F')))
                    return false;
            }
            return true;
        };

        if (!isDigits(partSize))
            continue;
        if (!isHex32(partMd5))
            continue;
        if (!isDigits(partNum))
            continue;

        // Build full file path and compute MD5
        std::wstring filePath = stdCombinePath(tempDir.c_str(), filename.c_str());

        std::wstring computedMd5;
        if (!ComputeFileMD5(filePath, computedMd5))
        {
            // If MD5 computation fails, do not delete
            continue;
        }

        // Compare with partMd5 (case-insensitive)
        if (_wcsicmp(partMd5.c_str(), computedMd5.c_str()) != 0)
        {
            // Do not delete if not matched
            continue;
        }

        // All validations passed, delete file
        DeleteFileW(filePath.c_str());

    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return true;
}

bool GetClipboardImage4(ClipImageData* imageData)
{
    std::wstring error;
    std::wstring tempImagePath = getTempImagePath();
    if(tempImagePath.empty())
		return false;   

    if (!SaveClipboardImageToFile(tempImagePath.c_str(), &error))
    {
        MessageBox(g_hwnd, error.c_str(), L"Error", MB_ICONERROR);
        return false;
    }

    const std::wstring ext = stdGetFileExtension(tempImagePath.c_str());

    /*
    Pseudocode / Plan:
    1. Open the saved file at 'tempImagePath' for reading.
    2. Initialize CryptoAPI context with CryptAcquireContext for hashing.
    3. Create an MD5 hash object with CryptCreateHash.
    4. Read the file in a loop (e.g. 64KB chunks) and feed each chunk to CryptHashData.
    5. Finalize and retrieve the raw hash bytes via CryptGetHashParam (HP_HASHVAL).
    6. Convert the raw bytes to a lowercase hexadecimal wide string.
    7. Store the resulting MD5 hex string into 'imageData->md5_' (if available).
    8. Clean up CryptoAPI objects and close the file handle.
    9. If any step fails, perform cleanup and report failure.
    */
    // Calculate MD5 and store it (if computation succeeds)
    std::wstring md5;
    if (!ComputeFileMD5(tempImagePath, md5))
    {
        MessageBox(g_hwnd, L"Failed to get md5", L"Error", MB_ICONERROR);
        return false;
    }

    const long fileSize = stdGetFileSize(tempImagePath.c_str());

    const std::wstring newFilenameWithoutExt = stdFormat(L"%d-%s",
        fileSize,
        md5.c_str());

    // Format of a file is '{size}-{md5}-{number}.bmp'
    // {number} is continuous number starting from '0'.
    // ex:3686454-8e361171231c729bf3815895ae7cd39c-0.bmp
    const std::wstring newFilename = newFilenameWithoutExt + L"-0" + ext;

	// Rename the file to include size and MD5
	std::wstring newFilePath = stdCombinePath(
        stdGetParentDirectory(tempImagePath), newFilename);
    if (stdFileExists(newFilePath))
    {
        newFilePath = GetUnexistingFile(
            stdGetParentDirectory(newFilePath).c_str(),
            (newFilenameWithoutExt + L"-").c_str(), ext.c_str());
    }
    DTRACE(L"Image temp file:" + newFilePath);

    if (!MoveFile(tempImagePath.c_str(), newFilePath.c_str()))
    {
        MessageBox(g_hwnd, L"Failed to rename file", L"Error", MB_ICONERROR);
		return false;
    }

    imageData->imagePath_ = newFilePath;
    return true;
}
bool GetClipboardImage3(ClipImageData* imageData)
{
    if (!OpenClipboard(g_hwnd))
        return false;
    ClipboardCloser clipboardCloser;

    if (IsClipboardFormatAvailable(CF_BITMAP))
    {
        imageData->format_ = CF_BITMAP;
        return GetClipboardImage4(imageData);
    }
    if (IsClipboardFormatAvailable(CF_DIB))
    {
        imageData->format_ = CF_DIB;
        return GetClipboardImage4(imageData);
    }
    if (IsClipboardFormatAvailable(CF_DIBV5))
    {
        imageData->format_ = CF_DIBV5;
        return GetClipboardImage4(imageData);
    }

    if (IsClipboardFormatAvailable(CF_HDROP))
    {
        HANDLE hDrop = GetClipboardData(CF_HDROP);
        if (hDrop)
        {
            UINT fileCount = DragQueryFileW((HDROP)hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < fileCount; ++i)
            {
                wchar_t filePath[MAX_PATH]{};
                DragQueryFileW((HDROP)hDrop, i, filePath, MAX_PATH);
                DTRACE(filePath);
                std::wstring ext = filePath;
                auto pos = ext.find_last_of(L'.');
                if (pos != std::wstring::npos)
                    ext = ext.substr(pos + 1);
                else
                    ext.clear();
                if (_wcsicmp(ext.c_str(), L"png") == 0 ||
                    _wcsicmp(ext.c_str(), L"jpg") == 0 ||
                    _wcsicmp(ext.c_str(), L"jpeg") == 0 ||
                    _wcsicmp(ext.c_str(), L"bmp") == 0 ||
                    _wcsicmp(ext.c_str(), L"gif") == 0)
                {
                    imageData->format_ = CF_HDROP;
                    imageData->imagePath_ = filePath;
                    return true;
                }
            }
        }
    }
    return false;
}
bool GetClipboardImage2(ClipImageData* imageData)
{
    if (!GetClipboardImage3(imageData))
        return false;

    return true;
}
bool GetClipboardImage(ClipImageData* imageData)
{
    if (!GetClipboardImage2(imageData))
        return false;

    static ClipImageData lastImageData;

    if(imageData->format_==0)
        return false;
    
    DWORD currentTick = GetTickCount();
    if (imageData->format_ == lastImageData.format_ &&
        imageData->imagePath_ == lastImageData.imagePath_ &&
        (currentTick - lastImageData.lastTick_) < 1000)
    {
        return false;
    }
    lastImageData = *imageData;
    lastImageData.lastTick_ = currentTick;
    return true;
}


void NotifyImageCopied()
{
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, L"iiv");
    wcscpy_s(g_nid.szInfo, I18N(L"Detected Clipboard change. Opening viewer..."));
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

std::wstring getViewer()
{
    std::wstring viewerPath = getSettings().getViewer();
    if (viewerPath.empty())
    {
        std::wstring iivViewPath = stdCombinePath(
            stdGetParentDirectory(stdGetModuleFileName()),
            L"iiv_view.exe");
        return iivViewPath;
    }
    // return L"C:/local/ImageGlass/ImageGlass.exe";
    // return L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
	return viewerPath;
}
void OpenViewer(const wchar_t* imagePath = nullptr)
{
    std::wstring exe = getViewer();
    std::wstring arg = stdFormat(L"%s", 
        imagePath ? stdAddDQIfNecessary(imagePath).c_str() : L"");

	// MessageBox(nullptr, stdFormat(L"exe: %s\narg: %s", exe.c_str(), arg.c_str()).c_str(), L"Debug", MB_OK);

    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpFile = exe.c_str();
    sei.lpParameters = arg.c_str();
    sei.nShow = SW_SHOWNORMAL;
    
    ShellExecuteExW(&sei);
}

void OnSettings()
{
    std::wstring exe = stdCombinePath(
        stdGetParentDirectory(stdGetModuleFileName()),
        L"iiv_setting.exe");
    std::wstring arg = stdFormat(L"--return-cmd %s", UrlEncodeStd(::GetCommandLine()).c_str());

    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpFile = exe.c_str();
	sei.lpParameters = arg.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) 
    {
		DestroyWindow(g_hwnd);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        AddClipboardFormatListener(hwnd);
        return 0;

    case WM_CLIPBOARDUPDATE:
    {
        ClipImageData clipImageData;
        if (GetClipboardImage(&clipImageData))
        {
            NotifyImageCopied();
            OpenViewer(clipImageData.imagePath_.c_str());
        }
        return 0;
    }
    break;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu();
        else if (lParam == WM_LBUTTONDBLCLK)
            OpenViewer();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TRAY_OPEN:
            OpenViewer();
            return 0;
		case ID_TRAY_SETTINGS:
            OnSettings();
            return 0;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    if (IsDuplicateInstance(IIV_MON_MUTEX_NAME))
    {
        MessageBox(nullptr, L"iiv_mon is already running", APP_NAME, MB_ICONERROR);
		return 1;
    }
    if (!getSettings().loadSettings())
    {
        MessageBox(nullptr, L"Failed to load settings", APP_NAME, MB_ICONERROR);
		return 1;
    }

    if (!RemoveOldTempImageFiles())
    {
		MessageBox(nullptr, L"Failed to remove old temp image files", APP_NAME, MB_ICONERROR);
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIconW(
        hInstance,
        MAKEINTRESOURCEW(IDI_ICON_MON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"iiv_mon",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!g_hwnd)
        return 1;

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = ID_TRAY;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_nid.hIcon = LoadIconW(
        hInstance,
        MAKEINTRESOURCEW(IDI_ICON_MON));
    wcscpy_s(g_nid.szTip, L"iiv - Clipboard Image Viewer");

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
