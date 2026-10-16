import { useState } from 'react';
import { login } from '../../services/authApi';
import { PasswordInput } from './PasswordInput';

interface Props {
  onLogin: (username: string, password: string, role?: string) => void;
  onSwitchToRegister: () => void;
}

export function LoginPage({ onLogin, onSwitchToRegister }: Props) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!username || !password) return;
    setLoading(true);
    setError('');
    try {
      const resp = await login(username, password);
      onLogin(username, password, resp.data.role);
    } catch (err: unknown) {
      const msg = (err as { response?: { data?: { detail?: string } } })?.response?.data?.detail;
      setError(msg || 'Login failed');
    }
    setLoading(false);
  };

  return (
    <div className="auth-page">
      <form className="auth-card" onSubmit={handleSubmit}>
        <div className="auth-title">Athena</div>
        <div className="auth-subtitle">Sign in to continue</div>

        {error && <div className="auth-error">{error}</div>}

        <div className="form-group">
          <label>Username</label>
          <input
            value={username}
            onChange={e => setUsername(e.target.value)}
            placeholder="admin"
            autoFocus
          />
        </div>
        <div className="form-group">
          <label>Password</label>
          <PasswordInput value={password} onChange={setPassword} placeholder="Enter password" />
        </div>

        <button className="btn btn-primary btn-block" type="submit" disabled={loading || !username || !password}>
          {loading ? 'Signing in...' : 'Sign In'}
        </button>

        <div className="auth-footer">
          Don&apos;t have an account?{' '}
          <a href="#" onClick={e => { e.preventDefault(); onSwitchToRegister(); }}>
            Create one
          </a>
        </div>
      </form>
    </div>
  );
}
