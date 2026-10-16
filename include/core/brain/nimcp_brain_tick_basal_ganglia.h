/**
 * @file nimcp_brain_tick_basal_ganglia.h
 * @brief Wave 8B-c hot-path tick driver for the basal ganglia
 *        (action selection / reward gating).
 *
 * WHAT: Single entry point that advances the enhanced basal ganglia
 *       (bg_enhanced_t) one step. Calls bg_enhanced_step(dt_ms) which
 *       advances striatum dynamics, beta oscillations, vigor, model-based
 *       evaluation, sequence chunking, hierarchical RL, neuromodulators,
 *       and outcome devaluation in one pass.
 *
 * WHY:  Prior to Wave 8B-c bg_enhanced_step had no recurring caller from
 *       the brain hot path. Action selection still worked because callers
 *       invoked basal_ganglia_select_action directly, but time-decaying
 *       state (beta oscillations, vigor decay, neuromodulator clearance,
 *       outcome devaluation timing) all froze between selection events.
 *
 * HOW:  Null-guarded on brain + brain->basal_ganglia + basal_ganglia_enabled.
 *       bg_enhanced_step is idempotent for small dt — over-calling only
 *       advances state faster.
 *
 * CONTRACT:
 *   - No-op on NULL brain.
 *   - No-op when brain->basal_ganglia is NULL or basal_ganglia_enabled
 *     is false.
 *   - Non-blocking; bg_enhanced_step takes the BG mutex internally.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_BASAL_GANGLIA_H
#define NIMCP_BRAIN_TICK_BASAL_GANGLIA_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the enhanced basal ganglia one tick.
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds. Passed
 *               through to bg_enhanced_step() unchanged.
 */
void brain_tick_basal_ganglia(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_BASAL_GANGLIA_H */
