import { useState, useEffect } from 'react';
import type { BrainDetail } from '../../types';
import { getBrainDetail, resizeBrain } from '../../services/brainApi';

interface Props {
  brainId: number;
  onClose: () => void;
}

function LossSparkline({ data }: { data: number[] }) {
  if (data.length < 2) return null;
  const max = Math.max(...data, 0.001);
  const min = Math.min(...data);
  const h = 48;
  const w = 200;
  const points = data.map((v, i) => {
    const x = (i / (data.length - 1)) * w;
    const y = h - ((v - min) / (max - min || 1)) * h;
    return `${x},${y}`;
  }).join(' ');

  return (
    <svg width={w} height={h} style={{ display: 'block', marginTop: 4 }}>
      <polyline
        points={points}
        fill="none"
        stroke="var(--accent)"
        strokeWidth="1.5"
      />
    </svg>
  );
}

// Log scale: slider 0-1000 maps to 10-500000
function sliderToNeurons(v: number): number {
  const min = Math.log(10);
  const max = Math.log(500000);
  return Math.round(Math.exp(min + (v / 1000) * (max - min)));
}
function neuronsToSlider(n: number): number {
  const min = Math.log(10);
  const max = Math.log(500000);
  return Math.round(((Math.log(Math.max(n, 10)) - min) / (max - min)) * 1000);
}

export function BrainDetailModal({ brainId, onClose }: Props) {
  const [detail, setDetail] = useState<BrainDetail | null>(null);
  const [error, setError] = useState('');
  const [resizeTarget, setResizeTarget] = useState<number | null>(null);
  const [resizing, setResizing] = useState(false);
  const [resizeError, setResizeError] = useState('');

  const loadDetail = () => {
    getBrainDetail(brainId)
      .then(r => {
        setDetail(r.data);
        setResizeTarget(null);
      })
      .catch(() => setError('Failed to load brain details'));
  };

  useEffect(() => { loadDetail(); }, [brainId]);

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [onClose]);

  const currentNeurons = detail?.probe?.num_neurons ?? 0;

  const handleResize = async () => {
    if (resizeTarget === null || resizeTarget === currentNeurons) return;
    setResizing(true);
    setResizeError('');
    try {
      await resizeBrain(brainId, resizeTarget);
      loadDetail();
    } catch {
      setResizeError('Resize failed');
    } finally {
      setResizing(false);
    }
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()} style={{ maxWidth: 480, maxHeight: '80vh', overflowY: 'auto' }}>
        <div className="modal-title" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          Brain Details
          <button
            type="button"
            onClick={onClose}
            style={{
              background: 'none', border: 'none', color: 'var(--text-muted)',
              fontSize: 18, cursor: 'pointer', padding: '0 2px', lineHeight: 1,
            }}
            aria-label="Close"
          >&times;</button>
        </div>
        {error && <div style={{ color: 'var(--error)', marginBottom: 12 }}>{error}</div>}
        {!detail && !error && <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-muted)' }}>Loading...</div>}
        {detail && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
            <section>
              <h4 style={{ margin: '0 0 6px', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Identity</h4>
              <div className="detail-grid">
                <span>Name</span><span>{detail.name}</span>
                <span>ID</span><span>{detail.id}</span>
                <span>Created</span><span>{new Date(detail.created_at).toLocaleString()}</span>
              </div>
            </section>

            <section>
              <h4 style={{ margin: '0 0 6px', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Configuration</h4>
              <div className="detail-grid">
                <span>Size</span><span>{detail.size_label}</span>
                <span>Task</span><span>{detail.task_label}</span>
                <span>Inputs</span><span>{detail.num_inputs}</span>
                <span>Outputs</span><span>{detail.num_outputs}</span>
                {detail.probe && <><span>Neurons</span><span>{detail.probe.num_neurons.toLocaleString()}</span></>}
              </div>
            </section>

            <section>
              <h4 style={{ margin: '0 0 6px', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Resize</h4>
              <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                <input
                  type="range" min={0} max={1000}
                  value={neuronsToSlider(resizeTarget ?? currentNeurons)}
                  onChange={(e) => setResizeTarget(sliderToNeurons(Number(e.target.value)))}
                  style={{ flex: 1 }}
                />
                <input
                  type="number" min={10} max={500000}
                  value={resizeTarget ?? currentNeurons}
                  onChange={(e) => {
                    const v = Number(e.target.value);
                    if (v >= 10 && v <= 500000) setResizeTarget(v);
                  }}
                  style={{ width: 80 }}
                />
                <button
                  type="button"
                  className="btn btn-primary"
                  style={{ padding: '4px 12px', fontSize: 12 }}
                  disabled={resizing || resizeTarget === null || resizeTarget === currentNeurons}
                  onClick={handleResize}
                >
                  {resizing ? 'Resizing...' : 'Apply'}
                </button>
              </div>
              {resizeTarget !== null && resizeTarget !== currentNeurons && (
                <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 4 }}>
                  {currentNeurons.toLocaleString()} → {resizeTarget.toLocaleString()} neurons
                </div>
              )}
              {resizeError && <div style={{ fontSize: 11, color: 'var(--error)', marginTop: 4 }}>{resizeError}</div>}
            </section>

            <section>
              <h4 style={{ margin: '0 0 6px', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Statistics</h4>
              <div className="detail-grid">
                <span>Inferences</span><span>{detail.total_inferences.toLocaleString()}</span>
                <span>Learning Steps</span><span>{detail.total_learning_steps.toLocaleString()}</span>
                <span>Accuracy</span><span>{(detail.accuracy * 100).toFixed(1)}%</span>
                <span>Last Loss</span><span>{detail.last_loss.toFixed(4)}</span>
              </div>
            </section>

            {detail.loss_history.length > 1 && (
              <section>
                <h4 style={{ margin: '0 0 6px', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Loss Trend</h4>
                <LossSparkline data={detail.loss_history} />
              </section>
            )}

            {(detail.dataset || detail.parent_id !== null) && (
              <section>
                <h4 style={{ margin: '0 0 6px', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Lineage</h4>
                <div className="detail-grid">
                  {detail.dataset && <><span>Dataset</span><span>{detail.dataset}</span></>}
                  {detail.parent_id !== null && <><span>Parent Brain</span><span>#{detail.parent_id}</span></>}
                </div>
              </section>
            )}
          </div>
        )}
        <div className="modal-actions" style={{ marginTop: 16 }}>
          <button className="btn btn-outline" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
}
