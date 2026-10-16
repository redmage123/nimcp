import type { DatasetInfo } from '../../types';
import { DatasetList } from './DatasetList';
import { CSVUploadPanel } from './CSVUploadPanel';

interface Props {
  datasets: DatasetInfo[];
  onRefresh: () => void;
}

export function DatasetPage({ datasets, onRefresh }: Props) {
  return (
    <div>
      <DatasetList datasets={datasets} />
      <div style={{ marginTop: 20 }}>
        <CSVUploadPanel onUploaded={onRefresh} />
      </div>
    </div>
  );
}
