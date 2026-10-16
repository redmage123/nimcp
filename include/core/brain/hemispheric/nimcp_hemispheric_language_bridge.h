/**
 * @file nimcp_hemispheric_language_bridge.h
 * @brief Lateralized grounded_language wiring across the bilateral
 *        brain (left = dominant for language, right = prosody/emotion).
 * @date 2026-05-05
 *
 * WHAT: Wires the left hemisphere's brain_t->grounded_lang as the
 *       primary linguistic processor and routes language events
 *       across the corpus callosum to the right hemisphere for
 *       emotional/prosodic processing. Models biological aphasia
 *       when callosum is severed or left hemisphere is injured.
 *
 * WHY:  Pre-bridge, the hemispheric brain made no use of GL even
 *       though its docs explicitly assert "Language (Broca's)" lives
 *       in the left hemisphere. Both hemispheres' brain_t could
 *       independently host a GL — wasteful and biologically wrong.
 *       The right hemisphere's documented role (prosody, emotional
 *       tone, narrative gestalt) was unfed.
 *
 * HOW:  Subscribe a callback to the LEFT GL's bus that forwards
 *       COMPREHENDED + PRODUCED events as COGNITIVE-channel callosum
 *       messages, and forwards GROUNDED events with non-zero arousal
 *       as EMOTIONAL-channel messages. The right hemisphere reads
 *       its receive queue on each tick and updates its emotion
 *       state. Lateralization is enforced: the right hemisphere's
 *       GL (if present) is muted by the bridge.
 */

#ifndef NIMCP_HEMISPHERIC_LANGUAGE_BRIDGE_H
#define NIMCP_HEMISPHERIC_LANGUAGE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Per-bridge stats — tracks left→right traffic counts.
 */
typedef struct {
    uint64_t lh_comprehensions;   /**< COMPREHENDED events from left */
    uint64_t lh_productions;      /**< PRODUCED events from left */
    uint64_t lh_groundings;       /**< GROUNDED events from left */
    uint64_t callosum_msgs_sent;  /**< Messages crossing the callosum */
    uint64_t callosum_msgs_dropped; /**< Dropped due to bandwidth/severance */
    uint64_t rh_emotion_updates;  /**< Right-hemisphere emotion taps */
    uint64_t aphasia_dropped;     /**< Dropped pre-send by aphasia gate */
} hemispheric_language_stats_t;

/**
 * @brief Wire grounded_language into the bilateral brain. Idempotent.
 *
 *   - LEFT GL becomes the primary lexical processor.
 *   - RIGHT GL (if present) is suppressed via a no-op mute flag so
 *     the two hemispheres don't compete for the same training updates.
 *   - A subscriber on the left GL forwards events across the callosum.
 *
 * Returns 0 on success, -1 if the hemispheric brain or its hemispheres
 * are missing, -2 if the left brain has no GL.
 *
 * @param hb     Bilateral brain
 */
int hemispheric_language_bridge_install(hemispheric_brain_t* hb);

/**
 * @brief Tear down the bridge (unsubscribe + clear stats). Used on
 *        hemispheric brain destroy and on split-brain simulation.
 */
void hemispheric_language_bridge_uninstall(hemispheric_brain_t* hb);

/**
 * @brief Drain the right hemisphere's pending callosum language
 *        messages. Called once per tick from the brain coordinator.
 *        Each message updates the right hemisphere's emotion state.
 *
 * @return Number of messages processed.
 */
int hemispheric_language_bridge_tick(hemispheric_brain_t* hb);

/**
 * @brief Read current bridge stats. Out is zero-filled on entry.
 */
int hemispheric_language_bridge_get_stats(
    hemispheric_brain_t* hb,
    hemispheric_language_stats_t* out);

/**
 * @brief Simulate aphasia by gating the left hemisphere's GL.
 *        When severity > 0, the bridge replaces the left GL's
 *        comprehend/produce confidence with a degraded scalar
 *        before fanning out (multiplicative).
 *
 * @param severity  [0, 1] — 0=no impairment, 1=total Broca-style aphasia
 */
void hemispheric_language_bridge_set_aphasia(hemispheric_brain_t* hb,
                                               float severity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEMISPHERIC_LANGUAGE_BRIDGE_H */
