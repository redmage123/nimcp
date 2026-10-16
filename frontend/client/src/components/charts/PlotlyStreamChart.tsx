import { useRef, useEffect, useCallback } from 'react';
import Plot from 'react-plotly.js';

interface Props {
  title: string;
  metrics: string[];
  data: Record<string, number[]>;
  maxPoints?: number;
}

const COLORS = [
  '#667eea', '#764ba2', '#10b981', '#f59e0b',
  '#ef4444', '#3b82f6', '#8b5cf6', '#ec4899',
];

const DARK_LAYOUT: Partial<Plotly.Layout> = {
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
  legend: {
    orientation: 'h',
    y: -0.15,
    font: { size: 10 },
  },
};

export function PlotlyStreamChart({ title, metrics, data, maxPoints = 200 }: Props) {
  const traces: Plotly.Data[] = metrics.map((metric, i) => {
    const y = data[metric] || [];
    const x = y.map((_, idx) => idx);
    return {
      x: x.slice(-maxPoints),
      y: y.slice(-maxPoints),
      type: 'scatter' as const,
      mode: 'lines' as const,
      name: metric,
      line: { color: COLORS[i % COLORS.length], width: 2 },
    };
  });

  const layout: Partial<Plotly.Layout> = {
    ...DARK_LAYOUT,
    title: { text: title, font: { size: 14, color: '#e0e0e0' } },
    height: 280,
  };

  return (
    <div className="chart-panel">
      <Plot
        data={traces}
        layout={layout}
        config={{ responsive: true, displayModeBar: false }}
        style={{ width: '100%', height: '100%' }}
      />
    </div>
  );
}
