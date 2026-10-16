/**
 * @file nimcp_mirror_neurons_sleep_bridge.h
 * @brief Sleep-Mirror Neurons Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and mirror neuron activity
 * WHY:  Mirror neuron activity, empathy, and action observation depend on sleep state
 * HOW:  Sleep state modulates mirroring activity, empathy sensitivity, and action replay
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active mirroring during action observation and social interaction
 * - DROWSY: Reduced mirroring capacity, decreased empathic resonance
 * - LIGHT NREM: Moderate mirroring suppression, some motor replay
 * - DEEP NREM: Strong mirroring suppression, motor quiescence
 * - REM: Enhanced action replay, dream-based social simulation
 *
 * Sleep deprivation effects on mirror neurons:
 * - Reduced action understanding and imitation accuracy
 * - Impaired empathy and emotional resonance
 * - Slower action recognition and prediction
 * - Difficulty with motor learning through observation
 * - Reduced social cognition capacity
 *
 * Sleep benefits mirror neuron system:
 * - REM consolidates observed actions into motor repertoire
 * - Sleep replays observed actions for offline learning
 * - NREM strengthens action-observation associations
 * - REM enhances social simulation and theory of mind
 * - Sleep prunes unused or weak mirror associations
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_NEURONS_SLEEP_BRIDGE_H
#define NIMCP_MIRROR_NEURONS_SLEEP_BRIDGE_H

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

/* Constants: Mirroring activity modulation by sleep state */
#define MIRROR_SLEEP_ACTIVITY_AWAKE         1.0f
#define MIRROR_SLEEP_ACTIVITY_DROWSY        0.6f
#define MIRROR_SLEEP_ACTIVITY_LIGHT_NREM    0.3f
#define MIRROR_SLEEP_ACTIVITY_DEEP_NREM     0.1f
#define MIRROR_SLEEP_ACTIVITY_REM           0.5f

/* Constants: Empathy sensitivity modulation by sleep state */
#define MIRROR_SLEEP_EMPATHY_AWAKE          1.0f
#define MIRROR_SLEEP_EMPATHY_DROWSY         0.7f
#define MIRROR_SLEEP_EMPATHY_LIGHT_NREM     0.4f
#define MIRROR_SLEEP_EMPATHY_DEEP_NREM      0.2f
#define MIRROR_SLEEP_EMPATHY_REM            0.8f

/* Constants: Action observation sensitivity by sleep state */
#define MIRROR_SLEEP_OBSERVATION_AWAKE      1.0f
#define MIRROR_SLEEP_OBSERVATION_DROWSY     0.5f
#define MIRROR_SLEEP_OBSERVATION_LIGHT_NREM 0.2f
#define MIRROR_SLEEP_OBSERVATION_DEEP_NREM  0.0f
#define MIRROR_SLEEP_OBSERVATION_REM        0.3f

/* Constants: Action replay activity during sleep (offline learning) */
#define MIRROR_SLEEP_REPLAY_AWAKE           0.1f
#define MIRROR_SLEEP_REPLAY_DROWSY          0.2f
#define MIRROR_SLEEP_REPLAY_LIGHT_NREM      0.5f
#define MIRROR_SLEEP_REPLAY_DEEP_NREM       0.3f
#define MIRROR_SLEEP_REPLAY_REM             1.0f

/**
 * WHAT: Configuration for mirror neurons sleep bridge
 * WHY:  Allow tuning of sleep effects on mirror neuron activity
 * HOW:  Enable/disable specific modulations and set strength
 */
typedef struct {
    bool enable_activity_modulation;      /**< Modulate overall mirroring activity */
    bool enable_empathy_modulation;       /**< Modulate empathic resonance */
    bool enable_observation_modulation;   /**< Modulate action observation sensitivity */
    bool enable_replay_modulation;        /**< Modulate offline action replay */
    float modulation_strength;            /**< Global strength of sleep effects [0, 1] */
} mirror_neurons_sleep_config_t;

/**
 * WHAT: Current effects of sleep on mirror neuron system
 * WHY:  Track computed modulation factors for application to mirror neurons
 * HOW:  Updated by bridge, queried by mirror neuron system
 */
typedef struct {
    float mirroring_activity_factor;      /**< Multiplier for mirror neuron firing rate */
    float empathy_modulation_factor;      /**< Multiplier for empathic resonance strength */
    float action_observation_factor;      /**< Multiplier for action observation sensitivity */
    float action_replay_factor;           /**< Multiplier for offline action replay rate */
    float motor_suppression_factor;       /**< Suppression of overt motor output during sleep */
    sleep_state_t current_state;          /**< Current sleep state */
    float sleep_pressure;                 /**< Current sleep pressure [0, 1] */
    bool replay_active;                   /**< Whether action replay is currently active */
} mirror_neurons_sleep_effects_t;

/**
 * WHAT: Opaque handle for mirror neurons sleep bridge
 * WHY:  Encapsulate implementation details
 * HOW:  Pointer to internal bridge structure
 */
typedef struct mirror_neurons_sleep_bridge_struct* mirror_neurons_sleep_bridge_t;

/**
 * WHAT: Initialize default sleep bridge configuration
 * WHY:  Provide sensible defaults for mirror neurons sleep integration
 * HOW:  Sets all modulations enabled with full strength
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 if config is NULL
 */
int mirror_neurons_sleep_default_config(mirror_neurons_sleep_config_t* config);

/**
 * WHAT: Create mirror neurons sleep bridge
 * WHY:  Enable sleep-dependent modulation of mirror neuron activity
 * HOW:  Allocates bridge, registers sleep state callbacks
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons in premotor cortex modulated by arousal state
 * - REM sleep replays observed actions for consolidation
 * - Sleep deprivation impairs action understanding
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep Sleep/wake system to integrate with
 * @return Bridge handle on success, NULL on failure
 */
mirror_neurons_sleep_bridge_t mirror_neurons_sleep_bridge_create(
    const mirror_neurons_sleep_config_t* config,
    sleep_system_t sleep);

/**
 * WHAT: Destroy mirror neurons sleep bridge
 * WHY:  Release resources and unregister callbacks
 * HOW:  Unregisters from sleep system, frees memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_neurons_sleep_bridge_destroy(mirror_neurons_sleep_bridge_t bridge);

/**
 * WHAT: Update mirror neurons sleep effects based on current sleep state
 * WHY:  Compute latest modulation factors from sleep system
 * HOW:  Queries sleep state and pressure, updates all effect factors
 *
 * BIOLOGICAL BASIS:
 * - Sleep pressure reduces mirroring capacity even when awake
 * - Transition states (drowsy) show graded effects
 * - REM enhances replay while suppressing overt motor output
 *
 * @param bridge Bridge to update
 * @return 0 on success, -1 on error
 */
int mirror_neurons_sleep_update(mirror_neurons_sleep_bridge_t bridge);

/**
 * WHAT: Get current sleep effects on mirror neurons
 * WHY:  Allow mirror neuron system to apply modulations
 * HOW:  Thread-safe copy of current effects structure
 *
 * @param bridge Bridge to query
 * @param effects Output structure for effects (must not be NULL)
 * @return 0 on success, -1 on error
 */
int mirror_neurons_sleep_get_effects(
    const mirror_neurons_sleep_bridge_t bridge,
    mirror_neurons_sleep_effects_t* effects);

/**
 * WHAT: Get current mirroring activity factor
 * WHY:  Quick access to primary modulation factor
 * HOW:  Thread-safe read of activity factor
 *
 * @param bridge Bridge to query
 * @return Activity factor [0, 1], or 1.0 if bridge is NULL
 */
float mirror_neurons_sleep_get_activity(const mirror_neurons_sleep_bridge_t bridge);

/**
 * WHAT: Check if action replay is currently active
 * WHY:  Determine if offline motor learning is occurring
 * HOW:  Returns true during sleep states with high replay factor
 *
 * BIOLOGICAL BASIS:
 * - REM sleep shows high motor cortex activity during dreams
 * - Observed actions are replayed during sleep for consolidation
 * - Replay active primarily during REM and light NREM
 *
 * @param bridge Bridge to query
 * @return true if replay active, false otherwise
 */
bool mirror_neurons_sleep_is_replay_active(const mirror_neurons_sleep_bridge_t bridge);

/**
 * WHAT: Get mirroring activity factor for specific sleep state
 * WHY:  Utility function for testing and configuration
 * HOW:  Returns constant factor for given state
 *
 * @param state Sleep state to query
 * @return Activity factor for that state
 */
float mirror_neurons_sleep_activity_for_state(sleep_state_t state);

/**
 * WHAT: Get empathy modulation factor for specific sleep state
 * WHY:  Utility function for testing and configuration
 * HOW:  Returns constant factor for given state
 *
 * @param state Sleep state to query
 * @return Empathy factor for that state
 */
float mirror_neurons_sleep_empathy_for_state(sleep_state_t state);

/**
 * WHAT: Get action observation factor for specific sleep state
 * WHY:  Utility function for testing and configuration
 * HOW:  Returns constant factor for given state
 *
 * @param state Sleep state to query
 * @return Observation factor for that state
 */
float mirror_neurons_sleep_observation_for_state(sleep_state_t state);

/**
 * WHAT: Get action replay factor for specific sleep state
 * WHY:  Utility function for testing and configuration
 * HOW:  Returns constant factor for given state
 *
 * @param state Sleep state to query
 * @return Replay factor for that state
 */
float mirror_neurons_sleep_replay_for_state(sleep_state_t state);

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed mirror neuron signals
 * HOW:  Register with bio_router using BIO_MODULE_MIRROR_NEURONS_SLEEP
 *
 * @param bridge Mirror neurons sleep bridge
 * @return 0 on success, -1 on error
 */
int mirror_neurons_sleep_connect_bio_async(mirror_neurons_sleep_bridge_t bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Mirror neurons sleep bridge
 * @return 0 on success, -1 on error
 */
int mirror_neurons_sleep_disconnect_bio_async(mirror_neurons_sleep_bridge_t bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query bio-async connection status
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Mirror neurons sleep bridge
 * @return true if connected, false otherwise
 */
bool mirror_neurons_sleep_is_bio_async_connected(const mirror_neurons_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_NEURONS_SLEEP_BRIDGE_H */
