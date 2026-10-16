/**
 * @file nimcp_brain_init_physics_bridges.c
 * @brief Physics upstream modules + 4 HALF-STATUE bridges init (Wave 8C)
 *
 * Creates brain->ephaptic_system, brain->thermo_state, brain->hh_population
 * on the heap (unlike the old nimcp_physics_brain_init.c statues that built
 * them on the stack and destroyed them immediately), then wires each of the
 * 4 HALF-STATUE physics bridges that have a real create/destroy API:
 *
 *   - ephaptic_bio_async  — Ephaptic → bio-router broadcast
 *   - ephaptic_fft        — Ephaptic LFP → FFT spectral analysis
 *   - hh_bio_async        — Hodgkin-Huxley → bio-router broadcast
 *   - thermo_bio_async    — Thermodynamics → bio-router broadcast
 *
 * The 3 quantum bridges (ephaptic_quantum, hh_quantum, thermo_quantum) are
 * purely functional — they expose individual analysis ops (anneal, walk,
 * entropy) that are called on-demand, not ticked from a hot path. They do
 * not appear here.
 *
 * All steps are null-tolerant: if bio_router isn't live yet or any upstream
 * module fails to create, the associated bridge(s) stay NULL and the tick
 * driver (brain_tick_physics_bridges) skips them each pass.
 */

#include "core/brain/factory/init/nimcp_brain_init_physics_bridges.h"

/* brain_internal first so its top-level typedefs win before the physics
 * module headers get pulled in. The physics modules use plain typedef'd
 * struct values (not pointer typedefs), so they don't collide with anything
 * on brain_internal — we just keep the include order consistent with the
 * rest of the factory init files. */
#include "core/brain/nimcp_brain_internal.h"

#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"

#include "physics/bridges/nimcp_ephaptic_bio_async_bridge.h"
#include "physics/bridges/nimcp_ephaptic_fft_bridge.h"
#include "physics/bridges/nimcp_hh_bio_async_bridge.h"
#include "physics/bridges/nimcp_thermo_bio_async_bridge.h"

#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#define PHYSICS_BRIDGES_LOG_MODULE "BRAIN_INIT_PHYSICS_BRIDGES"

/* Global bio-router singleton cache — same pattern as cochlea init. The
 * bio-async bridges all take a `bio_router_t` handle (itself a pointer
 * typedef) and store it opaquely, so this file-local slot is stable for
 * the brain's lifetime. */
static bio_router_t g_physics_router_cache = NULL;

/*=============================================================================
 * PER-MODULE INIT HELPERS — upstream physics modules
 *===========================================================================*/

static bool init_ephaptic_system(struct brain_struct* brain) {
    if (brain->ephaptic_system) return true;

    nimcp_ephaptic_system_t* sys =
        (nimcp_ephaptic_system_t*)nimcp_calloc(1, sizeof(nimcp_ephaptic_system_t));
    if (!sys) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_system calloc failed");
        return false;
    }

    nimcp_ephaptic_config_t cfg = nimcp_ephaptic_default_config();
    if (nimcp_ephaptic_init(sys, &cfg) != NIMCP_SUCCESS) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "nimcp_ephaptic_init failed");
        nimcp_free(sys);
        return false;
    }

    brain->ephaptic_system = (void*)sys;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE, "ephaptic_system created");
    return true;
}

static bool init_thermo_state(struct brain_struct* brain) {
    if (brain->thermo_state) return true;

    nimcp_thermodynamic_state_t* state =
        (nimcp_thermodynamic_state_t*)nimcp_calloc(
            1, sizeof(nimcp_thermodynamic_state_t));
    if (!state) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "thermo_state calloc failed");
        return false;
    }

    nimcp_thermo_config_t cfg = nimcp_thermo_default_config();
    if (nimcp_thermo_init(state, &cfg) != NIMCP_SUCCESS) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "nimcp_thermo_init failed");
        nimcp_free(state);
        return false;
    }

    brain->thermo_state = (void*)state;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE, "thermo_state created");
    return true;
}

static bool init_hh_population(struct brain_struct* brain) {
    if (brain->hh_population) return true;

    nimcp_hh_population_t* pop =
        (nimcp_hh_population_t*)nimcp_calloc(1, sizeof(nimcp_hh_population_t));
    if (!pop) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "hh_population calloc failed");
        return false;
    }

    /* Small default population — enough to exercise the bridge without
     * competing for GPU/CPU budget with the 2M-neuron main brain. */
    nimcp_hh_config_t hh_cfg = {
        .g_Na = 120.0f, .g_K = 36.0f, .g_L = 0.3f,
        .g_Ca_L = 0.0f, .g_Ca_T = 0.0f, .g_K_Ca = 0.0f,
        .g_K_A = 0.0f,  .g_H = 0.0f,
        .E_Na = 50.0f,  .E_K = -77.0f, .E_L = -54.4f,
        .E_Ca = 120.0f, .E_H = -30.0f,
        .C_m = 1.0f,    .V_rest = -65.0f,
        .temperature = 37.0f,
        .surface_area = 0.01f, .length = 100.0f, .diameter = 10.0f,
        .enable_calcium = false, .enable_adaptation = false,
        .enable_h_current = false,
        .dt_max = 0.1f, .adaptive_dt = false,
    };

    if (nimcp_hh_population_create(pop, 64u, &hh_cfg) != NIMCP_SUCCESS) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "nimcp_hh_population_create failed");
        nimcp_free(pop);
        return false;
    }

    brain->hh_population = (void*)pop;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
        "hh_population created (64 neurons)");
    return true;
}

/*=============================================================================
 * PER-BRIDGE INIT HELPERS
 *===========================================================================*/

static bool init_ephaptic_bio_async_bridge(struct brain_struct* brain) {
    if (brain->ephaptic_bio_async_bridge) return true;
    if (!brain->ephaptic_system) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_bio_async_bridge skipped — ephaptic_system is NULL");
        return true;
    }
    if (!bio_router_is_initialized()) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_bio_async_bridge skipped — bio_router not live");
        return true;
    }
    if (!g_physics_router_cache) g_physics_router_cache = bio_router_get_global();
    if (!g_physics_router_cache) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_bio_async_bridge skipped — bio_router_get_global NULL");
        return true;
    }

    ephaptic_bio_async_bridge_t* b = ephaptic_bio_async_bridge_create(NULL);
    if (!b) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_bio_async_bridge_create failed");
        return false;
    }
    (void)ephaptic_bio_async_connect(b, g_physics_router_cache);

    brain->ephaptic_bio_async_bridge =
        (struct ephaptic_bio_async_bridge_struct*)b;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
        "ephaptic_bio_async_bridge wired");
    return true;
}

static bool init_ephaptic_fft_bridge(struct brain_struct* brain) {
    if (brain->ephaptic_fft_bridge) return true;
    if (!brain->ephaptic_system) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_fft_bridge skipped — ephaptic_system is NULL");
        return true;
    }

    ephaptic_fft_bridge_t* b = ephaptic_fft_bridge_create(NULL);
    if (!b) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "ephaptic_fft_bridge_create failed");
        return false;
    }

    brain->ephaptic_fft_bridge = (struct ephaptic_fft_bridge_struct*)b;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
        "ephaptic_fft_bridge wired");
    return true;
}

static bool init_hh_bio_async_bridge(struct brain_struct* brain) {
    if (brain->hh_bio_async_bridge) return true;
    if (!brain->hh_population) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "hh_bio_async_bridge skipped — hh_population is NULL");
        return true;
    }
    if (!bio_router_is_initialized()) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "hh_bio_async_bridge skipped — bio_router not live");
        return true;
    }
    if (!g_physics_router_cache) g_physics_router_cache = bio_router_get_global();
    if (!g_physics_router_cache) return true;

    hh_bio_async_bridge_t* b = hh_bio_async_bridge_create(NULL);
    if (!b) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "hh_bio_async_bridge_create failed");
        return false;
    }

    /* Connect to the population (single-neuron path is NULL). */
    (void)hh_bio_async_connect(b, NULL,
                               (nimcp_hh_population_t*)brain->hh_population,
                               g_physics_router_cache);

    brain->hh_bio_async_bridge = (struct hh_bio_async_bridge_struct*)b;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE, "hh_bio_async_bridge wired");
    return true;
}

static bool init_thermo_bio_async_bridge(struct brain_struct* brain) {
    if (brain->thermo_bio_async_bridge) return true;
    if (!brain->thermo_state) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "thermo_bio_async_bridge skipped — thermo_state is NULL");
        return true;
    }
    if (!bio_router_is_initialized()) {
        LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
            "thermo_bio_async_bridge skipped — bio_router not live");
        return true;
    }
    if (!g_physics_router_cache) g_physics_router_cache = bio_router_get_global();
    if (!g_physics_router_cache) return true;

    thermo_bio_async_bridge_t* b = thermo_bio_async_bridge_create(NULL);
    if (!b) {
        LOG_MODULE_WARN(PHYSICS_BRIDGES_LOG_MODULE,
            "thermo_bio_async_bridge_create failed");
        return false;
    }

    (void)thermo_bio_async_connect(
        b, (nimcp_thermodynamic_state_t*)brain->thermo_state,
        g_physics_router_cache);

    brain->thermo_bio_async_bridge = (struct thermo_bio_async_bridge_struct*)b;
    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
        "thermo_bio_async_bridge wired");
    return true;
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

bool nimcp_brain_factory_init_physics_bridges_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_physics_bridges_subsystem: brain is NULL");
        return false;
    }

    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
        "Initializing physics upstream modules + 4 bridges (Wave 8C)");

    /* Upstream modules first — each bridge needs its upstream. */
    (void)init_ephaptic_system(brain);
    (void)init_thermo_state(brain);
    (void)init_hh_population(brain);

    /* Bridges. Each is null-tolerant in its upstream + bio_router deps. */
    (void)init_ephaptic_bio_async_bridge(brain);
    (void)init_ephaptic_fft_bridge(brain);
    (void)init_hh_bio_async_bridge(brain);
    (void)init_thermo_bio_async_bridge(brain);

    /* Flip the enabled flag if any bridge is live — the tick driver gates
     * on this so a fully-statued physics subsystem has zero per-tick cost. */
    brain->physics_bridges_enabled =
        (brain->ephaptic_bio_async_bridge != NULL) ||
        (brain->ephaptic_fft_bridge != NULL) ||
        (brain->hh_bio_async_bridge != NULL) ||
        (brain->thermo_bio_async_bridge != NULL);

    LOG_MODULE_INFO(PHYSICS_BRIDGES_LOG_MODULE,
        "physics bridges init complete (enabled=%d, ephaptic_bio=%p, "
        "ephaptic_fft=%p, hh_bio=%p, thermo_bio=%p)",
        (int)brain->physics_bridges_enabled,
        (void*)brain->ephaptic_bio_async_bridge,
        (void*)brain->ephaptic_fft_bridge,
        (void*)brain->hh_bio_async_bridge,
        (void*)brain->thermo_bio_async_bridge);

    return true;
}

void nimcp_brain_factory_destroy_physics_bridges_subsystem(brain_t brain) {
    if (!brain) return;

    /* Destroy bridges first — they may touch upstream during teardown. */
    if (brain->thermo_bio_async_bridge) {
        thermo_bio_async_bridge_destroy(
            (thermo_bio_async_bridge_t*)brain->thermo_bio_async_bridge);
        brain->thermo_bio_async_bridge = NULL;
    }
    if (brain->hh_bio_async_bridge) {
        hh_bio_async_bridge_destroy(
            (hh_bio_async_bridge_t*)brain->hh_bio_async_bridge);
        brain->hh_bio_async_bridge = NULL;
    }
    if (brain->ephaptic_fft_bridge) {
        ephaptic_fft_bridge_destroy(
            (ephaptic_fft_bridge_t*)brain->ephaptic_fft_bridge);
        brain->ephaptic_fft_bridge = NULL;
    }
    if (brain->ephaptic_bio_async_bridge) {
        ephaptic_bio_async_bridge_destroy(
            (ephaptic_bio_async_bridge_t*)brain->ephaptic_bio_async_bridge);
        brain->ephaptic_bio_async_bridge = NULL;
    }

    /* Then upstream modules. */
    if (brain->hh_population) {
        nimcp_hh_population_destroy(
            (nimcp_hh_population_t*)brain->hh_population);
        nimcp_free(brain->hh_population);
        brain->hh_population = NULL;
    }
    if (brain->thermo_state) {
        nimcp_thermo_destroy(
            (nimcp_thermodynamic_state_t*)brain->thermo_state);
        nimcp_free(brain->thermo_state);
        brain->thermo_state = NULL;
    }
    if (brain->ephaptic_system) {
        nimcp_ephaptic_destroy(
            (nimcp_ephaptic_system_t*)brain->ephaptic_system);
        nimcp_free(brain->ephaptic_system);
        brain->ephaptic_system = NULL;
    }

    brain->physics_bridges_enabled = false;
    g_physics_router_cache = NULL;
}
