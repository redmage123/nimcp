import { useState, useEffect, useCallback } from 'react';
import type { AuthState } from './types';
import { LoginPage } from './components/auth/LoginPage';
import { RegisterPage } from './components/auth/RegisterPage';
import { ChatLayout } from './components/chat/ChatLayout';
import { AdminLayout } from './components/admin/AdminLayout';
import api from './services/api';
import { getAuthMe } from './services/brainApi';
import './App.css';

type AuthPage = 'login' | 'register';

function applyAuth(username: string, password: string) {
  const token = btoa(`${username}:${password}`);
  api.defaults.headers.common['Authorization'] = `Basic ${token}`;
}

function loadSavedAuth(): { username: string; password: string; role?: string } | null {
  try {
    const saved = sessionStorage.getItem('nimcp_auth');
    if (saved) return JSON.parse(saved);
  } catch { /* ignore */ }
  const url = new URL(window.location.href);
  if (url.username) {
    return { username: decodeURIComponent(url.username), password: decodeURIComponent(url.password) };
  }
  return null;
}

export default function App() {
  const savedAuth = loadSavedAuth();
  const [authed, setAuthed] = useState(!!savedAuth);
  const [authState, setAuthState] = useState<AuthState | null>(
    savedAuth?.role ? { username: savedAuth.username, role: savedAuth.role as 'admin' | 'user' } : null
  );
  const [authPage, setAuthPage] = useState<AuthPage>('login');

  // Apply saved credentials on mount and fetch role if missing
  useEffect(() => {
    if (savedAuth) {
      applyAuth(savedAuth.username, savedAuth.password);
      if (!savedAuth.role) {
        // Fetch role from server
        getAuthMe().then(r => {
          const role = r.data.role as 'admin' | 'user';
          setAuthState({ username: savedAuth.username, role });
          sessionStorage.setItem('nimcp_auth', JSON.stringify({
            ...savedAuth, role,
          }));
        }).catch(() => {
          // If /me fails, assume user role
          setAuthState({ username: savedAuth.username, role: 'user' });
        });
      }
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const handleLogin = useCallback((username: string, password: string, role?: string) => {
    applyAuth(username, password);
    const userRole = (role || 'user') as 'admin' | 'user';
    sessionStorage.setItem('nimcp_auth', JSON.stringify({ username, password, role: userRole }));
    setAuthState({ username, role: userRole });
    setAuthed(true);
  }, []);

  const handleLogout = useCallback(() => {
    sessionStorage.removeItem('nimcp_auth');
    delete api.defaults.headers.common['Authorization'];
    setAuthed(false);
    setAuthState(null);
    setAuthPage('login');
  }, []);

  if (!authed || !authState) {
    if (authPage === 'register') {
      return (
        <RegisterPage
          onRegistered={(u, p) => handleLogin(u, p, 'user')}
          onSwitchToLogin={() => setAuthPage('login')}
        />
      );
    }
    return (
      <LoginPage
        onLogin={handleLogin}
        onSwitchToRegister={() => setAuthPage('register')}
      />
    );
  }

  if (authState.role === 'admin') {
    return <AdminLayout auth={authState} onLogout={handleLogout} />;
  }

  return <ChatLayout auth={authState} onLogout={handleLogout} />;
}
