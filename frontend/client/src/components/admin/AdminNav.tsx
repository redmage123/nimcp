import type { AppView } from '../../types';

interface Props {
  active: AppView;
  onChange: (view: AppView) => void;
}

const VIEWS: { id: AppView; label: string }[] = [
  { id: 'chat', label: 'Chat' },
  { id: 'dashboard', label: 'Dashboard' },
  { id: 'training', label: 'Training' },
  { id: 'probes', label: 'Probes' },
  { id: 'monitor', label: 'Monitor' },
];

export function AdminNav({ active, onChange }: Props) {
  return (
    <div className="admin-nav">
      {VIEWS.map(v => (
        <button
          key={v.id}
          className={`admin-nav-btn ${active === v.id ? 'active' : ''}`}
          onClick={() => onChange(v.id)}
        >
          {v.label}
        </button>
      ))}
    </div>
  );
}
