"""
Tenant Manager - Multitenant Support for NIMCP Web Demo

WHAT: Manages multiple isolated neural network instances for different tenants/sessions
WHY: Enables multiple users to use the web demo simultaneously without interference
HOW: Session-based tenant identification with automatic cleanup of inactive tenants

DESIGN PATTERNS:
- Singleton: Single TenantManager instance
- Factory: Creates network instances on demand
- Resource Pool: Manages multiple network instances with lifecycle

COMPLEXITY: O(1) for tenant operations with dictionary lookup
"""

import threading
import time
import uuid
from typing import Dict, Optional, Tuple
import nimcp


class TenantNetwork:
    """
    Represents a single tenant's neural network instance

    WHAT: Container for tenant-specific network state
    WHY: Encapsulates all tenant data for isolation
    HOW: Stores network, connections, config, and metadata
    """

    def __init__(self, tenant_id: str, network: nimcp.NeuralNetwork):
        self.tenant_id = tenant_id
        self.network = network
        self.connections = []  # Track connections for visualization
        self.config = {
            'num_neurons': 63,
            'input_neurons': list(range(0, 9)),
            'hidden_neurons': list(range(9, 59)),
            'output_neurons': list(range(59, 63)),
            'simulation_running': False,
            'current_time': 0,
            'current_pattern': None,
            'output_activations': [0.0, 0.0, 0.0, 0.0]
        }
        self.metrics_history = {
            'activity': [],
            'weights': [],
            'timestamps': []
        }
        self.simulation_thread = None
        self.last_accessed = time.time()
        self.created_at = time.time()


class TenantManager:
    """
    Manages multiple tenant network instances

    WHAT: Central registry for all tenant networks
    WHY: Provides isolation, lifecycle management, and cleanup
    HOW: Dictionary-based storage with automatic cleanup thread

    THREAD SAFETY: All operations protected by threading.Lock
    """

    def __init__(self, max_tenants: int = 100, idle_timeout: int = 3600):
        """
        Initialize tenant manager

        Args:
            max_tenants: Maximum number of concurrent tenants
            idle_timeout: Seconds of inactivity before tenant cleanup (default 1 hour)
        """
        self.tenants: Dict[str, TenantNetwork] = {}
        self.max_tenants = max_tenants
        self.idle_timeout = idle_timeout
        self.lock = threading.Lock()
        self.cleanup_thread = None
        self.running = True

        # Start cleanup thread
        self._start_cleanup_thread()

    def _start_cleanup_thread(self):
        """Start background thread for cleaning up idle tenants"""
        def cleanup_loop():
            while self.running:
                time.sleep(60)  # Check every minute
                self._cleanup_idle_tenants()

        self.cleanup_thread = threading.Thread(target=cleanup_loop, daemon=True)
        self.cleanup_thread.start()

    def _cleanup_idle_tenants(self):
        """Remove tenants that have been idle for too long"""
        with self.lock:
            current_time = time.time()
            idle_tenants = []

            for tenant_id, tenant_net in self.tenants.items():
                if current_time - tenant_net.last_accessed > self.idle_timeout:
                    idle_tenants.append(tenant_id)

            for tenant_id in idle_tenants:
                print(f"[TenantManager] Cleaning up idle tenant: {tenant_id}")
                self._destroy_tenant_unsafe(tenant_id)

    def create_tenant(self, tenant_id: Optional[str] = None) -> Tuple[str, TenantNetwork]:
        """
        Create a new tenant with isolated neural network

        Args:
            tenant_id: Optional tenant ID (auto-generated if None)

        Returns:
            Tuple of (tenant_id, TenantNetwork instance)

        Raises:
            RuntimeError: If max_tenants limit reached
        """
        with self.lock:
            if len(self.tenants) >= self.max_tenants:
                raise RuntimeError(f"Maximum tenant limit reached ({self.max_tenants})")

            # Generate tenant ID if not provided
            if tenant_id is None:
                tenant_id = str(uuid.uuid4())

            # Check if tenant already exists
            if tenant_id in self.tenants:
                return tenant_id, self.tenants[tenant_id]

            # Create neural network instance (directly with neuron count)
            network = nimcp.NeuralNetwork(63)

            # Initialize network with connections
            self._initialize_network(network)

            # Create tenant wrapper
            tenant_net = TenantNetwork(tenant_id, network)
            self.tenants[tenant_id] = tenant_net

            print(f"[TenantManager] Created tenant: {tenant_id} (total: {len(self.tenants)})")
            return tenant_id, tenant_net

    def get_tenant(self, tenant_id: str) -> Optional[TenantNetwork]:
        """
        Get tenant network by ID

        Args:
            tenant_id: Tenant identifier

        Returns:
            TenantNetwork instance or None if not found
        """
        with self.lock:
            tenant_net = self.tenants.get(tenant_id)
            if tenant_net:
                tenant_net.last_accessed = time.time()
            return tenant_net

    def destroy_tenant(self, tenant_id: str) -> bool:
        """
        Explicitly destroy a tenant and cleanup resources

        Args:
            tenant_id: Tenant identifier

        Returns:
            True if tenant was destroyed, False if not found
        """
        with self.lock:
            return self._destroy_tenant_unsafe(tenant_id)

    def _destroy_tenant_unsafe(self, tenant_id: str) -> bool:
        """Internal destroy without locking (caller must hold lock)"""
        if tenant_id not in self.tenants:
            return False

        tenant_net = self.tenants[tenant_id]

        # Stop simulation thread if running
        if tenant_net.simulation_thread and tenant_net.simulation_thread.is_alive():
            tenant_net.config['simulation_running'] = False
            tenant_net.simulation_thread.join(timeout=2.0)

        # Destroy network (Python binding should handle C cleanup)
        del tenant_net.network

        # Remove from registry
        del self.tenants[tenant_id]

        print(f"[TenantManager] Destroyed tenant: {tenant_id} (remaining: {len(self.tenants)})")
        return True

    def get_tenant_count(self) -> int:
        """Get current number of active tenants"""
        with self.lock:
            return len(self.tenants)

    def list_tenants(self) -> list:
        """Get list of all tenant IDs"""
        with self.lock:
            return list(self.tenants.keys())

    def shutdown(self):
        """Shutdown manager and cleanup all tenants"""
        self.running = False
        if self.cleanup_thread:
            self.cleanup_thread.join(timeout=5.0)

        with self.lock:
            tenant_ids = list(self.tenants.keys())
            for tenant_id in tenant_ids:
                self._destroy_tenant_unsafe(tenant_id)

    @staticmethod
    def _initialize_network(network: nimcp.NeuralNetwork):
        """
        Initialize network with standard connections

        WHAT: Set up input→hidden and hidden→output connections
        WHY: Provide a functional network for pattern recognition
        HOW: Random connections with moderate weights
        """
        import random

        # Input layer (0-8) → Hidden layer (9-58)
        for input_id in range(9):
            # Each input neuron connects to 10 random hidden neurons
            hidden_targets = random.sample(range(9, 59), 10)
            for hidden_id in hidden_targets:
                weight = random.uniform(0.3, 0.7)
                network.add_connection(input_id, hidden_id, weight)

        # Hidden layer (9-58) → Output layer (59-62)
        for hidden_id in range(9, 59):
            # Each hidden neuron connects to 2 output neurons
            output_targets = random.sample(range(59, 63), 2)
            for output_id in output_targets:
                weight = random.uniform(0.4, 0.8)
                network.add_connection(hidden_id, output_id, weight)
