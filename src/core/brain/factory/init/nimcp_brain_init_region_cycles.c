/**
 * @file nimcp_brain_init_region_cycles.c
 * @brief Wraps Wave 8B/8C tick drivers as driven coordinator cycles.
 *
 * Each tick driver (neuromod, sensorimotor, language, physics_bridges,
 * meta_learning, intuitive_physics, hypothalamus, entorhinal, cerebellum,
 * basal_ganglia) is called inline from brain_learn_vector + brain_decide.
 * This file registers each as an OBSERVATION-ONLY coordinator cycle; it no
 * longer spawns driver threads. The earlier driven registration double-fired
 * every tick on an unlocked thread concurrently with the RPC path (data
 * races + ~2x dt), so the inline call is now the single owner.
 */

#include "core/brain/factory/init/nimcp_brain_init_region_cycles.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "core/brain/nimcp_brain_tick_neuromod.h"
#include "core/brain/nimcp_brain_tick_sensorimotor.h"
#include "core/brain/nimcp_brain_tick_language.h"
#include "core/brain/nimcp_brain_tick_physics_bridges.h"
#include "core/brain/nimcp_brain_tick_meta_learning.h"
#include "core/brain/nimcp_brain_tick_intuitive_physics.h"
#include "core/brain/nimcp_brain_tick_hypothalamus.h"
#include "core/brain/nimcp_brain_tick_entorhinal.h"
#include "core/brain/nimcp_brain_tick_cerebellum.h"
#include "core/brain/nimcp_brain_tick_basal_ganglia.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_region_cycles, MESH_ADAPTER_CATEGORY_SYSTEM)

#define REGION_CYCLES_LOG_MODULE "BRAIN_INIT_REGION_CYCLES"

/* Single-owner policy (2026-11-23): every tick below is also called inline
 * from brain_learn_vector and brain_decide, which run under the daemon's
 * brain lock. Driving them from cc-* threads too raced those calls with no
 * lock (e.g. snn_language_bridge_tick vs produce/comprehend) and integrated
 * dt twice. The inline call is now the sole driver; the cycles are
 * registered observation-only so coordinator status still lists them. */

bool nimcp_brain_factory_init_region_cycles_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_region_cycles_subsystem: brain is NULL");
        return false;
    }

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord || !brain->cycle_coordinator_enabled) {
        LOG_MODULE_INFO(REGION_CYCLES_LOG_MODULE,
            "cycle coordinator absent — skipping region cycle registration");
        return false;
    }

    LOG_MODULE_INFO(REGION_CYCLES_LOG_MODULE,
        "Registering Wave 8B/8C region adapters (observation-only; ticked inline)");

    int ok_count = 0;

    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_NEUROMOD, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(NEUROMOD) failed");
    }

    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_SENSORIMOTOR, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(SENSORIMOTOR) failed");
    }

    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_LANGUAGE, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(LANGUAGE) failed");
    }

    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_PHYSICS_BRIDGES, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(PHYSICS_BRIDGES) failed");
    }

    /* Wave 4 meta_learning — 100ms cadence. meta_learner may be NULL if
     * enable_meta_learning was false; the tick driver null-guards. */
    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_META_LEARNING, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(META_LEARNING) failed");
    }

    /* Wave 5 intuitive_physics — 16ms cadence. Engine may be NULL if
     * enable_intuitive_physics was false; the tick driver null-guards. */
    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_INTUITIVE_PHYSICS, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(INTUITIVE_PHYSICS) failed");
    }

    /* Wave 8B-c hypothalamus — 100ms cadence (slow drive). */
    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_HYPOTHALAMUS, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(HYPOTHALAMUS) failed");
    }

    /* Wave 8B-c entorhinal — 16ms cadence. PARTIAL: driver is no-op
     * until brain_t exposes the entorhinal pointer. */
    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_ENTORHINAL, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(ENTORHINAL) failed");
    }

    /* Wave 8B-c cerebellum — 16ms cadence (bio-msg drain). */
    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_CEREBELLUM, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(CEREBELLUM) failed");
    }

    /* Wave 8B-c basal_ganglia — 16ms cadence (action selection /
     * reward gating). */
    if (brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_BASAL_GANGLIA, brain, NULL) == 0) {
        ok_count++;
    } else {
        LOG_MODULE_WARN(REGION_CYCLES_LOG_MODULE,
            "register(BASAL_GANGLIA) failed");
    }

    LOG_MODULE_INFO(REGION_CYCLES_LOG_MODULE,
        "region cycles registered: %d/10", ok_count);
    return ok_count > 0;
}

void nimcp_brain_factory_destroy_region_cycles_subsystem(brain_t brain) {
    if (!brain) return;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord) return;

    /* Unregister in reverse order (observation-only entries: no driver
     * thread to join any more). */
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BASAL_GANGLIA);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_CEREBELLUM);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_ENTORHINAL);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_HYPOTHALAMUS);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_INTUITIVE_PHYSICS);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_META_LEARNING);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_PHYSICS_BRIDGES);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_LANGUAGE);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_SENSORIMOTOR);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROMOD);
}
