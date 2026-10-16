import { useEffect, useState } from 'react';
import api from '../../services/api';

export function Header() {
  const [status, setStatus] = useState<{ version: string; brain_count: number; uptime_seconds: number } | null>(null);

  useEffect(() => {
    const poll = () => api.get('/system/status').then(r => setStatus(r.data)).catch(() => {});
    poll();
    const id = setInterval(poll, 5000);
    return () => clearInterval(id);
  }, []);

  return (
    <header className="header">
      <div className="header-title">NIMCP Dashboard</div>
      <div className="header-status">
        <span>
          <span className={`status-dot ${status ? 'ok' : 'err'}`} />
          {status ? 'Connected' : 'Disconnected'}
        </span>
        {status && (
          <>
            <span>Brains: {status.brain_count}</span>
            <span>v{status.version}</span>
          </>
        )}
      </div>
    </header>
  );
}
