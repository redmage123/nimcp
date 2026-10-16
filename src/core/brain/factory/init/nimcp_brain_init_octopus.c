/**
 * @file nimcp_brain_init_octopus.c
 * @brief Octopus cognitive module — factory init (Phase 1)
 *
 * Creates the octopus_system_t on brain startup with the default 8 arms
 * and attaches it to brain->octopus. Subsequent phases will register
 * bridges (swarm, immune, ethics, world, fep, bio-async) via the
 * octopus_set_*_hook() API — those live in their own init files.
 *
 * @see include/cognitive/octopus/nimcp_octopus.h
 */

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/octopus/nimcp_octopus.h"
#include "cognitive/octopus/nimcp_octopus_bridges.h"
#include "utils/logging/nimcp_logging.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize the octopus cognitive module on a brain.
 *
 * Safe to call on a partially-initialized brain; only allocates octopus
 * state and sets brain->octopus. Does NOT wire any bridges (that's
 * Phase 2+ via separate init files for each bridge).
 *
 * @return true on success (or if already initialized); false on allocation failure.
 */
bool nimcp_brain_factory_init_octopus_subsystem(brain_t brain) {
    if (!brain) return false;
    if (brain->octopus) return true;  /* idempotent */

    /* OCTOPUS_DEFAULT_N_ARMS (8) — matches biological octopus. Override
     * via brain config in a future phase when we add config plumbing. */
    octopus_system_t* ctx = octopus_create(0);
    if (!ctx) {
        NIMCP_LOGGING_ERROR("brain_init_octopus: octopus_create failed");
        return false;
    }
    brain->octopus = (void*)ctx;
    brain->octopus_enabled = true;

    /* Phase 2a: install bridge hooks that connect the octopus to peer
     * subsystems (ethics / swarm / world / fep / bio-async / immune).
     * Non-fatal if it fails — octopus still works, just without hooks. */
    if (!nimcp_octopus_install_bridges(brain)) {
        NIMCP_LOGGING_WARN("brain_init_octopus: bridge install failed "
                           "(octopus will run without peer hooks)");
    }

    NIMCP_LOGGING_INFO("brain_init_octopus: octopus module initialized "
                       "(%u arms, bridges wired)",
                       octopus_get_n_arms(ctx));
    return true;
}
