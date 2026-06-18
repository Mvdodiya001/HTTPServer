#pragma once

#include <mutex>
#include <map>
#include <deque>
#include <chrono>
#include <boost/asio/ip/address.hpp>

namespace asio = boost::asio;

class RateLimiter {
    std::mutex mutex_;
    std::map<asio::ip::address, std::deque<std::chrono::steady_clock::time_point>> clients_;
    const int max_requests = 100;
    const std::chrono::seconds window = std::chrono::seconds(10); 

public:
    bool is_allowed(asio::ip::address ip);
};
