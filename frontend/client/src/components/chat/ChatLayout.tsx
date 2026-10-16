import { useState, useEffect, useCallback } from 'react';
import type { AuthState, BrainInfo, Conversation } from '../../types';
import { ConversationSidebar } from './ConversationSidebar';
import { ChatArea } from './ChatArea';
import { useBrainProbe } from '../../hooks/useBrainProbe';
import * as brainApi from '../../services/brainApi';
import * as convApi from '../../services/conversationApi';

interface Props {
  auth: AuthState;
  onLogout: () => void;
  onAdminToggle?: () => void;
}

export function ChatLayout({ auth, onLogout, onAdminToggle }: Props) {
  const [brains, setBrains] = useState<BrainInfo[]>([]);
  const [activeBrainId, setActiveBrainId] = useState(0);
  const [conversations, setConversations] = useState<Conversation[]>([]);
  const [activeConvId, setActiveConvId] = useState<string | null>(null);

  const { connected, send, setChatCallback } = useBrainProbe(activeBrainId);

  const refreshBrains = useCallback(() => {
    brainApi.listBrains().then(r => setBrains(r.data)).catch(() => {});
  }, []);

  const refreshConversations = useCallback(() => {
    convApi.listConversations().then(r => setConversations(r.data)).catch(() => {});
  }, []);

  useEffect(() => {
    refreshBrains();
    refreshConversations();
    const id = setInterval(refreshBrains, 30000);
    return () => clearInterval(id);
  }, [refreshBrains, refreshConversations]);

  const handleNewChat = useCallback(async () => {
    try {
      const r = await convApi.createConversation(activeBrainId);
      refreshConversations();
      setActiveConvId(r.data.id);
    } catch { /* */ }
  }, [activeBrainId, refreshConversations]);

  const handleDeleteConv = useCallback(async (id: string) => {
    try {
      await convApi.deleteConversation(id);
      if (activeConvId === id) setActiveConvId(null);
      refreshConversations();
    } catch { /* */ }
  }, [activeConvId, refreshConversations]);

  const handleBrainChange = useCallback((id: number) => {
    setActiveBrainId(id);
  }, []);

  const activeBrain = brains.find(b => b.id === activeBrainId);
  const brainName = activeBrain?.name || 'Athena';

  return (
    <div className="chat-layout-root">
      <ConversationSidebar
        conversations={conversations}
        activeConvId={activeConvId}
        onSelect={setActiveConvId}
        onNew={handleNewChat}
        onDelete={handleDeleteConv}
        brains={brains}
        activeBrainId={activeBrainId}
        onBrainChange={handleBrainChange}
        auth={auth}
        onAdminToggle={onAdminToggle}
        onLogout={onLogout}
      />
      <ChatArea
        brainName={brainName}
        connected={connected}
        send={send}
        setChatCallback={setChatCallback}
        conversationId={activeConvId}
        brainId={activeBrainId}
      />
    </div>
  );
}
