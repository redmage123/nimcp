import { Doughnut } from 'react-chartjs-2';
import type { BrainProbe } from '../../types';

interface Props {
  probe: BrainProbe | null;
}

export function MemoryUsageChart({ probe }: Props) {
  if (!probe) {
    return <div className="chart-panel"><div className="panel-title">Memory Usage</div><div className="empty-state" style={{height:200}}>No data</div></div>;
  }

  const shared = probe.cow_shared_bytes;
  const priv = probe.cow_private_bytes;
  const other = Math.max(0, probe.memory_bytes - shared - priv);

  const isCow = probe.is_cow_clone;
  const labels = isCow ? ['COW Shared', 'COW Private', 'Other'] : ['Total Memory'];
  const values = isCow ? [shared, priv, other] : [probe.memory_bytes];
  const colors = isCow
    ? ['rgba(75, 192, 192, 0.7)', 'rgba(255, 99, 132, 0.7)', 'rgba(255, 206, 86, 0.7)']
    : ['rgba(102, 126, 234, 0.7)'];

  const data = {
    labels,
    datasets: [{
      data: values,
      backgroundColor: colors,
      borderWidth: 0,
    }],
  };

  const options = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { position: 'bottom' as const, labels: { color: '#e0e0e0', font: { size: 11 } } },
    },
  };

  return (
    <div className="chart-panel">
      <div className="panel-title">Memory Usage</div>
      <div style={{ height: 220 }}>
        <Doughnut data={data} options={options} />
      </div>
    </div>
  );
}
