#pragma once
#include <string>

namespace anyclaw {

enum class OpenClawStatus { Unknown, Running, Detected, Error, NotInstalled };

struct OpenClawInfo {
    std::string install_dir;
    std::string executable;
    std::string config_dir;
    std::string config_file;
    std::string version;
    int gateway_port = 3578;
};

class OpenClawManager {
public:
    // Detect OpenClaw installation (5 strategies)
    static OpenClawInfo detect();

    // Check gateway status (process + HTTP)
    static OpenClawStatus check_status(const OpenClawInfo& info);

    // Start/stop gateway
    static bool start_gateway(const OpenClawInfo& info);
    static bool stop_gateway(const OpenClawInfo& info);

    // Check if npm is available
    static bool is_npm_available();

    // Install OpenClaw ("npm" or "exe")
    static bool install(const std::string& method, const std::string& path, void* progress_cb);

    // Uninstall OpenClaw
    static bool uninstall(const std::string& method);

    // Fetch available models from OpenRouter
    static std::string fetch_models(const std::string& api_key);
};

} // namespace anyclaw
