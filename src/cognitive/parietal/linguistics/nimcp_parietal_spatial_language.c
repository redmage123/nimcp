/**
 * @file nimcp_parietal_spatial_language.c
 * @brief Spatial Language Processing Module Implementation
 * @version 1.0.0
 * @date 2025-01-31
 */

#include "cognitive/parietal/linguistics/nimcp_parietal_spatial_language.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_math_constants.h"

/* ============================================================================
 * INTERNAL CONSTANTS
 * ============================================================================ */

/** Pi constant */

/** Degrees to radians */
#define DEG_TO_RAD(d) ((d) * M_PI / 180.0)

/** Radians to degrees */
#define RAD_TO_DEG(r) ((r) * 180.0 / M_PI)

/* ============================================================================
 * INTERNAL TYPES
 * ============================================================================ */

/**
 * @brief Spatial language processor internal state
 */
struct spatial_language {
    /* Configuration */
    spatial_language_config_t config;

    /* Preposition definitions (built-in + custom) */
    preposition_definition_t prepositions[SPATIAL_PREPOSITION_COUNT + SPATIAL_LANG_MAX_CUSTOM_PREPOSITIONS];
    uint32_t num_prepositions;

    /* Metaphor mappings */
    spatial_metaphor_t metaphors[32];
    uint32_t num_metaphors;

    /* Current state */
    float current_precision;
    float inflammation_level;
    float fatigue_level;

    /* Frame selection priors (Bayesian) */
    float frame_priors[REF_FRAME_COUNT];

    /* Statistics */
    spatial_language_stats_t stats;

    /* Error tracking */
    uint32_t consecutive_errors;
};

/* ============================================================================
 * THREAD-LOCAL ERROR STORAGE
 * ============================================================================ */

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_last_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * PREPOSITION NAME TABLES
 * ============================================================================ */

/** Preposition word strings indexed by type */
static const char* g_preposition_words[SPATIAL_PREPOSITION_COUNT] = {
    [SPATIAL_PREP_NEAR]     = "near",
    [SPATIAL_PREP_FAR]      = "far",
    [SPATIAL_PREP_ADJACENT] = "adjacent",
    [SPATIAL_PREP_BESIDE]   = "beside",
    [SPATIAL_PREP_BY]       = "by",
    [SPATIAL_PREP_LEFT]     = "left",
    [SPATIAL_PREP_RIGHT]    = "right",
    [SPATIAL_PREP_FRONT]    = "front",
    [SPATIAL_PREP_BEHIND]   = "behind",
    [SPATIAL_PREP_ABOVE]    = "above",
    [SPATIAL_PREP_BELOW]    = "below",
    [SPATIAL_PREP_OVER]     = "over",
    [SPATIAL_PREP_UNDER]    = "under",
    [SPATIAL_PREP_ON]       = "on",
    [SPATIAL_PREP_BENEATH]  = "beneath",
    [SPATIAL_PREP_IN]       = "in",
    [SPATIAL_PREP_INSIDE]   = "inside",
    [SPATIAL_PREP_OUTSIDE]  = "outside",
    [SPATIAL_PREP_WITHIN]   = "within",
    [SPATIAL_PREP_THROUGH]  = "through",
    [SPATIAL_PREP_ACROSS]   = "across",
    [SPATIAL_PREP_ALONG]    = "along",
    [SPATIAL_PREP_TOWARD]   = "toward",
    [SPATIAL_PREP_AWAY]     = "away",
    [SPATIAL_PREP_INTO]     = "into",
    [SPATIAL_PREP_OUT_OF]   = "out",
    [SPATIAL_PREP_BETWEEN]  = "between",
    [SPATIAL_PREP_AMONG]    = "among",
    [SPATIAL_PREP_AROUND]   = "around",
    [SPATIAL_PREP_OPPOSITE] = "opposite",
    [SPATIAL_PREP_AT]       = "at",
    [SPATIAL_PREP_AGAINST]  = "against"
};

/** Reference frame names */
static const char* g_frame_names[REF_FRAME_COUNT] = {
    [REF_FRAME_EGOCENTRIC]  = "egocentric",
    [REF_FRAME_ALLOCENTRIC] = "allocentric",
    [REF_FRAME_INTRINSIC]   = "intrinsic",
    [REF_FRAME_RELATIVE]    = "relative",
    [REF_FRAME_GEOGRAPHIC]  = "geographic"
};

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Normalize a string to lowercase for comparison
 */
static void normalize_word(const char* input, char* output, uint32_t max_len) {
    uint32_t i = 0;
    while (input[i] && i < max_len - 1) {
        output[i] = (char)tolower((unsigned char)input[i]);
        i++;
    }
    output[i] = '\0';
}

/**
 * @brief Initialize default preposition definitions
 */
static void init_default_prepositions(spatial_language_t* sl) {
    for (uint32_t i = 0; i < SPATIAL_PREPOSITION_COUNT; i++) {
        preposition_definition_t* def = &sl->prepositions[i];

        def->type = (spatial_preposition_t)i;
        strncpy(def->word, g_preposition_words[i], sizeof(def->word) - 1);

        /* Initialize default membership functions based on preposition type */
        switch ((spatial_preposition_t)i) {
            /* Proximity prepositions */
            case SPATIAL_PREP_NEAR:
            case SPATIAL_PREP_ADJACENT:
            case SPATIAL_PREP_BESIDE:
            case SPATIAL_PREP_BY:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_FAR:
                def->distance_mf = fuzzy_mf_s_shaped(
                    sl->config.far_threshold * 0.5f,
                    sl->config.far_threshold
                );
                def->requires_reference = true;
                def->is_projective = false;
                break;

            /* Horizontal directional prepositions */
            case SPATIAL_PREP_LEFT:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma * 2);
                def->angle_mf = fuzzy_mf_gaussian(
                    DEG_TO_RAD(-90.0f),
                    DEG_TO_RAD(sl->config.angle_tolerance)
                );
                def->direction[0] = -1.0f;
                def->direction[1] = 0.0f;
                def->direction[2] = 0.0f;
                def->requires_reference = true;
                def->is_projective = true;
                break;

            case SPATIAL_PREP_RIGHT:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma * 2);
                def->angle_mf = fuzzy_mf_gaussian(
                    DEG_TO_RAD(90.0f),
                    DEG_TO_RAD(sl->config.angle_tolerance)
                );
                def->direction[0] = 1.0f;
                def->direction[1] = 0.0f;
                def->direction[2] = 0.0f;
                def->requires_reference = true;
                def->is_projective = true;
                break;

            case SPATIAL_PREP_FRONT:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma * 2);
                def->angle_mf = fuzzy_mf_gaussian(
                    0.0f,
                    DEG_TO_RAD(sl->config.angle_tolerance)
                );
                def->direction[0] = 0.0f;
                def->direction[1] = 1.0f;
                def->direction[2] = 0.0f;
                def->requires_reference = true;
                def->is_projective = true;
                break;

            case SPATIAL_PREP_BEHIND:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma * 2);
                def->angle_mf = fuzzy_mf_gaussian(
                    DEG_TO_RAD(180.0f),
                    DEG_TO_RAD(sl->config.angle_tolerance)
                );
                def->direction[0] = 0.0f;
                def->direction[1] = -1.0f;
                def->direction[2] = 0.0f;
                def->requires_reference = true;
                def->is_projective = true;
                break;

            /* Vertical prepositions */
            case SPATIAL_PREP_ABOVE:
            case SPATIAL_PREP_OVER:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma);
                def->direction[0] = 0.0f;
                def->direction[1] = 0.0f;
                def->direction[2] = 1.0f;
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_BELOW:
            case SPATIAL_PREP_UNDER:
            case SPATIAL_PREP_BENEATH:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma);
                def->direction[0] = 0.0f;
                def->direction[1] = 0.0f;
                def->direction[2] = -1.0f;
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_ON:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, 0.1f); /* Contact */
                def->direction[0] = 0.0f;
                def->direction[1] = 0.0f;
                def->direction[2] = 1.0f;
                def->requires_reference = true;
                def->is_projective = false;
                break;

            /* Containment prepositions */
            case SPATIAL_PREP_IN:
            case SPATIAL_PREP_INSIDE:
            case SPATIAL_PREP_WITHIN:
                def->distance_mf = fuzzy_mf_z_shaped(0.0f, 1.0f);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_OUTSIDE:
                def->distance_mf = fuzzy_mf_s_shaped(0.5f, 1.5f);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            /* Path prepositions */
            case SPATIAL_PREP_THROUGH:
            case SPATIAL_PREP_ACROSS:
            case SPATIAL_PREP_ALONG:
                def->distance_mf = fuzzy_mf_trapezoidal(0.0f, 0.0f, 10.0f, 15.0f);
                def->supports_path = true;
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_TOWARD:
            case SPATIAL_PREP_INTO:
                def->distance_mf = fuzzy_mf_z_shaped(0.0f, 5.0f);
                def->supports_path = true;
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_AWAY:
            case SPATIAL_PREP_OUT_OF:
                def->distance_mf = fuzzy_mf_s_shaped(0.0f, 5.0f);
                def->supports_path = true;
                def->requires_reference = true;
                def->is_projective = false;
                break;

            /* Complex relations */
            case SPATIAL_PREP_BETWEEN:
            case SPATIAL_PREP_AMONG:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma * 2);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_AROUND:
                def->distance_mf = fuzzy_mf_gaussian(2.0f, sl->config.near_sigma);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_OPPOSITE:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma * 3);
                def->angle_mf = fuzzy_mf_gaussian(
                    DEG_TO_RAD(180.0f),
                    DEG_TO_RAD(sl->config.angle_tolerance)
                );
                def->requires_reference = true;
                def->is_projective = true;
                break;

            /* Boundary relations */
            case SPATIAL_PREP_AT:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, 0.5f);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            case SPATIAL_PREP_AGAINST:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, 0.1f);
                def->requires_reference = true;
                def->is_projective = false;
                break;

            default:
                def->distance_mf = fuzzy_mf_gaussian(0.0f, sl->config.near_sigma);
                def->requires_reference = true;
                def->is_projective = false;
                break;
        }
    }

    sl->num_prepositions = SPATIAL_PREPOSITION_COUNT;
}

/**
 * @brief Find preposition definition by word
 */
static const preposition_definition_t* find_preposition(
    const spatial_language_t* sl,
    const char* word
) {
    char normalized[NIMCP_ID_BUFFER_SIZE];
    normalize_word(word, normalized, sizeof(normalized));

    for (uint32_t i = 0; i < sl->num_prepositions; i++) {
        char prep_normalized[NIMCP_ID_BUFFER_SIZE];
        normalize_word(sl->prepositions[i].word, prep_normalized, sizeof(prep_normalized));

        if (strcmp(normalized, prep_normalized) == 0) {
            return &sl->prepositions[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_preposition: validation failed");
    return NULL;
}

/**
 * @brief Compute frame selection likelihood
 */
static float compute_frame_likelihood(
    const spatial_language_t* sl,
    reference_frame_t frame,
    const frame_context_t* context
) {
    float likelihood = 1.0f;

    switch (frame) {
        case REF_FRAME_EGOCENTRIC:
            /* Favored when speaker info available, no intrinsic reference */
            if (context->has_speaker) {
                likelihood *= 1.5f;
            }
            if (context->reference_has_intrinsic) {
                likelihood *= 0.7f;
            }
            break;

        case REF_FRAME_ALLOCENTRIC:
            /* Favored for large-scale spatial descriptions */
            likelihood *= 1.0f;
            break;

        case REF_FRAME_INTRINSIC:
            /* Requires object with intrinsic front/back */
            if (context->reference_has_intrinsic) {
                likelihood *= 2.0f;
            } else {
                likelihood *= 0.1f;
            }
            break;

        case REF_FRAME_RELATIVE:
            /* Requires both speaker and listener */
            if (context->has_speaker && context->has_listener) {
                likelihood *= 1.3f;
                /* Stronger when perspectives aligned */
                likelihood *= (1.0f + context->perspective_alignment);
            } else {
                likelihood *= 0.5f;
            }
            break;

        case REF_FRAME_GEOGRAPHIC:
            /* Cardinal directions - less common */
            likelihood *= 0.3f;
            break;

        default:
            break;
    }

    return likelihood;
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

spatial_language_config_t spatial_language_default_config(void) {
    spatial_language_config_t config = {
        .near_sigma = SPATIAL_LANG_DEFAULT_NEAR_SIGMA,
        .far_threshold = SPATIAL_LANG_DEFAULT_FAR_THRESHOLD,
        .angle_tolerance = SPATIAL_LANG_DEFAULT_ANGLE_TOLERANCE,

        .egocentric_prior = 0.4f,
        .allocentric_prior = 0.3f,
        .intrinsic_prior = 0.3f,

        .enable_hedge_processing = true,
        .enable_metaphor_grounding = true,
        .enable_bio_async = false,
        .enable_world_model = false,
        .enable_mesh_participation = true,

        .inflammation_sensitivity = 0.3f,
        .fatigue_sensitivity = 0.2f
    };
    return config;
}

bool spatial_language_validate_config(const spatial_language_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_language_validate_config: config is NULL");
        return false;
    }

    if (config->near_sigma <= 0.0f) {
        return false;
    }
    if (config->far_threshold <= 0.0f) {
        return false;
    }
    if (config->angle_tolerance <= 0.0f || config->angle_tolerance > 180.0f) {
        return false;
    }

    /* Priors should sum to approximately 1.0 */
    float prior_sum = config->egocentric_prior + config->allocentric_prior + config->intrinsic_prior;
    if (prior_sum < 0.9f || prior_sum > 1.1f) {
        return false;
    }

    if (config->inflammation_sensitivity < 0.0f || config->inflammation_sensitivity > 1.0f) {
        return false;
    }
    if (config->fatigue_sensitivity < 0.0f || config->fatigue_sensitivity > 1.0f) {
        return false;
    }

    return true;
}

spatial_language_t* spatial_language_create(void) {
    return spatial_language_create_custom(NULL);
}

spatial_language_t* spatial_language_create_custom(const spatial_language_config_t* config) {
    spatial_language_t* sl = (spatial_language_t*)nimcp_calloc(1, sizeof(spatial_language_t));
    if (!sl) {
        set_last_error("Failed to allocate spatial language processor");
        NIMCP_THROW_TO_IMMUNE(LING_ERR_ALLOC_FAILED, "spatial_language_create_custom: allocation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config && spatial_language_validate_config(config)) {
        sl->config = *config;
    } else {
        sl->config = spatial_language_default_config();
    }

    /* Initialize frame priors */
    sl->frame_priors[REF_FRAME_EGOCENTRIC] = sl->config.egocentric_prior;
    sl->frame_priors[REF_FRAME_ALLOCENTRIC] = sl->config.allocentric_prior;
    sl->frame_priors[REF_FRAME_INTRINSIC] = sl->config.intrinsic_prior;
    sl->frame_priors[REF_FRAME_RELATIVE] = 0.0f;
    sl->frame_priors[REF_FRAME_GEOGRAPHIC] = 0.0f;

    /* Initialize preposition definitions */
    init_default_prepositions(sl);

    /* Initialize state */
    sl->current_precision = 0.8f;
    sl->inflammation_level = 0.0f;
    sl->fatigue_level = 0.0f;

    /* Reset statistics */
    memset(&sl->stats, 0, sizeof(sl->stats));

    return sl;
}

void spatial_language_destroy(spatial_language_t* sl) {
    if (sl) {
        nimcp_free(sl);
        sl = NULL;
    }
}

/* ============================================================================
 * PARSING API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_parse_preposition(
    spatial_language_t* sl,
    const char* word,
    spatial_semantics_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_parse_preposition: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_parse_preposition: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_parse_preposition: out is NULL");

    /* Find preposition definition */
    const preposition_definition_t* def = find_preposition(sl, word);
    if (!def) {
        set_last_error("Unknown spatial preposition: %s", word);
        sl->stats.unknown_words++;
        return LING_ERR_UNKNOWN_PREPOSITION;
    }

    /* Fill output */
    memset(out, 0, sizeof(*out));
    out->preposition = def->type;
    out->frame = REF_FRAME_EGOCENTRIC; /* Default frame */

    /* Copy direction vector */
    out->direction[0] = def->direction[0];
    out->direction[1] = def->direction[1];
    out->direction[2] = def->direction[2];

    /* Set MF parameters */
    out->distance_center = def->distance_mf.params[0];
    out->distance_spread = def->distance_mf.params[1];

    /* Initial membership is 1.0 (will be evaluated at actual distance) */
    out->distance_membership = 1.0f;
    out->angle_membership = 1.0f;

    /* Compute confidence (affected by inflammation/fatigue) */
    float base_confidence = 0.9f;
    base_confidence -= sl->inflammation_level * sl->config.inflammation_sensitivity;
    base_confidence -= sl->fatigue_level * sl->config.fatigue_sensitivity;
    out->overall_confidence = fmaxf(0.5f, base_confidence);
    out->frame_confidence = sl->frame_priors[REF_FRAME_EGOCENTRIC];

    out->hedge_applied = false;
    out->hedge_type = 0; /* FUZZY_HEDGE_NONE */

    /* Update statistics */
    sl->stats.prepositions_parsed++;
    sl->stats.avg_confidence = (sl->stats.avg_confidence * (sl->stats.prepositions_parsed - 1)
                               + out->overall_confidence) / sl->stats.prepositions_parsed;

    return LING_ERR_OK;
}

int spatial_language_parse_with_frame(
    spatial_language_t* sl,
    const char* word,
    reference_frame_t frame,
    spatial_semantics_t* out
) {
    int result = spatial_language_parse_preposition(sl, word, out);
    if (result != LING_ERR_OK) {
        return result;
    }

    if (frame >= REF_FRAME_COUNT) {
        set_last_error("Invalid reference frame: %d", frame);
        return LING_ERR_INVALID_FRAME;
    }

    out->frame = frame;
    out->frame_confidence = sl->frame_priors[frame];

    return LING_ERR_OK;
}

bool spatial_language_is_preposition(
    const spatial_language_t* sl,
    const char* word
) {
    if (!sl || !word) {
        return false;
    }
    return find_preposition(sl, word) != NULL;
}

int spatial_language_get_preposition_type(
    const spatial_language_t* sl,
    const char* word,
    spatial_preposition_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_preposition_type: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_preposition_type: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_preposition_type: out is NULL");

    const preposition_definition_t* def = find_preposition(sl, word);
    if (!def) {
        return LING_ERR_UNKNOWN_PREPOSITION;
    }

    *out = def->type;
    return LING_ERR_OK;
}

int spatial_language_get_preposition_word(
    const spatial_language_t* sl,
    spatial_preposition_t type,
    char* word,
    uint32_t max_len
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_preposition_word: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_preposition_word: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(max_len > 0, LING_ERR_INVALID_PARAM,
        "spatial_language_get_preposition_word: max_len is 0");

    if (type >= SPATIAL_PREPOSITION_COUNT) {
        set_last_error("Invalid preposition type: %d", type);
        return LING_ERR_INVALID_PARAM;
    }

    strncpy(word, g_preposition_words[type], max_len - 1);
    word[max_len - 1] = '\0';

    return LING_ERR_OK;
}

/* ============================================================================
 * REFERENCE FRAME API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_select_frame(
    spatial_language_t* sl,
    const frame_context_t* context,
    reference_frame_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_select_frame: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(context != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_select_frame: context is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_select_frame: out is NULL");

    /* Bayesian frame selection: P(frame|context) ∝ P(context|frame) * P(frame) */
    float posteriors[REF_FRAME_COUNT];
    float total = 0.0f;

    for (int i = 0; i < REF_FRAME_COUNT; i++) {
        float likelihood = compute_frame_likelihood(sl, (reference_frame_t)i, context);
        posteriors[i] = likelihood * sl->frame_priors[i];
        total += posteriors[i];
    }

    /* Normalize posteriors */
    if (total > 0.0f) {
        for (int i = 0; i < REF_FRAME_COUNT; i++) {
            posteriors[i] /= total;
        }
    }

    /* Find maximum posterior */
    reference_frame_t best_frame = REF_FRAME_EGOCENTRIC;
    float best_posterior = posteriors[0];

    for (int i = 1; i < REF_FRAME_COUNT; i++) {
        if (posteriors[i] > best_posterior) {
            best_posterior = posteriors[i];
            best_frame = (reference_frame_t)i;
        }
    }

    *out = best_frame;

    /* Update statistics */
    sl->stats.frames_selected++;
    switch (best_frame) {
        case REF_FRAME_EGOCENTRIC:
            sl->stats.egocentric_selections++;
            break;
        case REF_FRAME_ALLOCENTRIC:
            sl->stats.allocentric_selections++;
            break;
        case REF_FRAME_INTRINSIC:
            sl->stats.intrinsic_selections++;
            break;
        default:
            break;
    }

    return LING_ERR_OK;
}

int spatial_language_get_frame_probabilities(
    spatial_language_t* sl,
    const frame_context_t* context,
    float* probabilities
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_frame_probabilities: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(context != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_frame_probabilities: context is NULL");
    NIMCP_CHECK_THROW_IMMUNE(probabilities != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_frame_probabilities: probabilities is NULL");

    float total = 0.0f;

    for (int i = 0; i < REF_FRAME_COUNT; i++) {
        float likelihood = compute_frame_likelihood(sl, (reference_frame_t)i, context);
        probabilities[i] = likelihood * sl->frame_priors[i];
        total += probabilities[i];
    }

    /* Normalize */
    if (total > 0.0f) {
        for (int i = 0; i < REF_FRAME_COUNT; i++) {
            probabilities[i] /= total;
        }
    }

    return LING_ERR_OK;
}

int spatial_language_transform_frame(
    spatial_language_t* sl,
    spatial_semantics_t* semantics,
    reference_frame_t from_frame,
    reference_frame_t to_frame,
    const frame_context_t* context
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_transform_frame: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(semantics != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_transform_frame: semantics is NULL");

    if (from_frame == to_frame) {
        return LING_ERR_OK; /* No transform needed */
    }

    /* For projective prepositions (left/right), transform direction */
    const preposition_definition_t* def = NULL;
    for (uint32_t i = 0; i < sl->num_prepositions; i++) {
        if (sl->prepositions[i].type == semantics->preposition) {
            def = &sl->prepositions[i];
            break;
        }
    }

    if (def && def->is_projective && context) {
        /* Egocentric → Allocentric: Rotate by speaker orientation */
        if (from_frame == REF_FRAME_EGOCENTRIC && to_frame == REF_FRAME_ALLOCENTRIC) {
            if (context->has_speaker) {
                /* Simple 2D rotation by speaker heading */
                float heading = atan2f(context->speaker_orientation[1],
                                      context->speaker_orientation[0]);
                float cos_h = cosf(heading);
                float sin_h = sinf(heading);

                float new_x = semantics->direction[0] * cos_h - semantics->direction[1] * sin_h;
                float new_y = semantics->direction[0] * sin_h + semantics->direction[1] * cos_h;

                semantics->direction[0] = new_x;
                semantics->direction[1] = new_y;
            }
        }
        /* Allocentric → Egocentric: Inverse rotation */
        else if (from_frame == REF_FRAME_ALLOCENTRIC && to_frame == REF_FRAME_EGOCENTRIC) {
            if (context->has_speaker) {
                float heading = atan2f(context->speaker_orientation[1],
                                      context->speaker_orientation[0]);
                float cos_h = cosf(-heading);
                float sin_h = sinf(-heading);

                float new_x = semantics->direction[0] * cos_h - semantics->direction[1] * sin_h;
                float new_y = semantics->direction[0] * sin_h + semantics->direction[1] * cos_h;

                semantics->direction[0] = new_x;
                semantics->direction[1] = new_y;
            }
        }
    }

    semantics->frame = to_frame;
    semantics->frame_confidence = sl->frame_priors[to_frame];

    return LING_ERR_OK;
}

/* ============================================================================
 * FUZZY SEMANTICS API IMPLEMENTATION
 * ============================================================================ */

float spatial_language_evaluate_membership(
    const spatial_language_t* sl,
    const spatial_semantics_t* semantics,
    float distance,
    float angle
) {
    if (!sl || !semantics) return 0.0f;

    /* Find preposition definition */
    const preposition_definition_t* def = NULL;
    for (uint32_t i = 0; i < sl->num_prepositions; i++) {
        if (sl->prepositions[i].type == semantics->preposition) {
            def = &sl->prepositions[i];
            break;
        }
    }

    if (!def) return 0.0f;

    /* Evaluate distance membership */
    float dist_membership = fuzzy_mf_evaluate(&def->distance_mf, distance);

    /* Evaluate angle membership if applicable */
    float angle_membership = 1.0f;
    if (def->is_projective && def->angle_mf.type != FUZZY_MF_SINGLETON) {
        angle_membership = fuzzy_mf_evaluate(&def->angle_mf, angle);
    }

    /* Combine using T-norm (minimum for standard fuzzy AND) */
    float combined = fminf(dist_membership, angle_membership);

    /* Apply hedge if present */
    if (semantics->hedge_applied) {
        combined = fuzzy_apply_hedge(combined, (fuzzy_hedge_t)semantics->hedge_type);
    }

    return combined;
}

int spatial_language_apply_hedge(
    spatial_language_t* sl,
    spatial_semantics_t* semantics,
    uint8_t hedge
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_apply_hedge: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(semantics != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_apply_hedge: semantics is NULL");

    if (!sl->config.enable_hedge_processing) {
        return LING_ERR_OK; /* Silently ignore if disabled */
    }

    if (hedge >= FUZZY_HEDGE_TYPE_COUNT) {
        set_last_error("Invalid hedge type: %d", hedge);
        return LING_ERR_INVALID_PARAM;
    }

    /* Apply hedge transformation to memberships */
    semantics->distance_membership = fuzzy_apply_hedge(
        semantics->distance_membership, (fuzzy_hedge_t)hedge);
    semantics->angle_membership = fuzzy_apply_hedge(
        semantics->angle_membership, (fuzzy_hedge_t)hedge);

    semantics->hedge_applied = true;
    semantics->hedge_type = hedge;

    /* Update statistics */
    sl->stats.hedges_applied++;

    return LING_ERR_OK;
}

int spatial_language_get_distance_mf(
    const spatial_language_t* sl,
    spatial_preposition_t prep,
    fuzzy_mf_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_distance_mf: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_distance_mf: out is NULL");

    if (prep >= sl->num_prepositions) {
        return LING_ERR_INVALID_PARAM;
    }

    *out = sl->prepositions[prep].distance_mf;
    return LING_ERR_OK;
}

int spatial_language_get_angle_mf(
    const spatial_language_t* sl,
    spatial_preposition_t prep,
    fuzzy_mf_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_angle_mf: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_angle_mf: out is NULL");

    if (prep >= sl->num_prepositions) {
        return LING_ERR_INVALID_PARAM;
    }

    *out = sl->prepositions[prep].angle_mf;
    return LING_ERR_OK;
}

/* ============================================================================
 * METAPHOR API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_ground_metaphor(
    spatial_language_t* sl,
    const char* abstract_phrase,
    spatial_semantics_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_ground_metaphor: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(abstract_phrase != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_ground_metaphor: abstract_phrase is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_ground_metaphor: out is NULL");

    if (!sl->config.enable_metaphor_grounding) {
        return LING_ERR_INVALID_PARAM;
    }

    /* Search registered metaphors */
    for (uint32_t i = 0; i < sl->num_metaphors; i++) {
        if (strstr(abstract_phrase, sl->metaphors[i].source_domain)) {
            /* Found matching metaphor */
            int result = spatial_language_parse_preposition(
                sl, g_preposition_words[sl->metaphors[i].base_prep], out);

            if (result == LING_ERR_OK) {
                out->overall_confidence *= sl->metaphors[i].mapping_strength;
                sl->stats.metaphors_grounded++;
                return LING_ERR_OK;
            }
        }
    }

    /* Default vertical metaphors */
    if (strstr(abstract_phrase, "up") || strstr(abstract_phrase, "rise") ||
        strstr(abstract_phrase, "increase") || strstr(abstract_phrase, "high")) {
        return spatial_language_parse_preposition(sl, "above", out);
    }

    if (strstr(abstract_phrase, "down") || strstr(abstract_phrase, "fall") ||
        strstr(abstract_phrase, "decrease") || strstr(abstract_phrase, "low")) {
        return spatial_language_parse_preposition(sl, "below", out);
    }

    set_last_error("No metaphor mapping found for: %s", abstract_phrase);
    return LING_ERR_UNKNOWN_WORD;
}

int spatial_language_register_metaphor(
    spatial_language_t* sl,
    const spatial_metaphor_t* metaphor
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_register_metaphor: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(metaphor != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_register_metaphor: metaphor is NULL");

    if (sl->num_metaphors >= 32) {
        set_last_error("Maximum metaphor count reached");
        return LING_ERR_BUFFER_OVERFLOW;
    }

    sl->metaphors[sl->num_metaphors++] = *metaphor;
    return LING_ERR_OK;
}

/* ============================================================================
 * MESH INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_mesh_process(
    spatial_language_t* sl,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_mesh_process: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(request != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_mesh_process: request is NULL");
    NIMCP_CHECK_THROW_IMMUNE(belief != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_mesh_process: belief is NULL");

    if (request->type != LING_REQUEST_PARSE_SPATIAL) {
        return LING_ERR_INVALID_PARAM;
    }

    /* Parse the spatial preposition */
    spatial_semantics_t semantics;
    int result = spatial_language_parse_preposition(sl, request->input_word, &semantics);

    /* Populate belief */
    belief->belief_id = (uint32_t)(request->request_id & 0xFFFFFFFF);
    belief->source_module_id = BIO_MODULE_SPATIAL_LANGUAGE;
    snprintf(belief->topic, sizeof(belief->topic),
             "spatial_preposition_%s", request->input_word);

    if (result == LING_ERR_OK) {
        belief->certainty = semantics.overall_confidence;
        belief->precision = sl->current_precision;

        /* Encode semantics into belief vector */
        belief->vector_dim = 8;
        belief->belief_vector[0] = (float)semantics.preposition / (SPATIAL_PREPOSITION_COUNT > 0 ? SPATIAL_PREPOSITION_COUNT : 1);
        belief->belief_vector[1] = (float)semantics.frame / (REF_FRAME_COUNT > 0 ? REF_FRAME_COUNT : 1);
        belief->belief_vector[2] = semantics.distance_membership;
        belief->belief_vector[3] = semantics.angle_membership;
        belief->belief_vector[4] = semantics.direction[0];
        belief->belief_vector[5] = semantics.direction[1];
        belief->belief_vector[6] = semantics.direction[2];
        belief->belief_vector[7] = semantics.frame_confidence;
    } else {
        belief->certainty = 0.0f;
        belief->precision = LINGUISTICS_PRECISION_FLOOR;
        belief->vector_dim = 0;
    }

    belief->timestamp_ms = request->timestamp_ms;

    return result;
}

int spatial_language_mesh_update(
    spatial_language_t* sl,
    const linguistics_belief_t* neighbor_beliefs,
    uint32_t neighbor_count,
    linguistics_belief_t* updated_belief
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_mesh_update: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(updated_belief != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_mesh_update: updated_belief is NULL");

    if (neighbor_count == 0 || !neighbor_beliefs) {
        return LING_ERR_OK; /* Nothing to update from */
    }

    /* FEP belief update: μ' = μ - lr * Π * ε */
    float lr = LINGUISTICS_FEP_LEARNING_RATE;

    for (uint32_t i = 0; i < neighbor_count; i++) {
        const linguistics_belief_t* neighbor = &neighbor_beliefs[i];

        /* Skip if no vector or different dimension */
        if (neighbor->vector_dim == 0 ||
            neighbor->vector_dim != updated_belief->vector_dim) {
            continue;
        }

        /* Compute precision-weighted error */
        float precision = fminf(neighbor->precision, LINGUISTICS_PRECISION_CEILING);
        precision = fmaxf(precision, LINGUISTICS_PRECISION_FLOOR);

        for (uint32_t j = 0; j < updated_belief->vector_dim; j++) {
            float error = neighbor->belief_vector[j] - updated_belief->belief_vector[j];
            float delta = lr * precision * error;
            updated_belief->belief_vector[j] += delta;
        }

        /* Update certainty with weighted average */
        float weight = precision / (precision + updated_belief->precision);
        updated_belief->certainty = (1.0f - weight) * updated_belief->certainty +
                                     weight * neighbor->certainty;
    }

    return LING_ERR_OK;
}

float spatial_language_get_precision(const spatial_language_t* sl) {
    if (!sl) return LINGUISTICS_PRECISION_FLOOR;

    /* Precision decreases with inflammation and fatigue */
    float precision = sl->current_precision;
    precision -= sl->inflammation_level * sl->config.inflammation_sensitivity;
    precision -= sl->fatigue_level * sl->config.fatigue_sensitivity;

    /* Clamp to valid range */
    precision = fmaxf(precision, LINGUISTICS_PRECISION_FLOOR);
    precision = fminf(precision, LINGUISTICS_PRECISION_CEILING);

    return precision;
}

int spatial_language_get_mesh_handler(
    spatial_language_t* sl,
    linguistics_mesh_handler_t* handler
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_mesh_handler: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(handler != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_mesh_handler: handler is NULL");

    handler->process = (int (*)(void*, const linguistics_request_t*, linguistics_belief_t*))
                       spatial_language_mesh_process;
    handler->update = (int (*)(void*, const linguistics_belief_t*, uint32_t, linguistics_belief_t*))
                      spatial_language_mesh_update;
    handler->get_precision = (float (*)(void*))spatial_language_get_precision;
    handler->ctx = sl;

    return LING_ERR_OK;
}

/* ============================================================================
 * WORLD MODEL INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_predict_outcome(
    spatial_language_t* sl,
    const char* instruction,
    const float current_position[3],
    float predicted_position[3]
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_predict_outcome: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(instruction != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_predict_outcome: instruction is NULL");
    NIMCP_CHECK_THROW_IMMUNE(current_position != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_predict_outcome: current_position is NULL");
    NIMCP_CHECK_THROW_IMMUNE(predicted_position != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_predict_outcome: predicted_position is NULL");

    /* Parse instruction for spatial preposition */
    spatial_semantics_t semantics;
    int result = spatial_language_parse_preposition(sl, instruction, &semantics);

    if (result != LING_ERR_OK) {
        /* Try to extract preposition from instruction */
        for (uint32_t i = 0; i < sl->num_prepositions; i++) {
            if (strstr(instruction, sl->prepositions[i].word)) {
                result = spatial_language_parse_preposition(
                    sl, sl->prepositions[i].word, &semantics);
                break;
            }
        }
    }

    if (result != LING_ERR_OK) {
        /* Default: no movement */
        predicted_position[0] = current_position[0];
        predicted_position[1] = current_position[1];
        predicted_position[2] = current_position[2];
        return result;
    }

    /* Predict movement based on direction and typical distance */
    float move_distance = semantics.distance_center;
    if (move_distance == 0.0f) {
        move_distance = 1.0f; /* Default step */
    }

    predicted_position[0] = current_position[0] + semantics.direction[0] * move_distance;
    predicted_position[1] = current_position[1] + semantics.direction[1] * move_distance;
    predicted_position[2] = current_position[2] + semantics.direction[2] * move_distance;

    return LING_ERR_OK;
}

/* ============================================================================
 * MODULATION API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_set_inflammation(
    spatial_language_t* sl,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_set_inflammation: sl is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Inflammation level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    sl->inflammation_level = level;
    return LING_ERR_OK;
}

int spatial_language_set_fatigue(
    spatial_language_t* sl,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_set_fatigue: sl is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Fatigue level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    sl->fatigue_level = level;
    return LING_ERR_OK;
}

/* ============================================================================
 * STATISTICS API IMPLEMENTATION
 * ============================================================================ */

int spatial_language_get_stats(
    const spatial_language_t* sl,
    spatial_language_stats_t* stats
) {
    NIMCP_CHECK_THROW_IMMUNE(sl != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_stats: sl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(stats != NULL, LING_ERR_NULL_POINTER,
        "spatial_language_get_stats: stats is NULL");

    *stats = sl->stats;
    return LING_ERR_OK;
}

void spatial_language_reset_stats(spatial_language_t* sl) {
    if (sl) {
        memset(&sl->stats, 0, sizeof(sl->stats));
    }
}

const char* spatial_language_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * UTILITY API IMPLEMENTATION
 * ============================================================================ */

const char* spatial_language_preposition_name(spatial_preposition_t prep) {
    if (prep >= SPATIAL_PREPOSITION_COUNT) {
        return "unknown";
    }
    return g_preposition_words[prep];
}

const char* spatial_language_frame_name(reference_frame_t frame) {
    if (frame >= REF_FRAME_COUNT) {
        return "unknown";
    }
    return g_frame_names[frame];
}
