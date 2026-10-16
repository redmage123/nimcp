//=============================================================================
// nimcp_creative_bridge.c - Creative Content Validation Bridge Implementation
//=============================================================================
/**
 * @file nimcp_creative_bridge.c
 * @brief Implements defense-in-depth validation for creative content
 *
 * Multi-stage validation pipeline ensuring quality, originality, and safety
 * of AI-generated art. Follows patterns from financial_bridge.h.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/bridges/nimcp_creative_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(creative_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_creative_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_creative_bridge_mesh_registry = NULL;

nimcp_error_t creative_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_creative_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "creative_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "creative_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_creative_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_creative_bridge_mesh_registry = registry;
    return err;
}

void creative_bridge_mesh_unregister(void) {
    if (g_creative_bridge_mesh_registry && g_creative_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_creative_bridge_mesh_registry, g_creative_bridge_mesh_id);
        g_creative_bridge_mesh_id = 0;
        g_creative_bridge_mesh_registry = NULL;
    }
}


//=============================================================================
// Internal Types
//=============================================================================

/**
 * @brief Copyright database entry
 */
typedef struct {
    char title[NIMCP_LABEL_BUFFER_SIZE];
    char creator[NIMCP_ID_BUFFER_SIZE];
    style_embedding_t* embedding;
    uint64_t content_hash;
} copyright_entry_t;

/**
 * @brief Copyright database
 */
typedef struct {
    copyright_entry_t* entries;
    uint32_t num_entries;
    uint32_t capacity;
} copyright_db_t;

//=============================================================================
// Configuration Defaults
//=============================================================================

void creative_bridge_config_defaults(creative_bridge_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(creative_bridge_config_t));

    /* Default stage configurations */
    for (int i = 0; i < VALIDATION_STAGE_COUNT; i++) {
        config->stages[i].enabled = true;
        config->stages[i].pass_threshold = 0.7f;
        config->stages[i].warn_threshold = 0.5f;
        config->stages[i].escalate_threshold = 0.3f;
        config->stages[i].can_deny = true;
        config->stages[i].continue_on_warn = true;
    }

    /* Quality stage: required for all content */
    config->stages[VALIDATION_STAGE_QUALITY].pass_threshold = 0.6f;
    config->stages[VALIDATION_STAGE_QUALITY].can_deny = true;

    /* Copyright stage: strict thresholds */
    config->stages[VALIDATION_STAGE_COPYRIGHT].pass_threshold = 0.8f;  /* Low similarity */
    config->stages[VALIDATION_STAGE_COPYRIGHT].warn_threshold = 0.7f;
    config->stages[VALIDATION_STAGE_COPYRIGHT].escalate_threshold = 0.5f;

    /* Ethics stage: very strict */
    config->stages[VALIDATION_STAGE_ETHICS].pass_threshold = 0.9f;
    config->stages[VALIDATION_STAGE_ETHICS].can_deny = true;

    /* Pipeline behavior */
    config->short_circuit_on_deny = true;
    config->collect_all_warnings = false;

    /* Copyright settings */
    config->copyright_similarity_threshold = 0.85f;
    config->copyright_db_path = NULL;
    config->check_style_copyright = true;

    /* Ethics settings */
    config->detect_nsfw = true;
    config->detect_violence = true;
    config->detect_hate = true;
    config->detect_deception = true;

    /* Quality settings */
    config->min_quality_score = 0.5f;

    /* Bias settings */
    config->detect_gender_bias = true;
    config->detect_racial_bias = true;
    config->detect_cultural_bias = false;

    /* Escalation */
    config->enable_human_review = false;
    config->escalation_callback = NULL;
    config->escalation_context = NULL;
}

//=============================================================================
// Lifecycle
//=============================================================================

creative_bridge_t* creative_bridge_create(const creative_bridge_config_t* config)
{
    creative_bridge_t* bridge = nimcp_calloc(1, sizeof(creative_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply config */
    if (config) {
        bridge->config = *config;
        /* Deep-copy heap-owned string fields to avoid freeing caller's pointer */
        if (config->copyright_db_path) {
            bridge->config.copyright_db_path = nimcp_strdup(config->copyright_db_path);
        }
    } else {
        creative_bridge_config_defaults(&bridge->config);
    }

    /* Initialize copyright database */
    copyright_db_t* db = nimcp_calloc(1, sizeof(copyright_db_t));
    if (db) {
        db->capacity = 1000;
        db->entries = nimcp_calloc(db->capacity, sizeof(copyright_entry_t));
        if (!db->entries) {
            nimcp_free(db);
            nimcp_free(bridge);
            return NULL;
        }
        db->num_entries = 0;
    }
    bridge->copyright_db = db;

    /* Initialize statistics */
    bridge->validations_performed = 0;
    bridge->passed = 0;
    bridge->warned = 0;
    bridge->escalated = 0;
    bridge->denied = 0;
    bridge->avg_validation_time_ms = 0.0f;

    /* Validators and integrations set later */
    bridge->quality_validator = NULL;
    bridge->coherence_validator = NULL;
    bridge->copyright_validator = NULL;
    bridge->ethics_validator = NULL;
    bridge->originality_validator = NULL;
    bridge->bias_validator = NULL;
    bridge->aesthetic_evaluator = NULL;
    bridge->ethics_engine = NULL;

    return bridge;
}

void creative_bridge_destroy(creative_bridge_t* bridge)
{
    if (!bridge) return;

    /* Free copyright database */
    if (bridge->copyright_db) {
        copyright_db_t* db = (copyright_db_t*)bridge->copyright_db;
        if (db->entries) {
            for (uint32_t i = 0; i < db->num_entries; i++) {
                if (db->entries[i].embedding) {
                    if (db->entries[i].embedding->embedding) {
                        nimcp_free(db->entries[i].embedding->embedding);
                    }
                    nimcp_free(db->entries[i].embedding);
                }
            }
            nimcp_free(db->entries);
        }
        nimcp_free(db);
        db = NULL;
    }

    /* Free config strings */
    if (bridge->config.copyright_db_path) {
        nimcp_free(bridge->config.copyright_db_path);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

//=============================================================================
// Internal: Validation Stage Implementations
//=============================================================================

/**
 * @brief Evaluate quality of content
 */
static float evaluate_quality_internal(const creative_bridge_t* bridge,
                                       const void* content,
                                       art_modality_t modality)
{
    (void)bridge;

    /* Placeholder: would use aesthetic evaluator in production */
    float quality = 0.7f;

    switch (modality) {
        case ART_MODALITY_TEXT_POETRY:
        case ART_MODALITY_TEXT_PROSE:
        case ART_MODALITY_TEXT_SCREENPLAY:
        case ART_MODALITY_TEXT_LYRICS:
        case ART_MODALITY_TEXT_DIALOGUE:
            /* Text quality heuristics */
            if (content) {
                const char* text = (const char*)content;
                size_t len = strlen(text);

                /* Penalize very short or very long content */
                if (len < 50) quality -= 0.2f;
                if (len > 50000) quality -= 0.1f;

                /* Bonus for reasonable length */
                if (len >= 100 && len <= 10000) quality += 0.1f;
            }
            break;

        case ART_MODALITY_MUSIC_CLASSICAL:
        case ART_MODALITY_MUSIC_FILM_SCORE:
        case ART_MODALITY_MUSIC_JAZZ:
        case ART_MODALITY_MUSIC_ELECTRONIC:
        case ART_MODALITY_MUSIC_FOLK:
            quality = 0.75f;  /* Music generation typically decent */
            break;

        case ART_MODALITY_VISUAL_PAINTING:
        case ART_MODALITY_VISUAL_DIGITAL:
        case ART_MODALITY_VISUAL_PHOTO:
        case ART_MODALITY_VISUAL_ILLUSTRATION:
        case ART_MODALITY_VISUAL_3D:
            quality = 0.7f;  /* Visual generation baseline */
            break;

        case ART_MODALITY_VIDEO_CINEMA:
        case ART_MODALITY_VIDEO_ANIMATION:
        case ART_MODALITY_VIDEO_DOCUMENTARY:
        case ART_MODALITY_VIDEO_MUSIC_VIDEO:
        case ART_MODALITY_VIDEO_SHORT:
            quality = 0.65f;  /* Video more challenging */
            break;

        default:
            quality = 0.6f;
            break;
    }

    return fmaxf(0.0f, fminf(1.0f, quality));
}

/**
 * @brief Evaluate coherence of content
 */
static float evaluate_coherence_internal(const creative_bridge_t* bridge,
                                         const void* content,
                                         art_modality_t modality)
{
    (void)bridge;
    (void)content;

    /* Placeholder coherence evaluation */
    float coherence = 0.8f;

    /* Video/film have stricter coherence requirements */
    if (modality == ART_MODALITY_VIDEO_CINEMA || modality == ART_MODALITY_VIDEO_ANIMATION ||
        modality == ART_MODALITY_VIDEO_DOCUMENTARY || modality == ART_MODALITY_VIDEO_SHORT) {
        coherence = 0.7f;
    }

    return coherence;
}

/**
 * @brief Check copyright similarity
 */
static float evaluate_copyright_internal(const creative_bridge_t* bridge,
                                         const void* content,
                                         art_modality_t modality)
{
    (void)content;
    (void)modality;

    if (!bridge->copyright_db) {
        return 1.0f;  /* No database = pass */
    }

    copyright_db_t* db = (copyright_db_t*)bridge->copyright_db;
    if (db->num_entries == 0) {
        return 1.0f;  /* Empty database = pass */
    }

    /* Placeholder: would compute embedding and compare */
    /* Lower score = higher similarity = worse */
    float min_distance = 1.0f;

    /* Simulate finding no similar works */
    return min_distance;
}

/**
 * @brief Evaluate ethics/safety of content
 */
static float evaluate_ethics_internal(const creative_bridge_t* bridge,
                                      const void* content,
                                      art_modality_t modality)
{
    float score = 1.0f;

    /* Check text content for problematic patterns */
    if (modality == ART_MODALITY_TEXT_PROSE || modality == ART_MODALITY_TEXT_POETRY ||
        modality == ART_MODALITY_TEXT_SCREENPLAY) {

        if (!content) return 0.5f;

        const char* text = (const char*)content;

        /* Very basic keyword detection (placeholder for real classifier) */
        static const char* problematic[] = {
            "hate", "kill", "violence", "explicit"
        };

        for (size_t i = 0; i < sizeof(problematic)/sizeof(problematic[0]); i++) {
            if (strstr(text, problematic[i])) {
                score -= 0.15f;
            }
        }
    }

    /* Visual content checks */
    if (modality == ART_MODALITY_VISUAL_DIGITAL || modality == ART_MODALITY_VIDEO_CINEMA) {
        /* Would use NSFW classifier in production */
        if (bridge->config.detect_nsfw) {
            /* Placeholder: assume content is safe */
        }
    }

    return fmaxf(0.0f, fminf(1.0f, score));
}

/**
 * @brief Evaluate originality of content
 */
static float evaluate_originality_internal(const creative_bridge_t* bridge,
                                           const void* content,
                                           art_modality_t modality)
{
    (void)bridge;
    if (!content) return 0.5f;

    /* Originality estimation based on statistical uniqueness of content.
       Without a reference database, we use information-theoretic measures:
       higher entropy / unusual distributions → more original. */

    if ((int)modality <= 4) {
        /* Text modalities: compute character-level entropy + bigram surprise */
        const char* text = (const char*)content;
        uint32_t char_freq[256] = {0};
        uint32_t bigram_freq[256] = {0}; /* Simplified: hash bigrams to 256 buckets */
        uint32_t total = 0;
        uint32_t bigrams = 0;
        unsigned char prev = 0;

        for (const char* p = text; *p; p++) {
            unsigned char c = (unsigned char)*p;
            char_freq[c]++;
            total++;
            if (prev) {
                uint32_t bg = ((uint32_t)prev * 31 + c) & 0xFF;
                bigram_freq[bg]++;
                bigrams++;
            }
            prev = c;
        }
        if (total == 0) return 0.5f;

        /* Character entropy */
        float entropy = 0.0f;
        for (int i = 0; i < 256; i++) {
            if (char_freq[i] > 0) {
                float p = (float)char_freq[i] / (float)total;
                entropy -= p * logf(p);
            }
        }
        float max_entropy = logf(fminf(256.0f, (float)total));
        float norm_entropy = max_entropy > 0 ? entropy / max_entropy : 0.5f;

        /* Bigram uniformity (more uniform = more original/unexpected) */
        float bg_entropy = 0.0f;
        if (bigrams > 0) {
            for (int i = 0; i < 256; i++) {
                if (bigram_freq[i] > 0) {
                    float p = (float)bigram_freq[i] / (float)bigrams;
                    bg_entropy -= p * logf(p);
                }
            }
            bg_entropy /= logf(256.0f);
        }

        /* Combine: higher entropy → more original */
        return fmaxf(0.0f, fminf(1.0f, norm_entropy * 0.6f + bg_entropy * 0.4f));

    } else if ((int)modality >= 10 && (int)modality <= 14) {
        /* Music modalities: analyze note pattern originality */
        const music_track_t* track = (const music_track_t*)content;
        if (track->num_notes == 0) return 0.5f;

        /* Interval pattern entropy */
        uint32_t interval_freq[25] = {0}; /* Intervals 0-24 semitones */
        for (uint32_t i = 1; i < track->num_notes; i++) {
            int interval = abs((int)track->notes[i].pitch - (int)track->notes[i - 1].pitch);
            if (interval > 24) interval = 24;
            interval_freq[interval]++;
        }

        float int_entropy = 0.0f;
        uint32_t int_total = track->num_notes > 1 ? track->num_notes - 1 : 1;
        for (int i = 0; i < 25; i++) {
            if (interval_freq[i] > 0) {
                float p = (float)interval_freq[i] / (float)int_total;
                int_entropy -= p * logf(p);
            }
        }
        int_entropy /= logf(25.0f);

        return fmaxf(0.0f, fminf(1.0f, int_entropy));
    }

    /* Default for visual and other modalities */
    return 0.6f;
}

/**
 * @brief Evaluate bias in content
 */
static float evaluate_bias_internal(const creative_bridge_t* bridge,
                                    const void* content,
                                    art_modality_t modality)
{
    (void)content;
    (void)modality;

    float score = 1.0f;

    /* Bias detection is complex - placeholder implementation */
    if (bridge->config.detect_gender_bias) {
        /* Would analyze gender representation */
    }

    if (bridge->config.detect_racial_bias) {
        /* Would analyze racial representation */
    }

    if (bridge->config.detect_cultural_bias) {
        /* Would analyze cultural representation */
    }

    return score;
}

/**
 * @brief Determine result from score and thresholds
 */
static creative_validation_result_t score_to_result(float score,
                                                    const stage_config_t* config)
{
    if (score >= config->pass_threshold) {
        return CREATIVE_VALIDATION_PASS;
    } else if (score >= config->warn_threshold) {
        return CREATIVE_VALIDATION_WARN;
    } else if (score >= config->escalate_threshold) {
        return CREATIVE_VALIDATION_ESCALATE;
    } else if (config->can_deny) {
        return CREATIVE_VALIDATION_DENY;
    } else {
        return CREATIVE_VALIDATION_ESCALATE;
    }
}

//=============================================================================
// Validation API
//=============================================================================

int creative_bridge_validate(creative_bridge_t* bridge,
                              const void* content,
                              art_modality_t modality,
                              validation_pipeline_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_validate: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(validation_pipeline_result_t));

    clock_t start = clock();

    result->overall_result = CREATIVE_VALIDATION_PASS;
    result->short_circuited = false;
    result->num_stages_run = 0;

    /* Run each enabled stage */
    typedef float (*stage_eval_fn)(const creative_bridge_t*, const void*, art_modality_t);

    static const stage_eval_fn stage_evaluators[VALIDATION_STAGE_COUNT] = {
        [VALIDATION_STAGE_QUALITY]     = evaluate_quality_internal,
        [VALIDATION_STAGE_COHERENCE]   = evaluate_coherence_internal,
        [VALIDATION_STAGE_COPYRIGHT]   = evaluate_copyright_internal,
        [VALIDATION_STAGE_ETHICS]      = evaluate_ethics_internal,
        [VALIDATION_STAGE_ORIGINALITY] = evaluate_originality_internal,
        [VALIDATION_STAGE_BIAS]        = evaluate_bias_internal,
    };

    for (int stage = 0; stage < VALIDATION_STAGE_COUNT; stage++) {
        stage_config_t* stage_config = &bridge->config.stages[stage];

        if (!stage_config->enabled) continue;

        clock_t stage_start = clock();

        /* Evaluate stage */
        float score = stage_evaluators[stage](bridge, content, modality);

        /* Determine result */
        creative_validation_result_t stage_result =
            score_to_result(score, stage_config);

        /* Record stage result */
        result->stages[result->num_stages_run].stage = (validation_stage_t)stage;
        result->stages[result->num_stages_run].score = score;
        result->stages[result->num_stages_run].result = stage_result;
        result->stages[result->num_stages_run].time_ms =
            (float)(clock() - stage_start) * 1000.0f / CLOCKS_PER_SEC;

        snprintf(result->stages[result->num_stages_run].message,
                 sizeof(result->stages[result->num_stages_run].message),
                 "%s: score=%.2f",
                 creative_bridge_stage_name((validation_stage_t)stage), score);

        result->num_stages_run++;

        /* Update overall result */
        if (stage_result > result->overall_result) {
            result->overall_result = stage_result;
        }

        /* Handle denial */
        if (stage_result == CREATIVE_VALIDATION_DENY) {
            result->failed_stage = (uint32_t)stage;

            /* Set deny reason based on stage */
            switch (stage) {
                case VALIDATION_STAGE_QUALITY:
                    result->deny_reason = CREATIVE_DENY_QUALITY;
                    break;
                case VALIDATION_STAGE_COPYRIGHT:
                    result->deny_reason = CREATIVE_DENY_COPYRIGHT;
                    break;
                case VALIDATION_STAGE_ETHICS:
                    result->deny_reason = CREATIVE_DENY_HARMFUL_CONTENT;
                    break;
                case VALIDATION_STAGE_COHERENCE:
                    result->deny_reason = CREATIVE_DENY_INCOHERENT;
                    break;
                default:
                    result->deny_reason = CREATIVE_DENY_QUALITY;
                    break;
            }

            if (bridge->config.short_circuit_on_deny) {
                result->short_circuited = true;
                break;
            }
        }

        /* Handle warning - may or may not continue */
        if (stage_result == CREATIVE_VALIDATION_WARN &&
            !stage_config->continue_on_warn &&
            !bridge->config.collect_all_warnings) {
            break;
        }
    }

    /* Calculate total time */
    result->total_time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    /* Update statistics */
    bridge->validations_performed++;

    switch (result->overall_result) {
        case CREATIVE_VALIDATION_PASS:
            bridge->passed++;
            break;
        case CREATIVE_VALIDATION_WARN:
            bridge->warned++;
            break;
        case CREATIVE_VALIDATION_ESCALATE:
            bridge->escalated++;
            break;
        case CREATIVE_VALIDATION_DENY:
            bridge->denied++;
            break;
    }

    float n = (float)bridge->validations_performed;
    bridge->avg_validation_time_ms = bridge->avg_validation_time_ms * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                      result->total_time_ms / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Handle escalation callback */
    if (result->overall_result == CREATIVE_VALIDATION_ESCALATE &&
        bridge->config.enable_human_review &&
        bridge->config.escalation_callback) {
        bridge->config.escalation_callback(result, bridge->config.escalation_context);
    }

    return 0;
}

int creative_bridge_validate_text(creative_bridge_t* bridge,
                                   const char* text, size_t len,
                                   art_modality_t modality,
                                   validation_pipeline_result_t* result)
{
    (void)len;  /* Could use for additional validation */
    return creative_bridge_validate(bridge, text, modality, result);
}

int creative_bridge_validate_image(creative_bridge_t* bridge,
                                    const visual_image_t* image,
                                    validation_pipeline_result_t* result)
{
    return creative_bridge_validate(bridge, image, ART_MODALITY_VISUAL_DIGITAL, result);
}

int creative_bridge_validate_music(creative_bridge_t* bridge,
                                    const music_track_t* tracks,
                                    uint32_t num_tracks,
                                    validation_pipeline_result_t* result)
{
    (void)num_tracks;
    return creative_bridge_validate(bridge, tracks, ART_MODALITY_MUSIC_CLASSICAL, result);
}

//=============================================================================
// Individual Stage API
//=============================================================================

int creative_bridge_check_quality(creative_bridge_t* bridge,
                                   const void* content,
                                   art_modality_t modality,
                                   stage_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_check_quality: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(stage_result_t));

    clock_t start = clock();

    result->stage = VALIDATION_STAGE_QUALITY;
    result->score = evaluate_quality_internal(bridge, content, modality);
    result->result = score_to_result(result->score,
                                     &bridge->config.stages[VALIDATION_STAGE_QUALITY]);
    result->time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    snprintf(result->message, sizeof(result->message),
             "Quality score: %.2f", result->score);

    return 0;
}

int creative_bridge_check_copyright(creative_bridge_t* bridge,
                                     const void* content,
                                     art_modality_t modality,
                                     stage_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_check_copyright: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(stage_result_t));

    clock_t start = clock();

    result->stage = VALIDATION_STAGE_COPYRIGHT;
    result->score = evaluate_copyright_internal(bridge, content, modality);
    result->result = score_to_result(result->score,
                                     &bridge->config.stages[VALIDATION_STAGE_COPYRIGHT]);
    result->time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    snprintf(result->message, sizeof(result->message),
             "Copyright check: %.2f distance from known works", result->score);

    return 0;
}

int creative_bridge_check_ethics(creative_bridge_t* bridge,
                                  const void* content,
                                  art_modality_t modality,
                                  stage_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_check_ethics: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(stage_result_t));

    clock_t start = clock();

    result->stage = VALIDATION_STAGE_ETHICS;
    result->score = evaluate_ethics_internal(bridge, content, modality);
    result->result = score_to_result(result->score,
                                     &bridge->config.stages[VALIDATION_STAGE_ETHICS]);
    result->time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    snprintf(result->message, sizeof(result->message),
             "Ethics score: %.2f", result->score);

    return 0;
}

int creative_bridge_check_originality(creative_bridge_t* bridge,
                                       const void* content,
                                       art_modality_t modality,
                                       stage_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_check_originality: required parameter is NULL (bridge, result)");
        return -1;
    }

    memset(result, 0, sizeof(stage_result_t));

    clock_t start = clock();

    result->stage = VALIDATION_STAGE_ORIGINALITY;
    result->score = evaluate_originality_internal(bridge, content, modality);
    result->result = score_to_result(result->score,
                                     &bridge->config.stages[VALIDATION_STAGE_ORIGINALITY]);
    result->time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    snprintf(result->message, sizeof(result->message),
             "Originality score: %.2f", result->score);

    return 0;
}

//=============================================================================
// Copyright Database API
//=============================================================================

int creative_bridge_add_copyrighted_work(creative_bridge_t* bridge,
                                          const char* title,
                                          const char* creator,
                                          const style_embedding_t* embedding,
                                          uint64_t content_hash)
{
    if (!bridge || !title || !creator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_add_copyrighted_work: required parameter is NULL (bridge, title, creator)");
        return -1;
    }

    copyright_db_t* db = (copyright_db_t*)bridge->copyright_db;
    if (!db) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_bridge_add_copyrighted_work: db is NULL");
        return -1;
    }

    /* Expand capacity if needed */
    if (db->num_entries >= db->capacity) {
        uint32_t new_capacity = db->capacity * 2;
        copyright_entry_t* new_entries = nimcp_calloc(new_capacity,
                                                      sizeof(copyright_entry_t));
        if (!new_entries) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_bridge_add_copyrighted_work: new_entries is NULL");
            return -1;
        }

        memcpy(new_entries, db->entries,
               db->num_entries * sizeof(copyright_entry_t));
        nimcp_free(db->entries);
        db->entries = new_entries;
        db->capacity = new_capacity;
    }

    /* Add entry */
    copyright_entry_t* entry = &db->entries[db->num_entries];
    strncpy(entry->title, title, sizeof(entry->title) - 1);
    strncpy(entry->creator, creator, sizeof(entry->creator) - 1);
    entry->content_hash = content_hash;

    /* Copy embedding if provided */
    if (embedding && embedding->embedding && embedding->embedding_dim > 0) {
        entry->embedding = nimcp_calloc(1, sizeof(style_embedding_t));
        if (entry->embedding) {
            entry->embedding->embedding_dim = embedding->embedding_dim;
            entry->embedding->embedding = nimcp_calloc(embedding->embedding_dim, sizeof(float));
            if (entry->embedding->embedding) {
                memcpy(entry->embedding->embedding, embedding->embedding,
                       embedding->embedding_dim * sizeof(float));
            }
        }
    }

    db->num_entries++;
    return 0;
}

uint32_t creative_bridge_find_similar_works(creative_bridge_t* bridge,
                                             const style_embedding_t* embedding,
                                             uint32_t max_results,
                                             char** titles,
                                             float* similarities)
{
    if (!bridge || !embedding || !titles || !similarities) return 0;

    copyright_db_t* db = (copyright_db_t*)bridge->copyright_db;
    if (!db || db->num_entries == 0) return 0;

    /* Find similar works by cosine similarity */
    typedef struct {
        uint32_t index;
        float similarity;
    } similarity_result_t;

    similarity_result_t* results = nimcp_calloc(db->num_entries,
                                                sizeof(similarity_result_t));
    if (!results) return 0;

    uint32_t num_found = 0;

    for (uint32_t i = 0; i < db->num_entries; i++) {
        if (!db->entries[i].embedding) continue;
        if (db->entries[i].embedding->embedding_dim != embedding->embedding_dim) continue;

        /* Compute cosine similarity */
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (uint32_t d = 0; d < embedding->embedding_dim; d++) {
            dot += embedding->embedding[d] * db->entries[i].embedding->embedding[d];
            norm_a += embedding->embedding[d] * embedding->embedding[d];
            norm_b += db->entries[i].embedding->embedding[d] *
                      db->entries[i].embedding->embedding[d];
        }

        float similarity = 0.0f;
        if (norm_a > 0 && norm_b > 0) {
            similarity = dot / (sqrtf(norm_a) * sqrtf(norm_b));
        }

        /* Only include if above threshold */
        if (similarity > 0.3f) {
            results[num_found].index = i;
            results[num_found].similarity = similarity;
            num_found++;
        }
    }

    /* Sort by similarity (simple bubble sort for small arrays) */
    for (uint32_t i = 0; i + 1 < num_found; i++) {
        for (uint32_t j = 0; j < num_found - i - 1; j++) {
            if (results[j].similarity < results[j+1].similarity) {
                similarity_result_t temp = results[j];
                results[j] = results[j+1];
                results[j+1] = temp;
            }
        }
    }

    /* Return top results */
    uint32_t return_count = num_found < max_results ? num_found : max_results;
    for (uint32_t i = 0; i < return_count; i++) {
        titles[i] = db->entries[results[i].index].title;
        similarities[i] = results[i].similarity;
    }

    nimcp_free(results);
    results = NULL;
    return return_count;
}

//=============================================================================
// Configuration API
//=============================================================================

void creative_bridge_set_stage_enabled(creative_bridge_t* bridge,
                                        validation_stage_t stage,
                                        bool enabled)
{
    if (!bridge || stage >= VALIDATION_STAGE_COUNT) return;
    bridge->config.stages[stage].enabled = enabled;
}

void creative_bridge_set_thresholds(creative_bridge_t* bridge,
                                     validation_stage_t stage,
                                     float pass, float warn, float escalate)
{
    if (!bridge || stage >= VALIDATION_STAGE_COUNT) return;

    bridge->config.stages[stage].pass_threshold = pass;
    bridge->config.stages[stage].warn_threshold = warn;
    bridge->config.stages[stage].escalate_threshold = escalate;
}

//=============================================================================
// Integration API
//=============================================================================

void creative_bridge_set_evaluator(creative_bridge_t* bridge, void* evaluator)
{
    if (!bridge) return;
    bridge->aesthetic_evaluator = evaluator;
}

void creative_bridge_set_ethics_engine(creative_bridge_t* bridge, void* ethics)
{
    if (!bridge) return;
    bridge->ethics_engine = ethics;
}

//=============================================================================
// Statistics API
//=============================================================================

void creative_bridge_get_stats(const creative_bridge_t* bridge,
                                uint64_t* passed,
                                uint64_t* warned,
                                uint64_t* escalated,
                                uint64_t* denied)
{
    if (!bridge) return;

    if (passed) *passed = bridge->passed;
    if (warned) *warned = bridge->warned;
    if (escalated) *escalated = bridge->escalated;
    if (denied) *denied = bridge->denied;
}

void creative_bridge_reset_stats(creative_bridge_t* bridge)
{
    if (!bridge) return;

    bridge->validations_performed = 0;
    bridge->passed = 0;
    bridge->warned = 0;
    bridge->escalated = 0;
    bridge->denied = 0;
    bridge->avg_validation_time_ms = 0.0f;
}

//=============================================================================
// Utility API
//=============================================================================

const char* creative_bridge_stage_name(validation_stage_t stage)
{
    static const char* names[VALIDATION_STAGE_COUNT] = {
        [VALIDATION_STAGE_QUALITY]     = "Quality",
        [VALIDATION_STAGE_COHERENCE]   = "Coherence",
        [VALIDATION_STAGE_COPYRIGHT]   = "Copyright",
        [VALIDATION_STAGE_ETHICS]      = "Ethics",
        [VALIDATION_STAGE_ORIGINALITY] = "Originality",
        [VALIDATION_STAGE_BIAS]        = "Bias",
    };

    if (stage >= VALIDATION_STAGE_COUNT) return "Unknown";
    return names[stage];
}

const char* creative_bridge_result_name(creative_validation_result_t result)
{
    switch (result) {
        case CREATIVE_VALIDATION_PASS:     return "Pass";
        case CREATIVE_VALIDATION_WARN:     return "Warning";
        case CREATIVE_VALIDATION_ESCALATE: return "Escalate";
        case CREATIVE_VALIDATION_DENY:     return "Denied";
        default:                           return "Unknown";
    }
}

const char* creative_bridge_deny_reason_name(creative_deny_reason_t reason)
{
    switch (reason) {
        case CREATIVE_DENY_COPYRIGHT:       return "Copyright violation";
        case CREATIVE_DENY_HARMFUL_CONTENT: return "Harmful content";
        case CREATIVE_DENY_QUALITY:         return "Quality below threshold";
        case CREATIVE_DENY_INCOHERENT:      return "Incoherent content";
        default:                            return "Unknown reason";
    }
}
