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

    /* Placeholder loss = 0.0f. meta_adapt_learning_rate treats a zero loss
     * as "no new error signal" and returns the current LR unchanged. The
     * primary call site in brain_learn_vector still passes the real loss;
     * this tick is for the inference + coordinator paths where we have no
     * fresh error signal but still want the module exercised (e.g. for
     * internal state-decay or stats accumulation within the meta module). */
    (void)meta_adapt_learning_rate(brain->meta_learner,
                                   META_REGION_ASSOCIATION,
                                   0.0f);
}
