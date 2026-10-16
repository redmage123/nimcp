import Plot from 'react-plotly.js';
import type { ProbeConfig } from '../../types';

interface Props {
  config: ProbeConfig;
  data: Record<string, number[]>;
  currentValues?: Record<string, number>;
}

const COLORS = [
  '#667eea', '#764ba2', '#10b981', '#f59e0b',
  '#ef4444', '#3b82f6', '#8b5cf6', '#ec4899',
];

const DARK_BASE: Partial<Plotly.Layout> = {
  paper_bgcolor: 'transparent',
  plot_bgcolor: 'transparent',
  font: { color: '#9ca3af', size: 11 },
  margin: { t: 40, r: 20, b: 40, l: 50 },
  xaxis: {
    gridcolor: 'rgba(255,255,255,0.06)',
    zerolinecolor: 'rgba(255,255,255,0.1)',
  },
  yaxis: {
    gridcolor: 'rgba(255,255,255,0.06)',
    zerolinecolor: 'rgba(255,255,255,0.1)',
  },
};

function buildLineTraces(config: ProbeConfig, data: Record<string, number[]>): Plotly.Data[] {
  return config.metrics.map((metric, i) => {
    const y = data[metric] || [];
    return {
      x: y.map((_, idx) => idx),
      y,
      type: 'scatter' as const,
      mode: 'lines' as const,
      name: metric,
      line: { color: COLORS[i % COLORS.length], width: 2 },
    };
  });
}

function buildBarTraces(config: ProbeConfig, currentValues: Record<string, number>): Plotly.Data[] {
  return [{
    x: config.metrics,
    y: config.metrics.map(m => currentValues[m] || 0),
    type: 'bar' as const,
    marker: {
      color: config.metrics.map((_, i) => COLORS[i % COLORS.length]),
    },
  }];
}

function buildGaugeTrace(config: ProbeConfig, currentValues: Record<string, number>): Plotly.Data[] {
  const metric = config.metrics[0] || '';
  const value = currentValues[metric] || 0;
  const threshold = config.alert_thresholds?.[metric];
  return [{
    type: 'indicator' as const,
    mode: 'gauge+number' as const,
    value,
    title: { text: metric, font: { size: 12, color: '#e0e0e0' } },
    gauge: {
      axis: { range: [0, Math.max(value * 1.5, 100)] },
      bar: { color: COLORS[0] },
      bgcolor: 'rgba(255,255,255,0.06)',
      borderwidth: 0,
      threshold: threshold ? {
        line: { color: '#ef4444', width: 3 },
        thickness: 0.8,
        value: threshold,
      } : undefined,
    },
  } as Plotly.Data];
}

function buildThresholdShapes(config: ProbeConfig): Partial<Plotly.Shape>[] {
  if (!config.alert_thresholds) return [];
  return Object.values(config.alert_thresholds).map(value => ({
    type: 'line' as const,
    x0: 0,
    x1: 1,
    xref: 'paper' as const,
    y0: value,
    y1: value,
    line: { color: '#ef4444', width: 1, dash: 'dash' as const },
  }));
}

export function PlotlyProbeChart({ config, data, currentValues = {} }: Props) {
  let traces: Plotly.Data[];
  const layout: Partial<Plotly.Layout> = {
    ...DARK_BASE,
    title: { text: config.name, font: { size: 14, color: '#e0e0e0' } },
    height: 260,
  };

  switch (config.chart_type) {
    case 'bar':
      traces = buildBarTraces(config, currentValues);
      break;
    case 'gauge':
      traces = buildGaugeTrace(config, currentValues);
      layout.margin = { t: 40, r: 20, b: 20, l: 20 };
      break;
    default: // line
      traces = buildLineTraces(config, data);
      layout.shapes = buildThresholdShapes(config) as Plotly.Layout['shapes'];
      break;
  }

  // Alert indicator: red glow if any threshold breached
  const alertActive = config.alert_thresholds && Object.entries(config.alert_thresholds).some(
    ([metric, threshold]) => (currentValues[metric] || 0) > threshold
  );

  return (
    <div className={`chart-panel probe-card ${alertActive ? 'probe-alert' : ''}`}>
      <Plot
        data={traces}
        layout={layout}
        config={{ responsive: true, displayModeBar: false }}
        style={{ width: '100%', height: '100%' }}
      />
    </div>
  );
}
