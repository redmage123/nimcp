//=============================================================================
// nimcp_pr_sleep_bridge.c - Prime Resonant Sleep-Wake Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_sleep_bridge.c
 * @brief Implementation of sleep stage-based memory consolidation
 *
 * See nimcp_pr_sleep_bridge.h for detailed documentation.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_pr_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Platform layer for thread safety
#ifdef NIMCP_HAS_THREADS
#include "platform/thread/nimcp_mutex.h"
#endif

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_sleep_bridge_mesh_registry = NULL;

nimcp_error_t pr_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_sleep_bridge_mesh_registry = registry;
    return err;
}

void pr_sleep_bridge_mesh_unregister(void) {
    if (g_pr_sleep_bridge_mesh_registry && g_pr_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_sleep_bridge_mesh_registry, g_pr_sleep_bridge_mesh_id);
        g_pr_sleep_bridge_mesh_id = 0;
        g_pr_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_sleep_bridge module (instance-level) */
static inline void pr_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_SLEEP_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Constants
//=============================================================================

/** Magic number for validation */
#define PR_SLEEP_MAGIC              0x534C4550U  // "SLEP"

/** Minimum stage time before transition (simulated ms) */
#define PR_SLEEP_MIN_STAGE_TIME_MS  1000

/** Maximum replay history default */
#define PR_SLEEP_MAX_HISTORY_DEFAULT 1000

/** Typical NREM-REM cycle count before wake */
#define PR_SLEEP_CYCLES_BEFORE_WAKE 4

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal sleep bridge structure
 */
struct pr_sleep_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Validation magic */
    uint32_t magic;

    /** Configuration */
    pr_sleep_config_t config;

    /** Current state */
    pr_sleep_stage_t current_stage;
    pr_sleep_stage_state_t stage_states[PR_SLEEP_STAGE_COUNT];
    uint32_t nrem_rem_cycle_count;  /**< Cycles since sleep onset */
    uint64_t sleep_onset_time_ms;   /**< When entered sleep */

    /** External references */
    z_ladder_t ladder;
    entangle_graph_t graph;

    /** Replay buffer */
    pr_replay_candidate_t* replay_buffer;
    size_t replay_buffer_count;
    size_t replay_buffer_capacity;

    /** Replay history */
    pr_replay_event_t* replay_history;
    size_t replay_history_count;
    size_t replay_history_capacity;
    size_t replay_history_head;  /**< Circular buffer head */

    /** Statistics */
    pr_sleep_stats_t stats;

    /** Callbacks */
    pr_replay_callback_t replay_callback;
    void* replay_callback_data;
    pr_stage_callback_t stage_callback;
    void* stage_callback_data;
    pr_sleep_promotion_callback_t promotion_callback;
    void* promotion_callback_data;

    /** Thread safety */
#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_t* mutex;
#endif

    /** Timestamps */
    uint64_t last_consolidation_time_ms;
    uint64_t creation_time_ms;

    /** Instance-level health agent (B25 Upgrade) */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_sleep_bridge, struct pr_sleep_bridge_struct)

//=============================================================================
// Static Function Declarations
//=============================================================================

static uint64_t get_current_time_ms(void);
static float compute_replay_priority(
    const pr_sleep_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_sleep_stage_t stage
);
static pr_memory_type_t infer_memory_type(const pr_memory_node_t* node);
static int compare_replay_priority(const void* a, const void* b);
static void record_replay_event(
    pr_sleep_bridge_t bridge,
    const pr_replay_event_t* event
);
static float get_stage_promotion_boost(pr_sleep_stage_t stage, const pr_sleep_config_t* config);
static int consolidate_wake(pr_sleep_bridge_t bridge);
static int consolidate_n1(pr_sleep_bridge_t bridge);
static int consolidate_n2(pr_sleep_bridge_t bridge);
static int consolidate_n3_sws(pr_sleep_bridge_t bridge);
static int consolidate_rem(pr_sleep_bridge_t bridge);

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_sleep_config_t pr_sleep_config_default(void) {
    pr_sleep_config_t config;
    memset(&config, 0, sizeof(config));

    // Replay configuration
    config.replay_buffer_capacity = PR_SLEEP_DEFAULT_REPLAY_BUFFER;
    config.max_replay_per_cycle = PR_SLEEP_MAX_REPLAY_PER_CYCLE;
    config.replay_compression = PR_SLEEP_REPLAY_COMPRESSION;
    config.min_replay_strength = PR_SLEEP_MIN_REPLAY_STRENGTH;

    // Stage-specific parameters
    config.sws_promotion_boost = PR_SLEEP_SWS_PROMOTION_BOOST;
    config.sws_consolidation_factor = 1.5f;
    config.rem_emotional_factor = PR_SLEEP_REM_EMOTIONAL_FACTOR;
    config.rem_prune_threshold = PR_SLEEP_REM_PRUNE_THRESHOLD;

    // Cycles per stage (default values)
    config.cycles_per_stage[PR_SLEEP_STAGE_WAKE] = 1;
    config.cycles_per_stage[PR_SLEEP_STAGE_N1] = 3;
    config.cycles_per_stage[PR_SLEEP_STAGE_N2] = 5;
    config.cycles_per_stage[PR_SLEEP_STAGE_N3] = PR_SLEEP_DEFAULT_CYCLES;
    config.cycles_per_stage[PR_SLEEP_STAGE_REM] = 7;

    // Consolidation parameters
    config.consolidation_delta = PR_SLEEP_CONSOLIDATION_DELTA;
    config.association_strengthen_delta = PR_SLEEP_ASSOC_STRENGTHEN_DELTA;
    config.wake_decay_rate = PR_SLEEP_WAKE_DECAY_RATE;
    config.enable_reverse_replay = true;
    config.enable_random_replay = true;

    // Integration parameters
    config.enable_theta_gamma_sync = false;
    config.enable_entanglement_update = true;
    config.track_replay_history = true;
    config.max_replay_history = PR_SLEEP_MAX_HISTORY_DEFAULT;

    return config;
}

NIMCP_EXPORT bool pr_sleep_config_validate(const pr_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_config_validate: config is NULL");
        return false;
    }

    // Check replay configuration
    if (config->replay_buffer_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: config->replay_buffer_capacity is zero");
        return false;
    }
    if (config->max_replay_per_cycle == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: config->max_replay_per_cycle is zero");
        return false;
    }
    if (config->replay_compression <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }

    // Check factors are in valid range
    if (config->min_replay_strength < 0.0f || config->min_replay_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }
    if (config->sws_promotion_boost < 0.0f || config->sws_promotion_boost > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }
    if (config->rem_emotional_factor < 0.0f || config->rem_emotional_factor > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }
    if (config->rem_prune_threshold < 0.0f || config->rem_prune_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }

    // Check deltas are positive
    if (config->consolidation_delta <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }
    if (config->association_strengthen_delta <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_config_validate: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_sleep_bridge_t pr_sleep_bridge_create(const pr_sleep_config_t* config) {
    pr_sleep_config_t cfg;

    if (config) {
        if (!pr_sleep_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_sleep_bridge_create: pr_sleep_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_sleep_config_default();
    }

    // Allocate bridge structure
    pr_sleep_bridge_t bridge = (pr_sleep_bridge_t)nimcp_calloc(1, sizeof(struct pr_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->magic = PR_SLEEP_MAGIC;
    bridge->config = cfg;
    bridge->current_stage = PR_SLEEP_STAGE_WAKE;
    bridge->creation_time_ms = get_current_time_ms();

    // Allocate replay buffer
    bridge->replay_buffer_capacity = cfg.replay_buffer_capacity;
    bridge->replay_buffer = (pr_replay_candidate_t*)nimcp_calloc(
        bridge->replay_buffer_capacity, sizeof(pr_replay_candidate_t));
    if (!bridge->replay_buffer) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_sleep_bridge_create: bridge->replay_buffer is NULL");
        return NULL;
    }
    bridge->replay_buffer_count = 0;

    // Allocate replay history if enabled
    if (cfg.track_replay_history && cfg.max_replay_history > 0) {
        bridge->replay_history_capacity = cfg.max_replay_history;
        bridge->replay_history = (pr_replay_event_t*)nimcp_calloc(
            bridge->replay_history_capacity, sizeof(pr_replay_event_t));
        if (!bridge->replay_history) {
            nimcp_free(bridge->replay_buffer);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_sleep_bridge_create: bridge->replay_history is NULL");
            return NULL;
        }
    }

    // Initialize stage states
    for (int i = 0; i < PR_SLEEP_STAGE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_SLEEP_STAGE_COUNT > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)PR_SLEEP_STAGE_COUNT);
        }

        bridge->stage_states[i].stage = (pr_sleep_stage_t)i;
        bridge->stage_states[i].total_time_ms = 0;
    }

    // Set initial stage state
    bridge->stage_states[PR_SLEEP_STAGE_WAKE].entry_time_ms = bridge->creation_time_ms;

    // Initialize mutex if threading is available
#ifdef NIMCP_HAS_THREADS
    if (bridge_base_init(&bridge->base, 0, "pr_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge->replay_history);
        nimcp_free(bridge->replay_buffer);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }
#endif

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_sleep");
    return bridge;
}

NIMCP_EXPORT void pr_sleep_bridge_destroy(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_sleep");
    }

    // Invalidate magic
    bridge->magic = 0;

#ifdef NIMCP_HAS_THREADS
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
#endif

    if (bridge->replay_history) {
        nimcp_free(bridge->replay_history);
    }

    if (bridge->replay_buffer) {
        nimcp_free(bridge->replay_buffer);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_ladder(
    pr_sleep_bridge_t bridge,
    z_ladder_t ladder
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    bridge->ladder = ladder;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_entanglement(
    pr_sleep_bridge_t bridge,
    entangle_graph_t graph
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    bridge->graph = graph;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_reset(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    // Reset to wake state
    bridge->current_stage = PR_SLEEP_STAGE_WAKE;
    bridge->nrem_rem_cycle_count = 0;
    bridge->sleep_onset_time_ms = 0;

    // Clear stage states
    for (int i = 0; i < PR_SLEEP_STAGE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_SLEEP_STAGE_COUNT > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)PR_SLEEP_STAGE_COUNT);
        }

        memset(&bridge->stage_states[i], 0, sizeof(pr_sleep_stage_state_t));
        bridge->stage_states[i].stage = (pr_sleep_stage_t)i;
    }

    // Clear replay buffer
    bridge->replay_buffer_count = 0;

    // Clear replay history
    bridge->replay_history_count = 0;
    bridge->replay_history_head = 0;

    // Clear statistics
    memset(&bridge->stats, 0, sizeof(pr_sleep_stats_t));

    // Reset timestamps
    uint64_t now = get_current_time_ms();
    bridge->stage_states[PR_SLEEP_STAGE_WAKE].entry_time_ms = now;
    bridge->last_consolidation_time_ms = now;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

//=============================================================================
// Stage Management Functions
//=============================================================================

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_stage(
    pr_sleep_bridge_t bridge,
    pr_sleep_stage_t stage
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    if (stage >= PR_SLEEP_STAGE_COUNT) {
        return PR_SLEEP_ERROR_INVALID_STAGE;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    pr_sleep_stage_t old_stage = bridge->current_stage;
    uint64_t now = get_current_time_ms();

    // Update old stage's total time
    uint64_t stage_duration = now - bridge->stage_states[old_stage].entry_time_ms;
    bridge->stage_states[old_stage].total_time_ms += stage_duration;
    bridge->stats.time_per_stage_ms[old_stage] += stage_duration;

    // Transition to new stage
    bridge->current_stage = stage;
    bridge->stage_states[stage].entry_time_ms = now;
    bridge->stage_states[stage].consolidation_cycles = 0;
    bridge->stage_states[stage].memories_replayed = 0;
    bridge->stage_states[stage].promotions = 0;
    bridge->stats.entries_per_stage[stage]++;

    // Set dominant frequency based on stage
    switch (stage) {
        case PR_SLEEP_STAGE_WAKE:
            bridge->stage_states[stage].dominant_frequency = 10.0f;  // Alpha
            break;
        case PR_SLEEP_STAGE_N1:
            bridge->stage_states[stage].dominant_frequency = 6.0f;   // Theta
            break;
        case PR_SLEEP_STAGE_N2:
            bridge->stage_states[stage].dominant_frequency = PR_SLEEP_SPINDLE_FREQ;
            break;
        case PR_SLEEP_STAGE_N3:
            bridge->stage_states[stage].dominant_frequency = PR_SLEEP_SLOW_WAVE_FREQ;
            break;
        case PR_SLEEP_STAGE_REM:
            bridge->stage_states[stage].dominant_frequency = 6.0f;   // Theta
            break;
        default:
            bridge->stage_states[stage].dominant_frequency = 0.0f;
            break;
    }

    // Track sleep onset
    if (old_stage == PR_SLEEP_STAGE_WAKE && stage != PR_SLEEP_STAGE_WAKE) {
        bridge->sleep_onset_time_ms = now;
        bridge->nrem_rem_cycle_count = 0;
    }

    // Track wake time
    if (stage == PR_SLEEP_STAGE_WAKE && old_stage != PR_SLEEP_STAGE_WAKE) {
        bridge->stats.last_wake_time_ms = now;
    }

    // Track NREM-REM cycle completion
    if (old_stage == PR_SLEEP_STAGE_REM && stage != PR_SLEEP_STAGE_WAKE) {
        bridge->nrem_rem_cycle_count++;
        bridge->stats.total_sleep_cycles++;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    // Invoke callback (outside lock)
    if (bridge->stage_callback) {
        bridge->stage_callback(old_stage, stage, bridge->stage_callback_data);
    }

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_stage_t pr_sleep_bridge_get_stage(const pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_STAGE_WAKE;
    }
    return bridge->current_stage;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_stage_state(
    const pr_sleep_bridge_t bridge,
    pr_sleep_stage_state_t* state
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !state) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(((pr_sleep_bridge_t)bridge)->mutex);
#endif

    *state = bridge->stage_states[bridge->current_stage];

    // Update current stage time
    uint64_t now = get_current_time_ms();
    state->total_time_ms = bridge->stage_states[bridge->current_stage].total_time_ms +
                           (now - state->entry_time_ms);

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(((pr_sleep_bridge_t)bridge)->mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_stage_t pr_sleep_bridge_advance_stage(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_STAGE_WAKE;
    }

    pr_sleep_stage_t current = bridge->current_stage;
    pr_sleep_stage_t next;

    switch (current) {
        case PR_SLEEP_STAGE_WAKE:
            next = PR_SLEEP_STAGE_N1;
            break;
        case PR_SLEEP_STAGE_N1:
            next = PR_SLEEP_STAGE_N2;
            break;
        case PR_SLEEP_STAGE_N2:
            next = PR_SLEEP_STAGE_N3;
            break;
        case PR_SLEEP_STAGE_N3:
            // First time to REM, then cycle between N2-N3-REM
            next = PR_SLEEP_STAGE_REM;
            break;
        case PR_SLEEP_STAGE_REM:
            // After several cycles, wake up
            if (bridge->nrem_rem_cycle_count >= PR_SLEEP_CYCLES_BEFORE_WAKE - 1) {
                next = PR_SLEEP_STAGE_WAKE;
            } else {
                next = PR_SLEEP_STAGE_N2;  // Go back to N2, not N1
            }
            break;
        default:
            next = PR_SLEEP_STAGE_WAKE;
            break;
    }

    pr_sleep_bridge_set_stage(bridge, next);
    return next;
}

NIMCP_EXPORT bool pr_sleep_bridge_is_sleeping(const pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_is_sleeping: bridge is NULL");
        return false;
    }
    return bridge->current_stage != PR_SLEEP_STAGE_WAKE;
}

NIMCP_EXPORT bool pr_sleep_bridge_is_deep_sleep(const pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_is_deep_sleep: bridge is NULL");
        return false;
    }
    return bridge->current_stage == PR_SLEEP_STAGE_N3;
}

//=============================================================================
// Consolidation Functions
//=============================================================================

NIMCP_EXPORT int pr_sleep_bridge_consolidate(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_consolidate: bridge is NULL");
        return -1;
    }

    if (!bridge->ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_bridge_consolidate: bridge->ladder is NULL");
        return -1;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    int processed = 0;

    switch (bridge->current_stage) {
        case PR_SLEEP_STAGE_WAKE:
            processed = consolidate_wake(bridge);
            break;
        case PR_SLEEP_STAGE_N1:
            processed = consolidate_n1(bridge);
            break;
        case PR_SLEEP_STAGE_N2:
            processed = consolidate_n2(bridge);
            break;
        case PR_SLEEP_STAGE_N3:
            processed = consolidate_n3_sws(bridge);
            break;
        case PR_SLEEP_STAGE_REM:
            processed = consolidate_rem(bridge);
            break;
        default:
            processed = 0;
            break;
    }

    // Update statistics
    bridge->stage_states[bridge->current_stage].consolidation_cycles++;
    bridge->stats.cycles_per_stage[bridge->current_stage]++;
    bridge->last_consolidation_time_ms = get_current_time_ms();

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return processed;
}

NIMCP_EXPORT int pr_sleep_bridge_consolidate_cycles(
    pr_sleep_bridge_t bridge,
    uint32_t num_cycles
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_consolidate_cycles: bridge is NULL");
        return -1;
    }

    if (num_cycles == 0) {
        num_cycles = bridge->config.cycles_per_stage[bridge->current_stage];
    }

    int total_processed = 0;
    for (uint32_t i = 0; i < num_cycles; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_cycles > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)num_cycles);
        }

        int processed = pr_sleep_bridge_consolidate(bridge);
        if (processed < 0) {
            return total_processed > 0 ? total_processed : -1;
        }
        total_processed += processed;
    }

    return total_processed;
}

NIMCP_EXPORT int pr_sleep_bridge_run_sleep_cycles(
    pr_sleep_bridge_t bridge,
    uint32_t num_cycles
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_run_sleep_cycles: bridge is NULL");
        return -1;
    }

    int total_processed = 0;

    for (uint32_t cycle = 0; cycle < num_cycles; cycle++) {
        /* Phase 8: Loop progress heartbeat */
        if ((cycle & 0xFF) == 0 && num_cycles > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(cycle + 1) / (float)num_cycles);
        }

        // N1
        pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N1);
        total_processed += pr_sleep_bridge_consolidate_cycles(bridge, 0);

        // N2
        pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N2);
        total_processed += pr_sleep_bridge_consolidate_cycles(bridge, 0);

        // N3 (SWS) - most consolidation happens here
        pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_N3);
        total_processed += pr_sleep_bridge_consolidate_cycles(bridge, 0);

        // REM
        pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_REM);
        total_processed += pr_sleep_bridge_consolidate_cycles(bridge, 0);
    }

    // Return to wake
    pr_sleep_bridge_set_stage(bridge, PR_SLEEP_STAGE_WAKE);

    return total_processed;
}

//=============================================================================
// Memory Replay Functions
//=============================================================================

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_replay_candidates(
    pr_sleep_bridge_t bridge,
    pr_replay_candidate_t* candidates,
    size_t max_candidates,
    size_t* count
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !candidates || !count) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    if (!bridge->ladder) {
        return PR_SLEEP_ERROR_NO_LADDER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    *count = 0;
    size_t candidate_count = 0;
    uint64_t current_time = get_current_time_ms();

    // Clear internal buffer
    bridge->replay_buffer_count = 0;

    // Iterate through Z0 and Z1 tiers (primary replay candidates)
    for (int tier = PR_MEMORY_TIER_Z0; tier <= PR_MEMORY_TIER_Z1; tier++) {
        pr_memory_node_t* nodes[256];
        size_t node_count = 0;

        z_ladder_error_t err = z_ladder_get_nodes(
            bridge->ladder,
            (pr_memory_tier_t)tier,
            nodes,
            256,
            &node_count
        );

        if (err != Z_LADDER_SUCCESS) {
            continue;
        }

        for (size_t i = 0; i < node_count && candidate_count < max_candidates; i++) {
            pr_memory_node_t* node = nodes[i];
            if (!node) continue;

            // Check minimum strength
            if (node->current_strength < bridge->config.min_replay_strength) {
                continue;
            }

            // Compute replay priority
            float priority = compute_replay_priority(bridge, node, bridge->current_stage);
            if (priority <= 0.0f) {
                continue;
            }

            // Fill candidate structure
            pr_replay_candidate_t* cand = &candidates[candidate_count];
            cand->node_id = node->node_id;
            cand->tier = node->tier;
            cand->strength = node->current_strength;

            nimcp_quaternion_t state = pr_memory_node_get_state(node);
            cand->emotional_magnitude = fabsf(state.x);
            cand->salience = state.y;
            cand->entanglement_count = pr_memory_node_get_entanglement_count(node);
            cand->age_ms = current_time - node->created_time_ms;
            cand->replay_priority = priority;
            cand->type = infer_memory_type(node);
            cand->is_novel = (cand->age_ms < 24 * 60 * 60 * 1000);  // < 24 hours

            candidate_count++;
        }
    }

    // Sort by replay priority (descending)
    if (candidate_count > 1) {
        qsort(candidates, candidate_count, sizeof(pr_replay_candidate_t),
              compare_replay_priority);
    }

    *count = candidate_count;

    // Copy to internal buffer
    size_t to_copy = candidate_count < bridge->replay_buffer_capacity ?
                     candidate_count : bridge->replay_buffer_capacity;
    memcpy(bridge->replay_buffer, candidates, to_copy * sizeof(pr_replay_candidate_t));
    bridge->replay_buffer_count = to_copy;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_replay(
    pr_sleep_bridge_t bridge,
    uint64_t node_id,
    pr_replay_direction_t direction,
    pr_replay_event_t* event
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    if (!bridge->ladder) {
        return PR_SLEEP_ERROR_NO_LADDER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    // Find the node
    pr_memory_node_t* node = z_ladder_find(bridge->ladder, node_id);
    if (!node) {
#ifdef NIMCP_HAS_THREADS
        nimcp_mutex_unlock(bridge->base.mutex);
#endif
        return PR_SLEEP_ERROR_REPLAY_FAILED;
    }

    // Record pre-replay state
    pr_replay_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.node_id = node_id;
    ev.stage = bridge->current_stage;
    ev.direction = direction;
    ev.strength_before = node->current_strength;
    ev.tier_before = node->tier;
    ev.timestamp_ms = get_current_time_ms();

    // Apply consolidation based on stage
    float consolidation_boost = bridge->config.consolidation_delta;
    if (bridge->current_stage == PR_SLEEP_STAGE_N3) {
        consolidation_boost *= bridge->config.sws_consolidation_factor;
    }

    // Reinforce memory
    float new_strength = pr_memory_node_reinforce(node, consolidation_boost);
    ev.strength_after = new_strength;
    ev.consolidation_gained = new_strength - ev.strength_before;

    // Update quaternion consolidation component
    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    state.w = nimcp_myelin_clamp(state.w + consolidation_boost * 0.5f, 0.0f, 1.0f);
    pr_memory_node_update_state(node, state);

    // Check for promotion eligibility
    float promotion_boost = get_stage_promotion_boost(bridge->current_stage, &bridge->config);
    float eligibility = node->promotion_eligibility + promotion_boost;
    node->promotion_eligibility = nimcp_myelin_clamp(eligibility, 0.0f, 1.0f);

    ev.was_promoted = false;
    ev.tier_after = node->tier;

    // Attempt promotion if eligible
    if (pr_memory_node_check_eligibility(node) && node->tier < PR_MEMORY_TIER_Z3) {
        pr_memory_tier_t old_tier = node->tier;
        z_ladder_error_t promote_err = z_ladder_promote(bridge->ladder, node_id);
        if (promote_err == Z_LADDER_SUCCESS) {
            ev.was_promoted = true;
            ev.tier_after = node->tier;
            bridge->stats.total_promotions++;
            bridge->stage_states[bridge->current_stage].promotions++;

            // Invoke promotion callback
            if (bridge->promotion_callback) {
#ifdef NIMCP_HAS_THREADS
                nimcp_mutex_unlock(bridge->base.mutex);
#endif
                bridge->promotion_callback(
                    node_id, old_tier, ev.tier_after,
                    bridge->current_stage, bridge->promotion_callback_data);
#ifdef NIMCP_HAS_THREADS
                nimcp_mutex_lock(bridge->base.mutex);
#endif
            }
        }
    }

    // Update statistics
    bridge->stats.total_replays++;
    bridge->stage_states[bridge->current_stage].memories_replayed++;

    switch (direction) {
        case PR_REPLAY_FORWARD:
            bridge->stats.forward_replays++;
            break;
        case PR_REPLAY_REVERSE:
            bridge->stats.reverse_replays++;
            break;
        case PR_REPLAY_RANDOM:
            bridge->stats.random_replays++;
            break;
    }

    // Record event in history
    record_replay_event(bridge, &ev);

    // Copy event to output if requested
    if (event) {
        *event = ev;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    // Invoke replay callback (outside lock)
    if (bridge->replay_callback) {
        bridge->replay_callback(&ev, bridge->replay_callback_data);
    }

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT int pr_sleep_bridge_replay_sequence(
    pr_sleep_bridge_t bridge,
    const uint64_t* node_ids,
    size_t count,
    pr_replay_direction_t direction
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !node_ids || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_replay_sequence: required parameter is NULL (bridge, node_ids)");
        return -1;
    }

    int successful_replays = 0;

    // Determine iteration order based on direction
    if (direction == PR_REPLAY_FORWARD) {
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                                 (float)(i + 1) / (float)count);
            }

            pr_replay_event_t event;
            event.sequence_position = (uint32_t)i;
            if (pr_sleep_bridge_replay(bridge, node_ids[i], direction, &event) == PR_SLEEP_SUCCESS) {
                successful_replays++;
            }
        }
    } else if (direction == PR_REPLAY_REVERSE) {
        for (size_t i = count; i > 0; i--) {
            pr_replay_event_t event;
            event.sequence_position = (uint32_t)(count - i);
            if (pr_sleep_bridge_replay(bridge, node_ids[i - 1], direction, &event) == PR_SLEEP_SUCCESS) {
                successful_replays++;
            }
        }
    } else {
        // Random order - use simple Fisher-Yates shuffle
        uint64_t* shuffled = (uint64_t*)nimcp_calloc(count, sizeof(uint64_t));
        if (!shuffled) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate shuffled");

            return -1;
        }
        memcpy(shuffled, node_ids, count * sizeof(uint64_t));

        for (size_t i = count - 1; i > 0; i--) {
            size_t j = nimcp_tl_rand() % (i + 1);
            uint64_t temp = shuffled[i];
            shuffled[i] = shuffled[j];
            shuffled[j] = temp;
        }

        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                                 (float)(i + 1) / (float)count);
            }

            pr_replay_event_t event;
            event.sequence_position = (uint32_t)i;
            if (pr_sleep_bridge_replay(bridge, shuffled[i], direction, &event) == PR_SLEEP_SUCCESS) {
                successful_replays++;
            }
        }

        nimcp_free(shuffled);
        shuffled = NULL;
    }

    // Strengthen associations between co-replayed memories
    if (successful_replays > 1 && bridge->config.enable_entanglement_update) {
        pr_sleep_bridge_strengthen_associations(bridge, node_ids, count);
    }

    return successful_replays;
}

//=============================================================================
// Z-Ladder Promotion Functions
//=============================================================================

NIMCP_EXPORT int pr_sleep_bridge_promote_z_ladder(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_promote_z_ladder: bridge is NULL");
        return -1;
    }

    if (!bridge->ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_bridge_promote_z_ladder: bridge->ladder is NULL");
        return -1;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    int promoted = 0;
    float promotion_boost = get_stage_promotion_boost(bridge->current_stage, &bridge->config);

    // Check Z0 and Z1 for promotion candidates
    for (int tier = PR_MEMORY_TIER_Z0; tier <= PR_MEMORY_TIER_Z2; tier++) {
        pr_memory_node_t* nodes[256];
        size_t node_count = 0;

        z_ladder_error_t err = z_ladder_get_nodes(
            bridge->ladder,
            (pr_memory_tier_t)tier,
            nodes,
            256,
            &node_count
        );

        if (err != Z_LADDER_SUCCESS) {
            continue;
        }

        for (size_t i = 0; i < node_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && node_count > 256) {
                pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                                 (float)(i + 1) / (float)node_count);
            }

            pr_memory_node_t* node = nodes[i];
            if (!node) continue;

            // Apply sleep boost to eligibility
            float boosted_eligibility = node->promotion_eligibility + promotion_boost;

            // Check if eligible with boost
            bool eligible = (boosted_eligibility >= PR_NODE_PROMOTION_THRESHOLD - promotion_boost) &&
                           (node->current_strength >= 0.5f - promotion_boost * 0.5f);

            if (eligible) {
                pr_memory_tier_t old_tier = node->tier;
                z_ladder_error_t promote_err = z_ladder_promote(bridge->ladder, node->node_id);
                if (promote_err == Z_LADDER_SUCCESS) {
                    promoted++;
                    bridge->stats.total_promotions++;
                    bridge->stage_states[bridge->current_stage].promotions++;

                    if (bridge->promotion_callback) {
#ifdef NIMCP_HAS_THREADS
                        nimcp_mutex_unlock(bridge->base.mutex);
#endif
                        bridge->promotion_callback(
                            node->node_id, old_tier, node->tier,
                            bridge->current_stage, bridge->promotion_callback_data);
#ifdef NIMCP_HAS_THREADS
                        nimcp_mutex_lock(bridge->base.mutex);
#endif
                    }
                }
            }
        }
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return promoted;
}

NIMCP_EXPORT bool pr_sleep_bridge_promote_memory(
    pr_sleep_bridge_t bridge,
    uint64_t node_id
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_promote_memory: bridge is NULL");
        return false;
    }

    if (!bridge->ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_bridge_promote_memory: bridge->ladder is NULL");
        return false;
    }

    z_ladder_error_t err = z_ladder_promote(bridge->ladder, node_id);
    if (err == Z_LADDER_SUCCESS) {
        bridge->stats.total_promotions++;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_promote_memory: validation failed");
    return false;
}

NIMCP_EXPORT float pr_sleep_bridge_get_promotion_eligibility(
    pr_sleep_bridge_t bridge,
    uint64_t node_id
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return -1.0f;
    }

    if (!bridge->ladder) {
        return -1.0f;
    }

    pr_memory_node_t* node = z_ladder_find(bridge->ladder, node_id);
    if (!node) {
        return -1.0f;
    }

    float boost = get_stage_promotion_boost(bridge->current_stage, &bridge->config);
    return nimcp_myelin_clamp(node->promotion_eligibility + boost, 0.0f, 1.0f);
}

//=============================================================================
// Emotional Processing Functions (REM)
//=============================================================================

NIMCP_EXPORT int pr_sleep_bridge_emotional_process(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_emotional_process: bridge is NULL");
        return -1;
    }

    // Only process during REM
    if (bridge->current_stage != PR_SLEEP_STAGE_REM) {
        return 0;
    }

    if (!bridge->ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_bridge_emotional_process: bridge->ladder is NULL");
        return -1;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    int processed = 0;
    float total_reduction = 0.0f;

    // Iterate through all tiers looking for emotional memories
    for (int tier = PR_MEMORY_TIER_Z0; tier < PR_MEMORY_TIER_COUNT; tier++) {
        pr_memory_node_t* nodes[256];
        size_t node_count = 0;

        z_ladder_error_t err = z_ladder_get_nodes(
            bridge->ladder,
            (pr_memory_tier_t)tier,
            nodes,
            256,
            &node_count
        );

        if (err != Z_LADDER_SUCCESS) {
            continue;
        }

        for (size_t i = 0; i < node_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && node_count > 256) {
                pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                                 (float)(i + 1) / (float)node_count);
            }

            pr_memory_node_t* node = nodes[i];
            if (!node) continue;

            nimcp_quaternion_t state = pr_memory_node_get_state(node);
            float emotional_mag = fabsf(state.x);

            // Process if above threshold
            if (emotional_mag >= PR_SLEEP_MIN_EMOTIONAL_MAG) {
                // Reduce emotional magnitude while preserving sign
                float reduction = emotional_mag * bridge->config.rem_emotional_factor;
                float new_emotion = state.x * (1.0f - bridge->config.rem_emotional_factor);
                state.x = new_emotion;

                pr_memory_node_update_state(node, state);

                processed++;
                total_reduction += reduction;
            }
        }
    }

    // Update statistics
    bridge->stats.emotional_memories_processed += processed;
    if (processed > 0) {
        bridge->stats.avg_emotional_reduction =
            (bridge->stats.avg_emotional_reduction * (bridge->stats.emotional_memories_processed - processed) +
             total_reduction) / bridge->stats.emotional_memories_processed;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return processed;
}

NIMCP_EXPORT float pr_sleep_bridge_process_emotional_memory(
    pr_sleep_bridge_t bridge,
    uint64_t node_id
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return -1.0f;
    }

    if (!bridge->ladder) {
        return -1.0f;
    }

    pr_memory_node_t* node = z_ladder_find(bridge->ladder, node_id);
    if (!node) {
        return -1.0f;
    }

    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    float new_emotion = state.x * (1.0f - bridge->config.rem_emotional_factor);
    state.x = new_emotion;

    pr_memory_node_update_state(node, state);

    return fabsf(new_emotion);
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_emotional_memories(
    pr_sleep_bridge_t bridge,
    pr_replay_candidate_t* candidates,
    size_t max_candidates,
    size_t* count
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !candidates || !count) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    if (!bridge->ladder) {
        return PR_SLEEP_ERROR_NO_LADDER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    *count = 0;
    size_t candidate_count = 0;
    uint64_t current_time = get_current_time_ms();

    // Iterate through all tiers
    for (int tier = PR_MEMORY_TIER_Z0; tier < PR_MEMORY_TIER_COUNT && candidate_count < max_candidates; tier++) {
        pr_memory_node_t* nodes[256];
        size_t node_count = 0;

        z_ladder_error_t err = z_ladder_get_nodes(
            bridge->ladder,
            (pr_memory_tier_t)tier,
            nodes,
            256,
            &node_count
        );

        if (err != Z_LADDER_SUCCESS) {
            continue;
        }

        for (size_t i = 0; i < node_count && candidate_count < max_candidates; i++) {
            pr_memory_node_t* node = nodes[i];
            if (!node) continue;

            nimcp_quaternion_t state = pr_memory_node_get_state(node);
            float emotional_mag = fabsf(state.x);

            // Only include emotional memories
            if (emotional_mag < PR_SLEEP_MIN_EMOTIONAL_MAG) {
                continue;
            }

            pr_replay_candidate_t* cand = &candidates[candidate_count];
            cand->node_id = node->node_id;
            cand->tier = node->tier;
            cand->strength = node->current_strength;
            cand->emotional_magnitude = emotional_mag;
            cand->salience = state.y;
            cand->entanglement_count = pr_memory_node_get_entanglement_count(node);
            cand->age_ms = current_time - node->created_time_ms;
            cand->replay_priority = emotional_mag * cand->salience;
            cand->type = PR_MEMORY_TYPE_EMOTIONAL;
            cand->is_novel = (cand->age_ms < 24 * 60 * 60 * 1000);

            candidate_count++;
        }
    }

    *count = candidate_count;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

//=============================================================================
// Association Management Functions
//=============================================================================

NIMCP_EXPORT int pr_sleep_bridge_strengthen_associations(
    pr_sleep_bridge_t bridge,
    const uint64_t* node_ids,
    size_t count
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !node_ids || count < 2) {
        return 0;
    }

    if (!bridge->graph || !bridge->config.enable_entanglement_update) {
        return 0;
    }

    int strengthened = 0;

    // Strengthen associations between adjacent pairs in the sequence
    for (size_t i = 0; i < count - 1; i++) {
        float new_weight = entangle_strengthen_edge(
            bridge->graph,
            node_ids[i],
            node_ids[i + 1],
            bridge->config.association_strengthen_delta
        );

        if (new_weight >= 0.0f) {
            strengthened++;
        }
    }

    bridge->stats.associations_strengthened += strengthened;

    return strengthened;
}

NIMCP_EXPORT int pr_sleep_bridge_prune_weak_associations(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return 0;
    }

    // Only prune during REM
    if (bridge->current_stage != PR_SLEEP_STAGE_REM) {
        return 0;
    }

    if (!bridge->graph || !bridge->config.enable_entanglement_update) {
        return 0;
    }

    size_t pruned = entangle_prune_weak(bridge->graph, bridge->config.rem_prune_threshold);
    bridge->stats.associations_pruned += pruned;

    return (int)pruned;
}

//=============================================================================
// Callback Functions
//=============================================================================

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_replay_callback(
    pr_sleep_bridge_t bridge,
    pr_replay_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    bridge->replay_callback = callback;
    bridge->replay_callback_data = user_data;

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_stage_callback(
    pr_sleep_bridge_t bridge,
    pr_stage_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    bridge->stage_callback = callback;
    bridge->stage_callback_data = user_data;

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_set_promotion_callback(
    pr_sleep_bridge_t bridge,
    pr_sleep_promotion_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    bridge->promotion_callback = callback;
    bridge->promotion_callback_data = user_data;

    return PR_SLEEP_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_stats(
    const pr_sleep_bridge_t bridge,
    pr_sleep_stats_t* stats
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !stats) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(((pr_sleep_bridge_t)bridge)->mutex);
#endif

    *stats = bridge->stats;

    // Update current stage time
    uint64_t now = get_current_time_ms();
    uint64_t current_stage_time = now - bridge->stage_states[bridge->current_stage].entry_time_ms;
    stats->time_per_stage_ms[bridge->current_stage] += current_stage_time;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(((pr_sleep_bridge_t)bridge)->mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_reset_stats(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    memset(&bridge->stats, 0, sizeof(pr_sleep_stats_t));

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_get_replay_history(
    const pr_sleep_bridge_t bridge,
    pr_replay_event_t* events,
    size_t max_events,
    size_t* count
) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC || !events || !count) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

    if (!bridge->replay_history || !bridge->config.track_replay_history) {
        *count = 0;
        return PR_SLEEP_SUCCESS;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(((pr_sleep_bridge_t)bridge)->mutex);
#endif

    size_t to_copy = bridge->replay_history_count < max_events ?
                     bridge->replay_history_count : max_events;

    // Copy from circular buffer (most recent first)
    for (size_t i = 0; i < to_copy; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_copy > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)to_copy);
        }

        size_t idx = (bridge->replay_history_head + bridge->replay_history_capacity - 1 - i) %
                     bridge->replay_history_capacity;
        events[i] = bridge->replay_history[idx];
    }

    *count = to_copy;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(((pr_sleep_bridge_t)bridge)->mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

NIMCP_EXPORT pr_sleep_error_t pr_sleep_bridge_clear_replay_history(pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        return PR_SLEEP_ERROR_NULL_POINTER;
    }

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_lock(bridge->base.mutex);
#endif

    bridge->replay_history_count = 0;
    bridge->replay_history_head = 0;

#ifdef NIMCP_HAS_THREADS
    nimcp_mutex_unlock(bridge->base.mutex);
#endif

    return PR_SLEEP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_sleep_error_string(pr_sleep_error_t error) {
    switch (error) {
        case PR_SLEEP_SUCCESS:
            return "Success";
        case PR_SLEEP_ERROR_NULL_POINTER:
            return "Null pointer";
        case PR_SLEEP_ERROR_INVALID_STAGE:
            return "Invalid sleep stage";
        case PR_SLEEP_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case PR_SLEEP_ERROR_NOT_INITIALIZED:
            return "Bridge not initialized";
        case PR_SLEEP_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case PR_SLEEP_ERROR_NO_LADDER:
            return "Z-ladder not set";
        case PR_SLEEP_ERROR_REPLAY_FAILED:
            return "Replay operation failed";
        case PR_SLEEP_ERROR_STAGE_LOCKED:
            return "Stage transition blocked";
        default:
            return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_sleep_stage_name(pr_sleep_stage_t stage) {
    switch (stage) {
        case PR_SLEEP_STAGE_WAKE:
            return "Wake";
        case PR_SLEEP_STAGE_N1:
            return "N1 (Light Sleep)";
        case PR_SLEEP_STAGE_N2:
            return "N2 (Sleep Spindles)";
        case PR_SLEEP_STAGE_N3:
            return "N3 (Slow-Wave Sleep)";
        case PR_SLEEP_STAGE_REM:
            return "REM (Rapid Eye Movement)";
        default:
            return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_replay_direction_name(pr_replay_direction_t direction) {
    switch (direction) {
        case PR_REPLAY_FORWARD:
            return "Forward";
        case PR_REPLAY_REVERSE:
            return "Reverse";
        case PR_REPLAY_RANDOM:
            return "Random";
        default:
            return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_memory_type_name(pr_memory_type_t type) {
    switch (type) {
        case PR_MEMORY_TYPE_DECLARATIVE:
            return "Declarative";
        case PR_MEMORY_TYPE_PROCEDURAL:
            return "Procedural";
        case PR_MEMORY_TYPE_EMOTIONAL:
            return "Emotional";
        case PR_MEMORY_TYPE_SEMANTIC:
            return "Semantic";
        case PR_MEMORY_TYPE_EPISODIC:
            return "Episodic";
        default:
            return "Unknown";
    }
}

NIMCP_EXPORT void pr_sleep_bridge_print_summary(const pr_sleep_bridge_t bridge) {
    if (!bridge || bridge->magic != PR_SLEEP_MAGIC) {
        printf("Sleep Bridge: Invalid\n");
        return;
    }

    printf("=== Sleep Bridge Summary ===\n");
    printf("Current Stage: %s\n", pr_sleep_stage_name(bridge->current_stage));
    printf("NREM-REM Cycles: %u\n", bridge->nrem_rem_cycle_count);
    printf("Total Replays: %lu\n", (unsigned long)bridge->stats.total_replays);
    printf("Total Promotions: %lu\n", (unsigned long)bridge->stats.total_promotions);
    printf("Emotional Processed: %lu\n", (unsigned long)bridge->stats.emotional_memories_processed);
    printf("Associations Strengthened: %lu\n", (unsigned long)bridge->stats.associations_strengthened);
    printf("Associations Pruned: %lu\n", (unsigned long)bridge->stats.associations_pruned);

    printf("\nTime per Stage (ms):\n");
    for (int i = 0; i < PR_SLEEP_STAGE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_SLEEP_STAGE_COUNT > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)PR_SLEEP_STAGE_COUNT);
        }

        printf("  %s: %lu\n", pr_sleep_stage_name((pr_sleep_stage_t)i),
               (unsigned long)bridge->stats.time_per_stage_ms[i]);
    }

    printf("=============================\n");
}

NIMCP_EXPORT uint64_t pr_sleep_current_time_ms(void) {
    return get_current_time_ms();
}

NIMCP_EXPORT bool pr_sleep_bridge_validate(const pr_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_sleep_bridge_validate: bridge is NULL");
        return false;
    }

    if (bridge->magic != PR_SLEEP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_validate: validation failed");
        return false;
    }

    if (bridge->current_stage >= PR_SLEEP_STAGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "pr_sleep_bridge_validate: capacity exceeded");
        return false;
    }

    if (bridge->replay_buffer_count > bridge->replay_buffer_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_validate: validation failed");
        return false;
    }

    if (bridge->config.track_replay_history) {
        if (bridge->replay_history_count > bridge->replay_history_capacity) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_sleep_bridge_validate: validation failed");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Static Helper Functions
//=============================================================================

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static float compute_replay_priority(
    const pr_sleep_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_sleep_stage_t stage
) {
    if (!node) return 0.0f;

    float priority = 0.0f;

    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    float emotional_mag = fabsf(state.x);
    float salience = state.y;
    float consolidation = state.w;

    // Base priority from strength and salience
    priority = node->current_strength * 0.3f + salience * 0.3f;

    // Stage-specific adjustments
    switch (stage) {
        case PR_SLEEP_STAGE_N3:
            // SWS prefers declarative, well-consolidated memories
            if (node->tier <= PR_MEMORY_TIER_Z1) {
                priority += 0.3f;  // Boost recent memories
            }
            priority += consolidation * 0.2f;
            break;

        case PR_SLEEP_STAGE_REM:
            // REM prefers emotional and procedural memories
            priority += emotional_mag * 0.4f;
            break;

        case PR_SLEEP_STAGE_N2:
            // N2 is transitional, moderate all factors
            priority += 0.1f;
            break;

        default:
            break;
    }

    // Boost for entangled memories (well-connected)
    uint32_t entangle_count = pr_memory_node_get_entanglement_count(node);
    if (entangle_count > 0) {
        priority += 0.1f * (1.0f - 1.0f / (1.0f + entangle_count * 0.1f));
    }

    return nimcp_myelin_clamp(priority, 0.0f, 1.0f);
}

static pr_memory_type_t infer_memory_type(const pr_memory_node_t* node) {
    if (!node) return PR_MEMORY_TYPE_DECLARATIVE;

    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    float emotional_mag = fabsf(state.x);

    // Emotional if high emotional magnitude
    if (emotional_mag >= PR_SLEEP_MIN_EMOTIONAL_MAG) {
        return PR_MEMORY_TYPE_EMOTIONAL;
    }

    // Default to declarative/episodic for Z0/Z1, semantic for Z2/Z3
    if (node->tier <= PR_MEMORY_TIER_Z1) {
        return PR_MEMORY_TYPE_EPISODIC;
    } else {
        return PR_MEMORY_TYPE_SEMANTIC;
    }
}

static int compare_replay_priority(const void* a, const void* b) {
    const pr_replay_candidate_t* ca = (const pr_replay_candidate_t*)a;
    const pr_replay_candidate_t* cb = (const pr_replay_candidate_t*)b;

    // Descending order (highest priority first)
    if (ca->replay_priority > cb->replay_priority) return -1;
    if (ca->replay_priority < cb->replay_priority) return 1;
    return 0;
}

static void record_replay_event(
    pr_sleep_bridge_t bridge,
    const pr_replay_event_t* event
) {
    if (!bridge->replay_history || !bridge->config.track_replay_history) {
        return;
    }

    // Circular buffer insertion
    bridge->replay_history[bridge->replay_history_head] = *event;
    bridge->replay_history_head = (bridge->replay_history_head + 1) % bridge->replay_history_capacity;

    if (bridge->replay_history_count < bridge->replay_history_capacity) {
        bridge->replay_history_count++;
    }
}

static float get_stage_promotion_boost(pr_sleep_stage_t stage, const pr_sleep_config_t* config) {
    switch (stage) {
        case PR_SLEEP_STAGE_N3:
            return config->sws_promotion_boost;
        case PR_SLEEP_STAGE_N2:
            return config->sws_promotion_boost * 0.5f;
        case PR_SLEEP_STAGE_REM:
            return config->sws_promotion_boost * 0.3f;
        default:
            return 0.0f;
    }
}

//=============================================================================
// Stage-Specific Consolidation Functions
//=============================================================================

static int consolidate_wake(pr_sleep_bridge_t bridge) {
    // During wake, apply decay but no consolidation replay
    // This simulates normal forgetting during waking hours

    z_ladder_apply_decay(bridge->ladder, bridge->config.wake_decay_rate);

    return 0;
}

static int consolidate_n1(pr_sleep_bridge_t bridge) {
    // N1 is transitional - light stabilization
    // Minimal replay, mainly theta-driven stabilization

    pr_replay_candidate_t candidates[20];
    size_t count = 0;

    pr_sleep_error_t err = pr_sleep_bridge_get_replay_candidates(
        bridge, candidates, 20, &count);

    if (err != PR_SLEEP_SUCCESS || count == 0) {
        return 0;
    }

    // Replay only top few candidates
    size_t to_replay = count < 5 ? count : 5;
    int replayed = 0;

    for (size_t i = 0; i < to_replay; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_replay > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)to_replay);
        }

        if (pr_sleep_bridge_replay(bridge, candidates[i].node_id,
                                   PR_REPLAY_FORWARD, NULL) == PR_SLEEP_SUCCESS) {
            replayed++;
        }
    }

    return replayed;
}

static int consolidate_n2(pr_sleep_bridge_t bridge) {
    // N2 features sleep spindles (12-14Hz) that gate hippocampal-neocortical transfer
    // Moderate replay with spindle-gating simulation

    pr_replay_candidate_t candidates[50];
    size_t count = 0;

    pr_sleep_error_t err = pr_sleep_bridge_get_replay_candidates(
        bridge, candidates, 50, &count);

    if (err != PR_SLEEP_SUCCESS || count == 0) {
        return 0;
    }

    // Replay moderate number of candidates
    size_t to_replay = count < 20 ? count : 20;
    int replayed = 0;

    for (size_t i = 0; i < to_replay; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_replay > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)to_replay);
        }

        if (pr_sleep_bridge_replay(bridge, candidates[i].node_id,
                                   PR_REPLAY_FORWARD, NULL) == PR_SLEEP_SUCCESS) {
            replayed++;
        }
    }

    // Attempt some promotions
    pr_sleep_bridge_promote_z_ladder(bridge);

    return replayed;
}

static int consolidate_n3_sws(pr_sleep_bridge_t bridge) {
    // N3/SWS is the primary declarative memory consolidation stage
    // Maximum replay with hippocampal sharp-wave ripples

    pr_replay_candidate_t candidates[100];
    size_t count = 0;

    pr_sleep_error_t err = pr_sleep_bridge_get_replay_candidates(
        bridge, candidates, 100, &count);

    if (err != PR_SLEEP_SUCCESS || count == 0) {
        return 0;
    }

    // Replay maximum candidates
    size_t to_replay = count < bridge->config.max_replay_per_cycle ?
                       count : bridge->config.max_replay_per_cycle;

    // Build replay sequence
    uint64_t* replay_ids = (uint64_t*)nimcp_calloc(to_replay, sizeof(uint64_t));
    if (!replay_ids) {
        return 0;
    }

    for (size_t i = 0; i < to_replay; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_replay > 256) {
            pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                             (float)(i + 1) / (float)to_replay);
        }

        replay_ids[i] = candidates[i].node_id;
    }

    // Forward replay (primary in SWS)
    int replayed = pr_sleep_bridge_replay_sequence(bridge, replay_ids, to_replay,
                                                   PR_REPLAY_FORWARD);

    // Reverse replay for a subset (if enabled)
    if (bridge->config.enable_reverse_replay && to_replay > 5) {
        size_t reverse_count = to_replay / 4;
        pr_sleep_bridge_replay_sequence(bridge, replay_ids, reverse_count,
                                        PR_REPLAY_REVERSE);
    }

    nimcp_free(replay_ids);
    replay_ids = NULL;

    // Promote eligible memories (main promotion happens in SWS)
    pr_sleep_bridge_promote_z_ladder(bridge);

    return replayed;
}

static int consolidate_rem(pr_sleep_bridge_t bridge) {
    // REM is for procedural, emotional processing, and creative recombination

    int total_processed = 0;

    // 1. Emotional processing
    int emotional = pr_sleep_bridge_emotional_process(bridge);
    total_processed += emotional;

    // 2. Get replay candidates (emphasizing emotional/procedural)
    pr_replay_candidate_t candidates[50];
    size_t count = 0;

    pr_sleep_error_t err = pr_sleep_bridge_get_replay_candidates(
        bridge, candidates, 50, &count);

    if (err == PR_SLEEP_SUCCESS && count > 0) {
        // Replay with random order (creative recombination)
        size_t to_replay = count < 30 ? count : 30;

        uint64_t* replay_ids = (uint64_t*)nimcp_calloc(to_replay, sizeof(uint64_t));
        if (replay_ids) {
            for (size_t i = 0; i < to_replay; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && to_replay > 256) {
                    pr_sleep_bridge_heartbeat("pr_sleep_bri_loop",
                                     (float)(i + 1) / (float)to_replay);
                }

                replay_ids[i] = candidates[i].node_id;
            }

            pr_replay_direction_t direction = bridge->config.enable_random_replay ?
                                              PR_REPLAY_RANDOM : PR_REPLAY_FORWARD;

            int replayed = pr_sleep_bridge_replay_sequence(bridge, replay_ids, to_replay,
                                                           direction);
            total_processed += replayed;

            nimcp_free(replay_ids);
            replay_ids = NULL;
        }
    }

    // 3. Prune weak associations
    int pruned = pr_sleep_bridge_prune_weak_associations(bridge);
    (void)pruned;  // Tracked in stats

    // 4. Some promotion (less than SWS)
    pr_sleep_bridge_promote_z_ladder(bridge);

    return total_processed;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_sleep_bridge_set_instance_health_agent(
    pr_sleep_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_sleep_bridge_training_begin(pr_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_sleep_bridge_heartbeat_instance(bridge->health_agent, "pr_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int pr_sleep_bridge_training_end(pr_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    pr_sleep_bridge_heartbeat_instance(bridge->health_agent, "pr_sleep_bridge_training_end", 1.0f);
    return 0;
}

int pr_sleep_bridge_training_step(pr_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    pr_sleep_bridge_heartbeat_instance(bridge->health_agent, "pr_sleep_bridge_training_step", progress);
    return 0;
}
