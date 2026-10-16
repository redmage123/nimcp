#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_portia_swarm_logic_bridge.c - Unified Portia-Swarm-Logic Bridge Implementation
//=============================================================================

#include "portia/nimcp_portia_swarm_logic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "portia/nimcp_portia_swarm_bridge.h"
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_swarm_logic_bridge)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal bridge context
 */
struct portia_swarm_logic_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    // Connected bridges
    portia_logic_bridge_t* portia_logic;      /**< Portia logic bridge (may be NULL) */
    swarm_logic_bridge_t* swarm_logic;        /**< Swarm logic bridge (may be NULL) */
    portia_swarm_bridge_t* portia_swarm;      /**< Portia-Swarm bridge (may be NULL) */

    // Brain integration
    brain_t brain;                             /**< Brain for neuromodulation */

    // Immune system integration
    void* immune_system;                       /**< Brain immune system */

    // UMM integration
    void* umm;                                 /**< Unified Memory Manager */

    // Perception/Cortical training integration
    perception_training_bridge_t* perception_training; /**< Perception training bridge */
    cortical_training_bridge_t* cortical_training;     /**< Cortical training bridge */

    // Cross-module decision gates
    uint32_t collective_resource_gate;         /**< AND: local_ok AND swarm_ok */
    uint32_t emergency_mode_gate;              /**< OR: local_critical OR swarm_alert */
    uint32_t swarm_tier_gate;                  /**< IMPLIES: swarm_recommends IMPLIES local_can */
    uint32_t coordinated_degradation_gate;     /**< OR: local_degrade OR swarm_degrade */

    // Configuration
    portia_swarm_logic_config_t config;        /**< Bridge configuration */

    // Bio-async
    // Thread safety
    nimcp_mutex_t* mutex;                    /**< Mutex for thread safety */

    // Statistics
    portia_swarm_logic_stats_t stats;          /**< Operational statistics */

    // Neural logic network for unified gates
    neural_logic_network_t logic_network;      /**< Neural logic network */

    // State flags
    bool started;                              /**< Bridge is started */
};

//=============================================================================
// Configuration API
//=============================================================================

void portia_swarm_logic_default_config(portia_swarm_logic_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        return;
    }

    config->mode = PSL_MODE_COORDINATED;
    config->local_weight = 0.5f;
    config->collective_weight = 0.5f;
    config->enable_bio_async = true;
    config->enable_brain_integration = false;
    config->enable_immune_feedback = false;
    config->enable_umm_tracking = false;
    config->consensus_timeout_ms = PSL_DEFAULT_CONSENSUS_TIMEOUT_MS;
    config->confidence_threshold = PSL_DEFAULT_CONFIDENCE_THRESHOLD;
}

//=============================================================================
// Lifecycle API
//=============================================================================

portia_swarm_logic_bridge_t* portia_swarm_logic_create(
    const portia_swarm_logic_config_t* config,
    portia_logic_bridge_t* portia_logic,
    swarm_logic_bridge_t* swarm_logic,
    portia_swarm_bridge_t* portia_swarm)
{
    // Allocate bridge
    portia_swarm_logic_bridge_t* bridge = (portia_swarm_logic_bridge_t*)
        nimcp_malloc(sizeof(portia_swarm_logic_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(portia_swarm_logic_bridge_t));

    // Set configuration
    if (config) {
        bridge->config = *config;
    } else {
        portia_swarm_logic_default_config(&bridge->config);
    }

    // Connect bridges
    bridge->portia_logic = portia_logic;
    bridge->swarm_logic = swarm_logic;
    bridge->portia_swarm = portia_swarm;

    // Initialize bridge base (creates mutex, sets name)
    if (bridge_base_init(&bridge->base, 0, "portia_swarm_logic") != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_swarm_logic_create: bridge_base_init failed");
        return NULL;
    }

    // Create neural logic network for unified gates
    neural_logic_config_t logic_config = neural_logic_default_config(1000);
    logic_config.enable_bio_async = false;  // Bridge handles bio-async
    bridge->logic_network = neural_logic_create(&logic_config);
    if (!bridge->logic_network) {
        NIMCP_LOGGING_ERROR("Failed to create neural logic network");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_swarm_logic_create: bridge->logic_network is NULL");
        return NULL;
    }

    // Initialize standard unified gates
    bridge->collective_resource_gate = UINT32_MAX;
    bridge->emergency_mode_gate = UINT32_MAX;
    bridge->swarm_tier_gate = UINT32_MAX;
    bridge->coordinated_degradation_gate = UINT32_MAX;

    NIMCP_LOGGING_INFO("Unified Portia-Swarm-Logic bridge created");
    return bridge;
}

void portia_swarm_logic_destroy(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    // Stop if running
    if (bridge->started) {
        portia_swarm_logic_stop(bridge);
    }

    // Disconnect bio-async
    if (bridge->base.bio_async_enabled) {
        portia_swarm_logic_disconnect_bio_async(bridge);
    }

    // Destroy neural logic network
    if (bridge->logic_network) {
        neural_logic_destroy(bridge->logic_network);
    }

    // Cleanup bridge base (destroys mutex)
    bridge_base_cleanup(&bridge->base);

    // Free bridge
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Unified bridge destroyed");
}

int portia_swarm_logic_start(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->started) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("Bridge already started");
        return 0;
    }

    // Connect bio-async if enabled
    if (bridge->config.enable_bio_async && !bridge->base.bio_async_enabled) {
        int ret = portia_swarm_logic_connect_bio_async(bridge);
        if (ret != 0) {
            NIMCP_LOGGING_WARN("Failed to connect bio-async, continuing without it");
        }
    }

    bridge->started = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Unified bridge started");
    return 0;
}

int portia_swarm_logic_stop(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->started) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("Bridge already stopped");
        return 0;
    }

    bridge->started = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Unified bridge stopped");
    return 0;
}

//=============================================================================
// Integration API
//=============================================================================

int portia_swarm_logic_connect_brain(
    portia_swarm_logic_bridge_t* bridge,
    brain_t brain)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->brain = brain;

    // Connect brain to neural logic network for neuromodulation
    if (bridge->logic_network && brain) {
        neural_logic_set_brain(bridge->logic_network, brain);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Brain connected to unified bridge");
    return 0;
}

int portia_swarm_logic_connect_immune(
    portia_swarm_logic_bridge_t* bridge,
    void* immune_system)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_system = immune_system;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Immune system connected to unified bridge");
    return 0;
}

int portia_swarm_logic_connect_umm(
    portia_swarm_logic_bridge_t* bridge,
    void* umm)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->umm = umm;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("UMM connected to unified bridge");
    return 0;
}

int portia_swarm_logic_connect_perception_training(
    portia_swarm_logic_bridge_t* bridge,
    perception_training_bridge_t* perception_training)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->perception_training = perception_training;
    bridge->stats.perception_training_connected = (perception_training != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    if (perception_training) {
        NIMCP_LOGGING_INFO("Perception training connected to unified bridge");
    } else {
        NIMCP_LOGGING_INFO("Perception training disconnected from unified bridge");
    }
    return 0;
}

int portia_swarm_logic_connect_cortical_training(
    portia_swarm_logic_bridge_t* bridge,
    cortical_training_bridge_t* cortical_training)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->cortical_training = cortical_training;
    bridge->stats.cortical_training_connected = (cortical_training != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    if (cortical_training) {
        NIMCP_LOGGING_INFO("Cortical training connected to unified bridge");
    } else {
        NIMCP_LOGGING_INFO("Cortical training disconnected from unified bridge");
    }
    return 0;
}

float portia_swarm_logic_get_perception_confidence_modifier(
    const portia_swarm_logic_bridge_t* bridge)
{
    if (!bridge || !bridge->perception_training) {
        return 1.0f;  /* No modulation if not connected */
    }

    /* Get perception effects and compute confidence modifier */
    perception_training_effects_t effects;
    if (perception_training_get_effects(bridge->perception_training, &effects) != 0) {
        return 1.0f;
    }

    if (!effects.valid) {
        return 1.0f;
    }

    /*
     * BIOLOGICAL BASIS: Perceptual clarity influences decision confidence.
     * High visual confidence + high LR factor → boost decision confidence.
     * Low perception quality → reduce confidence (proceed cautiously).
     *
     * modifier = 0.5 + 0.5 × visual_confidence × lr_factor
     * Range: [0.5, 1.5] clamped
     */
    float confidence_base = 0.5f + 0.5f * effects.visual_confidence;
    float modifier = confidence_base * effects.lr_factor;

    /* Clamp to [0.5, 1.5] */
    if (modifier < 0.5f) modifier = 0.5f;
    if (modifier > 1.5f) modifier = 1.5f;

    return modifier;
}

float portia_swarm_logic_get_cortical_threshold_modifier(
    const portia_swarm_logic_bridge_t* bridge)
{
    if (!bridge || !bridge->cortical_training) {
        return 1.0f;  /* No modulation if not connected */
    }

    /* Get cortical effects and compute threshold modifier */
    cortical_training_effects_t effects;
    if (cortical_training_get_effects(bridge->cortical_training, &effects) != 0) {
        return 1.0f;
    }

    if (!effects.valid) {
        return 1.0f;
    }

    /*
     * BIOLOGICAL BASIS: Cortical stability influences decision thresholds.
     * High burst rate + stable predictions → lower thresholds (confident).
     * Prediction failure + high error → higher thresholds (cautious).
     *
     * Base threshold modifier from burst rate:
     * - High burst (>0.7) → lower threshold (0.7-0.85)
     * - Low burst (<0.3) → higher threshold (1.15-1.3)
     * - Medium burst → moderate threshold (~1.0)
     *
     * modifier = 1.0 - 0.3 × (burst_rate - 0.5)
     * If predictions unstable, add 0.15
     */
    float modifier = 1.0f - 0.3f * (effects.burst_rate - 0.5f);

    if (!effects.predictions_stable) {
        modifier += 0.15f;  /* More cautious when unstable */
    }

    /* Apply prediction error penalty */
    modifier += 0.1f * effects.prediction_error_mag;

    /* Clamp to [0.7, 1.3] */
    if (modifier < 0.7f) modifier = 0.7f;
    if (modifier > 1.3f) modifier = 1.3f;

    return modifier;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Evaluate local (Portia) decision
 *
 * WHAT: Checks if local Portia logic approves decision
 * WHY:  Individual resource constraints must be satisfied
 * HOW:  Queries portia_logic if available, otherwise returns true
 */
static bool evaluate_local_decision(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t decision_type,
    float* confidence_out)
{
    (void)decision_type;  // Reserved for future gate selection

    // If no Portia logic bridge, default to approve with low confidence
    if (!bridge->portia_logic) {
        if (confidence_out) *confidence_out = 0.3f;
        return true;
    }

    // In MODE_DISABLED or MODE_SWARM_ONLY, skip local evaluation
    if (bridge->config.mode == PSL_MODE_DISABLED ||
        bridge->config.mode == PSL_MODE_SWARM_ONLY) {
        if (confidence_out) *confidence_out = 0.0f;
        return false;  /* Normal flow - local evaluation skipped in this mode */
    }

    // TODO: When portia_logic_bridge API is available, evaluate actual gates
    // For now, simulate with simple heuristic
    if (confidence_out) *confidence_out = 0.7f;
    return true;
}

/**
 * @brief Evaluate swarm consensus decision
 *
 * WHAT: Checks if swarm consensus approves decision
 * WHY:  Collective intelligence guides individual actions
 * HOW:  Queries swarm_logic if available, otherwise returns true
 */
static bool evaluate_swarm_decision(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t decision_type,
    uint32_t* consensus_count_out,
    float* confidence_out)
{
    (void)decision_type;  // Reserved for future gate selection

    // If no Swarm logic bridge, default to approve with low confidence
    if (!bridge->swarm_logic) {
        if (consensus_count_out) *consensus_count_out = 0;
        if (confidence_out) *confidence_out = 0.3f;
        return true;
    }

    // In MODE_DISABLED or MODE_PORTIA_ONLY, skip swarm evaluation
    if (bridge->config.mode == PSL_MODE_DISABLED ||
        bridge->config.mode == PSL_MODE_PORTIA_ONLY) {
        if (consensus_count_out) *consensus_count_out = 0;
        if (confidence_out) *confidence_out = 0.0f;
        return false;  /* Normal flow - swarm evaluation skipped in this mode */
    }

    // TODO: When swarm_logic_bridge API expands, evaluate actual consensus
    // For now, simulate with simple heuristic
    if (consensus_count_out) *consensus_count_out = 5;
    if (confidence_out) *confidence_out = 0.8f;
    return true;
}

/**
 * @brief Combine local and swarm decisions
 *
 * WHAT: Merges local and swarm decisions using configured weights
 * WHY:  Unified decision must balance individual and collective needs
 * HOW:  Weighted average of confidences, logical combination of approvals
 */
static void combine_decisions(
    portia_swarm_logic_bridge_t* bridge,
    bool local_approved,
    float local_confidence,
    bool swarm_approved,
    float swarm_confidence,
    unified_decision_result_t* result)
{
    result->local_approved = local_approved;
    result->swarm_approved = swarm_approved;

    // Compute weighted confidence
    float total_weight = bridge->config.local_weight + bridge->config.collective_weight;
    if (total_weight > 0.0f) {
        result->confidence = (local_confidence * bridge->config.local_weight +
                            swarm_confidence * bridge->config.collective_weight) / total_weight;
    } else {
        result->confidence = 0.5f;
    }

    // Combine approvals based on mode
    switch (bridge->config.mode) {
        case PSL_MODE_PORTIA_ONLY:
            result->approved = local_approved;
            snprintf(result->reason, PSL_MAX_REASON_LENGTH, "Local decision only");
            break;

        case PSL_MODE_SWARM_ONLY:
            result->approved = swarm_approved;
            snprintf(result->reason, PSL_MAX_REASON_LENGTH, "Swarm consensus only");
            break;

        case PSL_MODE_COORDINATED:
            // Both must approve
            result->approved = local_approved && swarm_approved;
            if (result->approved) {
                snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                        "Coordinated approval (local + swarm)");
            } else {
                snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                        "Denied: %s%s",
                        local_approved ? "" : "local_denied ",
                        swarm_approved ? "" : "swarm_denied");
            }
            break;

        case PSL_MODE_CONSENSUS_REQUIRED:
            // Swarm must approve, local is advisory
            result->approved = swarm_approved;
            if (!local_approved) {
                snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                        "Swarm consensus overrides local denial");
            } else {
                snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                        "Consensus achieved");
            }
            break;

        default:
            result->approved = false;
            snprintf(result->reason, PSL_MAX_REASON_LENGTH, "Invalid mode");
            break;
    }

    // Check confidence threshold
    if (result->approved && result->confidence < bridge->config.confidence_threshold) {
        result->approved = false;
        snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                "Confidence (%.2f) below threshold (%.2f)",
                result->confidence, bridge->config.confidence_threshold);
    }
}

//=============================================================================
// Unified Decision API
//=============================================================================

int portia_swarm_logic_decide_tier_change(
    portia_swarm_logic_bridge_t* bridge,
    uint8_t current_tier,
    uint8_t proposed_tier,
    unified_decision_result_t* result)
{
    (void)current_tier;   // Reserved for tier-specific logic gates
    (void)proposed_tier;  // Reserved for tier-specific logic gates

    if (!bridge || !result) {
        NIMCP_LOGGING_ERROR("Null pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->started) {
        NIMCP_LOGGING_ERROR("Bridge not started");
        return NIMCP_ERROR_INVALID_STATE;
    }

    uint64_t start_time = get_timestamp_us();
    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(unified_decision_result_t));

    // Evaluate local decision
    float local_confidence = 0.0f;
    bool local_approved = evaluate_local_decision(bridge, 0, &local_confidence);

    // Evaluate swarm consensus
    uint32_t consensus_count = 0;
    float swarm_confidence = 0.0f;
    bool swarm_approved = evaluate_swarm_decision(bridge, 0, &consensus_count, &swarm_confidence);

    result->swarm_consensus_count = consensus_count;

    // Combine decisions
    combine_decisions(bridge, local_approved, local_confidence,
                     swarm_approved, swarm_confidence, result);

    // Update statistics
    bridge->stats.total_decisions++;
    if (local_approved && !swarm_approved) {
        bridge->stats.local_decisions++;
    } else if (!local_approved && swarm_approved) {
        bridge->stats.collective_decisions++;
    }

    if (swarm_approved) {
        bridge->stats.consensus_achieved++;
        bridge->stats.avg_consensus_confidence =
            (bridge->stats.avg_consensus_confidence * (bridge->stats.consensus_achieved - 1) +
             swarm_confidence) / bridge->stats.consensus_achieved;
    } else {
        bridge->stats.consensus_failed++;
    }

    uint64_t end_time = get_timestamp_us();
    result->decision_time_us = end_time - start_time;
    bridge->stats.avg_decision_time_us =
        (bridge->stats.avg_decision_time_us * (bridge->stats.total_decisions - 1) +
         result->decision_time_us) / bridge->stats.total_decisions;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Tier change decision: %s (confidence: %.2f)",
                      result->approved ? "approved" : "denied",
                      result->confidence);

    return 0;
}

int portia_swarm_logic_decide_degradation(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t feature_id,
    unified_decision_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_LOGGING_ERROR("Null pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->started) {
        NIMCP_LOGGING_ERROR("Bridge not started");
        return NIMCP_ERROR_INVALID_STATE;
    }

    uint64_t start_time = get_timestamp_us();
    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(unified_decision_result_t));

    // Evaluate local decision
    float local_confidence = 0.0f;
    bool local_approved = evaluate_local_decision(bridge, 1, &local_confidence);

    // Evaluate swarm consensus
    uint32_t consensus_count = 0;
    float swarm_confidence = 0.0f;
    bool swarm_approved = evaluate_swarm_decision(bridge, 1, &consensus_count, &swarm_confidence);

    result->swarm_consensus_count = consensus_count;

    // For degradation, use OR logic: degrade if EITHER local or swarm recommends
    result->local_approved = local_approved;
    result->swarm_approved = swarm_approved;
    result->approved = local_approved || swarm_approved;
    float weight_sum = bridge->config.local_weight + bridge->config.collective_weight;
    if (weight_sum > 0.0f) {
        result->confidence = (local_confidence * bridge->config.local_weight +
                             swarm_confidence * bridge->config.collective_weight) / weight_sum;
    } else {
        result->confidence = 0.5f;
    }

    if (result->approved) {
        snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                "Degradation triggered by %s%s",
                local_approved ? "local " : "",
                swarm_approved ? "swarm" : "");
    } else {
        snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                "No degradation needed");
    }

    // Update statistics
    bridge->stats.total_decisions++;
    if (local_approved && !swarm_approved) {
        bridge->stats.local_decisions++;
    } else if (!local_approved && swarm_approved) {
        bridge->stats.collective_decisions++;
    }

    uint64_t end_time = get_timestamp_us();
    result->decision_time_us = end_time - start_time;
    bridge->stats.avg_decision_time_us =
        (bridge->stats.avg_decision_time_us * (bridge->stats.total_decisions - 1) +
         result->decision_time_us) / bridge->stats.total_decisions;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Degradation decision for feature %u: %s",
                      feature_id, result->approved ? "degrade" : "maintain");

    return 0;
}

int portia_swarm_logic_decide_resource_allocation(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t target_id,
    float requested_amount,
    unified_decision_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_LOGGING_ERROR("Null pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->started) {
        NIMCP_LOGGING_ERROR("Bridge not started");
        return NIMCP_ERROR_INVALID_STATE;
    }

    if (requested_amount < 0.0f || requested_amount > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid requested_amount: %.2f", requested_amount);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint64_t start_time = get_timestamp_us();
    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(unified_decision_result_t));

    // Evaluate local decision
    float local_confidence = 0.0f;
    bool local_approved = evaluate_local_decision(bridge, 2, &local_confidence);

    // Evaluate swarm consensus
    uint32_t consensus_count = 0;
    float swarm_confidence = 0.0f;
    bool swarm_approved = evaluate_swarm_decision(bridge, 2, &consensus_count, &swarm_confidence);

    result->swarm_consensus_count = consensus_count;

    // Combine decisions
    combine_decisions(bridge, local_approved, local_confidence,
                     swarm_approved, swarm_confidence, result);

    // Update statistics
    bridge->stats.total_decisions++;

    uint64_t end_time = get_timestamp_us();
    result->decision_time_us = end_time - start_time;
    bridge->stats.avg_decision_time_us =
        (bridge->stats.avg_decision_time_us * (bridge->stats.total_decisions - 1) +
         result->decision_time_us) / bridge->stats.total_decisions;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Resource allocation decision for target %u (%.2f): %s",
                      target_id, requested_amount,
                      result->approved ? "approved" : "denied");

    return 0;
}

int portia_swarm_logic_decide_emergency_mode(
    portia_swarm_logic_bridge_t* bridge,
    unified_decision_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_LOGGING_ERROR("Null pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->started) {
        NIMCP_LOGGING_ERROR("Bridge not started");
        return NIMCP_ERROR_INVALID_STATE;
    }

    uint64_t start_time = get_timestamp_us();
    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(unified_decision_result_t));

    // Evaluate local decision
    float local_confidence = 0.0f;
    bool local_approved = evaluate_local_decision(bridge, 3, &local_confidence);

    // Evaluate swarm consensus
    uint32_t consensus_count = 0;
    float swarm_confidence = 0.0f;
    bool swarm_approved = evaluate_swarm_decision(bridge, 3, &consensus_count, &swarm_confidence);

    result->swarm_consensus_count = consensus_count;

    // For emergency mode, use OR logic: activate if EITHER local or swarm signals critical
    result->local_approved = local_approved;
    result->swarm_approved = swarm_approved;
    result->approved = local_approved || swarm_approved;
    float emerg_weight_sum = bridge->config.local_weight + bridge->config.collective_weight;
    if (emerg_weight_sum > 0.0f) {
        result->confidence = (local_confidence * bridge->config.local_weight +
                             swarm_confidence * bridge->config.collective_weight) / emerg_weight_sum;
    } else {
        result->confidence = 0.5f;
    }

    if (result->approved) {
        snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                "Emergency triggered by %s%s",
                local_approved ? "local " : "",
                swarm_approved ? "swarm" : "");
        bridge->stats.emergency_activations++;
    } else {
        snprintf(result->reason, PSL_MAX_REASON_LENGTH,
                "No emergency condition detected");
    }

    // Update statistics
    bridge->stats.total_decisions++;

    uint64_t end_time = get_timestamp_us();
    result->decision_time_us = end_time - start_time;
    bridge->stats.avg_decision_time_us =
        (bridge->stats.avg_decision_time_us * (bridge->stats.total_decisions - 1) +
         result->decision_time_us) / bridge->stats.total_decisions;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Emergency mode decision: %s",
                      result->approved ? "ACTIVATE" : "normal");

    return 0;
}

//=============================================================================
// Custom Gate API
//=============================================================================

int portia_swarm_logic_add_unified_gate(
    portia_swarm_logic_bridge_t* bridge,
    const char* expression,
    uint32_t* gate_id_out)
{
    if (!bridge || !expression || !gate_id_out) {
        NIMCP_LOGGING_ERROR("Null pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->logic_network) {
        NIMCP_LOGGING_ERROR("No logic network available");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // Simple expression parser for "A AND B", "A OR B", etc.
    logic_gate_type_t gate_type = LOGIC_GATE_AND;
    if (strstr(expression, "AND")) {
        gate_type = LOGIC_GATE_AND;
    } else if (strstr(expression, "OR")) {
        gate_type = LOGIC_GATE_OR;
    } else if (strstr(expression, "NOT")) {
        gate_type = LOGIC_GATE_NOT;
    } else if (strstr(expression, "XOR")) {
        gate_type = LOGIC_GATE_XOR;
    } else if (strstr(expression, "IMPLIES")) {
        gate_type = LOGIC_GATE_IMPLIES;
    } else {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_ERROR("Unsupported expression: %s", expression);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Create gate in neural logic network
    uint32_t gate_id = neural_logic_create_gate(bridge->logic_network, gate_type, 1.5f);
    if (gate_id == UINT32_MAX) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_ERROR("Failed to create logic gate");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    *gate_id_out = gate_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Created unified gate %u: %s", gate_id, expression);
    return 0;
}

bool portia_swarm_logic_evaluate_unified_gate(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t gate_id)
{
    if (!bridge || !bridge->logic_network) {
        NIMCP_LOGGING_ERROR("Invalid bridge or no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_swarm_logic_evaluate_unified_gate: required parameter is NULL (bridge, bridge->logic_network)");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // Evaluate gate with default inputs (1.0, 1.0)
    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;
    bool success = neural_logic_evaluate(bridge->logic_network, gate_id, inputs, 2, &output);

    nimcp_mutex_unlock(bridge->base.mutex);

    if (!success) {
        NIMCP_LOGGING_ERROR("Failed to evaluate gate %u", gate_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_swarm_logic_evaluate_unified_gate: success is NULL");
        return false;
    }

    return output > 0.5f;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int portia_swarm_logic_connect_bio_async(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async already connected");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_PORTIA_SWARM_LOGIC,
        .module_name = "portia_swarm_logic_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int portia_swarm_logic_disconnect_bio_async(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool portia_swarm_logic_is_bio_async_connected(const portia_swarm_logic_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}

int portia_swarm_logic_process_inbox(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    int count = bio_router_process_inbox(bridge->base.bio_ctx, 0);
    return count;
}

//=============================================================================
// Statistics API
//=============================================================================

int portia_swarm_logic_get_stats(
    const portia_swarm_logic_bridge_t* bridge,
    portia_swarm_logic_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_LOGGING_ERROR("Null pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // No lock needed for stats read (atomic-ish)
    *stats = bridge->stats;
    return 0;
}

int portia_swarm_logic_reset_stats(portia_swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(portia_swarm_logic_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Statistics reset");
    return 0;
}
