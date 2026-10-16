/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_thalamic_bridge.h - Information Geometry to Thalamic Attention Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_thalamic_bridge.h
 * @brief Bridge connecting Information Geometry with Thalamic Attention System
 *
 * WHAT: Provides bidirectional integration between Information Geometry and
 *       thalamic attention/gating mechanisms for geometry-guided attention.
 *
 * WHY:  Information geometry enables principled attention allocation:
 *       - Fisher information identifies high-information parameters/regions
 *       - Curvature reveals computational difficulty, guiding attention
 *       - KL divergence detects novelty requiring attention
 *       - Natural gradients prioritize salient information flow
 *
 * HOW:  Two-way integration:
 *       1. InfoGeo -> Thalamus: Geometric saliency drives attention gating
 *       2. Thalamus -> InfoGeo: Attention modulates Fisher computation
 *       3. Curvature maps guide attentional resource allocation
 *       4. KL novelty detection triggers attention shifts
 *
 * BIOLOGICAL BASIS:
 * ```
 * INFORMATION GEOMETRY                    THALAMIC ATTENTION
 * -----------------------------------------------------------------------
 * Fisher Information (high)           ->  Increased thalamic gain
 * Low Curvature Region                ->  Reduced attention (easy)
 * High Curvature Region               ->  Increased attention (difficult)
 * KL Divergence Spike                 ->  Attention shift (novelty)
 * Geodesic Distance (goal)            ->  Progress monitoring
 * Natural Gradient Direction          ->  Attention direction vector
 *
 * ATTENTION TO INFOGEO:
 * -----------------------------------------------------------------------
 * High Attention                      ->  More samples for Fisher
 * Low Attention                       ->  Sparse Fisher updates
 * Attention Shift                     ->  Manifold recomputation
 * Sustained Attention                 ->  Precise geometry estimation
 * ```
 *
 * COMPUTATIONAL THALAMUS ANALOGY:
 * The thalamus acts as a dynamic gain controller for cortical information flow.
 * Information geometry provides principled signals for this gain control:
 * - High Fisher info regions get amplified (important information)
 * - High curvature regions get more processing (computational difficulty)
 * - Novel distributions (high KL) trigger attentional capture
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_THALAMIC_BRIDGE_H
#define NIMCP_INFOGEO_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_THALAMIC_MODULE_NAME     "infogeo_thalamic_bridge"

/** Maximum attention channels */
#define INFOGEO_THALAMIC_MAX_CHANNELS    32

/** Maximum regions for attention map */
#define INFOGEO_THALAMIC_MAX_REGIONS     64

/** Default attention gain factor */
#define INFOGEO_THALAMIC_BASE_GAIN       1.0f

/** Maximum attention gain */
#define INFOGEO_THALAMIC_MAX_GAIN        4.0f

/** Minimum attention gain (suppression) */
#define INFOGEO_THALAMIC_MIN_GAIN        0.1f

/** KL threshold for novelty detection */
#define INFOGEO_THALAMIC_KL_NOVELTY      0.5f

/** Curvature threshold for difficulty */
#define INFOGEO_THALAMIC_CURV_THRESH     1.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Attention modulation source
 */
typedef enum {
    INFOGEO_ATTN_SOURCE_FISHER = 0,     /**< Fisher information magnitude */
    INFOGEO_ATTN_SOURCE_CURVATURE,      /**< Manifold curvature */
    INFOGEO_ATTN_SOURCE_KL,             /**< KL divergence (novelty) */
    INFOGEO_ATTN_SOURCE_GRADIENT,       /**< Natural gradient magnitude */
    INFOGEO_ATTN_SOURCE_COMBINED        /**< Weighted combination */
} infogeo_attn_source_t;

/**
 * @brief Thalamic gating mode
 */
typedef enum {
    INFOGEO_GATE_MODE_MULTIPLICATIVE = 0, /**< Multiply by gain */
    INFOGEO_GATE_MODE_ADDITIVE,           /**< Add bias */
    INFOGEO_GATE_MODE_THRESHOLD,          /**< Binary threshold gating */
    INFOGEO_GATE_MODE_SOFTMAX             /**< Softmax attention weights */
} infogeo_gate_mode_t;

/**
 * @brief Attention allocation strategy
 */
typedef enum {
    INFOGEO_ALLOC_UNIFORM = 0,          /**< Uniform attention */
    INFOGEO_ALLOC_FISHER_PROPORTIONAL,  /**< Proportional to Fisher info */
    INFOGEO_ALLOC_CURVATURE_INVERSE,    /**< Inverse curvature (easy first) */
    INFOGEO_ALLOC_CURVATURE_DIRECT,     /**< Direct curvature (hard first) */
    INFOGEO_ALLOC_NOVELTY_DRIVEN        /**< KL novelty driven */
} infogeo_alloc_strategy_t;

/**
 * @brief Attention shift trigger
 */
typedef enum {
    INFOGEO_SHIFT_NONE = 0,             /**< No shift triggered */
    INFOGEO_SHIFT_NOVELTY,              /**< KL divergence trigger */
    INFOGEO_SHIFT_DIFFICULTY,           /**< High curvature trigger */
    INFOGEO_SHIFT_IMPORTANCE,           /**< Fisher importance trigger */
    INFOGEO_SHIFT_EXTERNAL              /**< External trigger */
} infogeo_shift_trigger_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Information Geometry-Thalamic bridge
 */
typedef struct {
    /** Attention source settings */
    infogeo_attn_source_t attn_source;   /**< Primary attention source */
    float fisher_weight;                  /**< Weight for Fisher in combined */
    float curvature_weight;               /**< Weight for curvature */
    float kl_weight;                      /**< Weight for KL divergence */
    float gradient_weight;                /**< Weight for gradient magnitude */

    /** Gating settings */
    infogeo_gate_mode_t gate_mode;       /**< Thalamic gating mode */
    float base_gain;                      /**< Base attention gain */
    float max_gain;                       /**< Maximum gain */
    float min_gain;                       /**< Minimum gain (suppression) */
    float gain_time_constant_ms;          /**< Gain smoothing time constant */

    /** Allocation settings */
    infogeo_alloc_strategy_t allocation; /**< Attention allocation strategy */
    uint32_t num_channels;                /**< Number of attention channels */
    float total_attention_budget;         /**< Total attention capacity */
    bool enable_competition;              /**< Competitive attention */

    /** Shift detection settings */
    float kl_novelty_threshold;          /**< KL for novelty detection */
    float curvature_difficulty_thresh;    /**< Curvature for difficulty */
    float fisher_importance_thresh;       /**< Fisher for importance */
    float shift_cooldown_ms;              /**< Cooldown between shifts */

    /** Feedback settings */
    bool enable_feedback_to_fisher;      /**< Attention modulates Fisher */
    float attention_fisher_scaling;       /**< How attention scales Fisher */

    /** General settings */
    float update_interval_ms;            /**< Bridge update interval */
    bool enable_logging;                  /**< Enable logging */
} infogeo_thalamic_config_t;

/**
 * @brief Geometric saliency map for attention
 */
typedef struct {
    uint32_t region_id;                 /**< Region identifier */
    float fisher_saliency;              /**< Saliency from Fisher info */
    float curvature_saliency;           /**< Saliency from curvature */
    float kl_saliency;                  /**< Saliency from KL divergence */
    float combined_saliency;            /**< Combined saliency score */
    float current_attention;            /**< Current attention allocation */
    float target_attention;             /**< Target attention level */
} infogeo_saliency_entry_t;

/**
 * @brief Attention gain signal for thalamic gating
 */
typedef struct {
    uint32_t channel_id;                /**< Attention channel */
    float gain;                         /**< Attention gain value */
    float raw_gain;                     /**< Unsmoothed gain */
    infogeo_attn_source_t source;       /**< Source of gain signal */
    float source_value;                 /**< Raw source metric value */
} infogeo_gain_signal_t;

/**
 * @brief Attention shift event
 */
typedef struct {
    infogeo_shift_trigger_t trigger;    /**< What triggered the shift */
    uint32_t from_region;               /**< Previous attention focus */
    uint32_t to_region;                 /**< New attention focus */
    float trigger_value;                /**< Value that triggered shift */
    float threshold;                    /**< Threshold that was exceeded */
    float timestamp_ms;                 /**< When shift occurred */
} infogeo_attention_shift_t;

/**
 * @brief Curvature-based difficulty map
 */
typedef struct {
    uint32_t region_id;                 /**< Region identifier */
    float ricci_curvature;              /**< Ricci curvature */
    float difficulty_score;             /**< Normalized difficulty [0,1] */
    float recommended_attention;        /**< Attention based on difficulty */
    bool high_difficulty_flag;          /**< Above difficulty threshold */
} infogeo_difficulty_entry_t;

/**
 * @brief Novelty detection state
 */
typedef struct {
    float current_kl;                   /**< Current KL divergence */
    float baseline_kl;                  /**< Baseline KL level */
    float kl_rate_of_change;            /**< d(KL)/dt */
    bool novelty_detected;              /**< Novelty flag */
    float time_since_last_novelty_ms;   /**< Time since last detection */
    uint32_t novelty_region;            /**< Region with novelty */
} infogeo_novelty_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t saliency_computations;     /**< Saliency map updates */
    uint64_t gain_updates;              /**< Gain signal updates */
    uint64_t attention_shifts;          /**< Attention shifts triggered */
    uint64_t novelty_detections;        /**< Novelty events detected */
    uint64_t difficulty_alerts;         /**< Difficulty alerts issued */
    float avg_attention_gain;           /**< Average attention gain */
    float avg_saliency;                 /**< Average saliency */
    float total_attention_budget_used;  /**< Attention budget consumed */
    float last_update_ms;               /**< Last update timestamp */
} infogeo_thalamic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_thalamic_bridge_struct infogeo_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_default_config(
    infogeo_thalamic_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-Thalamic bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_thalamic_bridge_t* infogeo_thalamic_bridge_create(
    const infogeo_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_thalamic_bridge_destroy(
    infogeo_thalamic_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_reset(infogeo_thalamic_bridge_t* bridge);

//=============================================================================
// Saliency Computation API (InfoGeo -> Thalamus)
//=============================================================================

/**
 * @brief Update saliency from Fisher information
 *
 * WHAT: Computes attention saliency from Fisher information
 * WHY:  High Fisher info indicates important parameters
 * HOW:  Maps Fisher diagonal to regional saliency
 *
 * @param bridge Bridge handle
 * @param fisher_diagonal Fisher matrix diagonal
 * @param region_ids Region identifiers for each element
 * @param num_elements Number of elements
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_update_fisher_saliency(
    infogeo_thalamic_bridge_t* bridge,
    const float* fisher_diagonal,
    const uint32_t* region_ids,
    uint32_t num_elements
);

/**
 * @brief Update saliency from curvature
 *
 * WHAT: Computes attention saliency from manifold curvature
 * WHY:  High curvature = computational difficulty = needs attention
 * HOW:  Maps regional curvature to difficulty-based saliency
 *
 * @param bridge Bridge handle
 * @param curvatures Curvature values per region
 * @param region_ids Region identifiers
 * @param num_regions Number of regions
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_update_curvature_saliency(
    infogeo_thalamic_bridge_t* bridge,
    const float* curvatures,
    const uint32_t* region_ids,
    uint32_t num_regions
);

/**
 * @brief Update saliency from KL divergence (novelty)
 *
 * WHAT: Computes novelty-based saliency from KL divergence
 * WHY:  Distribution drift indicates novel information
 * HOW:  KL divergence from baseline triggers saliency
 *
 * @param bridge Bridge handle
 * @param kl_values KL divergence values per region
 * @param region_ids Region identifiers
 * @param num_regions Number of regions
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_update_kl_saliency(
    infogeo_thalamic_bridge_t* bridge,
    const float* kl_values,
    const uint32_t* region_ids,
    uint32_t num_regions
);

/**
 * @brief Get saliency map
 *
 * @param bridge Bridge handle
 * @param saliency_map Output saliency entries
 * @param max_entries Maximum entries to return
 * @return Number of entries, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_get_saliency_map(
    const infogeo_thalamic_bridge_t* bridge,
    infogeo_saliency_entry_t* saliency_map,
    uint32_t max_entries
);

//=============================================================================
// Attention Gating API
//=============================================================================

/**
 * @brief Compute attention gain signals
 *
 * WHAT: Computes thalamic gain signals from geometric saliency
 * WHY:  Converts geometric metrics to attention gains
 * HOW:  Maps combined saliency to gain values per channel
 *
 * @param bridge Bridge handle
 * @param gain_signals Output gain signals
 * @param num_channels Number of channels
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_compute_gains(
    infogeo_thalamic_bridge_t* bridge,
    infogeo_gain_signal_t* gain_signals,
    uint32_t num_channels
);

/**
 * @brief Apply attention gating to signal
 *
 * WHAT: Gates a signal using geometric attention
 * WHY:  Modulates information flow by importance
 * HOW:  Applies configured gating mode with current gains
 *
 * @param bridge Bridge handle
 * @param input Input signal to gate
 * @param output Output gated signal
 * @param size Signal size
 * @param channel_id Attention channel
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_gate_signal(
    infogeo_thalamic_bridge_t* bridge,
    const float* input,
    float* output,
    uint32_t size,
    uint32_t channel_id
);

/**
 * @brief Get current gain for channel
 *
 * @param bridge Bridge handle
 * @param channel_id Channel identifier
 * @return Current gain value, or -1.0 on error
 */
NIMCP_EXPORT float infogeo_thalamic_get_gain(
    const infogeo_thalamic_bridge_t* bridge,
    uint32_t channel_id
);

//=============================================================================
// Attention Shift API
//=============================================================================

/**
 * @brief Check for attention shift triggers
 *
 * WHAT: Checks if any geometric metrics trigger attention shift
 * WHY:  Enables novelty-driven attention capture
 * HOW:  Compares metrics against thresholds
 *
 * @param bridge Bridge handle
 * @param shift Output shift event (if triggered)
 * @return true if shift triggered, false otherwise
 */
NIMCP_EXPORT bool infogeo_thalamic_check_shift(
    infogeo_thalamic_bridge_t* bridge,
    infogeo_attention_shift_t* shift
);

/**
 * @brief Force attention shift to region
 *
 * @param bridge Bridge handle
 * @param target_region Region to shift attention to
 * @param trigger Reason for shift
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_force_shift(
    infogeo_thalamic_bridge_t* bridge,
    uint32_t target_region,
    infogeo_shift_trigger_t trigger
);

/**
 * @brief Get novelty detection state
 *
 * @param bridge Bridge handle
 * @param novelty Output novelty state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_get_novelty_state(
    const infogeo_thalamic_bridge_t* bridge,
    infogeo_novelty_state_t* novelty
);

//=============================================================================
// Attention Allocation API
//=============================================================================

/**
 * @brief Allocate attention budget across regions
 *
 * WHAT: Distributes attention budget based on geometric saliency
 * WHY:  Limited attention must be allocated optimally
 * HOW:  Uses configured allocation strategy
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_allocate_attention(
    infogeo_thalamic_bridge_t* bridge
);

/**
 * @brief Get attention allocation for region
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @return Attention allocation [0,1], or -1.0 on error
 */
NIMCP_EXPORT float infogeo_thalamic_get_allocation(
    const infogeo_thalamic_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Set attention allocation strategy
 *
 * @param bridge Bridge handle
 * @param strategy New allocation strategy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_set_allocation_strategy(
    infogeo_thalamic_bridge_t* bridge,
    infogeo_alloc_strategy_t strategy
);

//=============================================================================
// Feedback API (Thalamus -> InfoGeo)
//=============================================================================

/**
 * @brief Get attention-weighted Fisher sampling
 *
 * WHAT: Computes sampling weights for Fisher estimation
 * WHY:  Attended regions get more precise Fisher estimates
 * HOW:  Scales sampling by attention allocation
 *
 * @param bridge Bridge handle
 * @param region_ids Region identifiers
 * @param sampling_weights Output sampling weights
 * @param num_regions Number of regions
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_get_fisher_sampling_weights(
    const infogeo_thalamic_bridge_t* bridge,
    const uint32_t* region_ids,
    float* sampling_weights,
    uint32_t num_regions
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Smooths gains, checks shifts, updates allocations
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_update(
    infogeo_thalamic_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_get_stats(
    const infogeo_thalamic_bridge_t* bridge,
    infogeo_thalamic_stats_t* stats
);

/**
 * @brief Get difficulty map
 *
 * @param bridge Bridge handle
 * @param difficulty_map Output difficulty entries
 * @param max_entries Maximum entries to return
 * @return Number of entries, -1 on error
 */
NIMCP_EXPORT int infogeo_thalamic_get_difficulty_map(
    const infogeo_thalamic_bridge_t* bridge,
    infogeo_difficulty_entry_t* difficulty_map,
    uint32_t max_entries
);

/**
 * @brief Get current attention focus region
 *
 * @param bridge Bridge handle
 * @return Region ID with highest attention, or UINT32_MAX on error
 */
NIMCP_EXPORT uint32_t infogeo_thalamic_get_focus_region(
    const infogeo_thalamic_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_THALAMIC_BRIDGE_H */