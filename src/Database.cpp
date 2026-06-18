#include "Database.hpp"

namespace {
    struct StatementGuard {
        sqlite3_stmt* stmt = nullptr;
        ~StatementGuard() { if (stmt) sqlite3_finalize(stmt); }
        sqlite3_stmt** operator&() { return &stmt; }
        operator sqlite3_stmt*() const { return stmt; }
    };
}

Database::Database(std::shared_ptr<Logger> log) : logger_(log) {
    for (size_t i = 0; i < pool_size_; ++i) {
        sqlite3* db = nullptr;
        if (sqlite3_open("chat.db", &db)) {
            logger_->log(Logger::Level::ERROR, "DB", "Can't open database: " + std::string(sqlite3_errmsg(db)));
        } else {
            sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
            sqlite3_busy_timeout(db, 5000); // 5s timeout
            pool_.push_back(db);
        }
    }
    logger_->log(Logger::Level::INFO, "DB", "Opened database pool size: " + std::to_string(pool_.size()));
    init();
}

Database::~Database() {
    thread_pool_.join();
    for (auto db : pool_) {
        sqlite3_close(db);
    }
}

sqlite3* Database::checkout() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]{ return !pool_.empty(); });
    sqlite3* db = pool_.back();
    pool_.pop_back();
    return db;
}

void Database::release(sqlite3* db) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(db);
    }
    cv_.notify_one();
}

void Database::init() {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();

    const char* sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT UNIQUE NOT NULL, "
        "password TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "sender_id INTEGER,"
        "sender TEXT NOT NULL, "
        "content TEXT NOT NULL, "
        "timestamp TEXT DEFAULT CURRENT_TIMESTAMP);";

    char* errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        logger_->log(Logger::Level::ERROR, "DB", "SQL error: " + std::string(errMsg));
        sqlite3_free(errMsg);
    }

    // Schema Migrations
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN bio TEXT DEFAULT '';", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN status TEXT DEFAULT 'Online';", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE messages ADD COLUMN sender_id INTEGER DEFAULT 0;", 0, 0, 0);

    // New Tables for WhatsApp-like features
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS groups (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, created_by TEXT NOT NULL);", 0, 0, 0);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS group_members (group_id INTEGER, username TEXT, joined_at TEXT DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY(group_id, username));", 0, 0, 0);
    
    // Add missing columns for messages (Group support & DMs)
    // Note: SQLite ALTER TABLE is limited, so we do one by one and ignore errors if they exist
    sqlite3_exec(db, "ALTER TABLE messages ADD COLUMN group_id INTEGER DEFAULT NULL;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE messages ADD COLUMN recipient TEXT DEFAULT NULL;", 0, 0, 0);
}

bool Database::create_user(const std::string& username, const std::string& password) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "INSERT INTO users (username, password) VALUES (?, ?);";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    return (sqlite3_step(stmt) == SQLITE_DONE);
}

bool Database::verify_user(const std::string& username, const std::string& password) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?;";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    return (sqlite3_step(stmt) == SQLITE_ROW);
}

nlohmann::json Database::get_user_profile(const std::string& username) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json profile;
    std::string sql = "SELECT bio, status FROM users WHERE username = ?;";
    StatementGuard stmt;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            profile["username"] = username;
            profile["bio"] = bio ? bio : "";
            profile["status"] = status ? status : "";
        }
    }
    return profile;
}

bool Database::update_user_profile(const std::string& username, const std::string& bio, const std::string& status) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "UPDATE users SET bio = ?, status = ? WHERE username = ?;";
    StatementGuard stmt;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, bio.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_STATIC);

    return (sqlite3_step(stmt) == SQLITE_DONE);
}

void Database::add_message_json(const std::string& sender, const std::string& content) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "INSERT INTO messages (sender, content) VALUES (?, ?);";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
}

nlohmann::json Database::get_all_messages(int limit) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json root;
    nlohmann::json messages = nlohmann::json::array();
    
    std::string sql = "SELECT sender, content, timestamp FROM messages ORDER BY id DESC LIMIT ?;";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json msg;
            msg["sender"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            msg["content"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg["timestamp"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            messages.push_back(msg);
        }
    }
    
    root["messages"] = messages;
    return root; 
}

nlohmann::json Database::get_all_users() {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json root;
    nlohmann::json users = nlohmann::json::array();
    
    std::string sql = "SELECT username, status FROM users;";
    StatementGuard stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json u;
            u["username"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            u["status"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            users.push_back(u);
        }
    }
    root["users"] = users;
    return root;
}


void Database::async_create_user(std::string username, std::string password, std::function<void(bool)> cb) {
    boost::asio::post(thread_pool_, [this, u=std::move(username), p=std::move(password), cb=std::move(cb)]() {
        bool res = create_user(u, p);
        if(cb) cb(res);
    });
}

void Database::async_verify_user(std::string username, std::string password, std::function<void(bool)> cb) {
    boost::asio::post(thread_pool_, [this, u=std::move(username), p=std::move(password), cb=std::move(cb)]() {
        bool res = verify_user(u, p);
        if(cb) cb(res);
    });
}

void Database::async_get_messages(int limit, std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, limit, cb=std::move(cb)]() {
        auto res = get_all_messages(limit);
        if(cb) cb(std::move(res));
    });
}

void Database::async_get_users(std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, cb=std::move(cb)]() {
        auto res = get_all_users();
        if(cb) cb(std::move(res));
    });
}

void Database::async_add_message(std::string sender, std::string content) {
    boost::asio::post(thread_pool_, [this, s=std::move(sender), c=std::move(content)]() {
        add_message_json(s, c);
    });
}

void Database::async_get_profile(std::string username, std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, u=std::move(username), cb=std::move(cb)]() {
        auto res = get_user_profile(u);
        if(cb) cb(std::move(res));
    });
}

void Database::async_update_profile(std::string username, std::string bio, std::string status, std::function<void(bool)> cb) {
    boost::asio::post(thread_pool_, [this, u=std::move(username), b=std::move(bio), s=std::move(status), cb=std::move(cb)]() {
        bool res = update_user_profile(u, b, s);
        if(cb) cb(res);
    });
}

// =======================
//   New Group & DM Logic
// =======================

int Database::create_group(const std::string& name, const std::string& created_by) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    
    // 1. Create Group
    std::string sql = "INSERT INTO groups (name, created_by) VALUES (?, ?);";
    StatementGuard stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, created_by.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int group_id = static_cast<int>(sqlite3_last_insert_rowid(db));
        // 2. Add Creator as Member
        join_group(group_id, created_by);
        return group_id;
    }
    return -1;
}

bool Database::join_group(int group_id, const std::string& username) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "INSERT OR IGNORE INTO group_members (group_id, username) VALUES (?, ?);";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
    
    return (sqlite3_step(stmt) == SQLITE_DONE);
}

nlohmann::json Database::get_user_groups(const std::string& username) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json groups = nlohmann::json::array();
    
    std::string sql = "SELECT g.id, g.name FROM groups g JOIN group_members gm ON g.id = gm.group_id WHERE gm.username = ?;";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json g;
            g["id"] = sqlite3_column_int(stmt, 0);
            g["name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            groups.push_back(g);
        }
    }
    return groups;
}

std::vector<std::string> Database::get_group_members(int group_id) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::vector<std::string> members;
    std::string sql = "SELECT username FROM group_members WHERE group_id = ?;";
    StatementGuard stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, group_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            members.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
    }
    return members;
}

nlohmann::json Database::get_group_messages(int group_id, int limit) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json out;
    nlohmann::json msgs = nlohmann::json::array();
    
    std::string sql = "SELECT sender, content, timestamp FROM messages WHERE group_id = ? ORDER BY id DESC LIMIT ?;";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_int(stmt, 2, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json m;
            m["sender"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            m["content"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            m["timestamp"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            msgs.push_back(m);
        }
    }
    // Reverse for UI
    std::reverse(msgs.begin(), msgs.end());
    out["messages"] = msgs;
    return out;
}

nlohmann::json Database::get_dms(const std::string& user1, const std::string& user2, int limit) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json out;
    nlohmann::json msgs = nlohmann::json::array();
    
    // Get messages between two users (either direction)
    std::string sql = "SELECT sender, content, timestamp FROM messages WHERE (sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?) ORDER BY id DESC LIMIT ?;";
    StatementGuard stmt;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, user1.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user2.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user2.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user1.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json m;
            m["sender"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            m["content"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            m["timestamp"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            msgs.push_back(m);
        }
    }
    std::reverse(msgs.begin(), msgs.end());
    out["messages"] = msgs;
    return out;
}

nlohmann::json Database::get_recent_chats(const std::string& username) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    nlohmann::json out;
    nlohmann::json chats = nlohmann::json::array();
    
    // 1. Get Groups
    auto user_groups = get_user_groups(username);
    for(auto& g : user_groups) {
        g["type"] = "group";
        chats.push_back(g);
    }
    
    // 2. Get DM contacts (users we have chatted with)
    // Complex query: find unique users from messages where we are sender or recipient, exclude ourself
    std::string sql = "SELECT DISTINCT other_user FROM ("
                      "SELECT recipient as other_user FROM messages WHERE sender = ? AND recipient IS NOT NULL "
                      "UNION "
                      "SELECT sender as other_user FROM messages WHERE recipient = ?"
                      ");";
                      
    StatementGuard stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json c;
            c["type"] = "dm";
            c["name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            chats.push_back(c);
        }
    }
    
    out["chats"] = chats;
    return out;
}

void Database::add_group_message(int group_id, const std::string& sender, const std::string& content) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "INSERT INTO messages (group_id, sender, content) VALUES (?, ?, ?);";
    StatementGuard stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_text(stmt, 2, sender.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
}

void Database::add_dm_message(const std::string& sender, const std::string& recipient, const std::string& content) {
    ConnectionGuard guard(*this);
    sqlite3* db = guard.get();
    std::string sql = "INSERT INTO messages (sender, recipient, content) VALUES (?, ?, ?);";
    StatementGuard stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, recipient.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
}

// Async Implementations

void Database::async_create_group(std::string name, std::string created_by, std::function<void(int)> cb) {
    boost::asio::post(thread_pool_, [this, n=std::move(name), c=std::move(created_by), cb=std::move(cb)]() {
        int id = create_group(n, c);
        if(cb) cb(id);
    });
}

void Database::async_join_group(int group_id, std::string username, std::function<void(bool)> cb) {
    boost::asio::post(thread_pool_, [this, group_id, u=std::move(username), cb=std::move(cb)]() {
        bool res = join_group(group_id, u);
        if(cb) cb(res);
    });
}

void Database::async_get_user_groups(std::string username, std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, u=std::move(username), cb=std::move(cb)]() {
        auto res = get_user_groups(u);
        if(cb) cb(std::move(res));
    });
}

void Database::async_get_group_messages(int group_id, std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, group_id, cb=std::move(cb)]() {
        auto res = get_group_messages(group_id);
        if(cb) cb(std::move(res));
    });
}

void Database::async_get_dms(std::string user1, std::string user2, std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, u1=std::move(user1), u2=std::move(user2), cb=std::move(cb)]() {
        auto res = get_dms(u1, u2);
        if(cb) cb(std::move(res));
    });
}

void Database::async_get_recent_chats(std::string username, std::function<void(nlohmann::json)> cb) {
    boost::asio::post(thread_pool_, [this, u=std::move(username), cb=std::move(cb)]() {
        auto res = get_recent_chats(u);
        if(cb) cb(std::move(res));
    });
}

