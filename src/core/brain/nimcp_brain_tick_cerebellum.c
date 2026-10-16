/**
 * @file nimcp_brain_tick_cerebellum.c
 * @brief Implements brain_tick_cerebellum() — Wave 8B-c hot-path tick.
 *
 * Drains the cerebellum adapter's bio-router inbox so incoming
 * climbing-fibre / mossy / coordination messages are integrated into
 * forward-model state. Same pattern as the cingulate tick driver: no
 * public dt step exists, so the bio-msg drain serves as the
 * state-advancement primitive.
 *
 * AUDIT NOTE: This is a PARTIAL driver. cerebellum_adapter.h does not
 * expose a `cerebellum_step(dt_ms)` API (only data-driven calls like
 * cerebellum_update_forward_model and cerebellum_adapt_gains exist).
 * If/when such an API is added, this driver should call it in addition
 * to the bio-msg drain.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_cerebellum.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

/* Per-tick bio-router inbox drain budget. Matches the bound used by
 * brain_tick_sensorimotor for cingulate. */
#define _CER_BIO_BATCH 16u

void brain_tick_cerebellum(brain_t brain, float dt_ms)
{
    (void)dt_ms; /* signature parity; cerebellum has no public dt step */

    if (!brain) {
        return;
    }

    if (!brain->cerebellum || !brain->cerebellum_enabled) {
        return;
    }

    (void)cerebellum_process_bio_messages(
        (cerebellum_adapter_t*)brain->cerebellum, _CER_BIO_BATCH);
}
