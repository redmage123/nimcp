import React, { useState } from 'react'

const PRESET_PATTERNS = {
  vertical: [0, 1, 0, 0, 1, 0, 0, 1, 0],
  horizontal: [0, 0, 0, 1, 1, 1, 0, 0, 0],
  diagonal_down: [1, 0, 0, 0, 1, 0, 0, 0, 1],
  diagonal_up: [0, 0, 1, 0, 1, 0, 1, 0, 0]
}

const PATTERN_EXPLANATIONS = {
  vertical: {
    name: 'Vertical Line',
    goal: 'Recognize vertical patterns',
    description: 'The network learns to identify a vertical line in a 3x3 grid. This tests basic spatial pattern recognition.',
    output: 'Output neuron 0 should activate strongly when this pattern is presented.'
  },
  horizontal: {
    name: 'Horizontal Line',
    goal: 'Recognize horizontal patterns',
    description: 'The network learns to identify a horizontal line. This is distinct from vertical and tests if the network can differentiate orientations.',
    output: 'Output neuron 1 should activate strongly for horizontal patterns.'
  },
  diagonal_down: {
    name: 'Diagonal \\ Pattern',
    goal: 'Recognize diagonal patterns (top-left to bottom-right)',
    description: 'Tests the network\'s ability to recognize diagonal relationships. This is more complex than straight lines.',
    output: 'Output neuron 2 should activate for this diagonal orientation.'
  },
  diagonal_up: {
    name: 'Diagonal / Pattern',
    goal: 'Recognize diagonal patterns (bottom-left to top-right)',
    description: 'The opposite diagonal tests if the network can learn directional differences in diagonal patterns.',
    output: 'Output neuron 3 should respond to this diagonal direction.'
  }
}

function PatternInput({ onPresentPattern, onTrainPattern }) {
  const [pattern, setPattern] = useState(Array(9).fill(0))
  const [selectedPreset, setSelectedPreset] = useState('vertical')
  const [trainingLabel, setTrainingLabel] = useState(0)
  const [trainingIterations, setTrainingIterations] = useState(10)

  const toggleCell = (index) => {
    const newPattern = [...pattern]
    newPattern[index] = newPattern[index] === 1 ? 0 : 1
    setPattern(newPattern)
  }

  const loadPreset = (presetName) => {
    setSelectedPreset(presetName)
    setPattern([...PRESET_PATTERNS[presetName]])
  }

  const clearPattern = () => {
    setPattern(Array(9).fill(0))
  }

  const handlePresent = () => {
    onPresentPattern(pattern)
  }

  const handleTrain = () => {
    onTrainPattern(selectedPreset, trainingLabel, trainingIterations)
  }

  const currentExplanation = PATTERN_EXPLANATIONS[selectedPreset]

  return (
    <div className="pattern-input-panel">
      <h3>🎨 Pattern Recognition Test</h3>

      <div className="test-explanation">
        <div className="explanation-header">
          <strong>{currentExplanation.name}</strong>
        </div>
        <div className="explanation-content">
          <p><strong>🎯 Goal:</strong> {currentExplanation.goal}</p>
          <p><strong>📝 What it tests:</strong> {currentExplanation.description}</p>
          <p><strong>✅ Success criteria:</strong> {currentExplanation.output}</p>
        </div>
      </div>

      {/* 3x3 Grid */}
      <div className="pattern-grid">
        {pattern.map((cell, index) => (
          <div
            key={index}
            className={`pattern-cell ${cell === 1 ? 'active' : ''}`}
            onClick={() => toggleCell(index)}
            title={`Cell ${index + 1} - Click to toggle`}
          >
            {cell === 1 && '●'}
          </div>
        ))}
      </div>

      {/* Preset Patterns */}
      <div className="preset-controls">
        <label>Load Preset:</label>
        <div className="preset-buttons">
          <button
            className="btn btn-sm"
            onClick={() => loadPreset('vertical')}
          >
            |
          </button>
          <button
            className="btn btn-sm"
            onClick={() => loadPreset('horizontal')}
          >
            ─
          </button>
          <button
            className="btn btn-sm"
            onClick={() => loadPreset('diagonal_down')}
          >
            \
          </button>
          <button
            className="btn btn-sm"
            onClick={() => loadPreset('diagonal_up')}
          >
            /
          </button>
          <button
            className="btn btn-sm btn-warning"
            onClick={clearPattern}
          >
            Clear
          </button>
        </div>
      </div>

      {/* Actions */}
      <div className="pattern-actions">
        <button
          className="btn btn-primary"
          onClick={handlePresent}
        >
          👁️ Present Pattern
        </button>

        <div className="training-controls">
          <label>Training:</label>
          <select
            value={selectedPreset}
            onChange={(e) => setSelectedPreset(e.target.value)}
          >
            <option value="vertical">Vertical |</option>
            <option value="horizontal">Horizontal ─</option>
            <option value="diagonal_down">Diagonal \</option>
            <option value="diagonal_up">Diagonal /</option>
          </select>

          <label>Label (Output Neuron):</label>
          <input
            type="number"
            min="0"
            max="3"
            value={trainingLabel}
            onChange={(e) => setTrainingLabel(parseInt(e.target.value))}
          />

          <label>Iterations:</label>
          <input
            type="number"
            min="1"
            max="100"
            value={trainingIterations}
            onChange={(e) => setTrainingIterations(parseInt(e.target.value))}
          />

          <button
            className="btn btn-success"
            onClick={handleTrain}
          >
            🎓 Train Pattern
          </button>
        </div>
      </div>
    </div>
  )
}

export default PatternInput
