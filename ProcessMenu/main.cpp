#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <commctrl.h>
#include <richedit.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <atomic>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")

using namespace std;

// --- Control IDs ---
#define IDC_GROUPBOX 100
#define IDC_RADIO_EXISTING 101
#define IDC_RADIO_NEW 102
#define IDC_RADIO_NEW_LOG 103
#define IDC_PARENT_LABEL 104
#define IDC_PARENT_EDIT 105
#define IDC_EXE_PATH_LABEL 106
#define IDC_EXE_PATH_EDIT 107
#define IDC_BROWSE_BTN 108
#define IDC_LAUNCH_BTN 109
#define IDC_LOG_EDIT 110
#define IDC_COPY_LOG_BTN 111
#define IDC_SAVE_LOG_BTN 112
#define IDC_MEM_VIEWER_BTN 113
#define IDC_BROWSE_PARENT_BTN 114

// --- Process Picker Dialog Control IDs ---
#define IDC_PROCESS_LIST 400
#define ID_PROCESS_OK 401
#define ID_PROCESS_CANCEL 402

// --- Memory Viewer Window Control IDs ---
#define IDC_MV_ADDR_LABEL 200
#define IDC_MV_ADDR_EDIT 201
#define IDC_MV_VIEW_BTN 202
#define IDC_MV_LIST_BTN 203
#define IDC_MV_TAB 204
#define IDC_MV_DUMP_OUTPUT 205
#define IDC_MV_REGION_LIST 206
#define IDC_MV_MODULE_LIST 207
#define ID_COPY_ADDRESS 300
#define ID_VIEW_IN_DUMP 301
#define IDC_MV_THREAD_LIST 208

// --- Custom Window Messages ---
#define WM_USER_LOG (WM_APP + 1)
#define WM_USER_ADD_REGION (WM_APP + 2)
#define WM_USER_REGIONS_DONE (WM_APP + 3)
#define WM_USER_UPDATE_DUMP (WM_APP + 4)
#define WM_USER_DUMP_DONE (WM_APP + 5)
#define WM_USER_ADD_MODULE (WM_APP + 6)
#define WM_USER_MODULES_DONE (WM_APP + 7)
#define WM_USER_DEBUG_SESSION_ENDED (WM_APP + 8)
#define WM_USER_ADD_THREAD (WM_APP + 9)
#define WM_USER_THREADS_DONE (WM_APP + 10)

// --- Global Handles ---
HWND g_hLogEdit;
HWND g_hParentEdit;
HWND g_hExePathEdit;
HWND g_hRadioExisting, g_hRadioNew, g_hRadioNewLog;
HFONT g_hFont;
HFONT g_hMonoFont = NULL;
std::atomic<HANDLE> g_hDebuggedProcess = NULL;
HWND g_hMemView = NULL;
HANDLE g_hDebugThread = NULL;

// --- Structs for Threading ---
struct LogData
{
    std::wstring text;
    COLORREF color;
};

struct DebugThreadData
{
    HWND hMainWnd;
    std::wstring newParentName;
    std::wstring exePathStr;
    HANDLE hParentProcess;
    HANDLE hParentThread;
};

struct MemRegionData
{
    wchar_t baseAddr[20];
    wchar_t size[32];
    wchar_t state[32];
    wchar_t type[32];
    wchar_t protection[64];
};

struct ModuleData
{
    wchar_t name[MAX_PATH];
    wchar_t path[MAX_PATH];
    wchar_t baseAddr[20];
    wchar_t size[32];
};

struct ThreadData
{
    wchar_t threadId[20];
    wchar_t priority[20];
    wchar_t state[32];
};

struct ListRegionsThreadData
{
    HWND hMemView;
    HANDLE hProcess;
};

struct ListModulesThreadData
{
    HWND hMemView;
    HANDLE hProcess;
};

struct ListThreadsThreadData
{
    HWND hMemView;
    HANDLE hProcess;
};

struct ViewMemoryThreadData
{
    HWND hMemView;
    HANDLE hProcess;
    uintptr_t address;
};

// --- Function Prototypes ---
void LogEx(const wstring &text, COLORREF color);
void Log(const wstring &text);
void LogFEx(COLORREF color, const wchar_t *format, ...);
void LogF(const wchar_t *format, ...);
void LaunchWithExistingParent();
void LaunchWithNewParent(bool withLogging);
DWORD FindProcessIdByName(const wstring &processName);
void LaunchProcess(DWORD parentPid, const wstring &exePathStr);
void DebugLoop(PROCESS_INFORMATION &pi, HWND hMainWnd);
const wchar_t *GetExceptionString(DWORD exceptionCode);
bool BrowseForFile(HWND hwnd, wchar_t *filePath, DWORD nFilePathSize);
void HandleLaunch(HWND hwnd);
void HandleCopyLog(HWND hwnd);
void HandleSaveLog(HWND hwnd);
void HandleViewMemory(HWND hMemView);
void HandleListMemoryRegions(HWND hMemView);
void HandleListModules(HWND hMemView);
void HandleListThreads(HWND hMemView);
void SetMainControlsEnabled(HWND hwnd, BOOL bEnabled);
void CreateControls(HWND hwnd);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MemViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterMemViewClass(HINSTANCE hInstance);
void CreateMemViewControls(HWND hwnd);
void PostLogMessage(HWND hwnd, const std::wstring &text, COLORREF color);
void PostLogMessageF(HWND hwnd, COLORREF color, const wchar_t *format, ...);
void PostLogMessageF(HWND hwnd, const wchar_t *format, ...);
DWORD WINAPI DebugThreadProc(LPVOID lpParam);
DWORD WINAPI ListRegionsThreadProc(LPVOID lpParam);
DWORD WINAPI ListModulesThreadProc(LPVOID lpParam);
DWORD WINAPI ListThreadsThreadProc(LPVOID lpParam);
DWORD WINAPI ViewMemoryThreadProc(LPVOID lpParam);
LRESULT CALLBACK ProcessPickerDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterProcessPickerClass(HINSTANCE hInstance);
bool ShowProcessPickerDialog(HWND hOwner, wchar_t *selectedProcess, int maxLen);
void PopulateProcessList(HWND hList);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    // Initialize common controls for modern look
    INITCOMMONCONTROLSEX icex = {sizeof(icex)};
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    LoadLibrary(TEXT("Msftedit.dll"));
    RegisterMemViewClass(hInstance);
    RegisterProcessPickerClass(hInstance);

    // If this instance is launched as a dummy parent, do nothing but wait.
    if (wcscmp(pCmdLine, L"--dummy-parent") == 0)
    {
        Sleep(INFINITE);
        return 0;
    }

    const wchar_t CLASS_NAME[] = L"ProcessMenuWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Process Menu",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 510,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        NONCLIENTMETRICSW ncm = {};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        CreateControls(hwnd);
        break;
    }
    case WM_USER_LOG:
    {
        LogData *logData = (LogData *)wParam;
        if (logData)
        {
            LogEx(logData->text, logData->color);
            delete logData;
        }
        break;
    }
    case WM_USER_DEBUG_SESSION_ENDED:
    {
        SetMainControlsEnabled(hwnd, TRUE);
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_BROWSE_PARENT_BTN:
        {
            wchar_t processName[MAX_PATH] = {0};
            if (ShowProcessPickerDialog(hwnd, processName, MAX_PATH))
            {
                SetWindowTextW(g_hParentEdit, processName);
            }
            break;
        }
        case IDC_BROWSE_BTN:
        {
            wchar_t filePath[MAX_PATH] = {0};
            if (BrowseForFile(hwnd, filePath, MAX_PATH))
            {
                SetWindowTextW(g_hExePathEdit, filePath);
            }
            break;
        }
        case IDC_LAUNCH_BTN:
            HandleLaunch(hwnd);
            break;
        case IDC_COPY_LOG_BTN:
            HandleCopyLog(hwnd);
            break;
        case IDC_SAVE_LOG_BTN:
            HandleSaveLog(hwnd);
            break;
        case IDC_MEM_VIEWER_BTN:
            if (!IsWindow(g_hMemView))
            {
                g_hMemView = CreateWindowExW(
                    WS_EX_TOPMOST, L"MemoryViewerClass", L"Memory Viewer",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                    CW_USEDEFAULT, CW_USEDEFAULT, 700, 500,
                    hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
                if (g_hMemView)
                {
                    ShowWindow(g_hMemView, SW_SHOW);
                }
            }
            else
            {
                SetForegroundWindow(g_hMemView);
            }
            break;
        }
        break;
    }

    case WM_CLOSE:
        if (g_hDebugThread != NULL)
        {
            DWORD exitCode = 0;
            GetExitCodeThread(g_hDebugThread, &exitCode);
            if (exitCode == STILL_ACTIVE)
            {
                MessageBoxW(hwnd, L"A debugging session is in progress. Please wait for it to complete before closing.", L"Session Active", MB_OK | MB_ICONINFORMATION);
                return 0; // Veto the close
            }
        }
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        if (g_hDebugThread != NULL)
        {
            CloseHandle(g_hDebugThread);
        }
        if (g_hFont)
        {
            DeleteObject(g_hFont);
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd)
{
    // Create Group Box
    HWND hGroup = CreateWindowW(L"Button", L"Launch Mode", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                10, 10, 250, 120, hwnd, (HMENU)IDC_GROUPBOX, NULL, NULL);
    SendMessage(hGroup, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Create Radio Buttons
    g_hRadioExisting = CreateWindowW(L"Button", L"Use existing parent process", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                                     20, 30, 230, 20, hwnd, (HMENU)IDC_RADIO_EXISTING, NULL, NULL);
    SendMessage(g_hRadioExisting, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    g_hRadioNew = CreateWindowW(L"Button", L"Create new parent process", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                20, 60, 230, 20, hwnd, (HMENU)IDC_RADIO_NEW, NULL, NULL);
    SendMessage(g_hRadioNew, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    g_hRadioNewLog = CreateWindowW(L"Button", L"Create new parent and log events", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                   20, 90, 230, 20, hwnd, (HMENU)IDC_RADIO_NEW_LOG, NULL, NULL);
    SendMessage(g_hRadioNewLog, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_hRadioExisting, BM_SETCHECK, BST_CHECKED, 0);

    // Create Labels and Edits
    HWND hParentLabel = CreateWindowW(L"Static", L"Parent Process Name:", WS_CHILD | WS_VISIBLE, 270, 15, 150, 20, hwnd, (HMENU)IDC_PARENT_LABEL, NULL, NULL);
    SendMessage(hParentLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    g_hParentEdit = CreateWindowW(L"Edit", L"explorer.exe", WS_CHILD | WS_VISIBLE | WS_BORDER, 270, 35, 250, 25, hwnd, (HMENU)IDC_PARENT_EDIT, NULL, NULL);
    SendMessage(g_hParentEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    HWND hBrowseParent = CreateWindowW(L"Button", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 530, 35, 80, 25, hwnd, (HMENU)IDC_BROWSE_PARENT_BTN, NULL, NULL);
    SendMessage(hBrowseParent, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // --- Memory Viewer Button ---
    HWND hMemButton = CreateWindowW(L"Button", L"Memory Viewer", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 270, 65, 340, 30, hwnd, (HMENU)IDC_MEM_VIEWER_BTN, NULL, NULL);
    SendMessage(hMemButton, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    HWND hExeLabel = CreateWindowW(L"Static", L"Target Executable Path:", WS_CHILD | WS_VISIBLE, 10, 140, 200, 20, hwnd, (HMENU)IDC_EXE_PATH_LABEL, NULL, NULL);
    SendMessage(hExeLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    g_hExePathEdit = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 160, 500, 25, hwnd, (HMENU)IDC_EXE_PATH_EDIT, NULL, NULL);
    SendMessage(g_hExePathEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Create Buttons
    HWND hBrowse = CreateWindowW(L"Button", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 520, 160, 90, 25, hwnd, (HMENU)IDC_BROWSE_BTN, NULL, NULL);
    SendMessage(hBrowse, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    HWND hLaunch = CreateWindowW(L"Button", L"Launch Process", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 200, 600, 30, hwnd, (HMENU)IDC_LAUNCH_BTN, NULL, NULL);
    SendMessage(hLaunch, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Create Log Edit Control
    g_hLogEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                 10, 240, 600, 180, hwnd, (HMENU)IDC_LOG_EDIT, NULL, NULL);
    SendMessage(g_hLogEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Create Log Action Buttons
    HWND hCopyLog = CreateWindowW(L"Button", L"Copy Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 430, 295, 30, hwnd, (HMENU)IDC_COPY_LOG_BTN, NULL, NULL);
    SendMessage(hCopyLog, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    HWND hSaveLog = CreateWindowW(L"Button", L"Save Log As...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 315, 430, 295, 30, hwnd, (HMENU)IDC_SAVE_LOG_BTN, NULL, NULL);
    SendMessage(hSaveLog, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

void SetMainControlsEnabled(HWND hwnd, BOOL bEnabled)
{
    EnableWindow(g_hRadioExisting, bEnabled);
    EnableWindow(g_hRadioNew, bEnabled);
    EnableWindow(g_hRadioNewLog, bEnabled);
    EnableWindow(g_hParentEdit, bEnabled);
    EnableWindow(g_hExePathEdit, bEnabled);
    EnableWindow(GetDlgItem(hwnd, IDC_BROWSE_PARENT_BTN), bEnabled);
    EnableWindow(GetDlgItem(hwnd, IDC_BROWSE_BTN), bEnabled);
    EnableWindow(GetDlgItem(hwnd, IDC_LAUNCH_BTN), bEnabled);
}

void HandleLaunch(HWND hwnd)
{
    SetWindowTextW(g_hLogEdit, L""); // Clear log
    if (SendMessage(g_hRadioExisting, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        LaunchWithExistingParent();
    }
    else if (SendMessage(g_hRadioNew, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        LaunchWithNewParent(false);
    }
    else if (SendMessage(g_hRadioNewLog, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        LaunchWithNewParent(true);
    }
}

void HandleCopyLog(HWND hwnd)
{
    int len = GetWindowTextLengthW(g_hLogEdit);
    if (len == 0)
    {
        Log(L"Log is empty. Nothing to copy.");
        return;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
    if (!hMem)
    {
        LogFEx(RGB(255, 0, 0), L"Error: Failed to allocate memory for clipboard.");
        return;
    }

    wchar_t *pMem = (wchar_t *)GlobalLock(hMem);
    GetWindowTextW(g_hLogEdit, pMem, len + 1);
    GlobalUnlock(hMem);

    if (!OpenClipboard(hwnd))
    {
        GlobalFree(hMem);
        LogFEx(RGB(255, 0, 0), L"Error: Failed to open clipboard.");
        return;
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();

    LogFEx(RGB(0, 128, 0), L"Log content copied to clipboard.");
}

void HandleSaveLog(HWND hwnd)
{
    wchar_t filePath[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Save Log As";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"txt";

    if (GetSaveFileNameW(&ofn))
    {
        int len = GetWindowTextLengthW(g_hLogEdit);
        if (len == 0)
        {
            Log(L"Log is empty. Nothing to save.");
            return;
        }

        vector<wchar_t> buffer(len + 1);
        GetWindowTextW(g_hLogEdit, buffer.data(), len + 1);

        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            LogFEx(RGB(255, 0, 0), L"Error: Could not open file '%s' for writing. Error code: %d", filePath, GetLastError());
            return;
        }

        // Write UTF-16 LE BOM
        DWORD bytesWritten;
        WORD bom = 0xFEFF;
        WriteFile(hFile, &bom, sizeof(bom), &bytesWritten, NULL);

        // Write log content
        WriteFile(hFile, buffer.data(), len * sizeof(wchar_t), &bytesWritten, NULL);

        CloseHandle(hFile);
        LogFEx(RGB(0, 128, 0), L"Log successfully saved to '%s'", filePath);
    }
}

void HandleViewMemory(HWND hMemView)
{
    HANDLE currentProcess = g_hDebuggedProcess.load();
    if (currentProcess == NULL)
    {
        MessageBoxW(hMemView, L"No process is currently being debugged.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    HWND hAddrEdit = GetDlgItem(hMemView, IDC_MV_ADDR_EDIT);
    wchar_t addrStr[256];
    GetWindowTextW(hAddrEdit, addrStr, 256);

    uintptr_t address = 0;
    try
    {
        address = std::stoull(addrStr, nullptr, 16);
    }
    catch (...)
    {
        MessageBoxW(hMemView, L"Invalid memory address format. Please use hexadecimal (e.g., 0x401000).", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    EnableWindow(GetDlgItem(hMemView, IDC_MV_VIEW_BTN), FALSE);
    SetWindowTextW(GetDlgItem(hMemView, IDC_MV_DUMP_OUTPUT), L"Reading memory...");

    ViewMemoryThreadData *data = new ViewMemoryThreadData;
    data->hMemView = hMemView;
    data->hProcess = currentProcess;
    data->address = address;

    HANDLE hThread = CreateThread(NULL, 0, ViewMemoryThreadProc, data, 0, NULL);
    if (hThread == NULL)
    {
        MessageBoxW(hMemView, L"Failed to create memory view thread.", L"Error", MB_OK | MB_ICONERROR);
        delete data;
        EnableWindow(GetDlgItem(hMemView, IDC_MV_VIEW_BTN), TRUE);
    }
    else
    {
        CloseHandle(hThread);
    }
}

DWORD WINAPI ViewMemoryThreadProc(LPVOID lpParam)
{
    ViewMemoryThreadData *data = (ViewMemoryThreadData *)lpParam;
    if (!data)
        return 1;

    unsigned char buffer[256];
    SIZE_T bytesRead = 0;
    // Allow partial reads, which can happen when reading near the end of a memory region.
    // ReadProcessMemory will return FALSE and GetLastError() will be ERROR_PARTIAL_COPY.
    if (!ReadProcessMemory(data->hProcess, (LPCVOID)data->address, buffer, sizeof(buffer), &bytesRead) && GetLastError() != ERROR_PARTIAL_COPY)
    {
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"Could not read process memory. Error code: %d", GetLastError());
        std::wstring *result = new std::wstring(errorMsg);
        PostMessage(data->hMemView, WM_USER_UPDATE_DUMP, (WPARAM)result, 0);
        PostMessage(data->hMemView, WM_USER_DUMP_DONE, 0, 0);
        delete data;
        return 1;
    }

    if (bytesRead == 0)
    {
        std::wstring *result = new std::wstring(L"Zero bytes read from the specified address.");
        PostMessage(data->hMemView, WM_USER_UPDATE_DUMP, (WPARAM)result, 0);
        PostMessage(data->hMemView, WM_USER_DUMP_DONE, 0, 0);
        delete data;
        return 0;
    }

    std::wstringstream ss;
    ss << L"Memory dump at address 0x" << std::hex << data->address << L":\r\n\r\n";
    ss << L"Address           00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\r\n";
    ss << L"--------------------------------------------------------------------------------\r\n";

    for (SIZE_T i = 0; i < bytesRead; ++i)
    {
        if (i % 16 == 0)
        {
            wchar_t addrHex[20];
            swprintf(addrHex, 20, L"0x%016llX  ", (unsigned long long)(data->address + i));
            ss << addrHex;
        }

        wchar_t hex[4];
        swprintf(hex, 4, L"%02X ", buffer[i]);
        ss << hex;

        if ((i + 1) % 8 == 0)
        {
            ss << L" ";
        }

        if ((i + 1) % 16 == 0 || i == bytesRead - 1)
        {
            if (i == bytesRead - 1 && (i + 1) % 16 != 0)
            {
                for (size_t j = 0; j < 15 - (i % 16); ++j)
                {
                    ss << L"   ";
                }
                if ((i % 16) < 7)
                {
                    ss << L" ";
                }
            }

            ss << L"| ";

            for (SIZE_T j = i - (i % 16); j <= i; ++j)
            {
                if (iswprint(buffer[j]))
                {
                    ss << (wchar_t)buffer[j];
                }
                else
                {
                    ss << L".";
                }
            }
            ss << L"\r\n";
        }
    }

    std::wstring *result = new std::wstring(ss.str());
    PostMessage(data->hMemView, WM_USER_UPDATE_DUMP, (WPARAM)result, 0);
    PostMessage(data->hMemView, WM_USER_DUMP_DONE, 0, 0);
    delete data;
    return 0;
}

void HandleListMemoryRegions(HWND hMemView)
{
    HANDLE currentProcess = g_hDebuggedProcess.load();
    if (currentProcess == NULL)
    {
        MessageBoxW(hMemView, L"No process is currently being debugged.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    HWND hList = GetDlgItem(hMemView, IDC_MV_REGION_LIST);
    ListView_DeleteAllItems(hList);

    EnableWindow(GetDlgItem(hMemView, IDC_MV_LIST_BTN), FALSE);

    ListRegionsThreadData *data = new ListRegionsThreadData;
    data->hMemView = hMemView;
    data->hProcess = currentProcess;

    HANDLE hThread = CreateThread(NULL, 0, ListRegionsThreadProc, data, 0, NULL);
    if (hThread == NULL)
    {
        MessageBoxW(hMemView, L"Failed to create memory list thread.", L"Error", MB_OK | MB_ICONERROR);
        delete data;
        EnableWindow(GetDlgItem(hMemView, IDC_MV_LIST_BTN), TRUE);
    }
    else
    {
        CloseHandle(hThread);
    }
}

void HandleListModules(HWND hMemView)
{
    HANDLE currentProcess = g_hDebuggedProcess.load();
    if (currentProcess == NULL)
    {
        MessageBoxW(hMemView, L"No process is currently being debugged.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    HWND hList = GetDlgItem(hMemView, IDC_MV_MODULE_LIST);
    ListView_DeleteAllItems(hList);

    ListModulesThreadData *data = new ListModulesThreadData;
    data->hMemView = hMemView;
    data->hProcess = currentProcess;

    HANDLE hThread = CreateThread(NULL, 0, ListModulesThreadProc, data, 0, NULL);
    if (hThread == NULL)
    {
        MessageBoxW(hMemView, L"Failed to create module list thread.", L"Error", MB_OK | MB_ICONERROR);
        delete data;
    }
    else
    {
        CloseHandle(hThread);
    }
}

void HandleListThreads(HWND hMemView)
{
    HANDLE currentProcess = g_hDebuggedProcess.load();
    if (currentProcess == NULL)
    {
        MessageBoxW(hMemView, L"No process is currently being debugged.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    HWND hList = GetDlgItem(hMemView, IDC_MV_THREAD_LIST);
    ListView_DeleteAllItems(hList);

    ListThreadsThreadData *data = new ListThreadsThreadData;
    data->hMemView = hMemView;
    data->hProcess = currentProcess;

    HANDLE hThread = CreateThread(NULL, 0, ListThreadsThreadProc, data, 0, NULL);
    if (hThread == NULL)
    {
        MessageBoxW(hMemView, L"Failed to create thread list thread.", L"Error", MB_OK | MB_ICONERROR);
        delete data;
    }
    else
    {
        CloseHandle(hThread);
    }
}

DWORD WINAPI ListRegionsThreadProc(LPVOID lpParam)
{
    ListRegionsThreadData *data = (ListRegionsThreadData *)lpParam;
    if (!data)
        return 1;

    unsigned char *p = NULL;
    MEMORY_BASIC_INFORMATION mbi;

    while (VirtualQueryEx(data->hProcess, p, &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        MemRegionData *region = new MemRegionData;

        swprintf(region->baseAddr, 20, L"0x%016llX", (unsigned long long)mbi.BaseAddress);
        swprintf(region->size, 32, L"%llu KB", (unsigned long long)mbi.RegionSize / 1024);

        switch (mbi.State)
        {
        case MEM_COMMIT:
            wcscpy_s(region->state, L"Committed");
            break;
        case MEM_RESERVE:
            wcscpy_s(region->state, L"Reserved");
            break;
        case MEM_FREE:
            wcscpy_s(region->state, L"Free");
            break;
        default:
            wcscpy_s(region->state, L"Unknown");
        }

        switch (mbi.Type)
        {
        case MEM_IMAGE:
            wcscpy_s(region->type, L"Image");
            break;
        case MEM_MAPPED:
            wcscpy_s(region->type, L"Mapped");
            break;
        case MEM_PRIVATE:
            wcscpy_s(region->type, L"Private");
            break;
        default:
            wcscpy_s(region->type, L"---");
        }

        if (mbi.State != MEM_COMMIT)
        {
            wcscpy_s(region->protection, L"---");
        }
        else
        {
            std::wstring protStr;
            if (mbi.Protect & PAGE_EXECUTE)
                protStr += L"X";
            if (mbi.Protect & PAGE_EXECUTE_READ)
                protStr += L"RX";
            if (mbi.Protect & PAGE_EXECUTE_READWRITE)
                protStr += L"RWX";
            if (mbi.Protect & PAGE_EXECUTE_WRITECOPY)
                protStr += L"WCX";
            if (mbi.Protect & PAGE_NOACCESS)
                protStr += L"NA";
            if (mbi.Protect & PAGE_READONLY)
                protStr += L"R";
            if (mbi.Protect & PAGE_READWRITE)
                protStr += L"RW";
            if (mbi.Protect & PAGE_WRITECOPY)
                protStr += L"WC";
            if (protStr.empty())
                protStr = L"Unknown";
            if (mbi.Protect & PAGE_GUARD)
                protStr += L"+G";
            if (mbi.Protect & PAGE_NOCACHE)
                protStr += L"+NC";
            if (mbi.Protect & PAGE_WRITECOMBINE)
                protStr += L"+WC";
            wcscpy_s(region->protection, protStr.c_str());
        }

        PostMessage(data->hMemView, WM_USER_ADD_REGION, (WPARAM)region, 0);

        p += mbi.RegionSize;
    }

    PostMessage(data->hMemView, WM_USER_REGIONS_DONE, 0, 0);
    delete data;
    return 0;
}

DWORD WINAPI ListModulesThreadProc(LPVOID lpParam)
{
    ListModulesThreadData *data = (ListModulesThreadData *)lpParam;
    if (!data)
        return 1;

    DWORD processId = GetProcessId(data->hProcess);
    if (processId == 0)
    {
        delete data;
        return 1;
    }

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        PostMessage(data->hMemView, WM_USER_MODULES_DONE, 0, 0);
        delete data;
        return 1;
    }

    MODULEENTRY32W me32;
    me32.dwSize = sizeof(MODULEENTRY32W);

    if (Module32FirstW(hSnap, &me32))
    {
        do
        {
            ModuleData *module = new ModuleData;

            wcscpy_s(module->name, me32.szModule);
            wcscpy_s(module->path, me32.szExePath);
            swprintf(module->baseAddr, 20, L"0x%016llX", (unsigned long long)me32.modBaseAddr);
            swprintf(module->size, 32, L"%llu KB", (unsigned long long)me32.modBaseSize / 1024);

            PostMessage(data->hMemView, WM_USER_ADD_MODULE, (WPARAM)module, 0);

        } while (Module32NextW(hSnap, &me32));
    }

    CloseHandle(hSnap);
    PostMessage(data->hMemView, WM_USER_MODULES_DONE, 0, 0);
    delete data;
    return 0;
}

DWORD WINAPI ListThreadsThreadProc(LPVOID lpParam)
{
    ListThreadsThreadData *data = (ListThreadsThreadData *)lpParam;
    if (!data)
        return 1;

    DWORD processId = GetProcessId(data->hProcess);
    if (processId == 0)
    {
        delete data;
        return 1;
    }

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        PostMessage(data->hMemView, WM_USER_THREADS_DONE, 0, 0);
        delete data;
        return 1;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hSnap, &te32))
    {
        do
        {
            if (te32.th32OwnerProcessID == processId)
            {
                ThreadData *thread = new ThreadData;

                swprintf(thread->threadId, 20, L"%d", te32.th32ThreadID);
                swprintf(thread->priority, 20, L"%d", te32.tpBasePri);

                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread)
                {
                    DWORD suspendCount = SuspendThread(hThread);
                    if (suspendCount != (DWORD)-1)
                    {
                        ResumeThread(hThread);
                        if (suspendCount > 0)
                        {
                            wcscpy_s(thread->state, L"Suspended");
                        }
                        else
                        {
                            wcscpy_s(thread->state, L"Running");
                        }
                    }
                    else
                    {
                        wcscpy_s(thread->state, L"Unknown");
                    }
                    CloseHandle(hThread);
                }
                else
                {
                    wcscpy_s(thread->state, L"N/A");
                }

                PostMessage(data->hMemView, WM_USER_ADD_THREAD, (WPARAM)thread, 0);
            }
        } while (Thread32Next(hSnap, &te32));
    }

    CloseHandle(hSnap);
    PostMessage(data->hMemView, WM_USER_THREADS_DONE, 0, 0);
    delete data;
    return 0;
}

void LogEx(const wstring &text, COLORREF color)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[100];
    swprintf(timeBuf, 100, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    wstring fullText = timeBuf + text;

    SendMessage(g_hLogEdit, WM_SETREDRAW, FALSE, 0);

    long len = GetWindowTextLength(g_hLogEdit);
    SendMessage(g_hLogEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    if (len > 0)
    {
        SendMessage(g_hLogEdit, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
        len += 2;
        SendMessage(g_hLogEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    }

    CHARFORMAT2W cf;
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    cf.dwEffects = 0;
    SendMessage(g_hLogEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    SendMessage(g_hLogEdit, EM_REPLACESEL, 0, (LPARAM)fullText.c_str());

    SendMessage(g_hLogEdit, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g_hLogEdit, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void Log(const wstring &text)
{
    LogEx(text, RGB(0, 0, 0));
}

void LogFEx(COLORREF color, const wchar_t *format, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, sizeof(buffer) / sizeof(wchar_t), format, args);
    va_end(args);
    LogEx(buffer, color);
}

void LogF(const wchar_t *format, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, sizeof(buffer) / sizeof(wchar_t), format, args);
    va_end(args);
    Log(buffer);
}

void PostLogMessage(HWND hwnd, const std::wstring &text, COLORREF color)
{
    LogData *logData = new LogData{text, color};
    PostMessage(hwnd, WM_USER_LOG, (WPARAM)logData, 0);
}

void PostLogMessageF(HWND hwnd, COLORREF color, const wchar_t *format, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, sizeof(buffer) / sizeof(wchar_t), format, args);
    va_end(args);
    PostLogMessage(hwnd, buffer, color);
}

void PostLogMessageF(HWND hwnd, const wchar_t *format, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, sizeof(buffer) / sizeof(wchar_t), format, args);
    va_end(args);
    PostLogMessage(hwnd, buffer, RGB(0, 0, 0));
}

void LaunchWithExistingParent()
{
    wchar_t parentNameArr[MAX_PATH];
    GetWindowTextW(g_hParentEdit, parentNameArr, MAX_PATH);
    wstring parentName(parentNameArr);

    wchar_t exePathArr[MAX_PATH];
    GetWindowTextW(g_hExePathEdit, exePathArr, MAX_PATH);
    wstring exePathStr(exePathArr);

    if (parentName.empty() || exePathStr.empty())
    {
        LogFEx(RGB(255, 0, 0), L"Error: Parent process name and executable path cannot be empty.");
        return;
    }

    DWORD parentPid = FindProcessIdByName(parentName);
    if (parentPid == 0)
    {
        LogFEx(RGB(255, 0, 0), L"Error: Parent process '%s' not found.", parentName.c_str());
        return;
    }

    LogF(L"Found parent process '%s' with PID: %d", parentName.c_str(), parentPid);
    LaunchProcess(parentPid, exePathStr);
}

DWORD FindProcessIdByName(const wstring &processName)
{
    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    if (Process32FirstW(snapshot, &processEntry))
    {
        do
        {
            if (_wcsicmp(processEntry.szExeFile, processName.c_str()) == 0)
            {
                CloseHandle(snapshot);
                return processEntry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return 0;
}

void LaunchProcess(DWORD parentPid, const wstring &exePathStr)
{
    vector<wchar_t> exePath(&exePathStr[0], &exePathStr[0] + exePathStr.size() + 1);

    HANDLE hParentProcess = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, parentPid);
    if (hParentProcess == NULL)
    {
        LogFEx(RGB(255, 0, 0), L"Error: Could not get a handle to the parent process. Error code: %d", GetLastError());
        return;
    }

    STARTUPINFOEXW siex = {sizeof(siex)};
    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
    siex.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attributeListSize);

    if (siex.lpAttributeList == NULL || !InitializeProcThreadAttributeList(siex.lpAttributeList, 1, 0, &attributeListSize))
    {
        LogFEx(RGB(255, 0, 0), L"Error: Could not initialize process thread attribute list. Error code: %d", GetLastError());
        if (siex.lpAttributeList)
            HeapFree(GetProcessHeap(), 0, siex.lpAttributeList);
        CloseHandle(hParentProcess);
        return;
    }

    if (!UpdateProcThreadAttribute(siex.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hParentProcess, sizeof(HANDLE), NULL, NULL))
    {
        LogFEx(RGB(255, 0, 0), L"Error: Could not update parent process attribute. Error code: %d", GetLastError());
    }
    else
    {
        PROCESS_INFORMATION pi = {0};
        if (!CreateProcessW(NULL, exePath.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siex.StartupInfo, &pi))
        {
            LogFEx(RGB(255, 0, 0), L"Error: Failed to create the child process. Error code: %d", GetLastError());
        }
        else
        {
            LogFEx(RGB(0, 128, 0), L"Success! Process created with PID: %d", pi.dwProcessId);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    DeleteProcThreadAttributeList(siex.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, siex.lpAttributeList);
    CloseHandle(hParentProcess);
}

void LaunchWithNewParent(bool withLogging)
{
    wchar_t newParentNameArr[MAX_PATH];
    GetWindowTextW(g_hParentEdit, newParentNameArr, MAX_PATH);
    wstring newParentName(newParentNameArr);

    wchar_t exePathArr[MAX_PATH];
    GetWindowTextW(g_hExePathEdit, exePathArr, MAX_PATH);
    wstring exePathStr(exePathArr);

    if (newParentName.empty() || exePathStr.empty())
    {
        LogFEx(RGB(255, 0, 0), L"Error: New parent name and executable path cannot be empty.");
        return;
    }

    wchar_t currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);

    if (!CopyFileW(currentExePath, newParentName.c_str(), FALSE))
    {
        LogFEx(RGB(255, 0, 0), L"Error: Could not create a copy of the host executable. Error code: %d", GetLastError());
        return;
    }

    wstring commandLine = L"\"" + newParentName + L"\" --dummy-parent";
    vector<wchar_t> commandLineVec(commandLine.begin(), commandLine.end());
    commandLineVec.push_back(0);

    STARTUPINFOW siParent = {sizeof(siParent)};
    PROCESS_INFORMATION piParent = {0};

    if (!CreateProcessW(NULL, commandLineVec.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &siParent, &piParent))
    {
        LogFEx(RGB(255, 0, 0), L"Error: Failed to create the new parent process. Error code: %d", GetLastError());
        DeleteFileW(newParentName.c_str());
        return;
    }

    LogFEx(RGB(0, 128, 0), L"Created new parent process '%s' with PID: %d", newParentName.c_str(), piParent.dwProcessId);

    if (withLogging)
    {
        if (g_hDebugThread != NULL)
        {
            DWORD exitCode = 0;
            GetExitCodeThread(g_hDebugThread, &exitCode);
            if (exitCode == STILL_ACTIVE)
            {
                LogFEx(RGB(255, 165, 0), L"Warning: A debugging session is already in progress.");
                TerminateProcess(piParent.hProcess, 0);
                CloseHandle(piParent.hProcess);
                CloseHandle(piParent.hThread);
                DeleteFileW(newParentName.c_str());
                return;
            }
            // Previous thread finished, we can close the handle now.
            CloseHandle(g_hDebugThread);
            g_hDebugThread = NULL;
        }

        HWND hMainWnd = GetParent(g_hLogEdit);
        SetMainControlsEnabled(hMainWnd, FALSE);

        DebugThreadData *data = new DebugThreadData;
        data->hMainWnd = hMainWnd;
        data->newParentName = newParentName;
        data->exePathStr = exePathStr;
        data->hParentProcess = piParent.hProcess;
        data->hParentThread = piParent.hThread;

        g_hDebugThread = CreateThread(NULL, 0, DebugThreadProc, data, 0, NULL);
        if (g_hDebugThread == NULL)
        {
            LogFEx(RGB(255, 0, 0), L"Error: Failed to create debugger thread. Error code: %d", GetLastError());
            delete data;
            TerminateProcess(piParent.hProcess, 0);
            CloseHandle(piParent.hProcess);
            CloseHandle(piParent.hThread);
            DeleteFileW(newParentName.c_str());
        }
    }
    else
    {
        LaunchProcess(piParent.dwProcessId, exePathStr);
        TerminateProcess(piParent.hProcess, 0);
        CloseHandle(piParent.hProcess);
        CloseHandle(piParent.hThread);
        Sleep(100);
        if (!DeleteFileW(newParentName.c_str()))
        {
            LogFEx(RGB(255, 165, 0), L"\nWarning: Could not delete temporary file '%s'. Error code: %d", newParentName.c_str(), GetLastError());
        }
    }
}

DWORD WINAPI DebugThreadProc(LPVOID lpParam)
{
    DebugThreadData *data = (DebugThreadData *)lpParam;
    if (!data)
        return 1;

    vector<wchar_t> targetExePath(&data->exePathStr[0], &data->exePathStr[0] + data->exePathStr.size() + 1);
    PROCESS_INFORMATION piChild = {0};
    STARTUPINFOEXW siex = {sizeof(siex)};
    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
    siex.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attributeListSize);
    InitializeProcThreadAttributeList(siex.lpAttributeList, 1, 0, &attributeListSize);
    UpdateProcThreadAttribute(siex.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &data->hParentProcess, sizeof(HANDLE), NULL, NULL);

    if (!CreateProcessW(NULL, targetExePath.data(), NULL, NULL, FALSE, DEBUG_PROCESS | CREATE_NEW_CONSOLE | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siex.StartupInfo, &piChild))
    {
        PostLogMessageF(data->hMainWnd, RGB(255, 0, 0), L"Error: Failed to create the child process for debugging. Error code: %d", GetLastError());
    }
    else
    {
        PostLogMessageF(data->hMainWnd, RGB(0, 128, 0), L"Success! Process created with PID: %d. Attaching debugger...", piChild.dwProcessId);
        PostLogMessageF(data->hMainWnd, L"-------------------- BEGIN LOG --------------------");
        DebugLoop(piChild, data->hMainWnd);
        PostLogMessageF(data->hMainWnd, L"--------------------- END LOG ---------------------");
        CloseHandle(piChild.hProcess);
        CloseHandle(piChild.hThread);
    }
    DeleteProcThreadAttributeList(siex.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, siex.lpAttributeList);

    TerminateProcess(data->hParentProcess, 0);
    CloseHandle(data->hParentProcess);
    CloseHandle(data->hParentThread);

    Sleep(100);
    if (!DeleteFileW(data->newParentName.c_str()))
    {
        PostLogMessageF(data->hMainWnd, RGB(255, 165, 0), L"\nWarning: Could not delete temporary file '%s'. Error code: %d", data->newParentName.c_str(), GetLastError());
    }

    PostMessage(data->hMainWnd, WM_USER_DEBUG_SESSION_ENDED, 0, 0);
    delete data;
    return 0;
}

void DebugLoop(PROCESS_INFORMATION &pi, HWND hMainWnd)
{
    DEBUG_EVENT debugEvent = {0};
    bool keepDebugging = true;
    g_hDebuggedProcess = pi.hProcess;

    while (keepDebugging)
    {
        if (!WaitForDebugEvent(&debugEvent, 100))
        {
            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
            {
                break;
            }
            continue;
        }

        DWORD continueStatus = DBG_CONTINUE;

        switch (debugEvent.dwDebugEventCode)
        {
        case LOAD_DLL_DEBUG_EVENT:
        {
            if (debugEvent.u.LoadDll.hFile != NULL)
            {
                wchar_t path[MAX_PATH];
                if (GetFinalPathNameByHandleW(debugEvent.u.LoadDll.hFile, path, MAX_PATH, 0))
                {
                    wchar_t *filename = wcsrchr(path, L'\\');
                    PostLogMessageF(hMainWnd, RGB(100, 100, 255), L"  [DLL LOAD]   %s", (filename ? filename + 1 : path));
                }
                CloseHandle(debugEvent.u.LoadDll.hFile);
            }
            break;
        }

        case CREATE_THREAD_DEBUG_EVENT:
        {
            PostLogMessageF(hMainWnd, RGB(0, 150, 150), L"  [THREAD CREATE] Thread ID: %d", debugEvent.dwThreadId);
            break;
        }

        case EXIT_THREAD_DEBUG_EVENT:
        {
            PostLogMessageF(hMainWnd, RGB(0, 150, 150), L"  [THREAD EXIT]   Thread ID: %d, Exit Code: %d", debugEvent.dwThreadId, debugEvent.u.ExitThread.dwExitCode);
            break;
        }

        case EXIT_PROCESS_DEBUG_EVENT:
        {
            PostLogMessageF(hMainWnd, RGB(0, 128, 0), L"  [PROCESS EXIT]  Process ID: %d, Exit Code: %d", debugEvent.dwProcessId, debugEvent.u.ExitProcess.dwExitCode);
            keepDebugging = false;
            break;
        }

        case OUTPUT_DEBUG_STRING_EVENT:
        {
            const OUTPUT_DEBUG_STRING_INFO &debugStringInfo = debugEvent.u.DebugString;
            if (debugStringInfo.nDebugStringLength > 0)
            {
                if (debugStringInfo.fUnicode)
                {
                    std::vector<wchar_t> buffer(debugStringInfo.nDebugStringLength + 1, 0);
                    SIZE_T bytesRead = 0;
                    if (ReadProcessMemory(pi.hProcess, debugStringInfo.lpDebugStringData, buffer.data(), debugStringInfo.nDebugStringLength * sizeof(wchar_t), &bytesRead))
                    {
                        PostLogMessageF(hMainWnd, RGB(200, 200, 0), L"  [DEBUG STRING] %s", buffer.data());
                    }
                }
                else
                {
                    std::vector<char> buffer(debugStringInfo.nDebugStringLength + 1, 0);
                    SIZE_T bytesRead = 0;
                    if (ReadProcessMemory(pi.hProcess, debugStringInfo.lpDebugStringData, buffer.data(), debugStringInfo.nDebugStringLength, &bytesRead))
                    {
                        int len = MultiByteToWideChar(CP_ACP, 0, buffer.data(), -1, NULL, 0);
                        if (len > 0)
                        {
                            std::vector<wchar_t> wideBuffer(len);
                            MultiByteToWideChar(CP_ACP, 0, buffer.data(), -1, wideBuffer.data(), len);
                            PostLogMessageF(hMainWnd, RGB(200, 200, 0), L"  [DEBUG STRING] %s", wideBuffer.data());
                        }
                    }
                }
            }
            break;
        }

        case EXCEPTION_DEBUG_EVENT:
        {
            const EXCEPTION_RECORD &record = debugEvent.u.Exception.ExceptionRecord;
            PostLogMessageF(hMainWnd, RGB(255, 0, 0), L"  [EXCEPTION]     Code: 0x%X (%s) at address 0x%p", record.ExceptionCode, GetExceptionString(record.ExceptionCode), record.ExceptionAddress);

            if (debugEvent.u.Exception.dwFirstChance == 1)
            {
                PostLogMessage(hMainWnd, L"                  (First chance)", RGB(255, 0, 0));
            }

            continueStatus = DBG_EXCEPTION_NOT_HANDLED;
            break;
        }
        }
        ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);
    }
    g_hDebuggedProcess = NULL;
}

const wchar_t *GetExceptionString(DWORD exceptionCode)
{
    switch (exceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        return L"Access Violation";
    case EXCEPTION_BREAKPOINT:
        return L"Breakpoint";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return L"Datatype Misalignment";
    case EXCEPTION_SINGLE_STEP:
        return L"Single Step";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return L"Array Bounds Exceeded";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return L"Float Denormal Operand";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return L"Float Divide by Zero";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return L"Float Inexact Result";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return L"Float Invalid Operation";
    case EXCEPTION_FLT_OVERFLOW:
        return L"Float Overflow";
    case EXCEPTION_FLT_STACK_CHECK:
        return L"Float Stack Check";
    case EXCEPTION_FLT_UNDERFLOW:
        return L"Float Underflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return L"Integer Divide by Zero";
    case EXCEPTION_INT_OVERFLOW:
        return L"Integer Overflow";
    case EXCEPTION_PRIV_INSTRUCTION:
        return L"Privileged Instruction";
    case EXCEPTION_IN_PAGE_ERROR:
        return L"In Page Error";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return L"Illegal Instruction";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return L"Noncontinuable Exception";
    case EXCEPTION_STACK_OVERFLOW:
        return L"Stack Overflow";
    case EXCEPTION_INVALID_DISPOSITION:
        return L"Invalid Disposition";
    case EXCEPTION_GUARD_PAGE:
        return L"Guard Page";
    case EXCEPTION_INVALID_HANDLE:
        return L"Invalid Handle";
    default:
        return L"Unknown Exception";
    }
}

bool BrowseForFile(HWND hwnd, wchar_t *filePath, DWORD nFilePathSize)
{
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = filePath;
    ofn.lpstrFile[0] = L'\0';
    ofn.nMaxFile = nFilePathSize;
    ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Select an Executable File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileNameW(&ofn);
}

void RegisterProcessPickerClass(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] = L"ProcessPickerClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = ProcessPickerDlgProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassW(&wc);
}

struct ProcessPickerData
{
    wchar_t *selectedProcess;
    int maxLen;
    bool success;
};

bool ShowProcessPickerDialog(HWND hOwner, wchar_t *selectedProcess, int maxLen)
{
    ProcessPickerData data = {selectedProcess, maxLen, false};

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, L"ProcessPickerClass", L"Select Process",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 500,
        hOwner, NULL, (HINSTANCE)GetWindowLongPtr(hOwner, GWLP_HINSTANCE), &data);

    if (hDlg == NULL)
    {
        return false;
    }

    ShowWindow(hDlg, SW_SHOW);
    EnableWindow(hOwner, FALSE);

    MSG msg = {};
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(hOwner, TRUE);
    SetForegroundWindow(hOwner);

    return data.success;
}

void PopulateProcessList(HWND hList)
{
    ListView_DeleteAllItems(hList);

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE)
    {
        return;
    }

    if (Process32FirstW(hProcessSnap, &pe32))
    {
        int index = 0;
        do
        {
            LVITEMW item = {0};
            item.mask = LVIF_TEXT;
            item.iItem = index;
            item.iSubItem = 0;
            item.pszText = pe32.szExeFile;
            ListView_InsertItem(hList, &item);

            wchar_t pidStr[16];
            swprintf(pidStr, 16, L"%d", pe32.th32ProcessID);
            ListView_SetItemText(hList, index, 1, pidStr);

            index++;
        } while (Process32NextW(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
}

LRESULT CALLBACK ProcessPickerDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static ProcessPickerData *pData = nullptr;

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
        pData = (ProcessPickerData *)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);

        // Create Controls
        HWND hList = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                   10, 10, 360, 400, hwnd, (HMENU)IDC_PROCESS_LIST, NULL, NULL);
        SendMessage(hList, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        lvc.pszText = (LPWSTR)L"Process Name";
        lvc.cx = 270;
        ListView_InsertColumn(hList, 0, &lvc);
        lvc.pszText = (LPWSTR)L"PID";
        lvc.cx = 70;
        ListView_InsertColumn(hList, 1, &lvc);

        PopulateProcessList(hList);

        HWND hOkBtn = CreateWindowW(L"Button", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 100, 420, 80, 25, hwnd, (HMENU)ID_PROCESS_OK, NULL, NULL);
        SendMessage(hOkBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        HWND hCancelBtn = CreateWindowW(L"Button", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 200, 420, 80, 25, hwnd, (HMENU)ID_PROCESS_CANCEL, NULL, NULL);
        SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        break;
    }
    case WM_NOTIFY:
    {
        LPNMHDR lpnmh = (LPNMHDR)lParam;
        if (lpnmh->idFrom == IDC_PROCESS_LIST && lpnmh->code == NM_DBLCLK)
        {
            // Simulate OK button click on double-click
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_PROCESS_OK, 0), 0);
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if (wmId == ID_PROCESS_OK)
        {
            HWND hList = GetDlgItem(hwnd, IDC_PROCESS_LIST);
            int selected = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (selected != -1)
            {
                pData = (ProcessPickerData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
                ListView_GetItemText(hList, selected, 0, pData->selectedProcess, pData->maxLen);
                pData->success = true;
                DestroyWindow(hwnd);
            }
            else
            {
                MessageBoxW(hwnd, L"Please select a process.", L"No Selection", MB_OK | MB_ICONINFORMATION);
            }
        }
        else if (wmId == ID_PROCESS_CANCEL)
        {
            pData = (ProcessPickerData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            pData->success = false;
            DestroyWindow(hwnd);
        }
        break;
    }
    case WM_CLOSE:
        pData = (ProcessPickerData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        pData->success = false;
        DestroyWindow(hwnd);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void RegisterMemViewClass(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] = L"MemoryViewerClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = MemViewWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassW(&wc);
}

void CreateMemViewControls(HWND hwnd)
{
    // Create Labels and Edits
    HWND hAddrLabel = CreateWindowW(L"Static", L"Address:", WS_CHILD | WS_VISIBLE, 10, 15, 60, 20, hwnd, (HMENU)IDC_MV_ADDR_LABEL, NULL, NULL);
    SendMessage(hAddrLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    HWND hAddrEdit = CreateWindowW(L"Edit", L"0x", WS_CHILD | WS_VISIBLE | WS_BORDER, 75, 10, 200, 25, hwnd, (HMENU)IDC_MV_ADDR_EDIT, NULL, NULL);
    SendMessage(hAddrEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Create Buttons
    HWND hViewBtn = CreateWindowW(L"Button", L"View Memory", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 285, 10, 120, 25, hwnd, (HMENU)IDC_MV_VIEW_BTN, NULL, NULL);
    SendMessage(hViewBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Create Tab Control
    HWND hTab = CreateWindowW(WC_TABCONTROL, L"", WS_CHILD | WS_VISIBLE, 10, 45, 660, 400, hwnd, (HMENU)IDC_MV_TAB, NULL, NULL);
    SendMessage(hTab, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"Memory Dump";
    TabCtrl_InsertItem(hTab, 0, &tie);
    tie.pszText = (LPWSTR)L"Memory Regions";
    TabCtrl_InsertItem(hTab, 1, &tie);
    tie.pszText = (LPWSTR)L"Modules";
    TabCtrl_InsertItem(hTab, 2, &tie);
    tie.pszText = (LPWSTR)L"Threads";
    TabCtrl_InsertItem(hTab, 3, &tie);

    // Calculate the display area for the tab's content, relative to the main window
    RECT rcTabWindow;
    GetWindowRect(hTab, &rcTabWindow);
    MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rcTabWindow, 2); // Convert screen to client coords

    RECT rcTabClient;
    GetClientRect(hTab, &rcTabClient);
    TabCtrl_AdjustRect(hTab, FALSE, &rcTabClient);

    int childX = rcTabWindow.left + rcTabClient.left;
    int childY = rcTabWindow.top + rcTabClient.top;
    int childWidth = rcTabClient.right - rcTabClient.left;
    int childHeight = rcTabClient.bottom - rcTabClient.top;

    // Create Dump Output Edit Control
    HWND hDumpOutput = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY,
                                       childX, childY, childWidth, childHeight,
                                       hwnd, (HMENU)IDC_MV_DUMP_OUTPUT, NULL, NULL);
    if (g_hMonoFont == NULL)
    {
        g_hMonoFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    SendMessage(hDumpOutput, WM_SETFONT, (WPARAM)g_hMonoFont, TRUE);

    // Create Region List View
    HWND hRegionList = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                     childX, childY, childWidth, childHeight,
                                     hwnd, (HMENU)IDC_MV_REGION_LIST, NULL, NULL);
    SendMessage(hRegionList, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    ListView_SetExtendedListViewStyle(hRegionList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Get client width to precisely set column widths and avoid scrollbars/misalignment
    RECT rcList;
    GetClientRect(hRegionList, &rcList);
    int listWidth = rcList.right - rcList.left;

    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    int colWidths[] = {150, 100, 90, 90};
    const wchar_t *colNames[] = {L"Base Address", L"Size", L"State", L"Type"};
    int totalWidth = 0;

    for (int i = 0; i < 4; ++i)
    {
        lvc.iSubItem = i;
        lvc.pszText = (LPWSTR)colNames[i];
        lvc.cx = colWidths[i];
        ListView_InsertColumn(hRegionList, i, &lvc);
        totalWidth += colWidths[i];
    }

    lvc.iSubItem = 4;
    lvc.pszText = (LPWSTR)L"Protection";
    // Set the last column to fill the remaining space. Subtracting 4 pixels accounts for borders.
    lvc.cx = listWidth - totalWidth - 4;
    ListView_InsertColumn(hRegionList, 4, &lvc);

    // Create Module List View
    HWND hModuleList = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                     childX, childY, childWidth, childHeight,
                                     hwnd, (HMENU)IDC_MV_MODULE_LIST, NULL, NULL);
    SendMessage(hModuleList, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    ListView_SetExtendedListViewStyle(hModuleList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Setup columns for module list
    GetClientRect(hModuleList, &rcList);
    listWidth = rcList.right - rcList.left;

    LVCOLUMNW lvcModule = {0};
    lvcModule.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    const wchar_t *moduleColNames[] = {L"Module Name", L"Base Address", L"Size"};
    int moduleColWidths[] = {180, 150, 100};
    totalWidth = 0;

    for (int i = 0; i < 3; ++i)
    {
        lvcModule.iSubItem = i;
        lvcModule.pszText = (LPWSTR)moduleColNames[i];
        lvcModule.cx = moduleColWidths[i];
        ListView_InsertColumn(hModuleList, i, &lvcModule);
        totalWidth += moduleColWidths[i];
    }

    lvcModule.iSubItem = 3;
    lvcModule.pszText = (LPWSTR)L"Path";
    lvcModule.cx = listWidth - totalWidth - 4;
    ListView_InsertColumn(hModuleList, 3, &lvcModule);

    // Create Thread List View
    HWND hThreadList = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                     childX, childY, childWidth, childHeight,
                                     hwnd, (HMENU)IDC_MV_THREAD_LIST, NULL, NULL);
    SendMessage(hThreadList, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    ListView_SetExtendedListViewStyle(hThreadList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Setup columns for thread list
    GetClientRect(hThreadList, &rcList);
    listWidth = rcList.right - rcList.left;

    LVCOLUMNW lvcThread = {0};
    lvcThread.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    const wchar_t *threadColNames[] = {L"Thread ID", L"Base Priority", L"State"};
    int threadColWidths[] = {120, 120, 120};

    for (int i = 0; i < 3; ++i)
    {
        lvcThread.iSubItem = i;
        lvcThread.pszText = (LPWSTR)threadColNames[i];
        lvcThread.cx = threadColWidths[i];
        ListView_InsertColumn(hThreadList, i, &lvcThread);
    }

    // Show the first page, hide the second
    ShowWindow(hDumpOutput, SW_SHOW);
    ShowWindow(hRegionList, SW_HIDE);
    ShowWindow(hModuleList, SW_HIDE);
    ShowWindow(hThreadList, SW_HIDE);
}

LRESULT CALLBACK MemViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateMemViewControls(hwnd);
        break;
    }
    case WM_USER_ADD_REGION:
    {
        MemRegionData *region = (MemRegionData *)wParam;
        if (region)
        {
            HWND hList = GetDlgItem(hwnd, IDC_MV_REGION_LIST);
            int index = ListView_GetItemCount(hList);

            LVITEMW item = {0};
            item.mask = LVIF_TEXT;
            item.iItem = index;
            item.iSubItem = 0;
            item.pszText = region->baseAddr;
            ListView_InsertItem(hList, &item);

            ListView_SetItemText(hList, index, 1, region->size);
            ListView_SetItemText(hList, index, 2, region->state);
            ListView_SetItemText(hList, index, 3, region->type);
            ListView_SetItemText(hList, index, 4, region->protection);

            delete region;
        }
        break;
    }
    case WM_USER_ADD_MODULE:
    {
        ModuleData *module = (ModuleData *)wParam;
        if (module)
        {
            HWND hList = GetDlgItem(hwnd, IDC_MV_MODULE_LIST);
            int index = ListView_GetItemCount(hList);

            LVITEMW item = {0};
            item.mask = LVIF_TEXT;
            item.iItem = index;
            item.iSubItem = 0;
            item.pszText = module->name;
            ListView_InsertItem(hList, &item);

            ListView_SetItemText(hList, index, 1, module->baseAddr);
            ListView_SetItemText(hList, index, 2, module->size);
            ListView_SetItemText(hList, index, 3, module->path);

            delete module;
        }
        break;
    }
    case WM_USER_ADD_THREAD:
    {
        ThreadData *thread = (ThreadData *)wParam;
        if (thread)
        {
            HWND hList = GetDlgItem(hwnd, IDC_MV_THREAD_LIST);
            int index = ListView_GetItemCount(hList);

            LVITEMW item = {0};
            item.mask = LVIF_TEXT;
            item.iItem = index;
            item.iSubItem = 0;
            item.pszText = thread->threadId;
            ListView_InsertItem(hList, &item);

            ListView_SetItemText(hList, index, 1, thread->priority);
            ListView_SetItemText(hList, index, 2, thread->state);

            delete thread;
        }
        break;
    }
    case WM_USER_REGIONS_DONE:
    {
        EnableWindow(GetDlgItem(hwnd, IDC_MV_LIST_BTN), TRUE);
        break;
    }
    case WM_USER_MODULES_DONE:
    {
        break;
    }
    case WM_USER_THREADS_DONE:
    {
        break;
    }
    case WM_USER_UPDATE_DUMP:
    {
        std::wstring *result = (std::wstring *)wParam;
        if (result)
        {
            SetWindowTextW(GetDlgItem(hwnd, IDC_MV_DUMP_OUTPUT), result->c_str());
            delete result;
        }
        break;
    }
    case WM_USER_DUMP_DONE:
    {
        EnableWindow(GetDlgItem(hwnd, IDC_MV_VIEW_BTN), TRUE);
        break;
    }
    case WM_NOTIFY:
    {
        LPNMHDR lpnmh = (LPNMHDR)lParam;
        if (lpnmh->idFrom == IDC_MV_TAB && lpnmh->code == TCN_SELCHANGE)
        {
            int iPage = TabCtrl_GetCurSel(lpnmh->hwndFrom);
            ShowWindow(GetDlgItem(hwnd, IDC_MV_DUMP_OUTPUT), iPage == 0 ? SW_SHOW : SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_MV_REGION_LIST), iPage == 1 ? SW_SHOW : SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_MV_MODULE_LIST), iPage == 2 ? SW_SHOW : SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_MV_THREAD_LIST), iPage == 3 ? SW_SHOW : SW_HIDE);
            if (iPage == 1)
            {
                if (ListView_GetItemCount(GetDlgItem(hwnd, IDC_MV_REGION_LIST)) == 0)
                {
                    HandleListMemoryRegions(hwnd);
                }
            }
            else if (iPage == 2)
            {
                if (ListView_GetItemCount(GetDlgItem(hwnd, IDC_MV_MODULE_LIST)) == 0)
                {
                    HandleListModules(hwnd);
                }
            }
            else if (iPage == 3)
            {
                if (ListView_GetItemCount(GetDlgItem(hwnd, IDC_MV_THREAD_LIST)) == 0)
                {
                    HandleListThreads(hwnd);
                }
            }
        }
        else if (lpnmh->idFrom == IDC_MV_REGION_LIST && lpnmh->code == NM_RCLICK)
        {
            LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
            if (lpnmitem->iItem != -1)
            {
                HMENU hMenu = CreatePopupMenu();
                InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_COPY_ADDRESS, L"Copy Address");
                InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_VIEW_IN_DUMP, L"View in Memory Dump");

                POINT pt;
                GetCursorPos(&pt);
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_MV_VIEW_BTN:
            HandleViewMemory(hwnd);
            break;
        case ID_COPY_ADDRESS:
        {
            HWND hList = GetDlgItem(hwnd, IDC_MV_REGION_LIST);
            int selected = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (selected != -1)
            {
                wchar_t buffer[20];
                ListView_GetItemText(hList, selected, 0, buffer, 20);

                if (OpenClipboard(hwnd))
                {
                    EmptyClipboard();
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (wcslen(buffer) + 1) * sizeof(wchar_t));
                    if (hg)
                    {
                        memcpy(GlobalLock(hg), buffer, (wcslen(buffer) + 1) * sizeof(wchar_t));
                        GlobalUnlock(hg);
                        SetClipboardData(CF_UNICODETEXT, hg);
                    }
                    CloseClipboard();
                }
            }
            break;
        }
        case ID_VIEW_IN_DUMP:
        {
            HWND hList = GetDlgItem(hwnd, IDC_MV_REGION_LIST);
            int selected = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (selected != -1)
            {
                wchar_t buffer[20];
                ListView_GetItemText(hList, selected, 0, buffer, 20);

                SetWindowTextW(GetDlgItem(hwnd, IDC_MV_ADDR_EDIT), buffer);

                HWND hTab = GetDlgItem(hwnd, IDC_MV_TAB);
                TabCtrl_SetCurSel(hTab, 0);

                ShowWindow(GetDlgItem(hwnd, IDC_MV_DUMP_OUTPUT), SW_SHOW);
                ShowWindow(GetDlgItem(hwnd, IDC_MV_REGION_LIST), SW_HIDE);

                HandleViewMemory(hwnd);
            }
            break;
        }
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        if (g_hMonoFont)
        {
            DeleteObject(g_hMonoFont);
            g_hMonoFont = NULL;
        }
        g_hMemView = NULL;
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}