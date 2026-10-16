import { Line } from 'react-chartjs-2';
import type { BrainProbe } from '../../types';
import { chartOptions, lastPointRadius, lastPointColor } from './chartConfig';

interface Props {
  history: (BrainProbe & { timestamp: number })[];
}

export function InferenceTimeChart({ history }: Props) {
  if (history.length === 0) {
    return <div className="chart-panel"><div className="panel-title">Inference Time</div><div className="empty-state" style={{height:200}}>No data yet</div></div>;
  }

  const n = history.length;
  const data = {
    labels: history.map((_, i) => i + 1),
    datasets: [{
      label: 'Avg Inference (us)',
      data: history.map(h => h.avg_inference_time_us),
      borderColor: 'rgb(153, 102, 255)',
      backgroundColor: 'rgba(153, 102, 255, 0.1)',
      fill: true,
      tension: 0.4,
      pointRadius: lastPointRadius(n),
      pointBackgroundColor: lastPointColor(n, 'rgb(153, 102, 255)'),
      pointBorderColor: lastPointColor(n, 'rgb(153, 102, 255)'),
    }],
  };

  return (
    <div className="chart-panel">
      <div className="panel-title">Inference Time</div>
      <Line data={data} options={chartOptions('Microseconds', false)} />
    </div>
  );
}
