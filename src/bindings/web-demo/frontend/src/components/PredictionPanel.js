/**
 * PredictionPanel Component (Dataset-Aware)
 * ==========================================
 *
 * WHAT: Interface for making predictions with trained brain on any dataset
 * WHY:  Test brain inference and visualize confidence
 * HOW:  Dynamic form that adapts to dataset configuration
 */

import React, { useState, useEffect } from 'react';

function PredictionPanel({ onPredict, dataset, datasetInfo, loading }) {
  const [featuresInput, setFeaturesInput] = useState('');
  const [trueLabel, setTrueLabel] = useState('');
  const [predictionResult, setPredictionResult] = useState(null);
  const [predicting, setPredicting] = useState(false);

  // Update true label when dataset changes
  useEffect(() => {
    if (datasetInfo && datasetInfo.classes && datasetInfo.classes.length > 0) {
      setTrueLabel('');
    }
  }, [datasetInfo]);

  const handlePredict = async (e) => {
    e.preventDefault();
    setPredicting(true);
    setPredictionResult(null);

    try {
      // Parse features from input
      const features = featuresInput.split(',').map(f => parseFloat(f.trim()));

      // Validate feature count
      if (features.length !== datasetInfo.num_inputs) {
        setPredictionResult({
          success: false,
          error: `Expected ${datasetInfo.num_inputs} features, got ${features.length}`
        });
        setPredicting(false);
        return;
      }

      const result = await onPredict(features, trueLabel || null);
      setPredictionResult(result);
    } catch (err) {
      setPredictionResult({
        success: false,
        error: err.message
      });
    } finally {
      setPredicting(false);
    }
  };

  const loadExample = (className) => {
    if (!dataset || !dataset[className]) return;

    // Load last example (different from training examples)
    const examples = dataset[className];
    const example = examples[examples.length - 1];

    setFeaturesInput(example.join(', '));
    setTrueLabel(className);
  };

  const getConfidenceColor = (confidence) => {
    if (confidence >= 0.8) return '#10b981'; // Green
    if (confidence >= 0.6) return '#f59e0b'; // Orange
    return '#ef4444'; // Red
  };

  const getInputPlaceholder = () => {
    if (!datasetInfo) return 'Enter feature values...';

    if (datasetInfo.input_type === 'image') {
      return `Enter ${datasetInfo.num_inputs} pixel values (0-1), comma-separated`;
    }

    return `Enter ${datasetInfo.num_inputs} feature values, comma-separated`;
  };

  if (!datasetInfo) {
    return (
      <div className="panel prediction-panel">
        <h2>🔮 Prediction Panel</h2>
        <p>Loading dataset information...</p>
      </div>
    );
  }

  return (
    <div className="panel prediction-panel">
      <h2>🔮 Prediction Panel</h2>

      <div className="dataset-badge">
        <strong>Dataset:</strong> {datasetInfo.name}
      </div>

      <form onSubmit={handlePredict}>
        <div className="form-group">
          <label>Feature Values</label>
          <textarea
            value={featuresInput}
            onChange={(e) => setFeaturesInput(e.target.value)}
            placeholder={getInputPlaceholder()}
            rows="3"
            required
            style={{
              width: '100%',
              padding: '0.75rem',
              background: 'rgba(255, 255, 255, 0.05)',
              border: '1px solid var(--border)',
              borderRadius: '8px',
              color: 'var(--text-white)',
              fontSize: '0.9rem',
              fontFamily: 'monospace',
              resize: 'vertical'
            }}
          />
          <small style={{ color: 'var(--text-light)', fontSize: '0.85rem' }}>
            Expected: {datasetInfo.num_inputs} {datasetInfo.input_type} values
          </small>
        </div>

        <div className="form-group">
          <label>True Label (optional, for accuracy tracking)</label>
          <select
            value={trueLabel}
            onChange={(e) => setTrueLabel(e.target.value)}
          >
            <option value="">-- Select if known --</option>
            {datasetInfo.classes.map((cls) => (
              <option key={cls} value={cls}>
                {cls}
              </option>
            ))}
          </select>
        </div>

        <button
          type="submit"
          className="btn btn-primary"
          disabled={loading || predicting}
        >
          {predicting ? 'Predicting...' : 'Make Prediction'}
        </button>
      </form>

      {dataset && Object.keys(dataset).length > 0 && (
        <div className="example-buttons">
          <p className="help-text">Load test example:</p>
          {Object.keys(dataset).slice(0, 6).map((className) => (
            <button
              key={className}
              className="btn btn-outline btn-small"
              onClick={() => loadExample(className)}
            >
              {className}
            </button>
          ))}
        </div>
      )}

      {predictionResult && (
        <div className="prediction-result">
          {predictionResult.success ? (
            <>
              <h3>Prediction Result</h3>
              <div className="result-card">
                <div className="result-item">
                  <span className="result-label">Predicted Class:</span>
                  <span className="result-value prediction-class">
                    {predictionResult.prediction}
                  </span>
                </div>

                <div className="result-item">
                  <span className="result-label">Confidence:</span>
                  <div className="confidence-bar-container">
                    <div
                      className="confidence-bar"
                      style={{
                        width: `${predictionResult.confidence * 100}%`,
                        backgroundColor: getConfidenceColor(predictionResult.confidence)
                      }}
                    />
                    <span className="confidence-text">
                      {(predictionResult.confidence * 100).toFixed(1)}%
                    </span>
                  </div>
                </div>

                {predictionResult.correct !== null && (
                  <div className="result-item">
                    <span className="result-label">Accuracy:</span>
                    <span className={`result-value ${predictionResult.correct ? 'correct' : 'incorrect'}`}>
                      {predictionResult.correct ? '✅ Correct' : '❌ Incorrect'}
                    </span>
                  </div>
                )}

                <div className="result-item">
                  <span className="result-label">Inference Time:</span>
                  <span className="result-value">
                    {(predictionResult.elapsed * 1000).toFixed(2)} ms
                  </span>
                </div>
              </div>
            </>
          ) : (
            <div className="error-message">
              ❌ {predictionResult.error}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export default PredictionPanel;
