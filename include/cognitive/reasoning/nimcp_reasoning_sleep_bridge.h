/**
 * @file nimcp_reasoning_sleep_bridge.h
 * @brief Sleep-Reasoning Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and reasoning
 * WHY:  Reasoning capacity and style fundamentally depend on sleep state
 * HOW:  Sleep state modulates logical/creative reasoning, working memory access, inference speed
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full reasoning capacity, balanced logical and creative reasoning
 * - DROWSY: Reduced working memory, impaired logical reasoning, slower inference
 * - NREM (Light/Deep): Offline reasoning, memory consolidation, pattern extraction
 * - REM: Enhanced creative/associative reasoning, reduced logical reasoning, dream logic
 *
 * Sleep effects on reasoning:
 * - REM enhances creative problem-solving and insight (Ah-ha! moments after sleep)
 * - Deep NREM consolidates declarative knowledge and strengthens logical schemas
 * - Sleep deprivation impairs deductive reasoning and working memory integration
 * - REM supports associative/analogical reasoning through loose semantic activation
 * - NREM supports rule-based reasoning through memory consolidation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_REASONING_SLEEP_BRIDGE_H
#define NIMCP_REASONING_SLEEP_BRIDGE_H

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

/* Constants - Logical Reasoning Modulation */
#define REASONING_SLEEP_LOGICAL_AWAKE       1.0f
#define REASONING_SLEEP_LOGICAL_DROWSY      0.7f   /* Impaired working memory */
#define REASONING_SLEEP_LOGICAL_LIGHT_NREM  0.3f   /* Offline */
#define REASONING_SLEEP_LOGICAL_DEEP_NREM   0.1f   /* Minimal */
#define REASONING_SLEEP_LOGICAL_REM         0.4f   /* Dream logic, not formal */

/* Constants - Creative/Associative Reasoning Modulation */
#define REASONING_SLEEP_CREATIVE_AWAKE       1.0f
#define REASONING_SLEEP_CREATIVE_DROWSY      1.1f   /* Slightly enhanced, relaxed constraints */
#define REASONING_SLEEP_CREATIVE_LIGHT_NREM  0.8f   /* Background processing */
#define REASONING_SLEEP_CREATIVE_DEEP_NREM   0.5f   /* Pattern extraction */
#define REASONING_SLEEP_CREATIVE_REM         1.5f   /* Enhanced associative reasoning */

/* Constants - Inference Speed Modulation */
#define REASONING_SLEEP_SPEED_AWAKE       1.0f
#define REASONING_SLEEP_SPEED_DROWSY      0.6f   /* Slower processing */
#define REASONING_SLEEP_SPEED_LIGHT_NREM  0.2f   /* Offline, slow consolidation */
#define REASONING_SLEEP_SPEED_DEEP_NREM   0.1f   /* Minimal activity */
#define REASONING_SLEEP_SPEED_REM         0.8f   /* Active but dream-like */

/**
 * WHAT: Configuration for reasoning-sleep integration
 * WHY:  Control which aspects of reasoning are modulated by sleep
 * HOW:  Enable/disable specific modulation effects
 */
typedef struct {
    bool enable_logical_modulation;      /**< Modulate logical/deductive reasoning */
    bool enable_creative_modulation;     /**< Modulate creative/associative reasoning */
    bool enable_speed_modulation;        /**< Modulate inference speed */
    bool enable_working_memory_gating;   /**< Gate working memory access during sleep */
    float modulation_strength;           /**< Overall modulation strength [0-1] */
} reasoning_sleep_config_t;

/**
 * WHAT: Sleep effects on reasoning systems
 * WHY:  Quantify how sleep state affects different reasoning modes
 * HOW:  Factor values applied to reasoning parameters
 */
typedef struct {
    float logical_reasoning_factor;      /**< Logical/deductive reasoning capacity [0-1] */
    float creative_reasoning_factor;     /**< Creative/associative reasoning capacity [0-2] */
    float inference_speed_factor;        /**< Inference speed multiplier [0-1] */
    float working_memory_access_factor;  /**< Access to working memory [0-1] */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure [0-1] */
    bool reasoning_offline;              /**< Reasoning is offline (NREM) */
    bool rem_creativity_boost;           /**< REM-induced creativity enhancement */
} reasoning_sleep_effects_t;

/**
 * WHAT: Opaque handle to reasoning-sleep bridge
 * WHY:  Encapsulation - hide implementation details
 * HOW:  Forward declaration, pointer to struct
 */
typedef struct reasoning_sleep_bridge_struct* reasoning_sleep_bridge_t;

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default configuration for reasoning-sleep bridge
 * WHY:  Provide sensible defaults for typical use cases
 * HOW:  Return pre-configured struct with all modulations enabled
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int reasoning_sleep_default_config(reasoning_sleep_config_t* config);

/**
 * WHAT: Create reasoning-sleep bridge
 * WHY:  Initialize integration between reasoning and sleep systems
 * HOW:  Allocate structure, register callback, get initial state
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep Sleep system to integrate with
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
reasoning_sleep_bridge_t reasoning_sleep_bridge_create(
    const reasoning_sleep_config_t* config,
    sleep_system_t sleep);

/**
 * WHAT: Destroy reasoning-sleep bridge
 * WHY:  Free resources and unregister callbacks
 * HOW:  Unregister callback, free allocations
 *
 * @param bridge Bridge to destroy
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void reasoning_sleep_bridge_destroy(reasoning_sleep_bridge_t bridge);

/* ========================================================================
 * UPDATE AND QUERY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Update reasoning modulation based on current sleep state
 * WHY:  Synchronize reasoning parameters with sleep state
 * HOW:  Query sleep system, compute effects, update factors
 *
 * NOTE: This is called automatically via callback, but can be called
 *       manually for polling-based updates
 *
 * @param bridge Reasoning-sleep bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int reasoning_sleep_update(reasoning_sleep_bridge_t bridge);

/**
 * WHAT: Get current sleep effects on reasoning
 * WHY:  Allow reasoning module to query modulation factors
 * HOW:  Copy effects structure with mutex protection
 *
 * @param bridge Reasoning-sleep bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int reasoning_sleep_get_effects(const reasoning_sleep_bridge_t bridge,
                                 reasoning_sleep_effects_t* effects);

/**
 * WHAT: Get logical reasoning factor
 * WHY:  Quick query for logical reasoning capacity
 * HOW:  Return current logical_reasoning_factor
 *
 * @param bridge Reasoning-sleep bridge
 * @return Logical reasoning factor [0-1], 1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float reasoning_sleep_get_logical_factor(const reasoning_sleep_bridge_t bridge);

/**
 * WHAT: Get creative reasoning factor
 * WHY:  Quick query for creative reasoning capacity
 * HOW:  Return current creative_reasoning_factor
 *
 * @param bridge Reasoning-sleep bridge
 * @return Creative reasoning factor [0-2], 1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float reasoning_sleep_get_creative_factor(const reasoning_sleep_bridge_t bridge);

/**
 * WHAT: Check if reasoning is offline
 * WHY:  Determine if reasoning should halt (NREM sleep)
 * HOW:  Return offline flag from effects
 *
 * @param bridge Reasoning-sleep bridge
 * @return true if reasoning is offline, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool reasoning_sleep_is_offline(const reasoning_sleep_bridge_t bridge);

/**
 * WHAT: Check if REM creativity boost is active
 * WHY:  Determine if creative reasoning should be enhanced
 * HOW:  Return rem_creativity_boost flag
 *
 * @param bridge Reasoning-sleep bridge
 * @return true if REM creativity boost active, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool reasoning_sleep_is_rem_creative(const reasoning_sleep_bridge_t bridge);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get logical reasoning factor for a given sleep state
 * WHY:  Utility for testing or state-specific queries
 * HOW:  Map state to predefined factor
 *
 * @param state Sleep state to query
 * @return Logical reasoning factor for state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float reasoning_sleep_logical_for_state(sleep_state_t state);

/**
 * WHAT: Get creative reasoning factor for a given sleep state
 * WHY:  Utility for testing or state-specific queries
 * HOW:  Map state to predefined factor
 *
 * @param state Sleep state to query
 * @return Creative reasoning factor for state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float reasoning_sleep_creative_for_state(sleep_state_t state);

/**
 * WHAT: Get inference speed factor for a given sleep state
 * WHY:  Utility for testing or state-specific queries
 * HOW:  Map state to predefined factor
 *
 * @param state Sleep state to query
 * @return Inference speed factor for state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float reasoning_sleep_speed_for_state(sleep_state_t state);

/* ========================================================================
 * BIO-ASYNC INTEGRATION API
 * ======================================================================== */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed reasoning signals
 * HOW:  Register with bio_router using BIO_MODULE_REASONING_SLEEP
 *
 * @param bridge Reasoning-sleep bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int reasoning_sleep_connect_bio_async(reasoning_sleep_bridge_t bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Reasoning-sleep bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int reasoning_sleep_disconnect_bio_async(reasoning_sleep_bridge_t bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query bio-async connection status
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Reasoning-sleep bridge
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool reasoning_sleep_is_bio_async_connected(const reasoning_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_SLEEP_BRIDGE_H */
