// tray.cpp — System tray icon implementation for AnyClaw

#include "tray.h"

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <functional>

namespace anyclaw {

// ── Menu item IDs ────────────────────────────────────────────────────
enum TrayMenuItem : UINT {
    ID_TRAY_OPEN_SETTINGS  = 1001,
    ID_TRAY_RESTART        = 1002,
    ID_TRAY_VIEW_LOGS      = 1003,
    ID_TRAY_AUTOSTART      = 1004,
    ID_TRAY_ABOUT          = 1005,
    ID_TRAY_EXIT           = 1006,
};

// ── Constructor / Destructor ─────────────────────────────────────────

SystemTray::SystemTray()
    : m_hInstance(GetModuleHandle(nullptr))
    , m_hwnd(nullptr)
    , m_icon(nullptr)
    , m_current_color(IconColor::Gray)
{
    std::memset(&m_nid, 0, sizeof(m_nid));
}

SystemTray::~SystemTray() {
    destroy();
}

// ── Callbacks ────────────────────────────────────────────────────────

void SystemTray::set_callbacks(const TrayCallbacks& cb) {
    m_callbacks = cb;
}

// ── Icon generation ──────────────────────────────────────────────────

HICON SystemTray::create_colored_icon(IconColor color) {
    const int size = 16;

    // Color mapping
    COLORREF dot_color;
    COLORREF bg_color = RGB(32, 32, 32);  // Dark background

    switch (color) {
        case IconColor::Green:  dot_color = RGB(76, 175, 80);   break;
        case IconColor::Yellow: dot_color = RGB(255, 193, 7);   break;
        case IconColor::Red:    dot_color = RGB(244, 67, 54);   break;
        case IconColor::Gray:
        default:                dot_color = RGB(158, 158, 158);  break;
    }

    // Create DIB section
    HDC hdc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(hdc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = size;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP color_bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);

    if (!color_bmp || !bits) {
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, hdc);
        return nullptr;
    }

    // Fill pixel buffer
    auto* pixels = static_cast<uint32_t*>(bits);

    // Draw a filled circle (claw dot)
    const int cx = size / 2;
    const int cy = size / 2;
    const int radius = 6;
    const int radius_sq = radius * radius;

    // Pre-compute COLORREF to uint32_t (BGRA layout)
    auto to_pixel = [](COLORREF c) -> uint32_t {
        return (GetRValue(c)) | (GetGValue(c) << 8) | (GetBValue(c) << 16) | (0xFF << 24);
    };

    uint32_t dot_px = to_pixel(dot_color);
    uint32_t bg_px  = to_pixel(bg_color);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= radius_sq) {
                pixels[y * size + x] = dot_px;
            } else {
                pixels[y * size + x] = bg_px;
            }
        }
    }

    // Create mask bitmap (all transparent where circle is)
    HBITMAP mask_bmp = CreateBitmap(size, size, 1, 1, nullptr);

    // Create icon
    ICONINFO icon_info = {};
    icon_info.fIcon   = TRUE;
    icon_info.hbmColor = color_bmp;
    icon_info.hbmMask  = mask_bmp;

    HICON hIcon = CreateIconIndirect(&icon_info);

    // Cleanup
    DeleteObject(color_bmp);
    DeleteObject(mask_bmp);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, hdc);

    return hIcon;
}

// ── Create / Destroy ─────────────────────────────────────────────────

void SystemTray::create() {
    // Register a hidden message-only window for tray notifications
    static const char* CLASS_NAME = "AnyClaw_TrayClass";
    static bool class_registered = false;

    if (!class_registered) {
        WNDCLASSEXA wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc   = tray_wndproc_stub;
        wc.hInstance      = m_hInstance;
        wc.lpszClassName  = CLASS_NAME;
        RegisterClassExA(&wc);
        class_registered = true;
    }

    m_hwnd = CreateWindowExA(
        0, CLASS_NAME, "AnyClaw Tray",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, m_hInstance, this);

    if (!m_hwnd) return;

    // Create initial icon
    m_icon = create_colored_icon(IconColor::Gray);

    // Set up NOTIFYICONDATA
    m_nid.cbSize           = sizeof(NOTIFYICONDATAA);
    m_nid.hWnd             = m_hwnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_USER + 1;
    m_nid.hIcon            = m_icon;
    lstrcpynA(m_nid.szTip, "AnyClaw", sizeof(m_nid.szTip));

    Shell_NotifyIconA(NIM_ADD, &m_nid);

    // Use NOTIFYICON_VERSION_4 for better behavior
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconA(NIM_SETVERSION, &m_nid);
}

void SystemTray::destroy() {
    if (m_nid.hWnd) {
        Shell_NotifyIconA(NIM_DELETE, &m_nid);
        m_nid.hWnd = nullptr;
    }

    if (m_icon) {
        DestroyIcon(m_icon);
        m_icon = nullptr;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ── Update icon / tooltip ────────────────────────────────────────────

void SystemTray::update_icon(IconColor color) {
    if (color == m_current_color && m_icon) return;

    HICON new_icon = create_colored_icon(color);
    if (!new_icon) return;

    if (m_icon) {
        DestroyIcon(m_icon);
    }

    m_icon = new_icon;
    m_current_color = color;

    m_nid.uFlags  = NIF_ICON;
    m_nid.hIcon   = m_icon;
    Shell_NotifyIconA(NIM_MODIFY, &m_nid);
}

void SystemTray::update_tooltip(const std::string& tip) {
    m_nid.uFlags = NIF_TIP;
    lstrcpynA(m_nid.szTip, tip.c_str(), sizeof(m_nid.szTip));
    Shell_NotifyIconA(NIM_MODIFY, &m_nid);
}

// ── Balloon notification ─────────────────────────────────────────────

void SystemTray::show_balloon(const std::string& title, const std::string& message, int timeout_ms) {
    m_nid.uFlags = NIF_INFO;
    lstrcpynA(m_nid.szInfo, message.c_str(), sizeof(m_nid.szInfo));
    lstrcpynA(m_nid.szInfoTitle, title.c_str(), sizeof(m_nid.szInfoTitle));
    m_nid.dwInfoFlags = NIIF_INFO;
    m_nid.uTimeout = static_cast<UINT>(timeout_ms);
    Shell_NotifyIconA(NIM_MODIFY, &m_nid);
}

// ── Context menu ─────────────────────────────────────────────────────

void SystemTray::show_context_menu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuA(hMenu, MF_STRING, ID_TRAY_OPEN_SETTINGS, S(Str::TrayOpenSettings, m_lang));
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, ID_TRAY_RESTART, S(Str::TrayRestart, m_lang));
    AppendMenuA(hMenu, MF_STRING, ID_TRAY_VIEW_LOGS, S(Str::TrayViewLogs, m_lang));
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);

    // Autostart checkbox
    UINT autostart_flags = MF_STRING | (m_autostart_enabled ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuA(hMenu, autostart_flags, ID_TRAY_AUTOSTART, S(Str::TrayAutoStart, m_lang));

    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, ID_TRAY_ABOUT, S(Str::TrayAbout, m_lang));
    AppendMenuA(hMenu, MF_STRING, ID_TRAY_EXIT, S(Str::TrayExit, m_lang));

    // Required for TrackPopupMenu to work correctly
    SetForegroundWindow(m_hwnd);

    UINT cmd = TrackPopupMenu(hMenu,
        TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
        pt.x, pt.y, 0, m_hwnd, nullptr);

    DestroyMenu(hMenu);

    // Process the selected command
    switch (cmd) {
        case ID_TRAY_OPEN_SETTINGS:
            if (m_callbacks.on_open_settings) m_callbacks.on_open_settings();
            break;
        case ID_TRAY_RESTART:
            if (m_callbacks.on_restart) m_callbacks.on_restart();
            break;
        case ID_TRAY_VIEW_LOGS:
            if (m_callbacks.on_view_logs) m_callbacks.on_view_logs();
            break;
        case ID_TRAY_AUTOSTART:
            m_autostart_enabled = !m_autostart_enabled;
            if (m_callbacks.on_toggle_autostart) {
                m_callbacks.on_toggle_autostart(m_autostart_enabled);
            }
            break;
        case ID_TRAY_ABOUT:
            if (m_callbacks.on_about) m_callbacks.on_about();
            break;
        case ID_TRAY_EXIT:
            if (m_callbacks.on_exit) m_callbacks.on_exit();
            break;
    }

    // Per MS docs, must send a message to avoid menu "sticking"
    PostMessage(m_hwnd, WM_NULL, 0, 0);
}

// ── Message handler ──────────────────────────────────────────────────

void SystemTray::handle_message(LPARAM lParam) {
    UINT event = LOWORD(lParam);

    switch (event) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            show_context_menu();
            break;

        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            if (m_callbacks.on_open_settings) {
                m_callbacks.on_open_settings();
            }
            break;
    }
}

// ── Static WNDPROC bridge ────────────────────────────────────────────

LRESULT CALLBACK SystemTray::tray_wndproc_stub(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }

    auto* self = reinterpret_cast<SystemTray*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    if (self) {
        if (msg == WM_USER + 1) {
            self->handle_message(lParam);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ── Set autostart state (for UI sync) ────────────────────────────────

void SystemTray::set_autostart_enabled(bool enabled) {
    m_autostart_enabled = enabled;
}

bool SystemTray::is_autostart_enabled() const {
    return m_autostart_enabled;
}

void SystemTray::set_language(Lang lang) {
    m_lang = lang;
}

} // namespace anyclaw
