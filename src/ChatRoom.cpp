#include "ChatRoom.hpp"
#include "WSSession.hpp"
#include <sstream>
#include <nlohmann/json.hpp>
#include <algorithm>

ChatRoom::ChatRoom(std::shared_ptr<Database> db) : db_(db) {}

void ChatRoom::join(WSSession* s) { 
    std::lock_guard<std::mutex> lock(mutex_); 
    sessions_.insert(s);
}

void ChatRoom::leave(WSSession* s) { 
    std::lock_guard<std::mutex> lock(mutex_); 
    sessions_.erase(s);
    if (session_user_.count(s)) {
        std::string username = session_user_[s];
        online_users_.erase(username);
        session_user_.erase(s);
    }
    
    // Remove from wait queue
    auto it = std::find(waiting_queue_.begin(), waiting_queue_.end(), s);
    if(it != waiting_queue_.end()) {
        waiting_queue_.erase(it);
    }

    disconnect_partner_locked(s);
}

void ChatRoom::handle_message(WSSession* s, std::string msg) {
    try {
        auto j = nlohmann::json::parse(msg);
        if(!j.contains("action")) return;
        std::string action = j["action"];

        if(action == "login") {
            std::string username = j["username"];
            std::lock_guard<std::mutex> lock(mutex_); 
            online_users_[username] = s;
            session_user_[s] = username;
            
            // Send confirmation
             nlohmann::json out;
             out["type"] = "login_success";
             out["username"] = username;
             auto ss = std::make_shared<std::string const>(out.dump());
             s->send(ss);
             return;
        }

        // Feature: Direct Message
        if(action == "send_dm") {
            std::string recipient = j["recipient"];
            std::string content = j["content"];
            std::string sender = "Anon";
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if(session_user_.count(s)) sender = session_user_[s];
            }
            
            // 1. Persist
            db_->add_dm_message(sender, recipient, content);
            
            // 2. Send to recipient if online
            std::lock_guard<std::mutex> lock(mutex_);
            send_to_user_locked(recipient, nlohmann::json({
                {"type", "dm"},
                {"sender", sender},
                {"content", content},
                {"timestamp", "Just now"}
            }).dump());
             
             // Echo back to sender (confirm sent)
             s->send(std::make_shared<std::string const>(nlohmann::json({
                {"type", "dm_ack"},
                {"recipient", recipient},
                {"content", content},
                {"timestamp", "Just now"}
             }).dump()));

            return;
        }

        // Feature: Group Message
        if(action == "send_group") {
            int group_id = j["group_id"];
            std::string content = j["content"];
             std::string sender = "Anon";
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if(session_user_.count(s)) sender = session_user_[s];
            }

            // 1. Persist
            db_->add_group_message(group_id, sender, content);

            // 2. Broadcast
            std::lock_guard<std::mutex> lock(mutex_);
            broadcast_to_group_locked(group_id, sender, content);
            return;
        }
        
        // Feature: Create Group
        if(action == "create_group") {
            std::string name = j["name"];
            std::string creator = "Anon";
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if(session_user_.count(s)) creator = session_user_[s];
            }
            int gid = db_->create_group(name, creator);
             nlohmann::json out;
             out["type"] = "group_created";
             out["group_id"] = gid;
             out["name"] = name;
             s->send(std::make_shared<std::string const>(out.dump()));
             return;
        }

        // Feature: Join Group
        if(action == "join_group") {
             int gid = j["group_id"];
             std::string username = "Anon";
             {
                std::lock_guard<std::mutex> lock(mutex_);
                if(session_user_.count(s)) username = session_user_[s];
             }
             bool ok = db_->join_group(gid, username);
             nlohmann::json out;
             out["type"] = "group_joined";
             out["group_id"] = gid;
             out["success"] = ok;
             s->send(std::make_shared<std::string const>(out.dump()));
             return;
        }

        // Legacy: Random Chat
        std::lock_guard<std::mutex> lock(mutex_);

        if(action == "join") {
            if(pairs_.find(s) != pairs_.end()) return;
            auto it = std::find(waiting_queue_.begin(), waiting_queue_.end(), s);
            if(it != waiting_queue_.end()) return;
            find_match_locked(s);
        }
        else if(action == "message") { // Random Chat Message
            if(pairs_.find(s) != pairs_.end()) {
                auto partner = pairs_[s];
                std::string content = j["content"];
                
                nlohmann::json out;
                out["type"] = "message";
                out["content"] = content;
                out["sender"] = "Stranger";
                
                auto ss = std::make_shared<std::string const>(out.dump());
                partner->send(ss);
            }
        }
        else if(action == "next") {
            disconnect_partner_locked(s);
            find_match_locked(s);
        }
        else if(action == "typing") {
             if(pairs_.find(s) != pairs_.end()) {
                nlohmann::json out;
                out["type"] = "typing";
                auto ss = std::make_shared<std::string const>(out.dump());
                pairs_[s]->send(ss);
             }
        }
    } catch (...) {
        // Invalid JSON or action
    }
}

void ChatRoom::find_match_locked(WSSession* s) {
    if(waiting_queue_.empty()) {
        waiting_queue_.push_back(s);
        nlohmann::json out;
        out["type"] = "status";
        out["status"] = "waiting";
        auto ss = std::make_shared<std::string const>(out.dump());
        s->send(ss);
    } else {
        auto partner = waiting_queue_.front();
        waiting_queue_.pop_front();

        if(partner == s) {
            waiting_queue_.push_back(s);
            return;
        }

        pairs_[s] = partner;
        pairs_[partner] = s;

        nlohmann::json out;
        out["type"] = "status";
        out["status"] = "connected";
        auto ss = std::make_shared<std::string const>(out.dump());
        
        s->send(ss);
        partner->send(ss);
    }
}

void ChatRoom::disconnect_partner_locked(WSSession* s) {
    if(pairs_.find(s) != pairs_.end()) {
        auto partner = pairs_[s];
        pairs_.erase(s);
        pairs_.erase(partner);

        nlohmann::json out;
        out["type"] = "status";
        out["status"] = "disconnected";
        auto ss = std::make_shared<std::string const>(out.dump());
        
        partner->send(ss);
    }
}

void ChatRoom::send_to_user_locked(const std::string& username, const std::string& msg) {
    if (online_users_.count(username)) {
        auto ss = std::make_shared<std::string const>(msg);
        online_users_[username]->send(ss);
    }
}

void ChatRoom::broadcast_to_group_locked(int group_id, const std::string& sender, const std::string& msg) {
    // Note: accessing DB in lock. 
    // Ideally we cache group members, but for now we look them up.
    // However, we can't easily call db_ methods that take a lock if we already have a lock?
    // Database has its own mutex. Recursive locking? No.
    // ChatRoom mutex and Database mutex are different.
    // ChatRoom::mutex_ is held here. Database::mutex_ will be acquired in get_group_members.
    // This is fine as long as we don't have circular dependencies (Database calling ChatRoom).
    // Database never calls ChatRoom.
    
    // BUT: get_group_members is synchronous.
    auto members = db_->get_group_members(group_id);
    auto ss = std::make_shared<std::string const>(nlohmann::json({
        {"type", "group_message"},
        {"group_id", group_id},
        {"sender", sender},
        {"content", msg},
        {"timestamp", "Just now"}
    }).dump());

    for (const auto& member : members) {
        if (online_users_.count(member)) {
            online_users_[member]->send(ss);
        }
    }
}

