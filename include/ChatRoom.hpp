#pragma once

#include <mutex>
#include <set>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include "Database.hpp"

class WSSession; // Forward declaration

class ChatRoom {
    std::mutex mutex_;
    std::set<WSSession*> sessions_; // All connected sessions
    std::deque<WSSession*> waiting_queue_; // Users waiting for a match
    std::map<WSSession*, WSSession*> pairs_; // Active 1-on-1 pairs
    std::map<std::string, WSSession*> online_users_; // Username -> Session
    std::map<WSSession*, std::string> session_user_; // Session -> Username
    std::shared_ptr<Database> db_;

public:
    ChatRoom(std::shared_ptr<Database> db);
    void join(WSSession* s);
    void leave(WSSession* s);
    void handle_message(WSSession* s, std::string msg);
    
private:
    void find_match_locked(WSSession* s);
    void disconnect_partner_locked(WSSession* s);
    void send_to_user_locked(const std::string& username, const std::string& msg);
    void broadcast_to_group_locked(int group_id, const std::string& sender, const std::string& msg);
};
