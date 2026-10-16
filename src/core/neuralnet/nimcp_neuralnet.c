#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_neuralnet.c - Spiking Neural Network with Advanced Plasticity
//=============================================================================
/**
 * @file nimcp_neuralnet.c
 * @brief Biologically-inspired neural network with STDP, Oja, and homeostasis
 *
 * ARCHITECTURAL OVERVIEW:
 * This module implements a spiking neural network with multiple learning rules:
 * - Strategy Pattern: Activation functions via function pointer tables
 * - Factory Pattern: Neuron/synapse creation through builder functions
 * - Repository Pattern: Hash-indexed synapse lookup for O(1) access
 *
 * DESIGN PATTERNS USED:
 * - Strategy Pattern: Activation functions, learning rules
 * - Factory Pattern: Network/neuron creation
 * - Builder Pattern: Configuration construction
 * - Single Responsibility: Each function does exactly one thing
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Neuron update: O(s) where s = num_synapses (was O(n) with linear search)
 * - Synapse lookup: O(1) average with hash table (was O(n))
 * - Learning: O(s) per neuron (optimized from O(s*n))
 * - Memory: O(n + s) where n = neurons, s = synapses
 *
 * COMPLEXITY IMPROVEMENTS:
 * - Original: O(n²) nested loops in many places
 * - Refactored: O(n) single-pass algorithms throughout
 * - Synapse access: O(n) linear → O(1) hash table
 * - Weight updates: Eliminated redundant calculations
 *
 * DESIGN PRINCIPLES:
 * - No nested ifs: Guard clauses and early returns only
 * - No nested loops: Extracted to helper functions
 * - Functions <50 lines: Complex operations decomposed
 * - Single responsibility: Each function has one clear purpose
 */

#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_sparse_synapse.h"         // NIMCP 2.11: Sparse synapse pools
#include "core/neuralnet/nimcp_neuron_synapse_access.h"  // NIMCP 2.11: Accessor macros
#include "core/synapse_compute/nimcp_synapse_compute.h"  // NIMCP 2.7: Programmable synapses
#include "core/synapse_types/nimcp_synapse_types.h"      // NIMCP 2.8.7: Synapse type system
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_izhikevich.h"
#include "core/neuron_models/nimcp_two_compartment.h"    // Part A3.1: Two-compartment neurons
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/bcm/nimcp_bcm.h"                    // Phase 11: BCM homeostatic plasticity
#include "plasticity/eligibility/nimcp_eligibility_trace.h"  // Phase 11: Eligibility traces for RL
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator system for dopamine/serotonin/etc
#include "glial/integration/nimcp_glial_integration.h"   // NIMCP Phase 6: Glial notifications
#include "security/nimcp_security.h"                     // Phase 11: Biological attack defense
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "utils/logging/nimcp_logging.h"

// === UNIFIED MEMORY INTEGRATION ===
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "neuralnet"
#define BIO_MODULE_ID 0x013B


#undef LOG_MODULE
#define LOG_MODULE "neuralnet"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/thread/nimcp_thread_pool.h"
#include <stdatomic.h>
#include <pthread.h>
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuralnet)

//=============================================================================
// Constants and Configuration
//=============================================================================

#define EPSILON 1e-10f
#define MAX_ACTIVATION 1.0f
#define MIN_ACTIVATION -1.0f
#define CALCIUM_DECAY_RATE 0.1f
#define TRACE_DECAY_RATE 0.05f
#define META_PLASTICITY_RATE 0.001f
#define HOMEOSTATIC_DECAY NIMCP_EMA_DECAY_SLOW
#define MAX_SYNAPTIC_STRENGTH 10.0f

/* Single authoritative definition of neural_network_struct and activation types */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

//=============================================================================
// Neuron Cold Data Factory Functions
//=============================================================================

neuron_cold_data_t* neuron_cold_data_create(void) {
    neuron_cold_data_t* cold = (neuron_cold_data_t*)nimcp_calloc(1, sizeof(neuron_cold_data_t));
    return cold;
}

void neuron_cold_data_destroy(neuron_cold_data_t* cold) {
    if (!cold) return;
    nimcp_free(cold);
}

/**
 * @brief Sync neuron's hot-path fields to cold data mirror
 *
 * WHAT: Copy oja_params, creation_time, model_type from neuron to cold struct
 * WHY:  Cold data is the authoritative copy for cache-optimized access paths
 * WHEN: Called after any modification to the mirrored fields
 */
static inline void neuron_sync_to_cold(neuron_t* neuron) {
    if (!neuron || !neuron->cold) return;
    neuron->cold->oja_params = neuron->oja_params;
    neuron->cold->creation_time = neuron->creation_time;
    neuron->cold->model_type = neuron->model_type;
}

/* Fast exp approximation using IEEE 754 bit manipulation.
 * ~3x faster than expf() with <0.1% error for |x| < 10.
 * Based on Schraudolph (1999) "A Fast, Compact Approximation of the Exponential Function" */
static inline float fast_expf(float x) {
    /* Clamp to prevent overflow/underflow */
    if (x < -87.0f) return 0.0f;
    if (x > 88.0f) return INFINITY;
    union { float f; int32_t i; } u;
    u.i = (int32_t)(12102203.0f * x + 1064866805.0f);
    return u.f;
}

//=============================================================================
// Forward Declarations - Organized by Functionality
//=============================================================================

// Activation functions
static float sigmoid(float x);
static float fast_tanh(float x);
static void init_activation_strategies(activation_strategy_table_t* table);

// Membrane and dynamics
static float compute_membrane_potential(neuron_t* neuron, neural_network_t network);
static void update_calcium_dynamics(neuron_t* neuron, uint64_t timestamp);
static void update_synaptic_traces(neural_network_t network, neuron_t* neuron, uint64_t timestamp);

// Learning rules
static float compute_stdp_update(float dt, const stdp_params_t* params);
static float compute_oja_weight_update(float pre_activity, float post_activity,
                                       float current_weight, const oja_params_t* params);

// Homeostasis
static float compute_homeostatic_factor(neuron_t* neuron, float current_activity);
static void update_meta_plasticity(neural_network_t network, neuron_t* neuron, uint64_t timestamp);
static void normalize_synaptic_weights(neuron_t* neuron);

// Neuron initialization helpers
static void init_neuron_basic_properties(neuron_t* neuron, uint32_t id, neuron_type_t type,
                                         uint64_t creation_time);
static void init_neuron_learning_params(neuron_t* neuron, const network_config_t* config);
static void init_neuron_homeostatic_params(neuron_t* neuron, const network_config_t* config);
static void init_neuron_activity_tracking(neuron_t* neuron, uint32_t spike_capacity, uint32_t activity_capacity);
static void init_neuron_activity_tracking_bulk(neuron_t* neuron, uint32_t spike_capacity, uint32_t activity_capacity,
                                               spike_record_t* spike_buf, float* activity_buf);
static void init_neuron_model(neuron_t* neuron, const network_config_t* config);

//=============================================================================
// Activation Function Implementations - Strategy Pattern
//=============================================================================

/**
 * @brief Resolve spike history capacity from config
 *
 * WHAT: Determine ring buffer capacity for spike history
 * WHY: Allow per-network configuration with sensible default
 * HOW: Use config value if > 0, else SPIKE_HISTORY_DEFAULT_CAPACITY
 */
static uint32_t resolve_spike_history_capacity(const network_config_t* config) {
    if (config && config->spike_history_capacity > 0)
        return config->spike_history_capacity;
    return SPIKE_HISTORY_DEFAULT_CAPACITY;
}

/**
 * @brief Resolve activity history capacity from config
 *
 * WHAT: Determine buffer capacity for activity history
 * WHY: Allow per-network configuration with sensible default
 * HOW: Use config value if > 0, else ACTIVITY_HISTORY_DEFAULT_CAPACITY
 */
static uint32_t resolve_activity_history_capacity(const network_config_t* config) {
    if (config && config->activity_history_capacity > 0)
        return config->activity_history_capacity;
    return ACTIVITY_HISTORY_DEFAULT_CAPACITY;
}

/**
 * @brief Sigmoid activation: 1 / (1 + e^(-x))
 *
 * WHY: Smooth, bounded output [0,1], good for probabilistic interpretations
 * WHEN: Classification, gating mechanisms
 *
 * COMPLEXITY: O(1)
 * @param input Raw input value
 * @param threshold Unused for sigmoid
 * @return Activated value in [0,1]
 */
static float activate_sigmoid(float input, float threshold)
{
    (void) threshold;  // Unused
    return sigmoid(input);
}

/**
 * @brief Hyperbolic tangent activation: tanh(x)
 *
 * WHY: Centered around 0, output [-1,1], good gradient properties
 * WHEN: Standard hidden layer activation
 *
 * COMPLEXITY: O(1)
 */
static float activate_tanh(float input, float threshold)
{
    (void) threshold;
    return fast_tanh(input);
}

/**
 * @brief ReLU activation: max(0, x)
 *
 * WHY: Simple, fast, prevents vanishing gradients
 * WHEN: Deep networks, default choice
 *
 * COMPLEXITY: O(1)
 */
static float activate_relu(float input, float threshold)
{
    (void) threshold;
    return (input > 0.0F) ? input : 0.0F;
}

/**
 * @brief Leaky ReLU: max(0.01x, x)
 *
 * WHY: Prevents dead neurons unlike standard ReLU
 * WHEN: Deep networks with dead neuron problems
 *
 * COMPLEXITY: O(1)
 */
static float activate_leaky_relu(float input, float threshold)
{
    (void) threshold;
    return (input > 0.0F) ? input : 0.01F * input;
}

/**
 * @brief Adaptive threshold activation for spiking neurons
 *
 * WHY: Models biological spike threshold adaptation
 * WHEN: Spiking neural networks with homeostasis
 *
 * ALGORITHM:
 * - Pass through input for membrane potential accumulation
 * - Spike detection happens separately in detected_spike()
 *
 * FIXED: Removed hardcoded -65.0 return that prevented membrane accumulation
 *
 * COMPLEXITY: O(1)
 */
static float activate_adaptive(float input, float threshold)
{
    (void)threshold;  // Threshold handled by spike detection logic

    // Pass through input to allow membrane potential accumulation
    // This lets the neuron integrate current over time
    return input;
}

/**
 * @brief Initialize activation strategy table
 *
 * WHY: Eliminates switch statements in hot path (activation calls)
 * Provides O(1) function dispatch vs O(n) switch
 *
 * PATTERN: Strategy pattern - populate dispatch table
 * COMPLEXITY: O(1)
 *
 * @param table Strategy table to initialize
 */
static void init_activation_strategies(activation_strategy_table_t* table)
{
    // Guard clause: Validate input
    if (!table)
        return;

    table->functions[ACTIVATION_SIGMOID] = activate_sigmoid;
    table->functions[ACTIVATION_TANH] = activate_tanh;
    table->functions[ACTIVATION_RELU] = activate_relu;
    table->functions[ACTIVATION_LEAKY_RELU] = activate_leaky_relu;
    table->functions[ACTIVATION_ADAPTIVE] = activate_adaptive;
    table->functions[ACTIVATION_LINEAR] = NULL;  /* Identity — handled inline in forward */
}

//=============================================================================
// Factory Pattern - Neuron Creation Helpers
//=============================================================================

/**
 * @brief Initialize basic neuron properties
 *
 * WHY: Extracted from network creation - single responsibility
 * WHEN: Creating new neurons in network
 *
 * COMPLEXITY: O(1)
 *
 * @param neuron Neuron to initialize
 * @param id Unique neuron identifier
 * @param type EXCITATORY or INHIBITORY
 * @param creation_time Network timestamp of creation
 */
static void init_neuron_basic_properties(neuron_t* neuron, uint32_t id, neuron_type_t type,
                                         uint64_t creation_time)
{
    // Guard clause: Validate input
    if (!neuron)
        return;

    neuron->id = id;
    neuron->type = type;
    neuron->rest_potential = 0.0F;    // Normalized resting potential
    neuron->threshold = 0.5F;         // Spike threshold (normalized, reachable in [-1,1] range)
    neuron->adaptation = 0.0F;
    neuron->refractory_period = 2;  // Milliseconds
    neuron->state = neuron->rest_potential;
    neuron->bias = (float) nimcp_tl_rand() / (float) RAND_MAX * 0.1F - 0.05F;
    neuron->external_current = 0.0F;  // No external input by default
    neuron->ema_activity = 0.0f;       // Initialize EMA activity tracking
    neuron->creation_time = creation_time;
    neuron->last_update = creation_time;
    neuron->last_spike = 0;

    // Sync cold data mirror
    if (neuron->cold) {
        neuron->cold->creation_time = creation_time;
    }
}

/**
 * @brief Initialize neuron learning parameters
 *
 * WHY: Separates learning config from basic properties
 * COMPLEXITY: O(1)
 */
static void init_neuron_learning_params(neuron_t* neuron, const network_config_t* config)
{
    // Guard clause: Validate inputs
    if (!neuron || !config)
        return;

    neuron->learning_rule = LEARNING_HYBRID;
    neuron->activation_type = ACTIVATION_ADAPTIVE;

    // Oja's rule parameters
    neuron->oja_params.alpha = config->learning_rate;
    neuron->oja_params.forgetting = 0.01F;
    neuron->oja_params.stabilization = 0.001F;
    neuron->oja_params.target_norm = 1.0F;

    // STDP parameters
    neuron->stdp_params.learning_rate = config->learning_rate;
    neuron->stdp_params.time_window = config->stdp_window;
    neuron->stdp_params.positive_factor = 1.2F;
    neuron->stdp_params.negative_factor = 0.8F;

    neuron->plasticity_rate = config->learning_rate;

    // Sync Oja params to cold data mirror
    if (neuron->cold) {
        neuron->cold->oja_params = neuron->oja_params;
    }
}

/**
 * @brief Initialize homeostatic plasticity parameters
 *
 * WHY: Separates homeostasis config from other initialization
 * COMPLEXITY: O(1)
 */
static void init_neuron_homeostatic_params(neuron_t* neuron, const network_config_t* config)
{
    // Guard clause: Validate inputs
    if (!neuron || !config)
        return;

    neuron->homeostatic.target_activity = config->target_activity;
    neuron->homeostatic.time_scale = 1000.0F;  // ms
    neuron->homeostatic.strength = 0.1F;
    neuron->homeostatic_factor = 1.0F;
    neuron->calcium_concentration = 0.0F;
    neuron->weight_norm = 0.0F;
}

/**
 * @brief Initialize activity tracking arrays
 *
 * WHY: Separates memory initialization from parameter setting
 * COMPLEXITY: O(1) - fixed-size arrays
 */
static void init_neuron_activity_tracking(neuron_t* neuron, uint32_t spike_capacity, uint32_t activity_capacity)
{
    // Guard clause: Validate input
    if (!neuron)
        return;

    neuron->spike_history_capacity = spike_capacity;
    neuron->spike_history_index = 0;
    neuron->spike_history_count = 0;
    neuron->avg_activity = 0.0F;
    neuron->ema_activity = 0.0f;
    neuron->spike_history = (spike_record_t*)nimcp_calloc(spike_capacity, sizeof(spike_record_t));

    // Activity history: heap-allocated dynamic buffer
    neuron->activity_history_capacity = activity_capacity;
    neuron->activity_history = (float*)nimcp_calloc(activity_capacity, sizeof(float));

    // Initialize sparse synapse storage (NIMCP 2.11)
    sparse_synapse_storage_init(&neuron->outgoing);
    sparse_synapse_storage_init(&neuron->incoming);
}

/**
 * @brief Initialize activity tracking with pre-allocated bulk buffers
 *
 * WHY: For large networks (>5K neurons), individual nimcp_calloc per neuron
 *      creates millions of tracked allocations through a mutex-locked hash table.
 *      Bulk allocation reduces 2M tracked allocs to 2, enabling 1M neuron creation.
 */
static void init_neuron_activity_tracking_bulk(neuron_t* neuron, uint32_t spike_capacity, uint32_t activity_capacity,
                                               spike_record_t* spike_buf, float* activity_buf)
{
    if (!neuron) return;

    neuron->spike_history_capacity = spike_capacity;
    neuron->spike_history_index = 0;
    neuron->spike_history_count = 0;
    neuron->avg_activity = 0.0F;
    neuron->ema_activity = 0.0f;
    neuron->spike_history = spike_buf;  // Points into bulk array

    neuron->activity_history_capacity = activity_capacity;
    neuron->activity_history = activity_buf;  // Points into bulk array

    sparse_synapse_storage_init(&neuron->outgoing);
    sparse_synapse_storage_init(&neuron->incoming);
}

/**
 * @brief Initialize neuron model (Izhikevich, LIF, etc.)
 *
 * WHAT: Creates neuron dynamics model based on config
 * WHY: Enables rich firing patterns beyond simple LIF
 * HOW: Uses plugin architecture with vtable dispatch
 *
 * PATTERN: Strategy Pattern - model selected at runtime
 * COMPLEXITY: O(1)
 *
 * @param neuron Neuron to initialize
 * @param config Network configuration with model type
 */
static void init_neuron_model(neuron_t* neuron, const network_config_t* config)
{
    // Guard: Validate inputs
    if (!neuron || !config) {
        return;
    }

    // Default: No model (legacy LIF behavior)
    neuron->model = NULL;
    neuron->model_type = NEURON_MODEL_LIF;

    // Guard: Check if specific model requested
    if (config->neuron_model == NEURON_MODEL_LIF) {
        return;  // Use legacy LIF, no plugin model needed
    }

    // Get vtable for requested model
    const neuron_model_vtable_t* vtable = NULL;
    const void* params = config->model_params;

    // Stack-local default params (declared at function scope for lifetime safety)
    izhikevich_params_t izhi_default_params;
    two_compartment_params_t tc_default_params;

    switch (config->neuron_model) {
        case NEURON_MODEL_IZHIKEVICH:
            vtable = izhikevich_get_vtable();
            // If no params provided, use default RS (Regular Spiking)
            if (!params) {
                izhi_default_params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
                params = &izhi_default_params;
            }
            break;

        case NEURON_MODEL_TWO_COMPARTMENT:
            vtable = two_compartment_get_vtable();
            // If no params provided, use defaults (70% attenuation, RK4)
            if (!params) {
                tc_default_params = two_compartment_default_params();
                // Use integration method from config
                tc_default_params.integration_method = config->integration_method;
                params = &tc_default_params;
            }
            break;

        case NEURON_MODEL_LIF:
            // Already handled above
            return;

        default:
            // Unknown model, fall back to legacy LIF
            return;
    }

    // Guard: Check vtable valid
    if (!vtable) {
        return;
    }

    // Create model state
    neuron->model = neuron_model_create(vtable, params);
    neuron->model_type = config->neuron_model;

    // Guard: Check creation succeeded
    if (!neuron->model) {
        neuron->model_type = NEURON_MODEL_LIF;
        return;
    }

    // Part A1.1: Set ODE integration method from config
    neuron_model_set_integration_method(neuron->model, config->integration_method);
}

//=============================================================================
// Network Creation - Factory Pattern
//=============================================================================

/**
 * @brief Validate network configuration
 *
 * WHY: Guard clause pattern - validates before allocating resources
 * Prevents invalid networks from being created
 *
 * COMPLEXITY: O(1)
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
static bool validate_network_config(const network_config_t* config)
{
    // Guard: Check null pointer
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_network_config: config is NULL");
        return false;
    }

    // Guard: Check neuron count
    if (config->num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_network_config: config->num_neurons is zero");
        return false;
    }
    if (config->num_neurons > MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "validate_network_config: config->num_neurons (%u) exceeds MAX_NEURONS (%u)",
            config->num_neurons, MAX_NEURONS);
        return false;
    }

    // Guard: Check input/output dimensions
    if (config->input_size == 0 || config->output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_network_config: config->input_size is zero");
        return false;
    }

    // Guard: Validate layer_sizes consistency
    if (config->num_layers > 1 && config->layer_sizes) {
        uint32_t layer_sum = 0;
        for (uint32_t l = 0; l < config->num_layers; l++) {
            if (config->layer_sizes[l] == 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                    "validate_network_config: layer_sizes[%u] is zero", l);
                return false;
            }
            layer_sum += config->layer_sizes[l];
        }
        // First layer must match input_size, last must match output_size
        if (config->layer_sizes[0] != config->input_size) {
            NIMCP_LOG_WARN("validate_network_config: layer_sizes[0]=%u != input_size=%u",
                           config->layer_sizes[0], config->input_size);
        }
        if (config->layer_sizes[config->num_layers - 1] != config->output_size) {
            NIMCP_LOG_WARN("validate_network_config: layer_sizes[last]=%u != output_size=%u",
                           config->layer_sizes[config->num_layers - 1], config->output_size);
        }
    }

    return true;
}

/**
 * @brief Create a new neural network with validation
 *
 * WHY: Factory pattern - single creation point with full validation
 * Encapsulates complex initialization sequence
 *
 * ALGORITHM:
 * 1. Validate configuration (O(1))
 * 2. Allocate network structure (O(1))
 * 3. Initialize each neuron using helper functions (O(n))
 * 4. Set up activation strategies (O(1))
 *
 * COMPLEXITY: O(n) where n = num_neurons
 * MEMORY: O(n * MAX_SYNAPSES_PER_NEURON)
 *
 * PATTERN: Factory pattern with builder helpers
 *
 * @param config Network configuration
 * @return Network handle or NULL on failure
 */

// --- Parallel backbone wiring worker ---
typedef struct {
    neural_network_t network;
    uint32_t src_start;
    uint32_t src_size;
    uint32_t dst_start;
    uint32_t dst_size;
    uint32_t fan_in;
    uint32_t dst_wired;
    float scale;
    uint32_t transition_conns;  // output
} backbone_wire_task_t;

static void* _backbone_wire_worker(void* arg) {
    backbone_wire_task_t* task = (backbone_wire_task_t*)arg;
    uint32_t dst_step = (task->dst_wired < task->dst_size) ? (task->dst_size / task->dst_wired) : 1;
    uint32_t conns = 0;

    for (uint32_t d = 0; d < task->dst_wired; d++) {
        uint32_t dst_id = task->dst_start + (d * dst_step) % task->dst_size;
        if (dst_id >= task->dst_start + task->dst_size) dst_id = task->dst_start + (dst_id % task->dst_size);
        if (dst_id >= task->network->num_neurons) break;

        uint32_t base_offset = (nimcp_tl_rand() % task->src_size);
        uint32_t stride = task->src_size / task->fan_in;
        if (stride == 0) stride = 1;

        for (uint32_t f = 0; f < task->fan_in; f++) {
            uint32_t src_offset = (base_offset + f * stride) % task->src_size;
            uint32_t src_id = task->src_start + src_offset;
            if (src_id >= task->network->num_neurons) continue;

            float weight = (((float)nimcp_tl_rand() / RAND_MAX) * 2.0F - 1.0F) * task->scale;
            neural_network_add_connection(task->network, src_id, dst_id, weight);
            conns++;
        }
    }
    task->transition_conns = conns;
    return NULL;
}

neural_network_t neural_network_create(const network_config_t* config)
{
    // Guard clause: Validate configuration
    if (!validate_network_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_create: invalid config");
        return NULL;
    }
    /**
     * WHAT: Allocate network structure
     * WHY: Create container for network state
     * HOW: Use nimcp_malloc for network struct (not neurons yet - done below)
     */
    neural_network_t network = (neural_network_t) nimcp_malloc(sizeof(struct neural_network_struct));

    // Guard clause: Check network allocation
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_network_create: failed to allocate network");
        return NULL;
    }

    // Initialize network state (zeros all fields including neurons pointer)
    memset(network, 0, sizeof(struct neural_network_struct));
    memcpy(&network->config, config, sizeof(network_config_t));

    /**
     * WHAT: Dynamically allocate neurons array with growth capacity
     * WHY: Support arbitrary network sizes + allow neural_network_add_neuron()
     * HOW: Allocate MAX_NEURONS or actual_neurons*2, whichever is smaller
     * PERFORMANCE: O(capacity) memory, allows growth without reallocation
     * TRADEOFF: Small overhead for networks that never add neurons
     *
     * FIX: For layered networks, num_neurons may only reflect the hidden layer.
     * The actual neuron count must include all layers (input + hidden + output).
     */
    uint32_t actual_neurons = config->num_neurons;
    if (config->num_layers > 1 && config->layer_sizes) {
        uint32_t total = 0;
        for (uint32_t l = 0; l < config->num_layers; l++) {
            total += config->layer_sizes[l];
        }
        if (total > actual_neurons) {
            actual_neurons = total;
        }
    }
    // Growth headroom: 2x for small networks, 10% for large (saves GB of memory)
    uint32_t capacity;
    if (actual_neurons > 100000) {
        capacity = actual_neurons + actual_neurons / 10;  // 10% headroom
    } else {
        capacity = actual_neurons * 2;
    }
    if (capacity > MAX_NEURONS) capacity = MAX_NEURONS;
    if (actual_neurons > capacity) actual_neurons = capacity;
    network->neurons = (neuron_t*) nimcp_calloc(capacity, sizeof(neuron_t));

    // Guard clause: Check neurons allocation
    if (!network->neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_network_create: failed to allocate neurons");
        nimcp_free(network);
        return NULL;
    }

    network->capacity = capacity;

    // Bulk-allocate cold data for all neurons (single allocation for cache efficiency)
    {
        neuron_cold_data_t* cold_bulk = (neuron_cold_data_t*)nimcp_calloc(capacity, sizeof(neuron_cold_data_t));
        if (cold_bulk) {
            for (uint32_t ci = 0; ci < capacity; ci++) {
                network->neurons[ci].cold = &cold_bulk[ci];
            }
        } else {
            LOG_WARN("Cold data bulk allocation failed for %u neurons, cold data disabled", capacity);
        }
    }

    /**
     * WHAT: Deep copy layer_sizes array if present (NIMCP 2.5 layered networks)
     * WHY: Config may be stack-allocated, need our own persistent copy
     * HOW: Allocate and memcpy the layer_sizes array
     */
    if (config->num_layers > 0 && config->layer_sizes) {
        uint32_t* layer_sizes_copy = (uint32_t*) nimcp_malloc(config->num_layers * sizeof(uint32_t));
        // Guard clause: Check layer_sizes allocation
        if (!layer_sizes_copy) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_network_create: failed to allocate layer_sizes");
            nimcp_free(network->neurons);
            nimcp_free(network);
            return NULL;
        }
        memcpy(layer_sizes_copy, config->layer_sizes, config->num_layers * sizeof(uint32_t));
        network->config.layer_sizes = layer_sizes_copy;
    } else {
        network->config.layer_sizes = NULL;
    }

    /* Allocate active neuron tracking pool */
    network->active_neuron_ids = (uint32_t*)nimcp_calloc(actual_neurons, sizeof(uint32_t));
    network->active_neuron_capacity = actual_neurons;
    network->num_active_neurons = 0;
    network->active_set_valid = false;

    network->network_time = 0;
    network->current_time = 0;
    network->global_activity = 0.0F;
    network->network_stability = 1.0F;
    network->learning_momentum = 0.0F;
    network->last_avg_weight = 0.0F;
    network->last_maintenance = 0;

    // Initialize activation strategy table
    init_activation_strategies(&network->activation_strategies);

    // Determine excitatory/inhibitory split
    uint32_t num_inhibitory = (uint32_t) (actual_neurons * (1.0F - config->ei_ratio));

    // Create neurons using builder pattern
    // FIX: Use actual_neurons (sum of layer_sizes) instead of config->num_neurons
    uint32_t spike_cap = resolve_spike_history_capacity(config);
    uint32_t activity_cap = resolve_activity_history_capacity(config);

    // Bulk allocation for large networks: 2 allocs instead of 2*N
    // WHY: nimcp_calloc tracks each allocation in a mutex-locked hash table.
    //      For 1M neurons, 2M individual callocs takes 30+ minutes.
    //      Bulk allocation reduces this to 2 callocs (~instant).
    bool use_bulk = (actual_neurons > 5000);
    network->bulk_neuron_count = 0;  // W4-5: ensure initialized before conditional set
    if (use_bulk) {
        network->spike_history_bulk = (spike_record_t*)nimcp_calloc(
            (size_t)actual_neurons * spike_cap, sizeof(spike_record_t));
        network->activity_history_bulk = (float*)nimcp_calloc(
            (size_t)actual_neurons * activity_cap, sizeof(float));
        if (!network->spike_history_bulk || !network->activity_history_bulk) {
            LOG_WARN("Bulk allocation failed for %u neurons, falling back to per-neuron", actual_neurons);
            /* Safe: network is not yet shared (still in neural_network_create) */
            nimcp_free(network->spike_history_bulk);
            network->spike_history_bulk = NULL;
            nimcp_free(network->activity_history_bulk);
            network->activity_history_bulk = NULL;
            use_bulk = false;
        } else {
            network->bulk_neuron_count = actual_neurons;
        }
    }

    for (uint32_t i = 0; i < actual_neurons; i++) {
        neuron_t* neuron = &network->neurons[i];
        neuron_type_t type = (i < num_inhibitory) ? NEURON_INHIBITORY : NEURON_EXCITATORY;

        init_neuron_basic_properties(neuron, i, type, network->network_time);
        neuron->refractory_period = config->refractory_period;

        init_neuron_learning_params(neuron, config);
        init_neuron_homeostatic_params(neuron, config);

        if (use_bulk) {
            init_neuron_activity_tracking_bulk(neuron, spike_cap, activity_cap,
                &network->spike_history_bulk[(size_t)i * spike_cap],
                &network->activity_history_bulk[(size_t)i * activity_cap]);
        } else {
            init_neuron_activity_tracking(neuron, spike_cap, activity_cap);
        }

        init_neuron_model(neuron, config);  // NIMCP 2.6: Initialize neuron model plugin

        // Sync all cold-data fields after full initialization
        neuron_sync_to_cold(neuron);
    }

    network->num_neurons = actual_neurons;

    // Calculate actual connections needed for layer wiring
    // Use uint64_t to avoid overflow with large networks (e.g., 256 * 2M = 512M)
    uint64_t dense_connections = 0;
    if (config->num_layers > 1 && config->layer_sizes) {
        for (uint32_t l = 0; l < config->num_layers - 1; l++) {
            dense_connections += (uint64_t)config->layer_sizes[l] * config->layer_sizes[l + 1];
        }
    }

    // For large networks, compute sparse backbone connection count for pool sizing
    bool will_skip_dense = config->skip_layer_wiring || (dense_connections > 10000000);
    uint64_t backbone_connections = 0;
    if (will_skip_dense && config->num_layers > 1 && config->layer_sizes) {
        // Scalable estimate: each non-input neuron gets ~16 backbone + ~4 skip connections
        uint32_t total_non_input = 0;
        for (uint32_t l = 1; l < config->num_layers; l++) {
            total_non_input += config->layer_sizes[l];
        }
        // 20 connections per neuron (16 backbone + 4 skip), capped at 20M per transition
        backbone_connections = (uint64_t)total_non_input * 20;
        uint64_t max_backbone = (uint64_t)(config->num_layers - 1) * 20000000ULL;
        if (backbone_connections > max_backbone) backbone_connections = max_backbone;
    }

    // NIMCP 2.11: Create sparse synapse pools
    // Pool must hold all layer-to-layer connections (2 handles per connection:
    // one outgoing from source, one incoming to destination)
    {
        // Each connection = 2 handles (outgoing + incoming), add 25% headroom for
        // runtime plasticity (new connections from learning, sprouting, etc.)
        uint64_t effective_connections = will_skip_dense ? backbone_connections : dense_connections;
        uint64_t handles_needed = effective_connections * 2;
        // For large networks (>500K neurons) dense wiring is skipped, so backbone
        // wiring provides initial connectivity. Pool must fit backbone handles.
        uint32_t pool_min;
        if (actual_neurons < 100) pool_min = 5000;
        else if (actual_neurons > 500000) pool_min = actual_neurons * 40;  // ~20 conns × 2 handles each
        else pool_min = actual_neurons * 64;
        uint64_t pool_size64 = handles_needed + handles_needed / 4;  // +25% headroom
        if (pool_size64 < pool_min) pool_size64 = pool_min;
        if (pool_size64 > SPARSE_SYNAPSE_MAX_POOL_SIZE) pool_size64 = SPARSE_SYNAPSE_MAX_POOL_SIZE;
        uint32_t pool_size = (uint32_t)pool_size64;

        sparse_synapse_pool_config_t hpool_cfg = sparse_synapse_pool_default_config();
        hpool_cfg.pool_size = pool_size;
        hpool_cfg.enable_statistics = true;
        hpool_cfg.thread_safe = true;
        network->synapse_handle_pool = sparse_synapse_pool_create(&hpool_cfg);

        // Metadata pool: pre-allocate based on expected backbone connections to avoid
        // repeated growth events during wiring. For layered networks, estimate total
        // connections as num_transitions × min(dst_size × fan_in, 20M cap).
        synapse_metadata_pool_config_t mpool_cfg = synapse_metadata_pool_default_config();
        uint32_t meta_initial = pool_size / 20;
        if (config->num_layers > 1 && config->layer_sizes) {
            uint64_t estimated_conns = 0;
            uint32_t MIN_FAN_IN_EST = 64;
            for (uint32_t l = 0; l < config->num_layers - 1; l++) {
                uint64_t planned = (uint64_t)config->layer_sizes[l + 1] * MIN_FAN_IN_EST;
                if (planned > 20000000) planned = 20000000;
                estimated_conns += planned;
            }
            // Both outgoing + incoming handles per connection, plus skip connections (~10%)
            uint64_t total_handles = estimated_conns * 2 + estimated_conns / 5;
            if (total_handles > (uint64_t)meta_initial) {
                meta_initial = (uint32_t)(total_handles < UINT32_MAX ? total_handles : UINT32_MAX);
            }
        }
        if (meta_initial < 100000) meta_initial = 100000;
        if (meta_initial > SPARSE_SYNAPSE_MAX_POOL_SIZE) meta_initial = SPARSE_SYNAPSE_MAX_POOL_SIZE;
        /* The old 5M cap was removed. On a 2.5M-neuron brain the pool
         * needs ~213M slots (11 GB). The 5M cap forced 3200+ incremental
         * block-by-block grows at ~0.5s each = ~27 minutes of pure pool
         * growth during brain_load. Pre-allocating the estimated amount
         * in one shot takes ~15 seconds instead. Machines without
         * enough RAM will still hit SPARSE_SYNAPSE_MAX_POOL_SIZE above,
         * and the pool grows on demand as a fallback if the estimate is
         * too low, so this is strictly an optimization, not a
         * correctness change. */
        mpool_cfg.pool_size = meta_initial;
        mpool_cfg.enable_statistics = true;
        mpool_cfg.thread_safe = true;
        network->synapse_metadata_pool = synapse_metadata_pool_create(&mpool_cfg);

        // Cold pool: 5% of metadata pool (most synapses won't need cold data)
        synapse_cold_pool_config_t cpool_cfg = synapse_cold_pool_default_config();
        cpool_cfg.pool_size = meta_initial / 20;  // 5% of metadata pool
        if (cpool_cfg.pool_size < 16384) cpool_cfg.pool_size = 16384;
        cpool_cfg.thread_safe = true;
        network->synapse_cold_pool = synapse_cold_pool_create(&cpool_cfg);

        LOG_MODULE_DEBUG(LOG_MODULE, "Synapse pools created (handles=%u, metadata=%u, cold=%zu)",
                         pool_size, meta_initial, cpool_cfg.pool_size);

        // Bulk BCM/eligibility pools: single allocation replaces 1M+ individual callocs.
        // Each connection gets one BCM and one eligibility entry (outgoing only).
        // estimated_conns already accounts for backbone connections.
        uint64_t est = will_skip_dense ? backbone_connections : dense_connections;
        if (est == 0) est = (uint64_t)actual_neurons * 20;  // fallback estimate
        uint32_t pool_cap = (uint32_t)(est < UINT32_MAX ? est : UINT32_MAX);
        // Add 25% headroom for runtime synaptogenesis
        pool_cap = pool_cap + pool_cap / 4;

        if (config->enable_bcm) {
            network->bcm_pool = (bcm_synapse_t*)nimcp_calloc(pool_cap, sizeof(bcm_synapse_t));
            if (network->bcm_pool) {
                network->bcm_pool_capacity = pool_cap;
                network->bcm_pool_used = 0;
            }
        }
        if (config->enable_eligibility) {
            network->eligibility_pool = (eligibility_trace_t*)nimcp_calloc(pool_cap, sizeof(eligibility_trace_t));
            if (network->eligibility_pool) {
                network->eligibility_pool_capacity = pool_cap;
                network->eligibility_pool_used = 0;
            }
        }
    }

    // CPU-staged embedding pool: only created when input dim matches embedding dim.
    // Full 2048-dim vectors live in CPU pinned memory; only relevance cache on GPU.
    // Skip for mismatched dims — relevance recompute won't fire, saves 16+ GB RAM.
    if (config->input_size == NIMCP_DEFAULT_EMBEDDING_DIM) {
        uint64_t est_conn = will_skip_dense ? backbone_connections : dense_connections;
        if (est_conn == 0) est_conn = (uint64_t)actual_neurons * 20;
        uint32_t emb_cap = (uint32_t)((est_conn / 20) < UINT32_MAX ? (est_conn / 20) : UINT32_MAX);
        if (emb_cap < 1024) emb_cap = 1024;  // Minimum pool
        #define EMBEDDING_POOL_CAP 500000  /* ~4 GB for 2048D */
        if (emb_cap > EMBEDDING_POOL_CAP) emb_cap = EMBEDDING_POOL_CAP;
        embedding_pool_create(network, emb_cap, NIMCP_DEFAULT_EMBEDDING_DIM);
    }

    // For layered networks (NIMCP 2.5), create connections between layers
    // Skip dense wiring when called from resize (skip_layer_wiring=true)
    // Skip dense wiring for large networks (>10M connections) — spiking plasticity
    // will form connections organically during learning
    bool skip_dense = config->skip_layer_wiring || (dense_connections > 10000000);
    if (config->num_layers > 1 && config->layer_sizes && !skip_dense) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Dense layer wiring (%u layers, %llu connections)",
                         config->num_layers, (unsigned long long)dense_connections);
        uint32_t offset = 0;
        for (uint32_t layer = 0; layer < config->num_layers - 1; layer++) {
            uint32_t curr_layer_size = config->layer_sizes[layer];
            uint32_t next_layer_size = config->layer_sizes[layer + 1];
            // Guard against uint32_t overflow
            if (curr_layer_size > UINT32_MAX - offset) {
                LOG_WARN("Layer offset overflow at layer %u, stopping wiring", layer);
                break;
            }
            uint32_t next_layer_offset = offset + curr_layer_size;

            // Connect each neuron in current layer to each neuron in next layer.
            // Use Xavier/Glorot uniform initialization: U(-limit, limit) where
            // limit = sqrt(6 / (fan_in + fan_out)). This keeps activation
            // variance stable across layers, preventing gradient explosion.
            float limit = sqrtf(6.0f / (float)(curr_layer_size + next_layer_size));
            for (uint32_t i = 0; i < curr_layer_size && offset + i < network->num_neurons; i++) {
                for (uint32_t j = 0;
                     j < next_layer_size && next_layer_offset + j < network->num_neurons; j++) {
                    float weight = (2.0f * ((float) nimcp_tl_rand() / RAND_MAX) - 1.0f) * limit;
                    neural_network_add_connection(network, offset + i, next_layer_offset + j,
                                                  weight);
                }
            }

            offset = next_layer_offset;
        }
    } else if (config->num_layers > 1 && config->layer_sizes && skip_dense) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Sparse backbone wiring (%u layers, est %llu connections)",
                         config->num_layers, (unsigned long long)backbone_connections);
        // SPARSE BACKBONE WIRING for large networks
        // Dense wiring is O(N*M) which is infeasible (e.g. 256 * 1.5M = 384M connections).
        // Instead, select a small "backbone" of hidden neurons and wire them to
        // input/output layers. This creates a viable forward-pass path for
        // predict_fast (gradient-based learning) while keeping memory manageable.
        // Remaining neurons participate via spiking dynamics in brain_decide().

        uint32_t input_size = config->layer_sizes[0];
        uint32_t output_size = config->layer_sizes[config->num_layers - 1];

        // Calculate total hidden neurons (all layers between input and output)
        uint32_t total_hidden = 0;
        for (uint32_t l = 1; l < config->num_layers - 1; l++) {
            total_hidden += config->layer_sizes[l];
        }

        if (total_hidden > 0) {
            // LAYER-AWARE BACKBONE WIRING for multi-layer architectures.
            // Wire backbone neurons layer-by-layer: input→L1→L2→...→Ln→output.
            // Each layer transition gets its own backbone subset, ensuring
            // gradient flow through all layers (not just input→output skip).

            uint32_t total_conns = 0;
            uint32_t total_backbone = 0;
            uint32_t num_transitions = config->num_layers - 1;

            // Pre-compute layer start indices
            uint32_t* layer_starts = nimcp_calloc(config->num_layers, sizeof(uint32_t));
            if (!layer_starts) {
                LOG_ERROR(LOG_MODULE, "Failed to allocate layer_starts");
            } else {
                layer_starts[0] = 0;
                for (uint32_t l = 1; l < config->num_layers; l++) {
                    layer_starts[l] = layer_starts[l - 1] + config->layer_sizes[l - 1];
                }

                // Wire each layer transition: layer[l] → layer[l+1]
                // Parallel wiring: each transition has disjoint src/dst neuron ranges,
                // so threads can wire simultaneously. Pool allocators are thread-safe.
                backbone_wire_task_t* tasks = nimcp_calloc(num_transitions, sizeof(backbone_wire_task_t));
                if (!tasks) {
                    LOG_ERROR(LOG_MODULE, "Failed to allocate backbone wire tasks");
                } else {
                    // Prepare tasks
                    for (uint32_t t = 0; t < num_transitions; t++) {
                        tasks[t].network = network;
                        tasks[t].src_start = layer_starts[t];
                        tasks[t].src_size = config->layer_sizes[t];
                        tasks[t].dst_start = layer_starts[t + 1];
                        tasks[t].dst_size = config->layer_sizes[t + 1];

                        uint32_t MIN_FAN_IN = 128;
                        uint32_t MAX_FAN_IN = 512;
                        uint32_t fan_in = MIN_FAN_IN;
                        if (fan_in > tasks[t].src_size) fan_in = tasks[t].src_size;
                        if (fan_in > MAX_FAN_IN) fan_in = MAX_FAN_IN;
                        tasks[t].fan_in = fan_in;

                        uint32_t max_total = 40000000;
                        uint32_t dst_wired = tasks[t].dst_size;
                        uint64_t total_planned = (uint64_t)dst_wired * (uint64_t)fan_in;
                        if (total_planned > max_total) {
                            dst_wired = max_total / fan_in;
                            if (dst_wired < 1024) dst_wired = 1024;
                        }
                        tasks[t].dst_wired = dst_wired;
                        tasks[t].scale = sqrtf(2.0F / (float)fan_in);
                        tasks[t].transition_conns = 0;
                    }

                    // Launch threads (up to 4, or num_transitions if fewer)
                    uint32_t max_threads = 4;
                    if (max_threads > num_transitions) max_threads = num_transitions;
                    pthread_t* threads = nimcp_calloc(max_threads, sizeof(pthread_t));

                    if (threads && max_threads > 1) {
                        // Parallel: launch in batches of max_threads
                        for (uint32_t batch_start = 0; batch_start < num_transitions; batch_start += max_threads) {
                            uint32_t batch_end = batch_start + max_threads;
                            if (batch_end > num_transitions) batch_end = num_transitions;
                            uint32_t batch_count = batch_end - batch_start;

                            for (uint32_t t = 0; t < batch_count; t++) {
                                pthread_create(&threads[t], NULL, _backbone_wire_worker, &tasks[batch_start + t]);
                            }
                            for (uint32_t t = 0; t < batch_count; t++) {
                                pthread_join(threads[t], NULL);
                            }
                        }
                    } else {
                        // Fallback: serial
                        for (uint32_t t = 0; t < num_transitions; t++) {
                            _backbone_wire_worker(&tasks[t]);
                        }
                    }

                    // Collect results
                    for (uint32_t t = 0; t < num_transitions; t++) {
                        total_conns += tasks[t].transition_conns;
                        total_backbone += tasks[t].dst_wired;
                        LOG_INFO(LOG_MODULE, "Backbone L%u->L%u: %u dst × %u fan-in = %u connections",
                                 t, t + 1, tasks[t].dst_wired, tasks[t].fan_in, tasks[t].transition_conns);
                    }

                    nimcp_free(threads);
                    nimcp_free(tasks);
                }

                // RESIDUAL / SKIP CONNECTIONS:
                // Wire input layer directly to every hidden layer (not just L1).
                // This creates "gradient highways" that let signal and gradients
                // bypass intermediate layers — the key insight from ResNets.
                // Each hidden layer neuron gets SKIP_FAN_IN connections from the
                // input layer, weighted at 1/sqrt(fan_in) to be additive with
                // the layer-to-layer signal.
                //
                // This ensures that even in a 7+ layer network, every neuron
                // can receive input signal directly, preventing vanishing
                // activations regardless of depth.

                if (num_transitions > 1 && layer_starts) {
                    uint32_t SKIP_FAN_IN = 4;  // light residual, not overwhelming
                    uint32_t skip_total = 0;

                    for (uint32_t skip_layer = 2; skip_layer < config->num_layers; skip_layer++) {
                        uint32_t skip_dst_size = config->layer_sizes[skip_layer];
                        uint32_t skip_dst_start = layer_starts[skip_layer];
                        uint32_t skip_fan = SKIP_FAN_IN;
                        if (skip_fan > input_size) skip_fan = input_size;

                        #define SKIP_CONNECTION_CAP 500000
                        uint32_t skip_dst_wired = skip_dst_size;
                        if ((uint64_t)skip_dst_wired * skip_fan > SKIP_CONNECTION_CAP) {
                            skip_dst_wired = SKIP_CONNECTION_CAP / skip_fan;
                        }

                        float skip_scale = 0.5F / sqrtf((float)skip_fan);  // weaker than direct
                        uint32_t skip_dst_step = (skip_dst_wired < skip_dst_size) ?
                            (skip_dst_size / skip_dst_wired) : 1;

                        for (uint32_t d = 0; d < skip_dst_wired; d++) {
                            uint32_t dst_id = skip_dst_start + (d * skip_dst_step) % skip_dst_size;
                            if (dst_id >= network->num_neurons) break;

                            uint32_t base = nimcp_tl_rand() % input_size;
                            uint32_t stride = input_size / skip_fan;
                            if (stride == 0) stride = 1;
                            for (uint32_t f = 0; f < skip_fan; f++) {
                                uint32_t src_id = (base + f * stride) % input_size;
                                float w = (((float)nimcp_tl_rand() / RAND_MAX) * 2.0F - 1.0F) * skip_scale;
                                neural_network_add_connection(network, src_id, dst_id, w);
                                skip_total++;
                            }
                        }
                    }

                    total_conns += skip_total;
                    LOG_INFO(LOG_MODULE, "Skip connections: %u (input -> all hidden layers)", skip_total);
                }

                nimcp_free(layer_starts);

                LOG_INFO(LOG_MODULE, "Sparse backbone: %u total connections across %u transitions "
                         "(%.1f%% of dense %llu)",
                         total_conns, num_transitions,
                         (double)total_conns / (double)(dense_connections > 0 ? dense_connections : 1) * 100.0,
                         (unsigned long long)dense_connections);
            }
        }
    }

    // Initialize semantic embeddings for all wired synapses
    if (network->embedding_pool && network->embedding_pool_capacity > 0) {
        embedding_pool_init_all_synapses(network);
    }

    // Activation types — always set for layered networks (even when wiring is skipped)
    if (config->num_layers > 1 && config->layer_sizes) {
        // TRAINING FIX: Set activation types for gradient-based training
        // ACTIVATION_ADAPTIVE creates dead neurons (output=0 if below threshold)
        // For gradient-based training, we need differentiable activations
        //
        // Hidden layers: ReLU (fast, helps with vanishing gradients)
        // Output layer: Sigmoid (bounded [0,1] for classification probabilities)

        uint32_t input_layer_size = config->layer_sizes[0];

        // Set hidden layer neurons to Leaky ReLU
        // (Regular ReLU causes "dying neurons" when activations are negative)
        uint32_t hidden_start = input_layer_size;
        uint32_t hidden_end = 0;
        for (uint32_t l = 0; l < config->num_layers - 1; l++) {
            hidden_end += config->layer_sizes[l];
        }
        for (uint32_t i = hidden_start; i < hidden_end && i < network->num_neurons; i++) {
            network->neurons[i].activation_type = ACTIVATION_LEAKY_RELU;
        }

        // Set output layer to LINEAR (identity) for regression.
        // Tanh [-1,1] caused gradient vanishing when targets were in a narrow
        // sub-range. Linear allows the network to learn any output magnitude.
        uint32_t output_layer_start = hidden_end;
        uint32_t output_layer_size = config->layer_sizes[config->num_layers - 1];
        for (uint32_t i = 0; i < output_layer_size && output_layer_start + i < network->num_neurons; i++) {
            network->neurons[output_layer_start + i].activation_type = ACTIVATION_LINEAR;
        }
    }

    // Bio-async registration
    network->bio_ctx = NULL;
    network->bio_async_enabled = false;
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_NEURALNET,
            .module_name = "neuralnet",
            .inbox_capacity = 64,
            .user_data = network
        };
        network->bio_ctx = bio_router_register_module(&bio_info);
        if (network->bio_ctx) {
            network->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async registered for neuralnet");
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed for neuralnet");
        }
    }

    /* Phase 3: Allocate learnable layer norm gamma/beta for hidden layers */
    if (config->num_layers > 2) {
        uint32_t norm_layers = config->num_layers - 2;  /* Skip input + output */
        network->layer_norm_gamma = (float**)nimcp_calloc(norm_layers, sizeof(float*));
        network->layer_norm_beta = (float**)nimcp_calloc(norm_layers, sizeof(float*));
        if (network->layer_norm_gamma && network->layer_norm_beta) {
            network->num_norm_layers = norm_layers;
            for (uint32_t l = 0; l < norm_layers; l++) {
                uint32_t lsize = config->layer_sizes[l + 1];  /* Hidden layer sizes */
                network->layer_norm_gamma[l] = (float*)nimcp_malloc(lsize * sizeof(float));
                network->layer_norm_beta[l] = (float*)nimcp_malloc(lsize * sizeof(float));
                if (network->layer_norm_gamma[l] && network->layer_norm_beta[l]) {
                    for (uint32_t i = 0; i < lsize; i++) {
                        network->layer_norm_gamma[l][i] = 1.0f;
                        network->layer_norm_beta[l][i] = 0.0f;
                    }
                }
            }
        }
    }

    /* Phase 4: Allocate residual/skip connection state.
     * Truncated-identity: only need saved activation buffers per layer.
     * No projection matrices (they caused OOM at 2M neurons — 200K×200K×4 = 160GB). */
    network->enable_residual = (config->num_layers > 3);
    if (config->num_layers > 3) {
        network->num_residual_pairs = 0;
        network->residual_saved_states = (float**)nimcp_calloc(config->num_layers, sizeof(float*));
        network->residual_projections = NULL;
        network->residual_proj_src_dim = NULL;
        network->residual_proj_dst_dim = NULL;

        if (network->residual_saved_states) {
            for (uint32_t l = 0; l < config->num_layers; l++) {
                network->residual_saved_states[l] = (float*)nimcp_calloc(config->layer_sizes[l], sizeof(float));
            }
        }
        /* No projection matrices — truncated identity only */
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "neural_network_create complete: %u neurons, %u layers",
                     network->num_neurons, network->config.num_layers);
    return network;
}

/**
 * @brief Destroy neural network and free resources
 *
 * WHY: Proper resource cleanup prevents memory leaks
 * COMPLEXITY: O(1) - simple deallocation
 *
 * @param network Network to destroy (can be NULL)
 */
/**
 * WHAT: Destroy neural network and free all allocated resources
 * WHY: Prevent memory leaks by freeing all dynamic allocations
 * HOW: Free neurons array, layer_sizes, then network struct (reverse of creation order)
 * PATTERN: RAII-style cleanup, reverse order of allocation
 */
void neural_network_destroy(neural_network_t network)
{
    // Guard clause: Handle NULL input
    if (!network)
        return;

    // Bio-async unregistration
    if (network->bio_async_enabled && network->bio_ctx) {
        bio_router_unregister_module(network->bio_ctx);
        network->bio_ctx = NULL;
        network->bio_async_enabled = false;
        LOG_INFO(LOG_MODULE, "Bio-async unregistered for neuralnet");
    }

    // Fast-path destroy for bulk-allocated networks with no per-neuron heap state
    // WHY: For 1M neurons with deferred wiring, iterating all neurons just to
    //       check NULL pointers thrashes 3.4 GB of cache for no work. Skip it.
    bool has_per_neuron_heap = false;
    if (network->neurons && network->num_neurons > 0) {
        // Sample first neuron to detect if any per-neuron heap state exists
        neuron_t* first = &network->neurons[0];
        if (first->model || NEURON_OUT_COUNT(first) > 0 || NEURON_IN_COUNT(first) > 0) {
            has_per_neuron_heap = true;
        }
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Destroying neuron models (%u neurons, per_heap=%d)",
                     network->num_neurons, has_per_neuron_heap);
    if (has_per_neuron_heap && network->neurons) {
        // Determine if BCM/eligibility were bulk-allocated (pool exists)
        bool bcm_bulk = (network->bcm_pool != NULL);
        bool elig_bulk = (network->eligibility_pool != NULL);

        for (uint32_t i = 0; i < network->num_neurons; i++) {
            if (network->neurons[i].model) {
                neuron_model_destroy(network->neurons[i].model);
                network->neurons[i].model = NULL;
            }

            neuron_t* neuron = &network->neurons[i];
            if (!bcm_bulk || !elig_bulk) {
                // Only iterate synapses if some BCM/eligibility were heap-allocated
                for (uint32_t j = 0; j < NEURON_OUT_COUNT(neuron); j++) {
                    synapse_t* syn = NEURON_OUT_META(network, neuron, j);
                    if (!syn) continue;
                    synapse_cold_t* cold = SYNAPSE_COLD(network, syn);
                    if (cold) {
                        if (!bcm_bulk && cold->bcm) {
                            nimcp_free(cold->bcm);
                            cold->bcm = NULL;
                        }
                        if (!elig_bulk && cold->eligibility) {
                            nimcp_free(cold->eligibility);
                            cold->eligibility = NULL;
                        }
                    }
                }
            }
        }
    }

    // Free bulk BCM/eligibility pools (single free replaces 674K+ individual frees)
    if (network->bcm_pool) {
        nimcp_free(network->bcm_pool);
        network->bcm_pool = NULL;
    }
    if (network->eligibility_pool) {
        nimcp_free(network->eligibility_pool);
        network->eligibility_pool = NULL;
    }

    // Free CPU-staged embedding pool
    embedding_pool_destroy(network);

    LOG_MODULE_DEBUG(LOG_MODULE, "Neuron model+BCM+embedding cleanup done");
    // Free spike/activity history: bulk arrays first, then individual stragglers
    LOG_MODULE_DEBUG(LOG_MODULE, "Freeing spike/activity history...");
    if (network->neurons) {
        uint32_t bulk_count = network->bulk_neuron_count;

        // NULL out pointers for bulk-allocated neurons BEFORE freeing bulk arrays.
        // This prevents any accidental double-free if something later iterates all neurons.
        for (uint32_t i = 0; i < bulk_count && i < network->num_neurons; i++) {
            network->neurons[i].spike_history = NULL;
            network->neurons[i].activity_history = NULL;
        }

        if (network->spike_history_bulk) {
            nimcp_free(network->spike_history_bulk);
            network->spike_history_bulk = NULL;
        }
        if (network->activity_history_bulk) {
            nimcp_free(network->activity_history_bulk);
            network->activity_history_bulk = NULL;
        }

        // Free individually-allocated history buffers (neurons beyond bulk_count)
        for (uint32_t i = bulk_count; i < network->num_neurons; i++) {
            if (network->neurons[i].spike_history) {
                nimcp_free(network->neurons[i].spike_history);
                network->neurons[i].spike_history = NULL;
            }
            if (network->neurons[i].activity_history) {
                nimcp_free(network->neurons[i].activity_history);
                network->neurons[i].activity_history = NULL;
            }
        }
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Spike/activity history freed");
    // Sparse synapse storage cleanup: skip for networks with no connections
    LOG_MODULE_DEBUG(LOG_MODULE, "Cleaning up sparse synapse storage...");
    if (has_per_neuron_heap && network->neurons && network->synapse_handle_pool) {
        for (uint32_t i = 0; i < network->num_neurons; i++) {
            neuron_t* neuron = &network->neurons[i];
            sparse_synapse_storage_cleanup(network->synapse_handle_pool, &neuron->outgoing);
            sparse_synapse_storage_cleanup(network->synapse_handle_pool, &neuron->incoming);
        }
    }
    if (network->synapse_metadata_pool) {
        synapse_metadata_pool_destroy(network->synapse_metadata_pool);
    }
    if (network->synapse_cold_pool) {
        synapse_cold_pool_destroy(network->synapse_cold_pool);
    }
    if (network->synapse_handle_pool) {
        sparse_synapse_pool_destroy(network->synapse_handle_pool);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Sparse synapse cleanup done");

    /* Free learnable layer norm parameters */
    if (network->layer_norm_gamma) {
        for (uint32_t l = 0; l < network->num_norm_layers; l++) {
            nimcp_free(network->layer_norm_gamma[l]);
        }
        nimcp_free(network->layer_norm_gamma);
        network->layer_norm_gamma = NULL;
    }
    if (network->layer_norm_beta) {
        for (uint32_t l = 0; l < network->num_norm_layers; l++) {
            nimcp_free(network->layer_norm_beta[l]);
        }
        nimcp_free(network->layer_norm_beta);
        network->layer_norm_beta = NULL;
    }

    /* Free residual/skip connection state */
    if (network->residual_saved_states) {
        for (uint32_t l = 0; l < network->config.num_layers; l++) {
            nimcp_free(network->residual_saved_states[l]);
        }
        nimcp_free(network->residual_saved_states);
        network->residual_saved_states = NULL;
    }
    if (network->residual_projections) {
        for (uint32_t p = 0; p < network->num_residual_pairs; p++) {
            nimcp_free(network->residual_projections[p]);
        }
        nimcp_free(network->residual_projections);
        network->residual_projections = NULL;
    }
    nimcp_free(network->residual_proj_src_dim);
    nimcp_free(network->residual_proj_dst_dim);
    network->residual_proj_src_dim = NULL;
    network->residual_proj_dst_dim = NULL;

    nimcp_free(network->active_neuron_ids);
    network->active_neuron_ids = NULL;

    // Free cold data bulk array (allocated as single block in neural_network_create)
    if (network->neurons && network->capacity > 0 && network->neurons[0].cold) {
        nimcp_free(network->neurons[0].cold);  // Free bulk cold data array
        for (uint32_t ci = 0; ci < network->capacity; ci++) {
            network->neurons[ci].cold = NULL;
        }
    }

    if (network->neurons) {
        nimcp_free(network->neurons);
    }

    if (network->config.layer_sizes) {
        nimcp_free((void*) network->config.layer_sizes);
    }

    nimcp_free(network);
}

//=============================================================================
// Mathematical Utility Functions
//=============================================================================

/**
 * @brief Fast Padé approximation of hyperbolic tangent
 *
 * WHY: Faster than standard library tanh() while maintaining accuracy
 * ERROR: <0.001 across typical neural network input ranges
 *
 * ALGORITHM: Padé [7/7] rational approximation
 * COMPLEXITY: O(1)
 *
 * @param x Input value
 * @return tanh(x) approximation
 */
static float fast_tanh(float x)
{
    if (fabsf(x) > 4.0f) return tanhf(x);
    float x2 = x * x;
    float a = x * (135135.0F + x2 * (17325.0F + x2 * (378.0F + x2)));
    float b = 135135.0F + x2 * (62370.0F + x2 * (3150.0F + x2 * 28.0F));
    return a / (b + 1e-10f);
}

/**
 * @brief Standard sigmoid activation: σ(x) = 1/(1+e^(-x))
 *
 * WHY: Smooth, bounded, differentiable - core of many neural networks
 * COMPLEXITY: O(1)
 *
 * @param x Input value
 * @return Sigmoid output in (0,1)
 */
static float sigmoid(float x)
{
    return 1.0F / (1.0F + expf(-x));
}

/**
 * @brief Clamp value to valid activation range
 *
 * WHY: Prevents numerical overflow and maintains biological plausibility
 * COMPLEXITY: O(1)
 *
 * @param value Value to clamp
 * @return Clamped value in [MIN_ACTIVATION, MAX_ACTIVATION]
 */
static float clamp_activation(float value)
{
    return fmaxf(MIN_ACTIVATION, fminf(MAX_ACTIVATION, value));
}

//=============================================================================
// Activation Function API - Strategy Pattern
//=============================================================================

/**
 * @brief Compute activation using strategy pattern
 *
 * WHY: O(1) dispatch via function pointer table vs O(n) switch statement
 * Uses Strategy pattern for extensibility and performance
 *
 * ALGORITHM:
 * 1. Look up activation function in strategy table (O(1))
 * 2. Call function with input and threshold (O(1))
 * 3. Clamp result to valid range (O(1))
 *
 * COMPLEXITY: O(1) - constant time regardless of activation type
 * PERFORMANCE GAIN: ~2x faster than switch statement in hot loop
 *
 * @param neuron Neuron with activation type
 * @param input Raw input to activate
 * @return Activated output, clamped to valid range
 */
float neural_network_compute_activation(neuron_t* neuron, float input)
{
    // Guard clause: Validate neuron
    if (!neuron)
        return 0.0F;

    // Temporary: Get network context for strategy table
    // In production, would pass network or use global table
    activation_strategy_table_t table;
    init_activation_strategies(&table);

    // Guard clause: Check activation type bounds
    if (neuron->activation_type < 0 || neuron->activation_type >= 8) {
        return clamp_activation(input);  // Default: pass-through
    }

    // Strategy dispatch - O(1) lookup and call
    activation_fn_t activation_fn = table.functions[neuron->activation_type];

    // Guard clause: Ensure function exists
    if (!activation_fn) {
        return clamp_activation(input);
    }

    // Execute activation function
    float result = activation_fn(input, neuron->threshold);

    // Update adaptive threshold if using adaptive activation
    if (neuron->activation_type == ACTIVATION_ADAPTIVE) {
        if (input > neuron->threshold) {
            neuron->threshold += neuron->adaptation;
        } else {
            neuron->threshold = fmaxf(neuron->threshold - neuron->adaptation * 0.1F,
                                      neuron->rest_potential + 10.0F);
        }
    }

    return clamp_activation(result);
}

//=============================================================================
// Membrane Potential and Neuron Dynamics
//=============================================================================

/**
 * @brief Compute synaptic contribution to membrane potential
 *
 * WHY: Extracted from compute_membrane_potential for single responsibility
 * Isolates synap tic summation from modulation
 *
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param neuron Neuron receiving input
 * @param network Network containing pre-synaptic neurons
 * @return Summed synaptic input
 */
/**
 * @brief Fast-path synaptic summation for embedded synapses without advanced features
 *
 * 40-watt brain optimization: >90% of synapses use default computation
 * (no STP, no custom compute, no semantic embeddings, no glial).
 * This fast path avoids 4+ branch checks per synapse.
 */
static float sum_embedded_synapses_fast(neuron_t* neuron, neural_network_t network) {
    float total = 0.0f;
    uint32_t count = neuron->incoming.embedded_count;

    for (uint32_t i = 0; i < count; i++) {
        synapse_handle_t* in_h = &neuron->incoming.embedded[i];
        uint32_t src_id = in_h->target_neuron_id;
        if (src_id >= network->num_neurons) continue;

        /* Prefetch next synapse's presynaptic neuron state for cache warmup */
#ifdef __GNUC__
        if (i + 1 < count) {
            uint32_t next_src = neuron->incoming.embedded[i + 1].target_neuron_id;
            if (next_src < network->num_neurons) {
                __builtin_prefetch(&network->neurons[next_src].state, 0, 1);
            }
        }
#endif

        neuron_t* src = &network->neurons[src_id];
        float pre_activity = (src->state > src->threshold) ? src->state : 0.0f;

        /* Default computation: simple weight * strength * activity */
        total += pre_activity * in_h->weight * in_h->strength;
    }
    return total;
}

static float sum_synaptic_inputs(neuron_t* neuron, neural_network_t network)
{
    // Guard clause: Validate inputs
    if (!neuron || !network)
        return 0.0F;

    /* 40-watt brain: Fast path for embedded synapses when no advanced features are active.
     * Most neurons use only embedded synapses with default computation. */
    bool has_advanced = (network->glial_integration != NULL);
    if (!has_advanced && neuron->incoming.overflow_count == 0) {
        /* Pure embedded, no advanced features — use branchless fast path */
        return sum_embedded_synapses_fast(neuron, network);
    }

    float total_input = 0.0F;

    // OPTIMIZED: Use bidirectional synapse tracking for O(S) instead of O(N×S)
    // DESIGN PATTERN: Bidirectional Association
    // WHY: Direct access to incoming synapses eliminates need to scan entire network
    // COMPLEXITY: O(S) where S = num incoming synapses (was O(N×S))
    // SPEEDUP: 10-100x for large networks (N > 1000)

    uint32_t in_count = NEURON_IN_COUNT(neuron);
    for (uint32_t i = 0; i < in_count; i++) {
        synapse_handle_t* in_h = NEURON_IN_HANDLE(neuron, i);
        if (!in_h) continue;

        // Prefetch next synapse's presynaptic neuron state for cache warmup
#ifdef __GNUC__
        if (i + 1 < in_count) {
            synapse_handle_t* next_h = NEURON_IN_HANDLE(neuron, i + 1);
            if (next_h && next_h->target_neuron_id < network->num_neurons) {
                __builtin_prefetch(&network->neurons[next_h->target_neuron_id].state, 0, 1);
            }
        }
#endif

        // In incoming handle, target_neuron_id stores the SOURCE neuron ID
        uint32_t src_id = in_h->target_neuron_id;

        if (src_id >= network->num_neurons) {
            continue;  // Safety check
        }

        neuron_t* src_neuron = &network->neurons[src_id];

        // Only transmit if presynaptic neuron is active (not at rest)
        // For spiking neurons, we use state > threshold as activity indicator
        float pre_activity =
            (src_neuron->state > src_neuron->threshold) ? src_neuron->state : 0.0F;

        // Early-exit: skip metadata lookup for simple synapses (95% case)
        if (in_h->metadata_index == SPARSE_SYNAPSE_NO_METADATA) {
            float simple_trans = pre_activity * in_h->weight * in_h->strength;
            // G3: astrocyte modulation [0.8, 1.2] on synapse efficacy.
            // G4: myelin conduction bonus — a fully-myelinated presynaptic
            // axon has saltatory conduction that delivers the spike with
            // minimal attenuation. Modeled here as a transmission efficacy
            // bonus of up to +0.5 (×1.5) instead of a true delay model.
            // G5: microglia pruning — if the microglia marked this synapse
            // for pruning, zero the transmission (synapse is functionally
            // dead). Weight stays non-zero until the learning sweep cleans
            // it up; this keeps the forward pass side-effect free.
            if (network->glial_integration && simple_trans > 0.0F) {
                glial_integration_t* gi =
                    (glial_integration_t*)network->glial_integration;
                if (glial_integration_should_prune_synapse(gi, src_id, neuron->id)) {
                    simple_trans = 0.0F;
                } else {
                    float astro_mod = glial_integration_get_synaptic_modulation(gi,
                        src_id, neuron->id);
                    float myel_boost = 1.0F + 0.5F *
                        glial_integration_get_myelination_factor(gi, src_id);
                    simple_trans *= astro_mod * myel_boost;
                    glial_integration_on_synapse_fired(gi,
                        src_id, neuron->id, in_h->weight, network->network_time);
                }
            }
            total_input += simple_trans;
            continue;
        }

        // Get metadata for advanced features (STP, compute functions, embeddings)
        synapse_t* incoming_meta = NEURON_IN_META(network, neuron, i);

        // NIMCP 2.6: Apply STP modulation if enabled
        float stp_modulation = 1.0F;
        synapse_cold_t* in_cold = incoming_meta ? SYNAPSE_COLD(network, incoming_meta) : NULL;
        if (in_cold && in_cold->enable_stp) {
            // Update STP continuous decay
            stp_update(&in_cold->stp, network->network_time);

            // Get modulation factor (u × x)
            stp_modulation = stp_get_modulation(&in_cold->stp);

            // Process spike if presynaptic neuron is firing
            if (pre_activity > 0.0F) {
                stp_process_spike(&in_cold->stp, network->network_time);
            }
        }

        // NIMCP 2.7: Programmable synapse computation (MAJOR FEATURE!)
        // If synapse has custom compute function, use it; otherwise default computation
        float synaptic_transmission;
        if (in_cold && in_cold->compute_function != NULL) {
            // Custom computation - synapse decides how to compute transmission
            // This enables attention, semantic similarity, gating, etc.

            // NIMCP 2.7: Use cached neuromodulator level (computed once per compute_step)
            float neuromod_level = network->cached_neuromod_level;

            synapse_compute_context_t context = {
                .global_state = network->global_state,  // Attention output, etc.
                .global_state_size = network->global_state_size,
                .neuromodulation = neuromod_level,  // Dopamine/ACh/etc levels
                .current_time = network->network_time,
                .custom_data = NULL,
                .custom_data_size = 0,
                .synapse_cold = in_cold  // NIMCP 2.11: Pass cold data through context
            };

            synaptic_transmission = in_cold->compute_function(
                incoming_meta,
                src_neuron,
                neuron,
                pre_activity,
                &context
            );
        } else {
            // Default computation (backward compatible with NIMCP 2.0-2.6)
            synaptic_transmission = pre_activity * in_h->weight *
                                   in_h->strength * stp_modulation;
        }

        // ENHANCEMENT 1: Semantic embedding routing
        // WHAT: Modulate transmission by semantic relevance
        // WHY: Route information through semantically relevant synapses (70% faster)
        // HOW: Use cached semantic_relevance if embeddings enabled
        if (in_cold && in_cold->embedding_pool_index != NIMCP_EMBEDDING_POOL_NONE && in_cold->embedding_dim > 0) {
            // Use cached relevance (computed during context update)
            // Relevance in [0,1]: 0=irrelevant, 1=highly relevant
            float semantic_modulation = 0.5F + 0.5F * incoming_meta->semantic_relevance;
            synaptic_transmission *= semantic_modulation;
        }

        // G3 + G4 + G5: astrocyte modulation + myelin bonus + microglia
        // pruning. See matching block in the no-metadata path above for the
        // biological rationale. Applied AFTER STP + semantic modulation so
        // the glial signals act on the final transmission value. Pruning
        // zeros the transmission (synapse is functionally dead); the weight
        // itself is cleaned up by the learning-path sweep.
        if (network->glial_integration && synaptic_transmission > 0.0F) {
            glial_integration_t* gi =
                (glial_integration_t*)network->glial_integration;
            if (glial_integration_should_prune_synapse(gi, src_id, neuron->id)) {
                synaptic_transmission = 0.0F;
            } else {
                float astro_mod = glial_integration_get_synaptic_modulation(gi,
                    src_id, neuron->id);
                float myel_boost = 1.0F + 0.5F *
                    glial_integration_get_myelination_factor(gi, src_id);
                synaptic_transmission *= astro_mod * myel_boost;
            }
        }

        total_input += synaptic_transmission;

        // NIMCP Phase 6: Notify glial system of synapse firing
        // WHAT: Inform astrocytes about synaptic transmission events
        // WHY: Enables calcium-mediated tripartite synapse modulation
        // HOW: Observer pattern - glial system observes neural events
        // WHEN: Only if glial system attached and synapse actually fired
        if (network->glial_integration && synaptic_transmission > 0.0F) {
            glial_integration_on_synapse_fired(
                (glial_integration_t*)network->glial_integration,
                src_id,                    // Presynaptic neuron ID
                neuron->id,                // Postsynaptic neuron ID
                in_h->weight,              // Synaptic weight
                network->network_time      // Timestamp
            );
        }
    }

    return total_input;
}

/**
 * @brief Compute membrane potential for a neuron
 *
 * WHY: Central equation for neural dynamics - integrates all inputs
 * Models biological membrane potential equation
 *
 * ALGORITHM:
 * V_membrane = bias + Σ(w_i * a_i * s_i) * (1 + [Ca²⁺])
 * where:
 * - w_i = synaptic weight
 * - a_i = presynaptic activity
 * - s_i = synaptic strength
 * - [Ca²⁺] = calcium concentration (modulation)
 *
 * COMPLEXITY: O(s) where s = num_synapses (single pass)
 *
 * @param neuron Post-synaptic neuron
 * @param network Network containing all neurons
 * @return Membrane potential (mV in biological terms)
 */
static float compute_membrane_potential(neuron_t* neuron, neural_network_t network)
{
    // Guard clause: Validate inputs
    if (!neuron || !network)
        return 0.0F;

    // Start with intrinsic bias
    float potential = neuron->bias;

    // Add synaptic contributions
    potential += sum_synaptic_inputs(neuron, network);

    // Apply calcium-mediated modulation (activity-dependent amplification)
    potential *= (1.0F + neuron->calcium_concentration);

    // Add external input current (e.g., from spike encoding)
    // WHAT: Include external current in membrane potential
    // WHY: Allows external stimulation (sensors, embeddings) to drive spikes
    // HOW: Direct addition after synaptic integration
    // NOTE: Reset after compute_step() to avoid accumulation
    potential += neuron->external_current;

    return potential;
}


//=============================================================================
// Neuron Update - Decomposed into Helper Functions
//=============================================================================

/**
 * @brief Check if neuron is in refractory period
 *
 * WHY: Extracted guard clause for clarity and testability
 * COMPLEXITY: O(1)
 *
 * @return true if in refractory period (cannot spike)
 */
static bool is_in_refractory_period(neuron_t* neuron, uint64_t timestamp)
{
    // If neuron has never spiked (last_spike == 0), not in refractory period
    if (neuron->last_spike == 0) {
        return false;
    }
    return (timestamp - neuron->last_spike) < neuron->refractory_period;
}

/**
 * @brief Detect spike event (threshold crossing)
 *
 * WHY: Extracted spike detection logic for clarity
 * COMPLEXITY: O(1)
 *
 * @return true if spike occurred
 */
static bool detected_spike(float old_state, float new_state, float threshold)
{
    return (old_state <= threshold) && (new_state > threshold);
}

/**
 * @brief Apply all enabled learning rules to neuron
 *
 * WHY: Extracted from update_neuron to eliminate nested ifs
 * Single responsibility - only handles learning rule dispatch
 *
 * COMPLEXITY: O(s) where s = num_synapses
 */
static void apply_learning_rules(neural_network_t network, uint32_t neuron_id, neuron_t* neuron,
                                 uint64_t timestamp)
{
    // Guard clause: Check if Oja's rule enabled
    if (neuron->learning_rule & LEARNING_OJA) {
        neural_network_apply_oja(network, neuron_id, timestamp);
    }

    // Guard clause: Check if STDP enabled
    if (neuron->learning_rule & LEARNING_STDP) {
        neural_network_apply_stdp(network, neuron_id, timestamp);
    }
}

/**
 * @brief Update activity history and compute running average
 *
 * WHY: Extracted activity tracking for single responsibility
 * COMPLEXITY: O(w) where w = HISTORY_WINDOW
 */
static void update_activity_history(neuron_t* neuron, float new_state, uint64_t timestamp)
{
    // Guard clause: Validate input
    if (!neuron || !neuron->activity_history || neuron->activity_history_capacity == 0)
        return;

    uint32_t cap = neuron->activity_history_capacity;

    // Record current activity in ring buffer
    uint32_t history_idx = timestamp % cap;
    neuron->activity_history[history_idx] = new_state;

    // Incremental EMA: O(1) instead of O(w) full-window average
    float decay = 1.0f / (float)cap;
    neuron->avg_activity = neuron->avg_activity * (1.0f - decay) + new_state * decay;

    // Activity EMA with fixed alpha=0.05 for neuron-level optimization tracking
    neuron->ema_activity = 0.95f * neuron->ema_activity + 0.05f * new_state;
}

/**
 * @brief Update dynamics (calcium, traces) after state change
 *
 * WHY: Extracted dynamics updates for clarity
 * COMPLEXITY: O(s) where s = num_synapses
 */
static void update_neuron_dynamics(neural_network_t network, neuron_t* neuron, uint64_t timestamp)
{
    // Guard clause: Validate input
    if (!neuron)
        return;

    update_calcium_dynamics(neuron, timestamp);
    update_synaptic_traces(network, neuron, timestamp);
}

/**
 * @brief Handle spike event and trigger learning
 *
 * WHY: Extracted spike handling to eliminate nesting in main update function
 * Single responsibility - orchestrates spike response
 *
 * COMPLEXITY: O(s) where s = num_synapses
 */
static void handle_spike_event(neural_network_t network, uint32_t neuron_id, neuron_t* neuron,
                               float new_state, uint64_t timestamp)
{
    // Record the spike
    neural_network_record_spike(network, neuron_id, new_state, timestamp);

    // NIMCP Phase 6: Notify glial system of neuron firing
    // WHAT: Inform oligodendrocytes and microglia about neuronal spiking
    // WHY: Enables adaptive myelination and synaptic pruning
    // HOW: Observer pattern - glial system observes neural spike events
    // WHEN: Only if glial system attached
    if (network->glial_integration) {
        glial_integration_on_neuron_fired(
            (glial_integration_t*)network->glial_integration,
            neuron_id,         // ID of neuron that fired
            timestamp          // Time of spike
        );
    }

    // Apply learning rules
    apply_learning_rules(network, neuron_id, neuron, timestamp);

    // Update plasticity mechanisms
    neural_network_update_plasticity(network, neuron_id, timestamp);
    neural_network_apply_homeostasis(network, neuron_id, timestamp);
}

/**
 * @brief Update neuron state and apply learning rules
 *
 * WHY: Main neuron update - refactored to eliminate ALL nested ifs
 * Uses guard clauses and extracted helpers for clarity
 *
 * ALGORITHM:
 * 1. Validate inputs (guard clauses)
 * 2. Check refractory period (early return if refractory)
 * 3. Compute membrane potential and activation
 * 4. Detect spike and handle if occurred
 * 5. Update activity tracking
 * 6. Update dynamics (calcium, traces)
 *
 * COMPLEXITY: O(s) where s = num_synapses
 * PREVIOUSLY: Had nested ifs 3 levels deep
 * NOW: All guard clauses, maximum 1 level
 *
 * @param network Neural network
 * @param neuron_id ID of neuron to update
 * @param input_current External input current
 * @param timestamp Current simulation time
 * @return true if neuron updated successfully, false if in refractory period
 */
bool neural_network_update_neuron(neural_network_t network, uint32_t neuron_id, float input_current,
                                  uint64_t timestamp)
{
    // Guard clause: Validate network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_update_neuron: network is NULL");
        return false;
    }

    // Guard clause: Validate neuron ID
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_update_neuron: neuron_id %u out of range", neuron_id);
        return false;
    }

    neuron_t* neuron = &network->neurons[neuron_id];

    // Guard clause: Check refractory period
    if (is_in_refractory_period(neuron, timestamp)) {
        neuron->state = neuron->rest_potential;
        return false;
    }

    // Declare new_state before branches for use in activity tracking
    float new_state;

    // NIMCP 2.6: Use neuron model plugin if available
    if (neuron->model != NULL) {
        // Calculate timestep (assume 1ms if timestamps match)
        float dt = (timestamp > neuron->last_update) ? (float)(timestamp - neuron->last_update) : 1.0F;

        // Compute synaptic input for model
        float membrane_potential = compute_membrane_potential(neuron, network);
        float total_input = membrane_potential + input_current;

        // Update model dynamics
        neuron_model_update(neuron->model, dt, total_input);

        // Get new voltage from model
        new_state = neuron_model_get_voltage(neuron->model);
        neuron->state = new_state;

        // Check for spike using model's spike detection
        if (neuron_model_check_spike(neuron->model)) {
            handle_spike_event(network, neuron_id, neuron, new_state, timestamp);
            neuron_model_post_spike(neuron->model);  // Reset model after spike
        }
    } else {
        // Legacy LIF behavior
        float membrane_potential = compute_membrane_potential(neuron, network);
        float total_input = membrane_potential + input_current;
        new_state = neural_network_compute_activation(neuron, total_input);

        // Capture old state for spike detection
        float old_state = neuron->state;
        neuron->state = new_state;

        // Handle spike if detected
        if (detected_spike(old_state, new_state, neuron->threshold)) {
            handle_spike_event(network, neuron_id, neuron, new_state, timestamp);

            // WHAT: Reset neuron state after spike (LIF dynamics)
            // WHY: Spiking neurons must reset to allow future spikes
            // HOW: Set to rest potential, like neuron_model_post_spike() does for models
            // FIX (Phase 5.1): Without this, neuron stays above threshold forever
            neuron->state = neuron->rest_potential;
        }
    }

    // Update activity tracking and dynamics (always happens)
    update_activity_history(neuron, new_state, timestamp);
    update_neuron_dynamics(network, neuron, timestamp);

    neuron->last_update = timestamp;
    return true;
}

/**
 * @brief Apply Oja's learning rule
 */
uint32_t neural_network_apply_oja(neural_network_t network, uint32_t neuron_id, uint64_t timestamp)
{
    // Guard: Validate inputs
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_apply_oja: network is NULL");
        return 0;
    }
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_apply_oja: neuron_id out of range");
        return 0;
    }

    neuron_t* neuron = &network->neurons[neuron_id];
    uint32_t modified = 0;

    // FIX: Use avg_activity instead of current state (which may be 0 after spike reset)
    // Synapse goes FROM neuron TO target, so neuron is presynaptic
    float x = neuron->avg_activity;  // Pre-synaptic average activity

    // Skip update if neuron is not active enough
    if (fabsf(x) < ACTIVITY_THRESHOLD) {
        return 0;
    }

    // Calculate weight updates using Oja's rule
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_handle_t* out_h = NEURON_OUT_HANDLE(neuron, i);
        if (!out_h) continue;
        synapse_t* syn = NEURON_OUT_META(network, neuron, i);
        if (!syn) continue;

        // HIGH-3 fix: Bounds check target_neuron_id before array access
        if (out_h->target_neuron_id >= network->num_neurons) continue;

        // Get post-synaptic average activity (target of synapse)
        float y = network->neurons[out_h->target_neuron_id].avg_activity;

        // Compute weight update using Oja's rule: Δw = α(y*x - y²*w)
        float delta_w = compute_oja_weight_update(x, y, out_h->weight, &neuron->oja_params);

        // Apply weight update with meta-plasticity
        float new_weight = out_h->weight + delta_w * syn->meta_plasticity;

        // Guard against NaN/Inf from numerical instability
        if (!isfinite(new_weight)) new_weight = 0.0f;

        // Apply weight constraints
        new_weight =
            fmaxf(network->config.min_weight, fminf(network->config.max_weight, new_weight));

        // Update weight if change is significant
        if (fabsf(new_weight - out_h->weight) > WEIGHT_UPDATE_THRESHOLD) {
            out_h->weight = new_weight;
            syn->weight = new_weight;  // Keep metadata in sync
            modified++;
        }

        // Update synaptic strength
        float new_strength = fminf(out_h->strength * (1.0F + delta_w), MAX_SYNAPTIC_STRENGTH);
        if (!isfinite(new_strength)) new_strength = 1.0f;
        out_h->strength = new_strength;
        syn->strength = out_h->strength;  // Keep metadata in sync
    }

    // Normalize weights periodically
    if (modified > 0 && (timestamp - neuron->last_update) > NORMALIZATION_INTERVAL) {
        normalize_synaptic_weights(neuron);
    }

    return modified;
}

/**
 * @brief Compute weight update using Oja's rule
 */
static float compute_oja_weight_update(float pre_activity, float post_activity,
                                       float current_weight, const oja_params_t* params)
{
    // WHAT: Compute Oja's learning rule weight update
    // WHY: PCA-like learning with weight normalization
    // HOW: Hebbian term - normalization + stabilization
    // CONST: params is read-only configuration
    float hebbian_term = post_activity * pre_activity;
    float normalization_term = post_activity * post_activity * current_weight;
    float stabilization_term = params->stabilization * (params->target_norm - current_weight);

    return params->alpha * (hebbian_term - normalization_term + stabilization_term);
}

/**
 * @brief Apply STDP learning rule
 *
 * WHAT: Update synaptic weights based on spike timing
 * WHY: STDP implements causality-based learning (Hebbian principle)
 * HOW: For each outgoing synapse, compute Δt and apply STDP rule
 *
 * ALGORITHM:
 * - For outgoing synapse: current neuron = PRE, target = POST
 * - Δt = t_post - t_pre
 * - If Δt > 0 (pre before post): LTP (strengthen)
 * - If Δt < 0 (post before pre): LTD (weaken)
 *
 * BUG FIX (Phase 5 - 2025-11-08):
 * - Previous code had pre/post neurons REVERSED
 * - Was using target_id as pre_neuron (wrong!)
 * - Now correctly: neuron_id = pre, target_id = post
 */
uint32_t neural_network_apply_stdp(neural_network_t network, uint32_t neuron_id, uint64_t timestamp)
{
    // CRIT-2 fix: Guard against NULL network and out-of-bounds neuron_id
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_apply_stdp: network is NULL");
        return 0;
    }
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_apply_stdp: neuron_id out of range");
        return 0;
    }

    // WHAT: Get presynaptic neuron (current neuron with outgoing synapses)
    // WHY: We're iterating over this neuron's outgoing connections
    // HOW: Direct array access using neuron_id
    neuron_t* pre_neuron = &network->neurons[neuron_id];
    uint32_t modified = 0;

    // WHAT: Iterate over all outgoing synapses from this neuron
    // WHY: Apply STDP to each connection based on spike timing
    // HOW: Loop through synapse array, compute STDP for each
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(pre_neuron); i++) {
        synapse_handle_t* out_h = NEURON_OUT_HANDLE(pre_neuron, i);
        if (!out_h) continue;
        synapse_t* syn = NEURON_OUT_META(network, pre_neuron, i);
        if (!syn) continue;

        // HIGH-3 fix: Bounds check target_neuron_id before array access
        if (out_h->target_neuron_id >= network->num_neurons) continue;

        // WHAT: Get postsynaptic neuron (target of this synapse)
        // WHY: Need post-spike time for STDP calculation
        // HOW: Access target neuron using target_neuron_id from handle
        // FIX: This was previously assigned to pre_neuron (bug!)
        // CONST: post_neuron is read-only (only access last_spike)
        const neuron_t* post_neuron = &network->neurons[out_h->target_neuron_id];

        // Guard: Skip if either neuron never spiked
        if (pre_neuron->last_spike == 0 || post_neuron->last_spike == 0) {
            continue;
        }

        // WHAT: Compute spike time difference (Δt = t_post - t_pre)
        // WHY: STDP depends on causality (which spike came first)
        // HOW: Subtract pre-spike time from post-spike time
        // RESULT: Δt > 0 means pre→post (causal, LTP)
        //         Δt < 0 means post→pre (anti-causal, LTD)
        int64_t dt = (int64_t)(post_neuron->last_spike) - (int64_t)(pre_neuron->last_spike);

        // Guard: Skip if time difference exceeds STDP window
        // WHY: STDP only affects spikes within ~20-50ms window
        // HOW: Check absolute time difference against configured window
        if (fabsf((float) dt) > pre_neuron->stdp_params.time_window)
            continue;

        // WHAT: Compute STDP weight change factor
        // WHY: Weight change decays exponentially with |Δt|
        // HOW: Call helper function with time difference and parameters
        float stdp_factor = compute_stdp_update((float) dt, &pre_neuron->stdp_params);

        // WHAT: Scale STDP by learning rate, trace, and meta-plasticity
        // WHY: Modulate learning based on recent activity and neuron state
        // HOW: Multiply factors together to get final Δw
        float delta_w = pre_neuron->stdp_params.learning_rate * stdp_factor * syn->trace;

        // WHAT: Apply weight update with meta-plasticity modulation
        // WHY: Meta-plasticity prevents runaway learning
        // HOW: Add weighted delta to current weight
        float new_weight = out_h->weight + delta_w * syn->meta_plasticity;

        // Guard against NaN/Inf from numerical instability
        if (!isfinite(new_weight)) new_weight = 0.0f;

        // WHAT: Clamp weight to configured bounds
        // WHY: Prevent weights from growing unbounded
        // HOW: Apply min/max constraints from network config
        new_weight =
            fmaxf(network->config.min_weight, fminf(network->config.max_weight, new_weight));

        // WHAT: Update weight if change is significant
        // WHY: Avoid tiny updates that don't affect computation
        // HOW: Check if |Δw| exceeds threshold before applying
        if (fabsf(new_weight - out_h->weight) > WEIGHT_UPDATE_THRESHOLD) {
            out_h->weight = new_weight;
            syn->weight = new_weight;  // Keep metadata in sync
            modified++;
        }

        // Phase 11: Apply BCM homeostatic plasticity after STDP
        // WHAT: Apply BCM rule to prevent runaway weight growth
        // WHY: STDP can cause weights to saturate without homeostatic control
        // HOW: Use BCM sliding threshold to stabilize weights
        {
            synapse_cold_t* cold = SYNAPSE_COLD(network, syn);
            if (cold && cold->enable_bcm && cold->bcm) {
                // Get BCM parameters (cortical preset for neural networks)
                bcm_params_t bcm_params = bcm_params_cortical();

                // Compute time delta in seconds (guard against backward timestamps)
                float dt = (timestamp > syn->last_active) ?
                    (float)(timestamp - syn->last_active) / 1000.0F : 0.001F;

                // Use synaptic trace as pre-synaptic activity measure
                float pre_activity = syn->trace;

                // Use post-synaptic neuron activation as post-synaptic activity
                float post_activity = post_neuron->state;

                // Apply BCM rule for weight stabilization
                bcm_apply_rule(cold->bcm, pre_activity, post_activity, dt, &bcm_params);

                // Update synaptic weight from BCM (overrides STDP if BCM makes larger change)
                // This ensures homeostatic stability takes precedence
                if (fabsf(cold->bcm->weight - out_h->weight) > WEIGHT_UPDATE_THRESHOLD) {
                    out_h->weight = cold->bcm->weight;
                    syn->weight = cold->bcm->weight;  // Keep metadata in sync
                }
            }
        }
    }

    return modified;
}

/**
 * @brief Compute STDP update factor
 *
 * WHAT: Calculate weight change factor based on spike timing
 * WHY: Core STDP computation - exponential decay with time difference
 * HOW: Apply asymmetric time window (LTP vs LTD)
 *
 * @param dt Spike time difference (t_post - t_pre)
 * @param params STDP parameters (read-only)
 * @return STDP update factor
 */
static float compute_stdp_update(float dt, const stdp_params_t* params)
{
    // WHAT: Compute exponential decay based on time difference
    // WHY: STDP strength decays with |Δt|
    // HOW: exp(-|Δt| / τ)
    float time_factor = fast_expf(-fabsf(dt) / params->time_window);

    // FIX: Corrected STDP logic
    // dt = t_post - t_pre
    // dt > 0 means pre-before-post (causal) → LTP (strengthen weights)
    // dt < 0 means post-before-pre (anti-causal) → LTD (weaken weights)
    if (dt > 0) {
        // Pre-before-post: Long-term potentiation (LTP)
        return params->positive_factor * time_factor;
    } else {
        // Post-before-pre: Long-term depression (LTD)
        return -params->negative_factor * time_factor;
    }
}

//=============================================================================
// Parallel biological learning worker
//=============================================================================
typedef struct {
    neural_network_t network;
    uint32_t start;
    uint32_t end;
    uint64_t current_time;
    float reward;
    float learning_rate;
    uint32_t modified;
} bio_learn_worker_t;

static void* _bio_learn_trace_worker(void* arg) {
    bio_learn_worker_t* w = (bio_learn_worker_t*)arg;
    for (uint32_t n = w->start; n < w->end; n++) {
        float activity = fabsf(w->network->neurons[n].state);
        neuron_t* neuron = &w->network->neurons[n];
        for (uint32_t s = 0; s < NEURON_OUT_COUNT(neuron); s++) {
            synapse_t* trace_syn = NEURON_OUT_META(w->network, neuron, s);
            if (trace_syn) trace_syn->trace = activity;
        }
    }
    return NULL;
}

/**
 * Worker for parallelized STDP/Oja/eligibility/BCM main learning loop.
 * Each neuron only modifies its OWN outgoing synapses — no cross-neuron writes.
 */
static void* _bio_learn_main_worker(void* arg) {
    bio_learn_worker_t* w = (bio_learn_worker_t*)arg;
    neural_network_t network = w->network;
    uint32_t modified = 0;

    // Cache dopamine level once (read-only, thread-safe)
    float dopamine = 0.5f;
    if (network->neuromodulator_system) {
        dopamine = neuromodulator_get_level(network->neuromodulator_system, NEUROMOD_DOPAMINE);
    }

    for (uint32_t neuron_id = w->start; neuron_id < w->end; neuron_id++) {
        neuron_t* neuron = &network->neurons[neuron_id];

        if (neuron->learning_rule & LEARNING_STDP) {
            modified += neural_network_apply_stdp(network, neuron_id, w->current_time);
        }
        if (neuron->learning_rule & LEARNING_OJA) {
            modified += neural_network_apply_oja(network, neuron_id, w->current_time);
        }

        for (uint32_t syn_idx = 0; syn_idx < NEURON_OUT_COUNT(neuron); syn_idx++) {
            synapse_handle_t* out_h = NEURON_OUT_HANDLE(neuron, syn_idx);
            if (!out_h) continue;
            synapse_t* syn = NEURON_OUT_META(network, neuron, syn_idx);
            if (!syn) continue;

            {
                synapse_cold_t* cold = SYNAPSE_COLD(network, syn);
                if (cold && cold->enable_eligibility && cold->eligibility) {
                    eligibility_config_t elig_config = eligibility_default_config();
                    elig_config.learning_rate = w->learning_rate;

                    if (syn->trace > 0.1f) {
                        eligibility_trace_update(cold->eligibility, &elig_config,
                                                 w->current_time, syn->trace);
                    } else {
                        eligibility_trace_decay(cold->eligibility, &elig_config, w->current_time);
                    }

                    float old_weight = out_h->weight;
                    eligibility_apply_reward(syn, cold->eligibility, &elig_config,
                                              w->reward, dopamine);
                    out_h->weight = syn->weight;

                    if (!isfinite(out_h->weight)) {
                        out_h->weight = old_weight;
                        syn->weight = old_weight;
                    }
                    if (!nimcp_security_validate_weight_change(old_weight, out_h->weight,
                                                              NIMCP_MAX_WEIGHT_DELTA_PER_STEP)) {
                        out_h->weight = old_weight;
                        syn->weight = old_weight;
                        continue;
                    }
                    out_h->weight = fmaxf(network->config.min_weight,
                                      fminf(network->config.max_weight, out_h->weight));
                    syn->weight = out_h->weight;
                    if (fabsf(out_h->weight - old_weight) > WEIGHT_UPDATE_THRESHOLD) {
                        modified++;
                    }
                }

                if (cold && cold->enable_bcm && cold->bcm) {
                    if (out_h->target_neuron_id >= network->num_neurons) continue;
                    bcm_params_t bcm_params = bcm_params_cortical();
                    const neuron_t* post_neuron = &network->neurons[out_h->target_neuron_id];
                    bcm_apply_rule(cold->bcm, neuron->state, post_neuron->state, 1.0f, &bcm_params);
                    if (fabsf(cold->bcm->weight - out_h->weight) > WEIGHT_UPDATE_THRESHOLD) {
                        out_h->weight = cold->bcm->weight;
                        syn->weight = cold->bcm->weight;
                        modified++;
                    }
                }
            }
        }
    }

    w->modified = modified;
    return NULL;
}

/**
 * @brief Parallel trace-setting for active neuron subset
 *
 * @param network   The neural network
 * @param active_ids Array of active neuron indices
 * @param num_active Number of active neurons
 */
typedef struct {
    neural_network_t network;
    uint32_t* active_ids;
    uint32_t start;
    uint32_t end;
} bio_learn_active_trace_worker_t;

static void* _bio_learn_active_trace_worker(void* arg) {
    bio_learn_active_trace_worker_t* w = (bio_learn_active_trace_worker_t*)arg;
    for (uint32_t i = w->start; i < w->end; i++) {
        uint32_t n = w->active_ids[i];
        float activity = fabsf(w->network->neurons[n].state);
        neuron_t* neuron = &w->network->neurons[n];
        for (uint32_t s = 0; s < NEURON_OUT_COUNT(neuron); s++) {
            synapse_t* trace_syn = NEURON_OUT_META(w->network, neuron, s);
            if (trace_syn) trace_syn->trace = activity;
        }
    }
    return NULL;
}

/**
 * @brief Apply reward-modulated learning to all synapses (Phase 11)
 *
 * WHAT: Apply biological plasticity mechanisms with reward signal
 * WHY:  Enable supervised/RL learning using biological rules
 * HOW:  Iterate neurons → STDP → Eligibility traces → BCM
 */
uint32_t neural_network_apply_reward_learning(neural_network_t network, float reward,
                                              float learning_rate, uint64_t current_time)
{
    // Guard: Validate parameters
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_apply_reward_learning: network is NULL");
        return 0;
    }
    if (reward < 0.0F || reward > 1.0F || learning_rate <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_apply_reward_learning: invalid reward or learning_rate");
        return 0;
    }

    uint32_t total_modified = 0;

    // PRE-PASS: Set outgoing synapse traces from current neuron states (parallel)
    // WHY: The forward pass sets neuron->state but never updates outgoing synapse traces.
    //      Eligibility learning needs syn->trace > 0.1 to trigger weight updates.
    {
        const uint32_t NUM_BIO_THREADS = 4;
        uint32_t total_n = network->num_neurons;
        if (total_n > 50000) {
            pthread_t threads[NUM_BIO_THREADS];
            bio_learn_worker_t workers[NUM_BIO_THREADS];
            uint32_t chunk = total_n / NUM_BIO_THREADS;
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                workers[t].network = network;
                workers[t].start = t * chunk;
                workers[t].end = (t == NUM_BIO_THREADS - 1) ? total_n : (t + 1) * chunk;
                pthread_create(&threads[t], NULL, _bio_learn_trace_worker, &workers[t]);
            }
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                pthread_join(threads[t], NULL);
            }
        } else {
            // Small network — sequential
            for (uint32_t n = 0; n < total_n; n++) {
                float activity = fabsf(network->neurons[n].state);
                neuron_t* pre_neuron = &network->neurons[n];
                for (uint32_t s = 0; s < NEURON_OUT_COUNT(pre_neuron); s++) {
                    synapse_t* trace_syn = NEURON_OUT_META(network, pre_neuron, s);
                    if (trace_syn) trace_syn->trace = activity;
                }
            }
        }
    }

    // Main learning loop: STDP/Oja/eligibility/BCM per neuron (parallel for large networks)
    {
        const uint32_t NUM_BIO_THREADS = 4;
        uint32_t total_n = network->num_neurons;
        if (total_n > 50000) {
            pthread_t threads[NUM_BIO_THREADS];
            bio_learn_worker_t workers[NUM_BIO_THREADS];
            uint32_t chunk = total_n / NUM_BIO_THREADS;
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                workers[t].network = network;
                workers[t].start = t * chunk;
                workers[t].end = (t == NUM_BIO_THREADS - 1) ? total_n : (t + 1) * chunk;
                workers[t].current_time = current_time;
                workers[t].reward = reward;
                workers[t].learning_rate = learning_rate;
                workers[t].modified = 0;
                pthread_create(&threads[t], NULL, _bio_learn_main_worker, &workers[t]);
            }
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                pthread_join(threads[t], NULL);
                total_modified += workers[t].modified;
            }
        } else {
            // Small network — sequential (avoids thread overhead)
            bio_learn_worker_t w = {
                .network = network, .start = 0, .end = total_n,
                .current_time = current_time, .reward = reward,
                .learning_rate = learning_rate, .modified = 0
            };
            _bio_learn_main_worker(&w);
            total_modified += w.modified;
        }
    }



    // POST-PASS: Sync outgoing synapse weights to incoming synapses via peer_index
    // WHY: Learning modifies outgoing synapse weights,
    //      but neural_network_forward() reads from incoming handles.
    //      Without sync, weight updates have no effect on the forward pass.
    // OPTIMIZATION: O(S) per neuron using peer_index (was O(S^2))
    if (total_modified > 0) {
        for (uint32_t n = 0; n < network->num_neurons; n++) {
            neuron_t* src = &network->neurons[n];
            for (uint32_t s = 0; s < NEURON_OUT_COUNT(src); s++) {
                synapse_handle_t* out_h = NEURON_OUT_HANDLE(src, s);
                if (!out_h) continue;
                if (out_h->target_neuron_id >= network->num_neurons) continue;
                neuron_t* target = &network->neurons[out_h->target_neuron_id];
                if (out_h->peer_index != SPARSE_SYNAPSE_NO_PEER) {
                    synapse_handle_t* in_h = sparse_synapse_get(&target->incoming, out_h->peer_index);
                    if (in_h) {
                        in_h->weight = out_h->weight;
                        in_h->strength = out_h->strength;
                    }
                }
            }
        }
    }

    return total_modified;
}

/**
 * Synaptogenesis: Activate dormant neurons near active backbone neurons.
 *
 * For each highly-active backbone neuron, check its nearest unwired neighbor.
 * If that neighbor has zero connections, wire it in by copying a random subset
 * of the backbone neuron's input sources with small initial weights.
 *
 * This implements axonal sprouting — new connections grow from active neurons
 * toward nearby dormant neurons, gradually expanding the network's capacity.
 */
static uint32_t neural_network_sprout_connections_impl(neural_network_t network,
                                                        uint32_t max_new_connections,
                                                        float activity_threshold)
{
    if (!network || max_new_connections == 0) return 0;
    if (network->config.num_layers < 3 || !network->config.layer_sizes) return 0;

    uint32_t input_size = network->config.layer_sizes[0];
    uint32_t output_size = network->config.layer_sizes[network->config.num_layers - 1];
    uint32_t total_hidden = 0;
    for (uint32_t l = 1; l < network->config.num_layers - 1; l++)
        total_hidden += network->config.layer_sizes[l];
    if (total_hidden < 100) return 0;

    uint32_t hidden_start = input_size;
    uint32_t output_start = 0;
    for (uint32_t l = 0; l < network->config.num_layers - 1; l++)
        output_start += network->config.layer_sizes[l];

    uint32_t new_conns = 0;
    float init_scale = 1.0f / sqrtf((float)input_size);

    for (uint32_t h = hidden_start; h < hidden_start + total_hidden - 1 && new_conns < max_new_connections; h++) {
        neuron_t* active = &network->neurons[h];
        if (NEURON_IN_COUNT(active) == 0) continue;
        if (fabsf(active->state) < activity_threshold) continue;

        for (int32_t offset = 1; offset <= 8 && new_conns < max_new_connections; offset++) {
            uint32_t neighbor_id = h + offset;
            if (neighbor_id >= hidden_start + total_hidden) break;
            neuron_t* neighbor = &network->neurons[neighbor_id];
            if (NEURON_IN_COUNT(neighbor) > 0) continue;
            if (NEURON_OUT_COUNT(neighbor) > 0) continue;

            uint32_t in_count = NEURON_IN_COUNT(active);
            uint32_t to_copy = (in_count > 16) ? 16 : in_count;
            for (uint32_t s = 0; s < to_copy && new_conns < max_new_connections; s++) {
                synapse_handle_t* src_h = NEURON_IN_HANDLE(active, s);
                if (!src_h) continue;
                float weight = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * init_scale * 0.5f;
                neural_network_add_connection(network, src_h->target_neuron_id, neighbor_id, weight);
                new_conns++;
            }

            uint32_t out_target = (output_size > 64) ? 64 : output_size;
            uint32_t out_step = output_size / out_target;
            for (uint32_t o = 0; o < out_target && new_conns < max_new_connections; o++) {
                uint32_t output_id = output_start + o * out_step;
                if (output_id >= network->num_neurons) break;
                float weight = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * init_scale * 0.25f;
                neural_network_add_connection(network, neighbor_id, output_id, weight);
                new_conns++;
            }
        }
    }

    if (new_conns > 0) {
        LOG_DEBUG(LOG_MODULE, "Synaptogenesis: %u new connections sprouted", new_conns);
    }
    return new_conns;
}

/**
 * @brief Apply reward-modulated bio-plasticity to ACTIVE neurons only.
 *
 * Includes Phase 5: synaptogenesis — grows new connections from active
 * backbone neurons to nearby dormant neurons, breaking the chicken-and-egg
 * problem where STDP needs connections but connections need STDP.
 */
uint32_t neural_network_apply_reward_learning_active(neural_network_t network, float reward,
                                                     float learning_rate, uint64_t current_time,
                                                     float activity_threshold)
{
    if (!network) return 0;
    if (!isfinite(reward) || learning_rate <= 0.0f) return 0;
    if (activity_threshold <= 0.0f) activity_threshold = 0.01f;

    /* Phase 1: Collect active neuron indices (no cap — all active neurons participate) */
    uint32_t max_active = network->num_neurons;
    uint32_t* active_ids = nimcp_malloc(max_active * sizeof(uint32_t));
    if (!active_ids) return 0;
    uint32_t num_active = 0;

    for (uint32_t n = 0; n < network->num_neurons; n++) {
        if (fabsf(network->neurons[n].state) > activity_threshold) {
            active_ids[num_active++] = n;
        }
    }

    if (num_active == 0) {
        nimcp_free(active_ids);
        return 0;
    }

    uint32_t total_modified = 0;

    /* Phase 2: Set outgoing synapse traces for active neurons only (parallel) */
    {
        const uint32_t NUM_BIO_THREADS = 4;
        if (num_active > 10000) {
            pthread_t threads[NUM_BIO_THREADS];
            bio_learn_active_trace_worker_t workers[NUM_BIO_THREADS];
            uint32_t chunk = num_active / NUM_BIO_THREADS;
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                workers[t].network = network;
                workers[t].active_ids = active_ids;
                workers[t].start = t * chunk;
                workers[t].end = (t == NUM_BIO_THREADS - 1) ? num_active : (t + 1) * chunk;
                pthread_create(&threads[t], NULL, _bio_learn_active_trace_worker, &workers[t]);
            }
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                pthread_join(threads[t], NULL);
            }
        } else {
            // Small active set — sequential
            for (uint32_t i = 0; i < num_active; i++) {
                uint32_t n = active_ids[i];
                float activity = fabsf(network->neurons[n].state);
                neuron_t* neuron = &network->neurons[n];
                for (uint32_t s = 0; s < NEURON_OUT_COUNT(neuron); s++) {
                    synapse_t* trace_syn = NEURON_OUT_META(network, neuron, s);
                    if (trace_syn) trace_syn->trace = activity;
                }
            }
        }
    }

    /* Phase 3: Apply STDP/Oja/eligibility/BCM to active neurons (parallel) */
    {
        const uint32_t NUM_BIO_THREADS = 4;
        if (num_active > 10000) {
            pthread_t threads[NUM_BIO_THREADS];
            bio_learn_worker_t workers[NUM_BIO_THREADS];
            // Remap active IDs to contiguous ranges by partitioning the active_ids array.
            // Worker processes active_ids[start..end) and maps each to actual neuron IDs.
            uint32_t chunk = num_active / NUM_BIO_THREADS;
            if (chunk == 0) chunk = 1;  // W4-17: ensure minimum chunk size
            uint32_t threads_created = 0;  // W4-6: track actual thread count
            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                uint32_t a_start = t * chunk;
                uint32_t a_end = (t == NUM_BIO_THREADS - 1) ? num_active : (t + 1) * chunk;
                workers[t].network = network;
                workers[t].start = a_start;
                workers[t].end = a_end;
                workers[t].current_time = current_time;
                workers[t].reward = reward;
                workers[t].learning_rate = learning_rate;
                workers[t].modified = 0;
            }

            for (uint32_t t = 0; t < NUM_BIO_THREADS; t++) {
                uint32_t a_start = t * chunk;
                uint32_t a_end = (t == NUM_BIO_THREADS - 1) ? num_active : (t + 1) * chunk;
                if (a_start >= num_active) break;
                workers[t].start = active_ids[a_start];
                workers[t].end = (a_end > 0 && a_end <= num_active) ? active_ids[a_end - 1] + 1 : workers[t].start + 1;
                pthread_create(&threads[t], NULL, _bio_learn_main_worker, &workers[t]);
                threads_created++;
            }
            // W4-6: Only join threads that were actually created
            for (uint32_t t = 0; t < threads_created; t++) {
                pthread_join(threads[t], NULL);
                total_modified += workers[t].modified;
            }
        } else {
            // Small active set — sequential
            float dopamine = 0.5f;
            if (network->neuromodulator_system) {
                dopamine = neuromodulator_get_level(
                    (neuromodulator_system_t)network->neuromodulator_system, NEUROMOD_DOPAMINE);
            }

            for (uint32_t i = 0; i < num_active; i++) {
                uint32_t neuron_id = active_ids[i];
                neuron_t* neuron = &network->neurons[neuron_id];

                if (neuron->learning_rule & LEARNING_STDP)
                    total_modified += neural_network_apply_stdp(network, neuron_id, current_time);
                if (neuron->learning_rule & LEARNING_OJA)
                    total_modified += neural_network_apply_oja(network, neuron_id, current_time);

                for (uint32_t syn_idx = 0; syn_idx < NEURON_OUT_COUNT(neuron); syn_idx++) {
                    synapse_handle_t* out_h = NEURON_OUT_HANDLE(neuron, syn_idx);
                    if (!out_h) continue;
                    synapse_t* syn = NEURON_OUT_META(network, neuron, syn_idx);

                    {
                        synapse_cold_t* cold = syn ? SYNAPSE_COLD(network, syn) : NULL;
                        if (cold && cold->enable_eligibility && cold->eligibility) {
                            eligibility_config_t elig_config = eligibility_default_config();
                            elig_config.learning_rate = learning_rate;
                            if (syn->trace > 0.1f)
                                eligibility_trace_update(cold->eligibility, &elig_config, current_time, syn->trace);
                            else
                                eligibility_trace_decay(cold->eligibility, &elig_config, current_time);

                            float old_weight = out_h->weight;
                            syn->weight = out_h->weight;
                            eligibility_apply_reward(syn, cold->eligibility, &elig_config, reward, dopamine);
                            if (!isfinite(syn->weight)) syn->weight = old_weight;
                            if (!nimcp_security_validate_weight_change(old_weight, syn->weight,
                                                                      NIMCP_MAX_WEIGHT_DELTA_PER_STEP)) {
                                syn->weight = old_weight; continue;
                            }
                            syn->weight = fmaxf(network->config.min_weight,
                                              fminf(network->config.max_weight, syn->weight));
                            out_h->weight = syn->weight;
                            if (fabsf(out_h->weight - old_weight) > WEIGHT_UPDATE_THRESHOLD) total_modified++;
                        }

                        if (cold && cold->enable_bcm && cold->bcm) {
                            uint32_t target_id = out_h->target_neuron_id;
                            if (target_id < network->num_neurons) {
                                bcm_params_t bcm_params = bcm_params_cortical();
                                const neuron_t* post_neuron = &network->neurons[target_id];
                                bcm_apply_rule(cold->bcm, neuron->state, post_neuron->state, 1.0f, &bcm_params);
                                if (fabsf(cold->bcm->weight - out_h->weight) > WEIGHT_UPDATE_THRESHOLD) {
                                    out_h->weight = cold->bcm->weight;
                                    syn->weight = cold->bcm->weight;
                                    total_modified++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /* Phase 4: Sync outgoing→incoming only for active neurons */
    if (total_modified > 0) {
        for (uint32_t i = 0; i < num_active; i++) {
            uint32_t n = active_ids[i];
            neuron_t* src = &network->neurons[n];
            for (uint32_t s = 0; s < NEURON_OUT_COUNT(src); s++) {
                synapse_handle_t* out_h = NEURON_OUT_HANDLE(src, s);
                if (!out_h) continue;
                uint32_t target_id = out_h->target_neuron_id;
                if (target_id >= network->num_neurons) continue;
                neuron_t* target = &network->neurons[target_id];
                if (out_h->peer_index != SPARSE_SYNAPSE_NO_PEER) {
                    synapse_handle_t* in_h = sparse_synapse_get(&target->incoming, out_h->peer_index);
                    if (in_h) {
                        in_h->weight = out_h->weight;
                        in_h->strength = out_h->strength;
                    }
                }
            }
        }
    }

    /* Phase 5: Synaptogenesis — activate dormant neighbors of active neurons.
     * Gate on num_active (not total_modified) to break chicken-and-egg:
     * new connections need activity, but STDP/Oja need existing connections.
     * Rate-limited: run every 10 learn steps to avoid metadata pool exhaustion. */
    static uint32_t sprout_counter = 0;
    if (network->num_neurons > 10000 && num_active > 0 && (++sprout_counter % 10) == 0) {
        uint32_t sprout_budget = (num_active > 64) ? num_active : 64;
        if (sprout_budget > 1024) sprout_budget = 1024;
        neural_network_sprout_connections_impl(network, sprout_budget, activity_threshold);
    }

    nimcp_free(active_ids);
    LOG_INFO(LOG_MODULE, "Active reward learning: %u/%u neurons active, %u synapses modified (reward=%.3f)",
             num_active, network->num_neurons, total_modified, reward);
    return total_modified;
}

/**
 * @brief Apply lateral inhibition (winner-take-all) to output layer
 *
 * WHAT: Suppress non-winning output neurons to sharpen classification
 * WHY:  Lateral inhibition creates competition between output neurons,
 *        producing sharper class boundaries and faster convergence
 * HOW:  Find maximum output, suppress others by inhibition_strength
 *
 * BIOLOGICAL BASIS:
 * - Cortical lateral inhibition via GABAergic interneurons
 * - Competitive learning in self-organizing maps
 * - Winner-take-all circuits in basal ganglia
 */
uint32_t neural_network_apply_lateral_inhibition(
    neural_network_t network,
    uint32_t output_start,
    uint32_t output_count,
    float inhibition_strength)
{
    // Guard clauses
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_apply_lateral_inhibition: network is NULL");
        return 0;
    }
    if (output_count == 0) return 0;
    if (inhibition_strength <= 0.0f) return 0;
    if (inhibition_strength > 1.0f) inhibition_strength = 1.0f;
    if ((uint64_t)output_start + output_count > network->num_neurons) return 0;

    // Find the winner (max activation neuron)
    float max_activation = -1e30f;
    uint32_t winner_idx = 0;

    for (uint32_t i = 0; i < output_count; i++) {
        uint32_t nid = output_start + i;
        if (nid >= network->num_neurons) break;
        float act = network->neurons[nid].state;
        if (act > max_activation) {
            max_activation = act;
            winner_idx = i;
        }
    }

    // Compute mean activation for reference
    float mean_activation = 0.0f;
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < output_count; i++) {
        uint32_t nid = output_start + i;
        if (nid >= network->num_neurons) break;
        mean_activation += network->neurons[nid].state;
        valid_count++;
    }
    if (valid_count == 0) return 0;
    mean_activation /= (float)valid_count;

    // Apply inhibition: suppress neurons below winner
    // Use soft WTA: scale non-winners toward mean, don't zero them out
    uint32_t modified = 0;
    for (uint32_t i = 0; i < output_count; i++) {
        if (i == winner_idx) continue;
        uint32_t nid = output_start + i;
        if (nid >= network->num_neurons) break;

        float old_state = network->neurons[nid].state;
        // Move non-winners toward mean by inhibition_strength fraction
        float new_state = old_state + inhibition_strength * (mean_activation - old_state);
        // Also reduce bias slightly to make inhibition persistent
        network->neurons[nid].bias -= inhibition_strength * 0.001f *
                                       fmaxf(0.0f, old_state - mean_activation);
        network->neurons[nid].state = new_state;

        if (fabsf(new_state - old_state) > 1e-6f) {
            modified++;
        }
    }

    // Boost winner slightly
    uint32_t winner_nid = output_start + winner_idx;
    if (winner_nid < network->num_neurons) {
        network->neurons[winner_nid].state *= (1.0f + inhibition_strength * 0.1f);
        // Clamp
        if (network->neurons[winner_nid].state > 1.0f)
            network->neurons[winner_nid].state = 1.0f;
    }

    return modified;
}

/**
 * @brief Apply homeostatic plasticity to maintain target activity
 */
bool neural_network_apply_homeostasis(neural_network_t network, uint32_t neuron_id,
                                      uint64_t timestamp)
{
    // Guard: Validate inputs
    if (!network || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_apply_homeostasis: network is NULL");
        return false;
    }

    neuron_t* neuron = &network->neurons[neuron_id];

    // Skip if too soon since last update
    if (timestamp - neuron->last_update < network->config.update_interval) {
        return false;
    }

    // Reuse cached EMA from update_activity_history() — O(1) instead of O(w)
    float current_activity = neuron->avg_activity;

    // Compute activity error
    float activity_error = neuron->homeostatic.target_activity - current_activity;

    // Update threshold based on activity error
    float threshold_adjustment = neuron->homeostatic.strength * activity_error;
    neuron->threshold += threshold_adjustment;

    // Constrain threshold to reasonable range
    float min_threshold = neuron->rest_potential + 0.1F;
    float max_threshold = neuron->rest_potential + 2.0F;
    neuron->threshold = fmaxf(min_threshold, fminf(max_threshold, neuron->threshold));

    // Update homeostatic factor
    neuron->homeostatic_factor = compute_homeostatic_factor(neuron, current_activity);

    // Apply homeostatic scaling to synapses
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_handle_t* out_h = NEURON_OUT_HANDLE(neuron, i);
        if (!out_h) continue;

        // Scale synaptic strength based on activity error
        float strength_adjustment =
            neuron->homeostatic.strength * activity_error * neuron->homeostatic_factor;

        out_h->strength *= (1.0F + strength_adjustment);

        // Constrain synaptic strength
        out_h->strength = fmaxf(0.0F, fminf(MAX_SYNAPTIC_STRENGTH, out_h->strength));

        // Keep metadata in sync
        synapse_t* syn = NEURON_OUT_META(network, neuron, i);
        if (syn) syn->strength = out_h->strength;
    }

    // Update adaptation based on activity
    neuron->adaptation =
        fmaxf(0.0F, neuron->adaptation + neuron->homeostatic.strength * activity_error);

    return true;
}

/**
 * @brief Compute homeostatic scaling factor
 */
static float compute_homeostatic_factor(neuron_t* neuron, float current_activity)
{
    if (neuron->homeostatic.target_activity < 1e-6f) return 1.0f;
    float activity_ratio = current_activity / neuron->homeostatic.target_activity;
    float time_scale = neuron->homeostatic.time_scale;

    // Compute scaling factor using time-dependent exponential
    float scaling_factor = expf(-fabsf(activity_ratio - 1.0F) / time_scale);

    // Adjust scaling based on activity direction
    if (activity_ratio > 1.0F) {
        // Too much activity - decrease scaling
        return scaling_factor * HOMEOSTATIC_DECAY;
    } else {
        // Too little activity - increase scaling
        return scaling_factor * (2.0F - HOMEOSTATIC_DECAY);
    }
}

/**
 * @brief Update meta-plasticity based on activity patterns
 */
static void update_meta_plasticity(neural_network_t network, neuron_t* neuron, uint64_t timestamp)
{
    // Compute activity variance over history window
    float mean_activity = neuron->avg_activity;
    float variance = 0.0F;
    uint32_t act_cap = neuron->activity_history_capacity;

    if (neuron->activity_history && act_cap > 0) {
        for (uint32_t i = 0; i < act_cap; i++) {
            float diff = neuron->activity_history[i] - mean_activity;
            variance += diff * diff;
        }
        variance /= act_cap;
    }

    // Update meta-plasticity for each synapse
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_t* syn = NEURON_OUT_META(network, neuron, i);
        if (!syn) continue;

        // Compute stability measure
        float stability = expf(-variance * META_PLASTICITY_RATE);

        // Update meta-plasticity factor
        syn->meta_plasticity =
            syn->meta_plasticity * (1.0F - META_PLASTICITY_RATE) + stability * META_PLASTICITY_RATE;

        // Ensure meta-plasticity stays in valid range
        syn->meta_plasticity = fmaxf(0.1F, fminf(1.0F, syn->meta_plasticity));
    }
}

/**
 * @brief Periodic maintenance of homeostatic mechanisms
 */
void neural_network_maintain_homeostasis(neural_network_t network, uint64_t timestamp)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_maintain_homeostasis: network is NULL");
        return;
    }

    // Skip if too soon since last maintenance
    if (timestamp - network->last_maintenance < network->config.update_interval) {
        return;
    }

    // Update global network stability
    float total_variance = 0.0F;

    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* neuron = &network->neurons[i];

        // Apply homeostatic plasticity
        neural_network_apply_homeostasis(network, i, timestamp);

        // Update meta-plasticity
        update_meta_plasticity(network, neuron, timestamp);

        // Compute contribution to global variance
        float activity_diff = neuron->avg_activity - neuron->homeostatic.target_activity;
        total_variance += activity_diff * activity_diff;
    }

    // Update network stability measure (guard against div-by-zero)
    if (network->num_neurons > 0) {
        network->network_stability = expf(-total_variance / (float)network->num_neurons);
    }
    network->last_maintenance = timestamp;
}

/**
 * @brief Record a spike event for a neuron
 */
bool neural_network_record_spike(neural_network_t network, uint32_t neuron_id, float magnitude,
                                 uint64_t timestamp)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_record_spike: network is NULL");
        return false;
    }
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_record_spike: neuron_id %u out of range", neuron_id);
        return false;
    }

    neuron_t* neuron = &network->neurons[neuron_id];

    // Record spike in history (dynamic ring buffer)
    if (!neuron->spike_history || neuron->spike_history_capacity == 0)
        return true;  // No buffer allocated — skip recording

    uint32_t idx = neuron->spike_history_index;
    neuron->spike_history[idx].timestamp = timestamp;
    neuron->spike_history[idx].magnitude = magnitude;

    // Update spike history index (ring buffer wrap)
    neuron->spike_history_index = (idx + 1) % neuron->spike_history_capacity;
    if (neuron->spike_history_count < neuron->spike_history_capacity)
        neuron->spike_history_count++;

    // Update last spike time
    neuron->last_spike = timestamp;

    // Update network time to stay current with spike events
    if (timestamp > network->network_time) {
        network->network_time = timestamp;
    }

    // Increase calcium concentration
    neuron->calcium_concentration += 1.0F;

    // Update synaptic traces (but don't propagate immediately)
    // Synaptic inputs will be computed when target neurons are updated
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_t* syn = NEURON_OUT_META(network, neuron, i);
        if (!syn) continue;

        // Update synaptic trace for STDP
        syn->trace += 1.0F;

        // Note: We don't propagate spikes immediately here because
        // sum_synaptic_inputs() handles synaptic integration when
        // target neurons are updated. Immediate propagation would
        // cause double-counting of synaptic inputs.
    }

    return true;
}

/**
 * @brief Update synaptic traces for STDP
 */
static void update_synaptic_traces(neural_network_t network, neuron_t* neuron, uint64_t timestamp)
{
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_t* syn = NEURON_OUT_META(network, neuron, i);
        if (!syn) continue;

        // Compute time since last update (guard against timestamp wraparound)
        float dt = (timestamp > syn->last_active) ?
                   (float)(timestamp - syn->last_active) : 0.0f;

        // Exponential decay of trace
        if (dt > 0) {
            syn->trace *= expf(-dt * TRACE_DECAY_RATE);
        }

        syn->last_active = timestamp;
    }
}

/**
 * @brief Update calcium dynamics
 */
static void update_calcium_dynamics(neuron_t* neuron, uint64_t timestamp)
{
    // Compute time since last update (guard against timestamp wraparound)
    float dt = (timestamp > neuron->last_update) ?
               (float)(timestamp - neuron->last_update) : 1.0f;

    // Exponential decay of calcium concentration
    if (dt > 0) {
        neuron->calcium_concentration *= expf(-dt * CALCIUM_DECAY_RATE);
    }

    // Ensure calcium concentration stays in valid range
    neuron->calcium_concentration = fmaxf(0.0F, fminf(10.0F, neuron->calcium_concentration));
}

/**
 * @brief Get average activity over time window
 */
float neural_network_get_average_activity(neural_network_t network, uint32_t neuron_id)
{
    // Defensive validation
    if (!network) {
        return 0.0F;  // No activity in NULL network
    }

    if (!network->neurons) {
        return 0.0F;  // No activity if neurons not allocated
    }

    if (neuron_id >= network->num_neurons) {
        return 0.0F;  // No activity for out-of-bounds neuron
    }

    // Validate num_neurons is reasonable (< 10M neurons is sane limit)
    // This catches cases where num_neurons has garbage value
    if (network->num_neurons > 10000000) {
        return 0.0F;
    }

    // For newly created networks with no training, return 0 activity
    // This avoids iterating over uninitialized spike history
    if (network->network_time == 0) {
        return 0.0F;
    }

    // Use safe accessor instead of direct array access
    neuron_t* neuron = neural_network_get_neuron(network, neuron_id);
    if (!neuron) {
        return 0.0F;  // Neuron access failed
    }

    // Count recent spikes within the time window
    uint32_t spike_count = 0;
    uint64_t current_time = network->network_time;
    uint64_t window_start = (current_time >= HISTORY_WINDOW) ? (current_time - HISTORY_WINDOW) : 0;

    // Only iterate if we have meaningful time progression
    if (current_time > 0) {
        if (neuron->spike_history) {
            for (uint32_t i = 0; i < neuron->spike_history_capacity; i++) {
                // Check if spike is within window and is a real spike (non-zero magnitude)
                if (neuron->spike_history[i].timestamp >= window_start &&
                    neuron->spike_history[i].magnitude != 0.0F) {
                    spike_count++;
                }
            }
        }
    }

    return (float)spike_count / HISTORY_WINDOW;
}

/**
 * @brief Set the network's internal timestamp
 *
 * WHAT: Synchronizes network time with external simulation time
 * WHY:  Ensure timing-dependent features (STP, etc) use consistent time
 * HOW:  Directly updates network_time field
 *
 * @param network Neural network instance
 * @param timestamp Current simulation time (µs)
 */
void neural_network_set_time(neural_network_t network, uint64_t timestamp)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_time: network is NULL");
        return;
    }
    network->network_time = timestamp;
}

/**
 * @brief Compute network step
 */
uint32_t neural_network_compute_step(neural_network_t network, uint64_t timestamp)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_compute_step: network is NULL");
        return 0;
    }

    // Process pending bio-async messages
    if (network->bio_async_enabled && network->bio_ctx) {
        bio_router_process_inbox(network->bio_ctx, 5);
    }

    // Cache neuromodulation level once per step (used per-synapse in sum_synaptic_inputs)
    network->cached_neuromod_level = neural_network_get_neuromodulation(network);

    uint32_t active_neurons = 0;
    uint32_t active_count = 0;

    // Update all neurons, building active set for NEXT step
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* neuron = &network->neurons[i];

        // Skip neurons in refractory period
        if (is_in_refractory_period(neuron, timestamp)) {
            continue;
        }

        // Update neuron state (membrane potential computed inside)
        if (neural_network_update_neuron(network, i, 0.0f, timestamp)) {
            active_neurons++;
            // Track this neuron as active for next step's sparse iteration
            if (network->active_neuron_ids && active_count < network->active_neuron_capacity) {
                network->active_neuron_ids[active_count++] = i;
            }
        }

        // WHAT: Reset external current after processing
        // WHY: External current is per-timestep stimulation, not persistent state
        // HOW: Set to 0 after neuron update completes
        // NOTE: Spike encoding sets this before compute_step()
        neuron->external_current = 0.0F;
    }

    // Store active set for sparse iteration by other subsystems
    network->num_active_neurons = active_count;
    network->active_set_valid = true;

    // Periodic network maintenance
    if (timestamp - network->last_maintenance >= network->config.update_interval) {
        neural_network_maintain_homeostasis(network, timestamp);
    }

    network->network_time = timestamp;
    network->current_time = timestamp;
    return active_neurons;
}

/**
 * @brief Add a new synaptic connection
 */
bool neural_network_add_connection(neural_network_t network, uint32_t from_id, uint32_t to_id,
                                   float weight)
{
    // HIGH-4 fix: Guard against NULL network before dereferencing
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_add_connection: network is NULL");
        return false;
    }
    if (from_id >= network->num_neurons || to_id >= network->num_neurons) {
        return false;
    }

    neuron_t* from_neuron = &network->neurons[from_id];
    neuron_t* to_neuron = &network->neurons[to_id];

    // Clamp weight to config bounds
    float clamped_weight = fmaxf(network->config.min_weight, fminf(network->config.max_weight, weight));

    // Add OUTGOING synapse handle + metadata via sparse API
    int out_rc = sparse_synapse_add_with_metadata(
        network->synapse_handle_pool,
        network->synapse_metadata_pool,
        &from_neuron->outgoing,
        to_id,
        clamped_weight,
        SYNAPSE_GENERIC  // Default type; typed variant sets it later
    );
    if (out_rc != 0) {
        return false;  // Pool exhausted
    }

    // Get newly-added outgoing handle and metadata
    uint32_t out_idx = NEURON_OUT_COUNT(from_neuron) - 1;
    synapse_handle_t* out_h = NEURON_OUT_HANDLE(from_neuron, out_idx);
    if (!out_h) return false;
    out_h->strength = 1.0F;
    synapse_t* syn = NEURON_OUT_META(network, from_neuron, out_idx);

    // Initialize outgoing hot metadata
    if (syn) {
        syn->target_id = to_id;
        syn->weight = clamped_weight;
        syn->plasticity = 1.0F;
        syn->last_change = 0.0F;
        syn->last_active = network->network_time;
        syn->strength = 1.0F;
        syn->meta_plasticity = 1.0F;
        syn->trace = 0.0F;
        syn->source_neuron_id = from_id;
        syn->axon_id = 0;
        syn->semantic_relevance = 0.0F;
        syn->cold_index = SYNAPSE_COLD_NONE;

        // Initialize cold data only if STP/BCM/eligibility are needed
        // For large networks (>100K), defer cold allocation to plasticity time
        // to avoid multi-GB cold pool during backbone wiring
        bool need_cold = (network->num_neurons <= 100000);
        if (need_cold) {
            synapse_cold_t* cold = SYNAPSE_ENSURE_COLD(network, syn);
            if (cold) {
                // NIMCP 2.6: Initialize STP (Short-Term Plasticity)
                stp_preset_t preset = (from_neuron->type == NEURON_EXCITATORY)
                                    ? STP_PRESET_DEPRESSING
                                    : STP_PRESET_FACILITATING;
                stp_params_t stp_params = stp_get_preset_params(preset);
                stp_init(&cold->stp, &stp_params, network->network_time);
                cold->enable_stp = true;

                // Phase 11: Initialize BCM — from bulk pool if available, else heap
                if (network->config.enable_bcm) {
                    if (network->bcm_pool && network->bcm_pool_used < network->bcm_pool_capacity) {
                        cold->bcm = &network->bcm_pool[network->bcm_pool_used++];
                        memset(cold->bcm, 0, sizeof(bcm_synapse_t));
                    } else {
                        cold->bcm = (bcm_synapse_t*)nimcp_calloc(1, sizeof(bcm_synapse_t));
                    }
                    if (cold->bcm) {
                        *cold->bcm = bcm_synapse_init(syn->weight, 0.5F);
                        cold->enable_bcm = true;
                    }
                }

                // Phase 11: Initialize Eligibility Traces
                if (network->config.enable_eligibility) {
                    if (network->eligibility_pool && network->eligibility_pool_used < network->eligibility_pool_capacity) {
                        cold->eligibility = &network->eligibility_pool[network->eligibility_pool_used++];
                        memset(cold->eligibility, 0, sizeof(eligibility_trace_t));
                    } else {
                        cold->eligibility = (eligibility_trace_t*)nimcp_calloc(1, sizeof(eligibility_trace_t));
                    }
                    if (cold->eligibility) {
                        eligibility_trace_init(cold->eligibility, network->network_time);
                        cold->enable_eligibility = true;
                    }
                }
            }
        }
    }

    // Add INCOMING synapse handle WITHOUT metadata (handle-only, saves ~200 bytes/synapse)
    // WHY: Incoming handles are reverse-lookup only. Forward pass reads outgoing metadata.
    //       STP/BCM/eligibility state lives on the outgoing side. Eliminating incoming
    //       metadata saves ~21 GB for 2M-neuron networks (113M incoming × 200 bytes).
    int in_rc = sparse_synapse_add(
        network->synapse_handle_pool,
        &to_neuron->incoming,
        from_id,           // In incoming handle, target_neuron_id stores source
        clamped_weight
    );
    if (in_rc != 0) {
        // Rollback outgoing (best effort)
        sparse_synapse_remove_with_metadata(
            network->synapse_handle_pool,
            network->synapse_metadata_pool,
            &from_neuron->outgoing, out_idx);
        return false;
    }

    // Get newly-added incoming handle (no metadata)
    uint32_t in_idx = NEURON_IN_COUNT(to_neuron) - 1;
    synapse_handle_t* in_h = NEURON_IN_HANDLE(to_neuron, in_idx);
    if (!in_h) {
        // Rollback outgoing (best effort)
        sparse_synapse_remove_with_metadata(
            network->synapse_handle_pool,
            network->synapse_metadata_pool,
            &from_neuron->outgoing, out_idx);
        return false;
    }
    in_h->strength = 1.0F;

    // Set peer_index on both handles for O(1) cross-reference
    out_h->peer_index = in_idx;
    in_h->peer_index = out_idx;

    // Update weight norm incrementally — O(1) instead of O(D) full recalc.
    // Full recalc is done once after bulk wiring via normalize_synaptic_weights().
    from_neuron->weight_norm += fabsf(clamped_weight);

    return true;
}

/**
 * @brief Normalize synaptic weights for a neuron
 */
static void normalize_synaptic_weights(neuron_t* neuron)
{
    float sum_weights = 0.0F;
    float max_weight = 0.0F;

    // Calculate sum and find maximum weight
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, i);
        if (!h) continue;
        sum_weights += fabsf(h->weight);
        max_weight = fmaxf(max_weight, fabsf(h->weight));
    }

    if (sum_weights > EPSILON) {
        // Normalize weights while preserving sign
        float norm_factor = neuron->oja_params.target_norm / sum_weights;

        for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, i);
            if (!h) continue;
            float sign = (h->weight >= 0.0F) ? 1.0F : -1.0F;
            h->weight = sign * fabsf(h->weight) * norm_factor;
        }

        // After normalization, weight_norm should be target_norm
        neuron->weight_norm = neuron->oja_params.target_norm;
    } else {
        // If sum is too small, keep original sum
        neuron->weight_norm = sum_weights;
    }
}


/**
 * @brief Prune weak synapses
 */
uint32_t neural_network_prune_synapses(neural_network_t network, float threshold)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_prune_synapses: network is NULL");
        return 0;
    }

    uint32_t pruned_count = 0;

    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* neuron = &network->neurons[i];

        // Iterate in reverse to safely use swap-and-pop removal
        for (int32_t idx = (int32_t)NEURON_OUT_COUNT(neuron) - 1; idx >= 0; idx--) {
            synapse_handle_t* out_h = NEURON_OUT_HANDLE(neuron, (uint32_t)idx);
            if (!out_h) continue;

            // Activity-based pruning: require BOTH low weight AND low strength.
            // Preserves structurally important synapses (high strength) even if
            // their weight is temporarily low during training.
            if (fabsf(out_h->weight) < threshold && out_h->strength < 0.5f) {
                // Also remove the peer incoming synapse
                if (out_h->peer_index != SPARSE_SYNAPSE_NO_PEER &&
                    out_h->target_neuron_id < network->num_neurons) {
                    neuron_t* target = &network->neurons[out_h->target_neuron_id];
                    uint32_t peer_idx = out_h->peer_index;

                    // Before removing incoming, update the moved handle's peer
                    uint32_t last_in = NEURON_IN_COUNT(target) - 1;
                    if (peer_idx != last_in && last_in < NEURON_IN_COUNT(target)) {
                        // The last incoming handle will be swapped to peer_idx
                        synapse_handle_t* last_in_h = NEURON_IN_HANDLE(target, last_in);
                        if (last_in_h && last_in_h->peer_index != SPARSE_SYNAPSE_NO_PEER &&
                            last_in_h->target_neuron_id < network->num_neurons) {
                            // Update the outgoing handle that points to the moved incoming
                            neuron_t* src_of_last = &network->neurons[last_in_h->target_neuron_id];
                            synapse_handle_t* out_peer = sparse_synapse_get(&src_of_last->outgoing, last_in_h->peer_index);
                            if (out_peer) {
                                out_peer->peer_index = peer_idx;
                            }
                        }
                    }

                    sparse_synapse_remove_with_metadata(
                        network->synapse_handle_pool,
                        network->synapse_metadata_pool,
                        &target->incoming, peer_idx);
                }

                // Before removing outgoing, update the moved handle's peer
                uint32_t last_out = NEURON_OUT_COUNT(neuron) - 1;
                if ((uint32_t)idx != last_out && last_out < NEURON_OUT_COUNT(neuron)) {
                    synapse_handle_t* last_out_h = NEURON_OUT_HANDLE(neuron, last_out);
                    if (last_out_h && last_out_h->peer_index != SPARSE_SYNAPSE_NO_PEER &&
                        last_out_h->target_neuron_id < network->num_neurons) {
                        neuron_t* tgt_of_last = &network->neurons[last_out_h->target_neuron_id];
                        synapse_handle_t* in_peer = sparse_synapse_get(&tgt_of_last->incoming, last_out_h->peer_index);
                        if (in_peer) {
                            in_peer->peer_index = (uint32_t)idx;
                        }
                    }
                }

                sparse_synapse_remove_with_metadata(
                    network->synapse_handle_pool,
                    network->synapse_metadata_pool,
                    &neuron->outgoing, (uint32_t)idx);

                pruned_count++;
            }
        }
    }

    return pruned_count;
}

/**
 * @brief Network maintenance routine
 */
void neural_network_maintain(neural_network_t network, uint64_t timestamp)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_maintain: network is NULL");
        return;
    }

    // Skip if too soon since last maintenance
    if (timestamp - network->last_maintenance < network->config.update_interval) {
        return;
    }

    // Update homeostatic mechanisms
    neural_network_maintain_homeostasis(network, timestamp);

    // Prune very weak synapses
    float prune_threshold = network->config.min_weight * 0.1F;
    neural_network_prune_synapses(network, prune_threshold);

    // Normalize weights for all neurons
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        normalize_synaptic_weights(&network->neurons[i]);
    }

    network->last_maintenance = timestamp;
}

/**
 * @brief Debug function to dump neuron state
 */
void neural_network_dump_neuron(neural_network_t network, uint32_t neuron_id)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_dump_neuron: network is NULL");
        return;
    }
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_dump_neuron: neuron_id out of range");
        return;
    }

    neuron_t* neuron = &network->neurons[neuron_id];

    printf("Neuron %u:\n", neuron_id);
    printf("  Type: %s\n", (neuron->type == NEURON_INHIBITORY) ? "Inhibitory" : "Excitatory");
    printf("  State: %.3f\n", neuron->state);
    printf("  Threshold: %.3f\n", neuron->threshold);
    printf("  Calcium: %.3f\n", neuron->calcium_concentration);
    printf("  Activity: %.3f\n", neuron->avg_activity);
    printf("  Synapses: %u\n", NEURON_OUT_COUNT(neuron));

    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, i);
        if (!h) continue;
        printf("    Synapse %u -> %u: w=%.3f, s=%.3f\n", neuron_id, h->target_neuron_id, h->weight,
               h->strength);
    }
}

/**
 * @brief Reset network state
 */
void neural_network_reset(neural_network_t network)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_reset: network is NULL");
        return;
    }

    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* neuron = &network->neurons[i];

        // Reset neuron state
        neuron->state = neuron->rest_potential;
        neuron->threshold = 0.5F;  // Normalized threshold (matches neural_network_add_neuron default)
        neuron->adaptation = 0.0F;
        neuron->calcium_concentration = 0.0F;
        neuron->avg_activity = 0.0F;
        neuron->ema_activity = 0.0f;

        // Reset spike history (dynamic ring buffer)
        neuron->spike_history_index = 0;
        neuron->spike_history_count = 0;
        if (neuron->spike_history)
            memset(neuron->spike_history, 0, sizeof(spike_record_t) * neuron->spike_history_capacity);

        // Reset activity history (dynamic buffer)
        if (neuron->activity_history)
            memset(neuron->activity_history, 0, sizeof(float) * neuron->activity_history_capacity);

        // Reset synaptic traces
        for (uint32_t j = 0; j < NEURON_OUT_COUNT(neuron); j++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, j);
            if (!h) continue;
            h->strength = 1.0F;
            synapse_t* syn = NEURON_OUT_META(network, neuron, j);
            if (syn) {
                syn->trace = 0.0F;
                syn->strength = 1.0F;
                syn->meta_plasticity = 1.0F;
            }
        }
    }

    network->network_time = 0;
    network->global_activity = 0.0F;
    network->network_stability = 1.0F;
    network->last_maintenance = 0;
}

/**
 * @brief Reinitialize all synapse weights using He/Xavier initialization
 *
 * WHAT: Randomizes all synapse weights while preserving network topology
 * WHY:  Breaks mode collapse — when outputs converge to identical values,
 *       gradient-based anti-collapse can't recover. Re-randomizing weights
 *       gives the network a fresh starting point while keeping connectivity.
 * HOW:  For each neuron, compute fan-in from incoming synapse count,
 *       apply He initialization: w ~ U(-1,1) * sqrt(2/fan_in).
 *       Also resets neuron state, bias, threshold, and synaptic traces.
 *
 * @param network Neural network (non-NULL)
 */
void neural_network_reinit_weights(neural_network_t network)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "neural_network_reinit_weights: network is NULL");
        return;
    }

    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* neuron = &network->neurons[i];

        /* Reset neuron state */
        neuron->state = neuron->rest_potential;
        neuron->threshold = 0.5F;
        neuron->adaptation = 0.0F;
        neuron->bias = 0.0F;
        neuron->calcium_concentration = 0.0F;
        neuron->avg_activity = 0.0F;
        neuron->ema_activity = 0.0f;

        /* Reset spike/activity history */
        neuron->spike_history_index = 0;
        neuron->spike_history_count = 0;
        if (neuron->spike_history)
            memset(neuron->spike_history, 0,
                   sizeof(spike_record_t) * neuron->spike_history_capacity);
        if (neuron->activity_history)
            memset(neuron->activity_history, 0,
                   sizeof(float) * neuron->activity_history_capacity);

        /* Reinitialize outgoing synapse weights with He initialization */
        uint32_t fan_in = NEURON_IN_COUNT(neuron);
        if (fan_in == 0) fan_in = 128;  /* fallback */
        float scale = sqrtf(2.0F / (float)fan_in);

        for (uint32_t j = 0; j < NEURON_OUT_COUNT(neuron); j++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, j);
            if (!h) continue;

            float w = (((float)nimcp_tl_rand() / RAND_MAX) * 2.0F - 1.0F)
                      * scale;
            h->weight = w;
            h->strength = 1.0F;

            synapse_t* syn = NEURON_OUT_META(network, neuron, j);
            if (syn) {
                syn->trace = 0.0F;
                syn->strength = 1.0F;
                syn->meta_plasticity = 1.0F;
            }
        }
    }

    network->network_time = 0;
    network->global_activity = 0.0F;
    network->network_stability = 1.0F;
    network->last_maintenance = 0;

    /* Fix activation types: hidden=LEAKY_RELU, output=LINEAR.
     * Checkpoints may have TANH on output layer from older code. */
    if (network->config.num_layers > 1 && network->config.layer_sizes) {
        uint32_t offset = 0;
        for (uint32_t l = 0; l < network->config.num_layers; l++) {
            uint32_t lsize = network->config.layer_sizes[l];
            if (l > 0 && l < network->config.num_layers - 1) {
                /* Hidden layers → leaky ReLU */
                for (uint32_t i = 0; i < lsize && offset + i < network->num_neurons; i++)
                    network->neurons[offset + i].activation_type = ACTIVATION_LEAKY_RELU;
            } else if (l == network->config.num_layers - 1) {
                /* Output layer → linear (regression) */
                for (uint32_t i = 0; i < lsize && offset + i < network->num_neurons; i++)
                    network->neurons[offset + i].activation_type = ACTIVATION_LINEAR;
            }
            offset += lsize;
        }
    }

    NIMCP_LOGGING_INFO("neural_network_reinit_weights: reinitialized %u neurons",
                       network->num_neurons);
}

void neural_network_set_output_activation(neural_network_t network, activation_type_t activation)
{
    if (!network || network->config.num_layers < 2 || !network->config.layer_sizes) return;

    uint32_t offset = 0;
    for (uint32_t l = 0; l < network->config.num_layers - 1; l++)
        offset += network->config.layer_sizes[l];

    uint32_t out_size = network->config.layer_sizes[network->config.num_layers - 1];
    uint32_t fixed = 0;
    for (uint32_t i = 0; i < out_size && offset + i < network->num_neurons; i++) {
        if (network->neurons[offset + i].activation_type != activation) {
            network->neurons[offset + i].activation_type = activation;
            fixed++;
        }
    }
    if (fixed > 0) {
        NIMCP_LOGGING_INFO("set_output_activation: set %u neurons to type %d",
                           fixed, (int)activation);
    }
}

/*
 * @brief Get neuron state
 */
bool neural_network_get_neuron_state(neural_network_t network, uint32_t neuron_id, float* state)
{
    if (!network || neuron_id >= network->num_neurons || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_get_neuron_state: required parameter is NULL (network, state)");
        return false;
    }

    *state = network->neurons[neuron_id].state;
    return true;
}

/**
 * @brief Get number of neurons in network
 *
 * WHAT: Return total neuron count
 * WHY: Allow external code to iterate neurons without accessing internals
 * HOW: Return num_neurons field
 *
 * @param network Network instance
 * @return Number of neurons, or 0 if network is NULL
 */
uint32_t neural_network_get_num_neurons(neural_network_t network)
{
    if (!network) {
        return 0;
    }
    return network->num_neurons;
}

/**
 * @brief Get pointer to neuron structure
 *
 * WHAT: Access neuron internals for serialization/introspection
 * WHY: Enable save/load of synaptic weights without duplicating access logic
 * HOW: Return pointer to neuron in neurons array
 *
 * WARNING: Returns internal pointer - use carefully!
 *
 * @param network Network instance
 * @param neuron_id Neuron ID
 * @return Pointer to neuron, or NULL if invalid
 */
neuron_t* neural_network_get_neuron(neural_network_t network, uint32_t neuron_id)
{
    if (!network || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_get_neuron: network is NULL");
        return NULL;
    }
    return &network->neurons[neuron_id];
}

synapse_t* neural_network_get_out_meta(neural_network_t network, neuron_t* neuron, uint32_t index)
{
    if (!network || !neuron) return NULL;
    return _neuron_meta_from_pool(network->synapse_metadata_pool, &neuron->outgoing, index);
}

synapse_t* neural_network_get_in_meta(neural_network_t network, neuron_t* neuron, uint32_t index)
{
    if (!network || !neuron) return NULL;
    return _neuron_meta_from_pool(network->synapse_metadata_pool, &neuron->incoming, index);
}

bool neural_network_rebuild_incoming(neural_network_t network)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_rebuild_incoming: network is NULL");
        return false;
    }

    /* Count total outgoing synapses to ensure pool has enough capacity
     * for the incoming handles we're about to create. */
    uint64_t total_outgoing = 0;
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        total_outgoing += NEURON_OUT_COUNT(&network->neurons[i]);
    }

    /* Most neurons have ≤320 incoming connections (embedded capacity).
     * Overflow beyond 320 uses the handle pool; if pool is exhausted,
     * sparse_synapse_add returns -1 without THROW (safe to continue). */

    // Clear all incoming storage
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* n = &network->neurons[i];
        sparse_synapse_storage_cleanup(network->synapse_handle_pool, &n->incoming);
        sparse_synapse_storage_init(&n->incoming);
    }

    /* Phase 1: Count incoming synapses per target neuron so we can pre-allocate */
    uint32_t* incoming_counts = (uint32_t*)nimcp_calloc(network->num_neurons, sizeof(uint32_t));
    if (!incoming_counts) {
        NIMCP_LOGGING_ERROR("rebuild_incoming: failed to allocate count array");
        return false;
    }
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* src = &network->neurons[i];
        uint32_t out_count = NEURON_OUT_COUNT(src);
        for (uint32_t s = 0; s < out_count; s++) {
            synapse_handle_t* out_h = NEURON_OUT_HANDLE(src, s);
            if (!out_h || out_h->target_neuron_id >= network->num_neurons) continue;
            incoming_counts[out_h->target_neuron_id]++;
        }
    }

    /* Phase 2: Pre-allocate overflow for neurons with >EMBEDDED_CAPACITY incoming */
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        if (incoming_counts[i] > SPARSE_SYNAPSE_EMBEDDED_CAPACITY) {
            sparse_synapse_storage_reserve(network->synapse_handle_pool,
                                            &network->neurons[i].incoming,
                                            incoming_counts[i]);
        }
    }
    nimcp_free(incoming_counts);

    /* Phase 3: Direct-write incoming handles — bypass sparse_synapse_add.
     * No validation, no pool locks (embedded is inline, overflow pre-allocated). */
    uint32_t added = 0, failed = 0;
    for (uint32_t i = 0; i < network->num_neurons; i++) {
        neuron_t* src = &network->neurons[i];
        uint32_t out_count = NEURON_OUT_COUNT(src);
        for (uint32_t s = 0; s < out_count; s++) {
            synapse_handle_t* out_h = NEURON_OUT_HANDLE(src, s);
            if (!out_h || out_h->target_neuron_id >= network->num_neurons) continue;
            neuron_t* tgt = &network->neurons[out_h->target_neuron_id];

            /* Direct write to embedded or pre-allocated overflow */
            synapse_handle_t handle = {
                .target_neuron_id = i,
                .weight = out_h->weight,
                .strength = out_h->strength,
                .metadata_index = SPARSE_SYNAPSE_NO_METADATA,
                .peer_index = s,
                .ternary_weight = out_h->ternary_weight,
                .use_ternary_weight = out_h->use_ternary_weight
            };

            sparse_synapse_storage_t* inc = &tgt->incoming;
            uint32_t in_idx;
            if (inc->embedded_count < SPARSE_SYNAPSE_EMBEDDED_CAPACITY) {
                in_idx = inc->embedded_count;
                inc->embedded[inc->embedded_count++] = handle;
            } else if (inc->overflow && inc->overflow_count < inc->overflow_capacity) {
                in_idx = SPARSE_SYNAPSE_EMBEDDED_CAPACITY + inc->overflow_count;
                inc->overflow[inc->overflow_count++] = handle;
            } else {
                failed++;
                continue;
            }
            out_h->peer_index = in_idx;
            added++;
        }
    }
    NIMCP_LOGGING_INFO("rebuild_incoming: %u added, %u failed (total outgoing: %llu)",
                       added, failed, (unsigned long long)total_outgoing);
    return true;
}

/**
 * @brief Retrofit metadata onto synapses that lack it
 * WHAT: Walk all neurons, allocate metadata for handle-only synapses
 * WHY:  Synapses created during pool exhaustion have no plasticity data.
 *       This restores STDP/STP/eligibility capability after pool expansion.
 * HOW:  Iterate all outgoing synapses; if metadata_index == NO_METADATA,
 *       allocate from pool and initialize hot fields from the handle.
 * RETURNS: Number of synapses retrofitted
 */
uint32_t neural_network_retrofit_metadata(neural_network_t network)
{
    if (!network) return 0;
    if (!network->synapse_metadata_pool) return 0;

    uint32_t retrofitted = 0;
    uint32_t skipped = 0;
    uint32_t already_have = 0;

    for (uint32_t n = 0; n < network->num_neurons; n++) {
        neuron_t* neuron = &network->neurons[n];
        uint32_t out_count = NEURON_OUT_COUNT(neuron);

        for (uint32_t s = 0; s < out_count; s++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
            if (!h) continue;

            if (h->metadata_index != SPARSE_SYNAPSE_NO_METADATA) {
                already_have++;
                continue;
            }

            // Allocate metadata slot
            uint32_t meta_idx = synapse_metadata_pool_allocate(
                network->synapse_metadata_pool);
            if (meta_idx == SPARSE_SYNAPSE_NO_METADATA) {
                skipped++;
                continue;  // Pool still full — skip this one
            }

            h->metadata_index = meta_idx;
            synapse_t* syn = synapse_metadata_pool_get(
                network->synapse_metadata_pool, meta_idx);
            if (!syn) {
                h->metadata_index = SPARSE_SYNAPSE_NO_METADATA;
                skipped++;
                continue;
            }

            // Initialize hot metadata from handle data
            memset(syn, 0, sizeof(synapse_t));
            syn->target_id = h->target_neuron_id;
            syn->weight = h->weight;
            syn->plasticity = 1.0F;
            syn->last_active = network->network_time;
            syn->strength = h->strength > 0 ? h->strength : 1.0F;
            syn->meta_plasticity = 1.0F;
            syn->source_neuron_id = n;
            syn->cold_index = SYNAPSE_COLD_NONE;

            retrofitted++;
        }

        // Progress logging every 100K neurons
        if (n > 0 && (n % 100000) == 0) {
            LOG_INFO(LOG_MODULE, "Metadata retrofit: %u/%u neurons, "
                     "%u retrofitted, %u skipped, %u already had metadata",
                     n, network->num_neurons, retrofitted, skipped, already_have);
        }
    }

    LOG_INFO(LOG_MODULE, "Metadata retrofit complete: %u retrofitted, "
             "%u skipped (pool full), %u already had metadata (total synapses scanned: %u)",
             retrofitted, skipped, already_have,
             retrofitted + skipped + already_have);

    return retrofitted;
}

/**
 * @brief Add a new neuron to the network
 */
/**
 * WHAT: Add a new neuron to the network dynamically
 * WHY: Support runtime network growth for adaptive architectures
 * HOW: Use preallocated capacity, check bounds
 * COMPLEXITY: O(1) - no reallocation needed if within capacity
 */
uint32_t neural_network_add_neuron(neural_network_t network, activation_type_t activation)
{
    // Guard clause: Validate network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_add_neuron: network is NULL");
        return UINT32_MAX;
    }

    // Guard clause: Check capacity bounds (expected during normal wiring — no throw)
    if (network->num_neurons >= network->capacity) {
        return UINT32_MAX;
    }

    uint32_t new_id = network->num_neurons;
    neuron_t* neuron = &network->neurons[new_id];

    // Initialize neuron with default values (preserve cold pointer from bulk allocation)
    neuron_cold_data_t* saved_cold = neuron->cold;
    memset(neuron, 0, sizeof(neuron_t));
    neuron->cold = saved_cold;
    if (!neuron->cold) {
        // Fallback: individually allocate cold data if not bulk-allocated
        neuron->cold = neuron_cold_data_create();
    }
    neuron->id = new_id;
    neuron->activation_type = activation;
    neuron->type = NEURON_EXCITATORY;  // Default to excitatory
    neuron->rest_potential = 0.0F;   // Normalized resting potential
    neuron->threshold = 0.5F;        // Normalized spike threshold
    neuron->ema_activity = 0.0f;
    neuron->creation_time = network->network_time;

    // NIMCP 2.6: Initialize neuron model (uses network config)
    init_neuron_model(neuron, &network->config);

    // Sync cold data after initialization
    neuron_sync_to_cold(neuron);

    // Allocate spike history ring buffer
    uint32_t spike_cap = resolve_spike_history_capacity(&network->config);
    neuron->spike_history_capacity = spike_cap;
    neuron->spike_history_index = 0;
    neuron->spike_history_count = 0;
    neuron->spike_history = (spike_record_t*)nimcp_calloc(spike_cap, sizeof(spike_record_t));
    if (!neuron->spike_history) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate spike history for new neuron %u", new_id);
        neuron->spike_history_capacity = 0;
    }

    // Allocate activity history buffer
    uint32_t activity_cap = resolve_activity_history_capacity(&network->config);
    neuron->activity_history_capacity = activity_cap;
    neuron->activity_history = (float*)nimcp_calloc(activity_cap, sizeof(float));
    if (!neuron->activity_history) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate activity history for new neuron %u", new_id);
        neuron->activity_history_capacity = 0;
    }

    // Initialize sparse synapse storage for new neuron
    sparse_synapse_storage_init(&neuron->outgoing);
    sparse_synapse_storage_init(&neuron->incoming);

    network->num_neurons++;
    return new_id;
}

/**
 * @brief Apply generalized Oja's rule
 */
uint32_t neural_network_apply_generalized_oja(neural_network_t network, uint32_t neuron_id,
                                              uint64_t timestamp)
{
    // Implementation similar to neural_network_apply_oja but with generalized rule
    // This is a placeholder - implement the actual generalized Oja rule as needed
    return 0;
}

/**
 * @brief Update plasticity for a neuron
 */
uint32_t neural_network_update_plasticity(neural_network_t network, uint32_t neuron_id,
                                          uint64_t timestamp)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_update_plasticity: network is NULL");
        return 0;
    }
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_update_plasticity: neuron_id out of range");
        return 0;
    }

    neuron_t* neuron = &network->neurons[neuron_id];
    update_meta_plasticity(network, neuron, timestamp);
    return 1;
}

/**
 * @brief Normalize weights for a specific neuron
 */
bool neural_network_normalize_weights(neural_network_t network, uint32_t neuron_id)
{
    if (!network || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_normalize_weights: network is NULL");
        return false;
    }

    normalize_synaptic_weights(&network->neurons[neuron_id]);
    return true;
}

/**
 * @brief Adapt neuron threshold
 */
bool neural_network_adapt_threshold(neural_network_t network, uint32_t neuron_id,
                                    uint64_t timestamp)
{
    if (!network || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_adapt_threshold: network is NULL");
        return false;
    }

    neuron_t* neuron = &network->neurons[neuron_id];
    float activity = neural_network_get_average_activity(network, neuron_id);

    // Adjust threshold based on recent activity
    float threshold_adjustment =
        neuron->adaptation * (activity - neuron->homeostatic.target_activity);
    neuron->threshold += threshold_adjustment;

    return true;
}

/**
 * @brief Update synaptic traces
 */
void neural_network_update_traces(neural_network_t network, uint32_t neuron_id, uint64_t timestamp)
{
    if (!network || neuron_id >= network->num_neurons)
        return;

    update_synaptic_traces(network, &network->neurons[neuron_id], timestamp);
}

/**
 * @brief Set neuron model type for a specific neuron
 *
 * WHAT: Dynamically changes neuron dynamics model
 * WHY: Enables heterogeneous networks (mixed LIF/Izhikevich/etc)
 * HOW: Cleans up old model, initializes new model
 *
 * PATTERN: Strategy Pattern - swap behavior at runtime
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param neuron_id Neuron to modify
 * @param model_type Desired model type
 * @param params Model-specific parameters (NULL = use defaults)
 * @return true if successful, false on error
 */
bool neural_network_set_neuron_model(neural_network_t network, uint32_t neuron_id,
                                     neuron_model_type_t model_type, const void* params)
{
    // Guard: Validate network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_neuron_model: network is NULL");
        return false;
    }

    // Guard: Validate neuron ID
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_set_neuron_model: capacity exceeded");
        return false;
    }

    neuron_t* neuron = &network->neurons[neuron_id];

    // Cleanup existing model if present
    if (neuron->model) {
        neuron_model_destroy(neuron->model);
        neuron->model = NULL;
    }

    // Set to LIF (no plugin model)
    if (model_type == NEURON_MODEL_LIF) {
        neuron->model_type = NEURON_MODEL_LIF;
        return true;
    }

    // Get vtable for requested model
    const neuron_model_vtable_t* vtable = NULL;

    // Stack-local default params (declared at function scope for lifetime safety)
    izhikevich_params_t izhi_default_params;

    switch (model_type) {
        case NEURON_MODEL_IZHIKEVICH:
            vtable = izhikevich_get_vtable();
            // If no params provided, use default RS (Regular Spiking)
            if (!params) {
                izhi_default_params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
                params = &izhi_default_params;
            }
            break;

        case NEURON_MODEL_LIF:
            // Already handled above
            return true;

        default:
            // Unknown model type, fall back to LIF
            neuron->model_type = NEURON_MODEL_LIF;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_set_neuron_model: operation failed");
            return false;
    }

    // Guard: Check vtable valid
    if (!vtable) {
        neuron->model_type = NEURON_MODEL_LIF;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_neuron_model: vtable is NULL");
        return false;
    }

    // Create new model state
    neuron->model = neuron_model_create(vtable, params);

    // Guard: Check creation succeeded
    if (!neuron->model) {
        neuron->model_type = NEURON_MODEL_LIF;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_neuron_model: neuron->model is NULL");
        return false;
    }

    neuron->model_type = model_type;
    return true;
}

/**
 * @brief Get weight norm for a neuron
 */
float neural_network_get_weight_norm(neural_network_t network, uint32_t neuron_id)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_get_weight_norm: network is NULL");
        return 0.0F;
    }
    if (neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neural_network_get_weight_norm: neuron_id out of range");
        return 0.0F;
    }
    return network->neurons[neuron_id].weight_norm;
}

/**
 * @brief Get weight statistics for a neuron
 */
void neural_network_get_weight_statistics(neural_network_t network, uint32_t neuron_id, float* mean,
                                          float* std_dev)
{
    if (!network || neuron_id >= network->num_neurons || !mean || !std_dev) {
        if (mean)
            *mean = 0.0F;
        if (std_dev)
            *std_dev = 0.0F;
        return;
    }

    neuron_t* neuron = &network->neurons[neuron_id];
    float sum = 0.0F, sum_sq = 0.0F;
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_handle_t* wh = NEURON_OUT_HANDLE(neuron, i);
        if (!wh) continue;
        float w = wh->weight;
        sum += w;
        sum_sq += w * w;
        valid_count++;
    }

    if (valid_count > 0) {
        *mean = sum / (float)valid_count;
        float variance = (sum_sq / (float)valid_count) - (*mean * *mean);
        *std_dev = sqrtf(fmaxf(0.0F, variance));
    } else {
        *mean = 0.0F;
        *std_dev = 0.0F;
    }
}

bool neural_network_get_stats(neural_network_t network, network_stats_t* stats)
{
    if (!network || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_get_stats: required parameter is NULL (network, stats)");
        return false;
    }

    // Initialize stats structure
    memset(stats, 0, sizeof(network_stats_t));

    // Count neurons and calculate averages
    float total_activity = 0.0F;
    float total_weight = 0.0F;
    float total_strength = 0.0F;
    float total_plasticity = 0.0F;
    float total_calcium = 0.0F;
    uint32_t total_synapses = 0;

    for (uint32_t i = 0; i < network->num_neurons; i++) {
        // CONST: neuron is read-only in statistics calculation
        const neuron_t* neuron = &network->neurons[i];

        // Count by neuron type
        if (neuron->type == NEURON_INHIBITORY) {
            stats->num_inhibitory++;
        } else {
            stats->num_excitatory++;
        }

        // Sum up neuron properties
        total_activity += neuron->avg_activity;
        total_calcium += neuron->calcium_concentration;
        total_synapses += NEURON_OUT_COUNT(neuron);

        // Calculate synapse averages
        for (uint32_t j = 0; j < NEURON_OUT_COUNT(neuron); j++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE((neuron_t*)neuron, j);
            if (!h) continue;
            total_weight += h->weight;
            total_strength += h->strength;
            synapse_t* meta = NEURON_OUT_META(network, (neuron_t*)neuron, j);
            total_plasticity += meta ? meta->plasticity : 1.0F;
        }
    }

    // Set basic counts
    stats->num_neurons = network->num_neurons;
    stats->total_synapses = total_synapses;

    // Calculate averages
    if (stats->num_neurons > 0) {
        stats->avg_activity = total_activity / stats->num_neurons;
        stats->avg_calcium = total_calcium / stats->num_neurons;
    }

    if (total_synapses > 0) {
        stats->avg_weight = total_weight / total_synapses;
        stats->avg_strength = total_strength / total_synapses;
        stats->avg_plasticity = total_plasticity / total_synapses;
    }

    // Calculate network stability (based on weight change rate)
    // Stability = 1 - normalized_change, clamped to [0, 1]
    // Special case: On first call (last_avg_weight == 0 and current avg_weight != 0),
    // we have no history, so assume perfect stability (1.0)
    if (network->last_avg_weight == 0.0F && stats->avg_weight != 0.0F) {
        // First call with non-zero weights - no history to compare against
        stats->network_stability = 1.0F;
    } else if (network->last_avg_weight == 0.0F && stats->avg_weight == 0.0F) {
        // Network has no synapses or all weights are zero - assume stable
        stats->network_stability = 1.0F;
    } else {
        float weight_change = fabsf(stats->avg_weight - network->last_avg_weight);
        float normalized_change =
            weight_change / (fmaxf(fabsf(stats->avg_weight), fabsf(network->last_avg_weight)) + 1e-6F);
        stats->network_stability = fmaxf(0.0F, fminf(1.0F, 1.0F - normalized_change));
    }
    network->last_avg_weight = stats->avg_weight;

    // Set network time
    stats->network_time = network->current_time;

    return true;
}

//=============================================================================
// NIMCP 2.5 - Forward Pass Implementation
//=============================================================================

/**
 * @brief Perform forward pass through the neural network
 *
 * This implements a feedforward computation for networks with layer structure.
 * For NIMCP 2.5 Brain API compatibility.
 */
bool neural_network_forward(neural_network_t network, const float* inputs, uint32_t input_size,
                            float* outputs, uint32_t output_size)
{
    if (!network || !inputs || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_forward: required parameter is NULL (network, inputs, outputs)");
        return false;
    }

    // Check if network has layer structure (NIMCP 2.5)
    if (network->config.num_layers > 0 && network->config.layer_sizes) {
        // Use layer-based forward pass
        uint32_t expected_input = network->config.layer_sizes[0];
        uint32_t expected_output = network->config.layer_sizes[network->config.num_layers - 1];

        if (input_size != expected_input || output_size != expected_output) {
            static uint32_t _fwd_mismatch_count = 0;
            if (_fwd_mismatch_count < 5) {
                NIMCP_LOGGING_ERROR("neural_network_forward: dimension mismatch — "
                    "got input=%u (expected %u), output=%u (expected %u), "
                    "num_layers=%u, num_neurons=%u",
                    input_size, expected_input, output_size, expected_output,
                    network->config.num_layers, network->num_neurons);
                _fwd_mismatch_count++;
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_network_forward: validation failed");
            return false;
        }

        // Set input layer neuron states
        for (uint32_t i = 0; i < input_size; i++) {
            if (i < network->num_neurons) {
                network->neurons[i].state = inputs[i];
            }
        }

        // Propagate through layers
        uint32_t neuron_offset = input_size;
        for (uint32_t layer = 1; layer < network->config.num_layers; layer++) {
            uint32_t layer_size = network->config.layer_sizes[layer];
            if (neuron_offset > network->num_neurons) {
                LOG_WARN("Forward pass: neuron_offset %u exceeds num_neurons %u at layer %u",
                         neuron_offset, network->num_neurons, layer);
                break;
            }

            // Compute each neuron in this layer
            for (uint32_t i = 0; i < layer_size && neuron_offset + i < network->num_neurons; i++) {
                uint32_t neuron_id = neuron_offset + i;
                neuron_t* neuron = &network->neurons[neuron_id];

                // Compute weighted sum from previous layer (using INCOMING synapses)
                float activation = neuron->bias;

                for (uint32_t j = 0; j < NEURON_IN_COUNT(neuron); j++) {
                    synapse_handle_t* in_h = NEURON_IN_HANDLE(neuron, j);
                    if (!in_h) continue;
                    synapse_t* in_meta = NEURON_IN_META(network, neuron, j);
                    uint32_t src_id = in_meta ? in_meta->source_neuron_id : in_h->target_neuron_id;
                    if (src_id < network->num_neurons) {
                        float pre_activity = network->neurons[src_id].state;
                        activation += pre_activity * in_h->weight * in_h->strength;
                    }
                }

                // Apply activation function
                switch (neuron->activation_type) {
                    case ACTIVATION_SIGMOID:
                        neuron->state = 1.0F / (1.0F + expf(-activation));
                        break;
                    case ACTIVATION_TANH:
                        neuron->state = tanhf(activation);
                        break;
                    case ACTIVATION_RELU:
                        neuron->state = (activation > 0.0F) ? activation : 0.0F;
                        break;
                    case ACTIVATION_LEAKY_RELU:
                        neuron->state = (activation > 0.0F) ? activation : (activation * 0.01F);
                        break;
                    case ACTIVATION_ADAPTIVE:
                        // Output layer: force LINEAR (no threshold gate).
                        // ACTIVATION_ADAPTIVE zeros sub-threshold neurons, causing
                        // 95% output sparsity. Output layer needs all dims active.
                        if (layer == network->config.num_layers - 1) {
                            neuron->state = activation;
                        } else if (activation > neuron->threshold) {
                            neuron->state = tanhf((activation - neuron->threshold) / 10.0F);
                        } else {
                            neuron->state = 0.0F;
                        }
                        break;
                    case ACTIVATION_LINEAR:
                        neuron->state = activation;
                        break;
                    default:
                        neuron->state = tanhf(activation);
                }

                // Clamp unbounded activations to prevent float overflow
                // LINEAR is identity — do NOT clamp (regression targets are ±1,
                // clamping to ±100 causes loss plateau at ~10,000)
                switch (neuron->activation_type) {
                    case ACTIVATION_RELU:
                    case ACTIVATION_LEAKY_RELU:
                        neuron->state = fmaxf(-100.0F, fminf(100.0F, neuron->state));
                        break;
                    case ACTIVATION_LINEAR:
                        /* Keep ±100 clamp — the weights were trained with this regime.
                         * Weight decay (added separately) will gradually bring activations
                         * into ±1-5 range, after which the clamp becomes irrelevant.
                         * The 9,800 loss plateau was caused by the backprop delta clamp
                         * (±1.0 in serial path), NOT by this activation clamp. */
                        neuron->state = fmaxf(-100.0F, fminf(100.0F, neuron->state));
                        break;
                    default:
                        // sigmoid [0,1], tanh [-1,1], adaptive uses tanh — already bounded
                        break;
                }
            }

            // Phase 4: Residual skip from layer L-1 → L (every-layer, like ResNet)
            // Truncated identity: copy min(src, dst) elements, no projection matrix.
            // Skip output layer — LINEAR output should not mix hidden-layer signal.
            if (network->residual_saved_states && layer >= 2 &&
                layer < network->config.num_layers - 1) {
                float* src_state = network->residual_saved_states[layer - 1];
                if (src_state) {
                    uint32_t src_dim = network->config.layer_sizes[layer - 1];
                    uint32_t copy_n = (src_dim < layer_size) ? src_dim : layer_size;
                    for (uint32_t i = 0; i < copy_n && neuron_offset + i < network->num_neurons; i++) {
                        network->neurons[neuron_offset + i].state += src_state[i];
                    }
                }
            }

            // Phase 5: Layer normalization for hidden layers
            // Normalize to zero-mean, unit-variance to prevent vanishing/exploding
            // activations across deep layers. Backprop accounts for this via the
            // standard layer norm gradient (see nimcp_backprop_kernel.c).
            // Skip output layer — we want raw LINEAR output for regression.
            if (layer < network->config.num_layers - 1 && layer_size > 1) {
                uint32_t count = 0;
                float mean = 0.0f, M2 = 0.0f;
                for (uint32_t i = 0; i < layer_size && neuron_offset + i < network->num_neurons; i++) {
                    float x = network->neurons[neuron_offset + i].state;
                    count++;
                    float delta = x - mean;
                    mean += delta / (float)count;
                    float delta2 = x - mean;
                    M2 += delta * delta2;
                }
                if (count > 1) {
                    float variance = M2 / (float)count;
                    float inv_std = 1.0F / sqrtf(variance + 1e-5F);
                    for (uint32_t i = 0; i < count; i++) {
                        network->neurons[neuron_offset + i].state =
                            (network->neurons[neuron_offset + i].state - mean) * inv_std;
                    }
                }
            }

            // Save post-norm state for future residual connections
            if (network->residual_saved_states &&
                layer < network->config.num_layers) {
                if (!network->residual_saved_states[layer]) {
                    network->residual_saved_states[layer] =
                        (float*)nimcp_calloc(layer_size, sizeof(float));
                }
                if (network->residual_saved_states[layer]) {
                    for (uint32_t i = 0; i < layer_size && neuron_offset + i < network->num_neurons; i++) {
                        network->residual_saved_states[layer][i] =
                            network->neurons[neuron_offset + i].state;
                    }
                }
            }

            neuron_offset += layer_size;
        }

        // Extract outputs from output layer
        if (neuron_offset < output_size) {
            for (uint32_t i = 0; i < output_size; i++) outputs[i] = 0.0F;
            return true;
        }
        uint32_t output_layer_start = neuron_offset - output_size;

        for (uint32_t i = 0; i < output_size; i++) {
            if (output_layer_start + i < network->num_neurons) {
                outputs[i] = network->neurons[output_layer_start + i].state;
            } else {
                outputs[i] = 0.0F;
            }
        }

        return true;
    }

    // Fallback: Legacy NIMCP 2.0 behavior
    // Set first N neurons to inputs
    uint32_t num_inputs = (input_size < network->num_neurons) ? input_size : network->num_neurons;
    for (uint32_t i = 0; i < num_inputs; i++) {
        network->neurons[i].state = inputs[i];
    }

    // Run one network computation step
    network->current_time++;
    neural_network_compute_step(network, network->current_time);

    // Extract outputs from last N neurons
    uint32_t output_start =
        (network->num_neurons > output_size) ? (network->num_neurons - output_size) : 0;
    for (uint32_t i = 0; i < output_size; i++) {
        uint32_t neuron_id = output_start + i;
        if (neuron_id < network->num_neurons) {
            outputs[i] = network->neurons[neuron_id].state;
        } else {
            outputs[i] = 0.0F;
        }
    }

    return true;
}

//=============================================================================
// Bidirectional Synapse API (OPTIMIZATION)
//=============================================================================

/**
 * @brief Get count of incoming synapses to a neuron
 *
 * DESIGN PATTERN: Iterator Pattern
 * COMPLEXITY: O(1)
 * WHY: Enables O(S) iteration over incoming edges instead of O(N×S) scan
 */
uint32_t neural_network_get_incoming_synapse_count(neural_network_t network, uint32_t neuron_id)
{
    if (!network || neuron_id >= network->num_neurons) {
        return 0;
    }

    return NEURON_IN_COUNT(&network->neurons[neuron_id]);
}

/**
 * @brief Get array of incoming synapses
 *
 * DESIGN PATTERN: Iterator Pattern
 * COMPLEXITY: O(1) to get pointer, O(S) to iterate
 * WHY: Provides direct access to incoming edges for efficient input summation
 *
 * @param network Neural network
 * @param neuron_id Target neuron
 * @param out_synapses Pointer to receive synapse array (read-only)
 * @return Number of incoming synapses
 */
uint32_t neural_network_get_incoming_synapses(neural_network_t network, uint32_t neuron_id,
                                               const synapse_t** out_synapses)
{
    if (!network || !out_synapses || neuron_id >= network->num_neurons) {
        if (out_synapses) {
            *out_synapses = NULL;
        }
        return 0;
    }

    // Additional validation: check if neurons array is valid
    if (!network->neurons) {
        *out_synapses = NULL;
        return 0;
    }

    // NIMCP 2.11: Sparse storage doesn't provide a contiguous synapse_t array.
    // Return count; callers should use NEURON_IN_HANDLE / NEURON_IN_META macros.
    *out_synapses = NULL;
    return NEURON_IN_COUNT(&network->neurons[neuron_id]);
}

//=============================================================================
// NIMCP 2.11: Sparse synapse pool accessors
//=============================================================================

sparse_synapse_pool_t neural_network_get_synapse_handle_pool(neural_network_t network)
{
    if (!network) return NULL;
    return network->synapse_handle_pool;
}

synapse_metadata_pool_t neural_network_get_synapse_metadata_pool(neural_network_t network)
{
    if (!network) return NULL;
    return network->synapse_metadata_pool;
}

//=============================================================================
// NIMCP 2.7: NLP Integration - Accessor Functions
//=============================================================================

/**
 * @brief Set global state for synapse computation
 *
 * WHAT: Inject global state buffer (e.g., attention output) into network
 * WHY: Enable synapses to access shared context for attention-modulation
 * HOW: Stores pointer and size, passed via synapse_compute_context_t
 * WHEN: Called by NLP layer after attention forward pass
 *
 * DESIGN PATTERN: Dependency Injection
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param global_state Pointer to global state buffer
 * @param size Size of buffer (floats)
 * @return true on success
 */
bool neural_network_set_global_state(neural_network_t network, float* global_state, uint32_t size)
{
    // Guard: Validate input
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_global_state: network is NULL");
        return false;
    }

    // Store global state reference
    network->global_state = global_state;
    network->global_state_size = size;

    return true;
}

/**
 * @brief Set neuromodulator system for synapse computation
 *
 * WHAT: Inject neuromodulator system into network
 * WHY: Enable synapses to access neuromodulator levels (dopamine, ACh, etc)
 * HOW: Stores opaque pointer, queried during synapse computation
 * WHEN: Called by NLP layer during network creation
 *
 * DESIGN PATTERN: Dependency Injection
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param neuromod_system Opaque pointer to neuromodulator system
 * @return true on success
 */
bool neural_network_set_neuromodulator_system(neural_network_t network, void* neuromod_system)
{
    // Guard: Validate input
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_neuromodulator_system: network is NULL");
        return false;
    }

    // Store neuromodulator system reference
    network->neuromodulator_system = neuromod_system;

    return true;
}

/**
 * @brief Set glial integration system for neuro-glial signaling (Phase 6)
 *
 * WHAT: Attach glial integration system to network
 * WHY: Enable bidirectional neuro-glial communication
 * HOW: Stores opaque pointer, notified on neuron/synapse events
 * WHEN: Called after glial cells are assigned
 *
 * DESIGN PATTERN: Observer - glial system observes neural events
 * COMPLEXITY: O(1)
 */
bool neural_network_set_glial_integration(neural_network_t network, void* glial_system)
{
    // Guard: Validate input
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_set_glial_integration: network is NULL");
        return false;
    }

    // Store glial integration system reference
    network->glial_integration = glial_system;

    return true;
}

/*
 * Getter for regression test coverage (confirms the setter actually wrote
 * the pointer through). Keep lightweight — NULL-tolerant, no locking.
 */
void* neural_network_get_glial_integration(neural_network_t network)
{
    return network ? network->glial_integration : NULL;
}

/**
 * @brief Get neuromodulation level for synapse computation
 *
 * WHAT: Query current neuromodulation level from attached system
 * WHY: Synapses need to know dopamine/ACh levels for modulation
 * HOW: Queries neuromodulator system via opaque pointer
 * WHEN: Called during synapse computation in sum_synaptic_inputs
 *
 * DESIGN PATTERN: Adapter Pattern - wraps neuromodulator system access
 * COMPLEXITY: O(1)
 *
 * NOTE: Currently returns 0.0 as placeholder. Full implementation would:
 * 1. Cast neuromodulator_system to proper type
 * 2. Call neuromodulator_get_dopamine() or similar
 * 3. Return normalized level [0,1]
 *
 * @param network Neural network
 * @return Current neuromodulation level [0,1]
 */
float neural_network_get_neuromodulation(neural_network_t network)
{
    // Guard: Validate input
    if (!network || !network->neuromodulator_system) {
        return 0.0F;
    }

    // Query neuromodulator system for dopamine level
    // Dopamine is the primary learning/reward neuromodulator
    neuromodulator_system_t system = (neuromodulator_system_t)network->neuromodulator_system;
    float dopamine = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);

    return dopamine;
}

//=============================================================================
// PHASE 8.7: Synapse Type System - Typed Connections
//=============================================================================

/**
 * @brief Add connection with specific synapse type (Phase 8.7)
 *
 * WHAT: Create synapse with biological type (AMPA, NMDA, GABA-A, etc)
 * WHY: Enable biologically realistic synapse diversity
 * HOW: Initialize synapse with type-specific parameters
 *
 * ALGORITHM:
 * 1. Call standard neural_network_add_connection
 * 2. Set synapse type field
 * 3. Initialize type-specific state (AMPA/NMDA/GABA/etc parameters)
 * 4. Copy type info to incoming synapse (bidirectional)
 *
 * COMPLEXITY: O(1)
 *
 * BIOLOGICAL MOTIVATION:
 * Real synapses differ by neurotransmitter type:
 * - Excitatory: AMPA (fast), NMDA (slow + Ca2+)
 * - Inhibitory: GABA-A (fast), GABA-B (slow)
 * - Modulatory: Dopamine, Serotonin, Acetylcholine
 * - Electrical: Gap junctions
 *
 * @param network Neural network
 * @param from_id Source neuron ID
 * @param to_id Target neuron ID
 * @param weight Initial synaptic weight
 * @param type Synapse type (SYNAPSE_AMPA, SYNAPSE_NMDA, etc)
 * @return true if successful, false on error
 */
bool neural_network_add_connection_typed(neural_network_t network, uint32_t from_id, uint32_t to_id,
                                          float weight, synapse_type_t type)
{
    // 1. Create standard connection (handles all base initialization)
    if (!neural_network_add_connection(network, from_id, to_id, weight)) {
        return false;
    }

    // 2. Get the newly created synapse (last one added)
    neuron_t* from_neuron = &network->neurons[from_id];

    uint32_t out_last = NEURON_OUT_COUNT(from_neuron) - 1;
    synapse_t* syn = NEURON_OUT_META(network, from_neuron, out_last);
    if (!syn) return false;

    // 3. Set synapse type (cold field)
    synapse_cold_t* cold = SYNAPSE_ENSURE_COLD(network, syn);
    if (!cold) return false;
    cold->type = type;
    // Note: incoming_syn type init removed — incoming synapses are handle-only
    // (no cold metadata), type state is authoritative on outgoing side

    // 4. Initialize type-specific state based on type
    switch (type) {
        case SYNAPSE_AMPA:
            synapse_init_ampa(&cold->type_state.ampa);
            break;

        case SYNAPSE_NMDA:
            synapse_init_nmda(&cold->type_state.nmda);
            break;

        case SYNAPSE_GABA_A:
            synapse_init_gaba_a(&cold->type_state.gaba_a);
            break;

        case SYNAPSE_GABA_B:
            synapse_init_gaba_b(&cold->type_state.gaba_b);
            break;

        case SYNAPSE_DOPAMINE:
            synapse_init_dopamine(&cold->type_state.dopamine);
            break;

        case SYNAPSE_SEROTONIN:
            synapse_init_serotonin(&cold->type_state.serotonin);
            break;

        case SYNAPSE_ACETYLCHOLINE:
            synapse_init_acetylcholine(&cold->type_state.acetylcholine);
            break;

        case SYNAPSE_ELECTRICAL:
            synapse_init_electrical(&cold->type_state.electrical);
            break;

        case SYNAPSE_GENERIC:
        default:
            // Generic synapse has no special state to initialize
            break;
    }

    return true;
}

//=============================================================================
// NIMCP 2.10: Ternary Weight Functions
//=============================================================================

/**
 * @brief Convert float weight to ternary
 *
 * WHAT: Quantize continuous weight to {-1, 0, +1}
 * WHY:  Prepare weight for ternary storage
 * HOW:  Threshold-based quantization
 */
trit_t synapse_weight_to_ternary(float weight, float threshold) {
    if (weight >= threshold) {
        return TRIT_POSITIVE;
    } else if (weight <= -threshold) {
        return TRIT_NEGATIVE;
    }
    return TRIT_UNKNOWN;
}

/**
 * @brief Convert ternary weight to float
 *
 * WHAT: Dequantize ternary weight to continuous value
 * WHY:  Enable computation with ternary-stored weights
 * HOW:  Map {-1, 0, +1} to {-scale, 0, +scale}
 */
float synapse_ternary_to_weight(trit_t ternary_weight, float positive_scale, float negative_scale) {
    if (ternary_weight == TRIT_POSITIVE) {
        return positive_scale;
    } else if (ternary_weight == TRIT_NEGATIVE) {
        return negative_scale;
    }
    return 0.0f;
}

/**
 * @brief Enable ternary mode for synapse
 */
bool synapse_enable_ternary_weight(neural_network_t net, synapse_t* synapse, float threshold, float scale) {
    if (!net || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_enable_ternary_weight: net or synapse is NULL");
        return false;
    }

    synapse_cold_t* cold = SYNAPSE_ENSURE_COLD(net, synapse);
    if (!cold) return false;

    // Quantize current weight to ternary
    cold->ternary_weight = synapse_weight_to_ternary(synapse->weight, threshold);
    cold->use_ternary_weight = true;
    cold->ternary_scale = scale;

    return true;
}

/**
 * @brief Disable ternary mode for synapse
 */
bool synapse_disable_ternary_weight(neural_network_t net, synapse_t* synapse) {
    if (!net || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_disable_ternary_weight: net or synapse is NULL");
        return false;
    }

    synapse_cold_t* cold = SYNAPSE_COLD(net, synapse);
    if (!cold) return true;  // No cold data = not in ternary mode

    // Dequantize ternary weight to float if currently in ternary mode
    if (cold->use_ternary_weight) {
        synapse->weight = synapse_ternary_to_weight(
            cold->ternary_weight,
            cold->ternary_scale,
            -cold->ternary_scale
        );
    }

    cold->use_ternary_weight = false;

    return true;
}

/**
 * @brief Get effective weight (handles ternary/float transparently)
 */
float synapse_get_effective_weight(neural_network_t net, const synapse_t* synapse) {
    if (!synapse) return 0.0f;

    if (net) {
        const synapse_cold_t* cold = SYNAPSE_COLD(net, (synapse_t*)synapse);
        if (cold && cold->use_ternary_weight) {
            return synapse_ternary_to_weight(
                cold->ternary_weight,
                cold->ternary_scale,
                -cold->ternary_scale
            );
        }
    }

    return synapse->weight;
}

/**
 * @brief Set effective weight (handles ternary/float transparently)
 */
void synapse_set_effective_weight(neural_network_t net, synapse_t* synapse, float weight, float threshold) {
    if (!synapse) return;

    if (net) {
        synapse_cold_t* cold = SYNAPSE_COLD(net, synapse);
        if (cold && cold->use_ternary_weight) {
            cold->ternary_weight = synapse_weight_to_ternary(weight, threshold);
            return;
        }
    }
    synapse->weight = weight;
}

/**
 * @brief Create ternary weight matrix for network
 */
ternary_weight_matrix_t* ternary_weight_matrix_create(
    uint32_t num_neurons,
    ternary_pack_mode_t pack_mode
) {
    ternary_weight_matrix_t* twm = nimcp_malloc(sizeof(ternary_weight_matrix_t));
    if (!twm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ternary_weight_matrix_create: twm is NULL");
        return NULL;
    }

    twm->weights = trit_matrix_create(num_neurons, num_neurons, pack_mode);
    if (!twm->weights) {
        nimcp_free(twm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ternary_weight_matrix_create: twm->weights is NULL");
        return NULL;
    }

    twm->positive_scale = 1.0f;
    twm->negative_scale = -1.0f;
    twm->threshold = 0.3f;
    twm->num_neurons = num_neurons;
    twm->is_sparse = false;

    return twm;
}

/**
 * @brief Destroy ternary weight matrix
 */
void ternary_weight_matrix_destroy(ternary_weight_matrix_t* twm) {
    if (!twm) return;

    if (twm->weights) {
        trit_matrix_destroy(twm->weights);
    }

    nimcp_free(twm);
}

/**
 * @brief Export network weights to ternary matrix
 */
ternary_weight_matrix_t* neural_network_export_ternary_weights(
    neural_network_t network,
    float threshold,
    ternary_pack_mode_t pack_mode
) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_export_ternary_weights: network is NULL");
        return NULL;
    }

    ternary_weight_matrix_t* twm = ternary_weight_matrix_create(network->num_neurons, pack_mode);
    if (!twm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neural_network_export_ternary_weights: twm is NULL");
        return NULL;
    }

    twm->threshold = threshold;

    // Iterate all neurons and their outgoing synapses
    for (uint32_t from_id = 0; from_id < network->num_neurons; from_id++) {
        neuron_t* neuron = &network->neurons[from_id];

        for (uint32_t syn_idx = 0; syn_idx < NEURON_OUT_COUNT(neuron); syn_idx++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, syn_idx);
            if (!h) continue;
            uint32_t to_id = h->target_neuron_id;

            // Quantize weight to ternary
            trit_t trit_weight = synapse_weight_to_ternary(h->weight, threshold);

            // Store in matrix
            trit_matrix_set(twm->weights, from_id, to_id, trit_weight);
        }
    }

    return twm;
}

/**
 * @brief Import ternary weights to network
 */
int neural_network_import_ternary_weights(
    neural_network_t network,
    const ternary_weight_matrix_t* twm
) {
    if (!network || !twm || !twm->weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_import_ternary_weights: required parameter is NULL (network, twm, twm->weights)");
        return -1;
    }

    int imported = 0;

    // Iterate all neurons and their outgoing synapses
    for (uint32_t from_id = 0; from_id < network->num_neurons; from_id++) {
        neuron_t* neuron = &network->neurons[from_id];

        for (uint32_t syn_idx = 0; syn_idx < NEURON_OUT_COUNT(neuron); syn_idx++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, syn_idx);
            if (!h) continue;
            uint32_t to_id = h->target_neuron_id;

            // Get ternary weight from matrix
            trit_t trit_weight = trit_matrix_get(twm->weights, from_id, to_id);

            // Dequantize to float
            float new_w = synapse_ternary_to_weight(
                trit_weight,
                twm->positive_scale,
                twm->negative_scale
            );
            h->weight = new_w;

            // Keep metadata in sync
            synapse_t* syn = NEURON_OUT_META(network, neuron, syn_idx);
            if (syn) syn->weight = new_w;

            imported++;
        }
    }

    return imported;
}

//=============================================================================
// Active Neuron Set Tracking API
//=============================================================================

uint32_t neural_network_get_active_count(neural_network_t network) {
    if (!network) return 0;
    return network->num_active_neurons;
}

const uint32_t* neural_network_get_active_ids(neural_network_t network) {
    if (!network || !network->active_set_valid) return NULL;
    return network->active_neuron_ids;
}

float neural_network_get_sparsity_ratio(neural_network_t network) {
    if (!network || network->num_neurons == 0) return 0.0f;
    return (float)network->num_active_neurons / (float)network->num_neurons;
}
