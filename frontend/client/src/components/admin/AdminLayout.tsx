import { useState, useEffect, useCallback, useMemo } from 'react';
import type { AuthState, AppView, BrainInfo, DatasetInfo, TrainingProgress } from '../../types';
import { AdminNav } from './AdminNav';
import { ChatLayout } from '../chat/ChatLayout';
import { Header } from '../layout/Header';
import { Sidebar } from '../layout/Sidebar';
import { TabNav } from '../layout/TabNav';
import { DashboardPage } from '../dashboard/DashboardPage';
import { TrainingPage } from '../training/TrainingPage';
import { SnapshotPanel } from '../brains/SnapshotPanel';
import { BrainCreateModal } from '../brains/BrainCreateModal';
import { BrainDetailModal } from '../brains/BrainDetailModal';
import { AdminDashboard } from './AdminDashboard';
import { ProbeMonitor } from './ProbeMonitor';
import { useBrainProbe } from '../../hooks/useBrainProbe';
import api from '../../services/api';
import * as brainApi from '../../services/brainApi';
import { listDatasets } from '../../services/datasetApi';
import type { BrainCreate, Tab } from '../../types';

interface Props {
  auth: AuthState;
  onLogout: () => void;
}

export function AdminLayout({ auth, onLogout }: Props) {
  const [view, setView] = useState<AppView>('chat');
  const [brains, setBrains] = useState<BrainInfo[]>([]);
  const [activeBrainId, setActiveBrainId] = useState<number | null>(null);
  const [datasets, setDatasets] = useState<DatasetInfo[]>([]);
  const [showCreate, setShowCreate] = useState(false);
  const [detailBrainId, setDetailBrainId] = useState<number | null>(null);
  const [adminTab, setAdminTab] = useState<Tab>('dashboard');

  const { probe, history, trainingProgress, connected, send, setChatCallback } =
    useBrainProbe(activeBrainId);

  const refreshBrains = useCallback(() => {
    brainApi.listBrains().then(r => setBrains(r.data)).catch(() => {});
  }, []);

  const refreshDatasets = useCallback(() => {
    listDatasets().then(r => setDatasets(r.data)).catch(() => {});
  }, []);

  useEffect(() => {
    refreshBrains();
    refreshDatasets();
    const id = setInterval(refreshBrains, 10000);
    return () => clearInterval(id);
  }, [refreshBrains, refreshDatasets]);

  const trainingBrainId = useMemo(() => {
    if (!trainingProgress) return null;
    const tp = trainingProgress as unknown as TrainingProgress & { type: string };
    if (tp.type === 'training_progress' && tp.running) return tp.brain_id;
    return null;
  }, [trainingProgress]);

  const handleCreate = async (data: BrainCreate) => {
    try {
      const r = await brainApi.createBrain(data);
      setShowCreate(false);
      refreshBrains();
      setActiveBrainId(r.data.id);
    } catch { /* */ }
  };

  const handleDelete = async (id: number) => {
    if (!window.confirm('Delete this brain? This cannot be undone.')) return;
    await brainApi.deleteBrain(id);
    if (activeBrainId === id) setActiveBrainId(null);
    if (detailBrainId === id) setDetailBrainId(null);
    refreshBrains();
  };

  const handleRename = async (id: number, name: string) => {
    try {
      await brainApi.renameBrain(id, name);
      refreshBrains();
    } catch { /* */ }
  };

  if (view === 'chat') {
    return (
      <ChatLayout
        auth={auth}
        onLogout={onLogout}
        onAdminToggle={() => setView('dashboard')}
      />
    );
  }

  // Admin panel views: dashboard, training, probes
  return (
    <>
      <Header />
      <AdminNav active={view} onChange={setView} />
      <div className="app-layout">
        <Sidebar
          brains={brains}
          activeBrainId={activeBrainId}
          trainingBrainId={trainingBrainId}
          onSelect={setActiveBrainId}
          onDelete={handleDelete}
          onCreateClick={() => setShowCreate(true)}
          onRename={handleRename}
          onShowDetail={setDetailBrainId}
        />
        <div className="main-content">
          <div className="tab-content">
            {view === 'dashboard' && (
              <>
                <AdminDashboard brainId={activeBrainId} />
                <DashboardPage probe={probe} history={history} />
                {activeBrainId !== null && <SnapshotPanel brainId={activeBrainId} />}
              </>
            )}
            {view === 'training' && (
              <TrainingPage brainId={activeBrainId} datasets={datasets} trainingProgress={trainingProgress} onRefresh={refreshDatasets} />
            )}
            {view === 'probes' && (
              <AdminDashboard brainId={activeBrainId} />
            )}
            {view === 'monitor' && (
              <ProbeMonitor brainId={activeBrainId ?? 0} />
            )}
          </div>
        </div>
      </div>
      {showCreate && (
        <BrainCreateModal onClose={() => setShowCreate(false)} onCreate={handleCreate} />
      )}
      {detailBrainId !== null && (
        <BrainDetailModal brainId={detailBrainId} onClose={() => setDetailBrainId(null)} />
      )}
    </>
  );
}
