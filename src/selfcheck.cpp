#include "selfcheck.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace anyclaw {

// ── Helper: execute command and capture output ───────────────────────────────
static bool exec_cmd_simple(const std::string& cmd, std::string& output, int timeout_ms = 5000) {
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
    output.clear();
    char readBuf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hRead, readBuf, sizeof(readBuf), &bytesRead, nullptr) && bytesRead > 0) {
        output.append(readBuf, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout_ms));
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

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
    while (!output.empty() && (output.back() == '\r' || output.back() == '\n' || output.back() == ' '))
        output.pop_back();
    return rc == 0;
#endif
}

// ── Helper: simple HTTP GET via WinHTTP ───────────────────────────────────────
static int http_get_status(const std::string& url, int timeout_sec = 5) {
#ifdef _WIN32
    // Convert URL to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wurl(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wurl.data(), wlen);

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = _countof(path);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return -1;

    HINTERNET hSession = WinHttpOpen(L"AnyClaw-SelfCheck/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return -1;

    WinHttpSetTimeouts(hSession, timeout_sec * 1000, timeout_sec * 1000,
                       timeout_sec * 1000, timeout_sec * 1000);

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
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
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return status;
#else
    (void)url; (void)timeout_sec;
    return -1;
#endif
}

// ── check_nodejs() ───────────────────────────────────────────────────────────
CheckResult SelfCheck::check_nodejs() {
    std::string output;
    if (exec_cmd_simple("node --version", output) && !output.empty()) {
        return { "Node.js", CheckLevel::Pass, "Node.js " + output + " detected" };
    }
    return { "Node.js", CheckLevel::Fail,
        "Node.js not found. Please install Node.js from https://nodejs.org" };
}

// ── check_npm() ──────────────────────────────────────────────────────────────
CheckResult SelfCheck::check_npm() {
    std::string output;
    if (exec_cmd_simple("npm --version", output) && !output.empty()) {
        return { "npm", CheckLevel::Pass, "npm " + output + " detected" };
    }
    // npm not working — try repair
    CheckResult result{ "npm", CheckLevel::Fail, "npm not found or not working" };
    if (try_repair(result)) {
        return result;
    }
    return result;
}

// ── check_network() ──────────────────────────────────────────────────────────
CheckResult SelfCheck::check_network() {
    int status = http_get_status("https://openrouter.ai", 5);
    if (status >= 200 && status < 400) {
        return { "Network", CheckLevel::Pass, "Network connectivity OK" };
    }
    if (status == -1) {
        return { "Network", CheckLevel::Fail,
            "Cannot reach openrouter.ai (timeout or DNS failure)" };
    }
    return { "Network", CheckLevel::Warn,
        "openrouter.ai responded with HTTP " + std::to_string(status) };
}

// ── check_config_dir() ───────────────────────────────────────────────────────
CheckResult SelfCheck::check_config_dir() {
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) {
        return { "Config Dir", CheckLevel::Fail, "APPDATA environment variable not set" };
    }

    std::string config_dir = std::string(appdata) + "\\AnyClaw";

    if (fs::exists(config_dir) && fs::is_directory(config_dir)) {
        // Try to write a test file
        std::string test_file = config_dir + "\\.anyclaw_test";
        std::ofstream f(test_file);
        if (f.is_open()) {
            f << "test";
            f.close();
            fs::remove(test_file);
            return { "Config Dir", CheckLevel::Pass,
                "Config directory exists and is writable: " + config_dir };
        }
        return { "Config Dir", CheckLevel::Fail,
            "Config directory exists but is not writable: " + config_dir };
    }

    // Directory doesn't exist — try to create it
    CheckResult result{ "Config Dir", CheckLevel::Fail,
        "Config directory does not exist: " + config_dir };
    if (try_repair(result)) {
        return result;
    }
    return result;
}

// ── try_repair() ─────────────────────────────────────────────────────────────
bool SelfCheck::try_repair(CheckResult& result) {
    if (result.name == "npm") {
        // Try npm cache clean --force
        std::string output;
        if (exec_cmd_simple("npm cache clean --force", output, 10000)) {
            // Re-check after clean
            if (exec_cmd_simple("npm --version", output) && !output.empty()) {
                result.level = CheckLevel::Pass;
                result.message = "npm repaired (cache cleaned). Version: " + output;
                result.repaired = true;
                return true;
            }
        }
        return false;
    }

    if (result.name == "Config Dir") {
        const char* appdata = std::getenv("APPDATA");
        if (!appdata) return false;

        std::string config_dir = std::string(appdata) + "\\AnyClaw";
        try {
            if (fs::create_directories(config_dir)) {
                result.level = CheckLevel::Pass;
                result.message = "Config directory created: " + config_dir;
                result.repaired = true;
                return true;
            }
        } catch (...) {
            return false;
        }
        return false;
    }

    return false; // Unknown check — no repair strategy
}

// ── run() ────────────────────────────────────────────────────────────────────
bool SelfCheck::run(std::vector<CheckResult>& results) {
    results.clear();
    bool all_ok = true;

    // 1. Node.js
    auto r1 = check_nodejs();
    if (r1.level == CheckLevel::Fail) all_ok = false;
    results.push_back(std::move(r1));

    // 2. npm
    auto r2 = check_npm();
    if (r2.level == CheckLevel::Fail) all_ok = false;
    results.push_back(std::move(r2));

    // 3. Network
    auto r3 = check_network();
    if (r3.level == CheckLevel::Fail) all_ok = false;
    results.push_back(std::move(r3));

    // 4. Config directory
    auto r4 = check_config_dir();
    if (r4.level == CheckLevel::Fail) all_ok = false;
    results.push_back(std::move(r4));

    return all_ok;
}

} // namespace anyclaw
