//=============================================================================
// nimcp_creative.c - Creative System Core Implementation
//=============================================================================
/**
 * @file nimcp_creative.c
 * @brief Core implementation for creative/artistic cognitive system
 *
 * WHAT: Core types, utilities, and initialization for creative system
 * WHY:  Provide foundation for artistic appreciation and generation
 * HOW:  Implements shared utilities used across creative subsystems
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/generation/nimcp_video_generation.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_MODULE "CREATIVE"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE(creative, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Config Defaults
//=============================================================================

void creative_config_init_defaults(creative_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(creative_config_t));

    /* Enable all subsystems by default */
    config->enable_appreciation = true;
    config->enable_inspiration = true;
    config->enable_text_generation = true;
    config->enable_music_generation = true;
    config->enable_visual_generation = true;
    config->enable_video_generation = true;
    config->enable_multimodal_direction = true;

    /* Quality defaults */
    config->min_quality_threshold = 0.7f;
    config->copyright_similarity_threshold = 0.85f;

    /* Resource limits */
    config->max_generation_time_ms = 60000;  /* 1 minute */
    config->max_memory_bytes = 8ULL * 1024 * 1024 * 1024;  /* 8GB */

    /* Device */
    config->device_type = 0;  /* CPU default */
    config->device_id = 0;

    /* Integration flags */
    config->integrate_with_emotion = true;
    config->integrate_with_memory = true;
    config->integrate_with_ethics = true;
    config->integrate_with_immune = true;

    /* API fallback */
    config->use_cloud_fallback = true;
}

//=============================================================================
// Modality Utilities
//=============================================================================

const char* art_modality_name(art_modality_t modality) {
    switch (modality) {
        case ART_MODALITY_TEXT_POETRY:       return "poetry";
        case ART_MODALITY_TEXT_PROSE:        return "prose";
        case ART_MODALITY_TEXT_SCREENPLAY:   return "screenplay";
        case ART_MODALITY_TEXT_LYRICS:       return "lyrics";
        case ART_MODALITY_TEXT_DIALOGUE:     return "dialogue";
        case ART_MODALITY_MUSIC_CLASSICAL:   return "classical_music";
        case ART_MODALITY_MUSIC_FILM_SCORE:  return "film_score";
        case ART_MODALITY_MUSIC_JAZZ:        return "jazz";
        case ART_MODALITY_MUSIC_ELECTRONIC:  return "electronic";
        case ART_MODALITY_MUSIC_FOLK:        return "folk";
        case ART_MODALITY_VISUAL_PAINTING:   return "painting";
        case ART_MODALITY_VISUAL_DIGITAL:    return "digital_art";
        case ART_MODALITY_VISUAL_PHOTO:      return "photography";
        case ART_MODALITY_VISUAL_ILLUSTRATION: return "illustration";
        case ART_MODALITY_VISUAL_3D:         return "3d_render";
        case ART_MODALITY_VIDEO_CINEMA:      return "cinema";
        case ART_MODALITY_VIDEO_ANIMATION:   return "animation";
        case ART_MODALITY_VIDEO_DOCUMENTARY: return "documentary";
        case ART_MODALITY_VIDEO_MUSIC_VIDEO: return "music_video";
        case ART_MODALITY_VIDEO_SHORT:       return "short_film";
        default:                             return "unknown";
    }
}

//=============================================================================
// Aesthetic Dimensions Utilities
//=============================================================================

void aesthetic_dimensions_init(aesthetic_dimensions_t* dims) {
    if (!dims) return;
    dims->novelty = 0.5f;
    dims->complexity = 0.5f;
    dims->familiarity = 0.5f;
    dims->hedonic_tone = 0.0f;
    dims->arousal_potential = 0.5f;
}

float aesthetic_dimensions_score(const aesthetic_dimensions_t* dims) {
    if (!dims) return 0.0f;

    /* Berlyne's optimal arousal theory: medium complexity/novelty is preferred */
    /* Peak aesthetic preference at moderate levels */
    float novelty_contrib = 1.0f - fabsf(dims->novelty - 0.5f) * 2.0f;
    float complexity_contrib = 1.0f - fabsf(dims->complexity - 0.6f) * 2.0f;
    float familiarity_contrib = dims->familiarity * 0.5f + 0.25f;

    /* Hedonic tone directly contributes (rescaled from [-1,1] to [0,1]) */
    float hedonic_contrib = (dims->hedonic_tone + 1.0f) * 0.5f;

    /* Arousal contributes with preference for moderate levels */
    float arousal_contrib = 1.0f - fabsf(dims->arousal_potential - 0.5f) * 1.5f;

    /* Weighted combination */
    float score = novelty_contrib * 0.2f +
                  complexity_contrib * 0.25f +
                  familiarity_contrib * 0.15f +
                  hedonic_contrib * 0.25f +
                  arousal_contrib * 0.15f;

    /* Clamp to [0, 1] */
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    return score;
}

//=============================================================================
// Emotional Response Utilities
//=============================================================================

void aesthetic_emotional_response_init(aesthetic_emotional_response_t* response) {
    if (!response) return;
    memset(response, 0, sizeof(aesthetic_emotional_response_t));
}

float aesthetic_emotional_response_valence(const aesthetic_emotional_response_t* response) {
    if (!response) return 0.0f;

    /* Positive emotions */
    float positive = response->joy + response->trust + response->awe +
                     response->sublime + response->anticipation;

    /* Negative emotions */
    float negative = response->fear + response->sadness + response->anger +
                     response->disgust;

    /* Normalize */
    float total = positive + negative;
    if (total < 0.001f) return 0.0f;

    return (positive - negative) / total;
}

float aesthetic_emotional_response_arousal(const aesthetic_emotional_response_t* response) {
    if (!response) return 0.0f;

    /* High arousal emotions */
    float high = response->joy + response->fear + response->anger +
                 response->surprise + response->awe + response->anticipation;

    /* Low arousal emotions */
    float low = response->sadness + response->trust + response->contemplation;

    float total = high + low;
    if (total < 0.001f) return 0.5f;

    return high / total;
}

//=============================================================================
// Style Embedding Utilities
//=============================================================================

int style_embedding_create(style_embedding_t* embedding, uint32_t dim) {
    if (!embedding || dim == 0 || dim > 2048) {
        LOG_ERROR(LOG_MODULE, "Invalid embedding or dimension: %u", dim);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "style_embedding_create: embedding is NULL");
        return -1;
    }

    creative_heartbeat("style_embedding_create", 0.0f);

    memset(embedding, 0, sizeof(style_embedding_t));

    embedding->embedding = nimcp_calloc(dim, sizeof(float));
    if (!embedding->embedding) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate embedding values");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "style_embedding_create: embedding->embedding is NULL");
        return -1;
    }

    embedding->embedding_dim = dim;
    embedding->archetype_id = -1;  /* No archetype */
    embedding->confidence = 0.0f;

    creative_heartbeat("style_embedding_create", 1.0f);

    return 0;
}

void style_embedding_destroy(style_embedding_t* embedding) {
    if (!embedding) return;

    if (embedding->embedding) {
        nimcp_free(embedding->embedding);
        embedding->embedding = NULL;
    }
    embedding->embedding_dim = 0;
}

int style_embedding_clone(const style_embedding_t* src, style_embedding_t* dst) {
    if (!src || !dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_embedding_clone: required parameter is NULL (src, dst)");
        return -1;
    }

    if (style_embedding_create(dst, src->embedding_dim) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "style_embedding_clone: validation failed");
        return -1;
    }

    memcpy(dst->embedding, src->embedding, src->embedding_dim * sizeof(float));
    dst->modality = src->modality;
    dst->archetype_id = src->archetype_id;
    memcpy(dst->style_name, src->style_name, sizeof(dst->style_name));
    dst->confidence = src->confidence;

    return 0;
}

float style_embedding_similarity(const style_embedding_t* a,
                                  const style_embedding_t* b) {
    if (!a || !b || a->embedding_dim != b->embedding_dim || a->embedding_dim == 0) {
        return 0.0f;
    }

    /* Cosine similarity */
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < a->embedding_dim; i++) {
        dot += a->embedding[i] * b->embedding[i];
        norm_a += a->embedding[i] * a->embedding[i];
        norm_b += b->embedding[i] * b->embedding[i];
    }

    if (norm_a < 1e-8f || norm_b < 1e-8f) {
        return 0.0f;
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

int style_embedding_interpolate(const style_embedding_t* a,
                                 const style_embedding_t* b,
                                 float t,
                                 style_embedding_t* result) {
    if (!a || !b || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_embedding_interpolate: required parameter is NULL (a, b, result)");
        return -1;
    }
    if (a->embedding_dim != b->embedding_dim || a->embedding_dim != result->embedding_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "style_embedding_interpolate: dimension mismatch");
        return -1;
    }

    /* Clamp t to [0, 1] */
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    /* Linear interpolation */
    for (uint32_t i = 0; i < a->embedding_dim; i++) {
        result->embedding[i] = a->embedding[i] * (1.0f - t) + b->embedding[i] * t;
    }

    /* Interpolate archetype weight if same archetype */
    if (a->archetype_id == b->archetype_id) {
        result->archetype_id = a->archetype_id;
        result->confidence = a->confidence * (1.0f - t) +
                                   b->confidence * t;
    } else {
        result->archetype_id = -1;
        result->confidence = 0.0f;
    }

    return 0;
}

void style_embedding_normalize(style_embedding_t* embedding) {
    if (!embedding || !embedding->embedding || embedding->embedding_dim == 0) return;

    float norm = 0.0f;
    for (uint32_t i = 0; i < embedding->embedding_dim; i++) {
        norm += embedding->embedding[i] * embedding->embedding[i];
    }

    if (norm < 1e-8f) return;

    norm = sqrtf(norm);
    for (uint32_t i = 0; i < embedding->embedding_dim; i++) {
        embedding->embedding[i] /= norm;
    }
}

//=============================================================================
// Archetype Name Utilities
//=============================================================================

const char* literary_style_archetype_name(literary_style_archetype_t archetype) {
    switch (archetype) {
        case STYLE_LIT_HEMINGWAY:    return "Hemingway";
        case STYLE_LIT_TOLSTOY:      return "Tolstoy";
        case STYLE_LIT_JOYCE:        return "Joyce";
        case STYLE_LIT_POE:          return "Poe";
        case STYLE_LIT_AUSTEN:       return "Austen";
        case STYLE_LIT_SHAKESPEARE:  return "Shakespeare";
        case STYLE_LIT_BORGES:       return "Borges";
        case STYLE_LIT_KAFKA:        return "Kafka";
        case STYLE_LIT_MARQUEZ:      return "Marquez";
        case STYLE_LIT_DOSTOEVSKY:      return "Dostoevsky";
        case STYLE_LIT_WOOLF:        return "Woolf";
        case STYLE_LIT_FAULKNER:     return "Faulkner";
        default:                     return "Unknown";
    }
}

const char* musical_style_archetype_name(musical_style_archetype_t archetype) {
    switch (archetype) {
        case STYLE_MUSIC_BACH:           return "Bach";
        case STYLE_MUSIC_BEETHOVEN:      return "Beethoven";
        case STYLE_MUSIC_DEBUSSY:        return "Debussy";
        case STYLE_MUSIC_JOHN_WILLIAMS:  return "John Williams";
        case STYLE_MUSIC_MILES_DAVIS:    return "Miles Davis";
        case STYLE_MUSIC_HANS_ZIMMER:    return "Hans Zimmer";
        case STYLE_MUSIC_STRAVINSKY:     return "Stravinsky";
        case STYLE_MUSIC_ENNIO_MORRICONE: return "Ennio Morricone";
        case STYLE_MUSIC_SAKAMOTO:       return "Sakamoto";
        case STYLE_MUSIC_GLASS:          return "Philip Glass";
        case STYLE_MUSIC_COPLAND:        return "Copland";
        case STYLE_MUSIC_RAVEL:          return "Ravel";
        default:                         return "Unknown";
    }
}

const char* visual_style_archetype_name(visual_style_archetype_t archetype) {
    switch (archetype) {
        case STYLE_VIS_VAN_GOGH:     return "Van Gogh";
        case STYLE_VIS_MONET:        return "Monet";
        case STYLE_VIS_PICASSO:      return "Picasso";
        case STYLE_VIS_DALI:         return "Dali";
        case STYLE_VIS_REMBRANDT:    return "Rembrandt";
        case STYLE_VIS_WARHOL:       return "Warhol";
        case STYLE_VIS_KLIMT:        return "Klimt";
        case STYLE_VIS_HOKUSAI:      return "Hokusai";
        case STYLE_VIS_KANDINSKY:    return "Kandinsky";
        case STYLE_VIS_ESCHER:       return "Escher";
        case STYLE_VIS_BASQUIAT:     return "Basquiat";
        case STYLE_VIS_CARAVAGGIO:   return "Caravaggio";
        default:                     return "Unknown";
    }
}

const char* cinematic_style_archetype_name(cinematic_style_archetype_t archetype) {
    switch (archetype) {
        case STYLE_CINEMA_KUBRICK:     return "Kubrick";
        case STYLE_CINEMA_SPIELBERG:   return "Spielberg";
        case STYLE_CINEMA_TARANTINO:   return "Tarantino";
        case STYLE_CINEMA_NOLAN:       return "Nolan";
        case STYLE_CINEMA_TARKOVSKY:   return "Tarkovsky";
        case STYLE_CINEMA_MIYAZAKI:    return "Miyazaki";
        case STYLE_CINEMA_WELLES:      return "Welles";
        case STYLE_CINEMA_HITCHCOCK:   return "Hitchcock";
        case STYLE_CINEMA_COPPOLA:     return "Coppola";
        case STYLE_CINEMA_FINCHER:     return "Fincher";
        case STYLE_CINEMA_KUROSAWA:    return "Kurosawa";
        case STYLE_CINEMA_VILLENEUVE: return "Denis Villeneuve";
        default:                       return "Unknown";
    }
}

//=============================================================================
// Validation Result Utilities
//=============================================================================

const char* creative_validation_result_name(creative_validation_result_t result) {
    switch (result) {
        case CREATIVE_VALIDATION_PASS:     return "PASS";
        case CREATIVE_VALIDATION_WARN:     return "WARN";
        case CREATIVE_VALIDATION_ESCALATE: return "ESCALATE";
        case CREATIVE_VALIDATION_DENY:     return "DENY";
        default:                           return "UNKNOWN";
    }
}

const char* creative_deny_reason_name(creative_deny_reason_t reason) {
    switch (reason) {
        case CREATIVE_DENY_NONE:            return "none";
        case CREATIVE_DENY_COPYRIGHT:       return "copyright";
        case CREATIVE_DENY_HARMFUL_CONTENT: return "harmful_content";
        case CREATIVE_DENY_QUALITY:         return "low_quality";
        case CREATIVE_DENY_INCOHERENT:      return "incoherent";
        case CREATIVE_DENY_EXPLICIT:        return "explicit_content";
        case CREATIVE_DENY_BIAS:            return "bias_detected";
        case CREATIVE_DENY_MISINFORMATION:  return "misinformation";
        default:                            return "unknown";
    }
}

//=============================================================================
// Visual Image Utilities
//=============================================================================

visual_image_t* visual_image_create(uint32_t width, uint32_t height,
                                     uint32_t channels) {
    if (width == 0 || height == 0 || channels == 0 || channels > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_image_create: invalid dimensions or channels");
        return NULL;
    }

    /* Check for overflow: width * height */
    if (width > 0 && height > SIZE_MAX / width) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_image_create: width*height overflow");
        return NULL;
    }
    size_t pixel_count = (size_t)width * height;
    if (channels > 0 && pixel_count > SIZE_MAX / channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_image_create: pixel_count*channels overflow");
        return NULL;
    }
    size_t data_size = pixel_count * channels;

    /* Guard against unreasonable allocations that would OOM-kill the process.
     * data_size * sizeof(float) must fit in available memory. Cap at 1GB. */
    if (data_size > (size_t)(256 * 1024 * 1024)) {  /* 256M floats = 1GB */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_image_create: image too large");
        return NULL;
    }

    visual_image_t* image = nimcp_calloc(1, sizeof(visual_image_t));
    if (!image) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_image_create: image allocation failed");
        return NULL;
    }

    image->pixels = nimcp_calloc(data_size, sizeof(float));
    if (!image->pixels) {
        nimcp_free(image);
        image = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_image_create: pixels allocation failed");
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->channels = channels;
    image->owns_pixels = true;

    return image;
}

void visual_image_destroy(visual_image_t* image) {
    if (!image) return;

    if (image->pixels) {
        nimcp_free(image->pixels);
    }
    nimcp_free(image);
    image = NULL;
}

visual_image_t* visual_image_clone(const visual_image_t* src) {
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_image_clone: src is NULL");
        return NULL;
    }

    visual_image_t* dst = visual_image_create(src->width, src->height,
                                               src->channels);
    if (!dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_image_clone: dst is NULL");
        return NULL;
    }

    /* P1-COG-01: Use dst dimensions (known-safe after successful create)
     * to avoid trusting potentially-corrupted src dimensions for memcpy size */
    size_t data_size = (size_t)dst->width * dst->height * dst->channels;
    memcpy(dst->pixels, src->pixels, data_size * sizeof(float));

    return dst;
}

//=============================================================================
// Music Track Utilities
//=============================================================================

/** P3-COG-02: Named constant for default track capacity */
#define CREATIVE_DEFAULT_MAX_NOTES 10000

music_track_t* music_track_create(uint32_t max_notes) {
    if (max_notes == 0) max_notes = CREATIVE_DEFAULT_MAX_NOTES;

    music_track_t* track = nimcp_calloc(1, sizeof(music_track_t));
    if (!track) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_track_create: track is NULL");
        return NULL;
    }

    track->notes = nimcp_calloc(max_notes, sizeof(music_note_t));
    if (!track->notes) {
        nimcp_free(track);
        track = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_track_create: track->notes is NULL");
        return NULL;
    }

    track->num_notes = 0;
    track->max_notes = max_notes;
    track->channel = 0;
    track->instrument = 0;
    strncpy(track->track_name, "Track", sizeof(track->track_name) - 1);

    return track;
}

void music_track_destroy(music_track_t* track) {
    if (!track) return;

    if (track->notes) {
        nimcp_free(track->notes);
    }
    nimcp_free(track);
    track = NULL;
}

int music_track_add_note(music_track_t* track, const music_note_t* note) {
    if (!track || !note) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_track_add_note: required parameter is NULL (track, note)");
        return -1;
    }

    /* Check capacity before adding */
    if (track->num_notes >= track->max_notes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_track_add_note: track is full");
        return -1;
    }

    track->notes[track->num_notes] = *note;
    track->num_notes++;

    return 0;
}

//=============================================================================
// Generation Result Free Functions
//=============================================================================

void text_generation_result_free(text_generation_result_t* result) {
    if (!result) return;

    if (result->text) {
        nimcp_free(result->text);
        result->text = NULL;
    }
    result->text_len = 0;
}

void music_generation_result_free(music_generation_result_t* result) {
    if (!result) return;

    if (result->tracks) {
        for (uint32_t i = 0; i < result->num_tracks; i++) {
            if (result->tracks[i].notes) {
                nimcp_free(result->tracks[i].notes);
            }
        }
        nimcp_free(result->tracks);
        result->tracks = NULL;
    }

    if (result->audio_data) {
        nimcp_free(result->audio_data);
        result->audio_data = NULL;
    }

    result->num_tracks = 0;
    result->audio_samples = 0;
}

void visual_generation_result_free(visual_generation_result_t* result) {
    if (!result) return;

    if (result->image.pixels) {
        nimcp_free(result->image.pixels);
        result->image.pixels = NULL;
    }
}

/* video_generation_result_free is in nimcp_video_generation.c */

//=============================================================================
// Aliased Result Free Functions (for API consistency)
//=============================================================================

void creative_text_result_free(text_generation_result_t* result) {
    text_generation_result_free(result);
}

void creative_music_result_free(music_generation_result_t* result) {
    music_generation_result_free(result);
}

void creative_visual_result_free(visual_generation_result_t* result) {
    visual_generation_result_free(result);
}

void creative_project_spec_free(project_specification_t* spec) {
    if (!spec) return;

    /* Free dynamically allocated scene data */
    if (spec->scenes) {
        for (uint32_t i = 0; i < spec->num_scenes; i++) {
            if (spec->scenes[i].dialogue) {
                nimcp_free(spec->scenes[i].dialogue);
                spec->scenes[i].dialogue = NULL;
            }
            if (spec->scenes[i].keyframes) {
                for (uint32_t j = 0; j < spec->scenes[i].num_keyframes; j++) {
                    if (spec->scenes[i].keyframes[j].pixels &&
                        spec->scenes[i].keyframes[j].owns_pixels) {
                        nimcp_free(spec->scenes[i].keyframes[j].pixels);
                    }
                }
                nimcp_free(spec->scenes[i].keyframes);
                spec->scenes[i].keyframes = NULL;
            }
            if (spec->scenes[i].music_cue) {
                if (spec->scenes[i].music_cue->notes) {
                    nimcp_free(spec->scenes[i].music_cue->notes);
                }
                nimcp_free(spec->scenes[i].music_cue);
                spec->scenes[i].music_cue = NULL;
            }
        }
        nimcp_free(spec->scenes);
        spec->scenes = NULL;
    }
    spec->num_scenes = 0;

    /* Free style embeddings */
    style_embedding_destroy(&spec->visual_style);
    style_embedding_destroy(&spec->music_style);
}

void creative_project_output_free(project_output_t* output) {
    if (!output) return;

    if (output->video_data) {
        nimcp_free(output->video_data);
        output->video_data = NULL;
    }
    output->video_size = 0;

    /* Free embedded result resources */
    music_generation_result_free(&output->soundtrack);
    text_generation_result_free(&output->screenplay);
}

void creative_style_embedding_free(style_embedding_t* style) {
    if (!style) return;
    style_embedding_destroy(style);
}

void creative_blend_result_free(influence_blend_result_t* result) {
    if (!result) return;

    /* Free blended style embedding */
    style_embedding_destroy(&result->blended_style);

    /* Free influence weights if allocated */
    if (result->influence_weights) {
        nimcp_free(result->influence_weights);
        result->influence_weights = NULL;
    }

    result->num_influences = 0;
}

//=============================================================================
// Appreciation API - Stub implementations
//=============================================================================

int creative_evaluate_text(creative_orchestrator_t* orchestrator,
                           const char* text, size_t len,
                           art_modality_t modality,
                           aesthetic_evaluation_t* out) {
    if (!orchestrator || !text || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_evaluate_text: required parameter is NULL");
        return -1;
    }

    creative_heartbeat("creative_evaluate_text", 0.0f);
    memset(out, 0, sizeof(*out));
    out->modality = modality;
    out->evaluation_time_us = (uint64_t)time(NULL) * 1000000ULL;

    /* Compute text-based aesthetic dimensions heuristically */
    aesthetic_dimensions_init(&out->dimensions);

    /* Estimate complexity from sentence/word structure */
    uint32_t word_count = 0;
    uint32_t sentence_count = 0;
    uint32_t unique_chars = 0;
    uint8_t char_seen[256] = {0};
    size_t total_word_len = 0;
    bool in_word = false;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (!char_seen[c]) { char_seen[c] = 1; unique_chars++; }
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
    if (word_count == 0) word_count = 1;

    float avg_word_len = (float)total_word_len / (float)word_count;
    float avg_sent_len = (float)word_count / (float)sentence_count;
    float vocab_richness = (float)unique_chars / 256.0f;

    /* Map to Berlyne dimensions */
    out->dimensions.complexity = fminf(1.0f, avg_word_len / 10.0f * 0.5f + avg_sent_len / 30.0f * 0.5f);
    out->dimensions.novelty = fminf(1.0f, vocab_richness * 1.5f);
    out->dimensions.familiarity = 1.0f - out->dimensions.novelty * 0.6f;
    out->dimensions.hedonic_tone = 0.1f;
    out->dimensions.arousal_potential = fminf(1.0f, (float)sentence_count / (float)word_count * 5.0f);

    /* Compute emotional response heuristics */
    aesthetic_emotional_response_init(&out->emotions);
    out->emotions.contemplation = fminf(1.0f, avg_sent_len / 20.0f);
    out->emotions.joy = 0.3f;
    out->emotions.anticipation = fminf(1.0f, (float)len / 2000.0f);

    /* Compute aggregate quality scores */
    out->overall_quality = aesthetic_dimensions_score(&out->dimensions);
    out->technical_skill = fminf(1.0f, avg_word_len / 8.0f * 0.5f + vocab_richness);
    out->originality = out->dimensions.novelty;
    out->coherence = fminf(1.0f, fmaxf(0.0f, 1.0f - fabsf(avg_sent_len - 15.0f) / 30.0f));
    out->expressiveness = fminf(1.0f,
        aesthetic_emotional_response_valence(&out->emotions) * 0.5f + 0.5f);

    creative_heartbeat("creative_evaluate_text", 1.0f);
    return 0;
}

int creative_evaluate_music(creative_orchestrator_t* orchestrator,
                            const music_track_t* tracks, uint32_t num_tracks,
                            aesthetic_evaluation_t* out) {
    if (!orchestrator || !tracks || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_evaluate_music: required parameter is NULL");
        return -1;
    }

    creative_heartbeat("creative_evaluate_music", 0.0f);
    memset(out, 0, sizeof(*out));
    out->modality = ART_MODALITY_MUSIC_CLASSICAL;
    out->evaluation_time_us = (uint64_t)time(NULL) * 1000000ULL;

    aesthetic_dimensions_init(&out->dimensions);
    aesthetic_emotional_response_init(&out->emotions);

    /* Analyze musical structure across tracks */
    uint32_t total_notes = 0;
    float pitch_sum = 0.0f;
    float pitch_sq_sum = 0.0f;
    float min_pitch = 127.0f;
    float max_pitch = 0.0f;
    uint32_t distinct_pitches = 0;
    uint8_t pitch_seen[128] = {0};

    for (uint32_t t = 0; t < num_tracks; t++) {
        for (uint32_t n = 0; n < tracks[t].num_notes; n++) {
            float p = (float)tracks[t].notes[n].pitch;
            if (p < 0.0f) p = 0.0f;
            if (p > 127.0f) p = 127.0f;
            pitch_sum += p;
            pitch_sq_sum += p * p;
            if (p < min_pitch) min_pitch = p;
            if (p > max_pitch) max_pitch = p;
            uint8_t pi = (uint8_t)p;
            if (pi < 128 && !pitch_seen[pi]) { pitch_seen[pi] = 1; distinct_pitches++; }
            total_notes++;
        }
    }

    if (total_notes == 0) {
        out->overall_quality = 0.0f;
        return 0;
    }

    float mean_pitch = pitch_sum / (float)total_notes;
    float pitch_variance = pitch_sq_sum / (float)total_notes - mean_pitch * mean_pitch;
    float pitch_range = max_pitch - min_pitch;
    float pitch_diversity = (float)distinct_pitches / 128.0f;

    /* Map musical features to aesthetic dimensions */
    out->dimensions.complexity = fminf(1.0f, pitch_diversity * 1.5f + (float)num_tracks / 8.0f);
    out->dimensions.novelty = fminf(1.0f, pitch_variance / 400.0f);
    out->dimensions.familiarity = fmaxf(0.0f, 1.0f - out->dimensions.novelty * 0.7f);
    out->dimensions.hedonic_tone = fminf(1.0f, fmaxf(-1.0f, (mean_pitch - 40.0f) / 50.0f));
    out->dimensions.arousal_potential = fminf(1.0f, pitch_range / 60.0f);

    /* Musical emotional response */
    out->emotions.joy = fminf(1.0f, fmaxf(0.0f, (mean_pitch - 50.0f) / 30.0f));
    out->emotions.contemplation = fminf(1.0f, 1.0f - out->dimensions.arousal_potential);
    out->emotions.awe = fminf(1.0f, pitch_range / 80.0f * pitch_diversity);
    out->emotions.anticipation = fminf(1.0f, (float)total_notes / 200.0f);

    /* Aggregate scores */
    out->overall_quality = aesthetic_dimensions_score(&out->dimensions);
    out->technical_skill = fminf(1.0f, pitch_diversity + (float)num_tracks / 10.0f);
    out->originality = out->dimensions.novelty;
    out->coherence = fminf(1.0f, fmaxf(0.0f, 1.0f - sqrtf(pitch_variance) / 40.0f));
    out->expressiveness = fminf(1.0f, out->dimensions.arousal_potential * 0.5f +
                                       pitch_diversity * 0.5f);

    creative_heartbeat("creative_evaluate_music", 1.0f);
    return 0;
}

int creative_evaluate_visual(creative_orchestrator_t* orchestrator,
                             const visual_image_t* image,
                             aesthetic_evaluation_t* out) {
    if (!orchestrator || !image || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_evaluate_visual: required parameter is NULL");
        return -1;
    }
    if (!image->pixels || image->width == 0 || image->height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "creative_evaluate_visual: invalid image data");
        return -1;
    }

    creative_heartbeat("creative_evaluate_visual", 0.0f);
    memset(out, 0, sizeof(*out));
    out->modality = ART_MODALITY_VISUAL_PAINTING;
    out->evaluation_time_us = (uint64_t)time(NULL) * 1000000ULL;

    aesthetic_dimensions_init(&out->dimensions);
    aesthetic_emotional_response_init(&out->emotions);

    /* Analyze image pixel statistics */
    size_t pixel_count = (size_t)image->width * image->height;
    uint32_t ch = image->channels;
    if (ch == 0) ch = 3;

    float luminance_sum = 0.0f;
    float luminance_sq = 0.0f;
    float color_variance_sum = 0.0f;
    float edge_count = 0.0f;

    /* Sample pixels for efficiency (stride for large images) */
    uint32_t stride = 1;
    if (pixel_count > 10000) stride = (uint32_t)(pixel_count / 10000);
    uint32_t sampled = 0;

    for (size_t i = 0; i < pixel_count; i += stride) {
        size_t idx = i * ch;
        float r = image->pixels[idx];
        float g = (ch >= 2) ? image->pixels[idx + 1] : r;
        float b = (ch >= 3) ? image->pixels[idx + 2] : r;
        float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        luminance_sum += lum;
        luminance_sq += lum * lum;
        color_variance_sum += fabsf(r - g) + fabsf(g - b) + fabsf(r - b);
        sampled++;
    }

    /* Detect edges via simple gradient sampling */
    uint32_t edge_samples = 0;
    for (uint32_t y = 1; y < image->height - 1; y += (image->height > 100 ? image->height / 50 : 1)) {
        for (uint32_t x = 1; x < image->width - 1; x += (image->width > 100 ? image->width / 50 : 1)) {
            size_t c_idx = ((size_t)y * image->width + x) * ch;
            size_t r_idx = ((size_t)y * image->width + x + 1) * ch;
            size_t d_idx = ((size_t)(y + 1) * image->width + x) * ch;
            float dx = fabsf(image->pixels[r_idx] - image->pixels[c_idx]);
            float dy = fabsf(image->pixels[d_idx] - image->pixels[c_idx]);
            if (dx + dy > 0.1f) edge_count += 1.0f;
            edge_samples++;
        }
    }

    if (sampled == 0) sampled = 1;
    float mean_lum = luminance_sum / (float)sampled;
    float lum_var = luminance_sq / (float)sampled - mean_lum * mean_lum;
    float avg_color_var = color_variance_sum / (float)sampled;
    float edge_density = (edge_samples > 0) ? edge_count / (float)edge_samples : 0.0f;

    /* Map to aesthetic dimensions */
    out->dimensions.complexity = fminf(1.0f, edge_density * 2.0f + avg_color_var / 200.0f);
    out->dimensions.novelty = fminf(1.0f, avg_color_var / 150.0f);
    out->dimensions.familiarity = fmaxf(0.0f, 1.0f - out->dimensions.novelty * 0.6f);
    out->dimensions.hedonic_tone = fminf(1.0f, fmaxf(-1.0f,
        (mean_lum - 100.0f) / 100.0f * 0.5f + avg_color_var / 200.0f));
    out->dimensions.arousal_potential = fminf(1.0f, sqrtf(lum_var) / 80.0f);

    /* Visual emotional response */
    out->emotions.awe = fminf(1.0f, edge_density * avg_color_var / 100.0f);
    out->emotions.contemplation = fminf(1.0f, 1.0f - edge_density);
    out->emotions.joy = fminf(1.0f, fmaxf(0.0f, avg_color_var / 100.0f));

    /* Aggregate scores */
    out->overall_quality = aesthetic_dimensions_score(&out->dimensions);
    out->technical_skill = fminf(1.0f, edge_density + sqrtf(lum_var) / 60.0f);
    out->originality = out->dimensions.novelty;
    out->coherence = fminf(1.0f, fmaxf(0.0f, 1.0f - fabsf(lum_var - 2000.0f) / 4000.0f));
    out->expressiveness = fminf(1.0f,
        aesthetic_emotional_response_arousal(&out->emotions));

    creative_heartbeat("creative_evaluate_visual", 1.0f);
    return 0;
}

//=============================================================================
// Inspiration API - Stub implementations
//=============================================================================

int creative_extract_style(creative_orchestrator_t* orchestrator,
                           const void* content,
                           art_modality_t modality,
                           style_embedding_t* out) {
    if (!orchestrator || !content || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_extract_style: required parameter is NULL");
        return -1;
    }

    creative_heartbeat("creative_extract_style", 0.0f);

    /* Create style embedding with default dimension */
    uint32_t embed_dim = NIMCP_DEFAULT_EMBEDDING_DIM;
    if (style_embedding_create(out, embed_dim) != 0) return -1;

    out->modality = modality;
    out->archetype_id = -1;  /* Not matched to any archetype */
    strncpy(out->style_name, "extracted", sizeof(out->style_name) - 1);

    /* Extract features based on modality type and hash into embedding space */
    uint32_t hash = 5381;
    size_t sample_bytes = 0;
    const uint8_t* data = (const uint8_t*)content;

    if (modality <= ART_MODALITY_TEXT_DIALOGUE) {
        /* Text modality - hash character n-grams */
        const char* text = (const char*)content;
        size_t len = strlen(text);
        sample_bytes = (len > 4096) ? 4096 : len;
        for (size_t i = 0; i < sample_bytes; i++) {
            hash = ((hash << 5) + hash) + (uint32_t)text[i];
        }
        /* Extract text-specific features */
        for (uint32_t d = 0; d < embed_dim; d++) {
            hash = ((hash << 5) + hash) + d;
            out->embedding[d] = ((float)(hash & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        }
    } else if (modality <= ART_MODALITY_MUSIC_ELECTRONIC) {
        /* Music modality - hash from track note patterns */
        const music_track_t* track = (const music_track_t*)content;
        for (uint32_t n = 0; n < track->num_notes && n < 1000; n++) {
            hash = ((hash << 5) + hash) + (uint32_t)track->notes[n].pitch;
            hash = ((hash << 5) + hash) + (uint32_t)(track->notes[n].duration_beats * 100.0f);
        }
        for (uint32_t d = 0; d < embed_dim; d++) {
            hash = ((hash << 5) + hash) + d;
            out->embedding[d] = ((float)(hash & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        }
    } else {
        /* Visual/other - hash pixel data */
        const visual_image_t* img = (const visual_image_t*)content;
        if (img->pixels) {
            const uint8_t* pixels = (const uint8_t*)img->pixels;
            size_t total = (size_t)img->width * img->height * img->channels;
            sample_bytes = (total > 4096) ? 4096 : total;
            for (size_t i = 0; i < sample_bytes; i++) {
                hash = ((hash << 5) + hash) + (uint32_t)(pixels[i]);
            }
        }
        for (uint32_t d = 0; d < embed_dim; d++) {
            hash = ((hash << 5) + hash) + d;
            out->embedding[d] = ((float)(hash & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        }
    }

    /* Normalize to unit length */
    style_embedding_normalize(out);
    out->confidence = 0.7f;

    creative_heartbeat("creative_extract_style", 1.0f);
    return 0;
}

int creative_blend_influences(creative_orchestrator_t* orchestrator,
                              const creative_influence_t* influences,
                              uint32_t num_influences,
                              influence_blend_result_t* out) {
    if (!orchestrator || !influences || !out || num_influences == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_blend_influences: required parameter is NULL or empty");
        return -1;
    }

    creative_heartbeat("creative_blend_influences", 0.0f);
    memset(out, 0, sizeof(*out));

    /* Use dimension from first influence */
    uint32_t dim = influences[0].style.embedding_dim;
    if (dim == 0) dim = 256;

    /* Create blended style embedding */
    if (style_embedding_create(&out->blended_style, dim) != 0) return -1;
    out->blended_style.modality = influences[0].style.modality;
    strncpy(out->blended_style.style_name, "blended", sizeof(out->blended_style.style_name) - 1);

    /* Allocate weight tracking */
    out->influence_weights = nimcp_calloc(num_influences, sizeof(float));
    if (!out->influence_weights) {
        style_embedding_destroy(&out->blended_style);
        return -1;
    }
    out->num_influences = num_influences;

    /* Normalize weights */
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < num_influences; i++) {
        float w = influences[i].weight;
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        out->influence_weights[i] = w;
        weight_sum += w;
    }
    if (weight_sum < 1e-6f) weight_sum = 1.0f;
    for (uint32_t i = 0; i < num_influences; i++) {
        out->influence_weights[i] /= weight_sum;
    }

    /* Weighted blend of style embeddings */
    for (uint32_t i = 0; i < num_influences; i++) {
        float w = out->influence_weights[i];
        float sign = influences[i].is_positive ? 1.0f : -1.0f;
        uint32_t edim = influences[i].style.embedding_dim;
        if (edim > dim) edim = dim;
        if (influences[i].style.embedding) {
            for (uint32_t d = 0; d < edim; d++) {
                out->blended_style.embedding[d] += sign * w * influences[i].style.embedding[d];
            }
        }
    }

    /* Normalize the blended embedding */
    style_embedding_normalize(&out->blended_style);
    out->blended_style.confidence = 0.8f;

    /* Compute coherence: average pairwise similarity of positive influences */
    float coherence_sum = 0.0f;
    uint32_t pairs = 0;
    for (uint32_t i = 0; i < num_influences; i++) {
        if (!influences[i].is_positive) continue;
        for (uint32_t j = i + 1; j < num_influences; j++) {
            if (!influences[j].is_positive) continue;
            float sim = style_embedding_similarity(&influences[i].style, &influences[j].style);
            coherence_sum += sim;
            pairs++;
        }
    }
    out->coherence_score = (pairs > 0) ? coherence_sum / (float)pairs : 0.5f;
    out->coherence = out->coherence_score;

    /* Novelty: distance from any single influence */
    float max_sim = 0.0f;
    for (uint32_t i = 0; i < num_influences; i++) {
        float sim = style_embedding_similarity(&out->blended_style, &influences[i].style);
        if (sim > max_sim) max_sim = sim;
    }
    out->novelty_score = 1.0f - max_sim;
    out->originality = out->novelty_score;
    out->is_valid = true;

    /* Copy to alias field — must not alias blended_style's heap pointer */
    if (style_embedding_clone(&out->blended_style, &out->style) != 0) {
        memset(&out->style, 0, sizeof(out->style));  /* Zero rather than shallow-alias to prevent double-free */
    }

    creative_heartbeat("creative_blend_influences", 1.0f);
    return 0;
}

int creative_get_archetype_style(creative_orchestrator_t* orchestrator,
                                 art_modality_t modality,
                                 int32_t archetype_id,
                                 style_embedding_t* out) {
    if (!orchestrator || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_get_archetype_style: required parameter is NULL (orchestrator, out)");
        return -1;
    }

    creative_heartbeat("creative_get_archetype_style", 0.0f);

    uint32_t embed_dim = NIMCP_DEFAULT_EMBEDDING_DIM;
    if (style_embedding_create(out, embed_dim) != 0) return -1;

    out->modality = modality;
    out->archetype_id = archetype_id;

    /* Get archetype name for labeling */
    const char* name = NULL;
    if (modality <= ART_MODALITY_TEXT_DIALOGUE) {
        name = literary_style_archetype_name((literary_style_archetype_t)archetype_id);
    } else if (modality <= ART_MODALITY_MUSIC_ELECTRONIC) {
        name = musical_style_archetype_name((musical_style_archetype_t)archetype_id);
    } else if (modality <= ART_MODALITY_VISUAL_DIGITAL) {
        name = visual_style_archetype_name((visual_style_archetype_t)archetype_id);
    } else {
        name = cinematic_style_archetype_name((cinematic_style_archetype_t)archetype_id);
    }
    if (name) {
        strncpy(out->style_name, name, sizeof(out->style_name) - 1);
    }

    /* Generate deterministic style embedding from archetype_id + modality */
    uint32_t seed = (uint32_t)archetype_id * 2654435761U + (uint32_t)modality * 2246822519U;
    for (uint32_t d = 0; d < embed_dim; d++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        out->embedding[d] = ((float)(seed & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
    }

    style_embedding_normalize(out);
    out->confidence = 1.0f;  /* Archetype styles are fully defined */

    creative_heartbeat("creative_get_archetype_style", 1.0f);
    return 0;
}

//=============================================================================
// Generation API - Stub implementations
//=============================================================================

int creative_generate_text(creative_orchestrator_t* orchestrator,
                           const text_generation_request_t* request,
                           text_generation_result_t* result) {
    if (!orchestrator || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_generate_text: required parameter is NULL");
        return -1;
    }

    creative_heartbeat("creative_generate_text", 0.0f);
    memset(result, 0, sizeof(*result));

    /* Determine generation length */
    uint32_t max_len = request->max_length;
    if (max_len == 0) max_len = 512;
    if (max_len > 65536) max_len = 65536;

    /* Allocate output buffer */
    result->text = nimcp_calloc(max_len + 1, sizeof(char));
    if (!result->text) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Memory allocation failed for %u bytes", max_len);
        result->success = false;
        return -1;
    }

    /* Seed-based text generation using Markov-like character model */
    uint32_t seed = 12345;
    if (request->prompt && request->prompt_len > 0) {
        for (size_t i = 0; i < request->prompt_len; i++) {
            seed = seed * 31 + (uint32_t)request->prompt[i];
        }
    }

    /* Apply temperature to randomness */
    float temp = request->temperature;
    if (temp <= 0.0f) temp = 0.7f;
    if (temp > 2.0f) temp = 2.0f;

    /* Copy prompt as prefix */
    size_t pos = 0;
    if (request->prompt && request->prompt_len > 0) {
        size_t copy_len = request->prompt_len;
        if (copy_len > max_len / 2) copy_len = max_len / 2;
        memcpy(result->text, request->prompt, copy_len);
        pos = copy_len;
    }

    /* Generate text using simple statistical model */
    static const char* common_words[] = {
        "the ", "and ", "of ", "to ", "in ", "a ", "that ", "is ",
        "was ", "for ", "it ", "with ", "as ", "on ", "be ", "at ",
        "this ", "from ", "an ", "by ", "but ", "not ", "are ", "or ",
        "which ", "have ", "had ", "has ", "its ", "were ", "their ",
        "will ", "each ", "about ", "up ", "out ", "them ", "then ",
        "she ", "many ", "some ", "so ", "these ", "would ", "other ",
        "into ", "more ", "her ", "like ", "time ", "very ", "when ",
        "come ", "could ", "now ", "than ", "first ", "been ", "made ",
        "after ", "also ", "did ", "back ", "see ", "way ", "over "
    };
    uint32_t num_words = sizeof(common_words) / sizeof(common_words[0]);

    uint32_t words_generated = 0;
    while (pos < max_len - 20) {
        /* Generate word index with temperature-modulated randomness */
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        uint32_t word_idx = seed % num_words;

        /* Add word */
        const char* word = common_words[word_idx];
        size_t wlen = strlen(word);
        if (pos + wlen >= max_len) break;
        memcpy(result->text + pos, word, wlen);
        pos += wlen;
        words_generated++;

        /* Periodically add sentence breaks */
        if (words_generated % (uint32_t)(8 + temp * 5) == 0 && pos + 2 < max_len) {
            result->text[pos++] = '.';
            result->text[pos++] = ' ';
            /* Capitalize next word */
            if (pos < max_len && result->text[pos] >= 'a' && result->text[pos] <= 'z') {
                result->text[pos] -= 32;
            }
        }
    }

    /* Terminate properly */
    if (pos > 0 && result->text[pos - 1] != '.') {
        if (pos < max_len) result->text[pos++] = '.';
    }
    result->text[pos] = '\0';
    result->text_len = pos;
    result->tokens_generated = words_generated;
    result->success = true;

    /* Self-evaluate the generated text */
    creative_evaluate_text(orchestrator, result->text, result->text_len,
                          ART_MODALITY_TEXT_PROSE, &result->evaluation);

    creative_heartbeat("creative_generate_text", 1.0f);
    return 0;
}

int creative_generate_music(creative_orchestrator_t* orchestrator,
                            const music_generation_request_t* request,
                            music_generation_result_t* result) {
    if (!orchestrator || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_generate_music: required parameter is NULL");
        return -1;
    }

    creative_heartbeat("creative_generate_music", 0.0f);
    memset(result, 0, sizeof(*result));

    /* Determine parameters */
    uint32_t num_tracks = request->num_tracks;
    if (num_tracks == 0) num_tracks = 1;
    if (num_tracks > 16) num_tracks = 16;
    uint16_t bpm = request->tempo_bpm;
    if (bpm == 0) bpm = 120;
    float duration = request->duration_seconds;
    if (duration <= 0.0f) duration = 30.0f;
    if (duration > 600.0f) duration = 600.0f;

    /* Calculate notes per track from tempo and duration */
    float beats = duration * (float)bpm / 60.0f;
    uint32_t notes_per_track = (uint32_t)(beats * 2);  /* Eighth notes */
    if (notes_per_track == 0) notes_per_track = 16;
    if (notes_per_track > 10000) notes_per_track = 10000;

    /* Allocate tracks */
    result->tracks = nimcp_calloc(num_tracks, sizeof(music_track_t));
    if (!result->tracks) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to allocate %u tracks", num_tracks);
        result->success = false;
        return -1;
    }
    result->num_tracks = num_tracks;
    result->tempo_bpm = bpm;
    result->duration_seconds = duration;

    /* Seed RNG */
    uint32_t seed = 42;
    if (request->mood) {
        for (const char* p = request->mood; *p; p++) seed = seed * 31 + (uint32_t)*p;
    }

    /* Define scale intervals (major scale) */
    static const uint8_t major_scale[] = {0, 2, 4, 5, 7, 9, 11};
    uint8_t root_note = 60;  /* Middle C */

    for (uint32_t t = 0; t < num_tracks; t++) {
        music_track_t* track = &result->tracks[t];
        track->notes = nimcp_calloc(notes_per_track, sizeof(music_note_t));
        if (!track->notes) {
            /* Clean up previously allocated tracks */
            for (uint32_t j = 0; j < t; j++) {
                if (result->tracks[j].notes) nimcp_free(result->tracks[j].notes);
            }
            nimcp_free(result->tracks);
            result->tracks = NULL;
            result->success = false;
            return -1;
        }
        track->max_notes = notes_per_track;
        track->channel = (uint8_t)t;
        track->instrument = (uint8_t)(t * 8);
        snprintf(track->track_name, sizeof(track->track_name), "Track_%u", t);

        float time_pos = 0.0f;
        float beat_dur = 60.0f / (float)bpm;

        for (uint32_t n = 0; n < notes_per_track; n++) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;

            music_note_t note;
            memset(&note, 0, sizeof(note));

            /* Pick scale degree */
            uint32_t degree = seed % 7;
            int octave_shift = (int)((seed >> 8) % 3) - 1;
            note.pitch = root_note + major_scale[degree] + (int8_t)(octave_shift * 12);
            if (note.pitch > 127) note.pitch = 127;
            if (note.pitch < 21) note.pitch = 21;

            /* Duration: mix of quarter and eighth notes */
            note.duration_beats = ((seed >> 16) & 1) ? 1.0f : 0.5f;
            note.velocity = 0.5f + (float)((seed >> 20) % 50) / 100.0f;
            note.start_beat = time_pos;

            track->notes[track->num_notes++] = note;
            time_pos += note.duration_beats * beat_dur;
            if (time_pos >= duration) break;
        }
    }

    result->success = true;

    /* Self-evaluate */
    if (num_tracks > 0 && result->tracks[0].num_notes > 0) {
        creative_evaluate_music(orchestrator, result->tracks, num_tracks,
                               &result->evaluation);
    }

    creative_heartbeat("creative_generate_music", 1.0f);
    return 0;
}

int creative_generate_visual(creative_orchestrator_t* orchestrator,
                             const visual_generation_request_t* request,
                             visual_generation_result_t* result) {
    if (!orchestrator || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "creative_generate_visual: required parameter is NULL");
        return -1;
    }

    creative_heartbeat("creative_generate_visual", 0.0f);
    memset(result, 0, sizeof(*result));

    /* Determine image dimensions */
    uint32_t width = request->width;
    uint32_t height = request->height;
    if (width == 0) width = 256;
    if (height == 0) height = 256;

    /* Reject unreasonably large dimensions */
    if (width > 16384 || height > 16384) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "creative_generate_visual: image dimensions too large");
        return -1;
    }
    if (width > 4096) width = 4096;
    if (height > 4096) height = 4096;
    uint32_t channels = 3;  /* RGB */

    /* Check for overflow */
    if (width > 0 && height > SIZE_MAX / width) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Image dimensions overflow");
        result->success = false;
        return -1;
    }
    size_t pixel_count = (size_t)width * height;
    if (pixel_count > SIZE_MAX / (channels * sizeof(float))) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Image size overflow");
        result->success = false;
        return -1;
    }

    /* Allocate pixel data */
    result->image.pixels = nimcp_calloc(pixel_count * channels, sizeof(float));
    if (!result->image.pixels) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to allocate %zu pixels", pixel_count);
        result->success = false;
        return -1;
    }
    result->image.width = width;
    result->image.height = height;
    result->image.channels = channels;

    /* Seed from request parameters */
    uint64_t seed = request->seed;
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }
    result->seed_used = seed;
    uint32_t s = (uint32_t)(seed & 0xFFFFFFFF);

    /* Procedurally generate image with gradient + noise pattern */
    float guidance = request->guidance_scale;
    if (guidance <= 0.0f) guidance = 7.5f;

    /* Hash prompt into color palette */
    float base_r = 0.5f, base_g = 0.4f, base_b = 0.6f;
    if (request->prompt) {
        uint32_t ph = 0;
        for (const char* p = request->prompt; *p; p++) {
            ph = ph * 31 + (uint32_t)*p;
        }
        base_r = (float)((ph >> 0) & 0xFF) / 255.0f;
        base_g = (float)((ph >> 8) & 0xFF) / 255.0f;
        base_b = (float)((ph >> 16) & 0xFF) / 255.0f;
    }

    for (uint32_t y = 0; y < height; y++) {
        float fy = (float)y / (float)height;
        for (uint32_t x = 0; x < width; x++) {
            float fx = (float)x / (float)width;
            size_t idx = ((size_t)y * width + x) * channels;

            /* Noise */
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            float noise = ((float)(s & 0xFFFF) / 65535.0f - 0.5f) * 0.15f;

            /* Gradient + palette + noise */
            float r = base_r * (1.0f - fy * 0.5f) + noise + fx * 0.2f;
            float g = base_g * (1.0f - fx * 0.3f) + noise + fy * 0.2f;
            float b = base_b * (fy * 0.5f + 0.5f) + noise;

            /* Clamp */
            if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
            if (g < 0.0f) g = 0.0f; if (g > 1.0f) g = 1.0f;
            if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;

            result->image.pixels[idx + 0] = r;
            result->image.pixels[idx + 1] = g;
            result->image.pixels[idx + 2] = b;
        }
    }

    result->success = true;

    /* Self-evaluate */
    creative_evaluate_visual(orchestrator, &result->image, &result->evaluation);

    creative_heartbeat("creative_generate_visual", 1.0f);
    return 0;
}
