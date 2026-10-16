import type { AthenaStatus } from '../../types';

interface Props {
  status: AthenaStatus | null;
  onSave: () => void;
}

export function AthenaControls({ status, onSave }: Props) {
  return (
    <div className="panel" style={{ marginBottom: 16 }}>
      <div className="panel-title" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <span>Athena Controls</span>
        <div style={{ display: 'flex', gap: 8 }}>
          <button className="btn btn-sm btn-outline" onClick={onSave}>Save Checkpoint</button>
        </div>
      </div>
      {status ? (
        <div className="athena-controls-grid">
          <div className="athena-status-indicator">
            <div className={`athena-status-dot ${status.loaded ? 'online' : 'offline'}`} />
            <span>{status.loaded ? 'Online' : 'Offline'}</span>
            {status.name && <span style={{ color: 'var(--text-muted)', marginLeft: 8 }}>({status.name})</span>}
          </div>
          <div className="metrics-grid" style={{ marginTop: 8 }}>
            <div className="metric-card">
              <div className="metric-value">{(status.num_neurons || 0).toLocaleString()}</div>
              <div className="metric-label">Neurons</div>
            </div>
            <div className="metric-card">
              <div className="metric-value">{(status.total_inferences || 0).toLocaleString()}</div>
              <div className="metric-label">Inferences</div>
            </div>
            <div className="metric-card">
              <div className="metric-value">{(status.total_learning_steps || 0).toLocaleString()}</div>
              <div className="metric-label">Learning Steps</div>
            </div>
            <div className="metric-card">
              <div className="metric-value">{((status.accuracy || 0) * 100).toFixed(1)}%</div>
              <div className="metric-label">Accuracy</div>
            </div>
            <div className="metric-card">
              <div className="metric-value">{((status.memory_bytes || 0) / 1024 / 1024).toFixed(0)} MB</div>
              <div className="metric-label">Memory</div>
            </div>
          </div>
        </div>
      ) : (
        <div style={{ color: 'var(--text-muted)', padding: '12px 0' }}>Loading status...</div>
      )}
    </div>
  );
}
