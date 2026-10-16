//=============================================================================
// nimcp_hemispheric_glial_bridge.h - Per-Hemisphere Glial Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_glial_bridge.h
 * @brief Glial system integration for hemispheric brain architecture
 *
 * WHAT: Per-hemisphere glial networks with cross-hemisphere coordination
 * WHY:  Glial cells exhibit lateralized functions affecting cognition differently
 * HOW:  Separate astrocyte/oligodendrocyte/microglia networks per hemisphere,
 *       calcium wave transfer via callosum analog
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HEMISPHERIC GLIAL ASYMMETRIES:
 * ------------------------------
 * 1. Astrocyte Density:
 *    - Left hemisphere: Higher density in language regions (Broca's, Wernicke's)
 *    - Right hemisphere: More extensive coverage in spatial processing areas
 *    - Supports local specialization through metabolic support
 *
 * 2. Myelination Patterns:
 *    - Left hemisphere: Denser myelination in language pathways
 *    - Right hemisphere: Different myelination in emotional pathways
 *    - Affects signal timing and hemisphere-specific processing speed
 *
 * 3. Microglia and Synaptic Pruning:
 *    - Pruning patterns differ by hemisphere during development
 *    - Affects which circuits are preserved/eliminated per side
 *
 * CROSS-HEMISPHERE GLIAL COMMUNICATION:
 * -------------------------------------
 * - Astrocyte calcium waves can propagate through commissures
 * - Gliotransmitter diffusion affects bilateral regions
 * - Coordinated metabolic support during high-demand tasks
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    HEMISPHERIC GLIAL BRIDGE                             |
 * +=========================================================================+
 * |                                                                          |
 * |   LEFT HEMISPHERE GLIAL          RIGHT HEMISPHERE GLIAL                 |
 * |   ┌────────────────────┐         ┌────────────────────┐                |
 * |   │  Astrocyte Network │         │  Astrocyte Network │                |
 * |   │  - Language bias   │         │  - Spatial bias    │                |
 * |   │  - High Ca2+ sync  │◀───────▶│  - Holistic sync   │                |
 * |   └────────────────────┘         └────────────────────┘                |
 * |   ┌────────────────────┐         ┌────────────────────┐                |
 * |   │  Oligodendrocytes  │         │  Oligodendrocytes  │                |
 * |   │  - Dense language  │         │  - Dense spatial   │                |
 * |   │    pathway myelin  │         │    pathway myelin  │                |
 * |   └────────────────────┘         └────────────────────┘                |
 * |   ┌────────────────────┐         ┌────────────────────┐                |
 * |   │  Microglia Network │         │  Microglia Network │                |
 * |   │  - Circuit pruning │         │  - Circuit pruning │                |
 * |   └────────────────────┘         └────────────────────┘                |
 * |                                                                          |
 * |            ┌──────────────────────────────────────┐                     |
 * |            │      CROSS-HEMISPHERE BRIDGE         │                     |
 * |            │  - Calcium wave transfer             │                     |
 * |            │  - Metabolic demand signaling        │                     |
 * |            │  - Coordinated pruning signals       │                     |
 * |            └──────────────────────────────────────┘                     |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_GLIAL_BRIDGE_H
#define NIMCP_HEMISPHERIC_GLIAL_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "async/nimcp_bio_async.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default cross-hemisphere calcium transfer coefficient */
#define HEMI_GLIAL_CALCIUM_TRANSFER_COEFF    0.15f

/** Default cross-hemisphere metabolic coupling */
#define HEMI_GLIAL_METABOLIC_COUPLING        0.20f

/** Default myelin asymmetry factor (left/right ratio) */
#define HEMI_GLIAL_MYELIN_ASYMMETRY_BASE     1.0f

/** Maximum cross-hemisphere calcium wave speed (um/s) */
#define HEMI_GLIAL_MAX_WAVE_SPEED            20.0f

/** Astrocyte density bias: left hemisphere (language) */
#define HEMI_GLIAL_ASTRO_DENSITY_LEFT        1.2f

/** Astrocyte density bias: right hemisphere (spatial) */
#define HEMI_GLIAL_ASTRO_DENSITY_RIGHT       1.0f

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Glial specialization for hemisphere
 */
typedef enum {
    GLIAL_SPEC_BALANCED,          /**< Equal glial distribution */
    GLIAL_SPEC_LANGUAGE_DOMINANT, /**< Left - higher astrocyte density */
    GLIAL_SPEC_SPATIAL_DOMINANT,  /**< Right - extended astrocyte coverage */
    GLIAL_SPEC_CUSTOM             /**< Custom specialization */
} glial_specialization_t;

/**
 * @brief Per-hemisphere glial effects
 */
typedef struct {
    float astrocyte_modulation;    /**< Average synaptic modulation factor */
    float avg_calcium_level;       /**< Average astrocyte calcium (uM) */
    float myelination_factor;      /**< Average myelination strength */
    float pruning_rate;            /**< Current pruning rate */
    float metabolic_support;       /**< Metabolic support level (0-1) */
    float gliotransmitter_level;   /**< Glutamate/D-serine release */
} hemisphere_glial_effects_t;

/**
 * @brief Cross-hemisphere glial transfer state
 */
typedef struct {
    float calcium_transfer_rate;   /**< Rate of Ca2+ wave transfer */
    float metabolic_flow;          /**< Direction of metabolic support (-1 to 1) */
    bool wave_propagating;         /**< Calcium wave currently crossing */
    hemisphere_id_t wave_source;   /**< Source hemisphere of current wave */
    uint64_t wave_start_time;      /**< When wave started (us) */
} cross_hemisphere_glial_t;

/**
 * @brief Hemispheric glial bridge configuration
 */
typedef struct {
    // Per-hemisphere specialization
    glial_specialization_t left_specialization;
    glial_specialization_t right_specialization;

    // Astrocyte parameters
    float left_astrocyte_density_factor;   /**< Density multiplier for left */
    float right_astrocyte_density_factor;  /**< Density multiplier for right */
    float calcium_wave_threshold;          /**< Threshold for cross-hemi waves */

    // Myelination parameters
    float left_myelin_factor;              /**< Left hemisphere myelin bias */
    float right_myelin_factor;             /**< Right hemisphere myelin bias */

    // Cross-hemisphere coupling
    float calcium_transfer_coefficient;    /**< How much Ca2+ crosses */
    float metabolic_coupling_strength;     /**< Metabolic support coupling */
    bool enable_cross_hemisphere_waves;    /**< Allow Ca2+ waves to cross */

    // Bio-async settings
    bool enable_bio_async;                 /**< Enable bio-async messaging */
} hemispheric_glial_config_t;

/**
 * @brief Hemispheric glial bridge statistics
 */
typedef struct {
    uint64_t glial_updates;                /**< Total update cycles */
    uint64_t cross_hemisphere_waves;       /**< Waves that crossed hemispheres */
    uint64_t pruning_events_left;          /**< Pruning events in left */
    uint64_t pruning_events_right;         /**< Pruning events in right */
    float avg_left_calcium;                /**< Average left astrocyte Ca2+ */
    float avg_right_calcium;               /**< Average right astrocyte Ca2+ */
    float avg_left_myelination;            /**< Average left myelination */
    float avg_right_myelination;           /**< Average right myelination */
    float metabolic_balance;               /**< Net metabolic flow (- = L to R) */
} hemispheric_glial_stats_t;

/**
 * @brief Hemispheric glial bridge structure
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // Connected systems
    hemispheric_brain_t* brain;            /**< Hemispheric brain */

    // Per-hemisphere glial networks
    glial_integration_t* left_glial;       /**< Left hemisphere glial system */
    glial_integration_t* right_glial;      /**< Right hemisphere glial system */

    // Configuration
    hemispheric_glial_config_t config;

    // Per-hemisphere effects
    hemisphere_glial_effects_t left_effects;
    hemisphere_glial_effects_t right_effects;

    // Cross-hemisphere state
    cross_hemisphere_glial_t cross_state;

    // Statistics
    hemispheric_glial_stats_t stats;

    // Bio-async
    // Thread safety
    // State
    bool initialized;
} hemispheric_glial_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default hemispheric glial configuration
 */
hemispheric_glial_config_t hemispheric_glial_default_config(void);

/**
 * @brief Create hemispheric glial bridge
 *
 * @param config Bridge configuration
 * @param brain Hemispheric brain to connect
 * @return Bridge instance or NULL on failure
 */
hemispheric_glial_bridge_t* hemispheric_glial_create(
    const hemispheric_glial_config_t* config,
    hemispheric_brain_t* brain
);

/**
 * @brief Destroy hemispheric glial bridge
 */
void hemispheric_glial_destroy(hemispheric_glial_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update hemispheric glial systems
 *
 * WHAT: Steps both hemisphere glial networks and handles cross-hemi transfer
 * WHY:  Glial dynamics affect neural processing
 *
 * @param bridge Glial bridge instance
 * @param dt Time step in seconds
 * @return 0 on success, negative on error
 */
int hemispheric_glial_update(hemispheric_glial_bridge_t* bridge, float dt);

/**
 * @brief Apply glial modulation to hemispheric brain
 *
 * @param bridge Glial bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_glial_apply_modulation(hemispheric_glial_bridge_t* bridge);

//=============================================================================
// Astrocyte API
//=============================================================================

/**
 * @brief Stimulate astrocyte in specific hemisphere
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @param astrocyte_id Astrocyte ID within that hemisphere
 * @param intensity Stimulus intensity
 * @return 0 on success, negative on error
 */
int hemispheric_glial_stimulate_astrocyte(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t astrocyte_id,
    float intensity
);

/**
 * @brief Get astrocyte calcium level
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @param astrocyte_id Astrocyte ID
 * @return Calcium level in uM, or 0 on error
 */
float hemispheric_glial_get_astrocyte_calcium(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t astrocyte_id
);

/**
 * @brief Trigger cross-hemisphere calcium wave
 *
 * WHAT: Initiates calcium wave transfer between hemispheres
 * WHY:  Coordinates bilateral glial activity
 *
 * @param bridge Glial bridge
 * @param source Source hemisphere
 * @param intensity Wave intensity
 * @return 0 on success, negative on error
 */
int hemispheric_glial_trigger_cross_wave(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t source,
    float intensity
);

//=============================================================================
// Myelination API
//=============================================================================

/**
 * @brief Get myelination factor for neuron in hemisphere
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @param neuron_id Neuron ID
 * @return Myelination factor (0.0-1.0)
 */
float hemispheric_glial_get_myelination(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t neuron_id
);

/**
 * @brief Set hemisphere-specific myelin bias
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @param factor Myelin factor multiplier
 * @return 0 on success, negative on error
 */
int hemispheric_glial_set_myelin_factor(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float factor
);

//=============================================================================
// Metabolic API
//=============================================================================

/**
 * @brief Get metabolic support level for hemisphere
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @return Metabolic support level (0.0-1.0)
 */
float hemispheric_glial_get_metabolic_support(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Transfer metabolic resources between hemispheres
 *
 * @param bridge Glial bridge
 * @param amount Amount to transfer (positive = left to right)
 * @return 0 on success, negative on error
 */
int hemispheric_glial_transfer_metabolic(
    hemispheric_glial_bridge_t* bridge,
    float amount
);

//=============================================================================
// Pruning API
//=============================================================================

/**
 * @brief Check if synapse should be pruned in hemisphere
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @param synapse_id Synapse ID
 * @return true if synapse should be pruned
 */
bool hemispheric_glial_should_prune(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    uint32_t synapse_id
);

/**
 * @brief Get pruning rate for hemisphere
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @return Pruning rate (synapses per second)
 */
float hemispheric_glial_get_pruning_rate(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get glial effects for hemisphere
 */
hemisphere_glial_effects_t hemispheric_glial_get_effects(
    const hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Get bridge statistics
 */
hemispheric_glial_stats_t hemispheric_glial_get_stats(
    const hemispheric_glial_bridge_t* bridge
);

/**
 * @brief Reset bridge statistics
 */
void hemispheric_glial_reset_stats(hemispheric_glial_bridge_t* bridge);

/**
 * @brief Get cross-hemisphere glial state
 */
cross_hemisphere_glial_t hemispheric_glial_get_cross_state(
    const hemispheric_glial_bridge_t* bridge
);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect glial integration system to hemisphere
 *
 * @param bridge Glial bridge
 * @param hemisphere Target hemisphere
 * @param glial Glial integration system
 * @return 0 on success, negative on error
 */
int hemispheric_glial_connect_glial(
    hemispheric_glial_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    glial_integration_t* glial
);

//=============================================================================
// Bio-async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemispheric_glial_connect_bio_async(hemispheric_glial_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int hemispheric_glial_disconnect_bio_async(hemispheric_glial_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_GLIAL_BRIDGE_H
