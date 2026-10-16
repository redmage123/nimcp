/**
 * @file nimcp_astrocytes_sleep_bridge.h
 * @brief Sleep-Astrocyte Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and astrocytes
 * WHY:  Astrocytes are critical for adenosine accumulation, glymphatic clearance,
 *       and synaptic renormalization during sleep
 * HOW:  Sleep state modulates astrocyte calcium signaling, gliotransmitter release,
 *       and metabolic support
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active glutamate uptake, high metabolic demand, lactate shuttle active
 * - DROWSY: Adenosine accumulation increases, sleep pressure builds
 * - LIGHT_NREM: Calcium wave frequency increases, gap junction coupling increases
 * - DEEP_NREM: Peak glymphatic clearance (10-20x), synaptic downscaling
 * - REM: Moderate activity, selective metabolic support
 *
 * Key astrocyte-sleep functions:
 * - Adenosine release: Primary sleep pressure signal (A1 receptor activation)
 * - Glymphatic clearance: Astrocyte endfeet regulate perivascular flow
 * - Synaptic renormalization: Coordinate global downscaling during NREM
 * - Calcium waves: Sleep oscillations modulate astrocyte network activity
 * - Lactate shuttle: Sleep-dependent modulation of neuronal metabolism
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTES_SLEEP_BRIDGE_H
#define NIMCP_ASTROCYTES_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "utils/thread/nimcp_thread.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adenosine release modulation (relative to baseline) */
#define ASTRO_SLEEP_ADENOSINE_AWAKE         0.3f   /* Low baseline during activity */
#define ASTRO_SLEEP_ADENOSINE_DROWSY        0.7f   /* Building sleep pressure */
#define ASTRO_SLEEP_ADENOSINE_LIGHT_NREM    0.4f   /* Declining as sleep clears */
#define ASTRO_SLEEP_ADENOSINE_DEEP_NREM     0.2f   /* Minimal during restorative sleep */
#define ASTRO_SLEEP_ADENOSINE_REM           0.3f   /* Slight increase in REM */

/* Glymphatic clearance modulation */
#define ASTRO_SLEEP_GLYMPHATIC_AWAKE        0.1f   /* Minimal during wakefulness */
#define ASTRO_SLEEP_GLYMPHATIC_DROWSY       0.3f   /* Beginning to increase */
#define ASTRO_SLEEP_GLYMPHATIC_LIGHT_NREM   0.6f   /* Moderate clearance */
#define ASTRO_SLEEP_GLYMPHATIC_DEEP_NREM    1.0f   /* Peak clearance (10-20x baseline) */
#define ASTRO_SLEEP_GLYMPHATIC_REM          0.5f   /* Reduced during REM */

/* Calcium wave frequency modulation */
#define ASTRO_SLEEP_CALCIUM_AWAKE           1.0f   /* Active signaling */
#define ASTRO_SLEEP_CALCIUM_DROWSY          0.8f   /* Reducing */
#define ASTRO_SLEEP_CALCIUM_LIGHT_NREM      1.2f   /* Synchronized waves */
#define ASTRO_SLEEP_CALCIUM_DEEP_NREM       1.5f   /* Peak slow oscillation coupling */
#define ASTRO_SLEEP_CALCIUM_REM             0.9f   /* Moderate activity */

/* Gap junction coupling modulation */
#define ASTRO_SLEEP_COUPLING_AWAKE          0.6f   /* Reduced during local processing */
#define ASTRO_SLEEP_COUPLING_DROWSY         0.8f   /* Increasing */
#define ASTRO_SLEEP_COUPLING_LIGHT_NREM     1.0f   /* Full coupling */
#define ASTRO_SLEEP_COUPLING_DEEP_NREM      1.2f   /* Enhanced network coordination */
#define ASTRO_SLEEP_COUPLING_REM            0.9f   /* Slightly reduced */

/* Synaptic downscaling modulation */
#define ASTRO_SLEEP_DOWNSCALE_AWAKE         0.0f   /* No downscaling during wake */
#define ASTRO_SLEEP_DOWNSCALE_DROWSY        0.0f   /* Not yet active */
#define ASTRO_SLEEP_DOWNSCALE_LIGHT_NREM    0.3f   /* Beginning */
#define ASTRO_SLEEP_DOWNSCALE_DEEP_NREM     1.0f   /* Peak downscaling */
#define ASTRO_SLEEP_DOWNSCALE_REM           0.2f   /* Selective consolidation */

/**
 * WHAT: Configuration for astrocyte-sleep integration
 * WHY:  Control sensitivity to sleep states
 * HOW:  Enable/disable modulation features
 */
typedef struct {
    bool enable_adenosine_modulation;       /**< Modulate adenosine release by sleep state */
    bool enable_glymphatic_modulation;      /**< Modulate glymphatic clearance */
    bool enable_calcium_modulation;         /**< Modulate calcium wave dynamics */
    bool enable_coupling_modulation;        /**< Modulate gap junction coupling */
    bool enable_downscaling_modulation;     /**< Coordinate synaptic downscaling */
    bool enable_lactate_modulation;         /**< Modulate lactate shuttle */
    float modulation_strength;              /**< Overall strength [0-1] */
    float glymphatic_clearance_multiplier;  /**< Peak clearance boost [default: 15.0] */
    float adenosine_decay_rate;             /**< Adenosine clearance rate during sleep */
} astro_sleep_config_t;

/**
 * WHAT: Sleep-modulated astrocyte effects
 * WHY:  Track how sleep state affects astrocyte behavior
 * HOW:  Computed effects applied to astrocyte parameters
 */
typedef struct {
    float adenosine_level;                 /**< Current adenosine level [0-1] */
    float glymphatic_clearance_factor;     /**< Waste removal efficiency */
    float calcium_wave_factor;             /**< Calcium signaling modulation */
    float gap_junction_coupling_factor;    /**< Network coordination level */
    float synaptic_renormalization_factor; /**< Downscaling strength */
    float lactate_shuttle_factor;          /**< Metabolic support level */
    sleep_state_t current_state;           /**< Current sleep state */
    float sleep_pressure;                  /**< Current sleep pressure [0-1] */
    bool glymphatic_active;                /**< Glymphatic system active */
    bool downscaling_active;               /**< Synaptic downscaling in progress */
} astro_sleep_effects_t;

/**
 * WHAT: Opaque handle to astrocyte-sleep bridge
 * WHY:  Encapsulation of bridge implementation
 * HOW:  Pointer to internal structure
 */
typedef struct astro_sleep_bridge_struct* astro_sleep_bridge_t;

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default astrocyte-sleep configuration
 * WHY:  Sensible defaults for typical use
 * HOW:  Return pre-configured struct
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * DEFAULTS:
 * - All modulations enabled
 * - Modulation strength: 1.0 (full effect)
 * - Glymphatic multiplier: 15.0x (typical during deep sleep)
 * - Adenosine decay: 0.05 per minute
 */
int astro_sleep_default_config(astro_sleep_config_t* config);

/**
 * WHAT: Create astrocyte-sleep bridge
 * WHY:  Initialize integration between sleep system and astrocytes
 * HOW:  Allocate structures, register callbacks, set initial state
 *
 * @param config Configuration parameters (NULL for defaults)
 * @param sleep Sleep system handle
 * @return Bridge handle or NULL on failure
 */
astro_sleep_bridge_t astro_sleep_create(
    const astro_sleep_config_t* config,
    sleep_system_t sleep);

/**
 * WHAT: Destroy astrocyte-sleep bridge
 * WHY:  Free resources
 * HOW:  Unregister callbacks, free memory
 *
 * @param bridge Bridge handle (NULL-safe)
 */
void astro_sleep_destroy(astro_sleep_bridge_t bridge);

/* ========================================================================
 * ADENOSINE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Accumulate adenosine from neural activity
 * WHY:  Sleep pressure builds from metabolic activity
 * HOW:  Increase adenosine level based on activity
 *
 * @param bridge Bridge handle
 * @param activity_level Neural activity level [0-1]
 * @return Current adenosine level [0-1]
 */
float astro_sleep_accumulate_adenosine(
    astro_sleep_bridge_t bridge,
    float activity_level);

/**
 * WHAT: Clear adenosine (during sleep)
 * WHY:  Sleep reduces adenosine, restoring wakefulness capacity
 * HOW:  Decrease adenosine based on sleep depth
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return Remaining adenosine level [0-1]
 */
float astro_sleep_clear_adenosine(
    astro_sleep_bridge_t bridge,
    float dt_ms);

/**
 * WHAT: Get current adenosine level
 * WHY:  Query sleep pressure state
 * HOW:  Return cached adenosine level
 *
 * @param bridge Bridge handle
 * @return Adenosine level [0-1]
 */
float astro_sleep_get_adenosine_level(const astro_sleep_bridge_t bridge);

/* ========================================================================
 * GLYMPHATIC FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Enable glymphatic clearance mode
 * WHY:  Activate waste removal during sleep
 * HOW:  Set glymphatic factor based on sleep depth
 *
 * @param bridge Bridge handle
 * @return Current clearance rate multiplier
 */
float astro_sleep_enable_glymphatic(astro_sleep_bridge_t bridge);

/**
 * WHAT: Get current glymphatic clearance rate
 * WHY:  Query waste removal efficiency
 * HOW:  Return clearance multiplier based on sleep state
 *
 * @param bridge Bridge handle
 * @return Clearance rate multiplier (1.0 = baseline, up to 20x during deep sleep)
 */
float astro_sleep_get_clearance_rate(const astro_sleep_bridge_t bridge);

/**
 * WHAT: Check if glymphatic system is active
 * WHY:  Determine if in clearance mode
 * HOW:  Check current sleep state and clearance factor
 *
 * @param bridge Bridge handle
 * @return true if glymphatic clearance is active
 */
bool astro_sleep_is_glymphatic_active(const astro_sleep_bridge_t bridge);

/* ========================================================================
 * SYNAPTIC DOWNSCALING FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Initiate synaptic downscaling
 * WHY:  Coordinate homeostatic scaling during sleep
 * HOW:  Set downscaling factor based on sleep depth
 *
 * @param bridge Bridge handle
 * @return Downscaling factor [0-1]
 */
float astro_sleep_initiate_downscaling(astro_sleep_bridge_t bridge);

/**
 * WHAT: Get synaptic renormalization factor
 * WHY:  Query current downscaling strength
 * HOW:  Return renormalization factor
 *
 * @param bridge Bridge handle
 * @return Renormalization factor [0-1] (0 = no change, 1 = full downscale)
 */
float astro_sleep_get_renormalization_factor(const astro_sleep_bridge_t bridge);

/**
 * WHAT: Check if downscaling is active
 * WHY:  Determine if in synaptic homeostasis mode
 * HOW:  Check current sleep state and downscaling factor
 *
 * @param bridge Bridge handle
 * @return true if downscaling is active
 */
bool astro_sleep_is_downscaling_active(const astro_sleep_bridge_t bridge);

/* ========================================================================
 * CALCIUM AND COUPLING FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get calcium wave modulation factor
 * WHY:  Determine astrocyte network signaling level
 * HOW:  Return factor based on sleep state
 *
 * @param bridge Bridge handle
 * @return Calcium wave frequency factor
 */
float astro_sleep_get_calcium_factor(const astro_sleep_bridge_t bridge);

/**
 * WHAT: Get gap junction coupling factor
 * WHY:  Determine astrocyte network connectivity
 * HOW:  Return coupling factor based on sleep state
 *
 * @param bridge Bridge handle
 * @return Coupling factor
 */
float astro_sleep_get_coupling_factor(const astro_sleep_bridge_t bridge);

/* ========================================================================
 * STATE QUERY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get current sleep-modulated effects
 * WHY:  Query all astrocyte modulations at once
 * HOW:  Copy current effects to output struct
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int astro_sleep_get_effects(
    const astro_sleep_bridge_t bridge,
    astro_sleep_effects_t* effects);

/**
 * WHAT: Update bridge state for new timestep
 * WHY:  Evolve adenosine, clearance, and modulations
 * HOW:  Called each simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int astro_sleep_update(astro_sleep_bridge_t bridge, float dt_ms);

/* ========================================================================
 * BIO-ASYNC INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging
 * HOW:  Register with bio-async system
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int astro_sleep_connect_bio_async(astro_sleep_bridge_t bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister from bio-async system
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int astro_sleep_disconnect_bio_async(astro_sleep_bridge_t bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection status
 * HOW:  Return enabled flag
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool astro_sleep_is_bio_async_connected(const astro_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTES_SLEEP_BRIDGE_H */
