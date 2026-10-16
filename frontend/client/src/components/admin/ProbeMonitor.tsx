import { useState, useEffect, useRef, useCallback } from 'react';
import type { ProbeData } from '../../types';
import { fetchLiveProbeData } from '../../services/brainApi';

const HISTORY_SIZE = 30;
const REFRESH_MS = 2000;
const VOCAB_CAPACITY = 2048;

type Trend = 'up' | 'down' | 'flat';

interface MetricHistory {
  values: number[];
  trend: Trend;
}

function computeTrend(values: number[]): Trend {
  if (values.length < 2) return 'flat';
  const recent = values[values.length - 1];
  const prev = values[values.length - 2];
  const delta = recent - prev;
  const threshold = Math.max(Math.abs(prev) * 0.005, 1e-6);
  if (delta > threshold) return 'up';
  if (delta < -threshold) return 'down';
  return 'flat';
}

function trendArrow(trend: Trend): string {
  if (trend === 'up') return '\u25B2';
  if (trend === 'down') return '\u25BC';
  return '\u25CF';
}

function trendClass(trend: Trend): string {
  if (trend === 'up') return 'pm-trend-up';
  if (trend === 'down') return 'pm-trend-down';
  return 'pm-trend-flat';
}

function formatFloat(v: number | undefined | null, decimals = 4): string {
  if (v === undefined || v === null || !isFinite(v)) return '--';
  return v.toFixed(decimals);
}

function formatPercent(v: number | undefined | null): string {
  if (v === undefined || v === null || !isFinite(v)) return '--';
  return (v * 100).toFixed(1) + '%';
}

function formatInt(v: number | undefined | null): string {
  if (v === undefined || v === null) return '--';
  return Math.round(v).toLocaleString();
}

function formatBytes(bytes: number | undefined | null): string {
  if (bytes === undefined || bytes === null || bytes <= 0) return '--';
  if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(2) + ' GB';
  if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
  if (bytes >= 1024) return (bytes / 1024).toFixed(0) + ' KB';
  return bytes + ' B';
}

/** Health color for a metric: green=good, yellow=warning, red=bad */
function healthColor(value: number | undefined | null, thresholds: { green: number; yellow: number }, higherIsBetter: boolean): string {
  if (value === undefined || value === null || !isFinite(value)) return 'var(--text-muted)';
  if (higherIsBetter) {
    if (value >= thresholds.green) return 'var(--success)';
    if (value >= thresholds.yellow) return 'var(--warning)';
    return 'var(--danger)';
  } else {
    if (value <= thresholds.green) return 'var(--success)';
    if (value <= thresholds.yellow) return 'var(--warning)';
    return 'var(--danger)';
  }
}

/** Tiny inline sparkline rendered as an SVG */
function Sparkline({ values }: { values: number[] }) {
  if (values.length < 2) return null;
  const w = 60;
  const h = 20;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  const points = values.map((v, i) => {
    const x = (i / (values.length - 1)) * w;
    const y = h - ((v - min) / range) * h;
    return `${x},${y}`;
  }).join(' ');

  return (
    <svg className="pm-sparkline" width={w} height={h} viewBox={`0 0 ${w} ${h}`}>
      <polyline
        fill="none"
        stroke="var(--primary)"
        strokeWidth="1.5"
        points={points}
      />
    </svg>
  );
}

/** Single metric card */
function MetricCard({ label, value, history, color }: {
  label: string;
  value: string;
  history?: MetricHistory;
  color?: string;
}) {
  return (
    <div className="pm-card">
      <div className="pm-card-label">{label}</div>
      <div className="pm-card-value-row">
        <span className="pm-card-value" style={color ? { color } : undefined}>{value}</span>
        {history && (
          <span className={`pm-card-trend ${trendClass(history.trend)}`}>
            {trendArrow(history.trend)}
          </span>
        )}
      </div>
      {history && history.values.length >= 2 && (
        <Sparkline values={history.values} />
      )}
    </div>
  );
}

/** Progress bar with label */
function ProgressCard({ label, value, max, displayValue, color }: {
  label: string;
  value: number;
  max: number;
  displayValue: string;
  color?: string;
}) {
  const pct = max > 0 ? Math.min(100, (value / max) * 100) : 0;
  return (
    <div className="pm-card pm-card-wide">
      <div className="pm-card-label">{label}</div>
      <div className="pm-card-value" style={color ? { color } : undefined}>{displayValue}</div>
      <div className="pm-progress-bar-bg">
        <div
          className="pm-progress-bar-fill"
          style={{ width: `${pct}%`, background: color || 'var(--primary)' }}
        />
      </div>
    </div>
  );
}

export function ProbeMonitor({ brainId = 0 }: { brainId?: number }) {
  const [probe, setProbe] = useState<ProbeData | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [lastUpdate, setLastUpdate] = useState<number>(0);
  const historyRef = useRef<Record<string, MetricHistory>>({});

  const getHistory = useCallback((key: string): MetricHistory => {
    if (!historyRef.current[key]) {
      historyRef.current[key] = { values: [], trend: 'flat' };
    }
    return historyRef.current[key];
  }, []);

  const pushValue = useCallback((key: string, value: number | undefined | null): MetricHistory => {
    const h = getHistory(key);
    if (value !== undefined && value !== null && isFinite(value)) {
      h.values.push(value);
      if (h.values.length > HISTORY_SIZE) {
        h.values = h.values.slice(-HISTORY_SIZE);
      }
      h.trend = computeTrend(h.values);
    }
    return { ...h };
  }, [getHistory]);

  useEffect(() => {
    let cancelled = false;

    const poll = async () => {
      try {
        const resp = await fetchLiveProbeData(brainId);
        if (cancelled) return;
        setProbe(resp.data);
        setError(null);
        setLastUpdate(Date.now());
      } catch (err: unknown) {
        if (cancelled) return;
        const msg = err instanceof Error ? err.message : 'Failed to fetch probe data';
        setError(msg);
      }
    };

    poll();
    const id = setInterval(poll, REFRESH_MS);
    return () => { cancelled = true; clearInterval(id); };
  }, [brainId]);

  // Build histories whenever probe changes
  const histories = useRef<Record<string, MetricHistory>>({});
  if (probe) {
    const keys: [string, number | undefined | null][] = [
      ['weight_l2_norm', probe.weight_l2_norm],
      ['weight_mean_abs', probe.weight_mean_abs],
      ['weight_max_abs', probe.weight_max_abs],
      ['ema_gradient_norm', probe.ema_gradient_norm],
      ['ema_loss', probe.ema_loss],
      ['last_gradient_norm', probe.last_gradient_norm],
      ['accuracy', probe.accuracy],
      ['mean_label_accuracy', probe.mean_label_accuracy],
      ['worst_label_accuracy', probe.worst_label_accuracy],
      ['learning_velocity', probe.learning_velocity],
      ['confidence_calibration', probe.confidence_calibration],
      ['prediction_entropy', probe.prediction_entropy],
      ['memory_rss_bytes', probe.memory_rss_bytes],
      ['gpu_vram_bytes', probe.gpu_vram_bytes],
      ['neuron_utilization', probe.neuron_utilization],
      ['immune_inflammation', probe.immune_inflammation],
      ['immune_total_exceptions', probe.immune_total_exceptions],
      ['vocabulary_size', probe.vocabulary_size],
      ['response_diversity', probe.response_diversity],
      ['total_learning_steps', probe.total_learning_steps],
      ['num_labels_tracked', probe.num_labels_tracked],
      // Language & Emotion
      ['emotion_valence', probe.emotion_valence as number | undefined],
      ['emotion_arousal', probe.emotion_arousal as number | undefined],
      ['emotion_intensity', probe.emotion_intensity as number | undefined],
      ['speech_pitch_hz', probe.speech_pitch_hz as number | undefined],
    ];
    const next: Record<string, MetricHistory> = {};
    for (const [k, v] of keys) {
      next[k] = pushValue(k, v);
    }
    histories.current = next;
  }
  const h = histories.current;

  const secondsAgo = lastUpdate > 0 ? Math.round((Date.now() - lastUpdate) / 1000) : null;

  return (
    <div className="pm-root">
      <div className="pm-header">
        <span className="pm-header-title">Live Brain Probe Monitor</span>
        <span className="pm-header-status">
          {error ? (
            <span style={{ color: 'var(--danger)' }}>Error: {error}</span>
          ) : probe ? (
            <>
              <span className="status-dot ok" />
              Updated {secondsAgo !== null ? `${secondsAgo}s ago` : '--'}
              {' | '}
              Brain #{brainId}
              {' | '}
              {formatInt(probe.num_neurons)} neurons
            </>
          ) : (
            <span style={{ color: 'var(--text-muted)' }}>Connecting...</span>
          )}
        </span>
      </div>

      {!probe && !error && (
        <div className="pm-loading">Loading probe data...</div>
      )}

      {probe && (
        <div className="pm-sections">
          {/* Section 1: Training Dynamics */}
          <div className="pm-section">
            <div className="pm-section-title">Training Dynamics</div>
            <div className="pm-card-grid">
              <MetricCard
                label="Weight L2 Norm"
                value={formatFloat(probe.weight_l2_norm, 2)}
                history={h.weight_l2_norm}
              />
              <MetricCard
                label="Mean |W|"
                value={formatFloat(probe.weight_mean_abs, 6)}
                history={h.weight_mean_abs}
              />
              <MetricCard
                label="Max |W|"
                value={formatFloat(probe.weight_max_abs, 4)}
                history={h.weight_max_abs}
              />
              <MetricCard
                label="Gradient Norm"
                value={formatFloat(probe.last_gradient_norm, 4)}
                history={h.last_gradient_norm}
              />
              <MetricCard
                label="EMA Grad Norm"
                value={formatFloat(probe.ema_gradient_norm, 4)}
                history={h.ema_gradient_norm}
              />
              <MetricCard
                label="EMA Loss"
                value={formatFloat(probe.ema_loss, 4)}
                history={h.ema_loss}
              />
            </div>
            {probe.layer_grad_norms && probe.layer_grad_norms.length > 0 && (
              <div className="pm-layer-norms">
                <span className="pm-card-label">Per-Layer Gradient Norms:</span>
                <div className="pm-layer-norms-row">
                  {probe.layer_grad_norms.map((v, i) => (
                    <span key={i} className="pm-layer-norm-chip">
                      L{i}: {formatFloat(v, 4)}
                    </span>
                  ))}
                </div>
              </div>
            )}
          </div>

          {/* Section 2: Learning Quality */}
          <div className="pm-section">
            <div className="pm-section-title">Learning Quality</div>
            <div className="pm-card-grid">
              <MetricCard
                label="Accuracy"
                value={formatPercent(probe.accuracy)}
                history={h.accuracy}
                color={healthColor(probe.accuracy, { green: 0.8, yellow: 0.5 }, true)}
              />
              <MetricCard
                label="Mean Label Acc"
                value={formatPercent(probe.mean_label_accuracy)}
                history={h.mean_label_accuracy}
                color={healthColor(probe.mean_label_accuracy, { green: 0.7, yellow: 0.4 }, true)}
              />
              <MetricCard
                label="Worst Label Acc"
                value={formatPercent(probe.worst_label_accuracy)}
                history={h.worst_label_accuracy}
                color={healthColor(probe.worst_label_accuracy, { green: 0.5, yellow: 0.2 }, true)}
              />
              <MetricCard
                label="Learning Velocity"
                value={formatFloat(probe.learning_velocity, 4)}
                history={h.learning_velocity}
                color={healthColor(probe.learning_velocity, { green: 0.001, yellow: -0.01 }, true)}
              />
              <MetricCard
                label="Confidence Cal."
                value={formatFloat(probe.confidence_calibration, 3)}
                history={h.confidence_calibration}
                color={healthColor(probe.confidence_calibration, { green: 0.1, yellow: 0.3 }, false)}
              />
              <MetricCard
                label="Pred Entropy"
                value={formatFloat(probe.prediction_entropy, 3)}
                history={h.prediction_entropy}
              />
            </div>
          </div>

          {/* Section 3: Brain Health */}
          <div className="pm-section">
            <div className="pm-section-title">Brain Health</div>
            <div className="pm-card-grid">
              <ProgressCard
                label="Memory RSS"
                value={probe.memory_rss_bytes || 0}
                max={16 * 1073741824}
                displayValue={formatBytes(probe.memory_rss_bytes)}
                color={healthColor(
                  probe.memory_rss_bytes ? probe.memory_rss_bytes / 1073741824 : 0,
                  { green: 8, yellow: 12 },
                  false
                )}
              />
              <ProgressCard
                label="GPU VRAM"
                value={probe.gpu_vram_bytes || 0}
                max={20 * 1073741824}
                displayValue={formatBytes(probe.gpu_vram_bytes)}
                color={probe.gpu_available ? 'var(--info)' : 'var(--text-muted)'}
              />
              <MetricCard
                label="Neuron Utilization"
                value={formatPercent(probe.neuron_utilization)}
                history={h.neuron_utilization}
                color={healthColor(probe.neuron_utilization, { green: 0.3, yellow: 0.1 }, true)}
              />
              <MetricCard
                label="Inflammation"
                value={formatFloat(probe.immune_inflammation, 2)}
                history={h.immune_inflammation}
                color={healthColor(probe.immune_inflammation, { green: 1.0, yellow: 2.5 }, false)}
              />
              <MetricCard
                label="Total Exceptions"
                value={formatInt(probe.immune_total_exceptions)}
                history={h.immune_total_exceptions}
                color={healthColor(probe.immune_total_exceptions, { green: 10, yellow: 100 }, false)}
              />
              <MetricCard
                label="GPU"
                value={probe.gpu_available ? 'Available' : 'N/A'}
                color={probe.gpu_available ? 'var(--success)' : 'var(--text-muted)'}
              />
            </div>
          </div>

          {/* Section 4: Language & Emotion */}
          <div className="pm-section">
            <div className="pm-section-title">Language & Emotion</div>
            <div className="pm-card-grid">
              <MetricCard
                label="Valence"
                value={formatFloat(probe.emotion_valence as number | undefined, 2)}
                history={h.emotion_valence}
                color={healthColor(probe.emotion_valence as number | undefined, { green: 0.3, yellow: -0.3 }, true)}
              />
              <MetricCard
                label="Arousal"
                value={formatFloat(probe.emotion_arousal as number | undefined, 2)}
                history={h.emotion_arousal}
              />
              <MetricCard
                label="Emotion"
                value={
                  (['happy','sad','angry','afraid','disgust','surprise',
                    'interest','confused','frustrated','bored','proud',
                    'ashamed','enraged','hateful','despairing','panicked',
                    'calm','contempt','neutral'] as const)[
                    (probe.emotion_id as number | undefined) ?? 18
                  ] || 'neutral'
                }
                color="var(--primary)"
              />
              <MetricCard
                label="Intensity"
                value={formatPercent(probe.emotion_intensity as number | undefined)}
                history={h.emotion_intensity}
              />
              <MetricCard
                label="Speaking"
                value={(probe.is_speaking as boolean | undefined) ? 'Yes' : 'No'}
                color={(probe.is_speaking as boolean | undefined) ? 'var(--success)' : 'var(--text-muted)'}
              />
              <MetricCard
                label="Pitch"
                value={`${formatFloat(probe.speech_pitch_hz as number | undefined, 0)} Hz`}
                history={h.speech_pitch_hz}
              />
            </div>
          </div>

          {/* Section 5: Conversational Readiness */}
          <div className="pm-section">
            <div className="pm-section-title">Conversational Readiness</div>
            <div className="pm-card-grid">
              <ProgressCard
                label="Vocabulary Size"
                value={probe.vocabulary_size || probe.num_labels_tracked || 0}
                max={VOCAB_CAPACITY}
                displayValue={`${formatInt(probe.vocabulary_size || probe.num_labels_tracked)} / ${VOCAB_CAPACITY}`}
                color="var(--primary)"
              />
              <MetricCard
                label="Response Diversity"
                value={formatFloat(probe.response_diversity || probe.prediction_entropy, 3)}
                history={h.response_diversity}
              />
              <MetricCard
                label="Total Steps"
                value={formatInt(probe.total_learning_steps)}
                history={h.total_learning_steps}
              />
              <MetricCard
                label="Synapse Growth"
                value={formatInt(probe.synapse_growth)}
              />
              <MetricCard
                label="Labels Tracked"
                value={formatInt(probe.num_labels_tracked)}
                history={h.num_labels_tracked}
              />
              <MetricCard
                label="Synapses"
                value={formatInt(probe.num_synapses)}
              />
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
