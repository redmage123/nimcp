/**
 * @file nimcp_brain_init_entorhinal.h
 * @brief Entorhinal Cortex Initialization
 *
 * WHAT: Creates the entorhinal adapter (MTL grid-cell / path-integration /
 *       memory-gateway region) and stores it on brain->entorhinal so the
 *       Wave 8B-c tick driver can advance it.
 *
 * WHY:  Until this init runs, brain->entorhinal is NULL and the tick
 *       driver is a no-op. This wires the region into the brain init
 *       orchestrator alongside hypothalamus (Wave 15).
 *
 * HOW:  Thin wrapper around entorhinal_adapter_create(). The adapter
 *       itself is a MINIMAL implementation (see
 *       src/core/brain/regions/entorhinal/nimcp_entorhinal_adapter.c).
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_ENTORHINAL_H
#define NIMCP_BRAIN_INIT_ENTORHINAL_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the entorhinal subsystem on the given brain.
 *
 * Creates entorhinal_adapter_t with default config and stores it in
 * brain->entorhinal. Sets brain->entorhinal_enabled = true on success.
 *
 * Safe to call multiple times — if brain->entorhinal is already set,
 * returns true without re-creating.
 *
 * @param brain Brain instance (must be non-NULL).
 * @return true on success (or already-initialized), false on allocation
 *         failure. Non-fatal overall — caller is expected to treat a
 *         false return as a warning, not a hard error.
 */
bool nimcp_brain_factory_init_entorhinal_subsystem(brain_t brain);

/**
 * @brief Destroy the entorhinal subsystem.
 *
 * Destroys brain->entorhinal (if present) and clears the flag.
 */
void nimcp_brain_factory_destroy_entorhinal_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_ENTORHINAL_H */
