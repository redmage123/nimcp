import { useState, useEffect, useRef } from 'react';
import type { DatasetInfo, TrainingProgress, WSMessage } from '../../types';
import { startTraining, stopTraining, trainingStatus } from '../../services/trainingApi';

interface Props {
  brainId: number;
  datasets: DatasetInfo[];
  trainingProgress: WSMessage | null;
}

export function TrainingControls({ brainId, datasets, trainingProgress }: Props) {
  const [datasetId, setDatasetId] = useState(datasets[0]?.id || 'iris');
  const [epochs, setEpochs] = useState(10);
  const [running, setRunning] = useState(false);
  const [progress, setProgress] = useState<TrainingProgress | null>(null);
  const pollRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Handle WebSocket training messages
  useEffect(() => {
    if (!trainingProgress) return;
    const tp = trainingProgress as unknown as TrainingProgress & { type: string };
    if (tp.type === 'training_progress') {
      setProgress(tp);
      setRunning(tp.running);
    } else if (tp.type === 'training_complete' || tp.type === 'training_error') {
      setRunning(false);
    }
  }, [trainingProgress]);

  // Polling fallback — when running, poll status every 1s
  useEffect(() => {
    if (!running) {
      if (pollRef.current) { clearInterval(pollRef.current); pollRef.current = null; }
      return;
    }
    pollRef.current = setInterval(async () => {
      try {
        const { data } = await trainingStatus(brainId);
        setProgress(data);
        if (!data.running) {
          setRunning(false);
        }
      } catch { /* ignore */ }
    }, 1000);
    return () => { if (pollRef.current) clearInterval(pollRef.current); };
  }, [running, brainId]);

  const handleStart = async () => {
    try {
      setRunning(true);
      const { data } = await startTraining(brainId, datasetId, epochs);
      setProgress(data);
    } catch { setRunning(false); }
  };

  const handleStop = async () => {
    await stopTraining(brainId);
    setRunning(false);
  };

  const pct = progress && progress.total_steps > 0
    ? Math.round((progress.step / progress.total_steps) * 100)
    : 0;

  return (
    <div className="panel">
      <div className="panel-title">Training Controls</div>
      <div className="form-row">
        <div className="form-group">
          <label>Dataset</label>
          <select value={datasetId} onChange={e => setDatasetId(e.target.value)}>
            {datasets.map(d => <option key={d.id} value={d.id}>{d.name}</option>)}
          </select>
        </div>
        <div className="form-group">
          <label>Epochs</label>
          <input type="number" min={1} max={1000} value={epochs}
            onChange={e => setEpochs(Number(e.target.value))} />
        </div>
      </div>
      <div style={{ display: 'flex', gap: 8, marginBottom: 12 }}>
        <button className="btn btn-success" onClick={handleStart} disabled={running}>
          {running ? 'Training...' : 'Start Training'}
        </button>
        {running && (
          <button className="btn btn-danger" onClick={handleStop}>Stop</button>
        )}
      </div>
      {progress && (running || progress.step > 0) && (
        <>
          <div className="progress-bar-container">
            <div className={`progress-bar-fill${running ? ' active' : ''}`} style={{ width: `${pct}%` }} />
          </div>
          <div style={{ fontSize: 12, color: 'var(--text-muted)', marginBottom: 8 }}>
            {running
              ? `Step ${progress.step}/${progress.total_steps} (${pct}%) — Epoch ${progress.epoch}`
              : `Complete — ${progress.total_steps} steps in ${progress.elapsed_seconds.toFixed(1)}s`
            }
          </div>
          <div className="training-stats">
            <div className="training-stat">
              <div className="training-stat-value">{progress.loss.toFixed(4)}</div>
              <div className="training-stat-label">Loss</div>
            </div>
            <div className="training-stat">
              <div className="training-stat-value">{(progress.accuracy * 100).toFixed(1)}%</div>
              <div className="training-stat-label">Accuracy</div>
            </div>
            <div className="training-stat">
              <div className="training-stat-value">{progress.learning_rate.toExponential(2)}</div>
              <div className="training-stat-label">LR</div>
            </div>
            <div className="training-stat">
              <div className="training-stat-value">{progress.elapsed_seconds.toFixed(1)}s</div>
              <div className="training-stat-label">Elapsed</div>
            </div>
          </div>
        </>
      )}
    </div>
  );
}
