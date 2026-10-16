//=============================================================================
// nimcp_style_perception.c - Style Recognition and Analysis
//=============================================================================
/**
 * @file nimcp_style_perception.c
 * @brief Perceives and identifies artistic styles in content
 *
 * WHAT: Recognizes artistic styles across modalities
 * WHY:  Enable understanding of artistic language and conventions
 * HOW:  Pattern matching against known styles, neural style analysis
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/appreciation/nimcp_style_perception.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "STYLE_PERCEPTION"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(style_perception, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define MAX_EVOLUTION_POINTS 1024

//=============================================================================
// Archetype Descriptions (Static Data)
//=============================================================================

static const char* literary_archetype_names[] = {
    "Hemingway", "Tolstoy", "Joyce", "Poe", "Austen",
    "Shakespeare", "Borges", "Kafka", "Marquez",
    "Dostoevsky", "Woolf", "Faulkner"
};

static const char* literary_archetype_descriptions[] = {
    "Minimalist prose, short sentences, iceberg theory, masculine themes",
    "Epic scope, psychological depth, moral philosophy, Russian realism",
    "Stream of consciousness, experimental structure, Dublin settings",
    "Gothic horror, macabre themes, psychological terror, poetry",
    "Social comedy, romantic irony, Regency England, free indirect discourse",
    "Iambic pentameter, complex wordplay, timeless human themes",
    "Magical realism, labyrinths, infinite libraries, philosophical puzzles",
    "Absurdist, bureaucratic nightmares, alienation, transformation",
    "Magical realism, Colombian settings, multi-generational sagas",
    "Psychological depth, moral complexity, existential questioning",
    "Interior monologue, impressionistic, fragmented time",
    "Southern gothic, temporal shifts, multiple narrators"
};

static const char* music_archetype_names[] = {
    "Bach", "Beethoven", "Debussy", "John Williams",
    "Miles Davis", "Hans Zimmer", "Stravinsky", "Ennio Morricone",
    "Sakamoto", "Glass", "Copland", "Ravel"
};

static const char* music_archetype_descriptions[] = {
    "Baroque counterpoint, fugues, mathematical precision, religious works",
    "Classical/Romantic bridge, heroic themes, symphonic development",
    "Impressionism, whole-tone scales, atmospheric textures, French",
    "Leitmotifs, orchestral film scores, heroic fanfares, adventure themes",
    "Jazz improvisation, modal jazz, cool jazz, Kind of Blue",
    "Electronic-orchestral hybrid, pulsing rhythms, epic film scores",
    "Modernist, rhythmic complexity, dissonant harmonies, ballet scores",
    "Western film scores, eclectic instrumentation, memorable themes",
    "Minimalist, ambient, electronic-classical fusion, contemplative",
    "Minimalist, repetitive arpeggios, hypnotic, opera and film scores",
    "American pastoral, folk-influenced, open harmonies, Appalachian Spring",
    "Orchestral color, precision, jazz influence, Bolero"
};

static const char* visual_archetype_names[] = {
    "Van Gogh", "Monet", "Picasso", "Dali",
    "Warhol", "Rembrandt", "Klimt", "Escher",
    "Hokusai", "Basquiat", "Caravaggio", "Kandinsky"
};

static const char* visual_archetype_descriptions[] = {
    "Post-Impressionism, swirling brushwork, vivid colors, emotional intensity",
    "Impressionism, light studies, water lilies, plein air painting",
    "Cubism, multiple perspectives, geometric abstraction, periods",
    "Surrealism, melting clocks, dreamscapes, paranoid-critical method",
    "Pop art, celebrity culture, mass production, screen printing",
    "Baroque, chiaroscuro, Dutch Golden Age, psychological portraits",
    "Art nouveau, gold leaf, decorative patterns, symbolism",
    "Mathematical art, impossible geometry, tessellated patterns",
    "Ukiyo-e, nature studies, dynamic lines, Great Wave",
    "Neo-expressionist, raw energy, symbolic, street art influence",
    "Dramatic lighting, realism, intense chiaroscuro, Baroque",
    "Abstract, color theory, musical analogies, geometric composition"
};

static const char* cinema_archetype_names[] = {
    "Kubrick", "Spielberg", "Tarantino", "Nolan",
    "Tarkovsky", "Miyazaki", "Hitchcock", "Welles",
    "Kurosawa", "Fincher", "Villeneuve", "Coppola"
};

static const char* cinema_archetype_descriptions[] = {
    "Meticulous framing, one-point perspective, cold beauty, humanity themes",
    "Blockbuster entertainment, wonder, family themes, emotional manipulation",
    "Non-linear narrative, pop culture dialogue, violence stylization",
    "Mind-bending plots, practical effects, time manipulation, IMAX",
    "Poetic cinema, long takes, spiritual themes, Russian art film",
    "Hand-drawn animation, flight motifs, environmentalism, Studio Ghibli",
    "Suspense master, psychological tension, voyeuristic camera, audience manipulation",
    "Deep focus cinematography, innovative camera angles, ambitious storytelling",
    "Epic samurai dramas, weather as character, humanist philosophy, movement",
    "Dark aesthetic, meticulous detail, psychological thrillers, digital precision",
    "Atmospheric slow-burn, immersive visuals, grand scale, philosophical sci-fi",
    "Epic family sagas, chiaroscuro lighting, operatic storytelling, moral complexity"
};

//=============================================================================
// Config Defaults
//=============================================================================

void style_perception_config_defaults(style_perception_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(style_perception_config_t));

    /* Matching settings */
    config->match_threshold = 0.3f;
    config->max_matches = 5;
    config->detect_hybrids = true;
    config->hybrid_threshold = 0.25f;

    /* Analysis settings */
    config->extract_embedding = true;
    config->embedding_dim = 256;
    config->track_evolution = false;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static uint32_t get_archetype_count(art_modality_t modality) {
    if (art_modality_is_text(modality)) return STYLE_LIT_COUNT;
    if (art_modality_category(modality) == 1) return STYLE_MUSIC_COUNT;
    if (art_modality_category(modality) == 2) return STYLE_VIS_COUNT;
    if (art_modality_category(modality) == 3) {
        return STYLE_CINEMA_COUNT;
    }
    return 0;
}

static void initialize_archetype_embedding(style_embedding_t* emb, uint32_t dim,
                                            int32_t archetype_id, art_modality_t modality) {
    /* Initialize with deterministic pseudo-random values based on archetype */
    style_embedding_create(emb, dim);

    uint32_t seed = (uint32_t)(modality * 100 + archetype_id);
    for (uint32_t i = 0; i < dim; i++) {
        /* Simple PRNG for deterministic initialization */
        seed = seed * 1103515245 + 12345;
        emb->embedding[i] = ((float)(seed % 1000) / 500.0f) - 1.0f;
    }

    style_embedding_normalize(emb);
}

static float compute_text_features(const char* text, size_t len, float* features, uint32_t max_features) {
    if (!text || len == 0 || !features) return 0.0f;

    /* Extract simple text features */
    uint32_t num_features = max_features < 10 ? max_features : 10;

    /* Feature 0: Average word length */
    uint32_t words = 0;
    uint32_t word_len_sum = 0;
    uint32_t current_word_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == ' ' || text[i] == '\n' || text[i] == '\t') {
            if (current_word_len > 0) {
                word_len_sum += current_word_len;
                words++;
                current_word_len = 0;
            }
        } else {
            current_word_len++;
        }
    }
    if (current_word_len > 0) {
        word_len_sum += current_word_len;
        words++;
    }
    features[0] = words > 0 ? (float)word_len_sum / words / 10.0f : 0.0f;

    /* Feature 1: Sentence length (periods per word) */
    uint32_t periods = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '.') periods++;
    }
    features[1] = words > 0 ? (float)words / (periods + 1) / 20.0f : 0.0f;

    /* Feature 2: Punctuation density */
    uint32_t punct = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == ',' || c == ';' || c == ':' || c == '-' || c == '!' || c == '?') {
            punct++;
        }
    }
    features[2] = len > 0 ? (float)punct / len * 10.0f : 0.0f;

    /* Feature 3: Vocabulary richness (unique chars) */
    uint8_t seen[256] = {0};
    uint32_t unique = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)text[i];
        if (!seen[c]) {
            seen[c] = 1;
            unique++;
        }
    }
    features[3] = (float)unique / 128.0f;

    /* Feature 4-9: Character frequency features */
    for (uint32_t f = 4; f < num_features && f < max_features; f++) {
        char target = 'a' + (f - 4);
        uint32_t count = 0;
        for (size_t i = 0; i < len; i++) {
            char c = text[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c == target) count++;
        }
        features[f] = (float)count / (len > 0 ? len : 1) * 10.0f;
    }

    return (float)num_features;
}

//=============================================================================
// Lifecycle API
//=============================================================================

style_perception_t* style_perception_create(
    const style_perception_config_t* config) {

    style_perception_t* perc = nimcp_calloc(1, sizeof(style_perception_t));
    if (!perc) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate style perception");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_text_features: perc is NULL");
        return NULL;
    }

    if (config) {
        perc->config = *config;
    } else {
        style_perception_config_defaults(&perc->config);
    }

    uint32_t dim = perc->config.embedding_dim;

    /* Initialize literary archetypes */
    perc->literary_archetypes = nimcp_calloc(STYLE_LIT_COUNT, sizeof(style_embedding_t));
    if (perc->literary_archetypes) {
        for (int i = 0; i < STYLE_LIT_COUNT; i++) {
            initialize_archetype_embedding(&perc->literary_archetypes[i], dim,
                                           i, ART_MODALITY_TEXT_POETRY);
        }
    }

    /* Initialize music archetypes */
    perc->music_archetypes = nimcp_calloc(STYLE_MUSIC_COUNT, sizeof(style_embedding_t));
    if (perc->music_archetypes) {
        for (int i = 0; i < STYLE_MUSIC_COUNT; i++) {
            initialize_archetype_embedding(&perc->music_archetypes[i], dim,
                                           i, ART_MODALITY_MUSIC_CLASSICAL);
        }
    }

    /* Initialize visual archetypes */
    perc->visual_archetypes = nimcp_calloc(STYLE_VIS_COUNT, sizeof(style_embedding_t));
    if (perc->visual_archetypes) {
        for (int i = 0; i < STYLE_VIS_COUNT; i++) {
            initialize_archetype_embedding(&perc->visual_archetypes[i], dim,
                                           i, ART_MODALITY_VISUAL_PAINTING);
        }
    }

    /* Initialize cinema archetypes */
    perc->cinema_archetypes = nimcp_calloc(STYLE_CINEMA_COUNT, sizeof(style_embedding_t));
    if (perc->cinema_archetypes) {
        for (int i = 0; i < STYLE_CINEMA_COUNT; i++) {
            initialize_archetype_embedding(&perc->cinema_archetypes[i], dim,
                                           i, ART_MODALITY_VIDEO_CINEMA);
        }
    }

    LOG_INFO(LOG_MODULE, "Style perception created (dim=%u)", dim);

    return perc;
}

void style_perception_destroy(style_perception_t* perc) {
    if (!perc) return;

    /* Free archetype embeddings */
    if (perc->literary_archetypes) {
        for (int i = 0; i < STYLE_LIT_COUNT; i++) {
            style_embedding_destroy(&perc->literary_archetypes[i]);
        }
        nimcp_free(perc->literary_archetypes);
    }

    if (perc->music_archetypes) {
        for (int i = 0; i < STYLE_MUSIC_COUNT; i++) {
            style_embedding_destroy(&perc->music_archetypes[i]);
        }
        nimcp_free(perc->music_archetypes);
    }

    if (perc->visual_archetypes) {
        for (int i = 0; i < STYLE_VIS_COUNT; i++) {
            style_embedding_destroy(&perc->visual_archetypes[i]);
        }
        nimcp_free(perc->visual_archetypes);
    }

    if (perc->cinema_archetypes) {
        for (int i = 0; i < STYLE_CINEMA_COUNT; i++) {
            style_embedding_destroy(&perc->cinema_archetypes[i]);
        }
        nimcp_free(perc->cinema_archetypes);
    }

    /* Free evolution tracking */
    if (perc->current_evolution) {
        style_evolution_free(perc->current_evolution);
        nimcp_free(perc->current_evolution);
    }

    nimcp_free(perc);
    perc = NULL;

    LOG_INFO(LOG_MODULE, "Style perception destroyed");
}

//=============================================================================
// Analysis API
//=============================================================================

int style_perception_analyze(style_perception_t* perc,
                              const void* content,
                              art_modality_t modality,
                              style_analysis_result_t* result) {
    if (!perc || !content || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_destroy: required parameter is NULL (perc, content, result)");
        return -1;
    }

    memset(result, 0, sizeof(style_analysis_result_t));

    /* Get appropriate archetypes */
    style_embedding_t* archetypes = NULL;
    uint32_t num_archetypes = 0;

    if (art_modality_is_text(modality)) {
        archetypes = perc->literary_archetypes;
        num_archetypes = STYLE_LIT_COUNT;
    } else if (art_modality_category(modality) == 1) {
        archetypes = perc->music_archetypes;
        num_archetypes = STYLE_MUSIC_COUNT;
    } else if (art_modality_category(modality) == 2) {
        archetypes = perc->visual_archetypes;
        num_archetypes = STYLE_VIS_COUNT;
    } else if (art_modality_category(modality) == 3) {
        archetypes = perc->cinema_archetypes;
        num_archetypes = STYLE_CINEMA_COUNT;
    }

    if (!archetypes || num_archetypes == 0) {
        LOG_WARN(LOG_MODULE, "No archetypes for modality %d", modality);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "style_perception_destroy: archetypes is NULL");
        return -1;
    }

    /* Extract style embedding from content */
    style_embedding_create(&result->extracted_style, perc->config.embedding_dim);

    /* Simplified: create embedding based on content features */
    if (art_modality_is_text(modality)) {
        float features[256];
        compute_text_features((const char*)content, strlen((const char*)content),
                              features, perc->config.embedding_dim);
        for (uint32_t i = 0; i < perc->config.embedding_dim; i++) {
            result->extracted_style.embedding[i] = features[i % 10] +
                ((float)((i * 7) % 100) / 100.0f - 0.5f);
        }
    } else {
        /* For non-text, use placeholder embedding */
        for (uint32_t i = 0; i < perc->config.embedding_dim; i++) {
            result->extracted_style.embedding[i] = ((float)(i % 100) / 50.0f) - 1.0f;
        }
    }
    style_embedding_normalize(&result->extracted_style);

    /* Compare to archetypes */
    result->matches = nimcp_calloc(num_archetypes, sizeof(style_match_t));
    if (!result->matches) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "style_perception_destroy: result->matches is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < num_archetypes; i++) {
        float sim = style_embedding_similarity(&result->extracted_style, &archetypes[i]);
        result->matches[i].archetype_id = (int32_t)i;
        result->matches[i].modality = modality;
        result->matches[i].similarity = sim;
        result->matches[i].confidence = 0.8f;  /* Fixed confidence for now */

        const char* name = style_perception_archetype_name(modality, i);
        strncpy(result->matches[i].archetype_name, name,
                sizeof(result->matches[i].archetype_name) - 1);
    }
    result->num_matches = num_archetypes;

    /* Sort by similarity (bubble sort for simplicity) */
    for (uint32_t i = 0; i < num_archetypes - 1; i++) {
        for (uint32_t j = 0; j < num_archetypes - i - 1; j++) {
            if (result->matches[j].similarity < result->matches[j + 1].similarity) {
                style_match_t tmp = result->matches[j];
                result->matches[j] = result->matches[j + 1];
                result->matches[j + 1] = tmp;
            }
        }
    }

    /* Detect hybrid style */
    if (perc->config.detect_hybrids && num_archetypes >= 2) {
        float top = result->matches[0].similarity;
        float second = result->matches[1].similarity;
        if (top > 0.0f && second / top >= perc->config.hybrid_threshold) {
            result->is_hybrid = true;
            result->hybrid_coherence = 1.0f - (top - second);
        }
    }

    /* Compute originality (distance from all archetypes) */
    float avg_sim = 0.0f;
    for (uint32_t i = 0; i < num_archetypes; i++) {
        avg_sim += result->matches[i].similarity;
    }
    avg_sim /= num_archetypes;
    result->originality = 1.0f - avg_sim;

    perc->analyses_performed++;

    return 0;
}

int style_perception_analyze_text(style_perception_t* perc,
                                   const char* text, size_t len,
                                   style_analysis_result_t* result) {
    if (!perc || !text || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_destroy: required parameter is NULL (perc, text, result)");
        return -1;
    }
    (void)len;

    return style_perception_analyze(perc, text, ART_MODALITY_TEXT_POETRY, result);
}

int style_perception_analyze_music(style_perception_t* perc,
                                    const music_track_t* tracks, uint32_t num_tracks,
                                    style_analysis_result_t* result) {
    if (!perc || !tracks || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_destroy: required parameter is NULL (perc, tracks, result)");
        return -1;
    }
    (void)num_tracks;

    return style_perception_analyze(perc, tracks, ART_MODALITY_MUSIC_CLASSICAL, result);
}

int style_perception_analyze_visual(style_perception_t* perc,
                                     const visual_image_t* image,
                                     style_analysis_result_t* result) {
    if (!perc || !image || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_destroy: required parameter is NULL (perc, image, result)");
        return -1;
    }

    return style_perception_analyze(perc, image, ART_MODALITY_VISUAL_PAINTING, result);
}

//=============================================================================
// Comparison API
//=============================================================================

float style_perception_compare(const style_perception_t* perc,
                                const style_embedding_t* style_a,
                                const style_embedding_t* style_b) {
    (void)perc;
    return style_embedding_similarity(style_a, style_b);
}

int style_perception_closest_archetype(const style_perception_t* perc,
                                        const style_embedding_t* style,
                                        style_match_t* out) {
    if (!perc || !style || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_destroy: required parameter is NULL (perc, style, out)");
        return -1;
    }

    float best_sim = -1.0f;
    int32_t best_id = -1;
    art_modality_t best_modality = ART_MODALITY_TEXT_POETRY;
    style_embedding_t* best_archetypes = NULL;

    /* Check literary */
    if (perc->literary_archetypes) {
        for (int i = 0; i < STYLE_LIT_COUNT; i++) {
            float sim = style_embedding_similarity(style, &perc->literary_archetypes[i]);
            if (sim > best_sim) {
                best_sim = sim;
                best_id = i;
                best_modality = ART_MODALITY_TEXT_POETRY;
                best_archetypes = perc->literary_archetypes;
            }
        }
    }

    /* Check music */
    if (perc->music_archetypes) {
        for (int i = 0; i < STYLE_MUSIC_COUNT; i++) {
            float sim = style_embedding_similarity(style, &perc->music_archetypes[i]);
            if (sim > best_sim) {
                best_sim = sim;
                best_id = i;
                best_modality = ART_MODALITY_MUSIC_CLASSICAL;
                best_archetypes = perc->music_archetypes;
            }
        }
    }

    /* Check visual */
    if (perc->visual_archetypes) {
        for (int i = 0; i < STYLE_VIS_COUNT; i++) {
            float sim = style_embedding_similarity(style, &perc->visual_archetypes[i]);
            if (sim > best_sim) {
                best_sim = sim;
                best_id = i;
                best_modality = ART_MODALITY_VISUAL_PAINTING;
                best_archetypes = perc->visual_archetypes;
            }
        }
    }

    /* Check cinema */
    if (perc->cinema_archetypes) {
        for (int i = 0; i < STYLE_CINEMA_COUNT; i++) {
            float sim = style_embedding_similarity(style, &perc->cinema_archetypes[i]);
            if (sim > best_sim) {
                best_sim = sim;
                best_id = i;
                best_modality = ART_MODALITY_VIDEO_CINEMA;
                best_archetypes = perc->cinema_archetypes;
            }
        }
    }

    (void)best_archetypes;

    if (best_id < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "style_perception_closest_archetype: no matching archetype found");
        return -1;
    }

    out->archetype_id = best_id;
    out->modality = best_modality;
    out->similarity = best_sim;
    out->confidence = 0.8f;
    strncpy(out->archetype_name, style_perception_archetype_name(best_modality, best_id),
            sizeof(out->archetype_name) - 1);

    return 0;
}

uint32_t style_perception_decompose(const style_perception_t* perc,
                                     const style_embedding_t* style,
                                     float* weights,
                                     uint32_t max_archetypes) {
    if (!perc || !style || !weights) return 0;

    /* Get similarities to all archetypes (assumes same modality) */
    /* For simplicity, use literary archetypes */
    uint32_t count = STYLE_LIT_COUNT < max_archetypes ? STYLE_LIT_COUNT : max_archetypes;

    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float sim = style_embedding_similarity(style, &perc->literary_archetypes[i]);
        weights[i] = sim > 0 ? sim : 0.0f;
        sum += weights[i];
    }

    /* Normalize to sum to 1 */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < count; i++) {
            weights[i] /= sum;
        }
    }

    /* Count non-zero weights */
    uint32_t non_zero = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (weights[i] > 0.01f) non_zero++;
    }

    return non_zero;
}

//=============================================================================
// Evolution Tracking API
//=============================================================================

int style_perception_start_evolution_tracking(style_perception_t* perc) {
    if (!perc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_start_evolution_tracking: perc is NULL");
        return -1;
    }

    if (perc->current_evolution) {
        style_evolution_free(perc->current_evolution);
        nimcp_free(perc->current_evolution);
    }

    perc->current_evolution = nimcp_calloc(1, sizeof(style_evolution_t));
    if (!perc->current_evolution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "style_perception_start_evolution_tracking: perc->current_evolution is NULL");
        return -1;
    }

    perc->current_evolution->timeline = nimcp_calloc(MAX_EVOLUTION_POINTS,
                                                      sizeof(style_embedding_t));
    perc->current_evolution->timestamps = nimcp_calloc(MAX_EVOLUTION_POINTS, sizeof(float));

    if (!perc->current_evolution->timeline || !perc->current_evolution->timestamps) {
        style_evolution_free(perc->current_evolution);
        nimcp_free(perc->current_evolution);
        perc->current_evolution = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "style_perception_start_evolution_tracking: required parameter is NULL (perc->current_evolution->timeline, perc->current_evolution->timestamps)");
        return -1;
    }

    return 0;
}

int style_perception_add_evolution_point(style_perception_t* perc,
                                          const style_embedding_t* style,
                                          float timestamp) {
    if (!perc || !style || !perc->current_evolution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_start_evolution_tracking: required parameter is NULL (perc, style, perc->current_evolution)");
        return -1;
    }

    if (perc->current_evolution->num_points >= MAX_EVOLUTION_POINTS) {
        LOG_WARN(LOG_MODULE, "Evolution tracking full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "style_perception_start_evolution_tracking: capacity exceeded");
        return -1;
    }

    uint32_t idx = perc->current_evolution->num_points;

    /* Clone the style */
    style_embedding_clone(style, &perc->current_evolution->timeline[idx]);
    perc->current_evolution->timestamps[idx] = timestamp;
    perc->current_evolution->num_points++;

    /* Update drift metrics */
    if (idx > 0) {
        float dist = 1.0f - style_embedding_similarity(
            &perc->current_evolution->timeline[idx],
            &perc->current_evolution->timeline[idx - 1]);
        perc->current_evolution->total_drift += dist;

        float time_diff = timestamp - perc->current_evolution->timestamps[idx - 1];
        if (time_diff > 0.0f) {
            perc->current_evolution->drift_rate = dist / time_diff;
        }

        /* Check for dramatic shift */
        if (dist > 0.5f && !perc->current_evolution->has_dramatic_shift) {
            perc->current_evolution->has_dramatic_shift = true;
            perc->current_evolution->shift_point = idx;
        }
    }

    return 0;
}

int style_perception_get_evolution(const style_perception_t* perc,
                                    style_evolution_t* out) {
    if (!perc || !out || !perc->current_evolution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_start_evolution_tracking: required parameter is NULL (perc, out, perc->current_evolution)");
        return -1;
    }

    const style_evolution_t* src = perc->current_evolution;

    /* Deep copy scalar fields */
    out->num_points = src->num_points;
    out->total_drift = src->total_drift;
    out->drift_rate = src->drift_rate;
    out->has_dramatic_shift = src->has_dramatic_shift;
    out->shift_point = src->shift_point;
    out->timeline = NULL;
    out->timestamps = NULL;

    /* Deep copy timeline (array of style_embedding_t, each with float* embedding) */
    if (src->timeline && src->num_points > 0) {
        out->timeline = nimcp_calloc(src->num_points, sizeof(style_embedding_t));
        if (!out->timeline) return -1;
        for (uint32_t i = 0; i < src->num_points; i++) {
            style_embedding_clone(&src->timeline[i], &out->timeline[i]);
        }
    }

    /* Deep copy timestamps */
    if (src->timestamps && src->num_points > 0) {
        out->timestamps = nimcp_calloc(src->num_points, sizeof(float));
        if (!out->timestamps) {
            /* Clean up already-cloned timeline on failure */
            if (out->timeline) {
                for (uint32_t i = 0; i < src->num_points; i++) {
                    style_embedding_destroy(&out->timeline[i]);
                }
                nimcp_free(out->timeline);
                out->timeline = NULL;
            }
            return -1;
        }
        memcpy(out->timestamps, src->timestamps, src->num_points * sizeof(float));
    }

    return 0;
}

void style_perception_stop_evolution_tracking(style_perception_t* perc) {
    if (!perc || !perc->current_evolution) return;

    style_evolution_free(perc->current_evolution);
    nimcp_free(perc->current_evolution);
    perc->current_evolution = NULL;
}

//=============================================================================
// Archetype Info API
//=============================================================================

const char* style_perception_archetype_name(art_modality_t modality,
                                             int32_t archetype_id) {
    if (archetype_id < 0) return "Unknown";

    if (art_modality_is_text(modality)) {
        if (archetype_id < STYLE_LIT_COUNT) {
            return literary_archetype_names[archetype_id];
        }
    } else if (art_modality_category(modality) == 1) {
        if (archetype_id < STYLE_MUSIC_COUNT) {
            return music_archetype_names[archetype_id];
        }
    } else if (art_modality_category(modality) == 2) {
        if (archetype_id < STYLE_VIS_COUNT) {
            return visual_archetype_names[archetype_id];
        }
    } else if (art_modality_category(modality) == 3) {
        if (archetype_id < STYLE_CINEMA_COUNT) {
            return cinema_archetype_names[archetype_id];
        }
    }

    return "Unknown";
}

const char* style_perception_archetype_description(art_modality_t modality,
                                                    int32_t archetype_id) {
    if (archetype_id < 0) return "No description available";

    if (art_modality_is_text(modality)) {
        if (archetype_id < STYLE_LIT_COUNT) {
            return literary_archetype_descriptions[archetype_id];
        }
    } else if (art_modality_category(modality) == 1) {  /* Music (10-19) */
        if (archetype_id < STYLE_MUSIC_COUNT) {
            return music_archetype_descriptions[archetype_id];
        }
    } else if (art_modality_category(modality) == 2) {  /* Visual (20-29) */
        if (archetype_id < STYLE_VIS_COUNT) {
            return visual_archetype_descriptions[archetype_id];
        }
    } else if (art_modality_category(modality) == 3) {  /* Video (30-39) */
        if (archetype_id < STYLE_CINEMA_COUNT) {
            return cinema_archetype_descriptions[archetype_id];
        }
    }

    return "No description available";
}

int style_perception_get_archetype(const style_perception_t* perc,
                                    art_modality_t modality,
                                    int32_t archetype_id,
                                    style_embedding_t* out) {
    if (!perc || !out || archetype_id < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "style_perception_stop_evolution_tracking: required parameter is NULL (perc, out)");
        return -1;
    }

    style_embedding_t* archetypes = NULL;
    uint32_t max_id = 0;

    if (art_modality_is_text(modality)) {
        archetypes = perc->literary_archetypes;
        max_id = STYLE_LIT_COUNT;
    } else if (art_modality_category(modality) == 1) {  /* Music (10-19) */
        archetypes = perc->music_archetypes;
        max_id = STYLE_MUSIC_COUNT;
    } else if (art_modality_category(modality) == 2) {  /* Visual (20-29) */
        archetypes = perc->visual_archetypes;
        max_id = STYLE_VIS_COUNT;
    } else if (art_modality_category(modality) == 3) {  /* Video (30-39) */
        archetypes = perc->cinema_archetypes;
        max_id = STYLE_CINEMA_COUNT;
    }

    if (!archetypes || (uint32_t)archetype_id >= max_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "style_perception_stop_evolution_tracking: archetypes is NULL");
        return -1;
    }

    return style_embedding_clone(&archetypes[archetype_id], out);
}

uint32_t style_perception_archetype_count(art_modality_t modality) {
    return get_archetype_count(modality);
}

//=============================================================================
// Cortical Integration API
//=============================================================================

void style_perception_set_visual_cortex(style_perception_t* perc, void* visual_cortex) {
    if (!perc) return;
    perc->visual_cortex = visual_cortex;
}

void style_perception_set_audio_cortex(style_perception_t* perc, void* audio_cortex) {
    if (!perc) return;
    perc->audio_cortex = audio_cortex;
}

void style_perception_set_speech_cortex(style_perception_t* perc, void* speech_cortex) {
    if (!perc) return;
    perc->speech_cortex = speech_cortex;
}

//=============================================================================
// Cleanup
//=============================================================================

void style_analysis_result_free(style_analysis_result_t* result) {
    if (!result) return;

    if (result->matches) {
        nimcp_free(result->matches);
        result->matches = NULL;
    }
    result->num_matches = 0;

    style_embedding_destroy(&result->extracted_style);
}

void style_evolution_free(style_evolution_t* evolution) {
    if (!evolution) return;

    if (evolution->timeline) {
        for (uint32_t i = 0; i < evolution->num_points; i++) {
            style_embedding_destroy(&evolution->timeline[i]);
        }
        nimcp_free(evolution->timeline);
        evolution->timeline = NULL;
    }

    if (evolution->timestamps) {
        nimcp_free(evolution->timestamps);
        evolution->timestamps = NULL;
    }

    evolution->num_points = 0;
}
