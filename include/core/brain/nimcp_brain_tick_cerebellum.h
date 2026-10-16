/**
 * @file nimcp_brain_tick_cerebellum.h
 * @brief Wave 8B-c hot-path tick driver for the cerebellum adapter
 *        (motor-error forward model + bio-msg drain).
 *
 * WHAT: Single entry point that advances the cerebellum adapter one step.
 *       Drains the per-module bio-router inbox via
 *       cerebellum_process_bio_messages(), which is the adapter's
 *       state-advancement entry point for incoming climbing-fibre / mossy
 *       error / coordination requests.
 *
 * WHY:  The cerebellum_adapter exposes no public dt-only step function.
 *       Its forward-model and gain-adaptation entry points (e.g.
 *       cerebellum_update_forward_model, cerebellum_adapt_gains) all
 *       require external data and cannot be called blindly from a tick.
 *       Without a periodic driver the inbound bio-message queue grows
 *       unbounded and incoming error signals are never integrated.
 *
 * HOW:  Null-guarded on brain + brain->cerebellum + cerebellum_enabled.
 *       Drains up to _CER_BIO_BATCH messages per tick to bound wall time.
 *       dt_ms is accepted for signature uniformity but unused — the
 *       underlying API is data-driven, not time-driven. Marked PARTIAL
 *       in the wave 8B-c audit log: a future cerebellum_step(dt_ms) API
 *       would replace this drain.
 *
 * CONTRACT:
 *   - No-op on NULL brain.
 *   - No-op when brain->cerebellum is NULL or cerebellum_enabled is false.
 *   - Bounded-time: drains at most _CER_BIO_BATCH messages per call.
 *   - Non-blocking; cerebellum_process_bio_messages serialises internally.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_CEREBELLUM_H
#define NIMCP_BRAIN_TICK_CEREBELLUM_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the cerebellum adapter one tick (bio-msg drain).
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds. Currently
 *               unused — the cerebellum adapter's tickable surface is the
 *               bio-message drain, which is data-driven.
 */
void brain_tick_cerebellum(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_CEREBELLUM_H */
