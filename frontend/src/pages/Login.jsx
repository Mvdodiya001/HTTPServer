import React, { useState } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import '../App.css';

function Login() {
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const navigate = useNavigate();

    const handleLogin = async (e) => {
        e.preventDefault();
        try {
            const response = await fetch('/api/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password }),
            });
            const data = await response.json();

            if (response.ok) {
                // In a real app, store token: localStorage.setItem('token', data.token);
                localStorage.setItem('username', username);
                navigate('/chat');
            } else {
                alert(data.error || 'Login failed');
            }
        } catch (err) {
            alert('Network error');
        }
    };

    return (
        <div className="chat-container">
            <div className="glass-panel" style={{ maxWidth: '400px', width: '100%', padding: '2rem', display: 'flex', flexDirection: 'column', gap: '1.5rem' }}>
                <div className="header" style={{ textAlign: 'center' }}>
                    <h2 style={{ margin: 0, color: 'var(--accent-color)' }}>Welcome Back</h2>
                    <p style={{ color: 'var(--text-muted)', margin: '0.5rem 0 0 0' }}>Login to C++ Turbo Chat</p>
                </div>

                <form onSubmit={handleLogin} style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
                    <div>
                        <input
                            type="text"
                            placeholder="Username"
                            value={username}
                            onChange={(e) => setUsername(e.target.value)}
                            required
                            style={{ width: '100%', padding: '0.8rem', borderRadius: '4px', border: '1px solid rgba(255,255,255,0.1)', background: 'rgba(0,0,0,0.2)', color: 'white' }}
                        />
                    </div>
                    <div>
                        <input
                            type="password"
                            placeholder="Password"
                            value={password}
                            onChange={(e) => setPassword(e.target.value)}
                            required
                            style={{ width: '100%', padding: '0.8rem', borderRadius: '4px', border: '1px solid rgba(255,255,255,0.1)', background: 'rgba(0,0,0,0.2)', color: 'white' }}
                        />
                    </div>
                    <button type="submit" style={{ width: '100%', padding: '0.8rem', marginTop: '0.5rem' }}>LOGIN</button>
                </form>

                <div style={{ textAlign: 'center', fontSize: '0.9rem', color: 'var(--text-muted)' }}>
                    Don't have an account? <Link to="/signup" style={{ color: 'var(--accent-color)', textDecoration: 'none' }}>Sign up</Link>
                </div>
            </div>
        </div>
    );
}

export default Login;
