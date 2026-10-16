/**
 * @file nimcp_brain_init_biology.h
 * @brief Brain factory init for biology cluster (epigenetics, neurogenesis, NVC)
 *
 * WHAT: Creates the three biology modules and registers each with the cycle
 *       coordinator as a driven cycle. Before this wiring these modules were
 *       statues — header/implementation present, never instantiated, never
 *       ticked. See the "BIOLOGY MODULES" block in nimcp_brain_internal.h
 *       for per-module description + cadence rationale.
 *
 * WHY:  The state advancement half of the statue fix. Consumer-side wiring
 *       (STDP reading get_plasticity(), SNN listening to neurogenesis
 *       callbacks, metabolic modulation sampling BOLD) remains a separate
 *       follow-up — the present header only advances state.
 *
 * HOW:  Each create step is NULL-tolerant. A partial-failure during init
 *       leaves the earlier successful sub-modules running and the failed
 *       one as NULL; no crash. destroy(), invoked via brain_destroy, will
 *       unregister_driven each live module (stops + joins its driver
 *       thread) before freeing.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_BIOLOGY_H
#define NIMCP_BRAIN_INIT_BIOLOGY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — full definition in nimcp_brain_internal.h, not needed here. */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Create epigenetics + neurogenesis + neurovascular and register each
 *        as a driven cycle with the coordinator.
 *
 * Idempotent: skips any module whose field is already non-NULL. Returns true
 * if at least one module was successfully wired, false only if brain is NULL
 * or no module could be created.
 *
 * Preconditions:
 *   - brain->cycle_coordinator must exist (otherwise the register_driven call
 *     fails and the module stays as a statue even though it was created).
 *     Callers in the wave dispatcher should run this AFTER the cycle
 *     coordinator init wave.
 */
bool nimcp_brain_factory_init_biology_subsystem(brain_t brain);

/**
 * @brief Unregister all driven biology cycles and destroy the modules.
 *
 * Called from brain_destroy. Unregister implicitly joins each driver thread
 * (register_driven contract), so destruction is safe even while ticks are
 * in flight. Each sub-module destruction is NULL-tolerant.
 */
void nimcp_brain_factory_destroy_biology_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_BIOLOGY_H */
