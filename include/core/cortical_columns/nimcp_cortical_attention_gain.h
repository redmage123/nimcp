/**
 * @file nimcp_cortical_attention_gain.h
 * @brief Attention-Modulated Gain Control for Cortical Columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Implements attention-dependent gain modulation for cortical columns
 * WHY:  Attention enhances neural responses to attended features and locations,
 *       implementing the core attentional selection mechanism found in cortex.
 * HOW:  Multiplicative gain control based on Reynolds & Heeger (2009) Normalization
 *       Model, with layer-specific effects and FEP precision coupling.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * REYNOLDS & HEEGER (2009) NORMALIZATION MODEL:
 * ---------------------------------------------
 * - Attention acts as multiplicative gain on neural responses
 * - Feature-based attention: Neurons tuned to attended feature boosted globally
 * - Spatial attention: Neurons with RFs overlapping attended location boosted
 * - Layer II/III most affected (feedback connections from higher areas)
 * - Layer IV less affected (feedforward sensory input)
 * - Reference: Reynolds & Heeger (2009) "The Normalization Model of Attention"
 *
 * CORTICAL LAYER EFFECTS:
 * -----------------------
 * - Layer II/III (Superficial): 1.5-2.0x gain boost (feedback target)
 * - Layer IV (Granular): 1.1-1.3x gain boost (feedforward input)
 * - Layer V/VI (Deep): 1.2-1.5x gain boost (output/feedback source)
 * - Reference: Buffalo et al. (2010) "A backward progression of attentional effects"
 *
 * FEATURE-BASED ATTENTION:
 * ------------------------
 * - Boosts columns tuned to target feature (e.g., red color, vertical orientation)
 * - Gain boost proportional to tuning similarity
 * - Spreads across entire visual field (not location-specific)
 * - Reference: Maunsell & Treue (2006) "Feature-based attention in visual cortex"
 *
 * SPATIAL ATTENTION:
 * ------------------
 * - Spotlight attention: Gaussian gain profile centered on attended location
 * - Radius typically 1-3° of visual angle
 * - Winner-take-all: Unattended locations suppressed
 * - Reference: Posner (1980) "Orienting of attention"
 *
 * FEP PRECISION-ATTENTION COUPLING:
 * ---------------------------------
 * - Attention = precision weighting in Free Energy Principle
 * - High attention → high precision → sharper inference
 * - Low attention → low precision → broader prior
 * - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * MATHEMATICAL MODEL:
 * ===================
 *
 * Feature-Based Gain:
 *   g_feature = baseline + boost * selectivity(feature_distance)
 *   selectivity = exp(-distance² / 2σ²)
 *
 * Spatial Gain:
 *   g_spatial = baseline + boost * spotlight(rf_center, attention_center)
 *   spotlight = intensity * exp(-dist² / 2σ²)
 *
 * Layer-Specific Modulation:
 *   g_layer2/3 = g_attention * layer_23_gain_factor
 *   g_layer4   = g_attention * 1.0
 *   g_layer5/6 = g_attention * sqrt(layer_23_gain_factor)
 *
 * Total Gain:
 *   g_total = g_feature * g_spatial * g_layer * precision_factor
 *
 * Suppression (if enabled):
 *   g_unattended = baseline * suppression_factor  (typically 0.5-0.7)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_ATTENTION_GAIN_H
#define NIMCP_CORTICAL_ATTENTION_GAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration and Types
 * ============================================================================ */

/**
 * @brief Attention mode types
 *
 * WHAT: Different forms of attentional selection
 * WHY:  Different tasks require different attention strategies
 */
typedef enum {
    ATTENTION_NONE,              /**< No attentional modulation */
    ATTENTION_FEATURE_BASED,     /**< Boost specific feature globally */
    ATTENTION_SPATIAL,           /**< Spotlight attention at location */
    ATTENTION_OBJECT_BASED,      /**< Object-selective attention */
    ATTENTION_DIVIDED            /**< Multiple attention foci */
} attention_mode_t;

/**
 * @brief Spatial attention spotlight
 *
 * WHAT: Gaussian attention spotlight in cortical map space
 * WHY:  Spatial attention enhances processing at specific locations
 */
typedef struct {
    float center_x;              /**< X coordinate of spotlight center */
    float center_y;              /**< Y coordinate of spotlight center */
    float radius;                /**< Spotlight radius (σ in Gaussian) */
    float intensity;             /**< Spotlight intensity [0-1] */
} attention_spotlight_t;

/**
 * @brief Cortical attention gain configuration
 *
 * WHAT: Configuration parameters for attention-modulated gain control
 * WHY:  Specify attentional parameters at creation time
 */
typedef struct {
    attention_mode_t mode;       /**< Attention mode */
    float baseline_gain;         /**< Baseline gain (default: 1.0) */
    float max_gain_boost;        /**< Maximum multiplicative boost (default: 2.0) */
    float spatial_sigma;         /**< Spatial spotlight width (default: 2.0) */
    float feature_selectivity;   /**< Feature tuning width (default: 1.0) */
    bool enable_suppression;     /**< Suppress unattended regions (default: true) */
    float suppression_factor;    /**< Unattended suppression [0-1] (default: 0.6) */
    float layer_23_gain_factor;  /**< Extra gain for L2/3 (default: 1.8) */
    float layer_56_gain_factor;  /**< Extra gain for L5/6 (default: 1.3) */
    bool enable_fep_coupling;    /**< Couple attention to FEP precision */
    float precision_gain_slope;  /**< Precision → gain scaling (default: 0.5) */
} cortical_attention_config_t;

/**
 * @brief Per-minicolumn gain state
 *
 * WHAT: Current gain state for individual minicolumn
 * WHY:  Track attention effects at minicolumn level
 */
typedef struct {
    float feature_gain;          /**< Feature-based gain component */
    float spatial_gain;          /**< Spatial gain component */
    float layer_23_gain;         /**< Layer II/III gain */
    float layer_4_gain;          /**< Layer IV gain */
    float layer_56_gain;         /**< Layer V/VI gain */
    float total_gain;            /**< Combined gain */
    bool is_attended;            /**< Whether column is attended */
} minicolumn_gain_state_t;

/**
 * @brief Attention gain statistics
 *
 * WHAT: Runtime statistics for attention system
 * WHY:  Monitor attention effects and performance
 */
typedef struct {
    uint64_t total_updates;      /**< Total gain updates */
    uint32_t attended_columns;   /**< Number of attended columns */
    float avg_attended_gain;     /**< Average gain for attended */
    float avg_unattended_gain;   /**< Average gain for unattended */
    float max_gain_applied;      /**< Maximum gain observed */
    float feature_selectivity;   /**< Current feature selectivity */
    float spatial_coverage;      /**< Fraction of columns in spotlight */
} attention_gain_stats_t;

/**
 * @brief Cortical attention gain system
 *
 * WHAT: Complete attention-modulated gain control system
 * WHY:  Manage attentional effects on cortical columns
 */
typedef struct {
    /* Configuration */
    cortical_attention_config_t config;

    /* Target hypercolumn */
    hypercolumn_t* hypercolumn;

    /* Per-minicolumn gain states */
    minicolumn_gain_state_t* minicolumn_gains;
    uint32_t num_minicolumns;

    /* Attention targets */
    float target_feature;        /**< Target feature value for feature attention */
    attention_spotlight_t spotlight; /**< Spatial attention spotlight */
    attention_spotlight_t* spotlights; /**< Multiple spotlights for divided attention */
    uint32_t num_spotlights;     /**< Number of active spotlights */

    /* FEP integration */
    fep_system_t* fep_system;
    float current_precision;     /**< Current FEP precision estimate */

    /* Statistics */
    attention_gain_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} cortical_attention_gain_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default cortical attention configuration
 *
 * WHAT: Provide sensible default parameters
 * WHY:  Easy initialization with biologically-plausible values
 * HOW:  Set defaults based on Reynolds & Heeger (2009) model
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int cortical_attention_default_config(cortical_attention_config_t* config);

/**
 * @brief Create cortical attention gain system
 *
 * WHAT: Initialize attention-modulated gain control
 * WHY:  Enable attentional selection in cortical columns
 * HOW:  Allocate system, initialize gain states, connect to hypercolumn
 *
 * @param config Configuration (NULL for defaults)
 * @param hypercolumn Target hypercolumn
 * @return New attention system or NULL on failure
 */
cortical_attention_gain_t* cortical_attention_create(
    const cortical_attention_config_t* config,
    hypercolumn_t* hypercolumn
);

/**
 * @brief Destroy cortical attention gain system
 *
 * WHAT: Clean up attention system resources
 * WHY:  Proper memory management
 * HOW:  Free gain states, disconnect bio-async, destroy mutex
 *
 * @param attention Attention system (NULL safe)
 */
void cortical_attention_destroy(cortical_attention_gain_t* attention);

/* ============================================================================
 * Attention Control API
 * ============================================================================ */

/**
 * @brief Set attention mode
 *
 * WHAT: Switch between attention modes
 * WHY:  Different tasks require different attention types
 * HOW:  Update mode, reset gain states
 *
 * @param attention Attention system
 * @param mode New attention mode
 * @return 0 on success
 */
int cortical_attention_set_mode(
    cortical_attention_gain_t* attention,
    attention_mode_t mode
);

/**
 * @brief Set spatial attention spotlight
 *
 * WHAT: Position spatial attention spotlight
 * WHY:  Direct attention to specific cortical location
 * HOW:  Set spotlight center, radius, intensity
 *
 * @param attention Attention system
 * @param center_x X coordinate in cortical map
 * @param center_y Y coordinate in cortical map
 * @param radius Spotlight radius (σ)
 * @param intensity Spotlight intensity [0-1]
 * @return 0 on success
 */
int cortical_attention_set_spotlight(
    cortical_attention_gain_t* attention,
    float center_x,
    float center_y,
    float radius,
    float intensity
);

/**
 * @brief Set feature attention target
 *
 * WHAT: Specify target feature for feature-based attention
 * WHY:  Boost columns tuned to specific feature
 * HOW:  Set target feature value, compute tuning distances
 *
 * @param attention Attention system
 * @param target_feature Target feature value (e.g., orientation angle)
 * @return 0 on success
 */
int cortical_attention_set_feature_target(
    cortical_attention_gain_t* attention,
    float target_feature
);

/**
 * @brief Add spotlight for divided attention
 *
 * WHAT: Add additional attention spotlight
 * WHY:  Enable divided attention across multiple locations
 * HOW:  Append spotlight to spotlight array
 *
 * @param attention Attention system
 * @param center_x X coordinate
 * @param center_y Y coordinate
 * @param radius Spotlight radius
 * @param intensity Spotlight intensity
 * @return 0 on success
 */
int cortical_attention_add_spotlight(
    cortical_attention_gain_t* attention,
    float center_x,
    float center_y,
    float radius,
    float intensity
);

/**
 * @brief Clear all spotlights
 *
 * WHAT: Remove all attention spotlights
 * WHY:  Reset divided attention state
 * HOW:  Free spotlight array, reset count
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_clear_spotlights(cortical_attention_gain_t* attention);

/* ============================================================================
 * Gain Application API
 * ============================================================================ */

/**
 * @brief Update attention gain states
 *
 * WHAT: Recompute gain for all minicolumns based on attention state
 * WHY:  Keep gain current with attention changes
 * HOW:  Compute feature, spatial, and layer-specific gains
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_update_gains(cortical_attention_gain_t* attention);

/**
 * @brief Apply gain to minicolumn activation
 *
 * WHAT: Modulate minicolumn activation by attention gain
 * WHY:  Implement attentional enhancement/suppression
 * HOW:  Multiply activation by computed gain
 *
 * @param attention Attention system
 * @param minicolumn_idx Index of minicolumn
 * @param activation Current activation [0-1]
 * @return Modulated activation
 */
float cortical_attention_apply_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    float activation
);

/**
 * @brief Compute layer-specific gain
 *
 * WHAT: Calculate attention gain for specific cortical layer
 * WHY:  Different layers have different attentional susceptibility
 * HOW:  Apply layer-specific gain factors to base gain
 *
 * @param attention Attention system
 * @param minicolumn_idx Minicolumn index
 * @param layer Layer index (0=L2/3, 1=L4, 2=L5/6)
 * @return Layer-specific gain
 */
float cortical_attention_compute_layer_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    uint32_t layer
);

/**
 * @brief Apply gain to entire hypercolumn
 *
 * WHAT: Modulate all minicolumn activations
 * WHY:  Efficient batch gain application
 * HOW:  Iterate all minicolumns, apply individual gains
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_apply_gain_to_hypercolumn(
    cortical_attention_gain_t* attention
);

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

/**
 * @brief Connect to FEP system
 *
 * WHAT: Link attention to Free Energy Principle system
 * WHY:  Couple attention with FEP precision weighting
 * HOW:  Store FEP system pointer, enable precision coupling
 *
 * @param attention Attention system
 * @param fep_system FEP system
 * @return 0 on success
 */
int cortical_attention_connect_fep(
    cortical_attention_gain_t* attention,
    fep_system_t* fep_system
);

/**
 * @brief Disconnect from FEP system
 *
 * WHAT: Unlink attention from FEP
 * WHY:  Disable precision coupling
 * HOW:  Clear FEP pointer, disable coupling
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_disconnect_fep(cortical_attention_gain_t* attention);

/**
 * @brief Modulate precision based on attention
 *
 * WHAT: Update FEP precision estimate from attention state
 * WHY:  High attention → high precision (confident inference)
 * HOW:  Map attention gain to precision via sigmoid
 *
 * @param attention Attention system
 * @param base_precision Base FEP precision
 * @return Attention-modulated precision
 */
float cortical_attention_modulate_precision(
    const cortical_attention_gain_t* attention,
    float base_precision
);

/**
 * @brief Update attention from FEP precision
 *
 * WHAT: Adjust attention gain based on FEP precision
 * WHY:  High precision → focus attention, low precision → broaden
 * HOW:  Scale gain by precision estimate
 *
 * @param attention Attention system
 * @param precision Current FEP precision
 * @return 0 on success
 */
int cortical_attention_update_from_precision(
    cortical_attention_gain_t* attention,
    float precision
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get gain for minicolumn
 *
 * WHAT: Retrieve current gain state for minicolumn
 * WHY:  Query attention effects
 * HOW:  Return gain from precomputed state
 *
 * @param attention Attention system
 * @param minicolumn_idx Minicolumn index
 * @param gain_state Output gain state
 * @return 0 on success
 */
int cortical_attention_get_minicolumn_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    minicolumn_gain_state_t* gain_state
);

/**
 * @brief Check if minicolumn is attended
 *
 * WHAT: Determine if minicolumn falls within attention focus
 * WHY:  Binary attended/unattended classification
 * HOW:  Check if total gain exceeds threshold
 *
 * @param attention Attention system
 * @param minicolumn_idx Minicolumn index
 * @return true if attended
 */
bool cortical_attention_is_attended(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx
);

/**
 * @brief Get attention statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY:  Monitor attention system behavior
 * HOW:  Copy internal stats to output
 *
 * @param attention Attention system
 * @param stats Output statistics
 * @return 0 on success
 */
int cortical_attention_get_stats(
    const cortical_attention_gain_t* attention,
    attention_gain_stats_t* stats
);

/**
 * @brief Reset attention statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero stats structure
 *
 * @param attention Attention system
 */
void cortical_attention_reset_stats(cortical_attention_gain_t* attention);

/* ============================================================================
 * Bio-async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module attention coordination
 * HOW:  Register module, set up message handlers
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_connect_bio_async(cortical_attention_gain_t* attention);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async system
 * WHY:  Clean shutdown
 * HOW:  Deregister module, clear context
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_disconnect_bio_async(cortical_attention_gain_t* attention);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Determine if messaging is available
 * HOW:  Return bio_async_enabled flag
 *
 * @param attention Attention system
 * @return true if connected
 */
bool cortical_attention_is_bio_async_connected(
    const cortical_attention_gain_t* attention
);

/* ============================================================================
 * Ternary Gain Modulation API
 * ============================================================================ */

/**
 * @brief Ternary gain state
 *
 * WHAT: Discrete gain levels for efficient cortical computation
 * WHY:  Simplifies gain control to three biologically-relevant states
 * HOW:  SUPPRESSED reduces activity, BASELINE maintains, AMPLIFIED enhances
 *
 * BIOLOGICAL BASIS:
 * - Attention modulates cortical gain in discrete modes
 * - Inhibitory networks create suppressed states
 * - Feedforward/feedback balance determines baseline
 * - Cholinergic neuromodulation enables amplification
 */
typedef enum {
    CORTICAL_GAIN_SUPPRESSED = -1,   /**< Active suppression (inhibition dominant) */
    CORTICAL_GAIN_BASELINE = 0,      /**< Neutral/default gain */
    CORTICAL_GAIN_AMPLIFIED = 1      /**< Enhanced gain (attention focus) */
} ternary_cortical_gain_t;

/**
 * @brief Ternary gain configuration
 *
 * WHAT: Configuration for discrete ternary gain control
 * WHY:  Simplify continuous gain to discrete states
 * HOW:  Thresholds determine state transitions, gains set modulation strength
 */
typedef struct {
    float amplify_threshold;         /**< Attention threshold for amplification (default: 0.6) */
    float suppress_threshold;        /**< Attention threshold for suppression (default: 0.3) */
    float amplified_gain;            /**< Gain when amplified (default: 2.0) */
    float baseline_gain;             /**< Gain when baseline (default: 1.0) */
    float suppressed_gain;           /**< Gain when suppressed (default: 0.5) */
    bool use_hysteresis;             /**< Prevent rapid state changes */
    float hysteresis_margin;         /**< Hysteresis margin (default: 0.05) */
    bool per_layer;                  /**< Apply different ternary gains per layer */
    float layer_23_boost;            /**< Extra amplification for L2/3 (default: 1.2) */
} ternary_gain_config_t;

/**
 * @brief Ternary gain statistics
 *
 * WHAT: Runtime statistics for ternary gain system
 * WHY:  Monitor state distribution and transitions
 */
typedef struct {
    uint64_t total_updates;          /**< Total gain update calls */
    uint32_t suppressed_count;       /**< Columns in suppressed state */
    uint32_t baseline_count;         /**< Columns in baseline state */
    uint32_t amplified_count;        /**< Columns in amplified state */
    uint32_t state_transitions;      /**< Total state transitions */
    float avg_gain_applied;          /**< Average gain across columns */
} ternary_gain_stats_t;

/**
 * @brief Get default ternary gain configuration
 *
 * WHAT: Provide sensible defaults for ternary gain
 * WHY:  Easy initialization
 * HOW:  Set biologically-plausible thresholds and gains
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
static inline int ternary_gain_default_config(ternary_gain_config_t* config) {
    if (!config) return -1;

    config->amplify_threshold = 0.6f;
    config->suppress_threshold = 0.3f;
    config->amplified_gain = 2.0f;
    config->baseline_gain = 1.0f;
    config->suppressed_gain = 0.5f;
    config->use_hysteresis = true;
    config->hysteresis_margin = 0.05f;
    config->per_layer = true;
    config->layer_23_boost = 1.2f;

    return 0;
}

/**
 * @brief Enable ternary gain mode
 *
 * WHAT: Switch attention system to ternary gain mode
 * WHY:  Simplify continuous gain to discrete states
 * HOW:  Store ternary config, enable ternary processing
 *
 * @param attention Attention system
 * @param config Ternary configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int cortical_attention_enable_ternary(
    cortical_attention_gain_t* attention,
    const ternary_gain_config_t* config
);

/**
 * @brief Disable ternary gain mode
 *
 * WHAT: Return to continuous gain mode
 * WHY:  Allow switching between discrete and continuous
 * HOW:  Clear ternary config, disable ternary processing
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_disable_ternary(cortical_attention_gain_t* attention);

/**
 * @brief Check if ternary mode is enabled
 *
 * @param attention Attention system
 * @return true if ternary mode active
 */
bool cortical_attention_is_ternary(const cortical_attention_gain_t* attention);

/**
 * @brief Compute ternary gain state for minicolumn
 *
 * WHAT: Determine discrete gain state from continuous attention
 * WHY:  Map continuous attention to discrete gain level
 * HOW:  Apply thresholds with optional hysteresis
 *
 * @param attention Attention system
 * @param minicolumn_idx Minicolumn index
 * @param attention_value Continuous attention value [0,1]
 * @return Ternary gain state
 */
ternary_cortical_gain_t cortical_attention_compute_ternary_state(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    float attention_value
);

/**
 * @brief Apply ternary gain to activation
 *
 * WHAT: Modulate activation using discrete ternary gain
 * WHY:  Efficient attention-based modulation
 * HOW:  Look up ternary state, apply corresponding gain
 *
 * @param attention Attention system
 * @param minicolumn_idx Minicolumn index
 * @param activation Current activation
 * @return Gain-modulated activation
 */
float cortical_attention_apply_ternary_gain(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx,
    float activation
);

/**
 * @brief Get ternary state for minicolumn
 *
 * WHAT: Query current ternary gain state
 * WHY:  Access discrete state for external processing
 * HOW:  Return cached ternary state
 *
 * @param attention Attention system
 * @param minicolumn_idx Minicolumn index
 * @return Current ternary gain state
 */
ternary_cortical_gain_t cortical_attention_get_ternary_state(
    const cortical_attention_gain_t* attention,
    uint32_t minicolumn_idx
);

/**
 * @brief Update all ternary gain states
 *
 * WHAT: Recompute ternary states for all minicolumns
 * WHY:  Batch update after attention changes
 * HOW:  Apply thresholds to all continuous attention values
 *
 * @param attention Attention system
 * @return 0 on success
 */
int cortical_attention_update_ternary_states(
    cortical_attention_gain_t* attention
);

/**
 * @brief Get ternary gain statistics
 *
 * WHAT: Retrieve ternary-specific statistics
 * WHY:  Monitor state distribution
 * HOW:  Count columns in each state
 *
 * @param attention Attention system
 * @param stats Output statistics
 * @return 0 on success
 */
int cortical_attention_get_ternary_stats(
    const cortical_attention_gain_t* attention,
    ternary_gain_stats_t* stats
);

/**
 * @brief Convert ternary gain to float
 *
 * WHAT: Get gain multiplier for ternary state
 * WHY:  Convert discrete state to continuous gain
 * HOW:  Look up gain value from config
 *
 * @param attention Attention system
 * @param state Ternary gain state
 * @return Gain multiplier value
 */
float cortical_attention_ternary_to_gain(
    const cortical_attention_gain_t* attention,
    ternary_cortical_gain_t state
);

/**
 * @brief Convert attention value to ternary
 *
 * WHAT: Discretize continuous attention to ternary state
 * WHY:  Standalone conversion without context
 * HOW:  Apply configured thresholds
 *
 * @param config Ternary configuration
 * @param attention_value Continuous attention [0,1]
 * @return Ternary gain state
 */
static inline ternary_cortical_gain_t ternary_gain_from_attention(
    const ternary_gain_config_t* config,
    float attention_value
) {
    if (!config) {
        /* Use default thresholds */
        if (attention_value >= 0.6f) return CORTICAL_GAIN_AMPLIFIED;
        if (attention_value <= 0.3f) return CORTICAL_GAIN_SUPPRESSED;
        return CORTICAL_GAIN_BASELINE;
    }

    if (attention_value >= config->amplify_threshold) {
        return CORTICAL_GAIN_AMPLIFIED;
    }
    if (attention_value <= config->suppress_threshold) {
        return CORTICAL_GAIN_SUPPRESSED;
    }
    return CORTICAL_GAIN_BASELINE;
}

/**
 * @brief Get gain value for ternary state
 *
 * WHAT: Convert ternary state to gain multiplier
 * WHY:  Apply discrete state to activations
 * HOW:  Look up configured gain value
 *
 * @param config Ternary configuration
 * @param state Ternary gain state
 * @return Gain multiplier
 */
static inline float ternary_gain_to_value(
    const ternary_gain_config_t* config,
    ternary_cortical_gain_t state
) {
    if (!config) {
        /* Use default gains */
        switch (state) {
            case CORTICAL_GAIN_AMPLIFIED:  return 2.0f;
            case CORTICAL_GAIN_SUPPRESSED: return 0.5f;
            default:                        return 1.0f;
        }
    }

    switch (state) {
        case CORTICAL_GAIN_AMPLIFIED:  return config->amplified_gain;
        case CORTICAL_GAIN_SUPPRESSED: return config->suppressed_gain;
        default:                        return config->baseline_gain;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_ATTENTION_GAIN_H */
