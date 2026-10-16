/**
 * BenchmarkPanel Component
 * ========================
 *
 * WHAT: MNIST benchmark control panel with progress tracking
 * WHY:  Allow users to run standardized ML benchmarks on NIMCP
 * HOW:  Start benchmarks, poll progress, display comparative results
 */

import React, { useState, useEffect } from 'react';
import axios from 'axios';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  BarElement,
  Title,
  Tooltip,
  Legend,
} from 'chart.js';
import { Line, Bar } from 'react-chartjs-2';

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  BarElement,
  Title,
  Tooltip,
  Legend
);

const API_URL = '/api';

function BenchmarkPanel() {
  const [config, setConfig] = useState({
    num_neurons: 100000,
    epochs: 10,
    batch_size: 64,
    use_gpu: true
  });

  const [benchmarkRunning, setBenchmarkRunning] = useState(false);
  const [benchmarkStatus, setBenchmarkStatus] = useState(null);
  const [benchmarkResults, setBenchmarkResults] = useState(null);
  const [error, setError] = useState(null);

  // Poll benchmark status while running
  useEffect(() => {
    let interval;
    if (benchmarkRunning) {
      interval = setInterval(async () => {
        try {
          const response = await axios.get(`${API_URL}/benchmark/status`);
          if (response.data.success) {
            setBenchmarkStatus(response.data.status);

            if (response.data.status.status === 'completed') {
              // Fetch final results
              const resultsResponse = await axios.get(`${API_URL}/benchmark/results`);
              if (resultsResponse.data.success) {
                setBenchmarkResults(resultsResponse.data.results);
                setBenchmarkRunning(false);
              }
            } else if (response.data.status.status === 'failed') {
              setError(response.data.status.error || 'Benchmark failed');
              setBenchmarkRunning(false);
            }
          }
        } catch (err) {
          console.error('Failed to fetch benchmark status:', err);
        }
      }, 2000);
    }

    return () => {
      if (interval) clearInterval(interval);
    };
  }, [benchmarkRunning]);

  const startBenchmark = async () => {
    setError(null);
    setBenchmarkStatus(null);
    setBenchmarkResults(null);

    try {
      const response = await axios.post(`${API_URL}/benchmark/mnist/start`, config);
      if (response.data.success) {
        setBenchmarkRunning(true);
        setBenchmarkStatus({
          status: 'running',
          progress: 0,
          current_epoch: 0,
          total_epochs: config.epochs
        });
      } else {
        setError(response.data.message || 'Failed to start benchmark');
      }
    } catch (err) {
      setError(`Failed to start benchmark: ${err.message}`);
    }
  };

  const renderProgress = () => {
    if (!benchmarkStatus) return null;

    const progress = benchmarkStatus.progress || 0;
    const currentEpoch = benchmarkStatus.current_epoch || 0;
    const totalEpochs = benchmarkStatus.total_epochs || config.epochs;
    const currentMetrics = benchmarkStatus.current_metrics || {};

    return (
      <div className="benchmark-progress">
        <h3>Benchmark Progress</h3>

        <div className="progress-bar-container">
          <div className="progress-bar" style={{ width: `${progress}%` }}>
            <span className="progress-text">{progress.toFixed(1)}%</span>
          </div>
        </div>

        <div className="epoch-info">
          <span>Epoch {currentEpoch} / {totalEpochs}</span>
        </div>

        {currentMetrics.train_accuracy && (
          <div className="current-metrics">
            <div className="metric-item">
              <span className="label">Training Accuracy:</span>
              <span className="value">{currentMetrics.train_accuracy.toFixed(2)}%</span>
            </div>
            <div className="metric-item">
              <span className="label">Training Loss:</span>
              <span className="value">{currentMetrics.train_loss.toFixed(4)}</span>
            </div>
            {currentMetrics.test_accuracy && (
              <div className="metric-item">
                <span className="label">Test Accuracy:</span>
                <span className="value">{currentMetrics.test_accuracy.toFixed(2)}%</span>
              </div>
            )}
          </div>
        )}
      </div>
    );
  };

  const renderResults = () => {
    if (!benchmarkResults) return null;

    const metrics = benchmarkResults.metrics;
    const comparison = benchmarkResults.comparison;

    // Training accuracy chart
    const trainAccuracyData = {
      labels: metrics.train_accuracy.map((_, i) => i + 1),
      datasets: [
        {
          label: 'Training Accuracy',
          data: metrics.train_accuracy,
          borderColor: 'rgb(75, 192, 192)',
          backgroundColor: 'rgba(75, 192, 192, 0.1)',
          fill: true,
          tension: 0.4,
        }
      ]
    };

    // Training loss chart
    const trainLossData = {
      labels: metrics.train_loss.map((_, i) => i + 1),
      datasets: [
        {
          label: 'Training Loss',
          data: metrics.train_loss,
          borderColor: 'rgb(255, 99, 132)',
          backgroundColor: 'rgba(255, 99, 132, 0.1)',
          fill: true,
          tension: 0.4,
        }
      ]
    };

    // Comparison bar chart
    const comparisonData = {
      labels: ['NIMCP', 'PyTorch CNN (baseline)'],
      datasets: [
        {
          label: 'Test Accuracy (%)',
          data: [
            comparison?.nimcp?.accuracy || 0,
            comparison?.pytorch_cnn?.accuracy || 0
          ],
          backgroundColor: [
            'rgba(75, 192, 192, 0.6)',
            'rgba(153, 102, 255, 0.6)'
          ],
          borderColor: [
            'rgb(75, 192, 192)',
            'rgb(153, 102, 255)'
          ],
          borderWidth: 1
        }
      ]
    };

    const chartOptions = {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          position: 'top',
          labels: { color: '#e0e0e0' }
        }
      },
      scales: {
        x: {
          ticks: { color: '#e0e0e0' },
          grid: { color: 'rgba(255, 255, 255, 0.1)' }
        },
        y: {
          ticks: { color: '#e0e0e0' },
          grid: { color: 'rgba(255, 255, 255, 0.1)' },
          beginAtZero: true
        }
      }
    };

    return (
      <div className="benchmark-results">
        <h3>Benchmark Results</h3>

        <div className="results-summary">
          <div className="summary-card">
            <div className="summary-title">NIMCP Performance</div>
            <div className="summary-content">
              <div className="summary-metric">
                <span className="label">Final Test Accuracy:</span>
                <span className="value highlight">{metrics.test_accuracy[metrics.test_accuracy.length - 1].toFixed(2)}%</span>
              </div>
              <div className="summary-metric">
                <span className="label">Total Training Time:</span>
                <span className="value">{metrics.train_time.reduce((a, b) => a + b, 0).toFixed(2)}s</span>
              </div>
              <div className="summary-metric">
                <span className="label">Avg Inference Time:</span>
                <span className="value">{metrics.inference_time[metrics.inference_time.length - 1].toFixed(3)} ms</span>
              </div>
              <div className="summary-metric">
                <span className="label">Neurons:</span>
                <span className="value">{metrics.num_neurons.toLocaleString()}</span>
              </div>
              <div className="summary-metric">
                <span className="label">GPU Enabled:</span>
                <span className="value">{metrics.gpu_enabled ? '✅ Yes' : '❌ No'}</span>
              </div>
            </div>
          </div>

          {comparison && (
            <div className="summary-card">
              <div className="summary-title">Comparison vs Baseline</div>
              <div className="summary-content">
                <div className="summary-metric">
                  <span className="label">PyTorch CNN Accuracy:</span>
                  <span className="value">{comparison.pytorch_cnn.accuracy.toFixed(2)}%</span>
                </div>
                <div className="summary-metric">
                  <span className="label">Accuracy Difference:</span>
                  <span className={`value ${comparison.comparison.accuracy_diff >= 0 ? 'positive' : 'negative'}`}>
                    {comparison.comparison.accuracy_diff >= 0 ? '+' : ''}{comparison.comparison.accuracy_diff.toFixed(2)}%
                  </span>
                </div>
                <div className="summary-metric">
                  <span className="label">Training Time Ratio:</span>
                  <span className="value">{comparison.comparison.speedup.toFixed(2)}x</span>
                </div>
              </div>
            </div>
          )}
        </div>

        <div className="results-charts">
          <div className="chart-box">
            <h4>Training Accuracy</h4>
            <div className="chart-container-small">
              <Line data={trainAccuracyData} options={chartOptions} />
            </div>
          </div>

          <div className="chart-box">
            <h4>Training Loss</h4>
            <div className="chart-container-small">
              <Line data={trainLossData} options={chartOptions} />
            </div>
          </div>

          <div className="chart-box">
            <h4>Comparative Performance</h4>
            <div className="chart-container-small">
              <Bar data={comparisonData} options={chartOptions} />
            </div>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div className="benchmark-panel">
      <h2>🎯 MNIST Benchmark</h2>
      <p>Run standardized benchmark to prove NIMCP competitive performance</p>

      {error && (
        <div className="error-message">
          <span>⚠️ {error}</span>
          <button onClick={() => setError(null)}>✕</button>
        </div>
      )}

      {!benchmarkRunning && !benchmarkResults && (
        <div className="benchmark-config">
          <h3>Configuration</h3>

          <div className="config-grid">
            <div className="config-item">
              <label htmlFor="num_neurons">Number of Neurons</label>
              <select
                id="num_neurons"
                value={config.num_neurons}
                onChange={(e) => setConfig({ ...config, num_neurons: parseInt(e.target.value) })}
              >
                <option value={10000}>10,000 (Small)</option>
                <option value={50000}>50,000 (Medium)</option>
                <option value={100000}>100,000 (Large)</option>
                <option value={200000}>200,000 (Extra Large)</option>
              </select>
            </div>

            <div className="config-item">
              <label htmlFor="epochs">Training Epochs</label>
              <select
                id="epochs"
                value={config.epochs}
                onChange={(e) => setConfig({ ...config, epochs: parseInt(e.target.value) })}
              >
                <option value={5}>5 (Quick)</option>
                <option value={10}>10 (Standard)</option>
                <option value={20}>20 (Thorough)</option>
              </select>
            </div>

            <div className="config-item">
              <label htmlFor="batch_size">Batch Size</label>
              <select
                id="batch_size"
                value={config.batch_size}
                onChange={(e) => setConfig({ ...config, batch_size: parseInt(e.target.value) })}
              >
                <option value={32}>32</option>
                <option value={64}>64</option>
                <option value={128}>128</option>
              </select>
            </div>

            <div className="config-item">
              <label>
                <input
                  type="checkbox"
                  checked={config.use_gpu}
                  onChange={(e) => setConfig({ ...config, use_gpu: e.target.checked })}
                />
                Enable GPU Acceleration
              </label>
            </div>
          </div>

          <div className="benchmark-info">
            <h4>What will be tested:</h4>
            <ul>
              <li>MNIST digit classification (60K train, 10K test)</li>
              <li>Training time and convergence rate</li>
              <li>Inference speed and accuracy</li>
              <li>Comparison vs PyTorch CNN baseline (99.2% accuracy)</li>
            </ul>
          </div>

          <button
            className="btn btn-primary btn-large"
            onClick={startBenchmark}
          >
            🚀 Start Benchmark
          </button>
        </div>
      )}

      {benchmarkRunning && renderProgress()}
      {benchmarkResults && renderResults()}

      {benchmarkResults && (
        <button
          className="btn btn-secondary"
          onClick={() => {
            setBenchmarkResults(null);
            setBenchmarkStatus(null);
          }}
        >
          Run New Benchmark
        </button>
      )}
    </div>
  );
}

export default BenchmarkPanel;
