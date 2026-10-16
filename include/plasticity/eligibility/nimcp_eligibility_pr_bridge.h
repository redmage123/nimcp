//=============================================================================
// nimcp_eligibility_pr_bridge.h - Eligibility Traces ↔ Prime Resonant Bridge
//=============================================================================
/**
 * @file nimcp_eligibility_pr_bridge.h
 * @brief Bidirectional integration between eligibility traces and Prime Resonant memory
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge connecting eligibility traces with Prime Resonant memory system
 * WHY:  Eligibility signals should gate PR memory consolidation, and PR
 *       consolidation state should modulate eligibility trace decay
 * HOW:  Bidirectional communication where:
 *       - Eligibility traces → gate PR consolidation (quaternion.w updates)
 *       - PR consolidation → modulate eligibility trace decay rate
 *       - PR memory tier → set eligibility trace parameters
 *       - High eligibility + reward → trigger memory strengthening
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 *
 *   Eligibility → Prime Resonant:
 *   +-----------------------------------------------------------------------+
 *   |  Eligibility traces tag synapses for potential consolidation:         |
 *   |                                                                        |
 *   |  1. Eligibility Gates Consolidation:                                  |
 *   |     - High eligibility + reward signal → boost quaternion.w           |
 *   |     - "Tags" memories for consolidation during replay/sleep           |
 *   |     - Implements "tag and capture" (Frey & Morris, 1997)              |
 *   |                                                                        |
 *   |  2. Eligibility Updates Entanglement:                                 |
 *   |     - Co-eligible nodes → strengthen entanglement                     |
 *   |     - Temporal credit assignment for memory associations              |
 *   |                                                                        |
 *   |  3. Dopamine-Gated Capture:                                           |
 *   |     - Eligibility trace × dopamine burst → memory promotion           |
 *   |     - Moves memories up Z-ladder (Z0→Z1→Z2→Z3)                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Prime Resonant → Eligibility:
 *   +-----------------------------------------------------------------------+
 *   |  Memory state modulates eligibility dynamics:                          |
 *   |                                                                        |
 *   |  1. Consolidation Affects Decay Rate:                                 |
 *   |     - High consolidation → slower eligibility decay                   |
 *   |     - Consolidated memories retain eligibility longer                  |
 *   |     - λ_effective = λ_base × (1 + consolidation_boost)                |
 *   |                                                                        |
 *   |  2. Memory Tier Sets Parameters:                                      |
 *   |     - Z0 (working) → fast decay, high sensitivity                     |
 *   |     - Z3 (permanent) → slow decay, low sensitivity                    |
 *   |     - Tier-specific eligibility windows                               |
 *   |                                                                        |
 *   |  3. Resonance Boosts Eligibility:                                     |
 *   |     - High resonance → eligibility amplification                      |
 *   |     - Related memories share eligibility                              |
 *   +-----------------------------------------------------------------------+
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ELIGIBILITY_PR_BRIDGE_H
#define NIMCP_ELIGIBILITY_PR_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Consolidation gating */
#define ELIG_PR_CONSOLIDATION_BOOST_MAX   0.2f    /**< Max consolidation boost per event */
#define ELIG_PR_ELIGIBILITY_THRESHOLD     0.1f    /**< Min eligibility for consolidation */
#define ELIG_PR_REWARD_THRESHOLD          0.3f    /**< Min reward for capture */

/** Decay modulation */
#define ELIG_PR_DECAY_CONSOL_BOOST        0.3f    /**< Consolidation → decay slowdown */
#define ELIG_PR_DECAY_MIN_LAMBDA          0.8f    /**< Min decay constant */
#define ELIG_PR_DECAY_MAX_LAMBDA          0.99f   /**< Max decay constant */

/** Tier-specific parameters */
#define ELIG_PR_TIER_Z0_LAMBDA            0.9f    /**< Working memory decay */
#define ELIG_PR_TIER_Z1_LAMBDA            0.93f   /**< Short-term decay */
#define ELIG_PR_TIER_Z2_LAMBDA            0.96f   /**< Long-term decay */
#define ELIG_PR_TIER_Z3_LAMBDA            0.99f   /**< Permanent decay */

/** Resonance modulation */
#define ELIG_PR_RESONANCE_ELIG_BOOST      0.5f    /**< Resonance → eligibility boost */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Memory tier indices
 */
typedef enum {
    ELIG_PR_TIER_Z0 = 0,
    ELIG_PR_TIER_Z1,
    ELIG_PR_TIER_Z2,
    ELIG_PR_TIER_Z3,
    ELIG_PR_TIER_COUNT
} elig_pr_memory_tier_t;

/**
 * @brief Configuration for Eligibility-PR bridge
 */
typedef struct {
    /* Consolidation gating */
    float consolidation_boost_max;    /**< Max consolidation boost */
    float eligibility_threshold;      /**< Min eligibility for consolidation */
    float reward_threshold;           /**< Min reward for capture */
    bool enable_consolidation_gate;   /**< Enable eligibility → consolidation */

    /* Decay modulation */
    float decay_consolidation_boost;  /**< Consolidation → decay slowdown */
    float decay_min_lambda;           /**< Min decay constant */
    float decay_max_lambda;           /**< Max decay constant */
    bool enable_decay_modulation;     /**< Enable consolidation → decay */

    /* Tier parameters */
    float tier_lambdas[ELIG_PR_TIER_COUNT]; /**< Per-tier decay constants */
    bool enable_tier_modulation;      /**< Enable tier-based parameters */

    /* Resonance modulation */
    float resonance_elig_boost;       /**< Resonance → eligibility boost */
    bool enable_resonance_boost;      /**< Enable resonance → eligibility */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable async messaging */
} elig_pr_bridge_config_t;

/**
 * @brief Eligibility effects on PR (forward direction)
 */
typedef struct {
    uint64_t node_id;                 /**< Memory node ID */
    float eligibility;                /**< Eligibility trace value */
    float reward_signal;              /**< Reward signal */
    float consolidation_delta;        /**< Consolidation (quat.w) change */
    float entanglement_boost;         /**< Entanglement weight boost */
    bool tier_promotion;              /**< Whether tier promotion triggered */
    uint64_t timestamp_ms;            /**< Event timestamp */
} elig_pr_forward_effect_t;

/**
 * @brief PR effects on eligibility (backward direction)
 */
typedef struct {
    float consolidation_level;        /**< Quaternion.w [0, 1] */
    float resonance_score;            /**< Resonance [0, 1] */
    elig_pr_memory_tier_t tier;       /**< Memory tier */
    float effective_lambda;           /**< Modulated decay constant */
    float eligibility_boost;          /**< Eligibility amplification */
    float learning_rate_mod;          /**< Learning rate modulation */
} elig_pr_backward_effect_t;

/**
 * @brief Bridge state
 */
typedef struct {
    float current_consolidation;      /**< Latest consolidation level */
    float current_resonance;          /**< Latest resonance score */
    elig_pr_memory_tier_t current_tier; /**< Current memory tier */
    float cumulative_consol_delta;    /**< Accumulated consolidation change */
    uint32_t tier_promotions;         /**< Tier promotions triggered */
    float bridge_coherence;           /**< Eligibility-PR alignment [0,1] */
} elig_pr_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t consolidation_events;    /**< Consolidation gate triggers */
    uint64_t tier_promotions;         /**< Tier promotions */
    uint64_t decay_modulations;       /**< Decay rate modulations */
    uint64_t resonance_boosts;        /**< Resonance-based boosts */
    float total_consolidation_delta;  /**< Total consolidation change */
    float avg_effective_lambda;       /**< Average decay constant */
    uint64_t forward_calls;           /**< Eligibility → PR calls */
    uint64_t backward_calls;          /**< PR → Eligibility calls */
} elig_pr_bridge_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct elig_pr_bridge_struct* elig_pr_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

elig_pr_bridge_config_t elig_pr_bridge_default_config(void);
bool elig_pr_bridge_validate_config(const elig_pr_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

elig_pr_bridge_t elig_pr_bridge_create(const elig_pr_bridge_config_t* config);
void elig_pr_bridge_destroy(elig_pr_bridge_t bridge);
bool elig_pr_bridge_is_connected(elig_pr_bridge_t bridge);

//=============================================================================
// Forward Direction: Eligibility → Prime Resonant
//=============================================================================

/**
 * @brief Apply eligibility-gated consolidation
 *
 * WHAT: Use eligibility trace to gate memory consolidation
 * WHY:  Only eligible memories should be consolidated on reward
 */
int elig_pr_apply_consolidation_gate(elig_pr_bridge_t bridge,
                                     uint64_t node_id,
                                     float eligibility,
                                     float reward_signal,
                                     elig_pr_forward_effect_t* effect);

/**
 * @brief Check if tier promotion should occur
 */
int elig_pr_check_tier_promotion(elig_pr_bridge_t bridge,
                                 uint64_t node_id,
                                 float eligibility,
                                 float reward_signal,
                                 bool* should_promote);

/**
 * @brief Apply eligibility to entanglement update
 */
int elig_pr_apply_entanglement_update(elig_pr_bridge_t bridge,
                                      uint64_t source_id, uint64_t target_id,
                                      float eligibility,
                                      float* entangle_delta);

//=============================================================================
// Backward Direction: Prime Resonant → Eligibility
//=============================================================================

/**
 * @brief Get decay modulation from consolidation
 *
 * WHAT: Modulate eligibility decay based on consolidation
 * WHY:  Consolidated memories should retain eligibility longer
 */
int elig_pr_get_decay_modulation(elig_pr_bridge_t bridge,
                                 float consolidation,
                                 float base_lambda,
                                 float* modulated_lambda);

/**
 * @brief Get tier-based eligibility parameters
 */
int elig_pr_get_tier_parameters(elig_pr_bridge_t bridge,
                                elig_pr_memory_tier_t tier,
                                float* lambda,
                                float* sensitivity);

/**
 * @brief Apply resonance boost to eligibility
 */
int elig_pr_apply_resonance_boost(elig_pr_bridge_t bridge,
                                  float resonance,
                                  float base_eligibility,
                                  float* boosted_eligibility);

/**
 * @brief Compute combined modulation
 */
int elig_pr_compute_modulation(elig_pr_bridge_t bridge,
                               float consolidation, float resonance,
                               elig_pr_memory_tier_t tier,
                               float base_lambda,
                               elig_pr_backward_effect_t* effect);

//=============================================================================
// State and Statistics
//=============================================================================

int elig_pr_bridge_get_state(elig_pr_bridge_t bridge, elig_pr_bridge_state_t* state);
int elig_pr_bridge_get_stats(elig_pr_bridge_t bridge, elig_pr_bridge_stats_t* stats);
int elig_pr_bridge_reset_stats(elig_pr_bridge_t bridge);
int elig_pr_bridge_update(elig_pr_bridge_t bridge, float dt_ms);
float elig_pr_bridge_get_coherence(elig_pr_bridge_t bridge);
void elig_pr_bridge_print_summary(elig_pr_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_PR_BRIDGE_H */
