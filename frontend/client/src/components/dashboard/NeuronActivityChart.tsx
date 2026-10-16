import { Bar } from 'react-chartjs-2';
import type { BrainProbe } from '../../types';
import { chartOptions } from './chartConfig';

interface Props {
  probe: BrainProbe | null;
}

export function NeuronActivityChart({ probe }: Props) {
  if (!probe) {
    return <div className="chart-panel"><div className="panel-title">Neuron Activity</div><div className="empty-state" style={{height:200}}>No data</div></div>;
  }

  const activeRatio = probe.num_synapses > 0
    ? (probe.num_active_synapses / probe.num_synapses) * 100
    : 0;

  const data = {
    labels: ['Active Synapses %', 'Sparsity %'],
    datasets: [{
      label: 'Network Activity',
      data: [activeRatio, probe.avg_sparsity * 100],
      backgroundColor: ['rgba(54, 162, 235, 0.6)', 'rgba(255, 206, 86, 0.6)'],
      borderColor: ['rgb(54, 162, 235)', 'rgb(255, 206, 86)'],
      borderWidth: 1,
    }],
  };

  return (
    <div className="chart-panel">
      <div className="panel-title">Neuron Activity</div>
      <Bar data={data} options={chartOptions('Percentage', true)} />
    </div>
  );
}
