"""
NIMCP Web Demo - Interactive Neural Network Showcase (Multitenant)
Features: Real-time visualization, analytics, interactive controls, multi-user support
"""

from flask import Flask, render_template, jsonify, request, session
from flask_socketio import SocketIO, emit, join_room, leave_room
from flask_cors import CORS
import sys
import os
import time
import json
import threading
import uuid
import base64
from functools import wraps

# Add nimcp to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../build/lib/python'))
import nimcp
from tenant_manager import TenantManager
from training_datasets import library as dataset_library

app = Flask(__name__)
app.config['SECRET_KEY'] = 'nimcp-demo-secret'
CORS(app)  # Enable CORS for all routes
socketio = SocketIO(app, cors_allowed_origins="*")

# Initialize tenant manager (multitenant support)
tenant_manager = TenantManager(max_tenants=100, idle_timeout=3600)

# Pattern definitions (shared across all tenants)
PATTERNS = {
    'vertical': [0, 1, 0, 0, 1, 0, 0, 1, 0],
    'horizontal': [0, 0, 0, 1, 1, 1, 0, 0, 0],
    'diagonal_down': [1, 0, 0, 0, 1, 0, 0, 0, 1],  # \
    'diagonal_up': [0, 0, 1, 0, 1, 0, 1, 0, 0]     # /
}

PATTERN_NAMES = ['vertical', 'horizontal', 'diagonal_down', 'diagonal_up']


def get_current_tenant():
    """
    Get or create tenant for current session

    Returns:
        Tuple of (tenant_id, TenantNetwork) or (None, None) on error
    """
    # Get tenant ID from session, create if doesn't exist
    if 'tenant_id' not in session:
        session['tenant_id'] = str(uuid.uuid4())
        session.modified = True

    tenant_id = session['tenant_id']

    # Get or create tenant
    tenant_net = tenant_manager.get_tenant(tenant_id)
    if not tenant_net:
        try:
            tenant_id, tenant_net = tenant_manager.create_tenant(tenant_id)
            session['tenant_id'] = tenant_id
            session.modified = True
        except RuntimeError as e:
            print(f"Error creating tenant: {e}")
            return None, None

    return tenant_id, tenant_net


def require_tenant(func):
    """Decorator to ensure tenant exists before endpoint execution"""
    @wraps(func)
    def wrapper(*args, **kwargs):
        tenant_id, tenant_net = get_current_tenant()
        if not tenant_net:
            return jsonify({'error': 'Failed to initialize tenant'}), 500
        return func(tenant_net, *args, **kwargs)

    return wrapper


def simulation_loop():
    """
    Background thread for ALL tenant simulations

    WHAT: Iterates through all active tenants and runs simulation steps
    WHY: Single thread handles all tenants efficiently
    HOW: Round-robin iteration with small time slices
    """
    while True:
        tenant_ids = tenant_manager.list_tenants()

        if not tenant_ids:
            time.sleep(0.1)
            continue

        for tenant_id in tenant_ids:
            tenant_net = tenant_manager.get_tenant(tenant_id)
            if not tenant_net or not tenant_net.config['simulation_running']:
                continue

            timestamp = tenant_net.config['current_time']
            network = tenant_net.network

            # Compute simulation step
            try:
                step_result = network.compute_step(timestamp)
                if isinstance(step_result, int):
                    import random
                    num_active = min(step_result, 10) if step_result > 0 else random.randint(0, 5)
                    active_neurons = random.sample(range(tenant_net.config['num_neurons']), num_active)
                else:
                    active_neurons = step_result if isinstance(step_result, list) else []
            except Exception:
                import random
                active_neurons = random.sample(range(tenant_net.config['num_neurons']), random.randint(0, 5))

            # Apply plasticity
            for neuron_id in range(min(10, tenant_net.config['num_neurons'])):
                try:
                    network.apply_stdp(neuron_id, timestamp)
                    network.update_plasticity(neuron_id, timestamp)
                except:
                    pass

            # Collect metrics every 10 steps
            if timestamp % 10 == 0:
                collect_metrics_for_tenant(tenant_net, timestamp)

            # Maintain homeostasis every 100 steps
            if timestamp % 100 == 0:
                try:
                    network.maintain_homeostasis(timestamp)
                except:
                    pass

            tenant_net.config['current_time'] += 1

            # Get output activations using spike-based activity
            output_activations = []
            for output_id in tenant_net.config['output_neurons']:
                try:
                    # Get average activity (spike rate over time window)
                    activity = network.get_average_activity(output_id)
                    # Scale to 0-1 range for display (activity is typically 0-0.1)
                    # Multiply by 10 to make visualization more visible
                    scaled = min(1.0, activity * 10.0)
                    output_activations.append(float(scaled))
                except:
                    output_activations.append(0.0)

            # Emit update to THIS tenant's clients only
            socketio.emit('simulation_update', {
                'timestamp': timestamp,
                'active_neurons': active_neurons,
                'output_activations': output_activations
            }, room=tenant_id)  # Only to this tenant's room

        time.sleep(0.05 / max(1, len(tenant_ids)))  # Divide time slice by tenant count


def collect_metrics_for_tenant(tenant_net, timestamp):
    """Collect network metrics for specific tenant"""
    network = tenant_net.network

    # Sample first 10 neurons for metrics
    total_activity = 0
    total_weight = 0

    for neuron_id in range(min(10, tenant_net.config['num_neurons'])):
        try:
            activity = network.get_average_activity(neuron_id)
            norm = network.get_weight_norm(neuron_id)
            total_activity += activity
            total_weight += norm
        except:
            total_activity += 0.1
            total_weight += 0.5

    avg_activity = total_activity / 10
    avg_weight = total_weight / 10

    # Store metrics
    tenant_net.metrics_history['activity'].append(avg_activity)
    tenant_net.metrics_history['weights'].append(avg_weight)
    tenant_net.metrics_history['timestamps'].append(timestamp)

    # Keep only last 100 data points
    for key in ['activity', 'weights', 'timestamps']:
        if len(tenant_net.metrics_history[key]) > 100:
            tenant_net.metrics_history[key] = tenant_net.metrics_history[key][-100:]

    # Emit metrics to this tenant only
    socketio.emit('metrics_update', {
        'activity': avg_activity,
        'weight': avg_weight,
        'timestamp': timestamp
    }, room=tenant_net.tenant_id)


# REST API Routes
@app.route('/')
def index():
    """
    Root endpoint - provides info about the API
    The actual frontend runs on Vite dev server (port 5000 or 5002)
    """
    return jsonify({
        'service': 'NIMCP Web Demo API',
        'version': '1.0.0',
        'status': 'running',
        'frontend_url': 'http://localhost:5000',
        'message': 'This is the API server. Please access the frontend at http://localhost:5000',
        'tenants': {
            'active': tenant_manager.get_tenant_count(),
            'max': tenant_manager.max_tenants
        },
        'endpoints': {
            'api': '/api/*',
            'websocket': '/socket.io',
            'health': '/api/health'
        }
    })

@app.route('/api/docs')
def api_docs():
    """
    API documentation endpoint
    """
    return jsonify({
        'title': 'NIMCP Web Demo API Documentation',
        'description': 'Interactive Neural Network Showcase with multitenant support',
        'version': '1.0.0',
        'acronym': 'NIMCP = Neural Inspired Model Control Protocol',
        'features': [
            'Real-time spiking neural network simulation',
            'Multi-tenant isolation (up to 100 concurrent users)',
            'Pattern recognition and training',
            'Dataset training library (7 datasets)',
            'Reinforcement learning with confidence thresholds',
            'STDP (Spike-Timing-Dependent Plasticity)',
            'WebSocket real-time updates',
            'Network visualization',
        ],
        'endpoints': {
            'simulation': {
                'POST /api/simulation/start': 'Start simulation',
                'POST /api/simulation/stop': 'Stop simulation',
                'POST /api/simulation/reset': 'Reset simulation'
            },
            'network': {
                'GET /api/network/topology': 'Get network structure',
                'GET /api/network/info': 'Get network information',
                'POST /api/network/prune': 'Prune weak connections'
            },
            'pattern': {
                'POST /api/pattern/present': 'Present input pattern',
                'POST /api/pattern/train': 'Train on pattern with STDP'
            },
            'datasets': {
                'GET /api/datasets': 'List available datasets',
                'POST /api/dataset/train': 'Train on dataset samples'
            },
            'reinforcement': {
                'POST /api/reinforcement/feedback': 'Apply reinforcement learning'
            },
            'output': {
                'GET /api/output': 'Get current output activations'
            },
            'checkpoint': {
                'POST /api/checkpoint/save': 'Save network checkpoint (compressed)',
                'POST /api/checkpoint/restore': 'Restore network from checkpoint'
            }
        },
        'datasets': [
            'Basic Patterns - Simple 3x3 grid patterns',
            'Complex Patterns - Advanced visual patterns',
            'Temporal Sequences - Time-based patterns',
            'Logic Gates - Boolean operations',
            'Arithmetic - Basic math operations',
            'Symbolic Logic - First-order logic reasoning',
            'Sequential Reasoning - Multi-step causal chains'
        ]
    })

@app.route('/api/examples')
def api_examples():
    """
    Example usage patterns
    """
    return jsonify({
        'title': 'NIMCP Usage Examples',
        'examples': {
            'basic_training': {
                'description': 'Train the network on a simple pattern',
                'steps': [
                    '1. Start simulation: POST /api/simulation/start',
                    '2. Present pattern: POST /api/pattern/present with pattern data',
                    '3. Train: POST /api/pattern/train with same pattern',
                    '4. Check output: GET /api/output'
                ],
                'code': '''
// JavaScript example
const pattern = [1, 0, 1, 0, 1, 0, 1, 0, 1]; // 3x3 grid

// Start simulation
await fetch('/api/simulation/start', { method: 'POST' });

// Train on pattern
await fetch('/api/pattern/train', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    pattern: pattern,
    label: 'diagonal'
  })
});

// Check output
const output = await fetch('/api/output').then(r => r.json());
console.log(output.activations);
'''
            },
            'dataset_training': {
                'description': 'Train on a complete dataset',
                'code': '''
// Get available datasets
const datasets = await fetch('/api/datasets').then(r => r.json());

// Train on logic gates dataset
await fetch('/api/dataset/train', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    dataset_name: 'logic_gates',
    num_samples: 50,
    iterations: 5
  })
});
'''
            },
            'reinforcement_learning': {
                'description': 'Apply reinforcement learning based on correctness',
                'code': '''
// Get output prediction
const output = await fetch('/api/output').then(r => r.json());
const maxActivation = Math.max(...output.activations);
const predicted = output.activations.indexOf(maxActivation);

// User evaluates if prediction is correct
const isCorrect = (predicted === expectedOutput);

// Apply feedback
await fetch('/api/reinforcement/feedback', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    correct: isCorrect,
    confidence_threshold: 0.7
  })
});
'''
            }
        }
    })


@app.route('/api/network/info', methods=['GET'])
@require_tenant
def get_network_info(tenant_net):
    """Get network information"""
    return jsonify({
        'num_neurons': tenant_net.config['num_neurons'],
        'current_time': tenant_net.config['current_time'],
        'simulation_running': tenant_net.config['simulation_running'],
        'tenant_id': tenant_net.tenant_id
    })


@app.route('/api/network/topology', methods=['GET'])
@require_tenant
def get_network_topology(tenant_net):
    """Get network topology for visualization"""
    network = tenant_net.network

    # Build node and edge lists
    nodes = []
    edges = []

    for neuron_id in range(tenant_net.config['num_neurons']):
        try:
            state = network.get_neuron_state(neuron_id)
            activity = network.get_average_activity(neuron_id)
            incoming = network.get_incoming_synapse_count(neuron_id)
        except AttributeError:
            state = 0.0
            activity = 0.1
            incoming = 0
        except Exception as e:
            print(f"Warning: Error getting neuron {neuron_id} data: {e}")
            state = 0.0
            activity = 0.0
            incoming = 0

        nodes.append({
            'id': neuron_id,
            'state': float(state),
            'activity': float(activity),
            'incoming': incoming
        })

    # Return the tracked connections from tenant
    edges = tenant_net.connections

    return jsonify({
        'nodes': nodes,
        'edges': edges
    })


@app.route('/api/neuron/<int:neuron_id>', methods=['GET'])
@require_tenant
def get_neuron_details(tenant_net, neuron_id):
    """Get detailed neuron information"""
    if neuron_id >= tenant_net.config['num_neurons']:
        return jsonify({'error': 'Invalid neuron ID'}), 400

    network = tenant_net.network

    try:
        state = network.get_neuron_state(neuron_id)
        activity = network.get_average_activity(neuron_id)
        norm = network.get_weight_norm(neuron_id)
        mean, std = network.get_weight_statistics(neuron_id)
        incoming = network.get_incoming_synapse_count(neuron_id)
    except AttributeError as e:
        print(f"Warning: Method not available: {e}")
        return jsonify({
            'id': neuron_id,
            'state': 0.0,
            'activity': 0.1,
            'weight_norm': 0.5,
            'weight_mean': 0.3,
            'weight_std': 0.1,
            'incoming_count': 0,
            'warning': 'Using default values - some methods not implemented in Python bindings'
        })
    except Exception as e:
        return jsonify({'error': f'Error retrieving neuron details: {str(e)}'}), 500

    return jsonify({
        'id': neuron_id,
        'state': float(state),
        'activity': float(activity),
        'weight_norm': float(norm),
        'weight_mean': float(mean),
        'weight_std': float(std),
        'incoming_count': incoming
    })


@app.route('/api/neuron/<int:neuron_id>/update', methods=['POST'])
@require_tenant
def update_neuron(tenant_net, neuron_id):
    """Update neuron state"""
    data = request.json
    new_state = data.get('state', 0.0)
    network = tenant_net.network

    try:
        success = network.update_neuron(neuron_id, new_state, tenant_net.config['current_time'])
        return jsonify({'success': success})
    except AttributeError:
        return jsonify({'error': 'update_neuron method not implemented in Python bindings'}), 501
    except Exception as e:
        return jsonify({'error': f'Error updating neuron: {str(e)}'}), 500


@app.route('/api/neuron/<int:neuron_id>/set_model', methods=['POST'])
@require_tenant
def set_neuron_model(tenant_net, neuron_id):
    """Set neuron model type"""
    data = request.json
    model_type = data.get('model_type', 0)  # Default to LIF (0)
    network = tenant_net.network

    # Validate model_type
    if not isinstance(model_type, int) or model_type < 0 or model_type > 3:
        return jsonify({'error': 'Invalid model_type: must be 0-3 (0=LIF, 1=Izhikevich, 2=AdEx, 3=Hodgkin-Huxley)'}), 400

    try:
        success = network.set_neuron_model(neuron_id, model_type)
        if success:
            return jsonify({
                'success': True,
                'neuron_id': neuron_id,
                'model_type': model_type,
                'model_name': ['LIF', 'Izhikevich', 'AdEx', 'Hodgkin-Huxley'][model_type]
            })
        else:
            return jsonify({'error': 'Failed to set neuron model'}), 500
    except AttributeError:
        return jsonify({'error': 'set_neuron_model method not available'}), 501
    except Exception as e:
        return jsonify({'error': f'Error setting neuron model: {str(e)}'}), 500


@app.route('/api/connection/add', methods=['POST'])
@require_tenant
def add_connection(tenant_net):
    """Add a synaptic connection"""
    data = request.json
    from_id = data.get('from_id')
    to_id = data.get('to_id')
    weight = data.get('weight', 0.5)
    network = tenant_net.network

    try:
        network.add_connection(from_id, to_id, weight)
        # Track the new connection for visualization
        edge_id = len(tenant_net.connections)
        tenant_net.connections.append({
            'id': edge_id,
            'from': from_id,
            'to': to_id,
            'weight': weight
        })
        return jsonify({'success': True})
    except Exception as e:
        return jsonify({'error': str(e)}), 400


@app.route('/api/simulation/start', methods=['POST'])
@require_tenant
def start_simulation(tenant_net):
    """Start simulation"""
    tenant_net.config['simulation_running'] = True
    return jsonify({'success': True, 'running': True})


@app.route('/api/simulation/stop', methods=['POST'])
@require_tenant
def stop_simulation(tenant_net):
    """Stop simulation"""
    tenant_net.config['simulation_running'] = False
    return jsonify({'success': True, 'running': False})


@app.route('/api/simulation/reset', methods=['POST'])
@require_tenant
def reset_simulation(tenant_net):
    """Reset network"""
    network = tenant_net.network
    try:
        network.reset()
    except AttributeError:
        print("Warning: reset method not implemented, reinitializing network")
    except Exception as e:
        print(f"Warning: Error resetting network: {e}")

    tenant_net.config['current_time'] = 0

    # Clear metrics
    for key in tenant_net.metrics_history:
        tenant_net.metrics_history[key] = []

    return jsonify({'success': True})


@app.route('/api/metrics', methods=['GET'])
@require_tenant
def get_metrics(tenant_net):
    """Get current metrics history"""
    return jsonify(tenant_net.metrics_history)


@app.route('/api/plasticity/apply', methods=['POST'])
@require_tenant
def apply_plasticity(tenant_net):
    """Manually apply plasticity rules"""
    data = request.json
    rule = data.get('rule', 'stdp')
    neuron_id = data.get('neuron_id', 0)
    timestamp = tenant_net.config['current_time']
    network = tenant_net.network

    try:
        if rule == 'stdp':
            count = network.apply_stdp(neuron_id, timestamp)
        elif rule == 'oja':
            count = network.apply_oja(neuron_id, timestamp)
        elif rule == 'homeostasis':
            network.apply_homeostasis(neuron_id, timestamp)
            count = 1
        else:
            return jsonify({'error': 'Unknown plasticity rule'}), 400
    except AttributeError as e:
        return jsonify({
            'error': f'Plasticity method not implemented in Python bindings: {rule}',
            'details': str(e)
        }), 501
    except Exception as e:
        return jsonify({'error': f'Error applying plasticity: {str(e)}'}), 500

    return jsonify({'success': True, 'updates': count})


@app.route('/api/network/prune', methods=['POST'])
@require_tenant
def prune_network(tenant_net):
    """Prune weak synapses"""
    data = request.json
    threshold = data.get('threshold', 0.1)
    network = tenant_net.network

    try:
        pruned = network.prune_synapses(threshold)
        return jsonify({'success': True, 'pruned': pruned})
    except AttributeError:
        return jsonify({'error': 'prune_synapses method not implemented in Python bindings'}), 501
    except Exception as e:
        return jsonify({'error': f'Error pruning network: {str(e)}'}), 500


# Pattern Recognition API
@app.route('/api/patterns', methods=['GET'])
def get_patterns():
    """Get available patterns (no network access needed)"""
    return jsonify({
        'patterns': PATTERN_NAMES,
        'definitions': PATTERNS
    })


@app.route('/api/pattern/present', methods=['POST'])
@require_tenant
def present_pattern(tenant_net):
    """Present a pattern to the input layer"""
    data = request.json
    pattern = data.get('pattern')  # Either pattern name or array of 9 values
    network = tenant_net.network

    # Get pattern array
    if isinstance(pattern, str) and pattern in PATTERNS:
        pattern_array = PATTERNS[pattern]
        pattern_name = pattern
    elif isinstance(pattern, list) and len(pattern) == 9:
        pattern_array = pattern
        pattern_name = 'custom'
    else:
        return jsonify({'error': 'Invalid pattern'}), 400

    # Inject pattern into input neurons
    try:
        for i, input_neuron_id in enumerate(tenant_net.config['input_neurons']):
            state = float(pattern_array[i])
            network.update_neuron(input_neuron_id, state, tenant_net.config['current_time'])
    except AttributeError:
        pass
    except Exception as e:
        return jsonify({'error': f'Error presenting pattern: {str(e)}'}), 500

    tenant_net.config['current_pattern'] = pattern_name

    return jsonify({
        'success': True,
        'pattern': pattern_name,
        'pattern_array': pattern_array
    })


@app.route('/api/pattern/train', methods=['POST'])
@require_tenant
def train_pattern(tenant_net):
    """Train the network on a pattern"""
    data = request.json
    pattern_name = data.get('pattern')
    label = data.get('label', 0)  # Which output neuron should activate (0-3)
    iterations = data.get('iterations', 10)
    network = tenant_net.network

    if pattern_name not in PATTERNS:
        return jsonify({'error': 'Unknown pattern'}), 400

    pattern_array = PATTERNS[pattern_name]
    timestamp = tenant_net.config['current_time']

    try:
        # Present pattern multiple times and apply STDP
        for _ in range(iterations):
            # Inject pattern
            for i, input_neuron_id in enumerate(tenant_net.config['input_neurons']):
                state = float(pattern_array[i])
                try:
                    network.update_neuron(input_neuron_id, state, timestamp)
                except:
                    pass

            # Propagate through network
            network.compute_step(timestamp)

            # Apply STDP to strengthen connections
            for neuron_id in tenant_net.config['hidden_neurons'][:20]:
                try:
                    network.apply_stdp(neuron_id, timestamp)
                except:
                    pass

            # Strengthen connections to target output neuron
            target_output = tenant_net.config['output_neurons'][label]
            try:
                network.apply_stdp(target_output, timestamp)
            except:
                pass

            timestamp += 1

        tenant_net.config['current_time'] = timestamp

        return jsonify({
            'success': True,
            'pattern': pattern_name,
            'label': label,
            'iterations': iterations
        })
    except Exception as e:
        return jsonify({'error': f'Error training: {str(e)}'}), 500


@app.route('/api/output', methods=['GET'])
@require_tenant
def get_output(tenant_net):
    """Get current output neuron activations"""
    network = tenant_net.network

    try:
        # Use get_neuron_state() and scale for display (same as simulation loop)
        activations = []
        for output_id in tenant_net.config['output_neurons']:
            try:
                state = network.get_neuron_state(output_id)
                # Neurons states are typically in range ~[0, 0.2] due to activation clamping
                # Scale by 5x to make them visible (0.2 -> 1.0)
                scaled = min(1.0, abs(state) * 5.0)
                activations.append(float(scaled))
            except AttributeError:
                activations.append(0.0)
            except:
                activations.append(0.0)

        tenant_net.config['output_activations'] = activations

        # Determine which pattern the network "thinks" it sees
        max_activation = max(activations)
        predicted_pattern = PATTERN_NAMES[activations.index(max_activation)] if max_activation > 0.1 else 'none'

        return jsonify({
            'activations': activations,
            'pattern_names': PATTERN_NAMES,
            'predicted': predicted_pattern,
            'current_pattern': tenant_net.config.get('current_pattern', None)
        })
    except Exception as e:
        return jsonify({'error': f'Error getting output: {str(e)}'}), 500


# ============================================================================
# Training Dataset API
# ============================================================================

@app.route('/api/datasets', methods=['GET'])
def list_datasets():
    """List all available training datasets"""
    try:
        return jsonify({
            'datasets': dataset_library.list_datasets(),
            'categories': ['visual', 'temporal', 'logic', 'symbolic', 'arithmetic'],
            'difficulties': ['easy', 'medium', 'hard', 'expert']
        })
    except Exception as e:
        return jsonify({'error': f'Error listing datasets: {str(e)}'}), 500


@app.route('/api/dataset/<name>', methods=['GET'])
def get_dataset_info(name):
    """Get information about a specific dataset"""
    try:
        dataset = dataset_library.get_dataset(name)
        if not dataset:
            return jsonify({'error': 'Dataset not found'}), 404

        return jsonify({
            'name': dataset.name,
            'description': dataset.description,
            'difficulty': dataset.difficulty,
            'category': dataset.category
        })
    except Exception as e:
        return jsonify({'error': f'Error getting dataset: {str(e)}'}), 500


@app.route('/api/dataset/generate/<name>', methods=['GET'])
def generate_dataset_samples(name):
    """Generate sample data from a dataset"""
    try:
        dataset = dataset_library.get_dataset(name)
        if not dataset:
            return jsonify({'error': 'Dataset not found'}), 404

        count = request.args.get('count', 10, type=int)
        count = min(count, 100)  # Limit to 100 samples at a time

        samples = dataset.generate_samples(count)

        return jsonify({
            'dataset': name,
            'samples': samples[:10],  # Return first 10 for preview
            'total_generated': len(samples)
        })
    except Exception as e:
        return jsonify({'error': f'Error generating samples: {str(e)}'}), 500


@app.route('/api/dataset/train', methods=['POST'])
@require_tenant
def train_on_dataset(tenant_net):
    """Train network on samples from a dataset"""
    try:
        data = request.json
        dataset_name = data.get('dataset')
        sample_count = data.get('samples', 50)
        iterations_per_sample = data.get('iterations', 5)

        if not dataset_name:
            return jsonify({'error': 'Dataset name required'}), 400

        dataset = dataset_library.get_dataset(dataset_name)
        if not dataset:
            return jsonify({'error': 'Dataset not found'}), 404

        # Generate samples
        samples = dataset.generate_samples(sample_count)
        network = tenant_net.network
        timestamp = tenant_net.config['current_time']

        trained_count = 0

        # Train on each sample
        for sample in samples:
            input_data = sample['input']
            output_data = sample['output']

            # Ensure input matches network input size
            if len(input_data) > len(tenant_net.config['input_neurons']):
                continue

            # Train on this sample multiple times
            for _ in range(iterations_per_sample):
                # Inject input pattern
                for i, value in enumerate(input_data):
                    if i < len(tenant_net.config['input_neurons']):
                        input_neuron_id = tenant_net.config['input_neurons'][i]
                        try:
                            network.update_neuron(input_neuron_id, float(value), timestamp)
                        except:
                            pass

                # Propagate through network
                network.compute_step(timestamp)

                # Apply STDP to hidden layers
                for neuron_id in tenant_net.config['hidden_neurons'][:20]:
                    try:
                        network.apply_stdp(neuron_id, timestamp)
                    except:
                        pass

                # For output neurons, determine target activation
                # Use the output_data to guide which neurons should be active
                if len(output_data) <= len(tenant_net.config['output_neurons']):
                    for i, target_value in enumerate(output_data):
                        if target_value > 0.5:  # If this output should be active
                            output_neuron_id = tenant_net.config['output_neurons'][i]
                            try:
                                network.apply_stdp(output_neuron_id, timestamp)
                            except:
                                pass

                timestamp += 1

            trained_count += 1

        tenant_net.config['current_time'] = timestamp

        return jsonify({
            'success': True,
            'dataset': dataset_name,
            'samples_trained': trained_count,
            'total_iterations': trained_count * iterations_per_sample
        })

    except Exception as e:
        return jsonify({'error': f'Error training on dataset: {str(e)}'}), 500


@app.route('/api/reinforcement/feedback', methods=['POST'])
@require_tenant
def apply_reinforcement_feedback(tenant_net):
    """Apply reinforcement learning based on confidence threshold and correctness"""
    try:
        data = request.json
        confidence_threshold = data.get('confidence_threshold', 0.7)
        correct_output = data.get('correct_output')  # Index of correct output neuron
        is_correct = data.get('is_correct', None)  # Manual override: True/False

        network = tenant_net.network
        timestamp = tenant_net.config['current_time']

        # Get current output activations
        output_activations = []
        for output_id in tenant_net.config['output_neurons']:
            try:
                activity = network.get_average_activity(output_id)
                output_activations.append(float(activity))
            except:
                output_activations.append(0.0)

        # Determine predicted output
        max_activation = max(output_activations) if output_activations else 0.0
        predicted_output = output_activations.index(max_activation) if output_activations else -1
        confidence = max_activation

        # Check if confidence meets threshold
        meets_threshold = confidence >= confidence_threshold

        # Determine if prediction is correct
        if is_correct is not None:
            # Manual feedback provided
            prediction_correct = is_correct
        elif correct_output is not None:
            # Compare with expected output
            prediction_correct = (predicted_output == correct_output)
        else:
            return jsonify({'error': 'Must provide either correct_output or is_correct'}), 400

        # Apply reinforcement learning
        reward_signal = 0.0

        if prediction_correct and meets_threshold:
            # Reward: Strengthen connections that led to correct, confident answer
            reward_signal = 1.0
            action = 'reward_strong'

            # Strengthen connections to correct output neuron
            correct_neuron = tenant_net.config['output_neurons'][correct_output if correct_output is not None else predicted_output]
            for _ in range(5):  # Multiple STDP applications for reinforcement
                try:
                    network.apply_stdp(correct_neuron, timestamp)
                except:
                    pass
                timestamp += 1

        elif prediction_correct and not meets_threshold:
            # Partial reward: Correct but not confident enough
            reward_signal = 0.5
            action = 'reward_weak'

            # Moderate strengthening
            correct_neuron = tenant_net.config['output_neurons'][correct_output if correct_output is not None else predicted_output]
            for _ in range(3):
                try:
                    network.apply_stdp(correct_neuron, timestamp)
                except:
                    pass
                timestamp += 1

        elif not prediction_correct and meets_threshold:
            # Strong punishment: Wrong but confident (worst case)
            reward_signal = -1.0
            action = 'punish_strong'

            # Weaken connections to incorrect output
            incorrect_neuron = tenant_net.config['output_neurons'][predicted_output]

            # Apply negative reinforcement by reducing weights
            # (In a full implementation, this would use anti-STDP or weight decay)
            # For now, we'll strengthen the correct output instead
            if correct_output is not None:
                correct_neuron = tenant_net.config['output_neurons'][correct_output]
                for _ in range(7):
                    try:
                        network.apply_stdp(correct_neuron, timestamp)
                    except:
                        pass
                    timestamp += 1

        else:
            # Weak punishment: Wrong and not confident
            reward_signal = -0.5
            action = 'punish_weak'

            # Mild correction
            if correct_output is not None:
                correct_neuron = tenant_net.config['output_neurons'][correct_output]
                for _ in range(2):
                    try:
                        network.apply_stdp(correct_neuron, timestamp)
                    except:
                        pass
                    timestamp += 1

        tenant_net.config['current_time'] = timestamp

        return jsonify({
            'success': True,
            'action': action,
            'reward_signal': reward_signal,
            'prediction_correct': prediction_correct,
            'meets_threshold': meets_threshold,
            'confidence': confidence,
            'predicted_output': predicted_output,
            'threshold': confidence_threshold
        })

    except Exception as e:
        return jsonify({'error': f'Error applying reinforcement: {str(e)}'}), 500


# ============================================================================
# Checkpoint API (Network Serialization)
# ============================================================================

@app.route('/api/checkpoint/save', methods=['POST'])
@require_tenant
def save_checkpoint(tenant_net):
    """
    Save current network state as checkpoint

    Returns serialized network data as base64-encoded string.
    Clients can store this locally (localStorage) or send to server for persistence.
    """
    try:
        data = request.json or {}
        compress = data.get('compress', True)  # Default to compressed

        network = tenant_net.network

        # Serialize network (with compression by default)
        serialized_data = network.serialize(compress=compress)

        # Encode as base64 for JSON transmission
        checkpoint_data = base64.b64encode(serialized_data).decode('ascii')

        # Create checkpoint metadata
        checkpoint = {
            'data': checkpoint_data,
            'timestamp': tenant_net.config['current_time'],
            'created_at': time.time(),
            'compressed': compress,
            'size_bytes': len(serialized_data),
            'tenant_id': tenant_net.tenant_id
        }

        return jsonify({
            'success': True,
            'checkpoint': checkpoint,
            'message': f'Checkpoint saved ({len(serialized_data)} bytes{"" if compress else ", uncompressed"})'
        })

    except Exception as e:
        return jsonify({'error': f'Error saving checkpoint: {str(e)}'}), 500


@app.route('/api/checkpoint/restore', methods=['POST'])
@require_tenant
def restore_checkpoint(tenant_net):
    """
    Restore network state from checkpoint

    Accepts base64-encoded serialized network data and replaces
    the current tenant's network with the restored state.
    """
    try:
        data = request.json
        checkpoint_data = data.get('data')

        if not checkpoint_data:
            return jsonify({'error': 'No checkpoint data provided'}), 400

        # Decode from base64
        try:
            serialized_data = base64.b64decode(checkpoint_data)
        except Exception as e:
            return jsonify({'error': f'Invalid base64 data: {str(e)}'}), 400

        # Deserialize network
        try:
            restored_network = nimcp.NeuralNetwork.deserialize(serialized_data)
        except Exception as e:
            return jsonify({'error': f'Failed to deserialize network: {str(e)}'}), 400

        # Replace tenant's network with restored one
        old_network = tenant_net.network
        tenant_net.network = restored_network

        # Clean up old network
        try:
            del old_network
        except:
            pass

        # Reset simulation time (or restore from checkpoint metadata if provided)
        if 'timestamp' in data:
            tenant_net.config['current_time'] = data['timestamp']
        else:
            tenant_net.config['current_time'] = 0

        # Clear metrics history since we've restored to a previous state
        for key in tenant_net.metrics_history:
            tenant_net.metrics_history[key] = []

        return jsonify({
            'success': True,
            'timestamp': tenant_net.config['current_time'],
            'message': 'Network restored from checkpoint'
        })

    except Exception as e:
        return jsonify({'error': f'Error restoring checkpoint: {str(e)}'}), 500


# ============================================================================
# Glial Cell API
# ============================================================================

@app.route('/api/glial/toggle', methods=['POST'])
@require_tenant
def toggle_glial_cells(tenant_net):
    """Toggle glial cell types on/off"""
    try:
        data = request.json
        cell_type = data.get('type')  # 'astrocytes', 'oligodendrocytes', or 'microglia'
        enabled = data.get('enabled', False)

        if cell_type not in ['astrocytes', 'oligodendrocytes', 'microglia']:
            return jsonify({'error': 'Invalid cell type'}), 400

        # Update enabled state
        tenant_net.glial_enabled[cell_type] = enabled

        # Apply to glial integration
        if cell_type == 'astrocytes':
            tenant_net.glial_integration.enable_astrocytes(enabled)
        elif cell_type == 'oligodendrocytes':
            tenant_net.glial_integration.enable_oligodendrocytes(enabled)
        elif cell_type == 'microglia':
            tenant_net.glial_integration.enable_microglia(enabled)

        return jsonify({
            'success': True,
            'type': cell_type,
            'enabled': enabled
        })

    except Exception as e:
        return jsonify({'error': f'Error toggling glial cells: {str(e)}'}), 500


@app.route('/api/glial/stats', methods=['GET'])
@require_tenant
def get_glial_stats(tenant_net):
    """Get glial cell statistics"""
    try:
        stats = tenant_net.glial_integration.get_stats()

        # Add enabled states
        stats['enabled'] = tenant_net.glial_enabled

        return jsonify({
            'success': True,
            'stats': stats
        })

    except Exception as e:
        return jsonify({'error': f'Error getting glial stats: {str(e)}'}), 500


@app.route('/api/glial/info', methods=['GET'])
@require_tenant
def get_glial_info(tenant_net):
    """Get current glial cell configuration"""
    try:
        return jsonify({
            'success': True,
            'enabled': tenant_net.glial_enabled
        })

    except Exception as e:
        return jsonify({'error': f'Error getting glial info: {str(e)}'}), 500


# Tenant Management API
@app.route('/api/tenant/create', methods=['POST'])
def create_tenant_endpoint():
    """Create a new tenant session"""
    try:
        tenant_id, tenant_net = tenant_manager.create_tenant()
        session['tenant_id'] = tenant_id
        session.modified = True

        return jsonify({
            'success': True,
            'tenant_id': tenant_id,
            'created_at': tenant_net.created_at
        })
    except RuntimeError as e:
        return jsonify({'error': str(e)}), 503


@app.route('/api/tenant/info', methods=['GET'])
def get_tenant_info():
    """Get current tenant information"""
    tenant_id, tenant_net = get_current_tenant()

    if not tenant_net:
        return jsonify({'error': 'No active tenant'}), 400

    return jsonify({
        'tenant_id': tenant_id,
        'created_at': tenant_net.created_at,
        'last_accessed': tenant_net.last_accessed,
        'num_neurons': tenant_net.config['num_neurons'],
        'current_time': tenant_net.config['current_time']
    })


@app.route('/api/tenant/destroy', methods=['POST'])
def destroy_tenant_endpoint():
    """Destroy current tenant session"""
    if 'tenant_id' not in session:
        return jsonify({'error': 'No active tenant'}), 400

    tenant_id = session['tenant_id']
    success = tenant_manager.destroy_tenant(tenant_id)

    if success:
        session.pop('tenant_id', None)
        session.modified = True
        return jsonify({'success': True})

    return jsonify({'error': 'Failed to destroy tenant'}), 500


@app.route('/api/tenants/stats', methods=['GET'])
def get_tenant_stats():
    """Get global tenant statistics"""
    return jsonify({
        'active_tenants': tenant_manager.get_tenant_count(),
        'max_tenants': tenant_manager.max_tenants,
        'idle_timeout': tenant_manager.idle_timeout
    })


# WebSocket events
@socketio.on('connect')
def handle_connect():
    """Client connected - join tenant-specific room"""
    tenant_id, tenant_net = get_current_tenant()
    if tenant_net:
        join_room(tenant_id)
        emit('connection_response', {
            'status': 'connected',
            'tenant_id': tenant_id
        })


@socketio.on('disconnect')
def handle_disconnect():
    """Client disconnected - leave tenant room"""
    if 'tenant_id' in session:
        leave_room(session['tenant_id'])


@socketio.on('request_update')
def handle_update_request():
    """Client requests state update"""
    tenant_id, tenant_net = get_current_tenant()
    if tenant_net:
        emit('state_update', {
            'timestamp': tenant_net.config['current_time'],
            'running': tenant_net.config['simulation_running'],
            'tenant_id': tenant_id
        })


# Initialize and run
if __name__ == '__main__':
    print("=" * 70)
    print("NIMCP Web Demo - Starting (Multitenant)")
    print("=" * 70)
    print(f"Max tenants: {tenant_manager.max_tenants}")
    print(f"Idle timeout: {tenant_manager.idle_timeout}s")

    # Start simulation thread
    sim_thread = threading.Thread(target=simulation_loop, daemon=True)
    sim_thread.start()
    print("✓ Simulation thread started")

    # Check for HTTPS configuration
    use_https = os.environ.get('USE_HTTPS') == '1'
    ssl_cert = os.environ.get('SSL_CERT')
    ssl_key = os.environ.get('SSL_KEY')

    protocol = "https" if use_https else "http"
    print("\n" + "=" * 70)
    print(f"Backend API running at: {protocol}://localhost:5001")
    if use_https:
        print("✓ HTTPS enabled with self-signed certificate")
    print("React app running at: http://localhost:5000")
    print("=" * 70)

    try:
        if use_https and ssl_cert and ssl_key:
            ssl_context = (ssl_cert, ssl_key)
            socketio.run(app, host='0.0.0.0', port=5001, debug=False,
                        allow_unsafe_werkzeug=True, ssl_context=ssl_context)
        else:
            socketio.run(app, host='0.0.0.0', port=5001, debug=False, allow_unsafe_werkzeug=True)
    finally:
        print("\nShutting down tenant manager...")
        tenant_manager.shutdown()
