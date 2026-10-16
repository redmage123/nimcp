// ============================================================================
// nimcp_emotion_recognition_simple.c - Simple Text Emotion Recognition
// ============================================================================
/**
 * @file nimcp_emotion_recognition_simple.c
 * @brief Lightweight text-based emotion recognition for integration
 *
 * WHAT: Simple keyword-based emotion detection from text
 * WHY:  Provide basic emotion recognition without full multimodal system
 * HOW:  Keyword matching + sentiment analysis
 *
 * NOTE: This is a lightweight implementation to enable empathetic response
 *       integration. Full multimodal emotion recognition (nimcp_emotion_recognition.h)
 *       can replace this when implemented.
 */

#include <string.h>
#include <math.h>
#include "cognitive/nimcp_emotion_recognition.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: emotion detection events */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <ctype.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(emotion_recognition_simple, MESH_ADAPTER_CATEGORY_COGNITIVE)


// ============================================================================
// Emotion Keywords
// ============================================================================

static const char* ANGER_KEYWORDS[] = {
    "angry", "mad", "furious", "annoyed", "frustrated", "irritated",
    "hate", "stupid", "damn", "ridiculous", "terrible", NULL
};

static const char* FEAR_KEYWORDS[] = {
    "afraid", "scared", "worried", "anxious", "nervous", "terrified",
    "panic", "frightened", "concerned", NULL
};

static const char* SADNESS_KEYWORDS[] = {
    "sad", "unhappy", "depressed", "miserable", "hopeless", "crying",
    "tears", "lonely", "hurt", "pain", NULL
};

static const char* HAPPINESS_KEYWORDS[] = {
    "happy", "glad", "excited", "great", "wonderful", "amazing",
    "awesome", "fantastic", "love", "joy", NULL
};

static const char* CONFUSION_KEYWORDS[] = {
    "confused", "don't understand", "what", "how", "unclear",
    "lost", "puzzled", NULL
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert text to lowercase for matching
 */
static void to_lowercase(char* dest, const char* src, size_t max_len)
{
    size_t i = 0;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dest[i] = tolower((unsigned char)src[i]);
    }
    dest[i] = '\0';
}

/**
 * @brief Count keywords in text
 */
static int count_keywords(const char* text, const char* keywords[])
{
    char lowercase_text[NIMCP_ERROR_BUFFER_LARGE];
    to_lowercase(lowercase_text, text, sizeof(lowercase_text));

    int count = 0;
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strstr(lowercase_text, keywords[i]) != NULL) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Simple text-based emotion recognition
 *
 * WHAT: Detect emotion from text using keyword matching
 * WHY:  Enable empathetic response without full emotion recognition system
 * HOW:  Count emotion keywords, select dominant emotion
 *
 * @param text Input text to analyze
 * @param emotion_name Output: Detected emotion name
 * @param emotion_name_len Size of emotion_name buffer
 * @param confidence Output: Confidence [0,1]
 * @param valence Output: Valence [-1,1] (negative to positive)
 * @param arousal Output: Arousal [0,1] (calm to excited)
 * @return true if emotion detected, false otherwise
 *
 * COMPLEXITY: O(n) where n = text length
 */
bool emotion_recognize_text_simple(
    const char* text,
    char* emotion_name,
    size_t emotion_name_len,
    float* confidence,
    float* valence,
    float* arousal)
{
    // Guard: NULL checks
    if (!text || !emotion_name || !confidence || !valence || !arousal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "emotion_recognize_text_simple: invalid parameters");

            return false;
    }

    // Initialize outputs
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_simple_heartbeat("emotion_reco_emotion_recognize_te", 0.0f);


    emotion_name[0] = '\0';
    *confidence = 0.0F;
    *valence = 0.0F;
    *arousal = 0.0F;

    // Guard: Empty text
    if (text[0] == '\0') {
        strncpy(emotion_name, "neutral", emotion_name_len - 1);
        *confidence = 0.5F;
        *valence = 0.0F;
        *arousal = 0.0F;
        return true;
    }

    // Count keywords per emotion
    int anger_count = count_keywords(text, ANGER_KEYWORDS);
    int fear_count = count_keywords(text, FEAR_KEYWORDS);
    int sadness_count = count_keywords(text, SADNESS_KEYWORDS);
    int happiness_count = count_keywords(text, HAPPINESS_KEYWORDS);
    int confusion_count = count_keywords(text, CONFUSION_KEYWORDS);

    // Select dominant emotion
    int max_count = 0;
    const char* detected_emotion = "neutral";
    float detected_valence = 0.0F;
    float detected_arousal = 0.3F;

    if (anger_count > max_count) {
        max_count = anger_count;
        detected_emotion = "anger";
        detected_valence = -0.7F;
        detected_arousal = 0.8F;
    }
    if (fear_count > max_count) {
        max_count = fear_count;
        detected_emotion = "fear";
        detected_valence = -0.6F;
        detected_arousal = 0.9F;
    }
    if (sadness_count > max_count) {
        max_count = sadness_count;
        detected_emotion = "sadness";
        detected_valence = -0.6F;
        detected_arousal = 0.2F;
    }
    if (happiness_count > max_count) {
        max_count = happiness_count;
        detected_emotion = "happiness";
        detected_valence = 0.8F;
        detected_arousal = 0.6F;
    }
    if (confusion_count > max_count) {
        max_count = confusion_count;
        detected_emotion = "confusion";
        detected_valence = -0.2F;
        detected_arousal = 0.5F;
    }

    // Calculate confidence based on keyword density
    size_t text_len = strlen(text);
    float keyword_density = (max_count > 0) ?
        ((float)max_count / (text_len / 20.0F)) : 0.0F;

    // Confidence: 0.5 base + up to 0.4 based on keyword density
    *confidence = (max_count > 0) ? (0.5F + (keyword_density * 0.4F)) : 0.3F;
    if (*confidence > 1.0F) *confidence = 1.0F;

    // Set outputs
    strncpy(emotion_name, detected_emotion, emotion_name_len - 1);
    emotion_name[emotion_name_len - 1] = '\0';
    *valence = detected_valence;
    *arousal = detected_arousal;

    return true;
}

/* ============================================================================
 * W10 (2026-04-24): KG runtime emit + read-path for emotion detection.
 *
 * Callers that hold a brain_t invoke this post-detect to register the
 * detection in brain->internal_kg and query the last-detection bias.
 * ============================================================================ */
float emotion_recognition_simple_wave10_kg_emit(
    struct brain_struct* brain,
    const char* emotion_label,
    float confidence)
{
    if (!brain) return 0.5f;
    wave10_emorec_emit_detection(brain, emotion_label, confidence);
    return wave10_emorec_query_detection_bias(brain);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_recognition_simple_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_simple_heartbeat("emotion_reco_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Recognition_Simple");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_recognition_simple_heartbeat("emotion_reco_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Recognition_Simple");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Recognition_Simple");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Stubs
 * ============================================================================ */


void emotion_recognition_simple_set_instance_health_agent(nimcp_health_agent_t* agent) {
    g_emotion_recognition_simple_instance_health_agent = agent;
}

int emotion_recognition_simple_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_simple_training_begin: ctx is NULL");
        return -1;
    }
    emotion_recognition_simple_heartbeat_instance(g_emotion_recognition_simple_instance_health_agent, "erec_simple_training_begin", 0.0f);
    return 0;
}

int emotion_recognition_simple_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_simple_training_end: ctx is NULL");
        return -1;
    }
    emotion_recognition_simple_heartbeat_instance(g_emotion_recognition_simple_instance_health_agent, "erec_simple_training_end", 1.0f);
    return 0;
}

int emotion_recognition_simple_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_simple_training_step: ctx is NULL");
        return -1;
    }
    emotion_recognition_simple_heartbeat_instance(g_emotion_recognition_simple_instance_health_agent, "erec_simple_training_step", progress);
    return 0;
}
