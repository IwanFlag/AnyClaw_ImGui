// health.cpp — Health monitor implementation

#include "health.h"
#include <chrono>
#include <windows.h>

namespace anyclaw {

void HealthMonitor::start(const OpenClawInfo& info, int interval_sec) {
    if (running_.load()) return;

    interval_.store(interval_sec);
    running_.store(true);
    thread_ = std::thread(&HealthMonitor::monitor_loop, this, info);
}

void HealthMonitor::stop() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void HealthMonitor::set_interval(int sec) {
    interval_.store(sec);
}

void HealthMonitor::on_status_change(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

OpenClawStatus HealthMonitor::current_status() const {
    return status_.load();
}

std::string HealthMonitor::last_message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_message_;
}

bool HealthMonitor::is_alerting() const {
    return alerting_.load();
}

void HealthMonitor::monitor_loop(OpenClawInfo info) {
    int consecutive_failures = 0;

    while (running_.load()) {
        OpenClawStatus new_status = OpenClawManager::check_status(info);
        std::string message;
        bool alert = false;

        switch (new_status) {
            case OpenClawStatus::Running:
                message = "OpenClaw 运行中";
                consecutive_failures = 0;
                alert = false;
                break;

            case OpenClawStatus::Detected:
                // Try to start
                message = "检测到 OpenClaw 未运行，正在启动...";
                OpenClawManager::start_gateway(info);
                // Re-check after starting
                Sleep(3000);
                new_status = OpenClawManager::check_status(info);
                if (new_status != OpenClawStatus::Running) {
                    message = "OpenClaw 无法启动，请手动检查";
                    alert = true;
                } else {
                    message = "OpenClaw 已启动";
                }
                break;

            case OpenClawStatus::Error:
                consecutive_failures++;
                if (consecutive_failures >= 3) {
                    message = "OpenClaw 响应异常 (连续 " +
                              std::to_string(consecutive_failures) + " 次)";
                    alert = true;
                } else {
                    message = "OpenClaw 响应缓慢...";
                }
                break;

            case OpenClawStatus::NotInstalled:
                message = "未检测到 OpenClaw 安装";
                alert = true;
                break;

            default:
                message = "状态未知";
                break;
        }

        // Update state
        OpenClawStatus prev = status_.exchange(new_status);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_message_ = message;
        }
        alerting_.store(alert);

        // Notify callback if status changed
        if (prev != new_status || alert) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (callback_) {
                callback_(new_status, message);
            }
        }

        // Sleep for interval
        int ms = interval_.load() * 1000;
        auto start = std::chrono::steady_clock::now();
        while (running_.load()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= ms) {
                break;
            }
            Sleep(100);  // Check every 100ms if we should stop
        }
    }
}

} // namespace anyclaw
