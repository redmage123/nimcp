import type { DatasetInfo, WSMessage } from '../../types';
import { TrainingControls } from './TrainingControls';
import { BatchTrainingPanel } from './BatchTrainingPanel';
import { ScriptRunnerPanel } from './ScriptRunnerPanel';
import { DatasetManagerPanel } from './DatasetManagerPanel';

interface Props {
  brainId: number | null;
  datasets: DatasetInfo[];
  trainingProgress: WSMessage | null;
  onRefresh: () => void;
}

export function TrainingPage({ brainId, datasets, trainingProgress, onRefresh }: Props) {
  if (brainId === null) {
    return <div className="empty-state">Select a brain to configure training</div>;
  }

  return (
    <div className="training-layout">
      <div>
        <TrainingControls brainId={brainId} datasets={datasets} trainingProgress={trainingProgress} />
      </div>
      <div>
        <BatchTrainingPanel brainId={brainId} datasets={datasets} />
      </div>
      <div>
        <ScriptRunnerPanel brainId={brainId} />
      </div>
      <DatasetManagerPanel datasets={datasets} onRefresh={onRefresh} />
    </div>
  );
}
