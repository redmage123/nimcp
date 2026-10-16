/**
 * @file nimcp_brain_tick_meta_learning.h
 * @brief Hot-path + coordinator-driven tick for meta-learning LR adaptation.
 *
 * WHAT: Wraps meta_adapt_learning_rate() so it can be invoked uniformly from
 *       brain_learn_vector (training), brain_decide (inference), and a
 *       driven BRAIN_CYCLE_META_LEARNING cycle on the coordinator.
 *
 * WHY:  brain_learn_vector already calls meta_adapt_learning_rate with the
 *       real loss (line ~3105) — that call stays as the primary source of
 *       learning-rate adaptation. This tick driver exists so the module is
 *       exercised from paths that don't have a loss value (inference,
 *       coordinator). We pass 0.0f as a placeholder loss in those paths;
 *       meta_adapt_learning_rate interprets 0.0 as "no new signal" and
 *       simply returns the current LR without adjusting it — so calling
 *       from inference/coord is safe and has no unwanted effect on LR.
 *
 * CONTRACT:
 *   - No-op on NULL brain or NULL meta_learner.
 *   - Non-blocking.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_META_LEARNING_H
#define NIMCP_BRAIN_TICK_META_LEARNING_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance meta-learning one tick (LR-adaptation query).
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Ignored — meta_adapt_learning_rate() has no time arg.
 *               Accepted for uniform tick signature with the other
 *               brain_tick_* drivers.
 */
void brain_tick_meta_learning(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_META_LEARNING_H */
