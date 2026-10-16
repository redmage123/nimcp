import { useState } from 'react';
import type { DatasetInfo } from '../../types';
import { learnBatch } from '../../services/trainingApi';

interface Props {
  brainId: number;
  datasets: DatasetInfo[];
}

export function BatchTrainingPanel({ brainId, datasets }: Props) {
  const [datasetId, setDatasetId] = useState(datasets[0]?.id || 'iris');
  const [count, setCount] = useState(36);
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<{ trained: number; last_loss: number } | null>(null);

  const handleTrain = async () => {
    setLoading(true);
    try {
      const r = await learnBatch(brainId, datasetId, count);
      setResult(r.data);
    } catch { /* */ }
    setLoading(false);
  };

  return (
    <div className="panel">
      <div className="panel-title">Quick Batch Training</div>
      <div className="form-row">
        <div className="form-group">
          <label>Dataset</label>
          <select value={datasetId} onChange={e => setDatasetId(e.target.value)}>
            {datasets.map(d => <option key={d.id} value={d.id}>{d.name}</option>)}
          </select>
        </div>
        <div className="form-group">
          <label>Examples</label>
          <input type="number" min={1} max={10000} value={count}
            onChange={e => setCount(Number(e.target.value))} />
        </div>
      </div>
      <button className="btn btn-primary" onClick={handleTrain} disabled={loading}>
        {loading ? 'Training...' : 'Train Batch'}
      </button>
      {result && (
        <div style={{ marginTop: 8, fontSize: 12, color: 'var(--text-light)' }}>
          Trained {result.trained} examples. Last loss: {result.last_loss.toFixed(4)}
        </div>
      )}
    </div>
  );
}
