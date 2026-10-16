/**
 * @file nimcp_brain_tick_hypothalamus.h
 * @brief Wave 8B-c hot-path tick driver for the hypothalamus adapter
 *        (homeostasis / drives / circadian / HPA / autonomic).
 *
 * WHAT: Single entry point that advances the hypothalamus adapter one step.
 *       Internally calls hypothalamus_update(adapter, delta_time_us) which
 *       sequences SCN circadian, homeostatic regulation, HPA axis, and
 *       autonomic updates.
 *
 * WHY:  Prior to Wave 8B-c the hypothalamus_update() entry point had no
 *       hot-path or coordinator caller. SCN drift, homeostatic drives
 *       (hunger/thirst/thermoregulation), HPA cortisol decay and autonomic
 *       balance therefore froze whenever the bridge wasn't explicitly
 *       driven by another module.
 *
 * HOW:  Null-guarded on brain + brain->hypothalamus + hypothalamus_enabled.
 *       Converts dt_ms (float) → delta_time_us (uint64_t) for the underlying
 *       update API. Idempotent for small dt — over-calling advances state
 *       faster but not incorrectly.
 *
 * CONTRACT:
 *   - No-op on NULL brain.
 *   - No-op when brain->hypothalamus is NULL or hypothalamus_enabled is false.
 *   - Non-blocking (the adapter's own mutex serialises internal access).
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_HYPOTHALAMUS_H
#define NIMCP_BRAIN_TICK_HYPOTHALAMUS_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the hypothalamus adapter one tick.
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds.
 *               Converted to microseconds for the underlying API.
 */
void brain_tick_hypothalamus(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_HYPOTHALAMUS_H */
