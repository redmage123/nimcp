//=============================================================================
// nimcp_brain_tick_bio_async.c - Unified bio-async tick for brain region adapters
//=============================================================================
/**
 * @file nimcp_brain_tick_bio_async.c
 * @brief Drives *_process_bio_messages() on all brain region adapters.
 *
 * WHAT: Implements brain_tick_bio_async() — a single call that drains each
 *       brain region adapter's bio-router inbox and advances time-dependent
 *       state for adapters that need a time delta.
 *
 * WHY:  The per-adapter primary ops (hippocampus_process_bio_messages,
 *       prefrontal_process_bio_messages, occipital_process_bio_messages,
 *       parietal_cortex_process_bio_messages, temporal_process_bio_messages,
 *       insula_process_bio_messages, brainstem_update) had zero external
 *       callers. Without this driver, each adapter's inbox grows unbounded
 *       and brainstem time-dependent state never advances.
 *
 * HOW:  For each adapter, check NULL + `*_enabled` flag, then invoke the
 *       primary op with `_BIO_BATCH` as the per-tick drain budget. Brainstem
 *       uses `brainstem_update(adapter, dt_ms)` since it advances time rather
 *       than draining messages.
 *
 * DRAIN BUDGET: _BIO_BATCH = 16 messages per adapter per tick. Keeps per-tick
 *       wall time bounded while still making progress against the inbox.
 *
 * @author NIMCP Development Team
 * @date 2026-04-20
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_bio_async.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include "core/brain/regions/temporal/nimcp_temporal_adapter.h"
#include "core/brain/regions/insula/nimcp_insula_adapter.h"
#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"

#include "utils/logging/nimcp_logging.h"

/* Maximum bio-router messages drained per adapter per tick.
 * Bounds per-tick wall time while ensuring steady inbox progress. */
#define _BIO_BATCH 16u

void brain_tick_bio_async(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }

    NIMCP_LOGGING_TRACE("brain_tick_bio_async: dt_ms=%.3f", (double)dt_ms);

    /* Hippocampus — drains inbox up to _BIO_BATCH messages. */
    if (brain->hippocampus && brain->hippocampus_enabled) {
        (void)hippocampus_process_bio_messages(
            (hippocampus_adapter_t*)brain->hippocampus, _BIO_BATCH);
    }

    /* Prefrontal cortex. */
    if (brain->prefrontal && brain->prefrontal_enabled) {
        (void)prefrontal_process_bio_messages(
            (prefrontal_adapter_t*)brain->prefrontal, _BIO_BATCH);
    }

    /* Occipital cortex. */
    if (brain->occipital && brain->occipital_enabled) {
        (void)occipital_process_bio_messages(
            (occipital_adapter_t*)brain->occipital, _BIO_BATCH);
    }

    /* Parietal cortex adapter (distinct from cognitive parietal_lobe_t).
     * Field is `parietal_cortex`, not `parietal`, per brain_internal.h. */
    if (brain->parietal_cortex && brain->parietal_cortex_enabled) {
        (void)parietal_cortex_process_bio_messages(
            (parietal_adapter_t*)brain->parietal_cortex, _BIO_BATCH);
    }

    /* Temporal cortex. */
    if (brain->temporal && brain->temporal_enabled) {
        (void)temporal_process_bio_messages(
            (temporal_adapter_t*)brain->temporal, _BIO_BATCH);
    }

    /* Insula. */
    if (brain->insula && brain->insula_enabled) {
        (void)insula_process_bio_messages(
            (insula_adapter_t*)brain->insula, _BIO_BATCH);
    }

    /* Brainstem — advances time-dependent state via dt rather than draining
     * a message queue. Returns bool; discarded. */
    if (brain->brainstem && brain->brainstem_enabled) {
        (void)brainstem_update(
            (brainstem_adapter_t*)brain->brainstem, dt_ms);
    }
}
