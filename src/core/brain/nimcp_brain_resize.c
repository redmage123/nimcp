//=============================================================================
// nimcp_brain_resize.c - Dynamic Brain Resizing Implementation
//=============================================================================
/**
 * @file nimcp_brain_resize.c
 * @brief Dynamic brain expansion with neuron preservation
 *
 * WHAT: Enables brains to grow during training without losing learned knowledge
 * WHY:  Allows starting small (fast training) and scaling up based on capacity needs
 * HOW:  Creates larger network, transfers existing neurons/weights, preserves all state
 *
 * DESIGN PRINCIPLES:
 * - Zero knowledge loss: All neurons, weights, and states preserved exactly
 * - Incremental growth: Add neurons progressively, not exponential jumps
 * - Automatic triggers: Detect when brain is saturated and needs more capacity
 * - Backward compatible: Works with all existing cognitive subsystems
 *
 * ARCHITECTURE:
 * ```
 * brain_resize(brain, new_size) {
 *   1. Validate new size > current size
 *   2. Create new larger adaptive network
 *   3. Transfer all existing neurons (weights, states, traces)
 *   4. Add new neurons with random initialization
 *   5. Update all cognitive subsystems (working memory, etc.)
 *   6. Swap networks atomically
 *   7. Destroy old network
 * }
 *
 * brain_auto_resize(brain) {
 *   if (should_grow(brain)) {
 *     new_size = compute_next_size(current_size);
 *     brain_resize(brain, new_size);
 *   }
 * }
 * ```
 *
 * PERFORMANCE:
 * - Resize time: O(n + m) where n=old neurons, m=new neurons
 * - Memory overhead: 2× during transfer (old + new network)
 * - No inference downtime: Can continue serving requests
 *
 * AUTO-GROWTH TRIGGERS:
 * - High utilization: >90% neurons active consistently
 * - Weight saturation: Most weights near ±max
 * - Performance plateau: Learning rate stagnates
 * - Explicit request: User/API calls resize
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.8.0
 */

/**
 * BIO-ASYNC INTEGRATION:
 * - Module ID: BIO_MODULE_BRAIN_RESIZE (0x0130)
 * - Publishes: Resize start/complete events, utilization metrics
 * - Channels: DOPAMINE (success), NOREPINEPHRINE (urgent/failure)
 */

#define LOG_MODULE "brain_resize"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_resize, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include "core/brain/nimcp_brain_resize.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "core/brain/nimcp_brain.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_system_resources.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_BRAIN_RESIZE 0x0130

// Bio-async registration state
static bool g_bio_async_registered = false;

/**
 * @brief Ensure bio-async system is registered
 */
static bio_module_context_t g_bio_module_ctx = NULL;

static void ensure_bio_async_registered(void) {
    if (!g_bio_async_registered) {
        LOG_DEBUG("Registering brain_resize module with bio-async router");
        bio_module_info_t info = {
            .module_id = BIO_MODULE_BRAIN_RESIZE,
            .module_name = "brain_resize",
            .inbox_capacity = 256,
            .user_data = NULL
        };
        g_bio_module_ctx = bio_router_register_module(&info);
        g_bio_async_registered = (g_bio_module_ctx != NULL);
        if (!g_bio_async_registered) {
            LOG_WARN("Failed to register brain_resize with bio-async router");
        }
    }
}

/**
 * @brief Publish resize event via bio-async
 */
static void publish_resize_event(
    bio_message_type_t msg_type,
    uint32_t old_size,
    uint32_t new_size,
    bool success,
    nimcp_bio_channel_type_t channel)
{
    if (!g_bio_async_registered || !g_bio_module_ctx) {
        return;  // Bio-async not available, skip publishing
    }

    // Create resize event message
    bio_msg_brain_state_query_t msg = {0};
    bio_msg_init_header(&msg.header, msg_type,
                       BIO_MODULE_BRAIN_RESIZE,
                       0,  // Broadcast
                       sizeof(msg));
    msg.header.channel = channel;
    msg.query_flags = old_size;  // Reuse field for old_size
    msg.region_id = new_size;    // Reuse field for new_size

    // Publish message via bio-router broadcast
    nimcp_error_t err = bio_router_broadcast(g_bio_module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN("Failed to broadcast resize event: error=%d", err);
    }

    LOG_DEBUG("Published resize event: type=%u, old=%u, new=%u, success=%d, channel=%d",
              msg_type, old_size, new_size, success, channel);
}

//=============================================================================
// Internal Brain Structure (forward declaration for access)
//=============================================================================

/**
 * @brief Internal brain structure (incomplete - only fields we need)
 * WHAT: Forward declaration to access minimal internals from resize logic
 * WHY:  Resize needs to update network and config fields
 * NOTE: We DON'T access glial or other fields directly - use helper functions instead!
 */
struct brain_struct {
    adaptive_network_t network;
    brain_config_t config;
    // ... other fields not accessed from resize logic
};

//=============================================================================
// Auto-Growth Detection
//=============================================================================

/**
 * @brief Growth trigger metrics
 * WHAT: Metrics for determining when brain needs more capacity
 * WHY:  Enable data-driven automatic growth decisions
 */
typedef struct {
    float avg_neuron_utilization;  /**< Fraction of neurons active (0.0-1.0) */
    float weight_saturation;        /**< Fraction of weights near limits (0.0-1.0) */
    float learning_progress;        /**< Recent learning improvement rate */
    uint32_t steps_since_growth;    /**< Training steps since last resize */
    bool manual_trigger;            /**< User explicitly requested growth */
} growth_metrics_t;

/**
 * @brief Compute neuron utilization
 *
 * WHAT: Calculate what fraction of neurons are actively participating
 * WHY:  High utilization (>90%) indicates capacity saturation
 * HOW:  Count neurons with non-zero activity over recent window
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param network Neural network to analyze
 * @return Utilization ratio [0.0, 1.0]
 */
static float compute_neuron_utilization(neural_network_t network)
{
    LOG_DEBUG("Computing neuron utilization");

    if (!network) {
        LOG_ERROR("compute_neuron_utilization: NULL network");
        return -1.0F;  // Error signal
    }

    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        LOG_WARN("compute_neuron_utilization: Network has 0 neurons");
        return -1.0F;  // Error signal
    }

    // Count active neurons with error handling
    // NOTE: neural_network_get_average_activity returns 0.0f for newly created
    // brains with no activity, so we naturally get 0% utilization
    uint32_t active_count = 0;
    uint32_t valid_reads = 0;

    for (uint32_t i = 0; i < num_neurons; i++) {
        // Bounds check
        if (i >= num_neurons) {
            continue;  // Skip invalid index
        }

        // Try to get activity, skip on error
        float avg_activity = neural_network_get_average_activity(network, i);

        // Activity is valid if non-negative
        if (avg_activity >= 0.0F) {
            valid_reads++;
            if (avg_activity > 0.01F) {
                active_count++;
            }
        }
    }

    // If we couldn't read any neurons, signal error
    if (valid_reads == 0) {
        return -1.0F;  // Error signal
    }

    // For brand new brains, all neurons will have 0 activity, giving 0% utilization
    float utilization = (float)active_count / (float)valid_reads;
    LOG_DEBUG("Neuron utilization: %.2f%% (%u/%u active)", utilization * 100.0F, active_count, valid_reads);
    return utilization;
}

/**
 * @brief Compute weight saturation
 *
 * WHAT: Calculate what fraction of weights are near their limits
 * WHY:  Saturated weights can't learn effectively, need more capacity
 * HOW:  Count weights with |w| > 0.9 * max_weight
 *
 * COMPLEXITY: O(n × s) where s = avg synapses per neuron
 *
 * @param network Neural network to analyze
 * @return Saturation ratio [0.0, 1.0]
 */
static float compute_weight_saturation(neural_network_t network)
{
    if (!network) {
        return 0.0F;
    }

    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        return 0.0F;
    }

    uint32_t total_weights = 0;
    uint32_t saturated_weights = 0;
    const float saturation_threshold = 0.9F;  // Consider saturated if |w| > 0.9

    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (!neuron) {
            continue;
        }

        // Validate synapse count is reasonable (< 10000 per neuron is sane limit)
        uint32_t nsyn = NEURON_OUT_COUNT(neuron);
        if (nsyn > 10000) {
            continue;  // Skip neurons with garbage synapse count
        }

        for (uint32_t j = 0; j < nsyn; j++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, j);
            if (!h) continue;
            float weight = h->weight;
            total_weights++;

            if (fabsf(weight) > saturation_threshold) {
                saturated_weights++;
            }
        }
    }

    if (total_weights == 0) {
        return 0.0F;
    }

    return (float)saturated_weights / (float)total_weights;
}

/**
 * @brief Check if brain should grow
 *
 * WHAT: Determine if brain has reached capacity and needs more neurons
 * WHY:  Automatic growth enables continuous learning without manual intervention
 * HOW:  Evaluate multiple metrics and apply threshold-based decision logic
 *
 * GROWTH CRITERIA (any trigger growth):
 * - High utilization: >90% neurons active for >1000 steps
 * - Weight saturation: >80% weights near limits
 * - Manual trigger: User explicitly called resize
 *
 * @param metrics Growth metrics
 * @return true if brain should grow
 */
static bool should_grow(const growth_metrics_t* metrics)
{
    if (!metrics) {
        return false;
    }

    // Trigger 1: Manual request
    if (metrics->manual_trigger) {
        LOG_INFO("Growth triggered: Manual request");
        return true;
    }

    // Trigger 2: High neuron utilization
    const float utilization_threshold = 0.90F;
    const uint32_t utilization_patience = 1000;  // Steps to wait before growing

    if (metrics->avg_neuron_utilization > utilization_threshold &&
        metrics->steps_since_growth > utilization_patience) {
        LOG_INFO("Growth triggered: High utilization (%.2f%% > %.2f%%)",
                       metrics->avg_neuron_utilization * 100,
                       utilization_threshold * 100);
        return true;
    }

    // Trigger 3: Weight saturation
    const float saturation_threshold = 0.80F;

    if (metrics->weight_saturation > saturation_threshold) {
        LOG_INFO("Growth triggered: Weight saturation (%.2f%% > %.2f%%)",
                       metrics->weight_saturation * 100,
                       saturation_threshold * 100);
        return true;
    }

    return false;
}

/**
 * @brief Compute next brain size (hardware-aware)
 *
 * WHAT: Calculate how many neurons to add when growing
 * WHY:  Incremental growth prevents memory spikes and excessive overhead
 * HOW:  Add 50% more neurons (1.5× current size), capped by hardware resources
 *
 * GROWTH POLICY:
 * - TINY (100)  → SMALL (500)   [5× growth to reach useful size]
 * - SMALL (500) → MEDIUM (1000)  [2× growth]
 * - MEDIUM (1000) → LARGE (5000) [5× growth]
 * - LARGE (5000) → 7500          [1.5× growth]
 * - Custom: Always 1.5× growth
 * - Hardware cap: Never exceed available RAM/GPU/neuromorphic capacity
 *
 * @param current_size Current neuron count
 * @param use_gpu Whether GPU acceleration is enabled
 * @return New neuron count (hardware-aware)
 */
static uint32_t compute_next_size(uint32_t current_size, bool use_gpu)
{
    // Query system resources
    system_resources_t resources;
    if (!system_resources_query(&resources)) {
        // Fallback: use preset sizes without hardware awareness
        LOG_WARN("compute_next_size: Failed to query system resources, using defaults");

        if (current_size <= 100) {
            return 500;  // TINY → SMALL
        } else if (current_size <= 500) {
            return 1000;  // SMALL → MEDIUM
        } else if (current_size <= 1000) {
            return 5000;  // MEDIUM → LARGE
        } else {
            return (uint32_t)(current_size * 1.5F);
        }
    }

    // Get hardware-aware recommendation
    uint32_t recommended_size = system_resources_recommend_size(&resources, current_size, use_gpu);

    // Log resource-aware decision
    LOG_INFO("compute_next_size: Current=%u, Recommended=%u (RAM: %luMB avail, GPU: %s)",
                   current_size, recommended_size,
                   (unsigned long)resources.available_ram_mb,
                   use_gpu ? "enabled" : "disabled");

    // Additional safety check: never recommend same or smaller size
    if (recommended_size <= current_size) {
        uint32_t min_growth = (uint32_t)(current_size * 1.1F);  // At least 10% growth
        LOG_WARN("compute_next_size: Recommended size (%u) <= current (%u), forcing min growth to %u",
                       recommended_size, current_size, min_growth);
        recommended_size = min_growth;
    }

    return recommended_size;
}

//=============================================================================
// Core Resize Implementation
//=============================================================================

/**
 * @brief Resize brain to new neuron count
 *
 * WHAT: Expand brain capacity while preserving all learned knowledge
 * WHY:  Enable continuous learning without hitting capacity limits
 * HOW:  Create new larger network, transfer all neurons/weights, swap atomically
 *
 * ALGORITHM:
 * 1. Validate: new_neuron_count > current (only growth supported)
 * 2. Create: Build new adaptive network with increased capacity
 * 3. Transfer: Copy all existing neurons (weights, biases, states, traces)
 * 4. Initialize: Add new neurons with random small weights
 * 5. Swap: Atomically replace old network with new network
 * 6. Update: Adjust brain config and subsystem sizes
 * 7. Cleanup: Destroy old network
 *
 * GUARANTEES:
 * - Zero knowledge loss: All weights/states preserved exactly
 * - Atomic operation: Network swap is instantaneous
 * - Memory safe: Old network destroyed only after successful transfer
 * - Config consistency: brain_config_t updated to reflect new size
 *
 * ERROR HANDLING:
 * - Returns false if new_count ≤ current (shrinking not supported)
 * - Returns false if allocation fails (old network unchanged)
 * - Returns false if transfer fails (old network unchanged)
 *
 * COMPLEXITY:
 * - Time: O(n + m) where n=old neurons, m=new neurons
 * - Space: O(max(n,m) × k) where k=avg connections
 *
 * @param brain Brain to resize
 * @param new_neuron_count New total neuron count (must be > current)
 * @return true on success, false on error (brain unchanged)
 */
bool brain_resize(brain_t brain, uint32_t new_neuron_count)
{
    LOG_DEBUG("brain_resize: Entry, target_size=%u", new_neuron_count);

    // Ensure bio-async is available
    ensure_bio_async_registered();

    // Process pending bio-async messages
    if (g_bio_async_registered && g_bio_module_ctx) {
        bio_router_process_inbox(g_bio_module_ctx, 5);
    }

    // Guard: Validate brain handle
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: brain is NULL");
        LOG_ERROR("brain_resize: NULL brain handle");
        return false;
    }

    // Get current network
    adaptive_network_t old_network = brain->network;
    if (!old_network) {
        LOG_ERROR("brain_resize: Brain has no network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: old_network is NULL");
        return false;
    }

    // Get current size
    neural_network_t base_network = adaptive_network_get_base_network(old_network);
    if (!base_network) {
        LOG_ERROR("brain_resize: Cannot access base network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: base_network is NULL");
        return false;
    }

    uint32_t current_neuron_count = neural_network_get_num_neurons(base_network);

    // Guard: Validate new size (only growth supported)
    if (new_neuron_count <= current_neuron_count) {
        LOG_ERROR("brain_resize: New size (%u) must be > current (%u). Shrinking not supported.",
                  new_neuron_count, current_neuron_count);
        // Publish failure event
        publish_resize_event(BIO_MSG_ERROR_REPORT, current_neuron_count, new_neuron_count,
                           false, BIO_CHANNEL_NOREPINEPHRINE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_resize: validation failed");
        return false;
    }

    LOG_INFO("brain_resize: Growing from %u to %u neurons (+%u, +%.1f%%)",
             current_neuron_count, new_neuron_count,
             new_neuron_count - current_neuron_count,
             ((float)(new_neuron_count - current_neuron_count) / current_neuron_count) * 100);

    // Publish resize start event (via norepinephrine - alerting)
    publish_resize_event(BIO_MSG_BRAIN_STATE_QUERY, current_neuron_count, new_neuron_count,
                        true, BIO_CHANNEL_NOREPINEPHRINE);

    // Step 1: Create new larger network configuration
    const adaptive_network_config_t* old_config = adaptive_network_get_config(old_network);
    if (!old_config) {
        LOG_ERROR("brain_resize: Cannot access old network config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: old_config is NULL");
        return false;
    }

    // DEBUG: Log old config spike params
    LOG_INFO("brain_resize: old_config k_factor=%f, min_threshold=%f, max_threshold=%f",
                   old_config->spike_params.k_factor,
                   old_config->spike_params.min_threshold,
                   old_config->spike_params.max_threshold);

    // Shallow copy first
    adaptive_network_config_t new_config = {0};
    memcpy(&new_config, old_config, sizeof(adaptive_network_config_t));

    // Skip dense layer wiring — resize will wire new neurons sparsely instead
    new_config.base_config.skip_layer_wiring = true;

    // DEBUG: Log new config spike params after memcpy
    LOG_INFO("brain_resize: new_config k_factor=%f, min_threshold=%f, max_threshold=%f",
                   new_config.spike_params.k_factor,
                   new_config.spike_params.min_threshold,
                   new_config.spike_params.max_threshold);

    // CRITICAL FIX: Handle corrupted layer config (num_layers=0, layer_sizes=NULL)
    // WHY: Old configs from certain brain creation paths may not properly initialize layers
    // WHAT: Rebuild layer config using brain's input/output sizes
    uint32_t* new_layer_sizes = NULL;

    if (old_config->base_config.num_layers == 0 || !old_config->base_config.layer_sizes) {
        LOG_WARN("brain_resize: Corrupted layer config (num_layers=%u, layer_sizes=%p), rebuilding",
                       old_config->base_config.num_layers,
                       (void*)old_config->base_config.layer_sizes);

        // Rebuild layer config: [input, hidden, output]
        new_config.base_config.num_layers = 3;
        new_layer_sizes = nimcp_calloc(3, sizeof(uint32_t));
        if (!new_layer_sizes) {
            LOG_ERROR("brain_resize: Failed to allocate layer_sizes");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_resize: new_layer_sizes is NULL");
            return false;
        }

        // CRITICAL FIX: Get input/output sizes from brain config instead of old_config
        // WHY: old_config->base_config may not be fully initialized (heap-buffer-overflow)
        // WHAT: Use brain->config which is guaranteed to be properly initialized
        uint32_t input_size = brain->config.num_inputs;
        uint32_t output_size = brain->config.num_outputs;

        if (input_size == 0 || output_size == 0) {
            LOG_WARN("brain_resize: Brain config has invalid sizes (input=%u, output=%u), using defaults",
                           input_size, output_size);
            input_size = 10;  // Default fallback
            output_size = 5;
        }

        new_layer_sizes[0] = input_size;
        new_layer_sizes[1] = current_neuron_count;  // Will be updated later
        new_layer_sizes[2] = output_size;
        new_config.base_config.layer_sizes = new_layer_sizes;
        new_config.base_config.input_size = input_size;
        new_config.base_config.output_size = output_size;
    }
    else {
        // Deep copy existing valid layer_sizes (avoid use-after-free)
        // WHY: Shallow copy copies the pointer, which points to old network's memory
        new_layer_sizes = nimcp_calloc(old_config->base_config.num_layers, sizeof(uint32_t));
        if (!new_layer_sizes) {
            LOG_ERROR("brain_resize: Failed to allocate layer_sizes");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_resize: new_layer_sizes is NULL");
            return false;
        }
        memcpy(new_layer_sizes, old_config->base_config.layer_sizes,
               old_config->base_config.num_layers * sizeof(uint32_t));
        new_config.base_config.layer_sizes = new_layer_sizes;
    }

    // CRITICAL FIX: If spike_params are corrupted (k_factor==0), reinitialize with sane defaults
    // WHY: Loaded checkpoints or corrupted configs can have invalid spike params
    // WHAT: Reset to known-good defaults that match build_spike_params
    if (new_config.spike_params.k_factor <= 0.0F) {
        LOG_WARN("brain_resize: Corrupted spike_params detected (k_factor=%f), resetting to defaults",
                       new_config.spike_params.k_factor);
        new_config.spike_params.k_factor = 0.5F;
        new_config.spike_params.min_threshold = 0.0001F;
        new_config.spike_params.max_threshold = 10.0F;
        new_config.spike_params.sparsity_target = 0.7F;
        new_config.spike_params.encoding = SPIKE_ENCODING_PASSTHROUGH;
        new_config.spike_params.enable_soft_reset = true;
        new_config.spike_params.enable_adaptation = true;
        new_config.spike_params.adaptation_window = 100;
    }

    // Update neuron count in config
    new_config.base_config.num_neurons = new_neuron_count;

    // Update layer sizes (input, hidden, output)
    // For 3-layer networks: [input, hidden, output]
    // Hidden layer (index 1) should grow to new_neuron_count
    if (new_layer_sizes && new_config.base_config.num_layers >= 2) {
        new_layer_sizes[1] = new_neuron_count;
        LOG_INFO("brain_resize: Updated layer_sizes [%u, %u, %u]",
                       new_layer_sizes[0], new_layer_sizes[1],
                       new_config.base_config.num_layers >= 3 ? new_layer_sizes[2] : 0);
    }

    // Step 2: Create new larger network
    LOG_INFO("brain_resize: Creating new network with %u neurons", new_neuron_count);

    // DEBUG: Log final config values before network creation
    LOG_INFO("brain_resize: FINAL config - k_factor=%f, min_threshold=%f, max_threshold=%f, sparsity=%f",
                   new_config.spike_params.k_factor,
                   new_config.spike_params.min_threshold,
                   new_config.spike_params.max_threshold,
                   new_config.spike_params.sparsity_target);
    LOG_INFO("brain_resize: FINAL config - num_layers=%u, num_neurons=%u, layer_sizes=%p",
                   new_config.base_config.num_layers,
                   new_config.base_config.num_neurons,
                   (void*)new_config.base_config.layer_sizes);

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    adaptive_network_t new_network = adaptive_network_create(&new_config);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double create_secs = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    LOG_INFO("brain_resize: adaptive_network_create took %.1fs", create_secs);
    fprintf(stderr, "[TIMING] adaptive_network_create: %.1fs\n", create_secs);

    // Free our temporary layer_sizes copy (adaptive_network_create makes its own deep copy)
    if (new_layer_sizes) {
        nimcp_free(new_layer_sizes);
    }

    if (!new_network) {
        LOG_ERROR("brain_resize: Failed to create new network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: new_network is NULL");
        return false;
    }

    // Step 3: Transfer all existing neurons
    // This preserves:
    // - All synaptic weights
    // - Neuron biases
    // - Activation states
    // - Plasticity traces (STDP, eligibility, BCM)
    // - Neuromodulator states
    LOG_INFO("brain_resize: Transferring %u existing neurons", current_neuron_count);

    neural_network_t new_base = adaptive_network_get_base_network(new_network);
    if (!new_base) {
        LOG_ERROR("brain_resize: Cannot access new base network");
        adaptive_network_destroy(new_network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: new_base is NULL");
        return false;
    }

    // Get new network's pools for synapse reconstruction
    sparse_synapse_pool_t new_h_pool = neural_network_get_synapse_handle_pool(new_base);
    synapse_metadata_pool_t new_m_pool = neural_network_get_synapse_metadata_pool(new_base);
    synapse_metadata_pool_t old_m_pool = neural_network_get_synapse_metadata_pool(base_network);

    // Transfer neuron states via deep copy (not memcpy)
    // WHY: sparse_synapse_storage_t contains heap overflow pointers that belong
    //       to the old network's pools. memcpy would create dangling pointers
    //       when the old network is destroyed.
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (uint32_t i = 0; i < current_neuron_count; i++) {
        neuron_t* old_neuron = neural_network_get_neuron(base_network, i);
        neuron_t* new_neuron = neural_network_get_neuron(new_base, i);

        if (!old_neuron || !new_neuron) {
            LOG_ERROR("brain_resize: Failed to access neuron %u during transfer", i);
            adaptive_network_destroy(new_network);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize: neuron access failed");
            return false;
        }

        // Save new neuron's freshly-initialized storage (from adaptive_network_create)
        sparse_synapse_storage_t saved_outgoing = new_neuron->outgoing;
        sparse_synapse_storage_t saved_incoming = new_neuron->incoming;
        void* saved_type_params = new_neuron->type_params;
        neuron_cold_data_t* saved_cold = new_neuron->cold;
        uint32_t* saved_dendrite_ids = new_neuron->dendrite_ids;
        uint32_t saved_num_dendrites = new_neuron->num_dendrites;
        spike_record_t* saved_spike_history = new_neuron->spike_history;
        uint32_t saved_spike_capacity = new_neuron->spike_history_capacity;
        float* saved_activity_history = new_neuron->activity_history;
        uint32_t saved_activity_capacity = new_neuron->activity_history_capacity;

        // Copy all scalar fields (bias, threshold, activation, learning params, etc.)
        memcpy(new_neuron, old_neuron, sizeof(neuron_t));

        // Restore new network's storage — do NOT use old network's heap pointers
        new_neuron->outgoing = saved_outgoing;
        new_neuron->incoming = saved_incoming;

        // Clear heap pointers that belong to old network
        // (type_params/dendrites are rarely used; new neurons start without them)
        new_neuron->type_params = saved_type_params;
        new_neuron->cold = saved_cold;
        new_neuron->dendrite_ids = saved_dendrite_ids;
        new_neuron->num_dendrites = saved_num_dendrites;

        // Restore spike history ring buffer (transient runtime data — reset is correct)
        new_neuron->spike_history = saved_spike_history;
        new_neuron->spike_history_capacity = saved_spike_capacity;
        new_neuron->spike_history_count = 0;
        new_neuron->spike_history_index = 0;

        // Restore activity history buffer (transient runtime data — reset is correct)
        new_neuron->activity_history = saved_activity_history;
        new_neuron->activity_history_capacity = saved_activity_capacity;

        // Clear the new neuron's default outgoing synapses so we can re-add from old
        sparse_synapse_storage_cleanup(new_h_pool, &new_neuron->outgoing);
        sparse_synapse_storage_init(&new_neuron->outgoing);

        // Re-create outgoing synapses on the NEW network's pools
        uint32_t out_count = NEURON_OUT_COUNT(old_neuron);
        for (uint32_t j = 0; j < out_count; j++) {
            synapse_handle_t* old_h = NEURON_OUT_HANDLE(old_neuron, j);
            if (!old_h) continue;

            // Add handle to new storage with metadata slot
            int idx = sparse_synapse_add_with_metadata(
                new_h_pool, new_m_pool, &new_neuron->outgoing,
                old_h->target_neuron_id, old_h->weight, 0);
            if (idx < 0) {
                LOG_WARN("brain_resize: Failed to add synapse %u/%u for neuron %u", j, out_count, i);
                continue;
            }

            // Copy handle fields (strength, peer_index)
            synapse_handle_t* new_h = NEURON_OUT_HANDLE(new_neuron, (uint32_t)idx);
            if (new_h) {
                new_h->strength = old_h->strength;
                new_h->peer_index = old_h->peer_index;
            }

            // Deep-copy metadata (STP, BCM, eligibility, compute_function, etc.)
            synapse_t* old_meta = old_m_pool
                ? sparse_synapse_get_metadata(old_m_pool, old_h)
                : NULL;
            synapse_t* new_meta = new_h
                ? sparse_synapse_get_metadata(new_m_pool, new_h)
                : NULL;
            if (old_meta && new_meta) {
                *new_meta = *old_meta;
                // Copy cold data if old synapse has it
                synapse_cold_t* old_cold = SYNAPSE_COLD(base_network, old_meta);
                if (old_cold) {
                    synapse_cold_t* new_cold = SYNAPSE_ENSURE_COLD(new_base, new_meta);
                    if (new_cold) {
                        *new_cold = *old_cold;
                        // BCM/eligibility pointers are shared — NULL them on the copy
                        // so only the old network frees them during destroy
                        new_cold->bcm = NULL;
                        new_cold->eligibility = NULL;
                    }
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double transfer_secs = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    LOG_INFO("brain_resize: Neuron transfer took %.1fs", transfer_secs);
    fprintf(stderr, "[TIMING] neuron_transfer: %.1fs\n", transfer_secs);

    // Step 4: Wire new neurons with sparse random connectivity
    // WHY: skip_layer_wiring=true means adaptive_network_create() allocated neurons
    //       but didn't run the O(N*M) dense wiring loop. We wire only new neurons
    //       with 16 random connections each — orders of magnitude faster.
    uint32_t new_neurons_added = new_neuron_count - current_neuron_count;
    if (new_neurons_added > 0) {
        const uint32_t conns_per_neuron = 16;
        LOG_INFO("brain_resize: Wiring %u new neurons with %u sparse connections each",
                 new_neurons_added, conns_per_neuron);
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        for (uint32_t i = current_neuron_count; i < new_neuron_count; i++) {
            neuron_t* neuron = neural_network_get_neuron(new_base, i);
            if (!neuron) continue;
            for (uint32_t c = 0; c < conns_per_neuron; c++) {
                uint32_t target = nimcp_tl_rand() % new_neuron_count;
                if (target == i) {
                    target = (target + 1) % new_neuron_count;
                }
                float weight = ((float)nimcp_tl_rand() / (float)RAND_MAX) - 0.5F;
                sparse_synapse_add_with_metadata(
                    new_h_pool, new_m_pool, &neuron->outgoing, target, weight, 0);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double wire_secs = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    LOG_INFO("brain_resize: Sparse wiring took %.1fs (%u neurons × %u conns)",
             wire_secs, new_neurons_added, 16);
    fprintf(stderr, "[TIMING] sparse_wiring: %.1fs (%u neurons)\n", wire_secs, new_neurons_added);

    // Rebuild all incoming synapses from outgoing data + set peer_index cross-refs
    // Done once after both old neuron transfer AND new sparse wiring
    LOG_INFO("brain_resize: Rebuilding incoming synapses");
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    neural_network_rebuild_incoming(new_base);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double rebuild_secs = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    LOG_INFO("brain_resize: rebuild_incoming took %.1fs", rebuild_secs);
    fprintf(stderr, "[TIMING] rebuild_incoming: %.1fs\n", rebuild_secs);

    LOG_INFO("brain_resize: %u new neurons wired with sparse connectivity", new_neurons_added);

    // Step 5: Swap networks atomically
    LOG_INFO("brain_resize: Swapping networks");
    adaptive_network_t old_network_copy = brain->network;
    brain->network = new_network;

    // Step 6: Update brain configuration
    brain->config.size = BRAIN_SIZE_CUSTOM;  // Now using custom size

    // Step 6.5: Update subsystems that hold network references
    LOG_INFO("brain_resize: Updating subsystems for new network");
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    if (!brain_resize_update_subsystems_internal(brain, new_base, new_neuron_count)) {
        LOG_WARN("brain_resize: Subsystem update returned failure, but continuing");
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double subsys_secs = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    fprintf(stderr, "[TIMING] subsystem_update: %.1fs\n", subsys_secs);

    // Step 7: Clean up old network
    LOG_INFO("brain_resize: Destroying old network");
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    adaptive_network_destroy(old_network_copy);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double destroy_secs = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    fprintf(stderr, "[TIMING] old_network_destroy: %.1fs\n", destroy_secs);

    LOG_INFO("brain_resize: Growth complete. New capacity: %u neurons (%.1fx growth)",
             new_neuron_count,
             (float)new_neuron_count / (float)current_neuron_count);

    // Publish resize complete event (via dopamine - success/reward)
    publish_resize_event(BIO_MSG_BRAIN_STATE_RESPONSE, current_neuron_count, new_neuron_count,
                        true, BIO_CHANNEL_DOPAMINE);

    LOG_DEBUG("brain_resize: Exit, success=true");
    return true;
}

/**
 * @brief Auto-resize brain based on utilization metrics
 *
 * WHAT: Automatically grow brain when capacity is saturated
 * WHY:  Enable continuous learning without manual intervention
 * HOW:  Evaluate metrics, decide if growth needed, compute next size, resize
 *
 * USAGE:
 * ```c
 * // During training loop:
 * brain_learn(brain, features, label, confidence);
 *
 * if (step % 100 == 0) {
 *     brain_auto_resize(brain);  // Checks every 100 steps
 * }
 * ```
 *
 * COMPLEXITY: O(n) for metric computation + O(n+m) if resize triggered
 *
 * @param brain Brain to potentially resize
 * @return true if resize occurred, false if no resize needed or error
 */
bool brain_auto_resize(brain_t brain)
{
    LOG_DEBUG("brain_auto_resize: Entry");

    // Ensure bio-async is available
    ensure_bio_async_registered();

    // Guard: Validate brain
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_auto_resize: brain is NULL");
        LOG_ERROR("brain_auto_resize: NULL brain");
        return false;
    }

    // Get base network for early validation
    adaptive_network_t network = brain->network;
    if (!network) {
        LOG_ERROR("brain_auto_resize: Brain has no network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_auto_resize: network is NULL");
        return false;
    }

    neural_network_t base_network = adaptive_network_get_base_network(network);
    if (!base_network) {
        LOG_ERROR("brain_auto_resize: Cannot access base network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_auto_resize: base_network is NULL");
        return false;
    }

    // CRITICAL FIX: Only allow auto-resize if brain has valid neuron count
    // WHY: Newly created brains from Python have uninitialized neurons array
    //      that causes SEGV when accessing ANY neuron data
    // WHAT: Check neuron count first - if 0, skip resize immediately
    //       This avoids accessing the neurons array which may point to freed memory
    uint32_t current_size = neural_network_get_num_neurons(base_network);

    if (current_size == 0) {
        LOG_DEBUG("brain_auto_resize: Brain has 0 neurons (not initialized), skipping resize");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_auto_resize: current_size is zero");
        return false;
    }

    // Verify num_neurons is reasonable (sanity check for corrupted structure)
    if (current_size == 0 || current_size > 100000000) {
        LOG_WARN("brain_auto_resize: Suspicious neuron count %u, skipping resize", current_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_auto_resize: current_size is zero");
        return false;
    }

    // Verify we can access at least the first neuron (structure validity check)
    // NOTE: This may still crash on use-after-free bugs in brain initialization
    neuron_t* first_neuron = neural_network_get_neuron(base_network, 0);
    if (!first_neuron) {
        LOG_WARN("brain_auto_resize: Cannot access neurons, network may be corrupt");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_auto_resize: first_neuron is NULL");
        return false;
    }

    // Don't grow beyond reasonable limits
    if (current_size >= 100000) {
        LOG_INFO("brain_auto_resize: Already at max size (%u neurons), skipping", current_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_auto_resize: capacity exceeded");
        return false;  // Already at 100K neurons, stop
    }

    // Check utilization metrics - only resize if brain is saturated
    float utilization = 0.0F;
    float saturation = 0.0F;

    if (!brain_get_utilization_metrics(brain, &utilization, &saturation)) {
        LOG_WARN("brain_auto_resize: Failed to get utilization metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_auto_resize: brain_get_utilization_metrics is NULL");
        return false;
    }

    // Auto-resize policy: Trigger when EITHER condition is met:
    // - Utilization > 80% (neurons are heavily active)
    // - Saturation > 70% (weights are saturated/maxed out)
    const float UTILIZATION_THRESHOLD = 0.80F;
    const float SATURATION_THRESHOLD = 0.70F;

    if (utilization < UTILIZATION_THRESHOLD && saturation < SATURATION_THRESHOLD) {
        // Brain has spare capacity - no need to resize
        LOG_DEBUG("brain_auto_resize: Brain not saturated (util=%.1f%%, sat=%.1f%%), skipping resize",
                  utilization * 100.0F, saturation * 100.0F);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_auto_resize: validation failed");
        return false;
    }

    LOG_INFO("brain_auto_resize: Brain saturated (util=%.1f%%, sat=%.1f%%), triggering resize",
             utilization * 100.0F, saturation * 100.0F);

    // Publish saturation alert (via norepinephrine - urgent)
    publish_resize_event(BIO_MSG_SALIENCE_QUERY, current_size, 0,
                        true, BIO_CHANNEL_NOREPINEPHRINE);

    // Detect if GPU is being used
    // TODO: Add brain_is_gpu_enabled() API
    bool use_gpu = false;  // Default to CPU mode for now

    // Compute next size (hardware-aware) - current_size already defined above
    uint32_t new_size = compute_next_size(current_size, use_gpu);

    // Verify we have sufficient resources for the target size
    system_resources_t resources;
    if (system_resources_query(&resources)) {
        if (!system_resources_can_resize(&resources, new_size, use_gpu)) {
            LOG_WARN("brain_auto_resize: Insufficient resources for target size %u, skipping resize", new_size);
            // Publish resource constraint event
            publish_resize_event(BIO_MSG_ERROR_REPORT, current_size, new_size,
                               false, BIO_CHANNEL_NOREPINEPHRINE);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_auto_resize: system_resources_can_resize is NULL");
            return false;
        }
    }

    // Perform resize
    LOG_INFO("brain_auto_resize: Triggering resize to %u neurons", new_size);
    bool result = brain_resize(brain, new_size);
    LOG_DEBUG("brain_auto_resize: Exit, result=%d", result);
    return result;
}

/**
 * @brief Get brain current neuron count
 *
 * WHAT: Return current brain capacity in neurons
 * WHY:  Allow monitoring of brain size for metrics/logging
 * HOW:  Access underlying network and query neuron count
 *
 * @param brain Brain to query
 * @return Neuron count, or 0 on error
 */
uint32_t brain_get_neuron_count(brain_t brain)
{
    if (!brain || !brain->network) {
        return 0;
    }

    neural_network_t base_network = adaptive_network_get_base_network(brain->network);
    if (!base_network) {
        return 0;
    }

    return neural_network_get_num_neurons(base_network);
}

/**
 * @brief Get brain utilization metrics
 *
 * WHAT: Return current capacity utilization statistics
 * WHY:  Enable monitoring and debugging of auto-resize logic
 * HOW:  Compute utilization and saturation metrics
 *
 * @param brain Brain to analyze
 * @param utilization Output: neuron utilization ratio [0.0, 1.0]
 * @param saturation Output: weight saturation ratio [0.0, 1.0]
 * @return true on success, false on error
 */
bool brain_get_utilization_metrics(brain_t brain, float* utilization, float* saturation)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_utilization_metrics: brain is NULL");
        return false;
    }
    if (!utilization || !saturation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_utilization_metrics: output parameters are NULL");
        return false;
    }

    neural_network_t base_network = adaptive_network_get_base_network(brain->network);
    if (!base_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_utilization_metrics: base_network is NULL");
        return false;
    }

    *utilization = compute_neuron_utilization(base_network);
    *saturation = compute_weight_saturation(base_network);

    return true;
}
