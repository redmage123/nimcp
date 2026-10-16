import { useState, useEffect } from 'react';
import type { BrainCreate } from '../../types';

interface Props {
  onClose: () => void;
  onCreate: (data: BrainCreate) => void;
}

const SIZE_PRESETS = [
  { label: 'Tiny', neurons: 100 },
  { label: 'Small', neurons: 1000 },
  { label: 'Medium', neurons: 10000 },
  { label: 'Large', neurons: 100000 },
  { label: 'Custom', neurons: 0 },
];
const TASKS = ['Classification', 'Regression', 'Pattern Matching', 'Sequence', 'Association'];

function formatNeurons(n: number): string {
  if (n >= 1000) return `${(n / 1000).toFixed(n % 1000 === 0 ? 0 : 1)}K`;
  return String(n);
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
  return Math.round(((Math.log(n) - min) / (max - min)) * 1000);
}

export function BrainCreateModal({ onClose, onCreate }: Props) {
  const [name, setName] = useState('brain');
  const [sizeIdx, setSizeIdx] = useState(1);
  const [customNeurons, setCustomNeurons] = useState(5000);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [task, setTask] = useState<number | undefined>(undefined);
  const [numInputs, setNumInputs] = useState<number | undefined>(undefined);
  const [numOutputs, setNumOutputs] = useState<number | undefined>(undefined);

  const isCustom = sizeIdx === 4;
  const neuronCount = isCustom ? customNeurons : SIZE_PRESETS[sizeIdx].neurons;
  // Map to closest size enum for C API (0-3)
  const sizeEnum = isCustom ? (customNeurons <= 500 ? 0 : customNeurons <= 5000 ? 1 : customNeurons <= 50000 ? 2 : 3) : sizeIdx;

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const data: BrainCreate = { name, size: sizeEnum };
    if (isCustom) data.num_neurons = customNeurons;
    if (showAdvanced) {
      if (task !== undefined) data.task = task;
      if (numInputs !== undefined) data.num_inputs = numInputs;
      if (numOutputs !== undefined) data.num_outputs = numOutputs;
    }
    onCreate(data);
  };

  const isAutoDetect = !showAdvanced || (task === undefined && numInputs === undefined && numOutputs === undefined);

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [onClose]);

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()} style={{ maxHeight: '80vh', overflowY: 'auto' }}>
        <div className="modal-title" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          Create Brain
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
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label>Name</label>
            <input value={name} onChange={(e) => setName(e.target.value)} required />
          </div>
          <div className="form-group">
            <label>Size</label>
            <select value={sizeIdx} onChange={(e) => setSizeIdx(Number(e.target.value))}>
              {SIZE_PRESETS.map((s, i) => (
                <option key={i} value={i}>
                  {s.neurons > 0 ? `${s.label} (${formatNeurons(s.neurons)} neurons)` : s.label}
                </option>
              ))}
            </select>
          </div>
          {isCustom && (
            <div className="form-group">
              <label style={{ display: 'flex', justifyContent: 'space-between' }}>
                Neurons
                <span style={{ fontWeight: 'normal', color: 'var(--text-muted)', fontSize: 12 }}>
                  {neuronCount.toLocaleString()}
                </span>
              </label>
              <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                <input
                  type="range" min={0} max={1000}
                  value={neuronsToSlider(customNeurons)}
                  onChange={(e) => setCustomNeurons(sliderToNeurons(Number(e.target.value)))}
                  style={{ flex: 1 }}
                />
                <input
                  type="number" min={10} max={500000}
                  value={customNeurons}
                  onChange={(e) => {
                    const v = Number(e.target.value);
                    if (v >= 10 && v <= 500000) setCustomNeurons(v);
                  }}
                  style={{ width: 80 }}
                />
              </div>
            </div>
          )}
          {isAutoDetect && (
            <div style={{ padding: '8px 12px', marginBottom: 12, background: 'rgba(99,102,241,0.1)', borderRadius: 6, fontSize: 12, color: 'var(--text-muted)' }}>
              Conversational brain — ready to chat, learn, and grow
            </div>
          )}
          <div style={{ marginBottom: 12 }}>
            <button
              type="button"
              style={{
                background: 'none', border: 'none', color: 'var(--accent)',
                fontSize: 12, cursor: 'pointer', padding: 0,
                display: 'flex', alignItems: 'center', gap: 4,
              }}
              onClick={() => setShowAdvanced(!showAdvanced)}
            >
              <span style={{ transform: showAdvanced ? 'rotate(90deg)' : 'rotate(0)', transition: 'transform 0.2s', display: 'inline-block' }}>&#9654;</span>
              Advanced
            </button>
          </div>
          {showAdvanced && (
            <>
              <div className="form-group">
                <label>Task</label>
                <select
                  value={task ?? ''}
                  onChange={(e) => setTask(e.target.value === '' ? undefined : Number(e.target.value))}
                >
                  <option value="">Auto-detect</option>
                  {TASKS.map((t, i) => <option key={i} value={i}>{t}</option>)}
                </select>
              </div>
              <div className="form-row">
                <div className="form-group">
                  <label>Inputs</label>
                  <input
                    type="number" min={1} placeholder="Auto"
                    value={numInputs ?? ''}
                    onChange={(e) => setNumInputs(e.target.value === '' ? undefined : Number(e.target.value))}
                  />
                </div>
                <div className="form-group">
                  <label>Outputs</label>
                  <input
                    type="number" min={1} placeholder="Auto"
                    value={numOutputs ?? ''}
                    onChange={(e) => setNumOutputs(e.target.value === '' ? undefined : Number(e.target.value))}
                  />
                </div>
              </div>
            </>
          )}
          {neuronCount >= 10000 && (
            <div style={{ padding: '8px 12px', marginBottom: 12, background: 'rgba(245,158,11,0.15)', borderRadius: 6, fontSize: 12, color: 'var(--warning)' }}>
              Warning: {formatNeurons(neuronCount)} neurons allocates {neuronCount >= 100000 ? '~500MB+' : '~50MB'} RAM
            </div>
          )}
          <div className="modal-actions">
            <button type="button" className="btn btn-outline" onClick={onClose}>Cancel</button>
            <button type="submit" className="btn btn-primary">Create</button>
          </div>
        </form>
      </div>
    </div>
  );
}
