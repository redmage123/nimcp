/**
 * @file nimcp_language_parietal_bridge.c
 * @brief Parietal cortex ↔ grounded language for spatial/numerical semantics
 *
 * WHAT: Integrates parietal spatial reasoning with language for embodied grounding
 * WHY:  Enables spatial prepositions, numerical language, attention-guided semantics
 * HOW:  Parietal state (spatial maps, quantities, attention) → linguistic features
 *
 * Parietal language functions:
 *   - Spatial semantics: "above", "left", "inside" from dorsal stream
 *   - Numerical cognition: quantity → number words, arithmetic language
 *   - Attention guidance: saliency maps → topic focus
 *   - Sensorimotor grounding: action → spatial language
 *   - Visuospatial imagery: mental rotation → descriptive geometry
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/**
 * @brief Spatial semantic features from parietal cortex
 */
typedef struct {
    float x_position;          /**< Horizontal spatial coordinate */
    float y_position;          /**< Vertical spatial coordinate */
    float z_position;          /**< Depth coordinate */
    float spatial_attention;   /**< Attention weight at location */
    float object_size;         /**< Size estimate */
    float distance;            /**< Distance from reference point */
    float* spatial_map;        /**< Full spatial representation */
    uint32_t map_dim;          /**< Spatial map dimensionality */
} parietal_spatial_features_t;

/**
 * @brief Numerical semantic features from parietal cortex
 */
typedef struct {
    float quantity;            /**< Approximate number system (ANS) estimate */
    float magnitude;           /**< Magnitude representation */
    float ordinality;          /**< Order/sequence position */
    bool is_cardinal;          /**< Cardinal vs ordinal number */
    float precision;           /**< Numerical precision/confidence */
} parietal_numerical_features_t;

/**
 * @brief Extract spatial features from parietal cortex
 *
 * @param brain        Brain with parietal cortex
 * @param out_spatial  Output spatial features (allocated by caller)
 * @return 0 on success, -1 on error
 */
int parietal_extract_spatial_features(brain_t brain,
                                        parietal_spatial_features_t* out_spatial) {
    if (!brain || !out_spatial) return -1;

    /* Query parietal cortex adapter if available */
    if (brain->parietal_cortex) {
        /* Actual implementation: query parietal_cortex_get_spatial_state()
         * For now, placeholder using brain state. */
    }

    /* Placeholder spatial features from brain activity */
    out_spatial->x_position = 0.0f;
    out_spatial->y_position = 0.0f;
    out_spatial->z_position = 0.0f;
    out_spatial->spatial_attention = 0.5f;
    out_spatial->object_size = 1.0f;
    out_spatial->distance = 1.0f;

    /* Allocate spatial map (simplified 2D grid representation) */
    const uint32_t spatial_map_size = 64; /* 8×8 spatial grid */
    out_spatial->spatial_map = nimcp_malloc(spatial_map_size * sizeof(float));
    if (!out_spatial->spatial_map) return -1;
    out_spatial->map_dim = spatial_map_size;

    /* Initialize spatial map with attention-weighted coordinates */
    for (uint32_t i = 0; i < spatial_map_size; i++) {
        float x = (i % 8) / 8.0f;
        float y = (i / 8) / 8.0f;
        float dist_from_center = sqrtf((x - 0.5f)*(x - 0.5f) + (y - 0.5f)*(y - 0.5f));
        out_spatial->spatial_map[i] = expf(-dist_from_center * 2.0f); /* Gaussian attention */
    }

    NIMCP_LOGGING_DEBUG("Parietal spatial: pos=(%.2f,%.2f,%.2f) attention=%.3f",
                        out_spatial->x_position, out_spatial->y_position,
                        out_spatial->z_position, out_spatial->spatial_attention);

    return 0;
}

/**
 * @brief Extract numerical features from parietal cortex
 *
 * @param brain          Brain with parietal cortex
 * @param out_numerical  Output numerical features (allocated by caller)
 * @return 0 on success, -1 on error
 */
int parietal_extract_numerical_features(brain_t brain,
                                          parietal_numerical_features_t* out_numerical) {
    if (!brain || !out_numerical) return -1;

    /* Query parietal cortex for approximate number system (ANS) activation.
     * ANS represents quantities ~1-9 with decreasing precision for larger numbers. */

    /* Placeholder: derive from brain activity patterns */
    out_numerical->quantity = 3.0f; /* Approximate count */
    out_numerical->magnitude = logf(out_numerical->quantity + 1.0f); /* Log scale */
    out_numerical->ordinality = 0.0f; /* Not ordinal by default */
    out_numerical->is_cardinal = true;
    out_numerical->precision = 1.0f / (1.0f + out_numerical->quantity * 0.1f); /* Weber's law */

    NIMCP_LOGGING_DEBUG("Parietal numerical: quantity=%.1f magnitude=%.3f "
                        "precision=%.3f cardinal=%d",
                        out_numerical->quantity, out_numerical->magnitude,
                        out_numerical->precision, out_numerical->is_cardinal);

    return 0;
}

/**
 * @brief Map spatial features to spatial prepositions
 *
 * Determines appropriate spatial language based on geometric relationships.
 *
 * @param spatial        Spatial features
 * @param out_relation   Output spatial relation string (caller allocates buffer)
 * @param buffer_size    Size of output buffer
 * @return 0 on success, -1 on error
 */
int parietal_spatial_to_preposition(const parietal_spatial_features_t* spatial,
                                      char* out_relation,
                                      size_t buffer_size) {
    if (!spatial || !out_relation) return -1;

    /* Determine spatial relation from coordinates */
    if (spatial->z_position < -0.5f) {
        strncpy(out_relation, "behind", buffer_size - 1);
    } else if (spatial->z_position > 0.5f) {
        strncpy(out_relation, "in front of", buffer_size - 1);
    } else if (spatial->y_position > 0.5f) {
        strncpy(out_relation, "above", buffer_size - 1);
    } else if (spatial->y_position < -0.5f) {
        strncpy(out_relation, "below", buffer_size - 1);
    } else if (spatial->x_position > 0.5f) {
        strncpy(out_relation, "to the right of", buffer_size - 1);
    } else if (spatial->x_position < -0.5f) {
        strncpy(out_relation, "to the left of", buffer_size - 1);
    } else if (spatial->distance < 0.3f) {
        strncpy(out_relation, "inside", buffer_size - 1);
    } else {
        strncpy(out_relation, "near", buffer_size - 1);
    }
    out_relation[buffer_size - 1] = '\0';

    return 0;
}

/**
 * @brief Convert parietal numerical features to number words
 *
 * @param numerical    Numerical features
 * @param out_word     Output number word (caller allocates buffer)
 * @param buffer_size  Size of output buffer
 * @return 0 on success, -1 on error
 */
int parietal_number_to_word(const parietal_numerical_features_t* numerical,
                              char* out_word,
                              size_t buffer_size) {
    if (!numerical || !out_word) return -1;

    /* Map quantity to number word */
    int n = (int)(numerical->quantity + 0.5f); /* Round to nearest integer */

    const char* number_words[] = {
        "zero", "one", "two", "three", "four", "five",
        "six", "seven", "eight", "nine", "ten"
    };

    if (n >= 0 && n <= 10) {
        strncpy(out_word, number_words[n], buffer_size - 1);
    } else {
        snprintf(out_word, buffer_size, "%d", n);
    }
    out_word[buffer_size - 1] = '\0';

    return 0;
}

/**
 * @brief Integrate parietal spatial/numerical semantics into grounded language
 *
 * Enriches language production with spatial prepositions and numerical language.
 *
 * @param brain  Brain with parietal cortex
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_parietal(brain_t brain,
                                          grounded_language_t* gl,
                                          void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    /* Check if parietal coupling is enabled */
    extern bool grounded_language_parietal_enabled(const grounded_language_t* gl);
    if (!grounded_language_parietal_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Get cascade state */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Extract spatial features */
    parietal_spatial_features_t spatial;
    if (parietal_extract_spatial_features(brain, &spatial) == 0) {
        /* Blend spatial map into content_intent (dims 300-363) */
        const float w_spatial = 0.22f;
        uint32_t spatial_base = 300;

        for (uint32_t i = 0; i < spatial.map_dim &&
                           (spatial_base + i) < cs->dim; i++) {
            float s = spatial.spatial_map[i];
            if (isfinite(s)) {
                cs->content_intent[spatial_base + i] += w_spatial * s;
            }
        }

        /* Add spatial coordinate features (dims 364-366) */
        if (cs->dim > 366) {
            cs->content_intent[364] += w_spatial * spatial.x_position;
            cs->content_intent[365] += w_spatial * spatial.y_position;
            cs->content_intent[366] += w_spatial * spatial.z_position;
        }

        /* Free allocated spatial map */
        if (spatial.spatial_map) nimcp_free(spatial.spatial_map);
    }

    /* Extract numerical features */
    parietal_numerical_features_t numerical;
    if (parietal_extract_numerical_features(brain, &numerical) == 0) {
        /* Blend numerical features into content_intent (dims 370-375) */
        const float w_numerical = 0.15f;

        if (cs->dim > 375) {
            cs->content_intent[370] += w_numerical * numerical.quantity;
            cs->content_intent[371] += w_numerical * numerical.magnitude;
            cs->content_intent[372] += w_numerical * numerical.ordinality;
            cs->content_intent[373] += w_numerical * (numerical.is_cardinal ? 1.0f : 0.0f);
            cs->content_intent[374] += w_numerical * numerical.precision;
        }
    }

    NIMCP_LOGGING_DEBUG("Parietal integration: spatial_weight=%.2f numerical_weight=%.2f",
                        w_spatial, w_numerical);

    return 0;
}

/**
 * @brief Query whether parietal coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if parietal integration is active
 */
bool grounded_language_parietal_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    /* For now, always enabled when parietal cortex is present.
     * Future: add runtime flag to grounded_language struct. */
    return true;
}
