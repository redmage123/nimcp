import { useState, useEffect } from 'react';
import type { AthenaStatus, ProbeConfig, BrainInfo } from '../../types';
import * as brainApi from '../../services/brainApi';
import { AthenaControls } from './AthenaControls';
import { ProbeCard } from './ProbeCard';
import { ProbeBuilder } from './ProbeBuilder';

interface Props {
  brainId: number | null;
}

export function AdminDashboard({ brainId }: Props) {
  const [athenaStatus, setAthenaStatus] = useState<AthenaStatus | null>(null);
  const [users, setUsers] = useState<{ username: string; role: string }[]>([]);
  const [probes, setProbes] = useState<ProbeConfig[]>([]);
  const [brains, setBrains] = useState<BrainInfo[]>([]);
  const [showProbeBuilder, setShowProbeBuilder] = useState(false);

  useEffect(() => {
    brainApi.getAthenaStatus().then(r => setAthenaStatus(r.data)).catch(() => {});
    brainApi.listUsers().then(r => setUsers(r.data)).catch(() => {});
    brainApi.listProbeConfigs().then(r => setProbes(r.data)).catch(() => {});
    brainApi.listBrains().then(r => setBrains(r.data)).catch(() => {});
    const id = setInterval(() => {
      brainApi.getAthenaStatus().then(r => setAthenaStatus(r.data)).catch(() => {});
    }, 5000);
    return () => clearInterval(id);
  }, []);

  const handleRoleChange = async (username: string, role: string) => {
    try {
      await brainApi.updateUserRole(username, role);
      brainApi.listUsers().then(r => setUsers(r.data)).catch(() => {});
    } catch { /* */ }
  };

  const handleSaveAthena = async () => {
    try {
      await brainApi.saveAthena();
    } catch { /* */ }
  };

  const handleSaveProbe = async (config: Partial<ProbeConfig>) => {
    try {
      const resp = await brainApi.saveProbeConfig(config);
      setProbes(prev => {
        const idx = prev.findIndex(p => p.id === resp.data.id);
        if (idx >= 0) {
          const next = [...prev];
          next[idx] = resp.data;
          return next;
        }
        return [...prev, resp.data];
      });
      setShowProbeBuilder(false);
    } catch { /* */ }
  };

  const handleDeleteProbe = async (probeId: string) => {
    try {
      await brainApi.deleteProbeConfig(probeId);
      setProbes(prev => prev.filter(p => p.id !== probeId));
    } catch { /* */ }
  };

  return (
    <div className="admin-dashboard">
      <AthenaControls status={athenaStatus} onSave={handleSaveAthena} />

      <div className="panel" style={{ marginBottom: 16 }}>
        <div className="panel-title" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span>Custom Probes ({probes.length})</span>
          <button className="btn btn-sm btn-primary" onClick={() => setShowProbeBuilder(true)}>+ New Probe</button>
        </div>
        {probes.length === 0 ? (
          <div style={{ color: 'var(--text-muted)', padding: '16px 0' }}>
            No probes configured. Create one to monitor specific brain metrics in real-time.
          </div>
        ) : (
          <div className="probe-grid">
            {probes.map(p => (
              <ProbeCard
                key={p.id}
                config={p}
                brains={brains}
                onUpdate={handleSaveProbe}
                onDelete={handleDeleteProbe}
              />
            ))}
          </div>
        )}
      </div>

      <div className="panel">
        <div className="panel-title">Users ({users.length})</div>
        <div className="admin-user-list">
          {users.map(u => (
            <div key={u.username} className="admin-user-row">
              <span className="admin-user-name">{u.username}</span>
              <select
                value={u.role}
                onChange={e => handleRoleChange(u.username, e.target.value)}
                className="admin-role-select"
              >
                <option value="user">User</option>
                <option value="admin">Admin</option>
              </select>
            </div>
          ))}
        </div>
      </div>

      {showProbeBuilder && (
        <ProbeBuilder
          brains={brains}
          onSave={handleSaveProbe}
          onClose={() => setShowProbeBuilder(false)}
        />
      )}
    </div>
  );
}
