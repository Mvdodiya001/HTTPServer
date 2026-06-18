import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import '../App.css';

function UserProfile() {
    const navigate = useNavigate();
    const [profile, setProfile] = useState({ username: 'Loading...', bio: '', status: '' });
    const [isEditing, setIsEditing] = useState(false);
    const [editForm, setEditForm] = useState({ bio: '', status: '' });

    useEffect(() => {
        const user = localStorage.getItem('username');
        if (user) {
            fetch('/api/get_profile', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username: user })
            })
                .then(res => res.json())
                .then(data => {
                    if (data.username) {
                        setProfile(data);
                        setEditForm({ bio: data.bio, status: data.status });
                    }
                })
                .catch(err => console.error("Failed to load profile", err));
        } else {
            setProfile({ username: 'Guest', bio: 'Please login to see profile', status: 'Offline' });
        }
    }, []);

    const handleSave = async () => {
        try {
            const response = await fetch('/api/update_profile', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    username: profile.username,
                    bio: editForm.bio,
                    status: editForm.status
                })
            });
            const data = await response.json();
            if (data.status === 'success') {
                setProfile(prev => ({ ...prev, bio: editForm.bio, status: editForm.status }));
                setIsEditing(false);
                alert('Profile Updated!');
            } else {
                alert('Update failed');
            }
        } catch (err) {
            alert('Network error');
        }
    };

    return (
        <div className="chat-container">
            <div className="glass-panel" style={{ padding: '2rem', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '1.5rem' }}>
                <div className="header" style={{ width: '100%', textAlign: 'center' }}>
                    <h2 style={{ margin: 0, color: 'var(--accent-color)' }}>User Profile</h2>
                </div>

                <div style={{
                    width: '100px',
                    height: '100px',
                    borderRadius: '50%',
                    backgroundColor: 'rgba(255, 255, 255, 0.1)',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '3rem',
                    border: '2px solid var(--accent-color)'
                }}>
                    👤
                </div>

                <div style={{ textAlign: 'center', width: '100%' }}>
                    <h3 style={{ margin: '0.5rem 0' }}>{profile.username}</h3>
                    {isEditing ? (
                        <input
                            type="text"
                            value={editForm.status}
                            onChange={(e) => setEditForm(prev => ({ ...prev, status: e.target.value }))}
                            placeholder="Status"
                            style={{ width: '60%', padding: '0.5rem', textAlign: 'center', background: 'rgba(0,0,0,0.3)', border: '1px solid #555', color: 'white', borderRadius: '4px' }}
                        />
                    ) : (
                        <p style={{ color: 'var(--text-muted)', margin: 0 }}>{profile.status}</p>
                    )}
                </div>

                <div style={{ width: '100%', padding: '1rem', background: 'rgba(0,0,0,0.2)', borderRadius: '8px' }}>
                    <h4 style={{ marginTop: 0 }}>Bio</h4>
                    {isEditing ? (
                        <textarea
                            value={editForm.bio}
                            onChange={(e) => setEditForm(prev => ({ ...prev, bio: e.target.value }))}
                            placeholder="Tell us about yourself..."
                            rows="4"
                            style={{ width: '100%', padding: '0.5rem', background: 'rgba(0,0,0,0.3)', border: '1px solid #555', color: 'white', borderRadius: '4px', resize: 'none' }}
                        />
                    ) : (
                        <p style={{ fontSize: '0.9rem', lineHeight: '1.4' }}>
                            {profile.bio || "No bio available."}
                        </p>
                    )}
                </div>

                <div style={{ display: 'flex', gap: '1rem', width: '100%' }}>
                    {isEditing ? (
                        <>
                            <button onClick={handleSave} style={{ flex: 1, background: '#10b981' }}>Save</button>
                            <button onClick={() => setIsEditing(false)} style={{ flex: 1, background: '#ef4444' }}>Cancel</button>
                        </>
                    ) : (
                        <button onClick={() => setIsEditing(true)} style={{ flex: 1 }}>Edit Profile</button>
                    )}
                </div>

                <button onClick={() => navigate('/')} style={{ marginTop: 'auto', width: '100%', background: 'transparent', border: '1px solid rgba(255,255,255,0.2)' }}>
                    Back to Chat
                </button>
            </div>
        </div>
    );
}

export default UserProfile;
