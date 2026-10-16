import React, { useState } from 'react'
import GlialCellsExplainer from './GlialCellsExplainer'

const NIMCP_FEATURES = [
  {
    id: 'spiking',
    title: '⚡ Spiking Neural Networks',
    description: 'Unlike traditional artificial neural networks that use continuous activation values, NIMCP implements biologically realistic spiking neurons. Each neuron has a membrane potential that accumulates input until it reaches a threshold and "fires" a spike.',
    benefits: [
      'Energy efficient - only compute when neurons spike',
      'Temporal dynamics - timing of spikes matters',
      'Biological realism - mimics real brain neurons'
    ]
  },
  {
    id: 'stdp',
    title: '🧠 Spike-Timing-Dependent Plasticity (STDP)',
    description: 'NIMCP implements STDP, a Hebbian learning rule where synaptic weights change based on the precise timing of pre- and post-synaptic spikes. "Neurons that fire together, wire together."',
    benefits: [
      'Unsupervised learning from temporal patterns',
      'Causal relationships preserved',
      'Competitive learning emerges naturally'
    ]
  },
  {
    id: 'homeostasis',
    title: '⚖️ Homeostatic Plasticity',
    description: 'NIMCP includes homeostatic mechanisms that regulate neural activity to maintain stable network dynamics. Neurons adjust their excitability to achieve target firing rates.',
    benefits: [
      'Prevents runaway excitation or silence',
      'Self-stabilizing networks',
      'Robust to parameter changes'
    ]
  },
  {
    id: 'plasticity',
    title: '🔄 Multiple Plasticity Rules',
    description: 'Beyond STDP, NIMCP supports Oja\'s rule for weight normalization and other plasticity mechanisms to prevent unbounded weight growth.',
    benefits: [
      'Stable long-term learning',
      'Prevents weight saturation',
      'Balances competition and cooperation'
    ]
  },
  {
    id: 'gpu',
    title: '🚀 GPU Acceleration',
    description: 'NIMCP leverages CUDA for GPU acceleration, allowing large-scale spiking neural networks to run efficiently on modern hardware.',
    benefits: [
      'Massive parallelism',
      'Real-time processing of large networks',
      'Scalable to millions of neurons'
    ]
  }
]

const USE_CASES = [
  {
    id: 'pattern-recognition',
    title: '🎨 Pattern Recognition',
    description: 'Train the network to classify visual patterns using spike-based learning.',
    nimcpFeatures: ['STDP learns temporal correlations', 'Homeostasis prevents over-fitting', 'Sparse spike coding'],
    status: 'active'
  },
  {
    id: 'temporal-sequences',
    title: '⏱️ Temporal Sequence Learning',
    description: 'Learn and predict sequences where timing matters, like music or speech.',
    nimcpFeatures: ['Spike timing encodes temporal information', 'STDP learns causal chains', 'Delay adaptation'],
    status: 'coming-soon'
  },
  {
    id: 'homeostatic-balance',
    title: '⚖️ Homeostatic Self-Regulation',
    description: 'Watch the network automatically stabilize its activity levels.',
    nimcpFeatures: ['Intrinsic excitability adjustment', 'Synaptic scaling', 'Target firing rate maintenance'],
    status: 'coming-soon'
  },
  {
    id: 'synaptic-evolution',
    title: '🌱 Synaptic Evolution & Pruning',
    description: 'Observe how connections strengthen, weaken, and get pruned over time.',
    nimcpFeatures: ['STDP-driven weight changes', 'Weak synapse elimination', 'Network sparsification'],
    status: 'coming-soon'
  },
  {
    id: 'reservoir-computing',
    title: '🌊 Reservoir Computing',
    description: 'Use a random recurrent network as a computational reservoir for complex tasks.',
    nimcpFeatures: ['Rich temporal dynamics', 'Liquid state machine', 'Echo state property'],
    status: 'coming-soon'
  }
]

function NIMCPExplainer() {
  const [activeFeature, setActiveFeature] = useState(null)
  const [showUseCases, setShowUseCases] = useState(false)
  const [showGlialDocs, setShowGlialDocs] = useState(false)

  return (
    <div className="nimcp-explainer-panel">
      <div className="explainer-header">
        <h2>🧠 About NIMCP</h2>
        <p className="subtitle">
          Neuromorphic Integrated Multi-scale Cognitive Platform
        </p>
      </div>

      <div className="explainer-intro">
        <p>
          <strong>NIMCP</strong> is a C-based spiking neural network library that brings biological realism to artificial intelligence.
          Unlike traditional ANNs, NIMCP models the temporal dynamics of real neurons, making it ideal for:
        </p>
        <ul>
          <li>🕐 Time-sensitive pattern recognition</li>
          <li>🎵 Audio and video processing</li>
          <li>🤖 Neuromorphic robotics</li>
          <li>🔬 Computational neuroscience research</li>
          <li>⚡ Energy-efficient AI on neuromorphic hardware</li>
        </ul>
      </div>

      <div className="features-section">
        <h3>Key Features</h3>
        <div className="features-grid">
          {NIMCP_FEATURES.map(feature => (
            <div
              key={feature.id}
              className={`feature-card ${activeFeature === feature.id ? 'active' : ''}`}
              onClick={() => setActiveFeature(activeFeature === feature.id ? null : feature.id)}
            >
              <h4>{feature.title}</h4>
              <p>{feature.description}</p>
              {activeFeature === feature.id && (
                <div className="feature-benefits">
                  <strong>Benefits:</strong>
                  <ul>
                    {feature.benefits.map((benefit, idx) => (
                      <li key={idx}>{benefit}</li>
                    ))}
                  </ul>
                </div>
              )}
            </div>
          ))}
        </div>
      </div>

      <div className="use-cases-section">
        <h3>
          Use Cases & Demos
          <button
            className="btn btn-sm"
            onClick={() => setShowUseCases(!showUseCases)}
          >
            {showUseCases ? '▲ Hide' : '▼ Show'}
          </button>
        </h3>

        {showUseCases && (
          <div className="use-cases-grid">
            {USE_CASES.map(useCase => (
              <div
                key={useCase.id}
                className={`use-case-card ${useCase.status}`}
              >
                <div className="use-case-header">
                  <h4>{useCase.title}</h4>
                  {useCase.status === 'active' && (
                    <span className="badge badge-success">Active</span>
                  )}
                  {useCase.status === 'coming-soon' && (
                    <span className="badge badge-info">Coming Soon</span>
                  )}
                </div>
                <p>{useCase.description}</p>
                <div className="use-case-features">
                  <strong>NIMCP Features Used:</strong>
                  <ul>
                    {useCase.nimcpFeatures.map((feature, idx) => (
                      <li key={idx}>• {feature}</li>
                    ))}
                  </ul>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="glial-cells-section">
        <div className="glial-card-header">
          <h3>⭐ NIMCP's Secret Weapon: Glial Cells</h3>
          <button
            className="btn btn-primary"
            onClick={() => setShowGlialDocs(!showGlialDocs)}
          >
            {showGlialDocs ? '▲ Hide' : '📖 Learn More'}
          </button>
        </div>
        <p className="glial-intro">
          While neurons get all the glory, glial cells do 50% of the brain's computational work!
          NIMCP is one of the only neural network frameworks to model glial cells computationally.
        </p>
        {showGlialDocs && <GlialCellsExplainer />}
      </div>

      <div className="technical-details">
        <h3>🔧 Technical Architecture</h3>
        <div className="architecture-grid">
          <div className="arch-item">
            <strong>Core Engine:</strong>
            <span>C implementation for performance</span>
          </div>
          <div className="arch-item">
            <strong>GPU Support:</strong>
            <span>CUDA acceleration for large networks</span>
          </div>
          <div className="arch-item">
            <strong>Python Bindings:</strong>
            <span>Easy integration and prototyping</span>
          </div>
          <div className="arch-item">
            <strong>Memory Efficiency:</strong>
            <span>Copy-on-Write (COW) memory sharing</span>
          </div>
        </div>
      </div>

      <div className="learn-more">
        <h3>📚 Learn More</h3>
        <div className="resources">
          <a href="/api/docs" className="resource-link" target="_blank" rel="noopener noreferrer">
            📖 API Documentation
          </a>
          <a href="/api/examples" className="resource-link" target="_blank" rel="noopener noreferrer">
            💻 Code Examples
          </a>
          <a href="../../README.md" className="resource-link" download>
            📄 Project README
          </a>
        </div>
      </div>
    </div>
  )
}

export default NIMCPExplainer
