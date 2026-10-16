/**
 * @file nimcp_dragonfly_cortical_bridge.h
 * @brief Dragonfly-to-Cortical Column Bridge - TSDN as Hypercolumn
 *
 * WHAT: Bridge between dragonfly TSDN population vector and cortical columns
 * WHY:  Enable dragonfly direction encoding to integrate with cortical processing
 * HOW:  Map 16 TSDNs to minicolumns in a hypercolumn representing target direction
 *
 * BIOLOGICAL RATIONALE:
 * The dragonfly TSDN population (16 neurons at 22.5° intervals) is remarkably
 * similar to orientation hypercolumns in mammalian V1 (minicolumns at ~15-22.5°
 * intervals covering 180° of orientation). This bridge:
 *
 *   1. Maps each TSDN neuron to a minicolumn with matching preferred direction
 *   2. Enables winner-take-all or softmax competition across directions
 *   3. Provides bidirectional conversion between population vector and column format
 *   4. Integrates lateral inhibition for contrast enhancement
 *
 * ARCHITECTURE:
 *
 *   TSDN Population (16 neurons)          Direction Hypercolumn (16 minicolumns)
 *   ┌─────────────────────────┐           ┌─────────────────────────────────┐
 *   │ TSDN[0] → θ=0°          │ ────────→ │ Minicolumn[0] θ=0°   (Layer 4) │
 *   │ TSDN[1] → θ=22.5°       │ ────────→ │ Minicolumn[1] θ=22.5° (Layer 4)│
 *   │ ...                     │           │ ...                             │
 *   │ TSDN[15] → θ=337.5°     │ ────────→ │ Minicolumn[15] θ=337.5° (L4)   │
 *   └─────────────────────────┘           └─────────────────────────────────┘
 *            ↓                                        ↓
 *   Population Vector Readout           Hypercolumn Competition (WTA/Softmax)
 *            ↓                                        ↓
 *   Target Direction (continuous)        Winning Minicolumn (discrete + interp)
 *
 * INTEGRATION POINTS:
 * - dragonfly_tsdn.h: TSDN population vector encoding
 * - nimcp_cortical_column.h: Cortical column infrastructure
 * - dragonfly_parietal_bridge.h: Spatial coordinate transforms
 *
 * @author NIMCP Development Team
 * @date 2024-12-28
 */

#ifndef NIMCP_DRAGONFLY_CORTICAL_BRIDGE_H
#define NIMCP_DRAGONFLY_CORTICAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "constants/nimcp_math_constants.h"

//=============================================================================
// Forward Declarations
//=============================================================================

/* From nimcp_dragonfly.h */
typedef struct dragonfly_system_s dragonfly_system_t;

/* From nimcp_dragonfly_tsdn.h */
typedef struct tsdn_population_s tsdn_population_t;

/* From nimcp_cortical_column.h */
typedef struct hypercolumn hypercolumn_t;
typedef struct minicolumn minicolumn_t;
typedef struct cortical_column_pool cortical_column_pool_t;

//=============================================================================
// Configuration Constants
//=============================================================================

/** Number of direction minicolumns (matches TSDN count) */
#define DRAGONFLY_CORTICAL_MINICOLUMN_COUNT 16

/** Angular spacing between minicolumns (radians) */
#define DRAGONFLY_CORTICAL_ANGULAR_SPACING (NIMCP_TWO_PI_F / 16)

/** Neurons per minicolumn (typical cortical count) */
#define DRAGONFLY_CORTICAL_NEURONS_PER_MINICOLUMN 80

/** Lateral inhibition range (in minicolumn indices) */
#define DRAGONFLY_CORTICAL_INHIBITION_RANGE 4

/** Default softmax temperature */
#define DRAGONFLY_CORTICAL_DEFAULT_TEMPERATURE 1.0f

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Bridge mapping mode
 */
typedef enum {
    CORTICAL_MAP_DIRECT = 0,       /**< Direct 1:1 TSDN to minicolumn mapping */
    CORTICAL_MAP_INTERPOLATED,     /**< Interpolated mapping with overlapping RFs */
    CORTICAL_MAP_HIERARCHICAL      /**< Hierarchical with elevation as second dimension */
} cortical_mapping_mode_t;

/**
 * @brief Competition mode for direction selection
 */
typedef enum {
    CORTICAL_COMPETE_WTA = 0,      /**< Winner-take-all (sharpest selection) */
    CORTICAL_COMPETE_SOFTMAX,      /**< Soft probabilistic (allows uncertainty) */
    CORTICAL_COMPETE_K_WINNERS,    /**< K-winners (allows population coding) */
    CORTICAL_COMPETE_NONE          /**< No competition (raw activations) */
} cortical_competition_mode_t;

/**
 * @brief Direction hypercolumn type
 */
typedef enum {
    HYPERCOLUMN_AZIMUTH = 0,       /**< Horizontal direction (360°) */
    HYPERCOLUMN_ELEVATION,         /**< Vertical direction (180°, -90° to +90°) */
    HYPERCOLUMN_HEADING            /**< Combined azimuth + velocity direction */
} hypercolumn_type_t;

/**
 * @brief Cortical bridge configuration
 */
typedef struct {
    /* Mapping parameters */
    cortical_mapping_mode_t mapping_mode;
    uint32_t neurons_per_minicolumn;

    /* Competition parameters */
    cortical_competition_mode_t competition_mode;
    float temperature;             /**< Softmax temperature (default: 1.0) */
    uint32_t k_winners;            /**< Number of winners for K-WINNERS mode */

    /* Lateral inhibition */
    bool enable_lateral_inhibition;
    float inhibition_strength;     /**< Lateral inhibition strength [0, 1] */
    float inhibition_sigma;        /**< Inhibition spread (minicolumn units) */

    /* Hypercolumn configuration */
    hypercolumn_type_t hypercolumn_type;
    bool create_elevation_hypercolumn;  /**< Create second hypercolumn for elevation */

    /* Gain and adaptation */
    float gain_modulation;         /**< Global gain on TSDN inputs [0.1, 10] */
    bool enable_adaptation;        /**< Enable activity-dependent adaptation */
    float adaptation_tau;          /**< Adaptation time constant (ms) */

    /* Integration with existing modules */
    bool use_external_pool;        /**< Use external cortical column pool */
    cortical_column_pool_t* external_pool;

    /* Output configuration */
    bool interpolate_output;       /**< Enable sub-minicolumn interpolation */
    float interpolation_window;    /**< Interpolation window (radians) */
} dragonfly_cortical_config_t;

/**
 * @brief Direction representation in cortical format
 */
typedef struct {
    float azimuth;                 /**< Azimuth angle (radians) */
    float elevation;               /**< Elevation angle (radians) */
    float confidence;              /**< Direction confidence [0, 1] */

    /* Minicolumn activations */
    float activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    uint32_t winner_index;         /**< Index of most active minicolumn */
    float winner_activation;       /**< Activation of winner */

    /* Population statistics */
    float entropy;                 /**< Entropy of activation distribution */
    float sparseness;              /**< Sparseness of representation */
    uint64_t timestamp_us;         /**< Timestamp of this representation */
} cortical_direction_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t tsdn_to_cortical_count;   /**< TSDN -> cortical conversions */
    uint64_t cortical_to_tsdn_count;   /**< Cortical -> TSDN conversions */
    uint64_t hypercolumn_updates;      /**< Hypercolumn update calls */

    /* Competition metrics */
    float avg_winner_activation;   /**< Average winning minicolumn activation */
    float avg_entropy;             /**< Average distribution entropy */
    float avg_confidence;          /**< Average direction confidence */

    /* Performance */
    uint64_t total_processing_time_us;
    float avg_processing_time_us;

    /* Adaptation state */
    float current_gain;            /**< Current adapted gain */
    float adaptation_level;        /**< Current adaptation level */

    /* Error tracking */
    uint32_t conversion_errors;
    uint32_t invalid_inputs;
} cortical_bridge_stats_t;

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Opaque bridge structure
 */
typedef struct dragonfly_cortical_bridge_s dragonfly_cortical_bridge_t;

/**
 * @brief Get default configuration
 *
 * @param config Configuration structure to fill
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_bridge_default_config(dragonfly_cortical_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int dragonfly_cortical_bridge_validate_config(const dragonfly_cortical_config_t* config);

/**
 * @brief Create cortical bridge
 *
 * @param dragonfly Dragonfly system (optional, for direct integration)
 * @param tsdn TSDN population (optional, for standalone use)
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
dragonfly_cortical_bridge_t* dragonfly_cortical_bridge_create(
    dragonfly_system_t* dragonfly,
    tsdn_population_t* tsdn,
    const dragonfly_cortical_config_t* config
);

/**
 * @brief Destroy cortical bridge
 *
 * @param bridge Bridge handle
 */
void dragonfly_cortical_bridge_destroy(dragonfly_cortical_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_bridge_reset(dragonfly_cortical_bridge_t* bridge);

//=============================================================================
// TSDN to Cortical Conversion
//=============================================================================

/**
 * @brief Convert TSDN firing rates to cortical column activations
 *
 * Maps the 16 TSDN firing rates to minicolumn activations with optional
 * lateral inhibition and competition.
 *
 * @param bridge Bridge handle
 * @param tsdn_firing_rates Array of 16 TSDN firing rates [0, 1]
 * @param direction Output cortical direction representation
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_tsdn_to_column(
    dragonfly_cortical_bridge_t* bridge,
    const float* tsdn_firing_rates,
    cortical_direction_t* direction
);

/**
 * @brief Convert TSDN population vector directly to cortical format
 *
 * Uses TSDN population vector readout and maps to cortical representation.
 *
 * @param bridge Bridge handle
 * @param direction Output cortical direction
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_sync_from_tsdn(
    dragonfly_cortical_bridge_t* bridge,
    cortical_direction_t* direction
);

//=============================================================================
// Cortical to TSDN Conversion
//=============================================================================

/**
 * @brief Convert cortical direction to TSDN firing rates
 *
 * @param bridge Bridge handle
 * @param direction Cortical direction input
 * @param tsdn_firing_rates Output array of 16 TSDN firing rates
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_column_to_tsdn(
    dragonfly_cortical_bridge_t* bridge,
    const cortical_direction_t* direction,
    float* tsdn_firing_rates
);

/**
 * @brief Generate TSDN pattern from angle
 *
 * Creates expected TSDN firing pattern for a given direction angle.
 *
 * @param bridge Bridge handle
 * @param azimuth Target azimuth (radians)
 * @param elevation Target elevation (radians, ignored in 2D mode)
 * @param tsdn_firing_rates Output array of 16 TSDN firing rates
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_angle_to_tsdn(
    dragonfly_cortical_bridge_t* bridge,
    float azimuth,
    float elevation,
    float* tsdn_firing_rates
);

//=============================================================================
// Hypercolumn Operations
//=============================================================================

/**
 * @brief Update hypercolumn with new TSDN input
 *
 * Processes TSDN input through cortical column competition dynamics.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_update_hypercolumn(dragonfly_cortical_bridge_t* bridge);

/**
 * @brief Get current hypercolumn state
 *
 * @param bridge Bridge handle
 * @param direction Output current direction representation
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_get_direction(
    const dragonfly_cortical_bridge_t* bridge,
    cortical_direction_t* direction
);

/**
 * @brief Set competition parameters dynamically
 *
 * @param bridge Bridge handle
 * @param mode Competition mode
 * @param temperature Softmax temperature (only for SOFTMAX mode)
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_set_competition(
    dragonfly_cortical_bridge_t* bridge,
    cortical_competition_mode_t mode,
    float temperature
);

/**
 * @brief Apply lateral inhibition
 *
 * @param bridge Bridge handle
 * @param activations Input/output minicolumn activations
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_apply_lateral_inhibition(
    dragonfly_cortical_bridge_t* bridge,
    float* activations
);

//=============================================================================
// Interpolation and Readout
//=============================================================================

/**
 * @brief Interpolate direction from minicolumn activations
 *
 * Uses center-of-mass interpolation around the winner for sub-minicolumn
 * precision direction readout.
 *
 * @param bridge Bridge handle
 * @param activations Minicolumn activations
 * @param azimuth Output interpolated azimuth
 * @param confidence Output confidence
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_interpolate_direction(
    const dragonfly_cortical_bridge_t* bridge,
    const float* activations,
    float* azimuth,
    float* confidence
);

/**
 * @brief Compute entropy of activation distribution
 *
 * @param activations Minicolumn activations
 * @param count Number of minicolumns
 * @return Entropy in bits
 */
float dragonfly_cortical_compute_entropy(
    const float* activations,
    uint32_t count
);

/**
 * @brief Compute sparseness of activation
 *
 * @param activations Minicolumn activations
 * @param count Number of minicolumns
 * @return Sparseness [0, 1] (1 = maximally sparse)
 */
float dragonfly_cortical_compute_sparseness(
    const float* activations,
    uint32_t count
);

//=============================================================================
// Gain and Adaptation
//=============================================================================

/**
 * @brief Set gain modulation
 *
 * @param bridge Bridge handle
 * @param gain Gain value [0.1, 10]
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_set_gain(
    dragonfly_cortical_bridge_t* bridge,
    float gain
);

/**
 * @brief Get current gain
 *
 * @param bridge Bridge handle
 * @return Current gain value
 */
float dragonfly_cortical_get_gain(const dragonfly_cortical_bridge_t* bridge);

/**
 * @brief Update adaptation state
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_update_adaptation(
    dragonfly_cortical_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_bridge_get_stats(
    const dragonfly_cortical_bridge_t* bridge,
    cortical_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_bridge_reset_stats(dragonfly_cortical_bridge_t* bridge);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_bridge_get_config(
    const dragonfly_cortical_bridge_t* bridge,
    dragonfly_cortical_config_t* config
);

/**
 * @brief Set configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_cortical_bridge_set_config(
    dragonfly_cortical_bridge_t* bridge,
    const dragonfly_cortical_config_t* config
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get competition mode name
 *
 * @param mode Competition mode
 * @return Mode name string
 */
const char* dragonfly_cortical_competition_name(cortical_competition_mode_t mode);

/**
 * @brief Get mapping mode name
 *
 * @param mode Mapping mode
 * @return Mode name string
 */
const char* dragonfly_cortical_mapping_name(cortical_mapping_mode_t mode);

/**
 * @brief Get hypercolumn type name
 *
 * @param type Hypercolumn type
 * @return Type name string
 */
const char* dragonfly_cortical_hypercolumn_name(hypercolumn_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_CORTICAL_BRIDGE_H */
