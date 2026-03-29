#pragma once
// settings.h — AnyClaw configuration (persisted to %APPDATA%\AnyClaw\config.json)

#include "lang.h"
#include <string>
#include <vector>

namespace anyclaw {

struct OpenRouterModel {
    std::string id;
    std::string name;
    int context_length = 0;
    double pricing_prompt = 0.0;      // $ per 1M input tokens
    double pricing_completion = 0.0;  // $ per 1M output tokens
};

struct Config {
    // ── OpenClaw paths (detected at boot) ───────────────────────────────
    std::string openclaw_install_dir;
    std::string openclaw_executable;
    std::string openclaw_config_dir;
    std::string openclaw_config_file;
    std::string openclaw_version;
    int         openclaw_gateway_port = 3578;

    // ── User settings ───────────────────────────────────────────────────
    std::string api_key;
    std::string active_model;
    std::vector<OpenRouterModel> available_models;
    bool auto_start = true;              // default ON, can uncheck in settings
    int  health_interval_sec = 10;
    Lang display_language = Lang::Chinese; // default Chinese

    // ── State ───────────────────────────────────────────────────────────
    bool openclaw_detected = false;
    bool first_run = true;
    bool language_selected = false;      // has user chosen language?

    // ── Methods ─────────────────────────────────────────────────────────
    static Config load();
    void save() const;
    std::string config_dir() const;
    std::string config_path() const;
    std::string gateway_url() const;
};

} // namespace anyclaw
