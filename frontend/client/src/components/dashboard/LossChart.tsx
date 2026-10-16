import { Line } from 'react-chartjs-2';
import type { BrainProbe } from '../../types';
import { chartOptions, lastPointRadius, lastPointColor } from './chartConfig';

interface Props {
  history: (BrainProbe & { timestamp: number })[];
}

export function LossChart({ history }: Props) {
  const steps = history.filter(h => h.total_learning_steps > 0);
  if (steps.length === 0) {
    return <div className="chart-panel"><div className="panel-title">Training Loss</div><div className="empty-state" style={{height:200}}>No training data yet</div></div>;
  }

  const n = steps.length;
  const data = {
    labels: steps.map((_, i) => i + 1),
    datasets: [{
      label: 'Loss',
      data: steps.map(h => h.last_loss),
      borderColor: 'rgb(255, 99, 132)',
      backgroundColor: 'rgba(255, 99, 132, 0.1)',
      fill: true,
      tension: 0.4,
      pointRadius: lastPointRadius(n),
      pointBackgroundColor: lastPointColor(n, 'rgb(255, 99, 132)'),
      pointBorderColor: lastPointColor(n, 'rgb(255, 99, 132)'),
    }],
  };

  return (
    <div className="chart-panel">
      <div className="panel-title">Training Loss</div>
      <Line data={data} options={chartOptions('Loss', false)} />
    </div>
  );
}
