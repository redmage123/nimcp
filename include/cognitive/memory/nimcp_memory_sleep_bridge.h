/**
 * @file nimcp_memory_sleep_bridge.h
 * @brief Sleep-Systems Consolidation Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and systems consolidation
 * WHY:  Memory consolidation from hippocampus to cortex critically depends on sleep
 * HOW:  Sleep state modulates replay frequency, transfer rate, and consolidation strength
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Minimal replay, slow consolidation, active encoding
 * - DROWSY: Reduced encoding, preparation for consolidation
 * - LIGHT NREM: Hippocampal-cortical dialogue begins, memory sorting/triage
 * - DEEP NREM: Peak consolidation via slow oscillations, sharp-wave ripples drive transfer
 * - REM: Integration and abstraction, episodic → semantic transformation
 *
 * Sleep stages and memory consolidation (McClelland et al., 1995; Born & Wilhelm, 2012):
 * - NREM stages drive declarative memory consolidation (hippocampus → neocortex)
 * - Slow oscillations (0.5-1 Hz) coordinate hippocampal-cortical replay
 * - Sharp-wave ripples (100-200 Hz) in hippocampus trigger cortical plasticity
 * - REM sleep integrates memories into existing semantic networks
 * - Theta oscillations (4-8 Hz) in REM enable abstraction and generalization
 *
 * Sleep deprivation effects on consolidation:
 * - Reduced replay frequency (fewer consolidation opportunities)
 * - Impaired hippocampal-cortical coordination
 * - Slower transfer rate (memories remain hippocampus-dependent)
 * - Reduced semantic extraction (episodic details preserved but not abstracted)
 * - Increased forgetting of unrehearsed memories
 *
 * Sleep benefits systems consolidation:
 * - NREM consolidates hippocampal traces into stable cortical representations
 * - Sleep protects new cortical traces from interference
 * - Repeated replay strengthens cortical synapses
 * - REM enables schema formation and semantic network integration
 * - Sleep deprivation recovery can partially rescue consolidation deficits
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MEMORY_SLEEP_BRIDGE_H
#define NIMCP_MEMORY_SLEEP_BRIDGE_H

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

/* Replay frequency modulation (Hz, relative to baseline) */
#define MEMORY_SLEEP_REPLAY_AWAKE         0.1f   /* Minimal spontaneous replay */
#define MEMORY_SLEEP_REPLAY_DROWSY        0.5f   /* Increased replay as sleep approaches */
#define MEMORY_SLEEP_REPLAY_LIGHT_NREM    5.0f   /* Active hippocampal-cortical dialogue */
#define MEMORY_SLEEP_REPLAY_DEEP_NREM    10.0f   /* Peak replay (sharp-wave ripples) */
#define MEMORY_SLEEP_REPLAY_REM           3.0f   /* Integration/abstraction replay */

/* Transfer rate modulation (hippocampus → cortex, fraction per hour) */
#define MEMORY_SLEEP_TRANSFER_AWAKE       0.001f  /* 0.1% per hour */
#define MEMORY_SLEEP_TRANSFER_DROWSY      0.005f  /* 0.5% per hour */
#define MEMORY_SLEEP_TRANSFER_LIGHT_NREM  0.03f   /* 3% per hour */
#define MEMORY_SLEEP_TRANSFER_DEEP_NREM   0.05f   /* 5% per hour (peak) */
#define MEMORY_SLEEP_TRANSFER_REM         0.02f   /* 2% per hour */

/* Consolidation strength modulation (synaptic stabilization) */
#define MEMORY_SLEEP_CONSOLIDATION_AWAKE       0.1f  /* Minimal during wake */
#define MEMORY_SLEEP_CONSOLIDATION_DROWSY      0.2f
#define MEMORY_SLEEP_CONSOLIDATION_LIGHT_NREM  0.7f  /* Active consolidation */
#define MEMORY_SLEEP_CONSOLIDATION_DEEP_NREM   1.0f  /* Peak consolidation */
#define MEMORY_SLEEP_CONSOLIDATION_REM         0.8f  /* Integration */

/* Semantic extraction strength (episodic → semantic transformation) */
#define MEMORY_SLEEP_SEMANTIC_AWAKE       0.1f
#define MEMORY_SLEEP_SEMANTIC_DROWSY      0.2f
#define MEMORY_SLEEP_SEMANTIC_LIGHT_NREM  0.5f
#define MEMORY_SLEEP_SEMANTIC_DEEP_NREM   0.7f
#define MEMORY_SLEEP_SEMANTIC_REM         1.0f  /* Peak semantic extraction in REM */

/**
 * @struct memory_sleep_config_t
 * @brief Configuration for memory-sleep integration
 *
 * WHAT: Controls which sleep effects are applied to consolidation
 * WHY:  Allow selective modulation for testing and tuning
 * HOW:  Boolean flags enable/disable specific modulation pathways
 */
typedef struct {
    bool enable_replay_modulation;        /* Modulate replay frequency by sleep state */
    bool enable_transfer_modulation;      /* Modulate transfer rate by sleep state */
    bool enable_consolidation_modulation; /* Modulate consolidation strength */
    bool enable_semantic_extraction;      /* Enable semantic extraction in REM */
    float modulation_strength;            /* Overall scaling factor [0.0-1.0] */
} memory_sleep_config_t;

/**
 * @struct memory_sleep_effects_t
 * @brief Current sleep effects on memory consolidation
 *
 * WHAT: Computed modulation factors for consolidation system
 * WHY:  Encapsulate all sleep-dependent parameters in one structure
 * HOW:  Updated by bridge based on current sleep state
 *
 * BIOLOGICAL MAPPING:
 * - replay_frequency_factor: Sharp-wave ripple rate modulation
 * - transfer_rate_factor: Hippocampal-cortical coupling strength
 * - consolidation_strength_factor: Synaptic stabilization rate
 * - semantic_extraction_factor: Abstraction/generalization strength
 * - replay_active: Whether replay is occurring (SWS/REM vs awake)
 * - peak_consolidation: Whether in optimal consolidation window (deep NREM)
 */
typedef struct {
    float replay_frequency_factor;      /* Replay rate multiplier [0.1-10.0] */
    float transfer_rate_factor;         /* Transfer rate multiplier [0.001-0.05] */
    float consolidation_strength_factor;/* Consolidation strength [0.1-1.0] */
    float semantic_extraction_factor;   /* Semantic extraction strength [0.1-1.0] */
    sleep_state_t current_state;        /* Current sleep state */
    float sleep_pressure;               /* Current sleep pressure [0.0-1.0] */
    bool replay_active;                 /* True during NREM/REM */
    bool peak_consolidation;            /* True during deep NREM */
} memory_sleep_effects_t;

/**
 * @typedef memory_sleep_bridge_t
 * @brief Opaque handle to memory-sleep bridge
 *
 * WHAT: Handle to internal bridge structure
 * WHY:  Encapsulation - hide implementation details
 * HOW:  Pimpl idiom - pointer to internal structure
 */
typedef struct memory_sleep_bridge_struct* memory_sleep_bridge_t;

/**
 * @brief Initialize memory-sleep configuration with default values
 *
 * WHAT: Sets up default configuration for memory-sleep integration
 * WHY:  Provides sensible defaults based on biological parameters
 * HOW:  Enables all modulation pathways with full strength
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 on NULL pointer
 *
 * DEFAULTS:
 * - All modulation enabled
 * - Modulation strength = 1.0 (full effect)
 *
 * USAGE:
 * memory_sleep_config_t config;
 * memory_sleep_default_config(&config);
 */
int memory_sleep_default_config(memory_sleep_config_t* config);

/**
 * @brief Create memory-sleep integration bridge
 *
 * WHAT: Initializes bridge between sleep system and memory consolidation
 * WHY:  Enables sleep-dependent modulation of consolidation processes
 * HOW:  Allocates bridge, registers callback, initializes effects
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep Sleep system handle (must be valid)
 * @return Bridge handle or NULL on failure
 *
 * BIOLOGICAL BASIS:
 * - Connects sleep state machine to consolidation system
 * - Registers for sleep state change notifications
 * - Initializes effects based on current sleep state
 *
 * THREAD-SAFE: Yes (internal mutex)
 *
 * USAGE:
 * memory_sleep_bridge_t bridge = memory_sleep_bridge_create(&config, sleep_system);
 * if (!bridge) // handle error
 */
memory_sleep_bridge_t memory_sleep_bridge_create(
    const memory_sleep_config_t* config,
    sleep_system_t sleep);

/**
 * @brief Destroy memory-sleep bridge
 *
 * WHAT: Cleans up bridge and frees resources
 * WHY:  Prevents memory leaks
 * HOW:  Unregisters callback, frees mutex, frees structure
 *
 * @param bridge Bridge to destroy (can be NULL)
 *
 * USAGE:
 * memory_sleep_bridge_destroy(bridge);
 */
void memory_sleep_bridge_destroy(memory_sleep_bridge_t bridge);

/**
 * @brief Update bridge state (poll current sleep state)
 *
 * WHAT: Manually updates effects based on current sleep state
 * WHY:  Allows polling mode if callback not registered
 * HOW:  Queries sleep system, recomputes all effects
 *
 * @param bridge Bridge to update
 * @return 0 on success, -1 on NULL pointer
 *
 * NOTE: Automatic updates via callback are preferred
 * Use this only if callback registration fails
 *
 * USAGE:
 * memory_sleep_update(bridge);
 */
int memory_sleep_update(memory_sleep_bridge_t bridge);

/**
 * @brief Get current sleep effects on memory consolidation
 *
 * WHAT: Retrieves computed modulation factors
 * WHY:  Consolidation system needs current parameters
 * HOW:  Thread-safe copy of effects structure
 *
 * @param bridge Bridge to query (must be valid)
 * @param effects Output structure to fill
 * @return 0 on success, -1 on NULL pointer
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * memory_sleep_effects_t effects;
 * memory_sleep_get_effects(bridge, &effects);
 * float replay_hz = base_replay_hz * effects.replay_frequency_factor;
 */
int memory_sleep_get_effects(
    const memory_sleep_bridge_t bridge,
    memory_sleep_effects_t* effects);

/**
 * @brief Get current replay frequency factor
 *
 * WHAT: Returns current replay frequency multiplier
 * WHY:  Convenience function for most common query
 * HOW:  Thread-safe read of replay_frequency_factor
 *
 * @param bridge Bridge to query (must be valid)
 * @return Replay frequency factor [0.1-10.0], 1.0 on error
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * float factor = memory_sleep_get_replay_frequency(bridge);
 */
float memory_sleep_get_replay_frequency(const memory_sleep_bridge_t bridge);

/**
 * @brief Check if replay is currently active
 *
 * WHAT: Returns whether memory replay should occur
 * WHY:  Consolidation system needs to know when to trigger replay
 * HOW:  True during NREM and REM sleep
 *
 * @param bridge Bridge to query (must be valid)
 * @return true if replay active, false otherwise
 *
 * BIOLOGICAL BASIS:
 * - Replay occurs during sleep (all NREM and REM stages)
 * - Minimal replay during drowsiness
 * - Rare spontaneous replay when awake
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * if (memory_sleep_is_replay_active(bridge)) {
 *     // Execute memory replay
 * }
 */
bool memory_sleep_is_replay_active(const memory_sleep_bridge_t bridge);

/**
 * @brief Get replay frequency for specific sleep state
 *
 * WHAT: Returns replay frequency factor for given state
 * WHY:  Useful for testing and state-specific logic
 * HOW:  Lookup table mapping state to frequency
 *
 * @param state Sleep state to query
 * @return Replay frequency factor for that state
 *
 * STATELESS: Pure function (no side effects)
 *
 * USAGE:
 * float deep_nrem_replay = memory_sleep_replay_for_state(SLEEP_STATE_DEEP_NREM);
 */
float memory_sleep_replay_for_state(sleep_state_t state);

/**
 * @brief Get transfer rate for specific sleep state
 *
 * WHAT: Returns transfer rate factor for given state
 * WHY:  Useful for testing and state-specific logic
 * HOW:  Lookup table mapping state to transfer rate
 *
 * @param state Sleep state to query
 * @return Transfer rate factor for that state
 *
 * STATELESS: Pure function (no side effects)
 *
 * USAGE:
 * float deep_nrem_transfer = memory_sleep_transfer_for_state(SLEEP_STATE_DEEP_NREM);
 */
float memory_sleep_transfer_for_state(sleep_state_t state);

/**
 * @brief Get consolidation strength for specific sleep state
 *
 * WHAT: Returns consolidation strength factor for given state
 * WHY:  Useful for testing and state-specific logic
 * HOW:  Lookup table mapping state to consolidation strength
 *
 * @param state Sleep state to query
 * @return Consolidation strength for that state [0.1-1.0]
 *
 * STATELESS: Pure function (no side effects)
 *
 * USAGE:
 * float deep_nrem_consolidation = memory_sleep_consolidation_for_state(SLEEP_STATE_DEEP_NREM);
 */
float memory_sleep_consolidation_for_state(sleep_state_t state);

/**
 * @brief Get semantic extraction strength for specific sleep state
 *
 * WHAT: Returns semantic extraction factor for given state
 * WHY:  Useful for testing and state-specific logic
 * HOW:  Lookup table mapping state to semantic extraction
 *
 * @param state Sleep state to query
 * @return Semantic extraction strength for that state [0.1-1.0]
 *
 * BIOLOGICAL BASIS:
 * - REM sleep is optimal for semantic extraction/abstraction
 * - Deep NREM enables consolidation but less abstraction
 * - Awake state has minimal semantic transformation
 *
 * STATELESS: Pure function (no side effects)
 *
 * USAGE:
 * float rem_semantic = memory_sleep_semantic_for_state(SLEEP_STATE_REM);
 */
float memory_sleep_semantic_for_state(sleep_state_t state);

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed memory signals
 * HOW:  Register with bio_router using BIO_MODULE_MEMORY_SLEEP
 *
 * @param bridge Memory-sleep bridge
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * memory_sleep_connect_bio_async(bridge);
 */
int memory_sleep_connect_bio_async(memory_sleep_bridge_t bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Memory-sleep bridge
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * memory_sleep_disconnect_bio_async(bridge);
 */
int memory_sleep_disconnect_bio_async(memory_sleep_bridge_t bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Allow conditional bio-async usage
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Memory-sleep bridge
 * @return true if connected, false otherwise
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * if (memory_sleep_is_bio_async_connected(bridge)) {
 *     // Use bio-async features
 * }
 */
bool memory_sleep_is_bio_async_connected(const memory_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_SLEEP_BRIDGE_H */
