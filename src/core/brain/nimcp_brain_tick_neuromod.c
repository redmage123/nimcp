//=============================================================================
// nimcp_brain_tick_neuromod.c - Unified tick for neuromodulatory nuclei
//=============================================================================
/**
 * @file nimcp_brain_tick_neuromod.c
 * @brief Implements brain_tick_neuromod() — drives medulla + 4 neuromodulatory
 *        adapters (LC, VTA, raphe, habenula) one step per call.
 *
 * See header for rationale. This file is the sibling "driver" created by
 * Wave 8B-a; the call site in brain_learn_vector() / inference hot path is
 * wired from a separate patch.
 *
 * TIME UNITS:
 *   - Exported function takes dt_ms (consistent with every other
 *     brain_tick_* driver).
 *   - medulla_update() doc-comments dt as seconds; we convert ms → s.
 *   - The 4 neuromod adapter updates take dt in ms directly.
 *
 * TYPEDEF COLLISION NOTES:
 *   - brain_internal.h already includes nimcp_medulla.h — no trampoline
 *     needed for medulla.
 *   - The 4 adapter headers pull in their own region headers. In probing,
 *     none collided with brain_internal typedefs; if a future collision
 *     appears, wrap the offending include with a `#define foo_t local_t`
 *     trampoline like nimcp_brain_init_cochlea.c does.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_neuromod.h"
#include "core/brain/nimcp_brain_internal.h"

/* medulla_t is already declared via brain_internal.h's include of
 * nimcp_medulla.h — no extra include needed here. */

/* Four neuromodulatory-nucleus adapter APIs. Each adapter's *_update takes
 * (adapter_handle, float dt_ms) and returns an int/error code we discard. */
#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"
#include "core/brain/regions/vta/nimcp_vta_adapter.h"
#include "core/brain/regions/raphe/nimcp_raphe_adapter.h"
#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"

void brain_tick_neuromod(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }

    /* Medulla orchestrator — takes dt in SECONDS per nimcp_medulla.h. */
    if (brain->medulla && brain->medulla_enabled) {
        const float dt_s = dt_ms * 0.001f;
        (void)medulla_update(brain->medulla, dt_s);
    }

    /* Locus Coeruleus — norepinephrine / arousal & attention.
     * Adapter field is `lc_adapter`; gated by `lc_enabled`. */
    if (brain->lc_adapter && brain->lc_enabled) {
        (void)nimcp_lc_adapter_update(
            (nimcp_lc_adapter_t)brain->lc_adapter, dt_ms);
    }

    /* Ventral Tegmental Area — dopamine / reward & motivation. */
    if (brain->vta_adapter && brain->vta_enabled) {
        (void)nimcp_vta_adapter_update(
            (nimcp_vta_adapter_t)brain->vta_adapter, dt_ms);
    }

    /* Raphe nuclei — serotonin / mood, patience, impulse control. */
    if (brain->raphe_adapter && brain->raphe_enabled) {
        (void)nimcp_raphe_adapter_update(
            (nimcp_raphe_adapter_t)brain->raphe_adapter, dt_ms);
    }

    /* Habenula — "anti-reward" / negative RPE / aversion. */
    if (brain->habenula_adapter && brain->habenula_enabled) {
        (void)nimcp_habenula_adapter_update(
            (nimcp_habenula_adapter_t)brain->habenula_adapter, dt_ms);
    }
}
