#include "Listener.hpp"
#include <boost/asio/strand.hpp>

Listener::Listener(asio::io_context& i, ssl::context& ctx, tcp::endpoint e, std::string r, std::shared_ptr<RateLimiter> li, std::shared_ptr<Logger> lo, std::shared_ptr<ChatRoom> rm, std::shared_ptr<Database> db)
    : ioc_(i), ctx_(ctx), acc_(asio::make_strand(i)), root_(r), lim_(li), log_(lo), rm_(rm), db_(db) {
    acc_.open(e.protocol()); 
    acc_.set_option(asio::socket_base::reuse_address(true));
    acc_.set_option(asio::socket_base::keep_alive(true)); 
    acc_.bind(e); 
    acc_.listen();
}

void Listener::run() { do_accept(); }

void Listener::do_accept() { 
    acc_.async_accept(asio::make_strand(ioc_), beast::bind_front_handler(&Listener::on_acc, shared_from_this())); 
}

void Listener::on_acc(boost::system::error_code ec, tcp::socket s) { 
    if(!ec) {
        std::make_shared<HttpSession>(
            std::move(s), 
            ctx_, 
            root_, lim_, log_, rm_, db_
        )->run(); 
    }
    do_accept(); 
}
