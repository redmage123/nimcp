import React, { useState, useEffect, useRef } from 'react'

const PATTERN_NAMES = ['Vertical |', 'Horizontal ─', 'Diagonal \\', 'Diagonal /']
const PATTERN_COLORS = ['#3b82f6', '#10b981', '#f59e0b', '#ef4444']

// Map backend pattern names to expected output indices
const PATTERN_MAP = {
  'vertical': 0,
  'horizontal': 1,
  'diagonal_down': 2,
  'diagonal_up': 3
}

function OutputVisualization({ outputActivations, currentPattern }) {
  const [predictedPattern, setPredictedPattern] = useState('none')
  const [maxActivation, setMaxActivation] = useState(0)
  const [confidenceThreshold, setConfidenceThreshold] = useState(0.7)
  const [feedbackResult, setFeedbackResult] = useState(null)
  const [predictedIndex, setPredictedIndex] = useState(-1)
  const [expectedIndex, setExpectedIndex] = useState(null)
  const [isCorrect, setIsCorrect] = useState(null)
  const lastFeedbackRef = useRef(null)

  // Update predicted pattern from output activations
  useEffect(() => {
    if (outputActivations && outputActivations.length === 4) {
      const max = Math.max(...outputActivations)
      setMaxActivation(max)
      if (max > 0.1) {
        const idx = outputActivations.indexOf(max)
        setPredictedIndex(idx)
        setPredictedPattern(PATTERN_NAMES[idx])
      } else {
        setPredictedIndex(-1)
        setPredictedPattern('none')
      }
    }
  }, [outputActivations])

  // Update expected pattern from currentPattern
  useEffect(() => {
    if (currentPattern && PATTERN_MAP.hasOwnProperty(currentPattern)) {
      const expected = PATTERN_MAP[currentPattern]
      setExpectedIndex(expected)
    } else {
      setExpectedIndex(null)
    }
  }, [currentPattern])

  // Auto-check correctness and apply feedback
  useEffect(() => {
    if (predictedIndex >= 0 && expectedIndex !== null) {
      const correct = predictedIndex === expectedIndex
      setIsCorrect(correct)

      // Automatically apply feedback after a short delay (avoid rapid re-application)
      const feedbackKey = `${predictedIndex}-${expectedIndex}-${maxActivation.toFixed(2)}`
      if (lastFeedbackRef.current !== feedbackKey) {
        lastFeedbackRef.current = feedbackKey
        setTimeout(() => {
          applyAutomaticFeedback(correct, expectedIndex)
        }, 500)
      }
    } else {
      setIsCorrect(null)
    }
  }, [predictedIndex, expectedIndex, maxActivation])

  const meetsThreshold = maxActivation >= confidenceThreshold

  const applyAutomaticFeedback = async (correct, correctOutput) => {
    try {
      const response = await fetch('/api/reinforcement/feedback', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          confidence_threshold: confidenceThreshold,
          correct_output: correctOutput,
          is_correct: correct
        })
      })

      const result = await response.json()

      if (result.success) {
        setFeedbackResult({
          action: result.action,
          reward: result.reward_signal,
          confidence: result.confidence,
          meetsThreshold: result.meets_threshold,
          isCorrect: correct
        })

        // Clear feedback after 5 seconds
        setTimeout(() => setFeedbackResult(null), 5000)
      }
    } catch (error) {
      console.error('Error applying automatic feedback:', error)
    }
  }

  const getActivationPercentage = (activation) => {
    return Math.min(100, Math.max(0, activation * 100))
  }

  return (
    <div className="output-visualization-panel">
      <h3>🎯 Output Layer</h3>

      <div className="prediction-display">
        <div className="prediction-label">Predicted Pattern:</div>
        <div className={`prediction-value ${maxActivation > 0.1 ? 'active' : ''}`}>
          {predictedPattern}
        </div>
        <div className={`confidence-indicator ${meetsThreshold ? 'high' : 'low'}`}>
          Confidence: {(maxActivation * 100).toFixed(1)}%
          {!meetsThreshold && predictedIndex >= 0 && (
            <span className="confidence-warning"> ⚠️ Below threshold</span>
          )}
        </div>
      </div>

      {/* Auto-Feedback Status */}
      {expectedIndex !== null && predictedIndex >= 0 && (
        <div className={`auto-feedback-status ${isCorrect ? 'correct' : 'incorrect'}`}>
          <div className="status-header">
            {isCorrect ? (
              <>
                <span className="status-icon">✓</span>
                <span className="status-text">Correct Prediction!</span>
              </>
            ) : (
              <>
                <span className="status-icon">✗</span>
                <span className="status-text">Incorrect - Training</span>
              </>
            )}
          </div>
          <div className="status-details">
            Expected: {PATTERN_NAMES[expectedIndex]} |
            Predicted: {PATTERN_NAMES[predictedIndex]}
          </div>
        </div>
      )}

      {/* Confidence Threshold Control */}
      <div className="confidence-threshold-control">
        <label>
          Confidence Threshold: <strong>{(confidenceThreshold * 100).toFixed(0)}%</strong>
        </label>
        <input
          type="range"
          min="0"
          max="100"
          value={confidenceThreshold * 100}
          onChange={(e) => setConfidenceThreshold(e.target.value / 100)}
          className="threshold-slider"
        />
        <div className="threshold-marks">
          <span>0%</span>
          <span>50%</span>
          <span>100%</span>
        </div>
      </div>

      <div className="output-neurons">
        {outputActivations && outputActivations.map((activation, index) => {
          const percentage = getActivationPercentage(activation)
          const isMax = activation === maxActivation && maxActivation > 0.1
          const isExpected = index === expectedIndex

          return (
            <div key={index} className="output-neuron-container">
              <div className="output-neuron-label">
                {PATTERN_NAMES[index]}
                {isExpected && <span className="expected-marker"> (Expected)</span>}
              </div>

              <div className="output-neuron-bar-container">
                <div
                  className={`output-neuron-bar ${isMax ? 'predicted' : ''} ${isExpected ? 'expected' : ''}`}
                  style={{
                    width: `${percentage}%`,
                    backgroundColor: PATTERN_COLORS[index],
                    opacity: isMax ? 1 : 0.6
                  }}
                />
                <div className="output-neuron-value">
                  {activation.toFixed(3)}
                </div>
              </div>

              <div className={`output-neuron-indicator ${isMax ? 'active' : ''}`}>
                {isMax && '✓'}
              </div>
            </div>
          )
        })}
      </div>

      {/* Automatic Feedback Result */}
      {feedbackResult && (
        <div className={`feedback-result ${feedbackResult.isCorrect ? 'reward' : 'punish'}`}>
          <strong>
            {feedbackResult.action === 'reward_strong' && '🎉 Strong Reinforcement Applied'}
            {feedbackResult.action === 'reward_weak' && '👍 Weak Reinforcement Applied'}
            {feedbackResult.action === 'punish_strong' && '⚠️ Strong Correction Applied'}
            {feedbackResult.action === 'punish_weak' && '📝 Weak Correction Applied'}
          </strong>
          <div className="feedback-details">
            Reward Signal: {feedbackResult.reward > 0 ? '+' : ''}{feedbackResult.reward.toFixed(2)} |
            Confidence: {(feedbackResult.confidence * 100).toFixed(1)}% |
            {feedbackResult.meetsThreshold ? ' ✓ Met threshold' : ' ⚠️ Below threshold'}
          </div>
        </div>
      )}

      <div className="output-legend">
        <div className="legend-item">
          <div className="legend-color" style={{ backgroundColor: PATTERN_COLORS[0] }}></div>
          <span>Vertical</span>
        </div>
        <div className="legend-item">
          <div className="legend-color" style={{ backgroundColor: PATTERN_COLORS[1] }}></div>
          <span>Horizontal</span>
        </div>
        <div className="legend-item">
          <div className="legend-color" style={{ backgroundColor: PATTERN_COLORS[2] }}></div>
          <span>Diagonal \</span>
        </div>
        <div className="legend-item">
          <div className="legend-color" style={{ backgroundColor: PATTERN_COLORS[3] }}></div>
          <span>Diagonal /</span>
        </div>
      </div>
    </div>
  )
}

export default OutputVisualization
