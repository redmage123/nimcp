/**
 * @file nimcp_brain_tick_intuitive_physics.h
 * @brief Hot-path + coordinator-driven tick for the intuitive physics engine.
 *
 * WHAT: Wraps intuitive_physics_step() so it can be invoked uniformly from
 *       brain_learn_vector (training), the existing brain_decide stage task
 *       (inference), and a driven BRAIN_CYCLE_INTUITIVE_PHYSICS cycle.
 *
 * WHY:  The existing stage_physics_task inside brain_decide's parallel
 *       stage pipeline already fires on inference. Without this driver the
 *       engine doesn't advance during training (which skips that stage
 *       system) or during idle periods. Per-wave audit (2026-04-24) this
 *       is a HIGH-priority statue-fix — integration is in place but state
 *       only advances on one of three paths.
 *
 * CONTRACT:
 *   - No-op on NULL brain, NULL intuitive_physics, or when the module's
 *     enabled flag is cleared.
 *   - dt is expected in milliseconds; intuitive_physics_step takes seconds
 *     internally and we convert.
 *   - Non-blocking. Delegates to intuitive_physics_step().
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_INTUITIVE_PHYSICS_H
#define NIMCP_BRAIN_TICK_INTUITIVE_PHYSICS_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the intuitive physics engine one tick.
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds. Converted
 *               to seconds internally for intuitive_physics_step().
 */
void brain_tick_intuitive_physics(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_INTUITIVE_PHYSICS_H */
