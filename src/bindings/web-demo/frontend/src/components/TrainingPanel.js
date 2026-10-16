/**
 * TrainingPanel Component (Dataset-Aware)
 * ========================================
 *
 * WHAT: Interface for training the NIMCP brain on any dataset
 * WHY:  Allow users to train on individual or batch examples
 * HOW:  Dynamic forms that adapt to dataset configuration
 */

import React, { useState, useEffect } from 'react';

function TrainingPanel({ onTrain, onTrainBatch, dataset, datasetInfo, loading }) {
  const [featuresInput, setFeaturesInput] = useState('');
  const [selectedClass, setSelectedClass] = useState('');
  const [confidence, setConfidence] = useState('1.0');
  const [trainingStatus, setTrainingStatus] = useState('');

  // Update selected class when dataset changes
  useEffect(() => {
    if (datasetInfo && datasetInfo.classes && datasetInfo.classes.length > 0) {
      setSelectedClass(datasetInfo.classes[0]);
    }
  }, [datasetInfo]);

  const handleTrain = async (e) => {
    e.preventDefault();
    setTrainingStatus('Training...');

    try {
      // Parse features from input
      const features = featuresInput.split(',').map(f => parseFloat(f.trim()));

      // Validate feature count
      if (features.length !== datasetInfo.num_inputs) {
        setTrainingStatus(`❌ Expected ${datasetInfo.num_inputs} features, got ${features.length}`);
        return;
      }

      const result = await onTrain(features, selectedClass, parseFloat(confidence));
      setTrainingStatus(`✅ Trained! Loss: ${result.loss.toFixed(4)}`);

      setTimeout(() => setTrainingStatus(''), 3000);
    } catch (err) {
      setTrainingStatus(`❌ Error: ${err.message}`);
    }
  };

  const handleQuickTrain = async () => {
    if (!dataset) return;

    setTrainingStatus('Quick training...');

    // Create training batch from dataset
    const examples = [];
    Object.entries(dataset).forEach(([label, samples]) => {
      samples.forEach(features => {
        examples.push({ features, label, confidence: 1.0 });
      });
    });

    try {
      await onTrainBatch(examples);
      setTrainingStatus(`✅ Trained on ${examples.length} examples!`);
      setTimeout(() => setTrainingStatus(''), 3000);
    } catch (err) {
      setTrainingStatus(`❌ Error: ${err.message}`);
    }
  };

  const loadExample = (className) => {
    if (!dataset || !dataset[className]) return;

    const example = dataset[className][0];
    setFeaturesInput(example.join(', '));
    setSelectedClass(className);
  };

  // Get dataset-specific help text
  const getDatasetHelp = () => {
    if (!datasetInfo) return '';

    const exampleCount = dataset ? Object.values(dataset).reduce((sum, arr) => sum + arr.length, 0) : 0;

    return `Train on all ${exampleCount} ${datasetInfo.name.toLowerCase()} examples`;
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
      <div className="panel training-panel">
        <h2>🎓 Training Panel</h2>
        <p>Loading dataset information...</p>
      </div>
    );
  }

  return (
    <div className="panel training-panel">
      <h2>🎓 Training Panel</h2>

      <div className="dataset-badge">
        <strong>Dataset:</strong> {datasetInfo.name}
      </div>

      <div className="quick-train">
        <button
          className="btn btn-success"
          onClick={handleQuickTrain}
          disabled={loading || !dataset}
        >
          ⚡ Quick Train (All Dataset)
        </button>
        <p className="help-text">
          {getDatasetHelp()}
        </p>
      </div>

      <div className="divider">
        <span>OR</span>
      </div>

      <form onSubmit={handleTrain}>
        <h3>Train Single Example</h3>

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
          <label>Class</label>
          <select
            value={selectedClass}
            onChange={(e) => setSelectedClass(e.target.value)}
          >
            {datasetInfo.classes.map((cls) => (
              <option key={cls} value={cls}>
                {cls}
              </option>
            ))}
          </select>
        </div>

        <div className="form-group">
          <label>Confidence (0.0 - 1.0)</label>
          <input
            type="number"
            step="0.1"
            min="0"
            max="1"
            value={confidence}
            onChange={(e) => setConfidence(e.target.value)}
            required
          />
        </div>

        <div className="button-group">
          <button type="submit" className="btn btn-primary" disabled={loading}>
            Train Example
          </button>
        </div>

        {trainingStatus && (
          <div className="status-message">{trainingStatus}</div>
        )}
      </form>

      {dataset && Object.keys(dataset).length > 0 && (
        <div className="example-buttons">
          <p className="help-text">Load example:</p>
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
    </div>
  );
}

export default TrainingPanel;
