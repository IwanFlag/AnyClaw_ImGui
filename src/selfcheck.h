#pragma once
#include <string>
#include <vector>

namespace anyclaw {

enum class CheckLevel { Pass, Warn, Fail };

struct CheckResult {
    std::string name;
    CheckLevel level;
    std::string message;
    bool repaired = false;
};

class SelfCheck {
public:
    // Run all checks, return true if all pass (or repaired)
    static bool run(std::vector<CheckResult>& results);

private:
    static CheckResult check_nodejs();
    static CheckResult check_npm();
    static CheckResult check_network();
    static CheckResult check_config_dir();
    static bool try_repair(CheckResult& result);
};

} // namespace anyclaw
