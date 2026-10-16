"""
NIMCP Web Demo - Flask Backend v2.8.0
======================================

WHAT: REST API backend demonstrating NIMCP brain with COW cloning
WHY:  Show NIMCP capabilities including memory-efficient brain cloning
HOW:  Flask server exposing brain training/prediction/cloning endpoints

Features:
- Real-time training with progress tracking
- Interactive predictions
- Brain metrics and statistics
- Copy-on-Write brain cloning
- Multi-brain management
- Memory savings visualization
- Iris flower classification demo
"""

from flask import Flask, request, jsonify, send_file, redirect, send_from_directory
from flask_cors import CORS
import nimcp
import time
import threading
import queue
from datetime import datetime
import os
from benchmarks import NIMCPBenchmark, MNISTLoader, ComparativeBenchmark
from datasets import get_dataset, list_datasets

app = Flask(__name__,
            static_folder='../frontend/build',
            static_url_path='')
CORS(app)  # Enable CORS for React frontend

# Global brain instances and metrics (support for multiple brains + COW clones)
brains = {}  # Dict of brain_id -> brain instance
brain_metadata = {}  # Dict of brain_id -> metadata (parent_id, is_cow_clone, created_at, etc.)
brain_lock = threading.Lock()
next_brain_id = 0
primary_brain_id = None  # Track the main brain for backward compatibility

# Legacy single-brain support
brain = None
training_history = []
prediction_history = []
brain_metrics = {
    'total_trained': 0,
    'total_predictions': 0,
    'training_time': 0.0,
    'prediction_time': 0.0,
    'last_loss': 0.0,
    'accuracy_estimate': 0.0,
    'created_at': None,
    'status': 'not_initialized'
}

# Current dataset selection
current_dataset = None
current_dataset_name = 'iris'  # Default to Iris

# Benchmark state
benchmark_runner = None
benchmark_lock = threading.Lock()
benchmark_in_progress = False
benchmark_results = {}

# Iris dataset for demo
IRIS_DATA = {
    'setosa': [
        [5.1, 3.5, 1.4, 0.2],
        [4.9, 3.0, 1.4, 0.2],
        [4.7, 3.2, 1.3, 0.2],
        [4.6, 3.1, 1.5, 0.2],
        [5.0, 3.6, 1.4, 0.2],
    ],
    'versicolor': [
        [7.0, 3.2, 4.7, 1.4],
        [6.4, 3.2, 4.5, 1.5],
        [6.9, 3.1, 4.9, 1.5],
        [5.5, 2.3, 4.0, 1.3],
        [6.5, 2.8, 4.6, 1.5],
    ],
    'virginica': [
        [6.3, 3.3, 6.0, 2.5],
        [5.8, 2.7, 5.1, 1.9],
        [7.1, 3.0, 5.9, 2.1],
        [6.3, 2.9, 5.6, 1.8],
        [6.5, 3.0, 5.8, 2.2],
    ]
}

#=============================================================================
# Helper Functions
#=============================================================================

def normalize_features(features):
    """
    WHAT: Normalize features to 0-1 range
    WHY:  Improve training stability and convergence
    HOW:  Use dataset-specific normalization from current_dataset
    """
    global current_dataset

    # If dataset has a normalize method, use it
    if current_dataset and hasattr(current_dataset, 'normalize'):
        return current_dataset.normalize(features)

    # Otherwise, assume features are already normalized (0-1 range) or apply simple clipping
    return [max(0.0, min(1.0, float(f))) for f in features]

def calculate_accuracy():
    """
    WHAT: Estimate brain accuracy on recent predictions
    WHY:  Provide accuracy metric for dashboard
    HOW:  Count correct predictions in last 20 predictions
    """
    if len(prediction_history) < 5:
        return 0.0

    recent = prediction_history[-20:] if len(prediction_history) > 20 else prediction_history
    correct = sum(1 for p in recent if p.get('correct', False))
    return (correct / len(recent)) * 100.0

#=============================================================================
# API Endpoints
#=============================================================================

@app.route('/')
def index():
    """Serve React App"""
    return send_from_directory(app.static_folder, 'index.html')

# Catch all routes for React Router
@app.route('/<path:path>')
def serve_react_app(path):
    """Serve static files or index.html for React Router"""
    file_path = os.path.join(app.static_folder, path)
    if os.path.exists(file_path):
        return send_from_directory(app.static_folder, path)
    else:
        return send_from_directory(app.static_folder, 'index.html')

@app.route('/api/init', methods=['POST'])
def init_brain():
    """
    WHAT: Initialize NIMCP brain
    WHY:  Create brain instance for training/prediction
    HOW:  Call nimcp.Brain with config based on selected dataset
    """
    global brain, brain_metrics, brains, brain_metadata, next_brain_id, primary_brain_id
    global current_dataset, current_dataset_name

    try:
        # Get dataset selection from request (default to 'iris')
        # Use get_json(silent=True) to handle invalid JSON gracefully
        data = request.get_json(silent=True) or {}
        dataset_name = data.get('dataset', 'iris')

        # Load the selected dataset
        current_dataset_name = dataset_name
        current_dataset = get_dataset(dataset_name)
        config = current_dataset.get_config()

        with brain_lock:
            # Create brain with dataset-specific configuration
            brain = nimcp.Brain(
                name=f"{dataset_name}_brain",
                size=1,  # BRAIN_SMALL
                task=0,  # TASK_CLASSIFICATION
                num_inputs=config['num_inputs'],
                num_outputs=config['num_outputs']
            )

            brain_metrics['created_at'] = datetime.now().isoformat()
            brain_metrics['status'] = 'initialized'
            brain_metrics['total_trained'] = 0
            brain_metrics['total_predictions'] = 0
            brain_metrics['dataset'] = dataset_name

            # Store in new brain management system
            brain_id = next_brain_id
            next_brain_id += 1
            brains[brain_id] = brain
            brain_metadata[brain_id] = {
                'id': brain_id,
                'name': f"{dataset_name}_brain",
                'parent_id': None,
                'is_cow_clone': False,
                'created_at': datetime.now().isoformat(),
                'clone_count': 0,
                'total_trained': 0,
                'total_predictions': 0,
                'dataset': dataset_name
            }
            primary_brain_id = brain_id

        return jsonify({
            'success': True,
            'message': f'Brain initialized for {current_dataset.name}',
            'brain_id': brain_id,
            'dataset': dataset_name,
            'dataset_info': {
                'name': current_dataset.name,
                'description': current_dataset.description,
                'num_inputs': config['num_inputs'],
                'num_outputs': config['num_outputs'],
                'classes': config['classes'],
                'input_type': config['input_type']
            },
            'metrics': brain_metrics
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/train', methods=['POST'])
def train_brain():
    """
    WHAT: Train brain on single example
    WHY:  Update brain with new training data
    HOW:  Normalize features, call brain.learn(), track metrics
    """
    global brain, brain_metrics, training_history

    if brain is None:
        return jsonify({'success': False, 'error': 'Brain not initialized'}), 400

    try:
        data = request.get_json(silent=True)
        features = data.get('features')  # [sepal_length, sepal_width, petal_length, petal_width]
        label = data.get('label')        # 'setosa', 'versicolor', or 'virginica'
        confidence = data.get('confidence', 1.0)

        # Guard: Validate inputs
        if not features or not label:
            return jsonify({'success': False, 'error': 'Missing features or label'}), 400

        if len(features) != 4:
            return jsonify({'success': False, 'error': 'Expected 4 features'}), 400

        # Normalize features
        normalized = normalize_features(features)

        # Train
        start_time = time.time()
        with brain_lock:
            loss = brain.learn(normalized, label, confidence)
        elapsed = time.time() - start_time

        # Update metrics
        brain_metrics['total_trained'] += 1
        brain_metrics['training_time'] += elapsed
        brain_metrics['last_loss'] = float(loss)
        brain_metrics['status'] = 'training'

        # Record training history
        training_history.append({
            'timestamp': datetime.now().isoformat(),
            'loss': float(loss),
            'label': label,
            'elapsed': elapsed
        })

        # Keep only last 100 entries
        if len(training_history) > 100:
            training_history = training_history[-100:]

        return jsonify({
            'success': True,
            'loss': float(loss),
            'elapsed': elapsed,
            'metrics': brain_metrics
        })

    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/train-batch', methods=['POST'])
def train_batch():
    """
    WHAT: Train on multiple examples at once
    WHY:  Quickly initialize brain with dataset
    HOW:  Iterate through examples, train on each
    """
    global brain, brain_metrics, training_history

    if brain is None:
        return jsonify({'success': False, 'error': 'Brain not initialized'}), 400

    try:
        data = request.get_json(silent=True)
        examples = data.get('examples', [])

        if not examples:
            return jsonify({'success': False, 'error': 'No examples provided'}), 400

        results = []
        for example in examples:
            features = example.get('features')
            label = example.get('label')
            confidence = example.get('confidence', 1.0)

            normalized = normalize_features(features)

            with brain_lock:
                loss = brain.learn(normalized, label, confidence)

            brain_metrics['total_trained'] += 1
            brain_metrics['last_loss'] = float(loss)

            results.append({
                'label': label,
                'loss': float(loss)
            })

            training_history.append({
                'timestamp': datetime.now().isoformat(),
                'loss': float(loss),
                'label': label,
                'elapsed': 0.0
            })

        # Keep only last 100 entries
        if len(training_history) > 100:
            training_history = training_history[-100:]

        return jsonify({
            'success': True,
            'trained_count': len(results),
            'results': results,
            'metrics': brain_metrics
        })

    except Exception as e:
        print(f"ERROR in train-batch: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/predict', methods=['POST'])
def predict():
    """
    WHAT: Make prediction on features
    WHY:  Test trained brain, show inference
    HOW:  Normalize features, call brain.predict(), track metrics
    """
    global brain, brain_metrics, prediction_history

    if brain is None:
        return jsonify({'success': False, 'error': 'Brain not initialized'}), 400

    try:
        data = request.get_json(silent=True)
        features = data.get('features')
        true_label = data.get('true_label')  # Optional, for accuracy tracking

        # Guard: Validate inputs
        if not features:
            return jsonify({'success': False, 'error': 'Missing features'}), 400

        if len(features) != 4:
            return jsonify({'success': False, 'error': 'Expected 4 features'}), 400

        # Normalize features
        normalized = normalize_features(features)

        # Predict
        start_time = time.time()
        with brain_lock:
            predicted_label, confidence = brain.predict(normalized)
        elapsed = time.time() - start_time

        # Update metrics
        brain_metrics['total_predictions'] += 1
        brain_metrics['prediction_time'] += elapsed
        brain_metrics['status'] = 'predicting'

        # Check if prediction was correct
        correct = (predicted_label == true_label) if true_label else None

        # Record prediction history
        prediction_history.append({
            'timestamp': datetime.now().isoformat(),
            'predicted': predicted_label,
            'confidence': float(confidence),
            'true_label': true_label,
            'correct': correct,
            'elapsed': elapsed
        })

        # Keep only last 100 entries
        if len(prediction_history) > 100:
            prediction_history = prediction_history[-100:]

        # Update accuracy estimate
        brain_metrics['accuracy_estimate'] = calculate_accuracy()

        return jsonify({
            'success': True,
            'prediction': predicted_label,
            'confidence': float(confidence),
            'correct': correct,
            'elapsed': elapsed,
            'metrics': brain_metrics
        })

    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/metrics', methods=['GET'])
def get_metrics():
    """
    WHAT: Get current brain metrics and history
    WHY:  Provide data for dashboard visualization
    HOW:  Return metrics, training/prediction history
    """
    return jsonify({
        'success': True,
        'metrics': brain_metrics,
        'training_history': training_history[-50:],  # Last 50 training examples
        'prediction_history': prediction_history[-20:]  # Last 20 predictions
    })

@app.route('/api/status', methods=['GET'])
def get_status():
    """
    WHAT: Get brain status
    WHY:  Check if brain is initialized and ready
    HOW:  Return current status and basic info
    """
    return jsonify({
        'success': True,
        'initialized': brain is not None,
        'status': brain_metrics['status'],
        'metrics': brain_metrics
    })

@app.route('/api/datasets', methods=['GET'])
def get_available_datasets():
    """
    WHAT: List all available datasets
    WHY:  Allow frontend to show dataset selection
    HOW:  Return list of dataset metadata
    """
    try:
        datasets_list = list_datasets()
        return jsonify({
            'success': True,
            'datasets': datasets_list
        })
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/dataset', methods=['GET'])
def get_current_dataset_data():
    """
    WHAT: Get current dataset examples for training
    WHY:  Provide demo data to frontend
    HOW:  Return examples from selected dataset
    """
    global current_dataset, current_dataset_name

    try:
        # Initialize default dataset if not set
        if current_dataset is None:
            current_dataset = get_dataset('iris')
            current_dataset_name = 'iris'

        # Get sample examples from dataset
        examples_count = 30
        examples = current_dataset.get_examples(count=examples_count)

        # Convert to format expected by frontend (organize by class)
        dataset_by_class = {}
        for features, label in examples:
            if label not in dataset_by_class:
                dataset_by_class[label] = []
            dataset_by_class[label].append(features)

        config = current_dataset.get_config()

        return jsonify({
            'success': True,
            'dataset': dataset_by_class,
            'dataset_name': current_dataset_name,
            'dataset_info': {
                'name': current_dataset.name,
                'description': current_dataset.description,
                'num_inputs': config['num_inputs'],
                'num_outputs': config['num_outputs'],
                'input_type': config['input_type']
            },
            'feature_names': config['feature_names'],
            'classes': config['classes']
        })
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/reset', methods=['POST'])
def reset_brain():
    """
    WHAT: Reset brain and clear all metrics
    WHY:  Start fresh demo
    HOW:  Destroy brain, clear history, reset metrics
    """
    global brain, brain_metrics, training_history, prediction_history

    with brain_lock:
        brain = None
        training_history = []
        prediction_history = []
        brain_metrics = {
            'total_trained': 0,
            'total_predictions': 0,
            'training_time': 0.0,
            'prediction_time': 0.0,
            'last_loss': 0.0,
            'accuracy_estimate': 0.0,
            'created_at': None,
            'status': 'not_initialized'
        }

    return jsonify({
        'success': True,
        'message': 'Brain reset successfully'
    })

#=============================================================================
# Copy-on-Write (COW) Cloning Endpoints
#=============================================================================

@app.route('/api/brain/<int:brain_id>/clone_cow', methods=['POST'])
def clone_brain_cow(brain_id):
    """
    WHAT: Create COW clone of specified brain
    WHY:  Enable efficient memory sharing for parallel inference
    HOW:  Call brain.clone_cow(), track relationships and memory savings
    """
    global brains, brain_metadata, next_brain_id

    if brain_id not in brains:
        return jsonify({'success': False, 'error': f'Brain {brain_id} not found'}), 404

    try:
        with brain_lock:
            # Get parent brain
            parent_brain = brains[brain_id]

            # Create COW clone
            start_time = time.time()
            clone = parent_brain.clone_cow()
            clone_time = time.time() - start_time

            # Assign new brain ID
            clone_id = next_brain_id
            next_brain_id += 1

            # Store clone
            brains[clone_id] = clone
            brain_metadata[clone_id] = {
                'id': clone_id,
                'name': f"{brain_metadata[brain_id]['name']}_clone_{clone_id}",
                'parent_id': brain_id,
                'is_cow_clone': True,
                'created_at': datetime.now().isoformat(),
                'clone_time': clone_time,
                'clone_count': 0,
                'total_trained': 0,
                'total_predictions': 0
            }

            # Update parent clone count
            brain_metadata[brain_id]['clone_count'] += 1

            # Get COW statistics
            stats = clone.probe()

        return jsonify({
            'success': True,
            'message': f'COW clone created in {clone_time*1000:.2f}ms',
            'clone_id': clone_id,
            'parent_id': brain_id,
            'clone_time': clone_time,
            'cow_stats': {
                'is_cow_clone': stats.get('is_cow_clone', True),
                'shared_bytes': stats.get('cow_shared_bytes', 0),
                'private_bytes': stats.get('cow_private_bytes', 0),
                'ref_count': stats.get('cow_ref_count', 0),
                'memory_savings_pct': (stats.get('cow_shared_bytes', 0) / max(stats.get('memory_bytes', 1), 1)) * 100
            },
            'metadata': brain_metadata[clone_id]
        })

    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/brain/<int:brain_id>/cow_stats', methods=['GET'])
def get_cow_stats(brain_id):
    """
    WHAT: Get COW statistics for specified brain
    WHY:  Monitor memory sharing and efficiency
    HOW:  Call brain.probe(), extract COW metrics
    """
    if brain_id not in brains:
        return jsonify({'success': False, 'error': f'Brain {brain_id} not found'}), 404

    try:
        with brain_lock:
            brain_instance = brains[brain_id]
            stats = brain_instance.probe()
            metadata = brain_metadata[brain_id]

        return jsonify({
            'success': True,
            'brain_id': brain_id,
            'metadata': metadata,
            'cow_stats': {
                'is_cow_clone': stats.get('is_cow_clone', False),
                'shared_bytes': stats.get('cow_shared_bytes', 0),
                'private_bytes': stats.get('cow_private_bytes', 0),
                'total_bytes': stats.get('memory_bytes', 0),
                'ref_count': stats.get('cow_ref_count', 0),
                'memory_savings_pct': (stats.get('cow_shared_bytes', 0) / max(stats.get('memory_bytes', 1), 1)) * 100
            },
            'architecture': {
                'num_neurons': stats.get('num_neurons', 0),
                'num_synapses': stats.get('num_synapses', 0),
                'num_inputs': stats.get('num_inputs', 0),
                'num_outputs': stats.get('num_outputs', 0)
            }
        })

    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/brains', methods=['GET'])
def list_brains():
    """
    WHAT: List all brain instances (originals + clones)
    WHY:  Provide overview of brain hierarchy
    HOW:  Return all brains with metadata and relationships
    """
    brain_list = []

    with brain_lock:
        for brain_id, metadata in brain_metadata.items():
            # Get quick stats
            try:
                brain_instance = brains[brain_id]
                stats = brain_instance.probe()

                brain_info = {
                    **metadata,
                    'memory_bytes': stats.get('memory_bytes', 0),
                    'cow_shared_bytes': stats.get('cow_shared_bytes', 0),
                    'cow_private_bytes': stats.get('cow_private_bytes', 0),
                    'is_cow_clone': stats.get('is_cow_clone', False)
                }
                brain_list.append(brain_info)
            except Exception as e:
                # Skip if brain stats unavailable
                brain_list.append({**metadata, 'error': str(e)})

    return jsonify({
        'success': True,
        'brains': brain_list,
        'total_count': len(brain_list)
    })


@app.route('/api/brain/<int:brain_id>/delete', methods=['DELETE'])
def delete_brain(brain_id):
    """
    WHAT: Delete a brain instance
    WHY:  Clean up clones or reset state
    HOW:  Remove from brains dict, update parent clone count
    """
    global brains, brain_metadata, primary_brain_id

    if brain_id not in brains:
        return jsonify({'success': False, 'error': f'Brain {brain_id} not found'}), 404

    # Prevent deleting primary brain if it has clones
    metadata = brain_metadata[brain_id]
    if metadata['clone_count'] > 0:
        return jsonify({
            'success': False,
            'error': f'Cannot delete brain with {metadata["clone_count"]} active clones'
        }), 400

    try:
        with brain_lock:
            # Update parent's clone count if this is a clone
            parent_id = metadata['parent_id']
            if parent_id is not None and parent_id in brain_metadata:
                brain_metadata[parent_id]['clone_count'] -= 1

            # Delete brain
            del brains[brain_id]
            del brain_metadata[brain_id]

            # Clear primary_brain_id if deleting primary
            if primary_brain_id == brain_id:
                primary_brain_id = None

        return jsonify({
            'success': True,
            'message': f'Brain {brain_id} deleted successfully'
        })

    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/benchmark/mnist/start', methods=['POST'])
def start_mnist_benchmark():
    """
    WHAT: Start MNIST benchmark with 100K neurons
    WHY:  Demonstrate competitive performance on standard benchmark
    HOW:  Run benchmark in background thread, stream progress
    """
    global benchmark_runner, benchmark_in_progress, benchmark_results

    if benchmark_in_progress:
        return jsonify({
            'success': False,
            'error': 'Benchmark already in progress'
        }), 400

    try:
        data = request.get_json(silent=True) or {}
        num_neurons = data.get('num_neurons', 100000)
        n_epochs = data.get('epochs', 10)
        batch_size = data.get('batch_size', 64)
        use_gpu = data.get('use_gpu', True)

        with benchmark_lock:
            benchmark_in_progress = True
            benchmark_results = {
                'status': 'starting',
                'progress': 0,
                'current_epoch': 0,
                'total_epochs': n_epochs
            }

            benchmark_runner = NIMCPBenchmark(
                dataset_name="mnist",
                num_neurons=num_neurons,
                use_gpu=use_gpu
            )

        # Run benchmark in background thread
        def run_benchmark():
            global benchmark_in_progress, benchmark_results
            try:
                metrics = benchmark_runner.run_mnist(n_epochs=n_epochs, batch_size=batch_size)
                with benchmark_lock:
                    benchmark_results = {
                        'status': 'completed',
                        'metrics': metrics,
                        'progress': 100
                    }
            except Exception as e:
                with benchmark_lock:
                    benchmark_results = {
                        'status': 'failed',
                        'error': str(e)
                    }
            finally:
                benchmark_in_progress = False

        thread = threading.Thread(target=run_benchmark, daemon=True)
        thread.start()

        return jsonify({
            'success': True,
            'message': f'MNIST benchmark started with {num_neurons:,} neurons',
            'config': {
                'num_neurons': num_neurons,
                'epochs': n_epochs,
                'batch_size': batch_size,
                'use_gpu': use_gpu
            }
        })

    except Exception as e:
        benchmark_in_progress = False
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/benchmark/status', methods=['GET'])
def get_benchmark_status():
    """
    WHAT: Get current benchmark status and progress
    WHY:  Allow frontend to poll for progress updates
    HOW:  Return current benchmark_results dict
    """
    with benchmark_lock:
        return jsonify({
            'success': True,
            'in_progress': benchmark_in_progress,
            'results': benchmark_results
        })


@app.route('/api/benchmark/results', methods=['GET'])
def get_benchmark_results():
    """
    WHAT: Get completed benchmark results
    WHY:  Display final metrics and comparisons
    HOW:  Return full benchmark metrics and comparison table
    """
    with benchmark_lock:
        if not benchmark_results or benchmark_results.get('status') != 'completed':
            return jsonify({
                'success': False,
                'error': 'No completed benchmark results available'
            }), 404

        metrics = benchmark_results.get('metrics', {})
        comparison = ComparativeBenchmark.compare(metrics)

        return jsonify({
            'success': True,
            'metrics': metrics,
            'comparison': comparison
        })


@app.route('/api/init-large', methods=['POST'])
def init_large_brain():
    """
    WHAT: Initialize NIMCP brain with 100K neurons
    WHY:  Scale to competitive size for benchmarking
    HOW:  Create BRAIN_SIZE_LARGE with GPU support
    """
    global brain, brain_metrics

    try:
        data = request.get_json(silent=True) or {}
        num_inputs = data.get('num_inputs', 784)  # Default: MNIST
        num_outputs = data.get('num_outputs', 10)
        num_neurons = data.get('num_neurons', 100000)

        with brain_lock:
            # Create large brain for benchmarking
            brain = nimcp.Brain(
                name=f"large_brain_{num_neurons}",
                size=nimcp.BRAIN_SIZE_LARGE,
                task=nimcp.TASK_CLASSIFICATION,
                num_inputs=num_inputs,
                num_outputs=num_outputs
            )

            brain_metrics['created_at'] = datetime.now().isoformat()
            brain_metrics['status'] = 'initialized'
            brain_metrics['num_neurons'] = num_neurons
            brain_metrics['total_trained'] = 0
            brain_metrics['total_predictions'] = 0

        return jsonify({
            'success': True,
            'message': f'Large brain initialized with ~{num_neurons:,} neurons',
            'config': {
                'num_neurons': num_neurons,
                'num_inputs': num_inputs,
                'num_outputs': num_outputs
            },
            'metrics': brain_metrics
        })

    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/network/visualize', methods=['GET'])
def visualize_network():
    """
    WHAT: Get network structure for visualization
    WHY:  Enable frontend to render neural network graph
    HOW:  Return neuron positions, connections, activity levels

    RETURNS: {
        'neurons': [{'id': int, 'x': float, 'y': float, 'z': float, 'activity': float, 'type': str}],
        'connections': [{'from': int, 'to': int, 'weight': float}],
        'stats': {...}
    }
    """
    if brain is None:
        return jsonify({'success': False, 'error': 'Brain not initialized'}), 400

    try:
        # Get brain statistics
        try:
            stats = brain.get_stats()
        except AttributeError:
            # Brain might not have get_stats method, create minimal stats
            stats = {
                'num_neurons': 100,
                'num_synapses': 0
            }

        # For visualization, sample subset of neurons (max 1000 for performance)
        # Position neurons in 3D space based on layer structure
        neurons_data = []
        connections_data = []

        # TODO: Implement actual neuron/synapse extraction from brain
        # For now, return mock data for visualization
        num_neurons = min(stats.get('num_neurons', 0), 1000)

        for i in range(num_neurons):
            neurons_data.append({
                'id': i,
                'x': (i % 10) * 10,  # Grid layout
                'y': (i // 10) % 10 * 10,
                'z': (i // 100) * 10,
                'activity': 0.5,  # TODO: Get actual activity
                'type': 'excitatory' if i % 5 != 0 else 'inhibitory'
            })

        return jsonify({
            'success': True,
            'network': {
                'neurons': neurons_data,
                'connections': connections_data,
                'total_neurons': stats.get('num_neurons', 0)
            }
        })

    except Exception as e:
        print(f"ERROR in network visualization: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/docs')
def documentation():
    """
    WHAT: Serve documentation page
    WHY:  Provide access to README and documentation
    HOW:  Render simple HTML page with links to docs
    """
    docs_html = """
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>NIMCP Web Demo - Documentation</title>
        <style>
            body {
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', 'Oxygen', 'Ubuntu', 'Cantarell', sans-serif;
                max-width: 900px;
                margin: 50px auto;
                padding: 20px;
                line-height: 1.6;
                background: #0a0a0a;
                color: #e0e0e0;
            }
            h1 { color: #61dafb; border-bottom: 2px solid #61dafb; padding-bottom: 10px; }
            h2 { color: #61dafb; margin-top: 30px; }
            a { color: #61dafb; text-decoration: none; }
            a:hover { text-decoration: underline; }
            .section {
                background: #1a1a1a;
                padding: 20px;
                margin: 20px 0;
                border-radius: 8px;
                border-left: 4px solid #61dafb;
            }
            code {
                background: #2a2a2a;
                padding: 2px 6px;
                border-radius: 3px;
                color: #ffa500;
            }
            pre {
                background: #2a2a2a;
                padding: 15px;
                border-radius: 5px;
                overflow-x: auto;
            }
            .back-link {
                display: inline-block;
                margin: 20px 0;
                padding: 10px 20px;
                background: #61dafb;
                color: #0a0a0a;
                border-radius: 5px;
                font-weight: bold;
            }
            .back-link:hover {
                background: #4fa8c5;
                text-decoration: none;
            }
        </style>
    </head>
    <body>
        <h1>🧠 NIMCP Web Demo Documentation</h1>

        <a href="/" class="back-link">← Back to Demo</a>

        <div class="section">
            <h2>What is NIMCP?</h2>
            <p><strong>NIMCP (Neural Inspired Model Control Protocol)</strong> is a biologically-inspired spiking neural network library that provides temporal pattern recognition and adaptive learning capabilities.</p>
        </div>

        <div class="section">
            <h2>📖 Quick Start</h2>
            <p>This demo showcases NIMCP's capabilities through an interactive Iris flower classification task.</p>
            <ol>
                <li>Click <strong>"Initialize Brain"</strong> to create a new neural network</li>
                <li>Use <strong>"Train on Dataset"</strong> to train the brain on iris flower samples</li>
                <li>Enter custom features or use presets to <strong>make predictions</strong></li>
                <li>Monitor real-time <strong>metrics and visualizations</strong> as the brain learns</li>
            </ol>
        </div>

        <div class="section">
            <h2>🔧 API Endpoints</h2>
            <pre><code>POST /api/init           - Initialize brain
POST /api/train          - Train on single example
POST /api/train-batch    - Train on multiple examples
POST /api/predict        - Make prediction
GET  /api/metrics        - Get metrics and history
GET  /api/status         - Get brain status
GET  /api/dataset        - Get iris dataset
POST /api/reset          - Reset brain</code></pre>
        </div>

        <div class="section">
            <h2>📊 Features</h2>
            <ul>
                <li><strong>Real-time Visualization</strong> - Watch loss decrease as the brain learns</li>
                <li><strong>Interactive Predictions</strong> - Test the trained brain with custom inputs</li>
                <li><strong>Metrics Dashboard</strong> - Monitor accuracy, performance, and statistics</li>
                <li><strong>Biologically Inspired</strong> - Spiking neural networks with STDP learning</li>
            </ul>
        </div>

        <div class="section">
            <h2>🔗 Additional Resources</h2>
            <ul>
                <li><a href="/api/docs/readme" target="_blank">Full README</a></li>
                <li><a href="/api/docs/quick-start" target="_blank">Quick Start Guide</a></li>
                <li><a href="https://github.com/bbrelin/nimcp" target="_blank">GitHub Repository</a></li>
            </ul>
        </div>

        <div class="section">
            <h2>💡 Example Usage</h2>
            <pre><code>// Initialize brain
fetch('/api/init', { method: 'POST' })

// Train on example
fetch('/api/train', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    features: [5.1, 3.5, 1.4, 0.2],
    label: 'setosa',
    confidence: 1.0
  })
})

// Make prediction
fetch('/api/predict', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    features: [6.3, 3.3, 6.0, 2.5]
  })
})</code></pre>
        </div>

        <a href="/" class="back-link">← Back to Demo</a>

        <p style="text-align: center; margin-top: 50px; color: #666;">
            NIMCP v2.7.0 - Neural Inspired Model Control Protocol
        </p>
    </body>
    </html>
    """
    return docs_html

@app.route('/api/docs/readme')
def serve_readme():
    """
    WHAT: Serve README.md file
    WHY:  Provide detailed documentation
    HOW:  Send README.md from parent directory
    """
    readme_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'README.md')
    if os.path.exists(readme_path):
        return send_file(readme_path, mimetype='text/markdown', download_name='NIMCP-README.md')
    else:
        return jsonify({'success': False, 'error': 'README not found'}), 404

@app.route('/api/docs/quick-start')
def serve_quickstart():
    """
    WHAT: Serve QUICK_START.md file
    WHY:  Provide quick start guide
    HOW:  Send QUICK_START.md from parent directory
    """
    quickstart_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'QUICK_START.md')
    if os.path.exists(quickstart_path):
        return send_file(quickstart_path, mimetype='text/markdown', download_name='NIMCP-QUICK_START.md')
    else:
        return jsonify({'success': False, 'error': 'Quick Start guide not found'}), 404

#=============================================================================
# Main
#=============================================================================

if __name__ == '__main__':
    print("="*70)
    print("NIMCP Web Demo - Backend Server v2.8.0")
    print("="*70)
    print("Starting Flask server on http://localhost:5000")
    print("")
    print("Core Endpoints:")
    print("  POST /api/init           - Initialize brain")
    print("  POST /api/train          - Train on single example")
    print("  POST /api/train-batch    - Train on multiple examples")
    print("  POST /api/predict        - Make prediction")
    print("  GET  /api/metrics        - Get metrics and history")
    print("  GET  /api/status         - Get brain status")
    print("  GET  /api/dataset        - Get iris dataset")
    print("  POST /api/reset          - Reset brain")
    print("")
    print("COW Cloning Endpoints:")
    print("  POST /api/brain/<id>/clone_cow  - Create COW clone")
    print("  GET  /api/brain/<id>/cow_stats  - Get COW statistics")
    print("  GET  /api/brains                - List all brains")
    print("  DELETE /api/brain/<id>/delete   - Delete brain instance")
    print("")
    print("Documentation:")
    print("  GET  /docs               - Interactive documentation page")
    print("  GET  /api/docs/readme    - Download README.md")
    print("  GET  /api/docs/quick-start - Download QUICK_START.md")
    print("")
    print("Frontend: http://localhost:3000")
    print("="*70)

    app.run(debug=True, host='0.0.0.0', port=5001)
