/**
 * @file nimcp_brain_tick_biology.h
 * @brief Hot-path tick for the Wave-3 biology cluster
 *        (epigenetics + neurogenesis + neurovascular).
 *
 * WHAT: Single entry point that advances all three Wave-3 biology modules
 *       one step. Body mirrors the three `*_tick_wrapper` functions in
 *       `nimcp_brain_init_biology.c` — same null-guards, same dt units —
 *       but exposed publicly so brain_learn_vector() and brain_decide()
 *       can call it on their own cadence in addition to the coordinator's
 *       driven cycles.
 *
 * WHY:  Biology was previously advanced only via the cycle coordinator's
 *       own thread. If the coordinator is disabled or stalled, the three
 *       modules freeze. Adding a hot-path driver guarantees state still
 *       advances during training/inference regardless of coordinator state.
 *
 * HOW:  Safe to call on partially initialized brains; each module has its
 *       own null-guard + enabled flag. Redundant with the coordinator's
 *       driven cycles — same tick may fire 3 times per ~16ms window (once
 *       per path). All module updates are idempotent for small dt, so the
 *       cost is "state advances faster than real-time," not correctness.
 *
 * CONTRACT:
 *   - No-op on NULL brain.
 *   - No-op on each module that is NULL or has `*_enabled == false`.
 *   - Non-blocking. No locks held across calls into biology modules.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_BIOLOGY_H
#define NIMCP_BRAIN_TICK_BIOLOGY_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance all 3 Wave-3 biology modules one tick.
 *
 * Each module is null-guarded and gated by its enabled flag:
 *   - epigenetics  (dt in seconds, module contract; we convert from ms)
 *   - neurogenesis (dt in seconds, module contract; we convert from ms)
 *   - NVC system   (dt in milliseconds, module contract; passed through)
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds.
 */
void brain_tick_biology(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_BIOLOGY_H */
