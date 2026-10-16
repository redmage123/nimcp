/**
 * @file nimcp_brain_init_perception_bridges.h
 * @brief Round A/2: activate 10 previously-orphaned perception bridges.
 *
 * Creates, stores on brain, and (for tick-drivable bridges) hooks into
 * the per-step perception tick. Modules activated:
 *
 *   Predictive-coding (FEP) chain:
 *     - visual_cortex_fep_bridge
 *     - audio_cortex_fep_bridge
 *     - speech_cortex_fep_bridge
 *     - pr_predictive_bridge (consumer of the 3 above)
 *
 *   Immune-cortex modulation:
 *     - visual_immune_bridge
 *     - audio_immune_bridge
 *     - speech_immune_bridge
 *
 *   Cortical hypercolumn routing:
 *     - visual_cortical_bridge
 *     - audio_cortical_bridge
 *     - speech_cortical_bridge
 *
 * Cortical bridges are sensor-input-driven (not tick-driven) so they are
 * created + stored + destroyed here but not driven from the per-step tick.
 * They become available for the sensory ingest path to use.
 */
#ifndef NIMCP_BRAIN_INIT_PERCEPTION_BRIDGES_H
#define NIMCP_BRAIN_INIT_PERCEPTION_BRIDGES_H

#include <stdbool.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create all 10 perception bridges. Each is gated on its target
 *        subsystem existing (immune requires brain->immune_system, etc.)
 *        — missing targets produce NULL bridge slots, safe no-op later.
 * @return true always (non-fatal on any sub-failure).
 */
bool nimcp_brain_factory_init_perception_bridges_subsystem(brain_t brain);

/** Destroy every perception bridge. NULL-safe. */
void nimcp_brain_destroy_perception_bridges(brain_t brain);

/**
 * @brief Advance time-driven perception bridges one tick.
 *
 * Drives FEP bridges (3) + immune bridges (3) via their *_update(dt)
 * entry points. Cortical bridges are input-driven — skipped here.
 *
 * @param brain  Brain instance.
 * @param dt_ms  Wall-clock delta since the last tick (milliseconds).
 */
void nimcp_brain_tick_perception_bridges(brain_t brain, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif
