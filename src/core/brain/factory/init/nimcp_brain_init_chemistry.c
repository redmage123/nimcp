/**
 * @file nimcp_brain_init_chemistry.c
 * @brief Chemistry cluster init — protons + buffers + pH + NO
 *
 * Wires four previously-compile-disabled chemistry modules via a single
 * BRAIN_CYCLE_CHEMISTRY cycle at 10ms. All four have `_init(system, config)`
 * lifecycle APIs that require caller-allocated structs. The combined tick
 * advances all four in biological order each pass.
 *
 * Typedef-collision note: each chemistry header redefines `nimcp_brain_t`.
 * We sidestep the collision the same way as nimcp_brain_init_biology.c —
 * `#define nimcp_brain_t chem_brain_opaque_t` before the includes, `#undef`
 * after. The chemistry modules only stash the pointer opaquely so the cast
 * at use sites is safe.
 */

#include "core/brain/factory/init/nimcp_brain_init_chemistry.h"

/* Chemistry headers before brain_internal.h — see biology init for the
 * full rationale of this typedef-alias trick. */
#define nimcp_brain_t chem_brain_opaque_t
#include "chemistry/ph/nimcp_ph_dynamics.h"
#include "chemistry/ph/nimcp_proton_pumps.h"
#include "chemistry/ph/nimcp_buffer_systems.h"
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
#undef nimcp_brain_t

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_chemistry, MESH_ADAPTER_CATEGORY_SYSTEM)

#define CHEM_INIT_LOG_MODULE "BRAIN_INIT_CHEMISTRY"

/*=============================================================================
 * TICK WRAPPER — advances all 4 chemistry modules per cycle.
 *
 * Ordering rationale (short → long feedback loops):
 *   1. proton pumps   — move H+ across membranes (source term)
 *   2. buffer systems — absorb/release H+ (damping)
 *   3. pH dynamics    — integrate net H+ change into local pH
 *   4. NO system      — pH-dependent NOS activity; must see new pH
 *===========================================================================*/

static void chemistry_tick_wrapper(void* ctx) {
    struct brain_struct* brain = (struct brain_struct*)ctx;
    if (!brain || !brain->chemistry_enabled) return;

    /* All modules take dt in ms — coord cadence is 10ms. */
    const float dt_ms = 10.0f;

    if (brain->proton_pumps) {
        /* nimcp_pump_update needs 3 pH sample inputs. Use physiological
         * defaults (7.2 cyto, 7.4 extra, 5.5 vesicular) — these are the
         * resting values the model regresses toward when no external pH
         * module has fed it real values. If brain->ph_system is populated
         * and exposes per-compartment pH getters in a future wave, we
         * can sample real values here. */
        (void)nimcp_pump_update((nimcp_pump_system_t*)brain->proton_pumps,
                                dt_ms,
                                7.2f,   /* intracellular */
                                7.4f,   /* extracellular */
                                5.5f);  /* vesicular */
    }
    if (brain->buffer_manager) {
        (void)nimcp_buffer_update((nimcp_buffer_manager_t*)brain->buffer_manager, dt_ms);
    }
    if (brain->ph_system) {
        (void)nimcp_ph_update((nimcp_ph_system_t*)brain->ph_system, dt_ms);
    }
    if (brain->no_system) {
        (void)nimcp_no_update((nimcp_no_system_t*)brain->no_system, dt_ms);
    }
}

/*=============================================================================
 * INIT / DESTROY HELPERS (one per module) — minimize conditional nesting
 *===========================================================================*/

static bool chem_init_pumps(struct brain_struct* brain) {
    nimcp_pump_system_t* sys = (nimcp_pump_system_t*)nimcp_malloc(sizeof(*sys));
    if (!sys) return false;
    if (nimcp_pump_init(sys, NULL) != PUMP_OK) { nimcp_free(sys); return false; }
    brain->proton_pumps = (struct nimcp_pump_system_s*)sys;
    return true;
}

static bool chem_init_buffers(struct brain_struct* brain) {
    nimcp_buffer_manager_t* mgr = (nimcp_buffer_manager_t*)nimcp_malloc(sizeof(*mgr));
    if (!mgr) return false;
    if (nimcp_buffer_init(mgr, NULL) != BUFFER_OK) { nimcp_free(mgr); return false; }
    brain->buffer_manager = (struct nimcp_buffer_manager_s*)mgr;
    return true;
}

static bool chem_init_ph(struct brain_struct* brain) {
    nimcp_ph_system_t* sys = (nimcp_ph_system_t*)nimcp_malloc(sizeof(*sys));
    if (!sys) return false;
    if (nimcp_ph_init(sys, NULL) != PH_OK) { nimcp_free(sys); return false; }
    brain->ph_system = (struct nimcp_ph_system_s*)sys;
    return true;
}

static bool chem_init_no(struct brain_struct* brain) {
    nimcp_no_system_t* sys = (nimcp_no_system_t*)nimcp_malloc(sizeof(*sys));
    if (!sys) return false;
    if (nimcp_no_init(sys, NULL) != NO_OK) { nimcp_free(sys); return false; }
    brain->no_system = (struct nimcp_no_system_s*)sys;
    return true;
}

static void chem_teardown(struct brain_struct* brain) {
    if (brain->proton_pumps) {
        (void)nimcp_pump_shutdown((nimcp_pump_system_t*)brain->proton_pumps);
        nimcp_free(brain->proton_pumps);
        brain->proton_pumps = NULL;
    }
    if (brain->buffer_manager) {
        (void)nimcp_buffer_shutdown((nimcp_buffer_manager_t*)brain->buffer_manager);
        nimcp_free(brain->buffer_manager);
        brain->buffer_manager = NULL;
    }
    if (brain->ph_system) {
        (void)nimcp_ph_shutdown((nimcp_ph_system_t*)brain->ph_system);
        nimcp_free(brain->ph_system);
        brain->ph_system = NULL;
    }
    if (brain->no_system) {
        (void)nimcp_no_shutdown((nimcp_no_system_t*)brain->no_system);
        nimcp_free(brain->no_system);
        brain->no_system = NULL;
    }
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

bool nimcp_brain_factory_init_chemistry_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_chemistry_subsystem: brain is NULL");
        return false;
    }

    /* Idempotent: if any field is already set we assume this was already
     * wired. Avoids double-register_driven (which would fail). */
    if (brain->chemistry_enabled) return true;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord || !brain->cycle_coordinator_enabled) {
        LOG_MODULE_INFO(CHEM_INIT_LOG_MODULE,
            "cycle coordinator absent — skipping chemistry wiring");
        return false;
    }

    LOG_MODULE_INFO(CHEM_INIT_LOG_MODULE,
        "Initializing chemistry cluster (10ms cycle: protons, buffers, pH, NO)");

    if (!chem_init_pumps(brain))  { LOG_MODULE_WARN(CHEM_INIT_LOG_MODULE, "pump init failed");   chem_teardown(brain); return false; }
    if (!chem_init_buffers(brain)){ LOG_MODULE_WARN(CHEM_INIT_LOG_MODULE, "buffer init failed"); chem_teardown(brain); return false; }
    if (!chem_init_ph(brain))     { LOG_MODULE_WARN(CHEM_INIT_LOG_MODULE, "pH init failed");     chem_teardown(brain); return false; }
    if (!chem_init_no(brain))     { LOG_MODULE_WARN(CHEM_INIT_LOG_MODULE, "NO init failed");     chem_teardown(brain); return false; }

    int reg_rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_CHEMISTRY, 10000ull,  /* 10ms */
        chemistry_tick_wrapper, brain, NULL);
    if (reg_rc != 0) {
        LOG_MODULE_WARN(CHEM_INIT_LOG_MODULE,
            "register_driven(CHEMISTRY) failed — tearing down");
        chem_teardown(brain);
        return false;
    }

    brain->chemistry_enabled = true;
    LOG_MODULE_INFO(CHEM_INIT_LOG_MODULE,
        "chemistry wired (10ms cycle driving 4 modules)");
    return true;
}

void nimcp_brain_factory_destroy_chemistry_subsystem(brain_t brain) {
    if (!brain) return;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;

    /* Unregister first — joins the driver thread so no in-flight tick
     * races against the shutdowns below. */
    if (brain->chemistry_enabled && coord) {
        (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_CHEMISTRY);
        brain->chemistry_enabled = false;
    }

    chem_teardown(brain);
}
