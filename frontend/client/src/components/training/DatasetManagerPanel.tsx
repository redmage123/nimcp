import { useState, useRef } from 'react';
import type { DatasetInfo } from '../../types';
import { uploadCSV, deleteDatasetsBatch } from '../../services/datasetApi';

interface Props {
  datasets: DatasetInfo[];
  onRefresh: () => void;
}

export function DatasetManagerPanel({ datasets, onRefresh }: Props) {
  const [selected, setSelected] = useState<Set<string>>(new Set());
  const [deleting, setDeleting] = useState(false);

  // Upload state
  const [file, setFile] = useState<File | null>(null);
  const [name, setName] = useState('');
  const [labelCol, setLabelCol] = useState('label');
  const [uploading, setUploading] = useState(false);
  const [msg, setMsg] = useState('');
  const fileRef = useRef<HTMLInputElement>(null);

  const uploaded = datasets.filter(d => !d.is_builtin);
  const allUploadedSelected = uploaded.length > 0 && uploaded.every(d => selected.has(d.id));

  const toggleOne = (id: string) => {
    setSelected(prev => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  };

  const toggleAll = () => {
    if (allUploadedSelected) {
      setSelected(new Set());
    } else {
      setSelected(new Set(uploaded.map(d => d.id)));
    }
  };

  const handleDelete = async () => {
    const ids = [...selected];
    if (ids.length === 0) return;
    if (!window.confirm(`Delete ${ids.length} dataset${ids.length > 1 ? 's' : ''}? This cannot be undone.`)) return;
    setDeleting(true);
    try {
      await deleteDatasetsBatch(ids);
      setSelected(new Set());
      onRefresh();
    } catch (e: unknown) {
      setMsg(`Delete error: ${(e as Error).message}`);
    }
    setDeleting(false);
  };

  const handleUpload = async () => {
    if (!file || !name) return;
    setUploading(true);
    setMsg('');
    try {
      const r = await uploadCSV(file, name, labelCol);
      setMsg(`Uploaded: ${r.data.id}`);
      setFile(null);
      setName('');
      if (fileRef.current) fileRef.current.value = '';
      onRefresh();
    } catch (e: unknown) {
      setMsg(`Error: ${(e as Error).message}`);
    }
    setUploading(false);
  };

  return (
    <div className="panel" style={{ gridColumn: '1 / -1' }}>
      <div className="panel-title">Dataset Manager</div>

      {/* Dataset table */}
      <div style={{ overflowX: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 13 }}>
          <thead>
            <tr style={{ borderBottom: '1px solid var(--border)', textAlign: 'left' }}>
              <th style={{ padding: '8px 6px', width: 32 }}>
                {uploaded.length > 0 && (
                  <input
                    type="checkbox"
                    checked={allUploadedSelected}
                    onChange={toggleAll}
                    title="Select all uploaded datasets"
                  />
                )}
              </th>
              <th style={{ padding: '8px 6px' }}>Name</th>
              <th style={{ padding: '8px 6px' }}>Inputs</th>
              <th style={{ padding: '8px 6px' }}>Classes</th>
              <th style={{ padding: '8px 6px' }}>Examples</th>
              <th style={{ padding: '8px 6px' }}>Type</th>
            </tr>
          </thead>
          <tbody>
            {datasets.map(d => (
              <tr key={d.id} style={{ borderBottom: '1px solid var(--border)' }}>
                <td style={{ padding: '6px' }}>
                  {!d.is_builtin && (
                    <input
                      type="checkbox"
                      checked={selected.has(d.id)}
                      onChange={() => toggleOne(d.id)}
                    />
                  )}
                </td>
                <td style={{ padding: '6px' }}>
                  <span style={{ fontWeight: 500 }}>{d.name}</span>
                  <div style={{ fontSize: 11, color: 'var(--text-light)' }}>{d.description}</div>
                </td>
                <td style={{ padding: '6px' }}>{d.num_inputs}</td>
                <td style={{ padding: '6px' }}>{d.num_classes}</td>
                <td style={{ padding: '6px' }}>{d.total_examples || '-'}</td>
                <td style={{ padding: '6px' }}>
                  <span
                    className={`badge ${d.is_builtin ? 'badge-info' : 'badge-warn'}`}
                    style={{
                      fontSize: 11,
                      padding: '2px 8px',
                      borderRadius: 4,
                      background: d.is_builtin ? 'var(--accent-bg, #1a3a4a)' : 'var(--warn-bg, #3a3420)',
                      color: d.is_builtin ? 'var(--accent, #4fc3f7)' : 'var(--warn, #ffb74d)',
                    }}
                  >
                    {d.is_builtin ? 'Built-in' : 'Uploaded'}
                  </span>
                </td>
              </tr>
            ))}
            {datasets.length === 0 && (
              <tr>
                <td colSpan={6} style={{ padding: 16, textAlign: 'center', color: 'var(--text-light)' }}>
                  No datasets available
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      {/* Delete button */}
      {selected.size > 0 && (
        <div style={{ marginTop: 12 }}>
          <button
            className="btn btn-danger"
            onClick={handleDelete}
            disabled={deleting}
            style={{
              background: '#c62828',
              color: '#fff',
              border: 'none',
              padding: '6px 16px',
              borderRadius: 4,
              cursor: deleting ? 'not-allowed' : 'pointer',
            }}
          >
            {deleting ? 'Deleting...' : `Delete Selected (${selected.size})`}
          </button>
        </div>
      )}

      {/* Upload section */}
      <div style={{ marginTop: 16, borderTop: '1px solid var(--border)', paddingTop: 12 }}>
        <div style={{ fontSize: 13, fontWeight: 500, marginBottom: 8 }}>Upload CSV</div>
        <div
          className="upload-zone"
          onClick={() => fileRef.current?.click()}
          style={{ cursor: 'pointer' }}
        >
          <input
            ref={fileRef}
            type="file"
            accept=".csv"
            style={{ display: 'none' }}
            onChange={e => setFile(e.target.files?.[0] || null)}
          />
          {file ? file.name : 'Click to select CSV file'}
        </div>
        <div className="form-row" style={{ marginTop: 8 }}>
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
      </div>

      {msg && <div style={{ marginTop: 8, fontSize: 12, color: 'var(--text-light)' }}>{msg}</div>}
    </div>
  );
}
