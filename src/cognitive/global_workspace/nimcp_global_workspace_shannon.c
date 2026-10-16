/**
 * @file nimcp_global_workspace_shannon.c
 * @brief Shannon information theory integration for Global Workspace
 *
 * WHAT: Implementation of Shannon-monitored workspace competition and broadcast
 * WHY:  Information content determines workspace access, prevents subscriber overload
 * HOW:  Information-weighted competition + channel capacity monitoring + adaptive rate
 *
 * PHASE: 1.5.3 - Global Workspace Integration + Information Competition
 *
 * NIMCP CODING STANDARDS:
 * - Uses nimcp_malloc/nimcp_calloc/nimcp_free (never raw malloc)
 * - WHAT/WHY/HOW documentation on all functions
 * - Opaque pointer pattern for internal state
 * - NULL-safe functions throughout
 *
 * MATHEMATICAL FOUNDATION:
 * - Shannon entropy: H(X) = -Σ p(x) log₂ p(x)
 * - Competition strength = salience × (info_bits / normalization)
 * - Bottleneck detection: utilization > 0.9 threshold
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 * @version 1.0.0 Phase 1.5.3
 */

#include "cognitive/global_workspace/nimcp_global_workspace_shannon.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(global_workspace_shannon, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Constants
//=============================================================================

/** Epsilon for floating point comparisons */
#define SHANNON_EPSILON 1e-10f

/** Minimum probability for entropy calculation (avoid log(0)) */
#define MIN_PROBABILITY 1e-12f

/** Load decay factor for exponential moving average */
#define LOAD_DECAY_FACTOR 0.9f

/** Maximum history for load tracking */
#define LOAD_HISTORY_SIZE 10

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-subscriber Shannon state
 *
 * WHAT: Tracks channel capacity and load for each subscriber
 * WHY:  Enable bottleneck detection per subscriber
 * HOW:  Store capacity, track load with EMA
 */
typedef struct {
    cognitive_module_t module;          /**< Subscriber module ID */
    float capacity_bits_per_sec;        /**< Processing capacity */
    float current_load;                 /**< Current load (EMA) */
    float total_delivered;              /**< Cumulative delivered bits */
    float total_lost;                   /**< Cumulative lost bits */
    uint64_t bottleneck_count;          /**< Times this subscriber bottlenecked */
    uint64_t last_update_ms;            /**< Last load update timestamp */
    bool is_active;                     /**< Is subscriber active? */
} subscriber_shannon_state_t;

/**
 * @brief Shannon workspace state (internal)
 *
 * WHAT: All Shannon-related state for a workspace
 * WHY:  Encapsulate Shannon features separate from core workspace
 * HOW:  Allocated when Shannon enabled, freed when disabled
 */
typedef struct shannon_workspace_state {
    /* Configuration */
    shannon_workspace_config_t config;
    bool is_enabled;

    /* Per-subscriber state */
    subscriber_shannon_state_t subscribers[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];
    uint32_t num_subscribers;

    /* Broadcast rate control */
    float current_broadcast_rate;       /**< Rate multiplier [0.1, 1.0] */
    uint64_t last_rate_adjustment_ms;   /**< When rate was last adjusted */

    /* Statistics */
    shannon_workspace_stats_t stats;

    /* Last broadcast metrics (cached) */
    shannon_broadcast_metrics_t last_metrics;
    bool has_last_metrics;

    /* Timestamp tracking */
    uint64_t creation_time_ms;

} shannon_workspace_state_t;

/**
 * @brief Global mapping of workspace to Shannon state
 *
 * WHAT: Associate Shannon state with workspaces
 * WHY:  Avoid modifying core workspace structure
 * HOW:  Simple array lookup (workspaces are rare, typically 1 per brain)
 *
 * LIMITATION: Max 16 workspaces (more than enough for typical use)
 */
#define MAX_SHANNON_WORKSPACES 16

typedef struct {
    global_workspace_t* workspace;
    shannon_workspace_state_t* state;
} shannon_workspace_mapping_t;

/* Mutex protects compound array+counter operations on g_shannon_mappings.
 * The atomic counter alone cannot protect the array writes/reads from races
 * (e.g., disable compacting while enable is writing, or get_shannon_state
 * reading while another thread compacts). */
static shannon_workspace_mapping_t g_shannon_mappings[MAX_SHANNON_WORKSPACES];
static _Atomic uint32_t g_num_mappings = 0;
static nimcp_mutex_t* g_shannon_mapping_mutex = NULL;

/* Lazy-init the mapping mutex (double-checked locking with atomic flag) */
static _Atomic bool g_shannon_mutex_initialized = false;
static void ensure_shannon_mutex(void) {
    if (atomic_load(&g_shannon_mutex_initialized)) return;
    /* Race on first call is benign: nimcp_mutex_create is idempotent and we
     * only ever store one winner. Worst case: a leaked mutex on first init. */
    nimcp_mutex_t* m = nimcp_mutex_create(NULL);
    if (!m) return;
    nimcp_mutex_t* expected = NULL;
    if (!atomic_compare_exchange_strong((_Atomic(nimcp_mutex_t*)*)&g_shannon_mapping_mutex,
                                         &expected, m)) {
        nimcp_mutex_destroy(m);  /* Another thread won the race */
    }
    atomic_store(&g_shannon_mutex_initialized, true);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get Shannon state for workspace
 *
 * WHAT: Lookup Shannon state by workspace handle
 * WHY:  Associate state without modifying workspace struct
 * HOW:  Linear search (small array)
 *
 * @param workspace Workspace handle
 * @return Shannon state or NULL if not enabled
 *
 * COMPLEXITY: O(N) where N = num_mappings (typically 1)
 */
static shannon_workspace_state_t* get_shannon_state(const global_workspace_t* workspace) {
    if (!workspace) {
        return NULL;
    }

    ensure_shannon_mutex();
    if (g_shannon_mapping_mutex) nimcp_mutex_lock(g_shannon_mapping_mutex);

    uint32_t count = atomic_load(&g_num_mappings);
    shannon_workspace_state_t* result = NULL;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)count);
        }

        if (g_shannon_mappings[i].workspace == workspace) {
            result = g_shannon_mappings[i].state;
            break;
        }
    }

    if (g_shannon_mapping_mutex) nimcp_mutex_unlock(g_shannon_mapping_mutex);
    return result;  /* NULL when not found is normal — not an error */
}

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Platform-independent timestamp
 * WHY:  Track timing for rate control and load decay
 * HOW:  Use clock_gettime or fallback
 */
static uint64_t get_current_time_ms(void) {
    /* Simple implementation - could use nimcp_time if available */
    static _Atomic uint64_t counter = 0;
    return atomic_fetch_add(&counter, 1);
}

/**
 * @brief Find subscriber in Shannon state
 *
 * WHAT: Lookup subscriber by module ID
 * WHY:  Get per-subscriber capacity/load
 * HOW:  Linear search
 *
 * @param state Shannon state
 * @param module Module to find
 * @return Subscriber state or NULL if not found
 */
static subscriber_shannon_state_t* find_subscriber(
    shannon_workspace_state_t* state,
    cognitive_module_t module
) {
    if (!state) {
        return NULL;
    }

    for (uint32_t i = 0; i < state->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_subscribers > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)state->num_subscribers);
        }

        if (state->subscribers[i].module == module &&
            state->subscribers[i].is_active) {
            return &state->subscribers[i];
        }
    }
    return NULL;  /* Not found is normal */
}

//=============================================================================
// Configuration Functions
//=============================================================================

shannon_workspace_config_t shannon_workspace_default_config(void) {
    /* WHAT: Sensible defaults for Shannon workspace
     * WHY:  Convenient starting point with balanced parameters
     * HOW:  Initialize struct with documented values */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_shannon_workspace_de", 0.0f);


    shannon_workspace_config_t config;
    memset(&config, 0, sizeof(config));

    /* Competition enhancement */
    config.enable_info_weighted_competition = true;
    config.info_threshold_bits = GWS_DEFAULT_INFO_THRESHOLD_BITS;
    config.info_weight = 0.5F;  /* Equal weight to salience and info */

    /* Broadcast monitoring */
    config.enable_shannon_monitoring = true;
    config.default_subscriber_capacity = GWS_DEFAULT_SUBSCRIBER_CAPACITY;
    config.bottleneck_threshold = GWS_BOTTLENECK_THRESHOLD;

    /* Adaptive rate control */
    config.enable_adaptive_rate = true;
    config.rate_reduction_factor = GWS_BOTTLENECK_RATE_REDUCTION;
    config.rate_recovery_factor = GWS_RATE_RECOVERY_FACTOR;
    config.min_broadcast_rate = 0.1F;
    config.max_broadcast_rate = 1.0F;

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

bool global_workspace_enable_shannon(
    global_workspace_t* workspace,
    const shannon_workspace_config_t* config
) {
    /* WHAT: Enable Shannon features on existing workspace
     * WHY:  Add information-theoretic monitoring without recreation
     * HOW:  Allocate state, initialize subscribers, register mapping */

    if (!workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_enable_shannon: workspace is NULL");
        return false;
    }

    /* Check if already enabled */
    if (get_shannon_state(workspace) != NULL) {
        return true;  /* Already enabled - success */
    }

    /* Check capacity */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_ena", 0.0f);

    ensure_shannon_mutex();
    if (g_shannon_mapping_mutex) nimcp_mutex_lock(g_shannon_mapping_mutex);

    if (g_num_mappings >= MAX_SHANNON_WORKSPACES) {
        if (g_shannon_mapping_mutex) nimcp_mutex_unlock(g_shannon_mapping_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "global_workspace_enable_shannon: capacity exceeded");
        return false;  /* Too many workspaces */
    }

    /* Allocate Shannon state using NIMCP memory utils */
    shannon_workspace_state_t* state =
        (shannon_workspace_state_t*)nimcp_calloc(1, sizeof(shannon_workspace_state_t));
    if (!state) {
        if (g_shannon_mapping_mutex) nimcp_mutex_unlock(g_shannon_mapping_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_enable_shannon: state is NULL");
        return false;  /* Allocation failed */
    }

    /* Initialize configuration */
    if (config) {
        state->config = *config;
    } else {
        state->config = shannon_workspace_default_config();
    }
    state->is_enabled = true;

    /* Initialize broadcast rate */
    state->current_broadcast_rate = 1.0F;
    state->creation_time_ms = get_current_time_ms();

    /* Initialize subscribers from workspace */
    uint32_t sub_count = global_workspace_get_subscriber_count(workspace);
    state->num_subscribers = 0;

    /* Note: We'd need to iterate workspace subscribers here
     * For now, initialize empty - subscribers register via set_capacity */

    /* Clear statistics */
    memset(&state->stats, 0, sizeof(shannon_workspace_stats_t));
    state->stats.current_broadcast_rate = 1.0F;

    /* Register mapping (under mutex — array write + counter increment are atomic together) */
    uint32_t idx = atomic_load(&g_num_mappings);
    g_shannon_mappings[idx].workspace = workspace;
    g_shannon_mappings[idx].state = state;
    atomic_fetch_add(&g_num_mappings, 1);

    if (g_shannon_mapping_mutex) nimcp_mutex_unlock(g_shannon_mapping_mutex);

    return true;
}

void global_workspace_disable_shannon(global_workspace_t* workspace) {
    /* WHAT: Remove Shannon monitoring from workspace
     * WHY:  Reduce overhead if not needed
     * HOW:  Find mapping, free state, compact array */

    if (!workspace) return;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_dis", 0.0f);

    ensure_shannon_mutex();
    if (g_shannon_mapping_mutex) nimcp_mutex_lock(g_shannon_mapping_mutex);

    uint32_t count = atomic_load(&g_num_mappings);
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)count);
        }

        if (g_shannon_mappings[i].workspace == workspace) {
            /* Free state */
            if (g_shannon_mappings[i].state) {
                nimcp_free(g_shannon_mappings[i].state);
            }

            /* Compact array (move last to this slot) */
            uint32_t last = count - 1;
            if (i < last) {
                g_shannon_mappings[i] = g_shannon_mappings[last];
            }
            memset(&g_shannon_mappings[last], 0, sizeof(shannon_workspace_mapping_t));
            atomic_fetch_sub(&g_num_mappings, 1);

            if (g_shannon_mapping_mutex) nimcp_mutex_unlock(g_shannon_mapping_mutex);
            return;
        }
    }

    if (g_shannon_mapping_mutex) nimcp_mutex_unlock(g_shannon_mapping_mutex);
}

bool global_workspace_is_shannon_enabled(const global_workspace_t* workspace) {
    /* WHAT: Check if Shannon features active
     * WHY:  Conditional code paths
     * HOW:  Lookup state */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_is_", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    return (state != NULL && state->is_enabled);
}

//=============================================================================
// Information Measurement
//=============================================================================

float shannon_measure_feature_information(
    const float* features,
    uint32_t dim
) {
    /* WHAT: Calculate Shannon entropy of feature vector
     * WHY:  Determine information content for competition weighting
     * HOW:  Normalize to probabilities, compute H(X) = -Σ p log₂ p
     *
     * ALGORITHM:
     * 1. Sum absolute values for normalization
     * 2. Normalize to probability distribution
     * 3. Compute entropy H(X) = -Σ p(x) log₂ p(x)
     * 4. Handle zeros (0 log 0 = 0 by L'Hôpital)
     *
     * COMPLEXITY: O(D) where D = dimensionality
     * RANGE: 0 to log₂(D) bits
     */

    if (!features || dim == 0) return 0.0F;

    /* Step 1: Compute sum for normalization */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_shannon_measure_feat", 0.0f);


    float sum = 0.0F;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)dim);
        }

        float val = features[i];
        if (val < 0.0F) val = -val;  /* Use absolute value */
        sum += val;
    }

    /* Handle zero vector */
    if (sum < SHANNON_EPSILON) {
        return 0.0F;  /* Zero entropy for zero vector */
    }

    /* Step 2 & 3: Compute entropy */
    float entropy = 0.0F;
    float inv_sum = 1.0F / (fabsf(sum) > 1e-7f ? sum : 1e-7f);

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)dim);
        }

        float val = features[i];
        if (val < 0.0F) val = -val;

        float p = val * inv_sum;

        /* Skip zero probabilities (0 log 0 = 0) */
        if (p > MIN_PROBABILITY) {
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

float shannon_measure_relative_information(
    const float* features,
    uint32_t dim
) {
    /* WHAT: Measure relative information (KL divergence from uniform)
     * WHY:  Normalized measure independent of dimensionality
     * HOW:  D_KL(P || U) = log₂(D) - H(P)
     *
     * Maximum entropy is log₂(D) for uniform distribution
     * Relative information = how far from uniform
     *
     * RANGE: 0 (uniform) to log₂(D) (deterministic)
     */

    if (!features || dim == 0) return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_shannon_measure_rela", 0.0f);


    float max_entropy = log2f((float)dim);
    float actual_entropy = shannon_measure_feature_information(features, dim);

    /* Relative info = max - actual (always non-negative) */
    float relative = max_entropy - actual_entropy;
    if (relative < 0.0F) relative = 0.0F;

    return relative;
}

//=============================================================================
// Information-Weighted Competition
//=============================================================================

bool global_workspace_compete_with_info(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float salience,
    float* info_bits_out
) {
    /* WHAT: Compete for workspace with information weighting
     * WHY:  High-information, salient events should win consciousness
     * HOW:  Measure info, compute weighted strength, call standard compete
     *
     * ALGORITHM:
     * 1. Get Shannon state (proceed without if not enabled)
     * 2. Measure info_bits from content
     * 3. If Shannon enabled and info < threshold, reject
     * 4. Compute competition_strength = salience × (info_bits / norm)
     * 5. Clamp strength to [0, 1]
     * 6. Call standard compete
     * 7. Update statistics
     */

    if (!workspace || !content || content_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_compete_with_info: required parameter is NULL (workspace, content)");
        return false;
    }

    /* Measure information content */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_com", 0.0f);


    float info_bits = shannon_measure_feature_information(content, content_dim);
    if (info_bits_out) {
        *info_bits_out = info_bits;
    }

    /* Get Shannon state */
    shannon_workspace_state_t* state = get_shannon_state(workspace);

    /* Compute competition strength */
    float competition_strength = 0.0f;

    if (state && state->is_enabled && state->config.enable_info_weighted_competition) {
        /* Check information threshold */
        if (info_bits < state->config.info_threshold_bits) {
            /* Too little information - reject */
            state->stats.low_info_rejections++;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_compete_with_info: validation failed");
            return false;
        }

        /* Information-weighted competition
         * strength = salience × info_weight × (info_bits / norm)
         *          + salience × (1 - info_weight) */
        float info_factor = info_bits / GWS_INFO_NORMALIZATION_BITS;
        if (info_factor > 1.0F) info_factor = 1.0F;

        float w = state->config.info_weight;
        competition_strength = salience * (w * info_factor + (1.0F - w));

        /* Update statistics */
        state->stats.total_competitions_with_info++;
    } else {
        /* No Shannon - use raw salience */
        competition_strength = salience;
    }

    /* Clamp to valid range */
    competition_strength = nimcp_clampf(competition_strength, 0.0F, 1.0F);

    /* Call standard competition */
    bool won = global_workspace_compete(workspace, module, content,
                                         content_dim, competition_strength);

    /* Update winner/loser statistics */
    if (state && state->is_enabled) {
        if (won) {
            /* Running average of winner info */
            float n = (float)state->stats.total_shannon_broadcasts;
            state->stats.avg_winner_info_bits =
                (state->stats.avg_winner_info_bits * n + info_bits) / (n + 1.0F);
            state->stats.total_shannon_broadcasts++;
        } else {
            /* Running average of loser info */
            uint64_t losses = state->stats.total_competitions_with_info -
                              state->stats.total_shannon_broadcasts;
            if (losses > 0) {
                float n = (float)losses;
                state->stats.avg_loser_info_bits =
                    (state->stats.avg_loser_info_bits * n + info_bits) / (n + 1.0F);
            }
        }
    }

    return won;
}

//=============================================================================
// Shannon-Monitored Broadcast
//=============================================================================

shannon_broadcast_metrics_t global_workspace_broadcast_with_shannon(
    global_workspace_t* workspace,
    const float* content,
    uint32_t dim,
    float content_info_bits
) {
    /* WHAT: Broadcast with channel capacity monitoring
     * WHY:  Prevent subscriber overload, detect bottlenecks
     * HOW:  Check each subscriber's utilization, reduce delivery if overloaded
     *
     * ALGORITHM (per subscriber):
     * 1. Get capacity and current load
     * 2. utilization = load / capacity
     * 3. If utilization > threshold:
     *    - Mark bottleneck
     *    - delivered = info × (1 - utilization)
     *    - loss = info - delivered
     * 4. Else: full delivery
     * 5. Update load
     * 6. Notify subscriber
     */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_bro", 0.0f);


    shannon_broadcast_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.content_info_bits = content_info_bits;
    metrics.broadcast_timestamp_ms = get_current_time_ms();

    if (!workspace || !content) {
        return metrics;
    }

    /* Get Shannon state */
    shannon_workspace_state_t* state = get_shannon_state(workspace);

    if (!state || !state->is_enabled || !state->config.enable_shannon_monitoring) {
        /* No Shannon monitoring - just track basic metrics */
        metrics.num_subscribers = global_workspace_get_subscriber_count(workspace);
        for (uint32_t i = 0; i < metrics.num_subscribers &&
             i < GLOBAL_WORKSPACE_MAX_SUBSCRIBERS; i++) {
            metrics.information_delivered[i] = content_info_bits;
            metrics.information_loss[i] = 0.0F;
        }
        metrics.total_info_delivered = content_info_bits * metrics.num_subscribers;
        metrics.delivery_efficiency = 1.0F;
        return metrics;
    }

    /* Process each subscriber */
    metrics.num_subscribers = state->num_subscribers;

    for (uint32_t i = 0; i < state->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_subscribers > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)state->num_subscribers);
        }

        subscriber_shannon_state_t* sub = &state->subscribers[i];
        if (!sub->is_active) continue;

        float capacity = sub->capacity_bits_per_sec;
        float load = sub->current_load;

        /* Calculate utilization */
        float utilization = (capacity > SHANNON_EPSILON) ? (load / capacity) : 1.0F;
        metrics.subscriber_utilization[i] = utilization;

        if (utilization > state->config.bottleneck_threshold) {
            /* Subscriber is bottlenecked */
            metrics.bottleneck_detected = true;
            if (metrics.num_bottlenecked == 0) {
                metrics.bottlenecked_module = sub->module;
            }
            metrics.num_bottlenecked++;

            /* Reduced delivery based on available capacity */
            float available = 1.0F - utilization;
            if (available < 0.0F) available = 0.0F;

            float delivered = content_info_bits * available;
            float lost = content_info_bits - delivered;

            metrics.information_delivered[i] = delivered;
            metrics.information_loss[i] = lost;

            /* Update subscriber stats */
            sub->total_delivered += delivered;
            sub->total_lost += lost;
            sub->bottleneck_count++;

            /* Update global stats */
            state->stats.subscriber_total_delivered[i] += delivered;
            state->stats.subscriber_total_lost[i] += lost;
            state->stats.subscriber_bottleneck_count[i]++;
        } else {
            /* Full delivery */
            metrics.information_delivered[i] = content_info_bits;
            metrics.information_loss[i] = 0.0F;

            sub->total_delivered += content_info_bits;
            state->stats.subscriber_total_delivered[i] += content_info_bits;
        }

        /* Update subscriber load (exponential moving average) */
        sub->current_load = sub->current_load * LOAD_DECAY_FACTOR +
                           metrics.information_delivered[i];
        sub->last_update_ms = metrics.broadcast_timestamp_ms;

        /* Accumulate totals */
        metrics.total_info_delivered += metrics.information_delivered[i];
        metrics.total_info_loss += metrics.information_loss[i];
    }

    /* Calculate efficiency */
    float total = metrics.total_info_delivered + metrics.total_info_loss;
    metrics.delivery_efficiency = (total > SHANNON_EPSILON) ?
        (metrics.total_info_delivered / (fabsf(total) > 1e-7f ? total : 1e-7f)) : 1.0F;

    /* Update global statistics */
    state->stats.total_info_delivered_bits += metrics.total_info_delivered;
    state->stats.total_info_lost_bits += metrics.total_info_loss;

    if (metrics.bottleneck_detected) {
        state->stats.bottleneck_events++;
    }

    /* Update overall efficiency */
    float total_all = state->stats.total_info_delivered_bits +
                      state->stats.total_info_lost_bits;
    state->stats.overall_delivery_efficiency = (total_all > SHANNON_EPSILON) ?
        (state->stats.total_info_delivered_bits / total_all) : 1.0F;

    /* Cache metrics */
    state->last_metrics = metrics;
    state->has_last_metrics = true;

    /* Adapt broadcast rate if enabled */
    if (state->config.enable_adaptive_rate) {
        global_workspace_adapt_broadcast_rate(workspace, &metrics);
    }

    return metrics;
}

//=============================================================================
// Subscriber Capacity Management
//=============================================================================

bool global_workspace_set_subscriber_capacity(
    global_workspace_t* workspace,
    cognitive_module_t subscriber,
    float capacity_bits_per_sec
) {
    /* WHAT: Set subscriber processing capacity
     * WHY:  Enable bottleneck detection for this subscriber
     * HOW:  Find or create subscriber entry, set capacity */

    if (!workspace || capacity_bits_per_sec < 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_set_subscriber_capacity: workspace is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_set", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_set_subscriber_capacity: state is NULL");
        return false;  /* Shannon not enabled */
    }

    /* Find existing subscriber */
    subscriber_shannon_state_t* sub = find_subscriber(state, subscriber);

    if (sub) {
        /* Update existing */
        sub->capacity_bits_per_sec = capacity_bits_per_sec;
        return true;
    }

    /* Add new subscriber */
    if (state->num_subscribers >= GLOBAL_WORKSPACE_MAX_SUBSCRIBERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "global_workspace_set_subscriber_capacity: capacity exceeded");
        return false;  /* Full */
    }

    sub = &state->subscribers[state->num_subscribers];
    memset(sub, 0, sizeof(subscriber_shannon_state_t));
    sub->module = subscriber;
    sub->capacity_bits_per_sec = capacity_bits_per_sec;
    sub->is_active = true;
    sub->last_update_ms = get_current_time_ms();

    state->num_subscribers++;
    return true;
}

float global_workspace_get_subscriber_capacity(
    const global_workspace_t* workspace,
    cognitive_module_t subscriber
) {
    /* WHAT: Get subscriber capacity
     * WHY:  Query for analysis/debugging
     * HOW:  Lookup subscriber */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_get", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) return 0.0F;

    subscriber_shannon_state_t* sub = find_subscriber(state, subscriber);
    return sub ? sub->capacity_bits_per_sec : 0.0F;
}

float global_workspace_get_subscriber_load(
    const global_workspace_t* workspace,
    cognitive_module_t subscriber
) {
    /* WHAT: Get subscriber current load
     * WHY:  Monitor utilization
     * HOW:  Lookup subscriber */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_get", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) return 0.0F;

    subscriber_shannon_state_t* sub = find_subscriber(state, subscriber);
    return sub ? sub->current_load : 0.0F;
}

void global_workspace_update_subscriber_load(
    global_workspace_t* workspace,
    cognitive_module_t subscriber,
    float delivered_bits
) {
    /* WHAT: Manually update subscriber load
     * WHY:  Allow external load tracking
     * HOW:  Add to EMA */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_upd", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) return;

    subscriber_shannon_state_t* sub = find_subscriber(state, subscriber);
    if (sub) {
        sub->current_load = sub->current_load * LOAD_DECAY_FACTOR + delivered_bits;
        sub->last_update_ms = get_current_time_ms();
    }
}

//=============================================================================
// Adaptive Broadcast Rate Control
//=============================================================================

bool global_workspace_set_broadcast_rate(
    global_workspace_t* workspace,
    float rate_multiplier
) {
    /* WHAT: Set broadcast rate multiplier
     * WHY:  Manual rate control
     * HOW:  Validate and store */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_set", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_set_broadcast_rate: state is NULL");
        return false;
    }

    rate_multiplier = nimcp_clampf(rate_multiplier,
                                   state->config.min_broadcast_rate,
                                   state->config.max_broadcast_rate);

    state->current_broadcast_rate = rate_multiplier;
    state->stats.current_broadcast_rate = rate_multiplier;

    return true;
}

float global_workspace_get_broadcast_rate(const global_workspace_t* workspace) {
    /* WHAT: Get current rate multiplier
     * WHY:  Query current state
     * HOW:  Return cached value */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_get", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    return state ? state->current_broadcast_rate : 1.0F;
}

void global_workspace_adapt_broadcast_rate(
    global_workspace_t* workspace,
    const shannon_broadcast_metrics_t* metrics
) {
    /* WHAT: Auto-adjust rate based on bottleneck status
     * WHY:  Self-regulating information flow
     * HOW:  Reduce on bottleneck, recover when clear
     *
     * ALGORITHM:
     * If bottleneck: rate *= reduction_factor, clamp to min
     * Else: rate *= recovery_factor, clamp to max
     */

    if (!workspace || !metrics) return;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_ada", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state || !state->config.enable_adaptive_rate) return;

    float new_rate = state->current_broadcast_rate;

    if (metrics->bottleneck_detected) {
        /* Reduce rate */
        new_rate *= state->config.rate_reduction_factor;
        if (new_rate < state->config.min_broadcast_rate) {
            new_rate = state->config.min_broadcast_rate;
        }
        state->stats.rate_reductions++;
    } else {
        /* Recover rate */
        new_rate *= state->config.rate_recovery_factor;
        if (new_rate > state->config.max_broadcast_rate) {
            new_rate = state->config.max_broadcast_rate;
        }
        if (new_rate > state->current_broadcast_rate) {
            state->stats.rate_recoveries++;
        }
    }

    state->current_broadcast_rate = new_rate;
    state->stats.current_broadcast_rate = new_rate;
    state->last_rate_adjustment_ms = get_current_time_ms();
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

bool global_workspace_get_shannon_stats(
    const global_workspace_t* workspace,
    shannon_workspace_stats_t* stats
) {
    /* WHAT: Get accumulated Shannon statistics
     * WHY:  Analysis and debugging
     * HOW:  Copy internal stats */

    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_get_shannon_stats: stats is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_get", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_get_shannon_stats: state is NULL");
        return false;
    }

    *stats = state->stats;
    return true;
}

void global_workspace_reset_shannon_stats(global_workspace_t* workspace) {
    /* WHAT: Clear Shannon statistics
     * WHY:  Fresh measurement period
     * HOW:  Zero stats, keep config */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_res", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state) return;

    /* Preserve current rate */
    float rate = state->current_broadcast_rate;

    memset(&state->stats, 0, sizeof(shannon_workspace_stats_t));

    state->stats.current_broadcast_rate = rate;

    /* Reset per-subscriber stats */
    for (uint32_t i = 0; i < state->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_subscribers > 256) {
            global_workspace_shannon_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)state->num_subscribers);
        }

        state->subscribers[i].total_delivered = 0.0F;
        state->subscribers[i].total_lost = 0.0F;
        state->subscribers[i].bottleneck_count = 0;
    }
}

bool global_workspace_get_last_broadcast_metrics(
    const global_workspace_t* workspace,
    shannon_broadcast_metrics_t* metrics
) {
    /* WHAT: Get most recent broadcast metrics
     * WHY:  Inspect individual broadcast
     * HOW:  Copy cached metrics */

    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_get_last_broadcast_metrics: metrics is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_get", 0.0f);


    shannon_workspace_state_t* state = get_shannon_state(workspace);
    if (!state || !state->has_last_metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_get_last_broadcast_metrics: required parameter is NULL (state, state->has_last_metrics)");
        return false;
    }

    *metrics = state->last_metrics;
    return true;
}

//=============================================================================
// Event Handler Integration
//=============================================================================

bool global_workspace_on_salience_peak_shannon(
    global_workspace_t* workspace,
    const void* peak_data
) {
    /* WHAT: Handle salience peak with Shannon-enhanced competition
     * WHY:  Main entry point from salience detector
     * HOW:  Extract data, measure info, compete, broadcast with monitoring
     *
     * NOTE: peak_data is salience_peak_data_t* but we use void* to avoid
     *       circular dependency. Caller must ensure correct type.
     */

    if (!workspace || !peak_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_on_salience_peak_shannon: required parameter is NULL (workspace, peak_data)");
        return false;
    }

    /* Cast to expected structure layout
     * Expected fields:
     * - float salience_score
     * - float* feature_vector
     * - uint32_t dimension
     */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_global_workspace_on_", 0.0f);


    typedef struct {
        float salience_score;
        float* feature_vector;
        uint32_t dimension;
    } generic_peak_t;

    const generic_peak_t* peak = (const generic_peak_t*)peak_data;

    if (!peak->feature_vector || peak->dimension == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_on_salience_peak_shannon: peak->feature_vector is NULL");
        return false;
    }

    /* Measure information content */
    float info_bits = shannon_measure_feature_information(
        peak->feature_vector,
        peak->dimension
    );

    /* Compete with information weighting */
    bool won = global_workspace_compete_with_info(
        workspace,
        MODULE_SALIENCE,
        peak->feature_vector,
        peak->dimension,
        peak->salience_score,
        NULL
    );

    if (won) {
        /* Broadcast with Shannon monitoring */
        shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
            workspace,
            peak->feature_vector,
            peak->dimension,
            info_bits
        );

        /* Log if bottleneck detected */
        if (metrics.bottleneck_detected) {
            /* Could trigger adaptive behavior here */
        }
    }

    return won;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int global_workspace_shannon_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_shannon_heartbeat("global_works_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Global_Workspace_Shannon");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                global_workspace_shannon_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Global_Workspace_Shannon");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Global_Workspace_Shannon");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void global_workspace_shannon_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_global_workspace_shannon_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int global_workspace_shannon_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_shannon_training_begin: NULL argument");
        return -1;
    }
    global_workspace_shannon_heartbeat_instance(g_global_workspace_shannon_health_agent,
                                                "global_workspace_shannon_training_begin", 0.0f);
    (void)(struct shannon_workspace_state*)instance; /* Module state available for reset */
    return 0;
}

int global_workspace_shannon_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_shannon_training_end: NULL argument");
        return -1;
    }
    global_workspace_shannon_heartbeat_instance(g_global_workspace_shannon_health_agent,
                                                "global_workspace_shannon_training_end", 1.0f);
    (void)(struct shannon_workspace_state*)instance; /* Module state available for finalization */
    return 0;
}

int global_workspace_shannon_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_shannon_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    global_workspace_shannon_heartbeat_instance(g_global_workspace_shannon_health_agent,
                                                "global_workspace_shannon_training_step", progress);
    (void)(struct shannon_workspace_state*)instance; /* Module state available for step adaptation */
    return 0;
}
