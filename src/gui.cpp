// gui.cpp — ImGui settings window (with i18n support)

#include "gui.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace anyclaw {

// ── ImGui color constants (from PRD) ────────────────────────────────────
static const ImVec4 COLOR_PRIMARY  (0.200f, 0.600f, 0.949f, 1.0f);
static const ImVec4 COLOR_SUCCESS  (0.298f, 0.851f, 0.392f, 1.0f);
static const ImVec4 COLOR_WARNING  (1.000f, 0.800f, 0.000f, 1.0f);
static const ImVec4 COLOR_ERROR    (1.000f, 0.231f, 0.188f, 1.0f);
static const ImVec4 COLOR_TEXT     (0.902f, 0.902f, 0.918f, 1.0f);
static const ImVec4 COLOR_TEXT_DIM (0.502f, 0.502f, 0.533f, 1.0f);
static const ImVec4 COLOR_BG_CARD  (0.133f, 0.133f, 0.157f, 1.0f);

// Helper: get localized string
#define T(key) S(key, lang_)

// ── SettingsWindow ──────────────────────────────────────────────────────

void SettingsWindow::set_config(const Config& cfg) {
    config_ = cfg;
    edited_ = cfg;
    lang_ = cfg.display_language;
    strncpy_s(api_key_input_, cfg.api_key.c_str(), sizeof(api_key_input_) - 1);
}

void SettingsWindow::set_status(OpenClawStatus status, const std::string& message) {
    status_ = status;
    status_message_ = message;
}

void SettingsWindow::set_callbacks(const GuiCallbacks& cb) {
    callbacks_ = cb;
}

void SettingsWindow::show_first_run() {
    show_wizard_ = true;
    if (!config_.language_selected) {
        wizard_step_ = 0;  // Start at language selection
    } else {
        wizard_step_ = 1;  // Skip language, go to OpenClaw detection
    }
}

// ── Status helpers ──────────────────────────────────────────────────────
static ImVec4 status_color(OpenClawStatus s) {
    switch (s) {
        case OpenClawStatus::Running:      return COLOR_SUCCESS;
        case OpenClawStatus::Detected:     return COLOR_WARNING;
        case OpenClawStatus::Error:        return COLOR_ERROR;
        case OpenClawStatus::NotInstalled: return COLOR_ERROR;
        default:                           return COLOR_TEXT_DIM;
    }
}

static const char* status_icon(OpenClawStatus s) {
    switch (s) {
        case OpenClawStatus::Running:      return "●";
        case OpenClawStatus::Detected:     return "●";
        case OpenClawStatus::Error:        return "●";
        case OpenClawStatus::NotInstalled: return "●";
        default:                           return "○";
    }
}

static const char* status_str(OpenClawStatus s, Lang lang) {
    switch (s) {
        case OpenClawStatus::Running:      return S(Str::StatusRunning, lang);
        case OpenClawStatus::Detected:     return S(Str::StatusDetected, lang);
        case OpenClawStatus::Error:        return S(Str::StatusError, lang);
        case OpenClawStatus::NotInstalled: return S(Str::StatusNotInstalled, lang);
        default:                           return S(Str::StatusUnknown, lang);
    }
}

// ── Main render ─────────────────────────────────────────────────────────
void SettingsWindow::render(bool* p_open) {
    // Fill the entire GLFW window — no nested window chrome
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));

    if (!ImGui::Begin("##main", p_open, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    // Title bar inside the fullscreen panel
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.200f, 0.600f, 0.949f, 1.0f));
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text(T(Str::AppTitle));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    if (show_wizard_) {
        render_first_run_wizard();
    } else {
        render_tab_bar();
    }

    render_bottom_bar();

    ImGui::End();
    ImGui::PopStyleVar(3);
}

// ── Tab bar ─────────────────────────────────────────────────────────────
void SettingsWindow::render_tab_bar() {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem(T(Str::TabGeneral))) {
            active_tab_ = GuiTab::General;
            render_general_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T(Str::TabAccount))) {
            active_tab_ = GuiTab::Account;
            render_account_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T(Str::TabModels))) {
            active_tab_ = GuiTab::Models;
            render_models_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T(Str::TabAbout))) {
            active_tab_ = GuiTab::About;
            render_about_tab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// ── General tab ─────────────────────────────────────────────────────────
void SettingsWindow::render_general_tab() {
    ImGui::Spacing();

    // Status card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_BG_CARD);
    if (ImGui::BeginChild("##status_card", ImVec2(0, 100), ImGuiChildFlags_Borders)) {
        ImGui::Spacing();
        ImGui::Indent(8);

        ImGui::TextColored(status_color(status_), "%s", status_icon(status_));
        ImGui::SameLine();
        ImGui::TextColored(COLOR_TEXT, "%s", status_str(status_, lang_));

        if (!config_.openclaw_version.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), T(Str::LabelVersion), config_.openclaw_version.c_str());
            ImGui::TextColored(COLOR_TEXT_DIM, "%s", buf);
        }

        if (!config_.openclaw_config_dir.empty()) {
            char buf[512];
            snprintf(buf, sizeof(buf), T(Str::LabelPath), config_.openclaw_config_dir.c_str());
            ImGui::TextColored(COLOR_TEXT_DIM, "%s", buf);
        }

        ImGui::TextColored(COLOR_TEXT_DIM, T(Str::LabelPort), config_.openclaw_gateway_port);

        ImGui::Unindent(8);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Action buttons
    if (ImGui::Button(T(Str::LabelRestart))) {
        if (callbacks_.on_restart_gateway) callbacks_.on_restart_gateway();
    }
    ImGui::SameLine();
    if (ImGui::Button(T(Str::LabelViewLogs))) {
        if (callbacks_.on_open_logs) callbacks_.on_open_logs();
    }
    ImGui::SameLine();
    if (ImGui::Button(T(Str::LabelOpenDir))) {
        if (callbacks_.on_open_config_dir) callbacks_.on_open_config_dir();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Auto-start
    ImGui::Checkbox(T(Str::LabelAutoStart), &edited_.auto_start);

    ImGui::Spacing();

    // Health check interval
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt(T(Str::LabelHealthInterval), &edited_.health_interval_sec);
    if (edited_.health_interval_sec < 5) edited_.health_interval_sec = 5;
    if (edited_.health_interval_sec > 300) edited_.health_interval_sec = 300;

    ImGui::Spacing();

    // Language selector
    ImGui::TextColored(COLOR_TEXT, T(Str::LabelLanguage));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##lang", lang_name(lang_))) {
        if (ImGui::Selectable(lang_name(Lang::Chinese), lang_ == Lang::Chinese)) {
            lang_ = Lang::Chinese;
            edited_.display_language = Lang::Chinese;
        }
        if (ImGui::Selectable(lang_name(Lang::English), lang_ == Lang::English)) {
            lang_ = Lang::English;
            edited_.display_language = Lang::English;
        }
        ImGui::EndCombo();
    }

    // OpenClaw not detected warning
    if (!config_.openclaw_detected) {
        ImGui::Spacing();
        ImGui::TextColored(COLOR_WARNING, T(Str::LabelNotDetected));
        ImGui::SameLine();
        if (ImGui::Button(T(Str::LabelInstallOc))) {
            show_wizard_ = true;
            wizard_step_ = 1;
        }
    }

    // Status message
    if (!status_message_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(status_color(status_), "%s", status_message_.c_str());
    }
}

// ── Account tab ─────────────────────────────────────────────────────────
void SettingsWindow::render_account_tab() {
    ImGui::Spacing();
    ImGui::TextColored(COLOR_TEXT, T(Str::AccountTitle));
    ImGui::Spacing();

    if (!config_.api_key.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_BG_CARD);
        if (ImGui::BeginChild("##account_card", ImVec2(0, 80), ImGuiChildFlags_Borders)) {
            ImGui::Spacing();
            ImGui::Indent(8);
            ImGui::TextColored(COLOR_SUCCESS, T(Str::AccountConnected));

            std::string masked = config_.api_key.substr(0, 8) + "...";
            if (config_.api_key.size() > 12) {
                masked += config_.api_key.substr(config_.api_key.size() - 4);
            }
            char buf[256];
            snprintf(buf, sizeof(buf), T(Str::AccountApiKey), masked.c_str());
            ImGui::TextColored(COLOR_TEXT, "%s", buf);

            ImGui::Unindent(8);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        if (ImGui::Button(T(Str::AccountRefreshModels))) {
            if (callbacks_.on_fetch_models) callbacks_.on_fetch_models();
        }
        ImGui::SameLine();
        if (ImGui::Button(T(Str::AccountLogout))) {
            edited_.api_key.clear();
            api_key_input_[0] = '\0';
        }
    } else {
        ImGui::TextColored(COLOR_TEXT_DIM, T(Str::AccountInputHint));
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##apikey", T(Str::WizardInputApiKey),
                                  api_key_input_, sizeof(api_key_input_),
                                  ImGuiInputTextFlags_Password);

        ImGui::Spacing();
        if (ImGui::Button(T(Str::AccountConnect))) {
            edited_.api_key = api_key_input_;
            config_ = edited_;
            config_.save();
            if (callbacks_.on_save) callbacks_.on_save(config_);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(COLOR_TEXT_DIM, T(Str::AccountNotRegistered));
        ImGui::SameLine();
        if (ImGui::Button(T(Str::AccountGoRegister))) {
            if (callbacks_.on_open_browser) {
                callbacks_.on_open_browser("https://openrouter.ai");
            }
        }
    }
}

// ── Models tab ──────────────────────────────────────────────────────────
void SettingsWindow::render_models_tab() {
    ImGui::Spacing();

    if (!config_.openclaw_detected) {
        ImGui::TextColored(COLOR_TEXT_DIM, T(Str::ErrOpenClawNotInstalled));
        return;
    }

    if (config_.available_models.empty()) {
        ImGui::TextColored(COLOR_TEXT_DIM, T(Str::ModelNoModels));
        if (config_.api_key.empty()) {
            ImGui::TextColored(COLOR_TEXT_DIM, T(Str::ModelNeedAccount));
        } else {
            ImGui::SameLine();
            if (ImGui::Button(T(Str::AccountRefreshModels))) {
                if (callbacks_.on_fetch_models) callbacks_.on_fetch_models();
            }
        }
        return;
    }

    // Current model
    if (!config_.active_model.empty()) {
        char buf[256];
        snprintf(buf, sizeof(buf), T(Str::ModelCurrent), config_.active_model.c_str());
        ImGui::TextColored(COLOR_PRIMARY, "%s", buf);
        ImGui::Spacing();
    }

    // Search
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", T(Str::ModelSearchHint),
                              model_search_, sizeof(model_search_));

    ImGui::Spacing();

    // Model list
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_BG_CARD);
    if (ImGui::BeginChild("##models", ImVec2(0, 200), ImGuiChildFlags_Borders)) {
        std::string search_lower = model_search_;
        std::transform(search_lower.begin(), search_lower.end(),
                       search_lower.begin(), ::tolower);

        for (const auto& model : config_.available_models) {
            if (!search_lower.empty()) {
                std::string name_lower = model.name;
                std::transform(name_lower.begin(), name_lower.end(),
                              name_lower.begin(), ::tolower);
                std::string id_lower = model.id;
                std::transform(id_lower.begin(), id_lower.end(),
                              id_lower.begin(), ::tolower);
                if (name_lower.find(search_lower) == std::string::npos &&
                    id_lower.find(search_lower) == std::string::npos) {
                    continue;
                }
            }

            bool is_selected = (model.id == edited_.active_model);
            ImGui::Indent(8);

            if (is_selected) {
                ImGui::TextColored(COLOR_SUCCESS, "●");
                ImGui::SameLine();
            }

            std::string label = model.name.empty() ? model.id : model.name;
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                edited_.active_model = model.id;
            }

            ImGui::SameLine();
            ImGui::TextColored(COLOR_TEXT_DIM, "%s", model.id.c_str());

            if (model.context_length > 0) {
                ImGui::Indent(24);
                char ctx[32];
                if (model.context_length >= 1000000) {
                    snprintf(ctx, sizeof(ctx), "%.1fM", model.context_length / 1000000.0);
                } else if (model.context_length >= 1000) {
                    snprintf(ctx, sizeof(ctx), "%.0fK", model.context_length / 1000.0);
                } else {
                    snprintf(ctx, sizeof(ctx), "%d", model.context_length);
                }
                char ctx_label[128];
                snprintf(ctx_label, sizeof(ctx_label), T(Str::ModelContext), ctx);
                ImGui::TextColored(COLOR_TEXT_DIM, "%s", ctx_label);
                ImGui::Unindent(24);
            }

            ImGui::Unindent(8);
            ImGui::Spacing();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    char count_buf[64];
    snprintf(count_buf, sizeof(count_buf), T(Str::ModelCount), config_.available_models.size());
    ImGui::TextColored(COLOR_TEXT_DIM, "%s", count_buf);
}

// ── About tab ───────────────────────────────────────────────────────────
void SettingsWindow::render_about_tab() {
    ImGui::Spacing();
    ImGui::TextColored(COLOR_TEXT, T(Str::AboutVersion));
    ImGui::TextColored(COLOR_TEXT_DIM, T(Str::AboutDescription));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!config_.openclaw_version.empty()) {
        char buf[256];
        snprintf(buf, sizeof(buf), T(Str::AboutOcVersion), config_.openclaw_version.c_str());
        ImGui::TextColored(COLOR_TEXT_DIM, "%s", buf);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    if (ImGui::Button(T(Str::AboutGithub))) {
        if (callbacks_.on_open_browser)
            callbacks_.on_open_browser("https://github.com/openclaw/anyclaw");
    }
    ImGui::SameLine();
    if (ImGui::Button(T(Str::AboutDocs))) {
        if (callbacks_.on_open_browser)
            callbacks_.on_open_browser("https://docs.openclaw.ai");
    }
    ImGui::SameLine();
    if (ImGui::Button(T(Str::AboutCheckUpdate))) {
        if (callbacks_.on_open_browser)
            callbacks_.on_open_browser("https://github.com/openclaw/anyclaw/releases/latest");
    }
}

// ── First-run wizard ────────────────────────────────────────────────────
void SettingsWindow::render_first_run_wizard() {
    ImGui::TextColored(COLOR_PRIMARY, T(Str::WizardTitle));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    switch (wizard_step_) {
        case 0: {
            // ── Step 0: Language selection ───────────────────────────────
            ImGui::TextColored(COLOR_TEXT, T(Str::WizardLanguageTitle));
            ImGui::Spacing();
            ImGui::TextColored(COLOR_TEXT_DIM, T(Str::WizardLanguagePrompt));
            ImGui::Spacing();

            bool lang_zh = (edited_.display_language == Lang::Chinese);
            bool lang_en = (edited_.display_language == Lang::English);

            // Show system detected language hint
            Lang sys_lang = detect_system_language();
            ImGui::TextColored(COLOR_TEXT_DIM,
                lang_ == Lang::Chinese
                    ? ("系统语言: " + std::string(lang_name(sys_lang))).c_str()
                    : ("System language: " + std::string(lang_name(sys_lang))).c_str());
            ImGui::Spacing();

            if (ImGui::RadioButton(lang_name(Lang::Chinese), lang_zh)) {
                edited_.display_language = Lang::Chinese;
                lang_ = Lang::Chinese;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(lang_name(Lang::English), lang_en)) {
                edited_.display_language = Lang::English;
                lang_ = Lang::English;
            }

            ImGui::Spacing();
            if (ImGui::Button(T(Str::WizardNext))) {
                edited_.language_selected = true;
                config_ = edited_;
                config_.save();
                wizard_step_ = 1;
            }
            break;
        }

        case 1: {
            // ── Step 1: OpenClaw detection ──────────────────────────────
            ImGui::TextColored(COLOR_TEXT, T(Str::WizardStep2));
            ImGui::Spacing();

            if (config_.openclaw_detected) {
                ImGui::TextColored(COLOR_SUCCESS, (lang_ == Lang::Chinese) ? "[正常]" : "[OK]");
                ImGui::SameLine();
                ImGui::TextColored(COLOR_TEXT, T(Str::WizardDetected));

                if (!config_.openclaw_version.empty()) {
                    ImGui::TextColored(COLOR_TEXT_DIM, "%s: %s",
                        (lang_ == Lang::Chinese) ? "版本" : "Version",
                        config_.openclaw_version.c_str());
                }
                if (!config_.openclaw_install_dir.empty()) {
                    std::string dp = config_.openclaw_install_dir;
                    std::replace(dp.begin(), dp.end(), '\\', '/');
                    ImGui::TextColored(COLOR_TEXT_DIM, "%s: %s",
                        (lang_ == Lang::Chinese) ? "路径" : "Path",
                        dp.c_str());
                }

                ImGui::Spacing();
                if (ImGui::Button(T(Str::WizardNext))) {
                    wizard_step_ = 2;
                }
            } else {
                ImGui::TextColored(COLOR_WARNING, (lang_ == Lang::Chinese) ? "[!]" : "[!]");
                ImGui::SameLine();
                ImGui::TextColored(COLOR_TEXT, T(Str::WizardNotDetected));
                ImGui::Spacing();

                // Check npm availability before showing npm option
                bool npm_available = OpenClawManager::is_npm_available();

                if (npm_available) {
                    if (ImGui::Button(T(Str::WizardInstallNpm))) {
                        // Run self-check before installing
                        status_message_ = S(Str::NotifySelfCheckRunning, lang_);
                        // Install in background thread (simplified — real impl would use thread)
                        bool ok = OpenClawManager::install("npm", "", nullptr);
                        if (ok) {
                            status_message_ = S(Str::NotifyInstallSuccess, lang_);
                            // Re-detect
                            config_ = Config::load();
                            wizard_step_ = 2;
                        } else {
                            // Self-check: try again
                            status_message_ = S(Str::ErrSelfHealing, lang_);
                            ok = OpenClawManager::install("npm", "", nullptr);
                            if (ok) {
                                status_message_ = S(Str::NotifyInstallSuccess, lang_);
                                config_ = Config::load();
                                wizard_step_ = 2;
                            } else {
                                status_message_ = S(Str::ErrInstallFailed, lang_);
                                // Don't proceed — user must fix manually
                            }
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COLOR_TEXT_DIM, lang_ == Lang::Chinese
                        ? "(需要已安装 Node.js)" : "(Node.js required)");
                } else {
                    ImGui::TextColored(COLOR_ERROR, S(Str::ErrNodeNotFound, lang_));
                    ImGui::Spacing();
                }

                if (ImGui::Button(T(Str::WizardInstallExe))) {
                    OpenClawManager::install("exe", "", nullptr);
                    wizard_step_ = 2;
                }

                ImGui::Spacing();
                if (ImGui::Button(T(Str::WizardSkip))) {
                    show_wizard_ = false;
                }
            }

            ImGui::Spacing();
            if (ImGui::Button(T(Str::WizardPrev))) {
                wizard_step_ = 0;
            }
            break;
        }

        case 2: {
            // ── Step 2: OpenRouter account ──────────────────────────────
            ImGui::TextColored(COLOR_TEXT, T(Str::WizardStep3));
            ImGui::Spacing();

            if (!config_.api_key.empty()) {
                ImGui::TextColored(COLOR_SUCCESS, (lang_ == Lang::Chinese) ? "[已连接]" : "[OK]");
                ImGui::SameLine();
                ImGui::TextColored(COLOR_TEXT, T(Str::AccountConnected));
                ImGui::Spacing();
                if (ImGui::Button(T(Str::WizardNext))) {
                    wizard_step_ = 3;
                }
            } else {
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##wizard_key", T(Str::WizardInputApiKey),
                                          api_key_input_, sizeof(api_key_input_),
                                          ImGuiInputTextFlags_Password);

                ImGui::Spacing();
                if (ImGui::Button(T(Str::WizardComplete))) {
                    edited_.api_key = api_key_input_;
                    config_ = edited_;
                    wizard_step_ = 3;
                }
                ImGui::SameLine();
                if (ImGui::Button(T(Str::WizardSkip))) {
                    wizard_step_ = 3;
                }

                ImGui::Spacing();
                ImGui::TextColored(COLOR_TEXT_DIM, T(Str::WizardNoAccount));
                ImGui::SameLine();
                if (ImGui::Button(T(Str::WizardGoRegister))) {
                    if (callbacks_.on_open_browser)
                        callbacks_.on_open_browser("https://openrouter.ai");
                }
            }

            ImGui::Spacing();
            if (ImGui::Button(T(Str::WizardPrev))) {
                wizard_step_ = 1;
            }
            break;
        }

        case 3: {
            // ── Step 3: Model selection ─────────────────────────────────
            ImGui::TextColored(COLOR_TEXT, T(Str::WizardSelectModel));
            ImGui::Spacing();

            if (!config_.api_key.empty() && config_.available_models.empty()) {
                if (callbacks_.on_fetch_models) callbacks_.on_fetch_models();
            }

            if (!config_.available_models.empty()) {
                for (const auto& model : config_.available_models) {
                    bool is_selected = (model.id == edited_.active_model);
                    if (ImGui::Selectable(
                        model.name.empty() ? model.id.c_str() : model.name.c_str(),
                        is_selected)) {
                        edited_.active_model = model.id;
                    }
                }
            } else {
                ImGui::TextColored(COLOR_TEXT_DIM, T(Str::ModelNoModels));
            }

            ImGui::Spacing();
            if (ImGui::Button(T(Str::WizardFinish))) {
                config_ = edited_;
                config_.first_run = false;
                config_.save();
                if (callbacks_.on_save) callbacks_.on_save(config_);
                show_wizard_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(T(Str::WizardPrev))) {
                wizard_step_ = 2;
            }
            break;
        }
    }
}

// ── Bottom bar ──────────────────────────────────────────────────────────
void SettingsWindow::render_bottom_bar() {
    float avail_y = ImGui::GetContentRegionAvail().y;
    if (avail_y > 40) {
        ImGui::Dummy(ImVec2(0, avail_y - 40));
    }

    ImGui::Separator();

    if (show_wizard_) {
        // During wizard: only show "Exit Wizard" button, right-aligned
        const char* exitLabel = (lang_ == Lang::Chinese) ? "退出安装向导" : "Exit Setup Wizard";
        float btnWidth = ImGui::CalcTextSize(exitLabel).x + 32.0f;
        float offset = ImGui::GetContentRegionAvail().x - btnWidth;
        if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        if (ImGui::Button(exitLabel, ImVec2(btnWidth, 0))) {
            if (callbacks_.on_exit_and_cleanup) callbacks_.on_exit_and_cleanup();
        }
        return;
    }

    // Normal mode: Save / Cancel
    float button_width = 80.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float offset = ImGui::GetContentRegionAvail().x - button_width * 2 - spacing;

    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

    if (ImGui::Button(T(Str::Save), ImVec2(button_width, 0))) {
        config_ = edited_;
        config_.first_run = false;
        if (callbacks_.on_save) callbacks_.on_save(config_);
    }

    ImGui::SameLine();
    if (ImGui::Button(T(Str::Cancel), ImVec2(button_width, 0))) {
        edited_ = config_;
        lang_ = config_.display_language;
        strncpy_s(api_key_input_, config_.api_key.c_str(),
                  sizeof(api_key_input_) - 1);
    }
}

} // namespace anyclaw
