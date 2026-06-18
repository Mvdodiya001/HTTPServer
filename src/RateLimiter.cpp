#include "RateLimiter.hpp"

bool RateLimiter::is_allowed(asio::ip::address ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto& ts = clients_[ip];
    while (!ts.empty() && (now - ts.front() > window)) ts.pop_front();
    if (ts.size() >= max_requests) return false;
    ts.push_back(now);
    return true;
}
