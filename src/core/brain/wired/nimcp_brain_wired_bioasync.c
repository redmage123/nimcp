/**
 * @file nimcp_brain_wired_bioasync.c
 * @brief Wired-module group BRAIN_WIRED_BIOASYNC (dead-code audit 2026-11-24).
 *        See include/core/brain/nimcp_brain_wired.h for the lifecycle contract.
 *
 * WHAT: Connects eleven per-domain bio-async bridges to the brain's live
 *       bio-router (brain->bio_router, the process-global router created in
 *       nimcp_brain_init_bio_async_orchestrator.c) and drives them from the
 *       training tick:
 *         async_integration  - async task/promise coordinator (started, inbox drained)
 *         core               - core brain bridge (handlers: state query, health check)
 *         glial              - mirrors brain->glial counters (glial_integration_get_stats)
 *         gpu                - only when brain->gpu_enabled; host-side only, no GPU
 *                              allocation; mirrors gpu_ctx free memory
 *         language           - attached to brain->language_layer (orchestrator)
 *         perception         - only when a visual/audio/speech cortex exists
 *         quantum            - only when a quantum subsystem exists
 *         security           - only when BBB or security integration exists;
 *                              auto-lockdown and HEALTH_CHECK heartbeats disabled
 *         sleep              - mirrors brain->sleep_system; broadcasts
 *                              BIO_MSG_SLEEP_STAGE_CHANGE on real transitions and a
 *                              periodic BIO_MSG_ADENOSINE_UPDATE (pressure)
 *         swarm              - only when collective cognition (multi-brain) is live
 *         utils              - utilities bridge (heartbeat disabled)
 *
 * WHY:  These bridges were never compiled. Each now has a module registered on
 *       the router under a dedicated ID (0x7B00-0x7BFF), so live broadcasts
 *       reach it, and its inbox is drained here like brain_tick_bio_async()
 *       drains the region adapters.
 *
 * HOW:  tick() is rate-limited: inboxes drained every BIOASYNC_DRAIN_EVERY ticks
 *       (16 msgs per bridge), state sync + bridge updates every
 *       BIOASYNC_SYNC_EVERY ticks. Outbound broadcasts from the tick are limited
 *       to message types with real values behind them (sleep). Bridges whose
 *       message layouts overlap live handlers (glial calcium-wave / metabolic,
 *       core brain-state, security health-check) are inbound-only here.
 *
 * Not wired: src/middleware/events/nimcp_event_bus_async.c — built against the
 * removed middleware event bus (event_t / subscription_config_t), whose
 * function names (event_bus_publish/subscribe/get_stats/unsubscribe) are now
 * exported by core/events/nimcp_event_bus.c with incompatible signatures.
 *
 * No learned state: save() writes nothing, load() reports "fresh" (rc 1).
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"

#include "async/integration/nimcp_async_integration_bridge.h"
#include "core/integration/nimcp_core_bio_async_bridge.h"
#include "glial/integration/nimcp_glial_bio_async_bridge.h"
#include "gpu/integration/nimcp_gpu_bio_async_bridge.h"
#include "language/integration/nimcp_language_bio_async_bridge.h"
#include "perception/integration/nimcp_perception_bio_async_bridge.h"
#include "quantum/integration/nimcp_quantum_bio_async_bridge.h"
#include "security/integration/nimcp_security_bio_async_bridge.h"
#include "sleep/integration/nimcp_sleep_bio_async_bridge.h"
#include "swarm/integration/nimcp_swarm_bio_async_bridge.h"
#include "utils/integration/nimcp_utils_bio_async_bridge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Inbox drain cadence and budget (per bridge). 60 Hz tick -> 15 Hz drain. */
#define BIOASYNC_DRAIN_EVERY   4u
#define BIOASYNC_DRAIN_BUDGET  16u
/* State sync / bridge update cadence: ~2 s at 16.6 ms per tick. */
#define BIOASYNC_SYNC_EVERY    120u

enum {
    B_ASYNC = 0, B_CORE, B_GLIAL, B_GPU, B_LANGUAGE, B_PERCEPTION,
    B_QUANTUM, B_SECURITY, B_SLEEP, B_SWARM, B_UTILS, B_COUNT
};

static const char* const k_bridge_names[B_COUNT] = {
    "async_integration", "core", "glial", "gpu", "language", "perception",
    "quantum", "security", "sleep", "swarm", "utils"
};

typedef struct {
    async_integration_t*        async_integ;
    core_bio_async_bridge_t*    core;
    glial_bio_async_bridge_t*   glial;
    gpu_bio_bridge_t*           gpu;
    language_bio_bridge_t*      language;
    perception_bio_bridge_t*    perception;
    quantum_bio_async_bridge_t* quantum;
    security_bio_bridge_t*      security;
    sleep_bio_async_bridge_t*   sleep;
    swarm_bio_bridge_t*         swarm;
    utils_bio_bridge_t*         utils;

    uint64_t ticks;
    float    dt_accum_ms;               /* dt since last sync */
    uint64_t drained[B_COUNT];          /* messages drained per bridge */
    uint64_t syncs;
    uint32_t sleep_transitions;
    uint32_t glial_astrocytes;          /* last mirrored glial counters */
    uint32_t glial_microglia;
    uint64_t gpu_free_bytes;            /* last mirrored GPU free memory */
} bioasync_state_t;

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* All base-first bridges keep their router context in base.bio_ctx. */
static bool base_registered(const void* bridge) {
    return bridge && ((const bridge_base_t*)bridge)->bio_ctx != NULL;
}

static bool bridge_registered(const bioasync_state_t* s, int b) {
    switch (b) {
        case B_ASYNC:      return s->async_integ && s->async_integ->bio_context != NULL;
        case B_CORE:       return base_registered(s->core);
        case B_GLIAL:      return base_registered(s->glial);
        case B_GPU:        return base_registered(s->gpu);
        case B_LANGUAGE:   return s->language && language_bio_bridge_get_context(s->language) != NULL;
        case B_PERCEPTION: return base_registered(s->perception);
        case B_QUANTUM:    return s->quantum && quantum_bio_async_get_context(s->quantum) != NULL;
        case B_SECURITY:   return base_registered(s->security);
        case B_SLEEP:      return base_registered(s->sleep);
        case B_SWARM:      return base_registered(s->swarm);
        case B_UTILS:      return base_registered(s->utils);
        default:           return false;
    }
}

static bool bridge_present(const bioasync_state_t* s, int b) {
    switch (b) {
        case B_ASYNC:      return s->async_integ != NULL;
        case B_CORE:       return s->core != NULL;
        case B_GLIAL:      return s->glial != NULL;
        case B_GPU:        return s->gpu != NULL;
        case B_LANGUAGE:   return s->language != NULL;
        case B_PERCEPTION: return s->perception != NULL;
        case B_QUANTUM:    return s->quantum != NULL;
        case B_SECURITY:   return s->security != NULL;
        case B_SLEEP:      return s->sleep != NULL;
        case B_SWARM:      return s->swarm != NULL;
        case B_UTILS:      return s->utils != NULL;
        default:           return false;
    }
}

static void add_drained(bioasync_state_t* s, int b, long n) {
    if (n > 0) {
        s->drained[b] += (uint64_t)n;
    }
}

static void destroy_bridges(bioasync_state_t* s) {
    /* Each destroy disconnects (unregisters its router module) first. */
    if (s->swarm)       { swarm_bio_bridge_destroy(s->swarm);            s->swarm = NULL; }
    if (s->gpu)         { gpu_bio_bridge_destroy(s->gpu);                s->gpu = NULL; }
    if (s->sleep)       { sleep_bio_async_bridge_destroy(s->sleep);      s->sleep = NULL; }
    if (s->security)    { security_bio_bridge_destroy(s->security);      s->security = NULL; }
    if (s->quantum)     { quantum_bio_async_bridge_destroy(s->quantum);  s->quantum = NULL; }
    if (s->perception)  { perception_bio_async_bridge_destroy(s->perception); s->perception = NULL; }
    if (s->language)    { language_bio_bridge_destroy(s->language);      s->language = NULL; }
    if (s->glial)       { glial_bio_async_bridge_destroy(s->glial);      s->glial = NULL; }
    if (s->core)        { core_bio_async_bridge_destroy(s->core);        s->core = NULL; }
    if (s->utils)       { utils_bio_bridge_destroy(s->utils);            s->utils = NULL; }
    if (s->async_integ) { async_integration_destroy(s->async_integ);     s->async_integ = NULL; }
}

/* --------------------------------------------------------------------------
 * Per-bridge creation (each NULL-safe; a failure leaves that slot NULL)
 * -------------------------------------------------------------------------- */

static void create_async_integration(bioasync_state_t* s) {
    async_integration_config_t cfg;
    if (async_integration_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_bio_async = true;
    cfg.enable_logging = false;
    s->async_integ = async_integration_create(&cfg);
    if (!s->async_integ) {
        return;
    }
    /* start() connects to the router when enable_bio_async is set. */
    if (async_integration_start(s->async_integ) != 0 || !s->async_integ->bio_async_connected) {
        async_integration_destroy(s->async_integ);
        s->async_integ = NULL;
    }
}

static void create_core(bioasync_state_t* s, bio_router_t router) {
    core_bio_async_config_t cfg;
    if (core_bio_async_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_auto_broadcast = false;   /* BRAIN_STATE_RESPONSE has live handlers */
    cfg.enable_logging = false;
    s->core = core_bio_async_bridge_create(&cfg);
    if (s->core && core_bio_async_connect(s->core, router) != 0) {
        core_bio_async_bridge_destroy(s->core);
        s->core = NULL;
    }
}

static void create_glial(bioasync_state_t* s, brain_t brain, bio_router_t router) {
    if (!brain->glial) {
        return;
    }
    glial_bio_async_config_t cfg;
    if (glial_bio_async_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_auto_broadcast = false;
    s->glial = glial_bio_async_bridge_create(&cfg);
    if (s->glial && glial_bio_async_connect(s->glial, brain->glial, router) != 0) {
        glial_bio_async_bridge_destroy(s->glial);
        s->glial = NULL;
    }
}

static void create_gpu(bioasync_state_t* s, brain_t brain) {
    if (!brain->gpu_enabled || !brain->gpu_ctx) {
        return;
    }
    gpu_bio_bridge_config_t cfg;
    if (gpu_bio_bridge_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_broadcast = false;
    /* multigpu_ctx NULL -> single-device mode; create only mallocs host arrays. */
    s->gpu = gpu_bio_bridge_create(&cfg, NULL);
    if (s->gpu && gpu_bio_bridge_connect_bio_async(s->gpu) != NIMCP_SUCCESS) {
        gpu_bio_bridge_destroy(s->gpu);
        s->gpu = NULL;
    }
}

static void create_language(bioasync_state_t* s, brain_t brain, bio_router_t router) {
    if (!brain->language_layer) {
        return;
    }
    language_bio_bridge_config_t cfg;
    if (language_bio_bridge_default_config(&cfg) != 0) {
        return;
    }
    s->language = language_bio_bridge_create(&cfg);
    if (s->language &&
        language_bio_bridge_connect(s->language,
                                    (language_orchestrator_t*)brain->language_layer,
                                    router) != 0) {
        language_bio_bridge_destroy(s->language);
        s->language = NULL;
    }
}

static void create_perception(bioasync_state_t* s, brain_t brain, bio_router_t router) {
    if (!brain->visual_cortex && !brain->audio_cortex && !brain->speech_cortex) {
        return;
    }
    perception_bio_bridge_config_t cfg;
    if (perception_bio_async_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_auto_broadcast = false;
    s->perception = perception_bio_async_bridge_create(&cfg);
    if (s->perception && perception_bio_async_connect(s->perception, router) != 0) {
        perception_bio_async_bridge_destroy(s->perception);
        s->perception = NULL;
    }
}

static void create_quantum(bioasync_state_t* s, brain_t brain, bio_router_t router) {
    if (!brain->quantum_annealer && !brain->quantum_reasoner && !brain->quantum_shannon_diffusion) {
        return;
    }
    quantum_bio_async_config_t cfg;
    if (quantum_bio_async_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_auto_broadcast = false;   /* no live coherence value to report */
    s->quantum = quantum_bio_async_bridge_create(&cfg);
    if (s->quantum && quantum_bio_async_connect(s->quantum, router) != 0) {
        quantum_bio_async_bridge_destroy(s->quantum);
        s->quantum = NULL;
    }
}

static void create_security(bioasync_state_t* s, brain_t brain, bio_router_t router) {
    if (!brain->bbb_system && !brain->security_integration) {
        return;
    }
    security_bio_bridge_config_t cfg;
    if (security_bio_bridge_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_auto_broadcast = false;     /* heartbeat = BIO_MSG_HEALTH_CHECK (live handlers) */
    cfg.enable_metrics_broadcast = false;
    cfg.enable_auto_lockdown = false;      /* never lock down the live brain from here */
    s->security = security_bio_bridge_create(&cfg);
    if (s->security && security_bio_bridge_connect(s->security, router) != 0) {
        security_bio_bridge_destroy(s->security);
        s->security = NULL;
    }
}

static void create_sleep(bioasync_state_t* s, brain_t brain, bio_router_t router) {
    if (!brain->sleep_system) {
        return;
    }
    sleep_bio_async_config_t cfg;
    if (sleep_bio_async_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_auto_broadcast = true;              /* ADENOSINE_UPDATE: real pressure */
    cfg.state_broadcast_interval_ms = 2000;
    s->sleep = sleep_bio_async_bridge_create(&cfg);
    if (s->sleep && sleep_bio_async_connect(s->sleep, brain->sleep_system, router) != 0) {
        sleep_bio_async_bridge_destroy(s->sleep);
        s->sleep = NULL;
    }
}

static void create_swarm(bioasync_state_t* s, brain_t brain) {
    /* Only meaningful when this brain participates in a multi-brain collective. */
    if (!brain->collective_cognition || !brain->collective_cognition_enabled) {
        return;
    }
    swarm_bio_bridge_config_t cfg;
    swarm_bio_bridge_default_config(&cfg);
    cfg.auto_connect_bio_async = true;
    cfg.inbox_capacity = 64;
    s->swarm = swarm_bio_bridge_create(&cfg);
    if (s->swarm && !swarm_bio_bridge_is_bio_async_connected(s->swarm)) {
        swarm_bio_bridge_destroy(s->swarm);
        s->swarm = NULL;
    }
}

static void create_utils(bioasync_state_t* s, bio_router_t router) {
    utils_bio_bridge_config_t cfg;
    if (utils_bio_bridge_default_config(&cfg) != 0) {
        return;
    }
    cfg.enable_heartbeat = false;
    cfg.enable_auto_broadcast = false;
    s->utils = utils_bio_bridge_create(&cfg);
    if (s->utils && utils_bio_bridge_connect(s->utils, router) != 0) {
        utils_bio_bridge_destroy(s->utils);
        s->utils = NULL;
    }
}

/* --------------------------------------------------------------------------
 * Lifecycle hooks
 * -------------------------------------------------------------------------- */

int brain_wired_bioasync_create(struct brain_struct* brain) {
    if (!brain) {
        return -1;
    }
    if (brain->wired[BRAIN_WIRED_BIOASYNC]) {
        return 0;
    }
    /* Brains restored by brain_load never run the factory's bio-async waves,
     * so brain->bio_router is NULL on every resumed (production) brain even
     * though the router is a process-wide singleton. Attach to it here,
     * initializing it if nothing has yet. */
    if (!brain->bio_router) {
        if (bio_router_is_initialized() || bio_router_init(NULL) == NIMCP_SUCCESS) {
            brain->bio_router = bio_router_get_global();
            brain->bio_router_enabled = (brain->bio_router != NULL);
        }
    }
    bio_router_t router = (bio_router_t)brain->bio_router;
    if (!brain->bio_router_enabled || !router || !bio_router_is_initialized()) {
        fprintf(stderr, "[WIRED] BIOASYNC: no live bio-router on this brain, skipping\n");
        return -1;
    }

    bioasync_state_t* s = nimcp_calloc(1, sizeof(*s));
    if (!s) {
        return -1;
    }

    create_async_integration(s);
    create_core(s, router);
    create_glial(s, brain, router);
    create_gpu(s, brain);
    create_language(s, brain, router);
    create_perception(s, brain, router);
    create_quantum(s, brain, router);
    create_security(s, brain, router);
    create_sleep(s, brain, router);
    create_swarm(s, brain);
    create_utils(s, router);

    int registered = 0;
    for (int b = 0; b < B_COUNT; b++) {
        if (bridge_registered(s, b)) {
            registered++;
        }
    }
    if (registered == 0) {
        destroy_bridges(s);
        nimcp_free(s);
        return -1;
    }

    brain->wired[BRAIN_WIRED_BIOASYNC] = s;
    fprintf(stderr, "[WIRED] BIOASYNC: %d bridge(s) registered on the bio-router\n", registered);
    return 0;
}

static void bioasync_drain(bioasync_state_t* s) {
    const uint32_t n = BIOASYNC_DRAIN_BUDGET;
    if (s->async_integ) add_drained(s, B_ASYNC, (long)async_integration_process_messages(s->async_integ, n));
    if (s->core && core_bio_async_is_connected(s->core))
        add_drained(s, B_CORE, core_bio_async_process_inbox(s->core, n));
    if (s->glial && glial_bio_async_is_connected(s->glial))
        add_drained(s, B_GLIAL, glial_bio_async_process_inbox(s->glial, n));
    if (s->gpu) add_drained(s, B_GPU, (long)gpu_bio_bridge_process_messages(s->gpu, n));
    if (s->language && language_bio_bridge_is_connected(s->language))
        add_drained(s, B_LANGUAGE, language_bio_bridge_process_inbox(s->language, n));
    if (s->perception && perception_bio_async_is_connected(s->perception))
        add_drained(s, B_PERCEPTION, perception_bio_async_process_inbox(s->perception, n));
    if (s->quantum && quantum_bio_async_is_connected(s->quantum))
        add_drained(s, B_QUANTUM, quantum_bio_async_process_inbox(s->quantum, n));
    if (s->security && security_bio_bridge_is_connected(s->security))
        add_drained(s, B_SECURITY, security_bio_bridge_process_inbox(s->security, n));
    if (s->sleep && sleep_bio_async_is_connected(s->sleep))
        add_drained(s, B_SLEEP, sleep_bio_async_process_inbox(s->sleep, n));
    if (s->swarm) add_drained(s, B_SWARM, swarm_bio_bridge_process_messages(s->swarm, n));
    if (s->utils && utils_bio_bridge_is_connected(s->utils))
        add_drained(s, B_UTILS, utils_bio_bridge_process_inbox(s->utils, n));
}

static void bioasync_sync(bioasync_state_t* s, brain_t brain) {
    float dt = s->dt_accum_ms;
    s->dt_accum_ms = 0.0f;
    if (!isfinite(dt) || dt < 0.0f) dt = 0.0f;
    if (dt > 60000.0f) dt = 60000.0f;
    uint32_t dms = (uint32_t)dt;
    s->syncs++;

    /* Sleep: mirror real stage/pressure, broadcast stage change on transition,
     * then update() emits the periodic adenosine broadcast. */
    if (s->sleep && sleep_bio_async_is_connected(s->sleep)) {
        if (sleep_bio_async_sync_from_system(s->sleep) > 0) {
            s->sleep_transitions++;
        }
        (void)sleep_bio_async_update(s->sleep, dms);
    }

    /* Glial: mirror live counters into the bridge (inbound-only otherwise). */
    if (s->glial && glial_bio_async_is_connected(s->glial)) {
        glial_integration_stats_t gs;
        if (glial_bio_async_sync_from_integration(s->glial, &gs) == 0) {
            s->glial_astrocytes = gs.num_astrocytes;
            s->glial_microglia = gs.num_microglia;
        }
        (void)glial_bio_async_update(s->glial, dms);
    }

    /* GPU: host-side bookkeeping only (no CUDA calls). */
    if (s->gpu && brain->gpu_ctx && s->gpu->device_count > 0 && s->gpu->device_memory_free) {
        const nimcp_gpu_context_t* g = (const nimcp_gpu_context_t*)brain->gpu_ctx;
        uint64_t total = (uint64_t)g->total_memory;
        uint64_t used = (uint64_t)g->allocated_memory;
        s->gpu_free_bytes = total > used ? total - used : 0;
        s->gpu->device_memory_free[0] = s->gpu_free_bytes;
    }

    if (s->async_integ && s->async_integ->running) {
        (void)async_integration_update(s->async_integ, dt);
    }
    if (s->core && core_bio_async_is_connected(s->core)) {
        (void)core_bio_async_update(s->core, dms);
    }
    if (s->language && language_bio_bridge_is_connected(s->language)) {
        (void)language_bio_bridge_update(s->language, dms);
    }
    if (s->perception && perception_bio_async_is_connected(s->perception)) {
        (void)perception_bio_async_update(s->perception, dms);
    }
    if (s->quantum && quantum_bio_async_is_connected(s->quantum)) {
        (void)quantum_bio_async_update(s->quantum, dms);
    }
    if (s->security && security_bio_bridge_is_connected(s->security)) {
        (void)security_bio_bridge_update(s->security, dms);
    }
    if (s->swarm) {
        (void)swarm_bio_bridge_update(s->swarm, dms);
    }
    if (s->utils && utils_bio_bridge_is_connected(s->utils)) {
        (void)utils_bio_bridge_update(s->utils, dms);
    }
}

void brain_wired_bioasync_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    (void)f;
    (void)n;
    if (!brain) {
        return;
    }
    bioasync_state_t* s = (bioasync_state_t*)brain->wired[BRAIN_WIRED_BIOASYNC];
    if (!s) {
        return;
    }
    s->ticks++;
    if (isfinite(dt_ms) && dt_ms > 0.0f) {
        s->dt_accum_ms += dt_ms;
    }
    if (s->ticks % BIOASYNC_DRAIN_EVERY == 0) {
        bioasync_drain(s);
    }
    if (s->ticks % BIOASYNC_SYNC_EVERY == 0) {
        bioasync_sync(s, brain);
    }
}

int brain_wired_bioasync_save(struct brain_struct* brain, const char* path) {
    (void)brain;
    (void)path;
    return 0;   /* bridges hold routing state only; nothing learned to persist */
}

int brain_wired_bioasync_load(struct brain_struct* brain, const char* path) {
    (void)brain;
    (void)path;
    return 1;   /* fresh state */
}

void brain_wired_bioasync_destroy(struct brain_struct* brain) {
    if (!brain) {
        return;
    }
    bioasync_state_t* s = (bioasync_state_t*)brain->wired[BRAIN_WIRED_BIOASYNC];
    brain->wired[BRAIN_WIRED_BIOASYNC] = NULL;
    if (!s) {
        return;
    }
    destroy_bridges(s);
    nimcp_free(s);
}

int brain_wired_bioasync_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    if (!buf || cap == 0) {
        return 0;
    }
    bioasync_state_t* s = brain ? (bioasync_state_t*)brain->wired[BRAIN_WIRED_BIOASYNC] : NULL;
    if (!s) {
        return snprintf(buf, cap, "null");
    }
    uint32_t off = 0;
    int w = snprintf(buf, cap,
                     "{\"ticks\":%llu,\"syncs\":%llu,\"sleep_transitions\":%u,"
                     "\"glial_astrocytes\":%u,\"glial_microglia\":%u,\"gpu_free_bytes\":%llu,"
                     "\"bridges\":{",
                     (unsigned long long)s->ticks, (unsigned long long)s->syncs,
                     s->sleep_transitions, s->glial_astrocytes, s->glial_microglia,
                     (unsigned long long)s->gpu_free_bytes);
    if (w < 0 || (uint32_t)w >= cap) {
        return w;
    }
    off = (uint32_t)w;
    bool first = true;
    for (int b = 0; b < B_COUNT; b++) {
        if (!bridge_present(s, b)) {
            continue;
        }
        w = snprintf(buf + off, cap - off, "%s\"%s\":{\"registered\":%s,\"drained\":%llu}",
                     first ? "" : ",", k_bridge_names[b],
                     bridge_registered(s, b) ? "true" : "false",
                     (unsigned long long)s->drained[b]);
        if (w < 0 || (uint32_t)w >= cap - off) {
            return (int)(off + (uint32_t)(w < 0 ? 0 : w));
        }
        off += (uint32_t)w;
        first = false;
    }
    w = snprintf(buf + off, cap - off, "}}");
    if (w < 0) {
        return (int)off;
    }
    return (int)(off + (uint32_t)w);
}
