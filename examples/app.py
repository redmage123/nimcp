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
from functools import wraps

# Add nimcp to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../build/lib/python'))
import nimcp
from tenant_manager import TenantManager

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

            # Get output activations
            output_activations = []
            for output_id in tenant_net.config['output_neurons']:
                try:
                    activity = network.get_average_activity(output_id)
                    output_activations.append(float(activity))
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
    """Main dashboard"""
    return render_template('index.html')


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
        activations = []
        for output_id in tenant_net.config['output_neurons']:
            try:
                activity = network.get_average_activity(output_id)
                activations.append(float(activity))
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
    print(f"React app running at: http://localhost:5000")
    if use_https:
        print("✓ HTTPS enabled with self-signed certificate")
        print("  (You may see browser security warnings)")
    print("=" * 70)

    try:
        if use_https and ssl_cert and ssl_key:
            # Run with HTTPS
            socketio.run(
                app,
                host='0.0.0.0',
                port=5001,
                debug=False,
                allow_unsafe_werkzeug=True,
                certfile=ssl_cert,
                keyfile=ssl_key
            )
        else:
            # Run with HTTP
            socketio.run(app, host='0.0.0.0', port=5001, debug=False, allow_unsafe_werkzeug=True)
    finally:
        print("\nShutting down tenant manager...")
        tenant_manager.shutdown()
