// main.cpp — AnyClaw WinMain entry point
// OpenClaw Windows system tray manager

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "tray.h"
#include "gui.h"
#include "settings.h"
#include "health.h"
#include "openclaw_mgr.h"

#include <string>
#include <cstdio>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "shell32.lib")

namespace anyclaw {

// ── Globals for callback access ──────────────────────────────────────
static SystemTray*     g_tray = nullptr;
static HealthMonitor*  g_health_monitor = nullptr;
static SettingsWindow* g_settings_window = nullptr;
static GLFWwindow*     g_window = nullptr;
static Config          g_config;
static OpenClawInfo    g_openclaw_info;
static bool            g_show_window = false;

// ── Tray callbacks ───────────────────────────────────────────────────

static void on_tray_open_settings() {
    g_show_window = true;
    if (g_window) {
        glfwShowWindow(g_window);
        glfwFocusWindow(g_window);
    }
}

static void on_tray_restart_gateway() {
    OpenClawManager::stop_gateway(g_openclaw_info);
    Sleep(500);
    if (OpenClawManager::start_gateway(g_openclaw_info)) {
        if (g_tray) g_tray->show_balloon("AnyClaw", "Gateway restarted.");
    } else {
        if (g_tray) g_tray->show_balloon("AnyClaw", "Failed to restart gateway.");
    }
}

static void on_tray_view_logs() {
    std::string log_dir = g_config.config_dir() + "\\logs";
    CreateDirectoryA(log_dir.c_str(), nullptr);
    ShellExecuteA(nullptr, "open", log_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void on_tray_toggle_autostart(bool enabled) {
    g_config.auto_start = enabled;
    g_config.save();

    HKEY hKey;
    RegOpenKeyExA(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);

    if (enabled) {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string value = std::string("\"") + exe_path + "\" --minimized";
        RegSetValueExA(hKey, "AnyClaw", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>(value.size() + 1));
    } else {
        RegDeleteValueA(hKey, "AnyClaw");
    }
    RegCloseKey(hKey);
}

static void on_tray_about() {
    MessageBoxA(nullptr,
        "AnyClaw v1.0.0\n"
        "OpenClaw Windows Desktop Manager\n\n"
        "https://github.com/aetheros/anyclaw",
        "About AnyClaw",
        MB_ICONINFORMATION | MB_OK);
}

static void on_tray_exit() {
    if (g_window) {
        glfwSetWindowShouldClose(g_window, GLFW_TRUE);
    }
}

// ── Health status callback ───────────────────────────────────────────

static void on_health_status(OpenClawStatus status, const std::string& message) {
    if (!g_tray) return;

    IconColor icon;
    std::string tip;

    switch (status) {
        case OpenClawStatus::Running:
        case OpenClawStatus::Detected:
            icon = IconColor::Green;
            tip  = "AnyClaw - Gateway Running";
            break;
        case OpenClawStatus::Error:
            icon = IconColor::Red;
            tip  = "AnyClaw - Error: " + message;
            break;
        case OpenClawStatus::NotInstalled:
            icon = IconColor::Yellow;
            tip  = "AnyClaw - Not Installed";
            break;
        default:
            icon = IconColor::Gray;
            tip  = "AnyClaw - Unknown";
            break;
    }

    g_tray->update_icon(icon);
    g_tray->update_tooltip(tip);

    if (g_settings_window) {
        g_settings_window->set_status(status, message);
    }
}

// ── Win32 message pump ───────────────────────────────────────────────

static void pump_win32_messages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            if (g_window) glfwSetWindowShouldClose(g_window, GLFW_TRUE);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// ── WinMain ──────────────────────────────────────────────────────────

static int app_main(HINSTANCE hInstance, LPSTR lpCmdLine) {
    (void)hInstance;

    // Single-instance mutex
    HANDLE mutex = CreateMutexA(nullptr, TRUE, "AnyClaw_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    // Load config
    g_config = Config::load();

    // Detect OpenClaw
    g_openclaw_info = OpenClawManager::detect();
    g_config.openclaw_detected = (OpenClawManager::check_status(g_openclaw_info) != OpenClawStatus::NotInstalled);

    // ── GLFW + OpenGL 3.3 core ───────────────────────────────────────
    if (!glfwInit()) {
        MessageBoxA(nullptr, "Failed to initialize GLFW.", "AnyClaw Error", MB_ICONERROR);
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    g_window = glfwCreateWindow(800, 520, "AnyClaw", nullptr, nullptr);
    if (!g_window) {
        glfwTerminate();
        MessageBoxA(nullptr, "Failed to create GLFW window.", "AnyClaw Error", MB_ICONERROR);
        return 1;
    }

    glfwSetWindowSizeLimits(g_window, 640, 400, GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    // ── ImGui setup ──────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding  = 4.0f;
    style.GrabRounding   = 4.0f;

    // ── Load Chinese-capable font ─────────────────────────────────────
    {
        // Try Microsoft YaHei first, fall back to SimHei, then Segoe UI
        const char* fontPaths[] = {
            "C:\\Windows\\Fonts\\msyh.ttc",
            "C:\\Windows\\Fonts\\msyh.ttf",
            "C:\\Windows\\Fonts\\simhei.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
        };
        bool fontLoaded = false;
        for (const char* fp : fontPaths) {
            if (GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
                io.Fonts->AddFontFromFileTTF(fp, 22.0f, nullptr,
                    io.Fonts->GetGlyphRangesChineseFull());
                fontLoaded = true;
                break;
            }
        }
        if (!fontLoaded) {
            // Last resort: default font (will show ? for Chinese)
            io.Fonts->AddFontDefault();
        }
    }

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ── System Tray ──────────────────────────────────────────────────
    SystemTray tray;
    g_tray = &tray;

    TrayCallbacks tray_cb;
    tray_cb.on_open_settings    = on_tray_open_settings;
    tray_cb.on_restart          = on_tray_restart_gateway;
    tray_cb.on_view_logs        = on_tray_view_logs;
    tray_cb.on_toggle_autostart = on_tray_toggle_autostart;
    tray_cb.on_about            = on_tray_about;
    tray_cb.on_exit             = on_tray_exit;

    tray.set_callbacks(tray_cb);
    tray.set_autostart_enabled(g_config.auto_start);
    tray.create();

    // ── Health monitor ───────────────────────────────────────────────
    HealthMonitor health_monitor;
    g_health_monitor = &health_monitor;
    health_monitor.on_status_change(on_health_status);
    health_monitor.start(g_openclaw_info, g_config.health_interval_sec);

    // ── Settings window ──────────────────────────────────────────────
    SettingsWindow settings_window;
    g_settings_window = &settings_window;
    settings_window.set_config(g_config);

    GuiCallbacks gui_cb;
    gui_cb.on_save = [](const Config& cfg) {
        g_config = cfg;
        g_config.save();
        if (g_health_monitor) {
            g_health_monitor->set_interval(g_config.health_interval_sec);
        }
    };
    gui_cb.on_restart_gateway = on_tray_restart_gateway;
    gui_cb.on_open_logs = on_tray_view_logs;
    gui_cb.on_open_config_dir = []() {
        std::string dir = g_config.config_dir();
        CreateDirectoryA(dir.c_str(), nullptr);
        ShellExecuteA(nullptr, "open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    };
    gui_cb.on_open_browser = [](const std::string& url) {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    };
    gui_cb.on_fetch_models = []() {
        if (!g_config.api_key.empty()) {
            std::string json = OpenClawManager::fetch_models(g_config.api_key);
            (void)json;
        }
    };
    gui_cb.on_exit_and_cleanup = []() {
        // Delete config file and exit
        std::string path = g_config.config_path();
        DeleteFileA(path.c_str());
        g_show_window = false;
        glfwSetWindowShouldClose(g_window, GLFW_TRUE);
    };
    settings_window.set_callbacks(gui_cb);

    // ── First-run / startup behavior ─────────────────────────────────
    bool start_minimized = (strstr(lpCmdLine, "--minimized") != nullptr);

    if (g_config.first_run) {
        settings_window.show_first_run();
        g_show_window = true;
        glfwShowWindow(g_window);
    } else if (!start_minimized) {
        g_show_window = true;
        glfwShowWindow(g_window);
    }

    // ── Main loop ────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(g_window)) {
        pump_win32_messages();
        glfwPollEvents();

        if (g_show_window) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // ── Auto font scaling based on window size ─────────────────
            {
                int win_w, win_h;
                glfwGetWindowSize(g_window, &win_w, &win_h);
                // Scale relative to 800x520 baseline, clamped to 1.0–1.8
                float scale_x = win_w / 800.0f;
                float scale_y = win_h / 520.0f;
                float scale = (scale_x < scale_y) ? scale_x : scale_y;
                if (scale < 1.0f) scale = 1.0f;
                if (scale > 2.2f) scale = 2.2f;
                ImGui::GetIO().FontGlobalScale = scale;
            }

            settings_window.render(&g_show_window);

            ImGui::Render();

            int display_w, display_h;
            glfwGetFramebufferSize(g_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.10f, 0.10f, 0.10f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(g_window);
        } else {
            Sleep(50);
        }
    }

    // ── Cleanup ──────────────────────────────────────────────────────
    health_monitor.stop();
    tray.destroy();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g_window);
    glfwTerminate();

    ReleaseMutex(mutex);
    CloseHandle(mutex);

    return 0;
}

} // namespace anyclaw

// ── Entry point ──────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
    return anyclaw::app_main(hInstance, lpCmdLine);
}
