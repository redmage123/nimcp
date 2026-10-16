/**
 * @file nimcp_multimodal_language.c
 * @brief Multimodal language processor implementation
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "core/brain/regions/broca/nimcp_multimodal_language.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(multimodal_language, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct multimodal_language {
    multimodal_config_t config;
    multimodal_status_t status;
    multimodal_error_t last_error;
    multimodal_stats_t stats;

    bio_router_t* router;
    bool bio_registered;
};

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

multimodal_config_t multimodal_lang_default_config(void) {
    multimodal_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_gestures = MULTIMODAL_DEFAULT_MAX_GESTURES;
    config.max_expressions = MULTIMODAL_DEFAULT_MAX_EXPRESSIONS;
    config.sync_tolerance_ms = MULTIMODAL_DEFAULT_SYNC_TOLERANCE_MS;
    config.enable_auto_gestures = true;
    config.enable_auto_expressions = true;
    config.enable_gaze_tracking = true;
    config.enable_bio_async = false;

    return config;
}

multimodal_language_t* multimodal_lang_create(const multimodal_config_t* config) {
    multimodal_language_t* processor = (multimodal_language_t*)nimcp_calloc(1, sizeof(multimodal_language_t));
    if (!processor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_lang_create: failed to allocate processor");

        return NULL;

    }

    if (config) {
        processor->config = *config;
    } else {
        processor->config = multimodal_lang_default_config();
    }

    processor->status = MULTIMODAL_STATUS_IDLE;

    return processor;
}

void multimodal_lang_destroy(multimodal_language_t* processor) {
    if (!processor) return;
    nimcp_free(processor);
}

bool multimodal_lang_reset(multimodal_language_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    processor->status = MULTIMODAL_STATUS_IDLE;
    processor->last_error = MULTIMODAL_ERROR_NONE;

    return true;
}

/*=============================================================================
 * PLAN GENERATION
 *===========================================================================*/

bool multimodal_lang_generate_plan(
    multimodal_language_t* processor,
    const char* utterance,
    float speech_duration_ms,
    multimodal_plan_t* plan) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return false;
    }
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plan is NULL");
        return false;
    }
    if (speech_duration_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "speech_duration_ms <= 0");
        return false;
    }

    processor->status = MULTIMODAL_STATUS_PLANNING;
    memset(plan, 0, sizeof(multimodal_plan_t));

    plan->speech_duration_ms = speech_duration_ms;
    strncpy(plan->utterance, utterance, sizeof(plan->utterance) - 1);

    /* Auto-generate gestures if enabled */
    if (processor->config.enable_auto_gestures) {
        plan->gestures = (gesture_spec_t*)nimcp_calloc(processor->config.max_gestures, sizeof(gesture_spec_t));
        if (plan->gestures) {
            plan->gesture_count = multimodal_lang_auto_gestures(
                processor, utterance, plan->gestures, processor->config.max_gestures);
        }
    }

    /* Auto-generate expressions if enabled */
    if (processor->config.enable_auto_expressions) {
        plan->expressions = (expression_spec_t*)nimcp_calloc(processor->config.max_expressions, sizeof(expression_spec_t));
        if (plan->expressions) {
            plan->expression_count = multimodal_lang_auto_expressions(
                processor, utterance, 0.0f, plan->expressions, processor->config.max_expressions);
        }
    }

    /* Add default gaze pattern */
    if (processor->config.enable_gaze_tracking) {
        plan->gaze_events = (gaze_spec_t*)nimcp_calloc(16, sizeof(gaze_spec_t));
        if (plan->gaze_events) {
            plan->gaze_events[0].target = GAZE_TARGET_ADDRESSEE;
            plan->gaze_events[0].start_time_ms = 0;
            plan->gaze_events[0].duration_ms = speech_duration_ms * 0.7f;

            plan->gaze_events[1].target = GAZE_TARGET_AWAY;
            plan->gaze_events[1].start_time_ms = speech_duration_ms * 0.7f;
            plan->gaze_events[1].duration_ms = speech_duration_ms * 0.3f;

            plan->gaze_count = 2;
        }
    }

    plan->sync_score = 0.8f;  /* Base sync score */

    processor->stats.plans_generated++;
    processor->status = MULTIMODAL_STATUS_READY;

    /* W12 KG emit: one node per utterance plan. */
    w12_emit_multimodal_plan_auto(plan->gesture_count,
                                  plan->expression_count,
                                  plan->sync_score);
    return true;
}

bool multimodal_lang_add_gesture(
    multimodal_language_t* processor,
    multimodal_plan_t* plan,
    const gesture_spec_t* gesture) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plan is NULL");
        return false;
    }
    if (!gesture) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gesture is NULL");
        return false;
    }

    if (!plan->gestures) {
        plan->gestures = (gesture_spec_t*)nimcp_calloc(processor->config.max_gestures, sizeof(gesture_spec_t));
        if (!plan->gestures) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_lang_reset: plan->gestures is NULL");
            return false;
        }
    }

    if (plan->gesture_count >= processor->config.max_gestures) {
        processor->last_error = MULTIMODAL_ERROR_BUFFER_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "multimodal_lang_reset: capacity exceeded");
        return false;
    }

    plan->gestures[plan->gesture_count++] = *gesture;
    processor->stats.gestures_generated++;
    return true;
}

bool multimodal_lang_add_expression(
    multimodal_language_t* processor,
    multimodal_plan_t* plan,
    const expression_spec_t* expression) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plan is NULL");
        return false;
    }
    if (!expression) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expression is NULL");
        return false;
    }

    if (!plan->expressions) {
        plan->expressions = (expression_spec_t*)nimcp_calloc(processor->config.max_expressions, sizeof(expression_spec_t));
        if (!plan->expressions) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_lang_reset: plan->expressions is NULL");
            return false;
        }
    }

    if (plan->expression_count >= processor->config.max_expressions) {
        processor->last_error = MULTIMODAL_ERROR_BUFFER_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "multimodal_lang_reset: capacity exceeded");
        return false;
    }

    plan->expressions[plan->expression_count++] = *expression;
    processor->stats.expressions_generated++;
    return true;
}

bool multimodal_lang_add_gaze(
    multimodal_language_t* processor,
    multimodal_plan_t* plan,
    const gaze_spec_t* gaze) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plan is NULL");
        return false;
    }
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gaze is NULL");
        return false;
    }

    if (!plan->gaze_events) {
        plan->gaze_events = (gaze_spec_t*)nimcp_calloc(16, sizeof(gaze_spec_t));
        if (!plan->gaze_events) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_lang_reset: plan->gaze_events is NULL");
            return false;
        }
    }

    if (plan->gaze_count >= 16) {
        processor->last_error = MULTIMODAL_ERROR_BUFFER_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "multimodal_lang_reset: capacity exceeded");
        return false;
    }

    plan->gaze_events[plan->gaze_count++] = *gaze;
    processor->stats.gaze_events_generated++;
    return true;
}

void multimodal_lang_free_plan(multimodal_plan_t* plan) {
    if (!plan) return;

    nimcp_free(plan->gestures);
    nimcp_free(plan->expressions);
    nimcp_free(plan->gaze_events);

    plan->gestures = NULL;
    plan->expressions = NULL;
    plan->gaze_events = NULL;
    plan->gesture_count = 0;
    plan->expression_count = 0;
    plan->gaze_count = 0;
}

/*=============================================================================
 * SYNCHRONIZATION
 *===========================================================================*/

bool multimodal_lang_synchronize_plan(
    multimodal_language_t* processor,
    multimodal_plan_t* plan) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plan is NULL");
        return false;
    }

    processor->status = MULTIMODAL_STATUS_SYNCHRONIZING;

    /* Simple synchronization: ensure all events fit within speech duration */
    float max_time = plan->speech_duration_ms;

    for (uint32_t i = 0; i < plan->gesture_count; i++) {
        if (plan->gestures[i].start_time_ms + plan->gestures[i].duration_ms > max_time) {
            plan->gestures[i].duration_ms = max_time - plan->gestures[i].start_time_ms;
            if (plan->gestures[i].duration_ms < 0) plan->gestures[i].duration_ms = 0;
        }
    }

    for (uint32_t i = 0; i < plan->expression_count; i++) {
        if (plan->expressions[i].start_time_ms + plan->expressions[i].duration_ms > max_time) {
            plan->expressions[i].duration_ms = max_time - plan->expressions[i].start_time_ms;
            if (plan->expressions[i].duration_ms < 0) plan->expressions[i].duration_ms = 0;
        }
    }

    plan->sync_score = 0.9f;  /* Synchronized plan has higher score */

    processor->stats.avg_sync_score =
        (processor->stats.avg_sync_score * (processor->stats.plans_generated - 1) + plan->sync_score)
        / processor->stats.plans_generated;

    processor->status = MULTIMODAL_STATUS_READY;
    return true;
}

bool multimodal_lang_get_events_at_time(
    const multimodal_plan_t* plan,
    float time_ms,
    gesture_spec_t* gesture,
    expression_spec_t* expression,
    gaze_spec_t* gaze) {

    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_lang_free_plan: plan is NULL");
        return false;
    }

    /* Find active gesture */
    if (gesture) {
        memset(gesture, 0, sizeof(gesture_spec_t));
        for (uint32_t i = 0; i < plan->gesture_count; i++) {
            float end_time = plan->gestures[i].start_time_ms + plan->gestures[i].duration_ms;
            if (time_ms >= plan->gestures[i].start_time_ms && time_ms < end_time) {
                *gesture = plan->gestures[i];
                break;
            }
        }
    }

    /* Find active expression */
    if (expression) {
        memset(expression, 0, sizeof(expression_spec_t));
        for (uint32_t i = 0; i < plan->expression_count; i++) {
            float end_time = plan->expressions[i].start_time_ms + plan->expressions[i].duration_ms;
            if (time_ms >= plan->expressions[i].start_time_ms && time_ms < end_time) {
                *expression = plan->expressions[i];
                break;
            }
        }
    }

    /* Find active gaze */
    if (gaze) {
        memset(gaze, 0, sizeof(gaze_spec_t));
        for (uint32_t i = 0; i < plan->gaze_count; i++) {
            float end_time = plan->gaze_events[i].start_time_ms + plan->gaze_events[i].duration_ms;
            if (time_ms >= plan->gaze_events[i].start_time_ms && time_ms < end_time) {
                *gaze = plan->gaze_events[i];
                break;
            }
        }
    }

    return true;
}

/*=============================================================================
 * AUTO-GENERATION
 *===========================================================================*/

/* Words that typically trigger iconic gestures */
static const char* ICONIC_TRIGGERS[] = {
    "big", "small", "round", "square", "up", "down", "left", "right",
    "push", "pull", "open", "close", "fast", "slow",
    NULL
};

/* Words that typically trigger deictic gestures */
static const char* DEICTIC_TRIGGERS[] = {
    "this", "that", "here", "there", "these", "those",
    NULL
};

static bool contains_word(const char* text, const char* word) {
    if (!text || !word) {
        return false;
    }

    const char* p = text;
    size_t word_len = strlen(word);

    while ((p = strstr(p, word)) != NULL) {
        bool start_ok = (p == text) || !isalnum((unsigned char)*(p - 1));
        bool end_ok = !isalnum((unsigned char)*(p + word_len));
        if (start_ok && end_ok) return true;
        p++;
    }
    return false;
}

uint32_t multimodal_lang_auto_gestures(
    multimodal_language_t* processor,
    const char* utterance,
    gesture_spec_t* gestures,
    uint32_t max_gestures) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0;
    }
    if (!gestures) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gestures is NULL");
        return 0;
    }
    if (max_gestures == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_gestures is zero");
        return 0;
    }

    uint32_t count = 0;
    float duration_per_word = 200.0f;  /* Rough estimate */

    char lower[NIMCP_ERROR_BUFFER_LARGE];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && utterance[i]; i++) {
        lower[i] = tolower((unsigned char)utterance[i]);
    }
    lower[i] = '\0';

    /* Check for iconic gesture triggers */
    for (const char** trigger = ICONIC_TRIGGERS; *trigger && count < max_gestures; trigger++) {
        if (contains_word(lower, *trigger)) {
            gesture_spec_t* g = &gestures[count++];
            g->gesture_id = count;
            g->type = GESTURE_TYPE_ICONIC;
            g->start_time_ms = count * duration_per_word;
            g->duration_ms = duration_per_word * 1.5f;
            g->intensity = 0.7f;
            g->hand = 1;  /* Right hand */
            strncpy(g->associated_word, *trigger, sizeof(g->associated_word) - 1);

            processor->stats.gestures_generated++;
        }
    }

    /* Check for deictic gesture triggers */
    for (const char** trigger = DEICTIC_TRIGGERS; *trigger && count < max_gestures; trigger++) {
        if (contains_word(lower, *trigger)) {
            gesture_spec_t* g = &gestures[count++];
            g->gesture_id = count;
            g->type = GESTURE_TYPE_DEICTIC;
            g->start_time_ms = count * duration_per_word;
            g->duration_ms = duration_per_word;
            g->intensity = 0.8f;
            g->hand = 1;
            strncpy(g->associated_word, *trigger, sizeof(g->associated_word) - 1);

            processor->stats.gestures_generated++;
        }
    }

    /* Add beat gestures for emphasis words */
    static const char* EMPHASIS_WORDS[] = {"very", "really", "absolutely", "definitely", NULL};
    for (const char** word = EMPHASIS_WORDS; *word && count < max_gestures; word++) {
        if (contains_word(lower, *word)) {
            gesture_spec_t* g = &gestures[count++];
            g->gesture_id = count;
            g->type = GESTURE_TYPE_BEAT;
            g->start_time_ms = count * duration_per_word;
            g->duration_ms = duration_per_word * 0.5f;
            g->intensity = 0.5f;
            g->hand = 2;  /* Both hands */
            strncpy(g->associated_word, *word, sizeof(g->associated_word) - 1);

            processor->stats.gestures_generated++;
        }
    }

    return count;
}

uint32_t multimodal_lang_auto_expressions(
    multimodal_language_t* processor,
    const char* utterance,
    float emotion_valence,
    expression_spec_t* expressions,
    uint32_t max_expressions) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0;
    }
    if (!expressions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expressions is NULL");
        return 0;
    }
    if (max_expressions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_expressions is zero");
        return 0;
    }

    uint32_t count = 0;

    char lower[NIMCP_ERROR_BUFFER_LARGE];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && utterance[i]; i++) {
        lower[i] = tolower((unsigned char)utterance[i]);
    }
    lower[i] = '\0';

    /* Add expression based on content */
    expression_spec_t* e = &expressions[count++];
    e->expression_id = 1;
    e->start_time_ms = 0;
    e->duration_ms = 1000.0f;  /* Will be adjusted by plan */
    e->intensity = 0.5f;

    /* Detect emotion from content */
    if (contains_word(lower, "happy") || contains_word(lower, "great") ||
        contains_word(lower, "wonderful") || emotion_valence > 0.3f) {
        e->type = EXPRESSION_SMILE;
        e->intensity = 0.7f;
    } else if (contains_word(lower, "sad") || contains_word(lower, "sorry") ||
               emotion_valence < -0.3f) {
        e->type = EXPRESSION_FROWN;
        e->intensity = 0.6f;
    } else if (contains_word(lower, "what") || contains_word(lower, "really") ||
               contains_word(lower, "surprised")) {
        e->type = EXPRESSION_RAISED_EYEBROWS;
        e->intensity = 0.6f;
    } else {
        e->type = EXPRESSION_NEUTRAL;
    }

    strncpy(e->associated_content, utterance,
            sizeof(e->associated_content) - 1 < 64 ? sizeof(e->associated_content) - 1 : 64);

    processor->stats.expressions_generated++;
    return count;
}

/*=============================================================================
 * TYPE NAMES
 *===========================================================================*/

const char* multimodal_lang_gesture_name(gesture_type_t type) {
    static const char* names[] = {
        "NONE", "ICONIC", "METAPHORIC", "BEAT", "DEICTIC", "EMBLEMATIC"
    };
    if (type >= GESTURE_TYPE_COUNT) return "INVALID";
    return names[type];
}

const char* multimodal_lang_expression_name(expression_type_t type) {
    static const char* names[] = {
        "NEUTRAL", "SMILE", "FROWN", "RAISED_EYEBROWS",
        "SQUINT", "WIDE_EYES", "PURSED_LIPS"
    };
    if (type >= EXPRESSION_COUNT) return "INVALID";
    return names[type];
}

const char* multimodal_lang_gaze_name(gaze_target_t target) {
    static const char* names[] = {
        "FORWARD", "ADDRESSEE", "OBJECT", "AWAY", "UP", "DOWN"
    };
    if (target >= GAZE_TARGET_COUNT) return "INVALID";
    return names[target];
}

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

multimodal_status_t multimodal_lang_get_status(const multimodal_language_t* processor) {
    if (!processor) return MULTIMODAL_STATUS_ERROR;
    return processor->status;
}

multimodal_error_t multimodal_lang_get_last_error(const multimodal_language_t* processor) {
    if (!processor) return MULTIMODAL_ERROR_INTERNAL;
    return processor->last_error;
}

bool multimodal_lang_get_stats(const multimodal_language_t* processor, multimodal_stats_t* stats) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return false;
    }
    *stats = processor->stats;
    return true;
}

void multimodal_lang_reset_stats(multimodal_language_t* processor) {
    if (!processor) return;
    memset(&processor->stats, 0, sizeof(multimodal_stats_t));
}

bool multimodal_lang_get_config(const multimodal_language_t* processor, multimodal_config_t* config) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return false;
    }
    *config = processor->config;
    return true;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool multimodal_lang_register_bio_handler(
    multimodal_language_t* processor,
    bio_router_t* router) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");
        return false;
    }

    processor->router = router;
    processor->bio_registered = true;
    return true;
}
