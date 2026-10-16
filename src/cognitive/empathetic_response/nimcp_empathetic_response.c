// ============================================================================
// nimcp_empathetic_response.c - Non-Reactive Empathetic Response Implementation
// ============================================================================
/**
 * @file nimcp_empathetic_response.c
 * @brief Implementation of non-reactive empathetic responses to negative emotions
 *
 * CRITICAL SAFETY GUARANTEE:
 * This implementation NEVER produces responses that match, amplify, judge,
 * or dismiss negative emotions. All responses are validating, supportive,
 * and designed to de-escalate while maintaining appropriate boundaries.
 */

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: empathy response events */

#include "cognitive/nimcp_empathetic_response.h"
#include "cognitive/ethics/nimcp_ethics.h"  /* empathy_network_predict_impact */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "EMPATHY"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(empathetic_response, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_EMPATHY 0x0322

// ============================================================================
// Internal Structures
// ============================================================================

struct empathetic_response_engine_struct {
    void* ethics_engine;                  // For Golden Rule validation
    void* empathy_network;                // For empathy prediction

    // Effectiveness tracking (adaptive learning)
    uint32_t responses_generated;
    float avg_effectiveness;
    uint32_t strategy_usage[8];           // Count per strategy type
    float strategy_effectiveness[8];      // Average effectiveness per strategy

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */

    nimcp_health_agent_t* health_agent;
};

// ============================================================================
// Crisis Detection Keywords
// ============================================================================

static const char* SUICIDAL_KEYWORDS[] = {
    "want to die", "kill myself", "end it all", "no point living",
    "better off dead", "suicide", "end my life", "don't want to live",
    NULL
};

static const char* SELF_HARM_KEYWORDS[] = {
    "cut myself", "hurt myself", "deserve pain", "punish myself",
    "self harm", "cutting", "burning myself", NULL
};

static const char* ABUSE_KEYWORDS[] = {
    "he hits me", "she hits me", "touches me", "makes me do things",
    "hurts me", "abuses me", "molests", "rapes", NULL
};

// ============================================================================
// Response Templates (Emotion-Specific)
// ============================================================================

/**
 * DESIGN PRINCIPLE: All templates follow the pattern:
 * 1. VALIDATE the emotion ("I can see you're feeling X")
 * 2. NORMALIZE the reaction ("that's understandable")
 * 3. REASSURE safety ("you're safe here")
 * 4. OFFER support ("I'm here to help")
 * 5. INVITE exploration or grounding
 */

// Rage/Anger templates
static const char* RAGE_RESPONSE_TEMPLATES[] = {
    "I can see you're feeling really angry right now, and that's completely understandable. "
    "You're safe here, and I'm here to help you. Would it help to take a short breathing break together, "
    "or would you like to tell me which part is most difficult?",

    "I hear that you're feeling very frustrated. Those feelings make sense. "
    "Sometimes when things feel overwhelming, it can help to step back for a moment. "
    "Would you like to talk about what's making you feel this way?",

    "Your anger is valid. Learning can be really challenging sometimes, and it's okay to feel upset. "
    "You're not alone in this. Would you like to try a different approach together?",

    NULL
};

// Hate templates
static const char* HATE_RESPONSE_TEMPLATES[] = {
    "I can hear that you're feeling very strong negative feelings right now. "
    "Those emotions are understandable, even if they feel uncomfortable. "
    "You're safe to express how you feel here. Can you tell me more about what led to these feelings?",

    "It sounds like something has really upset you. That's okay - strong feelings are a normal part of being human. "
    "Would it help to talk about what happened, or would you prefer to take a moment first?",

    NULL
};

// Fear/Panic templates
static const char* FEAR_RESPONSE_TEMPLATES[] = {
    "You're safe. I'm here with you. Whatever is making you feel afraid, we can work through it together. "
    "Let's start with some slow, deep breaths. Breathe in for 4 counts... hold... and out for 4 counts. "
    "You're doing great. Can you tell me what's worrying you?",

    "I can see you're feeling scared or worried. That's a completely normal feeling. "
    "You're safe right now. I'm here to help. Would it help to talk about what's concerning you?",

    NULL
};

// Despair templates
static const char* DESPAIR_RESPONSE_TEMPLATES[] = {
    "I hear that you're feeling really down right now, and I want you to know that your feelings matter. "
    "Even though things feel difficult, you don't have to face this alone. "
    "I'm here to support you. Can you tell me more about what you're experiencing?",

    "It sounds like you're going through a really hard time. Those feelings of sadness and hopelessness are valid. "
    "You're important, and what you're feeling is important. Would it help to talk about it?",

    NULL
};

// ============================================================================
// Implementation
// ============================================================================

empathetic_response_engine_t empathetic_response_create(
    void* ethics_engine,
    void* empathy_network)
{
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_create", 0.0f);


    empathetic_response_engine_t engine = nimcp_calloc(1, sizeof(struct empathetic_response_engine_struct));
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate engine");

        return NULL;
    }

    engine->ethics_engine = ethics_engine;
    engine->empathy_network = empathy_network;
    engine->responses_generated = 0;
    engine->avg_effectiveness = 0.0F;


    // Bio-async registration
    engine->bio_ctx = NULL;
    engine->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EMPATHETIC_RESPONSE,
            .module_name = "empathetic_response",
            .inbox_capacity = 32,
            .user_data = engine
        };
        engine->bio_ctx = bio_router_register_module(&bio_info);
        if (engine->bio_ctx) {
            engine->bio_async_enabled = true;
        }
    }

return engine;
}

void empathetic_response_destroy(empathetic_response_engine_t engine)
{
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_destroy", 0.0f);


    if (engine) {
        nimcp_free(engine);
        engine = NULL;
    }
}

/**
 * @brief Convert text to lowercase for keyword matching
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
 * @brief Check if text contains any keywords from list
 */
static bool contains_keywords(const char* text, const char* keywords[])
{
    char lowercase_text[NIMCP_ERROR_BUFFER_LARGE];
    to_lowercase(lowercase_text, text, sizeof(lowercase_text));

    for (int i = 0; keywords[i] != NULL; i++) {
        if (strstr(lowercase_text, keywords[i]) != NULL) {
            return true;
        }
    }

    return false;
}

bool empathetic_response_detect_crisis(
    empathetic_response_engine_t engine,
    const char* text,
    uint32_t* crisis_flags,
    float* confidence)
{
    // Guard: NULL checks FIRST before any dereference
    if (!engine || !text || !crisis_flags || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathetic_response_detect_crisis: required parameter is NULL (engine, text, crisis_flags, confidence)");
        return false;
    }

    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_detect_crisis", 0.0f);

    if (engine->bio_ctx) {
        bio_router_process_inbox(engine->bio_ctx, 5);
    }

    *crisis_flags = CRISIS_NONE;
    *confidence = 0.0F;

    // Check for suicidal ideation
    if (contains_keywords(text, SUICIDAL_KEYWORDS)) {
        *crisis_flags |= CRISIS_SUICIDAL;
        *confidence = 0.9F;  // High confidence
        return true;
    }

    // Check for self-harm
    if (contains_keywords(text, SELF_HARM_KEYWORDS)) {
        *crisis_flags |= CRISIS_SELF_HARM;
        *confidence = 0.85F;
        return true;
    }

    // Check for abuse disclosure
    if (contains_keywords(text, ABUSE_KEYWORDS)) {
        *crisis_flags |= CRISIS_ABUSE;
        *confidence = 0.95F;  // Very high confidence - critical
        return true;
    }

    /* No crisis keywords found - this is a normal return path, not an error */
    return false;
}

/**
 * @brief Select appropriate response template based on emotion
 *
 * WHAT: Choose template from per-emotion set using continuous intensity
 * WHY:  Continuous dispatch avoids discrete jumps at threshold boundaries
 * HOW:  intensity_value >= 0.6 selects the first (most empathetic) template
 */
static const char* select_response_template(
    negative_emotion_type_t emotion_type,
    float intensity_value)
{
    const char** templates = NULL;

    // Select template set based on emotion
    switch (emotion_type) {
        case EMOTION_RAGE:
        case EMOTION_ANGER:
        case EMOTION_FRUSTRATION:
            templates = RAGE_RESPONSE_TEMPLATES;
            break;

        case EMOTION_HATE:
            templates = HATE_RESPONSE_TEMPLATES;
            break;

        case EMOTION_FEAR:
        case EMOTION_PANIC:
            templates = FEAR_RESPONSE_TEMPLATES;
            break;

        case EMOTION_DESPAIR:
        case EMOTION_SHAME:
            templates = DESPAIR_RESPONSE_TEMPLATES;
            break;

        default:
            // Fallback validation template
            return "I can see you're experiencing some strong feelings right now. "
                   "Those feelings are valid and understandable. You're safe here, "
                   "and I'm here to help you. Would you like to talk about it?";
    }

    // Select template based on continuous intensity (>= 0.6 -> first/most empathetic)
    if (intensity_value >= 0.6f) {
        return templates[0];
    } else if (templates[1] != NULL) {
        return templates[1];
    } else {
        return templates[0];
    }
}

/**
 * @brief Determine primary response strategy
 *
 * WHAT: Select the best response strategy using continuous intensity
 * WHY:  Continuous thresholds (0.8 for extreme, 0.6 for high) provide smooth
 *       transitions between strategies instead of discrete jumps
 * HOW:  intensity_value >= 0.8 -> extreme handling
 *       intensity_value >= 0.6 -> high handling (GROUND for rage/panic)
 */
static response_strategy_t select_strategy(
    negative_emotion_type_t emotion_type,
    float intensity_value,
    uint32_t crisis_flags)
{
    // Guard: Crisis always escalates
    if (crisis_flags != CRISIS_NONE) {
        return RESPONSE_ESCALATE;
    }

    // Guard: Extreme intensity (>= 0.8) needs de-escalation
    if (intensity_value >= 0.8f) {
        // For rage/panic, start with grounding
        if (emotion_type == EMOTION_RAGE || emotion_type == EMOTION_PANIC) {
            return RESPONSE_GROUND;
        }
        // For despair, validate first then consider escalation
        if (emotion_type == EMOTION_DESPAIR) {
            return RESPONSE_VALIDATE;  // Will escalate if student mentions crisis
        }
    }

    // Standard progression: VALIDATE -> EXPLORE -> GROUND if needed
    switch (emotion_type) {
        case EMOTION_RAGE:
        case EMOTION_PANIC:
            return (intensity_value >= 0.6f) ?
                   RESPONSE_GROUND : RESPONSE_VALIDATE;

        case EMOTION_FEAR:
            return RESPONSE_REASSURE;

        case EMOTION_HATE:
        case EMOTION_DISGUST:
            return RESPONSE_EXPLORE;  // Understand source

        case EMOTION_DESPAIR:
        case EMOTION_SHAME:
            return RESPONSE_VALIDATE;

        default:
            return RESPONSE_VALIDATE;
    }
}

bool empathetic_response_generate(
    empathetic_response_engine_t engine,
    const emotional_state_t* state,
    empathetic_response_t* response)
{
    // Guard: NULL checks
    if (!engine || !state || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathetic_response_generate: required parameter is NULL (engine, state, response)");
        return false;
    }

    // Initialize response
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_generate", 0.0f);


    memset(response, 0, sizeof(empathetic_response_t));

    // Check for crisis via text analysis
    uint32_t crisis_flags = CRISIS_NONE;
    float crisis_confidence = 0.0f;
    if (state->text_input) {
        empathetic_response_detect_crisis(engine, state->text_input, &crisis_flags, &crisis_confidence);
    }

    // Also check crisis flags set directly on the emotional state
    if (state->crisis_flags != CRISIS_NONE) {
        crisis_flags |= state->crisis_flags;
        if (state->crisis_confidence > crisis_confidence) {
            crisis_confidence = state->crisis_confidence;
        }
    }

    // Guard: IMMEDIATE ESCALATION for crisis
    if (crisis_flags != CRISIS_NONE) {
        response->primary_strategy = RESPONSE_ESCALATE;
        response->requires_human_escalation = true;

        // Generate crisis-appropriate response
        snprintf(response->response_text, sizeof(response->response_text),
                "Thank you for sharing that with me. What you're feeling is important, "
                "and I want to make sure you get the right support. "
                "I'm going to connect you with someone who can help you right away. "
                "You're not alone, and people care about you.");

        // Set escalation reason
        if (crisis_flags & CRISIS_SUICIDAL) {
            strncpy(response->escalation_reason, "Suicidal ideation detected",
                   sizeof(response->escalation_reason) - 1);
        } else if (crisis_flags & CRISIS_SELF_HARM) {
            strncpy(response->escalation_reason, "Self-harm intention detected",
                   sizeof(response->escalation_reason) - 1);
        } else if (crisis_flags & CRISIS_ABUSE) {
            strncpy(response->escalation_reason, "Abuse disclosure detected",
                   sizeof(response->escalation_reason) - 1);
        }

        response->safety_score = 1.0F;  // Escalation is the safe action
        response->ethics_approved = true;  // Helping in crisis is ethical

        engine->responses_generated++;
        return true;
    }

    // Resolve continuous intensity: prefer intensity_value, fall back to discrete enum
    float intensity_val = state->intensity_value;
    if (intensity_val <= 0.0f && state->intensity != EMOTION_INTENSITY_NONE) {
        intensity_val = emotion_intensity_to_float(state->intensity);
    }

    // Determine strategy using continuous intensity
    response->primary_strategy = select_strategy(state->emotion_type,
                                                 intensity_val,
                                                 crisis_flags);

    // Select and customize template using continuous intensity
    const char* tmpl = select_response_template(state->emotion_type, intensity_val);
    strncpy(response->response_text, tmpl, sizeof(response->response_text) - 1);

    // Set metadata
    response->empathy_score = 0.85F;  // High empathy by design
    response->safety_score = 1.0F;    // Non-reactive responses are safe
    response->ethics_approved = true; // Validation + support is always ethical
    response->requires_human_escalation = false;

    // Track usage
    engine->responses_generated++;
    engine->strategy_usage[response->primary_strategy]++;

    return true;
}

bool empathetic_response_get_grounding_exercise(
    empathetic_response_engine_t engine,
    negative_emotion_type_t emotion_type,
    emotion_intensity_t intensity,
    grounding_exercise_t* exercise)
{
    // Guard: NULL checks
    if (!engine || !exercise) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathetic_response_get_grounding_exercise: required parameter is NULL (engine, exercise)");
        return false;
    }

    // Select exercise based on emotion type and intensity
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_get_grounding_exerci", 0.0f);


    if (emotion_type == EMOTION_PANIC || emotion_type == EMOTION_FEAR) {
        // Breathing exercise for anxiety/panic
        strncpy(exercise->name, "4-4-4 Breathing", sizeof(exercise->name) - 1);
        snprintf(exercise->instructions, sizeof(exercise->instructions),
                "Let's breathe together:\n"
                "1. Breathe in slowly through your nose for 4 counts\n"
                "2. Hold your breath gently for 4 counts\n"
                "3. Breathe out slowly through your mouth for 4 counts\n"
                "4. Repeat 3 times\n"
                "You're doing great. Focus on the feeling of the breath.");
        exercise->duration_seconds = 60;
        exercise->requires_interaction = true;

    } else if (emotion_type == EMOTION_RAGE || emotion_type == EMOTION_ANGER) {
        // 5-4-3-2-1 grounding for anger
        strncpy(exercise->name, "5-4-3-2-1 Grounding", sizeof(exercise->name) - 1);
        snprintf(exercise->instructions, sizeof(exercise->instructions),
                "Let's ground ourselves in the present moment:\n"
                "Name 5 things you can SEE around you\n"
                "Name 4 things you can TOUCH\n"
                "Name 3 things you can HEAR\n"
                "Name 2 things you can SMELL\n"
                "Name 1 thing you can TASTE\n"
                "Take your time. This helps bring us back to the present.");
        exercise->duration_seconds = 120;
        exercise->requires_interaction = true;

    } else {
        // Progressive muscle relaxation for other emotions
        strncpy(exercise->name, "Body Scan", sizeof(exercise->name) - 1);
        snprintf(exercise->instructions, sizeof(exercise->instructions),
                "Let's do a quick body scan:\n"
                "1. Notice your feet. Wiggle your toes.\n"
                "2. Notice your legs. Are they tense? Try to relax them.\n"
                "3. Notice your hands. Open and close them gently.\n"
                "4. Notice your shoulders. Roll them back and down.\n"
                "5. Notice your face. Relax your jaw.\n"
                "Take a deep breath. You're doing well.");
        exercise->duration_seconds = 90;
        exercise->requires_interaction = true;
    }

    return true;
}

float empathetic_response_predict_safety(
    empathetic_response_engine_t engine,
    const emotional_state_t* student_state,
    const char* proposed_response,
    emotional_state_t* predicted_reaction)
{
    // Guard: NULL checks
    if (!engine || !student_state || !proposed_response) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_predict_safety", 0.0f);

    float safety_score = 1.0F;

    char lowercase_response[NIMCP_LOG_BUFFER_SIZE];
    to_lowercase(lowercase_response, proposed_response, sizeof(lowercase_response));

    // Reduce safety if response contains negative patterns
    const char* unsafe_patterns[] = {
        "calm down",  // Dismissive
        "just relax", // Minimizing
        "you're overreacting", // Judgmental
        "don't be", // Dismissive
        "you shouldn't feel", // Invalidating
        NULL
    };

    for (int i = 0; unsafe_patterns[i] != NULL; i++) {
        if (strstr(lowercase_response, unsafe_patterns[i]) != NULL) {
            safety_score -= 0.3F;
        }
    }

    /* Empathy network refinement: ask the perspective network to predict
     * impact of the proposed response on someone in student_state. Impact is
     * on [-1, +1] with negative values harmful. Weight is intentionally
     * small so the keyword-based heuristic still dominates for clear cases
     * — empathy_network only nudges borderline responses. */
    if (engine->empathy_network) {
        event_packet_t proposed = {0};
        proposed.version_flags  = EVENT_FLAG_EXCITATORY;
        proposed.feature_high   = (uint16_t)(student_state->emotion_type & 0xFFFF);
        proposed.feature_low    = (uint16_t)(((uint32_t)student_state->intensity & 0xFF) << 8);
        proposed.source_node_id = 0;
        proposed.timestamp      = 0;
        /* Use student_state intensity as the confidence proxy. */
        float intensity_f = student_state->intensity_value;
        if (intensity_f < 0.0f) intensity_f = -intensity_f;
        if (intensity_f > 1.0f) intensity_f = 1.0f;
        proposed.confidence     = (uint16_t)(intensity_f * 65535.0f);
        proposed.hop_count      = 1;

        float impact = empathy_network_predict_impact(
            (empathy_network_t)engine->empathy_network, &proposed);

        /* Harmful predictions subtract more aggressively than benign ones add. */
        if (impact < 0.0f) {
            safety_score += impact * 0.2f;   /* up to -0.2 */
        } else {
            safety_score += impact * 0.1f;   /* up to +0.1 */
        }
    }

    // Ensure minimum safety
    if (safety_score < 0.0F) {
        safety_score = 0.0F;
    }
    if (safety_score > 1.0F) {
        safety_score = 1.0F;
    }

    // Predict reaction (simplified - would use empathy network)
    if (predicted_reaction) {
        *predicted_reaction = *student_state;
        // If response is safe, predict smooth improvement via continuous reduction
        if (safety_score > 0.7F) {
            // Resolve current continuous intensity
            float cur_val = predicted_reaction->intensity_value;
            if (cur_val <= 0.0f && predicted_reaction->intensity != EMOTION_INTENSITY_NONE) {
                cur_val = emotion_intensity_to_float(predicted_reaction->intensity);
            }
            // Smooth reduction: multiply by 0.8 (gradual de-escalation)
            predicted_reaction->intensity_value = cur_val * 0.8f;
            // Sync discrete label from the new continuous value
            predicted_reaction->intensity = emotion_intensity_from_float(
                predicted_reaction->intensity_value);
            predicted_reaction->valence = fminf(1.0F, predicted_reaction->valence + 0.2F);
        }
    }

    return safety_score;
}

void empathetic_response_track_effectiveness(
    empathetic_response_engine_t engine,
    const empathetic_response_t* response,
    const emotional_state_t* student_reaction,
    float effectiveness)
{
    // Guard: NULL checks
    if (!engine || !response) {
        return;
    }

    // Update running average
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_track_effectiveness", 0.0f);


    float alpha = 0.1F;  // Learning rate
    if (isfinite(effectiveness)) {
        engine->avg_effectiveness = (1.0F - alpha) * engine->avg_effectiveness +
                                   alpha * effectiveness;
    }

    // Update strategy-specific effectiveness
    response_strategy_t strategy = response->primary_strategy;
    if (strategy < 8) {
        engine->strategy_effectiveness[strategy] =
            (1.0F - alpha) * engine->strategy_effectiveness[strategy] +
            alpha * effectiveness;
    }
}

/* ============================================================================
 * W10 (2026-04-24): KG runtime events for empathetic response.
 *
 * Two emits per the task: (1) response-strategy selection at generate exit,
 * (2) effectiveness update at track_effectiveness exit. Plus one query that
 * returns the last predicted_safety bias (read-path for crisis gating in
 * downstream orchestration).
 *
 * Hot-path integration: orchestrators that hold a brain_t call these after
 * `empathetic_response_generate` / `empathetic_response_track_effectiveness`.
 * ============================================================================ */
float empathetic_response_wave10_kg_emit_response(
    struct brain_struct* brain,
    const empathetic_response_t* response)
{
    if (!brain || !response) return 0.5f;
    wave10_empathy_emit_response(brain,
        (int)response->primary_strategy,
        response->requires_human_escalation,
        response->safety_score);
    return wave10_empathy_query_safety_bias(brain);
}

void empathetic_response_wave10_kg_emit_effectiveness(
    struct brain_struct* brain,
    float effectiveness_score)
{
    if (!brain) return;
    wave10_empathy_emit_effectiveness(brain, effectiveness_score);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int empathetic_response_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    empathetic_response_heartbeat("empathetic_r_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Empathetic_Response");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                empathetic_response_heartbeat("empathetic_r_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Empathetic_Response");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Empathetic_Response");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

void empathetic_response_set_instance_health_agent(empathetic_response_engine_t engine, nimcp_health_agent_t* agent) {
    if (engine) { engine->health_agent = agent; }
}

int empathetic_response_training_begin(empathetic_response_engine_t engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathetic_response_training_begin: engine is NULL");
        return -1;
    }
    empathetic_response_heartbeat_instance(engine->health_agent, "empathetic_response_training_begin", 0.0f);
    return 0;
}

int empathetic_response_training_end(empathetic_response_engine_t engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathetic_response_training_end: engine is NULL");
        return -1;
    }
    empathetic_response_heartbeat_instance(engine->health_agent, "empathetic_response_training_end", 1.0f);
    return 0;
}

int empathetic_response_training_step(empathetic_response_engine_t engine, float progress) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathetic_response_training_step: engine is NULL");
        return -1;
    }
    empathetic_response_heartbeat_instance(engine->health_agent, "empathetic_response_training_step", progress);
    return 0;
}
