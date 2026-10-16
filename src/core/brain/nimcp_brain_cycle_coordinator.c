// SPDX-License-Identifier: MIT
// Copyright (c) 2025 NIMCP Project
/**
 * @file nimcp_brain_cycle_coordinator.c
 * @brief Brain Cycle Coordinator implementation
 *
 * WHAT: Unified observability and coordination across all 9 brain cycle types
 * WHY:  Single point for health tracking, stall detection, dependency validation,
 *       and integration with immune, bio-async, KG, and other subsystems
 * HOW:  Fixed-size registry indexed by cycle type, Welford's algorithm for stats,
 *       FNV-1a hash for pattern fingerprinting, callback system for notifications
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_kg_io_dispatcher.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_cycle_coordinator, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define CYCLE_COORD_LOG_MODULE "cycle_coordinator"

//=============================================================================
// Internal Types
//=============================================================================

/** Per-cycle dependency entry */
typedef struct {
    brain_cycle_type_t dependent;
    brain_cycle_type_t dependency;
} cycle_dependency_t;

/** Health pattern fingerprint */
typedef struct {
    uint32_t pattern_hash;
    uint32_t occurrence_count;
    uint64_t last_seen_us;
} health_pattern_t;

/** Per-cycle internal entry */
typedef struct {
    brain_cycle_type_t      type;
    brain_cycle_category_t  category;
    brain_cycle_health_t    health;
    bool                    registered;
    bool                    enabled;
    bool                    running;
    void*                   cycle_handle;
    brain_cycle_health_fn_t health_fn;

    /* Timing */
    uint64_t last_tick_us;
    uint64_t expected_interval_us;
    uint64_t ticks_executed;
    uint64_t errors_encountered;

    /* Duration stats (Welford's online algorithm) */
    double   avg_duration_us;
    uint64_t max_duration_us;
    double   duration_mean;
    double   duration_m2;
    uint64_t duration_count;
    double   duration_stddev;

    /* Monitoring */
    float    monitoring_weight;

    /* Driver thread (only valid when `driven == true`).
     *
     * When a cycle is registered via register_driven(), the coordinator owns
     * a dedicated thread that loops: sleep(interval_us) → tick_fn(tick_ctx)
     *  → notify_tick(). driver_stop_flag is the cooperative shutdown signal;
     * unregister sets it then joins.
     */
    bool                  driven;
    brain_cycle_tick_fn_t tick_fn;
    void*                 tick_ctx;
    uint64_t              tick_interval_us;
    nimcp_thread_t        driver_thread;
    volatile int          driver_stop_flag;  /* 0 = run, 1 = stop (GCC atomics) */
} cycle_entry_t;

/** Internal coordinator structure */
struct brain_cycle_coordinator {
    /* Configuration */
    brain_cycle_coordinator_config_t config;

    /* Cycle registry */
    cycle_entry_t cycles[BRAIN_CYCLE_COUNT];
    uint32_t registered_count;

    /* Dependencies */
    cycle_dependency_t dependencies[BRAIN_CYCLE_COUNT * BRAIN_CYCLE_MAX_DEPENDENCIES];
    uint32_t dependency_count;

    /* Callbacks */
    brain_cycle_coordinator_callbacks_t callbacks[BRAIN_CYCLE_MAX_CALLBACKS];
    uint32_t callback_count;

    /* Health patterns (FNV-1a fingerprints) */
    health_pattern_t patterns[BRAIN_CYCLE_MAX_HEALTH_PATTERNS];
    uint32_t pattern_count;
    uint32_t mc_seed;

    /* Integration references */
    bio_module_context_t*              bio_context;
    brain_immune_system_t*             immune_system;
    kg_io_dispatcher_t*                kg_dispatcher;
    introspection_context_t*           introspection_ctx;
    hemispheric_brain_t*               hemispheric_brain;
    oscillations_fep_bridge_t*         fep_bridge;
    meta_learning_substrate_bridge_t*  meta_bridge;
    sfa_pink_noise_bridge_t*           pink_noise_bridge;
    snn_global_workspace_bridge_t*     gw_bridge;
    snn_attention_bridge_t*            attention_bridge;
    world_model_multimodal_t*          world_model;

    /* Statistics cache */
    brain_cycle_coordinator_stats_t stats;
    float prev_overall_health;

    /* Timing */
    uint64_t created_time_us;
    uint64_t last_health_check_us;
    uint64_t last_kg_write_us;
    uint64_t health_checks_performed;

    /* KG table name storage */
    char kg_table_name[NIMCP_LABEL_BUFFER_SIZE];

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Bio-Async Message Payloads (file-local)
//=============================================================================

typedef struct {
    bio_message_header_t header;
    uint32_t cycle_type;       /* brain_cycle_type_t */
    uint32_t old_health;       /* brain_cycle_health_t */
    uint32_t new_health;       /* brain_cycle_health_t */
} bio_msg_cycle_health_changed_t;

typedef struct {
    bio_message_header_t header;
    uint32_t cycle_type;
    uint64_t stall_duration_ms;
} bio_msg_cycle_stall_detected_t;

typedef struct {
    bio_message_header_t header;
    uint32_t dependent_type;
    uint32_t dependency_type;
} bio_msg_cycle_dependency_violated_t;

typedef struct {
    bio_message_header_t header;
    float    overall_health;
    uint32_t healthy_count;
    uint32_t degraded_count;
    uint32_t stalled_count;
    uint64_t uptime_ms;
} bio_msg_cycle_coordinator_stats_t;

//=============================================================================
// Static Helpers
//=============================================================================

static uint64_t get_timestamp_us(void) {
    return nimcp_time_monotonic_us();
}

static const char* get_cycle_name(brain_cycle_type_t type) {
    switch (type) {
    case BRAIN_CYCLE_IMMUNE_TICK:      return "immune_tick";
    case BRAIN_CYCLE_HEALTH_AGENT:     return "health_agent";
    case BRAIN_CYCLE_SLEEP_WAKE:       return "sleep_wake";
    case BRAIN_CYCLE_CIRCADIAN:        return "circadian";
    case BRAIN_CYCLE_AROUSAL:          return "arousal";
    case BRAIN_CYCLE_OSCILLATIONS:     return "oscillations";
    case BRAIN_CYCLE_GC_AGENT:         return "gc_agent";
    case BRAIN_CYCLE_IO_DISPATCHER:    return "io_dispatcher";
    case BRAIN_CYCLE_BRAIN_UPDATE:     return "brain_update";
    case BRAIN_CYCLE_LONG_TERM_MEMORY: return "long_term_memory";
    case BRAIN_CYCLE_NEUROGENESIS:     return "neurogenesis";
    case BRAIN_CYCLE_EPIGENETICS:      return "epigenetics";
    case BRAIN_CYCLE_NEUROVASCULAR:    return "neurovascular";
    case BRAIN_CYCLE_PREDICTIVE_IMMUNE: return "predictive_immune";
    case BRAIN_CYCLE_CHEMISTRY:        return "chemistry";
    case BRAIN_CYCLE_COCHLEA_BRIDGES:  return "cochlea_bridges";
    case BRAIN_CYCLE_NEUROMOD:         return "neuromod";
    case BRAIN_CYCLE_SENSORIMOTOR:     return "sensorimotor";
    case BRAIN_CYCLE_LANGUAGE:         return "language";
    case BRAIN_CYCLE_PHYSICS_BRIDGES:  return "physics_bridges";
    case BRAIN_CYCLE_META_LEARNING:    return "meta_learning";
    case BRAIN_CYCLE_INTUITIVE_PHYSICS: return "intuitive_physics";
    case BRAIN_CYCLE_HYPOTHALAMUS:     return "hypothalamus";
    case BRAIN_CYCLE_ENTORHINAL:       return "entorhinal";
    case BRAIN_CYCLE_CEREBELLUM:       return "cerebellum";
    case BRAIN_CYCLE_BASAL_GANGLIA:    return "basal_ganglia";
    default:                           return "unknown";
    }
}

static brain_cycle_category_t get_cycle_category(brain_cycle_type_t type) {
    switch (type) {
    case BRAIN_CYCLE_IMMUNE_TICK:
    case BRAIN_CYCLE_OSCILLATIONS:
    case BRAIN_CYCLE_BRAIN_UPDATE:
    case BRAIN_CYCLE_CHEMISTRY:
    case BRAIN_CYCLE_COCHLEA_BRIDGES:
    case BRAIN_CYCLE_NEUROMOD:
    case BRAIN_CYCLE_SENSORIMOTOR:
    case BRAIN_CYCLE_LANGUAGE:
    case BRAIN_CYCLE_PHYSICS_BRIDGES:
    case BRAIN_CYCLE_INTUITIVE_PHYSICS:
    case BRAIN_CYCLE_ENTORHINAL:
    case BRAIN_CYCLE_CEREBELLUM:
    case BRAIN_CYCLE_BASAL_GANGLIA:
        return BRAIN_CYCLE_CATEGORY_FAST;
    case BRAIN_CYCLE_HEALTH_AGENT:
    case BRAIN_CYCLE_LONG_TERM_MEMORY:
    case BRAIN_CYCLE_EPIGENETICS:
    case BRAIN_CYCLE_NEUROVASCULAR:
    case BRAIN_CYCLE_PREDICTIVE_IMMUNE:
    case BRAIN_CYCLE_META_LEARNING:
    case BRAIN_CYCLE_HYPOTHALAMUS:
        return BRAIN_CYCLE_CATEGORY_MEDIUM;
    case BRAIN_CYCLE_SLEEP_WAKE:
    case BRAIN_CYCLE_CIRCADIAN:
    case BRAIN_CYCLE_AROUSAL:
    case BRAIN_CYCLE_NEUROGENESIS:
        return BRAIN_CYCLE_CATEGORY_SLOW;
    case BRAIN_CYCLE_GC_AGENT:
    case BRAIN_CYCLE_IO_DISPATCHER:
        return BRAIN_CYCLE_CATEGORY_BACKGROUND;
    default:
        return BRAIN_CYCLE_CATEGORY_BACKGROUND;
    }
}

static uint64_t get_default_interval_us(brain_cycle_type_t type) {
    switch (type) {
    case BRAIN_CYCLE_IMMUNE_TICK:      return 50000;      /* 50ms */
    case BRAIN_CYCLE_HEALTH_AGENT:     return 100000;     /* 100ms */
    case BRAIN_CYCLE_OSCILLATIONS:     return 10000;      /* 10ms */
    case BRAIN_CYCLE_BRAIN_UPDATE:     return 16000;      /* 16ms (~60fps) */
    case BRAIN_CYCLE_LONG_TERM_MEMORY: return 100000;     /* 100ms — matches pr_consolidation_interval_us */
    case BRAIN_CYCLE_GC_AGENT:         return 60000000;   /* 60s */
    case BRAIN_CYCLE_EPIGENETICS:      return 100000;     /* 100ms — methylation is slow but update needs sub-second resolution for critical-period gating */
    case BRAIN_CYCLE_NEUROVASCULAR:    return 100000;     /* 100ms — HRF evolves over seconds; 100ms gives smooth interpolation */
    case BRAIN_CYCLE_NEUROGENESIS:     return 1000000;    /* 1s — neuron birth/pruning is slow; hourly would be closer to biology */
    case BRAIN_CYCLE_PREDICTIVE_IMMUNE: return 100000;    /* 100ms — interoceptive prediction + precision modulation cadence */
    case BRAIN_CYCLE_CHEMISTRY:        return 10000;     /* 10ms — ion pump / buffer dynamics are ms-scale; one tick runs protons→buffers→pH→NO in order */
    case BRAIN_CYCLE_COCHLEA_BRIDGES:  return 10000;     /* 10ms — auditory nerve ISIs are sub-ms; 10ms keeps bridge state fresh without oversampling */
    case BRAIN_CYCLE_NEUROMOD:         return 16000;     /* 16ms — neuromodulator release/decay is 10-100ms scale */
    case BRAIN_CYCLE_SENSORIMOTOR:     return 16000;     /* 16ms — matches BRAIN_UPDATE cadence; these are per-decide modules */
    case BRAIN_CYCLE_LANGUAGE:         return 16000;     /* 16ms — bio-msg drain; low overhead with 32-msg budget */
    case BRAIN_CYCLE_PHYSICS_BRIDGES:  return 16000;     /* 16ms — LFP sampling / inbox drain */
    case BRAIN_CYCLE_META_LEARNING:    return 100000;    /* 100ms — LR adaptation cadence; slower than inference hot path */
    case BRAIN_CYCLE_INTUITIVE_PHYSICS: return 16000;    /* 16ms — matches BRAIN_UPDATE cadence (stage_physics_task also uses 16ms) */
    case BRAIN_CYCLE_HYPOTHALAMUS:     return 100000;    /* 100ms — slow drive: SCN/HPA/autonomic evolve over seconds */
    case BRAIN_CYCLE_ENTORHINAL:       return 16000;     /* 16ms — grid/place cell phase precession ~theta band */
    case BRAIN_CYCLE_CEREBELLUM:       return 16000;     /* 16ms — bio-msg drain; matches BRAIN_UPDATE cadence */
    case BRAIN_CYCLE_BASAL_GANGLIA:    return 16000;     /* 16ms — beta oscillations + vigor decay are 10-100ms scale */
    case BRAIN_CYCLE_SLEEP_WAKE:       return 0;          /* state machine */
    case BRAIN_CYCLE_CIRCADIAN:        return 0;          /* continuous */
    case BRAIN_CYCLE_AROUSAL:          return 0;          /* event-driven */
    case BRAIN_CYCLE_IO_DISPATCHER:    return 0;          /* queue-driven */
    default:                           return 0;
    }
}

/** FNV-1a hash for health pattern fingerprinting */
static uint32_t fnv1a_hash(const uint8_t* data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

/** Update running mean/stddev using Welford's algorithm */
static void update_welford_stats(cycle_entry_t* e, uint64_t duration_us) {
    e->duration_count++;
    double val = (double)duration_us;
    double delta = val - e->duration_mean;
    e->duration_mean += delta / (double)e->duration_count;
    double delta2 = val - e->duration_mean;
    e->duration_m2 += delta * delta2;
    if (e->duration_count > 1) {
        e->duration_stddev = sqrt(e->duration_m2 / (double)(e->duration_count - 1));
    }

    /* EMA for avg_duration_us (alpha = 0.1) */
    if (e->ticks_executed <= 1) {
        e->avg_duration_us = val;
    } else {
        e->avg_duration_us = 0.1 * val + 0.9 * e->avg_duration_us;
    }

    if (duration_us > e->max_duration_us) {
        e->max_duration_us = duration_us;
    }
}

/** Z-score anomaly detection */
static bool is_duration_anomaly(const cycle_entry_t* e, uint64_t duration_us) {
    if (e->duration_count < 10 || e->duration_stddev < 1.0) {
        return false;
    }
    double z = ((double)duration_us - e->duration_mean) / e->duration_stddev;
    return fabs(z) > 3.0;
}

/** Track recurring health patterns via FNV-1a hash */
static void track_health_pattern(brain_cycle_coordinator_t* coord) {
    uint8_t health_state[BRAIN_CYCLE_COUNT];
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        health_state[i] = (uint8_t)coord->cycles[i].health;
    }
    uint32_t pattern = fnv1a_hash(health_state, BRAIN_CYCLE_COUNT);

    for (uint32_t i = 0; i < coord->pattern_count; i++) {
        if (coord->patterns[i].pattern_hash == pattern) {
            coord->patterns[i].occurrence_count++;
            coord->patterns[i].last_seen_us = get_timestamp_us();
            return;
        }
    }
    if (coord->pattern_count < BRAIN_CYCLE_MAX_HEALTH_PATTERNS) {
        health_pattern_t* p = &coord->patterns[coord->pattern_count++];
        p->pattern_hash = pattern;
        p->occurrence_count = 1;
        p->last_seen_us = get_timestamp_us();
    }
}

/** Compute overall health as weighted average of cycle healths */
static float compute_overall_health(const brain_cycle_coordinator_t* coord) {
    float total_weight = 0.0f;
    float weighted_health = 0.0f;

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered) continue;

        float w = e->monitoring_weight;
        float h;
        switch (e->health) {
        case BRAIN_CYCLE_HEALTH_HEALTHY:  h = 1.0f; break;
        case BRAIN_CYCLE_HEALTH_DEGRADED: h = 0.5f; break;
        case BRAIN_CYCLE_HEALTH_STALLED:  h = 0.1f; break;
        case BRAIN_CYCLE_HEALTH_ERROR:    h = 0.0f; break;
        case BRAIN_CYCLE_HEALTH_DISABLED: continue;
        default:                          h = 0.5f; break;
        }
        weighted_health += w * h;
        total_weight += w;
    }
    return (total_weight > 0.0f) ? (weighted_health / total_weight) : 1.0f;
}

/** Update per-category aggregate statistics */
static void update_category_stats(brain_cycle_coordinator_t* coord) {
    memset(coord->stats.categories, 0, sizeof(coord->stats.categories));
    coord->stats.total_cycles_registered = 0;
    coord->stats.total_cycles_healthy = 0;
    coord->stats.total_cycles_degraded = 0;
    coord->stats.total_cycles_stalled = 0;

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered) continue;

        coord->stats.total_cycles_registered++;
        brain_cycle_category_stats_t* cs = &coord->stats.categories[e->category];
        cs->total_cycles++;
        cs->total_ticks += e->ticks_executed;

        switch (e->health) {
        case BRAIN_CYCLE_HEALTH_HEALTHY:
            cs->healthy_cycles++;
            coord->stats.total_cycles_healthy++;
            break;
        case BRAIN_CYCLE_HEALTH_DEGRADED:
            cs->degraded_cycles++;
            coord->stats.total_cycles_degraded++;
            break;
        case BRAIN_CYCLE_HEALTH_STALLED:
            cs->stalled_cycles++;
            coord->stats.total_cycles_stalled++;
            break;
        case BRAIN_CYCLE_HEALTH_ERROR:
            /* ERROR is distinct from STALLED - count in stalled_cycles
             * for category stats but not in total_cycles_stalled */
            cs->stalled_cycles++;
            break;
        default:
            break;
        }
    }
}

//=============================================================================
// Forward Declarations - Integration Helpers
//=============================================================================

static void publish_bio_health_changed(brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type, brain_cycle_health_t old_h, brain_cycle_health_t new_h);
static void publish_bio_stall(brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type, uint64_t stall_ms);
static void publish_bio_dependency_violated(brain_cycle_coordinator_t* coord,
    brain_cycle_type_t dependent, brain_cycle_type_t dependency);
static void publish_bio_stats(brain_cycle_coordinator_t* coord);
static void report_immune_stall(brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type, uint64_t stall_ms);
static void report_immune_health_change(brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type, brain_cycle_health_t old_h, brain_cycle_health_t new_h);
static float read_immune_inflammation_sensitivity(brain_cycle_coordinator_t* coord);

//=============================================================================
// Callback Firing Helpers
//=============================================================================

static void fire_health_changed_callbacks(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    brain_cycle_health_t old_h,
    brain_cycle_health_t new_h)
{
    for (uint32_t i = 0; i < coord->callback_count; i++) {
        if (coord->callbacks[i].on_health_changed) {
            coord->callbacks[i].on_health_changed(
                type, old_h, new_h, coord->callbacks[i].user_data);
        }
    }
    publish_bio_health_changed(coord, type, old_h, new_h);
    report_immune_health_change(coord, type, old_h, new_h);
}

static void fire_stall_detected_callbacks(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t stall_duration_ms)
{
    for (uint32_t i = 0; i < coord->callback_count; i++) {
        if (coord->callbacks[i].on_stall_detected) {
            coord->callbacks[i].on_stall_detected(
                type, stall_duration_ms, coord->callbacks[i].user_data);
        }
    }
    publish_bio_stall(coord, type, stall_duration_ms);
    report_immune_stall(coord, type, stall_duration_ms);
}

static void fire_dependency_violated_callbacks(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t dependent,
    brain_cycle_type_t dependency)
{
    for (uint32_t i = 0; i < coord->callback_count; i++) {
        if (coord->callbacks[i].on_dependency_violated) {
            coord->callbacks[i].on_dependency_violated(
                dependent, dependency, coord->callbacks[i].user_data);
        }
    }
    publish_bio_dependency_violated(coord, dependent, dependency);
}

static void fire_overall_health_changed_callbacks(
    brain_cycle_coordinator_t* coord,
    float old_h,
    float new_h)
{
    for (uint32_t i = 0; i < coord->callback_count; i++) {
        if (coord->callbacks[i].on_overall_health_changed) {
            coord->callbacks[i].on_overall_health_changed(
                old_h, new_h, coord->callbacks[i].user_data);
        }
    }
    publish_bio_stats(coord);
}

//=============================================================================
// Integration Helpers - Bio-Async Publishing
//=============================================================================

static void publish_bio_health_changed(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    brain_cycle_health_t old_h,
    brain_cycle_health_t new_h)
{
    if (!coord->bio_context) return;

    bio_msg_cycle_health_changed_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_CYCLE_HEALTH_CHANGED,
                        BIO_MODULE_CYCLE_COORDINATOR, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.cycle_type = (uint32_t)type;
    msg.old_health = (uint32_t)old_h;
    msg.new_health = (uint32_t)new_h;
    bio_router_broadcast(*coord->bio_context, &msg, sizeof(msg));
}

static void publish_bio_stall(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t stall_ms)
{
    if (!coord->bio_context) return;

    bio_msg_cycle_stall_detected_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_CYCLE_STALL_DETECTED,
                        BIO_MODULE_CYCLE_COORDINATOR, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.cycle_type = (uint32_t)type;
    msg.stall_duration_ms = stall_ms;
    bio_router_broadcast(*coord->bio_context, &msg, sizeof(msg));
}

static void publish_bio_dependency_violated(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t dependent,
    brain_cycle_type_t dependency)
{
    if (!coord->bio_context) return;

    bio_msg_cycle_dependency_violated_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_CYCLE_DEPENDENCY_VIOLATED,
                        BIO_MODULE_CYCLE_COORDINATOR, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.dependent_type = (uint32_t)dependent;
    msg.dependency_type = (uint32_t)dependency;
    bio_router_broadcast(*coord->bio_context, &msg, sizeof(msg));
}

static void publish_bio_stats(brain_cycle_coordinator_t* coord)
{
    if (!coord->bio_context) return;

    bio_msg_cycle_coordinator_stats_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_CYCLE_COORDINATOR_STATS,
                        BIO_MODULE_CYCLE_COORDINATOR, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.overall_health = coord->stats.overall_health;
    msg.healthy_count = coord->stats.total_cycles_healthy;
    msg.degraded_count = coord->stats.total_cycles_degraded;
    msg.stalled_count = coord->stats.total_cycles_stalled;
    msg.uptime_ms = coord->stats.coordinator_uptime_ms;
    bio_router_broadcast(*coord->bio_context, &msg, sizeof(msg));
}

//=============================================================================
// Integration Helpers - Immune System Reporting
//=============================================================================

static void report_immune_stall(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t stall_ms)
{
    if (!coord->immune_system) return;

    const char* name = get_cycle_name(type);
    uint32_t severity = (uint32_t)(stall_ms / 1000 + 3);
    if (severity > 10) severity = 10;

    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        coord->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        (const uint8_t*)name,
        strlen(name),
        severity,
        (uint32_t)type,
        &antigen_id);
}

static void report_immune_health_change(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    brain_cycle_health_t old_h,
    brain_cycle_health_t new_h)
{
    if (!coord->immune_system) return;

    brain_cytokine_type_t ctype;
    float concentration;

    if (new_h == BRAIN_CYCLE_HEALTH_DEGRADED && old_h == BRAIN_CYCLE_HEALTH_HEALTHY) {
        /* Degradation: pro-inflammatory */
        ctype = BRAIN_CYTOKINE_IL1;
        concentration = 0.5f;
    } else if (new_h == BRAIN_CYCLE_HEALTH_STALLED || new_h == BRAIN_CYCLE_HEALTH_ERROR) {
        /* Stalled/error: stronger pro-inflammatory */
        ctype = BRAIN_CYTOKINE_IL1;
        concentration = 0.3f;
    } else if (new_h == BRAIN_CYCLE_HEALTH_HEALTHY &&
               (old_h == BRAIN_CYCLE_HEALTH_DEGRADED || old_h == BRAIN_CYCLE_HEALTH_STALLED ||
                old_h == BRAIN_CYCLE_HEALTH_ERROR)) {
        /* Recovery: anti-inflammatory */
        ctype = BRAIN_CYTOKINE_IL10;
        concentration = -0.2f;
    } else {
        return; /* No immune-relevant transition */
    }

    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(
        coord->immune_system,
        ctype,
        (uint32_t)type,  /* source_cell = cycle type */
        concentration,
        0,               /* target_region = 0 (broadcast) */
        &cytokine_id);
}

static float read_immune_inflammation_sensitivity(brain_cycle_coordinator_t* coord)
{
    if (!coord->immune_system) return 1.0f;

    brain_inflammation_level_t level =
        brain_immune_get_inflammation_level(coord->immune_system);

    /* Higher inflammation = more sensitive stall detection (higher divisor
     * in threshold = interval * multiplier / sensitivity).
     * Returns 0.5 - 2.0: STORM=2.0 (most sensitive), NONE=0.5 (least). */
    switch (level) {
    case INFLAMMATION_STORM:    return 2.0f;
    case INFLAMMATION_SYSTEMIC: return 1.5f;
    case INFLAMMATION_REGIONAL: return 1.0f;
    case INFLAMMATION_LOCAL:    return 0.75f;
    case INFLAMMATION_NONE:
    default:                    return 0.5f;
    }
}

/* Forward declaration for KG flush (Phase 4) */
static int flush_to_kg_unlocked(brain_cycle_coordinator_t* coord);

//=============================================================================
// Public API - Lifecycle
//=============================================================================

void brain_cycle_coordinator_default_config(brain_cycle_coordinator_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));

    config->enable_timing_checks = true;
    config->stall_threshold_multiplier = BRAIN_CYCLE_DEFAULT_STALL_MULTIPLIER;
    config->health_check_interval_ms = BRAIN_CYCLE_DEFAULT_HEALTH_CHECK_MS;
    config->enable_auto_health_check = false;
    config->enable_dependency_tracking = true;
    config->enable_logging = true;
    config->enable_debug_logging = false;
    config->kg_table_name = "brain_cycle_stats";
    config->kg_write_interval_ms = BRAIN_CYCLE_DEFAULT_KG_WRITE_MS;
    config->noise_health_sensitivity = 0.5f;
}

brain_cycle_coordinator_t* brain_cycle_coordinator_create(
    const brain_cycle_coordinator_config_t* config)
{
    brain_cycle_coordinator_t* coord = nimcp_malloc(sizeof(*coord));
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "brain_cycle_coordinator_create: allocation failed");
        return NULL;
    }
    memset(coord, 0, sizeof(*coord));

    /* Apply config */
    if (config) {
        coord->config = *config;
    } else {
        brain_cycle_coordinator_default_config(&coord->config);
    }

    /* Store KG table name */
    if (coord->config.kg_table_name) {
        snprintf(coord->kg_table_name, sizeof(coord->kg_table_name),
                 "%s", coord->config.kg_table_name);
    } else {
        snprintf(coord->kg_table_name, sizeof(coord->kg_table_name),
                 "brain_cycle_stats");
    }

    /* Initialize integration references from config */
    coord->bio_context = coord->config.bio_context;
    coord->immune_system = coord->config.immune_system;
    coord->kg_dispatcher = coord->config.kg_dispatcher;
    coord->introspection_ctx = coord->config.introspection_ctx;
    coord->hemispheric_brain = coord->config.hemispheric_brain;
    coord->fep_bridge = coord->config.fep_bridge;
    coord->meta_bridge = coord->config.meta_bridge;
    coord->pink_noise_bridge = coord->config.pink_noise_bridge;
    coord->gw_bridge = coord->config.gw_bridge;
    coord->attention_bridge = coord->config.attention_bridge;
    coord->world_model = coord->config.world_model;

    /* Initialize cycle entries with default categories and intervals */
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        coord->cycles[i].type = (brain_cycle_type_t)i;
        coord->cycles[i].category = get_cycle_category((brain_cycle_type_t)i);
        coord->cycles[i].expected_interval_us = get_default_interval_us((brain_cycle_type_t)i);
        coord->cycles[i].health = BRAIN_CYCLE_HEALTH_UNKNOWN;
        coord->cycles[i].monitoring_weight = 1.0f;
    }

    /* Create mutex */
    coord->mutex = nimcp_mutex_create(NULL);
    if (!coord->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "brain_cycle_coordinator_create: mutex creation failed");
        nimcp_free(coord);
        return NULL;
    }

    coord->created_time_us = get_timestamp_us();
    coord->prev_overall_health = 1.0f;
    coord->stats.overall_health = 1.0f;
    coord->mc_seed = (uint32_t)(coord->created_time_us & 0xFFFFFFFF);

    if (coord->config.enable_logging) {
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "Brain cycle coordinator created (timing=%s, deps=%s)",
            coord->config.enable_timing_checks ? "on" : "off",
            coord->config.enable_dependency_tracking ? "on" : "off");
    }

    return coord;
}

void brain_cycle_coordinator_destroy(brain_cycle_coordinator_t* coord) {
    if (!coord) return;

    /* Stop + join any driven cycles BEFORE freeing the coordinator — their
     * driver threads hold a pointer to `coord` and will segfault on next
     * notify_tick if we free underneath them. */
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        if (coord->cycles[i].registered && coord->cycles[i].driven) {
            (void)brain_cycle_coordinator_unregister(coord,
                (brain_cycle_type_t)i);
        }
    }

    if (coord->config.enable_logging) {
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "Brain cycle coordinator destroying (uptime=%lums, checks=%lu)",
            (unsigned long)((get_timestamp_us() - coord->created_time_us) / 1000),
            (unsigned long)coord->health_checks_performed);
    }

    if (coord->mutex) {
        nimcp_mutex_free(coord->mutex);
        coord->mutex = NULL;
    }

    nimcp_free(coord);
}

//=============================================================================
// Driver Thread Management (for register_driven cycles)
//
// Design:
//   - One pthread per driven cycle, owned by the coordinator.
//   - Thread loops: sleep(interval) → check stop → tick_fn() → notify_tick().
//   - Cooperative shutdown via atomic stop flag; unregister sets + joins.
//   - tick_fn reads are done once per register (immutable after register).
//   - The driver arg is heap-allocated so it survives past register_driven's
//     stack frame; the driver loop frees it on exit.
//
// Concurrency invariants:
//   - unregister MUST release the coordinator mutex before pthread_join, or
//     it deadlocks with the driver (which takes the mutex in notify_tick).
//   - `e->registered` stays true through join so stray notify_tick calls from
//     an in-flight tick are harmless (they update stats for a still-registered
//     entry). Registration flags are cleared AFTER join under the mutex.
//   - destroy() unregisters all driven cycles before freeing the coordinator.
//=============================================================================

typedef struct {
    brain_cycle_coordinator_t* coord;
    brain_cycle_type_t         type;
} cycle_driver_arg_t;

/* The driver thread's main loop. One instance per driven cycle. */
static void* cycle_driver_loop(void* arg_raw) {
    cycle_driver_arg_t* arg = (cycle_driver_arg_t*)arg_raw;
    brain_cycle_coordinator_t* coord = arg->coord;
    brain_cycle_type_t type = arg->type;
    int type_idx = (int)type;

    /* Thread name for debugging (truncated to 15 chars + NUL by Linux). */
    char tname[32];
    snprintf(tname, sizeof(tname), "cc-%s", get_cycle_name(type));
    (void)nimcp_thread_set_name(tname);

    /* Read immutable-after-register fields once under the mutex. */
    nimcp_mutex_lock(coord->mutex);
    cycle_entry_t* e = &coord->cycles[type_idx];
    uint64_t interval_us = e->tick_interval_us;
    brain_cycle_tick_fn_t tick_fn = e->tick_fn;
    void* tick_ctx = e->tick_ctx;
    nimcp_mutex_unlock(coord->mutex);

    if (!tick_fn || interval_us == 0) {
        /* Defensive: shouldn't happen, but don't spin forever. */
        nimcp_free(arg);
        return NULL;
    }

    while (__atomic_load_n(&coord->cycles[type_idx].driver_stop_flag,
                           __ATOMIC_ACQUIRE) == 0) {
        /* Sleep for the configured interval. Split long intervals into 100ms
         * slices so stop-flag polling stays responsive during shutdown
         * (a 60s GC cycle shouldn't delay shutdown by 60s). */
        const uint64_t slice_us = 100000u;  /* 100ms */
        uint64_t remaining = interval_us;
        while (remaining > 0 &&
               __atomic_load_n(&coord->cycles[type_idx].driver_stop_flag,
                               __ATOMIC_ACQUIRE) == 0) {
            uint64_t this_slice = (remaining > slice_us) ? slice_us : remaining;
            struct timespec ts = {
                .tv_sec  = (time_t)(this_slice / 1000000u),
                .tv_nsec = (long)((this_slice % 1000000u) * 1000u)
            };
            nanosleep(&ts, NULL);
            remaining -= this_slice;
        }

        if (__atomic_load_n(&coord->cycles[type_idx].driver_stop_flag,
                            __ATOMIC_ACQUIRE))
            break;

        uint64_t t0 = get_timestamp_us();
        tick_fn(tick_ctx);
        uint64_t dur = get_timestamp_us() - t0;

        (void)brain_cycle_coordinator_notify_tick(coord, type, dur);
    }

    nimcp_free(arg);
    return NULL;
}

//=============================================================================
// Public API - Registration
//=============================================================================

int brain_cycle_coordinator_register(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    void* cycle_handle,
    brain_cycle_health_fn_t health_fn)
{
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_register: coord is NULL");
        return -1;
    }
    if ((int)type < 0 || (int)type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_register: invalid cycle type %d", (int)type);
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    cycle_entry_t* e = &coord->cycles[(int)type];
    if (e->registered) {
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_register: cycle %s already registered",
            get_cycle_name(type));
        return -1;
    }

    e->registered = true;
    e->enabled = true;
    e->running = true;
    e->cycle_handle = cycle_handle;
    e->health_fn = health_fn;
    e->health = BRAIN_CYCLE_HEALTH_UNKNOWN;
    e->last_tick_us = get_timestamp_us();
    coord->registered_count++;

    nimcp_mutex_unlock(coord->mutex);

    if (coord->config.enable_logging) {
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "Registered cycle '%s' (category=%s, interval=%luus)",
            get_cycle_name(type),
            brain_cycle_category_name(e->category),
            (unsigned long)e->expected_interval_us);
    }

    return 0;
}

int brain_cycle_coordinator_register_driven(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t interval_us,
    brain_cycle_tick_fn_t tick_fn,
    void* tick_ctx,
    brain_cycle_health_fn_t health_fn)
{
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_register_driven: coord is NULL");
        return -1;
    }
    if ((int)type < 0 || (int)type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_register_driven: invalid cycle type %d", (int)type);
        return -1;
    }
    if (!tick_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_register_driven: tick_fn is NULL");
        return -1;
    }
    if (interval_us < 1000u) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_register_driven: interval_us=%llu below 1ms floor",
            (unsigned long long)interval_us);
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    cycle_entry_t* e = &coord->cycles[(int)type];
    if (e->registered) {
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_register_driven: cycle %s already registered",
            get_cycle_name(type));
        return -1;
    }

    /* Populate registration + driver fields BEFORE spawning the thread so the
     * driver's first read-under-lock sees consistent state. */
    e->registered = true;
    e->enabled = true;
    e->running = true;
    e->cycle_handle = tick_ctx;  /* expose to health_fn as the cycle handle */
    e->health_fn = health_fn;
    e->health = BRAIN_CYCLE_HEALTH_UNKNOWN;
    e->last_tick_us = get_timestamp_us();
    e->expected_interval_us = interval_us;

    e->driven = true;
    e->tick_fn = tick_fn;
    e->tick_ctx = tick_ctx;
    e->tick_interval_us = interval_us;
    __atomic_store_n(&e->driver_stop_flag, 0, __ATOMIC_RELEASE);

    coord->registered_count++;

    /* Allocate the thread arg BEFORE releasing the mutex — on OOM we can
     * roll back the registration cleanly without another thread observing
     * a half-registered state. */
    cycle_driver_arg_t* arg = (cycle_driver_arg_t*)nimcp_malloc(sizeof(*arg));
    if (!arg) {
        /* Rollback */
        e->registered = false;
        e->enabled = false;
        e->running = false;
        e->driven = false;
        e->tick_fn = NULL;
        e->tick_ctx = NULL;
        e->tick_interval_us = 0;
        coord->registered_count--;
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "brain_cycle_coordinator_register_driven: OOM on driver arg");
        return -1;
    }
    arg->coord = coord;
    arg->type = type;

    nimcp_mutex_unlock(coord->mutex);

    /* Spawn thread OUTSIDE the mutex — nimcp_thread_create shouldn't need it
     * and holding the mutex across a thread-create call is bad style. */
    nimcp_result_t rc = nimcp_thread_create(&e->driver_thread,
                                            cycle_driver_loop, arg, NULL);
    if (rc != NIMCP_OK) {
        /* Rollback registration. We're racing with nothing because the
         * driver thread never started. */
        nimcp_mutex_lock(coord->mutex);
        e->registered = false;
        e->enabled = false;
        e->running = false;
        e->driven = false;
        e->tick_fn = NULL;
        e->tick_ctx = NULL;
        e->tick_interval_us = 0;
        coord->registered_count--;
        nimcp_mutex_unlock(coord->mutex);
        nimcp_free(arg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "brain_cycle_coordinator_register_driven: thread_create failed for %s",
            get_cycle_name(type));
        return -1;
    }

    if (coord->config.enable_logging) {
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "Registered driven cycle '%s' (interval=%luus)",
            get_cycle_name(type), (unsigned long)interval_us);
    }

    return 0;
}

int brain_cycle_coordinator_unregister(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type)
{
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_unregister: coord is NULL");
        return -1;
    }
    if ((int)type < 0 || (int)type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_unregister: invalid cycle type %d", (int)type);
        return -1;
    }

    /* Step 1: under mutex, confirm registered + signal driver stop if driven.
     * We do NOT clear `registered` yet — the driver may still call notify_tick
     * during its last tick, and we want those calls to be harmless updates
     * rather than errors. Registration is fully cleared AFTER join.           */
    nimcp_mutex_lock(coord->mutex);

    cycle_entry_t* e = &coord->cycles[(int)type];
    if (!e->registered) {
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_cycle_coordinator_unregister: e->registered is NULL");
        return -1;
    }

    bool was_driven = e->driven;
    nimcp_thread_t driver_thread = e->driver_thread;

    if (was_driven) {
        __atomic_store_n(&e->driver_stop_flag, 1, __ATOMIC_RELEASE);
    }

    nimcp_mutex_unlock(coord->mutex);

    /* Step 2: join the driver thread WITHOUT holding the mutex.
     * The driver's notify_tick calls take the mutex; joining under lock
     * would deadlock. Joining is bounded by the driver's sleep slice (100ms)
     * plus one tick's duration. */
    if (was_driven) {
        (void)nimcp_thread_join(driver_thread, NULL);
    }

    /* Step 3: under mutex, clear all registration/driver fields. */
    nimcp_mutex_lock(coord->mutex);

    e->registered = false;
    e->enabled = false;
    e->running = false;
    e->cycle_handle = NULL;
    e->health_fn = NULL;
    e->driven = false;
    e->tick_fn = NULL;
    e->tick_ctx = NULL;
    e->tick_interval_us = 0;
    __atomic_store_n(&e->driver_stop_flag, 0, __ATOMIC_RELEASE);
    if (coord->registered_count > 0) coord->registered_count--;

    nimcp_mutex_unlock(coord->mutex);

    if (coord->config.enable_logging) {
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "Unregistered %scycle '%s'",
            was_driven ? "driven " : "",
            get_cycle_name(type));
    }

    return 0;
}

//=============================================================================
// Public API - Tick Notification
//=============================================================================

int brain_cycle_coordinator_notify_tick(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t duration_us)
{
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_notify_tick: coord is NULL");
        return -1;
    }
    if ((int)type < 0 || (int)type >= BRAIN_CYCLE_COUNT) {
        return -1;
    }

    uint64_t now = get_timestamp_us();

    nimcp_mutex_lock(coord->mutex);

    cycle_entry_t* e = &coord->cycles[(int)type];
    if (!e->registered) {
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_cycle_coordinator_notify_tick: e->registered is NULL");
        return -1;
    }

    e->last_tick_us = now;
    e->ticks_executed++;
    update_welford_stats(e, duration_us);

    /* Z-score anomaly detection */
    if (coord->config.enable_timing_checks && is_duration_anomaly(e, duration_us)) {
        coord->stats.timing_anomalies_detected++;
        if (coord->config.enable_logging) {
            LOG_MODULE_WARN(CYCLE_COORD_LOG_MODULE,
                "Timing anomaly in '%s': duration=%luus (mean=%.1f, stddev=%.1f)",
                get_cycle_name(type),
                (unsigned long)duration_us,
                e->duration_mean,
                e->duration_stddev);
        }
    }

    /* Update health from timing if no health_fn */
    if (!e->health_fn && e->expected_interval_us > 0) {
        brain_cycle_health_t old_health = e->health;
        if (duration_us > e->expected_interval_us * coord->config.stall_threshold_multiplier) {
            e->health = BRAIN_CYCLE_HEALTH_DEGRADED;
        } else {
            e->health = BRAIN_CYCLE_HEALTH_HEALTHY;
        }
        if (old_health != e->health && old_health != BRAIN_CYCLE_HEALTH_UNKNOWN) {
            fire_health_changed_callbacks(coord, type, old_health, e->health);
        }
    } else if (!e->health_fn) {
        /* Event-driven/state-machine cycles are healthy if ticking */
        if (e->health == BRAIN_CYCLE_HEALTH_UNKNOWN) {
            e->health = BRAIN_CYCLE_HEALTH_HEALTHY;
        }
    }

    nimcp_mutex_unlock(coord->mutex);

    return 0;
}

//=============================================================================
// Public API - Health Check
//=============================================================================

int brain_cycle_coordinator_check_health(brain_cycle_coordinator_t* coord) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_check_health: coord is NULL");
        return -1;
    }

    uint64_t now = get_timestamp_us();
    uint32_t issues = 0;

    nimcp_mutex_lock(coord->mutex);

    coord->last_health_check_us = now;
    coord->stats.last_health_check_us = now;
    coord->health_checks_performed++;

    /* Pass 1: Update all cycle health states */
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered || !e->enabled) continue;

        brain_cycle_health_t old_health = e->health;
        brain_cycle_health_t new_health = old_health;

        /* Query health function if available */
        if (e->health_fn) {
            new_health = e->health_fn(e->cycle_handle);
        }

        /* Check for stall (only for cycles with expected intervals) */
        if (coord->config.enable_timing_checks && e->expected_interval_us > 0) {
            uint64_t elapsed = now - e->last_tick_us;
            float sensitivity = read_immune_inflammation_sensitivity(coord);
            uint64_t stall_threshold = (uint64_t)(e->expected_interval_us *
                coord->config.stall_threshold_multiplier / sensitivity);

            if (elapsed > stall_threshold && e->ticks_executed > 0) {
                new_health = BRAIN_CYCLE_HEALTH_STALLED;
                uint64_t stall_ms = elapsed / 1000;
                issues++;

                fire_stall_detected_callbacks(coord, e->type, stall_ms);

                if (coord->config.enable_logging) {
                    LOG_MODULE_ERROR(CYCLE_COORD_LOG_MODULE,
                        "Cycle '%s' STALLED: no tick for %lums (threshold=%lums)",
                        get_cycle_name(e->type),
                        (unsigned long)stall_ms,
                        (unsigned long)(stall_threshold / 1000));
                }
            }
        }

        /* Count non-healthy health_fn results as issues */
        if (new_health == BRAIN_CYCLE_HEALTH_ERROR ||
            new_health == BRAIN_CYCLE_HEALTH_DEGRADED) {
            if (old_health != new_health) {
                issues++;
            }
        }

        /* Apply new health */
        e->health = new_health;

        /* Fire callbacks on health change.
         * Suppress only UNKNOWN -> HEALTHY (benign initial activation).
         * Allow UNKNOWN -> ERROR/DEGRADED/STALLED to fire callbacks. */
        if (old_health != new_health &&
            !(old_health == BRAIN_CYCLE_HEALTH_UNKNOWN &&
              new_health == BRAIN_CYCLE_HEALTH_HEALTHY)) {
            fire_health_changed_callbacks(coord, e->type, old_health, new_health);

            if (coord->config.enable_logging) {
                LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
                    "Cycle '%s' health: %s -> %s",
                    get_cycle_name(e->type),
                    brain_cycle_health_name(old_health),
                    brain_cycle_health_name(new_health));
            }
        }
    }

    /* Pass 2: Check dependencies (after all health states are updated) */
    if (coord->config.enable_dependency_tracking) {
        for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
            cycle_entry_t* e = &coord->cycles[i];
            if (!e->registered || !e->enabled) continue;

            for (uint32_t d = 0; d < coord->dependency_count; d++) {
                if (coord->dependencies[d].dependent != e->type) continue;

                int dep_idx = (int)coord->dependencies[d].dependency;
                if (dep_idx >= 0 && dep_idx < BRAIN_CYCLE_COUNT) {
                    cycle_entry_t* dep_e = &coord->cycles[dep_idx];
                    if (dep_e->registered &&
                        dep_e->health != BRAIN_CYCLE_HEALTH_HEALTHY &&
                        dep_e->health != BRAIN_CYCLE_HEALTH_UNKNOWN &&
                        dep_e->health != BRAIN_CYCLE_HEALTH_DISABLED) {
                        issues++;
                        coord->stats.dependency_violations++;
                        fire_dependency_violated_callbacks(
                            coord, e->type, coord->dependencies[d].dependency);
                    }
                }
            }
        }
    }

    /* Update category statistics */
    update_category_stats(coord);

    /* Compute and track overall health */
    float old_overall = coord->prev_overall_health;
    float new_overall = compute_overall_health(coord);
    coord->stats.overall_health = new_overall;

    if (fabsf(new_overall - old_overall) > 0.01f) {
        coord->prev_overall_health = new_overall;
        fire_overall_health_changed_callbacks(coord, old_overall, new_overall);
    }

    /* Track health pattern */
    track_health_pattern(coord);

    /* Auto-flush cycle stats to KG at configured interval */
    if (coord->kg_dispatcher && coord->config.kg_write_interval_ms > 0) {
        uint64_t kg_elapsed_ms = (now - coord->last_kg_write_us) / 1000;
        if (kg_elapsed_ms >= coord->config.kg_write_interval_ms) {
            flush_to_kg_unlocked(coord);
        }
    }

    /* Update uptime */
    coord->stats.coordinator_uptime_ms = (now - coord->created_time_us) / 1000;

    nimcp_mutex_unlock(coord->mutex);

    return (int)issues;
}

//=============================================================================
// Public API - Query
//=============================================================================

int brain_cycle_coordinator_get_status(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    brain_cycle_status_t* status)
{
    if (!coord || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_get_status: NULL argument");
        return -1;
    }
    if ((int)type < 0 || (int)type >= BRAIN_CYCLE_COUNT) return -1;

    /* Cast away const for mutex (read-only operation) */
    nimcp_mutex_lock(((brain_cycle_coordinator_t*)coord)->mutex);

    const cycle_entry_t* e = &coord->cycles[(int)type];
    if (!e->registered) {
        nimcp_mutex_unlock(((brain_cycle_coordinator_t*)coord)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_cycle_coordinator_get_status: e->registered is NULL");
        return -1;
    }

    memset(status, 0, sizeof(*status));
    status->type = e->type;
    status->name = get_cycle_name(e->type);
    status->category = e->category;
    status->health = e->health;
    status->enabled = e->enabled;
    status->running = e->running;
    status->last_tick_us = e->last_tick_us;
    status->expected_interval_us = e->expected_interval_us;
    status->ticks_executed = e->ticks_executed;
    status->errors_encountered = e->errors_encountered;
    status->avg_duration_us = e->avg_duration_us;
    status->max_duration_us = e->max_duration_us;
    status->monitoring_weight = e->monitoring_weight;

    nimcp_mutex_unlock(((brain_cycle_coordinator_t*)coord)->mutex);
    return 0;
}

int brain_cycle_coordinator_get_all_status(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_status_t* statuses,
    uint32_t* count)
{
    if (!coord || !statuses || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_get_all_status: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((brain_cycle_coordinator_t*)coord)->mutex);

    uint32_t n = 0;
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered) continue;

        brain_cycle_status_t* s = &statuses[n];
        memset(s, 0, sizeof(*s));
        s->type = e->type;
        s->name = get_cycle_name(e->type);
        s->category = e->category;
        s->health = e->health;
        s->enabled = e->enabled;
        s->running = e->running;
        s->last_tick_us = e->last_tick_us;
        s->expected_interval_us = e->expected_interval_us;
        s->ticks_executed = e->ticks_executed;
        s->errors_encountered = e->errors_encountered;
        s->avg_duration_us = e->avg_duration_us;
        s->max_duration_us = e->max_duration_us;
        s->monitoring_weight = e->monitoring_weight;
        n++;
    }
    *count = n;

    nimcp_mutex_unlock(((brain_cycle_coordinator_t*)coord)->mutex);
    return 0;
}

int brain_cycle_coordinator_get_stats(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_coordinator_stats_t* stats)
{
    if (!coord || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_get_stats: NULL argument");
        return -1;
    }

    brain_cycle_coordinator_t* mutable_coord =
        (brain_cycle_coordinator_t*)coord;
    nimcp_mutex_lock(mutable_coord->mutex);

    /* Refresh category stats before returning */
    update_category_stats(mutable_coord);

    /* Recompute overall health */
    mutable_coord->stats.overall_health = compute_overall_health(coord);

    /* Refresh uptime */
    uint64_t now = get_timestamp_us();
    mutable_coord->stats.coordinator_uptime_ms =
        (now - mutable_coord->created_time_us) / 1000;

    *stats = coord->stats;
    nimcp_mutex_unlock(mutable_coord->mutex);
    return 0;
}

//=============================================================================
// Public API - Diagnostics
//=============================================================================

int brain_cycle_coordinator_diagnose(
    const brain_cycle_coordinator_t* coord,
    char* buffer,
    size_t buffer_size)
{
    if (!coord || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_cycle_coordinator_diagnose: required parameter is NULL (coord, buffer)");
        return -1;
    }

    nimcp_mutex_lock(((brain_cycle_coordinator_t*)coord)->mutex);

    uint32_t issues = 0;
    int offset = 0;
    int remaining = (int)buffer_size;

    offset += snprintf(buffer + offset, (size_t)remaining,
        "=== Brain Cycle Coordinator Diagnostics ===\n"
        "Uptime: %lums | Registered: %u | Health: %.2f\n\n",
        (unsigned long)((get_timestamp_us() - coord->created_time_us) / 1000),
        coord->stats.total_cycles_registered,
        coord->stats.overall_health);
    remaining = (int)buffer_size - offset;
    if (remaining <= 0) goto done;

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered) continue;

        bool is_issue = (e->health == BRAIN_CYCLE_HEALTH_STALLED ||
                         e->health == BRAIN_CYCLE_HEALTH_ERROR);
        if (is_issue) issues++;

        int written = snprintf(buffer + offset, (size_t)remaining,
            "[%s] %s | health=%s | ticks=%lu | avg=%.1fus | max=%luus%s\n",
            brain_cycle_category_name(e->category),
            get_cycle_name(e->type),
            brain_cycle_health_name(e->health),
            (unsigned long)e->ticks_executed,
            e->avg_duration_us,
            (unsigned long)e->max_duration_us,
            is_issue ? " ***ISSUE***" : "");
        offset += written;
        remaining = (int)buffer_size - offset;
        if (remaining <= 0) goto done;
    }

    if (coord->dependency_count > 0) {
        int written = snprintf(buffer + offset, (size_t)remaining,
            "\nDependencies (%u):\n", coord->dependency_count);
        offset += written;
        remaining = (int)buffer_size - offset;

        for (uint32_t d = 0; d < coord->dependency_count && remaining > 0; d++) {
            written = snprintf(buffer + offset, (size_t)remaining,
                "  %s -> %s\n",
                get_cycle_name(coord->dependencies[d].dependent),
                get_cycle_name(coord->dependencies[d].dependency));
            offset += written;
            remaining = (int)buffer_size - offset;
        }
    }

done:
    nimcp_mutex_unlock(((brain_cycle_coordinator_t*)coord)->mutex);
    return (int)issues;
}

void brain_cycle_coordinator_log_state(const brain_cycle_coordinator_t* coord) {
    if (!coord) return;

    nimcp_mutex_lock(((brain_cycle_coordinator_t*)coord)->mutex);

    LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
        "=== Brain Cycle State (registered=%u, health=%.2f) ===",
        coord->stats.total_cycles_registered,
        coord->stats.overall_health);

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered) continue;

        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "  [%s] %s: health=%s ticks=%lu avg=%.1fus err=%lu",
            brain_cycle_category_name(e->category),
            get_cycle_name(e->type),
            brain_cycle_health_name(e->health),
            (unsigned long)e->ticks_executed,
            e->avg_duration_us,
            (unsigned long)e->errors_encountered);
    }

    nimcp_mutex_unlock(((brain_cycle_coordinator_t*)coord)->mutex);
}

//=============================================================================
// Public API - Dependency Management
//=============================================================================

int brain_cycle_coordinator_add_dependency(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t dependent,
    brain_cycle_type_t dependency)
{
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_add_dependency: coord is NULL");
        return -1;
    }
    if ((int)dependent < 0 || (int)dependent >= BRAIN_CYCLE_COUNT ||
        (int)dependency < 0 || (int)dependency >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_cycle_coordinator_add_dependency: invalid cycle type");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    uint32_t max_deps = BRAIN_CYCLE_COUNT * BRAIN_CYCLE_MAX_DEPENDENCIES;
    if (coord->dependency_count >= max_deps) {
        nimcp_mutex_unlock(coord->mutex);
        LOG_MODULE_WARN(CYCLE_COORD_LOG_MODULE,
            "Dependency array full (%u/%u)", coord->dependency_count, max_deps);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_cycle_coordinator_add_dependency: capacity exceeded");
        return -1;
    }

    coord->dependencies[coord->dependency_count].dependent = dependent;
    coord->dependencies[coord->dependency_count].dependency = dependency;
    coord->dependency_count++;

    nimcp_mutex_unlock(coord->mutex);

    if (coord->config.enable_logging) {
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE,
            "Added dependency: %s -> %s",
            get_cycle_name(dependent), get_cycle_name(dependency));
    }

    return 0;
}

int brain_cycle_coordinator_check_dependencies(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    bool* satisfied)
{
    if (!coord || !satisfied) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_check_dependencies: NULL argument");
        return -1;
    }

    *satisfied = true;

    nimcp_mutex_lock(((brain_cycle_coordinator_t*)coord)->mutex);

    for (uint32_t i = 0; i < coord->dependency_count; i++) {
        if (coord->dependencies[i].dependent != type) continue;

        int dep_idx = (int)coord->dependencies[i].dependency;
        if (dep_idx >= 0 && dep_idx < BRAIN_CYCLE_COUNT) {
            const cycle_entry_t* dep_e = &coord->cycles[dep_idx];
            if (dep_e->registered &&
                dep_e->health != BRAIN_CYCLE_HEALTH_HEALTHY &&
                dep_e->health != BRAIN_CYCLE_HEALTH_UNKNOWN &&
                dep_e->health != BRAIN_CYCLE_HEALTH_DISABLED) {
                *satisfied = false;
                break;
            }
        }
    }

    nimcp_mutex_unlock(((brain_cycle_coordinator_t*)coord)->mutex);
    return 0;
}

//=============================================================================
// Public API - Callback Registration
//=============================================================================

int brain_cycle_coordinator_register_callbacks(
    brain_cycle_coordinator_t* coord,
    const brain_cycle_coordinator_callbacks_t* cbs)
{
    if (!coord || !cbs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_register_callbacks: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    if (coord->callback_count >= BRAIN_CYCLE_MAX_CALLBACKS) {
        nimcp_mutex_unlock(coord->mutex);
        LOG_MODULE_WARN(CYCLE_COORD_LOG_MODULE,
            "Callback array full (%u/%u)",
            coord->callback_count, (uint32_t)BRAIN_CYCLE_MAX_CALLBACKS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brain_cycle_coordinator_register_callbacks: capacity exceeded");
        return -1;
    }

    coord->callbacks[coord->callback_count++] = *cbs;

    nimcp_mutex_unlock(coord->mutex);

    if (coord->config.enable_logging) {
        LOG_MODULE_DEBUG(CYCLE_COORD_LOG_MODULE,
            "Registered callback set (total=%u)", coord->callback_count);
    }

    return 0;
}

int brain_cycle_coordinator_unregister_callbacks(
    brain_cycle_coordinator_t* coord,
    void* user_data)
{
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_unregister_callbacks: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    bool found = false;
    for (uint32_t i = 0; i < coord->callback_count; i++) {
        if (coord->callbacks[i].user_data == user_data) {
            for (uint32_t j = i; j + 1 < coord->callback_count; j++) {
                coord->callbacks[j] = coord->callbacks[j + 1];
            }
            coord->callback_count--;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(coord->mutex);
    return found ? 0 : -1;
}

//=============================================================================
// Public API - Integration Connection Functions
//=============================================================================

#define CONNECT_IMPL(func_name, field_name, type_name, log_name) \
int func_name( \
    brain_cycle_coordinator_t* coord, \
    type_name* ptr) \
{ \
    if (!coord) { \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, \
            #func_name ": coord is NULL"); \
        return -1; \
    } \
    nimcp_mutex_lock(coord->mutex); \
    coord->field_name = ptr; \
    nimcp_mutex_unlock(coord->mutex); \
    if (coord->config.enable_logging) { \
        LOG_MODULE_INFO(CYCLE_COORD_LOG_MODULE, \
            "Connected to " log_name); \
    } \
    return 0; \
}

CONNECT_IMPL(brain_cycle_coordinator_connect_bio_async, bio_context,
             bio_module_context_t, "bio-async")
CONNECT_IMPL(brain_cycle_coordinator_connect_immune, immune_system,
             brain_immune_system_t, "immune system")
CONNECT_IMPL(brain_cycle_coordinator_connect_kg, kg_dispatcher,
             kg_io_dispatcher_t, "KG I/O dispatcher")
CONNECT_IMPL(brain_cycle_coordinator_connect_introspection, introspection_ctx,
             introspection_context_t, "introspection")
CONNECT_IMPL(brain_cycle_coordinator_connect_hemispheric, hemispheric_brain,
             hemispheric_brain_t, "hemispheric brain")
CONNECT_IMPL(brain_cycle_coordinator_connect_fep, fep_bridge,
             oscillations_fep_bridge_t, "FEP bridge")
CONNECT_IMPL(brain_cycle_coordinator_connect_meta_learning, meta_bridge,
             meta_learning_substrate_bridge_t, "meta-learning")
CONNECT_IMPL(brain_cycle_coordinator_connect_pink_noise, pink_noise_bridge,
             sfa_pink_noise_bridge_t, "pink noise")
CONNECT_IMPL(brain_cycle_coordinator_connect_global_workspace, gw_bridge,
             snn_global_workspace_bridge_t, "global workspace")
CONNECT_IMPL(brain_cycle_coordinator_connect_attention, attention_bridge,
             snn_attention_bridge_t, "attention")
CONNECT_IMPL(brain_cycle_coordinator_connect_world_model, world_model,
             world_model_multimodal_t, "world model")

#undef CONNECT_IMPL

//=============================================================================
// Public API - Persistence
//=============================================================================

/**
 * @brief Internal KG flush (must be called with mutex held)
 */
static int flush_to_kg_unlocked(brain_cycle_coordinator_t* coord) {
    if (!coord->kg_dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flush_to_kg_unlocked: coord->kg_dispatcher is NULL");
        return -1;
    }

    coord->last_kg_write_us = get_timestamp_us();

    /* Format ILP lines for each registered cycle and write to KG */
    char line_buf[NIMCP_ERROR_BUFFER_LARGE];
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const cycle_entry_t* e = &coord->cycles[i];
        if (!e->registered) continue;

        int len = snprintf(line_buf, sizeof(line_buf),
            "%s,cycle=%s health=%di,ticks=%luu,avg_duration=%.2f,max_duration=%luu %lu",
            coord->kg_table_name,
            get_cycle_name(e->type),
            (int)e->health,
            (unsigned long)e->ticks_executed,
            e->avg_duration_us,
            (unsigned long)e->max_duration_us,
            (unsigned long)(coord->last_kg_write_us / 1000));
        if (len > 0) {
            kg_io_write_async(coord->kg_dispatcher, coord->kg_table_name,
                              line_buf, (size_t)len, NULL, NULL);
        }
    }

    if (coord->config.enable_debug_logging) {
        LOG_MODULE_DEBUG(CYCLE_COORD_LOG_MODULE, "Flushed cycle stats to KG");
    }

    return 0;
}

int brain_cycle_coordinator_flush_to_kg(brain_cycle_coordinator_t* coord) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_cycle_coordinator_flush_to_kg: coord is NULL");
        return -1;
    }
    if (!coord->kg_dispatcher) {
        return -1; /* KG not connected */
    }

    nimcp_mutex_lock(coord->mutex);
    int rc = flush_to_kg_unlocked(coord);
    nimcp_mutex_unlock(coord->mutex);

    return rc;
}

//=============================================================================
// Public API - Utility Functions
//=============================================================================

const char* brain_cycle_type_name(brain_cycle_type_t type) {
    return get_cycle_name(type);
}

const char* brain_cycle_health_name(brain_cycle_health_t health) {
    switch (health) {
    case BRAIN_CYCLE_HEALTH_UNKNOWN:  return "UNKNOWN";
    case BRAIN_CYCLE_HEALTH_HEALTHY:  return "HEALTHY";
    case BRAIN_CYCLE_HEALTH_DEGRADED: return "DEGRADED";
    case BRAIN_CYCLE_HEALTH_STALLED:  return "STALLED";
    case BRAIN_CYCLE_HEALTH_ERROR:    return "ERROR";
    case BRAIN_CYCLE_HEALTH_DISABLED: return "DISABLED";
    default:                          return "UNKNOWN";
    }
}

const char* brain_cycle_category_name(brain_cycle_category_t category) {
    switch (category) {
    case BRAIN_CYCLE_CATEGORY_FAST:       return "FAST";
    case BRAIN_CYCLE_CATEGORY_MEDIUM:     return "MEDIUM";
    case BRAIN_CYCLE_CATEGORY_SLOW:       return "SLOW";
    case BRAIN_CYCLE_CATEGORY_BACKGROUND: return "BACKGROUND";
    default:                              return "UNKNOWN";
    }
}

brain_cycle_category_t brain_cycle_get_category(brain_cycle_type_t type) {
    return get_cycle_category(type);
}

uint64_t brain_cycle_get_default_interval_us(brain_cycle_type_t type) {
    return get_default_interval_us(type);
}
