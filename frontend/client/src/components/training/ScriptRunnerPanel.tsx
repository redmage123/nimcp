import { useState, useEffect, useRef } from 'react';
import type { ScriptInfo, ScriptStatus } from '../../types';
import * as scriptApi from '../../services/scriptApi';

interface Props {
  brainId: number;
}

export function ScriptRunnerPanel({ brainId }: Props) {
  const [scripts, setScripts] = useState<ScriptInfo[]>([]);
  const [selectedScript, setSelectedScript] = useState('');
  const [status, setStatus] = useState<ScriptStatus | null>(null);
  const [error, setError] = useState('');
  const outputRef = useRef<HTMLPreElement>(null);
  const pollRef = useRef<ReturnType<typeof setInterval> | null>(null);

  useEffect(() => {
    scriptApi.listScripts()
      .then(r => {
        setScripts(r.data);
        const avail = r.data.filter(s => s.exists);
        if (avail.length > 0 && !selectedScript) setSelectedScript(avail[0].id);
      })
      .catch(() => {});
  }, []);

  const isRunning = status?.status === 'running' || status?.status === 'starting';

  useEffect(() => {
    // Poll while running
    if (isRunning) {
      pollRef.current = setInterval(() => {
        scriptApi.scriptStatus().then(r => {
          setStatus(r.data);
        }).catch(() => {});
      }, 1000);
    }
    return () => {
      if (pollRef.current) clearInterval(pollRef.current);
    };
  }, [isRunning]);

  // Auto-scroll output
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [status?.stdout_lines]);

  const handleRun = async () => {
    if (!selectedScript) return;
    setError('');
    try {
      const r = await scriptApi.runScript(selectedScript, brainId);
      setStatus(r.data);
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : 'Failed to start script';
      setError(msg);
    }
  };

  const handleStop = async () => {
    try {
      await scriptApi.stopScript();
      setStatus(prev => prev ? { ...prev, status: 'stopped' } : null);
    } catch {
      setError('Failed to stop script');
    }
  };

  const statusColor = !status ? 'var(--text-muted)'
    : status.status === 'running' ? 'var(--success)'
    : status.status === 'completed' ? 'var(--accent)'
    : status.status === 'failed' ? 'var(--error)'
    : 'var(--warning)';

  return (
    <div className="card">
      <h3 className="card-title">Script Runner</h3>
      <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 12, flexWrap: 'wrap' }}>
        <select
          value={selectedScript}
          onChange={(e) => setSelectedScript(e.target.value)}
          disabled={isRunning}
          style={{ flex: 1, minWidth: 180 }}
        >
          {scripts.filter(s => s.exists).map(s => (
            <option key={s.id} value={s.id}>{s.name}</option>
          ))}
        </select>
        {!isRunning ? (
          <button className="btn btn-primary" onClick={handleRun} disabled={!selectedScript}>
            Run
          </button>
        ) : (
          <button className="btn btn-outline" onClick={handleStop} style={{ borderColor: 'var(--error)', color: 'var(--error)' }}>
            Stop
          </button>
        )}
        {status && (
          <span style={{ fontSize: 12, color: statusColor, fontWeight: 600, textTransform: 'uppercase' }}>
            {status.status}
          </span>
        )}
      </div>
      {error && <div style={{ color: 'var(--error)', fontSize: 12, marginBottom: 8 }}>{error}</div>}
      {status?.stdout_lines && status.stdout_lines.length > 0 && (
        <pre
          ref={outputRef}
          style={{
            background: '#1a1a2e',
            color: '#e0e0e0',
            padding: 12,
            borderRadius: 6,
            fontSize: 11,
            lineHeight: 1.5,
            maxHeight: 240,
            overflow: 'auto',
            margin: 0,
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-all',
          }}
        >
          {status.stdout_lines.join('\n')}
        </pre>
      )}
      {status && status.exit_code !== null && status.exit_code !== undefined && (
        <div style={{ fontSize: 12, marginTop: 8, color: 'var(--text-muted)' }}>
          Exit code: {status.exit_code}
          {status.total_lines ? ` | ${status.total_lines} total lines` : ''}
        </div>
      )}
    </div>
  );
}
