/*
 * brightness_bridge.c — Lightweight HID Brightness → ClickMonitorDDC Bridge
 *
 * Captures HID Consumer Control brightness keys (Fn+☀) and calls
 * ClickMonitorDDC directly to adjust external monitor brightness.
 *
 * Place this exe in the SAME directory as ClickMonitorDDC*.exe.
 * Runs silently with a system tray icon. Right-click to exit.
 *
 * Build:
 *   gcc -O2 -mwindows -o brightness_bridge.exe brightness_bridge.c
 *
 * (c) 2026 — Public Domain
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <stdio.h>

/* ── Configuration ──────────────────────────────────────────────────── */

/* Wildcard pattern to find ClickMonitorDDC exe (must be in the same folder) */
#define CLICKMONITOR_PATTERN  L"ClickMonitorDDC*.exe"

/* Brightness step per key press (percentage points) */
#define BRIGHTNESS_STEP   10

/* ── Constants ──────────────────────────────────────────────────────── */

#define WM_TRAYICON      (WM_USER + 1)
#define IDM_EXIT         1001
#define TRAY_ICON_ID     1

/* HID Consumer Control usage page */
#define HID_USAGE_PAGE_CONSUMER  0x000C
#define HID_USAGE_CONSUMER_CTRL  0x0001

/* Brightness HID usage IDs */
#define HID_BRIGHTNESS_UP    0x006F
#define HID_BRIGHTNESS_DOWN  0x0070

/* ── Globals ────────────────────────────────────────────────────────── */

static NOTIFYICONDATAW  g_nid;
static HWND             g_hwnd;
static HINSTANCE        g_hInst;
static WCHAR            g_clickmonitor_path[MAX_PATH];

/* ── Find ClickMonitorDDC in the same directory as this exe ─────────── */

static BOOL find_clickmonitor(void)
{
    WCHAR dir[MAX_PATH];
    GetModuleFileNameW(NULL, dir, MAX_PATH);

    /* Strip filename to get directory */
    WCHAR *lastSlash = wcsrchr(dir, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';

    /* Search for ClickMonitorDDC*.exe in the same directory */
    WCHAR searchPath[MAX_PATH];
    _snwprintf(searchPath, MAX_PATH, L"%s%s", dir, CLICKMONITOR_PATTERN);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return FALSE;

    /* Use the first match */
    _snwprintf(g_clickmonitor_path, MAX_PATH, L"%s%s", dir, fd.cFileName);
    FindClose(hFind);
    return TRUE;
}

/* ── Call ClickMonitorDDC to adjust brightness ──────────────────────── */

static void adjust_brightness(const WCHAR *direction)
{
    WCHAR cmdline[512];
    _snwprintf(cmdline, 512, L"\"%s\" %s", g_clickmonitor_path, direction);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    /* CREATE_NO_WINDOW prevents any console flash */
    if (CreateProcessW(
            NULL, cmdline, NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

/* ── Parse HID report for brightness usage codes ───────────────────── */

static void handle_hid_input(HRAWINPUT hRawInput)
{
    UINT size = 0;
    GetRawInputData(hRawInput, RID_INPUT, NULL, &size,
                    sizeof(RAWINPUTHEADER));
    if (size == 0) return;

    BYTE *buf = (BYTE *)_alloca(size);
    if (GetRawInputData(hRawInput, RID_INPUT, buf, &size,
                        sizeof(RAWINPUTHEADER)) == (UINT)-1)
        return;

    RAWINPUT *raw = (RAWINPUT *)buf;
    if (raw->header.dwType != RIM_TYPEHID) return;

    DWORD count      = raw->data.hid.dwCount;
    DWORD reportSize = raw->data.hid.dwSizeHid;
    BYTE *reports    = raw->data.hid.bRawData;

    for (DWORD i = 0; i < count; i++) {
        BYTE *report = reports + i * reportSize;
        WORD usage = 0;

        /*
         * Extract usage value from HID report.
         * Your keyboard sends 3-byte reports: [reportID] [usageLo] [usageHi]
         * e.g. [08 6F 00] = brightness up, [08 70 00] = brightness down
         */
        if (reportSize >= 3)
            usage = report[1] | ((WORD)report[2] << 8);
        else if (reportSize >= 2)
            usage = report[0] | ((WORD)report[1] << 8);

        if (usage == HID_BRIGHTNESS_UP) {
            WCHAR cmd[32];
            _snwprintf(cmd, 32, L"b +%d", BRIGHTNESS_STEP);
            adjust_brightness(cmd);
        }
        else if (usage == HID_BRIGHTNESS_DOWN) {
            WCHAR cmd[32];
            _snwprintf(cmd, 32, L"b -%d", BRIGHTNESS_STEP);
            adjust_brightness(cmd);
        }
    }
}

/* ── System tray icon ──────────────────────────────────────────────── */

static void tray_add(HWND hwnd)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = TRAY_ICON_ID;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpyW(g_nid.szTip, L"Brightness Bridge");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void tray_remove(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void tray_show_menu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit Brightness Bridge");

    /* Required for the menu to dismiss properly */
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

/* ── Window procedure ──────────────────────────────────────────────── */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_INPUT:
        handle_hid_input((HRAWINPUT)lParam);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
            tray_show_menu(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        tray_remove();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ── Register for Raw Input (HID Consumer Control) ─────────────────── */

static BOOL register_raw_input(HWND hwnd)
{
    RAWINPUTDEVICE rid;
    rid.usUsagePage = HID_USAGE_PAGE_CONSUMER;
    rid.usUsage     = HID_USAGE_CONSUMER_CTRL;
    rid.dwFlags     = RIDEV_INPUTSINK; /* receive even when not foreground */
    rid.hwndTarget  = hwnd;

    return RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

/* ── Entry point (no console window — WinMain) ─────────────────────── */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_hInst = hInstance;

    /* Prevent multiple instances */
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BrightnessBridgeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    /* Find ClickMonitorDDC in the same directory */
    if (!find_clickmonitor()) {
        MessageBoxW(NULL,
            L"ClickMonitorDDC not found!\n\n"
            L"Place brightness_bridge.exe in the same folder\n"
            L"as ClickMonitorDDC*.exe.",
            L"Brightness Bridge", MB_ICONERROR | MB_OK);
        CloseHandle(hMutex);
        return 1;
    }

    /* Register window class */
    WNDCLASSEXW wc    = {0};
    wc.cbSize         = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc    = WndProc;
    wc.hInstance       = hInstance;
    wc.lpszClassName  = L"BrightnessBridgeClass";
    RegisterClassExW(&wc);

    /* Create message-only window (invisible) */
    g_hwnd = CreateWindowExW(
        0, L"BrightnessBridgeClass", L"BrightnessBridge",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL
    );

    if (!g_hwnd) { CloseHandle(hMutex); return 1; }

    /* Register for HID Raw Input */
    if (!register_raw_input(g_hwnd)) { CloseHandle(hMutex); return 1; }

    /* Add system tray icon */
    tray_add(g_hwnd);

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
