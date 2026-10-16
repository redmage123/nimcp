import { useState } from 'react';
import { register } from '../../services/authApi';
import { PasswordInput } from './PasswordInput';

interface Props {
  onRegistered: (username: string, password: string) => void;
  onSwitchToLogin: () => void;
}

export function RegisterPage({ onRegistered, onSwitchToLogin }: Props) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [confirm, setConfirm] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!username || !password || !confirm) return;
    if (password !== confirm) {
      setError('Passwords do not match');
      return;
    }
    setLoading(true);
    setError('');
    try {
      await register(username, password);
      onRegistered(username, password);
    } catch (err: unknown) {
      const msg = (err as { response?: { data?: { detail?: string } } })?.response?.data?.detail;
      setError(msg || 'Registration failed');
    }
    setLoading(false);
  };

  return (
    <div className="auth-page">
      <form className="auth-card" onSubmit={handleSubmit}>
        <div className="auth-title">Create Account</div>
        <div className="auth-subtitle">Register for NIMCP Dashboard</div>

        {error && <div className="auth-error">{error}</div>}

        <div className="form-group">
          <label>Username</label>
          <input
            value={username}
            onChange={e => setUsername(e.target.value)}
            placeholder="Choose a username"
            autoFocus
          />
        </div>
        <div className="form-group">
          <label>Password</label>
          <PasswordInput value={password} onChange={setPassword} placeholder="At least 6 characters" />
        </div>
        <div className="form-group">
          <label>Confirm Password</label>
          <PasswordInput value={confirm} onChange={setConfirm} placeholder="Re-enter password" />
        </div>

        <button
          className="btn btn-primary btn-block"
          type="submit"
          disabled={loading || !username || !password || !confirm}
        >
          {loading ? 'Creating account...' : 'Create Account'}
        </button>

        <div className="auth-footer">
          Already have an account?{' '}
          <a href="#" onClick={e => { e.preventDefault(); onSwitchToLogin(); }}>
            Sign in
          </a>
        </div>
      </form>
    </div>
  );
}
