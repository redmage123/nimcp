/**
 * @file nimcp_brain_init_biology.c
 * @brief Biology cluster init — epigenetics + neurogenesis + NVC
 *
 * Wires three modules that existed as statues prior to 2026-04-24 into the
 * cycle coordinator's register_driven API. See the companion header for the
 * scope of this change (state advancement only; consumer-side wiring is a
 * separate follow-up).
 *
 * Per-module rationale for cycle type + interval lives in the coordinator's
 * get_default_interval_us() — this file only passes the intervals through.
 */

#include "core/brain/factory/init/nimcp_brain_init_biology.h"

/* Biology module headers BEFORE brain_internal.h.
 *
 * Each of these headers does `typedef struct nimcp_brain_struct* nimcp_brain_t`
 * as a local forward decl. That name collides with nimcp.h's
 * `typedef struct nimcp_brain_handle* nimcp_brain_t` (pulled in transitively
 * by brain_internal.h). We sidestep the collision by redirecting the biology
 * typedef name to a file-local alias before inclusion, then restoring. The
 * biology function signatures in the macro-redirected headers take
 * `bio_brain_opaque_t` — same pointer type, so we just cast at the call
 * site (biology modules stash the pointer opaquely without dereferencing). */
#define nimcp_brain_t bio_brain_opaque_t
#include "biology/epigenetics/nimcp_epigenetics.h"
#include "biology/neurogenesis/nimcp_neurogenesis.h"
#include "biology/neurovascular/nimcp_neurovascular.h"
#undef nimcp_brain_t

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_biology, MESH_ADAPTER_CATEGORY_SYSTEM)

#define BIO_INIT_LOG_MODULE "BRAIN_INIT_BIOLOGY"

/*=============================================================================
 * TICK WRAPPERS
 *
 * The coordinator's register_driven takes a (void*)->void callback. Each
 * biology module has a different update signature (taking dt in seconds
 * or milliseconds). These wrappers adapt the ABI and skip the call
 * defensively if the module was destroyed between registration and tick.
 *===========================================================================*/

static void epigenetics_tick_wrapper(void* ctx) {
    struct brain_struct* brain = (struct brain_struct*)ctx;
    if (!brain || !brain->epigenetics || !brain->epigenetics_enabled) return;
    /* 100ms cycle → 0.1s in seconds */
    (void)nimcp_epigenetics_update(brain->epigenetics, 0.1f);
}

static void neurogenesis_tick_wrapper(void* ctx) {
    struct brain_struct* brain = (struct brain_struct*)ctx;
    if (!brain || !brain->neurogenesis || !brain->neurogenesis_enabled) return;
    /* 1s cycle → 1.0s in seconds */
    (void)nimcp_neurogenesis_update(brain->neurogenesis, 1.0f);
}

static void nvc_tick_wrapper(void* ctx) {
    struct brain_struct* brain = (struct brain_struct*)ctx;
    if (!brain || !brain->nvc_system || !brain->neurovascular_enabled) return;
    /* 100ms cycle → 100.0ms per nvc_update's ms-dt contract */
    (void)nimcp_nvc_update(brain->nvc_system, 100.0f);
}

/*=============================================================================
 * PER-MODULE INIT HELPERS
 *===========================================================================*/

static bool init_epigenetics(struct brain_struct* brain,
                             brain_cycle_coordinator_t* coord) {
    if (brain->epigenetics) return true;  /* idempotent */

    nimcp_epigenetics_config_t cfg = nimcp_epigenetics_default_config();
    brain->epigenetics = nimcp_epigenetics_create(&cfg);
    if (!brain->epigenetics) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "epigenetics_create failed — module stays NULL");
        return false;
    }

    /* Opaque pointer: biology module stashes it without dereferencing. */
    nimcp_epigenetics_error_t rc = nimcp_epigenetics_init(
        brain->epigenetics, (bio_brain_opaque_t)brain);
    if (rc != EPIGENETICS_OK) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "epigenetics_init failed (%d) — destroying", (int)rc);
        nimcp_epigenetics_destroy(brain->epigenetics);
        brain->epigenetics = NULL;
        return false;
    }

    /* Drive at 100ms. Passing brain as tick_ctx so wrapper can reach all state. */
    int reg_rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_EPIGENETICS, 100000ull,
        epigenetics_tick_wrapper, brain, NULL);
    if (reg_rc != 0) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "register_driven(EPIGENETICS) failed — destroying");
        nimcp_epigenetics_shutdown(brain->epigenetics);
        nimcp_epigenetics_destroy(brain->epigenetics);
        brain->epigenetics = NULL;
        return false;
    }

    brain->epigenetics_enabled = true;
    LOG_MODULE_INFO(BIO_INIT_LOG_MODULE, "epigenetics wired (100ms cycle)");
    return true;
}

static bool init_neurogenesis(struct brain_struct* brain,
                              brain_cycle_coordinator_t* coord) {
    if (brain->neurogenesis) return true;

    nimcp_neurogenesis_config_t cfg = nimcp_neurogenesis_default_config();
    brain->neurogenesis = nimcp_neurogenesis_create(&cfg);
    if (!brain->neurogenesis) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "neurogenesis_create failed — module stays NULL");
        return false;
    }

    nimcp_neurogenesis_error_t rc = nimcp_neurogenesis_init(
        brain->neurogenesis, (bio_brain_opaque_t)brain);
    if (rc != NEUROGENESIS_OK) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "neurogenesis_init failed (%d) — destroying", (int)rc);
        nimcp_neurogenesis_destroy(brain->neurogenesis);
        brain->neurogenesis = NULL;
        return false;
    }

    int reg_rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_NEUROGENESIS, 1000000ull,  /* 1s */
        neurogenesis_tick_wrapper, brain, NULL);
    if (reg_rc != 0) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "register_driven(NEUROGENESIS) failed — destroying");
        nimcp_neurogenesis_shutdown(brain->neurogenesis);
        nimcp_neurogenesis_destroy(brain->neurogenesis);
        brain->neurogenesis = NULL;
        return false;
    }

    brain->neurogenesis_enabled = true;
    LOG_MODULE_INFO(BIO_INIT_LOG_MODULE, "neurogenesis wired (1s cycle)");
    return true;
}

static bool init_neurovascular(struct brain_struct* brain,
                               brain_cycle_coordinator_t* coord) {
    if (brain->nvc_system) return true;

    /* NVC has no create() — caller allocates the struct then calls init().
     * The struct is a fixed-size POD (NVC_MAX_UNITS * unit struct + config +
     * metrics), so nimcp_malloc is sufficient. */
    brain->nvc_system = (nimcp_nvc_system_t*)nimcp_malloc(
        sizeof(nimcp_nvc_system_t));
    if (!brain->nvc_system) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE, "nvc_system alloc failed");
        return false;
    }

    nimcp_nvc_error_t rc = nimcp_nvc_init(brain->nvc_system, NULL);
    if (rc != NVC_OK) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "nvc_init failed (%d) — freeing", (int)rc);
        nimcp_free(brain->nvc_system);
        brain->nvc_system = NULL;
        return false;
    }

    int reg_rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_NEUROVASCULAR, 100000ull,  /* 100ms */
        nvc_tick_wrapper, brain, NULL);
    if (reg_rc != 0) {
        LOG_MODULE_WARN(BIO_INIT_LOG_MODULE,
            "register_driven(NEUROVASCULAR) failed — freeing");
        (void)nimcp_nvc_shutdown(brain->nvc_system);
        nimcp_free(brain->nvc_system);
        brain->nvc_system = NULL;
        return false;
    }

    brain->neurovascular_enabled = true;
    LOG_MODULE_INFO(BIO_INIT_LOG_MODULE, "neurovascular wired (100ms cycle)");
    return true;
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

bool nimcp_brain_factory_init_biology_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_biology_subsystem: brain is NULL");
        return false;
    }

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord || !brain->cycle_coordinator_enabled) {
        /* No coordinator → periodic driving has nowhere to attach. We'd
         * either have to fall back to a per-module thread (duplicating the
         * very boilerplate register_driven exists to eliminate) or skip.
         * Skipping keeps the codebase honest — the modules aren't silently
         * created-but-not-advanced; they aren't created at all, matching
         * their prior statue state. A caller that wants biology MUST enable
         * the coordinator (it's the default on brain_create_full). */
        LOG_MODULE_INFO(BIO_INIT_LOG_MODULE,
            "cycle coordinator absent — skipping biology wiring");
        return false;
    }

    LOG_MODULE_INFO(BIO_INIT_LOG_MODULE,
        "Initializing biology cluster (epigenetics + neurogenesis + NVC)");

    bool any_ok = false;
    any_ok |= init_epigenetics(brain, coord);
    any_ok |= init_neurogenesis(brain, coord);
    any_ok |= init_neurovascular(brain, coord);

    return any_ok;
}

void nimcp_brain_factory_destroy_biology_subsystem(brain_t brain) {
    if (!brain) return;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;

    /* Each unregister stops + joins the driver thread. Order doesn't matter
     * because each cycle has its own thread. */
    if (brain->epigenetics_enabled && coord) {
        (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_EPIGENETICS);
        brain->epigenetics_enabled = false;
    }
    if (brain->epigenetics) {
        (void)nimcp_epigenetics_shutdown(brain->epigenetics);
        nimcp_epigenetics_destroy(brain->epigenetics);
        brain->epigenetics = NULL;
    }

    if (brain->neurogenesis_enabled && coord) {
        (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROGENESIS);
        brain->neurogenesis_enabled = false;
    }
    if (brain->neurogenesis) {
        (void)nimcp_neurogenesis_shutdown(brain->neurogenesis);
        nimcp_neurogenesis_destroy(brain->neurogenesis);
        brain->neurogenesis = NULL;
    }

    if (brain->neurovascular_enabled && coord) {
        (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROVASCULAR);
        brain->neurovascular_enabled = false;
    }
    if (brain->nvc_system) {
        (void)nimcp_nvc_shutdown(brain->nvc_system);
        nimcp_free(brain->nvc_system);
        brain->nvc_system = NULL;
    }
}
