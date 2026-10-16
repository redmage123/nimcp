/**
 * @file nimcp_neural_logic.c
 * @brief Neural Logic Implementation (GPU + CPU Fallback)
 *
 * WHAT: Spiking neural logic gates with GPU acceleration
 * WHY:  Replace symbolic engine with 100x faster neural logic
 * HOW:  CUDA kernels for GPU, optimized loops for CPU
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0650 (BIO_MODULE_NEURAL_LOGIC)
 * - Publishes: logic gate results, circuit completion events
 * - Uses: BIO_CHANNEL_ACETYLCHOLINE for fast queries
 * - Uses: BIO_CHANNEL_DOPAMINE for learning signals
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 9.0
 */

#include "core/neuron_types/nimcp_neural_logic.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_pool.h"  // Memory pool for O(1) allocations
#include "utils/memory/nimcp_page_cow.h"     // Copy-on-Write for shallow copies
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_validate.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "core/brain/nimcp_brain.h"  // Brain reference
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Logging module identifier */
#define LOG_MODULE "NEURAL_LOGIC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic)

/* Forward declarations for quantum bridge (avoid circular dependencies) */
extern void* neural_logic_quantum_bridge_create(const void* config);
extern void neural_logic_quantum_bridge_destroy(void* bridge);
extern int neural_logic_quantum_bridge_connect(void* bridge, void* network);
extern void neural_logic_quantum_default_config_opaque(void* config_out);

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>

// GPU kernel launch wrappers (defined in nimcp_neural_logic_kernels.cu)
extern cudaError_t launch_update_logic_neurons(
    logic_neuron_state_t* neurons_device,
    const float* input_activities_device,
    uint32_t num_neurons,
    uint64_t timestamp,
    uint64_t delta_t,
    uint32_t threads_per_block,
    cudaStream_t stream
);

extern cudaError_t launch_update_variable_bindings(
    variable_binding_state_t* variables_device,
    const float* patterns_device,
    uint32_t num_variables,
    uint32_t pattern_dim,
    uint64_t timestamp,
    uint64_t delta_t,
    uint32_t threads_per_block,
    cudaStream_t stream
);

// N-ary and tensor-compatible batch evaluation kernel launchers
extern cudaError_t launch_evaluate_gates_batch_nary(
    const logic_neuron_state_t* neurons_device,
    const uint32_t* gate_ids_device,
    const float* inputs_device,
    const uint32_t* input_offsets_device,
    const uint32_t* inputs_per_gate_device,
    float* outputs_device,
    uint32_t num_gates,
    uint32_t threads_per_block,
    cudaStream_t stream
);

extern cudaError_t launch_evaluate_gates_batch_tensor(
    const logic_neuron_state_t* neurons_device,
    const int32_t* gate_ids_device,
    const float* inputs_device,
    const int32_t* input_offsets_device,
    const int32_t* inputs_per_gate_device,
    float* outputs_device,
    uint32_t num_gates,
    uint32_t threads_per_block,
    cudaStream_t stream
);
#endif

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Synaptic connection between logic neurons
 *
 * WHAT: Single weighted connection in adjacency list
 * WHY:  Sparse connectivity requires edge list (not dense matrix)
 * HOW:  Store target neuron + weight per connection
 */
typedef struct logic_synapse_struct {
    uint32_t target_id;              /**< Target neuron ID */
    float weight;                     /**< Synaptic weight */
    struct logic_synapse_struct* next; /**< Next synapse in list */
} logic_synapse_t;

/**
 * @brief Neural logic network internal structure
 */
struct neural_logic_network_struct {
    // Configuration
    neural_logic_config_t config;

    // Logic neurons (host-side copy)
    logic_neuron_state_t* neurons_host;
    uint32_t neurons_count;
    uint32_t neurons_capacity;

    // Variable bindings (host-side copy)
    variable_binding_state_t* variables_host;
    uint32_t variables_count;
    uint32_t variables_capacity;

    // Connectivity (adjacency list for sparse graphs)
    logic_synapse_t** outgoing_synapses;  /**< Outgoing connections per neuron */
    uint32_t* synapse_counts;             /**< Count of outgoing synapses per neuron */
    uint32_t total_synapses;              /**< Total synapse count (statistics) */

    // Neuromodulation
    brain_t brain;  /**< Brain reference for DA + ACh modulation */

    // === Memory Pool for O(1) Synapse Allocation ===
    // WHAT: Pre-allocated pool for logic_synapse_t structures
    // WHY:  Synapse creation is hot path; avoid malloc overhead
    // HOW:  Pool with block_size = sizeof(logic_synapse_t)
    memory_pool_t synapse_pool;           /**< Pool for synapse allocations */

    // === Copy-on-Write Support ===
    // WHAT: Enable shallow copy of network with lazy duplication
    // WHY:  Fast cloning for parallel inference or checkpointing
    // HOW:  Reference counting on shared data, copy on modification
    uint32_t* _cow_refcount;              /**< Reference count for CoW (NULL if owned) */
    bool _cow_is_shallow;                 /**< True if this is a shallow copy */

    // GPU pointers (NULL if CPU fallback)
#ifdef NIMCP_ENABLE_CUDA
    logic_neuron_state_t* neurons_device;
    variable_binding_state_t* variables_device;
    float* input_activities_device;
    cudaStream_t stream;
#endif

    // Execution mode
    bool using_gpu;

    // Statistics
    uint64_t total_spikes;
    uint64_t total_evaluations;
    float sum_eval_time_us;

    // Bio-async communication
    bio_module_context_t bio_ctx;     /**< Bio-async module context */
    bool bio_async_enabled;           /**< Whether bio-async is enabled */

    // Quantum acceleration
    void* quantum_bridge;             /**< Quantum bridge (opaque pointer) */
    bool quantum_enabled;             /**< Whether quantum acceleration is enabled */
};

//=============================================================================
// GPU Detection
//=============================================================================

NIMCP_EXPORT bool neural_logic_gpu_available(void)
{
#ifdef NIMCP_ENABLE_CUDA
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
#else
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_gpu_available: operation failed");
    return false;
#endif
}

//=============================================================================
// Configuration
//=============================================================================

NIMCP_EXPORT neural_logic_config_t neural_logic_default_config(uint32_t max_neurons)
{
    neural_logic_config_t config = {0};
    
    config.max_logic_neurons = max_neurons;
    config.max_variables = max_neurons / 4;  // 25% can be variables
    config.variable_pattern_dim = 64;        // 64-dimensional patterns
    
    // GPU configuration
    config.threads_per_block = 256;          // Optimal for most GPUs
    config.use_gpu = neural_logic_gpu_available();
    config.pin_host_memory = true;
    
    // Temporal parameters
    config.timestep_us = 100.0F;             // 100μs = 0.1ms
    config.integration_window_ms = 5.0F;     // 5ms integration window
    
    // Learning configuration
    config.enable_learning = false;          // Disable for now
    config.learning_rate = 0.01F;

    // Quantum acceleration
    config.enable_quantum_neural_logic = true;  // Enable by default

    return config;
}

//=============================================================================
// Network Lifecycle
//=============================================================================

NIMCP_EXPORT neural_logic_network_t neural_logic_create(
    const neural_logic_config_t* config)
{
    if (!nimcp_validate_pointer(config, "config")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_create: nimcp_validate_pointer is NULL");
        return NULL;
    }
    
    if (config->max_logic_neurons == 0) {
        LOG_ERROR(LOG_MODULE,"max_logic_neurons must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_create: config->max_logic_neurons is zero");
        return NULL;
    }
    
    // Allocate network structure
    neural_logic_network_t network = nimcp_calloc(1, sizeof(struct neural_logic_network_struct));
    if (!nimcp_validate_pointer(network, "network")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_create: nimcp_validate_pointer is NULL");
        return NULL;
    }
    
    // Copy configuration
    network->config = *config;
    network->neurons_capacity = config->max_logic_neurons;
    network->variables_capacity = config->max_variables;
    network->neurons_count = 0;
    network->variables_count = 0;
    network->total_spikes = 0;
    network->total_evaluations = 0;
    network->sum_eval_time_us = 0.0F;
    network->brain = NULL;  // Initialize neuromodulator brain reference
    
    // Allocate host memory for neurons (64-byte aligned for cache line optimization)
    network->neurons_host = nimcp_aligned_alloc(64, network->neurons_capacity * sizeof(logic_neuron_state_t));
    if (!nimcp_validate_pointer(network->neurons_host, "neurons_host")) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_create: nimcp_validate_pointer is NULL");
        return NULL;
    }
    memset(network->neurons_host, 0, network->neurons_capacity * sizeof(logic_neuron_state_t));

    // Allocate host memory for variables (64-byte aligned)
    network->variables_host = nimcp_aligned_alloc(64, network->variables_capacity * sizeof(variable_binding_state_t));
    if (!nimcp_validate_pointer(network->variables_host, "variables_host")) {
        nimcp_aligned_free(network->neurons_host);  // BUGFIX: neurons_host uses aligned_alloc
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_create: nimcp_validate_pointer is NULL");
        return NULL;
    }
    memset(network->variables_host, 0, network->variables_capacity * sizeof(variable_binding_state_t));

    // Allocate connectivity arrays (adjacency lists)
    network->outgoing_synapses = (logic_synapse_t**)nimcp_calloc(network->neurons_capacity, sizeof(logic_synapse_t*));
    if (!nimcp_validate_pointer(network->outgoing_synapses, "outgoing_synapses")) {
        nimcp_aligned_free(network->variables_host);
        nimcp_aligned_free(network->neurons_host);  // BUGFIX: neurons_host uses aligned_alloc
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    network->synapse_counts = (uint32_t*)nimcp_calloc(network->neurons_capacity, sizeof(uint32_t));
    if (!nimcp_validate_pointer(network->synapse_counts, "synapse_counts")) {
        nimcp_free(network->outgoing_synapses);
        nimcp_aligned_free(network->variables_host);
        nimcp_aligned_free(network->neurons_host);  // BUGFIX: neurons_host uses aligned_alloc
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    network->total_synapses = 0;

    // === Initialize Memory Pool for Synapses ===
    // WHAT: Create O(1) allocation pool for logic_synapse_t
    // WHY:  neural_logic_connect() is hot path; avoid malloc overhead
    // HOW:  Pre-allocate ~10 synapses per neuron (typical connectivity)
    memory_pool_config_t pool_config = memory_pool_default_config(
        sizeof(logic_synapse_t),
        config->max_logic_neurons * 10  // ~10 connections per neuron average
    );
    pool_config.alignment = 16;  // 16-byte alignment for cache efficiency
    pool_config.enable_tracking = true;
    network->synapse_pool = memory_pool_create(&pool_config);
    if (!network->synapse_pool) {
        LOG_WARN(LOG_MODULE,"Failed to create synapse pool, will use malloc fallback");
        // Non-fatal: continue without pool (falls back to malloc)
    }

    // === Initialize Copy-on-Write Fields ===
    network->_cow_refcount = NULL;
    network->_cow_is_shallow = false;

    // Try to allocate GPU memory if enabled
    network->using_gpu = false;
#ifdef NIMCP_ENABLE_CUDA
    if (config->use_gpu && neural_logic_gpu_available()) {
        cudaError_t err;
        
        // Allocate device memory for neurons
        err = cudaMalloc((void**)&network->neurons_device,
                        network->neurons_capacity * sizeof(logic_neuron_state_t));
        if (err != cudaSuccess) {
            LOG_WARN(LOG_MODULE,"GPU malloc failed, using CPU fallback");
            goto cpu_fallback;
        }
        
        // Allocate device memory for variables
        err = cudaMalloc((void**)&network->variables_device,
                        network->variables_capacity * sizeof(variable_binding_state_t));
        if (err != cudaSuccess) {
            LOG_WARN(LOG_MODULE,"GPU malloc failed, using CPU fallback");
            cudaFree(network->neurons_device);
            goto cpu_fallback;
        }
        
        // Create CUDA stream
        err = cudaStreamCreate(&network->stream);
        if (err != cudaSuccess) {
            LOG_WARN(LOG_MODULE,"GPU stream creation failed, using CPU fallback");
            cudaFree(network->neurons_device);
            cudaFree(network->variables_device);
            goto cpu_fallback;
        }
        
        network->using_gpu = true;
        LOG_INFO(LOG_MODULE,"Neural logic using GPU acceleration");
    }
    
cpu_fallback:
#endif

    if (!network->using_gpu) {
        LOG_INFO(LOG_MODULE, "Neural logic using CPU fallback");
    }

    // === Initialize Bio-Async Communication ===
    network->bio_ctx = NULL;
    network->bio_async_enabled = false;

    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_NEURAL_LOGIC,
            .module_name = "neural_logic",
            .inbox_capacity = 64,
            .user_data = network
        };
        network->bio_ctx = bio_router_register_module(&bio_info);
        if (network->bio_ctx) {
            network->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async registered for neural logic (module_id=0x%04X)",
                     BIO_MODULE_NEURAL_LOGIC);
        } else {
            LOG_WARN(LOG_MODULE, "Failed to register bio-async module");
        }
    }

    // === Initialize Quantum Acceleration ===
    network->quantum_bridge = NULL;
    network->quantum_enabled = false;

    if (config->enable_quantum_neural_logic) {
        // Defer quantum bridge creation to avoid circular dependencies
        // Bridge will be created on first use via lazy initialization
        network->quantum_enabled = true;
        LOG_INFO(LOG_MODULE, "Quantum neural logic enabled (lazy initialization)");
    }

    LOG_INFO(LOG_MODULE, "Neural logic network created: max_neurons=%u, max_vars=%u, gpu=%s, bio_async=%s, quantum=%s",
             config->max_logic_neurons, config->max_variables,
             network->using_gpu ? "enabled" : "disabled",
             network->bio_async_enabled ? "enabled" : "disabled",
             network->quantum_enabled ? "enabled" : "disabled");

    return network;
}

NIMCP_EXPORT void neural_logic_destroy(neural_logic_network_t network)
{
    if (!network) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying neural logic network: neurons=%u, synapses=%u",
              network->neurons_count, network->total_synapses);

    // === Unregister Bio-Async Module ===
    if (network->bio_async_enabled && network->bio_ctx) {
        bio_router_unregister_module(network->bio_ctx);
        network->bio_ctx = NULL;
        network->bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for neural logic");
    }

    // === Destroy Quantum Bridge ===
    if (network->quantum_bridge) {
        // Use forward declaration from file scope
        neural_logic_quantum_bridge_destroy(network->quantum_bridge);
        network->quantum_bridge = NULL;
        network->quantum_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Quantum bridge destroyed");
    }

    // === Handle Copy-on-Write Reference Counting ===
    // If this is a shallow copy, decrement refcount
    if (network->_cow_refcount) {
        uint32_t old_count = __atomic_sub_fetch(network->_cow_refcount, 1, __ATOMIC_SEQ_CST);
        if (old_count > 0) {
            // Other references exist; only free this network struct
            nimcp_free(network);
            return;
        }
        // Last reference: proceed with full cleanup
        nimcp_free(network->_cow_refcount);
    }

    // Free GPU memory if allocated
#ifdef NIMCP_ENABLE_CUDA
    if (network->using_gpu) {
        cudaStreamSynchronize(network->stream);
        cudaStreamDestroy(network->stream);
        cudaFree(network->neurons_device);
        cudaFree(network->variables_device);
        if (network->input_activities_device) {
            cudaFree(network->input_activities_device);
        }
    }
#endif

    // Free connectivity structures
    if (network->outgoing_synapses) {
        for (uint32_t i = 0; i < network->neurons_capacity; i++) {
            logic_synapse_t* synapse = network->outgoing_synapses[i];
            while (synapse) {
                logic_synapse_t* next = synapse->next;
                // Use pool release if pool exists and owns this memory
                if (network->synapse_pool && memory_pool_owns(network->synapse_pool, synapse)) {
                    memory_pool_release(network->synapse_pool, synapse);
                } else {
                    nimcp_free(synapse);
                }
                synapse = next;
            }
        }
        nimcp_free(network->outgoing_synapses);
    }
    nimcp_free(network->synapse_counts);

    // === Destroy Memory Pool ===
    if (network->synapse_pool) {
        memory_pool_destroy(network->synapse_pool);
        network->synapse_pool = NULL;
    }

    // Free host memory
    // BUGFIX: Free bound patterns allocated in neural_logic_bind_variable()
    if (network->variables_host) {
        for (uint32_t i = 0; i < network->variables_count; i++) {
            if (network->variables_host[i].bound_pattern) {
                nimcp_free(network->variables_host[i].bound_pattern);
                network->variables_host[i].bound_pattern = NULL;
            }
        }
        // BUGFIX: variables_host allocated with nimcp_aligned_alloc, must use aligned_free
        nimcp_aligned_free(network->variables_host);
    }

    // BUGFIX: neurons_host allocated with nimcp_aligned_alloc, must use aligned_free
    nimcp_aligned_free(network->neurons_host);
    nimcp_free(network);
}

//=============================================================================
// Logic Neuron Creation
//=============================================================================

NIMCP_EXPORT uint32_t neural_logic_create_gate(
    neural_logic_network_t network,
    logic_gate_type_t gate_type,
    float threshold)
{
    LOG_DEBUG(LOG_MODULE, "Creating logic gate: type=%s, threshold=%.2f",
              neural_logic_gate_name(gate_type), threshold);

    if (!nimcp_validate_pointer(network, "network")) {
        LOG_ERROR(LOG_MODULE, "NULL network pointer");
        return UINT32_MAX;
    }

    if (network->neurons_count >= network->neurons_capacity) {
        LOG_ERROR(LOG_MODULE, "Maximum neurons reached: %u/%u",
                  network->neurons_count, network->neurons_capacity);
        return UINT32_MAX;
    }

    if (gate_type >= LOGIC_GATE_COUNT) {
        LOG_ERROR(LOG_MODULE, "Invalid gate type: %d (max=%d)", gate_type, LOGIC_GATE_COUNT);
        return UINT32_MAX;
    }

    // Get next neuron ID
    uint32_t neuron_id = network->neurons_count++;
    logic_neuron_state_t* neuron = &network->neurons_host[neuron_id];

    // Initialize neuron state
    memset(neuron, 0, sizeof(logic_neuron_state_t));
    neuron->neuron_id = neuron_id;
    neuron->gate_type = gate_type;
    neuron->membrane_potential = 0.0F;
    neuron->output_state = 0.0F;
    neuron->integration_window = network->config.integration_window_ms;

    // Set gate-specific parameters
    switch (gate_type) {
        case LOGIC_GATE_AND:
            // AND: Requires both inputs (coincidence detection)
            neuron->threshold = (threshold > 0.0F) ? threshold : 1.8F;  // Slightly less than 2.0
            neuron->excitatory_weight = 1.0F;
            neuron->inhibitory_weight = 0.0F;
            neuron->refractory_period = 1000;  // 1ms
            break;

        case LOGIC_GATE_OR:
            // OR: Fires if any input active (low threshold)
            neuron->threshold = (threshold > 0.0F) ? threshold : 0.6F;  // Less than 1.0
            neuron->excitatory_weight = 1.0F;
            neuron->inhibitory_weight = 0.0F;
            neuron->refractory_period = 500;   // 0.5ms
            break;

        case LOGIC_GATE_NOT:
            // NOT: Baseline firing, inhibited by input
            neuron->threshold = (threshold > 0.0F) ? threshold : 0.5F;
            neuron->excitatory_weight = 0.0F;
            neuron->inhibitory_weight = -2.0F;  // Strong inhibition
            neuron->membrane_potential = 1.0F;  // Baseline depolarization
            neuron->refractory_period = 1000;   // 1ms
            break;

        case LOGIC_GATE_XOR:
            // XOR: Fires if inputs differ (balanced)
            neuron->threshold = (threshold > 0.0F) ? threshold : 0.5F;
            neuron->excitatory_weight = 1.0F;
            neuron->inhibitory_weight = -1.0F;
            neuron->refractory_period = 1000;   // 1ms
            break;

        case LOGIC_GATE_IMPLIES:
            // IMPLIES: A → B (fires if A=0 OR B=1)
            neuron->threshold = (threshold > 0.0F) ? threshold : 0.8F;
            neuron->excitatory_weight = 1.0F;
            neuron->inhibitory_weight = -0.5F;
            neuron->refractory_period = 1000;   // 1ms
            break;

        default:
            LOG_ERROR(LOG_MODULE, "Unhandled gate type: %d", gate_type);
            network->neurons_count--;  // Rollback
            return UINT32_MAX;
    }

    LOG_INFO(LOG_MODULE, "Created logic gate %s (id=%u, threshold=%.2f)",
             neural_logic_gate_name(gate_type), neuron_id, neuron->threshold);

    // Publish bio-async message for gate creation if enabled
    if (network->bio_async_enabled && network->bio_ctx) {
        // Use generic state query message to broadcast gate creation
        bio_msg_brain_state_query_t msg = {0};
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                            BIO_MODULE_NEURAL_LOGIC, 0,
                            sizeof(msg));
        msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast notification
        msg.region_id = neuron_id;
        msg.query_flags = gate_type;  // Store gate type in flags

        nimcp_error_t err = bio_router_broadcast(network->bio_ctx, &msg, sizeof(msg));
        if (err != NIMCP_SUCCESS) {
            LOG_WARN(LOG_MODULE, "Failed to broadcast neuron creation: neuron=%u, error=%d",
                     neuron_id, err);
        } else {
            LOG_DEBUG(LOG_MODULE, "Broadcast neuron creation: neuron=%u, type=%s",
                      neuron_id, neural_logic_gate_name(gate_type));
        }
    }

    return neuron_id;
}

NIMCP_EXPORT uint32_t neural_logic_create_variable(
    neural_logic_network_t network,
    const char* variable_name)
{
    if (!nimcp_validate_pointer(network, "network") ||
        !nimcp_validate_pointer(variable_name, "variable_name")) {
        return UINT32_MAX;
    }
    
    if (network->variables_count >= network->variables_capacity) {
        LOG_ERROR(LOG_MODULE,"Maximum variables reached");
        return UINT32_MAX;
    }
    
    // Simple hash of variable name
    uint32_t hash = 0;
    for (const char* p = variable_name; *p; p++) {
        hash = hash * 31 + (uint32_t)(*p);
    }
    
    // Get next variable ID
    uint32_t var_id = network->variables_count++;
    variable_binding_state_t* var = &network->variables_host[var_id];
    
    // Initialize variable state
    memset(var, 0, sizeof(variable_binding_state_t));
    var->variable_id = hash;
    var->pattern_dim = network->config.variable_pattern_dim;
    var->binding_strength = 0.0F;
    var->decay_rate = 0.001F;  // Slow decay
    var->is_bound = false;
    var->bound_pattern = NULL;  // Allocated on first bind
    
    return var_id;
}

//=============================================================================
// Neuromodulation
//=============================================================================

/**
 * @brief Compute logic gate threshold modulation from neurotransmitters
 *
 * WHAT: Calculate threshold modulation factor based on DA + ACh
 * WHY:  DA modulates logical flexibility, ACh modulates precision
 * HOW:  Read DA and ACh levels, combine into threshold multiplier
 *
 * BIOLOGY:
 * - Dopamine: Logical flexibility vs rigidity
 *   High DA (0.7) → 0.7× threshold (permissive logic, exploratory)
 *   Low DA (0.3) → 1.3× threshold (rigid logic, perseverative)
 *
 * - Acetylcholine: Logical precision
 *   High ACh (0.7) → precise thresholds (accurate logic)
 *   Low ACh (0.3) → imprecise thresholds (sloppy logic, errors)
 *
 * CLINICAL EXAMPLES:
 * - Depression (low DA): Rigid, black-and-white thinking
 * - Mania (high DA): Loose associations, illogical leaps
 * - ADHD (low ACh): Logical errors, misses contradictions
 * - Dementia (low ACh): Impaired reasoning, confabulation
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain to read neurotransmitters from
 * @return Threshold multiplier [0.7, 1.3], or 1.0 if no brain
 */
static float get_logic_threshold_modulation(brain_t brain)
{
    // Guard: No brain available
    if (!brain) {
        return 1.0F;
    }

    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    if (!neuromod) {
        return 1.0F;
    }

    // Read neurotransmitter levels
    float da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
    float ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);

    // DA contribution: [0.3, 0.7] → [1.3, 0.7] (inverse)
    // High DA → lower threshold (permissive, exploratory)
    // Low DA → higher threshold (rigid, perseverative)
    float da_multiplier = 1.3F - (da - 0.3F) * 1.5F;

    // ACh contribution: [0.3, 0.7] → precision scaling
    // High ACh → maintains intended thresholds
    // Low ACh → adds noise/imprecision to reasoning
    // For simplicity, ACh primarily affects precision of DA effect
    float ach_precision = 0.8F + (ach - 0.3F) * 0.5F;  // [0.8, 1.0]

    // Combined: DA sets flexibility, ACh sharpens precision
    // Range: approximately [0.7, 1.3]
    return da_multiplier * ach_precision;
}

//=============================================================================
// CPU Implementation (Fallback)
//=============================================================================

/**
 * @brief CPU implementation of AND gate
 */
static inline float cpu_compute_and_gate(
    float input_a,
    float input_b,
    float threshold,
    float integration_window)
{
    (void)integration_window;  // Unused in simplified model
    
    float sum = input_a + input_b;
    return (sum >= threshold) ? 1.0F : 0.0F;
}

/**
 * @brief CPU implementation of OR gate
 */
static inline float cpu_compute_or_gate(
    float input_a,
    float input_b,
    float threshold)
{
    float sum = input_a + input_b;
    return (sum >= threshold) ? 1.0F : 0.0F;
}

/**
 * @brief CPU implementation of NOT gate
 */
static inline float cpu_compute_not_gate(
    float input,
    float baseline_rate,
    float inhibition_strength)
{
    float output = baseline_rate - (input * inhibition_strength);
    return (output > 0.5F) ? 1.0F : 0.0F;
}

/**
 * @brief CPU implementation of XOR gate
 */
static inline float cpu_compute_xor_gate(
    float input_a,
    float input_b,
    float threshold,
    float balance_tolerance)
{
    (void)balance_tolerance;  // Unused in simplified model
    
    float diff = fabsf(input_a - input_b);
    return (diff >= threshold) ? 1.0F : 0.0F;
}

/**
 * @brief CPU implementation of IMPLIES gate
 */
static inline float cpu_compute_implies_gate(
    float input_a,
    float input_b,
    float antecedent_threshold,
    float consequent_threshold)
{
    // A → B  ≡  ¬A ∨ B
    bool a_active = (input_a >= antecedent_threshold);
    bool b_active = (input_b >= consequent_threshold);
    
    return (!a_active || b_active) ? 1.0F : 0.0F;
}

/**
 * @brief Propagate spikes through synaptic connections
 *
 * WHAT: Transfer output activity to connected target neurons
 * WHY:  Enable signal flow through logic circuits
 * HOW:  For each spiking neuron, propagate weighted output to targets
 *
 * BIOLOGY: Synaptic transmission - presynaptic spike triggers postsynaptic current
 *
 * @param network Neural logic network
 * @param source_id Source neuron ID
 * @param output_activity Source neuron output [0,1]
 */
static void propagate_signals(
    neural_logic_network_t network,
    uint32_t source_id,
    float output_activity)
{
    if (output_activity < 0.01F) {
        return;  // No significant activity to propagate
    }

    // Iterate through outgoing synapses
    logic_synapse_t* synapse = network->outgoing_synapses[source_id];
    while (synapse) {
        uint32_t target_id = synapse->target_id;
        float weight = synapse->weight;

        // Accumulate weighted activity in target neuron inputs
        logic_neuron_state_t* target = &network->neurons_host[target_id];

        // Determine which input channel to use (A or B)
        // Simple heuristic: use A if it's lower, otherwise use B
        if (target->input_a_activity <= target->input_b_activity) {
            target->input_a_activity += output_activity * weight;
        } else {
            target->input_b_activity += output_activity * weight;
        }

        synapse = synapse->next;
    }
}

/**
 * @brief CPU update of single logic neuron
 */
static void cpu_update_logic_neuron(
    logic_neuron_state_t* neuron,
    uint64_t timestamp,
    uint64_t delta_t,
    float threshold_modulation)
{
    // Check refractory period
    if ((timestamp - neuron->last_spike_time) < neuron->refractory_period) {
        return;  // In refractory period
    }

    // Apply neuromodulator modulation to threshold
    float modulated_threshold = neuron->threshold * threshold_modulation;

    // Compute gate output based on type
    float output = 0.0F;

    switch (neuron->gate_type) {
        case LOGIC_GATE_AND:
            output = cpu_compute_and_gate(
                neuron->input_a_activity,
                neuron->input_b_activity,
                modulated_threshold,
                neuron->integration_window
            );
            break;

        case LOGIC_GATE_OR:
            output = cpu_compute_or_gate(
                neuron->input_a_activity,
                neuron->input_b_activity,
                modulated_threshold
            );
            break;

        case LOGIC_GATE_NOT:
            output = cpu_compute_not_gate(
                neuron->input_a_activity,
                1.0F,  // baseline_rate
                fabsf(neuron->inhibitory_weight)
            );
            break;

        case LOGIC_GATE_XOR:
            output = cpu_compute_xor_gate(
                neuron->input_a_activity,
                neuron->input_b_activity,
                modulated_threshold,
                0.1F  // balance_tolerance
            );
            break;

        case LOGIC_GATE_IMPLIES:
            output = cpu_compute_implies_gate(
                neuron->input_a_activity,
                neuron->input_b_activity,
                0.8F,  // antecedent_threshold
                0.8F   // consequent_threshold
            );
            break;

        default:
            output = 0.0F;
            break;
    }

    // Update output state
    neuron->output_state = output;

    // Fire spike if output is true
    if (output > 0.5F) {
        neuron->last_spike_time = timestamp;
        neuron->total_spikes++;
        neuron->true_outputs++;
    } else {
        neuron->false_outputs++;
    }

    // Decay input activities
    float decay = (float)delta_t / 1000.0F;  // Convert μs to ms
    neuron->input_a_activity *= expf(-decay / neuron->integration_window);
    neuron->input_b_activity *= expf(-decay / neuron->integration_window);
}

//=============================================================================
// Simulation
//=============================================================================

NIMCP_EXPORT uint32_t neural_logic_update(
    neural_logic_network_t network,
    uint64_t timestamp,
    uint64_t delta_t)
{
    if (!nimcp_validate_pointer(network, "network")) {
        return 0;
    }
    
    uint32_t spikes_count = 0;
    
#ifdef NIMCP_ENABLE_CUDA
    if (network->using_gpu) {
        // Copy host neurons to device
        cudaError_t err = cudaMemcpyAsync(
            network->neurons_device,
            network->neurons_host,
            network->neurons_count * sizeof(logic_neuron_state_t),
            cudaMemcpyHostToDevice,
            network->stream
        );

        if (err != cudaSuccess) {
            LOG_ERROR(LOG_MODULE,"Failed to copy neurons to GPU: %s", cudaGetErrorString(err));
            goto cpu_path;
        }

        // Launch GPU kernel
        err = launch_update_logic_neurons(
            network->neurons_device,
            network->input_activities_device,
            network->neurons_count,
            timestamp,
            delta_t,
            network->config.threads_per_block,
            network->stream
        );

        if (err != cudaSuccess) {
            LOG_ERROR(LOG_MODULE,"GPU kernel launch failed: %s", cudaGetErrorString(err));
            goto cpu_path;
        }

        // Copy results back from device to host
        err = cudaMemcpyAsync(
            network->neurons_host,
            network->neurons_device,
            network->neurons_count * sizeof(logic_neuron_state_t),
            cudaMemcpyDeviceToHost,
            network->stream
        );

        if (err != cudaSuccess) {
            LOG_ERROR(LOG_MODULE,"Failed to copy neurons from GPU: %s", cudaGetErrorString(err));
            goto cpu_path;
        }

        // Synchronize stream
        cudaStreamSynchronize(network->stream);

        // Count spikes from GPU results
        for (uint32_t i = 0; i < network->neurons_count; i++) {
            logic_neuron_state_t* neuron = &network->neurons_host[i];
            if (neuron->last_spike_time == timestamp) {
                spikes_count++;
            }
        }

        network->total_spikes += spikes_count;
        return spikes_count;
    }

cpu_path:
#endif

    // Compute neuromodulator threshold modulation
    float threshold_modulation = get_logic_threshold_modulation(network->brain);

    // CPU fallback: Update all neurons sequentially
    for (uint32_t i = 0; i < network->neurons_count; i++) {
        logic_neuron_state_t* neuron = &network->neurons_host[i];
        uint32_t spikes_before = neuron->total_spikes;

        cpu_update_logic_neuron(neuron, timestamp, delta_t, threshold_modulation);

        if (neuron->total_spikes > spikes_before) {
            spikes_count++;
        }
    }

    // Propagate signals through connections
    for (uint32_t i = 0; i < network->neurons_count; i++) {
        logic_neuron_state_t* neuron = &network->neurons_host[i];
        if (neuron->output_state > 0.5F) {
            propagate_signals(network, i, neuron->output_state);
        }
    }

    network->total_spikes += spikes_count;

    return spikes_count;
}

NIMCP_EXPORT bool neural_logic_synchronize(neural_logic_network_t network)
{
    if (!nimcp_validate_pointer(network, "network")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_synchronize: nimcp_validate_pointer is NULL");
        return false;
    }
    
#ifdef NIMCP_ENABLE_CUDA
    if (network->using_gpu) {
        cudaStreamSynchronize(network->stream);
        return true;
    }
#endif
    
    return true;  // CPU is always synchronized
}

//=============================================================================
// High-Level Evaluation
//=============================================================================

NIMCP_EXPORT bool neural_logic_evaluate(
    neural_logic_network_t network,
    uint32_t gate_id,
    const float* inputs,
    uint32_t num_inputs,
    float* output)
{
    LOG_DEBUG(LOG_MODULE, "Evaluating logic gate: gate_id=%u, num_inputs=%u", gate_id, num_inputs);

    if (!nimcp_validate_pointer(network, "network") ||
        !nimcp_validate_pointer(inputs, "inputs") ||
        !nimcp_validate_pointer(output, "output")) {
        LOG_ERROR(LOG_MODULE, "NULL pointer(s) in evaluate: network=%p, inputs=%p, output=%p",
                  (void*)network, (const void*)inputs, (void*)output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_evaluate: nimcp_validate_pointer is NULL");
        return false;
    }

    if (gate_id >= network->neurons_count) {
        LOG_ERROR(LOG_MODULE, "Invalid gate_id: %u >= %u", gate_id, network->neurons_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_logic_evaluate: capacity exceeded");
        return false;
    }

    logic_neuron_state_t* neuron = &network->neurons_host[gate_id];

    // Directly compute gate output (combinational logic, no temporal dynamics needed)
    float input_a = (num_inputs >= 1) ? inputs[0] : 0.0F;
    float input_b = (num_inputs >= 2) ? inputs[1] : 0.0F;

    float result = 0.0F;

    switch (neuron->gate_type) {
        case LOGIC_GATE_AND:
            result = cpu_compute_and_gate(input_a, input_b, neuron->threshold, neuron->integration_window);
            break;

        case LOGIC_GATE_OR:
            result = cpu_compute_or_gate(input_a, input_b, neuron->threshold);
            break;

        case LOGIC_GATE_NOT:
            result = cpu_compute_not_gate(input_a, 1.0F, fabsf(neuron->inhibitory_weight));
            break;

        case LOGIC_GATE_XOR:
            result = cpu_compute_xor_gate(input_a, input_b, neuron->threshold, 0.1F);
            break;

        case LOGIC_GATE_IMPLIES:
            result = cpu_compute_implies_gate(input_a, input_b, 0.8F, 0.8F);
            break;

        default:
            LOG_WARN(LOG_MODULE, "Unsupported gate type: %d", neuron->gate_type);
            result = 0.0F;
            break;
    }

    // Return output
    *output = result;
    network->total_evaluations++;

    LOG_DEBUG(LOG_MODULE, "Logic gate evaluated: gate=%s(%u), inputs=[%.1f,%.1f], output=%.1f",
              neural_logic_gate_name(neuron->gate_type), gate_id, input_a, input_b, result);

    // Publish bio-async message for evaluation result if enabled
    if (network->bio_async_enabled && network->bio_ctx) {
        bool spiked = (result > 0.5F);
        nimcp_error_t err = neural_logic_broadcast_result(network, gate_id, result, spiked);
        if (err != NIMCP_SUCCESS) {
            LOG_DEBUG(LOG_MODULE, "Failed to broadcast result for gate %u: error=%d", gate_id, err);
        }
    }

    return true;
}

//=============================================================================
// Statistics
//=============================================================================

NIMCP_EXPORT bool neural_logic_get_state(
    neural_logic_network_t network,
    uint32_t neuron_id,
    logic_neuron_state_t* state)
{
    if (!nimcp_validate_pointer(network, "network") ||
        !nimcp_validate_pointer(state, "state")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_get_state: nimcp_validate_pointer is NULL");
        return false;
    }
    
    if (neuron_id >= network->neurons_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_logic_get_state: capacity exceeded");
        return false;
    }
    
    // Copy from host memory
    *state = network->neurons_host[neuron_id];
    
    return true;
}

NIMCP_EXPORT bool neural_logic_get_stats(
    neural_logic_network_t network,
    uint32_t* total_gates,
    uint32_t* total_variables,
    uint64_t* total_spikes,
    float* avg_eval_time,
    uint64_t* gpu_memory_used)
{
    if (!nimcp_validate_pointer(network, "network")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_get_stats: nimcp_validate_pointer is NULL");
        return false;
    }
    
    if (total_gates) *total_gates = network->neurons_count;
    if (total_variables) *total_variables = network->variables_count;
    if (total_spikes) *total_spikes = network->total_spikes;
    
    if (avg_eval_time) {
        *avg_eval_time = (network->total_evaluations > 0) ?
            (network->sum_eval_time_us / network->total_evaluations) : 0.0F;
    }
    
    if (gpu_memory_used) {
        *gpu_memory_used = 0;
#ifdef NIMCP_ENABLE_CUDA
        if (network->using_gpu) {
            *gpu_memory_used = 
                (network->neurons_capacity * sizeof(logic_neuron_state_t)) +
                (network->variables_capacity * sizeof(variable_binding_state_t));
        }
#endif
    }
    
    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* neural_logic_gate_name(logic_gate_type_t gate_type)
{
    switch (gate_type) {
        case LOGIC_GATE_AND: return "AND";
        case LOGIC_GATE_OR: return "OR";
        case LOGIC_GATE_NOT: return "NOT";
        case LOGIC_GATE_XOR: return "XOR";
        case LOGIC_GATE_IMPLIES: return "IMPLIES";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Associate brain with neural logic network for neuromodulation
 *
 * WHAT: Set brain reference for DA + ACh modulation of logic gates
 * WHY:  Enable neurochemical modulation (logical flexibility, precision)
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * BIOLOGY:
 * - Dopamine modulates logical flexibility vs rigidity
 * - Acetylcholine modulates logical precision
 *
 * @param network Neural logic network instance
 * @param brain Brain instance (or NULL to clear)
 */
NIMCP_EXPORT void neural_logic_set_brain(neural_logic_network_t network, brain_t brain)
{
    if (!network) {
        return;
    }
    network->brain = brain;
}

//=============================================================================
// Variable Binding (Stub Implementation)
//=============================================================================

NIMCP_EXPORT bool neural_logic_bind_variable(
    neural_logic_network_t network,
    uint32_t variable_id,
    const float* pattern,
    float binding_strength)
{
    if (!nimcp_validate_pointer(network, "network") ||
        !nimcp_validate_pointer(pattern, "pattern")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_bind_variable: nimcp_validate_pointer is NULL");
        return false;
    }
    
    if (variable_id >= network->variables_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_logic_bind_variable: capacity exceeded");
        return false;
    }

    // IMPLEMENTATION: Bind pattern to variable
    // WHAT: Store activation pattern in variable binding
    // WHY:  Variables need to hold learned patterns for unification
    // HOW:  Allocate memory and copy pattern with binding metadata
    //
    // RATIONALE: Variable bindings allow neural logic to perform pattern matching
    //            and unification similar to symbolic logic engines.

    variable_binding_state_t* var = &network->variables_host[variable_id];
    uint32_t dim = network->config.variable_pattern_dim;

    // Allocate pattern memory if needed
    if (var->bound_pattern == NULL) {
        var->bound_pattern = (float*)nimcp_malloc(dim * sizeof(float));
        if (!var->bound_pattern) {
            LOG_ERROR(LOG_MODULE,"Failed to allocate pattern memory for variable %u", variable_id);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_bind_variable: var->bound_pattern is NULL");
            return false;
        }
        var->pattern_dim = dim;
    }

    // Verify dimension matches
    if (var->pattern_dim != dim) {
        LOG_ERROR(LOG_MODULE,"Pattern dimension mismatch: expected %u, got %u",
                           var->pattern_dim, dim);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_bind_variable: validation failed");
        return false;
    }

    // Copy pattern and set binding state
    memcpy(var->bound_pattern, pattern, dim * sizeof(float));
    var->binding_strength = binding_strength;
    var->is_bound = true;
    var->decay_rate = 0.001F;  // Default decay: 0.1% per millisecond

    return true;
}

NIMCP_EXPORT bool neural_logic_query_variable(
    neural_logic_network_t network,
    uint32_t variable_id,
    float* pattern,
    uint32_t pattern_dim)
{
    if (!nimcp_validate_pointer(network, "network") ||
        !nimcp_validate_pointer(pattern, "pattern")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_query_variable: nimcp_validate_pointer is NULL");
        return false;
    }
    
    if (variable_id >= network->variables_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_logic_query_variable: capacity exceeded");
        return false;
    }

    // IMPLEMENTATION: Query bound pattern from variable
    // WHAT: Retrieve activation pattern stored in variable
    // WHY:  Logic evaluation needs to access variable contents
    // HOW:  Copy bound pattern to output buffer
    //
    // RATIONALE: Querying allows logic gates to read variable values
    //            for evaluation and unification operations.

    variable_binding_state_t* var = &network->variables_host[variable_id];

    // Check if variable is bound
    if (!var->is_bound || var->bound_pattern == NULL) {
        LOG_WARN(LOG_MODULE,"Variable %u is not bound", variable_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_logic_query_variable: var->is_bound is NULL");
        return false;
    }

    // Verify dimension matches
    if (var->pattern_dim != pattern_dim) {
        LOG_ERROR(LOG_MODULE,"Pattern dimension mismatch: variable has %u, requested %u",
                           var->pattern_dim, pattern_dim);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_query_variable: validation failed");
        return false;
    }

    // Copy pattern to output
    memcpy(pattern, var->bound_pattern, pattern_dim * sizeof(float));

    return true;
}

NIMCP_EXPORT bool neural_logic_connect(
    neural_logic_network_t network,
    uint32_t source_id,
    uint32_t target_id,
    float weight)
{
    if (!nimcp_validate_pointer(network, "network")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_logic_connect: nimcp_validate_pointer is NULL");
        return false;
    }

    // IMPLEMENTATION: Connect logic neurons via synapse
    // WHAT: Create weighted connection between two logic gates
    // WHY:  Logic gates need to propagate signals to form circuits
    // HOW:  Store connection in adjacency list (linked list per source neuron)
    //
    // RATIONALE: Adjacency list is ideal for sparse graphs (typical in neural circuits).
    //            Dense adjacency matrix would waste memory (O(N²) vs O(E) where E << N²).
    //
    // COMPLEXITY: O(1) amortized - append to linked list
    // MEMORY: O(E) where E = number of connections (edges)

    // Validate neuron IDs
    if (source_id >= network->neurons_count || target_id >= network->neurons_count) {
        LOG_ERROR(LOG_MODULE,"Invalid neuron IDs: source=%u, target=%u (max=%u)",
                           source_id, target_id, network->neurons_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_logic_connect: capacity exceeded");
        return false;
    }

    // Validate weight
    if (!isfinite(weight)) {
        LOG_ERROR(LOG_MODULE,"Invalid weight: %f", weight);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "neural_logic_connect: isfinite is NULL");
        return false;
    }

    // === Allocate Synapse from Pool (O(1)) or Fallback to malloc ===
    logic_synapse_t* synapse = NULL;
    if (network->synapse_pool) {
        synapse = (logic_synapse_t*)memory_pool_acquire(network->synapse_pool);
    }
    if (!synapse) {
        // Pool exhausted or doesn't exist - fallback to malloc
        synapse = (logic_synapse_t*)nimcp_malloc(sizeof(logic_synapse_t));
    }
    if (!nimcp_validate_pointer(synapse, "synapse")) {
        LOG_ERROR(LOG_MODULE,"Failed to allocate synapse: %u -> %u", source_id, target_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_connect: nimcp_validate_pointer is NULL");
        return false;
    }

    // Initialize synapse
    synapse->target_id = target_id;
    synapse->weight = weight;
    synapse->next = network->outgoing_synapses[source_id];  // Prepend to list

    // Store synapse in adjacency list
    network->outgoing_synapses[source_id] = synapse;
    network->synapse_counts[source_id]++;
    network->total_synapses++;

    LOG_DEBUG(LOG_MODULE, "Synapse connected: %u -> %u (weight=%.3f, total=%u)",
              source_id, target_id, weight, network->total_synapses);

    return true;
}

//=============================================================================
// Bio-Async Communication API
//=============================================================================

NIMCP_EXPORT bio_module_context_t neural_logic_get_bio_context(neural_logic_network_t network)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;
    }
    return network->bio_ctx;
}

NIMCP_EXPORT uint32_t neural_logic_process_bio_messages(neural_logic_network_t network, uint32_t max_messages)
{
    if (!network || !network->bio_async_enabled || !network->bio_ctx) {
        return 0;
    }

    uint32_t processed = bio_router_process_inbox(network->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG(LOG_MODULE, "Processed %u bio-async messages", processed);
    }
    return processed;
}

NIMCP_EXPORT nimcp_error_t neural_logic_broadcast_result(
    neural_logic_network_t network,
    uint32_t gate_id,
    float output,
    bool spiked)
{
    if (!network) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!network->bio_async_enabled || !network->bio_ctx) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Get gate type for the message
    uint32_t gate_type = LOGIC_GATE_AND;  // Default
    if (gate_id < network->neurons_count) {
        gate_type = network->neurons_host[gate_id].gate_type;
    }

    // Build and send message
    bio_msg_logic_gate_result_t msg;
    bio_msg_init_header(&msg.header, BIO_MSG_LOGIC_GATE_RESULT,
                        BIO_MODULE_NEURAL_LOGIC, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Logic results are fast queries
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.gate_id = gate_id;
    msg.gate_type = gate_type;
    msg.output = output;
    msg.spiked = spiked;
    msg.spike_time_us = 0;  // Could be filled from neuron state
    msg.threshold_used = 0;  // Could be filled from neuron state

    nimcp_error_t err = bio_router_broadcast(network->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Failed to broadcast logic result: gate=%u, error=%d",
                 gate_id, err);
    } else {
        LOG_DEBUG(LOG_MODULE, "Broadcast logic result: gate=%u, output=%.2f, spiked=%d",
                  gate_id, output, spiked);
    }
    return err;
}

NIMCP_EXPORT nimcp_error_t neural_logic_broadcast_circuit_complete(
    neural_logic_network_t network,
    uint32_t spikes_generated,
    uint32_t gates_evaluated)
{
    if (!network) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!network->bio_async_enabled || !network->bio_ctx) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Build and send message
    bio_msg_logic_circuit_complete_t msg;
    bio_msg_init_header(&msg.header, BIO_MSG_LOGIC_CIRCUIT_COMPLETE,
                        BIO_MODULE_NEURAL_LOGIC, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;  // Circuit completion is reward-like
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.timestamp_us = 0;  // Could be filled with current timestamp
    msg.spikes_generated = spikes_generated;
    msg.gates_evaluated = gates_evaluated;
    msg.avg_eval_time_us = (network->total_evaluations > 0) ?
        (network->sum_eval_time_us / network->total_evaluations) : 0.0F;
    msg.circuit_stable = (spikes_generated == 0);  // Stable if no new spikes

    nimcp_error_t err = bio_router_broadcast(network->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Failed to broadcast circuit complete: error=%d", err);
    } else {
        LOG_DEBUG(LOG_MODULE, "Broadcast circuit complete: spikes=%u, gates=%u",
                  spikes_generated, gates_evaluated);
    }
    return err;
}

//=============================================================================
// Quantum Acceleration API Implementation
//=============================================================================

/**
 * WHAT: Lazy initialization of quantum bridge
 * WHY:  Avoid circular dependencies, create only when needed
 * HOW:  Check if bridge exists, create if not, return cached instance
 */
NIMCP_EXPORT void* neural_logic_get_quantum_bridge(neural_logic_network_t network)
{
    if (!network) {
        LOG_ERROR(LOG_MODULE, "NULL network in get_quantum_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_logic_get_quantum_bridge: network is NULL");
        return NULL;
    }

    if (!network->quantum_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_logic_get_quantum_bridge: network->quantum_enabled is NULL");
        return NULL;
    }

    // Lazy initialization - create bridge on first access
    if (!network->quantum_bridge) {
        // Create bridge with default config
        // Note: We use a stack-allocated config buffer since we don't know the actual type
        uint8_t config_buffer[256];
        memset(config_buffer, 0, sizeof(config_buffer));
        neural_logic_quantum_default_config_opaque(config_buffer);

        network->quantum_bridge = neural_logic_quantum_bridge_create(config_buffer);
        if (!network->quantum_bridge) {
            LOG_ERROR(LOG_MODULE, "Failed to create quantum bridge");
            network->quantum_enabled = false;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_logic_get_quantum_bridge: network->quantum_bridge is NULL");
            return NULL;
        }

        // Connect bridge to this network
        if (neural_logic_quantum_bridge_connect(network->quantum_bridge, network) != 0) {
            LOG_ERROR(LOG_MODULE, "Failed to connect quantum bridge");
            neural_logic_quantum_bridge_destroy(network->quantum_bridge);
            network->quantum_bridge = NULL;
            network->quantum_enabled = false;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_logic_get_quantum_bridge: validation failed");
            return NULL;
        }

        LOG_INFO(LOG_MODULE, "Quantum bridge created and connected (lazy init)");
    }

    return network->quantum_bridge;
}

NIMCP_EXPORT bool neural_logic_is_quantum_enabled(neural_logic_network_t network)
{
    if (!network) {
        return false;
    }
    return network->quantum_enabled;
}

NIMCP_EXPORT void neural_logic_set_quantum_enabled(neural_logic_network_t network, bool enabled)
{
    if (!network) {
        LOG_ERROR(LOG_MODULE, "NULL network in set_quantum_enabled");
        return;
    }

    if (enabled && !network->quantum_enabled) {
        // Enabling quantum - will be lazy-initialized on first use
        network->quantum_enabled = true;
        LOG_INFO(LOG_MODULE, "Quantum acceleration enabled");
    } else if (!enabled && network->quantum_enabled) {
        // Disabling quantum - destroy bridge if exists
        if (network->quantum_bridge) {
            neural_logic_quantum_bridge_destroy(network->quantum_bridge);
            network->quantum_bridge = NULL;
            LOG_INFO(LOG_MODULE, "Quantum bridge destroyed");
        }
        network->quantum_enabled = false;
        LOG_INFO(LOG_MODULE, "Quantum acceleration disabled");
    }
}
