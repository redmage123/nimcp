//=============================================================================
// nimcp_brain_tick_neuromod.h - Unified tick for neuromodulatory nuclei
//=============================================================================
/**
 * @file nimcp_brain_tick_neuromod.h
 * @brief Advances the 5 neuromodulatory brain regions one tick.
 *
 * WHAT: Unified tick driver for the brain's neuromodulatory nuclei:
 *       - Medulla oblongata (arousal/protection/circadian orchestrator)
 *       - Locus Coeruleus (LC)       — norepinephrine / arousal & attention
 *       - Ventral Tegmental Area     — dopamine / reward & motivation
 *       - Raphe nuclei               — serotonin / mood & impulse control
 *       - Habenula                   — aversion / "anti-reward" signal
 *
 * WHY:  Per the 2026-04-24 audit, all 5 regions are created at brain init
 *       (see `nimcp_brain_init_neuromod.c` and the medulla init path) but
 *       their per-step update functions have zero external callers — they
 *       are "producer statues": allocated, wired to downstream consumers,
 *       yet never ticked. Without this driver their internal time-dependent
 *       state (arousal decay, DA/NE/5-HT release dynamics, RPE signals,
 *       circadian phase) never advances.
 *
 * HOW:  Single entry point `brain_tick_neuromod(brain, dt_ms)` invoked from
 *       the brain's per-step pipeline. Each region is NULL-guarded and gated
 *       by its `*_enabled` flag. Medulla takes dt in seconds — we convert.
 *       LC/VTA/raphe/habenula adapters take dt in milliseconds directly.
 *
 * CONTRACT:
 *   - Safe to call on a partially initialized brain (missing regions skipped).
 *   - Never blocks.
 *   - No-op on NULL brain.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_NEUROMOD_H
#define NIMCP_BRAIN_TICK_NEUROMOD_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the 5 neuromodulatory nuclei by one tick.
 *
 * For each of medulla, LC, VTA, raphe, habenula: null-guards the region
 * pointer, checks its enabled flag, and calls the region's update with
 * the supplied time delta. No-op on NULL brain or on regions that aren't
 * initialized or are disabled.
 *
 * @param brain   Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms   Elapsed time since last tick, in milliseconds.
 */
void brain_tick_neuromod(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_NEUROMOD_H */
