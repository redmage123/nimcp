/**
 * @file nimcp_brain_init_cochlea.h
 * @brief Brain factory init for cochlea subsystem + 15 consumer bridges (Wave 8A)
 *
 * WHAT: Creates brain->cochlea (cochlea_t) and all 15 cochlea_*_bridge_t
 *       consumers, then registers a single BRAIN_CYCLE_COCHLEA_BRIDGES driven
 *       cycle at 10ms that ticks every live bridge each cycle.
 *
 * WHY:  The 2026-04-24 consumer-bridge inventory flagged the 15 cochlea
 *       bridges as HALF-STATUEs — .c files present and built into libnimcp,
 *       but zero callers from brain_decide / brain_learn_vector. Without a
 *       driver the bridges never tick. Wave 8A attaches them to the cycle
 *       coordinator and also wires update calls into both hot paths so the
 *       bridges are exercised during inference and training.
 *
 * HOW:  Each bridge create step is NULL-tolerant: if the bridge's dep on
 *       brain_struct (e.g. brain->immune_system, brain->fep_orchestrator,
 *       brain->broca) is NULL at init time, the corresponding bridge stays
 *       NULL and the cycle wrapper skips it each tick. Partial success is
 *       a normal outcome.
 *
 * ORDERING:
 *   - Depends on: brain->cycle_coordinator (always created),
 *     brain->immune_system (Wave 11), brain->fep_orchestrator (Wave 14),
 *     brain->medulla (Wave 14), brain->substrate/thalamic_router (Wave 3),
 *     brain->audio_cortex (Wave 1 multimodal), brain->occipital (Wave 15),
 *     brain->broca / brain->collective_cognition / brain->rcog_engine
 *     (later waves). Bridges whose dep is still NULL at the time this
 *     function runs are skipped.
 *   - Runs in Wave 14 or later so the immune + FEP prereqs exist.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_BRAIN_INIT_COCHLEA_H
#define NIMCP_BRAIN_INIT_COCHLEA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — full definition in nimcp_brain_internal.h, not needed here. */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Create cochlea + 15 bridges and register BRAIN_CYCLE_COCHLEA_BRIDGES.
 *
 * Returns true if at least the cochlea itself was successfully created
 * (bridge failures are soft). Returns false only if brain is NULL, the
 * cycle coordinator is absent, or the cochlea could not be created.
 */
bool nimcp_brain_factory_init_cochlea_subsystem(brain_t brain);

/**
 * @brief Unregister the driven cycle, destroy all bridges, destroy cochlea.
 *
 * Called from brain_destroy. Unregister joins the driver thread before any
 * bridge pointer is freed so no tick can be in flight during destruction.
 */
void nimcp_brain_factory_destroy_cochlea_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_COCHLEA_H */
