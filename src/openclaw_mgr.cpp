#include "openclaw_mgr.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <winhttp.h>
#endif

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include "http_client.h"

namespace fs = std::filesystem;

namespace anyclaw {

// ── Helpers ──────────────────────────────────────────────────────────────────

// Execute a command and capture stdout. Returns true if process exit code == 0.
static bool exec_cmd(const std::string& cmd, std::string& output, DWORD timeout_ms = 5000) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};

    // Wrap in cmd /C so shell redirection works
    std::string full = "cmd /C \"" + cmd + "\"";
    std::vector<char> buf(full.begin(), full.end());
    buf.push_back('\0');

    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    CloseHandle(hWrite);

    // Read output
    output.clear();
    char readBuf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hRead, readBuf, sizeof(readBuf), &bytesRead, nullptr) && bytesRead > 0) {
        output.append(readBuf, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, timeout_ms);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    // Trim whitespace
    while (!output.empty() && (output.back() == '\r' || output.back() == '\n' || output.back() == ' '))
        output.pop_back();

    return exitCode == 0;
#else
    (void)timeout_ms;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    output.clear();
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    int rc = pclose(pipe);
    // Trim
    while (!output.empty() && (output.back() == '\r' || output.back() == '\n' || output.back() == ' '))
        output.pop_back();
    return rc == 0;
#endif
}

// Check if a process name is running (Windows only via toolhelp snapshot)
static bool is_process_running(const std::string& process_name) {
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);

    if (Process32First(snap, &pe)) {
        do {
            // pe.szExeFile is char[260]
            if (_wcsicmp(pe.szExeFile, std::wstring(process_name.begin(), process_name.end()).c_str()) == 0) {
                CloseHandle(snap);
                return true;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return false;
#else
    (void)process_name;
    return false;
#endif
}

// Simple HTTP GET using WinHTTP, returns status code; body goes into response
static int http_get(const std::string& url, std::string& response, int timeout_sec = 3) {
#ifdef _WIN32
    // Convert URL to wide string
    wchar_t wurl[4096];
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wurl, 4096);

    // Parse URL (wide version)
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t wpath[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = wpath;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return -1;

    HINTERNET hSession = WinHttpOpen(L"AnyClaw/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return -1;

    WinHttpSetTimeouts(hSession, timeout_sec * 1000, timeout_sec * 1000,
                       timeout_sec * 1000, timeout_sec * 1000);

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    int status = -1;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD statusCode = 0;
            DWORD size = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size,
                WINHTTP_NO_HEADER_INDEX);
            status = static_cast<int>(statusCode);

            // Read body
            response.clear();
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                std::vector<char> buf(avail + 1);
                DWORD read = 0;
                WinHttpReadData(hRequest, buf.data(), avail, &read);
                buf[read] = '\0';
                response.append(buf.data(), read);
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return status;
#else
    (void)url; (void)response; (void)timeout_sec;
    return -1;
#endif
}

// Read openclaw.json config to extract gateway_port and version
static bool read_openclaw_config(const std::string& config_file, OpenClawInfo& info) {
    std::ifstream f(config_file);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Very simple JSON field extraction — look for "gateway_port": <number>
    auto extract_int = [&](const std::string& key) -> int {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return -1;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return -1;
        pos++;
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) pos++;
        int val = 0;
        while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
            val = val * 10 + (content[pos] - '0');
            pos++;
        }
        return val;
    };

    auto extract_string = [&](const std::string& key) -> std::string {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = content.find(':', pos);
        if (pos == std::string::npos) return "";
        pos = content.find('"', pos + 1);
        if (pos == std::string::npos) return "";
        auto end = content.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return content.substr(pos + 1, end - pos - 1);
    };

    int port = extract_int("gateway_port");
    if (port > 0) info.gateway_port = port;

    std::string ver = extract_string("version");
    if (!ver.empty()) info.version = ver;

    return true;
}

// ── detect() ─────────────────────────────────────────────────────────────────

OpenClawInfo OpenClawManager::detect() {
    OpenClawInfo info;
    std::string output;

    // Strategy 1: PATH lookup (where openclaw)
    if (exec_cmd("where openclaw", output) && !output.empty()) {
        // output may contain multiple lines; take first
        auto nl = output.find('\n');
        std::string exe_path = (nl != std::string::npos) ? output.substr(0, nl) : output;
        // Remove trailing \r if present
        while (!exe_path.empty() && (exe_path.back() == '\r' || exe_path.back() == '\n'))
            exe_path.pop_back();

        if (fs::exists(exe_path)) {
            info.executable = exe_path;
            info.install_dir = fs::path(exe_path).parent_path().string();
            // Try reading config
            info.config_dir = info.install_dir;
            info.config_file = info.install_dir + "\\openclaw.json";
            read_openclaw_config(info.config_file, info);
            // Get version
            if (info.version.empty() && exec_cmd("openclaw --version", output)) {
                info.version = output;
            }
            return info;
        }
    }

    // Strategy 2: npm global directory
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        std::string npm_global = std::string(appdata) + "\\npm\\node_modules\\openclaw";
        std::string cmd_path = npm_global + "\\openclaw.cmd";
        if (fs::exists(cmd_path)) {
            info.install_dir = npm_global;
            info.executable = cmd_path;
            info.config_dir = npm_global;
            info.config_file = npm_global + "\\openclaw.json";
            read_openclaw_config(info.config_file, info);
            if (info.version.empty()) {
                exec_cmd("npx openclaw --version", output);
                info.version = output;
            }
            return info;
        }
    }

    // Strategy 3: Program Files
    const char* program_files = std::getenv("ProgramFiles");
    if (!program_files) program_files = "C:\\Program Files";
    std::string pf_dir = std::string(program_files) + "\\OpenClaw";
    if (fs::exists(pf_dir + "\\openclaw.exe") || fs::exists(pf_dir + "\\openclaw.cmd")) {
        info.install_dir = pf_dir;
        info.executable = fs::exists(pf_dir + "\\openclaw.exe")
            ? pf_dir + "\\openclaw.exe" : pf_dir + "\\openclaw.cmd";
        info.config_dir = pf_dir;
        info.config_file = pf_dir + "\\openclaw.json";
        read_openclaw_config(info.config_file, info);
        return info;
    }

    // Strategy 4: Registry (HKCU\Software\OpenClaw\InstallDir)
#ifdef _WIN32
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\OpenClaw", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buf[MAX_PATH] = {};
            DWORD bufSize = sizeof(buf);
            if (RegQueryValueExA(hKey, "InstallDir", nullptr, nullptr,
                                 reinterpret_cast<LPBYTE>(buf), &bufSize) == ERROR_SUCCESS) {
                std::string reg_dir = buf;
                if (fs::exists(reg_dir)) {
                    info.install_dir = reg_dir;
                    if (fs::exists(reg_dir + "\\openclaw.exe"))
                        info.executable = reg_dir + "\\openclaw.exe";
                    else if (fs::exists(reg_dir + "\\openclaw.cmd"))
                        info.executable = reg_dir + "\\openclaw.cmd";
                    info.config_dir = reg_dir;
                    info.config_file = reg_dir + "\\openclaw.json";
                    read_openclaw_config(info.config_file, info);
                    RegCloseKey(hKey);
                    return info;
                }
            }
            RegCloseKey(hKey);
        }
    }
#endif

    // Strategy 5: Common paths scan
    std::vector<std::string> common_paths;
    if (appdata) {
        common_paths.push_back(std::string(appdata) + "\\openclaw");
        common_paths.push_back(std::string(appdata) + "\\npm\\openclaw");
    }
    const char* local_appdata = std::getenv("LOCALAPPDATA");
    if (local_appdata) {
        common_paths.push_back(std::string(local_appdata) + "\\openclaw");
        common_paths.push_back(std::string(local_appdata) + "\\Programs\\openclaw");
    }
    common_paths.push_back("C:\\OpenClaw");
    common_paths.push_back("C:\\tools\\openclaw");

    for (const auto& dir : common_paths) {
        if (fs::exists(dir + "\\openclaw.exe")) {
            info.install_dir = dir;
            info.executable = dir + "\\openclaw.exe";
            info.config_dir = dir;
            info.config_file = dir + "\\openclaw.json";
            read_openclaw_config(info.config_file, info);
            return info;
        }
        if (fs::exists(dir + "\\openclaw.cmd")) {
            info.install_dir = dir;
            info.executable = dir + "\\openclaw.cmd";
            info.config_dir = dir;
            info.config_file = dir + "\\openclaw.json";
            read_openclaw_config(info.config_file, info);
            return info;
        }
    }

    return info; // empty — not found
}

// ── check_status() ───────────────────────────────────────────────────────────

OpenClawStatus OpenClawManager::check_status(const OpenClawInfo& info) {
    bool process_found = is_process_running("node.exe") || is_process_running("openclaw.exe");

    // If OpenClaw is not installed at all, return NotInstalled
    if (info.executable.empty() && !process_found) {
        return OpenClawStatus::NotInstalled;
    }

    // HTTP health check
    std::string url = "http://127.0.0.1:" + std::to_string(info.gateway_port) + "/health";
    std::string response;
    int status = http_get(url, response, 3);

    if (process_found && status == 200) {
        return OpenClawStatus::Running;
    }
    if (process_found && status != 200) {
        return OpenClawStatus::Error;
    }
    if (!process_found && !info.executable.empty()) {
        return OpenClawStatus::Detected;
    }

    return OpenClawStatus::NotInstalled;
}

// ── start_gateway() ──────────────────────────────────────────────────────────

bool OpenClawManager::start_gateway(const OpenClawInfo& info) {
    std::string cmd;
    if (!info.executable.empty()) {
        // Use the detected executable directly
        cmd = "\"" + info.executable + "\" gateway start";
    } else {
        // Fallback: try openclaw from PATH
        cmd = "openclaw gateway start";
    }

    // Check if it's an npm-based install (.cmd extension)
    if (!info.executable.empty() &&
        info.executable.size() >= 4 &&
        info.executable.substr(info.executable.size() - 4) == ".cmd") {
        cmd = "\"" + info.executable + "\" gateway start";
    }

#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::string full = "cmd /C " + cmd;
    std::vector<char> buf(full.begin(), full.end());
    buf.push_back('\0');

    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | DETACHED_PROCESS,
                        nullptr, nullptr, &si, &pi)) {
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
#else
    return std::system((cmd + " &").c_str()) == 0;
#endif
}

// ── stop_gateway() ───────────────────────────────────────────────────────────

bool OpenClawManager::stop_gateway(const OpenClawInfo& info) {
    std::string cmd;
    if (!info.executable.empty()) {
        cmd = "\"" + info.executable + "\" gateway stop";
    } else {
        cmd = "openclaw gateway stop";
    }

    std::string output;
    return exec_cmd(cmd, output, 10000);
}

// ── is_npm_available() ──────────────────────────────────────────────────────

bool OpenClawManager::is_npm_available() {
    std::string output;
    return exec_cmd("npm --version", output, 5000);
}

// ── install() ────────────────────────────────────────────────────────────────

bool OpenClawManager::install(const std::string& method, const std::string& path, void* progress_cb) {
    (void)path; (void)progress_cb; // unused for now

    if (method == "npm") {
#ifdef _WIN32
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::string cmd = "cmd /C npm install -g openclaw";
        std::vector<char> buf(cmd.begin(), cmd.end());
        buf.push_back('\0');

        // Spawn npm install in a new console window (non-blocking)
        // This allows the GUI to remain responsive during installation
        if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE,
                            CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
            return false;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;  // Async — installation runs in separate console window
#else
        return std::system("npm install -g openclaw &") == 0;
#endif
    }

    if (method == "exe") {
#ifdef _WIN32
        // Open browser to GitHub releases
        ShellExecuteA(nullptr, "open",
            "https://github.com/IwanFlag/AnyClaw_ImGui/releases/latest",
            nullptr, nullptr, SW_SHOWNORMAL);
        return true;
#else
        return false;
#endif
    }

    return false;
}

// ── uninstall() ──────────────────────────────────────────────────────────────

bool OpenClawManager::uninstall(const std::string& method) {
    if (method == "npm") {
        std::string output;
        return exec_cmd("npm uninstall -g openclaw", output, 30000);
    }

    if (method == "exe") {
#ifdef _WIN32
        // Try to find uninstaller in common locations
        const char* program_files = std::getenv("ProgramFiles");
        if (!program_files) program_files = "C:\\Program Files";
        std::string uninstaller = std::string(program_files) + "\\OpenClaw\\Uninstall.exe";
        if (fs::exists(uninstaller)) {
            ShellExecuteA(nullptr, "open", uninstaller.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return true;
        }
        // Also check LOCALAPPDATA
        const char* local_appdata = std::getenv("LOCALAPPDATA");
        if (local_appdata) {
            uninstaller = std::string(local_appdata) + "\\openclaw\\Uninstall.exe";
            if (fs::exists(uninstaller)) {
                ShellExecuteA(nullptr, "open", uninstaller.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return true;
            }
        }
#endif
        return false;
    }

    return false;
}

// ── fetch_models() ───────────────────────────────────────────────────────────

std::string OpenClawManager::fetch_models(const std::string& api_key) {
    HttpClient client;
    client.set_timeout(15000, 15000);

    auto resp = client.get_auth("https://openrouter.ai/api/v1/models", api_key);
    if (resp.status_code == 200) {
        return resp.body;
    }
    return "";
}

} // namespace anyclaw
