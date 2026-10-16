import { useState, useRef, useEffect } from 'react';
import type { BrainInfo } from '../../types';

interface Props {
  brains: BrainInfo[];
  activeBrainId: number | null;
  trainingBrainId?: number | null;
  onSelect: (id: number) => void;
  onDelete: (id: number) => void;
  onCreateClick: () => void;
  onRename?: (id: number, name: string) => void;
  onShowDetail?: (id: number) => void;
}

function InlineName({ name, onCommit, onCancel }: { name: string; onCommit: (v: string) => void; onCancel: () => void }) {
  const ref = useRef<HTMLInputElement>(null);
  const [value, setValue] = useState(name);

  useEffect(() => { ref.current?.focus(); ref.current?.select(); }, []);

  const commit = () => {
    const trimmed = value.trim();
    if (trimmed && trimmed !== name) onCommit(trimmed);
    else onCancel();
  };

  return (
    <input
      ref={ref}
      className="brain-card-name-input"
      value={value}
      onChange={(e) => setValue(e.target.value)}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === 'Enter') commit();
        if (e.key === 'Escape') onCancel();
      }}
      onClick={(e) => e.stopPropagation()}
      maxLength={128}
    />
  );
}

export function Sidebar({ brains, activeBrainId, trainingBrainId, onSelect, onDelete, onCreateClick, onRename, onShowDetail }: Props) {
  const [editingId, setEditingId] = useState<number | null>(null);

  return (
    <aside className="sidebar">
      <div className="sidebar-header">Brains</div>
      <div className="brain-list">
        {brains.map((b) => {
          const isActive = b.id === activeBrainId;
          const isTraining = b.id === trainingBrainId;
          const cls = `brain-card${isActive ? ' active' : ''}${isTraining ? ' training' : ''}`;
          return (
            <div
              key={b.id}
              className={cls}
              onClick={() => { onSelect(b.id); if (onShowDetail) onShowDetail(b.id); }}
            >
              <button
                className="brain-card-delete"
                onClick={(e) => { e.stopPropagation(); onDelete(b.id); }}
                title="Delete"
              >
                x
              </button>
              {editingId === b.id ? (
                <InlineName
                  name={b.name}
                  onCommit={(v) => { setEditingId(null); if (onRename) onRename(b.id, v); }}
                  onCancel={() => setEditingId(null)}
                />
              ) : (
                <div
                  className="brain-card-name"
                  onDoubleClick={(e) => { e.stopPropagation(); if (onRename) setEditingId(b.id); }}
                >
                  {b.name}
                </div>
              )}
              <div className="brain-card-meta">
                {b.probe ? `${b.probe.num_neurons.toLocaleString()} neurons` : 'Loading...'}
                {b.probe?.is_cow_clone && ' (COW)'}
              </div>
            </div>
          );
        })}
        {brains.length === 0 && (
          <div style={{ padding: 16, fontSize: 12, color: 'var(--text-muted)' }}>
            No brains yet
          </div>
        )}
      </div>
      <div className="sidebar-actions">
        <button className="btn btn-primary btn-block" onClick={onCreateClick}>
          + New Brain
        </button>
      </div>
    </aside>
  );
}
