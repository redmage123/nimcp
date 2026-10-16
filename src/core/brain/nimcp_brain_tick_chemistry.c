/**
 * @file nimcp_brain_tick_chemistry.c
 * @brief Implements brain_tick_chemistry() — hot-path tick for the
 *        Wave-6 chemistry cluster.
 *
 * TYPEDEF-COLLISION NOTE:
 *   Each chemistry header redefines `nimcp_brain_t` as its own local
 *   forward decl. We sidestep the collision with nimcp.h's typedef
 *   using the same `#define nimcp_brain_t chem_brain_opaque_t` trick
 *   as nimcp_brain_init_chemistry.c.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#define nimcp_brain_t chem_brain_opaque_t
#include "chemistry/ph/nimcp_ph_dynamics.h"
#include "chemistry/ph/nimcp_proton_pumps.h"
#include "chemistry/ph/nimcp_buffer_systems.h"
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
#undef nimcp_brain_t

#include "core/brain/nimcp_brain_tick_chemistry.h"
#include "core/brain/nimcp_brain_internal.h"

void brain_tick_chemistry(brain_t brain, float dt_ms)
{
    if (!brain || !brain->chemistry_enabled) {
        return;
    }

    /* 1. Proton pumps — H+ source term. Needs three pH samples; use
     *    physiological defaults (7.2 cyto, 7.4 extra, 5.5 vesicular)
     *    per the chemistry_tick_wrapper comment. */
    if (brain->proton_pumps) {
        (void)nimcp_pump_update((nimcp_pump_system_t*)brain->proton_pumps,
                                dt_ms,
                                7.2f,   /* intracellular */
                                7.4f,   /* extracellular */
                                5.5f);  /* vesicular */
    }

    /* 2. Buffer systems — absorb/release H+. */
    if (brain->buffer_manager) {
        (void)nimcp_buffer_update(
            (nimcp_buffer_manager_t*)brain->buffer_manager, dt_ms);
    }

    /* 3. pH dynamics — integrates net H+ into local pH. */
    if (brain->ph_system) {
        (void)nimcp_ph_update((nimcp_ph_system_t*)brain->ph_system, dt_ms);
    }

    /* 4. NO system — pH-dependent NOS activity; must see new pH. */
    if (brain->no_system) {
        (void)nimcp_no_update((nimcp_no_system_t*)brain->no_system, dt_ms);
    }
}
