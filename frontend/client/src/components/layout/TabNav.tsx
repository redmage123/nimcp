import type { Tab } from '../../types';

interface Props {
  active: Tab;
  onChange: (tab: Tab) => void;
}

const TABS: { key: Tab; label: string }[] = [
  { key: 'dashboard', label: 'Dashboard' },
  { key: 'training', label: 'Training' },
  { key: 'chat', label: 'Chat' },
  { key: 'datasets', label: 'Datasets' },
  { key: 'benchmarks', label: 'Benchmarks' },
];

export function TabNav({ active, onChange }: Props) {
  return (
    <nav className="tab-nav">
      {TABS.map((t) => (
        <button
          key={t.key}
          className={`tab-btn ${active === t.key ? 'active' : ''}`}
          onClick={() => onChange(t.key)}
        >
          {t.label}
        </button>
      ))}
    </nav>
  );
}
