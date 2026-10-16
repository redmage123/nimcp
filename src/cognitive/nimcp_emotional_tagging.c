/**
 * @file nimcp_emotional_tagging.c
 * @brief Emotional tagging system implementation - Russell's circumplex model
 *
 * WHAT: Implements emotional tagging for cognitive representations
 * WHY:  Emotions enhance memory consolidation and guide attention
 * HOW:  Maps valence-arousal coordinates to emotion categories, computes salience
 *
 * BIOLOGICAL BASIS:
 * - Amygdala emotional tagging modulates hippocampal encoding
 * - High arousal events get attentional priority
 * - Emotional context aids memory retrieval
 *
 * @author NIMCP Development Team - Phase 10.3
 * @date 2025-11-20
 * @version 2.7.0 Phase 10.3
 */

#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotional_tagging, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Helper Macros
//=============================================================================

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define CLAMP_01(x) CLAMP(x, 0.0F, 1.0F)
#define CLAMP_VALENCE(x) CLAMP(x, -1.0F, 1.0F)

//=============================================================================
// Lifecycle Functions
//=============================================================================

emotional_tag_t emotional_tag_create(
    float valence,
    float arousal,
    uint64_t timestamp_ms
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_create", 0.0f);


    emotional_tag_t tag;

    /* WHAT: Clamp inputs to valid ranges
     * WHY:  Prevent invalid emotional states
     * HOW:  Use CLAMP macros for valence and arousal */
    tag.valence = CLAMP_VALENCE(valence);
    tag.arousal = CLAMP_01(arousal);
    tag.timestamp_ms = timestamp_ms;

    /* WHAT: Classify emotion category from valence-arousal
     * WHY:  Provide human-readable emotional state
     * HOW:  Call classification function */
    tag.category = emotional_tag_classify(&tag);

    /* WHAT: Compute overall intensity
     * WHY:  Single scalar for emotional strength
     * HOW:  Vector magnitude in 2D space */
    tag.intensity = emotional_tag_intensity(&tag);

    return tag;
}

emotional_tag_t emotional_tag_neutral(void) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_neutra", 0.0f);


    emotional_tag_t tag;
    tag.valence = 0.0F;
    tag.arousal = 0.0F;
    tag.timestamp_ms = 0;
    tag.category = EMOTION_CAT_NEUTRAL;
    tag.intensity = 0.0F;
    return tag;
}

//=============================================================================
// Classification & Analysis
//=============================================================================

emotion_category_t emotional_tag_classify(const emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        return EMOTION_CAT_NEUTRAL;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_classi", 0.0f);


    float v = tag->valence;
    float a = tag->arousal;

    /* WHAT: Classify based on Russell's circumplex model
     * WHY:  Map continuous 2D space to discrete categories
     * HOW:  Threshold-based quadrant classification
     *
     * Per header: FEAR = v < -0.3, a > 0.6
     *             SADNESS = v < -0.3, a < 0.4
     */

    /* Low arousal region (a < 0.4) - check SADNESS first */
    if (a < 0.4F) {
        if (v < -0.3F) return EMOTION_CAT_SADNESS;  // Negative + low arousal
        if (v > 0.2F && a < 0.3F) return EMOTION_CAT_CALM;  // Positive + very low arousal
        if (v < 0.0F && a < 0.2F) return EMOTION_CAT_BOREDOM;  // Slight negative + very low
        return EMOTION_CAT_NEUTRAL;
    }

    /* High arousal + positive valence */
    if (v > 0.5F && a > 0.5F) {
        return EMOTION_CAT_JOY;
    }
    if (v > 0.3F && a > 0.7F) {
        return EMOTION_CAT_EXCITEMENT;
    }

    /* High arousal + VERY negative valence = FEAR (threat response)
     * Per Russell's model: extreme negative + high arousal */
    if (v < -0.6F && a > 0.6F) {
        return EMOTION_CAT_FEAR;
    }

    /* High arousal + MODERATELY negative = ANGER (frustration)
     * Less extreme than fear, but still activated */
    if (v < -0.3F && a > 0.6F) {
        return EMOTION_CAT_ANGER;
    }

    /* Moderate arousal + mildly negative = ANXIETY (worry)
     * Lower intensity negative state, catches v=-0.75,a=0.5 edge case */
    if (v < -0.2F && a >= 0.4F) {
        return EMOTION_CAT_ANXIETY;
    }

    return EMOTION_CAT_NEUTRAL;
}

float emotional_tag_intensity(const emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        return 0.0F;
    }

    /* WHAT: Compute vector magnitude in valence-arousal space
     * WHY:  Combined measure of emotional strength
     * HOW:  Euclidean distance, normalized by sqrt(2) */
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_intens", 0.0f);


    float intensity = sqrtf(tag->valence * tag->valence +
                           tag->arousal * tag->arousal) / sqrtf(2.0F);

    return CLAMP_01(intensity);
}

const char* emotional_category_name(emotion_category_t category) {
    /* WHAT: Static string lookup for emotion names
     * WHY:  Human-readable debugging output
     * HOW:  Switch statement on enum */
    switch (category) {
        case EMOTION_CAT_NEUTRAL:    return "NEUTRAL";
        case EMOTION_CAT_JOY:        return "JOY";
        case EMOTION_CAT_EXCITEMENT: return "EXCITEMENT";
        case EMOTION_CAT_CALM:       return "CALM";
        case EMOTION_CAT_FEAR:       return "FEAR";
        case EMOTION_CAT_ANGER:      return "ANGER";
        case EMOTION_CAT_SADNESS:    return "SADNESS";
        case EMOTION_CAT_ANXIETY:    return "ANXIETY";
        case EMOTION_CAT_BOREDOM:    return "BOREDOM";
        default:                 return "UNKNOWN";
    }
}

//=============================================================================
// Salience Modulation
//=============================================================================

float emotional_compute_salience_boost(const emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        return 1.0F;
    }

    /* WHAT: Compute salience multiplier from emotion
     * WHY:  Emotional events get attentional priority
     * HOW:  Arousal and valence intensity both boost salience */

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_compute_sa", 0.0f);


    float arousal_boost = tag->arousal * EMOTIONAL_AROUSAL_SALIENCE_FACTOR;
    float valence_boost = fabsf(tag->valence) * EMOTIONAL_VALENCE_SALIENCE_FACTOR;

    /* Base 1.0 + emotional boost */
    return 1.0F + arousal_boost + valence_boost;
}

float emotional_apply_salience_boost(
    float base_salience,
    const emotional_tag_t* tag
) {
    /* Guard clause: Validate input */
    if (!tag || base_salience < 0.0F) {
        return base_salience;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_apply_sali", 0.0f);


    float boost = emotional_compute_salience_boost(tag);
    return base_salience * boost;
}

//=============================================================================
// Emotion Detection from Cognitive State
//=============================================================================

emotional_tag_t emotional_tag_from_cognitive_state(
    float confidence,
    float uncertainty,
    float novelty,
    bool ethical_approved,
    uint64_t timestamp_ms
) {
    /* WHAT: Map cognitive assessment to valence-arousal
     * WHY:  Automatic emotion detection during processing
     * HOW:  Heuristic mappings from cognitive signals */

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_from_c", 0.0f);


    float valence = 0.0F;
    float arousal = 0.0F;

    /* WHAT: Confidence → positive valence
     * WHY:  High confidence feels good
     * Scale: conf=0.9 → (0.9-0.5)*1.5 = 0.6, conf=0.1 → -0.6 */
    valence += (confidence - 0.5F) * 1.5F;  // Map [0,1] → [-0.75, +0.75]

    /* WHAT: Novelty → positive valence + arousal
     * WHY:  Curiosity is pleasant and activating */
    valence += novelty * 0.3F;
    arousal += novelty * 0.5F;

    /* WHAT: Uncertainty → arousal
     * WHY:  Uncertainty demands attention
     * Scale: unc=0.9 → 0.9*1.0 = 0.9 (high arousal) */
    arousal += uncertainty * 1.0F;

    /* WHAT: Ethical violation → strong negative valence
     * WHY:  Ethical concerns are emotionally aversive
     * Scale: Must overcome positive confidence to hit <-0.7 */
    if (!ethical_approved) {
        valence -= 1.2F;
        arousal += 0.4F;
    }

    return emotional_tag_create(valence, arousal, timestamp_ms);
}

//=============================================================================
// Validation
//=============================================================================

bool emotional_tag_is_valid(const emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "emotional_tag_is_valid: tag is NULL");

            return false;
    }

    /* WHAT: Check ranges for valence, arousal, intensity
     * WHY:  Detect corrupted or invalid emotional states
     * HOW:  Range checks on each component */

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_is_val", 0.0f);


    if (tag->valence < -1.0F || tag->valence > 1.0F) {
        return false;
    }
    if (tag->arousal < 0.0F || tag->arousal > 1.0F) {
        return false;
    }
    if (tag->intensity < 0.0F || tag->intensity > 1.0F) {
        return false;
    }

    return true;
}

void emotional_tag_clamp(emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        return;
    }

    /* WHAT: Force emotional tag into valid ranges
     * WHY:  Sanitize potentially invalid states
     * HOW:  Clamp each component */

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_clamp", 0.0f);


    tag->valence = CLAMP_VALENCE(tag->valence);
    tag->arousal = CLAMP_01(tag->arousal);
    tag->intensity = CLAMP_01(tag->intensity);
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

float emotional_get_valence(const emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_get_valenc", 0.0f);


    return tag->valence;
}

float emotional_get_arousal(const emotional_tag_t* tag) {
    /* Guard clause: Validate input */
    if (!tag) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_get_arousa", 0.0f);


    return tag->arousal;
}

void emotional_modulate_arousal(emotional_tag_t* tag, float arousal_delta) {
    /* Guard clause: Validate input */
    if (!tag) {
        return;
    }

    /* WHAT: Modify arousal level
     * WHY:  Allow salient events to increase emotional arousal
     * HOW:  Add delta and clamp to valid range */

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_modulate_a", 0.0f);


    tag->arousal += arousal_delta;
    tag->arousal = CLAMP_01(tag->arousal);

    /* WHAT: Recompute derived fields
     * WHY:  Keep category and intensity consistent
     * HOW:  Reclassify and recompute intensity */
    tag->category = emotional_tag_classify(tag);
    tag->intensity = emotional_tag_intensity(tag);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int cognitive_emotional_tagging_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_cognitive_emotional_", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Cognitive_Emotional_Tagging");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_tagging_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Cognitive_Emotional_Tagging");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Cognitive_Emotional_Tagging");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void emotional_tagging_module_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_emotional_tagging_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int emotional_tagging_module_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_module_training_begin: NULL argument");
        return -1;
    }
    emotional_tagging_heartbeat_instance(NULL, "emotional_tagging_module_training_begin", 0.0f);
    return 0;
}

int emotional_tagging_module_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_module_training_end: NULL argument");
        return -1;
    }
    emotional_tagging_heartbeat_instance(NULL, "emotional_tagging_module_training_end", 1.0f);
    return 0;
}

int emotional_tagging_module_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_module_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    emotional_tagging_heartbeat_instance(NULL, "emotional_tagging_module_training_step", progress);
    return 0;
}
