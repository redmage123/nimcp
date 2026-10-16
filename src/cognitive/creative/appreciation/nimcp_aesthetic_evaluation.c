//=============================================================================
// nimcp_aesthetic_evaluation.c - Aesthetic Quality Evaluation Implementation
//=============================================================================
/**
 * @file nimcp_aesthetic_evaluation.c
 * @brief Implementation of aesthetic quality evaluation
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/appreciation/nimcp_aesthetic_evaluation.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define LOG_MODULE "AESTHETIC_EVAL"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(aesthetic_evaluation, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Configuration Defaults
//=============================================================================

void aesthetic_evaluator_config_defaults(aesthetic_evaluator_config_t* config) {
    if (!config) return;

    /* Dimension weights (balanced) */
    config->novelty_weight = 0.2f;
    config->complexity_weight = 0.2f;
    config->familiarity_weight = 0.2f;
    config->hedonic_weight = 0.2f;
    config->arousal_weight = 0.2f;

    /* Quality thresholds */
    config->min_quality_threshold = 0.3f;
    config->max_complexity_preference = 0.7f;
    config->optimal_novelty = 0.6f;

    /* Enable all modalities */
    config->enable_text_analysis = true;
    config->enable_music_analysis = true;
    config->enable_visual_analysis = true;

    /* Integration flags */
    config->use_emotion_system = false;
    config->use_memory_system = false;

    /* Resource limits */
    config->max_analysis_time_ms = 1000;
}

//=============================================================================
// Lifecycle API
//=============================================================================

aesthetic_evaluator_t* aesthetic_evaluator_create(
    const aesthetic_evaluator_config_t* config) {

    aesthetic_evaluator_t* eval = nimcp_calloc(1, sizeof(aesthetic_evaluator_t));
    if (!eval) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate aesthetic evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "aesthetic_evaluator_config_defaults: eval is NULL");
        return NULL;
    }

    /* Apply config */
    if (config) {
        memcpy(&eval->config, config, sizeof(aesthetic_evaluator_config_t));
    } else {
        aesthetic_evaluator_config_defaults(&eval->config);
    }

    /* Initialize statistics */
    eval->total_evaluations = 0;
    eval->avg_quality_score = 0.0f;
    eval->last_evaluation_us = 0;

    LOG_INFO(LOG_MODULE, "Aesthetic evaluator created");
    return eval;
}

void aesthetic_evaluator_destroy(aesthetic_evaluator_t* eval) {
    if (!eval) return;

    /* Free any allocated models - currently stubs */

    nimcp_free(eval);
    eval = NULL;
    LOG_INFO(LOG_MODULE, "Aesthetic evaluator destroyed");
}

//=============================================================================
// Evaluation API - Stub implementations
//=============================================================================

int aesthetic_evaluate_text(aesthetic_evaluator_t* eval,
                            const char* text, size_t len,
                            art_modality_t modality,
                            aesthetic_evaluation_t* out) {
    if (!eval || !text || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "aesthetic_evaluator_destroy: required parameter is NULL (eval, text, out)");
        return -1;
    }

    memset(out, 0, sizeof(aesthetic_evaluation_t));

    /* Real text aesthetic analysis based on linguistic features */

    /* 1. Vocabulary diversity (type-token ratio proxy): count unique chars / total */
    uint32_t char_freq[256] = {0};
    uint32_t total_chars = 0;
    uint32_t unique_chars = 0;
    uint32_t word_count = 0;
    uint32_t sentence_count = 0;
    uint32_t total_word_len = 0;
    bool in_word = false;

    for (size_t i = 0; i < len && text[i]; i++) {
        unsigned char c = (unsigned char)text[i];
        char_freq[c]++;
        total_chars++;

        if (c == '.' || c == '!' || c == '?') sentence_count++;

        if (c == ' ' || c == '\n' || c == '\t') {
            if (in_word) { word_count++; in_word = false; }
        } else {
            if (!in_word) in_word = true;
            total_word_len++;
        }
    }
    if (in_word) word_count++;
    if (sentence_count == 0) sentence_count = 1;

    for (int i = 0; i < 256; i++)
        if (char_freq[i] > 0) unique_chars++;

    /* 2. Compute aesthetic dimensions */

    /* Novelty: character entropy (Shannon) normalized to [0,1] */
    float entropy = 0.0f;
    if (total_chars > 0) {
        for (int i = 0; i < 256; i++) {
            if (char_freq[i] > 0) {
                float p = (float)char_freq[i] / (float)total_chars;
                entropy -= p * logf(p);
            }
        }
        entropy /= logf(256.0f); /* Normalize to [0,1] */
    }
    out->dimensions.novelty = fminf(1.0f, entropy);

    /* Complexity: average sentence length + avg word length */
    float avg_sent_len = (float)word_count / (float)sentence_count;
    float avg_word_len = word_count > 0 ? (float)total_word_len / (float)word_count : 0.0f;
    /* Flesch-Kincaid grade level proxy: higher = more complex */
    float complexity_raw = 0.39f * avg_sent_len + 11.8f * (avg_word_len / 5.0f) - 15.59f;
    out->dimensions.complexity = fmaxf(0.0f, fminf(1.0f, complexity_raw / 20.0f));

    /* Familiarity: inverse of unique char ratio (common text ≈ familiar) */
    float unique_ratio = total_chars > 0 ? (float)unique_chars / (float)total_chars : 0.5f;
    out->dimensions.familiarity = 1.0f - fminf(1.0f, unique_ratio * 3.0f);

    /* Hedonic tone: punctuation expressiveness [-1, +1] */
    float excl = (float)char_freq['!'];
    float quest = (float)char_freq['?'];
    float period = (float)(char_freq['.'] > 0 ? char_freq['.'] : 1);
    float expressive = (excl + quest) / (period + excl + quest);
    out->dimensions.hedonic_tone = fmaxf(-1.0f, fminf(1.0f, expressive * 2.0f - 0.5f));

    /* Arousal: variety of punctuation + capitalization rate */
    float caps = 0.0f;
    for (int i = 'A'; i <= 'Z'; i++) caps += char_freq[i];
    float alpha = caps;
    for (int i = 'a'; i <= 'z'; i++) alpha += char_freq[i];
    float cap_rate = alpha > 0 ? caps / alpha : 0.0f;
    out->dimensions.arousal_potential = fminf(1.0f, cap_rate * 3.0f + expressive * 0.5f);

    /* Wundt curve: optimal complexity at config preference */
    float opt_diff = fabsf(out->dimensions.complexity - eval->config.max_complexity_preference);
    float wundt_mod = 1.0f - opt_diff * 0.5f; /* Penalize deviation from optimal */

    /* Calculate weighted score */
    float score = 0.0f;
    score += out->dimensions.novelty * eval->config.novelty_weight;
    score += out->dimensions.complexity * eval->config.complexity_weight * wundt_mod;
    score += out->dimensions.familiarity * eval->config.familiarity_weight;
    score += (out->dimensions.hedonic_tone + 1.0f) / 2.0f * eval->config.hedonic_weight;
    score += out->dimensions.arousal_potential * eval->config.arousal_weight;

    /* Update statistics */
    eval->total_evaluations++;
    eval->avg_quality_score = (eval->avg_quality_score * (eval->total_evaluations - 1) + score) /
                               eval->total_evaluations;

    return 0;
}

int aesthetic_evaluate_music(aesthetic_evaluator_t* eval,
                              const music_track_t* tracks, uint32_t num_tracks,
                              aesthetic_evaluation_t* out) {
    if (!eval || !tracks || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "aesthetic_evaluator_destroy: required parameter is NULL (eval, tracks, out)");
        return -1;
    }

    memset(out, 0, sizeof(aesthetic_evaluation_t));

    /* Real music aesthetic analysis based on note properties */
    uint32_t total_notes = 0;
    uint8_t pitch_histogram[128] = {0};
    float total_duration = 0.0f;
    float pitch_sum = 0.0f;
    float interval_sum = 0.0f;
    uint32_t interval_count = 0;

    for (uint32_t t = 0; t < num_tracks; t++) {
        for (uint32_t n = 0; n < tracks[t].num_notes; n++) {
            const music_note_t* note = &tracks[t].notes[n];
            if (note->pitch < 128) pitch_histogram[note->pitch]++;
            pitch_sum += (float)note->pitch;
            total_duration += note->duration_beats;
            total_notes++;

            /* Interval analysis */
            if (n > 0) {
                int interval = abs((int)note->pitch - (int)tracks[t].notes[n - 1].pitch);
                interval_sum += (float)interval;
                interval_count++;
            }
        }
    }

    if (total_notes == 0) {
        eval->total_evaluations++;
        return 0;
    }

    /* Novelty: pitch entropy (unique pitches / used spectrum) */
    uint32_t unique_pitches = 0;
    float pitch_entropy = 0.0f;
    for (int i = 0; i < 128; i++) {
        if (pitch_histogram[i] > 0) {
            unique_pitches++;
            float p = (float)pitch_histogram[i] / (float)total_notes;
            pitch_entropy -= p * logf(p);
        }
    }
    pitch_entropy /= logf(128.0f); /* Normalize */
    out->dimensions.novelty = fminf(1.0f, pitch_entropy);

    /* Complexity: interval variety + rhythmic diversity */
    float avg_interval = interval_count > 0 ? interval_sum / (float)interval_count : 0.0f;
    float pitch_range = (float)unique_pitches / 128.0f;
    out->dimensions.complexity = fminf(1.0f, (avg_interval / 12.0f) * 0.5f + pitch_range * 0.5f);

    /* Familiarity: consonant interval ratio (unison, 3rd, 5th, octave) */
    /* Approximate: if avg interval is common (3-7 semitones), more familiar */
    float fam = 1.0f - fabsf(avg_interval - 5.0f) / 12.0f;
    out->dimensions.familiarity = fmaxf(0.0f, fminf(1.0f, fam));

    /* Hedonic tone: major/minor tendency from pitch distribution */
    /* Approximate: higher average pitch → brighter/happier tone */
    float avg_pitch = pitch_sum / (float)total_notes;
    out->dimensions.hedonic_tone = fmaxf(-1.0f, fminf(1.0f, (avg_pitch - 60.0f) / 30.0f));

    /* Arousal: note density (notes per beat) + interval size */
    float avg_duration = total_duration / (float)total_notes;
    float density = avg_duration > 0.0f ? 1.0f / avg_duration : 1.0f;
    out->dimensions.arousal_potential = fminf(1.0f, density * 0.3f + avg_interval / 24.0f);

    eval->total_evaluations++;

    return 0;
}

int aesthetic_evaluate_visual(aesthetic_evaluator_t* eval,
                               const visual_image_t* image,
                               aesthetic_evaluation_t* out) {
    if (!eval || !image || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "aesthetic_evaluator_destroy: required parameter is NULL (eval, image, out)");
        return -1;
    }

    memset(out, 0, sizeof(aesthetic_evaluation_t));

    /* Stub: set reasonable defaults */
    out->dimensions.novelty = 0.5f;
    out->dimensions.complexity = 0.5f;
    out->dimensions.familiarity = 0.5f;
    out->dimensions.hedonic_tone = 0.3f;
    out->dimensions.arousal_potential = 0.5f;

    eval->total_evaluations++;

    return 0;
}

int aesthetic_evaluate(aesthetic_evaluator_t* eval,
                        const void* content, art_modality_t modality,
                        aesthetic_evaluation_t* out) {
    if (!eval || !content || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "aesthetic_evaluator_destroy: required parameter is NULL (eval, content, out)");
        return -1;
    }

    /* Route to appropriate modality-specific evaluator */
    switch (modality) {
        case ART_MODALITY_TEXT_PROSE:
        case ART_MODALITY_TEXT_POETRY:
        case ART_MODALITY_TEXT_SCREENPLAY:
        case ART_MODALITY_TEXT_LYRICS:
        case ART_MODALITY_TEXT_DIALOGUE:
            return aesthetic_evaluate_text(eval, (const char*)content,
                                          strlen((const char*)content), modality, out);

        case ART_MODALITY_VISUAL_PAINTING:
        case ART_MODALITY_VISUAL_DIGITAL:
        case ART_MODALITY_VISUAL_PHOTO:
        case ART_MODALITY_VISUAL_ILLUSTRATION:
        case ART_MODALITY_VISUAL_3D:
            return aesthetic_evaluate_visual(eval, (const visual_image_t*)content, out);

        case ART_MODALITY_MUSIC_CLASSICAL:
        case ART_MODALITY_MUSIC_FILM_SCORE:
        case ART_MODALITY_MUSIC_JAZZ:
        case ART_MODALITY_MUSIC_ELECTRONIC:
        case ART_MODALITY_MUSIC_FOLK:
            return aesthetic_evaluate_music(eval, (const music_track_t*)content, 1, out);

        default:
            LOG_WARN(LOG_MODULE, "Unsupported modality: %d", modality);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "aesthetic_evaluator_destroy: operation failed");
            return -1;
    }
}

//=============================================================================
// Integration API
//=============================================================================

void aesthetic_evaluator_set_emotion_system(aesthetic_evaluator_t* eval,
                                             void* emotion_system) {
    if (!eval) return;
    eval->emotion_system = emotion_system;
    eval->config.use_emotion_system = (emotion_system != NULL);
}

void aesthetic_evaluator_set_memory_system(aesthetic_evaluator_t* eval,
                                            void* memory_system) {
    if (!eval) return;
    eval->memory_system = memory_system;
    eval->config.use_memory_system = (memory_system != NULL);
}

void aesthetic_evaluator_set_visual_cortex(aesthetic_evaluator_t* eval,
                                            void* visual_cortex) {
    if (!eval) return;
    eval->visual_cortex = visual_cortex;
}

void aesthetic_evaluator_set_audio_cortex(aesthetic_evaluator_t* eval,
                                           void* audio_cortex) {
    if (!eval) return;
    eval->audio_cortex = audio_cortex;
}

void aesthetic_evaluator_set_speech_cortex(aesthetic_evaluator_t* eval,
                                            void* speech_cortex) {
    if (!eval) return;
    eval->speech_cortex = speech_cortex;
}

//=============================================================================
// Statistics API
//=============================================================================

uint64_t aesthetic_evaluator_get_total_evaluations(const aesthetic_evaluator_t* eval) {
    return eval ? eval->total_evaluations : 0;
}

float aesthetic_evaluator_get_avg_quality(const aesthetic_evaluator_t* eval) {
    return eval ? eval->avg_quality_score : 0.0f;
}

void aesthetic_evaluator_reset_stats(aesthetic_evaluator_t* eval) {
    if (!eval) return;
    eval->total_evaluations = 0;
    eval->avg_quality_score = 0.0f;
}
