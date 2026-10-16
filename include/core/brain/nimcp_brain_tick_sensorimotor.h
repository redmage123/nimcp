//=============================================================================
// nimcp_brain_tick_sensorimotor.h — Sensorimotor + emotional region tick driver
//=============================================================================
/**
 * @file nimcp_brain_tick_sensorimotor.h
 * @brief Advances time-dependent state of sensorimotor + emotional brain regions.
 *
 * WHAT: Unified tick that drives state advancement for six brain regions whose
 *       primary dt-based step functions had zero external callers prior to
 *       Wave 8B-b — motor, olfactory, gustatory, somatosensory, amygdala, and
 *       cingulate cortex.
 *
 * WHY:  Each of these regions is created during brain init but was a "producer
 *       statue" — never ticked from any hot path. Without this driver:
 *       - Olfactory sniff-cycle / adaptation never advances
 *       - Gustatory activation / taste adaptation never decays
 *       - Somatosensory receptive-field state never updates
 *       - Amygdala fear / anxiety never decays toward baseline
 *       - Motor trajectory execution timer never advances
 *       - Cingulate conflict/error accumulator never emits control signals
 *
 * HOW:  Single entry point `brain_tick_sensorimotor(brain, dt_ms)` invoked
 *       from the brain's per-step pipeline. Each region is NULL-guarded and
 *       gated by its `*_enabled` flag. All step functions take `dt_ms` and
 *       a region-appropriate physiological default for extra args (0.0f
 *       neutral for amygdala/cingulate).
 *
 * CONTRACT:
 *   - Safe to call on a partially initialized brain (missing regions skipped).
 *   - Never blocks; each region advances state in O(region-size).
 *   - No-op on NULL brain or dt_ms <= 0.0f.
 *   - No INFO-level logging in the tick body (only TRACE).
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_SENSORIMOTOR_H
#define NIMCP_BRAIN_TICK_SENSORIMOTOR_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance sensorimotor + emotional regions one tick.
 *
 * Drives motor (trajectory execution), olfactory (olfact_update), gustatory
 * (gust_update), somatosensory (soma_update), amygdala (amygdala_step with
 * neutral threat/safety defaults), and cingulate cortex (state integration
 * with neutral conflict defaults).
 *
 * @param brain   Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms   Elapsed time since last tick, in milliseconds. Must be > 0
 *                for amygdala (guarded internally). Forwarded to every region
 *                that needs a time delta.
 */
void brain_tick_sensorimotor(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_SENSORIMOTOR_H */
