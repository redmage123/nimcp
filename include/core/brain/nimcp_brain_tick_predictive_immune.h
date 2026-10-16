/**
 * @file nimcp_brain_tick_predictive_immune.h
 * @brief Hot-path tick for the predictive-immune coupling module.
 *
 * WHAT: Single entry point that advances the predictive_immune coupling
 *       module one step. Mirrors the predictive_immune_tick_wrapper in
 *       nimcp_brain_init_predictive_immune.c but exposed publicly so
 *       brain_learn_vector() and brain_decide() can call it as part of
 *       their per-step pipelines in addition to the coordinator's 100ms
 *       driven cycle.
 *
 * WHY:  Without a hot-path call, the module only advances while the
 *       coordinator thread is alive. This driver guarantees state advances
 *       during training and inference even if the coordinator is disabled
 *       or stalled.
 *
 * CONTRACT:
 *   - No-op on NULL brain, NULL predictive_immune, or when
 *     predictive_immune_enabled is false.
 *   - Non-blocking. Delegates to predictive_immune_update().
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_PREDICTIVE_IMMUNE_H
#define NIMCP_BRAIN_TICK_PREDICTIVE_IMMUNE_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the predictive-immune coupling module one tick.
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds.
 *               predictive_immune_update() treats dt_ms as a
 *               timestep label; internal dynamics are rate/event
 *               driven, not clocked by this argument.
 */
void brain_tick_predictive_immune(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_PREDICTIVE_IMMUNE_H */
