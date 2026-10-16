import { useState, useEffect } from 'react';
import { createSnapshot, listSnapshots, restoreSnapshot, deleteSnapshot } from '../../services/brainApi';

interface Props {
  brainId: number;
}

export function SnapshotPanel({ brainId }: Props) {
  const [snapshots, setSnapshots] = useState<{ name: string; path: string }[]>([]);
  const [newName, setNewName] = useState('');
  const [loading, setLoading] = useState(false);

  const refresh = () => {
    listSnapshots(brainId).then(r => setSnapshots(r.data)).catch(() => {});
  };

  useEffect(() => { refresh(); }, [brainId]);

  const handleSave = async () => {
    if (!newName.trim()) return;
    setLoading(true);
    await createSnapshot(brainId, newName.trim());
    setNewName('');
    refresh();
    setLoading(false);
  };

  const handleRestore = async (name: string) => {
    setLoading(true);
    await restoreSnapshot(brainId, name);
    setLoading(false);
  };

  const handleDelete = async (name: string) => {
    await deleteSnapshot(brainId, name);
    refresh();
  };

  return (
    <div className="panel" style={{ marginTop: 16 }}>
      <div className="panel-title">Snapshots</div>
      <div style={{ display: 'flex', gap: 8, marginBottom: 8 }}>
        <input
          style={{ flex: 1, padding: '6px 10px', background: 'rgba(0,0,0,0.3)', border: '1px solid var(--border)', borderRadius: 'var(--radius)', color: 'var(--text-light)', fontSize: 13 }}
          value={newName}
          onChange={e => setNewName(e.target.value)}
          placeholder="Snapshot name"
        />
        <button className="btn btn-primary btn-sm" onClick={handleSave} disabled={loading || !newName.trim()}>
          Save
        </button>
      </div>
      <div className="snapshot-list">
        {snapshots.map(s => (
          <div key={s.name} className="snapshot-item">
            <span className="snapshot-name">{s.name}</span>
            <div style={{ display: 'flex', gap: 4 }}>
              <button className="btn btn-outline btn-sm" onClick={() => handleRestore(s.name)} disabled={loading}>
                Restore
              </button>
              <button className="btn btn-danger btn-sm" onClick={() => handleDelete(s.name)}>
                Del
              </button>
            </div>
          </div>
        ))}
        {snapshots.length === 0 && (
          <div style={{ fontSize: 12, color: 'var(--text-muted)', padding: 8 }}>No snapshots</div>
        )}
      </div>
    </div>
  );
}
