/**
 * @file nimcp_emotional_tagging.c
 * @brief Implementation of emotional tagging system
 *
 * WHAT: Tags cognitive representations with emotional states
 * WHY:  Emotions enhance memory and guide attention
 * HOW:  Russell's circumplex model (valence × arousal)
 *
 * CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (no nested ifs)
 * - Single Responsibility Principle
 * - WHAT-WHY-HOW documentation
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

#define LOG_MODULE "EMOTIONAL_TAGGING"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(emotional_tagging, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_EMOTIONAL_TAGGING 0x0326
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "core/brain/nimcp_brain.h"  // Brain reference
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

//=============================================================================
// Helper Functions (Internal, <50 lines each)
//=============================================================================

/**
 * @brief Compute absolute value
 *
 * WHAT: Get magnitude without sign
 * WHY:  Valence intensity (positive or negative both matter)
 * HOW:  Standard fabs()
 */
static inline float abs_float(float value)
{
    return fabsf(value);
}

/**
 * @brief Check if emotion is high arousal
 *
 * WHAT: Test if arousal exceeds threshold
 * WHY:  Classify emotions into arousal categories
 * HOW:  Compare to 0.5 threshold
 */
static inline bool is_high_arousal(float arousal)
{
    return arousal > 0.5f;
}

/**
 * @brief Check if emotion is positive valence
 *
 * WHAT: Test if valence is positive
 * WHY:  Classify emotions into valence categories
 * HOW:  Compare to threshold
 */
static inline bool is_positive_valence(float valence)
{
    return valence > 0.2f;
}

/**
 * @brief Check if emotion is negative valence
 *
 * WHAT: Test if valence is negative
 * WHY:  Classify emotions into valence categories
 * HOW:  Compare to threshold
 */
static inline bool is_negative_valence(float valence)
{
    return valence < -0.2f;
}

/**
 * @brief Classify high-arousal positive emotions
 *
 * WHAT: Determine if JOY or EXCITEMENT
 * WHY:  Distinguish between high-arousal positive states
 * HOW:  Threshold on valence strength
 */
static emotion_category_t classify_high_arousal_positive(float valence, float arousal)
{
    // Guard: Very high valence → JOY
    if (valence > 0.5f && arousal > 0.5f) {
        return EMOTION_JOY;
    }

    // High arousal, moderate positive → EXCITEMENT
    return EMOTION_EXCITEMENT;
}

/**
 * @brief Classify high-arousal negative emotions
 *
 * WHAT: Determine if FEAR, ANGER, or ANXIETY
 * WHY:  Distinguish between high-arousal negative states
 * HOW:  Threshold on valence and arousal strength
 */
static emotion_category_t classify_high_arousal_negative(float valence, float arousal)
{
    // Guard: Very negative + very high arousal → FEAR
    if (valence < -0.5f && arousal > 0.7f) {
        return EMOTION_FEAR;
    }

    // Guard: Moderately negative + high arousal → ANGER
    if (valence < -0.4f) {
        return EMOTION_ANGER;
    }

    // Default: ANXIETY (negative + aroused but not extreme)
    return EMOTION_ANXIETY;
}

/**
 * @brief Classify low-arousal emotions
 *
 * WHAT: Determine if CALM, SADNESS, BOREDOM, or NEUTRAL
 * WHY:  Distinguish between low-arousal states
 * HOW:  Threshold on valence
 */
static emotion_category_t classify_low_arousal(float valence, float arousal)
{
    // Guard: Positive + low arousal → CALM
    if (valence > 0.2f) {
        return EMOTION_CALM;
    }

    // Guard: Negative + very low arousal → BOREDOM
    if (valence < -0.1f && arousal < 0.2f) {
        return EMOTION_BOREDOM;
    }

    // Guard: Moderately negative + low arousal → SADNESS
    if (valence < -0.3f) {
        return EMOTION_SADNESS;
    }

    // Default: NEUTRAL (near origin)
    return EMOTION_NEUTRAL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Create emotional tag from valence and arousal
 *
 * WHAT: Initialize emotional tag structure
 * WHY:  Primary constructor for emotional states
 * HOW:  Clamp values → classify → compute intensity
 */
emotional_tag_t emotional_tag_create(
    float valence,
    float arousal,
    uint64_t timestamp_ms)
{
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_create", 0.0f);


    emotional_tag_t tag;

    // Clamp to valid ranges
    tag.valence = nimcp_clampf(valence, -1.0f, 1.0f);
    tag.arousal = nimcp_clampf(arousal, 0.0f, 1.0f);
    tag.timestamp_ms = timestamp_ms;

    // Classify emotion category
    tag.category = emotional_tag_classify(&tag);

    // Compute intensity
    tag.intensity = emotional_tag_intensity(&tag);

    return tag;
}

/**
 * @brief Create neutral emotional tag
 *
 * WHAT: Initialize with zero emotion
 * WHY:  Convenience for neutral states
 * HOW:  Set all fields to neutral/zero
 */
emotional_tag_t emotional_tag_neutral(void)
{
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_neutra", 0.0f);


    emotional_tag_t tag = {
        .valence = 0.0f,
        .arousal = 0.0f,
        .timestamp_ms = 0,
        .category = EMOTION_CAT_NEUTRAL,
        .intensity = 0.0f
    };
    return tag;
}

/**
 * @brief Classify emotion into discrete category
 *
 * WHAT: Map 2D coordinates to emotion category
 * WHY:  Provide human-readable emotional state
 * HOW:  Hierarchical classification with guard clauses
 *
 * COMPLEXITY: O(1) - ~10 comparisons maximum
 */
emotion_category_t emotional_tag_classify(const emotional_tag_t* tag)
{
    // Guard: NULL tag
    if (!tag) {
        return EMOTION_NEUTRAL;
    }

    // Extract values
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_classi", 0.0f);


    float valence = tag->valence;
    float arousal = tag->arousal;

    // High arousal emotions
    if (is_high_arousal(arousal)) {
        if (is_positive_valence(valence)) {
            return classify_high_arousal_positive(valence, arousal);
        }
        if (is_negative_valence(valence)) {
            return classify_high_arousal_negative(valence, arousal);
        }
    }

    // Low arousal emotions
    return classify_low_arousal(valence, arousal);
}

/**
 * @brief Compute emotional intensity
 *
 * WHAT: Vector magnitude in valence-arousal space
 * WHY:  Single scalar for emotional strength
 * HOW:  Euclidean distance from origin, normalized
 *
 * FORMULA: sqrt(valence² + arousal²) / sqrt(2)
 * RANGE: [0.0, 1.0]
 */
float emotional_tag_intensity(const emotional_tag_t* tag)
{
    // Guard: NULL tag
    if (!tag) {
        return 0.0f;
    }

    // Compute magnitude
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_intens", 0.0f);


    float val_sq = tag->valence * tag->valence;
    float aro_sq = tag->arousal * tag->arousal;
    float magnitude = sqrtf(val_sq + aro_sq);

    // Normalize to [0, 1] (max possible magnitude is sqrt(2))
    return magnitude / NIMCP_SQRT2_F;  // sqrt(2)
}

/**
 * @brief Get emotion category name
 *
 * WHAT: Convert enum to string
 * WHY:  Debugging and logging
 * HOW:  Static string array lookup
 */
const char* emotional_category_name(emotion_category_t category)
{
    static const char* names[] = {
        "NEUTRAL",
        "JOY",
        "EXCITEMENT",
        "CALM",
        "FEAR",
        "ANGER",
        "SADNESS",
        "ANXIETY",
        "BOREDOM"
    };

    // Guard: Invalid category
    if (category < 0 || category >= 9) {
        return "UNKNOWN";
    }

    return names[category];
}

/**
 * @brief Compute salience boost from emotion
 *
 * WHAT: Multiplier for base salience
 * WHY:  Emotional events grab attention
 * HOW:  Arousal and valence intensity both contribute
 *
 * FORMULA: 1.0 + (arousal × 0.5) + (|valence| × 0.3)
 * RANGE: [1.0, 1.8]
 */
float emotional_compute_salience_boost(const emotional_tag_t* tag)
{
    // Guard: NULL tag → no boost
    if (!tag) {
        return 1.0f;
    }

    // Arousal contribution (up to +0.5)
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_compute_sa", 0.0f);


    float arousal_boost = tag->arousal * EMOTIONAL_AROUSAL_SALIENCE_FACTOR;

    // Valence intensity contribution (up to +0.3)
    float valence_boost = abs_float(tag->valence) * EMOTIONAL_VALENCE_SALIENCE_FACTOR;

    // Total boost (baseline 1.0 + contributions)
    return 1.0f + arousal_boost + valence_boost;
}

/**
 * @brief Apply emotional boost to salience
 *
 * WHAT: Compute final salience with emotion
 * WHY:  Convenience function
 * HOW:  Multiply base × boost
 */
float emotional_apply_salience_boost(
    float base_salience,
    const emotional_tag_t* tag)
{
    // Guard: Invalid base salience
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_apply_sali", 0.0f);


    if (base_salience < 0.0f) {
        return 0.0f;
    }

    float boost = emotional_compute_salience_boost(tag);
    return base_salience * boost;
}

/**
 * @brief Detect emotion from cognitive state
 *
 * WHAT: Infer emotional tag from processing outputs
 * WHY:  Automatic emotion tagging during inference
 * HOW:  Map cognitive signals to valence-arousal space
 *
 * MAPPINGS:
 * - confidence → valence
 * - uncertainty → arousal
 * - novelty → positive valence + arousal
 * - ethical violation → negative valence
 */
emotional_tag_t emotional_tag_from_cognitive_state(
    float confidence,
    float uncertainty,
    float novelty,
    bool ethical_approved,
    uint64_t timestamp_ms)
{
    // Initialize with neutral
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_from_c", 0.0f);


    float valence = 0.0f;
    float arousal = 0.0f;

    // Map confidence to valence: high confidence = positive
    // confidence ∈ [0,1] → valence ∈ [-1,+1]
    valence = (confidence - 0.5f) * 2.0f;

    // Map uncertainty to arousal: high uncertainty = high arousal
    arousal = nimcp_clampf(uncertainty * 1.2f, 0.0f, 1.0f);

    // Novelty adds positive valence and arousal (curiosity)
    if (novelty > 0.5f) {
        valence += (novelty - 0.5f) * 0.6f;  // Boost positive valence
        arousal += (novelty - 0.5f) * 0.4f;   // Boost arousal
    }

    // Ethical violation → strong negative valence + high arousal
    if (!ethical_approved) {
        valence = -0.8f;  // Strong negative
        arousal = nimcp_clampf(arousal + 0.3f, 0.0f, 1.0f);  // Increase arousal
    }

    // Create tag with computed values
    return emotional_tag_create(valence, arousal, timestamp_ms);
}

/**
 * @brief Apply neuromodulator adjustments to valence and arousal
 *
 * WHAT: Modulate emotional state by dopamine and serotonin levels
 * WHY:  Neurotransmitters affect mood, arousal, and valence
 * HOW:  Read DA/5-HT, adjust valence/arousal accordingly
 *
 * BIOLOGY:
 * - Dopamine: Reward/motivation → positive valence
 * - Serotonin: Mood stability → balanced valence, lower arousal
 *
 * COMPLEXITY: O(1)
 *
 * @param valence Input valence (modified in-place)
 * @param arousal Input arousal (modified in-place)
 * @param brain Brain to read neurotransmitters from
 */
static void apply_neuromodulator_emotion_modulation(float* valence,
                                                    float* arousal,
                                                    brain_t brain)
{
    // Guard: Early return if no brain
    if (!brain || !valence || !arousal) {
        return;
    }

    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    if (!neuromod) {
        return;
    }

    // Read neurotransmitter levels
    float da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
    float serotonin = neuromodulator_get_level(neuromod, NEUROMOD_SEROTONIN);

    // Dopamine modulates valence: High DA → more positive
    // DA range [0.3, 0.7] → valence shift [-0.2, +0.2]
    float da_shift = (da - 0.5f) * 0.4f;
    *valence += da_shift;

    // Serotonin modulates arousal: High 5-HT → lower arousal
    // 5-HT range [0.3, 0.7] → arousal multiplier [1.2, 0.8]
    float serotonin_mult = 1.2f - (serotonin - 0.3f);
    *arousal *= serotonin_mult;

    // Clamp to valid ranges
    *valence = nimcp_clampf(*valence, -1.0f, 1.0f);
    *arousal = nimcp_clampf(*arousal, 0.0f, 1.0f);
}

/**
 * @brief Create emotional tag with neuromodulator integration
 *
 * WHAT: Convert cognitive state to emotion with chemical modulation
 * WHY:  Model how neurotransmitters affect emotional experience
 * HOW:  Compute base emotion, then modulate by DA/5-HT levels
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain to read neurotransmitters from
 * @param confidence Decision confidence [0, 1]
 * @param uncertainty Decision uncertainty [0, 1]
 * @param novelty Input novelty [0, 1]
 * @param ethical_approved Whether action is ethically approved
 * @param timestamp_ms Timestamp
 * @return Emotional tag with neuromodulator modulation
 */
emotional_tag_t emotional_tag_from_brain_state(
    brain_t brain,
    float confidence,
    float uncertainty,
    float novelty,
    bool ethical_approved,
    uint64_t timestamp_ms)
{
    // Compute base emotional state
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_from_b", 0.0f);


    float valence = (confidence - 0.5f) * 2.0f;
    float arousal = nimcp_clampf(uncertainty * 1.2f, 0.0f, 1.0f);

    // Novelty boosts
    if (novelty > 0.5f) {
        valence += (novelty - 0.5f) * 0.6f;
        arousal += (novelty - 0.5f) * 0.4f;
    }

    // Ethical violation
    if (!ethical_approved) {
        valence = -0.8f;
        arousal = nimcp_clampf(arousal + 0.3f, 0.0f, 1.0f);
    }

    // Apply neuromodulator modulation
    apply_neuromodulator_emotion_modulation(&valence, &arousal, brain);

    // Create tag
    return emotional_tag_create(valence, arousal, timestamp_ms);
}

/**
 * @brief Validate emotional tag
 *
 * WHAT: Check if values are in valid ranges
 * WHY:  Prevent invalid emotional states
 * HOW:  Range checks
 */
bool emotional_tag_is_valid(const emotional_tag_t* tag)
{
    // Guard: NULL tag
    if (!tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "emotional_tag_is_valid: tag is NULL");

            return false;
    }

    // Check ranges
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_is_val", 0.0f);


    if (tag->valence < -1.0f || tag->valence > 1.0f) {
        return false;
    }

    if (tag->arousal < 0.0f || tag->arousal > 1.0f) {
        return false;
    }

    if (tag->intensity < 0.0f || tag->intensity > 1.0f) {
        return false;
    }

    return true;
}

/**
 * @brief Clamp emotional tag to valid ranges
 *
 * WHAT: Force values into valid bounds
 * WHY:  Sanitize potentially invalid states
 * HOW:  Clamp each field
 */
void emotional_tag_clamp(emotional_tag_t* tag)
{
    // Guard: NULL tag
    if (!tag) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_tag_clamp", 0.0f);


    tag->valence = nimcp_clampf(tag->valence, -1.0f, 1.0f);
    tag->arousal = nimcp_clampf(tag->arousal, 0.0f, 1.0f);
    tag->intensity = nimcp_clampf(tag->intensity, 0.0f, 1.0f);

    // Recompute category and intensity after clamping
    tag->category = emotional_tag_classify(tag);
    tag->intensity = emotional_tag_intensity(tag);
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get emotional valence from tag
 *
 * WHAT: Query emotional positivity/negativity
 * WHY:  Salience can bias attention based on mood
 * HOW:  Return valence from emotional tag
 *
 * BIOLOGY: Mood-congruent perception bias
 *          Negative mood → attention drawn to negative cues
 *          Positive mood → attention drawn to positive cues
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag
 * @return Valence [-1, +1] (negative to positive)
 */
float emotional_get_valence(const emotional_tag_t* tag)
{
    // Guard: NULL tag
    if (!tag) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_get_valenc", 0.0f);


    return tag->valence;
}

/**
 * @brief Get emotional arousal from tag
 *
 * WHAT: Query emotional activation level
 * WHY:  Salience boosted by high arousal
 * HOW:  Return arousal from emotional tag
 *
 * BIOLOGY: High arousal increases attention and salience sensitivity
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag
 * @return Arousal [0, 1] (calm to excited)
 */
float emotional_get_arousal(const emotional_tag_t* tag)
{
    // Guard: NULL tag
    if (!tag) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_get_arousa", 0.0f);


    return tag->arousal;
}

/**
 * @brief Modulate arousal in emotional tag
 *
 * WHAT: Allow salience to influence emotional arousal
 * WHY:  Surprising/salient events increase arousal
 * HOW:  Add delta to current arousal, clamped to [0, 1]
 *
 * BIOLOGY: Unexpected events (prediction errors) increase arousal via LC-NE system
 *
 * COMPLEXITY: O(1)
 *
 * @param tag Emotional tag to modulate (modified in place)
 * @param arousal_delta Arousal change [-1, +1]
 */
void emotional_modulate_arousal(emotional_tag_t* tag, float arousal_delta)
{
    // Guard: NULL tag
    if (!tag) {
        return;
    }

    // WHAT: Add delta to arousal
    // WHY:  Salience can increase or decrease arousal
    // HOW:  Clamp result to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_emotional_modulate_a", 0.0f);


    tag->arousal += arousal_delta;
    tag->arousal = nimcp_clampf(tag->arousal, 0.0f, 1.0f);

    // WHAT: Recompute derived fields
    // WHY:  Arousal change may change category and intensity
    // HOW:  Re-classify and recompute intensity
    tag->category = emotional_tag_classify(tag);
    tag->intensity = emotional_tag_intensity(tag);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_tagging_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_heartbeat("emotional_ta_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_Tagging");
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

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_Tagging");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_Tagging");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Stubs
 * ============================================================================ */


void emotional_tagging_set_instance_health_agent(nimcp_health_agent_t* agent) {
    g_emotional_tagging_instance_health_agent = agent;
}

int emotional_tagging_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_training_begin: ctx is NULL");
        return -1;
    }
    emotional_tagging_heartbeat_instance(g_emotional_tagging_instance_health_agent, "etag_training_begin", 0.0f);
    return 0;
}

int emotional_tagging_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_training_end: ctx is NULL");
        return -1;
    }
    emotional_tagging_heartbeat_instance(g_emotional_tagging_instance_health_agent, "etag_training_end", 1.0f);
    return 0;
}

int emotional_tagging_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_training_step: ctx is NULL");
        return -1;
    }
    emotional_tagging_heartbeat_instance(g_emotional_tagging_instance_health_agent, "etag_training_step", progress);
    return 0;
}
