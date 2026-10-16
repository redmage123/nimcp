//=============================================================================
// nimcp_brain_tick_bio_async.h - Unified bio-async tick for brain region adapters
//=============================================================================
/**
 * @file nimcp_brain_tick_bio_async.h
 * @brief Drains per-module bio-router inboxes across all brain region adapters.
 *
 * WHAT: Unified tick that drives the *_process_bio_messages() primary op for
 *       each brain region adapter (hippocampus, prefrontal, occipital, parietal
 *       cortex, temporal, insula, brainstem).
 *
 * WHY:  These primary ops currently have zero external callers — they are
 *       "statues" from the audit. Without a driver, each adapter's bio_router
 *       inbox grows unbounded and their time-dependent state never advances.
 *
 * HOW:  Single entry point `brain_tick_bio_async(brain, dt_ms)` invoked from
 *       the brain's per-step pipeline. Each adapter is NULL-guarded and gated
 *       by its `*_enabled` flag. Per-tick drain budget is `_BIO_BATCH` messages
 *       (see .c). Brainstem uses `brainstem_update(adapter, dt_ms)` since it
 *       advances time-dependent state rather than draining a message queue.
 *
 * CONTRACT:
 *   - Safe to call on a partially initialized brain (missing adapters skipped).
 *   - Never blocks; each adapter drains at most _BIO_BATCH messages per tick.
 *   - No-op on NULL brain.
 *
 * @author NIMCP Development Team
 * @date 2026-04-20
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_BIO_ASYNC_H
#define NIMCP_BRAIN_TICK_BIO_ASYNC_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance all registered brain region adapters one bio-async tick.
 *
 * Drains each adapter's per-module bio-router inbox (up to an internal batch
 * budget) and advances brainstem time-dependent state. No-op on brains where
 * a region adapter isn't initialized or is disabled.
 *
 * @param brain   Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms   Elapsed time since last tick, in milliseconds. Forwarded to
 *                adapters that need a time delta (currently: brainstem).
 */
void brain_tick_bio_async(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_BIO_ASYNC_H */
