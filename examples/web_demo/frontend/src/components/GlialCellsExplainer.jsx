import React, { useState } from 'react'
import './GlialCellsExplainer.css'

const GLIAL_CELL_TYPES = [
  {
    id: 'astrocytes',
    icon: '⭐',
    name: 'Astrocytes',
    tagline: 'The Modulators & Homeostats',
    description: 'Star-shaped cells that cover ~100,000 synapses and orchestrate synaptic transmission and network stability.',
    biologicalFacts: [
      'Cover multiple synapses with their processes',
      'Use calcium waves to communicate (10-30 µm/s)',
      'Release glutamate and D-serine to modulate transmission',
      'Provide metabolic support (ATP/lactate) to neurons',
      'Enforce homeostatic plasticity over hours-to-days'
    ],
    howTheyAffectOutputs: [
      {
        title: '🎚️ Synaptic Strength Modulation',
        description: 'Astrocytes dynamically adjust synaptic strength based on calcium levels. When calcium is elevated, they release glutamate that enhances synaptic transmission, making active pathways stronger.'
      },
      {
        title: '⚖️ Homeostatic Regulation',
        description: 'They monitor average network activity and scale all synapses multiplicatively to maintain target activity levels. This prevents runaway excitation or network silence, directly affecting output stability.'
      },
      {
        title: '🧠 BCM Threshold Control',
        description: 'Astrocytes shift the BCM (Bienenstock-Cooper-Munro) plasticity threshold, determining whether synapses undergo LTP (strengthening) or LTD (weakening). This implements "metaplasticity" - plasticity of plasticity.'
      },
      {
        title: '🌊 Calcium Wave Propagation',
        description: 'Calcium waves spread through gap junctions between astrocytes, coordinating plasticity across spatial domains. This creates regional learning zones in the network.'
      }
    ],
    outputImpact: {
      immediate: 'Astrocyte glutamate release can boost synaptic transmission by 20-50%, directly increasing signal amplitude reaching output neurons.',
      longTerm: 'Homeostatic scaling over hours prevents output saturation or silence, maintaining network sensitivity across a wide range of input strengths.',
      learning: 'BCM threshold modulation shapes which patterns get learned - elevated astrocyte activity makes it harder to potentiate (requiring stronger input), implementing adaptive learning thresholds.'
    }
  },
  {
    id: 'oligodendrocytes',
    icon: '⚡',
    name: 'Oligodendrocytes',
    tagline: 'The Speed Optimizers',
    description: 'Cells that wrap myelin around axons, increasing signal speed by 10-100x through adaptive myelination.',
    biologicalFacts: [
      'Each oligodendrocyte myelinates 10-50 axons',
      'Myelin increases conduction velocity from 1 m/s to 50-100 m/s',
      'Adaptive myelination: high-activity axons get more myelin',
      'Myelination remodels over hours to days',
      'Myelin production is metabolically expensive (ATP cost)'
    ],
    howTheyAffectOutputs: [
      {
        title: '🚀 Conduction Velocity Boost',
        description: 'Oligodendrocytes wrap myelin sheaths around frequently-firing axons. Formula: velocity = base_velocity × (1 + myelin_level × 49). A fully myelinated axon conducts 50x faster than unmyelinated.'
      },
      {
        title: '⏱️ Temporal Precision',
        description: 'Faster conduction means tighter temporal synchronization. Signals from different neurons arrive at outputs with precise relative timing, crucial for coincidence detection and temporal coding.'
      },
      {
        title: '🎯 Activity-Dependent Optimization',
        description: 'High-activity pathways get preferentially myelinated. This creates "fast lanes" for frequently used information routes, making the network more efficient at its learned tasks.'
      },
      {
        title: '⚡ Resource Allocation',
        description: 'Each oligodendrocyte has limited total myelin production capacity. It must allocate resources across its 10-50 axons, creating competition and prioritization of important connections.'
      }
    ],
    outputImpact: {
      immediate: 'A myelinated pathway can deliver spikes to output neurons 10-50ms faster than unmyelinated, dramatically affecting spike timing-dependent computations.',
      longTerm: 'Over hours, frequently activated pathways become "information highways" with 50x faster conduction, making learned behaviors execute faster and more reliably.',
      learning: 'Myelination creates temporal credit assignment - neurons that consistently participate in successful output patterns get speed-boosted, reinforcing correct pathways.'
    }
  },
  {
    id: 'microglia',
    icon: '🧹',
    name: 'Microglia',
    tagline: 'The Pruners & Optimizers',
    description: 'Immune cells that continuously survey synapses and prune weak/unused connections for circuit optimization.',
    biologicalFacts: [
      'Continuously survey synapses within ~50-100 µm radius',
      'Prune weak or inactive connections during development',
      'Activity-dependent refinement: preserve active, remove inactive',
      'Critical for circuit optimization and preventing overfitting',
      'Maintain healthy synaptic density'
    ],
    howTheyAffectOutputs: [
      {
        title: '✂️ Weak Synapse Elimination',
        description: 'Microglia track activity scores for each synapse (exponentially decaying). Synapses below a threshold get pruned, removing noise from the network.'
      },
      {
        title: '🎯 Signal-to-Noise Improvement',
        description: 'By eliminating weak, spurious connections, microglia increase the proportion of signal-carrying synapses. This directly improves output clarity and reduces false activations.'
      },
      {
        title: '📉 Network Sparsification',
        description: 'Dense random networks become sparse, optimized circuits. Pruning reduces computational cost while preserving or improving function - achieving more with less.'
      },
      {
        title: '🛡️ Overfitting Prevention',
        description: 'Continuous pruning of rarely-used connections prevents the network from memorizing noise. This is biological regularization, improving generalization to new patterns.'
      }
    ],
    outputImpact: {
      immediate: 'Pruning removes noisy background connections that create spurious output activations. Output neurons receive cleaner, more consistent input patterns.',
      longTerm: 'Over many pruning cycles, the network converges to a sparse, efficient topology where every remaining connection is functionally significant for producing correct outputs.',
      learning: 'Microglia implement "use it or lose it" learning. Only connections that consistently contribute to useful outputs survive, creating maximally efficient circuits.'
    }
  }
]

function GlialCellsExplainer() {
  const [selectedCell, setSelectedCell] = useState('astrocytes')
  const [activeSection, setActiveSection] = useState('overview')

  const currentCell = GLIAL_CELL_TYPES.find(c => c.id === selectedCell)

  return (
    <div className="glial-explainer-panel">
      <div className="glial-header">
        <h2>🌟 NIMCP's Secret Weapon: Glial Cells</h2>
        <p className="glial-tagline">
          While neurons get all the glory, glial cells do 50% of the brain's computational work!
        </p>
      </div>

      <div className="glial-intro">
        <div className="intro-highlight">
          <h3>Why Glial Cells Matter</h3>
          <p>
            Traditional neural networks completely ignore glial cells. But in the brain, glia outnumber neurons 1:1
            and perform critical functions: modulating synapses, optimizing signal speed, and pruning connections.
          </p>
          <p>
            <strong>NIMCP is one of the only neural network frameworks to model glial cells computationally.</strong> This
            gives NIMCP networks biological superpowers: self-regulation, adaptive optimization, and automatic circuit refinement.
          </p>
        </div>
      </div>

      <div className="cell-type-selector">
        <h3>Select Glial Cell Type</h3>
        <div className="cell-buttons">
          {GLIAL_CELL_TYPES.map(cell => (
            <button
              key={cell.id}
              className={`cell-button ${selectedCell === cell.id ? 'active' : ''}`}
              onClick={() => setSelectedCell(cell.id)}
            >
              <span className="cell-icon">{cell.icon}</span>
              <span className="cell-name">{cell.name}</span>
            </button>
          ))}
        </div>
      </div>

      {currentCell && (
        <div className="cell-details">
          <div className="cell-header">
            <span className="cell-icon-large">{currentCell.icon}</span>
            <div>
              <h2>{currentCell.name}</h2>
              <p className="cell-tagline">{currentCell.tagline}</p>
            </div>
          </div>

          <div className="section-tabs">
            <button
              className={`section-tab ${activeSection === 'overview' ? 'active' : ''}`}
              onClick={() => setActiveSection('overview')}
            >
              📖 Overview
            </button>
            <button
              className={`section-tab ${activeSection === 'mechanisms' ? 'active' : ''}`}
              onClick={() => setActiveSection('mechanisms')}
            >
              ⚙️ How They Work
            </button>
            <button
              className={`section-tab ${activeSection === 'impact' ? 'active' : ''}`}
              onClick={() => setActiveSection('impact')}
            >
              🎯 Output Impact
            </button>
          </div>

          {activeSection === 'overview' && (
            <div className="section-content">
              <p className="cell-description">{currentCell.description}</p>

              <div className="biological-facts">
                <h4>🔬 Biological Facts</h4>
                <ul>
                  {currentCell.biologicalFacts.map((fact, idx) => (
                    <li key={idx}>{fact}</li>
                  ))}
                </ul>
              </div>
            </div>
          )}

          {activeSection === 'mechanisms' && (
            <div className="section-content">
              <h4>How {currentCell.name} Affect Neural Network Outputs</h4>
              <div className="mechanisms-grid">
                {currentCell.howTheyAffectOutputs.map((mechanism, idx) => (
                  <div key={idx} className="mechanism-card">
                    <h5>{mechanism.title}</h5>
                    <p>{mechanism.description}</p>
                  </div>
                ))}
              </div>
            </div>
          )}

          {activeSection === 'impact' && (
            <div className="section-content">
              <h4>🎯 Impact on Network Outputs</h4>

              <div className="impact-timeline">
                <div className="impact-item">
                  <div className="impact-badge immediate">Immediate</div>
                  <div className="impact-description">
                    <strong>Milliseconds:</strong> {currentCell.outputImpact.immediate}
                  </div>
                </div>

                <div className="impact-item">
                  <div className="impact-badge longterm">Long-term</div>
                  <div className="impact-description">
                    <strong>Hours to Days:</strong> {currentCell.outputImpact.longTerm}
                  </div>
                </div>

                <div className="impact-item">
                  <div className="impact-badge learning">Learning</div>
                  <div className="impact-description">
                    <strong>Learning Impact:</strong> {currentCell.outputImpact.learning}
                  </div>
                </div>
              </div>

              <div className="impact-summary">
                <h5>Summary: How {currentCell.name} Change Everything</h5>
                <p>
                  {currentCell.id === 'astrocytes' &&
                    'Without astrocytes, networks would suffer from runaway activity or silence, and couldn\'t adapt learning rates to input statistics. Astrocytes provide the "thermostat" that keeps networks operating in optimal regimes.'}
                  {currentCell.id === 'oligodendrocytes' &&
                    'Without oligodendrocytes, all signals would travel at the same slow speed, eliminating temporal coding and coincidence detection. Myelination creates temporal structure that\'s essential for complex computation.'}
                  {currentCell.id === 'microglia' &&
                    'Without microglia, networks would remain dense and noisy, wasting energy on useless connections. Pruning transforms random networks into optimized circuits that generalize well to new inputs.'}
                </p>
              </div>
            </div>
          )}
        </div>
      )}

      <div className="glial-integration">
        <h3>🔗 Glial-Neuronal Integration in NIMCP</h3>
        <div className="integration-grid">
          <div className="integration-card">
            <h4>Astrocyte ↔ Synapse</h4>
            <p>Astrocytes modulate synaptic strength through glutamate release, directly in the signal propagation path.</p>
          </div>
          <div className="integration-card">
            <h4>Oligodendrocyte ↔ Axon</h4>
            <p>Myelination adjusts signal delay: <code>delay = distance / conduction_velocity</code></p>
          </div>
          <div className="integration-card">
            <h4>Microglia ↔ Network</h4>
            <p>Pruning removes synapses below activity threshold, permanently restructuring the network topology.</p>
          </div>
        </div>
      </div>

      <div className="comparative-advantage">
        <h3>🏆 NIMCP's Unique Advantage</h3>
        <div className="comparison-table">
          <table>
            <thead>
              <tr>
                <th>Feature</th>
                <th>Traditional ANNs</th>
                <th>NIMCP with Glial Cells</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td>Homeostasis</td>
                <td>Manual hyperparameter tuning required</td>
                <td>Automatic via astrocyte feedback</td>
              </tr>
              <tr>
                <td>Signal Speed</td>
                <td>Uniform (unrealistic)</td>
                <td>Adaptive via oligodendrocyte myelination</td>
              </tr>
              <tr>
                <td>Network Pruning</td>
                <td>Separate training phase + heuristics</td>
                <td>Continuous via microglia surveillance</td>
              </tr>
              <tr>
                <td>Energy Efficiency</td>
                <td>All connections always active</td>
                <td>Pruned + optimized connections</td>
              </tr>
              <tr>
                <td>Generalization</td>
                <td>Prone to overfitting</td>
                <td>Built-in regularization via pruning</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <div className="glial-future">
        <h3>🚀 Future Directions</h3>
        <p>
          Future NIMCP demos will allow you to toggle glial cells on/off and observe the dramatic performance differences.
          You'll see firsthand how networks with glial support learn faster, generalize better, and self-regulate more reliably.
        </p>
      </div>
    </div>
  )
}

export default GlialCellsExplainer
