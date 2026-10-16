//=============================================================================
// nimcp_brain_tick_language.h - Language tick driver (broca + wernicke)
//=============================================================================
/**
 * @file nimcp_brain_tick_language.h
 * @brief Drains bio-router inboxes for the language regions (broca, wernicke).
 *
 * WHAT: Unified tick that drives the *_process_bio_messages() primary op for
 *       the two language-region adapters (broca, wernicke).
 *
 * WHY:  `broca_process_bio_messages` and `wernicke_process_bio_messages` had
 *       zero external callers prior to Wave 8B-d. Without a driver, each
 *       adapter's bio_router inbox grows unbounded and the language loop
 *       never advances state in response to incoming messages.
 *
 * HOW:  Single entry point `brain_tick_language(brain, dt_ms)` invoked from
 *       the brain's per-step pipeline. Each adapter is NULL-guarded and gated
 *       by its `*_enabled` flag. Per-tick drain budget is 32 messages per
 *       adapter, matching the language pipeline's expected message rate
 *       (higher than `_BIO_BATCH=16` used by the Round A regions because
 *       broca+wernicke talk back-and-forth via the arcuate fasciculus and
 *       accumulate in pairs).
 *
 * CONTRACT:
 *   - Safe to call on a partially initialized brain (missing adapters skipped).
 *   - Never blocks; each adapter drains at most 32 messages per tick.
 *   - No-op on NULL brain.
 *   - `dt_ms` is accepted for signature-consistency with other tick drivers
 *     but is currently unused — language adapters drain messages rather than
 *     advancing time-dependent state.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_LANGUAGE_H
#define NIMCP_BRAIN_TICK_LANGUAGE_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance broca + wernicke adapters one language tick.
 *
 * Drains each language-region adapter's per-module bio-router inbox (up to
 * 32 messages per adapter). No-op on brains where a region adapter isn't
 * initialized or is disabled.
 *
 * @param brain   Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms   Elapsed time since last tick, in milliseconds. Accepted for
 *                signature-consistency with other tick drivers; currently
 *                unused by language adapters.
 */
void brain_tick_language(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_LANGUAGE_H */
