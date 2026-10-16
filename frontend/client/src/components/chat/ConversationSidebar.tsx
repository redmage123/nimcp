import { useState } from 'react';
import type { Conversation, BrainInfo, AuthState } from '../../types';

interface Props {
  conversations: Conversation[];
  activeConvId: string | null;
  onSelect: (id: string) => void;
  onNew: () => void;
  onDelete: (id: string) => void;
  brains: BrainInfo[];
  activeBrainId: number;
  onBrainChange: (id: number) => void;
  auth: AuthState;
  onAdminToggle?: () => void;
  onLogout: () => void;
}

function groupByDate(conversations: Conversation[]): Record<string, Conversation[]> {
  const groups: Record<string, Conversation[]> = {};
  const now = new Date();
  const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const yesterday = new Date(today.getTime() - 86400000);
  const weekAgo = new Date(today.getTime() - 7 * 86400000);

  for (const conv of conversations) {
    const d = new Date(conv.updated_at);
    let label: string;
    if (d >= today) label = 'Today';
    else if (d >= yesterday) label = 'Yesterday';
    else if (d >= weekAgo) label = 'This week';
    else label = 'Older';
    (groups[label] ??= []).push(conv);
  }
  return groups;
}

export function ConversationSidebar({
  conversations, activeConvId, onSelect, onNew, onDelete,
  brains, activeBrainId, onBrainChange, auth, onAdminToggle, onLogout,
}: Props) {
  const [hoveredId, setHoveredId] = useState<string | null>(null);
  const groups = groupByDate(conversations);
  const groupOrder = ['Today', 'Yesterday', 'This week', 'Older'];

  return (
    <div className="conv-sidebar">
      <button className="conv-new-btn" onClick={onNew}>+ New Chat</button>

      <div className="conv-brain-select">
        <label>Brain</label>
        <select value={activeBrainId} onChange={e => onBrainChange(Number(e.target.value))}>
          {brains.map(b => (
            <option key={b.id} value={b.id}>{b.name}</option>
          ))}
        </select>
      </div>

      <div className="conv-list">
        {groupOrder.map(label => {
          const items = groups[label];
          if (!items?.length) return null;
          return (
            <div key={label}>
              <div className="conv-date-header">{label}</div>
              {items.map(conv => (
                <div
                  key={conv.id}
                  className={`conv-item ${conv.id === activeConvId ? 'active' : ''}`}
                  onClick={() => onSelect(conv.id)}
                  onMouseEnter={() => setHoveredId(conv.id)}
                  onMouseLeave={() => setHoveredId(null)}
                >
                  <span className="conv-item-title">{conv.title}</span>
                  {hoveredId === conv.id && (
                    <button
                      className="conv-item-delete"
                      onClick={e => { e.stopPropagation(); onDelete(conv.id); }}
                      title="Delete"
                    >
                      &times;
                    </button>
                  )}
                </div>
              ))}
            </div>
          );
        })}
        {conversations.length === 0 && (
          <div className="conv-empty">No conversations yet</div>
        )}
      </div>

      <div className="conv-user-menu">
        {auth.role === 'admin' && onAdminToggle && (
          <button className="conv-admin-btn" onClick={onAdminToggle}>Admin Panel</button>
        )}
        <div className="conv-user-info">
          <span className="conv-username">{auth.username}</span>
          <button className="conv-logout-btn" onClick={onLogout}>Logout</button>
        </div>
      </div>
    </div>
  );
}
