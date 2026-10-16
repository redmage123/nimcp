//=============================================================================
// nimcp_creative_pattern_extractor.c - Artistic Pattern Extraction
//=============================================================================
/**
 * @file nimcp_creative_pattern_extractor.c
 * @brief Extracts structural and thematic patterns from artistic works
 *
 * WHAT: Identifies recurring patterns, motifs, and structures in art
 * WHY:  Enable learning from and reproducing artistic techniques
 * HOW:  Multi-level pattern analysis (surface, structural, thematic)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/inspiration/nimcp_creative_pattern_extractor.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "CREATIVE_PATTERN"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(creative_pattern_extractor, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define DEFAULT_FEATURE_DIM 64
#define DEFAULT_MAX_PATTERNS 100
#define DEFAULT_KNOWN_CAPACITY 1000

//=============================================================================
// Config Defaults
//=============================================================================

void creative_pattern_extractor_config_defaults(
    creative_pattern_extractor_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(creative_pattern_extractor_config_t));

    config->extraction_level = PATTERN_LEVEL_ALL;
    config->min_prevalence = 0.1f;
    config->min_distinctiveness = 0.2f;
    config->max_patterns = DEFAULT_MAX_PATTERNS;

    config->feature_dim = DEFAULT_FEATURE_DIM;
    config->normalize_features = true;

    config->analyze_structure = true;
    config->analyze_semantics = false;  /* Requires external model */
    config->cross_reference = true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static float compute_feature_similarity(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }

    if (mag_a < 0.0001f || mag_b < 0.0001f) return 0.0f;
    return dot / (sqrtf(mag_a) * sqrtf(mag_b));
}

static void normalize_feature_vector(float* vec, uint32_t dim) {
    if (!vec || dim == 0) return;

    float mag = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        mag += vec[i] * vec[i];
    }
    mag = sqrtf(mag);

    if (mag > 0.0001f) {
        for (uint32_t i = 0; i < dim; i++) {
            vec[i] /= mag;
        }
    }
}

static const char* pattern_category_name(creative_pattern_category_t cat) {
    switch (cat) {
        case PATTERN_TEXT_RHYTHM: return "text_rhythm";
        case PATTERN_TEXT_SYNTAX: return "text_syntax";
        case PATTERN_TEXT_VOCABULARY: return "text_vocabulary";
        case PATTERN_TEXT_NARRATIVE: return "text_narrative";
        case PATTERN_TEXT_IMAGERY: return "text_imagery";
        case PATTERN_TEXT_THEME: return "text_theme";
        case PATTERN_MUSIC_MELODY: return "music_melody";
        case PATTERN_MUSIC_HARMONY: return "music_harmony";
        case PATTERN_MUSIC_RHYTHM: return "music_rhythm";
        case PATTERN_MUSIC_FORM: return "music_form";
        case PATTERN_MUSIC_TEXTURE: return "music_texture";
        case PATTERN_MUSIC_MOTIF: return "music_motif";
        case PATTERN_VISUAL_COMPOSITION: return "visual_composition";
        case PATTERN_VISUAL_COLOR: return "visual_color";
        case PATTERN_VISUAL_TEXTURE: return "visual_texture";
        case PATTERN_VISUAL_SHAPE: return "visual_shape";
        case PATTERN_VISUAL_LIGHT: return "visual_light";
        case PATTERN_VISUAL_SYMBOL: return "visual_symbol";
        case PATTERN_CINEMA_SHOT: return "cinema_shot";
        case PATTERN_CINEMA_EDITING: return "cinema_editing";
        case PATTERN_CINEMA_SOUND: return "cinema_sound";
        case PATTERN_CINEMA_NARRATIVE: return "cinema_narrative";
        case PATTERN_CINEMA_VISUAL: return "cinema_visual";
        case PATTERN_CINEMA_PACING: return "cinema_pacing";
        default: return "unknown";
    }
}

static void extract_text_patterns(creative_pattern_extractor_t* ext,
                                   const char* text, size_t len,
                                   extracted_pattern_t** patterns,
                                   uint32_t* num_patterns) {
    uint32_t count = 0;
    uint32_t capacity = ext->config.max_patterns;
    *patterns = nimcp_calloc(capacity, sizeof(extracted_pattern_t));
    if (!*patterns) return;

    /* Extract rhythm pattern (sentence length distribution) */
    if (count < capacity) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_TEXT_RHYTHM;
        p->level = PATTERN_LEVEL_SURFACE;
        strncpy(p->name, "sentence_rhythm", sizeof(p->name) - 1);
        strncpy(p->description, "Sentence length and rhythm pattern", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* Compute sentence length histogram */
            uint32_t sent_lengths[20] = {0};
            uint32_t current_len = 0;
            uint32_t num_sentences = 0;

            for (size_t i = 0; i < len; i++) {
                current_len++;
                if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
                    uint32_t bucket = current_len / 10;
                    if (bucket >= 20) bucket = 19;
                    sent_lengths[bucket]++;
                    num_sentences++;
                    current_len = 0;
                }
            }

            /* Fill feature vector */
            for (uint32_t i = 0; i < p->feature_dim && i < 20; i++) {
                p->feature_vector[i] = num_sentences > 0 ?
                    (float)sent_lengths[i] / num_sentences : 0.0f;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 1.0f;
            p->distinctiveness = 0.5f;
            p->importance = 0.4f;
            p->occurrence_count = num_sentences;
            count++;
        }
    }

    /* Extract vocabulary pattern */
    if (count < capacity) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_TEXT_VOCABULARY;
        p->level = PATTERN_LEVEL_SURFACE;
        strncpy(p->name, "vocabulary_profile", sizeof(p->name) - 1);
        strncpy(p->description, "Character frequency and vocabulary pattern", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* Character frequency (a-z) */
            uint32_t char_freq[26] = {0};
            uint32_t total_letters = 0;

            for (size_t i = 0; i < len; i++) {
                char c = text[i];
                if (c >= 'A' && c <= 'Z') c += 32;
                if (c >= 'a' && c <= 'z') {
                    char_freq[c - 'a']++;
                    total_letters++;
                }
            }

            /* Fill feature vector with char frequencies */
            for (uint32_t i = 0; i < p->feature_dim && i < 26; i++) {
                p->feature_vector[i] = total_letters > 0 ?
                    (float)char_freq[i] / total_letters : 0.0f;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 1.0f;
            p->distinctiveness = 0.6f;
            p->importance = 0.3f;
            p->occurrence_count = total_letters;
            count++;
        }
    }

    /* Extract structural pattern (paragraph distribution) */
    if (count < capacity && ext->config.analyze_structure) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_TEXT_NARRATIVE;
        p->level = PATTERN_LEVEL_STRUCTURAL;
        strncpy(p->name, "paragraph_structure", sizeof(p->name) - 1);
        strncpy(p->description, "Paragraph length and distribution pattern", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* Count paragraphs and their lengths */
            uint32_t para_count = 0;
            uint32_t current_para_len = 0;
            uint32_t para_lengths[50] = {0};

            for (size_t i = 0; i < len; i++) {
                current_para_len++;
                if (text[i] == '\n' && i + 1 < len && text[i + 1] == '\n') {
                    uint32_t bucket = current_para_len / 100;
                    if (bucket >= 50) bucket = 49;
                    para_lengths[bucket]++;
                    para_count++;
                    current_para_len = 0;
                }
            }

            /* Fill feature vector */
            for (uint32_t i = 0; i < p->feature_dim && i < 50; i++) {
                p->feature_vector[i] = para_count > 0 ?
                    (float)para_lengths[i] / (para_count > 0 ? para_count : 1) : 0.0f;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 0.8f;
            p->distinctiveness = 0.4f;
            p->importance = 0.5f;
            p->occurrence_count = para_count > 0 ? para_count : 1;
            count++;
        }
    }

    *num_patterns = count;
}

static void extract_music_patterns(creative_pattern_extractor_t* ext,
                                    const music_track_t* tracks, uint32_t num_tracks,
                                    extracted_pattern_t** patterns,
                                    uint32_t* num_patterns) {
    uint32_t count = 0;
    uint32_t capacity = ext->config.max_patterns;
    *patterns = nimcp_calloc(capacity, sizeof(extracted_pattern_t));
    if (!*patterns) return;

    /* Aggregate notes across tracks */
    uint32_t total_notes = 0;
    for (uint32_t t = 0; t < num_tracks; t++) {
        total_notes += tracks[t].num_notes;
    }

    /* Extract pitch distribution pattern */
    if (count < capacity && total_notes > 0) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_MUSIC_MELODY;
        p->level = PATTERN_LEVEL_SURFACE;
        strncpy(p->name, "pitch_distribution", sizeof(p->name) - 1);
        strncpy(p->description, "Pitch class distribution pattern", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* Pitch class histogram (12 classes) */
            uint32_t pitch_class[12] = {0};

            for (uint32_t t = 0; t < num_tracks; t++) {
                for (uint32_t n = 0; n < tracks[t].num_notes; n++) {
                    uint8_t pc = tracks[t].notes[n].pitch % 12;
                    pitch_class[pc]++;
                }
            }

            /* Fill feature vector */
            for (uint32_t i = 0; i < p->feature_dim && i < 12; i++) {
                p->feature_vector[i] = (float)pitch_class[i] / total_notes;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 1.0f;
            p->distinctiveness = 0.7f;
            p->importance = 0.6f;
            p->occurrence_count = total_notes;
            count++;
        }
    }

    /* Extract rhythm pattern */
    if (count < capacity && total_notes > 0) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_MUSIC_RHYTHM;
        p->level = PATTERN_LEVEL_SURFACE;
        strncpy(p->name, "rhythm_pattern", sizeof(p->name) - 1);
        strncpy(p->description, "Note duration distribution", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* Duration histogram (bucketed) */
            float duration_sum = 0.0f;
            uint32_t duration_hist[16] = {0};

            for (uint32_t t = 0; t < num_tracks; t++) {
                for (uint32_t n = 0; n < tracks[t].num_notes; n++) {
                    float dur = tracks[t].notes[n].duration_beats;
                    duration_sum += dur;

                    uint32_t bucket = (uint32_t)(dur * 4);  /* Quarter beats */
                    if (bucket >= 16) bucket = 15;
                    duration_hist[bucket]++;
                }
            }

            /* Fill feature vector */
            for (uint32_t i = 0; i < p->feature_dim && i < 16; i++) {
                p->feature_vector[i] = (float)duration_hist[i] / total_notes;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 1.0f;
            p->distinctiveness = 0.6f;
            p->importance = 0.5f;
            p->occurrence_count = total_notes;
            count++;
        }
    }

    *num_patterns = count;
}

static void extract_visual_patterns(creative_pattern_extractor_t* ext,
                                     const visual_image_t* image,
                                     extracted_pattern_t** patterns,
                                     uint32_t* num_patterns) {
    uint32_t count = 0;
    uint32_t capacity = ext->config.max_patterns;
    *patterns = nimcp_calloc(capacity, sizeof(extracted_pattern_t));
    if (!*patterns) return;

    uint64_t pixels = (uint64_t)image->width * image->height;

    /* Extract color pattern */
    if (count < capacity && image->pixels && pixels > 0) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_VISUAL_COLOR;
        p->level = PATTERN_LEVEL_SURFACE;
        strncpy(p->name, "color_distribution", sizeof(p->name) - 1);
        strncpy(p->description, "Color histogram pattern", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* RGB histogram (8 bins per channel = 24 features) */
            uint32_t r_hist[8] = {0}, g_hist[8] = {0}, b_hist[8] = {0};

            uint64_t sample_step = pixels > 10000 ? pixels / 10000 : 1;
            uint32_t samples = 0;

            for (uint64_t i = 0; i < pixels; i += sample_step) {
                uint32_t idx = (uint32_t)(i * image->channels);
                if (image->channels >= 3) {
                    r_hist[image->pixels[idx] / 32]++;
                    g_hist[image->pixels[idx + 1] / 32]++;
                    b_hist[image->pixels[idx + 2] / 32]++;
                } else {
                    r_hist[image->pixels[idx] / 32]++;
                    g_hist[image->pixels[idx] / 32]++;
                    b_hist[image->pixels[idx] / 32]++;
                }
                samples++;
            }

            /* Fill feature vector */
            for (uint32_t i = 0; i < 8 && i < p->feature_dim; i++) {
                p->feature_vector[i] = (float)r_hist[i] / samples;
            }
            for (uint32_t i = 0; i < 8 && (i + 8) < p->feature_dim; i++) {
                p->feature_vector[i + 8] = (float)g_hist[i] / samples;
            }
            for (uint32_t i = 0; i < 8 && (i + 16) < p->feature_dim; i++) {
                p->feature_vector[i + 16] = (float)b_hist[i] / samples;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 1.0f;
            p->distinctiveness = 0.7f;
            p->importance = 0.6f;
            p->occurrence_count = samples;
            count++;
        }
    }

    /* Extract composition pattern (simplified) */
    if (count < capacity && image->pixels && pixels > 0 && ext->config.analyze_structure) {
        extracted_pattern_t* p = &(*patterns)[count];
        p->category = PATTERN_VISUAL_COMPOSITION;
        p->level = PATTERN_LEVEL_STRUCTURAL;
        strncpy(p->name, "composition_grid", sizeof(p->name) - 1);
        strncpy(p->description, "Spatial intensity distribution (rule of thirds grid)", sizeof(p->description) - 1);

        p->feature_dim = ext->config.feature_dim;
        p->feature_vector = nimcp_calloc(p->feature_dim, sizeof(float));

        if (p->feature_vector) {
            /* Compute 3x3 grid average intensities */
            float grid[9] = {0};
            uint32_t grid_counts[9] = {0};

            uint64_t sample_step = pixels > 10000 ? pixels / 10000 : 1;

            for (uint64_t i = 0; i < pixels; i += sample_step) {
                uint32_t x = (uint32_t)(i % image->width);
                uint32_t y = (uint32_t)(i / image->width);

                uint32_t gx = (x * 3) / image->width;
                uint32_t gy = (y * 3) / image->height;
                if (gx >= 3) gx = 2;
                if (gy >= 3) gy = 2;

                uint32_t grid_idx = gy * 3 + gx;

                uint32_t pix_idx = (uint32_t)(i * image->channels);
                float intensity = 0.0f;
                if (image->channels >= 3) {
                    intensity = (image->pixels[pix_idx] + image->pixels[pix_idx + 1] +
                                 image->pixels[pix_idx + 2]) / (3.0f * 255.0f);
                } else {
                    intensity = image->pixels[pix_idx] / 255.0f;
                }

                grid[grid_idx] += intensity;
                grid_counts[grid_idx]++;
            }

            /* Fill feature vector */
            for (uint32_t i = 0; i < 9 && i < p->feature_dim; i++) {
                p->feature_vector[i] = grid_counts[i] > 0 ?
                    grid[i] / grid_counts[i] : 0.0f;
            }

            if (ext->config.normalize_features) {
                normalize_feature_vector(p->feature_vector, p->feature_dim);
            }

            p->prevalence = 1.0f;
            p->distinctiveness = 0.5f;
            p->importance = 0.4f;
            p->occurrence_count = 1;
            count++;
        }
    }

    *num_patterns = count;
}

//=============================================================================
// Lifecycle API
//=============================================================================

creative_pattern_extractor_t* creative_pattern_extractor_create(
    const creative_pattern_extractor_config_t* config) {

    creative_pattern_extractor_t* ext = nimcp_calloc(1, sizeof(creative_pattern_extractor_t));
    if (!ext) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate pattern extractor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: ext is NULL");
        return NULL;
    }

    if (config) {
        ext->config = *config;
    } else {
        creative_pattern_extractor_config_defaults(&ext->config);
    }

    /* Allocate known pattern storage */
    ext->known_capacity = DEFAULT_KNOWN_CAPACITY;
    ext->known_patterns = nimcp_calloc(ext->known_capacity, sizeof(extracted_pattern_t));
    if (!ext->known_patterns) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate known patterns");
        nimcp_free(ext);
        ext = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: ext->known_patterns is NULL");
        return NULL;
    }

    LOG_INFO(LOG_MODULE, "Creative pattern extractor created");

    return ext;
}

void creative_pattern_extractor_destroy(creative_pattern_extractor_t* ext) {
    if (!ext) return;

    /* Free known patterns */
    for (uint32_t i = 0; i < ext->num_known_patterns; i++) {
        if (ext->known_patterns[i].feature_vector) {
            nimcp_free(ext->known_patterns[i].feature_vector);
        }
    }
    nimcp_free(ext->known_patterns);

    nimcp_free(ext);
    ext = NULL;

    LOG_INFO(LOG_MODULE, "Creative pattern extractor destroyed");
}

//=============================================================================
// Extraction API
//=============================================================================

int creative_pattern_extract_text(creative_pattern_extractor_t* ext,
                                   const char* text, size_t len,
                                   art_modality_t modality,
                                   pattern_extraction_result_t* result) {
    if (!ext || !text || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_pattern_extractor_destroy: required parameter is NULL (ext, text, result)");
        return -1;
    }
    (void)modality;

    memset(result, 0, sizeof(pattern_extraction_result_t));

    uint64_t start_time = get_time_ms();

    extract_text_patterns(ext, text, len, &result->patterns, &result->num_patterns);

    result->modality = modality;
    result->extraction_confidence = 0.8f;
    result->coverage = result->num_patterns > 0 ? 0.7f : 0.0f;
    result->extraction_time_ms = get_time_ms() - start_time;

    ext->extractions_performed++;
    ext->avg_patterns_per_work = ext->avg_patterns_per_work * 0.95f +
                                  result->num_patterns * 0.05f;

    return 0;
}

int creative_pattern_extract_music(creative_pattern_extractor_t* ext,
                                    const music_track_t* tracks, uint32_t num_tracks,
                                    pattern_extraction_result_t* result) {
    if (!ext || !tracks || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_pattern_extractor_destroy: required parameter is NULL (ext, tracks, result)");
        return -1;
    }

    memset(result, 0, sizeof(pattern_extraction_result_t));

    uint64_t start_time = get_time_ms();

    extract_music_patterns(ext, tracks, num_tracks, &result->patterns, &result->num_patterns);

    result->modality = ART_MODALITY_MUSIC_CLASSICAL;
    result->extraction_confidence = 0.8f;
    result->coverage = result->num_patterns > 0 ? 0.7f : 0.0f;
    result->extraction_time_ms = get_time_ms() - start_time;

    ext->extractions_performed++;
    ext->avg_patterns_per_work = ext->avg_patterns_per_work * 0.95f +
                                  result->num_patterns * 0.05f;

    return 0;
}

int creative_pattern_extract_visual(creative_pattern_extractor_t* ext,
                                     const visual_image_t* image,
                                     pattern_extraction_result_t* result) {
    if (!ext || !image || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_pattern_extractor_destroy: required parameter is NULL (ext, image, result)");
        return -1;
    }

    memset(result, 0, sizeof(pattern_extraction_result_t));

    uint64_t start_time = get_time_ms();

    extract_visual_patterns(ext, image, &result->patterns, &result->num_patterns);

    result->modality = ART_MODALITY_VISUAL_PAINTING;
    result->extraction_confidence = 0.8f;
    result->coverage = result->num_patterns > 0 ? 0.7f : 0.0f;
    result->extraction_time_ms = get_time_ms() - start_time;

    ext->extractions_performed++;
    ext->avg_patterns_per_work = ext->avg_patterns_per_work * 0.95f +
                                  result->num_patterns * 0.05f;

    return 0;
}

//=============================================================================
// Pattern Matching API
//=============================================================================

uint32_t creative_pattern_find(const creative_pattern_extractor_t* ext,
                                const void* content,
                                art_modality_t modality,
                                const extracted_pattern_t* patterns,
                                uint32_t num_patterns,
                                pattern_match_t* matches,
                                uint32_t max_matches) {
    if (!ext || !content || !patterns || !matches) return 0;
    (void)modality;

    /* Simplified: just report matches based on category similarity */
    uint32_t found = 0;

    for (uint32_t i = 0; i < num_patterns && found < max_matches; i++) {
        /* For now, report all patterns as matched with varying similarity */
        matches[found].pattern_idx = i;
        matches[found].similarity = patterns[i].prevalence * 0.8f;
        matches[found].position = 0.0f;
        matches[found].span = 1.0f;
        found++;
    }

    return found;
}

float creative_pattern_compare(const creative_pattern_extractor_t* ext,
                                const extracted_pattern_t* patterns_a, uint32_t num_a,
                                const extracted_pattern_t* patterns_b, uint32_t num_b) {
    if (!ext || !patterns_a || !patterns_b || num_a == 0 || num_b == 0) return 0.0f;

    float total_sim = 0.0f;
    uint32_t comparisons = 0;

    /* Compare patterns of same category */
    for (uint32_t i = 0; i < num_a; i++) {
        for (uint32_t j = 0; j < num_b; j++) {
            if (patterns_a[i].category == patterns_b[j].category &&
                patterns_a[i].feature_vector && patterns_b[j].feature_vector &&
                patterns_a[i].feature_dim == patterns_b[j].feature_dim) {

                float sim = compute_feature_similarity(
                    patterns_a[i].feature_vector,
                    patterns_b[j].feature_vector,
                    patterns_a[i].feature_dim);

                total_sim += sim;
                comparisons++;
            }
        }
    }

    return comparisons > 0 ? total_sim / comparisons : 0.0f;
}

//=============================================================================
// Pattern Database API
//=============================================================================

int32_t creative_pattern_db_add(creative_pattern_extractor_t* ext,
                                 const extracted_pattern_t* pattern) {
    if (!ext || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_pattern_extractor_destroy: required parameter is NULL (ext, pattern)");
        return -1;
    }

    if (ext->num_known_patterns >= ext->known_capacity) {
        LOG_WARN(LOG_MODULE, "Pattern database full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "creative_pattern_extractor_destroy: capacity exceeded");
        return -1;
    }

    uint32_t idx = ext->num_known_patterns;
    ext->known_patterns[idx] = *pattern;

    /* Deep copy feature vector */
    if (pattern->feature_vector && pattern->feature_dim > 0) {
        ext->known_patterns[idx].feature_vector =
            nimcp_calloc(pattern->feature_dim, sizeof(float));
        if (ext->known_patterns[idx].feature_vector) {
            memcpy(ext->known_patterns[idx].feature_vector,
                   pattern->feature_vector,
                   pattern->feature_dim * sizeof(float));
        }
    }

    ext->num_known_patterns++;

    return (int32_t)idx;
}

uint32_t creative_pattern_db_find_similar(const creative_pattern_extractor_t* ext,
                                           const extracted_pattern_t* query,
                                           uint32_t max_results,
                                           extracted_pattern_t* results) {
    if (!ext || !query || !results) return 0;

    /* Find patterns similar to query */
    typedef struct {
        uint32_t idx;
        float sim;
    } scored_t;

    scored_t* scores = nimcp_calloc(ext->num_known_patterns, sizeof(scored_t));
    if (!scores) return 0;

    for (uint32_t i = 0; i < ext->num_known_patterns; i++) {
        scores[i].idx = i;

        if (ext->known_patterns[i].category == query->category &&
            ext->known_patterns[i].feature_vector && query->feature_vector) {

            scores[i].sim = compute_feature_similarity(
                ext->known_patterns[i].feature_vector,
                query->feature_vector,
                ext->known_patterns[i].feature_dim);
        } else {
            scores[i].sim = 0.0f;
        }
    }

    /* Sort by similarity */
    for (uint32_t i = 0; i + 1 < ext->num_known_patterns; i++) {
        for (uint32_t j = 0; j < ext->num_known_patterns - i - 1; j++) {
            if (scores[j].sim < scores[j + 1].sim) {
                scored_t tmp = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = tmp;
            }
        }
    }

    /* Copy top results — deep-copy feature_vector to avoid aliasing extractor's heap pointer */
    uint32_t count = 0;
    for (uint32_t i = 0; i < ext->num_known_patterns && count < max_results; i++) {
        if (scores[i].sim >= ext->config.min_distinctiveness) {
            const extracted_pattern_t* src = &ext->known_patterns[scores[i].idx];
            results[count] = *src;
            if (src->feature_vector && src->feature_dim > 0) {
                results[count].feature_vector = nimcp_calloc(src->feature_dim, sizeof(float));
                if (results[count].feature_vector) {
                    memcpy(results[count].feature_vector, src->feature_vector,
                           src->feature_dim * sizeof(float));
                } else {
                    results[count].feature_vector = NULL;
                    results[count].feature_dim = 0;
                }
            } else {
                results[count].feature_vector = NULL;
            }
            count++;
        }
    }

    nimcp_free(scores);
    scores = NULL;

    return count;
}

uint32_t creative_pattern_db_by_category(const creative_pattern_extractor_t* ext,
                                          creative_pattern_category_t category,
                                          uint32_t max_results,
                                          extracted_pattern_t* results) {
    if (!ext || !results) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < ext->num_known_patterns && count < max_results; i++) {
        if (ext->known_patterns[i].category == category) {
            const extracted_pattern_t* src = &ext->known_patterns[i];
            results[count] = *src;
            /* Deep-copy feature_vector to avoid aliasing extractor's heap pointer */
            if (src->feature_vector && src->feature_dim > 0) {
                results[count].feature_vector = nimcp_calloc(src->feature_dim, sizeof(float));
                if (results[count].feature_vector) {
                    memcpy(results[count].feature_vector, src->feature_vector,
                           src->feature_dim * sizeof(float));
                } else {
                    results[count].feature_vector = NULL;
                    results[count].feature_dim = 0;
                }
            } else {
                results[count].feature_vector = NULL;
            }
            count++;
        }
    }

    return count;
}

//=============================================================================
// Analysis API
//=============================================================================

void creative_pattern_analyze_distribution(const creative_pattern_extractor_t* ext,
                                            const pattern_extraction_result_t* result,
                                            uint32_t* category_counts,
                                            uint32_t* level_counts) {
    if (!ext || !result) return;
    (void)ext;

    if (category_counts) {
        memset(category_counts, 0, 80 * sizeof(uint32_t));
    }
    if (level_counts) {
        memset(level_counts, 0, 5 * sizeof(uint32_t));
    }

    for (uint32_t i = 0; i < result->num_patterns; i++) {
        if (category_counts) {
            int cat = (int)result->patterns[i].category;
            if (cat >= 0 && cat < 80) {
                category_counts[cat]++;
            }
        }
        if (level_counts) {
            int lvl = (int)result->patterns[i].level;
            if (lvl >= 0 && lvl < 5) {
                level_counts[lvl]++;
            }
        }
    }
}

uint32_t creative_pattern_top_n(const pattern_extraction_result_t* result,
                                 uint32_t n,
                                 uint32_t* indices) {
    if (!result || !indices || result->num_patterns == 0) return 0;

    /* Sort by importance */
    typedef struct {
        uint32_t idx;
        float importance;
    } scored_t;

    scored_t* scores = nimcp_calloc(result->num_patterns, sizeof(scored_t));
    if (!scores) return 0;

    for (uint32_t i = 0; i < result->num_patterns; i++) {
        scores[i].idx = i;
        scores[i].importance = result->patterns[i].importance;
    }

    /* Sort */
    for (uint32_t i = 0; i + 1 < result->num_patterns; i++) {
        for (uint32_t j = 0; j < result->num_patterns - i - 1; j++) {
            if (scores[j].importance < scores[j + 1].importance) {
                scored_t tmp = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = tmp;
            }
        }
    }

    uint32_t count = n < result->num_patterns ? n : result->num_patterns;
    for (uint32_t i = 0; i < count; i++) {
        indices[i] = scores[i].idx;
    }

    nimcp_free(scores);
    scores = NULL;

    return count;
}

float creative_pattern_uniqueness(const creative_pattern_extractor_t* ext,
                                   const extracted_pattern_t* pattern) {
    if (!ext || !pattern || !pattern->feature_vector) return 1.0f;

    float max_sim = 0.0f;

    for (uint32_t i = 0; i < ext->num_known_patterns; i++) {
        if (ext->known_patterns[i].category == pattern->category &&
            ext->known_patterns[i].feature_vector) {

            float sim = compute_feature_similarity(
                pattern->feature_vector,
                ext->known_patterns[i].feature_vector,
                pattern->feature_dim);

            if (sim > max_sim) {
                max_sim = sim;
            }
        }
    }

    return 1.0f - max_sim;
}

//=============================================================================
// Cortical Integration API
//=============================================================================

void creative_pattern_extractor_set_cortical_columns(
    creative_pattern_extractor_t* ext, void* cortical_columns) {
    if (!ext) return;
    ext->cortical_columns = cortical_columns;
}

//=============================================================================
// Cleanup
//=============================================================================

void pattern_extraction_result_free(pattern_extraction_result_t* result) {
    if (!result) return;

    if (result->patterns) {
        for (uint32_t i = 0; i < result->num_patterns; i++) {
            extracted_pattern_free(&result->patterns[i]);
        }
        nimcp_free(result->patterns);
        result->patterns = NULL;
    }
    result->num_patterns = 0;
}

void extracted_pattern_free(extracted_pattern_t* pattern) {
    if (!pattern) return;

    if (pattern->feature_vector) {
        nimcp_free(pattern->feature_vector);
        pattern->feature_vector = NULL;
    }
    pattern->feature_dim = 0;
}
