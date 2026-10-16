import { useState, useEffect, useRef, useCallback } from 'react';
import type { ChatMessage, WSMessage, ConversationDetail, AvatarState, AvatarIdentity } from '../../types';
import { getConversation } from '../../services/conversationApi';
import { getIdentity } from '../../services/brainApi';
import { AvatarFace } from './AvatarFace';

interface Props {
  brainName: string;
  connected: boolean;
  send: (data: Record<string, unknown>) => void;
  setChatCallback: (cb: ((msg: WSMessage) => void) | null) => void;
  conversationId: string | null;
  brainId: number;
  micActive?: boolean;
  camActive?: boolean;
  onToggleMic?: () => void;
  onToggleCam?: () => void;
}

let msgIdCounter = 0;

function ConfidenceBar({ confidence }: { confidence: number }) {
  const color = confidence > 0.7 ? '#22c55e' : confidence > 0.4 ? '#eab308' : '#ef4444';
  return (
    <div style={{
      height: 3, borderRadius: 2, marginTop: 4,
      background: 'rgba(255,255,255,0.1)', overflow: 'hidden',
    }}>
      <div style={{
        height: '100%', width: `${Math.min(confidence * 100, 100)}%`,
        background: color, borderRadius: 2, transition: 'width 0.3s ease',
      }} />
    </div>
  );
}

function NeuralState({ msg }: { msg: ChatMessage }) {
  const [expanded, setExpanded] = useState(false);
  const hasMetrics = msg.confidence !== undefined || msg.num_active_neurons !== undefined
    || msg.sparsity !== undefined || msg.inference_time_us !== undefined;
  if (!hasMetrics) return null;

  return (
    <div style={{ marginTop: 4 }}>
      <button
        onClick={() => setExpanded(!expanded)}
        style={{
          background: 'none', border: 'none', padding: 0, cursor: 'pointer',
          fontSize: 11, color: 'var(--text-muted)', display: 'flex',
          alignItems: 'center', gap: 4,
        }}
      >
        <span style={{
          transform: expanded ? 'rotate(90deg)' : 'rotate(0)',
          transition: 'transform 0.2s', display: 'inline-block', fontSize: 8,
        }}>&#9654;</span>
        Neural State
        {msg.time_ms !== undefined && (
          <span style={{ opacity: 0.6 }}> ({msg.time_ms.toFixed(0)}ms)</span>
        )}
      </button>
      {expanded && (
        <div className="chat-msg-meta" style={{ marginTop: 4 }}>
          {[
            msg.confidence !== undefined ? `Confidence: ${(msg.confidence * 100).toFixed(1)}%` : null,
            msg.num_active_neurons !== undefined ? `Active neurons: ${msg.num_active_neurons}` : null,
            msg.sparsity !== undefined ? `Sparsity: ${(msg.sparsity * 100).toFixed(1)}%` : null,
            msg.inference_time_us !== undefined ? `Inference: ${msg.inference_time_us}us` : null,
            msg.label ? `Label: ${msg.label}` : null,
          ].filter(Boolean).join(' | ')}
        </div>
      )}
    </div>
  );
}

const MODES = [
  { id: 'chat', label: 'Chat' },
  { id: 'teach', label: 'Teach' },
  { id: 'introspect', label: 'Introspect' },
  { id: 'speak', label: 'Speak' },
];

const PLACEHOLDERS: Record<string, string> = {
  chat: 'Message Athena...',
  teach: "Teach (e.g., 'cats: furry animals that purr')",
  introspect: 'Press Enter to ask about itself',
  speak: 'Say something for Athena to respond to...',
};

export function ChatArea({
  brainName, connected, send, setChatCallback,
  conversationId, brainId,
  micActive, camActive, onToggleMic, onToggleCam,
}: Props) {
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [text, setText] = useState('');
  const [mode, setMode] = useState('chat');
  const [showModeMenu, setShowModeMenu] = useState(false);
  const [avatarState, setAvatarState] = useState<AvatarState | null>(null);
  const [identity, setIdentity] = useState<AvatarIdentity | null>(null);
  const [showAvatar, setShowAvatar] = useState(true);
  const bottomRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // Fetch identity for avatar rendering
  useEffect(() => {
    getIdentity(brainId).then(r => setIdentity(r.data)).catch(() => {});
  }, [brainId]);

  // Load conversation history from REST
  useEffect(() => {
    setMessages([]);
    if (!conversationId) return;
    getConversation(conversationId).then(r => {
      const conv = r.data as ConversationDetail;
      if (conv.messages) {
        setMessages(conv.messages.map(m => ({
          id: ++msgIdCounter,
          sender: m.role === 'user' ? 'user' as const : 'brain' as const,
          text: m.text,
          mode: m.metadata?.mode as string | undefined,
        })));
      }
    }).catch(() => {});
  }, [conversationId]);

  const handleResponse = useCallback((msg: WSMessage) => {
    const resp = msg as unknown as {
      label?: string; confidence?: number; message?: string;
      time_ms?: number; explanation?: string; sparsity?: number;
      num_active_neurons?: number; inference_time_us?: number;
      output_vector?: number[]; cognitive_state?: Record<string, unknown>;
      spoken_text?: string; speech_confidence?: number; speech_fluency?: number;
      avatar?: AvatarState;
      identity_updated?: boolean; identity?: AvatarIdentity;
    };

    // Update avatar state if present
    if (resp.avatar) {
      setAvatarState(resp.avatar);
    }
    // Update identity if brain learned new appearance from conversation
    if (resp.identity_updated && resp.identity) {
      setIdentity(resp.identity);
    }

    // Build display text: prefer spoken_text for speech mode
    const displayText = resp.spoken_text
      ? `${resp.message || ''}\n\n${resp.spoken_text}`
      : resp.message || '';

    setMessages(prev => [...prev, {
      id: ++msgIdCounter,
      sender: 'brain' as const,
      text: displayText,
      label: resp.label,
      confidence: resp.confidence,
      time_ms: resp.time_ms,
      explanation: resp.explanation,
      sparsity: resp.sparsity,
      num_active_neurons: resp.num_active_neurons,
      inference_time_us: resp.inference_time_us,
      spoken_text: resp.spoken_text,
      speech_confidence: resp.speech_confidence,
      speech_fluency: resp.speech_fluency,
      avatar: resp.avatar,
    }]);
  }, []);

  useEffect(() => {
    setChatCallback(handleResponse);
    return () => setChatCallback(null);
  }, [setChatCallback, handleResponse]);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages.length]);

  const handleSend = (e: React.FormEvent) => {
    e.preventDefault();
    if (!text.trim() && mode !== 'introspect') return;
    const msgText = text.trim() || (mode === 'introspect' ? 'Tell me about yourself' : '');
    setMessages(prev => [...prev, {
      id: ++msgIdCounter, sender: 'user', text: msgText, mode,
    }]);
    send({
      type: 'chat', text: msgText, mode,
      conversation_id: conversationId,
    });
    setText('');
    setShowModeMenu(false);
  };

  return (
    <div className="chat-area">
      <div className="chat-area-header">
        <span className="chat-area-brain-name">{brainName}</span>
        <div className="chat-area-header-right">
          <button
            className={`chat-avatar-toggle ${showAvatar ? 'active' : ''}`}
            onClick={() => setShowAvatar(!showAvatar)}
            title={showAvatar ? 'Hide avatar' : 'Show avatar'}
          >
            Avatar
          </button>
          <span className={`chat-area-status ${connected ? 'connected' : 'disconnected'}`}>
            <span className={`status-dot ${connected ? 'ok' : 'err'}`} />
            {connected ? 'Connected' : 'Disconnected'}
          </span>
        </div>
      </div>

      <div className="chat-area-body">
      {showAvatar && (
        <AvatarFace avatar={avatarState} identity={identity} />
      )}

      <div className="chat-area-messages">
        {messages.length === 0 && (
          <div className="chat-area-empty">
            <div className="chat-area-empty-title">Start a conversation with {brainName}</div>
            <div className="chat-area-empty-subtitle">
              Powered by real neural computation
            </div>
          </div>
        )}
        {messages.map(m => (
          <div key={m.id} className={`chat-msg ${m.sender}`}>
            <div className="chat-msg-sender">
              {m.sender === 'brain' ? brainName : 'You'}
            </div>
            <div style={{ whiteSpace: 'pre-wrap' }}>{m.text}</div>
            {m.sender === 'brain' && m.spoken_text && (
              <div className="chat-msg-speech">
                <span className="chat-msg-speech-icon">&#128483;</span>
                <span>{m.spoken_text}</span>
                {m.speech_confidence !== undefined && (
                  <span className="chat-msg-speech-meta">
                    conf: {(m.speech_confidence * 100).toFixed(0)}%
                    {m.speech_fluency !== undefined && ` | fluency: ${(m.speech_fluency * 100).toFixed(0)}%`}
                  </span>
                )}
              </div>
            )}
            {m.sender === 'brain' && m.confidence !== undefined && (
              <ConfidenceBar confidence={m.confidence} />
            )}
            {m.sender === 'brain' && <NeuralState msg={m} />}
          </div>
        ))}
        <div ref={bottomRef} />
      </div>
      </div>{/* close chat-area-body */}

      <div className="chat-area-input-container">
        <div className="chat-area-input-toolbar">
          <div className="chat-mode-selector">
            <button
              className="chat-mode-current"
              onClick={() => setShowModeMenu(!showModeMenu)}
            >
              {MODES.find(m => m.id === mode)?.label || 'Chat'} &#9662;
            </button>
            {showModeMenu && (
              <div className="chat-mode-dropdown">
                {MODES.map(m => (
                  <button
                    key={m.id}
                    className={`chat-mode-option ${mode === m.id ? 'active' : ''}`}
                    onClick={() => { setMode(m.id); setShowModeMenu(false); }}
                  >
                    {m.label}
                  </button>
                ))}
              </div>
            )}
          </div>
          {onToggleMic && (
            <button
              className={`chat-media-btn ${micActive ? 'active' : ''}`}
              onClick={onToggleMic}
              title={micActive ? 'Stop microphone' : 'Start microphone'}
            >
              &#127908;
            </button>
          )}
          {onToggleCam && (
            <button
              className={`chat-media-btn ${camActive ? 'active' : ''}`}
              onClick={onToggleCam}
              title={camActive ? 'Stop camera' : 'Start camera'}
            >
              &#127909;
            </button>
          )}
        </div>
        <form className="chat-area-input-row" onSubmit={handleSend}>
          <input
            ref={inputRef}
            className="chat-area-input"
            value={text}
            onChange={e => setText(e.target.value)}
            placeholder={PLACEHOLDERS[mode] || 'Type a message...'}
            disabled={!connected}
          />
          <button className="btn btn-primary" type="submit" disabled={!connected}>
            Send
          </button>
        </form>
      </div>
    </div>
  );
}
