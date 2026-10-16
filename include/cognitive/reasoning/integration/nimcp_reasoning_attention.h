/**
 * @file nimcp_reasoning_attention.h
 * @brief MODULE 1: Reasoning-Attention Integration
 * @version 1.0.0
 * @date 2025-11-20
 *
 * SOLE RESPONSIBILITY: Focus attention on novel logical facts
 *
 * WHAT: Integration module connecting reasoning events to attention mechanism
 * WHY:  Novel facts, contradictions, and successful proofs deserve attentional focus
 * HOW:  Subscribe to reasoning events, compute salience scores, boost attention
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex tracks logical novelty and allocates attention
 * - Anterior cingulate cortex detects contradictions (error detection)
 * - Dopamine release on successful inference (reward prediction)
 *
 * MODULARIZATION:
 * - Single Responsibility: ONLY handles attention modulation for reasoning
 * - Dependencies: Event bus, attention mechanism
 * - Integration: Event-driven, no direct coupling to reasoning engine
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_REASONING_ATTENTION_H
#define NIMCP_REASONING_ATTENTION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct fault_attention fault_attention_t;
typedef struct reasoning_attention reasoning_attention_t;

//=============================================================================
// Constants
//=============================================================================

#define REASONING_ATTENTION_NOVEL_FACT_SALIENCE    0.8f  /**< Salience for novel facts */
#define REASONING_ATTENTION_CONTRADICTION_SALIENCE 1.0f  /**< Max salience for contradictions */
#define REASONING_ATTENTION_PROOF_FOUND_SALIENCE   0.6f  /**< Salience for successful proofs */
#define REASONING_ATTENTION_DECAY_TAU_MS           5000  /**< Attention decay time constant */

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Reasoning-attention integration configuration
 */
typedef struct {
    bool enable_novel_fact_boost;       /**< Boost attention on novel facts */
    bool enable_contradiction_boost;    /**< Boost attention on contradictions */
    bool enable_proof_found_boost;      /**< Boost attention on proof success */

    float novel_fact_salience;          /**< Salience weight for novel facts [0.0, 1.0] */
    float contradiction_salience;       /**< Salience weight for contradictions [0.0, 1.0] */
    float proof_found_salience;         /**< Salience weight for proofs [0.0, 1.0] */

    uint32_t attention_decay_tau_ms;    /**< Decay time constant (ms) */
    float min_salience_threshold;       /**< Minimum salience to apply [0.0, 1.0] */
} reasoning_attention_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Reasoning-attention integration statistics
 */
typedef struct {
    uint64_t total_events_processed;    /**< Total reasoning events handled */
    uint64_t novel_fact_boosts;         /**< Novel fact attention boosts */
    uint64_t contradiction_boosts;      /**< Contradiction attention boosts */
    uint64_t proof_found_boosts;        /**< Proof success attention boosts */
    float avg_salience_applied;         /**< Average salience boost applied */
    float max_salience_applied;         /**< Maximum salience boost applied */
    uint64_t avg_callback_time_us;      /**< Average callback execution time */
} reasoning_attention_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create reasoning-attention integration with default configuration
 *
 * WHAT: Initialize attention integration for reasoning events
 * WHY:  Enable attention modulation based on logical discoveries
 * HOW:  Subscribe to reasoning events, allocate tracking structures
 *
 * @param event_bus Event bus to subscribe to reasoning events (non-NULL)
 * @param attention Attention mechanism to modulate (non-NULL)
 * @return Integration handle, NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~512 bytes
 */
reasoning_attention_t* reasoning_attention_create(
    event_bus_t event_bus,
    fault_attention_t* attention
);

/**
 * @brief Create reasoning-attention integration with custom configuration
 *
 * @param event_bus Event bus to subscribe to reasoning events (non-NULL)
 * @param attention Attention mechanism to modulate (non-NULL)
 * @param config Custom configuration (NULL for defaults)
 * @return Integration handle, NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~512 bytes
 */
reasoning_attention_t* reasoning_attention_create_custom(
    event_bus_t event_bus,
    fault_attention_t* attention,
    const reasoning_attention_config_t* config
);

/**
 * @brief Destroy reasoning-attention integration
 *
 * WHAT: Unsubscribe from events and free resources
 * WHY:  Clean shutdown and prevent memory leaks
 * HOW:  Unsubscribe from event bus, free tracking structures
 *
 * @param integration Integration handle (NULL-safe)
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees ~512 bytes
 */
void reasoning_attention_destroy(reasoning_attention_t* integration);

//=============================================================================
// Core Functions
//=============================================================================

/**
 * @brief Compute salience score for a logical fact
 *
 * WHAT: Calculate attention weight for a fact based on novelty and importance
 * WHY:  Quantify how much attention this fact deserves
 * HOW:  Factor in novelty, contradiction potential, and proof utility
 *
 * ALGORITHM:
 * salience = base_salience +
 *            novelty_factor * (1.0 - familiarity) +
 *            importance_factor * proof_utility
 *
 * @param integration Integration handle (non-NULL)
 * @param fact_description Fact description string (non-NULL)
 * @param is_novel Whether fact is newly derived
 * @param is_contradiction Whether fact contradicts existing knowledge
 * @return Salience score [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <5μs
 */
float reasoning_attention_compute_fact_salience(
    reasoning_attention_t* integration,
    const char* fact_description,
    bool is_novel,
    bool is_contradiction
);

/**
 * @brief Event callback for reasoning events
 *
 * WHAT: Handle reasoning events and modulate attention
 * WHY:  React to logical discoveries in real-time
 * HOW:  Compute salience, apply attention boost
 *
 * HANDLES:
 * - EVENT_NOVEL_FACT_DERIVED: Boost attention to novel facts
 * - EVENT_CONTRADICTION_DETECTED: Max attention to contradictions
 * - EVENT_PROOF_FOUND: Moderate attention to successful proofs
 *
 * @param event Reasoning event (non-NULL)
 * @param context Reasoning-attention integration handle
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <10μs
 */
void reasoning_attention_callback(const brain_event_t* event, void* context);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current configuration
 *
 * @param integration Integration handle (non-NULL)
 * @param config Output configuration (non-NULL)
 * @return true on success, false on NULL parameters
 */
bool reasoning_attention_get_config(
    const reasoning_attention_t* integration,
    reasoning_attention_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param integration Integration handle (non-NULL)
 * @param config New configuration (non-NULL, validated)
 * @return true on success, false if invalid
 */
bool reasoning_attention_set_config(
    reasoning_attention_t* integration,
    const reasoning_attention_config_t* config
);

/**
 * @brief Get integration statistics
 *
 * @param integration Integration handle (non-NULL)
 * @param stats Output statistics (non-NULL)
 * @return true on success, false on NULL parameters
 */
bool reasoning_attention_get_stats(
    const reasoning_attention_t* integration,
    reasoning_attention_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param integration Integration handle (non-NULL)
 * @return true on success, false on NULL parameter
 */
bool reasoning_attention_reset_stats(reasoning_attention_t* integration);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration structure
 */
reasoning_attention_config_t reasoning_attention_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate (non-NULL)
 * @return true if valid, false otherwise
 */
bool reasoning_attention_validate_config(const reasoning_attention_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_ATTENTION_H
