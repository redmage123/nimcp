import type { BrainProbe } from '../../types';
import { ProbeMetrics } from './ProbeMetrics';
import { LossChart } from './LossChart';
import { AccuracyChart } from './AccuracyChart';
import { NeuronActivityChart } from './NeuronActivityChart';
import { MemoryUsageChart } from './MemoryUsageChart';
import { InferenceTimeChart } from './InferenceTimeChart';
import { NeuralActivityCanvas } from './NeuralActivityCanvas';

interface Props {
  probe: BrainProbe | null;
  history: (BrainProbe & { timestamp: number })[];
}

export function DashboardPage({ probe, history }: Props) {
  if (!probe) {
    return <div className="empty-state">Select or create a brain to view the dashboard</div>;
  }

  return (
    <div>
      <ProbeMetrics probe={probe} />
      <div className="charts-grid">
        <NeuralActivityCanvas probe={probe} />
        <LossChart history={history} />
        <AccuracyChart history={history} />
        <NeuronActivityChart probe={probe} />
        <MemoryUsageChart probe={probe} />
        <InferenceTimeChart history={history} />
      </div>
    </div>
  );
}
