/**
 * @file nimcp_logic_sleep_bridge.h
 * @brief Sleep-Logic Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and logical reasoning
 * WHY:  Logical reasoning is severely impaired during sleep deprivation
 * HOW:  Sleep state modulates inference capacity, deduction accuracy, logical consistency
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP STAGE EFFECTS ON LOGICAL REASONING:
 * -----------------------------------------
 * 1. AWAKE State:
 *    - Full logical reasoning capacity
 *    - Deductive inference operational
 *    - Syllogistic reasoning intact
 *    - Reference: Killgore et al. (2006) "Sleep deprivation reduces perceived emotional intelligence"
 *
 * 2. DROWSY State:
 *    - Reduced logical consistency (increased contradictions)
 *    - Slower inference speed
 *    - Impaired working memory for logical premises
 *    - Reference: Harrison & Horne (2000) "Sleep loss and temporal memory"
 *
 * 3. NREM Sleep (Light & Deep):
 *    - Logical reasoning essentially offline
 *    - Procedural logic consolidation (if-then rules, schemas)
 *    - Strengthening of logical associations
 *    - Reference: Wagner et al. (2004) "Sleep inspires insight"
 *
 * 4. REM Sleep:
 *    - Reduced logical constraints (dream bizarreness)
 *    - Creative recombination of logical elements
 *    - Associative rather than deductive reasoning
 *    - Reference: Walker et al. (2002) "Cognitive flexibility across sleep-wake cycle"
 *
 * SLEEP DEPRIVATION EFFECTS:
 * -------------------------
 * - Impaired deductive reasoning (syllogism errors)
 * - Reduced ability to detect logical inconsistencies
 * - Slower inference processing
 * - Difficulty maintaining logical premises in working memory
 * - Increased reliance on heuristics vs. logical analysis
 * - Reference: Durmer & Dinges (2005) "Neurocognitive consequences of sleep deprivation"
 *
 * SLEEP-DEPENDENT CONSOLIDATION:
 * -----------------------------
 * - NREM sleep consolidates procedural logical rules
 * - Deep sleep strengthens if-then associations
 * - REM sleep integrates logical schemas with episodic memory
 * - Sleep-dependent insight (aha moments after sleep)
 * - Reference: Stickgold & Walker (2013) "Sleep-dependent memory triage"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        LOGIC-SLEEP BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SLEEP → LOGIC PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  AWAKE       │ ──→ Inference Capacity: 100%                    │  ║
 * ║   │   │              │ ──→ Deduction Accuracy: 100%                    │  ║
 * ║   │   │              │ ──→ Consistency Check: Enabled                  │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  DROWSY      │ ──→ Inference Capacity: 60%                     │  ║
 * ║   │   │              │ ──→ Deduction Accuracy: 70%                     │  ║
 * ║   │   │              │ ──→ Consistency Check: Degraded                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  NREM        │ ──→ Logic OFFLINE (Consolidation Mode)          │  ║
 * ║   │   │              │ ──→ Procedural Rule Strengthening               │  ║
 * ║   │   │              │ ──→ If-Then Schema Integration                  │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  REM         │ ──→ Associative Logic (Creative Mode)           │  ║
 * ║   │   │              │ ──→ Reduced Constraints                         │  ║
 * ║   │   │              │ ──→ Schema Recombination                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LOGIC_SLEEP_BRIDGE_H
#define NIMCP_LOGIC_SLEEP_BRIDGE_H

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

/* ============================================================================
 * Constants - Sleep State Performance Modulation
 * ============================================================================ */

/* Inference capacity modulation (ability to perform deductive reasoning) */
#define LOGIC_SLEEP_INFERENCE_AWAKE       1.0f   /**< Full inference capacity */
#define LOGIC_SLEEP_INFERENCE_DROWSY      0.6f   /**< Reduced inference speed/depth */
#define LOGIC_SLEEP_INFERENCE_LIGHT_NREM  0.1f   /**< Minimal inference (consolidation mode) */
#define LOGIC_SLEEP_INFERENCE_DEEP_NREM   0.0f   /**< Logic offline (deep consolidation) */
#define LOGIC_SLEEP_INFERENCE_REM         0.3f   /**< Associative rather than deductive */

/* Deduction accuracy modulation (correctness of logical inferences) */
#define LOGIC_SLEEP_ACCURACY_AWAKE        1.0f   /**< Full accuracy */
#define LOGIC_SLEEP_ACCURACY_DROWSY       0.7f   /**< Increased errors */
#define LOGIC_SLEEP_ACCURACY_LIGHT_NREM   0.2f   /**< Highly error-prone */
#define LOGIC_SLEEP_ACCURACY_DEEP_NREM    0.0f   /**< Not operational */
#define LOGIC_SLEEP_ACCURACY_REM          0.4f   /**< Creative but logically inconsistent */

/* Logical consistency checking (ability to detect contradictions) */
#define LOGIC_SLEEP_CONSISTENCY_AWAKE     1.0f   /**< Full consistency checking */
#define LOGIC_SLEEP_CONSISTENCY_DROWSY    0.5f   /**< Reduced contradiction detection */
#define LOGIC_SLEEP_CONSISTENCY_NREM      0.0f   /**< Consistency checking offline */
#define LOGIC_SLEEP_CONSISTENCY_REM       0.2f   /**< Minimal consistency (dream logic) */

/* Procedural rule consolidation (NREM-specific) */
#define LOGIC_SLEEP_CONSOLIDATION_NREM    1.0f   /**< Active rule strengthening */
#define LOGIC_SLEEP_CONSOLIDATION_DEEP    1.5f   /**< Enhanced consolidation in deep sleep */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Logic sleep bridge configuration
 *
 * Controls which sleep-logic integration features are enabled
 */
typedef struct {
    /* Feature enables */
    bool enable_inference_modulation;      /**< Modulate inference capacity by sleep state */
    bool enable_accuracy_modulation;       /**< Modulate deduction accuracy by sleep state */
    bool enable_consistency_modulation;    /**< Modulate consistency checking by sleep state */
    bool enable_nrem_consolidation;        /**< Enable procedural rule consolidation during NREM */
    bool enable_rem_creativity;            /**< Enable associative logic during REM */

    /* Sensitivity tuning */
    float modulation_strength;             /**< Global modulation multiplier [0.5-2.0] */

    /* Sleep pressure sensitivity */
    float sleep_pressure_threshold;        /**< Pressure level that starts degrading logic [0.5-0.9] */
} logic_sleep_config_t;

/**
 * @brief Current sleep effects on logical reasoning
 *
 * Computed effects based on current sleep state and pressure
 */
typedef struct {
    /* Performance modulation factors */
    float inference_capacity_factor;       /**< Multiplier for inference speed/depth [0-1] */
    float deduction_accuracy_factor;       /**< Multiplier for deduction correctness [0-1] */
    float consistency_check_factor;        /**< Multiplier for contradiction detection [0-1] */

    /* State information */
    sleep_state_t current_state;           /**< Current sleep state */
    float sleep_pressure;                  /**< Current sleep pressure [0-1] */

    /* Functional status */
    bool logic_offline;                    /**< True if logic reasoning is offline */
    bool consolidation_mode;               /**< True if in NREM consolidation mode */
    bool creative_mode;                    /**< True if in REM associative mode */

    /* Consolidation metrics (NREM) */
    float rule_consolidation_rate;         /**< Rate of procedural rule strengthening */

    /* Sleep pressure effects (when awake) */
    float pressure_inference_penalty;      /**< Inference reduction from sleep pressure */
    float pressure_accuracy_penalty;       /**< Accuracy reduction from sleep pressure */
} logic_sleep_effects_t;

/**
 * @brief Opaque handle to logic-sleep bridge
 *
 * Internal structure is implementation detail
 */
typedef struct logic_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    logic_sleep_config_t config;           /**< Bridge configuration */
    sleep_system_t sleep_system;           /**< Sleep/wake system handle */
    logic_sleep_effects_t effects;         /**< Current computed effects */

    bool initialized;                      /**< Initialization flag */
    bool callback_registered;              /**< Track if callback is registered for cleanup */
} logic_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration for logic-sleep integration
 * WHY:  Easy initialization with biologically-inspired defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int logic_sleep_default_config(logic_sleep_config_t* config);

/**
 * @brief Create logic-sleep bridge
 *
 * WHAT: Initialize bidirectional logic-sleep integration
 * WHY:  Enable realistic sleep-dependent logical reasoning modulation
 * HOW:  Allocate structure, link sleep system, register callback
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep/wake system handle
 * @return New bridge or NULL on failure
 */
logic_sleep_bridge_t* logic_sleep_bridge_create(
    const logic_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * @brief Destroy logic-sleep bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callback, free structure
 *
 * @param bridge Bridge to destroy
 */
void logic_sleep_bridge_destroy(logic_sleep_bridge_t* bridge);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update logic-sleep bridge state
 *
 * WHAT: Recompute sleep effects on logical reasoning
 * WHY:  Keep effects synchronized with current sleep state
 * HOW:  Query sleep system, compute modulation factors
 *
 * @param bridge Logic-sleep bridge
 * @return 0 on success, -1 on error
 */
int logic_sleep_update(logic_sleep_bridge_t* bridge);

/**
 * @brief Get current sleep effects on logic
 *
 * WHAT: Retrieve computed effects structure
 * WHY:  Allow logic module to adjust its behavior
 * HOW:  Thread-safe copy of effects
 *
 * @param bridge Logic-sleep bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int logic_sleep_get_effects(
    const logic_sleep_bridge_t* bridge,
    logic_sleep_effects_t* effects
);

/**
 * @brief Get inference capacity factor
 *
 * WHAT: Get current inference capacity multiplier
 * WHY:  Quick access to most critical logic parameter
 * HOW:  Thread-safe read of inference_capacity_factor
 *
 * @param bridge Logic-sleep bridge
 * @return Inference capacity factor [0-1]
 */
float logic_sleep_get_inference_capacity(logic_sleep_bridge_t* bridge);

/**
 * @brief Check if logic is offline
 *
 * WHAT: Determine if logical reasoning is functionally offline
 * WHY:  Allow logic module to skip processing during sleep
 * HOW:  Returns true for deep NREM states
 *
 * @param bridge Logic-sleep bridge
 * @return true if logic is offline (deep sleep)
 */
bool logic_sleep_is_offline(logic_sleep_bridge_t* bridge);

/**
 * @brief Check if in consolidation mode
 *
 * WHAT: Determine if in NREM consolidation mode
 * WHY:  Signal to logic system to consolidate procedural rules
 * HOW:  Returns true for NREM states
 *
 * @param bridge Logic-sleep bridge
 * @return true if in NREM consolidation mode
 */
bool logic_sleep_is_consolidation_mode(logic_sleep_bridge_t* bridge);

/* ============================================================================
 * Sleep State Mapping Functions
 * ============================================================================ */

/**
 * @brief Get inference capacity for sleep state
 *
 * WHAT: Map sleep state to inference capacity factor
 * WHY:  Encapsulate state-to-performance mapping
 * HOW:  Switch on sleep state, return constant
 *
 * @param state Sleep state
 * @return Inference capacity factor [0-1]
 */
float logic_sleep_inference_for_state(sleep_state_t state);

/**
 * @brief Get deduction accuracy for sleep state
 *
 * WHAT: Map sleep state to deduction accuracy factor
 * WHY:  Encapsulate state-to-accuracy mapping
 * HOW:  Switch on sleep state, return constant
 *
 * @param state Sleep state
 * @return Deduction accuracy factor [0-1]
 */
float logic_sleep_accuracy_for_state(sleep_state_t state);

/**
 * @brief Get consistency checking for sleep state
 *
 * WHAT: Map sleep state to consistency check factor
 * WHY:  Encapsulate state-to-consistency mapping
 * HOW:  Switch on sleep state, return constant
 *
 * @param state Sleep state
 * @return Consistency check factor [0-1]
 */
float logic_sleep_consistency_for_state(sleep_state_t state);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed logic signals
 * HOW:  Register with bio_router using BIO_MODULE_KNOWLEDGE_SYMBOLIC_LOGIC
 *
 * @param bridge Logic-sleep bridge
 * @return 0 on success, -1 on error
 */
int logic_sleep_connect_bio_async(logic_sleep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Logic-sleep bridge
 * @return 0 on success, -1 on error
 */
int logic_sleep_disconnect_bio_async(logic_sleep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Allow conditional bio-async usage
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Logic-sleep bridge
 * @return true if connected
 */
bool logic_sleep_is_bio_async_connected(const logic_sleep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOGIC_SLEEP_BRIDGE_H */
