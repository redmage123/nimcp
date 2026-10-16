//=============================================================================
// nimcp_swarm_emergence.h - NIMCP Swarm Emergence System
//=============================================================================
/**
 * @file nimcp_swarm_emergence.h
 * @brief Tier-based cognitive capability unlocks for swarm systems
 *
 * WHAT: Emergence system that unlocks cognitive capabilities based on swarm size
 * WHY:  Enable collective intelligence that scales with network size
 * HOW:  Monitor connected drones, calculate coherence, unlock tier-specific abilities
 *
 * ARCHITECTURE:
 *
 *   Swarm Size         Tier              Unlocked Capabilities
 *   ┌──────────┐       ┌──────────┐      ┌─────────────────────────┐
 *   │ N = 1    │  -->  │INDIVIDUAL│  --> │ Local reactive behavior │
 *   └──────────┘       └──────────┘      └─────────────────────────┘
 *   ┌──────────┐       ┌──────────┐      ┌─────────────────────────┐
 *   │ N = 2-3  │  -->  │  PAIR    │  --> │ Stereo sensing          │
 *   └──────────┘       └──────────┘      │ Confirmation voting     │
 *                                         └─────────────────────────┘
 *   ┌──────────┐       ┌──────────┐      ┌─────────────────────────┐
 *   │ N = 4-7  │  -->  │  SQUAD   │  --> │ Distributed memory      │
 *   └──────────┘       └──────────┘      │ Formation control       │
 *                                         └─────────────────────────┘
 *   ┌──────────┐       ┌──────────┐      ┌─────────────────────────┐
 *   │ N = 8-15 │  -->  │ PLATOON  │  --> │ Collective attention    │
 *   └──────────┘       └──────────┘      │ Multi-step planning     │
 *                                         └─────────────────────────┘
 *   ┌──────────┐       ┌──────────┐      ┌─────────────────────────┐
 *   │ N = 16-31│  -->  │ COMPANY  │  --> │ Emergent reasoning      │
 *   └──────────┘       └──────────┘      │ Prediction              │
 *                                         └─────────────────────────┘
 *   ┌──────────┐       ┌──────────┐      ┌─────────────────────────┐
 *   │ N >= 32  │  -->  │BATTALION │  --> │ Meta-cognition          │
 *   └──────────┘       └──────────┘      │ Swarm self-model        │
 *                                         └─────────────────────────┘
 *
 * TIER TRANSITION RULES:
 * - Hysteresis prevents tier flapping during size fluctuations
 * - Coherence threshold must be met for tier advancement
 * - Different thresholds for upward vs downward transitions
 * - Stability counter requires N consecutive readings before change
 *
 * FEATURES:
 * - Real-time tier calculation based on swarm state
 * - Capability flags for easy feature checking
 * - Coherence-based quality gating
 * - Hysteresis for stability
 * - Timestamp tracking for tier changes
 * - Query API for capability availability
 *
 * THREAD SAFETY:
 * - All functions are thread-safe
 * - Context uses internal mutex for synchronization
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_EMERGENCE_H
#define NIMCP_SWARM_EMERGENCE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/** @brief Magic value for validation */
#define NIMCP_SWARM_EMERGENCE_MAGIC 0x5357524D  // 'SWRM'

/** @brief Default minimum coherence for tier advancement */
#define NIMCP_SWARM_DEFAULT_COHERENCE_THRESHOLD 0.7f

/** @brief Default stability count before tier change */
#define NIMCP_SWARM_DEFAULT_STABILITY_COUNT 5

/** @brief Default hysteresis margin (drones) */
#define NIMCP_SWARM_DEFAULT_HYSTERESIS 1

/** @brief Minimum healthy drone ratio for tier advancement */
#define NIMCP_SWARM_MIN_HEALTH_RATIO 0.75f

//=============================================================================
// Emergence Tier Enumeration
//=============================================================================

/**
 * @brief Swarm emergence tiers
 *
 * Each tier represents a discrete level of collective intelligence
 * unlocked by achieving sufficient swarm size and coherence.
 */
typedef enum {
    SWARM_TIER_INDIVIDUAL = 0,    /**< N=1: Local reactive behavior only */
    SWARM_TIER_PAIR = 1,          /**< N=2-3: Cooperative sensing */
    SWARM_TIER_SQUAD = 2,         /**< N=4-7: Distributed working memory */
    SWARM_TIER_PLATOON = 3,       /**< N=8-15: Meta-attention, planning */
    SWARM_TIER_COMPANY = 4,       /**< N=16-31: Emergent reasoning */
    SWARM_TIER_BATTALION = 5,     /**< N>=32: Full meta-cognition */
    SWARM_TIER_COUNT              /**< Number of tiers */
} swarm_emergence_tier_t;

//=============================================================================
// Capability Flags
//=============================================================================

/**
 * @brief Swarm capabilities unlocked per tier
 *
 * Each tier unlocks a cumulative set of capabilities.
 * Higher tiers include all capabilities of lower tiers.
 */
typedef struct {
    // PAIR tier (N >= 2)
    bool stereo_sensing;           /**< Stereoscopic/triangulation sensing */
    bool confirmation_voting;      /**< Multi-agent confirmation of observations */

    // SQUAD tier (N >= 4)
    bool distributed_memory;       /**< Distributed working memory across agents */
    bool formation_control;        /**< Coordinated formation flying/movement */

    // PLATOON tier (N >= 8)
    bool collective_attention;     /**< Shared attention mechanism */
    bool multi_step_planning;      /**< Multi-step collaborative planning */

    // COMPANY tier (N >= 16)
    bool emergent_reasoning;       /**< Emergent reasoning from collective */
    bool prediction;               /**< Predictive modeling of environment */

    // BATTALION tier (N >= 32)
    bool meta_cognition;           /**< Meta-cognitive monitoring of swarm */
    bool swarm_self_model;         /**< Self-model of swarm capabilities */
} swarm_capabilities_t;

//=============================================================================
// Swarm State Structure
//=============================================================================

/**
 * @brief Swarm state snapshot for tier calculation
 *
 * This structure captures the current state of the swarm
 * for tier determination.
 */
typedef struct {
    uint32_t connected_drones;     /**< Number of connected drones */
    uint32_t healthy_drones;       /**< Number of healthy drones */
    float collective_coherence;    /**< Coherence metric [0.0-1.0] */
    uint64_t timestamp;            /**< Timestamp of state (nanoseconds) */
} swarm_state_t;

//=============================================================================
// Emergence Context
//=============================================================================

/**
 * @brief Swarm emergence context (opaque)
 *
 * Internal structure tracking swarm state and tier transitions.
 * Use swarm_emergence_create() to allocate.
 */
typedef struct swarm_emergence_ctx swarm_emergence_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create swarm emergence context
 *
 * WHAT: Allocates and initializes emergence tracking context
 * WHY:  Manage tier transitions and capability unlocking
 * HOW:  Allocates context, sets defaults, initializes state
 *
 * @return Emergence context or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * swarm_emergence_ctx_t* ctx = swarm_emergence_create();
 * if (ctx) {
 *     // Use context
 *     swarm_emergence_destroy(ctx);
 * }
 * ```
 */
swarm_emergence_ctx_t* swarm_emergence_create(void);

/**
 * @brief Destroy swarm emergence context
 *
 * @param ctx Emergence context (may be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
void swarm_emergence_destroy(swarm_emergence_ctx_t* ctx);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set coherence threshold for tier advancement
 *
 * @param ctx Emergence context
 * @param threshold Coherence threshold [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int swarm_emergence_set_coherence_threshold(
    swarm_emergence_ctx_t* ctx,
    float threshold
);

/**
 * @brief Set stability count for tier changes
 *
 * @param ctx Emergence context
 * @param count Number of stable readings required
 * @return 0 on success, -1 on error
 */
int swarm_emergence_set_stability_count(
    swarm_emergence_ctx_t* ctx,
    uint32_t count
);

/**
 * @brief Set hysteresis margin
 *
 * Hysteresis prevents tier flapping by requiring different
 * thresholds for upward vs downward transitions.
 *
 * @param ctx Emergence context
 * @param margin Hysteresis margin (number of drones)
 * @return 0 on success, -1 on error
 */
int swarm_emergence_set_hysteresis(
    swarm_emergence_ctx_t* ctx,
    uint32_t margin
);

//=============================================================================
// State Update API
//=============================================================================

/**
 * @brief Update emergence state with current swarm status
 *
 * WHAT: Updates internal state and recalculates tier
 * WHY:  Keep tier synchronized with swarm state
 * HOW:  Applies hysteresis, checks coherence, updates tier
 *
 * @param ctx Emergence context
 * @param state Current swarm state
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * swarm_state_t state = {
 *     .connected_drones = 10,
 *     .healthy_drones = 9,
 *     .collective_coherence = 0.85,
 *     .timestamp = get_timestamp_ns()
 * };
 * swarm_emergence_update(ctx, &state);
 * ```
 */
int swarm_emergence_update(
    swarm_emergence_ctx_t* ctx,
    const swarm_state_t* state
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current emergence tier
 *
 * @param ctx Emergence context
 * @return Current tier or SWARM_TIER_INDIVIDUAL on error
 */
swarm_emergence_tier_t swarm_emergence_get_tier(
    const swarm_emergence_ctx_t* ctx
);

/**
 * @brief Get current capabilities
 *
 * @param ctx Emergence context
 * @param capabilities Output capabilities structure
 * @return 0 on success, -1 on error
 */
int swarm_emergence_get_capabilities(
    const swarm_emergence_ctx_t* ctx,
    swarm_capabilities_t* capabilities
);

/**
 * @brief Check if specific capability is available
 *
 * @param ctx Emergence context
 * @param capability_name Capability name string
 * @return true if available, false otherwise
 *
 * EXAMPLE:
 * ```c
 * if (swarm_emergence_can_do(ctx, "distributed_memory")) {
 *     // Use distributed memory
 * }
 * ```
 */
bool swarm_emergence_can_do(
    const swarm_emergence_ctx_t* ctx,
    const char* capability_name
);

/**
 * @brief Get connected drone count
 *
 * @param ctx Emergence context
 * @return Connected drone count or 0 on error
 */
uint32_t swarm_emergence_get_connected_count(
    const swarm_emergence_ctx_t* ctx
);

/**
 * @brief Get healthy drone count
 *
 * @param ctx Emergence context
 * @return Healthy drone count or 0 on error
 */
uint32_t swarm_emergence_get_healthy_count(
    const swarm_emergence_ctx_t* ctx
);

/**
 * @brief Get collective coherence
 *
 * @param ctx Emergence context
 * @return Coherence value [0.0-1.0] or 0.0 on error
 */
float swarm_emergence_get_coherence(
    const swarm_emergence_ctx_t* ctx
);

/**
 * @brief Get timestamp of last tier change
 *
 * @param ctx Emergence context
 * @return Timestamp in nanoseconds or 0 on error
 */
uint64_t swarm_emergence_get_tier_change_time(
    const swarm_emergence_ctx_t* ctx
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get tier name string
 *
 * @param tier Emergence tier
 * @return Tier name string
 *
 * EXAMPLE:
 * ```c
 * const char* name = swarm_emergence_get_tier_name(SWARM_TIER_PLATOON);
 * // Returns "PLATOON"
 * ```
 */
const char* swarm_emergence_get_tier_name(swarm_emergence_tier_t tier);

/**
 * @brief Get minimum drone count for tier
 *
 * @param tier Emergence tier
 * @return Minimum drone count
 */
uint32_t swarm_emergence_get_tier_min_drones(swarm_emergence_tier_t tier);

/**
 * @brief Get maximum drone count for tier
 *
 * @param tier Emergence tier
 * @return Maximum drone count (UINT32_MAX for highest tier)
 */
uint32_t swarm_emergence_get_tier_max_drones(swarm_emergence_tier_t tier);

/**
 * @brief Calculate tier from drone count
 *
 * @param drone_count Number of drones
 * @return Appropriate tier for count
 */
swarm_emergence_tier_t swarm_emergence_calculate_tier_from_count(
    uint32_t drone_count
);

/**
 * @brief Get tier description string
 *
 * @param tier Emergence tier
 * @return Description string
 */
const char* swarm_emergence_get_tier_description(swarm_emergence_tier_t tier);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Swarm emergence statistics
 */
typedef struct {
    uint64_t total_updates;           /**< Total state updates */
    uint64_t tier_changes;            /**< Number of tier changes */
    uint64_t stability_violations;    /**< Stability requirement violations */
    uint64_t coherence_failures;      /**< Coherence threshold failures */
    uint64_t health_failures;         /**< Health ratio failures */
    uint32_t max_drones_seen;         /**< Maximum drones observed */
    uint32_t min_drones_seen;         /**< Minimum drones observed */
    float max_coherence_seen;         /**< Maximum coherence observed */
    float min_coherence_seen;         /**< Minimum coherence observed */
    swarm_emergence_tier_t highest_tier_reached; /**< Highest tier reached */
    uint64_t time_in_tier[SWARM_TIER_COUNT]; /**< Time spent in each tier (ns) */
} swarm_emergence_stats_t;

/**
 * @brief Get emergence statistics
 *
 * @param ctx Emergence context
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int swarm_emergence_get_stats(
    const swarm_emergence_ctx_t* ctx,
    swarm_emergence_stats_t* stats
);

/**
 * @brief Reset emergence statistics
 *
 * @param ctx Emergence context
 */
void swarm_emergence_reset_stats(swarm_emergence_ctx_t* ctx);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate swarm state structure
 *
 * @param state Swarm state to validate
 * @return true if valid, false otherwise
 */
bool swarm_emergence_validate_state(const swarm_state_t* state);

/**
 * @brief Check if context is valid
 *
 * @param ctx Emergence context
 * @return true if valid, false otherwise
 */
bool swarm_emergence_is_valid(const swarm_emergence_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SWARM_EMERGENCE_H
