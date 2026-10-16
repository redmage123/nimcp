import { useEffect, useRef, useCallback, useState } from 'react';
import type { WSMessage } from '../types';

type MessageHandler = (msg: WSMessage) => void;

export function useWebSocket(brainId: number | null, onMessage: MessageHandler) {
  const wsRef = useRef<WebSocket | null>(null);
  const [connected, setConnected] = useState(false);
  const onMessageRef = useRef(onMessage);
  onMessageRef.current = onMessage;

  useEffect(() => {
    if (brainId === null) return;

    let ws: WebSocket;
    let retryTimer: ReturnType<typeof setTimeout>;
    let alive = true;

    function connect() {
      const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
      let url = `${proto}//${location.host}/ws/${brainId}`;
      // Browser WebSocket API can't send Authorization headers, so pass
      // credentials as a query param for the server to check.
      try {
        const saved = sessionStorage.getItem('nimcp_auth');
        if (saved) {
          const { username, password } = JSON.parse(saved);
          url += `?auth=${encodeURIComponent(btoa(`${username}:${password}`))}`;
        }
      } catch { /* ignore */ }
      ws = new WebSocket(url);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        ws.send(JSON.stringify({ type: 'subscribe', channels: ['probe', 'training'] }));
      };

      ws.onmessage = (ev) => {
        try {
          const msg = JSON.parse(ev.data) as WSMessage;
          onMessageRef.current(msg);
        } catch { /* ignore malformed */ }
      };

      ws.onclose = () => {
        setConnected(false);
        if (alive) retryTimer = setTimeout(connect, 2000);
      };

      ws.onerror = () => ws.close();
    }

    connect();
    return () => {
      alive = false;
      clearTimeout(retryTimer);
      wsRef.current?.close();
      wsRef.current = null;
      setConnected(false);
    };
  }, [brainId]);

  const send = useCallback((data: Record<string, unknown>) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(data));
    }
  }, []);

  return { connected, send };
}
