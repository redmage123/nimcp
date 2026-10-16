//=============================================================================
// nimcp_stdp_pr_bridge.h - STDP ↔ Prime Resonant Memory Bridge
//=============================================================================
/**
 * @file nimcp_stdp_pr_bridge.h
 * @brief Bidirectional integration between STDP plasticity and Prime Resonant memory
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge connecting STDP (spike-timing dependent plasticity) with Prime
 *       Resonant memory system (entanglement graph, resonance, quaternion states)
 * WHY:  STDP weight changes should update memory entanglement strengths, and
 *       PR memory resonance should modulate STDP learning rates for coherent
 *       memory-plasticity integration
 * HOW:  Bidirectional communication where:
 *       - STDP LTP/LTD events → update PR entanglement edge weights
 *       - PR resonance scores → modulate STDP learning rate
 *       - PR consolidation (quaternion.w) → gate STDP plasticity
 *       - STDP burst events → trigger PR memory access/retrieval
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 *
 *   STDP → Prime Resonant Memory:
 *   +-----------------------------------------------------------------------+
 *   |  Synaptic plasticity shapes associative memory structure:             |
 *   |                                                                        |
 *   |  1. LTP Strengthens Associations:                                     |
 *   |     - Pre-before-post → correct temporal prediction                   |
 *   |     - Increases entanglement edge weight between memories             |
 *   |     - Memories that "fire together" become more connected             |
 *   |                                                                        |
 *   |  2. LTD Weakens Spurious Associations:                                |
 *   |     - Post-before-pre → incorrect temporal prediction                 |
 *   |     - Decreases entanglement edge weight                              |
 *   |     - Prunes irrelevant memory connections                            |
 *   |                                                                        |
 *   |  3. Burst-Triggered Memory Access:                                    |
 *   |     - Dopamine burst during STDP → memory consolidation signal        |
 *   |     - Increases quaternion.w (consolidation) of associated memories   |
 *   +-----------------------------------------------------------------------+
 *
 *   Prime Resonant Memory → STDP:
 *   +-----------------------------------------------------------------------+
 *   |  Memory state modulates synaptic plasticity:                          |
 *   |                                                                        |
 *   |  1. Resonance Modulates Learning Rate:                                |
 *   |     - High resonance → enhanced STDP (memories should stay connected) |
 *   |     - Low resonance → reduced STDP (allow divergence)                 |
 *   |     - η_effective = η_base × resonance_score                          |
 *   |                                                                        |
 *   |  2. Consolidation Gates Plasticity:                                   |
 *   |     - High quaternion.w → reduced plasticity (stable memory)          |
 *   |     - Low quaternion.w → enhanced plasticity (learning phase)         |
 *   |     - Implements memory protection for consolidated memories          |
 *   |                                                                        |
 *   |  3. Memory Tier Affects STDP Parameters:                              |
 *   |     - Z0 (working) → fast, strong STDP                                |
 *   |     - Z1 (short-term) → moderate STDP                                 |
 *   |     - Z2 (long-term) → slow, weak STDP                                |
 *   |     - Z3 (permanent) → minimal STDP (near-frozen)                     |
 *   +-----------------------------------------------------------------------+
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STDP_PR_BRIDGE_H
#define NIMCP_STDP_PR_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Resonance modulation of learning rate */
#define STDP_PR_RESONANCE_LR_MIN          0.2f    /**< Min LR scaling at low resonance */
#define STDP_PR_RESONANCE_LR_MAX          2.0f    /**< Max LR scaling at high resonance */
#define STDP_PR_RESONANCE_LR_BASELINE     1.0f    /**< Baseline LR factor */

/** Consolidation gating thresholds */
#define STDP_PR_CONSOLIDATION_GATE_HIGH   0.8f    /**< Above this, strongly reduce STDP */
#define STDP_PR_CONSOLIDATION_GATE_LOW    0.3f    /**< Below this, full STDP allowed */
#define STDP_PR_CONSOLIDATION_REDUCTION   0.2f    /**< LR factor for consolidated memories */

/** Entanglement weight update factors */
#define STDP_PR_LTP_ENTANGLE_GAIN         0.1f    /**< LTP → entanglement increase */
#define STDP_PR_LTD_ENTANGLE_DECAY        0.05f   /**< LTD → entanglement decrease */
#define STDP_PR_ENTANGLE_MIN              0.01f   /**< Minimum entanglement weight */
#define STDP_PR_ENTANGLE_MAX              1.0f    /**< Maximum entanglement weight */

/** Memory tier STDP rate multipliers */
#define STDP_PR_TIER_Z0_RATE              1.0f    /**< Working memory: full STDP */
#define STDP_PR_TIER_Z1_RATE              0.7f    /**< Short-term: 70% STDP */
#define STDP_PR_TIER_Z2_RATE              0.4f    /**< Long-term: 40% STDP */
#define STDP_PR_TIER_Z3_RATE              0.1f    /**< Permanent: 10% STDP */

/** Burst consolidation thresholds */
#define STDP_PR_BURST_CONSOLIDATION_GAIN  0.05f   /**< Consolidation boost per burst */
#define STDP_PR_BURST_MIN_WEIGHT_CHANGE   0.01f   /**< Min weight change to trigger */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Memory tier indices (matching Z-Ladder)
 */
typedef enum {
    STDP_PR_TIER_Z0 = 0,  /**< Working memory */
    STDP_PR_TIER_Z1,      /**< Short-term memory */
    STDP_PR_TIER_Z2,      /**< Long-term memory */
    STDP_PR_TIER_Z3,      /**< Permanent memory */
    STDP_PR_TIER_COUNT
} stdp_pr_memory_tier_t;

/**
 * @brief STDP event type for PR notification
 */
typedef enum {
    STDP_PR_EVENT_LTP = 0,     /**< Long-term potentiation */
    STDP_PR_EVENT_LTD,         /**< Long-term depression */
    STDP_PR_EVENT_BURST_LTP,   /**< Burst-amplified LTP */
    STDP_PR_EVENT_BURST_LTD,   /**< Burst-amplified LTD */
    STDP_PR_EVENT_COUNT
} stdp_pr_event_type_t;

/**
 * @brief Configuration for STDP-PR bridge
 */
typedef struct {
    /* Resonance modulation */
    float resonance_lr_min;           /**< Min LR at low resonance */
    float resonance_lr_max;           /**< Max LR at high resonance */
    float resonance_sensitivity;      /**< Resonance → LR scaling curve */

    /* Consolidation gating */
    float consolidation_gate_high;    /**< High consolidation threshold */
    float consolidation_gate_low;     /**< Low consolidation threshold */
    float consolidation_reduction;    /**< LR factor for consolidated */
    bool enable_consolidation_gate;   /**< Enable consolidation gating */

    /* Entanglement updates */
    float ltp_entangle_gain;          /**< LTP → entanglement increase */
    float ltd_entangle_decay;         /**< LTD → entanglement decrease */
    bool enable_entangle_updates;     /**< Enable entanglement updates */

    /* Tier-based modulation */
    float tier_rates[STDP_PR_TIER_COUNT]; /**< Per-tier STDP rates */
    bool enable_tier_modulation;      /**< Enable tier-based STDP */

    /* Burst consolidation */
    float burst_consolidation_gain;   /**< Consolidation boost per burst */
    bool enable_burst_consolidation;  /**< Enable burst → consolidation */

    /* Bio-async messaging */
    bool enable_bio_async;            /**< Enable async messaging */
} stdp_pr_bridge_config_t;

/**
 * @brief STDP effects on PR memory (forward direction)
 */
typedef struct {
    uint64_t source_node_id;          /**< Source memory node */
    uint64_t target_node_id;          /**< Target memory node */
    stdp_pr_event_type_t event_type;  /**< Type of STDP event */
    float weight_change;              /**< Synaptic weight change */
    float entangle_delta;             /**< Entanglement weight delta */
    float consolidation_boost;        /**< Consolidation (quat.w) boost */
    uint64_t timestamp_ms;            /**< Event timestamp */
} stdp_pr_forward_effect_t;

/**
 * @brief PR memory effects on STDP (backward direction)
 */
typedef struct {
    float resonance_score;            /**< Current resonance [0, 1] */
    float consolidation_level;        /**< Quaternion.w [0, 1] */
    stdp_pr_memory_tier_t memory_tier; /**< Current Z-ladder tier */
    float lr_modulation;              /**< Computed LR modulation factor */
    float effective_a_plus;           /**< Modulated A+ amplitude */
    float effective_a_minus;          /**< Modulated A- amplitude */
    bool plasticity_allowed;          /**< Is plasticity allowed? */
} stdp_pr_backward_effect_t;

/**
 * @brief Bridge state for tracking
 */
typedef struct {
    float current_resonance;          /**< Latest resonance score */
    float current_consolidation;      /**< Latest consolidation level */
    stdp_pr_memory_tier_t current_tier; /**< Current memory tier */
    float cumulative_entangle_delta;  /**< Accumulated entanglement change */
    uint64_t last_ltp_time_ms;        /**< Last LTP event time */
    uint64_t last_ltd_time_ms;        /**< Last LTD event time */
    float bridge_coherence;           /**< STDP-PR alignment [0, 1] */
} stdp_pr_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Event counts */
    uint64_t ltp_events;              /**< LTP event count */
    uint64_t ltd_events;              /**< LTD event count */
    uint64_t burst_events;            /**< Burst event count */
    uint64_t blocked_by_consolidation; /**< Events blocked by consolidation */

    /* Entanglement updates */
    uint64_t entangle_increases;      /**< Entanglement weight increases */
    uint64_t entangle_decreases;      /**< Entanglement weight decreases */
    float total_entangle_delta;       /**< Total entanglement change */

    /* Modulation statistics */
    float avg_resonance_modulation;   /**< Average resonance LR factor */
    float avg_tier_modulation;        /**< Average tier LR factor */
    float avg_consolidation_gate;     /**< Average consolidation gate */

    /* Performance */
    uint64_t forward_calls;           /**< STDP → PR calls */
    uint64_t backward_calls;          /**< PR → STDP calls */
    float avg_forward_time_us;        /**< Average forward call time */
    float avg_backward_time_us;       /**< Average backward call time */
} stdp_pr_bridge_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct stdp_pr_bridge_struct* stdp_pr_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides biologically-plausible starting point
 *
 * @return Default configuration
 */
stdp_pr_bridge_config_t stdp_pr_bridge_default_config(void);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool stdp_pr_bridge_validate_config(const stdp_pr_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create STDP-PR bridge
 *
 * WHAT: Initialize bidirectional STDP ↔ Prime Resonant bridge
 * WHY:  Entry point for memory-plasticity integration
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 */
stdp_pr_bridge_t stdp_pr_bridge_create(const stdp_pr_bridge_config_t* config);

/**
 * @brief Destroy STDP-PR bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void stdp_pr_bridge_destroy(stdp_pr_bridge_t bridge);

/**
 * @brief Check if bridge is connected and healthy
 *
 * @param bridge Bridge handle
 * @return true if connected and healthy
 */
bool stdp_pr_bridge_is_connected(stdp_pr_bridge_t bridge);

//=============================================================================
// Forward Direction: STDP → Prime Resonant
//=============================================================================

/**
 * @brief Notify PR of LTP event
 *
 * WHAT: Report STDP potentiation to update entanglement
 * WHY:  Strengthened synapses should strengthen memory associations
 *
 * @param bridge Bridge handle
 * @param source_id Source memory node ID
 * @param target_id Target memory node ID
 * @param weight_change Synaptic weight change (positive)
 * @param effect Output: effect on PR memory (may be NULL)
 * @return 0 on success, -1 on error
 */
int stdp_pr_notify_ltp(stdp_pr_bridge_t bridge,
                       uint64_t source_id, uint64_t target_id,
                       float weight_change,
                       stdp_pr_forward_effect_t* effect);

/**
 * @brief Notify PR of LTD event
 *
 * WHAT: Report STDP depression to update entanglement
 * WHY:  Weakened synapses should weaken memory associations
 *
 * @param bridge Bridge handle
 * @param source_id Source memory node ID
 * @param target_id Target memory node ID
 * @param weight_change Synaptic weight change (negative)
 * @param effect Output: effect on PR memory (may be NULL)
 * @return 0 on success, -1 on error
 */
int stdp_pr_notify_ltd(stdp_pr_bridge_t bridge,
                       uint64_t source_id, uint64_t target_id,
                       float weight_change,
                       stdp_pr_forward_effect_t* effect);

/**
 * @brief Notify PR of burst-triggered STDP event
 *
 * WHAT: Report dopamine-amplified STDP for memory consolidation
 * WHY:  Burst events signal importance, trigger consolidation boost
 *
 * @param bridge Bridge handle
 * @param source_id Source memory node ID
 * @param target_id Target memory node ID
 * @param weight_change Synaptic weight change
 * @param is_ltp true for LTP, false for LTD
 * @param effect Output: effect on PR memory (may be NULL)
 * @return 0 on success, -1 on error
 */
int stdp_pr_notify_burst(stdp_pr_bridge_t bridge,
                         uint64_t source_id, uint64_t target_id,
                         float weight_change, bool is_ltp,
                         stdp_pr_forward_effect_t* effect);

/**
 * @brief Batch notify multiple STDP events
 *
 * @param bridge Bridge handle
 * @param events Array of forward effects to apply
 * @param count Number of events
 * @return Number of events successfully applied
 */
int stdp_pr_notify_batch(stdp_pr_bridge_t bridge,
                         const stdp_pr_forward_effect_t* events,
                         size_t count);

//=============================================================================
// Backward Direction: Prime Resonant → STDP
//=============================================================================

/**
 * @brief Get STDP modulation from PR memory state
 *
 * WHAT: Query PR memory to modulate STDP parameters
 * WHY:  Memory state (resonance, consolidation, tier) should affect learning
 *
 * @param bridge Bridge handle
 * @param node_id Memory node ID to query
 * @param effect Output: modulation effects on STDP
 * @return 0 on success, -1 on error
 */
int stdp_pr_get_modulation(stdp_pr_bridge_t bridge,
                           uint64_t node_id,
                           stdp_pr_backward_effect_t* effect);

/**
 * @brief Apply resonance-based learning rate modulation
 *
 * WHAT: Scale STDP learning rate by resonance score
 * WHY:  High resonance memories should have enhanced plasticity
 *
 * @param bridge Bridge handle
 * @param resonance Resonance score [0, 1]
 * @param base_lr Base learning rate
 * @param modulated_lr Output: modulated learning rate
 * @return 0 on success, -1 on error
 */
int stdp_pr_apply_resonance_modulation(stdp_pr_bridge_t bridge,
                                       float resonance, float base_lr,
                                       float* modulated_lr);

/**
 * @brief Apply consolidation gating to STDP
 *
 * WHAT: Gate STDP plasticity based on consolidation level
 * WHY:  Consolidated memories should have reduced plasticity
 *
 * @param bridge Bridge handle
 * @param consolidation Consolidation level (quaternion.w) [0, 1]
 * @param base_lr Base learning rate
 * @param gated_lr Output: gated learning rate
 * @return 0 on success, -1 on error
 */
int stdp_pr_apply_consolidation_gate(stdp_pr_bridge_t bridge,
                                     float consolidation, float base_lr,
                                     float* gated_lr);

/**
 * @brief Get tier-based STDP rate multiplier
 *
 * WHAT: Get STDP rate based on memory tier
 * WHY:  Different memory tiers have different plasticity levels
 *
 * @param bridge Bridge handle
 * @param tier Memory tier (Z0-Z3)
 * @param rate_multiplier Output: tier-based rate multiplier
 * @return 0 on success, -1 on error
 */
int stdp_pr_get_tier_rate(stdp_pr_bridge_t bridge,
                          stdp_pr_memory_tier_t tier,
                          float* rate_multiplier);

/**
 * @brief Compute combined STDP modulation
 *
 * WHAT: Compute final STDP parameters from all PR factors
 * WHY:  Unified modulation considering resonance, consolidation, and tier
 *
 * @param bridge Bridge handle
 * @param resonance Resonance score [0, 1]
 * @param consolidation Consolidation level [0, 1]
 * @param tier Memory tier
 * @param base_a_plus Base LTP amplitude
 * @param base_a_minus Base LTD amplitude
 * @param effect Output: combined modulation effect
 * @return 0 on success, -1 on error
 */
int stdp_pr_compute_modulation(stdp_pr_bridge_t bridge,
                               float resonance, float consolidation,
                               stdp_pr_memory_tier_t tier,
                               float base_a_plus, float base_a_minus,
                               stdp_pr_backward_effect_t* effect);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @param state Output: current bridge state
 * @return 0 on success, -1 on error
 */
int stdp_pr_bridge_get_state(stdp_pr_bridge_t bridge,
                             stdp_pr_bridge_state_t* state);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output: bridge statistics
 * @return 0 on success, -1 on error
 */
int stdp_pr_bridge_get_stats(stdp_pr_bridge_t bridge,
                             stdp_pr_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int stdp_pr_bridge_reset_stats(stdp_pr_bridge_t bridge);

/**
 * @brief Update bridge state (call periodically)
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
int stdp_pr_bridge_update(stdp_pr_bridge_t bridge, float dt_ms);

/**
 * @brief Get bridge coherence (STDP-PR alignment)
 *
 * @param bridge Bridge handle
 * @return Coherence [0, 1], or -1 on error
 */
float stdp_pr_bridge_get_coherence(stdp_pr_bridge_t bridge);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
void stdp_pr_bridge_print_summary(stdp_pr_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_PR_BRIDGE_H */
