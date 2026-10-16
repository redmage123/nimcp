import { useState, useEffect, useCallback, useMemo } from 'react';
import { Bar, Radar } from 'react-chartjs-2';
import type { BenchmarkResult, BenchmarkSummary, BenchmarkInfo, CognitiveMetrics } from '../../types';
import * as benchmarkApi from '../../services/benchmarkApi';
import '../../components/dashboard/chartConfig';

// ---------------------------------------------------------------------------
// Chart helpers
// ---------------------------------------------------------------------------

const NIMCP_COLOR = 'rgba(52, 152, 219, 0.8)';
const NIMCP_BORDER = 'rgb(52, 152, 219)';

function barOptions(title: string, yLabel: string, stacked = false) {
  return {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 300 },
    plugins: {
      legend: { display: true, position: 'top' as const, labels: { color: '#ccc', font: { size: 11 } } },
      title: { display: true, text: title, color: '#ddd', font: { size: 14 } },
    },
    scales: {
      x: {
        ticks: { color: '#9ca3af', maxRotation: 45 },
        grid: { color: 'rgba(255,255,255,0.06)' },
        stacked,
      },
      y: {
        title: { display: true, text: yLabel, color: '#9ca3af' },
        ticks: { color: '#9ca3af' },
        grid: { color: 'rgba(255,255,255,0.06)' },
        beginAtZero: true,
        stacked,
      },
    },
  } as const;
}

function radarOptions(title: string) {
  return {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 300 },
    plugins: {
      legend: { display: false },
      title: { display: true, text: title, color: '#ddd', font: { size: 14 } },
    },
    scales: {
      r: {
        angleLines: { color: 'rgba(255,255,255,0.1)' },
        grid: { color: 'rgba(255,255,255,0.1)' },
        pointLabels: { color: '#ccc', font: { size: 11 } },
        ticks: { color: '#999', backdropColor: 'transparent', stepSize: 0.2 },
        suggestedMin: 0,
        suggestedMax: 1,
      },
    },
  } as const;
}

// ---------------------------------------------------------------------------
// Chart components
// ---------------------------------------------------------------------------

function MLAccuracyChart({ results }: { results: BenchmarkResult[] }) {
  const mlResults = results.filter(r => r.category === 'ml');
  if (mlResults.length === 0) return null;

  const labels = mlResults.map(r => r.benchmark_id);

  // Collect all reference model names across results
  const refModels = new Set<string>();
  mlResults.forEach(r => Object.keys(r.reference_scores).forEach(k => refModels.add(k)));
  const refList = Array.from(refModels);

  const refColors = [
    'rgba(231, 76, 60, 0.6)',   // red
    'rgba(46, 204, 113, 0.6)',  // green
    'rgba(241, 196, 15, 0.6)',  // yellow
    'rgba(155, 89, 182, 0.6)',  // purple
    'rgba(230, 126, 34, 0.6)',  // orange
  ];
  const refBorders = [
    'rgb(231, 76, 60)',
    'rgb(46, 204, 113)',
    'rgb(241, 196, 15)',
    'rgb(155, 89, 182)',
    'rgb(230, 126, 34)',
  ];

  const datasets = [
    {
      label: 'NIMCP',
      data: mlResults.map(r => r.accuracy * 100),
      backgroundColor: NIMCP_COLOR,
      borderColor: NIMCP_BORDER,
      borderWidth: 1,
    },
    ...refList.map((model, i) => ({
      label: model.toUpperCase(),
      data: mlResults.map(r => (r.reference_scores[model] ?? 0) * 100),
      backgroundColor: refColors[i % refColors.length],
      borderColor: refBorders[i % refBorders.length],
      borderWidth: 1,
    })),
  ];

  return (
    <div className="chart-panel" style={{ marginBottom: 24 }}>
      <div style={{ height: 350 }}>
        <Bar data={{ labels, datasets }} options={barOptions('ML Classification — Accuracy (%)', 'Accuracy %')} />
      </div>
    </div>
  );
}


function GenAIAccuracyChart({ results }: { results: BenchmarkResult[] }) {
  const genaiResults = results.filter(r => r.category === 'generative_ai');
  if (genaiResults.length === 0) return null;

  const labels = genaiResults.map(r => r.benchmark_id);
  const refKeys = ['gpt4', 'claude35', 'llama70b', 'random'];
  const refLabels = ['GPT-4', 'Claude 3.5', 'Llama 70B', 'Random'];
  const refColors = [
    'rgba(46, 204, 113, 0.6)',
    'rgba(155, 89, 182, 0.6)',
    'rgba(241, 196, 15, 0.6)',
    'rgba(149, 165, 166, 0.4)',
  ];
  const refBorders = [
    'rgb(46, 204, 113)',
    'rgb(155, 89, 182)',
    'rgb(241, 196, 15)',
    'rgb(149, 165, 166)',
  ];

  const datasets = [
    {
      label: 'NIMCP',
      data: genaiResults.map(r => r.accuracy * 100),
      backgroundColor: NIMCP_COLOR,
      borderColor: NIMCP_BORDER,
      borderWidth: 1,
    },
    ...refKeys.map((key, i) => ({
      label: refLabels[i],
      data: genaiResults.map(r => (r.reference_scores[key] ?? 0) * 100),
      backgroundColor: refColors[i],
      borderColor: refBorders[i],
      borderWidth: 1,
    })),
  ];

  return (
    <div className="chart-panel" style={{ marginBottom: 24 }}>
      <p style={{ fontSize: 12, color: '#888', margin: '0 0 4px 8px' }}>
        Questions encoded as feature vectors — measures pattern learning, not language understanding.
      </p>
      <div style={{ height: 350 }}>
        <Bar data={{ labels, datasets }} options={barOptions('Generative AI Benchmarks — Accuracy (%)', 'Accuracy %')} />
      </div>
    </div>
  );
}


function CognitiveRadarChart({ results }: { results: BenchmarkResult[] }) {
  const cogResults = results.filter(r => r.cognitive !== null);
  if (cogResults.length === 0) return null;

  const cog = cogResults[0].cognitive as CognitiveMetrics;

  // Normalize metrics to [0, 1] for radar
  const radarLabels = [
    'Working Memory',
    'Theta-Gamma',
    'PAC Index',
    'Workspace',
    'Ethics',
    'Knowledge',
  ];

  const radarValues = [
    Math.min(cog.working_memory_capacity / 7, 1),       // 7 = Miller's number
    Math.min(cog.oscillation_coherence, 1),
    Math.min(cog.pac_index, 1),
    Math.min(cog.workspace_avg_strength, 1),
    Math.max(0, Math.min((cog.ethics_separation + 1) / 2, 1)), // map [-1,1] -> [0,1]
    Math.min(cog.knowledge_coverage, 1),
  ];

  const data = {
    labels: radarLabels,
    datasets: [{
      label: 'Cognitive Profile',
      data: radarValues,
      backgroundColor: 'rgba(52, 152, 219, 0.2)',
      borderColor: NIMCP_BORDER,
      borderWidth: 2,
      pointBackgroundColor: NIMCP_BORDER,
      pointBorderColor: '#fff',
      pointRadius: 4,
    }],
  };

  return (
    <div className="chart-panel" style={{ marginBottom: 24 }}>
      <div style={{ height: 350, maxWidth: 500, margin: '0 auto' }}>
        <Radar data={data} options={radarOptions('Cognitive Profile')} />
      </div>
    </div>
  );
}


function CognitiveMetricsCards({ results }: { results: BenchmarkResult[] }) {
  const cogResults = results.filter(r => r.cognitive !== null);
  if (cogResults.length === 0) return null;

  const cog = cogResults[0].cognitive as CognitiveMetrics;

  const metrics = [
    { label: 'Working Memory', value: `${cog.working_memory_capacity}/7`,
      detail: `${(cog.working_memory_occupancy * 100).toFixed(0)}% occupied` },
    { label: 'Theta-Gamma Coherence', value: cog.oscillation_coherence.toFixed(3),
      detail: 'Phase synchronization [0-1]' },
    { label: 'PAC Index', value: cog.pac_index.toFixed(3),
      detail: 'Phase-amplitude coupling [0-1]' },
    { label: 'Workspace Broadcasts', value: String(cog.workspace_broadcasts),
      detail: `Avg strength: ${cog.workspace_avg_strength.toFixed(3)}` },
    { label: 'Ethics Separation', value: cog.ethics_separation > 0 ? `+${cog.ethics_separation.toFixed(3)}` : cog.ethics_separation.toFixed(3),
      detail: `Harmful: ${cog.ethics_harmful_score.toFixed(2)} | Beneficial: ${cog.ethics_beneficial_score.toFixed(2)}` },
    { label: 'Knowledge Concepts', value: String(cog.knowledge_concepts),
      detail: `Coverage: ${(cog.knowledge_coverage * 100).toFixed(0)}%` },
  ];

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: 12, marginBottom: 24 }}>
      {metrics.map(m => (
        <div key={m.label} style={cardStyle}>
          <div style={{ fontSize: 11, color: '#999', textTransform: 'uppercase' }}>{m.label}</div>
          <div style={{ fontSize: 22, fontWeight: 'bold', margin: '4px 0' }}>{m.value}</div>
          <div style={{ fontSize: 11, color: '#777' }}>{m.detail}</div>
        </div>
      ))}
    </div>
  );
}


function EfficiencyChart({ results }: { results: BenchmarkResult[] }) {
  if (results.length === 0) return null;

  const labels = results.map(r => r.benchmark_id);

  const data = {
    labels,
    datasets: [
      {
        label: 'Train Time (s)',
        data: results.map(r => r.train_time_seconds),
        backgroundColor: 'rgba(52, 152, 219, 0.7)',
        borderColor: 'rgb(52, 152, 219)',
        borderWidth: 1,
        yAxisID: 'y',
      },
      {
        label: 'Inference (us)',
        data: results.map(r => r.inference_time_us),
        backgroundColor: 'rgba(46, 204, 113, 0.7)',
        borderColor: 'rgb(46, 204, 113)',
        borderWidth: 1,
        yAxisID: 'y1',
      },
    ],
  };

  const opts = {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 300 },
    plugins: {
      legend: { display: true, position: 'top' as const, labels: { color: '#ccc', font: { size: 11 } } },
      title: { display: true, text: 'Training & Inference Efficiency', color: '#ddd', font: { size: 14 } },
    },
    scales: {
      x: {
        ticks: { color: '#9ca3af', maxRotation: 45 },
        grid: { color: 'rgba(255,255,255,0.06)' },
      },
      y: {
        type: 'linear' as const,
        position: 'left' as const,
        title: { display: true, text: 'Train Time (s)', color: '#9ca3af' },
        ticks: { color: '#9ca3af' },
        grid: { color: 'rgba(255,255,255,0.06)' },
        beginAtZero: true,
      },
      y1: {
        type: 'linear' as const,
        position: 'right' as const,
        title: { display: true, text: 'Inference (us)', color: '#9ca3af' },
        ticks: { color: '#9ca3af' },
        grid: { drawOnChartArea: false },
        beginAtZero: true,
      },
    },
  } as const;

  return (
    <div className="chart-panel" style={{ marginBottom: 24 }}>
      <div style={{ height: 300 }}>
        <Bar data={data} options={opts} />
      </div>
    </div>
  );
}


function SparsityChart({ results }: { results: BenchmarkResult[] }) {
  if (results.length === 0) return null;

  const labels = results.map(r => r.benchmark_id);

  const data = {
    labels,
    datasets: [
      {
        label: 'Sparsity %',
        data: results.map(r => r.sparsity * 100),
        backgroundColor: 'rgba(241, 196, 15, 0.7)',
        borderColor: 'rgb(241, 196, 15)',
        borderWidth: 1,
      },
      {
        label: 'Active Neuron %',
        data: results.map(r => r.active_neuron_ratio * 100),
        backgroundColor: 'rgba(231, 76, 60, 0.7)',
        borderColor: 'rgb(231, 76, 60)',
        borderWidth: 1,
      },
    ],
  };

  return (
    <div className="chart-panel" style={{ marginBottom: 24 }}>
      <div style={{ height: 300 }}>
        <Bar data={data} options={barOptions('Network Efficiency', 'Percentage %')} />
      </div>
    </div>
  );
}


// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------

function BenchmarkControls({
  benchmarks,
  onRun,
  running,
  onStop,
}: {
  benchmarks: BenchmarkInfo[];
  onRun: (id: string, size: number, strategy: string, epochs: number) => void;
  running: boolean;
  onStop: () => void;
}) {
  const [benchmarkId, setBenchmarkId] = useState('all');
  const [brainSize, setBrainSize] = useState(1);
  const [strategy, setStrategy] = useState('auto');
  const [epochs, setEpochs] = useState(10);

  return (
    <div style={{ display: 'flex', gap: 12, alignItems: 'center', flexWrap: 'wrap', marginBottom: 20 }}>
      <label>
        Benchmark:
        <select value={benchmarkId} onChange={e => setBenchmarkId(e.target.value)}
                style={{ marginLeft: 6 }}>
          <option value="all">Run All</option>
          <optgroup label="ML Classification">
            {benchmarks.filter(b => b.category === 'ml').map(b => (
              <option key={b.id} value={b.id}>{b.name}</option>
            ))}
          </optgroup>
          <optgroup label="Generative AI (Adapted)">
            {benchmarks.filter(b => b.category === 'generative_ai').map(b => (
              <option key={b.id} value={b.id}>{b.name}</option>
            ))}
          </optgroup>
        </select>
      </label>

      <label>
        Size:
        <select value={brainSize} onChange={e => setBrainSize(Number(e.target.value))}
                style={{ marginLeft: 6 }}>
          <option value={0}>Tiny</option>
          <option value={1}>Small</option>
          <option value={2}>Medium</option>
        </select>
      </label>

      <label>
        Strategy:
        <select value={strategy} onChange={e => setStrategy(e.target.value)}
                style={{ marginLeft: 6 }}>
          <option value="auto">Auto</option>
          <option value="gradient">Gradient</option>
          <option value="hebbian">Hebbian</option>
          <option value="hybrid">Hybrid</option>
        </select>
      </label>

      <label>
        Epochs:
        <input type="number" min={1} max={100} value={epochs}
               onChange={e => setEpochs(Number(e.target.value))}
               style={{ width: 60, marginLeft: 6 }} />
      </label>

      {running ? (
        <button className="btn btn-danger" onClick={onStop}>Stop</button>
      ) : (
        <button className="btn btn-primary"
                onClick={() => onRun(benchmarkId, brainSize, strategy, epochs)}>
          Run Benchmark
        </button>
      )}
    </div>
  );
}


function SummaryBar({ summary }: { summary: BenchmarkSummary }) {
  return (
    <div style={{ display: 'flex', gap: 16, marginBottom: 20, flexWrap: 'wrap' }}>
      <div style={summaryCardStyle}>
        <div style={{ fontSize: 11, color: '#999' }}>ML Accuracy (avg)</div>
        <div style={{ fontSize: 24, fontWeight: 'bold' }}>
          {(summary.overall_ml_accuracy * 100).toFixed(1)}%
        </div>
      </div>
      <div style={summaryCardStyle}>
        <div style={{ fontSize: 11, color: '#999' }}>GenAI Accuracy (avg)</div>
        <div style={{ fontSize: 24, fontWeight: 'bold' }}>
          {(summary.overall_genai_accuracy * 100).toFixed(1)}%
        </div>
      </div>
      <div style={summaryCardStyle}>
        <div style={{ fontSize: 11, color: '#999' }}>Cognitive Health</div>
        <div style={{ fontSize: 24, fontWeight: 'bold' }}>
          {(summary.cognitive_health_score * 100).toFixed(0)}%
        </div>
      </div>
      <div style={summaryCardStyle}>
        <div style={{ fontSize: 11, color: '#999' }}>Benchmarks Run</div>
        <div style={{ fontSize: 24, fontWeight: 'bold' }}>
          {summary.results.length}
        </div>
      </div>
      {summary.timestamp && (
        <div style={{ ...summaryCardStyle, flex: 'none' }}>
          <div style={{ fontSize: 11, color: '#999' }}>Completed</div>
          <div style={{ fontSize: 13 }}>
            {new Date(summary.timestamp).toLocaleString()}
          </div>
        </div>
      )}
    </div>
  );
}


// ---------------------------------------------------------------------------
// Main page
// ---------------------------------------------------------------------------

export function BenchmarkPage() {
  const [benchmarks, setBenchmarks] = useState<BenchmarkInfo[]>([]);
  const [summary, setSummary] = useState<BenchmarkSummary | null>(null);
  const [running, setRunning] = useState(false);
  const [statusText, setStatusText] = useState('');

  const fetchBenchmarks = useCallback(() => {
    benchmarkApi.listBenchmarks().then(r => setBenchmarks(r.data)).catch(() => {});
  }, []);

  const fetchResults = useCallback(() => {
    benchmarkApi.getBenchmarkResults().then(r => {
      if (r.data && r.data.results && r.data.results.length > 0) {
        setSummary(r.data);
      }
    }).catch(() => {});
  }, []);

  useEffect(() => {
    fetchBenchmarks();
    fetchResults();
  }, [fetchBenchmarks, fetchResults]);

  // Poll status while running
  useEffect(() => {
    if (!running) return;
    const id = setInterval(() => {
      benchmarkApi.getBenchmarkStatus().then(r => {
        setRunning(r.data.running);
        if (r.data.current_benchmark) {
          setStatusText(`Running: ${r.data.current_benchmark}...`);
        }
        if (!r.data.running) {
          setStatusText('');
          fetchResults();
        }
      }).catch(() => {});
    }, 2000);
    return () => clearInterval(id);
  }, [running, fetchResults]);

  // Split results by category for charts
  const { mlResults, genaiResults } = useMemo(() => {
    if (!summary) return { mlResults: [], genaiResults: [] };
    return {
      mlResults: summary.results.filter(r => r.category === 'ml'),
      genaiResults: summary.results.filter(r => r.category === 'generative_ai'),
    };
  }, [summary]);

  const handleRun = async (id: string, size: number, strategy: string, epochs: number) => {
    try {
      setRunning(true);
      setStatusText(`Starting ${id}...`);
      await benchmarkApi.runBenchmark({
        benchmark_id: id,
        brain_size: size,
        strategy,
        epochs,
        include_cognitive: true,
      });
    } catch {
      setRunning(false);
      setStatusText('Failed to start benchmark');
    }
  };

  const handleStop = async () => {
    try {
      await benchmarkApi.stopBenchmark();
      setRunning(false);
      setStatusText('Stopped');
    } catch { /* */ }
  };

  return (
    <div style={{ padding: 16 }}>
      <h2 style={{ margin: '0 0 16px' }}>Benchmarks</h2>

      <BenchmarkControls
        benchmarks={benchmarks}
        onRun={handleRun}
        running={running}
        onStop={handleStop}
      />

      {running && (
        <div style={{
          padding: '10px 16px', background: '#1a3a4a', borderRadius: 8,
          marginBottom: 16, display: 'flex', alignItems: 'center', gap: 12,
        }}>
          <div style={{
            width: 16, height: 16, border: '2px solid #444',
            borderTopColor: '#3498db', borderRadius: '50%',
            animation: 'spin 1s linear infinite',
          }} />
          <span>{statusText || 'Running benchmarks...'}</span>
          <style>{`@keyframes spin { to { transform: rotate(360deg) } }`}</style>
        </div>
      )}

      {summary && !running && (
        <>
          <SummaryBar summary={summary} />

          {/* ML accuracy grouped bar chart */}
          {mlResults.length > 0 && (
            <MLAccuracyChart results={summary.results} />
          )}

          {/* GenAI accuracy grouped bar chart */}
          {genaiResults.length > 0 && (
            <GenAIAccuracyChart results={summary.results} />
          )}

          {/* Cognitive radar + metric cards side by side */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16, marginBottom: 8 }}>
            <div>
              <h3 style={{ margin: '0 0 8px' }}>Cognitive Metrics (Unique to NIMCP)</h3>
              <CognitiveRadarChart results={summary.results} />
            </div>
            <div>
              <h3 style={{ margin: '0 0 8px' }}>Cognitive Details</h3>
              <CognitiveMetricsCards results={summary.results} />
            </div>
          </div>

          {/* Efficiency charts */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
            <EfficiencyChart results={summary.results} />
            <SparsityChart results={summary.results} />
          </div>
        </>
      )}

      {!summary && !running && (
        <div style={{ color: '#888', textAlign: 'center', padding: 40 }}>
          No benchmark results yet. Click "Run Benchmark" to start.
        </div>
      )}
    </div>
  );
}


// ---------------------------------------------------------------------------
// Styles
// ---------------------------------------------------------------------------

const cardStyle: React.CSSProperties = {
  background: '#1a1a2e', padding: 16, borderRadius: 8, border: '1px solid #333',
};

const summaryCardStyle: React.CSSProperties = {
  background: '#1a1a2e', padding: '12px 20px', borderRadius: 8,
  border: '1px solid #333', flex: 1, minWidth: 140,
};
