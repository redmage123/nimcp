/**
 * NIMCP Web Demo - React Frontend v2.7.0
 * =======================================
 *
 * WHAT: Interactive web interface for NIMCP brain demonstration
 * WHY:  Visualize brain training, predictions, and metrics in real-time
 * HOW:  React app with Chart.js, communicating with Flask backend
 */

import React, { useState, useEffect } from 'react';
import TrainingPanel from './components/TrainingPanel';
import PredictionPanel from './components/PredictionPanel';
import MetricsDashboard from './components/MetricsDashboard';
import BenchmarkPanel from './components/BenchmarkPanel';
import NetworkVisualization from './components/NetworkVisualization';
import axios from 'axios';
import './App.css';

const API_URL = '/api';

function App() {
  const [brainInitialized, setBrainInitialized] = useState(false);
  const [primaryBrainId, setPrimaryBrainId] = useState(null);
  const [metrics, setMetrics] = useState(null);
  const [trainingHistory, setTrainingHistory] = useState([]);
  const [predictionHistory, setPredictionHistory] = useState([]);
  const [dataset, setDataset] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [activeTab, setActiveTab] = useState('demo'); // 'demo', 'docs', 'benchmark', 'visualization'

  // Dataset selection
  const [availableDatasets, setAvailableDatasets] = useState([]);
  const [selectedDataset, setSelectedDataset] = useState('iris'); // Default to Iris
  const [datasetInfo, setDatasetInfo] = useState(null);

  // Poll metrics every 2 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      if (brainInitialized) {
        fetchMetrics();
      }
    }, 2000);

    return () => clearInterval(interval);
  }, [brainInitialized]);

  // Fetch available datasets on mount
  useEffect(() => {
    const fetchAvailableDatasets = async () => {
      try {
        const response = await axios.get(`${API_URL}/datasets`);
        if (response.data.success) {
          setAvailableDatasets(response.data.datasets);
        }
      } catch (err) {
        console.error('Failed to fetch available datasets:', err);
      }
    };
    fetchAvailableDatasets();
  }, []);

  // Fetch dataset on mount
  useEffect(() => {
    fetchDataset();
  }, []);

  const fetchMetrics = async () => {
    try {
      const response = await axios.get(`${API_URL}/metrics`);
      if (response.data.success) {
        setMetrics(response.data.metrics);
        setTrainingHistory(response.data.training_history);
        setPredictionHistory(response.data.prediction_history);
      }
    } catch (err) {
      console.error('Failed to fetch metrics:', err);
    }
  };

  const fetchDataset = async () => {
    try {
      const response = await axios.get(`${API_URL}/dataset`);
      if (response.data.success) {
        setDataset(response.data.dataset);
      }
    } catch (err) {
      console.error('Failed to fetch dataset:', err);
    }
  };

  const initializeBrain = async () => {
    setLoading(true);
    setError(null);
    try {
      const response = await axios.post(`${API_URL}/init`, {
        dataset: selectedDataset
      });
      if (response.data.success) {
        setBrainInitialized(true);
        setPrimaryBrainId(response.data.brain_id);
        setMetrics(response.data.metrics);
        setDatasetInfo(response.data.dataset_info);
      }
    } catch (err) {
      setError(`Failed to initialize brain: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  const resetBrain = async () => {
    setLoading(true);
    try {
      await axios.post(`${API_URL}/reset`);
      setBrainInitialized(false);
      setMetrics(null);
      setTrainingHistory([]);
      setPredictionHistory([]);
    } catch (err) {
      setError(`Failed to reset brain: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  const trainExample = async (features, label, confidence = 1.0) => {
    try {
      const response = await axios.post(`${API_URL}/train`, {
        features,
        label,
        confidence
      });
      if (response.data.success) {
        await fetchMetrics();
        return response.data;
      }
    } catch (err) {
      setError(`Training failed: ${err.message}`);
      throw err;
    }
  };

  const trainBatch = async (examples) => {
    setLoading(true);
    try {
      const response = await axios.post(`${API_URL}/train-batch`, {
        examples
      });
      if (response.data.success) {
        await fetchMetrics();
      }
    } catch (err) {
      setError(`Batch training failed: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  const makePrediction = async (features, trueLabel = null) => {
    try {
      const response = await axios.post(`${API_URL}/predict`, {
        features,
        true_label: trueLabel
      });
      if (response.data.success) {
        await fetchMetrics();
        return response.data;
      }
    } catch (err) {
      setError(`Prediction failed: ${err.message}`);
      throw err;
    }
  };

  return (
    <div className="App">
      <header className="App-header">
        <h1>🧠 NIMCP Web Demo v2.8.0</h1>
        <p>Interactive Neural Brain with COW Cloning & GPU Acceleration</p>
      </header>

      {error && (
        <div className="error-banner">
          <span>⚠️ {error}</span>
          <button onClick={() => setError(null)}>✕</button>
        </div>
      )}

      <nav className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'demo' ? 'active' : ''}`}
          onClick={() => setActiveTab('demo')}
        >
          🎓 Interactive Demo
        </button>
        <button
          className={`tab-button ${activeTab === 'docs' ? 'active' : ''}`}
          onClick={() => setActiveTab('docs')}
        >
          📚 Documentation
        </button>
        <button
          className={`tab-button ${activeTab === 'benchmark' ? 'active' : ''}`}
          onClick={() => setActiveTab('benchmark')}
        >
          🎯 MNIST Benchmark
        </button>
        <button
          className={`tab-button ${activeTab === 'visualization' ? 'active' : ''}`}
          onClick={() => setActiveTab('visualization')}
        >
          🌐 Network Visualization
        </button>
      </nav>

      <div className="container">
        {activeTab === 'demo' && (
          <>
            {!brainInitialized ? (
              <div className="init-panel">
                <h2>Initialize NIMCP Brain</h2>

                <div className="dataset-selector">
                  <label htmlFor="dataset-select">
                    <strong>Select Dataset:</strong>
                  </label>
                  <select
                    id="dataset-select"
                    value={selectedDataset}
                    onChange={(e) => setSelectedDataset(e.target.value)}
                    className="dataset-dropdown"
                  >
                    {availableDatasets.map((ds) => (
                      <option key={ds.id} value={ds.id}>
                        {ds.name} ({ds.num_inputs} inputs → {ds.num_outputs} outputs)
                      </option>
                    ))}
                  </select>
                </div>

                {availableDatasets.length > 0 && selectedDataset && (
                  <>
                    {(() => {
                      const ds = availableDatasets.find(d => d.id === selectedDataset);
                      return ds ? (
                        <>
                          <p>{ds.description}</p>
                          <ul>
                            <li>{ds.num_inputs} input features ({ds.input_type})</li>
                            <li>{ds.num_outputs} output classes: {ds.classes.join(', ')}</li>
                            <li>Small brain size with adaptive learning</li>
                          </ul>
                        </>
                      ) : null;
                    })()}
                  </>
                )}

                <button
                  className="btn btn-primary btn-large"
                  onClick={initializeBrain}
                  disabled={loading || availableDatasets.length === 0}
                >
                  {loading ? 'Initializing...' : '🚀 Initialize Brain'}
                </button>
              </div>
            ) : (
              <>
                <div className="status-bar">
                  <div className="status-item">
                    <span className="status-label">Dataset:</span>
                    <span className="status-value status-dataset">
                      {datasetInfo?.name || selectedDataset}
                    </span>
                  </div>
                  <div className="status-item">
                    <span className="status-label">Status:</span>
                    <span className="status-value status-active">
                      {metrics?.status || 'Active'}
                    </span>
                  </div>
                  <div className="status-item">
                    <span className="status-label">Trained:</span>
                    <span className="status-value">{metrics?.total_trained || 0}</span>
                  </div>
                  <div className="status-item">
                    <span className="status-label">Predictions:</span>
                    <span className="status-value">{metrics?.total_predictions || 0}</span>
                  </div>
                  <div className="status-item">
                    <span className="status-label">Accuracy:</span>
                    <span className="status-value">
                      {metrics?.accuracy_estimate?.toFixed(1) || 0}%
                    </span>
                  </div>
                  <button
                    className="btn btn-danger btn-small"
                    onClick={resetBrain}
                    disabled={loading}
                  >
                    Reset
                  </button>
                </div>

                <div className="panels-grid">
                  <TrainingPanel
                    onTrain={trainExample}
                    onTrainBatch={trainBatch}
                    dataset={dataset}
                    datasetInfo={datasetInfo}
                    loading={loading}
                  />

                  <PredictionPanel
                    onPredict={makePrediction}
                    dataset={dataset}
                    datasetInfo={datasetInfo}
                    loading={loading}
                  />
                </div>

                <MetricsDashboard
                  metrics={metrics}
                  trainingHistory={trainingHistory}
                  predictionHistory={predictionHistory}
                />
              </>
            )}
          </>
        )}

        {activeTab === 'docs' && (
          <div className="documentation-panel">
            <h2>📚 NIMCP Documentation</h2>

            <div className="doc-cards-grid">
              <div className="doc-card">
                <h3><span className="doc-card-icon">🧠</span>What is NIMCP?</h3>
                <p>
                  NIMCP (Neural Inspired Model Control Protocol) is a biologically-inspired neural network
                  implementation that mimics the structure and behavior of biological neurons and synapses.
                </p>
              </div>

              <div className="doc-card">
                <h3><span className="doc-card-icon">✨</span>Key Features</h3>
                <ul>
                  <li><strong>Biological Fidelity:</strong> Membrane potentials, refractory periods, STDP</li>
                  <li><strong>GPU Acceleration:</strong> CUDA support for massive scale</li>
                  <li><strong>Real-time Learning:</strong> Immediate weight updates</li>
                  <li><strong>Flexible Architecture:</strong> Configurable neurons & synapses</li>
                </ul>
              </div>

              <div className="doc-card">
                <h3><span className="doc-card-icon">🎓</span>Interactive Demo</h3>
                <p>Hands-on experience with NIMCP across 6 different datasets:</p>
                <ul>
                  <li><strong>Dataset Selection:</strong> Choose from multiple problem types</li>
                  <li><strong>Training:</strong> Single examples or batch training</li>
                  <li><strong>Prediction:</strong> Test learned classifications</li>
                  <li><strong>Metrics:</strong> Real-time accuracy & neuron activity</li>
                </ul>
              </div>

              <div className="doc-card doc-card-full">
                <h3><span className="doc-card-icon">📊</span>Available Datasets</h3>
                <div className="api-endpoints">
                  <div className="api-endpoint">
                    <code>Iris Flowers</code>
                    <p>Classic classification: 4 features (sepal/petal measurements) → 3 species</p>
                  </div>
                  <div className="api-endpoint">
                    <code>MNIST Digits</code>
                    <p>Handwritten digit recognition: 784 pixels (28×28) → 10 digits (0-9)</p>
                  </div>
                  <div className="api-endpoint">
                    <code>Titanic Survival</code>
                    <p>Binary classification: 8 passenger features → survived/died</p>
                  </div>
                  <div className="api-endpoint">
                    <code>Visual Patterns</code>
                    <p>Shape recognition: 64 pixels (8×8 grid) → 4 geometric patterns</p>
                  </div>
                  <div className="api-endpoint">
                    <code>XOR Logic Gates</code>
                    <p>Non-linear problem: 2 binary inputs → XOR truth table</p>
                  </div>
                  <div className="api-endpoint">
                    <code>Sine Wave Prediction</code>
                    <p>Time series: 20 timesteps → 4 discretized output ranges</p>
                  </div>
                </div>
              </div>

              <div className="doc-card">
                <h3><span className="doc-card-icon">🎯</span>MNIST Benchmark</h3>
                <p>
                  Standardized performance tests comparing NIMCP against traditional deep learning.
                </p>
                <ul>
                  <li>Training accuracy & convergence rate</li>
                  <li>Inference speed benchmarks</li>
                  <li>GPU utilization efficiency</li>
                </ul>
              </div>

              <div className="doc-card">
                <h3><span className="doc-card-icon">🌐</span>Network Visualization</h3>
                <p>Explore the 3D neural network structure:</p>
                <ul>
                  <li><strong style={{color: '#00ff88'}}>Green:</strong> Excitatory neurons (increase activity)</li>
                  <li><strong style={{color: '#ff4444'}}>Red:</strong> Inhibitory neurons (decrease activity)</li>
                  <li><strong style={{color: '#4488ff'}}>Blue:</strong> Synaptic connections</li>
                </ul>
                <p>Click neurons to see detailed state information.</p>
              </div>

              <div className="doc-card">
                <h3><span className="doc-card-icon">🏗️</span>Architecture</h3>
                <p>NIMCP's layered architecture:</p>
                <ul>
                  <li><strong>Core Layer:</strong> C implementations</li>
                  <li><strong>CUDA Layer:</strong> GPU kernels</li>
                  <li><strong>Python Bindings:</strong> High-level API</li>
                  <li><strong>Web Interface:</strong> Flask + React</li>
                </ul>
              </div>

              <div className="doc-card doc-card-full">
                <h3><span className="doc-card-icon">🔌</span>API Reference</h3>
                <div className="api-endpoints">
                  <div className="api-endpoint">
                    <code>POST /api/init</code>
                    <p>Initialize a new brain instance</p>
                  </div>
                  <div className="api-endpoint">
                    <code>POST /api/train</code>
                    <p>Train on a single example (features, label, confidence)</p>
                  </div>
                  <div className="api-endpoint">
                    <code>POST /api/train-batch</code>
                    <p>Train on multiple examples at once</p>
                  </div>
                  <div className="api-endpoint">
                    <code>POST /api/predict</code>
                    <p>Make a prediction on input features</p>
                  </div>
                  <div className="api-endpoint">
                    <code>GET /api/metrics</code>
                    <p>Get current brain metrics and performance statistics</p>
                  </div>
                </div>
              </div>
            </div>

            <div className="doc-footer">
              <p>
                For more detailed documentation, visit the{' '}
                <a href="https://github.com/bbrelin/nimcp" target="_blank" rel="noopener noreferrer">
                  GitHub Repository →
                </a>
              </p>
            </div>
          </div>
        )}

        {activeTab === 'benchmark' && <BenchmarkPanel />}

        {activeTab === 'visualization' && (
          <NetworkVisualization brainInitialized={brainInitialized} />
        )}
      </div>

      <footer className="App-footer">
        <p>
          NIMCP v2.8.0 - Neural Inspired Model Control Protocol with COW Cloning
        </p>
        <p>
          <a href="https://github.com/bbrelin/nimcp" target="_blank" rel="noopener noreferrer">
            GitHub
          </a>
          {' · '}
          <a href="/docs" target="_blank" rel="noopener noreferrer">
            Documentation
          </a>
        </p>
      </footer>
    </div>
  );
}

export default App;
