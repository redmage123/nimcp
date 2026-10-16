/**
 * @file nimcp_brain_tick_physics_bridges.h
 * @brief Tick driver for 4 HALF-STATUE physics bridges (Wave 8C)
 *
 * Called from brain_learn_vector alongside the other bridge tick drivers.
 * Ticks each live physics bridge (ephaptic_bio_async, ephaptic_fft,
 * hh_bio_async, thermo_bio_async). NULL bridges are skipped.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_TICK_PHYSICS_BRIDGES_H
#define NIMCP_BRAIN_TICK_PHYSICS_BRIDGES_H

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Advance all live physics bridges by dt_ms.
 *
 * Null-safe on brain and on every bridge. No-op if
 * brain->physics_bridges_enabled is false.
 */
void brain_tick_physics_bridges(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TICK_PHYSICS_BRIDGES_H */
