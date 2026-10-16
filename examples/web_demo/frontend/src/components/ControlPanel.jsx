import React, { useState } from 'react'

function ControlPanel({
  simulationRunning,
  onStart,
  onStop,
  onReset,
  onAddConnection,
  onApplyPlasticity,
  onPrune,
  onSetNeuronModel,
  selectedNeuron
}) {
  const [fromNeuron, setFromNeuron] = useState('')
  const [toNeuron, setToNeuron] = useState('')
  const [weight, setWeight] = useState('0.5')
  const [targetNeuron, setTargetNeuron] = useState('0')
  const [plasticityRule, setPlasticityRule] = useState('stdp')
  const [pruneThreshold, setPruneThreshold] = useState('0.1')
  const [modelType, setModelType] = useState('0')
  const [modelNeuron, setModelNeuron] = useState('0')

  const handleAddConnection = () => {
    const from = parseInt(fromNeuron)
    const to = parseInt(toNeuron)
    const w = parseFloat(weight)

    if (!isNaN(from) && !isNaN(to) && !isNaN(w)) {
      onAddConnection(from, to, w)
      setFromNeuron('')
      setToNeuron('')
    }
  }

  const handleApplyPlasticity = () => {
    const neuronId = parseInt(targetNeuron)
    if (!isNaN(neuronId)) {
      onApplyPlasticity(plasticityRule, neuronId)
    }
  }

  const handlePrune = () => {
    const threshold = parseFloat(pruneThreshold)
    if (!isNaN(threshold)) {
      onPrune(threshold)
    }
  }

  const handleSetNeuronModel = () => {
    const neuronId = parseInt(modelNeuron)
    const type = parseInt(modelType)
    if (!isNaN(neuronId) && !isNaN(type)) {
      onSetNeuronModel(neuronId, type)
    }
  }

  return (
    <div className="panel control-panel">
      <h2>Controls</h2>

      <div className="controls">
        {/* Simulation Controls */}
        <div className="control-group">
          <label>
            <span className={`status-indicator ${simulationRunning ? 'running' : 'stopped'}`} />
            Simulation Status
          </label>
          <button
            className="btn btn-success"
            onClick={onStart}
            disabled={simulationRunning}
          >
            ▶️ Start Simulation
          </button>
          <button
            className="btn btn-danger"
            onClick={onStop}
            disabled={!simulationRunning}
          >
            ⏸️ Stop Simulation
          </button>
          <button className="btn btn-warning" onClick={onReset}>
            🔄 Reset Network
          </button>
        </div>

        {/* Connection Controls */}
        <div className="control-group">
          <label>Add Connection</label>
          <input
            type="number"
            placeholder="From Neuron ID"
            value={fromNeuron}
            onChange={(e) => setFromNeuron(e.target.value)}
            min="0"
          />
          <input
            type="number"
            placeholder="To Neuron ID"
            value={toNeuron}
            onChange={(e) => setToNeuron(e.target.value)}
            min="0"
          />
          <input
            type="number"
            placeholder="Weight"
            value={weight}
            onChange={(e) => setWeight(e.target.value)}
            step="0.1"
          />
          <button className="btn btn-primary" onClick={handleAddConnection}>
            ➕ Add Synapse
          </button>
        </div>

        {/* Plasticity Controls */}
        <div className="control-group">
          <label>Apply Plasticity</label>
          <select
            value={plasticityRule}
            onChange={(e) => setPlasticityRule(e.target.value)}
          >
            <option value="stdp">STDP (Spike-Timing)</option>
            <option value="oja">Oja's Rule</option>
            <option value="homeostasis">Homeostasis</option>
          </select>
          <input
            type="number"
            placeholder="Neuron ID"
            value={targetNeuron}
            onChange={(e) => setTargetNeuron(e.target.value)}
            min="0"
          />
          <button className="btn btn-primary" onClick={handleApplyPlasticity}>
            ⚡ Apply Plasticity
          </button>
        </div>

        {/* Network Maintenance */}
        <div className="control-group">
          <label>Network Maintenance</label>
          <input
            type="number"
            placeholder="Prune Threshold"
            value={pruneThreshold}
            onChange={(e) => setPruneThreshold(e.target.value)}
            step="0.01"
          />
          <button className="btn btn-warning" onClick={handlePrune}>
            ✂️ Prune Weak Synapses
          </button>
        </div>

        {/* Neuron Model Selection */}
        <div className="control-group">
          <label>Neuron Model (NIMCP 2.6)</label>
          <select
            value={modelType}
            onChange={(e) => setModelType(e.target.value)}
          >
            <option value="0">LIF (Default)</option>
            <option value="1">Izhikevich - Regular Spiking</option>
          </select>
          <input
            type="number"
            placeholder="Neuron ID"
            value={modelNeuron}
            onChange={(e) => setModelNeuron(e.target.value)}
            min="0"
          />
          <button className="btn btn-primary" onClick={handleSetNeuronModel}>
            🧠 Set Neuron Model
          </button>
        </div>

      </div>
    </div>
  )
}

export default ControlPanel
