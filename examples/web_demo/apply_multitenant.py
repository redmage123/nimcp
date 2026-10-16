#!/usr/bin/env python3
"""
Script to apply multitenant changes to app.py

This script reads app.py.backup and generates a new app.py with multitenant support
"""

import re

# Read the backup
with open('app.py.backup', 'r') as f:
    content = f.read()

# Step 1: Update imports
imports_old = """from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
from flask_cors import CORS
import sys
import os
import time
import json
import threading

# Add nimcp to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../build/lib/python'))
import nimcp"""

imports_new = """from flask import Flask, render_template, jsonify, request, session
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
from tenant_manager import TenantManager"""

content = content.replace(imports_old, imports_new)

# Step 2: Replace global state
global_state_old = """# Global network state
network = None
network_connections = []  # Track all connections for visualization
network_config = {
    'num_neurons': 63,  # 9 input + 50 hidden + 4 output
    'input_neurons': list(range(0, 9)),  # Neurons 0-8 (3x3 grid)
    'hidden_neurons': list(range(9, 59)),  # Neurons 9-58
    'output_neurons': list(range(59, 63)),  # Neurons 59-62 (4 patterns)
    'simulation_running': False,
    'current_time': 0,
    'current_pattern': None,
    'output_activations': [0.0, 0.0, 0.0, 0.0]
}

# Pattern definitions
PATTERNS = {
    'vertical': [0, 1, 0, 0, 1, 0, 0, 1, 0],
    'horizontal': [0, 0, 0, 1, 1, 1, 0, 0, 0],
    'diagonal_down': [1, 0, 0, 0, 1, 0, 0, 0, 1],  # \\
    'diagonal_up': [0, 0, 1, 0, 1, 0, 1, 0, 0]     # /
}

PATTERN_NAMES = ['vertical', 'horizontal', 'diagonal_down', 'diagonal_up']

metrics_history = {
    'activity': [],
    'weights': [],
    'spikes': [],
    'timestamps': []
}"""

global_state_new = """# Initialize tenant manager (multitenant support)
tenant_manager = TenantManager(max_tenants=100, idle_timeout=3600)

# Pattern definitions (shared across all tenants)
PATTERNS = {
    'vertical': [0, 1, 0, 0, 1, 0, 0, 1, 0],
    'horizontal': [0, 0, 0, 1, 1, 1, 0, 0, 0],
    'diagonal_down': [1, 0, 0, 0, 1, 0, 0, 0, 1],  # \\
    'diagonal_up': [0, 0, 1, 0, 1, 0, 1, 0, 0]     # /
}

PATTERN_NAMES = ['vertical', 'horizontal', 'diagonal_down', 'diagonal_up']


def get_current_tenant():
    \"\"\"Get or create tenant for current session\"\"\"
    if 'tenant_id' not in session:
        session['tenant_id'] = str(uuid.uuid4())
        session.modified = True

    tenant_id = session['tenant_id']
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
    \"\"\"Decorator to ensure tenant exists before endpoint execution\"\"\"
    @wraps(func)
    def wrapper(*args, **kwargs):
        tenant_id, tenant_net = get_current_tenant()
        if not tenant_net:
            return jsonify({'error': 'Failed to initialize tenant'}), 500
        return func(tenant_net, *args, **kwargs)
    return wrapper"""

content = content.replace(global_state_old, global_state_new)

# Step 3: Remove init_network function (it's now in TenantManager)
# Find and remove the init_network function
init_network_pattern = r"# Initialize network\ndef init_network\(\):.*?(?=\n# Simulation loop|def simulation_loop)"
content = re.sub(init_network_pattern, "", content, flags=re.DOTALL)

# Write the transformed content
with open('app_multitenant.py', 'w') as f:
    f.write(content)

print("✓ Created app_multitenant.py")
print("  Review the file, then: mv app_multitenant.py app.py")
