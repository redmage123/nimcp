import React, { useState, useEffect } from 'react'
import './GlialControls.css'

/**
 * Glial Cell Controls and Statistics Display
 *
 * Demonstrates NIMCP's glial cell infrastructure:
 * - Astrocytes: Synaptic modulation and homeostasis
 * - Oligodendrocytes: Adaptive myelination for signal speed
 * - Microglia: Synaptic pruning and optimization
 */

function GlialControls({ glialEnabled, onToggleGlial, glialStats }) {
  const [selectedType, setSelectedType] = useState('astrocytes')
  const [showDetails, setShowDetails] = useState(false)

  const glialTypes = [
    {
      id: 'astrocytes',
      name: 'Astrocytes',
      icon: '⭐',
      enabled: glialEnabled.astrocytes,
      color: '#4CAF50',
      description: 'Modulate synaptic strength and maintain homeostasis'
    },
    {
      id: 'oligodendrocytes',
      name: 'Oligodendrocytes',
      icon: '⚡',
      enabled: glialEnabled.oligodendrocytes,
      color: '#2196F3',
      description: 'Speed up signal transmission through myelination'
    },
    {
      id: 'microglia',
      name: 'Microglia',
      icon: '🧹',
      enabled: glialEnabled.microglia,
      color: '#FF9800',
      description: 'Prune weak connections to optimize the network'
    }
  ]

  const currentType = glialTypes.find(t => t.id === selectedType)
  const stats = {
    astrocytes: glialStats?.astrocytes || { modulations: 0, avgModulation: 1.0, coverage: 0 },
    oligodendrocytes: glialStats?.oligodendrocytes || { myelinations: 0, avgSpeed: 1.0, coverage: 0 },
    microglia: glialStats?.microglia || { prunings: 0, avgActivity: 0.5, coverage: 0 }
  }

  return (
    <div className="glial-controls-container">
      <div className="glial-header">
        <h3>🌟 Glial Cells</h3>
        <button
          className="info-button"
          onClick={() => setShowDetails(!showDetails)}
          title="Learn more about glial cells"
        >
          {showDetails ? '✕' : 'ℹ️'}
        </button>
      </div>

      {showDetails && (
        <div className="glial-info-box">
          <p>
            <strong>Glial cells</strong> are the brain's support cells that outnumber neurons 1:1.
            NIMCP models three types that dramatically improve network performance:
          </p>
          <ul>
            <li><strong>Astrocytes</strong> - Regulate synaptic strength and prevent runaway activity</li>
            <li><strong>Oligodendrocytes</strong> - Speed up important connections through myelination</li>
            <li><strong>Microglia</strong> - Remove weak connections to prevent overfitting</li>
          </ul>
        </div>
      )}

      <div className="glial-toggles">
        {glialTypes.map(type => (
          <div key={type.id} className="glial-toggle-row">
            <button
              className={`glial-type-button ${selectedType === type.id ? 'active' : ''}`}
              onClick={() => setSelectedType(type.id)}
              style={{ borderColor: type.enabled ? type.color : '#ccc' }}
            >
              <span className="glial-icon">{type.icon}</span>
              <span className="glial-name">{type.name}</span>
            </button>

            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={type.enabled}
                onChange={() => onToggleGlial(type.id)}
              />
              <span className="toggle-slider" style={{
                backgroundColor: type.enabled ? type.color : '#ccc'
              }}></span>
            </label>
          </div>
        ))}
      </div>

      {currentType && (
        <div className="glial-stats-panel">
          <div className="stats-header">
            <span className="stats-icon" style={{ color: currentType.color }}>
              {currentType.icon}
            </span>
            <h4>{currentType.name} Activity</h4>
          </div>

          <div className="stats-grid">
            {currentType.id === 'astrocytes' && (
              <>
                <div className="stat-item">
                  <span className="stat-label">Modulations</span>
                  <span className="stat-value">{stats.astrocytes.modulations.toLocaleString()}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Avg. Modulation</span>
                  <span className="stat-value">{stats.astrocytes.avgModulation.toFixed(2)}x</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Synapse Coverage</span>
                  <span className="stat-value">{(stats.astrocytes.coverage * 100).toFixed(0)}%</span>
                </div>
              </>
            )}

            {currentType.id === 'oligodendrocytes' && (
              <>
                <div className="stat-item">
                  <span className="stat-label">Myelinations</span>
                  <span className="stat-value">{stats.oligodendrocytes.myelinations.toLocaleString()}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Avg. Speed Boost</span>
                  <span className="stat-value">{stats.oligodendrocytes.avgSpeed.toFixed(1)}x</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Neuron Coverage</span>
                  <span className="stat-value">{(stats.oligodendrocytes.coverage * 100).toFixed(0)}%</span>
                </div>
              </>
            )}

            {currentType.id === 'microglia' && (
              <>
                <div className="stat-item">
                  <span className="stat-label">Synapses Pruned</span>
                  <span className="stat-value">{stats.microglia.prunings.toLocaleString()}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Avg. Activity</span>
                  <span className="stat-value">{stats.microglia.avgActivity.toFixed(2)}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Monitoring Coverage</span>
                  <span className="stat-value">{(stats.microglia.coverage * 100).toFixed(0)}%</span>
                </div>
              </>
            )}
          </div>

          {!currentType.enabled && (
            <div className="disabled-overlay">
              <p>🔒 {currentType.name} Disabled</p>
              <p className="disabled-hint">Toggle above to enable</p>
            </div>
          )}
        </div>
      )}

      <div className="glial-summary">
        <div className="summary-stat">
          <span className="summary-label">Total Glial Cells</span>
          <span className="summary-value">
            {(glialEnabled.astrocytes ? 1 : 0) +
             (glialEnabled.oligodendrocytes ? 1 : 0) +
             (glialEnabled.microglia ? 1 : 0)} / 3 Active
          </span>
        </div>
      </div>
    </div>
  )
}

export default GlialControls
