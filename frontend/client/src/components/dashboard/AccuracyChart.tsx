import { Line } from 'react-chartjs-2';
import type { BrainProbe } from '../../types';
import { chartOptions, lastPointRadius, lastPointColor } from './chartConfig';

interface Props {
  history: (BrainProbe & { timestamp: number })[];
}

export function AccuracyChart({ history }: Props) {
  if (history.length === 0) {
    return <div className="chart-panel"><div className="panel-title">Accuracy</div><div className="empty-state" style={{height:200}}>No data yet</div></div>;
  }

  const n = history.length;
  const data = {
    labels: history.map((_, i) => i + 1),
    datasets: [{
      label: 'Accuracy %',
      data: history.map(h => h.accuracy * 100),
      borderColor: 'rgb(75, 192, 192)',
      backgroundColor: 'rgba(75, 192, 192, 0.1)',
      fill: true,
      tension: 0.4,
      pointRadius: lastPointRadius(n),
      pointBackgroundColor: lastPointColor(n, 'rgb(75, 192, 192)'),
      pointBorderColor: lastPointColor(n, 'rgb(75, 192, 192)'),
    }],
  };

  return (
    <div className="chart-panel">
      <div className="panel-title">Accuracy</div>
      <Line data={data} options={chartOptions('Accuracy %', false)} />
    </div>
  );
}
