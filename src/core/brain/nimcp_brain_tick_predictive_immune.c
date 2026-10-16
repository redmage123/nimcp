/**
 * @file nimcp_brain_tick_predictive_immune.c
 * @brief Implements brain_tick_predictive_immune() — hot-path tick for
 *        the predictive-immune coupling module.
 *
 * See nimcp_brain_tick_predictive_immune.h for rationale. This file is
 * the thin driver wired into brain_learn_vector() and brain_decide() so
 * state still advances when the cycle coordinator isn't running.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_predictive_immune.h"
#include "core/brain/nimcp_brain_internal.h"

/* predictive_immune header — doesn't redefine nimcp_brain_t so safe to
 * include after brain_internal.h. */
#include "cognitive/nimcp_predictive_immune.h"

void brain_tick_predictive_immune(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }
    if (!brain->predictive_immune || !brain->predictive_immune_enabled) {
        return;
    }

    /* Module's dt-in-ms contract — pass straight through. Internal dynamics
     * are event/rate-driven; the value is effectively a label. */
    (void)predictive_immune_update(brain->predictive_immune, dt_ms);
}
