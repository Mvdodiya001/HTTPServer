#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>
#include "HttpSession.hpp"
#include "RateLimiter.hpp"
#include "Logger.hpp"
#include "ChatRoom.hpp"
#include "Database.hpp"

#include <boost/asio/ssl.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace ssl = asio::ssl;

class Listener : public std::enable_shared_from_this<Listener> {
    asio::io_context& ioc_; 
    ssl::context& ctx_;
    tcp::acceptor acc_; 
    std::string root_;
    std::shared_ptr<RateLimiter> lim_; 
    std::shared_ptr<Logger> log_; 
    std::shared_ptr<ChatRoom> rm_; 
    std::shared_ptr<Database> db_;
public:
    Listener(asio::io_context& i, ssl::context& ctx, tcp::endpoint e, std::string r, std::shared_ptr<RateLimiter> li, std::shared_ptr<Logger> lo, std::shared_ptr<ChatRoom> rm, std::shared_ptr<Database> db);
    void run();
    void do_accept();
    void on_acc(boost::system::error_code ec, tcp::socket s);
};
