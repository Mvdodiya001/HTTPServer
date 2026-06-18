import React, { useState, useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import Sidebar from '../components/Sidebar';
import '../App.css';

function Chat() {
    const [messages, setMessages] = useState([]);
    const [input, setInput] = useState('');
    const [chats, setChats] = useState([]);
    const [activeChat, setActiveChat] = useState({ type: 'global', name: 'Global Chat' });
    const ws = useRef(null);
    const scrollRef = useRef(null);
    const navigate = useNavigate();
    const username = localStorage.getItem('username') || 'Anon';

    const fetchChats = () => {
        fetch('/api/recent_chats', {
            method: 'POST',
            body: JSON.stringify({ username })
        })
            .then(res => res.json())
            .then(data => {
                if (data.chats) setChats(data.chats);
            })
            .catch(console.error);
    };

    const fetchMessages = () => {
        let endpoint = '/api/messages'; // global
        let body = null;

        if (activeChat.type === 'group') {
            endpoint = '/api/group_messages';
            body = JSON.stringify({ group_id: activeChat.id });
        } else if (activeChat.type === 'dm') {
            endpoint = '/api/dms';
            body = JSON.stringify({ user1: username, user2: activeChat.name });
        }

        const options = body ? { method: 'POST', body } : { method: 'GET' };

        fetch(endpoint, options)
            .then(res => res.json())
            .then(data => {
                if (data.messages) {
                    setMessages(data.messages.reverse());
                } else {
                    setMessages([]);
                }
            })
            .catch(console.error);
    };

    useEffect(() => {
        fetchMessages();
    }, [activeChat]);

    useEffect(() => {
        fetchChats();

        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        ws.current = new WebSocket(`${protocol}//${window.location.host}/`);

        ws.current.onopen = () => {
            console.log("WS Connected");
            ws.current.send(JSON.stringify({ action: 'login', username }));
        };

        ws.current.onmessage = (event) => {
            try {
                const msg = JSON.parse(event.data);

                // Handle different message types
                if (msg.type === 'group_message') {
                    if (activeChat.type === 'group' && activeChat.id === msg.group_id) {
                        setMessages(prev => [...prev, msg]);
                    }
                } else if (msg.type === 'dm') {
                    if (activeChat.type === 'dm' && activeChat.name === msg.sender) {
                        setMessages(prev => [...prev, msg]);
                    }
                } else if (msg.type === 'dm_ack') {
                    if (activeChat.type === 'dm' && activeChat.name === msg.recipient) {
                        setMessages(prev => [...prev, { ...msg, sender: 'You' }]); // Optimistic update confirmation
                    }
                } else if (msg.type === 'message') {
                    // Random chat logic
                    if (activeChat.type === 'global') {
                        setMessages(prev => [...prev, msg]);
                    }
                } else if (msg.type === 'status') {
                    // Random chat status
                    if (activeChat.type === 'global') {
                        setMessages(prev => [...prev, { sender: 'System', content: `Status: ${msg.status}`, timestamp: new Date().toLocaleTimeString() }]);
                    }
                }
            } catch (e) {
                console.error(e);
            }
        };

        return () => {
            if (ws.current) ws.current.close();
        };
    }, []);

    useEffect(() => {
        if (scrollRef.current) {
            scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
        }
    }, [messages]);

    const handleSend = () => {
        if (input.trim() && ws.current) {
            let msg = {};
            if (activeChat.type === 'group') {
                msg = { action: 'send_group', group_id: activeChat.id, content: input };
            } else if (activeChat.type === 'dm') {
                msg = { action: 'send_dm', recipient: activeChat.name, content: input };
            } else {
                // Global / Random
                msg = { action: 'message', content: input };
            }

            ws.current.send(JSON.stringify(msg));
            if (activeChat.type === 'global') {
                setMessages(prev => [...prev, { sender: 'You', content: input, timestamp: new Date().toLocaleTimeString() }]);
            }
            setInput('');
        }
    };

    const createGroup = (name) => {
        fetch('/api/create_group', {
            method: 'POST',
            body: JSON.stringify({ name, username })
        }).then(() => {
            fetchChats();
        });
    };

    const joinGroup = (gid) => {
        fetch('/api/join_group', {
            method: 'POST',
            body: JSON.stringify({ group_id: parseInt(gid), username })
        }).then(() => {
            fetchChats();
        });
    };

    return (
        <div className="chat-container" style={{ display: 'flex', flexDirection: 'row' }}>
            <Sidebar
                chats={chats}
                onSelectChat={setActiveChat}
                onCreateGroup={createGroup}
                onJoinGroup={joinGroup}
                onRefresh={fetchChats}
            />

            <div className="glass-panel" style={{ flex: 1, margin: '1rem', display: 'flex', flexDirection: 'column' }}>
                <div className="header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div>
                        <h2 style={{ margin: 0, color: 'var(--accent-color)' }}>{activeChat.name}</h2>
                        <small style={{ color: 'var(--text-muted)' }}>
                            {activeChat.type === 'global' ? 'Random Pairing Mode' : (activeChat.type === 'group' ? `Group ID: ${activeChat.id}` : 'Direct Message')}
                        </small>
                    </div>
                    <div>
                        <span style={{ marginRight: '1rem' }}>Logged in as: {username}</span>
                        <button onClick={() => navigate('/profile')} style={{ padding: '0.5rem 1rem', fontSize: '0.8rem' }}>Profile</button>
                    </div>
                </div>

                <div className="messages-list" ref={scrollRef}>
                    {messages.map((msg, index) => {
                        const isMine = msg.sender === username || msg.sender === 'You';
                        return (
                            <div key={index} className={`message-bubble ${isMine ? 'message-mine' : 'message-other'}`}>
                                <span className="msg-sender" style={{ color: isMine ? '#0f172a' : 'var(--accent-color)' }}>
                                    {isMine ? 'You' : msg.sender}
                                </span>
                                <span className="msg-content">{msg.content}</span>
                                {msg.timestamp && (
                                    <span className="msg-time" style={{ color: isMine ? 'rgba(0,0,0,0.6)' : 'rgba(255,255,255,0.6)' }}>
                                        {msg.timestamp}
                                    </span>
                                )}
                            </div>
                        );
                    })}
                </div>

                <div className="input-area">
                    <input
                        value={input}
                        onChange={(e) => setInput(e.target.value)}
                        onKeyPress={(e) => e.key === 'Enter' && handleSend()}
                        placeholder={`Message ${activeChat.name}...`}
                    />
                    <button onClick={handleSend}>SEND</button>
                </div>
            </div>
        </div>
    );
}

export default Chat;
