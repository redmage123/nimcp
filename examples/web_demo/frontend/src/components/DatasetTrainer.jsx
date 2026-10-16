import React, { useState, useEffect } from 'react'

const DATASET_PURPOSES = {
  'basic_patterns': 'Teaches the network to recognize fundamental visual patterns in a spatial grid, similar to how early visual cortex neurons respond to edges and orientations.',
  'complex_patterns': 'Challenges the network with more sophisticated visual arrangements requiring multi-feature integration, like recognizing corners, crosses, or spiral formations.',
  'temporal_sequences': 'Trains temporal pattern recognition by exposing the network to patterns that change over time, essential for understanding motion and sequence prediction.',
  'logic_gates': 'Teaches Boolean logic operations (AND, OR, XOR, etc.), demonstrating the network\'s ability to learn symbolic computational rules.',
  'arithmetic': 'Develops mathematical reasoning capabilities by learning basic arithmetic operations and numerical relationships.',
  'symbolic_logic': 'Advanced logical reasoning including first-order logic constructs like modus ponens and syllogistic reasoning.',
  'sequential_reasoning': 'Multi-step causal reasoning where the network must learn dependencies across sequential decision points.'
}

const DATASET_LEARNING = {
  'basic_patterns': 'Synaptic weights strengthen between input neurons representing active pattern cells and corresponding output neurons, forming feature detectors.',
  'complex_patterns': 'Neurons learn to detect compound features through combinations of simpler patterns, building hierarchical representations.',
  'temporal_sequences': 'Timing-dependent synaptic changes (STDP) allow the network to capture temporal correlations and predict future states.',
  'logic_gates': 'Input-output mappings are encoded in synaptic weights, with the network discovering logical relationships through supervised learning.',
  'arithmetic': 'The network learns numerical relationships and can generalize to perform calculations on previously unseen number combinations.',
  'symbolic_logic': 'Abstract reasoning patterns emerge from exposure to logical inference rules, enabling deductive and inductive reasoning.',
  'sequential_reasoning': 'Chained dependencies are learned through backpropagation of temporal credit assignment, linking actions to outcomes.'
}

function DatasetTrainer() {
  const [datasets, setDatasets] = useState([])
  const [categories, setCategories] = useState([])
  const [selectedCategory, setSelectedCategory] = useState('all')
  const [selectedDataset, setSelectedDataset] = useState(null)
  const [sampleCount, setSampleCount] = useState(50)
  const [iterations, setIterations] = useState(5)
  const [training, setTraining] = useState(false)
  const [trainingProgress, setTrainingProgress] = useState({ current: 0, total: 0 })
  const [trainingResult, setTrainingResult] = useState(null)

  const getDatasetPurpose = (category, datasetId) => {
    return DATASET_PURPOSES[datasetId] || `Trains ${category} pattern recognition capabilities.`
  }

  const getDatasetLearning = (datasetId) => {
    return DATASET_LEARNING[datasetId] || 'The network adjusts connection strengths through STDP (Spike-Timing-Dependent Plasticity) to associate inputs with correct outputs.'
  }

  useEffect(() => {
    // Fetch available datasets
    fetch('/api/datasets')
      .then(res => res.json())
      .then(data => {
        setDatasets(data.datasets)
        setCategories(data.categories)
      })
      .catch(err => console.error('Error loading datasets:', err))
  }, [])

  const filteredDatasets = selectedCategory === 'all'
    ? datasets
    : datasets.filter(d => d.category === selectedCategory)

  const handleTrain = async () => {
    if (!selectedDataset) {
      alert('Please select a dataset first')
      return
    }

    setTraining(true)
    setTrainingResult(null)
    setTrainingProgress({ current: 0, total: sampleCount })

    try {
      // Simulate progress updates during training
      // (In a real implementation, this would use WebSockets or SSE)
      const progressInterval = setInterval(() => {
        setTrainingProgress(prev => {
          if (prev.current < prev.total) {
            return { ...prev, current: prev.current + 1 }
          }
          return prev
        })
      }, 100) // Update every 100ms

      const response = await fetch('/api/dataset/train', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          dataset: selectedDataset.id,
          samples: sampleCount,
          iterations: iterations
        })
      })

      clearInterval(progressInterval)

      const result = await response.json()

      if (result.success) {
        setTrainingProgress({ current: result.samples_trained, total: result.samples_trained })
        setTrainingResult({
          success: true,
          message: `Successfully trained on ${result.samples_trained} samples (${result.total_iterations} total iterations)`
        })
      } else {
        setTrainingResult({
          success: false,
          message: result.error || 'Training failed'
        })
      }
    } catch (err) {
      setTrainingResult({
        success: false,
        message: 'Network error: ' + err.message
      })
    } finally {
      setTraining(false)
    }
  }

  const getDifficultyColor = (difficulty) => {
    const colors = {
      easy: '#10b981',
      medium: '#f59e0b',
      hard: '#ef4444',
      expert: '#8b5cf6'
    }
    return colors[difficulty] || '#6b7280'
  }

  const getCategoryIcon = (category) => {
    const icons = {
      visual: '👁️',
      temporal: '⏱️',
      logic: '🧠',
      symbolic: '🔮',
      arithmetic: '🔢'
    }
    return icons[category] || '📊'
  }

  return (
    <div className="dataset-trainer-panel">
      <h3>📚 Training Dataset Library</h3>

      {/* Category Filter */}
      <div className="category-filter">
        <label>Category:</label>
        <select
          value={selectedCategory}
          onChange={(e) => setSelectedCategory(e.target.value)}
          className="form-select"
        >
          <option value="all">All Categories</option>
          {categories.map(cat => (
            <option key={cat} value={cat}>
              {getCategoryIcon(cat)} {cat.charAt(0).toUpperCase() + cat.slice(1)}
            </option>
          ))}
        </select>
      </div>

      {/* Dataset List */}
      <div className="dataset-list">
        {filteredDatasets.map(dataset => (
          <div
            key={dataset.id}
            className={`dataset-card ${selectedDataset?.id === dataset.id ? 'selected' : ''}`}
            onClick={() => setSelectedDataset(dataset)}
          >
            <div className="dataset-header">
              <span className="dataset-icon">{getCategoryIcon(dataset.category)}</span>
              <span className="dataset-name">{dataset.name}</span>
              <span
                className="difficulty-badge"
                style={{ backgroundColor: getDifficultyColor(dataset.difficulty) }}
              >
                {dataset.difficulty}
              </span>
            </div>
            <div className="dataset-description">
              {dataset.description}
            </div>
          </div>
        ))}
      </div>

      {/* Training Controls */}
      {selectedDataset && (
        <>
          <div className="dataset-explanation">
            <h4>📖 About {selectedDataset.name}</h4>
            <div className="explanation-content">
              <p><strong>📝 Description:</strong> {selectedDataset.description}</p>
              <p><strong>🎯 Purpose:</strong> {getDatasetPurpose(selectedDataset.category, selectedDataset.id)}</p>
              <p><strong>🧪 What the Network Learns:</strong> {getDatasetLearning(selectedDataset.id)}</p>
              <p><strong>✅ Success Metric:</strong> The network should correctly predict the output based on input patterns with increasing confidence after training.</p>
            </div>
          </div>

          <div className="training-config">
            <h4>⚙️ Training Configuration</h4>

          <div className="config-row">
            <label>Number of Samples:</label>
            <input
              type="number"
              min="10"
              max="200"
              value={sampleCount}
              onChange={(e) => setSampleCount(parseInt(e.target.value))}
              className="form-input"
            />
          </div>

          <div className="config-row">
            <label>Iterations per Sample:</label>
            <input
              type="number"
              min="1"
              max="20"
              value={iterations}
              onChange={(e) => setIterations(parseInt(e.target.value))}
              className="form-input"
            />
          </div>

          <button
            className="btn btn-success btn-large"
            onClick={handleTrain}
            disabled={training}
          >
            {training ? '⏳ Training...' : '🎓 Train Network'}
          </button>
          </div>

          {training && (
            <div className="training-progress">
              <div className="progress-bar-container">
                <div
                  className="progress-bar-fill"
                  style={{
                    width: `${(trainingProgress.current / trainingProgress.total) * 100}%`
                  }}
                />
              </div>
              <div className="progress-text">
                Training: {trainingProgress.current} / {trainingProgress.total} samples
                ({Math.round((trainingProgress.current / trainingProgress.total) * 100)}%)
              </div>
            </div>
          )}

          {trainingResult && !training && (
            <div className={`training-result ${trainingResult.success ? 'success' : 'error'}`}>
              {trainingResult.success ? '✓' : '✗'} {trainingResult.message}
            </div>
          )}
        </>
      )}
    </div>
  )
}

export default DatasetTrainer
