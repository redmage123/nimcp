import { useState } from 'react';
import type { ProbeConfig, BrainInfo } from '../../types';

interface Props {
  brains: BrainInfo[];
  initial?: ProbeConfig;
  onSave: (config: Partial<ProbeConfig>) => void;
  onClose: () => void;
}

const AVAILABLE_METRICS = [
  'num_neurons', 'num_synapses', 'accuracy', 'last_loss',
  'utilization', 'avg_sparsity', 'memory_bytes',
  'total_inferences', 'total_learning_steps',
  'avg_inference_time_us', 'current_learning_rate',
];

export function ProbeBuilder({ brains, initial, onSave, onClose }: Props) {
  const [name, setName] = useState(initial?.name || '');
  const [brainId, setBrainId] = useState(initial?.brain_id ?? 0);
  const [metrics, setMetrics] = useState<string[]>(initial?.metrics || ['accuracy', 'last_loss']);
  const [refreshMs, setRefreshMs] = useState(initial?.refresh_ms ?? 2000);
  const [chartType, setChartType] = useState<'line' | 'bar' | 'gauge'>(initial?.chart_type || 'line');
  const [alertMetric, setAlertMetric] = useState('');
  const [alertValue, setAlertValue] = useState('');
  const [thresholds, setThresholds] = useState<Record<string, number>>(initial?.alert_thresholds || {});

  const toggleMetric = (m: string) => {
    setMetrics(prev =>
      prev.includes(m) ? prev.filter(x => x !== m) : [...prev, m]
    );
  };

  const addThreshold = () => {
    if (alertMetric && alertValue) {
      setThresholds(prev => ({ ...prev, [alertMetric]: parseFloat(alertValue) }));
      setAlertMetric('');
      setAlertValue('');
    }
  };

  const removeThreshold = (key: string) => {
    setThresholds(prev => {
      const next = { ...prev };
      delete next[key];
      return next;
    });
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim() || metrics.length === 0) return;
    onSave({
      id: initial?.id,
      name: name.trim(),
      brain_id: brainId,
      metrics,
      refresh_ms: refreshMs,
      chart_type: chartType,
      alert_thresholds: Object.keys(thresholds).length > 0 ? thresholds : undefined,
    });
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={e => e.stopPropagation()} style={{ width: 500 }}>
        <div className="modal-title">{initial ? 'Edit Probe' : 'Create Probe'}</div>
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label>Name</label>
            <input value={name} onChange={e => setName(e.target.value)} placeholder="My probe" autoFocus />
          </div>

          <div className="form-row">
            <div className="form-group">
              <label>Brain</label>
              <select value={brainId} onChange={e => setBrainId(Number(e.target.value))}>
                {brains.map(b => <option key={b.id} value={b.id}>{b.name}</option>)}
              </select>
            </div>
            <div className="form-group">
              <label>Chart Type</label>
              <select value={chartType} onChange={e => setChartType(e.target.value as 'line' | 'bar' | 'gauge')}>
                <option value="line">Time Series</option>
                <option value="bar">Bar Chart</option>
                <option value="gauge">Gauge</option>
              </select>
            </div>
          </div>

          <div className="form-group">
            <label>Refresh Interval: {refreshMs}ms</label>
            <input
              type="range" min={500} max={10000} step={500}
              value={refreshMs} onChange={e => setRefreshMs(Number(e.target.value))}
              style={{ width: '100%' }}
            />
          </div>

          <div className="form-group">
            <label>Metrics</label>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6, marginTop: 4 }}>
              {AVAILABLE_METRICS.map(m => (
                <button
                  key={m}
                  type="button"
                  className={`chat-mode-btn ${metrics.includes(m) ? 'active' : ''}`}
                  onClick={() => toggleMetric(m)}
                  style={{ fontSize: 11 }}
                >
                  {m}
                </button>
              ))}
            </div>
          </div>

          <div className="form-group">
            <label>Alert Thresholds</label>
            <div style={{ display: 'flex', gap: 6, marginTop: 4 }}>
              <select value={alertMetric} onChange={e => setAlertMetric(e.target.value)} style={{ flex: 1 }}>
                <option value="">Select metric...</option>
                {metrics.map(m => <option key={m} value={m}>{m}</option>)}
              </select>
              <input
                type="number" step="any" placeholder="Threshold"
                value={alertValue} onChange={e => setAlertValue(e.target.value)}
                style={{ width: 100 }}
              />
              <button type="button" className="btn btn-sm btn-outline" onClick={addThreshold}>Add</button>
            </div>
            {Object.entries(thresholds).map(([k, v]) => (
              <div key={k} style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 4, fontSize: 12 }}>
                <span style={{ color: 'var(--text-muted)' }}>{k} &gt; {v}</span>
                <button type="button" className="conv-item-delete" onClick={() => removeThreshold(k)}>&times;</button>
              </div>
            ))}
          </div>

          <div className="modal-actions">
            <button type="button" className="btn btn-outline" onClick={onClose}>Cancel</button>
            <button type="submit" className="btn btn-primary" disabled={!name.trim() || metrics.length === 0}>
              {initial ? 'Update' : 'Create'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
