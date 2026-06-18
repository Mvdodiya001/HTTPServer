#include "HttpSession.hpp"
#include "WSSession.hpp"
#include <sstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using error_code = boost::system::error_code;

// Helper function local to this file
static beast::string_view get_mime_type(beast::string_view path) {
    using beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        return (pos == beast::string_view::npos) ? "" : path.substr(pos);
    }();
    if (iequals(ext, ".html") || iequals(ext, ".htm")) return "text/html";
    if (iequals(ext, ".css"))  return "text/css";
    if (iequals(ext, ".js"))   return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".png"))  return "image/png";
    return "application/text";
}

HttpSession::HttpSession(tcp::socket&& s, ssl::context& ctx, std::string r, std::shared_ptr<RateLimiter> lim, std::shared_ptr<Logger> log, std::shared_ptr<ChatRoom> rm, std::shared_ptr<Database> db)
    : stream_(beast::tcp_stream(std::move(s)), ctx), root_(r), limiter_(lim), logger_(log), room_(rm), db_(db) {}

HttpSession::~HttpSession() {
    if(!is_transferred_) {
        // Just close, no need to async shutdown in destructor as it might be destroyed after move
    }
}

void HttpSession::run() {
    asio::dispatch(stream_.get_executor(),
        beast::bind_front_handler(&HttpSession::do_handshake, shared_from_this()));
}

void HttpSession::do_handshake() {
    stream_.async_handshake(ssl::stream_base::server,
        beast::bind_front_handler(&HttpSession::on_handshake, shared_from_this()));
}

void HttpSession::on_handshake(beast::error_code ec) {
    if(ec) return; 
    do_read();
}

void HttpSession::do_read() {
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
    auto parser = std::make_shared<http::request_parser<http::string_body>>();
    parser->body_limit(1024 * 1024); // 1MB

    http::async_read(stream_, buffer_, *parser, [this, self=shared_from_this(), parser](error_code ec, std::size_t){
        if(ec) return;
        on_req(parser->release());
    });
}

void HttpSession::on_req(http::request<http::string_body> req) {
    auto ip = beast::get_lowest_layer(stream_).socket().remote_endpoint().address();
    
    // CORS Preflight
    if (req.method() == http::verb::options) {
        s_res_ = {http::status::ok, req.version()};
        s_res_.set(http::field::server, "Secure-C++-Server");
        s_res_.set(http::field::access_control_allow_origin, "*");
        s_res_.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
        s_res_.set(http::field::access_control_allow_headers, "Content-Type");
        s_res_.body() = "";
        s_res_.prepare_payload();
        http::async_write(stream_, s_res_, beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), false));
        return;
    }

    // 1. WebSocket Upgrade
    if(websocket::is_upgrade(req)) {
        is_transferred_ = true;
        std::make_shared<WSSession>(std::move(stream_), room_, logger_)->run(std::move(req));
        return;
    }

    // 2. API Endpoints
    if (req.target() == "/api/signup" && req.method() == http::verb::post) {
        handle_signup(req);
        return;
    }
    if (req.target() == "/api/login" && req.method() == http::verb::post) {
        handle_login(req);
        return;
    }
    if (req.target() == "/api/get_profile" && req.method() == http::verb::post) {
        handle_get_profile(req);
        return;
    }
    if (req.target() == "/api/update_profile" && req.method() == http::verb::post) {
        handle_update_profile(req);
        return;
    }
    if (req.target() == "/api/messages" && req.method() == http::verb::get) {
        handle_get_messages(req);
        return;
    }
    if (req.target() == "/api/users" && req.method() == http::verb::get) {
        handle_get_users(req);
        return;
    }
    
    // New Routes
    if (req.target() == "/api/create_group" && req.method() == http::verb::post) {
        handle_create_group(req);
        return;
    }
    if (req.target() == "/api/join_group" && req.method() == http::verb::post) {
        handle_join_group(req);
        return;
    }
    if (req.target() == "/api/group_messages" && req.method() == http::verb::post) {
        handle_get_group_messages(req);
        return;
    }
    if (req.target() == "/api/dms" && req.method() == http::verb::post) {
        handle_get_dms(req);
        return;
    }
    if (req.target() == "/api/recent_chats" && req.method() == http::verb::post) {
        handle_get_recent_chats(req);
        return;
    }

    // 3. Static Files & SPA
    beast::string_view target = req.target();
    if(target == "/") target = "/index.html";
    
    std::string path = root_;
    path.append(target.data(), target.size());
    beast::error_code fe;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, fe);

    // SPA Fallback
    if(fe && target.rfind('.') == std::string::npos && target.find("/api/") == std::string::npos) {
            std::string index_path = root_ + "/index.html";
            body.open(index_path.c_str(), beast::file_mode::scan, fe);
    }

    if(fe) return send_err(http::status::not_found, "Not Found");

    f_res_ = {std::piecewise_construct, std::make_tuple(std::move(body)), std::make_tuple(http::status::ok, req.version())};
    f_res_.set(http::field::server, "Secure-C++-Server");
    f_res_.set(http::field::content_type, get_mime_type(path));
    f_res_.content_length(f_res_.body().size());
    f_res_.keep_alive(req.keep_alive());
    http::async_write(stream_, f_res_, beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), f_res_.keep_alive()));
}

void HttpSession::send_json(std::string json) {
    s_res_ = {http::status::ok, 11};
    s_res_.set(http::field::server, "Secure-C++-Server");
    s_res_.set(http::field::content_type, "application/json");
    s_res_.set(http::field::access_control_allow_origin, "*");
    s_res_.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
    s_res_.set(http::field::access_control_allow_headers, "Content-Type");
    s_res_.body() = json;
    s_res_.prepare_payload();
    http::async_write(stream_, s_res_, beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), false));
}

void HttpSession::handle_signup(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        std::string user = j.at("username").get<std::string>();
        std::string pass = j.at("password").get<std::string>();

        db_->async_create_user(user, pass, [this, self=shared_from_this()](bool success) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, success]() {
                if(success) send_json("{\"status\":\"success\"}");
                else send_err(http::status::conflict, "{\"error\":\"User exists\"}");
            });
        });
    } catch(...) {
        send_err(http::status::bad_request, "{\"error\":\"Invalid JSON\"}");
    }
}

void HttpSession::handle_login(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        std::string user = j.at("username").get<std::string>();
        std::string pass = j.at("password").get<std::string>();

        db_->async_verify_user(user, pass, [this, self=shared_from_this()](bool success) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, success]() {
                if(success) {
                    send_json("{\"status\":\"success\", "
                              "\"token\":\"fake-jwt-token-for-demo\"}");
                } else {
                    send_err(http::status::unauthorized, "{\"error\":\"Invalid credentials\"}");
                }
            });
        });
    } catch(...) {
        send_err(http::status::bad_request, "{\"error\":\"Invalid JSON\"}");
    }
}

void HttpSession::handle_get_profile(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        std::string user = j.at("username").get<std::string>();
        
        db_->async_get_profile(user, [this, self=shared_from_this()](nlohmann::json profile) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, profile]() {
                if (profile.empty()) {
                    send_err(http::status::not_found, "{\"error\":\"User not found\"}");
                } else {
                    send_json(profile.dump());
                }
            });
        });
    } catch(...) {
        send_err(http::status::bad_request, "{\"error\":\"Invalid JSON\"}");
    }
}

void HttpSession::handle_update_profile(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        std::string user = j.at("username").get<std::string>();
        std::string bio = j.at("bio").get<std::string>();
        std::string status = j.at("status").get<std::string>();
        
        db_->async_update_profile(user, bio, status, [this, self=shared_from_this()](bool success) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, success]() {
                    if (success) {
                        send_json("{\"status\":\"success\"}");
                    } else {
                        send_err(http::status::internal_server_error, "{\"error\":\"Update failed\"}");
                    }
            });
        });
    } catch(...) {
        send_err(http::status::bad_request, "{\"error\":\"Invalid JSON\"}");
    }
}

void HttpSession::handle_get_messages(const http::request<http::string_body>& req) {
    db_->async_get_messages(50, [this, self=shared_from_this()](nlohmann::json msgs) {
        asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, msgs]() {
            send_json(msgs.dump());
        });
    });
}

void HttpSession::handle_get_users(const http::request<http::string_body>& req) {
    db_->async_get_users([this, self=shared_from_this()](nlohmann::json users) {
        asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, users]() {
            send_json(users.dump());
        });
    });
}


void HttpSession::handle_create_group(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        std::string name = j.at("name");
        std::string creator = j.at("username");
        
        db_->async_create_group(name, creator, [this, self=shared_from_this()](int id) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, id]() {
                if(id != -1) send_json(nlohmann::json({{"status", "success"}, {"group_id", id}}).dump());
                else send_err(http::status::internal_server_error, "{\"error\": \"Failed to create group\"}");
            });
        });
    } catch(...) { send_err(http::status::bad_request, "{\"error\": \"Invalid JSON\"}"); }
}

void HttpSession::handle_join_group(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        int gid = j.at("group_id");
        std::string username = j.at("username");
        
        db_->async_join_group(gid, username, [this, self=shared_from_this()](bool s) {
             asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, s]() {
                send_json(nlohmann::json({{"status", s ? "success" : "failure"}}).dump());
             });
        });
    } catch(...) { send_err(http::status::bad_request, "{\"error\": \"Invalid JSON\"}"); }
}

void HttpSession::handle_get_group_messages(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        int gid = j.at("group_id");
        
        db_->async_get_group_messages(gid, [this, self=shared_from_this()](nlohmann::json m) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, m]() {
               send_json(m.dump()); 
            });
        });
    } catch(...) { send_err(http::status::bad_request, "{\"error\": \"Invalid JSON\"}"); }
}

void HttpSession::handle_get_dms(const http::request<http::string_body>& req) {
    try {
        auto j = nlohmann::json::parse(req.body());
        std::string u1 = j.at("user1");
        std::string u2 = j.at("user2");
        
        db_->async_get_dms(u1, u2, [this, self=shared_from_this()](nlohmann::json m) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, m]() {
               send_json(m.dump()); 
            });
        });
    } catch(...) { send_err(http::status::bad_request, "{\"error\": \"Invalid JSON\"}"); }
}

void HttpSession::handle_get_recent_chats(const http::request<http::string_body>& req) {
    try {
         auto j = nlohmann::json::parse(req.body());
         std::string u = j.at("username");
         
         db_->async_get_recent_chats(u, [this, self=shared_from_this()](nlohmann::json c) {
            asio::post(beast::get_lowest_layer(stream_).get_executor(), [this, self, c]() {
                send_json(c.dump());
            });
         });
    } catch(...) { send_err(http::status::bad_request, "{\"error\": \"Invalid JSON\"}"); }
}


void HttpSession::send_err(http::status s, std::string m) {
    s_res_ = {s, 11}; 
    s_res_.set(http::field::access_control_allow_origin, "*");
    s_res_.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
    s_res_.set(http::field::access_control_allow_headers, "Content-Type");
    s_res_.body() = m; 
    s_res_.prepare_payload();
    http::async_write(stream_, s_res_, beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), false));
}

void HttpSession::on_write(bool k, error_code, std::size_t) { if(k) do_read(); else beast::get_lowest_layer(stream_).socket().shutdown(tcp::socket::shutdown_send); }
