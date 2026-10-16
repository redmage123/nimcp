import { useState } from 'react';
import type { TrainingConfig } from '../../types';

const LOSS_TYPES = ['MSE', 'Cross-Entropy', 'Binary CE', 'Huber', 'MAE', 'Focal', 'KL-Div'];
const OPTIMIZERS = ['SGD', 'Momentum', 'Adam', 'AdamW', 'RMSProp', 'Adagrad'];
const SCHEDULERS = ['Constant', 'Step', 'Exponential', 'Cosine', 'WarmupCosine', 'ReducePlateau', 'Cyclic'];

interface Props {
  onConfigure: (cfg: TrainingConfig) => void;
}

export function TrainingConfigForm({ onConfigure }: Props) {
  const [cfg, setCfg] = useState<TrainingConfig>({
    loss_type: 0,
    optimizer_type: 3,
    scheduler_type: 0,
    learning_rate: 0.001,
    weight_decay: 0,
    momentum: 0.9,
    beta1: 0.9,
    beta2: 0.999,
    epsilon: 1e-8,
    scheduler_step_size: 100,
    scheduler_gamma: 0.1,
    warmup_steps: 0,
    enable_gradient_clipping: false,
    gradient_clip_value: 1.0,
    enable_biological_modulation: false,
    biological_blend: 0.5,
  });

  const set = <K extends keyof TrainingConfig>(k: K, v: TrainingConfig[K]) =>
    setCfg(prev => ({ ...prev, [k]: v }));

  return (
    <div className="panel">
      <div className="panel-title">Training Configuration</div>
      <div className="form-row">
        <div className="form-group">
          <label>Loss Function</label>
          <select value={cfg.loss_type} onChange={e => set('loss_type', Number(e.target.value))}>
            {LOSS_TYPES.map((l, i) => <option key={i} value={i}>{l}</option>)}
          </select>
        </div>
        <div className="form-group">
          <label>Optimizer</label>
          <select value={cfg.optimizer_type} onChange={e => set('optimizer_type', Number(e.target.value))}>
            {OPTIMIZERS.map((o, i) => <option key={i} value={i}>{o}</option>)}
          </select>
        </div>
        <div className="form-group">
          <label>Scheduler</label>
          <select value={cfg.scheduler_type} onChange={e => set('scheduler_type', Number(e.target.value))}>
            {SCHEDULERS.map((s, i) => <option key={i} value={i}>{s}</option>)}
          </select>
        </div>
      </div>
      <div className="form-row">
        <div className="form-group">
          <label>Learning Rate</label>
          <input type="number" step="0.0001" value={cfg.learning_rate}
            onChange={e => set('learning_rate', Number(e.target.value))} />
        </div>
        <div className="form-group">
          <label>Weight Decay</label>
          <input type="number" step="0.0001" value={cfg.weight_decay}
            onChange={e => set('weight_decay', Number(e.target.value))} />
        </div>
        <div className="form-group">
          <label>Momentum</label>
          <input type="number" step="0.01" min={0} max={1} value={cfg.momentum}
            onChange={e => set('momentum', Number(e.target.value))} />
        </div>
      </div>
      <div className="form-row">
        <div className="form-group">
          <label>Beta1</label>
          <input type="number" step="0.001" min={0} max={1} value={cfg.beta1}
            onChange={e => set('beta1', Number(e.target.value))} />
        </div>
        <div className="form-group">
          <label>Beta2</label>
          <input type="number" step="0.001" min={0} max={1} value={cfg.beta2}
            onChange={e => set('beta2', Number(e.target.value))} />
        </div>
        <div className="form-group">
          <label>Scheduler Step</label>
          <input type="number" min={1} value={cfg.scheduler_step_size}
            onChange={e => set('scheduler_step_size', Number(e.target.value))} />
        </div>
      </div>
      <div className="form-row">
        <div className="form-group">
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <input type="checkbox" checked={cfg.enable_gradient_clipping}
              onChange={e => set('enable_gradient_clipping', e.target.checked)} />
            Gradient Clipping
          </label>
          {cfg.enable_gradient_clipping && (
            <input type="number" step="0.1" value={cfg.gradient_clip_value}
              onChange={e => set('gradient_clip_value', Number(e.target.value))} />
          )}
        </div>
        <div className="form-group">
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <input type="checkbox" checked={cfg.enable_biological_modulation}
              onChange={e => set('enable_biological_modulation', e.target.checked)} />
            Biological Modulation
          </label>
          {cfg.enable_biological_modulation && (
            <input type="range" min={0} max={1} step={0.05} value={cfg.biological_blend}
              onChange={e => set('biological_blend', Number(e.target.value))}
              style={{ width: '100%' }} />
          )}
        </div>
      </div>
      <button className="btn btn-primary" onClick={() => onConfigure(cfg)}>Apply Config</button>
    </div>
  );
}
