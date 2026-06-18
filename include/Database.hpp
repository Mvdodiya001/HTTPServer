#pragma once

#include <sqlite3.h>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <functional>
#include "Logger.hpp"

class Database {
    std::vector<sqlite3*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    const size_t pool_size_ = 5;
    std::shared_ptr<Logger> logger_;

    struct ConnectionGuard {
        Database& parent;
        sqlite3* db;
        ConnectionGuard(Database& p) : parent(p) { db = parent.checkout(); }
        ~ConnectionGuard() { parent.release(db); }
        sqlite3* get() { return db; }
        operator sqlite3*() { return db; }
    };

    sqlite3* checkout();
    void release(sqlite3* db);

public:
    Database(std::shared_ptr<Logger> log);
    ~Database();

    void init();
    bool create_user(const std::string& username, const std::string& password);
    bool verify_user(const std::string& username, const std::string& password);
    nlohmann::json get_user_profile(const std::string& username);
    bool update_user_profile(const std::string& username, const std::string& bio, const std::string& status);
    void add_message_json(const std::string& sender, const std::string& content);
    nlohmann::json get_all_messages(int limit = 50);
    nlohmann::json get_all_users();
    // Messaging & Groups API
    int create_group(const std::string& name, const std::string& created_by);
    bool join_group(int group_id, const std::string& username);
    nlohmann::json get_user_groups(const std::string& username);
    std::vector<std::string> get_group_members(int group_id);
    nlohmann::json get_group_messages(int group_id, int limit = 50);
    nlohmann::json get_dms(const std::string& user1, const std::string& user2, int limit = 50);
    nlohmann::json get_recent_chats(const std::string& username);
    void add_group_message(int group_id, const std::string& sender, const std::string& content);
    void add_dm_message(const std::string& sender, const std::string& recipient, const std::string& content);

    // Async Wrappers
    void async_create_user(std::string username, std::string password, std::function<void(bool)> cb);
    void async_verify_user(std::string username, std::string password, std::function<void(bool)> cb);
    void async_get_messages(int limit, std::function<void(nlohmann::json)> cb);
    void async_get_users(std::function<void(nlohmann::json)> cb);
    void async_add_message(std::string sender, std::string content);
    void async_get_profile(std::string username, std::function<void(nlohmann::json)> cb);
    void async_update_profile(std::string username, std::string bio, std::string status, std::function<void(bool)> cb);

    // New Async Wrappers
    void async_create_group(std::string name, std::string created_by, std::function<void(int)> cb);
    void async_join_group(int group_id, std::string username, std::function<void(bool)> cb);
    void async_get_user_groups(std::string username, std::function<void(nlohmann::json)> cb);
    void async_get_group_messages(int group_id, std::function<void(nlohmann::json)> cb);
    void async_get_dms(std::string user1, std::string user2, std::function<void(nlohmann::json)> cb);
    void async_get_recent_chats(std::string username, std::function<void(nlohmann::json)> cb);

private:
    boost::asio::thread_pool thread_pool_{1}; 
};
