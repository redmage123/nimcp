import React, { useState, useEffect } from 'react'
import { io } from 'socket.io-client'
import NetworkVisualization from './components/NetworkVisualization'
import ControlPanel from './components/ControlPanel'
import MetricsCharts from './components/MetricsCharts'
import StatsCards from './components/StatsCards'
import ActivityLog from './components/ActivityLog'
import NeuronDetails from './components/NeuronDetails'
import PatternInput from './components/PatternInput'
import OutputVisualization from './components/OutputVisualization'
import NIMCPExplainer from './components/NIMCPExplainer'
import LiveExplainer from './components/LiveExplainer'
import DatasetTrainer from './components/DatasetTrainer'
import CheckpointManager from './components/CheckpointManager'
import GlialControls from './components/GlialControls'
import GlialCellsExplainer from './components/GlialCellsExplainer'

// Use relative path so Socket.IO connects through Vite proxy
// This works both locally and remotely
const socket = io()

function App() {
  // State
  const [simulationRunning, setSimulationRunning] = useState(false)
  const [timestamp, setTimestamp] = useState(0)
  const [networkTopology, setNetworkTopology] = useState({ nodes: [], edges: [] })
  const [selectedNeuron, setSelectedNeuron] = useState(null)
  const [activeNeurons, setActiveNeurons] = useState([])
  const [logs, setLogs] = useState([])
  const [showConnections, setShowConnections] = useState('all') // 'none', 'selected', 'all' - default to 'all' to show network activity
  const [outputActivations, setOutputActivations] = useState([0, 0, 0, 0])
  const [currentPattern, setCurrentPattern] = useState(null)
  const [activeTab, setActiveTab] = useState('demo') // 'demo' or 'about'
  const [sidebarTab, setSidebarTab] = useState('controls') // 'controls', 'patterns', 'datasets', 'output', 'glial'
  const [bottomTab, setBottomTab] = useState('metrics') // 'metrics', 'stats', 'logs'
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false)

  // Glial cell state
  const [glialEnabled, setGlialEnabled] = useState({
    astrocytes: false,
    oligodendrocytes: false,
    microglia: false
  })
  const [glialStats, setGlialStats] = useState(null)

  // Metrics state
  const [metricsData, setMetricsData] = useState({
    activity: [],
    weight: [],
    timestamps: []
  })

  // Add log entry
  const addLog = (category, message) => {
    const entry = {
      timestamp: new Date().toLocaleTimeString(),
      category,
      message
    }
    setLogs(prev => [...prev.slice(-49), entry])
  }

  // Load network topology
  const loadTopology = async () => {
    try {
      const response = await fetch('/api/network/topology')
      const data = await response.json()
      setNetworkTopology(data)
      addLog('Network', `Loaded ${data.nodes.length} neurons`)
    } catch (error) {
      console.error('Error loading topology:', error)
      addLog('Error', 'Failed to load network topology')
    }
  }

  // WebSocket handlers
  useEffect(() => {
    socket.on('connect', () => {
      console.log('✅ WebSocket connected to server')
      addLog('WebSocket', 'Connected to server')
    })

    socket.on('connect_error', (error) => {
      console.error('❌ WebSocket connection error:', error)
      addLog('Error', `WebSocket connection failed: ${error.message}`)
    })

    socket.on('disconnect', (reason) => {
      console.warn('⚠️ WebSocket disconnected:', reason)
      addLog('WebSocket', `Disconnected: ${reason}`)
    })

    socket.on('simulation_update', (data) => {
      console.log('📡 Simulation update received:', data)
      setTimestamp(data.timestamp)
      if (data.active_neurons && Array.isArray(data.active_neurons)) {
        setActiveNeurons(data.active_neurons)
      }
      if (data.output_activations && Array.isArray(data.output_activations)) {
        setOutputActivations(data.output_activations)
      }
    })

    socket.on('metrics_update', (data) => {
      setMetricsData(prev => {
        const newActivity = [...prev.activity, data.activity].slice(-100)
        const newWeight = [...prev.weight, data.weight].slice(-100)
        const newTimestamps = [...prev.timestamps, data.timestamp].slice(-100)

        return {
          activity: newActivity,
          weight: newWeight,
          timestamps: newTimestamps
        }
      })
    })

    return () => {
      socket.off('connect')
      socket.off('simulation_update')
      socket.off('metrics_update')
    }
  }, [])

  // Load topology on mount
  useEffect(() => {
    loadTopology()
  }, [])

  // API functions
  const startSimulation = async () => {
    console.log('🚀 START button clicked')
    try {
      const response = await fetch('/api/simulation/start', { method: 'POST' })
      console.log('📥 Start response:', response.status, response.statusText)
      const data = await response.json()
      console.log('📦 Start data:', data)
      if (data.success) {
        setSimulationRunning(true)
        addLog('Simulation', 'Started')
        console.log('✅ Simulation state set to RUNNING')
      } else {
        console.warn('⚠️ Start request unsuccessful:', data)
        addLog('Error', 'Start request returned unsuccessful')
      }
    } catch (error) {
      console.error('❌ Start simulation error:', error)
      addLog('Error', 'Failed to start simulation')
    }
  }

  const stopSimulation = async () => {
    try {
      const response = await fetch('/api/simulation/stop', { method: 'POST' })
      const data = await response.json()
      if (data.success) {
        setSimulationRunning(false)
        addLog('Simulation', 'Stopped')
      }
    } catch (error) {
      addLog('Error', 'Failed to stop simulation')
    }
  }

  const resetSimulation = async () => {
    try {
      const response = await fetch('/api/simulation/reset', { method: 'POST' })
      const data = await response.json()
      if (data.success) {
        setTimestamp(0)
        setMetricsData({ activity: [], weight: [], timestamps: [] })
        await loadTopology()
        addLog('Simulation', 'Reset complete')
      }
    } catch (error) {
      addLog('Error', 'Failed to reset simulation')
    }
  }

  const addConnection = async (fromId, toId, weight) => {
    try {
      const response = await fetch('/api/connection/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ from_id: fromId, to_id: toId, weight })
      })
      const data = await response.json()
      if (data.success) {
        addLog('Connection', `${fromId} → ${toId} (w=${weight.toFixed(2)})`)
        await loadTopology()
      } else {
        addLog('Error', data.error || 'Failed to add connection')
      }
    } catch (error) {
      addLog('Error', 'Failed to add connection')
    }
  }

  const applyPlasticity = async (rule, neuronId) => {
    try {
      const response = await fetch('/api/plasticity/apply', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ rule, neuron_id: neuronId })
      })
      const data = await response.json()
      if (data.success) {
        addLog('Plasticity', `${rule.toUpperCase()} applied to neuron ${neuronId}`)
      } else {
        addLog('Error', data.error || 'Failed to apply plasticity')
      }
    } catch (error) {
      addLog('Error', 'Failed to apply plasticity')
    }
  }

  const pruneNetwork = async (threshold) => {
    try {
      const response = await fetch('/api/network/prune', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ threshold })
      })
      const data = await response.json()
      if (data.success) {
        addLog('Prune', `Removed ${data.pruned} weak synapses`)
        await loadTopology()
      } else {
        addLog('Error', data.error || 'Failed to prune network')
      }
    } catch (error) {
      addLog('Error', 'Failed to prune network')
    }
  }

  const setNeuronModel = async (neuronId, modelType) => {
    try {
      const response = await fetch(`/api/neuron/${neuronId}/set_model`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ model_type: modelType })
      })
      const data = await response.json()
      if (data.success) {
        const modelName = data.model_name || ['LIF', 'Izhikevich', 'AdEx', 'Hodgkin-Huxley'][modelType]
        addLog('Model', `Neuron ${neuronId} set to ${modelName}`)
      } else {
        addLog('Error', data.error || 'Failed to set neuron model')
      }
    } catch (error) {
      addLog('Error', 'Failed to set neuron model')
    }
  }

  // Pattern Recognition functions
  const presentPattern = async (pattern) => {
    try {
      const response = await fetch('/api/pattern/present', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ pattern })
      })
      const data = await response.json()
      if (data.success) {
        addLog('Pattern', `Presented ${data.pattern} pattern`)
      } else {
        addLog('Error', data.error || 'Failed to present pattern')
      }
    } catch (error) {
      addLog('Error', 'Failed to present pattern')
    }
  }

  const trainPattern = async (pattern, label, iterations) => {
    try {
      const response = await fetch('/api/pattern/train', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ pattern, label, iterations })
      })
      const data = await response.json()
      if (data.success) {
        addLog('Training', `Trained ${data.pattern} → Output ${data.label} (${data.iterations}x)`)
      } else {
        addLog('Error', data.error || 'Failed to train pattern')
      }
    } catch (error) {
      addLog('Error', 'Failed to train pattern')
    }
  }

  // Glial cell functions
  const toggleGlialCell = async (type) => {
    const newEnabled = !glialEnabled[type]
    try {
      const response = await fetch('/api/glial/toggle', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type, enabled: newEnabled })
      })
      const data = await response.json()
      if (data.success) {
        setGlialEnabled(prev => ({ ...prev, [type]: newEnabled }))
        const cellNames = {
          astrocytes: 'Astrocytes',
          oligodendrocytes: 'Oligodendrocytes',
          microglia: 'Microglia'
        }
        addLog('Glial', `${cellNames[type]} ${newEnabled ? 'enabled' : 'disabled'}`)
      } else {
        addLog('Error', data.error || 'Failed to toggle glial cells')
      }
    } catch (error) {
      addLog('Error', 'Failed to toggle glial cells')
    }
  }

  const loadGlialInfo = async () => {
    try {
      const response = await fetch('/api/glial/info')
      const data = await response.json()
      if (data.success && data.enabled) {
        setGlialEnabled(data.enabled)
      }
    } catch (error) {
      console.error('Error loading glial info:', error)
    }
  }

  const loadGlialStats = async () => {
    try {
      const response = await fetch('/api/glial/stats')
      const data = await response.json()
      if (data.success && data.stats) {
        setGlialStats(data.stats)
      }
    } catch (error) {
      console.error('Error loading glial stats:', error)
    }
  }

  // Load glial info on mount
  useEffect(() => {
    loadGlialInfo()
  }, [])

  // Poll glial stats when glial tab is active
  useEffect(() => {
    if (sidebarTab === 'glial') {
      loadGlialStats()
      const interval = setInterval(loadGlialStats, 1000) // Poll every second
      return () => clearInterval(interval)
    }
  }, [sidebarTab])

  // Poll output activations periodically when simulation is running
  useEffect(() => {
    if (simulationRunning) {
      const interval = setInterval(async () => {
        try {
          const response = await fetch('/api/output')
          const data = await response.json()
          if (data.activations) {
            setOutputActivations(data.activations)
          }
          if (data.current_pattern) {
            setCurrentPattern(data.current_pattern)
          }
        } catch (error) {
          console.error('Error fetching output activations:', error)
        }
      }, 500) // Poll every 500ms

      return () => clearInterval(interval)
    }
  }, [simulationRunning])

  return (
    <div className="app">
      <header className="header">
        <h1>🧠 NIMCP - Neural Network Interactive Showcase</h1>
        <p>Real-time Spiking Neural Network with Plasticity, Analytics & Visualization</p>

        <div className="header-tabs">
          <button
            className={`tab-button ${activeTab === 'demo' ? 'active' : ''}`}
            onClick={() => setActiveTab('demo')}
          >
            🎮 Live Demo
          </button>
          <button
            className={`tab-button ${activeTab === 'about' ? 'active' : ''}`}
            onClick={() => setActiveTab('about')}
          >
            📚 About NIMCP
          </button>
        </div>
      </header>

      <div className="container">
        {activeTab === 'about' ? (
          <NIMCPExplainer />
        ) : (
        <>
        <div className="dashboard">
          {!sidebarCollapsed && (
            <div className="left-sidebar">
              <div className="sidebar-header">
                <div className="sidebar-tabs">
                  <button
                    className={`sidebar-tab ${sidebarTab === 'controls' ? 'active' : ''}`}
                    onClick={() => setSidebarTab('controls')}
                    title="Simulation Controls"
                  >
                    🎮
                  </button>
                  <button
                    className={`sidebar-tab ${sidebarTab === 'patterns' ? 'active' : ''}`}
                    onClick={() => setSidebarTab('patterns')}
                    title="Pattern Recognition"
                  >
                    🔲
                  </button>
                  <button
                    className={`sidebar-tab ${sidebarTab === 'datasets' ? 'active' : ''}`}
                    onClick={() => setSidebarTab('datasets')}
                    title="Dataset Training"
                  >
                    📊
                  </button>
                  <button
                    className={`sidebar-tab ${sidebarTab === 'output' ? 'active' : ''}`}
                    onClick={() => setSidebarTab('output')}
                    title="Output & Feedback"
                  >
                    📈
                  </button>
                  <button
                    className={`sidebar-tab ${sidebarTab === 'checkpoint' ? 'active' : ''}`}
                    onClick={() => setSidebarTab('checkpoint')}
                    title="Checkpoints"
                  >
                    💾
                  </button>
                  <button
                    className={`sidebar-tab ${sidebarTab === 'glial' ? 'active' : ''}`}
                    onClick={() => setSidebarTab('glial')}
                    title="Glial Cells"
                  >
                    🌟
                  </button>
                </div>
                <button
                  className="collapse-btn"
                  onClick={() => setSidebarCollapsed(true)}
                  title="Collapse sidebar"
                >
                  ◀
                </button>
              </div>

              <div className="sidebar-content">
                {sidebarTab === 'controls' && (
                  <>
                    <ControlPanel
                      simulationRunning={simulationRunning}
                      onStart={startSimulation}
                      onStop={stopSimulation}
                      onReset={resetSimulation}
                      onAddConnection={addConnection}
                      onApplyPlasticity={applyPlasticity}
                      onPrune={pruneNetwork}
                      onSetNeuronModel={setNeuronModel}
                      selectedNeuron={selectedNeuron}
                    />
                    <NeuronDetails neuronId={selectedNeuron} />
                  </>
                )}

                {sidebarTab === 'patterns' && (
                  <PatternInput
                    onPresentPattern={presentPattern}
                    onTrainPattern={trainPattern}
                  />
                )}

                {sidebarTab === 'datasets' && (
                  <DatasetTrainer />
                )}

                {sidebarTab === 'output' && (
                  <OutputVisualization
                    outputActivations={outputActivations}
                    currentPattern={currentPattern}
                  />
                )}

                {sidebarTab === 'checkpoint' && (
                  <CheckpointManager onLog={addLog} />
                )}

                {sidebarTab === 'glial' && (
                  <GlialControls
                    glialEnabled={glialEnabled}
                    onToggleGlial={toggleGlialCell}
                    glialStats={glialStats}
                  />
                )}
              </div>
            </div>
          )}

          {sidebarCollapsed && (
            <button
              className="expand-btn"
              onClick={() => setSidebarCollapsed(false)}
              title="Expand sidebar"
            >
              ▶
            </button>
          )}

          <div className={`main-panel ${sidebarCollapsed ? 'expanded' : ''}`}>
            <div className="viz-controls">
              <label>Connections:</label>
              <select
                value={showConnections}
                onChange={(e) => setShowConnections(e.target.value)}
                className="connection-toggle"
              >
                <option value="none">Hidden</option>
                <option value="selected">Selected Only</option>
                <option value="all">All</option>
              </select>
            </div>

            <NetworkVisualization
              topology={networkTopology}
              activeNeurons={activeNeurons}
              onNodeSelect={setSelectedNeuron}
              selectedNeuron={selectedNeuron}
              showConnections={showConnections}
            />

            <LiveExplainer
              simulationRunning={simulationRunning}
              activeNeurons={activeNeurons}
              outputActivations={outputActivations}
              timestamp={timestamp}
            />
          </div>
        </div>

        <div className="bottom-panel">
          <div className="bottom-tabs">
            <button
              className={`bottom-tab ${bottomTab === 'metrics' ? 'active' : ''}`}
              onClick={() => setBottomTab('metrics')}
            >
              📊 Metrics
            </button>
            <button
              className={`bottom-tab ${bottomTab === 'stats' ? 'active' : ''}`}
              onClick={() => setBottomTab('stats')}
            >
              📈 Statistics
            </button>
            <button
              className={`bottom-tab ${bottomTab === 'logs' ? 'active' : ''}`}
              onClick={() => setBottomTab('logs')}
            >
              📝 Activity Log
            </button>
          </div>

          <div className="bottom-content">
            {bottomTab === 'metrics' && <MetricsCharts metricsData={metricsData} />}

            {bottomTab === 'stats' && (
              <StatsCards
                neuronCount={networkTopology.nodes.length}
                timestamp={timestamp}
                activityLevel={metricsData.activity[metricsData.activity.length - 1] || 0}
                avgWeight={metricsData.weight[metricsData.weight.length - 1] || 0}
              />
            )}

            {bottomTab === 'logs' && <ActivityLog logs={logs} />}
          </div>
        </div>
        </>
        )}
      </div>
    </div>
  )
}

export default App
