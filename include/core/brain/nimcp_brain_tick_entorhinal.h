/**
 * @file nimcp_brain_tick_entorhinal.h
 * @brief Wave 8B-c hot-path tick driver for the entorhinal cortex
 *        (spatial / grid cells / MTL).
 *
 * WHAT: Single entry point intended to advance the entorhinal cortex
 *       (entorhinal_adapter_t / nimcp_entorhinal_t) one step. The
 *       underlying APIs are entorhinal_adapter_update(adapter, dt) and
 *       entorhinal_bidirectional_update(ec, dt).
 *
 * STATUS: PARTIAL. The entorhinal cortex is NOT owned by `brain_t` at
 * the time of writing — there is no brain->entorhinal field. The adapter
 * is created via entorhinal_adapter_create() but stored only in
 * downstream bridges (perirhinal->entorhinal_bridge.entorhinal,
 * hippocampus->entorhinal_bridge.entorhinal). Until a brain accessor is
 * added (e.g. brain_get_entorhinal()) this driver is a documented no-op
 * shell that null-guards on brain and returns. The header + .c are in
 * place so that future wiring only requires uncommenting the body.
 *
 * WHY:  Even as a no-op shell, registering this driver:
 *       - Documents the integration gap (no brain owner for entorhinal)
 *       - Reserves the BRAIN_CYCLE_ENTORHINAL coordinator slot
 *       - Provides a single seam for the future fix
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_TICK_ENTORHINAL_H
#define NIMCP_BRAIN_TICK_ENTORHINAL_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the entorhinal cortex one tick (PARTIAL — no-op until
 *        brain_t exposes the entorhinal pointer).
 *
 * @param brain  Internal brain pointer. Safe on NULL (no-op).
 * @param dt_ms  Elapsed time since last tick, in milliseconds. Currently
 *               unused — see STATUS in the file header.
 */
void brain_tick_entorhinal(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_ENTORHINAL_H */
