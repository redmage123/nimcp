#!/usr/bin/env python3
"""
NIMCP Simple Web Demo - Clean Backend

A simplified web interface that's easy to understand and use.
No complex React build, no WebSockets, just clean REST API.

USAGE:
    python3 simple_web_demo.py

Then open: http://localhost:5500
"""

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import sys
import os
import numpy as np

# Add NIMCP to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../build/lib/python'))

try:
    import nimcp
except ImportError:
    print("Error: NIMCP Python bindings not found")
    print("Build NIMCP first: cd build && make")
    sys.exit(1)

#=============================================================================
# FLASK APP CONFIGURATION
#=============================================================================

app = Flask(__name__, static_folder='.')
CORS(app)  # Allow cross-origin requests

# Global brain instance
brain = None
training_data = []
metrics = {
    'total_predictions': 0,
    'total_training': 0,
    'avg_confidence': 0.0,
    'avg_epistemic_quality': 0.0
}

#=============================================================================
# API ENDPOINTS - Simple and Clean
#=============================================================================

@app.route('/')
def index():
    """Serve the simple UI"""
    return send_from_directory('.', 'simple_ui.html')

@app.route('/api/init', methods=['POST'])
def init_brain():
    """Initialize the brain"""
    global brain

    data = request.json
    input_dim = data.get('input_dim', 8)
    num_classes = data.get('num_classes', 3)

    try:
        brain = nimcp.Brain(
            name="simple_web_demo",
            size=nimcp.BrainSize.SMALL,
            task=nimcp.BrainTask.CLASSIFICATION,
            num_inputs=input_dim,
            num_outputs=num_classes
        )

        return jsonify({
            'success': True,
            'message': 'Brain initialized successfully',
            'neurons': 1000,
            'input_dim': input_dim,
            'num_classes': num_classes
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@app.route('/api/train', methods=['POST'])
def train():
    """Train the brain with a single example"""
    global brain, training_data, metrics

    if brain is None:
        return jsonify({'success': False, 'error': 'Brain not initialized'}), 400

    data = request.json
    features = data.get('features', [])
    label = data.get('label', '')
    confidence = data.get('confidence', 0.9)

    try:
        success = brain.learn(features, label, confidence)

        if success:
            training_data.append({'features': features, 'label': label})
            metrics['total_training'] += 1

        return jsonify({
            'success': success,
            'total_training': metrics['total_training'],
            'message': f'Trained on label: {label}'
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@app.route('/api/predict', methods=['POST'])
def predict():
    """Make a prediction"""
    global brain, metrics

    if brain is None:
        return jsonify({'success': False, 'error': 'Brain not initialized'}), 400

    data = request.json
    features = data.get('features', [])

    try:
        result = brain.predict(features)

        metrics['total_predictions'] += 1
        metrics['avg_confidence'] = (
            (metrics['avg_confidence'] * (metrics['total_predictions'] - 1) +
             result.get('confidence', 0.0)) / metrics['total_predictions']
        )
        metrics['avg_epistemic_quality'] = (
            (metrics['avg_epistemic_quality'] * (metrics['total_predictions'] - 1) +
             result.get('epistemic_quality', 0.0)) / metrics['total_predictions']
        )

        return jsonify({
            'success': True,
            'prediction': result.get('label', 'unknown'),
            'confidence': result.get('confidence', 0.0),
            'epistemic_quality': result.get('epistemic_quality', 0.0),
            'credibility': result.get('credibility', 0.0),
            'requires_verification': result.get('requires_verification', False),
            'bias_detected': result.get('bias_detected', False),
            'ethical_approved': result.get('ethical_approved', True)
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@app.route('/api/metrics', methods=['GET'])
def get_metrics():
    """Get current metrics"""
    return jsonify({
        'success': True,
        'metrics': metrics,
        'training_examples': len(training_data)
    })

@app.route('/api/reset', methods=['POST'])
def reset():
    """Reset the brain"""
    global brain, training_data, metrics

    brain = None
    training_data = []
    metrics = {
        'total_predictions': 0,
        'total_training': 0,
        'avg_confidence': 0.0,
        'avg_epistemic_quality': 0.0
    }

    return jsonify({
        'success': True,
        'message': 'Brain reset successfully'
    })

#=============================================================================
# MAIN
#=============================================================================

if __name__ == '__main__':
    print("\n" + "="*60)
    print("NIMCP Simple Web Demo - Clean Backend")
    print("="*60)
    print("\nStarting server...")
    print("Open: http://localhost:5500")
    print("Press Ctrl+C to stop")
    print("="*60 + "\n")

    app.run(host='0.0.0.0', port=5500, debug=True)
