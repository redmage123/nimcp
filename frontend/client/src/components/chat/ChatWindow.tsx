import { useEffect, useRef, useState } from 'react';
import type { ChatMessage } from '../../types';

interface Props {
  messages: ChatMessage[];
}

function ConfidenceBar({ confidence }: { confidence: number }) {
  const color = confidence > 0.7 ? '#22c55e' : confidence > 0.4 ? '#eab308' : '#ef4444';
  return (
    <div style={{
      height: 3, borderRadius: 2, marginTop: 4,
      background: 'rgba(255,255,255,0.1)', overflow: 'hidden',
    }}>
      <div style={{
        height: '100%', width: `${Math.min(confidence * 100, 100)}%`,
        background: color, borderRadius: 2,
        transition: 'width 0.3s ease',
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
          {msg.cognitive_state && (
            <div style={{ marginTop: 2 }}>
              {[
                msg.cognitive_state.utilization !== undefined
                  ? `Utilization: ${(msg.cognitive_state.utilization * 100).toFixed(0)}%` : null,
                msg.cognitive_state.neuron_count
                  ? `Total neurons: ${msg.cognitive_state.neuron_count.toLocaleString()}` : null,
                msg.cognitive_state.total_learning_steps
                  ? `Learning steps: ${msg.cognitive_state.total_learning_steps}` : null,
              ].filter(Boolean).join(' | ')}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export function ChatWindow({ messages }: Props) {
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages.length]);

  return (
    <div className="chat-window">
      {messages.length === 0 && (
        <div className="empty-state" style={{ flex: 1 }}>
          <div>Chat with the brain — it will respond conversationally</div>
          <div style={{ fontSize: 11, color: 'var(--text-muted)' }}>
            The brain uses real neural computation to form its responses
          </div>
        </div>
      )}
      {messages.map((m) => (
        <div key={m.id} className={`chat-msg ${m.sender}`}>
          <div>{m.text}</div>
          {m.sender === 'brain' && m.confidence !== undefined && (
            <ConfidenceBar confidence={m.confidence} />
          )}
          {m.sender === 'brain' && <NeuralState msg={m} />}
          {m.mode && m.sender === 'user' && m.mode !== 'chat' && (
            <div className="chat-msg-meta">Mode: {m.mode}</div>
          )}
        </div>
      ))}
      <div ref={bottomRef} />
    </div>
  );
}
