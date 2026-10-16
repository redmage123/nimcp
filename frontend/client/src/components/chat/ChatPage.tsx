import { useState, useEffect, useCallback } from 'react';
import type { ChatMessage, WSMessage } from '../../types';
import { ChatWindow } from './ChatWindow';
import { ChatInput } from './ChatInput';

interface Props {
  brainId: number | null;
  send: (data: Record<string, unknown>) => void;
  connected: boolean;
  setChatCallback: (cb: ((msg: WSMessage) => void) | null) => void;
}

let msgIdCounter = 0;

export function ChatPage({ brainId, send, connected, setChatCallback }: Props) {
  const [messages, setMessages] = useState<ChatMessage[]>([]);

  const handleResponse = useCallback((msg: WSMessage) => {
    const resp = msg as unknown as {
      label?: string; confidence?: number; message?: string;
      time_ms?: number; probe?: Record<string, unknown>;
      explanation?: string; sparsity?: number;
      num_active_neurons?: number; inference_time_us?: number;
      output_vector?: number[];
      cognitive_state?: { utilization?: number; neuron_count?: number;
        total_inferences?: number; total_learning_steps?: number };
    };
    setMessages(prev => [...prev, {
      id: ++msgIdCounter,
      sender: 'brain' as const,
      text: resp.message || JSON.stringify(resp.probe || {}),
      label: resp.label,
      confidence: resp.confidence,
      time_ms: resp.time_ms,
      explanation: resp.explanation,
      sparsity: resp.sparsity,
      num_active_neurons: resp.num_active_neurons,
      inference_time_us: resp.inference_time_us,
      output_vector: resp.output_vector,
      cognitive_state: resp.cognitive_state,
    }]);
  }, []);

  useEffect(() => {
    setChatCallback(handleResponse);
    return () => setChatCallback(null);
  }, [setChatCallback, handleResponse]);

  // Clear messages when brain changes
  useEffect(() => {
    setMessages([]);
  }, [brainId]);

  const handleSend = (text: string, mode: string) => {
    if (mode === 'introspect') {
      setMessages(prev => [...prev, {
        id: ++msgIdCounter, sender: 'user', text: text.trim() || 'Tell me about yourself', mode,
      }]);
      send({ type: 'chat', text: text.trim() || '', mode: 'introspect' });
      return;
    }
    if (!text.trim()) return;
    setMessages(prev => [...prev, {
      id: ++msgIdCounter, sender: 'user', text, mode,
    }]);
    send({ type: 'chat', text, mode });
  };

  if (brainId === null) {
    return <div className="empty-state">Select a brain to start chatting</div>;
  }

  return (
    <div className="chat-layout panel">
      <ChatWindow messages={messages} />
      <ChatInput onSend={handleSend} disabled={!connected} />
    </div>
  );
}
