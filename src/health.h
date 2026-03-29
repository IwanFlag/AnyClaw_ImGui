#pragma once
// health.h — Periodic OpenClaw health monitor

#include "openclaw_mgr.h"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

namespace anyclaw {

class HealthMonitor {
public:
    using StatusCallback = std::function<void(OpenClawStatus status, const std::string& message)>;

    HealthMonitor() = default;
    ~HealthMonitor() { stop(); }

    void start(const OpenClawInfo& info, int interval_sec = 10);
    void stop();
    void set_interval(int sec);
    void on_status_change(StatusCallback cb);

    OpenClawStatus current_status() const;
    std::string    last_message() const;
    bool           is_alerting() const;

private:
    void monitor_loop(OpenClawInfo info);

    std::atomic<bool> running_{false};
    std::atomic<int>  interval_{10};
    std::atomic<OpenClawStatus> status_{OpenClawStatus::Unknown};
    std::string last_message_;
    std::atomic<bool> alerting_{false};

    mutable std::mutex mutex_;
    StatusCallback callback_;
    std::thread thread_;
};

} // namespace anyclaw
