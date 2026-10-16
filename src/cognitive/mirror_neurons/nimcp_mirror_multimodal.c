/**
 * @file nimcp_mirror_multimodal.c
 * @brief Multi-Modal Action Features for Mirror Neurons - Implementation
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Multi-modal action feature representation for enhanced mirror neuron processing
 * WHY:  Actions are perceived through multiple sensory modalities - visual, motor, auditory, semantic
 * HOW:  Unified feature structure with modality-specific vectors and cross-modal fusion
 */

#include "cognitive/mirror_neurons/nimcp_mirror_multimodal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_multimodal, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define COHERENCE_THRESHOLD           0.6f
#include "constants/nimcp_constants.h"
#define EPSILON                       NIMCP_EPSILON_NUMERICAL
#define VISUAL_MOTOR_CORRELATION      0.8f
#define SEMANTIC_CONFIDENCE_BOOST     0.1f

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute dot product of two float arrays
 */
static float dot_product(const float* a, const float* b, size_t len) {
    float sum = 0.0f;
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            mirror_multimodal_heartbeat("mirror_multi_loop",
                             (float)(i + 1) / (float)len);
        }

        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Compute vector magnitude
 */
static float magnitude(const float* v, size_t len) {
    return sqrtf(dot_product(v, v, len));
}

/**
 * @brief Compute cosine similarity between two vectors
 */
static float cosine_similarity(const float* a, const float* b, size_t len) {
    float mag_a = magnitude(a, len);
    float mag_b = magnitude(b, len);
    if (mag_a < EPSILON || mag_b < EPSILON) {
        return 0.0f;
    }
    return dot_product(a, b, len) / (mag_a * mag_b);
}

/**
 * @brief Normalize weights to sum to 1.0
 */
static void normalize_weights(float* w1, float* w2, float* w3, float* w4) {
    float sum = *w1 + *w2 + *w3 + *w4;
    if (sum < EPSILON) {
        *w1 = *w2 = *w3 = *w4 = 0.25f;
        return;
    }
    *w1 /= sum;
    *w2 /= sum;
    *w3 /= sum;
    *w4 /= sum;
}

/**
 * @brief Flatten visual features to array
 */
static void flatten_visual(const visual_features_t* v, float* out, size_t* len) {
    size_t idx = 0;
    for (size_t i = 0; i < 3 && idx < NIMCP_MULTIMODAL_VISUAL_DIM; i++, idx++) {
        out[idx] = v->motion[i];
    }
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_VISUAL_DIM; i++, idx++) {
        out[idx] = v->posture[i];
    }
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_VISUAL_DIM; i++, idx++) {
        out[idx] = v->trajectory[i];
    }
    out[idx++] = v->biological_motion;
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_VISUAL_DIM; i++, idx++) {
        out[idx] = v->object_features[i];
    }
    *len = idx;
}

/**
 * @brief Flatten motor features to array
 */
static void flatten_motor(const motor_features_t* m, float* out, size_t* len) {
    size_t idx = 0;
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_MOTOR_DIM; i++, idx++) {
        out[idx] = m->effector[i];
    }
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_MOTOR_DIM; i++, idx++) {
        out[idx] = m->grip_type[i];
    }
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_MOTOR_DIM; i++, idx++) {
        out[idx] = m->force_profile[i];
    }
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_MOTOR_DIM; i++, idx++) {
        out[idx] = m->kinematics[i];
    }
    *len = idx;
}

/**
 * @brief Flatten auditory features to array
 */
static void flatten_auditory(const auditory_features_t* a, float* out, size_t* len) {
    size_t idx = 0;
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_AUDITORY_DIM; i++, idx++) {
        out[idx] = a->action_sound[i];
    }
    for (size_t i = 0; i < 2 && idx < NIMCP_MULTIMODAL_AUDITORY_DIM; i++, idx++) {
        out[idx] = a->vocalization[i];
    }
    for (size_t i = 0; i < 2 && idx < NIMCP_MULTIMODAL_AUDITORY_DIM; i++, idx++) {
        out[idx] = a->rhythm[i];
    }
    *len = idx;
}

/**
 * @brief Flatten semantic features to array
 */
static void flatten_semantic(const semantic_features_t* s, float* out, size_t* len) {
    size_t idx = 0;
    for (size_t i = 0; i < 4 && idx < NIMCP_MULTIMODAL_SEMANTIC_DIM; i++, idx++) {
        out[idx] = s->category[i];
    }
    out[idx++] = s->transitivity;
    for (size_t i = 0; i < 2 && idx < NIMCP_MULTIMODAL_SEMANTIC_DIM; i++, idx++) {
        out[idx] = s->goal[i];
    }
    out[idx++] = s->affordance;
    *len = idx;
}

/* ============================================================================
 * Core API - Feature Initialization
 * ============================================================================ */

multimodal_action_features_t multimodal_features_init(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_features_", 0.0f);


    multimodal_action_features_t features;
    memset(&features, 0, sizeof(features));

    /* Set default fusion weights */
    features.visual_weight = NIMCP_MULTIMODAL_VISUAL_WEIGHT;
    features.motor_weight = NIMCP_MULTIMODAL_MOTOR_WEIGHT;
    features.auditory_weight = NIMCP_MULTIMODAL_AUDITORY_WEIGHT;
    features.semantic_weight = NIMCP_MULTIMODAL_SEMANTIC_WEIGHT;

    features.fused_valid = false;
    features.overall_confidence = 0.0f;

    return features;
}

fusion_config_t multimodal_get_default_fusion_config(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_get_defau", 0.0f);


    fusion_config_t config;

    config.visual_weight = NIMCP_MULTIMODAL_VISUAL_WEIGHT;
    config.motor_weight = NIMCP_MULTIMODAL_MOTOR_WEIGHT;
    config.auditory_weight = NIMCP_MULTIMODAL_AUDITORY_WEIGHT;
    config.semantic_weight = NIMCP_MULTIMODAL_SEMANTIC_WEIGHT;
    config.normalize_weights = true;
    config.require_min_confidence = false;
    config.min_confidence = NIMCP_CONFIDENCE_LOW;

    return config;
}

/* ============================================================================
 * Core API - Feature Conversion
 * ============================================================================ */

bool multimodal_from_action(const action_t* action,
                            multimodal_action_features_t* out_features) {
    if (!action || !out_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_from_action: required parameter is NULL");
        return false;
    }

    *out_features = multimodal_features_init();

    /* Map action features to modalities */
    /* features[0-7]   -> visual */
    /* features[8-15]  -> motor */
    /* features[16-23] -> auditory */
    /* features[24-31] -> semantic */

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_from_acti", 0.0f);


    uint32_t nf = action->num_features;

    /* Visual features (0-7) */
    if (nf > 0) {
        out_features->visual.motion[0] = action->features[0];
        out_features->visual.motion[1] = nf > 1 ? action->features[1] : 0.0f;
        out_features->visual.motion[2] = nf > 2 ? action->features[2] : 0.0f;
    }
    if (nf > 3) {
        out_features->visual.posture[0] = action->features[3];
        out_features->visual.posture[1] = nf > 4 ? action->features[4] : 0.0f;
        out_features->visual.posture[2] = nf > 5 ? action->features[5] : 0.0f;
        out_features->visual.posture[3] = nf > 6 ? action->features[6] : 0.0f;
    }
    if (nf > 7) {
        out_features->visual.biological_motion = action->features[7];
    }
    out_features->visual.confidence = action->confidence;
    out_features->visual.valid_mask = 0xFFFF;

    /* Motor features (8-15) */
    if (nf > 8) {
        for (size_t i = 0; i < 4 && (8 + i) < nf; i++) {
            out_features->motor.effector[i] = action->features[8 + i];
        }
    }
    if (nf > 12) {
        for (size_t i = 0; i < 4 && (12 + i) < nf; i++) {
            out_features->motor.grip_type[i] = action->features[12 + i];
        }
    }
    out_features->motor.confidence = action->confidence;
    out_features->motor.valid_mask = 0xFFFF;

    /* Auditory features (16-23) */
    if (nf > 16) {
        for (size_t i = 0; i < 4 && (16 + i) < nf; i++) {
            out_features->auditory.action_sound[i] = action->features[16 + i];
        }
    }
    if (nf > 20) {
        out_features->auditory.vocalization[0] = action->features[20];
        out_features->auditory.vocalization[1] = nf > 21 ? action->features[21] : 0.0f;
    }
    if (nf > 22) {
        out_features->auditory.rhythm[0] = action->features[22];
        out_features->auditory.rhythm[1] = nf > 23 ? action->features[23] : 0.0f;
    }
    out_features->auditory.confidence = action->confidence;
    out_features->auditory.valid_mask = 0xFF;

    /* Semantic features (24-31) */
    if (nf > 24) {
        for (size_t i = 0; i < 4 && (24 + i) < nf; i++) {
            out_features->semantic.category[i] = action->features[24 + i];
        }
    }
    if (nf > 28) {
        out_features->semantic.transitivity = action->features[28];
    }
    if (nf > 29) {
        out_features->semantic.goal[0] = action->features[29];
        out_features->semantic.goal[1] = nf > 30 ? action->features[30] : 0.0f;
    }
    if (nf > 31) {
        out_features->semantic.affordance = action->features[31];
    }
    out_features->semantic.confidence = action->confidence;
    out_features->semantic.valid_mask = 0xFF;

    /* Copy metadata */
    out_features->action_id = action->action_id;
    out_features->timestamp = action->timestamp;
    out_features->overall_confidence = action->confidence;

    return true;
}

bool multimodal_to_action(const multimodal_action_features_t* features,
                          action_t* out_action) {
    if (!features || !out_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_to_action: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_to_action", 0.0f);


    memset(out_action, 0, sizeof(action_t));
    out_action->action_id = features->action_id;
    out_action->timestamp = features->timestamp;
    out_action->confidence = features->overall_confidence;
    out_action->num_features = 32;

    /* Use fused if available, otherwise reconstruct from modalities */
    if (features->fused_valid) {
        for (size_t i = 0; i < 32; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && 32 > 256) {
                mirror_multimodal_heartbeat("mirror_multi_loop",
                                 (float)(i + 1) / (float)32);
            }

            out_action->features[i] = features->fused[i];
        }
        return true;
    }

    /* Visual (0-7) */
    out_action->features[0] = features->visual.motion[0];
    out_action->features[1] = features->visual.motion[1];
    out_action->features[2] = features->visual.motion[2];
    out_action->features[3] = features->visual.posture[0];
    out_action->features[4] = features->visual.posture[1];
    out_action->features[5] = features->visual.posture[2];
    out_action->features[6] = features->visual.posture[3];
    out_action->features[7] = features->visual.biological_motion;

    /* Motor (8-15) */
    for (size_t i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            mirror_multimodal_heartbeat("mirror_multi_loop",
                             (float)(i + 1) / (float)4);
        }

        out_action->features[8 + i] = features->motor.effector[i];
        out_action->features[12 + i] = features->motor.grip_type[i];
    }

    /* Auditory (16-23) */
    for (size_t i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            mirror_multimodal_heartbeat("mirror_multi_loop",
                             (float)(i + 1) / (float)4);
        }

        out_action->features[16 + i] = features->auditory.action_sound[i];
    }
    out_action->features[20] = features->auditory.vocalization[0];
    out_action->features[21] = features->auditory.vocalization[1];
    out_action->features[22] = features->auditory.rhythm[0];
    out_action->features[23] = features->auditory.rhythm[1];

    /* Semantic (24-31) */
    for (size_t i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            mirror_multimodal_heartbeat("mirror_multi_loop",
                             (float)(i + 1) / (float)4);
        }

        out_action->features[24 + i] = features->semantic.category[i];
    }
    out_action->features[28] = features->semantic.transitivity;
    out_action->features[29] = features->semantic.goal[0];
    out_action->features[30] = features->semantic.goal[1];
    out_action->features[31] = features->semantic.affordance;

    return true;
}

/* ============================================================================
 * Core API - Feature Setting
 * ============================================================================ */

bool multimodal_set_visual(multimodal_action_features_t* features,
                           const visual_features_t* visual) {
    if (!features || !visual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_set_visual: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_set_visua", 0.0f);


    features->visual = *visual;
    features->fused_valid = false;  /* Invalidate fusion */
    return true;
}

bool multimodal_set_motor(multimodal_action_features_t* features,
                          const motor_features_t* motor) {
    if (!features || !motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_set_motor: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_set_motor", 0.0f);


    features->motor = *motor;
    features->fused_valid = false;
    return true;
}

bool multimodal_set_auditory(multimodal_action_features_t* features,
                             const auditory_features_t* auditory) {
    if (!features || !auditory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_set_auditory: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_set_audit", 0.0f);


    features->auditory = *auditory;
    features->fused_valid = false;
    return true;
}

bool multimodal_set_semantic(multimodal_action_features_t* features,
                             const semantic_features_t* semantic) {
    if (!features || !semantic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_set_semantic: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_set_seman", 0.0f);


    features->semantic = *semantic;
    features->fused_valid = false;
    return true;
}

/* ============================================================================
 * Core API - Feature Fusion
 * ============================================================================ */

bool multimodal_compute_fusion(multimodal_action_features_t* features,
                               const fusion_config_t* config) {
    if (!features) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "multimodal_compute_fusion: features is NULL");

            return false;

        }

    /* Use provided config or defaults */
    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_compute_f", 0.0f);


    fusion_config_t cfg = config ? *config : multimodal_get_default_fusion_config();

    /* Apply weights */
    float vw = cfg.visual_weight;
    float mw = cfg.motor_weight;
    float aw = cfg.auditory_weight;
    float sw = cfg.semantic_weight;

    if (cfg.normalize_weights) {
        normalize_weights(&vw, &mw, &aw, &sw);
    }

    /* Filter by confidence if required */
    if (cfg.require_min_confidence) {
        if (features->visual.confidence < cfg.min_confidence) vw = 0.0f;
        if (features->motor.confidence < cfg.min_confidence) mw = 0.0f;
        if (features->auditory.confidence < cfg.min_confidence) aw = 0.0f;
        if (features->semantic.confidence < cfg.min_confidence) sw = 0.0f;

        /* Re-normalize after filtering */
        normalize_weights(&vw, &mw, &aw, &sw);
    }

    /* Flatten each modality */
    float visual_flat[NIMCP_MULTIMODAL_VISUAL_DIM];
    float motor_flat[NIMCP_MULTIMODAL_MOTOR_DIM];
    float auditory_flat[NIMCP_MULTIMODAL_AUDITORY_DIM];
    float semantic_flat[NIMCP_MULTIMODAL_SEMANTIC_DIM];
    size_t vlen, mlen, alen, slen;

    flatten_visual(&features->visual, visual_flat, &vlen);
    flatten_motor(&features->motor, motor_flat, &mlen);
    flatten_auditory(&features->auditory, auditory_flat, &alen);
    flatten_semantic(&features->semantic, semantic_flat, &slen);

    /* Compute fused vector: concatenate weighted modality features */
    memset(features->fused, 0, sizeof(features->fused));
    size_t idx = 0;

    /* Visual portion (8 dims in fused) */
    for (size_t i = 0; i < 8 && i < vlen && idx < NIMCP_MULTIMODAL_FUSED_DIM; i++, idx++) {
        features->fused[idx] = visual_flat[i] * vw;
    }

    /* Motor portion (8 dims in fused) */
    for (size_t i = 0; i < 8 && i < mlen && idx < NIMCP_MULTIMODAL_FUSED_DIM; i++, idx++) {
        features->fused[idx] = motor_flat[i] * mw;
    }

    /* Auditory portion (8 dims in fused) */
    for (size_t i = 0; i < 8 && i < alen && idx < NIMCP_MULTIMODAL_FUSED_DIM; i++, idx++) {
        features->fused[idx] = auditory_flat[i] * aw;
    }
    /* Pad if needed */
    while (idx < 24) {
        features->fused[idx++] = 0.0f;
    }

    /* Semantic portion (8 dims in fused) */
    for (size_t i = 0; i < 8 && i < slen && idx < NIMCP_MULTIMODAL_FUSED_DIM; i++, idx++) {
        features->fused[idx] = semantic_flat[i] * sw;
    }

    /* Update weights in features struct */
    features->visual_weight = vw;
    features->motor_weight = mw;
    features->auditory_weight = aw;
    features->semantic_weight = sw;

    /* Compute overall confidence */
    features->overall_confidence = vw * features->visual.confidence +
                                   mw * features->motor.confidence +
                                   aw * features->auditory.confidence +
                                   sw * features->semantic.confidence;

    features->fused_valid = true;
    return true;
}

bool multimodal_set_weights(multimodal_action_features_t* features,
                            float visual_weight,
                            float motor_weight,
                            float auditory_weight,
                            float semantic_weight,
                            bool normalize) {
    if (!features) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "multimodal_set_weights: features is NULL");

            return false;

        }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_set_weigh", 0.0f);


    features->visual_weight = nimcp_clampf(visual_weight, 0.0f, 1.0f);
    features->motor_weight = nimcp_clampf(motor_weight, 0.0f, 1.0f);
    features->auditory_weight = nimcp_clampf(auditory_weight, 0.0f, 1.0f);
    features->semantic_weight = nimcp_clampf(semantic_weight, 0.0f, 1.0f);

    if (normalize) {
        normalize_weights(&features->visual_weight, &features->motor_weight,
                          &features->auditory_weight, &features->semantic_weight);
    }

    features->fused_valid = false;  /* Invalidate fusion */
    return true;
}

/* ============================================================================
 * Core API - Feature Comparison
 * ============================================================================ */

float multimodal_compare(const multimodal_action_features_t* features_a,
                         const multimodal_action_features_t* features_b) {
    if (!features_a || !features_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_compare: required parameter is NULL");
        return -1.0f;
    }

    /* Use fused vectors if available for both */
    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_compare", 0.0f);


    if (features_a->fused_valid && features_b->fused_valid) {
        float sim = cosine_similarity(features_a->fused, features_b->fused,
                                      NIMCP_MULTIMODAL_FUSED_DIM);
        return nimcp_clampf((sim + 1.0f) / 2.0f, 0.0f, 1.0f);
    }

    /* Otherwise compute weighted modality similarities */
    float visual_sim = multimodal_compare_visual(features_a, features_b);
    float motor_sim = multimodal_compare_motor(features_a, features_b);
    float auditory_sim = multimodal_compare_auditory(features_a, features_b);
    float semantic_sim = multimodal_compare_semantic(features_a, features_b);

    /* Handle errors */
    if (visual_sim < 0) visual_sim = 0.5f;
    if (motor_sim < 0) motor_sim = 0.5f;
    if (auditory_sim < 0) auditory_sim = 0.5f;
    if (semantic_sim < 0) semantic_sim = 0.5f;

    /* Use average weights from both features */
    float vw = (features_a->visual_weight + features_b->visual_weight) / 2.0f;
    float mw = (features_a->motor_weight + features_b->motor_weight) / 2.0f;
    float aw = (features_a->auditory_weight + features_b->auditory_weight) / 2.0f;
    float sw = (features_a->semantic_weight + features_b->semantic_weight) / 2.0f;

    normalize_weights(&vw, &mw, &aw, &sw);

    return vw * visual_sim + mw * motor_sim + aw * auditory_sim + sw * semantic_sim;
}

float multimodal_compare_visual(const multimodal_action_features_t* features_a,
                                const multimodal_action_features_t* features_b) {
    if (!features_a || !features_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_compare_visual: required parameter is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_compare_v", 0.0f);


    float flat_a[NIMCP_MULTIMODAL_VISUAL_DIM];
    float flat_b[NIMCP_MULTIMODAL_VISUAL_DIM];
    size_t len_a, len_b;

    flatten_visual(&features_a->visual, flat_a, &len_a);
    flatten_visual(&features_b->visual, flat_b, &len_b);

    size_t len = len_a < len_b ? len_a : len_b;
    if (len == 0) return 0.0f;

    float sim = cosine_similarity(flat_a, flat_b, len);
    return nimcp_clampf((sim + 1.0f) / 2.0f, 0.0f, 1.0f);
}

float multimodal_compare_motor(const multimodal_action_features_t* features_a,
                               const multimodal_action_features_t* features_b) {
    if (!features_a || !features_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_compare_motor: required parameter is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_compare_m", 0.0f);


    float flat_a[NIMCP_MULTIMODAL_MOTOR_DIM];
    float flat_b[NIMCP_MULTIMODAL_MOTOR_DIM];
    size_t len_a, len_b;

    flatten_motor(&features_a->motor, flat_a, &len_a);
    flatten_motor(&features_b->motor, flat_b, &len_b);

    size_t len = len_a < len_b ? len_a : len_b;
    if (len == 0) return 0.0f;

    float sim = cosine_similarity(flat_a, flat_b, len);
    return nimcp_clampf((sim + 1.0f) / 2.0f, 0.0f, 1.0f);
}

float multimodal_compare_auditory(const multimodal_action_features_t* features_a,
                                  const multimodal_action_features_t* features_b) {
    if (!features_a || !features_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_compare_auditory: required parameter is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_compare_a", 0.0f);


    float flat_a[NIMCP_MULTIMODAL_AUDITORY_DIM];
    float flat_b[NIMCP_MULTIMODAL_AUDITORY_DIM];
    size_t len_a, len_b;

    flatten_auditory(&features_a->auditory, flat_a, &len_a);
    flatten_auditory(&features_b->auditory, flat_b, &len_b);

    size_t len = len_a < len_b ? len_a : len_b;
    if (len == 0) return 0.0f;

    float sim = cosine_similarity(flat_a, flat_b, len);
    return nimcp_clampf((sim + 1.0f) / 2.0f, 0.0f, 1.0f);
}

float multimodal_compare_semantic(const multimodal_action_features_t* features_a,
                                  const multimodal_action_features_t* features_b) {
    if (!features_a || !features_b) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_compare_s", 0.0f);


    float flat_a[NIMCP_MULTIMODAL_SEMANTIC_DIM];
    float flat_b[NIMCP_MULTIMODAL_SEMANTIC_DIM];
    size_t len_a, len_b;

    flatten_semantic(&features_a->semantic, flat_a, &len_a);
    flatten_semantic(&features_b->semantic, flat_b, &len_b);

    size_t len = len_a < len_b ? len_a : len_b;
    if (len == 0) return 0.0f;

    float sim = cosine_similarity(flat_a, flat_b, len);
    return nimcp_clampf((sim + 1.0f) / 2.0f, 0.0f, 1.0f);
}

bool multimodal_check_coherence(const multimodal_action_features_t* features,
                                float* out_coherence) {
    if (!features) {
        if (out_coherence) *out_coherence = 0.0f;
        return false;
    }

    /* Check cross-modal consistency */
    /* Visual-Motor correlation: movement kinematics should match */
    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_check_coh", 0.0f);


    float visual_flat[NIMCP_MULTIMODAL_VISUAL_DIM];
    float motor_flat[NIMCP_MULTIMODAL_MOTOR_DIM];
    size_t vlen, mlen;

    flatten_visual(&features->visual, visual_flat, &vlen);
    flatten_motor(&features->motor, motor_flat, &mlen);

    /* Compare motion features with motor kinematics */
    float vm_coherence = 0.5f;  /* Default neutral */
    if (vlen >= 3 && mlen >= 4) {
        /* Motion direction should correlate with effector activity */
        float motion_mag = sqrtf(visual_flat[0] * visual_flat[0] +
                                 visual_flat[1] * visual_flat[1] +
                                 visual_flat[2] * visual_flat[2]);
        float effector_act = (motor_flat[0] + motor_flat[1] +
                              motor_flat[2] + motor_flat[3]) / 4.0f;

        /* Both high or both low = coherent */
        float diff = fabsf(motion_mag - effector_act);
        vm_coherence = 1.0f - nimcp_clampf(diff, 0.0f, 1.0f);
    }

    /* Semantic-Action coherence: semantic features should match action category */
    float sem_coherence = features->semantic.confidence;

    /* Weight coherences by confidence */
    float total_weight = features->visual.confidence + features->motor.confidence;
    if (total_weight < EPSILON) total_weight = 1.0f;

    float coherence = (vm_coherence * features->visual.confidence * VISUAL_MOTOR_CORRELATION +
                       sem_coherence * SEMANTIC_CONFIDENCE_BOOST) /
                      (features->visual.confidence * VISUAL_MOTOR_CORRELATION +
                       SEMANTIC_CONFIDENCE_BOOST + EPSILON);

    if (out_coherence) *out_coherence = coherence;

    return coherence >= COHERENCE_THRESHOLD;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* multimodal_visual_type_name(visual_feature_type_t type) {
    switch (type) {
        case VISUAL_FEATURE_MOTION:     return "motion";
        case VISUAL_FEATURE_POSTURE:    return "posture";
        case VISUAL_FEATURE_TRAJECTORY: return "trajectory";
        case VISUAL_FEATURE_BIOLOGICAL: return "biological";
        case VISUAL_FEATURE_OBJECT:     return "object";
        default:                        return "unknown";
    }
}

const char* multimodal_motor_type_name(motor_feature_type_t type) {
    switch (type) {
        case MOTOR_FEATURE_EFFECTOR:   return "effector";
        case MOTOR_FEATURE_GRIP:       return "grip";
        case MOTOR_FEATURE_FORCE:      return "force";
        case MOTOR_FEATURE_KINEMATICS: return "kinematics";
        case MOTOR_FEATURE_TIMING:     return "timing";
        default:                       return "unknown";
    }
}

const char* multimodal_auditory_type_name(auditory_feature_type_t type) {
    switch (type) {
        case AUDITORY_FEATURE_ACTION_SOUND:  return "action_sound";
        case AUDITORY_FEATURE_VOCALIZATION:  return "vocalization";
        case AUDITORY_FEATURE_RHYTHM:        return "rhythm";
        default:                             return "unknown";
    }
}

const char* multimodal_semantic_type_name(semantic_feature_type_t type) {
    switch (type) {
        case SEMANTIC_FEATURE_CATEGORY:     return "category";
        case SEMANTIC_FEATURE_TRANSITIVITY: return "transitivity";
        case SEMANTIC_FEATURE_GOAL:         return "goal";
        case SEMANTIC_FEATURE_AFFORDANCE:   return "affordance";
        default:                            return "unknown";
    }
}

void multimodal_print(const multimodal_action_features_t* features,
                      const char* prefix) {
    if (!features) return;
    /* Phase 8: Heartbeat at operation start */
    mirror_multimodal_heartbeat("mirror_multi_multimodal_print", 0.0f);


    const char* pfx = prefix ? prefix : "";

    nimcp_log(LOG_LEVEL_DEBUG, "%sMultimodal Features (action_id=%u):", pfx, features->action_id);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  Visual: conf=%.2f, motion=[%.2f,%.2f,%.2f], bio=%.2f",
              pfx, features->visual.confidence,
              features->visual.motion[0], features->visual.motion[1], features->visual.motion[2],
              features->visual.biological_motion);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  Motor: conf=%.2f, effector=[%.2f,%.2f,%.2f,%.2f]",
              pfx, features->motor.confidence,
              features->motor.effector[0], features->motor.effector[1],
              features->motor.effector[2], features->motor.effector[3]);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  Auditory: conf=%.2f", pfx, features->auditory.confidence);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  Semantic: conf=%.2f, trans=%.2f", pfx,
              features->semantic.confidence, features->semantic.transitivity);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  Weights: v=%.2f m=%.2f a=%.2f s=%.2f",
              pfx, features->visual_weight, features->motor_weight,
              features->auditory_weight, features->semantic_weight);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  Overall confidence: %.2f, fused=%s",
              pfx, features->overall_confidence, features->fused_valid ? "yes" : "no");
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_multimodal_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_multimodal_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_multimodal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_multimodal_training_begin: NULL argument");
        return -1;
    }
    mirror_multimodal_heartbeat_instance(NULL, "mirror_multimodal_training_begin", 0.0f);
    return 0;
}

int mirror_multimodal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_multimodal_training_end: NULL argument");
        return -1;
    }
    mirror_multimodal_heartbeat_instance(NULL, "mirror_multimodal_training_end", 1.0f);
    return 0;
}

int mirror_multimodal_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_multimodal_training_step: NULL argument");
        return -1;
    }
    mirror_multimodal_heartbeat_instance(NULL, "mirror_multimodal_training_step", progress);
    return 0;
}
