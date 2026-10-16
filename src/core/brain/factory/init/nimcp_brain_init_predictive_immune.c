/**
 * @file nimcp_brain_init_predictive_immune.c
 * @brief Predictive-immune coupling init — wires the integration module
 *
 * Converts predictive_immune from a statue (implementation present, never
 * instantiated) into a live, periodically-ticked brain subsystem. Depends on
 * brain->predictive_network (Wave 8) + brain->immune_system (Wave 11) having
 * already been created — Wave 12 is the earliest wave that satisfies both.
 *
 * State advancement only: this file creates the module and runs its
 * bidirectional update cycle at 100ms. Consumer-side wiring (downstream
 * readers of the inflammation→precision modulation output; upstream callers
 * invoking immune antigen presentation on prediction-error spikes) is a
 * separate follow-up — the module's own start() hooks its internal
 * callbacks, which is sufficient for the state-advancement half.
 */

#include "core/brain/factory/init/nimcp_brain_init_predictive_immune.h"

/* Module header. Verified (2026-04-24) to NOT redefine nimcp_brain_t — so
 * unlike the biology cluster we can include it directly without the
 * `#define nimcp_brain_t` trampoline. */
#include "cognitive/nimcp_predictive_immune.h"

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_predictive_immune, MESH_ADAPTER_CATEGORY_SYSTEM)

#define PREDIMMUNE_INIT_LOG_MODULE "BRAIN_INIT_PREDICTIVE_IMMUNE"

/*=============================================================================
 * TICK WRAPPER
 *
 * The coordinator's register_driven takes a (void*)->void callback. The
 * predictive_immune_update signature is (system*, float dt) returning
 * nimcp_result_t. The header documents `dt` as "Time step (ms)"; the
 * implementation currently ignores the numeric value (uses it only as a
 * timestep label — all real dynamics are event/rate-driven in
 * update_interoceptive_state + compute_precision_modulation). We pass
 * 100.0f for consistency with the 100ms coordinator cadence.
 *===========================================================================*/

static void predictive_immune_tick_wrapper(void* ctx) {
    struct brain_struct* brain = (struct brain_struct*)ctx;
    if (!brain || !brain->predictive_immune || !brain->predictive_immune_enabled) return;
    /* 100ms cycle → 100.0 ms per the module's dt-in-ms contract. */
    (void)predictive_immune_update(brain->predictive_immune, 100.0f);
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

bool nimcp_brain_factory_init_predictive_immune_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_predictive_immune_subsystem: brain is NULL");
        return false;
    }

    /* Idempotent — already wired on a previous call. */
    if (brain->predictive_immune) return true;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord || !brain->cycle_coordinator_enabled) {
        LOG_MODULE_INFO(PREDIMMUNE_INIT_LOG_MODULE,
            "cycle coordinator absent — skipping predictive_immune wiring");
        return false;
    }

    /* Hard prerequisites: predictive_immune_create rejects NULL args. If
     * either is missing we skip cleanly and leave the field NULL (module
     * stays a statue, no crash). */
    if (!brain->predictive_network) {
        LOG_MODULE_INFO(PREDIMMUNE_INIT_LOG_MODULE,
            "brain->predictive_network is NULL — skipping predictive_immune "
            "wiring (is the predictive subsystem enabled?)");
        return false;
    }
    if (!brain->immune_system) {
        LOG_MODULE_INFO(PREDIMMUNE_INIT_LOG_MODULE,
            "brain->immune_system is NULL — skipping predictive_immune "
            "wiring (is the immune subsystem enabled?)");
        return false;
    }

    LOG_MODULE_INFO(PREDIMMUNE_INIT_LOG_MODULE,
        "Initializing predictive-immune coupling (100ms cycle)");

    /* Use defaults (NULL config → predictive_immune_default_config internally). */
    brain->predictive_immune = predictive_immune_create(
        NULL,
        brain->predictive_network,
        brain->immune_system);
    if (!brain->predictive_immune) {
        LOG_MODULE_WARN(PREDIMMUNE_INIT_LOG_MODULE,
            "predictive_immune_create failed — module stays NULL");
        return false;
    }

    /* start() activates internal callbacks so inflammation/error feedback
     * loops fire as events happen. Needed before register_driven so the
     * first tick finds a live, callback-wired system. */
    nimcp_result_t start_rc = predictive_immune_start(brain->predictive_immune);
    if (start_rc != NIMCP_SUCCESS) {
        LOG_MODULE_WARN(PREDIMMUNE_INIT_LOG_MODULE,
            "predictive_immune_start failed (%d) — destroying",
            (int)start_rc);
        predictive_immune_destroy(brain->predictive_immune);
        brain->predictive_immune = NULL;
        return false;
    }

    int reg_rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, 100000ull,  /* 100ms */
        predictive_immune_tick_wrapper, brain, NULL);
    if (reg_rc != 0) {
        LOG_MODULE_WARN(PREDIMMUNE_INIT_LOG_MODULE,
            "register_driven(PREDICTIVE_IMMUNE) failed — destroying");
        (void)predictive_immune_stop(brain->predictive_immune);
        predictive_immune_destroy(brain->predictive_immune);
        brain->predictive_immune = NULL;
        return false;
    }

    brain->predictive_immune_enabled = true;
    LOG_MODULE_INFO(PREDIMMUNE_INIT_LOG_MODULE,
        "predictive_immune wired (100ms cycle)");
    return true;
}

void nimcp_brain_factory_destroy_predictive_immune_subsystem(brain_t brain) {
    if (!brain) return;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;

    /* Unregister first — joins the driver thread, so no in-flight tick
     * survives to touch the system after we destroy it. */
    if (brain->predictive_immune_enabled && coord) {
        (void)brain_cycle_coordinator_unregister(coord,
            BRAIN_CYCLE_PREDICTIVE_IMMUNE);
        brain->predictive_immune_enabled = false;
    }
    if (brain->predictive_immune) {
        (void)predictive_immune_stop(brain->predictive_immune);
        predictive_immune_destroy(brain->predictive_immune);
        brain->predictive_immune = NULL;
    }
}
