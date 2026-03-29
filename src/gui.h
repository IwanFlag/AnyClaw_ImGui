#pragma once
// gui.h — ImGui settings window

#include "settings.h"
#include "openclaw_mgr.h"
#include "lang.h"
#include <string>
#include <functional>

namespace anyclaw {

enum class GuiTab {
    General,
    Account,
    Models,
    About
};

struct GuiCallbacks {
    std::function<void(const Config&)> on_save;
    std::function<void()> on_restart_gateway;
    std::function<void()> on_open_logs;
    std::function<void()> on_open_config_dir;
    std::function<void(const std::string& url)> on_open_browser;
    std::function<void()> on_fetch_models;
    std::function<void()> on_exit_and_cleanup;  // Exit app + remove config
};

class SettingsWindow {
public:
    SettingsWindow() = default;

    void set_config(const Config& cfg);
    void set_status(OpenClawStatus status, const std::string& message);
    void set_callbacks(const GuiCallbacks& cb);
    void show_first_run();
    void render(bool* p_open);

private:
    void render_tab_bar();
    void render_general_tab();
    void render_account_tab();
    void render_models_tab();
    void render_about_tab();
    void render_first_run_wizard();
    void render_bottom_bar();

    Config config_;
    Config edited_;           // working copy for editing
    OpenClawStatus status_ = OpenClawStatus::Unknown;
    std::string status_message_;

    GuiTab active_tab_ = GuiTab::General;
    GuiCallbacks callbacks_;

    // Language
    Lang lang_ = Lang::Chinese;

    // First-run wizard
    bool show_wizard_ = false;
    int wizard_step_ = 0;  // 0=language, 1=openclaw, 2=account, 3=model

    // Model search
    char model_search_[128] = {};

    // API key input (masked display)
    char api_key_input_[256] = {};

    // Status message (transient)
    char status_buf_[512] = {};
    float status_timer_ = 0.0f;
};

} // namespace anyclaw
