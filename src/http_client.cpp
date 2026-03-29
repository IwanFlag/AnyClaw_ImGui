// http_client.cpp — WinHTTP wrapper implementation

#include "http_client.h"
#include <sstream>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace anyclaw {

// ── Helpers ─────────────────────────────────────────────────────────────

static std::wstring to_wstring(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring ws(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

static std::string to_string(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, NULL, NULL);
    return s;
}

// ── HttpClient ──────────────────────────────────────────────────────────

HttpClient::HttpClient() {
    hSession_ = WinHttpOpen(L"AnyClaw/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (hSession_) {
        WinHttpSetTimeouts(hSession_,
            connect_timeout_ms_, connect_timeout_ms_,
            read_timeout_ms_, read_timeout_ms_);
    }
}

HttpClient::~HttpClient() {
    if (hSession_) {
        WinHttpCloseHandle(hSession_);
    }
}

void HttpClient::set_timeout(int connect_ms, int read_ms) {
    connect_timeout_ms_ = connect_ms;
    read_timeout_ms_ = read_ms;
    if (hSession_) {
        WinHttpSetTimeouts(hSession_, connect_ms, connect_ms, read_ms, read_ms);
    }
}

HttpClient::UrlParts HttpClient::parse_url(const std::string& url) {
    UrlParts parts;
    std::wstring wurl = to_wstring(url);

    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = 1;
    uc.dwUrlPathLength = 1;
    uc.dwExtraInfoLength = 1;

    WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc);

    parts.host = std::wstring(uc.lpszHostName, uc.dwHostNameLength);
    parts.path = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) {
        parts.path += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    }
    parts.port = uc.nPort;
    parts.is_https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    return parts;
}

HttpResponse HttpClient::execute(const std::string& method,
                                  const std::string& url,
                                  const std::string& body,
                                  const std::vector<HttpHeader>& headers) {
    HttpResponse resp;

    if (!hSession_) {
        resp.error = "WinHTTP session not initialized";
        return resp;
    }

    UrlParts up = parse_url(url);

    // Connect
    HINTERNET hConnect = WinHttpConnect(hSession_, up.host.c_str(), up.port, 0);
    if (!hConnect) {
        resp.error = "WinHttpConnect failed: " + std::to_string(GetLastError());
        return resp;
    }

    // Open request
    DWORD flags = up.is_https ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wmethod = to_wstring(method);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        wmethod.c_str(), up.path.c_str(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!hRequest) {
        resp.error = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    // Add headers
    for (const auto& h : headers) {
        std::wstring wh = to_wstring(h.name + ": " + h.value);
        WinHttpAddRequestHeaders(hRequest, wh.c_str(), (DWORD)wh.size(),
            WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    BOOL send_ok;
    if (body.empty()) {
        send_ok = WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    } else {
        send_ok = WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0);
    }

    if (!send_ok) {
        resp.error = "WinHttpSendRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        resp.error = "WinHttpReceiveResponse failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    // Get status code
    DWORD status_size = sizeof(DWORD);
    DWORD status_code = 0;
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
        WINHTTP_NO_HEADER_INDEX);
    resp.status_code = (int)status_code;

    // Read body
    std::string response_body;
    DWORD bytes_available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
        std::vector<char> buf(bytes_available + 1);
        DWORD bytes_read = 0;
        if (WinHttpReadData(hRequest, buf.data(), bytes_available, &bytes_read)) {
            response_body.append(buf.data(), bytes_read);
        }
        bytes_available = 0;
    }

    resp.body = response_body;
    resp.success = (status_code >= 200 && status_code < 300);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    return resp;
}

HttpResponse HttpClient::get(const std::string& url,
                              const std::vector<HttpHeader>& headers) {
    return execute("GET", url, "", headers);
}

HttpResponse HttpClient::post(const std::string& url,
                               const std::string& json_body,
                               const std::vector<HttpHeader>& headers) {
    return execute("POST", url, json_body, headers);
}

HttpResponse HttpClient::get_auth(const std::string& url, const std::string& token) {
    std::vector<HttpHeader> headers = {
        {"Authorization", "Bearer " + token}
    };
    return get(url, headers);
}

HttpResponse HttpClient::post_auth(const std::string& url,
                                    const std::string& token,
                                    const std::string& json_body) {
    std::vector<HttpHeader> headers = {
        {"Authorization", "Bearer " + token},
        {"Content-Type", "application/json"}
    };
    return post(url, json_body, headers);
}

} // namespace anyclaw
