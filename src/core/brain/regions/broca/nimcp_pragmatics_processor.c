/**
 * @file nimcp_pragmatics_processor.c
 * @brief Pragmatics processor implementation
 *
 * WHAT: Speech act recognition and Gricean maxim processing
 * WHY:  Enable understanding of speaker intent beyond literal meaning
 * HOW:  Classify speech acts, detect implicature, track context
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pragmatics_processor, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Participant record
 */
typedef struct {
    uint32_t id;
    char name[NIMCP_ID_BUFFER_SIZE];
    bool active;
} participant_record_t;

/**
 * @brief Pragmatics processor internal state
 */
struct pragmatics_processor {
    pragmatics_config_t config;
    pragmatics_status_t status;
    pragmatics_error_t last_error;
    pragmatics_stats_t stats;

    /* Context tracking */
    utterance_context_t* context_buffer;
    uint32_t context_count;
    uint32_t context_head;
    uint32_t next_utterance_id;

    /* Participants */
    participant_record_t participants[PRAGMATICS_MAX_PARTICIPANTS];
    uint32_t participant_count;

    /* Bio-async */
    bio_router_t* router;
    bool bio_registered;

    /* Timing */
    double last_processing_time_ms;
};

/*=============================================================================
 * SPEECH ACT DETECTION PATTERNS
 *===========================================================================*/

typedef struct {
    const char* pattern;
    speech_act_type_t act;
    float base_confidence;
} speech_act_pattern_t;

static const speech_act_pattern_t SPEECH_ACT_PATTERNS[] = {
    /* Directives - explicit commands with please */
    {"please", SPEECH_ACT_REQUEST, 0.7f},
    {"do ", SPEECH_ACT_COMMAND, 0.5f},
    {"don't", SPEECH_ACT_COMMAND, 0.55f},

    /* Questions - these will be detected as indirect requests later */
    {"could you", SPEECH_ACT_QUESTION, 0.8f},
    {"can you", SPEECH_ACT_QUESTION, 0.75f},
    {"would you", SPEECH_ACT_QUESTION, 0.8f},
    {"what ", SPEECH_ACT_QUESTION, 0.85f},
    {"where ", SPEECH_ACT_QUESTION, 0.85f},
    {"when ", SPEECH_ACT_QUESTION, 0.85f},
    {"why ", SPEECH_ACT_QUESTION, 0.85f},
    {"how ", SPEECH_ACT_QUESTION, 0.85f},
    {"who ", SPEECH_ACT_QUESTION, 0.85f},
    /* Note: "is " and "are " removed - too generic */

    /* Commissives */
    {"i will", SPEECH_ACT_PROMISE, 0.7f},
    {"i'll", SPEECH_ACT_PROMISE, 0.65f},
    {"i promise", SPEECH_ACT_PROMISE, 0.95f},
    {"i offer", SPEECH_ACT_OFFER, 0.9f},

    /* Expressives */
    {"thank", SPEECH_ACT_THANK, 0.9f},
    {"sorry", SPEECH_ACT_APOLOGIZE, 0.85f},
    {"apologize", SPEECH_ACT_APOLOGIZE, 0.95f},
    {"congratulations", SPEECH_ACT_CONGRATULATE, 0.95f},
    {"welcome", SPEECH_ACT_WELCOME, 0.8f},
    {"hello", SPEECH_ACT_GREET, 0.9f},
    {"hi ", SPEECH_ACT_GREET, 0.85f},
    {"hey ", SPEECH_ACT_GREET, 0.8f},

    /* Declaratives */
    {"i declare", SPEECH_ACT_DECLARE, 0.95f},
    {"i announce", SPEECH_ACT_ANNOUNCE, 0.95f},
    {"i name", SPEECH_ACT_NAME, 0.9f},
    {"i appoint", SPEECH_ACT_APPOINT, 0.95f},

    {NULL, SPEECH_ACT_UNKNOWN, 0.0f}
};

/*=============================================================================
 * SCALAR IMPLICATURE WORDS
 *===========================================================================*/

typedef struct {
    const char* weak;
    const char* strong;
} scalar_pair_t;

static const scalar_pair_t SCALAR_PAIRS[] = {
    {"some", "all"},
    {"sometimes", "always"},
    {"possible", "certain"},
    {"might", "must"},
    {"or", "and"},
    {"warm", "hot"},
    {"good", "excellent"},
    {"like", "love"},
    {NULL, NULL}
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Case-insensitive substring search
 */
static bool contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle) {
        return false;
    }

    size_t h_len = strlen(haystack);
    size_t n_len = strlen(needle);

    if (n_len > h_len) {
        return false; /* Needle longer than haystack - normal not-found case */
    }

    for (size_t i = 0; i <= h_len - n_len; i++) {
        bool match = true;
        for (size_t j = 0; j < n_len && match; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = false;
            }
        }
        if (match) return true;
    }
    return false;
}

/**
 * @brief Check if utterance ends with question mark
 */
static bool ends_with_question(const char* utterance) {
    if (!utterance) {
        return false;
    }
    size_t len = strlen(utterance);
    if (len == 0) {
        return false;
    }

    /* Skip trailing whitespace */
    while (len > 0 && isspace((unsigned char)utterance[len - 1])) {
        len--;
    }
    return len > 0 && utterance[len - 1] == '?';
}

/**
 * @brief Get current time in milliseconds
 */
static double get_time_ms(void) {
    /* Simplified - would use platform time in real implementation */
    return 0.0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

pragmatics_config_t pragmatics_default_config(void) {
    pragmatics_config_t config;
    memset(&config, 0, sizeof(config));

    config.context_depth = PRAGMATICS_DEFAULT_CONTEXT_DEPTH;
    config.max_utterances = PRAGMATICS_DEFAULT_MAX_UTTERANCES;
    config.max_speech_acts = PRAGMATICS_DEFAULT_MAX_SPEECH_ACTS;
    config.classification_threshold = 0.5f;

    config.implicature_depth = PRAGMATICS_DEFAULT_IMPLICATURE_DEPTH;
    config.enable_scalar_implicature = true;
    config.enable_indirect_speech = true;

    config.enable_grice_analysis = true;
    config.grice_sensitivity = 0.6f;

    config.enable_bio_async = false;
    config.enable_emotion_integration = false;
    config.enable_working_memory = true;

    return config;
}

pragmatics_processor_t* pragmatics_create(const pragmatics_config_t* config) {
    pragmatics_processor_t* processor = (pragmatics_processor_t*)nimcp_calloc(
        1, sizeof(pragmatics_processor_t));
    if (!processor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pragmatics_create: failed to allocate processor");

        return NULL;

    }

    /* Apply config */
    if (config) {
        processor->config = *config;
    } else {
        processor->config = pragmatics_default_config();
    }

    /* Allocate context buffer */
    processor->context_buffer = (utterance_context_t*)nimcp_calloc(
        processor->config.max_utterances, sizeof(utterance_context_t));
    if (!processor->context_buffer) {
        nimcp_free(processor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pragmatics_create: processor->context_buffer is NULL");
        return NULL;
    }

    processor->status = PRAGMATICS_STATUS_IDLE;
    processor->last_error = PRAGMATICS_ERROR_NONE;
    processor->context_count = 0;
    processor->context_head = 0;
    processor->next_utterance_id = 1;
    processor->participant_count = 0;
    processor->router = NULL;
    processor->bio_registered = false;

    return processor;
}

void pragmatics_destroy(pragmatics_processor_t* processor) {
    if (!processor) return;

    if (processor->context_buffer) {
        nimcp_free(processor->context_buffer);
    }

    nimcp_free(processor);
}

bool pragmatics_reset(pragmatics_processor_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pragmatics_reset: processor is NULL");
        return false;
    }

    processor->status = PRAGMATICS_STATUS_IDLE;
    processor->last_error = PRAGMATICS_ERROR_NONE;
    processor->context_count = 0;
    processor->context_head = 0;
    processor->next_utterance_id = 1;

    if (processor->context_buffer) {
        memset(processor->context_buffer, 0,
               processor->config.max_utterances * sizeof(utterance_context_t));
    }

    return true;
}

/*=============================================================================
 * SPEECH ACT CLASSIFICATION
 *===========================================================================*/

bool pragmatics_classify_speech_act(
    pragmatics_processor_t* processor,
    const char* utterance,
    uint32_t speaker_id,
    speech_act_result_t* result) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        processor->last_error = PRAGMATICS_ERROR_INVALID_INPUT;
        return false;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        processor->last_error = PRAGMATICS_ERROR_INVALID_INPUT;
        return false;
    }

    double start_time = get_time_ms();
    processor->status = PRAGMATICS_STATUS_CLASSIFYING;

    memset(result, 0, sizeof(speech_act_result_t));
    result->primary_act = SPEECH_ACT_UNKNOWN;
    result->secondary_act = SPEECH_ACT_UNKNOWN;
    result->target_participant = speaker_id;

    /* Check for question mark ending (strong question indicator) */
    bool is_question_form = ends_with_question(utterance);

    /* Pattern matching for speech act detection */
    float best_confidence = 0.0f;
    speech_act_type_t best_act = SPEECH_ACT_UNKNOWN;

    for (const speech_act_pattern_t* p = SPEECH_ACT_PATTERNS; p->pattern; p++) {
        if (contains_ci(utterance, p->pattern)) {
            float confidence = p->base_confidence;

            /* Boost question acts if question mark present */
            if (is_question_form && p->act == SPEECH_ACT_QUESTION) {
                confidence += 0.1f;
                if (confidence > 1.0f) confidence = 1.0f;
            }

            if (confidence > best_confidence) {
                best_confidence = confidence;
                best_act = p->act;
            }
        }
    }

    /* Default to assertion if no pattern matched */
    if (best_act == SPEECH_ACT_UNKNOWN && strlen(utterance) > 0) {
        best_act = SPEECH_ACT_ASSERT;
        best_confidence = 0.4f;
    }

    result->primary_act = best_act;
    result->primary_confidence = best_confidence;

    /* Detect indirect speech acts */
    if (processor->config.enable_indirect_speech) {
        speech_act_type_t indirect;
        float indirect_conf = pragmatics_detect_indirect_act(
            processor, utterance, best_act, &indirect);

        if (indirect_conf > processor->config.classification_threshold &&
            indirect != best_act) {
            /* For indirect directives (request/command) from questions,
             * the intended meaning is more important than surface form */
            if ((indirect == SPEECH_ACT_REQUEST || indirect == SPEECH_ACT_COMMAND ||
                 indirect == SPEECH_ACT_SUGGEST_ACTION) &&
                best_act == SPEECH_ACT_QUESTION) {
                /* Swap - make the directive the primary act */
                result->secondary_act = best_act;
                result->secondary_confidence = best_confidence;
                result->primary_act = indirect;
                result->primary_confidence = indirect_conf;
            } else {
                result->secondary_act = indirect;
                result->secondary_confidence = indirect_conf;
            }
            result->is_indirect = true;
        }
    }

    /* Update stats */
    processor->stats.speech_acts_classified++;
    if (best_act < SPEECH_ACT_COUNT) {
        processor->stats.act_type_counts[best_act]++;
    }
    if (result->is_indirect) {
        processor->stats.indirect_acts_detected++;
    }

    processor->last_processing_time_ms = get_time_ms() - start_time;
    processor->stats.total_processing_time_ms += processor->last_processing_time_ms;
    processor->status = PRAGMATICS_STATUS_READY;

    return true;
}

float pragmatics_detect_indirect_act(
    pragmatics_processor_t* processor,
    const char* utterance,
    speech_act_type_t surface_act,
    speech_act_type_t* indirect_act) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0.0f;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0.0f;
    }
    if (!indirect_act) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "indirect_act is NULL");
        return 0.0f;
    }

    *indirect_act = surface_act;

    /* Common indirect speech patterns */

    /* "Can you X?" / "Could you X?" -> REQUEST */
    if (surface_act == SPEECH_ACT_QUESTION) {
        if (contains_ci(utterance, "can you") ||
            contains_ci(utterance, "could you") ||
            contains_ci(utterance, "would you mind")) {
            *indirect_act = SPEECH_ACT_REQUEST;
            return 0.85f;
        }

        /* "Why don't you X?" -> SUGGEST_ACTION */
        if (contains_ci(utterance, "why don't you") ||
            contains_ci(utterance, "why not")) {
            *indirect_act = SPEECH_ACT_SUGGEST_ACTION;
            return 0.8f;
        }
    }

    /* "I wish X" -> REQUEST (implicitly) */
    if (contains_ci(utterance, "i wish") ||
        contains_ci(utterance, "it would be nice")) {
        *indirect_act = SPEECH_ACT_REQUEST;
        return 0.6f;
    }

    return 0.0f;
}

const char* pragmatics_speech_act_name(speech_act_type_t act) {
    static const char* names[] = {
        "UNKNOWN",
        "ASSERT", "CLAIM", "CONCLUDE", "REPORT", "SUGGEST_FACT",
        "COMMAND", "REQUEST", "SUGGEST_ACTION", "ADVISE", "QUESTION", "INVITE",
        "PROMISE", "OFFER", "THREAT", "REFUSE", "PLEDGE",
        "THANK", "APOLOGIZE", "CONGRATULATE", "COMPLAIN", "WELCOME", "GREET",
        "DECLARE", "ANNOUNCE", "NAME", "APPOINT"
    };

    if (act >= SPEECH_ACT_COUNT) return "INVALID";
    return names[act];
}

/*=============================================================================
 * GRICEAN ANALYSIS
 *===========================================================================*/

bool pragmatics_analyze_grice(
    pragmatics_processor_t* processor,
    const char* utterance,
    const utterance_context_t* context,
    grice_analysis_result_t* result) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        processor->last_error = PRAGMATICS_ERROR_INVALID_INPUT;
        return false;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        processor->last_error = PRAGMATICS_ERROR_INVALID_INPUT;
        return false;
    }

    if (!processor->config.enable_grice_analysis) {
        /* Fill with observed (neutral) values */
        for (int i = 0; i < GRICE_MAXIM_COUNT; i++) {
            result->violations[i] = MAXIM_OBSERVED;
            result->adherence_scores[i] = 1.0f;
        }
        result->overall_cooperativeness = 1.0f;
        result->flouting_detected = false;
        return true;
    }

    processor->status = PRAGMATICS_STATUS_ANALYZING;

    /* Initialize result */
    memset(result, 0, sizeof(grice_analysis_result_t));
    for (int i = 0; i < GRICE_MAXIM_COUNT; i++) {
        result->adherence_scores[i] = 1.0f;
        result->violations[i] = MAXIM_OBSERVED;
    }

    size_t utterance_len = strlen(utterance);

    /* QUANTITY: Check if utterance is too short or too long */
    if (utterance_len < 5) {
        result->adherence_scores[GRICE_MAXIM_QUANTITY] = 0.5f;
    } else if (utterance_len > 500) {
        result->adherence_scores[GRICE_MAXIM_QUANTITY] = 0.7f;
    }

    /* QUALITY: Check for hedging language that might indicate uncertainty */
    if (contains_ci(utterance, "i think") ||
        contains_ci(utterance, "maybe") ||
        contains_ci(utterance, "perhaps") ||
        contains_ci(utterance, "might be")) {
        /* Hedging is cooperative - indicates awareness of uncertainty */
        result->adherence_scores[GRICE_MAXIM_QUALITY] = 0.9f;
    }

    /* RELATION: Check relevance to context if provided */
    if (context) {
        /* Simple check - more sophisticated would use semantic similarity */
        result->adherence_scores[GRICE_MAXIM_RELATION] = 0.8f;
    }

    /* MANNER: Check for clarity */
    if (contains_ci(utterance, "um") ||
        contains_ci(utterance, "uh") ||
        contains_ci(utterance, "like,") ||
        contains_ci(utterance, "you know")) {
        result->adherence_scores[GRICE_MAXIM_MANNER] = 0.7f;
    }

    /* Calculate overall cooperativeness */
    float sum = 0.0f;
    for (int i = 0; i < GRICE_MAXIM_COUNT; i++) {
        sum += result->adherence_scores[i];

        /* Check for violations */
        if (result->adherence_scores[i] < processor->config.grice_sensitivity) {
            result->violations[i] = MAXIM_VIOLATED_FLOUTED;
            result->flouting_detected = true;
            processor->stats.maxim_violations_found++;
        }
    }
    result->overall_cooperativeness = sum / GRICE_MAXIM_COUNT;

    processor->status = PRAGMATICS_STATUS_READY;
    return true;
}

const char* pragmatics_grice_maxim_name(grice_maxim_t maxim) {
    static const char* names[] = {
        "QUANTITY", "QUALITY", "RELATION", "MANNER"
    };

    if (maxim >= GRICE_MAXIM_COUNT) return "INVALID";
    return names[maxim];
}

/*=============================================================================
 * IMPLICATURE DETECTION
 *===========================================================================*/

uint32_t pragmatics_detect_implicatures(
    pragmatics_processor_t* processor,
    const char* utterance,
    const grice_analysis_result_t* grice_result,
    implicature_result_t* implicatures,
    uint32_t max_implicatures) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0;
    }
    if (!implicatures) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "implicatures is NULL");
        return 0;
    }
    if (max_implicatures == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_implicatures is zero");
        return 0;
    }

    processor->status = PRAGMATICS_STATUS_INFERRING;
    uint32_t count = 0;

    /* Check for scalar implicatures */
    if (processor->config.enable_scalar_implicature && count < max_implicatures) {
        implicature_result_t scalar;
        if (pragmatics_detect_scalar_implicature(processor, utterance, &scalar)) {
            implicatures[count++] = scalar;
        }
    }

    /* Check for implicatures from maxim violations */
    if (grice_result && grice_result->flouting_detected && count < max_implicatures) {
        for (int i = 0; i < GRICE_MAXIM_COUNT && count < max_implicatures; i++) {
            if (grice_result->violations[i] == MAXIM_VIOLATED_FLOUTED) {
                implicature_result_t impl;
                memset(&impl, 0, sizeof(impl));
                impl.type = IMPLICATURE_CONVERSATIONAL;
                impl.confidence = 0.6f;
                impl.triggered_by = (grice_maxim_t)i;
                snprintf(impl.implied_content, sizeof(impl.implied_content),
                         "Flouting of %s maxim suggests additional meaning",
                         pragmatics_grice_maxim_name((grice_maxim_t)i));
                implicatures[count++] = impl;
            }
        }
    }

    processor->stats.implicatures_detected += count;
    processor->status = PRAGMATICS_STATUS_READY;
    return count;
}

bool pragmatics_detect_scalar_implicature(
    pragmatics_processor_t* processor,
    const char* utterance,
    implicature_result_t* result) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return false;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return false;
    }

    for (const scalar_pair_t* pair = SCALAR_PAIRS; pair->weak; pair++) {
        if (contains_ci(utterance, pair->weak) &&
            !contains_ci(utterance, pair->strong)) {
            memset(result, 0, sizeof(implicature_result_t));
            result->type = IMPLICATURE_SCALAR;
            result->confidence = 0.75f;
            result->triggered_by = GRICE_MAXIM_QUANTITY;
            snprintf(result->implied_content, sizeof(result->implied_content),
                     "Use of '%s' implies 'not %s'", pair->weak, pair->strong);
            return true;
        }
    }

    return false; /* No scalar implicature found - normal case */
}

/*=============================================================================
 * CONTEXT MANAGEMENT
 *===========================================================================*/

uint32_t pragmatics_add_to_context(
    pragmatics_processor_t* processor,
    const char* utterance,
    uint32_t speaker_id,
    uint64_t timestamp_ms) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0;
    }

    /* Get slot in circular buffer */
    uint32_t slot = processor->context_head;

    /* Fill context entry */
    utterance_context_t* entry = &processor->context_buffer[slot];
    entry->utterance_id = processor->next_utterance_id++;
    entry->speaker_id = speaker_id;
    entry->timestamp_ms = timestamp_ms;
    entry->salience = 1.0f;  /* Most recent has highest salience */
    strncpy(entry->content, utterance, sizeof(entry->content) - 1);
    entry->content[sizeof(entry->content) - 1] = '\0';

    /* Classify speech act */
    pragmatics_classify_speech_act(processor, utterance, speaker_id, &entry->speech_act);

    /* Update head pointer */
    processor->context_head = (processor->context_head + 1) % processor->config.max_utterances;
    if (processor->context_count < processor->config.max_utterances) {
        processor->context_count++;
    }

    /* Decay salience of older entries */
    for (uint32_t i = 0; i < processor->context_count; i++) {
        uint32_t idx = (processor->context_head + processor->config.max_utterances - 1 - i)
                       % processor->config.max_utterances;
        if (idx != slot) {
            processor->context_buffer[idx].salience *= 0.9f;
        }
    }

    processor->stats.utterances_processed++;
    return entry->utterance_id;
}

uint32_t pragmatics_get_context(
    pragmatics_processor_t* processor,
    utterance_context_t* context,
    uint32_t max_entries) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "context is NULL");
        return 0;
    }
    if (max_entries == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_entries is zero");
        return 0;
    }

    uint32_t count = (processor->context_count < max_entries) ?
                     processor->context_count : max_entries;

    /* Return most recent first */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (processor->context_head + processor->config.max_utterances - 1 - i)
                       % processor->config.max_utterances;
        context[i] = processor->context_buffer[idx];
    }

    return count;
}

void pragmatics_clear_context(pragmatics_processor_t* processor) {
    if (!processor) return;

    processor->context_count = 0;
    processor->context_head = 0;

    if (processor->context_buffer) {
        memset(processor->context_buffer, 0,
               processor->config.max_utterances * sizeof(utterance_context_t));
    }
}

bool pragmatics_register_participant(
    pragmatics_processor_t* processor,
    uint32_t participant_id,
    const char* name) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "name is NULL");
        return false;
    }
    if (processor->participant_count >= PRAGMATICS_MAX_PARTICIPANTS) return false;

    /* Check for existing participant */
    for (uint32_t i = 0; i < processor->participant_count; i++) {
        if (processor->participants[i].id == participant_id) {
            strncpy(processor->participants[i].name, name,
                    sizeof(processor->participants[i].name) - 1);
            return true;
        }
    }

    /* Add new participant */
    participant_record_t* p = &processor->participants[processor->participant_count++];
    p->id = participant_id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->active = true;

    return true;
}

/*=============================================================================
 * FULL ANALYSIS
 *===========================================================================*/

bool pragmatics_analyze(
    pragmatics_processor_t* processor,
    const char* utterance,
    uint32_t speaker_id,
    uint64_t timestamp_ms,
    pragmatic_analysis_t* analysis) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        processor->last_error = PRAGMATICS_ERROR_INVALID_INPUT;
        return false;
    }
    if (!analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis is NULL");
        processor->last_error = PRAGMATICS_ERROR_INVALID_INPUT;
        return false;
    }

    double start_time = get_time_ms();
    memset(analysis, 0, sizeof(pragmatic_analysis_t));

    /* 1. Classify speech act */
    if (!pragmatics_classify_speech_act(processor, utterance, speaker_id,
                                        &analysis->speech_act)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pragmatics_clear_context: operation failed");
        return false;
    }

    /* 2. Get relevant context */
    utterance_context_t recent_context[4];
    uint32_t context_count = pragmatics_get_context(processor, recent_context, 4);

    /* 3. Analyze Gricean maxims */
    if (!pragmatics_analyze_grice(processor, utterance,
                                  context_count > 0 ? &recent_context[0] : NULL,
                                  &analysis->grice_analysis)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pragmatics_clear_context: operation failed");
        return false;
    }

    /* 4. Detect implicatures */
    analysis->implicature_count = pragmatics_detect_implicatures(
        processor, utterance, &analysis->grice_analysis,
        analysis->implicatures, PRAGMATICS_DEFAULT_IMPLICATURE_DEPTH);

    /* 5. Calculate context relevance */
    analysis->context_relevance = analysis->grice_analysis.adherence_scores[GRICE_MAXIM_RELATION];

    /* Store relevant context IDs */
    for (uint32_t i = 0; i < context_count && i < 4; i++) {
        analysis->relevant_context_ids[i] = recent_context[i].utterance_id;
        analysis->relevant_context_count++;
    }

    /* 6. Add to context */
    pragmatics_add_to_context(processor, utterance, speaker_id, timestamp_ms);

    analysis->processing_time_ms = get_time_ms() - start_time;
    processor->stats.avg_processing_time_ms =
        processor->stats.total_processing_time_ms /
        (double)(processor->stats.utterances_processed > 0 ?
                 processor->stats.utterances_processed : 1);

    /* W12 KG emit: one node per completed pragmatic analysis. */
    w12_emit_pragmatics_analyze_auto((int)analysis->speech_act.primary_act,
                                     speaker_id,
                                     analysis->implicature_count,
                                     analysis->context_relevance);

    return true;
}

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

pragmatics_status_t pragmatics_get_status(const pragmatics_processor_t* processor) {
    if (!processor) return PRAGMATICS_STATUS_ERROR;
    return processor->status;
}

pragmatics_error_t pragmatics_get_last_error(const pragmatics_processor_t* processor) {
    if (!processor) return PRAGMATICS_ERROR_INTERNAL;
    return processor->last_error;
}

bool pragmatics_get_stats(const pragmatics_processor_t* processor, pragmatics_stats_t* stats) {
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

void pragmatics_reset_stats(pragmatics_processor_t* processor) {
    if (!processor) return;
    memset(&processor->stats, 0, sizeof(pragmatics_stats_t));
}

bool pragmatics_get_config(const pragmatics_processor_t* processor, pragmatics_config_t* config) {
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

bool pragmatics_register_bio_handler(
    pragmatics_processor_t* processor,
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

bool pragmatics_send_analysis(
    pragmatics_processor_t* processor,
    const pragmatic_analysis_t* analysis) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis is NULL");
        return false;
    }
    if (!processor->bio_registered || !processor->router) return false;

    /* Would send via bio-async in real implementation */
    (void)analysis;
    return true;
}
