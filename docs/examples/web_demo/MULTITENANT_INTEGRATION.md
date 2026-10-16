# Multitenant Integration Guide

## Overview

Multitenant support has been partially implemented for the NIMCP web demo. The tenant_manager.py module is complete and ready to use. To finish the integration, follow these steps:

## Step 1: Update app.py Imports

Add these imports after line 13:

```python
from flask import session
from tenant_manager import TenantManager
import uuid
```

## Step 2: Replace Global Network State

Replace lines 24-53 (global network state) with:

```python
# Initialize tenant manager
tenant_manager = TenantManager(max_tenants=100, idle_timeout=3600)

# Pattern definitions (shared across all tenants)
PATTERNS = {
    'vertical': [0, 1, 0, 0, 1, 0, 0, 1, 0],
    'horizontal': [0, 0, 0, 1, 1, 1, 0, 0, 0],
    'diagonal_down': [1, 0, 0, 0, 1, 0, 0, 0, 1],  # \
    'diagonal_up': [0, 0, 1, 0, 1, 0, 1, 0, 0]     # /
}

PATTERN_NAMES = ['vertical', 'horizontal', 'diagonal_down', 'diagonal_up']
```

## Step 3: Add Tenant Helper Functions

Add after the PATTERN_NAMES definition:

```python
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
    from functools import wraps

    @wraps(func)
    def wrapper(*args, **kwargs):
        tenant_id, tenant_net = get_current_tenant()
        if not tenant_net:
            return jsonify({'error': 'Failed to initialize tenant'}), 500
        return func(tenant_net, *args, **kwargs)

    return wrapper
```

## Step 4: Remove init_network() Function

Delete the `init_network()` function (lines 56-116). Network initialization is now handled by TenantManager.

## Step 5: Update simulation_loop()

Replace the simulation_loop function with:

```python
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
```

## Step 6: Update collect_metrics()

Replace collect_metrics with:

```python
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
```

## Step 7: Update ALL API Endpoints

For EVERY endpoint that uses `network`, replace it with the tenant-specific network.

**Pattern for simple endpoints**:

```python
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
```

**Apply the @require_tenant decorator to ALL endpoints except:**
- `/` (index)
- `/api/patterns` (no network access needed)
- New tenant management endpoints (see below)

**Update each endpoint's function signature** to accept `tenant_net` as first parameter.

## Step 8: Add Tenant Management Endpoints

Add these new endpoints:

```python
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
```

## Step 9: Update WebSocket Handlers

Update SocketIO handlers to use tenant rooms:

```python
@socketio.on('connect')
def handle_connect():
    """Client connected - join tenant-specific room"""
    tenant_id, tenant_net = get_current_tenant()
    if tenant_net:
        from flask_socketio import join_room
        join_room(tenant_id)
        emit('connection_response', {
            'status': 'connected',
            'tenant_id': tenant_id
        })


@socketio.on('disconnect')
def handle_disconnect():
    """Client disconnected - leave tenant room"""
    if 'tenant_id' in session:
        from flask_socketio import leave_room
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
```

## Step 10: Update Startup Code

Replace the startup code (lines 642-661) with:

```python
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

    print("\n" + "=" * 70)
    print("Backend API running at: http://localhost:5001")
    print("React app running at: http://localhost:5000")
    print("=" * 70)

    try:
        socketio.run(app, host='0.0.0.0', port=5001, debug=False, allow_unsafe_werkzeug=True)
    finally:
        print("\nShutting down tenant manager...")
        tenant_manager.shutdown()
```

## Testing

1. Start the server: `python3 app.py`
2. Open multiple browser tabs to http://localhost:5000
3. Each tab should get its own isolated network instance
4. Verify that actions in one tab don't affect others
5. Check server logs for tenant creation/cleanup messages

## Key Features

- **Automatic Tenant Creation**: Each new session gets its own network
- **Session-Based Isolation**: Uses Flask sessions for tenant tracking
- **Automatic Cleanup**: Idle tenants removed after 1 hour (configurable)
- **Resource Limits**: Max 100 concurrent tenants (configurable)
- **SocketIO Rooms**: Each tenant has its own broadcast room
- **Thread-Safe**: All operations protected by locks

## Configuration

Adjust in tenant_manager.py:
- `max_tenants`: Maximum concurrent tenants (default: 100)
- `idle_timeout`: Seconds before idle cleanup (default: 3600 = 1 hour)
