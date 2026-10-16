/**
 * @file nimcp_microglia_sleep_bridge.h
 * @brief Sleep-Microglia Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and microglia
 * WHY:  Microglia are critical for sleep-dependent synaptic pruning and waste clearance
 * HOW:  Sleep state modulates phagocytosis rate, surveillance activity, and pruning behavior
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Low surveillance activity, minimal pruning (synapses active)
 * - DROWSY: Surveillance ramps up, microglia extend processes
 * - LIGHT_NREM: Increased surveillance, initial synaptic assessment
 * - DEEP_NREM: Peak phagocytosis, aggressive pruning of weak synapses
 * - REM: Moderate activity, selective pruning of non-critical connections
 *
 * Sleep-dependent microglia functions:
 * - Glymphatic clearance: Waste removal increases 10-20x during sleep
 * - Synaptic pruning: Primarily occurs during deep NREM sleep
 * - Process extension: Microglia extend processes to survey more synapses
 * - Cytokine regulation: Anti-inflammatory cytokines peak during deep sleep
 * - Activity-dependent refinement: Remove synapses with low activity scores
 *
 * The glymphatic system:
 * - Interstitial fluid flow increases during sleep
 * - Microglia contribute to metabolic waste removal
 * - Critical for clearing beta-amyloid and other neurotoxic proteins
 * - Sleep deprivation impairs glymphatic function
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MICROGLIA_SLEEP_BRIDGE_H
#define NIMCP_MICROGLIA_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Phagocytosis rate modulation (relative to baseline) */
#define MICROGLIA_SLEEP_PHAGO_AWAKE         0.1f   /* Minimal pruning while awake */
#define MICROGLIA_SLEEP_PHAGO_DROWSY        0.3f   /* Ramping up */
#define MICROGLIA_SLEEP_PHAGO_LIGHT_NREM    0.6f   /* Active assessment */
#define MICROGLIA_SLEEP_PHAGO_DEEP_NREM     1.0f   /* Peak pruning */
#define MICROGLIA_SLEEP_PHAGO_REM           0.5f   /* Selective pruning */

/* Surveillance activity modulation */
#define MICROGLIA_SLEEP_SURVEILLANCE_AWAKE         0.5f   /* Reduced during activity */
#define MICROGLIA_SLEEP_SURVEILLANCE_DROWSY        0.8f   /* Increasing */
#define MICROGLIA_SLEEP_SURVEILLANCE_LIGHT_NREM    1.0f   /* Full surveillance */
#define MICROGLIA_SLEEP_SURVEILLANCE_DEEP_NREM     1.2f   /* Enhanced surveillance */
#define MICROGLIA_SLEEP_SURVEILLANCE_REM           0.9f   /* Slightly reduced */

/* Process extension modulation (affects surveillance radius) */
#define MICROGLIA_SLEEP_PROCESS_AWAKE         0.7f   /* Retracted during activity */
#define MICROGLIA_SLEEP_PROCESS_DROWSY        0.9f   /* Extending */
#define MICROGLIA_SLEEP_PROCESS_LIGHT_NREM    1.0f   /* Fully extended */
#define MICROGLIA_SLEEP_PROCESS_DEEP_NREM     1.1f   /* Maximum extension */
#define MICROGLIA_SLEEP_PROCESS_REM           1.0f   /* Maintained */

/**
 * WHAT: Configuration for microglia-sleep integration
 * WHY:  Control sensitivity to sleep states
 * HOW:  Enable/disable modulation features
 */
typedef struct {
    bool enable_phagocytosis_modulation;    /**< Modulate pruning rate by sleep state */
    bool enable_surveillance_modulation;     /**< Modulate surveillance activity */
    bool enable_process_modulation;          /**< Modulate process extension */
    bool enable_glymphatic_clearance;        /**< Enhanced waste removal during sleep */
    float modulation_strength;               /**< Overall strength [0-1] */
    float glymphatic_clearance_multiplier;   /**< Clearance boost during deep sleep [default: 15.0] */
} microglia_sleep_config_t;

/**
 * WHAT: Sleep-modulated microglia effects
 * WHY:  Track how sleep state affects microglia behavior
 * HOW:  Computed effects applied to microglia parameters
 */
typedef struct {
    float phagocytosis_rate_factor;      /**< Multiplier for pruning rate */
    float surveillance_activity_factor;   /**< Multiplier for surveillance */
    float process_extension_factor;       /**< Multiplier for process radius */
    float glymphatic_clearance_factor;    /**< Waste removal efficiency */
    float pruning_threshold_adjustment;   /**< Lower = more aggressive pruning */
    sleep_state_t current_state;          /**< Current sleep state */
    float sleep_pressure;                 /**< Current sleep pressure [0-1] */
    bool microglia_enhanced;              /**< Enhanced mode during deep sleep */
    bool glymphatic_active;               /**< Glymphatic system active */
} microglia_sleep_effects_t;

/**
 * WHAT: Opaque handle to microglia-sleep bridge
 * WHY:  Encapsulation of bridge implementation
 * HOW:  Pointer to internal structure
 */
typedef struct microglia_sleep_bridge_struct* microglia_sleep_bridge_t;

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default microglia-sleep configuration
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
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int microglia_sleep_default_config(microglia_sleep_config_t* config);

/**
 * WHAT: Create microglia-sleep bridge
 * WHY:  Initialize integration between sleep system and microglia
 * HOW:  Allocate structures, register callbacks, set initial state
 *
 * @param config Configuration parameters (NULL for defaults)
 * @param sleep Sleep system handle
 * @return Bridge handle or NULL on failure
 *
 * BIOLOGICAL BASIS:
 * - Registers callback to receive sleep state changes
 * - Immediately queries current sleep state
 * - Initializes modulation factors based on state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
microglia_sleep_bridge_t microglia_sleep_bridge_create(
    const microglia_sleep_config_t* config,
    sleep_system_t sleep);

/**
 * WHAT: Destroy microglia-sleep bridge
 * WHY:  Free resources and prevent memory leaks
 * HOW:  Unregister callbacks, free allocations
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void microglia_sleep_bridge_destroy(microglia_sleep_bridge_t bridge);

/* ========================================================================
 * UPDATE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Update microglia effects based on current sleep state
 * WHY:  Refresh modulation factors (polling mode)
 * HOW:  Query sleep system, recompute effects
 *
 * NOTE: This function is only needed if callback registration failed.
 * Normal operation uses automatic callbacks for immediate updates.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect microglia immediately
 * - Sleep pressure accumulation reduces pruning threshold
 * - Deep sleep triggers peak phagocytosis and glymphatic clearance
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int microglia_sleep_update(microglia_sleep_bridge_t bridge);

/**
 * WHAT: Get current sleep-modulated effects
 * WHY:  Retrieve computed factors for microglia parameters
 * HOW:  Thread-safe copy of effects structure
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 *
 * USAGE:
 * - Call to get current modulation factors
 * - Apply factors to microglia network parameters
 * - Example: pruning_rate *= effects.phagocytosis_rate_factor
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int microglia_sleep_get_effects(const microglia_sleep_bridge_t bridge,
                                 microglia_sleep_effects_t* effects);

/* ========================================================================
 * QUERY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get phagocytosis rate factor
 * WHY:  Quick access to pruning modulation
 * HOW:  Thread-safe read of current factor
 *
 * @param bridge Bridge handle
 * @return Phagocytosis rate multiplier [0-1]
 *
 * BIOLOGICAL BASIS:
 * - Deep NREM: 1.0 (peak pruning)
 * - Awake: 0.1 (minimal pruning)
 * - REM: 0.5 (selective pruning)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float microglia_sleep_get_phagocytosis_rate(const microglia_sleep_bridge_t bridge);

/**
 * WHAT: Get surveillance activity factor
 * WHY:  Quick access to surveillance modulation
 * HOW:  Thread-safe read of current factor
 *
 * @param bridge Bridge handle
 * @return Surveillance activity multiplier [0.5-1.2]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float microglia_sleep_get_surveillance_activity(const microglia_sleep_bridge_t bridge);

/**
 * WHAT: Check if glymphatic clearance is active
 * WHY:  Determine if waste removal is enhanced
 * HOW:  True during deep NREM sleep
 *
 * @param bridge Bridge handle
 * @return true if glymphatic clearance is active
 *
 * BIOLOGICAL BASIS:
 * - Glymphatic system is 10-20x more active during deep sleep
 * - Interstitial space expands by ~60% during sleep
 * - Critical for clearing neurotoxic proteins
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool microglia_sleep_is_glymphatic_active(const microglia_sleep_bridge_t bridge);

/**
 * WHAT: Check if microglia are in enhanced mode
 * WHY:  Determine if peak activity period (deep NREM)
 * HOW:  True during deep NREM sleep
 *
 * @param bridge Bridge handle
 * @return true if microglia are in enhanced mode
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool microglia_sleep_is_enhanced(const microglia_sleep_bridge_t bridge);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get phagocytosis factor for a given sleep state
 * WHY:  Static lookup for testing or simulation
 * HOW:  Return constant based on state
 *
 * @param state Sleep state
 * @return Phagocytosis rate factor for that state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float microglia_sleep_phagocytosis_for_state(sleep_state_t state);

/**
 * WHAT: Get surveillance factor for a given sleep state
 * WHY:  Static lookup for testing or simulation
 * HOW:  Return constant based on state
 *
 * @param state Sleep state
 * @return Surveillance activity factor for that state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float microglia_sleep_surveillance_for_state(sleep_state_t state);

/**
 * WHAT: Get process extension factor for a given sleep state
 * WHY:  Static lookup for testing or simulation
 * HOW:  Return constant based on state
 *
 * @param state Sleep state
 * @return Process extension factor for that state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float microglia_sleep_process_extension_for_state(sleep_state_t state);

/* ========================================================================
 * BIO-ASYNC INTEGRATION API
 * ======================================================================== */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed microglia signals
 * HOW:  Register with bio_router using BIO_MODULE_MICROGLIA_SLEEP
 *
 * @param bridge Microglia-sleep bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int microglia_sleep_connect_bio_async(microglia_sleep_bridge_t bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Microglia-sleep bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int microglia_sleep_disconnect_bio_async(microglia_sleep_bridge_t bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query bio-async connection status
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Microglia-sleep bridge
 * @return true if connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool microglia_sleep_is_bio_async_connected(const microglia_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MICROGLIA_SLEEP_BRIDGE_H */
