import { useState, useCallback, useRef, useEffect } from 'react';
import type { BrainProbe, WSMessage } from '../types';
import { useWebSocket } from './useWebSocket';

const MAX_HISTORY = 200;

export function useBrainProbe(brainId: number | null) {
  const [probe, setProbe] = useState<BrainProbe | null>(null);
  const [history, setHistory] = useState<(BrainProbe & { timestamp: number })[]>([]);
  const [trainingProgress, setTrainingProgress] = useState<WSMessage | null>(null);
  const chatCallbackRef = useRef<((msg: WSMessage) => void) | null>(null);

  // Reset state when brain changes (fixes stale data after delete)
  useEffect(() => {
    setProbe(null);
    setHistory([]);
    setTrainingProgress(null);
  }, [brainId]);

  const handleMessage = useCallback((msg: WSMessage) => {
    if (msg.type === 'probe') {
      const p = msg as unknown as BrainProbe & { timestamp?: number };
      setProbe(p);
      setHistory((prev) => {
        const next = [...prev, { ...p, timestamp: Date.now() }];
        return next.length > MAX_HISTORY ? next.slice(-MAX_HISTORY) : next;
      });
    } else if (msg.type === 'training_progress' || msg.type === 'training_complete' || msg.type === 'training_error') {
      setTrainingProgress(msg);
    } else if (msg.type === 'chat_response') {
      chatCallbackRef.current?.(msg);
    }
  }, []);

  const { connected, send } = useWebSocket(brainId, handleMessage);

  const setChatCallback = useCallback((cb: ((msg: WSMessage) => void) | null) => {
    chatCallbackRef.current = cb;
  }, []);

  return { probe, history, trainingProgress, connected, send, setChatCallback };
}
