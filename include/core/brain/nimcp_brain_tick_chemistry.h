/**
 * @file nimcp_brain_tick_chemistry.h
 * @brief Hot-path tick for the Wave-6 chemistry cluster
 *        (proton pumps + buffer systems + pH + NO).
 *
 * WHAT: Single entry point that advances all 4 chemistry modules one step.
 *       Body mirrors the chemistry_tick_wrapper in
 *       nimcp_brain_init_chemistry.c (protons → buffers → pH → NO order)
 *       but is exposed publicly so brain_learn_vector() and brain_decide()
 *       can call it on their own cadence.
 *
 * WHY:  Chemistry was previously advanced only via the coordinator's driven
 *       10ms cycle. Adding hot-path drivers guarantees state still advances
 *       during training/inference even when the coordinator is disabled.
 *
 * ORDERING (biological):
 *   1. proton pumps   — move H+ across membranes (source term)
 *   2. buffer systems — absorb/release H+ (damping)
 *   3. pH dynamics    — integrate net H+ change into local pH
 *   4. NO system      — pH-dependent NOS activity; must see new pH
 *
 * CONTRACT:
 *   - No-op on NULL brain or when brain->chemistry_enabled is false.
 *   - Each module is individually null-guarded.
 *   - Non-blocking.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_CHEMISTRY_H
#define NIMCP_BRAIN_TICK_CHEMISTRY_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the 4 Wave-6 chemistry modules one tick.
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds.
 */
void brain_tick_chemistry(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_CHEMISTRY_H */
