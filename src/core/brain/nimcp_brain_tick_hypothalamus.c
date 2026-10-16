/**
 * @file nimcp_brain_tick_hypothalamus.c
 * @brief Implements brain_tick_hypothalamus() — Wave 8B-c hot-path tick.
 *
 * Drives the hypothalamus_update() pipeline (SCN circadian, homeostatic
 * regulation, HPA axis, autonomic outputs). See the header for rationale.
 *
 * NOTE on dt units: hypothalamus_update() takes uint64_t microseconds; we
 * accept float milliseconds at the brain layer for parity with the other
 * tick drivers (biology/chemistry/sensorimotor/etc.).
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_hypothalamus.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"

void brain_tick_hypothalamus(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }

    if (!brain->hypothalamus || !brain->hypothalamus_enabled) {
        return;
    }

    /* Convert ms → us for the adapter's update API. Clamp to >= 1 us so
     * the adapter never sees a zero-delta tick (its internal state machine
     * uses delta_time_us as a smoothing constant). */
    uint64_t delta_us = (uint64_t)(dt_ms * 1000.0f);
    if (delta_us == 0) {
        delta_us = 1;
    }

    (void)hypothalamus_update(
        (hypothalamus_adapter_t*)brain->hypothalamus, delta_us);
}
