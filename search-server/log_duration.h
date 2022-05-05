#pragma once

#include <chrono>
#include <iostream>
#include <thread>

#define LOG_DURATION_STREAM(x, y) LogDuration foo(x, std::cerr)

class LogDuration {
public:

    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& id, std::ostream& out) : id_(id) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        std::cerr << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    const std::string id_;
    const Clock::time_point start_time_ = Clock::now();
};