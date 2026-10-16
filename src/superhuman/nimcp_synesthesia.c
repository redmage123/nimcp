/**
 * @file nimcp_synesthesia.c
 * @brief Implementation of superhuman cross-modal perception module
 *
 * WHAT: Provides synesthesia-like cross-modal sensory associations
 * WHY:  Enable enhanced perception through automatic sensory binding
 * HOW:  Learned cross-modal mappings, concurrent sensory activation
 *
 * @version Phase T12: Superhuman Enhancement Modules
 * @date 2026-01-13
 */

#include "superhuman/nimcp_synesthesia.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synesthesia)

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define SYNESTHESIA_LOG_MODULE "SYNESTHESIA"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Association entry in storage
 */
typedef struct association_entry {
    cross_modal_association_t association;
    struct association_entry* hash_next;
} association_entry_t;

/**
 * @brief Grapheme-color entry
 */
typedef struct grapheme_color_entry {
    grapheme_color_mapping_t mapping;
    struct grapheme_color_entry* next;
} grapheme_color_entry_t;

/**
 * @brief Sound-shape entry
 */
typedef struct sound_shape_entry {
    sound_shape_mapping_t mapping;
    struct sound_shape_entry* next;
} sound_shape_entry_t;

/**
 * @brief Internal module structure
 */
struct synesthesia_module {
    /* Configuration */
    synesthesia_config_t config;

    /* Generic association storage (hash table by ID) */
    association_entry_t** association_store;
    uint32_t store_capacity;
    uint64_t association_count;
    uint64_t next_association_id;

    /* Grapheme-color mappings (indexed by codepoint) */
    grapheme_color_entry_t** grapheme_colors;
    uint32_t grapheme_color_capacity;
    uint32_t grapheme_color_count;

    /* Sound-shape mappings (list) */
    sound_shape_entry_t* sound_shapes;
    uint32_t sound_shape_count;

    /* Taste-touch mappings stored in generic association store */

    /* State */
    bool inhibited;
    synesthesia_status_t status;
    synesthesia_error_t last_error;

    /* Callbacks */
    synesthesia_experience_callback_t experience_callback;
    void* experience_user_data;
    synesthesia_cascade_callback_t cascade_callback;
    void* cascade_user_data;
    synesthesia_learning_callback_t learning_callback;
    void* learning_user_data;

    /* Statistics */
    synesthesia_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(synesthesia_module_t* module, synesthesia_error_t error) {
    if (!module) return;
    module->last_error = error;
    if (error != SYNESTHESIA_ERROR_NONE) {
        module->status = SYNESTHESIA_STATUS_ERROR;
        LOG_ERROR("[%s] Error: %d", SYNESTHESIA_LOG_MODULE, error);
    }
}

/**
 * @brief Hash function for association ID
 */
static uint32_t hash_association_id(uint64_t id, uint32_t capacity) {
    return (uint32_t)(id % capacity);
}

/**
 * @brief Hash function for codepoint
 */
static uint32_t hash_codepoint(uint32_t codepoint, uint32_t capacity) {
    return codepoint % capacity;
}

/**
 * @brief Compute feature similarity (cosine similarity)
 */
static float compute_similarity(const float* a, const float* b, uint32_t size) {
    if (!a || !b || size == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    return denom > 0.0f ? dot / denom : 0.0f;
}

/**
 * @brief Free association entry
 */
static void free_association_entry(association_entry_t* entry) {
    if (!entry) return;

    if (entry->association.inducer_features) {
        nimcp_free(entry->association.inducer_features);
    }
    if (entry->association.concurrent_features) {
        nimcp_free(entry->association.concurrent_features);
    }

    nimcp_free(entry);
}

/**
 * @brief Free grapheme-color entry
 */
static void free_grapheme_color_entry(grapheme_color_entry_t* entry) {
    if (!entry) return;
    if (entry->mapping.grapheme.visual_features) {
        nimcp_free(entry->mapping.grapheme.visual_features);
    }
    nimcp_free(entry);
}

/**
 * @brief Free sound-shape entry
 */
static void free_sound_shape_entry(sound_shape_entry_t* entry) {
    if (!entry) return;
    if (entry->mapping.sound.timbre) {
        nimcp_free(entry->mapping.sound.timbre);
    }
    if (entry->mapping.shape.contour) {
        nimcp_free(entry->mapping.shape.contour);
    }
    nimcp_free(entry);
}

/**
 * @brief Compute bouba-kiki score for sound features
 */
static float compute_bouba_kiki(const synesthesia_sound_t* sound) {
    if (!sound) return 0.0f;

    /* Higher pitch, sharper attack = more "kiki"
     * Lower pitch, softer attack = more "bouba" */
    float pitch_factor = (sound->pitch - 0.5f) * 2.0f;  /* Normalize around 0 */
    float attack_factor = sound->attack * 2.0f - 1.0f;
    float loudness_factor = sound->loudness - 0.5f;

    return fminf(1.0f, fmaxf(-1.0f,
        pitch_factor * 0.4f + attack_factor * 0.4f + loudness_factor * 0.2f));
}

/**
 * @brief Generate shape parameters from bouba-kiki score
 */
static void generate_shape_from_score(float score, synesthesia_shape_t* shape) {
    if (!shape) return;

    /* Bouba (score < 0): rounded, smooth, larger
     * Kiki (score > 0): angular, sharp, smaller */
    float t = (score + 1.0f) / 2.0f;  /* Normalize to [0, 1] */

    shape->roundness = 1.0f - t;
    shape->sharpness = t;
    shape->size = 0.3f + (1.0f - t) * 0.5f;
    shape->complexity = 0.2f + t * 0.6f;
    shape->symmetry = 0.5f + (1.0f - fabsf(score)) * 0.4f;
    shape->num_vertices = (uint32_t)(3 + t * 9);  /* 3 (circle-ish) to 12 (star-ish) */
    shape->contour = NULL;
    shape->contour_size = 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

synesthesia_config_t synesthesia_default_config(void) {
    synesthesia_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_associations = SYNESTHESIA_DEFAULT_MAX_ASSOCIATIONS;
    config.max_cascade_depth = SYNESTHESIA_DEFAULT_MAX_CASCADE_DEPTH;
    config.feature_dim = SYNESTHESIA_DEFAULT_FEATURE_DIM;
    config.color_channels = SYNESTHESIA_DEFAULT_COLOR_CHANNELS;
    config.activation_threshold = SYNESTHESIA_DEFAULT_ACTIVATION_THRESHOLD;
    config.cascade_decay = SYNESTHESIA_DEFAULT_CASCADE_DECAY;
    config.concurrent_boost = 1.2f;
    config.learning_rate = SYNESTHESIA_DEFAULT_LEARNING_RATE;
    config.consistency_threshold = 0.7f;
    config.enable_hebbian_learning = true;
    config.enable_grapheme_color = true;
    config.enable_chromesthesia = true;
    config.enable_spatial_sequence = false;
    config.enable_lexical_gustatory = true;
    config.enable_auditory_tactile = true;
    config.enable_mirror_touch = false;
    config.enable_bidirectional = true;
    config.enable_cascade = true;
    config.enable_inhibition = true;
    config.max_concurrent_cascades = 4;

    return config;
}

synesthesia_module_t* synesthesia_create(const synesthesia_config_t* config) {
    LOG_INFO("[%s] Creating synesthesia module", SYNESTHESIA_LOG_MODULE);

    synesthesia_module_t* module = nimcp_calloc(1, sizeof(synesthesia_module_t));
    if (!module) {
        LOG_ERROR("[%s] Failed to allocate module", SYNESTHESIA_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(synesthesia_module_t),
                           "synesthesia_create: Failed to allocate module");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        module->config = *config;
    } else {
        module->config = synesthesia_default_config();
    }

    /* Initialize association storage */
    module->store_capacity = module->config.max_associations;
    module->association_store = nimcp_calloc(module->store_capacity, sizeof(association_entry_t*));
    if (!module->association_store) {
        LOG_ERROR("[%s] Failed to allocate association store", SYNESTHESIA_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           module->store_capacity * sizeof(association_entry_t*),
                           "synesthesia_create: Failed to allocate association store");
        synesthesia_destroy(module);
        return NULL;
    }
    module->next_association_id = 1;

    /* Initialize grapheme-color storage */
    if (module->config.enable_grapheme_color) {
        module->grapheme_color_capacity = 256;  /* ASCII + extended */
        module->grapheme_colors = nimcp_calloc(module->grapheme_color_capacity,
                                               sizeof(grapheme_color_entry_t*));
        if (!module->grapheme_colors) {
            LOG_ERROR("[%s] Failed to allocate grapheme-color store", SYNESTHESIA_LOG_MODULE);
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                               module->grapheme_color_capacity * sizeof(grapheme_color_entry_t*),
                               "synesthesia_create: Failed to allocate grapheme-color store");
            synesthesia_destroy(module);
            return NULL;
        }
    }

    module->status = SYNESTHESIA_STATUS_IDLE;
    module->last_error = SYNESTHESIA_ERROR_NONE;
    module->inhibited = false;

    LOG_INFO("[%s] Synesthesia module created (max_associations=%u)",
             SYNESTHESIA_LOG_MODULE, module->config.max_associations);

    return module;
}

void synesthesia_destroy(synesthesia_module_t* module) {
    if (!module) return;

    LOG_INFO("[%s] Destroying synesthesia module", SYNESTHESIA_LOG_MODULE);

    /* Free association store */
    if (module->association_store) {
        for (uint32_t i = 0; i < module->store_capacity; i++) {
            association_entry_t* entry = module->association_store[i];
            while (entry) {
                association_entry_t* next = entry->hash_next;
                free_association_entry(entry);
                entry = next;
            }
        }
        nimcp_free(module->association_store);
    }

    /* Free grapheme-color store */
    if (module->grapheme_colors) {
        for (uint32_t i = 0; i < module->grapheme_color_capacity; i++) {
            grapheme_color_entry_t* entry = module->grapheme_colors[i];
            while (entry) {
                grapheme_color_entry_t* next = entry->next;
                free_grapheme_color_entry(entry);
                entry = next;
            }
        }
        nimcp_free(module->grapheme_colors);
    }

    /* Free sound-shape list */
    sound_shape_entry_t* ss_entry = module->sound_shapes;
    while (ss_entry) {
        sound_shape_entry_t* next = ss_entry->next;
        free_sound_shape_entry(ss_entry);
        ss_entry = next;
    }

    nimcp_free(module);
    LOG_DEBUG("[%s] Module destroyed", SYNESTHESIA_LOG_MODULE);
}

bool synesthesia_reset(synesthesia_module_t* module) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_reset: NULL module pointer");
        return false;
    }

    LOG_DEBUG("[%s] Resetting module", SYNESTHESIA_LOG_MODULE);

    /* Clear association store */
    for (uint32_t i = 0; i < module->store_capacity; i++) {
        association_entry_t* entry = module->association_store[i];
        while (entry) {
            association_entry_t* next = entry->hash_next;
            free_association_entry(entry);
            entry = next;
        }
        module->association_store[i] = NULL;
    }
    module->association_count = 0;
    module->next_association_id = 1;

    /* Clear grapheme-color store */
    if (module->grapheme_colors) {
        for (uint32_t i = 0; i < module->grapheme_color_capacity; i++) {
            grapheme_color_entry_t* entry = module->grapheme_colors[i];
            while (entry) {
                grapheme_color_entry_t* next = entry->next;
                free_grapheme_color_entry(entry);
                entry = next;
            }
            module->grapheme_colors[i] = NULL;
        }
        module->grapheme_color_count = 0;
    }

    /* Clear sound-shape list */
    sound_shape_entry_t* ss_entry = module->sound_shapes;
    while (ss_entry) {
        sound_shape_entry_t* next = ss_entry->next;
        free_sound_shape_entry(ss_entry);
        ss_entry = next;
    }
    module->sound_shapes = NULL;
    module->sound_shape_count = 0;

    /* Reset statistics */
    memset(&module->stats, 0, sizeof(synesthesia_stats_t));

    module->inhibited = false;
    module->status = SYNESTHESIA_STATUS_IDLE;
    module->last_error = SYNESTHESIA_ERROR_NONE;

    return true;
}

/*=============================================================================
 * GRAPHEME-COLOR SYNESTHESIA
 *===========================================================================*/

bool synesthesia_add_grapheme_color(
    synesthesia_module_t* module,
    const grapheme_t* grapheme,
    const synesthesia_color_t* color,
    float strength
) {
    if (!module || !grapheme || !color) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_add_grapheme_color: Invalid parameters (module=%p, grapheme=%p, color=%p)",
                              (void*)module, (void*)grapheme, (void*)color);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_grapheme_color || !module->grapheme_colors) {
        return true;  /* Silently succeed if disabled */
    }

    LOG_DEBUG("[%s] Adding grapheme-color mapping (codepoint=%u, r=%.2f, g=%.2f, b=%.2f)",
              SYNESTHESIA_LOG_MODULE, grapheme->codepoint, color->r, color->g, color->b);

    /* Create entry */
    grapheme_color_entry_t* entry = nimcp_calloc(1, sizeof(grapheme_color_entry_t));
    if (!entry) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(grapheme_color_entry_t),
                           "synesthesia_add_grapheme_color: Failed to allocate entry");
        set_error(module, SYNESTHESIA_ERROR_MAPPING_FAILED);
        return false;
    }

    entry->mapping.grapheme = *grapheme;
    if (grapheme->visual_features && grapheme->feature_count > 0) {
        entry->mapping.grapheme.visual_features = nimcp_calloc(grapheme->feature_count, sizeof(float));
        if (entry->mapping.grapheme.visual_features) {
            memcpy(entry->mapping.grapheme.visual_features, grapheme->visual_features,
                   grapheme->feature_count * sizeof(float));
        }
    }
    entry->mapping.color = *color;
    entry->mapping.strength = strength;
    entry->mapping.consistency = 1.0f;

    /* Insert into hash table */
    uint32_t hash_idx = hash_codepoint(grapheme->codepoint, module->grapheme_color_capacity);
    entry->next = module->grapheme_colors[hash_idx];
    module->grapheme_colors[hash_idx] = entry;
    module->grapheme_color_count++;

    module->stats.grapheme_color_count++;
    module->stats.total_associations++;
    module->stats.active_associations++;

    return true;
}

bool synesthesia_get_grapheme_color(
    synesthesia_module_t* module,
    const grapheme_t* grapheme,
    synesthesia_color_t* color
) {
    if (!module || !grapheme || !color) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_get_grapheme_color: Invalid parameters (module=%p, grapheme=%p, color=%p)",
                              (void*)module, (void*)grapheme, (void*)color);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (module->inhibited) {
        module->stats.inhibited_activations++;
        return false;
    }

    if (!module->grapheme_colors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synesthesia_get_grapheme_color: module->grapheme_colors is NULL");
        return false;
    }

    uint32_t hash_idx = hash_codepoint(grapheme->codepoint, module->grapheme_color_capacity);
    grapheme_color_entry_t* entry = module->grapheme_colors[hash_idx];

    while (entry) {
        if (entry->mapping.grapheme.codepoint == grapheme->codepoint) {
            *color = entry->mapping.color;
            module->stats.total_activations++;
            return true;
        }
        entry = entry->next;
    }

    return false;
}

bool synesthesia_get_char_color(
    synesthesia_module_t* module,
    char character,
    synesthesia_color_t* color
) {
    grapheme_t grapheme;
    memset(&grapheme, 0, sizeof(grapheme));
    grapheme.codepoint = (uint32_t)(unsigned char)character;
    grapheme.utf8[0] = character;
    grapheme.utf8[1] = '\0';
    grapheme.is_digit = (character >= '0' && character <= '9');
    grapheme.is_letter = ((character >= 'A' && character <= 'Z') ||
                          (character >= 'a' && character <= 'z'));

    return synesthesia_get_grapheme_color(module, &grapheme, color);
}

bool synesthesia_colorize_text(
    synesthesia_module_t* module,
    const char* text,
    synesthesia_color_t* colors,
    uint32_t max_colors,
    uint32_t* color_count
) {
    if (!module || !text || !colors || !color_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_colorize_text: Invalid parameters (module=%p, text=%p, colors=%p)",
                              (void*)module, (void*)text, (void*)colors);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    *color_count = 0;
    const char* ptr = text;

    while (*ptr && *color_count < max_colors) {
        synesthesia_color_t color;
        if (synesthesia_get_char_color(module, *ptr, &color)) {
            colors[*color_count] = color;
        } else {
            /* Default color for unmapped characters */
            colors[*color_count].r = 0.5f;
            colors[*color_count].g = 0.5f;
            colors[*color_count].b = 0.5f;
            colors[*color_count].alpha = 1.0f;
        }
        (*color_count)++;
        ptr++;
    }

    return true;
}

/*=============================================================================
 * SOUND-SHAPE SYNESTHESIA
 *===========================================================================*/

bool synesthesia_add_sound_shape(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound,
    const synesthesia_shape_t* shape,
    float strength
) {
    if (!module || !sound || !shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_add_sound_shape: Invalid parameters (module=%p, sound=%p, shape=%p)",
                              (void*)module, (void*)sound, (void*)shape);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    LOG_DEBUG("[%s] Adding sound-shape mapping (pitch=%.2f, roundness=%.2f)",
              SYNESTHESIA_LOG_MODULE, sound->pitch, shape->roundness);

    /* Create entry */
    sound_shape_entry_t* entry = nimcp_calloc(1, sizeof(sound_shape_entry_t));
    if (!entry) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(sound_shape_entry_t),
                           "synesthesia_add_sound_shape: Failed to allocate entry");
        set_error(module, SYNESTHESIA_ERROR_MAPPING_FAILED);
        return false;
    }

    entry->mapping.sound = *sound;
    if (sound->timbre && sound->timbre_dim > 0) {
        entry->mapping.sound.timbre = nimcp_calloc(sound->timbre_dim, sizeof(float));
        if (entry->mapping.sound.timbre) {
            memcpy(entry->mapping.sound.timbre, sound->timbre, sound->timbre_dim * sizeof(float));
        }
    }

    entry->mapping.shape = *shape;
    if (shape->contour && shape->contour_size > 0) {
        entry->mapping.shape.contour = nimcp_calloc(shape->contour_size, sizeof(float));
        if (entry->mapping.shape.contour) {
            memcpy(entry->mapping.shape.contour, shape->contour, shape->contour_size * sizeof(float));
        }
    }

    entry->mapping.strength = strength;
    entry->mapping.bouba_kiki_score = compute_bouba_kiki(sound);

    /* Add to list */
    entry->next = module->sound_shapes;
    module->sound_shapes = entry;
    module->sound_shape_count++;

    module->stats.sound_shape_count++;
    module->stats.total_associations++;
    module->stats.active_associations++;

    return true;
}

bool synesthesia_get_sound_shape(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound,
    synesthesia_shape_t* shape
) {
    if (!module || !sound || !shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_get_sound_shape: Invalid parameters (module=%p, sound=%p, shape=%p)",
                              (void*)module, (void*)sound, (void*)shape);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (module->inhibited) {
        module->stats.inhibited_activations++;
        return false;
    }

    /* Find best matching sound */
    sound_shape_entry_t* best_entry = NULL;
    float best_similarity = 0.0f;

    sound_shape_entry_t* entry = module->sound_shapes;
    while (entry) {
        /* Compare pitch and attack primarily */
        float pitch_sim = 1.0f - fabsf(entry->mapping.sound.pitch - sound->pitch);
        float attack_sim = 1.0f - fabsf(entry->mapping.sound.attack - sound->attack);
        float similarity = pitch_sim * 0.5f + attack_sim * 0.5f;

        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_entry = entry;
        }
        entry = entry->next;
    }

    if (best_entry && best_similarity > module->config.activation_threshold) {
        *shape = best_entry->mapping.shape;
        /* Don't copy contour pointer - caller should use generate function if needed */
        shape->contour = NULL;
        shape->contour_size = 0;
        module->stats.total_activations++;
        return true;
    }

    return false;
}

float synesthesia_bouba_kiki_score(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound
) {
    if (!module || !sound) return 0.0f;
    return compute_bouba_kiki(sound);
}

bool synesthesia_generate_shape_from_sound(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound,
    synesthesia_shape_t* shape
) {
    if (!module || !sound || !shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_generate_shape_from_sound: Invalid parameters (module=%p, sound=%p, shape=%p)",
                              (void*)module, (void*)sound, (void*)shape);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    memset(shape, 0, sizeof(synesthesia_shape_t));

    float score = compute_bouba_kiki(sound);
    generate_shape_from_score(score, shape);

    module->stats.total_activations++;

    return true;
}

/*=============================================================================
 * TASTE-TOUCH SYNESTHESIA
 *===========================================================================*/

bool synesthesia_add_taste_touch(
    synesthesia_module_t* module,
    const synesthesia_taste_t* taste,
    const synesthesia_tactile_t* tactile,
    float strength
) {
    if (!module || !taste || !tactile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_add_taste_touch: Invalid parameters (module=%p, taste=%p, tactile=%p)",
                              (void*)module, (void*)taste, (void*)tactile);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    /* Store as generic association */
    float taste_features[7] = {
        taste->sweet, taste->sour, taste->salty,
        taste->bitter, taste->umami, taste->intensity, taste->pleasantness
    };

    float tactile_features[7] = {
        tactile->pressure, tactile->temperature, tactile->texture_roughness,
        tactile->vibration, tactile->location_x, tactile->location_y, tactile->spread
    };

    uint64_t id = synesthesia_create_association(
        module,
        MODALITY_GUSTATORY, taste_features, 7,
        MODALITY_TACTILE, tactile_features, 7,
        strength
    );

    return (id != 0);
}

bool synesthesia_get_taste_touch(
    synesthesia_module_t* module,
    const synesthesia_taste_t* taste,
    synesthesia_tactile_t* tactile
) {
    if (!module || !taste || !tactile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_get_taste_touch: Invalid parameters (module=%p, taste=%p, tactile=%p)",
                              (void*)module, (void*)taste, (void*)tactile);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (module->inhibited) {
        module->stats.inhibited_activations++;
        return false;
    }

    float taste_features[7] = {
        taste->sweet, taste->sour, taste->salty,
        taste->bitter, taste->umami, taste->intensity, taste->pleasantness
    };

    synesthetic_experience_t experience;
    memset(&experience, 0, sizeof(experience));

    if (synesthesia_trigger_experience(module, MODALITY_GUSTATORY, taste_features, 7, 1.0f, &experience)) {
        if (experience.tactile_count > 0 && experience.tactile) {
            *tactile = experience.tactile[0];
            synesthesia_free_experience(&experience);
            return true;
        }
        synesthesia_free_experience(&experience);
    }

    return false;
}

/*=============================================================================
 * GENERIC CROSS-MODAL ASSOCIATIONS
 *===========================================================================*/

uint64_t synesthesia_create_association(
    synesthesia_module_t* module,
    sensory_modality_t inducer_modality,
    const float* inducer_features,
    uint32_t inducer_count,
    sensory_modality_t concurrent_modality,
    const float* concurrent_features,
    uint32_t concurrent_count,
    float strength
) {
    if (!module || !inducer_features || inducer_count == 0 ||
        !concurrent_features || concurrent_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_create_association: Invalid parameters (module=%p, inducer=%p, concurrent=%p)",
                              (void*)module, (void*)inducer_features, (void*)concurrent_features);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return 0;
    }

    if (module->association_count >= module->config.max_associations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
                              "synesthesia_create_association: Max associations reached (%lu >= %u)",
                              (unsigned long)module->association_count, module->config.max_associations);
        set_error(module, SYNESTHESIA_ERROR_CAPACITY_EXCEEDED);
        return 0;
    }

    LOG_DEBUG("[%s] Creating association (inducer=%d, concurrent=%d, strength=%.2f)",
              SYNESTHESIA_LOG_MODULE, inducer_modality, concurrent_modality, strength);

    /* Create entry */
    association_entry_t* entry = nimcp_calloc(1, sizeof(association_entry_t));
    if (!entry) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(association_entry_t),
                           "synesthesia_create_association: Failed to allocate entry");
        set_error(module, SYNESTHESIA_ERROR_MAPPING_FAILED);
        return 0;
    }

    uint64_t id = module->next_association_id++;
    entry->association.association_id = id;
    entry->association.type = SYNESTHESIA_TYPE_COUNT;  /* Generic */
    entry->association.inducer_modality = inducer_modality;
    entry->association.concurrent_modality = concurrent_modality;
    entry->association.strength = strength;
    entry->association.consistency = 1.0f;
    entry->association.automaticity = strength;
    entry->association.activation_count = 0;
    entry->association.reverse_association_id = 0;

    /* Copy inducer features */
    entry->association.inducer_features = nimcp_calloc(inducer_count, sizeof(float));
    if (!entry->association.inducer_features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, inducer_count * sizeof(float),
                           "synesthesia_create_association: Failed to allocate inducer features");
        free_association_entry(entry);
        set_error(module, SYNESTHESIA_ERROR_MAPPING_FAILED);
        return 0;
    }
    memcpy(entry->association.inducer_features, inducer_features, inducer_count * sizeof(float));
    entry->association.inducer_feature_count = inducer_count;

    /* Copy concurrent features */
    entry->association.concurrent_features = nimcp_calloc(concurrent_count, sizeof(float));
    if (!entry->association.concurrent_features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, concurrent_count * sizeof(float),
                           "synesthesia_create_association: Failed to allocate concurrent features");
        free_association_entry(entry);
        set_error(module, SYNESTHESIA_ERROR_MAPPING_FAILED);
        return 0;
    }
    memcpy(entry->association.concurrent_features, concurrent_features, concurrent_count * sizeof(float));
    entry->association.concurrent_feature_count = concurrent_count;

    /* Store in hash table */
    uint32_t hash_idx = hash_association_id(id, module->store_capacity);
    entry->hash_next = module->association_store[hash_idx];
    module->association_store[hash_idx] = entry;
    module->association_count++;

    /* Create reverse association if bidirectional enabled */
    if (module->config.enable_bidirectional) {
        association_entry_t* reverse = nimcp_calloc(1, sizeof(association_entry_t));
        if (reverse) {
            uint64_t reverse_id = module->next_association_id++;
            reverse->association.association_id = reverse_id;
            reverse->association.type = SYNESTHESIA_TYPE_COUNT;
            reverse->association.inducer_modality = concurrent_modality;
            reverse->association.concurrent_modality = inducer_modality;
            reverse->association.strength = strength * 0.5f;  /* Weaker reverse */
            reverse->association.consistency = 1.0f;
            reverse->association.automaticity = strength * 0.5f;
            reverse->association.reverse_association_id = id;

            /* Copy features (swapped) */
            reverse->association.inducer_features = nimcp_calloc(concurrent_count, sizeof(float));
            reverse->association.concurrent_features = nimcp_calloc(inducer_count, sizeof(float));
            if (reverse->association.inducer_features && reverse->association.concurrent_features) {
                memcpy(reverse->association.inducer_features, concurrent_features,
                       concurrent_count * sizeof(float));
                reverse->association.inducer_feature_count = concurrent_count;
                memcpy(reverse->association.concurrent_features, inducer_features,
                       inducer_count * sizeof(float));
                reverse->association.concurrent_feature_count = inducer_count;

                uint32_t reverse_hash = hash_association_id(reverse_id, module->store_capacity);
                reverse->hash_next = module->association_store[reverse_hash];
                module->association_store[reverse_hash] = reverse;
                module->association_count++;

                entry->association.reverse_association_id = reverse_id;
            } else {
                free_association_entry(reverse);
            }
        }
    }

    module->stats.total_associations++;
    module->stats.active_associations = module->association_count;

    return id;
}

bool synesthesia_trigger_experience(
    synesthesia_module_t* module,
    sensory_modality_t inducer_modality,
    const float* inducer_features,
    uint32_t inducer_count,
    float intensity,
    synesthetic_experience_t* experience
) {
    if (!module || !inducer_features || inducer_count == 0 || !experience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_trigger_experience: Invalid parameters (module=%p, features=%p, experience=%p)",
                              (void*)module, (void*)inducer_features, (void*)experience);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (module->inhibited) {
        set_error(module, SYNESTHESIA_ERROR_INHIBITION_ACTIVE);
        module->stats.inhibited_activations++;
        return false;
    }

    module->status = SYNESTHESIA_STATUS_MAPPING;
    memset(experience, 0, sizeof(synesthetic_experience_t));

    LOG_DEBUG("[%s] Triggering experience (modality=%d, intensity=%.2f)",
              SYNESTHESIA_LOG_MODULE, inducer_modality, intensity);

    /* Find matching associations */
    uint32_t max_matches = 16;
    float* match_strengths = nimcp_calloc(max_matches, sizeof(float));
    float** match_features = nimcp_calloc(max_matches, sizeof(float*));
    uint32_t* match_counts = nimcp_calloc(max_matches, sizeof(uint32_t));
    sensory_modality_t* match_modalities = nimcp_calloc(max_matches, sizeof(sensory_modality_t));
    uint32_t match_count = 0;

    if (!match_strengths || !match_features || !match_counts || !match_modalities) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_matches * sizeof(float),
                           "synesthesia_trigger_experience: Failed to allocate match arrays");
        if (match_strengths) nimcp_free(match_strengths);
        if (match_features) nimcp_free(match_features);
        if (match_counts) nimcp_free(match_counts);
        if (match_modalities) nimcp_free(match_modalities);
        set_error(module, SYNESTHESIA_ERROR_MAPPING_FAILED);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "synesthesia_trigger_experience: validation failed");
        return false;
    }

    /* Search associations */
    for (uint32_t i = 0; i < module->store_capacity && match_count < max_matches; i++) {
        association_entry_t* entry = module->association_store[i];
        while (entry && match_count < max_matches) {
            if (entry->association.inducer_modality == inducer_modality) {
                /* Compute similarity */
                uint32_t cmp_count = (inducer_count < entry->association.inducer_feature_count)
                                     ? inducer_count : entry->association.inducer_feature_count;
                float sim = compute_similarity(inducer_features, entry->association.inducer_features, cmp_count);

                if (sim > module->config.activation_threshold) {
                    match_strengths[match_count] = sim * entry->association.strength * intensity;
                    match_features[match_count] = entry->association.concurrent_features;
                    match_counts[match_count] = entry->association.concurrent_feature_count;
                    match_modalities[match_count] = entry->association.concurrent_modality;
                    match_count++;

                    entry->association.activation_count++;
                }
            }
            entry = entry->hash_next;
        }
    }

    /* Build experience from matches */
    experience->type = SYNESTHESIA_GRAPHEME_COLOR;  /* Default */
    experience->inducer_modality = inducer_modality;
    experience->inducer_intensity = intensity;

    /* Count modalities */
    uint32_t color_count = 0, tactile_count = 0, taste_count = 0;
    for (uint32_t i = 0; i < match_count; i++) {
        if (match_modalities[i] == MODALITY_VISUAL) color_count++;
        else if (match_modalities[i] == MODALITY_TACTILE) tactile_count++;
        else if (match_modalities[i] == MODALITY_GUSTATORY) taste_count++;
    }

    /* Allocate result arrays */
    if (color_count > 0) {
        experience->colors = nimcp_calloc(color_count, sizeof(synesthesia_color_t));
        if (experience->colors) {
            uint32_t idx = 0;
            for (uint32_t i = 0; i < match_count && idx < color_count; i++) {
                if (match_modalities[i] == MODALITY_VISUAL && match_counts[i] >= 3) {
                    experience->colors[idx].r = match_features[i][0] * match_strengths[i];
                    experience->colors[idx].g = match_features[i][1] * match_strengths[i];
                    experience->colors[idx].b = match_features[i][2] * match_strengths[i];
                    experience->colors[idx].alpha = 1.0f;
                    idx++;
                }
            }
            experience->color_count = idx;
        }
    }

    if (tactile_count > 0) {
        experience->tactile = nimcp_calloc(tactile_count, sizeof(synesthesia_tactile_t));
        if (experience->tactile) {
            uint32_t idx = 0;
            for (uint32_t i = 0; i < match_count && idx < tactile_count; i++) {
                if (match_modalities[i] == MODALITY_TACTILE && match_counts[i] >= 4) {
                    experience->tactile[idx].pressure = match_features[i][0] * match_strengths[i];
                    experience->tactile[idx].temperature = match_features[i][1] * match_strengths[i];
                    experience->tactile[idx].texture_roughness = match_features[i][2] * match_strengths[i];
                    experience->tactile[idx].vibration = match_features[i][3] * match_strengths[i];
                    idx++;
                }
            }
            experience->tactile_count = idx;
        }
    }

    /* Calculate quality metrics */
    float total_strength = 0.0f;
    for (uint32_t i = 0; i < match_count; i++) {
        total_strength += match_strengths[i];
    }
    experience->overall_intensity = match_count > 0 ? total_strength / match_count : 0.0f;
    experience->vividness = experience->overall_intensity;
    experience->involuntariness = experience->overall_intensity;

    /* Cleanup */
    nimcp_free(match_strengths);
    nimcp_free(match_features);
    nimcp_free(match_counts);
    nimcp_free(match_modalities);

    /* Update stats */
    module->stats.total_activations++;

    /* Invoke callback */
    if (module->experience_callback) {
        module->experience_callback(experience, module->experience_user_data);
    }

    module->status = SYNESTHESIA_STATUS_IDLE;

    return (match_count > 0);
}

bool synesthesia_get_association(
    const synesthesia_module_t* module,
    uint64_t association_id,
    cross_modal_association_t* association
) {
    if (!module || association_id == 0 || !association) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_get_association: Invalid parameters (module=%p, id=%lu, assoc=%p)",
                              (void*)module, (unsigned long)association_id, (void*)association);
        return false;
    }

    uint32_t hash_idx = hash_association_id(association_id, module->store_capacity);
    association_entry_t* entry = module->association_store[hash_idx];

    while (entry) {
        if (entry->association.association_id == association_id) {
            *association = entry->association;
            /* Don't copy feature pointers */
            association->inducer_features = NULL;
            association->concurrent_features = NULL;
            return true;
        }
        entry = entry->hash_next;
    }

    return false;
}

bool synesthesia_update_strength(
    synesthesia_module_t* module,
    uint64_t association_id,
    float new_strength
) {
    if (!module || association_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_update_strength: Invalid parameters (module=%p, id=%lu)",
                              (void*)module, (unsigned long)association_id);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    uint32_t hash_idx = hash_association_id(association_id, module->store_capacity);
    association_entry_t* entry = module->association_store[hash_idx];

    while (entry) {
        if (entry->association.association_id == association_id) {
            float old_strength = entry->association.strength;
            entry->association.strength = fminf(1.0f, fmaxf(0.0f, new_strength));

            if (module->learning_callback) {
                module->learning_callback(association_id, old_strength, new_strength,
                                         module->learning_user_data);
            }

            if (new_strength > old_strength) {
                module->stats.associations_strengthened++;
            } else {
                module->stats.associations_weakened++;
            }

            return true;
        }
        entry = entry->hash_next;
    }

    set_error(module, SYNESTHESIA_ERROR_ASSOCIATION_NOT_FOUND);
    return false;
}

bool synesthesia_remove_association(
    synesthesia_module_t* module,
    uint64_t association_id
) {
    if (!module || association_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_remove_association: Invalid parameters (module=%p, id=%lu)",
                              (void*)module, (unsigned long)association_id);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    uint32_t hash_idx = hash_association_id(association_id, module->store_capacity);
    association_entry_t* entry = module->association_store[hash_idx];
    association_entry_t* prev = NULL;

    while (entry) {
        if (entry->association.association_id == association_id) {
            if (prev) {
                prev->hash_next = entry->hash_next;
            } else {
                module->association_store[hash_idx] = entry->hash_next;
            }
            free_association_entry(entry);
            module->association_count--;
            module->stats.active_associations = module->association_count;
            return true;
        }
        prev = entry;
        entry = entry->hash_next;
    }

    set_error(module, SYNESTHESIA_ERROR_ASSOCIATION_NOT_FOUND);
    return false;
}

/*=============================================================================
 * CASCADE PROPAGATION
 *===========================================================================*/

bool synesthesia_cascade(
    synesthesia_module_t* module,
    sensory_modality_t start_modality,
    const float* start_features,
    uint32_t feature_count,
    cascade_mode_t mode,
    cascade_result_t* result
) {
    if (!module || !start_features || feature_count == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_cascade: Invalid parameters (module=%p, features=%p, result=%p)",
                              (void*)module, (void*)start_features, (void*)result);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_cascade) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synesthesia_cascade: module->config is NULL");
        return false;
    }

    if (module->inhibited) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "synesthesia_cascade: Module is inhibited");
        set_error(module, SYNESTHESIA_ERROR_INHIBITION_ACTIVE);
        return false;
    }

    module->status = SYNESTHESIA_STATUS_CASCADING;
    memset(result, 0, sizeof(cascade_result_t));

    LOG_DEBUG("[%s] Starting cascade (modality=%d, mode=%d)",
              SYNESTHESIA_LOG_MODULE, start_modality, mode);

    /* Track activated modalities */
    bool activated[MODALITY_COUNT] = {false};
    float activation_strength[MODALITY_COUNT] = {0.0f};
    uint64_t triggered[256];
    uint32_t triggered_count = 0;

    /* Current activation frontier */
    float* current_features = nimcp_calloc(module->config.feature_dim, sizeof(float));
    uint32_t current_count = (feature_count < module->config.feature_dim)
                             ? feature_count : module->config.feature_dim;
    memcpy(current_features, start_features, current_count * sizeof(float));
    sensory_modality_t current_modality = start_modality;
    float current_strength = 1.0f;

    activated[start_modality] = true;
    activation_strength[start_modality] = 1.0f;

    /* Cascade loop */
    uint32_t depth = 0;
    while (depth < module->config.max_cascade_depth && current_strength > module->config.activation_threshold) {
        /* Find associations from current modality */
        for (uint32_t i = 0; i < module->store_capacity; i++) {
            association_entry_t* entry = module->association_store[i];
            while (entry) {
                if (entry->association.inducer_modality == current_modality) {
                    float sim = compute_similarity(current_features, entry->association.inducer_features,
                                                   current_count < entry->association.inducer_feature_count
                                                   ? current_count : entry->association.inducer_feature_count);

                    if (sim > module->config.activation_threshold) {
                        sensory_modality_t target = entry->association.concurrent_modality;

                        /* Check if bidirectional or broadcast mode allows activation */
                        bool allow = (mode == CASCADE_BROADCAST) ||
                                    (mode == CASCADE_UNIDIRECTIONAL && !activated[target]) ||
                                    (mode == CASCADE_BIDIRECTIONAL);

                        if (allow && !activated[target]) {
                            activated[target] = true;
                            activation_strength[target] = current_strength * entry->association.strength;

                            if (triggered_count < 256) {
                                triggered[triggered_count++] = entry->association.association_id;
                            }

                            /* Update current for next iteration */
                            if (entry->association.concurrent_feature_count <= module->config.feature_dim) {
                                memcpy(current_features, entry->association.concurrent_features,
                                       entry->association.concurrent_feature_count * sizeof(float));
                                current_count = entry->association.concurrent_feature_count;
                                current_modality = target;
                            }
                        }
                    }
                }
                entry = entry->hash_next;
            }
        }

        current_strength *= module->config.cascade_decay;
        depth++;
    }

    nimcp_free(current_features);

    /* Build result */
    uint32_t activated_count = 0;
    for (int i = 0; i < MODALITY_COUNT; i++) {
        if (activated[i]) activated_count++;
    }

    if (activated_count > 0) {
        result->activated_modalities = nimcp_calloc(activated_count, sizeof(sensory_modality_t));
        result->activation_strengths = nimcp_calloc(activated_count, sizeof(float));

        if (result->activated_modalities && result->activation_strengths) {
            uint32_t idx = 0;
            for (int i = 0; i < MODALITY_COUNT; i++) {
                if (activated[i]) {
                    result->activated_modalities[idx] = (sensory_modality_t)i;
                    result->activation_strengths[idx] = activation_strength[i];
                    idx++;
                }
            }
            result->modality_count = activated_count;
        }
    }

    if (triggered_count > 0) {
        result->triggered_associations = nimcp_calloc(triggered_count, sizeof(uint64_t));
        if (result->triggered_associations) {
            memcpy(result->triggered_associations, triggered, triggered_count * sizeof(uint64_t));
            result->triggered_count = triggered_count;
        }
    }

    result->cascade_depth = depth;

    /* Update stats */
    module->stats.cascade_activations += triggered_count;
    module->stats.avg_cascade_depth =
        (module->stats.avg_cascade_depth * module->stats.cascade_activations + depth) /
        (module->stats.cascade_activations + 1);

    /* Invoke callback */
    if (module->cascade_callback) {
        module->cascade_callback(result, module->cascade_user_data);
    }

    module->status = SYNESTHESIA_STATUS_IDLE;

    return (activated_count > 1);
}

bool synesthesia_set_inhibition(synesthesia_module_t* module, bool inhibit) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_set_inhibition: NULL module pointer");
        return false;
    }

    module->inhibited = inhibit;
    module->status = inhibit ? SYNESTHESIA_STATUS_INHIBITED : SYNESTHESIA_STATUS_IDLE;

    LOG_DEBUG("[%s] Inhibition %s", SYNESTHESIA_LOG_MODULE, inhibit ? "enabled" : "disabled");

    return true;
}

bool synesthesia_is_inhibited(const synesthesia_module_t* module) {
    if (!module) {
        return false;
    }
    return module->inhibited;
}

/*=============================================================================
 * LEARNING AND TRAINING
 *===========================================================================*/

uint64_t synesthesia_train_cooccurrence(
    synesthesia_module_t* module,
    sensory_modality_t inducer_modality,
    const float* inducer_features,
    uint32_t inducer_count,
    sensory_modality_t concurrent_modality,
    const float* concurrent_features,
    uint32_t concurrent_count
) {
    if (!module || !inducer_features || !concurrent_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_train_cooccurrence: Invalid parameters (module=%p, inducer=%p, concurrent=%p)",
                              (void*)module, (void*)inducer_features, (void*)concurrent_features);
        set_error(module, SYNESTHESIA_ERROR_INVALID_INPUT);
        return 0;
    }

    if (!module->config.enable_hebbian_learning) {
        return 0;
    }

    module->status = SYNESTHESIA_STATUS_TRAINING;

    /* Search for existing similar association */
    uint64_t existing_id = 0;
    float best_similarity = 0.0f;

    for (uint32_t i = 0; i < module->store_capacity; i++) {
        association_entry_t* entry = module->association_store[i];
        while (entry) {
            if (entry->association.inducer_modality == inducer_modality &&
                entry->association.concurrent_modality == concurrent_modality) {

                uint32_t cmp = (inducer_count < entry->association.inducer_feature_count)
                               ? inducer_count : entry->association.inducer_feature_count;
                float sim = compute_similarity(inducer_features, entry->association.inducer_features, cmp);

                if (sim > best_similarity && sim > module->config.consistency_threshold) {
                    best_similarity = sim;
                    existing_id = entry->association.association_id;
                }
            }
            entry = entry->hash_next;
        }
    }

    if (existing_id != 0) {
        /* Strengthen existing association */
        uint32_t hash_idx = hash_association_id(existing_id, module->store_capacity);
        association_entry_t* entry = module->association_store[hash_idx];
        while (entry) {
            if (entry->association.association_id == existing_id) {
                float old_strength = entry->association.strength;
                entry->association.strength = fminf(1.0f,
                    entry->association.strength + module->config.learning_rate);
                entry->association.consistency = (entry->association.consistency + best_similarity) / 2.0f;

                if (module->learning_callback) {
                    module->learning_callback(existing_id, old_strength,
                                             entry->association.strength, module->learning_user_data);
                }

                module->stats.associations_strengthened++;
                break;
            }
            entry = entry->hash_next;
        }

        module->status = SYNESTHESIA_STATUS_IDLE;
        return existing_id;
    }

    /* Create new association */
    uint64_t new_id = synesthesia_create_association(
        module, inducer_modality, inducer_features, inducer_count,
        concurrent_modality, concurrent_features, concurrent_count,
        module->config.learning_rate
    );

    if (new_id != 0) {
        module->stats.associations_learned++;
    }

    module->status = SYNESTHESIA_STATUS_IDLE;

    return new_id;
}

uint32_t synesthesia_decay_unused(
    synesthesia_module_t* module,
    float decay_rate,
    uint32_t min_activations
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_decay_unused: NULL module pointer");
        return 0;
    }

    uint32_t decayed = 0;

    for (uint32_t i = 0; i < module->store_capacity; i++) {
        association_entry_t* entry = module->association_store[i];
        while (entry) {
            if (entry->association.activation_count < min_activations) {
                float old_strength = entry->association.strength;
                entry->association.strength *= (1.0f - decay_rate);

                if (entry->association.strength < 0.01f) {
                    entry->association.strength = 0.0f;
                }

                if (old_strength > entry->association.strength) {
                    decayed++;
                    module->stats.associations_weakened++;
                }
            }
            entry = entry->hash_next;
        }
    }

    LOG_DEBUG("[%s] Decayed %u associations", SYNESTHESIA_LOG_MODULE, decayed);

    return decayed;
}

uint32_t synesthesia_init_grapheme_colors(synesthesia_module_t* module) {
    if (!module || !module->config.enable_grapheme_color) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_init_grapheme_colors: Invalid module or feature disabled");
        return 0;
    }

    LOG_DEBUG("[%s] Initializing standard grapheme-color palette", SYNESTHESIA_LOG_MODULE);

    /* Standard letter-color associations based on synesthesia research */
    /* Vowels tend to have lighter, more saturated colors */
    struct {
        char c;
        float r, g, b;
    } mappings[] = {
        {'A', 1.0f, 0.0f, 0.0f},    /* Red */
        {'B', 0.0f, 0.0f, 0.8f},    /* Blue */
        {'C', 1.0f, 1.0f, 0.0f},    /* Yellow */
        {'D', 0.0f, 0.6f, 0.0f},    /* Green */
        {'E', 0.0f, 1.0f, 0.0f},    /* Bright green */
        {'F', 0.5f, 0.0f, 0.5f},    /* Purple */
        {'G', 1.0f, 0.5f, 0.0f},    /* Orange */
        {'H', 0.6f, 0.4f, 0.2f},    /* Brown */
        {'I', 1.0f, 1.0f, 1.0f},    /* White */
        {'J', 0.0f, 0.5f, 0.5f},    /* Teal */
        {'K', 0.3f, 0.3f, 0.3f},    /* Gray */
        {'L', 0.5f, 1.0f, 0.5f},    /* Light green */
        {'M', 0.8f, 0.0f, 0.0f},    /* Dark red */
        {'N', 0.6f, 0.3f, 0.0f},    /* Brown */
        {'O', 1.0f, 1.0f, 1.0f},    /* White */
        {'P', 1.0f, 0.75f, 0.8f},   /* Pink */
        {'Q', 0.5f, 0.5f, 0.5f},    /* Gray */
        {'R', 0.8f, 0.0f, 0.0f},    /* Red */
        {'S', 1.0f, 1.0f, 0.0f},    /* Yellow */
        {'T', 0.0f, 0.5f, 0.0f},    /* Dark green */
        {'U', 0.5f, 0.0f, 1.0f},    /* Purple/violet */
        {'V', 0.5f, 0.0f, 0.5f},    /* Purple */
        {'W', 0.0f, 0.0f, 0.5f},    /* Dark blue */
        {'X', 0.2f, 0.2f, 0.2f},    /* Dark gray */
        {'Y', 1.0f, 1.0f, 0.5f},    /* Light yellow */
        {'Z', 0.1f, 0.1f, 0.1f},    /* Near black */
        /* Digits */
        {'0', 1.0f, 1.0f, 1.0f},    /* White */
        {'1', 0.0f, 0.0f, 0.0f},    /* Black */
        {'2', 0.0f, 0.0f, 1.0f},    /* Blue */
        {'3', 0.0f, 1.0f, 0.0f},    /* Green */
        {'4', 1.0f, 0.0f, 0.0f},    /* Red */
        {'5', 1.0f, 0.5f, 0.0f},    /* Orange */
        {'6', 0.5f, 0.0f, 0.5f},    /* Purple */
        {'7', 1.0f, 1.0f, 0.0f},    /* Yellow */
        {'8', 0.6f, 0.3f, 0.0f},    /* Brown */
        {'9', 0.5f, 0.5f, 0.5f},    /* Gray */
    };

    uint32_t count = sizeof(mappings) / sizeof(mappings[0]);

    for (uint32_t i = 0; i < count; i++) {
        grapheme_t grapheme;
        memset(&grapheme, 0, sizeof(grapheme));
        grapheme.codepoint = (uint32_t)mappings[i].c;
        grapheme.utf8[0] = mappings[i].c;
        grapheme.utf8[1] = '\0';
        grapheme.is_digit = (mappings[i].c >= '0' && mappings[i].c <= '9');
        grapheme.is_letter = (mappings[i].c >= 'A' && mappings[i].c <= 'Z');

        synesthesia_color_t color;
        color.r = mappings[i].r;
        color.g = mappings[i].g;
        color.b = mappings[i].b;
        color.alpha = 1.0f;
        color.saturation = 1.0f;
        color.luminance = 0.5f;

        synesthesia_add_grapheme_color(module, &grapheme, &color, 0.9f);

        /* Add lowercase versions */
        if (grapheme.is_letter) {
            grapheme.codepoint = (uint32_t)(mappings[i].c + 32);  /* Lowercase */
            grapheme.utf8[0] = (char)(mappings[i].c + 32);
            color.saturation = 0.8f;  /* Slightly less saturated */
            synesthesia_add_grapheme_color(module, &grapheme, &color, 0.85f);
        }
    }

    LOG_INFO("[%s] Initialized %u grapheme-color mappings", SYNESTHESIA_LOG_MODULE, count * 2 - 10);

    return count * 2 - 10;
}

uint32_t synesthesia_init_bouba_kiki(synesthesia_module_t* module) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_init_bouba_kiki: NULL module pointer");
        return 0;
    }

    LOG_DEBUG("[%s] Initializing bouba-kiki mappings", SYNESTHESIA_LOG_MODULE);

    /* Create prototype bouba sound (low, soft) */
    synesthesia_sound_t bouba_sound = {
        .pitch = 0.2f,
        .loudness = 0.4f,
        .timbre = NULL,
        .timbre_dim = 0,
        .duration_ms = 300.0f,
        .attack = 0.1f,
        .decay = 0.7f
    };

    synesthesia_shape_t bouba_shape;
    memset(&bouba_shape, 0, sizeof(bouba_shape));
    bouba_shape.roundness = 0.9f;
    bouba_shape.sharpness = 0.1f;
    bouba_shape.size = 0.7f;
    bouba_shape.complexity = 0.2f;
    bouba_shape.symmetry = 0.8f;
    bouba_shape.num_vertices = 0;

    synesthesia_add_sound_shape(module, &bouba_sound, &bouba_shape, 0.9f);

    /* Create prototype kiki sound (high, sharp) */
    synesthesia_sound_t kiki_sound = {
        .pitch = 0.8f,
        .loudness = 0.6f,
        .timbre = NULL,
        .timbre_dim = 0,
        .duration_ms = 150.0f,
        .attack = 0.9f,
        .decay = 0.2f
    };

    synesthesia_shape_t kiki_shape;
    memset(&kiki_shape, 0, sizeof(kiki_shape));
    kiki_shape.roundness = 0.1f;
    kiki_shape.sharpness = 0.9f;
    kiki_shape.size = 0.4f;
    kiki_shape.complexity = 0.8f;
    kiki_shape.symmetry = 0.6f;
    kiki_shape.num_vertices = 7;

    synesthesia_add_sound_shape(module, &kiki_sound, &kiki_shape, 0.9f);

    LOG_INFO("[%s] Initialized 2 bouba-kiki prototype mappings", SYNESTHESIA_LOG_MODULE);

    return 2;
}

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

bool synesthesia_set_experience_callback(
    synesthesia_module_t* module,
    synesthesia_experience_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_set_experience_callback: NULL module pointer");
        return false;
    }
    module->experience_callback = callback;
    module->experience_user_data = user_data;
    return true;
}

bool synesthesia_set_cascade_callback(
    synesthesia_module_t* module,
    synesthesia_cascade_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_set_cascade_callback: NULL module pointer");
        return false;
    }
    module->cascade_callback = callback;
    module->cascade_user_data = user_data;
    return true;
}

bool synesthesia_set_learning_callback(
    synesthesia_module_t* module,
    synesthesia_learning_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "synesthesia_set_learning_callback: NULL module pointer");
        return false;
    }
    module->learning_callback = callback;
    module->learning_user_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

synesthesia_status_t synesthesia_get_status(const synesthesia_module_t* module) {
    if (!module) return SYNESTHESIA_STATUS_ERROR;
    return module->status;
}

synesthesia_error_t synesthesia_get_last_error(const synesthesia_module_t* module) {
    if (!module) return SYNESTHESIA_ERROR_NOT_INITIALIZED;
    return module->last_error;
}

const char* synesthesia_error_string(synesthesia_error_t error) {
    switch (error) {
        case SYNESTHESIA_ERROR_NONE: return "No error";
        case SYNESTHESIA_ERROR_INVALID_INPUT: return "Invalid input";
        case SYNESTHESIA_ERROR_MAPPING_FAILED: return "Mapping failed";
        case SYNESTHESIA_ERROR_MODALITY_NOT_FOUND: return "Modality not found";
        case SYNESTHESIA_ERROR_ASSOCIATION_NOT_FOUND: return "Association not found";
        case SYNESTHESIA_ERROR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case SYNESTHESIA_ERROR_CASCADE_OVERFLOW: return "Cascade overflow";
        case SYNESTHESIA_ERROR_TRAINING_FAILED: return "Training failed";
        case SYNESTHESIA_ERROR_INHIBITION_ACTIVE: return "Inhibition active";
        case SYNESTHESIA_ERROR_NOT_INITIALIZED: return "Not initialized";
        case SYNESTHESIA_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* synesthesia_status_string(synesthesia_status_t status) {
    switch (status) {
        case SYNESTHESIA_STATUS_IDLE: return "Idle";
        case SYNESTHESIA_STATUS_MAPPING: return "Mapping";
        case SYNESTHESIA_STATUS_CASCADING: return "Cascading";
        case SYNESTHESIA_STATUS_TRAINING: return "Training";
        case SYNESTHESIA_STATUS_INHIBITED: return "Inhibited";
        case SYNESTHESIA_STATUS_READY: return "Ready";
        case SYNESTHESIA_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool synesthesia_get_stats(const synesthesia_module_t* module, synesthesia_stats_t* stats) {
    if (!module || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_get_stats: Invalid parameters (module=%p, stats=%p)",
                              (void*)module, (void*)stats);
        return false;
    }
    *stats = module->stats;
    return true;
}

bool synesthesia_get_config(const synesthesia_module_t* module, synesthesia_config_t* config) {
    if (!module || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "synesthesia_get_config: Invalid parameters (module=%p, config=%p)",
                              (void*)module, (void*)config);
        return false;
    }
    *config = module->config;
    return true;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

void synesthesia_free_experience(synesthetic_experience_t* experience) {
    if (!experience) return;
    if (experience->colors) nimcp_free(experience->colors);
    if (experience->shapes) {
        for (uint32_t i = 0; i < experience->shape_count; i++) {
            if (experience->shapes[i].contour) {
                nimcp_free(experience->shapes[i].contour);
            }
        }
        nimcp_free(experience->shapes);
    }
    if (experience->tactile) nimcp_free(experience->tactile);
    if (experience->tastes) nimcp_free(experience->tastes);
    memset(experience, 0, sizeof(synesthetic_experience_t));
}

void synesthesia_free_cascade(cascade_result_t* cascade) {
    if (!cascade) return;
    if (cascade->activated_modalities) nimcp_free(cascade->activated_modalities);
    if (cascade->activation_strengths) nimcp_free(cascade->activation_strengths);
    if (cascade->triggered_associations) nimcp_free(cascade->triggered_associations);
    memset(cascade, 0, sizeof(cascade_result_t));
}

void synesthesia_free_shape(synesthesia_shape_t* shape) {
    if (!shape) return;
    if (shape->contour) nimcp_free(shape->contour);
    memset(shape, 0, sizeof(synesthesia_shape_t));
}

void synesthesia_hsv_to_rgb(float h, float s, float v, synesthesia_color_t* color) {
    if (!color) return;

    /* Normalize hue to [0, 360) */
    while (h >= 360.0f) h -= 360.0f;
    while (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60.0f) {
        r = c; g = x; b = 0;
    } else if (h < 120.0f) {
        r = x; g = c; b = 0;
    } else if (h < 180.0f) {
        r = 0; g = c; b = x;
    } else if (h < 240.0f) {
        r = 0; g = x; b = c;
    } else if (h < 300.0f) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    color->r = r + m;
    color->g = g + m;
    color->b = b + m;
    color->alpha = 1.0f;
    color->saturation = s;
    color->luminance = v;
}

void synesthesia_rgb_to_hsv(const synesthesia_color_t* color, float* h, float* s, float* v) {
    if (!color || !h || !s || !v) return;

    float max_c = fmaxf(fmaxf(color->r, color->g), color->b);
    float min_c = fminf(fminf(color->r, color->g), color->b);
    float delta = max_c - min_c;

    *v = max_c;

    if (max_c > 0.0f) {
        *s = delta / max_c;
    } else {
        *s = 0.0f;
        *h = 0.0f;
        return;
    }

    if (delta < 0.00001f) {
        *h = 0.0f;
        return;
    }

    if (max_c == color->r) {
        *h = 60.0f * fmodf((color->g - color->b) / delta, 6.0f);
    } else if (max_c == color->g) {
        *h = 60.0f * ((color->b - color->r) / delta + 2.0f);
    } else {
        *h = 60.0f * ((color->r - color->g) / delta + 4.0f);
    }

    if (*h < 0.0f) *h += 360.0f;
}
