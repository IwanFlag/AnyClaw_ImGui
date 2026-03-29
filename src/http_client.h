#pragma once
// http_client.h — WinHTTP wrapper

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

namespace anyclaw {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    bool success = false;
    std::string error;
};

struct HttpHeader {
    std::string name;
    std::string value;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Set timeout in milliseconds
    void set_timeout(int connect_ms, int read_ms);

    // GET request
    HttpResponse get(const std::string& url,
                     const std::vector<HttpHeader>& headers = {});

    // POST request with JSON body
    HttpResponse post(const std::string& url,
                      const std::string& json_body,
                      const std::vector<HttpHeader>& headers = {});

    // Convenience: GET with Bearer token
    HttpResponse get_auth(const std::string& url, const std::string& token);

    // Convenience: POST with Bearer token and JSON
    HttpResponse post_auth(const std::string& url,
                           const std::string& token,
                           const std::string& json_body);

private:
    HINTERNET hSession_ = NULL;
    int connect_timeout_ms_ = 10000;
    int read_timeout_ms_ = 10000;

    struct UrlParts {
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        bool is_https = true;
    };

    UrlParts parse_url(const std::string& url);
    HttpResponse execute(const std::string& method,
                         const std::string& url,
                         const std::string& body,
                         const std::vector<HttpHeader>& headers);
};

} // namespace anyclaw
