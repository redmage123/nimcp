/**
 * @file nimcp_brain_init_physics_bridges.h
 * @brief Brain factory init for 4 HALF-STATUE physics bridges (Wave 8C)
 *
 * WHAT: Creates the 3 upstream physics modules (ephaptic_system, thermo_state,
 *       hh_population) + 4 consumer bridges (ephaptic_bio_async, ephaptic_fft,
 *       hh_bio_async, thermo_bio_async) and stores them on brain_t so they
 *       live for the brain's lifetime. The 3 quantum "bridges" are purely
 *       functional (no create/destroy/update) and are not wired here.
 *
 * WHY:  The 2026-04-24 consumer-bridge inventory flagged these 4 physics
 *       bridges as HALF-STATUEs — .c files exist and compile, but zero
 *       callers from brain_decide / brain_learn_vector. Without a driver
 *       the bridges never tick. Wave 8C attaches them to the hot path so
 *       they are exercised during training + inference.
 *
 * HOW:  Each bridge create step is NULL-tolerant: if bio_router is not live
 *       or the upstream module couldn't be created, the corresponding bridge
 *       stays NULL and the tick driver skips it each pass. Partial success
 *       is a normal outcome.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_PHYSICS_BRIDGES_H
#define NIMCP_BRAIN_INIT_PHYSICS_BRIDGES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — full definition in nimcp_brain_internal.h, not needed here. */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Create physics upstream modules + 4 bridges.
 *
 * Returns true on partial or full success. Returns false only if brain is
 * NULL. All downstream failures (module create, bridge create, bio_router
 * absent) are soft — we keep whatever we managed to wire and move on.
 */
bool nimcp_brain_factory_init_physics_bridges_subsystem(brain_t brain);

/**
 * @brief Destroy all physics bridges + upstream modules.
 *
 * Called from brain_destroy. Bridges destroyed first (they may touch
 * upstream), then upstream modules freed.
 */
void nimcp_brain_factory_destroy_physics_bridges_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_PHYSICS_BRIDGES_H */
