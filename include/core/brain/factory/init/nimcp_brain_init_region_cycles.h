/**
 * @file nimcp_brain_init_region_cycles.h
 * @brief Register Wave 8B/8C tick drivers as driven cycles on the coordinator.
 *
 * WHAT: Wraps the four brain_tick_* functions (neuromod, sensorimotor,
 *       language, physics_bridges) as BRAIN_CYCLE_* driven cycles so they
 *       advance on their own thread every ~16ms regardless of whether
 *       brain_decide / brain_learn_vector is being called.
 *
 * WHY:  These 4 tick drivers are currently hooked into brain_learn_vector
 *       (training) and brain_decide (inference). During pure inference
 *       with no learning the training hook is silent; during idle periods
 *       neither fires. The coordinator ensures state still advances.
 *
 * HOW:  Registers each as its own cycle type (BRAIN_CYCLE_NEUROMOD etc.)
 *       at 16ms cadence. Uses the thin wrapper pattern — the actual tick
 *       functions are shared with the hot-path hooks, so there's only one
 *       implementation of e.g. brain_tick_neuromod and both drivers call it.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_REGION_CYCLES_H
#define NIMCP_BRAIN_INIT_REGION_CYCLES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Register 4 region-adapter driven cycles with the coordinator.
 * No-op if brain->cycle_coordinator is NULL or disabled.
 * Returns true iff at least one cycle was registered.
 */
bool nimcp_brain_factory_init_region_cycles_subsystem(brain_t brain);

/**
 * @brief Unregister the 4 region-adapter cycles (joins driver threads).
 * NULL-tolerant; safe to call when none were registered.
 */
void nimcp_brain_factory_destroy_region_cycles_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_REGION_CYCLES_H */
