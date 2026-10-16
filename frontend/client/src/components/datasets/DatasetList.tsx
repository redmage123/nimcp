import type { DatasetInfo } from '../../types';

interface Props {
  datasets: DatasetInfo[];
}

export function DatasetList({ datasets }: Props) {
  return (
    <div className="dataset-grid">
      {datasets.map((d) => (
        <div key={d.id} className="dataset-card">
          <div className="dataset-card-title">{d.name}</div>
          <div className="dataset-card-desc">{d.description}</div>
          <div className="dataset-card-stats">
            <span>Inputs: {d.num_inputs}</span>
            <span>Classes: {d.num_classes}</span>
            <span>Examples: {d.total_examples}</span>
          </div>
        </div>
      ))}
    </div>
  );
}
