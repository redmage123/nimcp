/**
 * @file nimcp_brain_init_predictive_immune.h
 * @brief Brain factory init for predictive-immune coupling module
 *
 * WHAT: Creates the predictive-immune integration system and registers it
 *       with the cycle coordinator as a driven cycle (100ms cadence). Before
 *       this wiring the module existed as a statue — header/implementation
 *       present, never instantiated, never ticked.
 *
 * WHY:  The integration layer implements bidirectional coupling between the
 *       brain's predictive processing (free-energy/interoceptive inference)
 *       and the brain immune system:
 *         - Immune → Predictive: inflammation reduces prediction precision
 *         - Predictive → Immune: large prediction errors trigger immune
 *           antigen presentation (adversarial input detector)
 *
 * HOW:  Requires brain->predictive_network (Wave 8) and brain->immune_system
 *       (Wave 11) to exist. If either is NULL at init time, the module is
 *       not created and this factory returns false (module stays a statue —
 *       matching its pre-wiring state). The create function is invoked,
 *       then predictive_immune_start() activates its callbacks, then
 *       register_driven attaches the 100ms periodic tick.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_PREDICTIVE_IMMUNE_H
#define NIMCP_BRAIN_INIT_PREDICTIVE_IMMUNE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — full definition in nimcp_brain_internal.h, not needed here. */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Create predictive-immune coupling and register as a driven cycle.
 *
 * Idempotent: skips creation if brain->predictive_immune is already non-NULL.
 * Returns true on success, false if brain is NULL, if prerequisites
 * (predictive_network, immune_system, cycle_coordinator) are unavailable, or
 * if any step of create + start + register_driven fails.
 *
 * Preconditions:
 *   - brain->cycle_coordinator must exist (register_driven needs it).
 *   - brain->predictive_network must be non-NULL (Wave 8 predictive subsystem).
 *   - brain->immune_system must be non-NULL (Wave 11 immune subsystem).
 *
 * Safe to call from Wave 12 or later. Earlier waves will see NULL prereqs
 * and the function will cleanly skip.
 */
bool nimcp_brain_factory_init_predictive_immune_subsystem(brain_t brain);

/**
 * @brief Unregister driven cycle, stop integration, and destroy the module.
 *
 * Called from brain_destroy. unregister_driven joins the driver thread, then
 * predictive_immune_stop() unregisters its callbacks, then destroy() frees
 * the module. NULL-tolerant; safe to call even if init never ran.
 */
void nimcp_brain_factory_destroy_predictive_immune_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_PREDICTIVE_IMMUNE_H */
