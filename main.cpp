#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <filesystem>
#include <boost/asio.hpp>

#include <boost/asio/ssl.hpp>
#include "server_certificate.hpp"

#include "Logger.hpp"
#include "Database.hpp"
#include "RateLimiter.hpp"
#include "ChatRoom.hpp"
#include "Listener.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace ssl = asio::ssl;

int main() {
    //let's  made  deoplyment  ready  

    auto const root = std::filesystem::current_path().string() + "/frontend/dist";
    auto log = std::make_shared<Logger>();
    auto lim = std::make_shared<RateLimiter>();
    auto db = std::make_shared<Database>(log);
    auto rm = std::make_shared<ChatRoom>(db);
    auto const threads = std::max<int>(1, std::thread::hardware_concurrency());
    asio::io_context ioc{threads};

    // SSL Context
    ssl::context ctx{ssl::context::tlsv12};
    load_server_certificate(ctx);

    std::make_shared<Listener>(ioc, ctx, tcp::endpoint{asio::ip::make_address("0.0.0.0"), 8888}, root, lim, log, rm, db)->run();

    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](beast::error_code const&, int sig){
        ioc.stop();
        std::cout << "\nShutdown signal " << sig << " received. Stopping..." << std::endl;
    });

    std::cout << "--- SECURE FULL-STACK SERVER ONLINE ---" << std::endl;
    std::cout << "Listening on Port: 8888" << std::endl;
    std::cout << "Database: SQLite3 Enabled" << std::endl;
    std::cout << "Concurrency: " << threads << " threads" << std::endl;

    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(int i = 0; i < threads - 1; ++i) v.emplace_back([&ioc]{ ioc.run(); });
    ioc.run();
    
    for(auto& t : v) t.join();
    return 0;
}
