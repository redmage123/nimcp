/**
 * @file nimcp_brain_init_chemistry.h
 * @brief Brain factory init for chemistry cluster
 *
 * WHAT: Creates proton_pumps + buffer_manager + ph_system + no_system and
 *       registers a single BRAIN_CYCLE_CHEMISTRY driven cycle at 10ms that
 *       ticks all four in biological order (protons → buffers → pH → NO).
 *
 * WHY:  All four chemistry modules were statues prior to 2026-04-24 — .c
 *       files were not compiled into the library at all. Wave 6 added them
 *       to CMakeLists and wires them into the cycle coordinator via one
 *       shared cycle type (fewer threads than 4 separate cycles, and the
 *       modules are tightly coupled biologically anyway).
 *
 * SCOPE: State advancement only. Consumer-side wiring (e.g. substrate
 *        consulting pH to modulate membrane potential, plasticity scaling
 *        by local NO concentration) is a separate follow-up.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_CHEMISTRY_H
#define NIMCP_BRAIN_INIT_CHEMISTRY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — full definition in nimcp_brain_internal.h, not needed here. */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Allocate + init all 4 chemistry modules and register the chemistry
 *        cycle with the coordinator.
 *
 * Preconditions:
 *   - brain->cycle_coordinator must exist (otherwise we skip — the modules
 *     aren't created because there's nowhere to drive them from; this
 *     matches the state-advancement-only scope).
 *
 * Postconditions on success:
 *   - brain->proton_pumps / buffer_manager / ph_system / no_system all
 *     non-NULL and initialized.
 *   - brain->chemistry_enabled == true.
 *   - BRAIN_CYCLE_CHEMISTRY is registered as a driven cycle at 10ms.
 *
 * Returns true iff the cycle was successfully registered (all 4 modules
 * live). On partial failure rolls back: already-initialized modules are
 * shut down, all fields return to NULL, returns false.
 */
bool nimcp_brain_factory_init_chemistry_subsystem(brain_t brain);

/**
 * @brief Unregister the chemistry cycle and shut down all 4 modules.
 * Called from brain_destroy; NULL-tolerant; safe to call multiple times.
 */
void nimcp_brain_factory_destroy_chemistry_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_CHEMISTRY_H */
