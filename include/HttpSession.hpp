#pragma once

#include <boost/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>
#include "RateLimiter.hpp"
#include "Logger.hpp"
#include "ChatRoom.hpp"
#include "Database.hpp"

#include <boost/asio/ssl.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
    asio::ssl::stream<beast::tcp_stream> stream_; // Wait, no. asio::ssl::stream<beast::tcp_stream>
    beast::flat_buffer buffer_;
    std::string root_;
    std::shared_ptr<RateLimiter> limiter_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<ChatRoom> room_;
    std::shared_ptr<Database> db_;
    http::response<http::file_body> f_res_;
    http::response<http::string_body> s_res_;
    bool is_transferred_ = false;

public:
    HttpSession(tcp::socket&& s, ssl::context& ctx, std::string r, std::shared_ptr<RateLimiter> lim, std::shared_ptr<Logger> log, std::shared_ptr<ChatRoom> rm, std::shared_ptr<Database> db);
    ~HttpSession();

    void run();
    void do_handshake();
    void on_handshake(beast::error_code ec);
    void do_read();
    void on_req(http::request<http::string_body> req);
    void send_json(std::string json);
    void handle_signup(const http::request<http::string_body>& req);
    void handle_login(const http::request<http::string_body>& req);
    void handle_get_profile(const http::request<http::string_body>& req);
    void handle_update_profile(const http::request<http::string_body>& req);
    void handle_get_messages(const http::request<http::string_body>& req);
    void handle_get_users(const http::request<http::string_body>& req);
    
    // New Handlers
    void handle_create_group(const http::request<http::string_body>& req);
    void handle_join_group(const http::request<http::string_body>& req);
    void handle_get_group_messages(const http::request<http::string_body>& req);
    void handle_get_dms(const http::request<http::string_body>& req);
    void handle_get_recent_chats(const http::request<http::string_body>& req);
    void send_err(http::status s, std::string m);
    void on_write(bool k, beast::error_code, std::size_t);
};
