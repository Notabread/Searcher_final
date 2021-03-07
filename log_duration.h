#pragma once

#include <chrono>
#include <iostream>
#include <string>

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(name, stream) LogDuration UNIQUE_VAR_NAME_PROFILE(name, stream)

class LogDuration {
public:
    explicit LogDuration(const std::string& operation_name, std::ostream& out = std::cerr)
            : operation_name_(operation_name), out_(out) {};

    ~LogDuration() {
        using namespace std::literals;
        const auto end_time = std::chrono::steady_clock::now();
        const auto dur = end_time - start_time_;
        out_ << operation_name_ << ": "
             << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
             << " ms"s
             << std::endl;
    }

private:
    const std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    const std::string operation_name_;
    std::ostream& out_;
};
