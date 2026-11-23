/**
 * @file nimcp_brain_tick_meta_learning.c
 * @brief Implements brain_tick_meta_learning() — tick-uniform wrapper
 *        around meta_adapt_learning_rate().
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_meta_learning.h"
#include "core/brain/nimcp_brain_internal.h"

#include "cognitive/nimcp_meta_learning.h"

void brain_tick_meta_learning(brain_t brain, float dt_ms)
{
    (void)dt_ms;  /* meta_adapt_learning_rate has no time arg. */

    if (!brain || !brain->meta_learner) {
        return;
    }

    /* No error signal is available on the tick path, so do NOT call
     * meta_adapt_learning_rate here. The old placeholder loss=0.0f was not a
     * no-op: the bold-driver rule halves the LR whenever loss >= prev_loss
     * and then stores 0 as prev_loss, so every later real loss also counted
     * as "worse" and pinned the region LR at LR_MIN. The real-loss update
     * lives in brain_learn_vector. Kept as a tick-uniform entry point. */
}
