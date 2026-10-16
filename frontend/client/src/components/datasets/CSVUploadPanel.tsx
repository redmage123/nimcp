import { useState, useRef } from 'react';
import { uploadCSV } from '../../services/datasetApi';

interface Props {
  onUploaded: () => void;
}

export function CSVUploadPanel({ onUploaded }: Props) {
  const [name, setName] = useState('');
  const [labelCol, setLabelCol] = useState('label');
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [msg, setMsg] = useState('');
  const fileRef = useRef<HTMLInputElement>(null);

  const handleUpload = async () => {
    if (!file || !name) return;
    setUploading(true);
    setMsg('');
    try {
      const r = await uploadCSV(file, name, labelCol);
      setMsg(`Uploaded: ${r.data.id}`);
      setFile(null);
      setName('');
      onUploaded();
    } catch (e: unknown) {
      setMsg(`Error: ${(e as Error).message}`);
    }
    setUploading(false);
  };

  return (
    <div className="panel">
      <div className="panel-title">Upload CSV Dataset</div>
      <div
        className="upload-zone"
        onClick={() => fileRef.current?.click()}
      >
        <input
          ref={fileRef}
          type="file"
          accept=".csv"
          onChange={e => setFile(e.target.files?.[0] || null)}
        />
        {file ? file.name : 'Click to select CSV file'}
      </div>
      <div className="form-row" style={{ marginTop: 12 }}>
        <div className="form-group">
          <label>Dataset Name</label>
          <input value={name} onChange={e => setName(e.target.value)} placeholder="my_dataset" />
        </div>
        <div className="form-group">
          <label>Label Column</label>
          <input value={labelCol} onChange={e => setLabelCol(e.target.value)} />
        </div>
      </div>
      <button className="btn btn-primary" onClick={handleUpload} disabled={uploading || !file || !name}>
        {uploading ? 'Uploading...' : 'Upload'}
      </button>
      {msg && <div style={{ marginTop: 8, fontSize: 12, color: 'var(--text-light)' }}>{msg}</div>}
    </div>
  );
}
