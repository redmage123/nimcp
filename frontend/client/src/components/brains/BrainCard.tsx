import type { BrainInfo } from '../../types';

interface Props {
  brain: BrainInfo;
  active: boolean;
  training?: boolean;
  onSelect: () => void;
  onDelete: () => void;
}

export function BrainCard({ brain, active, training, onSelect, onDelete }: Props) {
  const cls = `brain-card${active ? ' active' : ''}${training ? ' training' : ''}`;
  return (
    <div className={cls} onClick={onSelect}>
      <button className="brain-card-delete" onClick={(e) => { e.stopPropagation(); onDelete(); }} title="Delete">
        x
      </button>
      <div className="brain-card-name">{brain.name}</div>
      <div className="brain-card-meta">
        {brain.probe ? `${brain.probe.num_neurons.toLocaleString()} neurons` : 'Loading...'}
        {brain.probe?.is_cow_clone && ' (COW)'}
      </div>
    </div>
  );
}
