#pragma once

#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <vector>
#include <string>
#include "ChatRoom.hpp"
#include "Logger.hpp"

#include <boost/asio/strand.hpp>

#include <boost/asio/ssl.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

class WSSession : public std::enable_shared_from_this<WSSession> {
    struct QueuedMessage {
        std::shared_ptr<std::string const> payload;
        bool is_ping;
    };

    websocket::stream<asio::ssl::stream<beast::tcp_stream>> ws_;
    asio::any_io_executor strand_;
    asio::steady_timer timer_;
    beast::flat_buffer buffer_;
    std::shared_ptr<ChatRoom> room_;
    std::shared_ptr<Logger> logger_;
    std::vector<QueuedMessage> queue_;

public:
    WSSession(asio::ssl::stream<beast::tcp_stream>&& stream, std::shared_ptr<ChatRoom> room, std::shared_ptr<Logger> log);

    void run(http::request<http::string_body> req);
    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void send(std::shared_ptr<std::string const> const& msg);
    void do_write();
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void on_ping(beast::error_code ec);
    void on_timer(beast::error_code ec);
};
