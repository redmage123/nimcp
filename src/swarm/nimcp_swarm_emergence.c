#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_swarm_emergence.c - NIMCP Swarm Emergence System Implementation
//=============================================================================

// Define POSIX feature test macros before any includes
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200112L
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#include "swarm/nimcp_swarm_emergence.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_emergence)

#include "utils/thread/nimcp_atomic.h"

/* Thread-safe BBB registration using atomic compare-exchange */
static nimcp_atomic_bool_t g_bbb_registered = {0};

/**
 * @brief Initialize BBB security for emergence (thread-safe)
 *
 * Uses atomic compare-exchange to ensure exactly one thread
 * performs the registration, preventing TOCTOU race conditions.
 */
static void emergence_init_bbb(void)
{
    bool expected = false;
    if (nimcp_atomic_compare_exchange_bool(&g_bbb_registered, &expected, true,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        bbb_register_module("swarm_emergence", BBB_MODULE_TYPE_SWARM);
        bbb_audit_log(BBB_AUDIT_INFO, "swarm_emergence", "init", "Module registered with BBB");
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal emergence context structure
 */
struct swarm_emergence_ctx {
    uint32_t magic;                           /**< Magic for validation */

    // Current state
    uint32_t connected_drones;                /**< Connected drones */
    uint32_t healthy_drones;                  /**< Healthy drones */
    float collective_coherence;               /**< Coherence [0.0-1.0] */
    swarm_emergence_tier_t current_tier;      /**< Current tier */
    swarm_capabilities_t capabilities;        /**< Current capabilities */
    uint64_t tier_change_time;                /**< Last tier change time (ns) */
    uint64_t last_update_time;                /**< Last update time (ns) */

    // Configuration
    float coherence_threshold;                /**< Min coherence for advancement */
    uint32_t hysteresis_margin;               /**< Hysteresis margin (drones) */
    uint32_t required_stability_count;        /**< Readings for tier change */

    // Hysteresis state
    uint32_t stability_count;                 /**< Current stability counter */
    swarm_emergence_tier_t pending_tier;      /**< Pending tier (during stability) */
    bool in_transition;                       /**< In tier transition */

    // Statistics
    swarm_emergence_stats_t stats;            /**< Emergence statistics */

    // Synchronization
    nimcp_mutex_t mutex;                      /**< Mutex for thread safety */
};

//=============================================================================
// Tier Configuration Tables
//=============================================================================

/**
 * @brief Tier boundary definitions
 */
typedef struct {
    swarm_emergence_tier_t tier;
    uint32_t min_drones;
    uint32_t max_drones;
    const char* name;
    const char* description;
} tier_config_t;

static const tier_config_t TIER_CONFIGS[SWARM_TIER_COUNT] = {
    {
        SWARM_TIER_INDIVIDUAL,
        1, 1,
        "INDIVIDUAL",
        "Local reactive behavior only"
    },
    {
        SWARM_TIER_PAIR,
        2, 3,
        "PAIR",
        "Cooperative sensing with confirmation voting"
    },
    {
        SWARM_TIER_SQUAD,
        4, 7,
        "SQUAD",
        "Distributed working memory and formation control"
    },
    {
        SWARM_TIER_PLATOON,
        8, 15,
        "PLATOON",
        "Collective attention and multi-step planning"
    },
    {
        SWARM_TIER_COMPANY,
        16, 31,
        "COMPANY",
        "Emergent reasoning and predictive modeling"
    },
    {
        SWARM_TIER_BATTALION,
        32, UINT32_MAX,
        "BATTALION",
        "Full meta-cognition and swarm self-model"
    }
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Calculate tier from drone count
 */
static swarm_emergence_tier_t calculate_tier_from_count_internal(uint32_t count)
{
    for (int i = 0; i < SWARM_TIER_COUNT; i++) {
        if (count >= TIER_CONFIGS[i].min_drones &&
            count <= TIER_CONFIGS[i].max_drones) {
            return TIER_CONFIGS[i].tier;
        }
    }
    return SWARM_TIER_INDIVIDUAL;
}

/**
 * @brief Update capabilities based on tier
 */
static void update_capabilities(swarm_emergence_ctx_t* ctx)
{
    if (!ctx) return;

    // Clear all capabilities
    memset(&ctx->capabilities, 0, sizeof(swarm_capabilities_t));

    // Enable capabilities based on tier (cumulative)
    switch (ctx->current_tier) {
        case SWARM_TIER_BATTALION:
            ctx->capabilities.meta_cognition = true;
            ctx->capabilities.swarm_self_model = true;
            // Fall through

        case SWARM_TIER_COMPANY:
            ctx->capabilities.emergent_reasoning = true;
            ctx->capabilities.prediction = true;
            // Fall through

        case SWARM_TIER_PLATOON:
            ctx->capabilities.collective_attention = true;
            ctx->capabilities.multi_step_planning = true;
            // Fall through

        case SWARM_TIER_SQUAD:
            ctx->capabilities.distributed_memory = true;
            ctx->capabilities.formation_control = true;
            // Fall through

        case SWARM_TIER_PAIR:
            ctx->capabilities.stereo_sensing = true;
            ctx->capabilities.confirmation_voting = true;
            // Fall through

        case SWARM_TIER_INDIVIDUAL:
        default:
            // Individual has no special capabilities
            break;
    }
}

/**
 * @brief Apply hysteresis to tier calculation
 */
static swarm_emergence_tier_t apply_hysteresis(
    swarm_emergence_ctx_t* ctx,
    swarm_emergence_tier_t new_tier
)
{
    if (!ctx) return SWARM_TIER_INDIVIDUAL;

    // If no change, reset stability counter
    if (new_tier == ctx->current_tier) {
        ctx->stability_count = 0;
        ctx->in_transition = false;
        return ctx->current_tier;
    }

    // Check if this is a new transition
    if (!ctx->in_transition || new_tier != ctx->pending_tier) {
        ctx->pending_tier = new_tier;
        ctx->stability_count = 1;
        ctx->in_transition = true;
        ctx->stats.stability_violations++;
        return ctx->current_tier;  // Stay at current tier
    }

    // Same pending tier, increment stability
    ctx->stability_count++;

    // Check if we've reached required stability
    if (ctx->stability_count >= ctx->required_stability_count) {
        // Transition approved
        ctx->stability_count = 0;
        ctx->in_transition = false;
        return new_tier;
    }

    // Not stable yet
    return ctx->current_tier;
}

/**
 * @brief Check health ratio
 */
static bool check_health_ratio(const swarm_state_t* state)
{
    if (!state) {
        return false;
    }
    if (state->connected_drones == 0) {
        return false;  /* Zero connected drones - normal initial state */
    }

    float health_ratio = (float)state->healthy_drones / (float)state->connected_drones;
    return health_ratio >= NIMCP_SWARM_MIN_HEALTH_RATIO;
}

/**
 * @brief Update tier statistics
 */
static void update_tier_stats(swarm_emergence_ctx_t* ctx, uint64_t current_time)
{
    if (!ctx || ctx->last_update_time == 0) return;

    uint64_t elapsed = current_time - ctx->last_update_time;
    if (ctx->current_tier < SWARM_TIER_COUNT) {
        ctx->stats.time_in_tier[ctx->current_tier] += elapsed;
    }
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

swarm_emergence_ctx_t* swarm_emergence_create(void)
{
    emergence_init_bbb();

    swarm_emergence_ctx_t* ctx = (swarm_emergence_ctx_t*)nimcp_malloc(
        sizeof(swarm_emergence_ctx_t)
    );

    if (!ctx) {
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_emergence", "create_error", "Failed to allocate context");
        LOG_ERROR("Failed to allocate swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate swarm emergence context");
        return NULL;
    }

    // Initialize structure
    memset(ctx, 0, sizeof(swarm_emergence_ctx_t));

    ctx->magic = NIMCP_SWARM_EMERGENCE_MAGIC;
    ctx->current_tier = SWARM_TIER_INDIVIDUAL;
    ctx->pending_tier = SWARM_TIER_INDIVIDUAL;
    ctx->coherence_threshold = NIMCP_SWARM_DEFAULT_COHERENCE_THRESHOLD;
    ctx->hysteresis_margin = NIMCP_SWARM_DEFAULT_HYSTERESIS;
    ctx->required_stability_count = NIMCP_SWARM_DEFAULT_STABILITY_COUNT;
    ctx->tier_change_time = get_time_ns();
    ctx->last_update_time = ctx->tier_change_time;

    // Initialize statistics
    ctx->stats.min_drones_seen = UINT32_MAX;
    ctx->stats.min_coherence_seen = 1.0F;

    // Initialize mutex
    if (nimcp_mutex_init(&ctx->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize swarm emergence mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize swarm emergence mutex");
        nimcp_free(ctx);
        return NULL;
    }

    LOG_INFO("Created swarm emergence context");

    return ctx;
}

void swarm_emergence_destroy(swarm_emergence_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid swarm emergence context magic in destroy");
        return;
    }

    // Destroy mutex
    nimcp_mutex_destroy(&ctx->mutex);

    // Clear magic
    ctx->magic = 0;

    // Free context
    nimcp_free(ctx);

    LOG_INFO("Destroyed swarm emergence context");
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

int swarm_emergence_set_coherence_threshold(
    swarm_emergence_ctx_t* ctx,
    float threshold
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in set_coherence_threshold");
        return -1;
    }

    if (threshold < 0.0F || threshold > 1.0F) {
        LOG_ERROR("Invalid coherence threshold: %f (must be 0.0-1.0)", threshold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid coherence threshold: %f", threshold);
        return -1;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->coherence_threshold = threshold;
    nimcp_mutex_unlock(&ctx->mutex);

    LOG_DEBUG("Set coherence threshold to %.2f", threshold);

    return 0;
}

int swarm_emergence_set_stability_count(
    swarm_emergence_ctx_t* ctx,
    uint32_t count
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in set_stability_count");
        return -1;
    }

    if (count == 0) {
        LOG_ERROR("Stability count must be at least 1");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Stability count must be at least 1");
        return -1;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->required_stability_count = count;
    nimcp_mutex_unlock(&ctx->mutex);

    LOG_DEBUG("Set stability count to %u", count);

    return 0;
}

int swarm_emergence_set_hysteresis(
    swarm_emergence_ctx_t* ctx,
    uint32_t margin
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in set_hysteresis");
        return -1;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->hysteresis_margin = margin;
    nimcp_mutex_unlock(&ctx->mutex);

    LOG_DEBUG("Set hysteresis margin to %u", margin);

    return 0;
}

//=============================================================================
// State Update API Implementation
//=============================================================================

int swarm_emergence_update(
    swarm_emergence_ctx_t* ctx,
    const swarm_state_t* state
)
{
    if (!bbb_check_pointer(ctx, "swarm_emergence_update") ||
        ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_emergence", "update_error", "Invalid context");
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in update");
        return -1;
    }

    if (!bbb_check_pointer(state, "swarm_emergence_update")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_emergence", "update_error", "NULL swarm state");
        LOG_ERROR("NULL swarm state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL swarm state in update");
        return -1;
    }

    if (!swarm_emergence_validate_state(state)) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_emergence", "update_error",
                     "Invalid swarm state: drones=%u, coherence=%.2f",
                     state->connected_drones, state->collective_coherence);
        LOG_ERROR("Invalid swarm state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid swarm state: drones=%u, coherence=%.2f", state->connected_drones, state->collective_coherence);
        return -1;
    }

    nimcp_mutex_lock(&ctx->mutex);

    uint64_t current_time = get_time_ns();

    // Update tier statistics for previous tier
    update_tier_stats(ctx, current_time);

    // Update state
    ctx->connected_drones = state->connected_drones;
    ctx->healthy_drones = state->healthy_drones;
    ctx->collective_coherence = state->collective_coherence;
    ctx->last_update_time = current_time;

    // Update statistics
    ctx->stats.total_updates++;

    if (state->connected_drones > ctx->stats.max_drones_seen) {
        ctx->stats.max_drones_seen = state->connected_drones;
    }
    if (state->connected_drones < ctx->stats.min_drones_seen) {
        ctx->stats.min_drones_seen = state->connected_drones;
    }
    if (state->collective_coherence > ctx->stats.max_coherence_seen) {
        ctx->stats.max_coherence_seen = state->collective_coherence;
    }
    if (state->collective_coherence < ctx->stats.min_coherence_seen) {
        ctx->stats.min_coherence_seen = state->collective_coherence;
    }

    // Check health ratio
    if (!check_health_ratio(state)) {
        ctx->stats.health_failures++;
        LOG_WARN("Swarm health ratio below threshold");
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;  // Don't change tier on health failure
    }

    // Check coherence threshold for advancement
    if (state->collective_coherence < ctx->coherence_threshold) {
        ctx->stats.coherence_failures++;
        LOG_DEBUG("Swarm coherence below threshold: %.2f < %.2f",
                  state->collective_coherence, ctx->coherence_threshold);
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;  // Don't change tier on coherence failure
    }

    // Calculate new tier from count
    swarm_emergence_tier_t raw_tier = calculate_tier_from_count_internal(
        state->connected_drones
    );

    // Apply hysteresis
    swarm_emergence_tier_t new_tier = apply_hysteresis(ctx, raw_tier);

    // Check if tier changed
    if (new_tier != ctx->current_tier) {
        swarm_emergence_tier_t old_tier = ctx->current_tier;

        ctx->current_tier = new_tier;
        ctx->tier_change_time = current_time;
        ctx->stats.tier_changes++;

        if (new_tier > ctx->stats.highest_tier_reached) {
            ctx->stats.highest_tier_reached = new_tier;
        }

        // Update capabilities
        update_capabilities(ctx);

        LOG_INFO("Swarm tier changed: %s -> %s (drones=%u, coherence=%.2f)",
                 swarm_emergence_get_tier_name(old_tier),
                 swarm_emergence_get_tier_name(new_tier),
                 state->connected_drones,
                 state->collective_coherence);
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return 0;
}

//=============================================================================
// Query API Implementation
//=============================================================================

swarm_emergence_tier_t swarm_emergence_get_tier(
    const swarm_emergence_ctx_t* ctx
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return SWARM_TIER_INDIVIDUAL;
    }

    return ctx->current_tier;
}

int swarm_emergence_get_capabilities(
    const swarm_emergence_ctx_t* ctx,
    swarm_capabilities_t* capabilities
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in get_capabilities");
        return -1;
    }

    if (!capabilities) {
        LOG_ERROR("NULL capabilities pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL capabilities pointer in get_capabilities");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&ctx->mutex);
    *capabilities = ctx->capabilities;
    nimcp_mutex_unlock((nimcp_mutex_t*)&ctx->mutex);

    return 0;
}

bool swarm_emergence_can_do(
    const swarm_emergence_ctx_t* ctx,
    const char* capability_name
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return false;
    }

    if (!capability_name) {
        return false;
    }

    swarm_capabilities_t caps;
    if (swarm_emergence_get_capabilities(ctx, &caps) != 0) {
        return false;
    }

    // Check capability by name
    if (strcmp(capability_name, "stereo_sensing") == 0) {
        return caps.stereo_sensing;
    } else if (strcmp(capability_name, "confirmation_voting") == 0) {
        return caps.confirmation_voting;
    } else if (strcmp(capability_name, "distributed_memory") == 0) {
        return caps.distributed_memory;
    } else if (strcmp(capability_name, "formation_control") == 0) {
        return caps.formation_control;
    } else if (strcmp(capability_name, "collective_attention") == 0) {
        return caps.collective_attention;
    } else if (strcmp(capability_name, "multi_step_planning") == 0) {
        return caps.multi_step_planning;
    } else if (strcmp(capability_name, "emergent_reasoning") == 0) {
        return caps.emergent_reasoning;
    } else if (strcmp(capability_name, "prediction") == 0) {
        return caps.prediction;
    } else if (strcmp(capability_name, "meta_cognition") == 0) {
        return caps.meta_cognition;
    } else if (strcmp(capability_name, "swarm_self_model") == 0) {
        return caps.swarm_self_model;
    }

    LOG_WARN("Unknown capability name: %s", capability_name);
    return false;
}

uint32_t swarm_emergence_get_connected_count(
    const swarm_emergence_ctx_t* ctx
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return 0;
    }

    return ctx->connected_drones;
}

uint32_t swarm_emergence_get_healthy_count(
    const swarm_emergence_ctx_t* ctx
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return 0;
    }

    return ctx->healthy_drones;
}

float swarm_emergence_get_coherence(
    const swarm_emergence_ctx_t* ctx
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return 0.0F;
    }

    return ctx->collective_coherence;
}

uint64_t swarm_emergence_get_tier_change_time(
    const swarm_emergence_ctx_t* ctx
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return 0;
    }

    return ctx->tier_change_time;
}

//=============================================================================
// Utility API Implementation
//=============================================================================

const char* swarm_emergence_get_tier_name(swarm_emergence_tier_t tier)
{
    if (tier >= SWARM_TIER_COUNT) {
        return "UNKNOWN";
    }

    return TIER_CONFIGS[tier].name;
}

uint32_t swarm_emergence_get_tier_min_drones(swarm_emergence_tier_t tier)
{
    if (tier >= SWARM_TIER_COUNT) {
        return 0;
    }

    return TIER_CONFIGS[tier].min_drones;
}

uint32_t swarm_emergence_get_tier_max_drones(swarm_emergence_tier_t tier)
{
    if (tier >= SWARM_TIER_COUNT) {
        return 0;
    }

    return TIER_CONFIGS[tier].max_drones;
}

swarm_emergence_tier_t swarm_emergence_calculate_tier_from_count(
    uint32_t drone_count
)
{
    return calculate_tier_from_count_internal(drone_count);
}

const char* swarm_emergence_get_tier_description(swarm_emergence_tier_t tier)
{
    if (tier >= SWARM_TIER_COUNT) {
        return "Unknown tier";
    }

    return TIER_CONFIGS[tier].description;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int swarm_emergence_get_stats(
    const swarm_emergence_ctx_t* ctx,
    swarm_emergence_stats_t* stats
)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in get_stats");
        return -1;
    }

    if (!stats) {
        LOG_ERROR("NULL stats pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL stats pointer in get_stats");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&ctx->mutex);
    *stats = ctx->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)&ctx->mutex);

    return 0;
}

void swarm_emergence_reset_stats(swarm_emergence_ctx_t* ctx)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        LOG_ERROR("Invalid swarm emergence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid swarm emergence context in reset_stats");
        return;
    }

    nimcp_mutex_lock(&ctx->mutex);

    // Reset all statistics
    memset(&ctx->stats, 0, sizeof(swarm_emergence_stats_t));
    ctx->stats.min_drones_seen = UINT32_MAX;
    ctx->stats.min_coherence_seen = 1.0F;

    nimcp_mutex_unlock(&ctx->mutex);

    LOG_INFO("Reset swarm emergence statistics");
}

//=============================================================================
// Validation API Implementation
//=============================================================================

bool swarm_emergence_validate_state(const swarm_state_t* state)
{
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_validate_state: state is NULL");
        return false;
    }

    // Check healthy drones doesn't exceed connected
    if (state->healthy_drones > state->connected_drones) {
        LOG_ERROR("Healthy drones (%u) exceeds connected drones (%u)",
                  state->healthy_drones, state->connected_drones);
        return false;
    }

    // Check coherence range
    if (state->collective_coherence < 0.0F || state->collective_coherence > 1.0F) {
        LOG_ERROR("Invalid coherence value: %f (must be 0.0-1.0)",
                  state->collective_coherence);
        return false;
    }

    // Check for reasonable drone count
    if (state->connected_drones > 10000) {
        LOG_WARN("Very large swarm size: %u drones", state->connected_drones);
    }

    return true;
}

bool swarm_emergence_is_valid(const swarm_emergence_ctx_t* ctx)
{
    if (!ctx) {
        return false;
    }

    if (ctx->magic != NIMCP_SWARM_EMERGENCE_MAGIC) {
        return false;
    }

    return true;
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about swarm emergence
 *
 * WHAT: Query knowledge graph for self-knowledge about swarm emergence module
 * WHY:  Enable self-awareness by introspecting module's identity in KG
 * HOW:  Query entity, observations, and relations from knowledge graph
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if entity found, 0 if not found or error
 */
int swarm_emergence_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) {
        return 0;
    }

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Emergence");
    if (self) {
        LOG_INFO("KG Self-Knowledge: Found entity '%s' of type '%s'",
                 self->name, self->entity_type);
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("  Observation[%u]: %s", i, self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Emergence");
    if (connections) {
        LOG_INFO("KG Self-Knowledge: Swarm_Emergence has %u outgoing connections",
                 connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Emergence");
    if (incoming) {
        LOG_INFO("KG Self-Knowledge: Swarm_Emergence has %u incoming connections",
                 incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
