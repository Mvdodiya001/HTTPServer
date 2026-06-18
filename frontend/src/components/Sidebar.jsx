import React, { useState } from 'react';

function Sidebar({ chats, onSelectChat, onCreateGroup, onJoinGroup, onRefresh }) {
    const [groupName, setGroupName] = useState('');
    const [joinId, setJoinId] = useState('');
    const [showCreate, setShowCreate] = useState(false);
    const [showJoin, setShowJoin] = useState(false);

    return (
        <div className="sidebar" style={{ width: '250px', background: 'rgba(0,0,0,0.2)', padding: '1rem', borderRight: '1px solid rgba(255,255,255,0.1)', display: 'flex', flexDirection: 'column' }}>
            <h3 style={{ color: 'var(--accent-color)' }}>Chats</h3>

            <div className="chat-list" style={{ flex: 1, overflowY: 'auto' }}>
                <div
                    className="chat-item"
                    onClick={() => onSelectChat({ type: 'global', name: 'Global Chat' })}
                    style={{ padding: '10px', cursor: 'pointer', marginBottom: '5px', borderRadius: '5px' }}
                >
                    <span style={{ color: '#fff' }}>🌍 Global Chat (Random)</span>
                </div>

                {chats.map((chat, idx) => (
                    <div
                        key={idx}
                        className="chat-item"
                        onClick={() => onSelectChat(chat)}
                        style={{ padding: '10px', cursor: 'pointer', marginBottom: '5px', borderRadius: '5px', background: 'rgba(255,255,255,0.05)' }}
                    >
                        <span style={{ color: '#fff' }}>
                            {chat.type === 'group' ? '👥 ' : '👤 '}
                            {chat.name}
                        </span>
                    </div>
                ))}
            </div>

            <div className="sidebar-actions" style={{ marginTop: 'auto', display: 'flex', flexDirection: 'column', gap: '0.5rem' }}>
                <button onClick={() => setShowCreate(!showCreate)}>+ New Group</button>
                {showCreate && (
                    <div className="modal-mini">
                        <input
                            placeholder="Group Name"
                            value={groupName}
                            onChange={e => setGroupName(e.target.value)}
                            style={{ width: '100%', marginBottom: '5px' }}
                        />
                        <button onClick={() => { onCreateGroup(groupName); setGroupName(''); setShowCreate(false); }}>Create</button>
                    </div>
                )}

                <button onClick={() => setShowJoin(!showJoin)}>→ Join Group</button>
                {showJoin && (
                    <div className="modal-mini">
                        <input
                            placeholder="Group ID"
                            value={joinId}
                            onChange={e => setJoinId(e.target.value)}
                            style={{ width: '100%', marginBottom: '5px' }}
                        />
                        <button onClick={() => { onJoinGroup(joinId); setJoinId(''); setShowJoin(false); }}>Join</button>
                    </div>
                )}
                <button onClick={onRefresh} style={{ background: 'transparent', border: '1px solid #666' }}>↻ Refresh</button>
            </div>
        </div>
    );
}

export default Sidebar;
