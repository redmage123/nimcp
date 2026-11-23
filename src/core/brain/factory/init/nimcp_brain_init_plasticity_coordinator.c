//=============================================================================
// nimcp_brain_init_plasticity_coordinator.c - Plasticity Coordinator Init
//=============================================================================
/**
 * @file nimcp_brain_init_plasticity_coordinator.c
 * @brief Plasticity Coordinator subsystem initialization for brain
 *
 * WHAT: Plasticity Coordinator initialization and mechanism registration
 * WHY:  Unified management of 8 plasticity mechanisms with conflict resolution
 * HOW:  Creates coordinator, connects bio-async and immune, registers mechanisms
 *
 * BIOLOGICAL BASIS:
 * The brain employs multiple plasticity mechanisms at different timescales:
 * - Fast (ms): STP - vesicle depletion/facilitation
 * - Medium (100ms-1s): STDP, BCM - spike-timing and rate-based rules
 * - Slow (minutes-hours): Homeostatic, synaptic scaling
 * This coordinator resolves conflicts when mechanisms disagree (STDP→LTP vs BCM→LTD).
 *
 * DESIGN PATTERNS:
 * - Factory: Created by brain factory during lifecycle init
 * - Registry: Stores and manages plasticity mechanisms
 * - Strategy: Configurable conflict resolution strategies
 * - Mediator: Coordinates cross-mechanism interactions
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_PLASTICITY_COORD"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/time/nimcp_time.h"

#include <pthread.h>

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_plasticity_coordinator, MESH_ADAPTER_CATEGORY_SYSTEM)

//=============================================================================
// Structural plasticity update adapter (real elapsed time)
//=============================================================================

/* WHY: every caller of plasticity_coordinator_update() passes a fixed dt
 * (1.0f, or even a prediction error), and structural_plasticity_update()
 * treats it as seconds, so each 100 ms tick advanced spine maturation/decay
 * by a full second (10x too fast). The adapter measures the true monotonic
 * elapsed time per structural system instead of trusting the caller's dt. */
#define SP_DT_SLOTS        16
#define SP_DT_MAX_SEC      60.0f   /* cap = one maturation period; bounds stale/first-call gaps */
#define SP_DT_DEFAULT_SEC  0.1f    /* registered update interval (100 ms) */

static pthread_mutex_t g_sp_dt_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { void* system; uint64_t last_us; } g_sp_dt_slots[SP_DT_SLOTS];

static int structural_plasticity_update_realtime(void* handle, float dt_unused) {
    (void)dt_unused;
    uint64_t now_us = nimcp_time_monotonic_us();
    float dt_sec = SP_DT_DEFAULT_SEC;

    pthread_mutex_lock(&g_sp_dt_lock);
    int slot = -1, free_slot = -1;
    for (int i = 0; i < SP_DT_SLOTS; i++) {
        if (g_sp_dt_slots[i].system == handle) { slot = i; break; }
        if (!g_sp_dt_slots[i].system && free_slot < 0) free_slot = i;
    }
    if (slot >= 0) {
        uint64_t last = g_sp_dt_slots[slot].last_us;
        dt_sec = (now_us > last) ? (float)((double)(now_us - last) / 1e6) : 0.0f;
        if (dt_sec > SP_DT_MAX_SEC) dt_sec = SP_DT_MAX_SEC;
        g_sp_dt_slots[slot].last_us = now_us;
    } else if (free_slot >= 0) {
        /* First call for this system: assume one registered interval elapsed. */
        g_sp_dt_slots[free_slot].system = handle;
        g_sp_dt_slots[free_slot].last_us = now_us;
    }
    /* Table full: fall back to the registered interval (still far closer
     * to reality than the caller's fixed 1.0 s). */
    pthread_mutex_unlock(&g_sp_dt_lock);

    return structural_plasticity_update((structural_plasticity_system_t*)handle, dt_sec);
}


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize plasticity coordinator subsystem
 *
 * WHAT: Creates and configures plasticity coordinator for brain
 * WHY:  Centralizes management of all plasticity mechanisms
 * HOW:  Guard clause checks, create coordinator, wire dependencies
 *
 * @param brain Brain instance to initialize coordinator for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain_t brain) {
    /* Guard clause: NULL check */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_plasticity_coordinator_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields to defaults */
    brain->plasticity_coordinator = NULL;
    brain->plasticity_coordinator_enabled = false;

    /* Check if plasticity is relevant for this brain */
    /* Enable if we have any plasticity-related subsystems */
    bool should_enable = brain->bio_async_enabled || brain->immune_enabled;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Plasticity coordinator skipped (no dependencies)");
        return true;
    }

    /* Create coordinator configuration */
    plasticity_coordinator_config_t config;
    plasticity_coordinator_default_config(&config);

    /* Configure based on brain settings */
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_brain_immune = brain->immune_enabled;
    config.enable_statistics = true;
    config.enable_logging = true;
    config.enable_energy_tracking = true;

    /* Create coordinator */
    plasticity_coordinator_t* coord = plasticity_coordinator_create(&config);
    if (!coord) {
        NIMCP_LOGGING_WARN("Failed to create plasticity coordinator - "
                          "continuing without plasticity coordination");
        return true;  /* Non-fatal */
    }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        if (plasticity_coordinator_connect_brain_immune(coord, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Plasticity coordinator connected to brain immune");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect plasticity coordinator to immune");
        }
    }

    /* Connect to bio-async router if available */
    if (brain->bio_async_enabled) {
        if (plasticity_coordinator_connect_bio_async(coord) == 0) {
            NIMCP_LOGGING_DEBUG("Plasticity coordinator connected to bio-async");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect plasticity coordinator to bio-async");
        }
    }

    /* Store coordinator in brain */
    brain->plasticity_coordinator = coord;
    brain->plasticity_coordinator_enabled = true;

    /* Wire training-plasticity bridge immediately if already created (it is,
     * because Wave 12 — nimcp_brain_factory_init_training_subsystem — runs
     * before this Wave 24 init). This avoids a window where STDP/BCM would
     * run concurrently with backprop if brain_enable_multi_network_training()
     * is never called (e.g., inference-only brains that still run
     * biological plasticity via the coordinator). */
    if (brain->plasticity_bridge && brain->enable_plasticity_bridge) {
        if (plasticity_coordinator_set_plasticity_bridge(coord, brain->plasticity_bridge) == 0) {
            NIMCP_LOGGING_DEBUG("Plasticity coordinator wired to training-plasticity bridge "
                               "for backprop gating");
        }
    }

    /* Register biological plasticity mechanisms with their update intervals.
     * NOTE: the eight entries below are DECLARATION-ONLY (NULL handle and
     * update_fn): plasticity_coordinator_update() skips them, so they only
     * provide registry/priority bookkeeping. The real STDP/BCM/homeostatic/
     * eligibility/STP updates run in each network's own learning path, not
     * through this coordinator. Only "structural" (below) has a callback. */
    uint32_t mech_id = 0;
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "stdp", PLASTICITY_TYPE_STDP,
        NULL, NULL, NULL, 1.0f, 0.1f, 10, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "bcm", PLASTICITY_TYPE_BCM,
        NULL, NULL, NULL, 0.9f, 0.15f, 50, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "homeostatic", PLASTICITY_TYPE_HOMEOSTATIC,
        NULL, NULL, NULL, 0.7f, 0.05f, 1000, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "eligibility", PLASTICITY_TYPE_ELIGIBILITY,
        NULL, NULL, NULL, 0.95f, 0.12f, 10, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "dendritic", PLASTICITY_TYPE_DENDRITIC,
        NULL, NULL, NULL, 0.6f, 0.2f, 100, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "stp", PLASTICITY_TYPE_STP,
        NULL, NULL, NULL, 0.5f, 0.08f, 20, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "adaptive", PLASTICITY_TYPE_ADAPTIVE,
        NULL, NULL, NULL, 0.8f, 0.1f, 100, &mech_id);
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "predictive", PLASTICITY_TYPE_PREDICTIVE,
        NULL, NULL, NULL, 0.85f, 0.18f, 50, &mech_id);
    /* Structural plasticity (synaptogenesis): form/prune synapses during learning */
    brain->structural_plasticity = NULL;
    brain->structural_plasticity_enabled = false;
    structural_plasticity_config_t sp_config;
    structural_plasticity_default_config(&sp_config);
    /* Override biological defaults for training speed */
    sp_config.formation_threshold_hz = 5.0f;   /* lower from 50 Hz — training produces moderate activity */
    sp_config.maturation_time_sec = 60.0f;     /* 1 minute vs 24 hours — training runs fast */
    structural_plasticity_system_t* sp = structural_plasticity_create(&sp_config);
    if (sp) {
        brain->structural_plasticity = sp;
        brain->structural_plasticity_enabled = true;
        plasticity_coordinator_register_mechanism(
            brain->plasticity_coordinator, "structural", PLASTICITY_TYPE_STRUCTURAL,
            sp, structural_plasticity_update_realtime,
            NULL, 0.9f, 0.3f, 100, &mech_id);
        NIMCP_LOGGING_INFO("Structural plasticity (synaptogenesis) registered");
    } else {
        NIMCP_LOGGING_WARN("Failed to create structural plasticity — continuing without");
    }

    /* 8 declaration-only entries + structural (callback-driven) when created. */
    NIMCP_LOGGING_INFO("Registered %d plasticity mechanisms in coordinator "
                       "(%d with update callbacks)",
                       brain->structural_plasticity ? 9 : 8,
                       brain->structural_plasticity ? 1 : 0);

    /* Log success */
    plasticity_coordinator_stats_t stats;
    if (plasticity_coordinator_get_stats(coord, &stats) == 0) {
        NIMCP_LOGGING_INFO("Plasticity coordinator initialized: "
                          "mechanisms=%u, state=%s, immune=%s, bio_async=%s",
                          stats.total_mechanisms,
                          plasticity_coordinator_state_to_string(stats.current_state),
                          brain->immune_enabled ? "connected" : "disabled",
                          brain->bio_async_enabled ? "connected" : "disabled");
    } else {
        NIMCP_LOGGING_INFO("Plasticity coordinator initialized");
    }

    return true;
}
