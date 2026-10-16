import { useRef, useEffect, useCallback } from 'react';
import type { BrainProbe } from '../../types';

interface Props {
  probe: BrainProbe | null;
}

function fmt(n: number, d = 0): string {
  if (n >= 1_000_000) return (n / 1_000_000).toFixed(1) + 'M';
  if (n >= 1_000) return (n / 1_000).toFixed(1) + 'K';
  return d > 0 ? n.toFixed(d) : n.toLocaleString();
}

function fmtBytes(b: number): string {
  if (b >= 1_073_741_824) return (b / 1_073_741_824).toFixed(1) + ' GB';
  if (b >= 1_048_576) return (b / 1_048_576).toFixed(1) + ' MB';
  if (b >= 1024) return (b / 1024).toFixed(1) + ' KB';
  return b + ' B';
}

function MetricCard({ label, value, color }: { label: string; value: string; color: string }) {
  const prevValue = useRef(value);
  const elRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (prevValue.current !== value && elRef.current) {
      const el = elRef.current;
      el.classList.remove('flash');
      // Force reflow to restart animation
      void el.offsetWidth;
      el.classList.add('flash');
      prevValue.current = value;
    }
  }, [value]);

  const handleAnimEnd = useCallback(() => {
    elRef.current?.classList.remove('flash');
  }, []);

  return (
    <div className="metric-card">
      <div
        ref={elRef}
        className="metric-value"
        style={{ color }}
        onAnimationEnd={handleAnimEnd}
      >
        {value}
      </div>
      <div className="metric-label">{label}</div>
    </div>
  );
}

export function ProbeMetrics({ probe }: Props) {
  if (!probe) return null;

  const cards = [
    { label: 'Neurons', value: fmt(probe.num_neurons), color: 'var(--primary)' },
    { label: 'Synapses', value: fmt(probe.num_synapses), color: 'var(--info)' },
    { label: 'Accuracy', value: (probe.accuracy * 100).toFixed(1) + '%', color: 'var(--success)' },
    { label: 'Learning Rate', value: probe.current_learning_rate.toExponential(2), color: 'var(--warning)' },
    { label: 'Memory', value: fmtBytes(probe.memory_bytes), color: 'var(--secondary)' },
    { label: 'Inference', value: probe.avg_inference_time_us.toFixed(0) + ' us', color: 'var(--danger)' },
  ];

  return (
    <div className="metrics-grid">
      {cards.map((c) => (
        <MetricCard key={c.label} label={c.label} value={c.value} color={c.color} />
      ))}
    </div>
  );
}
