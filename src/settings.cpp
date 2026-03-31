// settings.cpp — Config persistence for AnyClaw
// JSON serialization matching settings.h Config struct

#include "settings.h"
#include "lang.h"

#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

namespace anyclaw {

// ── Path helpers ─────────────────────────────────────────────────────

std::string Config::config_dir() const {
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
        return std::string(appdata) + "\\AnyClaw";
    }
    const char* env = std::getenv("APPDATA");
    if (env) return std::string(env) + "\\AnyClaw";
    return ".\\AnyClaw";
}

std::string Config::config_path() const {
    return config_dir() + "\\config.json";
}

std::string Config::gateway_url() const {
    return "http://127.0.0.1:" + std::to_string(openclaw_gateway_port) + "/health";
}

// ── JSON helpers ─────────────────────────────────────────────────────

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string extract_string(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return "";
    std::string val = trim(line.substr(colon + 1));
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        val = val.substr(1, val.size() - 2);
    }
    // Unescape
    std::string result;
    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '\\' && i + 1 < val.size()) {
            switch (val[i + 1]) {
                case '"':  result += '"';  ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                default:   result += val[i]; break;
            }
        } else {
            result += val[i];
        }
    }
    return result;
}

int extract_int(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return 0;
    std::string val = trim(line.substr(colon + 1));
    try { return std::stoi(val); } catch (...) { return 0; }
}

double extract_double(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return 0.0;
    std::string val = trim(line.substr(colon + 1));
    try { return std::stod(val); } catch (...) { return 0.0; }
}

bool extract_bool(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;
    return trim(line.substr(colon + 1)) == "true";
}

// Extract model array from "available_models": [{...}, {...}]
// Simplified: just extract id, name, context_length for each model
std::vector<OpenRouterModel> extract_models(const std::string& all_content) {
    std::vector<OpenRouterModel> models;

    size_t arr_start = all_content.find("\"available_models\"");
    if (arr_start == std::string::npos) return models;
    arr_start = all_content.find('[', arr_start);
    if (arr_start == std::string::npos) return models;

    // Find matching ]
    int depth = 0;
    size_t arr_end = arr_start;
    for (size_t i = arr_start; i < all_content.size(); ++i) {
        if (all_content[i] == '[') depth++;
        if (all_content[i] == ']') { depth--; if (depth == 0) { arr_end = i; break; } }
    }

    std::string arr_content = all_content.substr(arr_start, arr_end - arr_start + 1);

    // Parse each object {...}
    size_t pos = 0;
    while (pos < arr_content.size()) {
        size_t obj_start = arr_content.find('{', pos);
        if (obj_start == std::string::npos) break;
        int obj_depth = 0;
        size_t obj_end = obj_start;
        for (size_t i = obj_start; i < arr_content.size(); ++i) {
            if (arr_content[i] == '{') obj_depth++;
            if (arr_content[i] == '}') { obj_depth--; if (obj_depth == 0) { obj_end = i; break; } }
        }
        std::string obj = arr_content.substr(obj_start, obj_end - obj_start + 1);

        OpenRouterModel m;
        // Extract fields from object
        std::istringstream iss(obj);
        std::string line;
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.find("\"id\"") != std::string::npos)
                m.id = extract_string(line);
            else if (line.find("\"name\"") != std::string::npos)
                m.name = extract_string(line);
            else if (line.find("\"context_length\"") != std::string::npos)
                m.context_length = extract_int(line);
            // Handle nested pricing: "pricing": {"prompt": "0.001", "completion": "0.002"}
            else if (line.find("\"pricing\"") != std::string::npos) {
                // Find the pricing object {...}
                size_t p_start = line.find('{');
                size_t p_end = line.find('}');
                if (p_start != std::string::npos && p_end != std::string::npos && p_end > p_start) {
                    std::string pricing_obj = line.substr(p_start, p_end - p_start + 1);
                    size_t p_colon = pricing_obj.find(':');
                    if (p_colon != std::string::npos) {
                        std::string inner = pricing_obj.substr(p_colon + 1);
                        // Extract prompt
                        size_t prompt_pos = inner.find("\"prompt\"");
                        if (prompt_pos != std::string::npos) {
                            m.pricing_prompt = extract_double(inner.substr(prompt_pos));
                        }
                        // Extract completion
                        size_t comp_pos = inner.find("\"completion\"");
                        if (comp_pos != std::string::npos) {
                            m.pricing_completion = extract_double(inner.substr(comp_pos));
                        }
                    }
                }
            }
        }
        if (!m.id.empty()) models.push_back(m);
        pos = obj_end + 1;
    }

    return models;
}

std::string json_escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
        }
    }
    return result;
}

std::string serialize_models(const std::vector<OpenRouterModel>& models) {
    std::string result = "[\n";
    for (size_t i = 0; i < models.size(); ++i) {
        const auto& m = models[i];
        result += "      { \"id\": \"" + json_escape(m.id) + "\"";
        if (!m.name.empty()) result += ", \"name\": \"" + json_escape(m.name) + "\"";
        if (m.context_length > 0) result += ", \"context_length\": " + std::to_string(m.context_length);
        if (m.pricing_prompt > 0.0) result += ", \"pricing_prompt\": " + std::to_string(m.pricing_prompt);
        if (m.pricing_completion > 0.0) result += ", \"pricing_completion\": " + std::to_string(m.pricing_completion);
        result += " }";
        if (i + 1 < models.size()) result += ",";
        result += "\n";
    }
    result += "    ]";
    return result;
}

} // anonymous namespace

// ── Load ─────────────────────────────────────────────────────────────

Config Config::load() {
    Config cfg;

    std::string path = cfg.config_path();
    std::ifstream file(path);
    if (!file.is_open()) {
        return cfg;  // First run — defaults
    }

    // Read entire file for model array parsing
    std::ostringstream content_stream;
    content_stream << file.rdbuf();
    std::string content = content_stream.str();
    file.close();

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        line = trim(line);
        if (line.empty() || line == "{" || line == "}" || line[0] == '#') continue;
        if (line.back() == ',') line.pop_back();
        line = trim(line);

        if (line.find("\"api_key\"") != std::string::npos) {
            cfg.api_key = extract_string(line);
        } else if (line.find("\"active_model\"") != std::string::npos) {
            cfg.active_model = extract_string(line);
        } else if (line.find("\"auto_start\"") != std::string::npos) {
            cfg.auto_start = extract_bool(line);
        } else if (line.find("\"health_interval_sec\"") != std::string::npos) {
            cfg.health_interval_sec = extract_int(line);
        } else if (line.find("\"display_language\"") != std::string::npos) {
            std::string lang = extract_string(line);
            cfg.display_language = (lang == "en") ? Lang::English : Lang::Chinese;
        } else if (line.find("\"first_run\"") != std::string::npos) {
            cfg.first_run = extract_bool(line);
        } else if (line.find("\"language_selected\"") != std::string::npos) {
            cfg.language_selected = extract_bool(line);
        } else if (line.find("\"openclaw_gateway_port\"") != std::string::npos) {
            cfg.openclaw_gateway_port = extract_int(line);
        }
    }

    // Parse models array
    cfg.available_models = extract_models(content);

    return cfg;
}

// ── Save ─────────────────────────────────────────────────────────────

void Config::save() const {
    std::string dir = config_dir();
    CreateDirectoryA(dir.c_str(), nullptr);

    std::string path = config_path();
    std::string tmp_path = path + ".tmp";

    std::string lang_str = (display_language == Lang::English) ? "en" : "zh";

    std::ostringstream json;
    json << "{\n";
    json << "  \"api_key\": \""              << json_escape(api_key)              << "\",\n";
    json << "  \"active_model\": \""        << json_escape(active_model)        << "\",\n";
    json << "  \"auto_start\": "            << (auto_start ? "true" : "false")  << ",\n";
    json << "  \"health_interval_sec\": "   << health_interval_sec              << ",\n";
    json << "  \"display_language\": \""    << lang_str                         << "\",\n";
    json << "  \"first_run\": "             << (first_run ? "true" : "false")   << ",\n";
    json << "  \"language_selected\": "     << (language_selected ? "true" : "false") << ",\n";
    json << "  \"openclaw_gateway_port\": " << openclaw_gateway_port            << ",\n";
    json << "  \"available_models\": "      << serialize_models(available_models) << "\n";
    json << "}\n";

    std::ofstream tmp(tmp_path, std::ios::trunc);
    if (!tmp.is_open()) return;
    tmp << json.str();
    tmp.close();

    MoveFileExA(tmp_path.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

} // namespace anyclaw
