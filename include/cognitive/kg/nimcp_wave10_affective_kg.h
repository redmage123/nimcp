//=============================================================================
// nimcp_wave10_affective_kg.h - Wave W10: Affective / Social KG Integration
//=============================================================================
/**
 * @file nimcp_wave10_affective_kg.h
 * @brief Wave W10 emit + query helpers for 10 affective / social modules.
 *
 * WHY: W10 wires the following `src/cognitive/` modules into
 *      `brain->internal_kg` with structural root nodes + runtime emit events
 *      + one query/read-back each:
 *
 *        1. emotions/nimcp_emotional_system.c           — cog_emotions
 *        2. theory_of_mind/nimcp_theory_of_mind.c       — cog_theory_of_mind
 *        3. mirror_neurons/nimcp_mirror_neurons_part_core.c — cog_mirror_neurons
 *        4. social/nimcp_social_interaction.c           — cog_social_interaction
 *        5. collective_cognition/nimcp_collective_cognition_part_core.c
 *                                                       — cog_collective_cognition
 *        6. personality/nimcp_personality.c             — cog_personality
 *        7. empathetic_response/nimcp_empathetic_response.c — cog_empathetic_response
 *        8. emotion_recognition/nimcp_emotion_recognition_simple.c
 *                                                       — cog_emotion_recognition
 *        9. grief/nimcp_grief_and_loss.c                — cog_grief
 *       10. shadow/nimcp_shadow_emotions.c              — cog_shadow_emotions
 *
 * Each emit function:
 *   - is null/disabled/kg-missing tolerant (silent no-op),
 *   - self-elevates to ADMIN via `brain->internal_kg_admin_token`
 *     (see docs/claude/kg-node-naming-registry.md §7),
 *   - emits a unique event node `<owner>_event_<kind>_<ts>` linked back
 *     to the structural root via BRAIN_KG_EDGE_SENDS_TO,
 *   - restores READ access before returning.
 *
 * Each query function returns a single float (bias value in [0,1] or a
 * neutral 0.5f when no relevant state node is present). Read-paths are
 * designed to let emotion/affect state bias other modules' decisions
 * without coupling hard data types across subsystems.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_WAVE10_AFFECTIVE_KG_H
#define NIMCP_WAVE10_AFFECTIVE_KG_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/* ---------------------------------------------------------------------------
 * Structural root init — called once per brain at init time. Idempotent.
 * Creates all 10 root nodes + cross-edges that make sense without runtime
 * state (e.g. emotion -> mirror-neurons "resonance" etc.).
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT int nimcp_wave10_affective_kg_init(struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 1. emotions: emit a dimensional state transition + query current bias.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_emotions_emit_state_change(
    struct brain_struct* brain,
    float valence, float arousal, float intensity);

/**
 * Query current emotion valence bias from KG (read-path: other modules
 * can bias decisions by emotion state). Returns intensity stored as
 * metadata on the `cog_emotions` root node, else 0.5f neutral.
 */
NIMCP_EXPORT float wave10_emotions_query_arousal_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 2. theory_of_mind: emit a belief/false-belief detection event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_tom_emit_belief_event(
    struct brain_struct* brain,
    uint32_t agent_id,
    const char* belief_kind,
    float confidence);

/** Returns 1.0f if ToM has recorded at least one agent model, else 0.5f. */
NIMCP_EXPORT float wave10_tom_query_model_count_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 3. mirror_neurons: emit an action-observation / mirror-activation event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_mirror_emit_activation(
    struct brain_struct* brain,
    uint32_t action_id,
    float activation);

/** Returns 1.0f if mirror neurons are actively observing, else 0.5f. */
NIMCP_EXPORT float wave10_mirror_query_activity_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 4. social_interaction: emit a per-episode convergence/trust event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_social_emit_episode_outcome(
    struct brain_struct* brain,
    uint32_t num_agents,
    float convergence,
    float reward);

/** Returns stored convergence metadata in [0,1], else 0.5f. */
NIMCP_EXPORT float wave10_social_query_convergence_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 5. collective_cognition: emit a collective-consciousness level transition.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_collective_emit_level_change(
    struct brain_struct* brain,
    int level,
    float phi);

/** Returns stored phi bias metadata, else 0.5f. */
NIMCP_EXPORT float wave10_collective_query_phi_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 6. personality: emit a trait-expression event (Big 5 snapshot).
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_personality_emit_trait_expression(
    struct brain_struct* brain,
    float openness, float conscientiousness, float extraversion,
    float agreeableness, float neuroticism);

/** Returns extraversion stored metadata, else 0.5f. */
NIMCP_EXPORT float wave10_personality_query_extraversion_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 7. empathetic_response: emit a response-strategy selection / effectiveness.
 *    The engine already has an `empathy_network` activated upstream; this
 *    layer adds KG-visibility to the generation + tracking hot paths.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_empathy_emit_response(
    struct brain_struct* brain,
    int strategy,
    bool crisis_detected,
    float predicted_safety);

NIMCP_EXPORT void wave10_empathy_emit_effectiveness(
    struct brain_struct* brain,
    float effectiveness_score);

/** Returns stored predicted_safety bias, else 0.5f. */
NIMCP_EXPORT float wave10_empathy_query_safety_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 8. emotion_recognition: emit a detected-other-emotion event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_emorec_emit_detection(
    struct brain_struct* brain,
    const char* emotion_label,
    float confidence);

/** Returns last-detection confidence bias, else 0.5f. */
NIMCP_EXPORT float wave10_emorec_query_detection_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 9. grief: emit a grief stage transition.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_grief_emit_stage_transition(
    struct brain_struct* brain,
    int from_stage, int to_stage,
    float pain_intensity);

/** Returns stored pain-intensity bias, else 0.5f. */
NIMCP_EXPORT float wave10_grief_query_pain_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 10. shadow_emotions: emit a shadow-pattern activation event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave10_shadow_emit_activation(
    struct brain_struct* brain,
    const char* shadow_name,
    float intensity);

/** Returns stored shadow intensity (max across shadows), else 0.5f. */
NIMCP_EXPORT float wave10_shadow_query_intensity_bias(
    struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WAVE10_AFFECTIVE_KG_H */
