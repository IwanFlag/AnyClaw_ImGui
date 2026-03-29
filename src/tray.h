#pragma once
// tray.h — System tray icon and context menu for AnyClaw

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

namespace anyclaw {

enum class IconColor {
    Green,   // running
    Yellow,  // checking
    Red,     // error
    Gray     // unconfigured
};

struct TrayCallbacks {
    std::function<void()> on_open_settings;
    std::function<void()> on_restart;
    std::function<void()> on_view_logs;
    std::function<void(bool)> on_toggle_autostart;
    std::function<void()> on_about;
    std::function<void()> on_exit;
};

class SystemTray {
public:
    SystemTray();
    ~SystemTray();

    void set_callbacks(const TrayCallbacks& cb);

    void create();
    void destroy();

    void update_icon(IconColor color);
    void update_tooltip(const std::string& tip);
    void show_balloon(const std::string& title, const std::string& message, int timeout_ms = 3000);

    void set_autostart_enabled(bool enabled);
    bool is_autostart_enabled() const;

private:
    HINSTANCE          m_hInstance = nullptr;
    HWND               m_hwnd = nullptr;
    HICON              m_icon = nullptr;
    NOTIFYICONDATAA    m_nid = {};
    IconColor          m_current_color = IconColor::Gray;
    bool               m_autostart_enabled = false;
    TrayCallbacks      m_callbacks;

    HICON create_colored_icon(IconColor color);
    void show_context_menu();
    void handle_message(LPARAM lParam);

    static LRESULT CALLBACK tray_wndproc_stub(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace anyclaw
