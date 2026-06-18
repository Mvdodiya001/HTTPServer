#pragma once

#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>

class Logger {
    std::mutex mutex_;
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    void log(const std::string& module, const std::string& msg, const std::string& meta = "") {
        log(Level::INFO, module, msg, meta);
    }

    void log(Level level, const std::string& module, const std::string& msg, const std::string& meta = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "] "
                  << "[" << level_to_string(level) << "] "
                  << "[" << module << "] " << msg;
        if(!meta.empty()) std::cout << " [" << meta << "]";
        std::cout << std::endl;
    }

private:
    std::string level_to_string(Level level) {
        switch(level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO";
            case Level::WARNING: return "WARN";
            case Level::ERROR: return "ERROR";
            default: return "INFO";
        }
    }
};
