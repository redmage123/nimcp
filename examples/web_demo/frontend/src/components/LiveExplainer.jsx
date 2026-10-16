import React, { useState, useEffect } from 'react'

function LiveExplainer({ simulationRunning, activeNeurons, outputActivations, timestamp }) {
  const [currentExplanation, setCurrentExplanation] = useState('')
  const [insights, setInsights] = useState([])

  useEffect(() => {
    if (!simulationRunning) {
      setCurrentExplanation('⏸️ Simulation paused. Click START to begin neural activity.')
      return
    }

    // Analyze current state and provide explanations
    const numActive = activeNeurons.length
    const maxOutput = Math.max(...outputActivations)
    const avgOutput = outputActivations.reduce((a, b) => a + b, 0) / outputActivations.length

    let explanation = ''
    let newInsights = []

    if (numActive === 0) {
      explanation = '🔇 Network is quiet - no neurons firing. This could mean the network is in a refractory period or waiting for input.'
      newInsights.push({
        type: 'info',
        text: 'NIMCP neurons have realistic refractory periods after firing, preventing immediate re-activation.'
      })
    } else if (numActive < 5) {
      explanation = `⚡ Sparse activity: ${numActive} neurons firing. This is typical of biological networks and energy-efficient computing.`
      newInsights.push({
        type: 'success',
        text: 'Sparse coding is a key advantage of spiking networks - only active neurons consume energy.'
      })
    } else if (numActive < 15) {
      explanation = `🧠 Moderate activity: ${numActive} neurons active. The network is processing information through spike propagation.`
      newInsights.push({
        type: 'info',
        text: 'Watch how spikes propagate: active neurons (green) send signals through connections (orange) to downstream neurons.'
      })
    } else {
      explanation = `🔥 High activity: ${numActive} neurons firing! This burst of activity might indicate pattern recognition or network instability.`
      newInsights.push({
        type: 'warning',
        text: 'Homeostatic mechanisms should activate to prevent runaway excitation. Watch for activity normalization.'
      })
    }

    // Add output-specific insights
    if (maxOutput > 0.3) {
      const predictedIdx = outputActivations.indexOf(maxOutput)
      const patterns = ['Vertical', 'Horizontal', 'Diagonal \\', 'Diagonal /']
      newInsights.push({
        type: 'success',
        text: `🎯 Output neuron ${predictedIdx} (${patterns[predictedIdx]}) strongly activated! STDP is strengthening connections that led to this prediction.`
      })
    } else if (avgOutput < 0.1) {
      newInsights.push({
        type: 'info',
        text: '📉 Low output activation - network hasn\'t learned to recognize the current input pattern yet. Try training!'
      })
    }

    // Add temporal insights
    if (timestamp > 0 && timestamp % 50 === 0) {
      newInsights.push({
        type: 'info',
        text: `⏱️ Time step ${timestamp}: Synaptic weights have been updated ${Math.floor(timestamp / 10)} times via STDP.`
      })
    }

    setCurrentExplanation(explanation)
    setInsights(newInsights.slice(0, 3)) // Keep only 3 most recent insights

  }, [simulationRunning, activeNeurons, outputActivations, timestamp])

  return (
    <div className="live-explainer-panel">
      <h3>🔍 What's Happening Now</h3>

      <div className="current-explanation">
        <p className="explanation-text">{currentExplanation}</p>
        <div className="timestamp-display">Time step: {timestamp}</div>
      </div>

      {insights.length > 0 && (
        <div className="insights-container">
          <h4>💡 Insights</h4>
          {insights.map((insight, idx) => (
            <div key={idx} className={`insight-card insight-${insight.type}`}>
              {insight.text}
            </div>
          ))}
        </div>
      )}

      <div className="nimcp-features-active">
        <h4>🔬 NIMCP Features in Action</h4>
        <div className="features-list">
          <div className="feature-status">
            <span className="feature-icon">⚡</span>
            <span className="feature-name">Spiking Dynamics</span>
            <span className={`feature-badge ${activeNeurons.length > 0 ? 'active' : 'inactive'}`}>
              {activeNeurons.length > 0 ? 'Active' : 'Idle'}
            </span>
          </div>

          <div className="feature-status">
            <span className="feature-icon">🧠</span>
            <span className="feature-name">STDP Learning</span>
            <span className={`feature-badge ${simulationRunning ? 'active' : 'inactive'}`}>
              {simulationRunning ? 'Active' : 'Idle'}
            </span>
          </div>

          <div className="feature-status">
            <span className="feature-icon">⚖️</span>
            <span className="feature-name">Homeostasis</span>
            <span className="feature-badge active">Monitoring</span>
          </div>

          <div className="feature-status">
            <span className="feature-icon">🔗</span>
            <span className="feature-name">Synaptic Plasticity</span>
            <span className={`feature-badge ${simulationRunning ? 'active' : 'inactive'}`}>
              {simulationRunning ? 'Updating' : 'Idle'}
            </span>
          </div>
        </div>
      </div>

      <div className="explainer-tips">
        <h4>💭 Try This</h4>
        <ul>
          <li>🎨 Present a pattern and watch input neurons activate (neurons 0-8)</li>
          <li>🎓 Train patterns to see STDP strengthen relevant connections</li>
          <li>👁️ Set "Show Connections" to "All" to see signal flow</li>
          <li>🔍 Click neurons to see detailed statistics</li>
        </ul>
      </div>
    </div>
  )
}

export default LiveExplainer
