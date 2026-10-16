/**
 * @file nimcp_brain_tick_biology.c
 * @brief Implements brain_tick_biology() — hot-path tick for
 *        epigenetics + neurogenesis + NVC.
 *
 * See nimcp_brain_tick_biology.h for the rationale; this file is the thin
 * driver wired into brain_learn_vector() and brain_decide() so biology
 * state advances even when the cycle coordinator isn't running.
 *
 * TYPEDEF-COLLISION NOTE:
 *   Each biology header redefines `nimcp_brain_t` as its own local
 *   forward decl of `nimcp_brain_struct*`, which collides with nimcp.h's
 *   `nimcp_brain_handle*`. We use the same `#define nimcp_brain_t
 *   bio_brain_opaque_t` trick as nimcp_brain_init_biology.c to sidestep
 *   the collision. Biology modules only stash the pointer opaquely, so
 *   the cast at use sites is safe.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

/* Biology headers BEFORE brain_internal.h — same trampoline as
 * nimcp_brain_init_biology.c. */
#define nimcp_brain_t bio_brain_opaque_t
#include "biology/epigenetics/nimcp_epigenetics.h"
#include "biology/neurogenesis/nimcp_neurogenesis.h"
#include "biology/neurovascular/nimcp_neurovascular.h"
#undef nimcp_brain_t

#include "core/brain/nimcp_brain_tick_biology.h"
#include "core/brain/nimcp_brain_internal.h"

void brain_tick_biology(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }

    /* Epigenetics: update takes dt in SECONDS. Default coord cadence is
     * 100ms; hot-path runs at ~16ms. Either way just convert. */
    if (brain->epigenetics && brain->epigenetics_enabled) {
        const float dt_s = dt_ms * 0.001f;
        (void)nimcp_epigenetics_update(brain->epigenetics, dt_s);
    }

    /* Neurogenesis: update takes dt in SECONDS. Biological cadence is
     * hours; we feed whatever dt we have — the module integrates
     * against its own fixed-cell-cycle clock and ignores huge dts. */
    if (brain->neurogenesis && brain->neurogenesis_enabled) {
        const float dt_s = dt_ms * 0.001f;
        (void)nimcp_neurogenesis_update(brain->neurogenesis, dt_s);
    }

    /* NVC: update takes dt in MILLISECONDS directly. HRF evolves over
     * seconds so the 16ms cadence gives smooth interpolation. */
    if (brain->nvc_system && brain->neurovascular_enabled) {
        (void)nimcp_nvc_update(brain->nvc_system, dt_ms);
    }
}
