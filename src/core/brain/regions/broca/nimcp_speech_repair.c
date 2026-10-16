/**
 * @file nimcp_speech_repair.c
 * @brief Speech repair processor implementation
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "core/brain/regions/broca/nimcp_speech_repair.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(speech_repair, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct speech_repair {
    speech_repair_config_t config;
    repair_status_t status;
    repair_error_t last_error;
    repair_stats_t stats;

    repair_instance_t* repair_history;
    uint32_t repair_count;
    uint32_t next_repair_id;

    bio_router_t* router;
    bool bio_registered;
};

/*=============================================================================
 * FILLER WORDS
 *===========================================================================*/

static const char* FILLER_WORDS[] = {
    "um", "uh", "er", "ah", "eh", "mm", "hmm",
    "like", "you know", "i mean", "sort of", "kind of",
    NULL
};

static const char* REPAIR_CUE_PHRASES[] = {
    "i mean", "sorry", "no wait", "actually", "rather",
    "or rather", "that is", "well",
    NULL
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static bool str_contains_word(const char* text, const char* word) {
    if (!text || !word) {
        return false; /* NULL inputs to helper — just return false */
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

static void str_to_lower(const char* src, char* dst, size_t size) {
    size_t i;
    for (i = 0; i < size - 1 && src[i]; i++) {
        dst[i] = tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

speech_repair_config_t speech_repair_default_config(void) {
    speech_repair_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_repairs = REPAIR_DEFAULT_MAX_REPAIRS;
    config.history_size = REPAIR_DEFAULT_HISTORY_SIZE;
    config.detection_window_ms = REPAIR_DEFAULT_DETECTION_WINDOW_MS;
    config.pause_threshold_ms = 200.0f;
    config.enable_auto_correction = true;
    config.enable_bio_async = false;
    config.preserve_disfluencies = false;

    return config;
}

speech_repair_t* speech_repair_create(const speech_repair_config_t* config) {
    speech_repair_t* processor = (speech_repair_t*)nimcp_calloc(1, sizeof(speech_repair_t));
    if (!processor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_repair_create: failed to allocate processor");

        return NULL;

    }

    if (config) {
        processor->config = *config;
    } else {
        processor->config = speech_repair_default_config();
    }

    processor->repair_history = (repair_instance_t*)nimcp_calloc(
        processor->config.max_repairs, sizeof(repair_instance_t));
    if (!processor->repair_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate repair history");
        nimcp_free(processor);
        return NULL;
    }

    processor->status = REPAIR_STATUS_IDLE;
    processor->next_repair_id = 1;

    return processor;
}

void speech_repair_destroy(speech_repair_t* processor) {
    if (!processor) return;
    nimcp_free(processor->repair_history);
    nimcp_free(processor);
}

bool speech_repair_reset(speech_repair_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    processor->repair_count = 0;
    processor->status = REPAIR_STATUS_IDLE;
    processor->last_error = REPAIR_ERROR_NONE;

    return true;
}

/*=============================================================================
 * DISFLUENCY DETECTION
 *===========================================================================*/

uint32_t speech_repair_detect_disfluencies(
    speech_repair_t* processor,
    const char* utterance,
    disfluency_t* disfluencies,
    uint32_t max_disfluencies) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0;
    }
    if (!disfluencies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "disfluencies is NULL");
        return 0;
    }
    if (max_disfluencies == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_disfluencies is zero");
        return 0;
    }

    processor->status = REPAIR_STATUS_DETECTING;
    uint32_t count = 0;

    char lower[NIMCP_ERROR_BUFFER_LARGE];
    str_to_lower(utterance, lower, sizeof(lower));

    /* Check for filled pauses */
    for (const char** filler = FILLER_WORDS; *filler && count < max_disfluencies; filler++) {
        if (str_contains_word(lower, *filler)) {
            disfluency_t* d = &disfluencies[count++];
            d->type = DISFLUENCY_FILLED_PAUSE;

            /* Find position */
            const char* pos = strstr(lower, *filler);
            d->position = (uint32_t)(pos - lower);
            d->length = (uint32_t)strlen(*filler);
            strncpy(d->content, *filler, sizeof(d->content) - 1);
            d->confidence = 0.9f;

            processor->stats.disfluencies_detected++;
        }
    }

    /* Check for word fragments (words ending with -) */
    const char* p = utterance;
    while (*p && count < max_disfluencies) {
        if (*p == '-' && p > utterance && isalpha((unsigned char)*(p - 1))) {
            disfluency_t* d = &disfluencies[count++];
            d->type = DISFLUENCY_WORD_FRAGMENT;
            d->position = (uint32_t)(p - utterance);
            d->length = 1;
            d->content[0] = '-';
            d->content[1] = '\0';
            d->confidence = 0.85f;

            processor->stats.disfluencies_detected++;
        }
        p++;
    }

    processor->status = REPAIR_STATUS_READY;
    return count;
}

bool speech_repair_is_filler(const char* word) {
    if (!word) {
        return false;
    }

    char lower[NIMCP_ID_BUFFER_SIZE];
    str_to_lower(word, lower, sizeof(lower));

    for (const char** filler = FILLER_WORDS; *filler; filler++) {
        if (strcmp(lower, *filler) == 0) return true;
    }
    return false;
}

const char* speech_repair_disfluency_name(disfluency_type_t type) {
    static const char* names[] = {
        "NONE", "FILLED_PAUSE", "SILENT_PAUSE", "LENGTHENING",
        "WORD_FRAGMENT", "REPETITION", "FALSE_START"
    };
    if (type >= DISFLUENCY_COUNT) return "INVALID";
    return names[type];
}

/*=============================================================================
 * REPAIR DETECTION
 *===========================================================================*/

uint32_t speech_repair_detect_repairs(
    speech_repair_t* processor,
    const char* utterance,
    repair_instance_t* repairs,
    uint32_t max_repairs) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return 0;
    }
    if (!repairs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "repairs is NULL");
        return 0;
    }
    if (max_repairs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_repairs is zero");
        return 0;
    }

    processor->status = REPAIR_STATUS_DETECTING;
    uint32_t count = 0;

    char lower[NIMCP_ERROR_BUFFER_LARGE];
    str_to_lower(utterance, lower, sizeof(lower));

    /* Check for repair cue phrases */
    for (const char** cue = REPAIR_CUE_PHRASES; *cue && count < max_repairs; cue++) {
        if (str_contains_word(lower, *cue)) {
            repair_instance_t* r = &repairs[count++];
            r->repair_id = processor->next_repair_id++;
            r->type = REPAIR_TYPE_CORRECTION;

            /* Find cue position */
            const char* pos = strstr(lower, *cue);
            r->edit_signal = EDIT_SIGNAL_CUE_PHRASE;
            strncpy(r->edit_content, *cue, sizeof(r->edit_content) - 1);

            /* Everything before cue is reparandum */
            size_t rep_len = pos - lower;
            if (rep_len > REPAIR_MAX_REPARANDUM_LENGTH - 1) {
                rep_len = REPAIR_MAX_REPARANDUM_LENGTH - 1;
            }
            strncpy(r->reparandum, utterance, rep_len);
            r->reparandum[rep_len] = '\0';
            r->reparandum_start = 0;
            r->reparandum_end = (uint32_t)rep_len;

            /* Everything after cue is alteration */
            const char* alt_start = pos + strlen(*cue);
            while (*alt_start == ' ') alt_start++;
            strncpy(r->alteration, alt_start, sizeof(r->alteration) - 1);
            r->alteration_start = (uint32_t)(alt_start - lower);
            r->alteration_end = (uint32_t)strlen(lower);

            r->confidence = 0.8f;
            processor->stats.repairs_detected++;
            processor->stats.repair_type_counts[r->type]++;
        }
    }

    /* Check for repetition patterns (word word) */
    const char* word_start = utterance;
    while (*word_start && count < max_repairs) {
        /* Skip non-alpha */
        while (*word_start && !isalpha((unsigned char)*word_start)) word_start++;
        if (!*word_start) break;

        /* Find word end */
        const char* word_end = word_start;
        while (*word_end && isalpha((unsigned char)*word_end)) word_end++;

        size_t word_len = word_end - word_start;
        if (word_len > 0 && word_len < 32) {
            /* Check if next word is same */
            const char* next_word = word_end;
            while (*next_word && !isalpha((unsigned char)*next_word)) next_word++;

            if (*next_word && strncmp(word_start, next_word, word_len) == 0 &&
                !isalpha((unsigned char)next_word[word_len])) {
                repair_instance_t* r = &repairs[count++];
                r->repair_id = processor->next_repair_id++;
                r->type = REPAIR_TYPE_REPETITION;

                strncpy(r->reparandum, word_start, word_len);
                r->reparandum[word_len] = '\0';
                r->reparandum_start = (uint32_t)(word_start - utterance);
                r->reparandum_end = (uint32_t)(word_end - utterance);

                strncpy(r->alteration, next_word, word_len);
                r->alteration[word_len] = '\0';

                r->edit_signal = EDIT_SIGNAL_NONE;
                r->confidence = 0.9f;

                processor->stats.repairs_detected++;
                processor->stats.repair_type_counts[r->type]++;
            }
        }

        word_start = word_end;
    }

    processor->status = REPAIR_STATUS_READY;
    return count;
}

const char* speech_repair_type_name(repair_type_t type) {
    static const char* names[] = {
        "NONE", "RESTART", "REPLACEMENT", "INSERTION",
        "DELETION", "CORRECTION", "REPETITION"
    };
    if (type >= REPAIR_TYPE_COUNT) return "INVALID";
    return names[type];
}

/*=============================================================================
 * FULL ANALYSIS
 *===========================================================================*/

bool speech_repair_analyze(
    speech_repair_t* processor,
    const char* utterance,
    repair_analysis_t* analysis) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return false;
    }
    if (!analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis is NULL");
        return false;
    }

    memset(analysis, 0, sizeof(repair_analysis_t));

    /* Detect disfluencies */
    analysis->disfluency_count = speech_repair_detect_disfluencies(
        processor, utterance, analysis->disfluencies, 8);

    /* Detect repairs */
    analysis->repair_count = speech_repair_detect_repairs(
        processor, utterance, analysis->repairs, 4);

    /* Generate cleaned output if enabled */
    if (processor->config.enable_auto_correction) {
        speech_repair_clean(processor, utterance, analysis->cleaned_output,
                           sizeof(analysis->cleaned_output));
        analysis->has_cleaned_output = true;
    }

    /* Calculate fluency score */
    size_t word_count = 0;
    const char* p = utterance;
    while (*p) {
        while (*p && !isalpha((unsigned char)*p)) p++;
        if (*p) {
            word_count++;
            while (*p && isalpha((unsigned char)*p)) p++;
        }
    }

    if (word_count > 0) {
        float disfluency_penalty = (float)analysis->disfluency_count / word_count * 0.3f;
        float repair_penalty = (float)analysis->repair_count / word_count * 0.5f;
        analysis->fluency_score = 1.0f - disfluency_penalty - repair_penalty;
        if (analysis->fluency_score < 0.0f) analysis->fluency_score = 0.0f;

        analysis->repair_rate = (float)analysis->repair_count / word_count * 100.0f;

        processor->stats.avg_fluency_score =
            (processor->stats.avg_fluency_score * processor->stats.utterances_processed +
             analysis->fluency_score) / (processor->stats.utterances_processed + 1);
    } else {
        analysis->fluency_score = 1.0f;
    }

    processor->stats.utterances_processed++;
    return true;
}

bool speech_repair_clean(
    speech_repair_t* processor,
    const char* utterance,
    char* cleaned,
    size_t cleaned_size) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return false;
    }
    if (!cleaned) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cleaned buffer is NULL");
        return false;
    }
    if (cleaned_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cleaned_size is zero");
        return false;
    }

    /* Simple cleaning: remove filler words */
    char lower[NIMCP_ERROR_BUFFER_LARGE];
    str_to_lower(utterance, lower, sizeof(lower));

    /* Copy while skipping fillers */
    size_t out_pos = 0;
    const char* p = utterance;

    while (*p && out_pos < cleaned_size - 1) {
        bool is_filler = false;

        /* Check if current position starts a filler */
        for (const char** filler = FILLER_WORDS; *filler; filler++) {
            size_t filler_len = strlen(*filler);
            char temp[NIMCP_ID_BUFFER_SIZE];
            strncpy(temp, p, filler_len);
            temp[filler_len] = '\0';

            char temp_lower[NIMCP_ID_BUFFER_SIZE];
            str_to_lower(temp, temp_lower, sizeof(temp_lower));

            if (strcmp(temp_lower, *filler) == 0) {
                /* Check word boundaries */
                bool start_ok = (p == utterance) || !isalnum((unsigned char)*(p - 1));
                bool end_ok = !isalnum((unsigned char)p[filler_len]);

                if (start_ok && end_ok) {
                    is_filler = true;
                    p += filler_len;
                    /* Skip trailing space */
                    while (*p == ' ') p++;
                    processor->stats.auto_corrections_made++;
                    break;
                }
            }
        }

        if (!is_filler) {
            cleaned[out_pos++] = *p++;
        }
    }

    cleaned[out_pos] = '\0';

    /* Clean up multiple spaces */
    char* dst = cleaned;
    const char* src = cleaned;
    bool prev_space = false;
    while (*src) {
        if (*src == ' ') {
            if (!prev_space) {
                *dst++ = *src;
            }
            prev_space = true;
        } else {
            *dst++ = *src;
            prev_space = false;
        }
        src++;
    }
    *dst = '\0';

    return true;
}

/*=============================================================================
 * REPAIR GENERATION
 *===========================================================================*/

bool speech_repair_generate_correction(
    speech_repair_t* processor,
    const char* original,
    const char* correction,
    repair_type_t type,
    char* output,
    size_t output_size) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!original) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "original is NULL");
        return false;
    }
    if (!correction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "correction is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output buffer is NULL");
        return false;
    }
    if (output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "output_size is zero");
        return false;
    }

    switch (type) {
        case REPAIR_TYPE_CORRECTION:
            snprintf(output, output_size, "%s, I mean, %s", original, correction);
            break;

        case REPAIR_TYPE_REPLACEMENT:
            snprintf(output, output_size, "%s, no, %s", original, correction);
            break;

        case REPAIR_TYPE_RESTART:
            snprintf(output, output_size, "%s... %s", original, correction);
            break;

        default:
            snprintf(output, output_size, "%s %s", original, correction);
            break;
    }

    return true;
}

bool speech_repair_insert_hesitation(
    speech_repair_t* processor,
    const char* input,
    uint32_t position,
    disfluency_type_t type,
    char* output,
    size_t output_size) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "input is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output buffer is NULL");
        return false;
    }
    if (output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "output_size is zero");
        return false;
    }

    const char* hesitation = "um";
    switch (type) {
        case DISFLUENCY_FILLED_PAUSE: hesitation = "um"; break;
        case DISFLUENCY_SILENT_PAUSE: hesitation = "..."; break;
        case DISFLUENCY_LENGTHENING: hesitation = "-"; break;
        default: hesitation = "um"; break;
    }

    size_t input_len = strlen(input);
    if (position > input_len) position = (uint32_t)input_len;

    size_t needed = input_len + strlen(hesitation) + 2;  // +2 for surrounding spaces
    if (needed >= output_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: capacity exceeded");
        return false;
    }

    int written = snprintf(output, output_size, "%.*s %s %s",
                           (int)position, input, hesitation, input + position);
    if (written < 0 || (size_t)written >= output_size) {
        output[output_size - 1] = '\0';
    }

    return true;
}

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

repair_status_t speech_repair_get_status(const speech_repair_t* processor) {
    if (!processor) return REPAIR_STATUS_ERROR;
    return processor->status;
}

repair_error_t speech_repair_get_last_error(const speech_repair_t* processor) {
    if (!processor) return REPAIR_ERROR_INTERNAL;
    return processor->last_error;
}

bool speech_repair_get_stats(const speech_repair_t* processor, repair_stats_t* stats) {
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

void speech_repair_reset_stats(speech_repair_t* processor) {
    if (!processor) return;
    memset(&processor->stats, 0, sizeof(repair_stats_t));
}

bool speech_repair_get_config(const speech_repair_t* processor, speech_repair_config_t* config) {
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

bool speech_repair_register_bio_handler(
    speech_repair_t* processor,
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
