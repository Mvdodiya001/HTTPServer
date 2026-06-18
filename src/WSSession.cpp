#include "WSSession.hpp"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>

WSSession::WSSession(asio::ssl::stream<beast::tcp_stream>&& stream, std::shared_ptr<ChatRoom> room, std::shared_ptr<Logger> log)
    : ws_(std::move(stream)), strand_(ws_.get_executor()), 
      timer_(ws_.get_executor(), std::chrono::seconds(15)),
      room_(room), logger_(log) {}

void WSSession::run(http::request<http::string_body> req) {
    ws_.async_accept(req, asio::bind_executor(strand_, beast::bind_front_handler(&WSSession::on_accept, shared_from_this())));
}

void WSSession::on_accept(beast::error_code ec) {
    if (ec) return;
    room_->join(this);
    logger_->log(Logger::Level::INFO, "CHAT", "User connected", beast::get_lowest_layer(ws_).socket().remote_endpoint().address().to_string());
    
    // Start Heartbeat
    on_timer({});
    
    do_read(); // Wait for messages
}

void WSSession::do_read() {
    ws_.async_read(buffer_, asio::bind_executor(strand_, beast::bind_front_handler(&WSSession::on_read, shared_from_this())));
}

void WSSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    if(ec == websocket::error::closed) return;
    if(ec) {
            logger_->log(Logger::Level::WARNING, "CHAT", "Read error: " + ec.message());
            room_->leave(this);
            return;
    }
    
    room_->handle_message(this, beast::buffers_to_string(buffer_.data()));
    buffer_.consume(buffer_.size());
    
    ws_.async_read(buffer_, asio::bind_executor(strand_, beast::bind_front_handler(&WSSession::on_read, shared_from_this())));
}

void WSSession::send(std::shared_ptr<std::string const> const& msg) {
    asio::post(strand_, [this, self=shared_from_this(), msg](){
        queue_.push_back({msg, false});
        if(queue_.size() > 1) return; // Already writing
        do_write();
    });
}

void WSSession::do_write() {
    if(queue_.front().is_ping) {
        ws_.async_ping("", asio::bind_executor(strand_, beast::bind_front_handler(&WSSession::on_ping, shared_from_this())));
    } else {
        ws_.async_write(asio::buffer(*queue_.front().payload), 
            asio::bind_executor(strand_, beast::bind_front_handler(&WSSession::on_write, shared_from_this())));
    }
}

void WSSession::on_write(beast::error_code ec, std::size_t) {
    if(ec) {
        logger_->log(Logger::Level::ERROR, "CHAT", "Write error: " + ec.message());
        room_->leave(this);
        return; 
    }
    queue_.erase(queue_.begin());
    if(!queue_.empty()) do_write();
}

void WSSession::on_ping(beast::error_code ec) {
    if(ec) {
        logger_->log(Logger::Level::WARNING, "CHAT", "Ping error: " + ec.message());
        room_->leave(this);
        return;
    }
    queue_.erase(queue_.begin());
    if(!queue_.empty()) do_write();
}

void WSSession::on_timer(beast::error_code ec) {
    if(ec && ec != asio::error::operation_aborted) return;
    
    if(ws_.is_open()) {
        // Enqueue Ping
        asio::post(strand_, [this, self=shared_from_this()](){
            queue_.push_back({nullptr, true});
            if(queue_.size() > 1) return;
            do_write();
        });
        
        timer_.expires_after(std::chrono::seconds(15));
        timer_.async_wait(asio::bind_executor(strand_, beast::bind_front_handler(&WSSession::on_timer, shared_from_this())));
    }
}
