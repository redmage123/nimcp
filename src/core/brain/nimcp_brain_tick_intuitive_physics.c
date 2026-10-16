/**
 * @file nimcp_brain_tick_intuitive_physics.c
 * @brief Implements brain_tick_intuitive_physics() — tick-uniform wrapper
 *        around intuitive_physics_step().
 *
 * Same semantics as the existing stage_physics_task in
 * nimcp_brain_parallel_stages.c (0.016s fixed dt used there). This driver
 * exists so the engine can also be advanced from the training path and
 * from a coordinator cycle.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_intuitive_physics.h"
#include "core/brain/nimcp_brain_internal.h"

#include "cognitive/physics/nimcp_intuitive_physics.h"

void brain_tick_intuitive_physics(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }
    if (!brain->intuitive_physics || !brain->intuitive_physics_enabled) {
        return;
    }

    /* intuitive_physics_step takes dt in SECONDS. */
    const float dt_s = dt_ms * 0.001f;
    (void)intuitive_physics_step(brain->intuitive_physics, dt_s);
}
