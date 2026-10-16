import { useState, useEffect, useRef } from 'react';
import type { ProbeConfig, BrainProbe } from '../../types';
import { PlotlyProbeChart } from '../charts/PlotlyProbeChart';
import { ProbeBuilder } from './ProbeBuilder';
import type { BrainInfo } from '../../types';
import * as brainApi from '../../services/brainApi';

interface Props {
  config: ProbeConfig;
  brains: BrainInfo[];
  onUpdate: (config: Partial<ProbeConfig>) => void;
  onDelete: (id: string) => void;
}

const MAX_HISTORY = 200;

export function ProbeCard({ config, brains, onUpdate, onDelete }: Props) {
  const [data, setData] = useState<Record<string, number[]>>({});
  const [currentValues, setCurrentValues] = useState<Record<string, number>>({});
  const [editing, setEditing] = useState(false);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  useEffect(() => {
    const poll = () => {
      brainApi.probeBrain(config.brain_id).then(r => {
        const probe: Record<string, unknown> = r.data as unknown as Record<string, unknown>;
        const vals: Record<string, number> = {};
        for (const m of config.metrics) {
          if (m in probe && typeof probe[m] === 'number') {
            vals[m] = probe[m] as number;
          }
        }
        setCurrentValues(vals);
        setData(prev => {
          const next = { ...prev };
          for (const m of config.metrics) {
            const arr = next[m] || [];
            arr.push(vals[m] || 0);
            next[m] = arr.length > MAX_HISTORY ? arr.slice(-MAX_HISTORY) : arr;
          }
          return next;
        });
      }).catch(() => {});
    };

    poll();
    intervalRef.current = setInterval(poll, config.refresh_ms);
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [config.brain_id, config.refresh_ms, config.metrics.join(',')]);

  return (
    <>
      <div className="probe-card-wrapper">
        <div className="probe-card-header">
          <span className="probe-card-name">{config.name}</span>
          <div className="probe-card-actions">
            <button className="btn btn-sm btn-outline" onClick={() => setEditing(true)}>Edit</button>
            <button className="btn btn-sm btn-outline btn-danger" onClick={() => onDelete(config.id)}>Delete</button>
          </div>
        </div>
        <PlotlyProbeChart config={config} data={data} currentValues={currentValues} />
      </div>
      {editing && (
        <ProbeBuilder
          brains={brains}
          initial={config}
          onSave={(updated) => { onUpdate(updated); setEditing(false); }}
          onClose={() => setEditing(false)}
        />
      )}
    </>
  );
}
