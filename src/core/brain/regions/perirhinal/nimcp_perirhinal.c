/**
 * @file nimcp_perirhinal.c
 * @brief Perirhinal Cortex - Object Recognition and Familiarity Memory
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(perirhinal, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static float compute_cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    return (denom > 1e-8f) ? (dot / denom) : 0.0f;
}

static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

perirhinal_config_t perirhinal_default_config(void) {
    perirhinal_config_t config = {0};

    /* Object cell parameters */
    config.num_object_cells = PERIRHINAL_DEFAULT_OBJECT_CELLS;
    config.object_selectivity = 0.8f;
    config.view_invariance_target = PERIRHINAL_DEFAULT_VIEW_INVARIANCE;

    /* Familiarity cell parameters */
    config.num_familiarity_cells = PERIRHINAL_DEFAULT_FAMILIARITY_CELLS;
    config.familiarity_threshold = PERIRHINAL_FAMILIARITY_THRESHOLD;
    config.repetition_suppression_rate = 0.1f;

    /* Novelty detector parameters */
    config.num_novelty_cells = PERIRHINAL_DEFAULT_NOVELTY_CELLS;
    config.novelty_threshold = PERIRHINAL_NOVELTY_THRESHOLD;
    config.habituation_rate = 0.05f;

    /* Recency cell parameters */
    config.num_recency_cells = PERIRHINAL_DEFAULT_RECENCY_CELLS;
    config.recency_decay_rate = PERIRHINAL_RECENCY_DECAY_RATE;
    config.max_recency_time_s = 3600.0f;  /* 1 hour */

    /* Memory parameters */
    config.max_stored_objects = PERIRHINAL_DEFAULT_MAX_STORED_OBJECTS;
    config.feature_dim = PERIRHINAL_DEFAULT_FEATURE_DIM;
    config.identity_dim = PERIRHINAL_DEFAULT_OBJECT_DIM;
    config.max_views_per_object = 8;

    /* Recognition parameters */
    config.recognition_threshold = 0.6f;
    config.max_alternatives = 5;
    config.view_matching_weight = 0.4f;
    config.feature_matching_weight = 0.6f;

    /* Enable all integrations by default */
    config.enable_entorhinal = true;
    config.enable_security = true;
    config.enable_immune = true;
    config.enable_bio_async = true;
    config.enable_snn = true;
    config.enable_plasticity = true;
    config.enable_stdp = true;
    config.enable_cognitive = true;
    config.enable_training = true;
    config.enable_substrate = true;
    config.enable_resonance = true;
    config.enable_thalamic = true;
    config.enable_hippocampus = true;
    config.enable_perception = true;
    config.enable_swarm = true;
    config.enable_dragonfly = true;
    config.enable_portia = true;
    config.enable_cerebellum = true;
    config.enable_medulla = true;
    config.enable_omni = true;
    config.enable_hypothalamus = true;
    config.enable_logic = true;
    config.enable_kg = true;

    /* Processing options */
    config.enable_view_invariance = true;
    config.enable_semantic_associations = true;
    config.enable_context_binding = true;
    config.enable_recency_tracking = true;

    /* Learning parameters */
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
    config.eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;

    /* Oscillation parameters */
    config.theta_frequency = 8.0f;
    config.gamma_frequency = 40.0f;
    config.phase_coupling_strength = 0.5f;

    return config;
}

nimcp_perirhinal_t* perirhinal_create(const perirhinal_config_t* config) {
    nimcp_perirhinal_t* pr = (nimcp_perirhinal_t*)nimcp_calloc(1, sizeof(nimcp_perirhinal_t));
    if (!pr) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        pr->config = *config;
    } else {
        pr->config = perirhinal_default_config();
    }

    /* Allocate object cells */
    pr->num_object_cells = pr->config.num_object_cells;
    pr->object_cells = (nimcp_object_cell_t*)nimcp_calloc(
        pr->num_object_cells, sizeof(nimcp_object_cell_t));
    if (!pr->object_cells) goto error;

    /* Initialize object cells */
    for (uint32_t i = 0; i < pr->num_object_cells; i++) {
        pr->object_cells[i].cell_id = i;
        pr->object_cells[i].selectivity = pr->config.object_selectivity;
        pr->object_cells[i].view_invariance = pr->config.view_invariance_target;
        pr->object_cells[i].feature_dim = pr->config.feature_dim;
        pr->object_cells[i].feature_weights = (float*)nimcp_calloc(
            pr->config.feature_dim, sizeof(float));
        if (!pr->object_cells[i].feature_weights) goto error;

        /* Random initialization of weights */
        for (uint32_t j = 0; j < pr->config.feature_dim; j++) {
            pr->object_cells[i].feature_weights[j] =
                ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        pr->object_cells[i].learning_rate = pr->config.learning_rate;
    }

    /* Allocate familiarity cells */
    pr->num_familiarity_cells = pr->config.num_familiarity_cells;
    pr->familiarity_cells = (nimcp_familiarity_cell_t*)nimcp_calloc(
        pr->num_familiarity_cells, sizeof(nimcp_familiarity_cell_t));
    if (!pr->familiarity_cells) goto error;

    /* Initialize familiarity cells */
    for (uint32_t i = 0; i < pr->num_familiarity_cells; i++) {
        pr->familiarity_cells[i].cell_id = i;
        pr->familiarity_cells[i].familiarity_threshold = pr->config.familiarity_threshold;
        pr->familiarity_cells[i].adaptation_rate = pr->config.repetition_suppression_rate;
        pr->familiarity_cells[i].baseline_response = 1.0f;
        pr->familiarity_cells[i].trace_size = pr->config.max_stored_objects;
        pr->familiarity_cells[i].trace_decay = NIMCP_EMA_DECAY_DEFAULT;
        pr->familiarity_cells[i].memory_trace = (float*)nimcp_calloc(
            pr->config.max_stored_objects, sizeof(float));
        if (!pr->familiarity_cells[i].memory_trace) goto error;
    }

    /* Allocate novelty detectors */
    pr->num_novelty_cells = pr->config.num_novelty_cells;
    pr->novelty_cells = (nimcp_novelty_detector_t*)nimcp_calloc(
        pr->num_novelty_cells, sizeof(nimcp_novelty_detector_t));
    if (!pr->novelty_cells) goto error;

    /* Initialize novelty detectors */
    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        pr->novelty_cells[i].cell_id = i;
        pr->novelty_cells[i].novelty_threshold = pr->config.novelty_threshold;
        pr->novelty_cells[i].habituation_rate = pr->config.habituation_rate;
        pr->novelty_cells[i].recovery_rate = 0.01f;
        pr->novelty_cells[i].feature_dim = pr->config.feature_dim;
        pr->novelty_cells[i].expected_features = (float*)nimcp_calloc(
            pr->config.feature_dim, sizeof(float));
        pr->novelty_cells[i].feature_variance = (float*)nimcp_calloc(
            pr->config.feature_dim, sizeof(float));
        if (!pr->novelty_cells[i].expected_features ||
            !pr->novelty_cells[i].feature_variance) goto error;

        /* Initialize variance to 1.0 */
        for (uint32_t j = 0; j < pr->config.feature_dim; j++) {
            pr->novelty_cells[i].feature_variance[j] = 1.0f;
        }
    }

    /* Allocate recency cells */
    pr->num_recency_cells = pr->config.num_recency_cells;
    pr->recency_cells = (nimcp_recency_cell_t*)nimcp_calloc(
        pr->num_recency_cells, sizeof(nimcp_recency_cell_t));
    if (!pr->recency_cells) goto error;

    /* Initialize recency cells */
    for (uint32_t i = 0; i < pr->num_recency_cells; i++) {
        pr->recency_cells[i].cell_id = i;
        pr->recency_cells[i].decay_rate = pr->config.recency_decay_rate;
        pr->recency_cells[i].time_constant = pr->config.max_recency_time_s / pr->num_recency_cells;
        /* Preferred recency spans the range */
        pr->recency_cells[i].preferred_recency =
            (float)i * pr->config.max_recency_time_s / pr->num_recency_cells;
        pr->recency_cells[i].tuning_width =
            pr->config.max_recency_time_s / (2.0f * pr->num_recency_cells);
    }

    /* Allocate stored objects */
    pr->max_stored_objects = pr->config.max_stored_objects;
    pr->stored_objects = (nimcp_stored_object_t*)nimcp_calloc(
        pr->max_stored_objects, sizeof(nimcp_stored_object_t));
    if (!pr->stored_objects) goto error;

    /* Initialize stored objects */
    for (uint32_t i = 0; i < pr->max_stored_objects; i++) {
        pr->stored_objects[i].object_id = UINT32_MAX;  /* Invalid/empty */
    }
    pr->num_stored_objects = 0;

    /* Initialize processing state */
    pr->current_input_dim = pr->config.feature_dim;
    pr->current_visual_input = (float*)nimcp_calloc(pr->current_input_dim, sizeof(float));
    if (!pr->current_visual_input) goto error;

    /* Initialize timing */
    pr->creation_time_ms = get_current_time_ms();
    pr->last_update_ms = pr->creation_time_ms;
    pr->simulation_dt_ms = 1.0f;

    /* Set initial status */
    pr->status = PERIRHINAL_STATUS_READY;
    pr->last_error = PERIRHINAL_ERROR_NONE;

    return pr;

error:
    perirhinal_destroy(pr);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_create: operation failed");
    return NULL;
}

void perirhinal_destroy(nimcp_perirhinal_t* pr) {
    if (!pr) return;

    /* Free object cells */
    if (pr->object_cells) {
        for (uint32_t i = 0; i < pr->num_object_cells; i++) {
            nimcp_free(pr->object_cells[i].feature_weights);
        }
        nimcp_free(pr->object_cells);
    }

    /* Free familiarity cells */
    if (pr->familiarity_cells) {
        for (uint32_t i = 0; i < pr->num_familiarity_cells; i++) {
            nimcp_free(pr->familiarity_cells[i].memory_trace);
        }
        nimcp_free(pr->familiarity_cells);
    }

    /* Free novelty detectors */
    if (pr->novelty_cells) {
        for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
            nimcp_free(pr->novelty_cells[i].expected_features);
            nimcp_free(pr->novelty_cells[i].feature_variance);
        }
        nimcp_free(pr->novelty_cells);
    }

    /* Free recency cells */
    nimcp_free(pr->recency_cells);

    /* Free stored objects */
    if (pr->stored_objects) {
        for (uint32_t i = 0; i < pr->max_stored_objects; i++) {
            if (pr->stored_objects[i].object_id != UINT32_MAX) {
                nimcp_free(pr->stored_objects[i].visual_features);
                nimcp_free(pr->stored_objects[i].identity_vector);
                nimcp_free(pr->stored_objects[i].spatial_context);
                nimcp_free(pr->stored_objects[i].associated_objects);
                nimcp_free(pr->stored_objects[i].association_strengths);
                if (pr->stored_objects[i].view_vectors) {
                    for (uint32_t v = 0; v < pr->stored_objects[i].num_views; v++) {
                        nimcp_free(pr->stored_objects[i].view_vectors[v]);
                    }
                    nimcp_free(pr->stored_objects[i].view_vectors);
                }
            }
        }
        nimcp_free(pr->stored_objects);
    }

    nimcp_free(pr->current_visual_input);
    nimcp_free(pr);
}

int perirhinal_reset(nimcp_perirhinal_t* pr) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_reset: pr is NULL");
        return -1;
    }

    /* Reset statistics */
    pr->updates_processed = 0;
    pr->objects_encoded = 0;
    pr->objects_recognized = 0;
    pr->novelty_detections = 0;
    pr->mean_recognition_confidence = 0.0f;
    pr->mean_familiarity_signal = 0.0f;
    pr->total_processing_time_ms = 0.0;

    /* Reset cell activations */
    for (uint32_t i = 0; i < pr->num_object_cells; i++) {
        pr->object_cells[i].activation = 0.0f;
        pr->object_cells[i].eligibility_trace = 0.0f;
    }

    for (uint32_t i = 0; i < pr->num_familiarity_cells; i++) {
        pr->familiarity_cells[i].activation = 0.0f;
        pr->familiarity_cells[i].repetition_suppression = 0.0f;
        memset(pr->familiarity_cells[i].memory_trace, 0,
               pr->config.max_stored_objects * sizeof(float));
    }

    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        pr->novelty_cells[i].activation = 0.0f;
        pr->novelty_cells[i].prediction_error = 0.0f;
        pr->novelty_cells[i].habituation_state = 0.0f;
    }

    for (uint32_t i = 0; i < pr->num_recency_cells; i++) {
        pr->recency_cells[i].activation = 0.0f;
    }

    /* Reset processing state */
    pr->current_familiarity = 0.0f;
    pr->current_novelty = 0.0f;
    pr->current_recency = 0.0f;

    pr->status = PERIRHINAL_STATUS_READY;
    pr->last_error = PERIRHINAL_ERROR_NONE;

    return 0;
}

int perirhinal_update(nimcp_perirhinal_t* pr, float dt) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_update: pr is NULL");
        return -1;
    }

    uint64_t start_time = get_current_time_ms();

    /* Decay recency signals */
    perirhinal_decay_recency(pr, dt);

    /* Update novelty detectors (recovery from habituation) */
    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        pr->novelty_cells[i].habituation_state *=
            (1.0f - pr->novelty_cells[i].recovery_rate * dt);
    }

    /* Decay familiarity memory traces */
    for (uint32_t i = 0; i < pr->num_familiarity_cells; i++) {
        for (uint32_t j = 0; j < pr->familiarity_cells[i].trace_size; j++) {
            pr->familiarity_cells[i].memory_trace[j] *=
                pr->familiarity_cells[i].trace_decay;
        }
    }

    /* Decay eligibility traces */
    for (uint32_t i = 0; i < pr->num_object_cells; i++) {
        pr->object_cells[i].eligibility_trace *= pr->config.eligibility_decay;
    }

    pr->updates_processed++;
    pr->last_update_ms = get_current_time_ms();
    pr->total_processing_time_ms += (double)(pr->last_update_ms - start_time);
    pr->simulation_dt_ms = dt * 1000.0f;

    return 0;
}

/*=============================================================================
 * OBJECT RECOGNITION API
 *===========================================================================*/

int perirhinal_encode_object(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim,
    const char* name, uint32_t* object_id_out) {

    if (!pr || !visual_features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_update: required parameter is NULL (pr, visual_features)");
        return -1;
    }

    /* Find empty slot */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < pr->max_stored_objects; i++) {
        if (pr->stored_objects[i].object_id == UINT32_MAX) {
            slot = i;
            break;
        }
    }

    if (slot == UINT32_MAX) {
        pr->last_error = PERIRHINAL_ERROR_MEMORY_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_update: validation failed");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[slot];
    obj->object_id = slot;

    /* Copy name */
    if (name) {
        strncpy(obj->name, name, sizeof(obj->name) - 1);
        obj->name[sizeof(obj->name) - 1] = '\0';
    }

    /* Allocate and copy visual features */
    obj->feature_dim = feature_dim;
    obj->visual_features = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!obj->visual_features) {
        obj->object_id = UINT32_MAX;
        pr->last_error = PERIRHINAL_ERROR_ENCODING_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perirhinal_update: obj->visual_features is NULL");
        return -1;
    }
    memcpy(obj->visual_features, visual_features, feature_dim * sizeof(float));

    /* Create identity vector */
    obj->identity_dim = pr->config.identity_dim;
    obj->identity_vector = (float*)nimcp_calloc(obj->identity_dim, sizeof(float));
    if (!obj->identity_vector) {
        nimcp_free(obj->visual_features);
        obj->object_id = UINT32_MAX;
        pr->last_error = PERIRHINAL_ERROR_ENCODING_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perirhinal_update: obj->identity_vector is NULL");
        return -1;
    }

    /* Simple identity encoding from visual features */
    for (uint32_t i = 0; i < obj->identity_dim && i < feature_dim; i++) {
        obj->identity_vector[i] = visual_features[i];
    }

    /* Initialize first view */
    obj->num_views = 1;
    obj->view_vectors = (float**)nimcp_malloc(sizeof(float*));
    if (obj->view_vectors) {
        obj->view_vectors[0] = (float*)nimcp_malloc(feature_dim * sizeof(float));
        if (obj->view_vectors[0]) {
            memcpy(obj->view_vectors[0], visual_features, feature_dim * sizeof(float));
        }
    }

    /* Initialize familiarity */
    obj->familiarity_strength = 0.1f;  /* Newly encoded = low familiarity */
    obj->recency_signal = 1.0f;        /* Just seen */
    obj->last_seen_ms = get_current_time_ms();
    obj->encounter_count = 1;

    /* Initialize associations */
    obj->associated_objects = NULL;
    obj->association_strengths = NULL;
    obj->num_associations = 0;

    /* Initialize context */
    obj->spatial_context = NULL;
    obj->spatial_dim = 0;
    obj->context_binding_strength = 0.0f;

    pr->num_stored_objects++;
    pr->objects_encoded++;

    if (object_id_out) {
        *object_id_out = obj->object_id;
    }

    pr->status = PERIRHINAL_STATUS_ENCODING;
    return 0;
}

int perirhinal_recognize_object(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim,
    perirhinal_recognition_result_t* result) {

    if (!pr || !visual_features || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_update: required parameter is NULL (pr, visual_features, result)");
        return -1;
    }

    pr->status = PERIRHINAL_STATUS_RECOGNIZING;
    memset(result, 0, sizeof(perirhinal_recognition_result_t));

    float best_match = 0.0f;
    uint32_t best_id = UINT32_MAX;

    /* Search through stored objects */
    for (uint32_t i = 0; i < pr->max_stored_objects; i++) {
        if (pr->stored_objects[i].object_id == UINT32_MAX) continue;

        nimcp_stored_object_t* obj = &pr->stored_objects[i];
        float similarity = 0.0f;

        /* Compare with visual features */
        if (obj->visual_features && obj->feature_dim == feature_dim) {
            similarity = compute_cosine_similarity(
                visual_features, obj->visual_features, feature_dim);
        }

        /* Also check views if view invariance is enabled */
        if (pr->config.enable_view_invariance && obj->view_vectors) {
            for (uint32_t v = 0; v < obj->num_views; v++) {
                if (obj->view_vectors[v]) {
                    float view_sim = compute_cosine_similarity(
                        visual_features, obj->view_vectors[v], feature_dim);
                    if (view_sim > similarity) {
                        similarity = view_sim;
                    }
                }
            }
        }

        if (similarity > best_match) {
            best_match = similarity;
            best_id = obj->object_id;
        }
    }

    /* Fill result */
    result->object_id = best_id;
    result->match_confidence = best_match;

    /* Classify confidence level */
    if (best_match < 0.3f) {
        result->confidence_level = RECOGNITION_CONFIDENCE_NONE;
    } else if (best_match < 0.5f) {
        result->confidence_level = RECOGNITION_CONFIDENCE_LOW;
    } else if (best_match < 0.7f) {
        result->confidence_level = RECOGNITION_CONFIDENCE_MEDIUM;
    } else if (best_match < 0.9f) {
        result->confidence_level = RECOGNITION_CONFIDENCE_HIGH;
    } else {
        result->confidence_level = RECOGNITION_CONFIDENCE_CERTAIN;
    }

    /* Get familiarity and novelty */
    if (best_id != UINT32_MAX) {
        nimcp_stored_object_t* obj = &pr->stored_objects[best_id];
        result->familiarity_strength = obj->familiarity_strength;
        result->recency_signal = obj->recency_signal;
        result->last_seen_ms = obj->last_seen_ms;
        result->encounter_count = obj->encounter_count;
        result->familiarity_type = perirhinal_classify_familiarity(pr, obj->familiarity_strength);
    }

    /* Compute novelty */
    result->novelty_signal = perirhinal_compute_novelty(pr, visual_features, feature_dim);
    result->is_novel = (result->novelty_signal > pr->config.novelty_threshold);

    pr->objects_recognized++;
    pr->mean_recognition_confidence =
        (pr->mean_recognition_confidence * (pr->objects_recognized - 1) + best_match)
        / pr->objects_recognized;

    pr->status = PERIRHINAL_STATUS_READY;
    return 0;
}

int perirhinal_add_object_view(nimcp_perirhinal_t* pr,
    uint32_t object_id, const float* view_features, uint32_t feature_dim) {

    if (!pr || !view_features || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (pr, view_features)");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX) {
        pr->last_error = PERIRHINAL_ERROR_OBJECT_NOT_FOUND;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    if (obj->num_views >= pr->config.max_views_per_object) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "unknown: capacity exceeded");
        return -1;  /* Max views reached */
    }

    /* Reallocate view array */
    float** new_views = (float**)nimcp_realloc(obj->view_vectors,
        (obj->num_views + 1) * sizeof(float*));
    if (!new_views) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: new_views is NULL");
        return -1;
    }

    obj->view_vectors = new_views;
    obj->view_vectors[obj->num_views] = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!obj->view_vectors[obj->num_views]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: obj->view_vectors is NULL");
        return -1;
    }

    memcpy(obj->view_vectors[obj->num_views], view_features, feature_dim * sizeof(float));
    obj->num_views++;

    return 0;
}

const nimcp_stored_object_t* perirhinal_get_object(const nimcp_perirhinal_t* pr,
    uint32_t object_id) {

    if (!pr || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "unknown: pr is NULL");
        return NULL;
    }
    if (pr->stored_objects[object_id].object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }
    return &pr->stored_objects[object_id];
}

int perirhinal_update_familiarity(nimcp_perirhinal_t* pr, uint32_t object_id) {
    if (!pr || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "perirhinal_update_familiarity: pr is NULL");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_update_familiarity: validation failed");
        return -1;
    }

    /* Increase familiarity with diminishing returns */
    obj->familiarity_strength += (1.0f - obj->familiarity_strength) * 0.1f;
    obj->encounter_count++;
    obj->last_seen_ms = get_current_time_ms();
    obj->recency_signal = 1.0f;

    return 0;
}

int perirhinal_forget_object(nimcp_perirhinal_t* pr, uint32_t object_id) {
    if (!pr || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "perirhinal_forget_object: pr is NULL");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_forget_object: validation failed");
        return -1;
    }

    /* Free allocated memory */
    nimcp_free(obj->visual_features);
    nimcp_free(obj->identity_vector);
    nimcp_free(obj->spatial_context);
    nimcp_free(obj->associated_objects);
    nimcp_free(obj->association_strengths);

    if (obj->view_vectors) {
        for (uint32_t v = 0; v < obj->num_views; v++) {
            nimcp_free(obj->view_vectors[v]);
        }
        nimcp_free(obj->view_vectors);
    }

    /* Mark as empty */
    memset(obj, 0, sizeof(nimcp_stored_object_t));
    obj->object_id = UINT32_MAX;

    pr->num_stored_objects--;
    return 0;
}

/*=============================================================================
 * FAMILIARITY API
 *===========================================================================*/

float perirhinal_compute_familiarity(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim) {

    if (!pr || !visual_features) return 0.0f;

    pr->status = PERIRHINAL_STATUS_FAMILIARITY_COMPUTING;

    float max_familiarity = 0.0f;

    /* Compare with all stored objects */
    for (uint32_t i = 0; i < pr->max_stored_objects; i++) {
        if (pr->stored_objects[i].object_id == UINT32_MAX) continue;

        nimcp_stored_object_t* obj = &pr->stored_objects[i];

        if (obj->visual_features && obj->feature_dim == feature_dim) {
            float similarity = compute_cosine_similarity(
                visual_features, obj->visual_features, feature_dim);

            /* Familiarity is combination of match and prior familiarity */
            float familiarity = similarity * obj->familiarity_strength;
            if (familiarity > max_familiarity) {
                max_familiarity = familiarity;
            }
        }
    }

    /* Update familiarity cells */
    for (uint32_t i = 0; i < pr->num_familiarity_cells; i++) {
        pr->familiarity_cells[i].activation = sigmoid(
            (max_familiarity - pr->familiarity_cells[i].familiarity_threshold) * 5.0f);
    }

    pr->current_familiarity = max_familiarity;
    pr->mean_familiarity_signal =
        (pr->mean_familiarity_signal * 0.99f) + (max_familiarity * 0.01f);

    pr->status = PERIRHINAL_STATUS_READY;
    return max_familiarity;
}

familiarity_type_t perirhinal_classify_familiarity(const nimcp_perirhinal_t* pr,
    float familiarity_signal) {

    (void)pr;  /* May use config in future */

    if (familiarity_signal < 0.2f) return FAMILIARITY_TYPE_NOVEL;
    if (familiarity_signal < 0.4f) return FAMILIARITY_TYPE_SEEN_BEFORE;
    if (familiarity_signal < 0.6f) return FAMILIARITY_TYPE_FAMILIAR;
    if (familiarity_signal < 0.8f) return FAMILIARITY_TYPE_VERY_FAMILIAR;
    return FAMILIARITY_TYPE_KNOWN;
}

bool perirhinal_is_familiar(const nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim) {

    if (!pr || !visual_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_forget_object: required parameter is NULL (pr, visual_features)");
        return false;
    }

    /* Cast away const for internal computation */
    float familiarity = perirhinal_compute_familiarity(
        (nimcp_perirhinal_t*)pr, visual_features, feature_dim);
    return familiarity > pr->config.familiarity_threshold;
}

float perirhinal_get_object_familiarity(const nimcp_perirhinal_t* pr,
    uint32_t object_id) {

    if (!pr || object_id >= pr->max_stored_objects) return 0.0f;
    if (pr->stored_objects[object_id].object_id == UINT32_MAX) return 0.0f;
    return pr->stored_objects[object_id].familiarity_strength;
}

/*=============================================================================
 * NOVELTY API
 *===========================================================================*/

float perirhinal_compute_novelty(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim) {

    if (!pr || !visual_features) return 1.0f;

    pr->status = PERIRHINAL_STATUS_NOVELTY_DETECTING;

    /* Novelty = 1 - familiarity (inverse relationship) */
    float familiarity = perirhinal_compute_familiarity(pr, visual_features, feature_dim);
    float novelty = 1.0f - familiarity;

    /* Also compute prediction error */
    float prediction_error = 0.0f;
    for (uint32_t i = 0; i < pr->num_novelty_cells && i < feature_dim; i++) {
        float expected = pr->novelty_cells[i].expected_features[
            i % pr->config.feature_dim];
        float variance = pr->novelty_cells[i].feature_variance[
            i % pr->config.feature_dim];
        float diff = visual_features[i] - expected;
        prediction_error += (diff * diff) / (variance + 1e-6f);
    }
    prediction_error = sqrtf(prediction_error / fminf(pr->num_novelty_cells, feature_dim));

    /* Update novelty cells */
    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        /* Account for habituation */
        float effective_novelty = novelty * (1.0f - pr->novelty_cells[i].habituation_state);
        pr->novelty_cells[i].activation = effective_novelty;
        pr->novelty_cells[i].prediction_error = prediction_error;
        pr->novelty_cells[i].surprise_signal = effective_novelty * prediction_error;
    }

    pr->current_novelty = novelty;

    if (novelty > pr->config.novelty_threshold) {
        pr->novelty_detections++;
    }

    pr->status = PERIRHINAL_STATUS_READY;
    return novelty;
}

bool perirhinal_is_novel(const nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim) {

    if (!pr || !visual_features) return true;

    float novelty = perirhinal_compute_novelty(
        (nimcp_perirhinal_t*)pr, visual_features, feature_dim);
    return novelty > pr->config.novelty_threshold;
}

float perirhinal_get_surprise_signal(const nimcp_perirhinal_t* pr) {
    if (!pr || pr->num_novelty_cells == 0) return 0.0f;

    float total_surprise = 0.0f;
    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        total_surprise += pr->novelty_cells[i].surprise_signal;
    }
    return total_surprise / pr->num_novelty_cells;
}

int perirhinal_habituate(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim) {

    if (!pr || !visual_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_get_surprise_signal: required parameter is NULL (pr, visual_features)");
        return -1;
    }

    /* Update expected features (running average) */
    float alpha = pr->config.habituation_rate;
    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        for (uint32_t j = 0; j < feature_dim && j < pr->config.feature_dim; j++) {
            pr->novelty_cells[i].expected_features[j] =
                (1.0f - alpha) * pr->novelty_cells[i].expected_features[j] +
                alpha * visual_features[j];
        }
        /* Increase habituation state */
        pr->novelty_cells[i].habituation_state =
            fminf(1.0f, pr->novelty_cells[i].habituation_state +
                  pr->novelty_cells[i].habituation_rate);
    }

    return 0;
}

/*=============================================================================
 * RECENCY API
 *===========================================================================*/

int perirhinal_update_recency(nimcp_perirhinal_t* pr, uint32_t object_id) {
    if (!pr || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "perirhinal_update_recency: pr is NULL");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_update_recency: validation failed");
        return -1;
    }

    obj->last_seen_ms = get_current_time_ms();
    obj->recency_signal = 1.0f;

    return 0;
}

float perirhinal_get_recency_signal(const nimcp_perirhinal_t* pr,
    uint32_t object_id) {

    if (!pr || object_id >= pr->max_stored_objects) return 0.0f;
    if (pr->stored_objects[object_id].object_id == UINT32_MAX) return 0.0f;
    return pr->stored_objects[object_id].recency_signal;
}

uint64_t perirhinal_get_time_since_encounter(const nimcp_perirhinal_t* pr,
    uint32_t object_id) {

    if (!pr || object_id >= pr->max_stored_objects) return UINT64_MAX;
    if (pr->stored_objects[object_id].object_id == UINT32_MAX) return UINT64_MAX;

    uint64_t now = get_current_time_ms();
    return now - pr->stored_objects[object_id].last_seen_ms;
}

int perirhinal_decay_recency(nimcp_perirhinal_t* pr, float dt) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_decay_recency: pr is NULL");
        return -1;
    }

    pr->status = PERIRHINAL_STATUS_RECENCY_UPDATING;

    float decay_factor = expf(-pr->config.recency_decay_rate * dt);

    for (uint32_t i = 0; i < pr->max_stored_objects; i++) {
        if (pr->stored_objects[i].object_id != UINT32_MAX) {
            pr->stored_objects[i].recency_signal *= decay_factor;
        }
    }

    /* Update recency cells */
    for (uint32_t i = 0; i < pr->num_recency_cells; i++) {
        pr->recency_cells[i].activation *= decay_factor;
    }

    pr->status = PERIRHINAL_STATUS_READY;
    return 0;
}

/*=============================================================================
 * SEMANTIC ASSOCIATION API
 *===========================================================================*/

int perirhinal_create_association(nimcp_perirhinal_t* pr,
    uint32_t object_id_a, uint32_t object_id_b, float strength) {

    if (!pr || object_id_a >= pr->max_stored_objects ||
        object_id_b >= pr->max_stored_objects) return -1;

    nimcp_stored_object_t* obj_a = &pr->stored_objects[object_id_a];
    nimcp_stored_object_t* obj_b = &pr->stored_objects[object_id_b];

    if (obj_a->object_id == UINT32_MAX || obj_b->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_decay_recency: validation failed");
        return -1;
    }

    /* Check if association already exists */
    for (uint32_t i = 0; i < obj_a->num_associations; i++) {
        if (obj_a->associated_objects[i] == object_id_b) {
            obj_a->association_strengths[i] = strength;
            return 0;
        }
    }

    /* Add new association */
    uint32_t* new_ids = (uint32_t*)nimcp_realloc(obj_a->associated_objects,
        (obj_a->num_associations + 1) * sizeof(uint32_t));
    float* new_strengths = (float*)nimcp_realloc(obj_a->association_strengths,
        (obj_a->num_associations + 1) * sizeof(float));

    if (!new_ids || !new_strengths) {
        nimcp_free(new_ids);
        nimcp_free(new_strengths);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_decay_recency: required parameter is NULL (new_ids, new_strengths)");
        return -1;
    }

    obj_a->associated_objects = new_ids;
    obj_a->association_strengths = new_strengths;
    obj_a->associated_objects[obj_a->num_associations] = object_id_b;
    obj_a->association_strengths[obj_a->num_associations] = strength;
    obj_a->num_associations++;

    return 0;
}

int perirhinal_get_associations(const nimcp_perirhinal_t* pr,
    uint32_t object_id, uint32_t* associated_ids, float* strengths,
    uint32_t max_associations, uint32_t* num_found) {

    if (!pr || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "perirhinal_decay_recency: pr is NULL");
        return -1;
    }

    const nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_decay_recency: validation failed");
        return -1;
    }

    uint32_t count = (obj->num_associations < max_associations) ?
                     obj->num_associations : max_associations;

    if (associated_ids && obj->associated_objects) {
        memcpy(associated_ids, obj->associated_objects, count * sizeof(uint32_t));
    }
    if (strengths && obj->association_strengths) {
        memcpy(strengths, obj->association_strengths, count * sizeof(float));
    }
    if (num_found) {
        *num_found = count;
    }

    return 0;
}

int perirhinal_strengthen_association(nimcp_perirhinal_t* pr,
    uint32_t object_id_a, uint32_t object_id_b, float delta) {

    if (!pr || object_id_a >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "perirhinal_decay_recency: pr is NULL");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[object_id_a];
    if (obj->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_decay_recency: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < obj->num_associations; i++) {
        if (obj->associated_objects[i] == object_id_b) {
            obj->association_strengths[i] =
                fminf(1.0f, fmaxf(0.0f, obj->association_strengths[i] + delta));
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_decay_recency: validation failed");
    return -1;  /* Association not found */
}

/*=============================================================================
 * CONTEXT BINDING API
 *===========================================================================*/

int perirhinal_bind_spatial_context(nimcp_perirhinal_t* pr,
    uint32_t object_id, const float* spatial_context, uint32_t spatial_dim) {

    if (!pr || !spatial_context || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_decay_recency: required parameter is NULL (pr, spatial_context)");
        return -1;
    }

    nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_decay_recency: validation failed");
        return -1;
    }

    /* Free old context if exists */
    nimcp_free(obj->spatial_context);

    obj->spatial_dim = spatial_dim;
    obj->spatial_context = (float*)nimcp_malloc(spatial_dim * sizeof(float));
    if (!obj->spatial_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perirhinal_decay_recency: obj->spatial_context is NULL");
        return -1;
    }

    memcpy(obj->spatial_context, spatial_context, spatial_dim * sizeof(float));
    obj->context_binding_strength = 1.0f;

    return 0;
}

int perirhinal_get_spatial_context(const nimcp_perirhinal_t* pr,
    uint32_t object_id, float* context_out, uint32_t max_dim) {

    if (!pr || !context_out || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_decay_recency: required parameter is NULL (pr, context_out)");
        return -1;
    }

    const nimcp_stored_object_t* obj = &pr->stored_objects[object_id];
    if (obj->object_id == UINT32_MAX || !obj->spatial_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_decay_recency: obj->spatial_context is NULL");
        return -1;
    }

    uint32_t copy_dim = (obj->spatial_dim < max_dim) ? obj->spatial_dim : max_dim;
    memcpy(context_out, obj->spatial_context, copy_dim * sizeof(float));

    return (int)copy_dim;
}

int perirhinal_find_by_context(const nimcp_perirhinal_t* pr,
    const float* spatial_context, uint32_t spatial_dim,
    uint32_t* object_ids, float* match_strengths,
    uint32_t max_results, uint32_t* num_found) {

    if (!pr || !spatial_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_decay_recency: required parameter is NULL (pr, spatial_context)");
        return -1;
    }

    uint32_t found = 0;

    for (uint32_t i = 0; i < pr->max_stored_objects && found < max_results; i++) {
        const nimcp_stored_object_t* obj = &pr->stored_objects[i];
        if (obj->object_id == UINT32_MAX || !obj->spatial_context) continue;

        uint32_t min_dim = (obj->spatial_dim < spatial_dim) ? obj->spatial_dim : spatial_dim;
        float similarity = compute_cosine_similarity(
            spatial_context, obj->spatial_context, min_dim);

        if (similarity > 0.3f) {  /* Threshold for context match */
            if (object_ids) object_ids[found] = obj->object_id;
            if (match_strengths) match_strengths[found] = similarity;
            found++;
        }
    }

    if (num_found) *num_found = found;
    return 0;
}

/*=============================================================================
 * ENTORHINAL INTEGRATION API
 *===========================================================================*/

int perirhinal_send_to_entorhinal(nimcp_perirhinal_t* pr, uint32_t object_id) {
    if (!pr || object_id >= pr->max_stored_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "perirhinal_send_to_entorhinal: pr is NULL");
        return -1;
    }
    if (!pr->entorhinal_bridge.entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_send_to_entorhinal: pr->entorhinal_bridge is NULL");
        return -1;
    }

    /* In real implementation, would send object representation */
    pr->entorhinal_bridge.items_transferred++;
    return 0;
}

int perirhinal_receive_from_entorhinal(nimcp_perirhinal_t* pr,
    const float* spatial_context, uint32_t spatial_dim) {

    if (!pr || !spatial_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_send_to_entorhinal: required parameter is NULL (pr, spatial_context)");
        return -1;
    }

    /* Store as current spatial context for binding */
    /* Implementation would bind to currently active objects */
    (void)spatial_dim;

    return 0;
}

int perirhinal_sync_entorhinal(nimcp_perirhinal_t* pr) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_sync_entorhinal: pr is NULL");
        return -1;
    }
    if (!pr->entorhinal_bridge.entorhinal) return 0;

    /* Synchronization logic would go here */
    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

int perirhinal_process_incoming(nimcp_perirhinal_t* pr) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_process_incoming: pr is NULL");
        return -1;
    }

    /* Process from perception layer if connected */
    if (pr->perception_bridge.perception && pr->perception_bridge.visual_input) {
        perirhinal_process_visual_input(pr,
            pr->perception_bridge.visual_input,
            pr->perception_bridge.visual_dim);
    }

    return 0;
}

int perirhinal_send_outgoing(nimcp_perirhinal_t* pr) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_send_outgoing: pr is NULL");
        return -1;
    }

    /* Send to cognitive bridge if connected */
    if (pr->cognitive_bridge.hub) {
        pr->cognitive_bridge.cognitive_events_sent++;
    }

    /* Sync with entorhinal */
    perirhinal_sync_entorhinal(pr);

    return 0;
}

int perirhinal_bidirectional_update(nimcp_perirhinal_t* pr, float dt) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }

    perirhinal_process_incoming(pr);
    perirhinal_update(pr, dt);
    perirhinal_send_outgoing(pr);

    return 0;
}

int perirhinal_process_visual_input(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim) {

    if (!pr || !visual_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: required parameter is NULL (pr, visual_features)");
        return -1;
    }

    /* Store current input */
    if (feature_dim != pr->current_input_dim) {
        float* new_input = (float*)nimcp_realloc(pr->current_visual_input,
            feature_dim * sizeof(float));
        if (!new_input) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perirhinal_bidirectional_update: new_input is NULL");
            return -1;
        }
        pr->current_visual_input = new_input;
        pr->current_input_dim = feature_dim;
    }
    memcpy(pr->current_visual_input, visual_features, feature_dim * sizeof(float));

    /* Compute familiarity and novelty */
    perirhinal_compute_familiarity(pr, visual_features, feature_dim);
    perirhinal_compute_novelty(pr, visual_features, feature_dim);

    /* Update object cell activations */
    for (uint32_t i = 0; i < pr->num_object_cells; i++) {
        float activation = 0.0f;
        if (pr->object_cells[i].feature_weights) {
            for (uint32_t j = 0; j < feature_dim &&
                 j < pr->object_cells[i].feature_dim; j++) {
                activation += visual_features[j] *
                              pr->object_cells[i].feature_weights[j];
            }
        }
        pr->object_cells[i].activation = sigmoid(activation);
    }

    return 0;
}

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

int perirhinal_init_entorhinal_bridge(nimcp_perirhinal_t* pr,
    nimcp_entorhinal_t* entorhinal) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->entorhinal_bridge.entorhinal = entorhinal;
    pr->entorhinal_bridge.spatial_context_weight = 0.5f;
    pr->entorhinal_bridge.object_context_binding = 0.7f;
    return 0;
}

int perirhinal_init_security_bridge(nimcp_perirhinal_t* pr,
    nimcp_security_context_t* security_ctx,
    nimcp_access_control_t* access_control) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->security_bridge.security_ctx = security_ctx;
    pr->security_bridge.access_control = access_control;
    pr->security_bridge.access_level = 1;
    return 0;
}

int perirhinal_init_immune_bridge(nimcp_perirhinal_t* pr,
    brain_immune_system_t* immune) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->immune_bridge.immune = immune;
    pr->immune_bridge.health_score = 1.0f;
    return 0;
}

int perirhinal_init_bio_async_bridge(nimcp_perirhinal_t* pr,
    nimcp_bio_router_t* router) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->bio_async_bridge.router = router;
    return 0;
}

int perirhinal_init_snn_bridge(nimcp_perirhinal_t* pr,
    nimcp_snn_network_t* snn) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->snn_bridge.snn = snn;
    return 0;
}

int perirhinal_init_plasticity_bridge(nimcp_perirhinal_t* pr,
    nimcp_plasticity_manager_t* plasticity,
    nimcp_stdp_rule_t* stdp_rule) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->plasticity_bridge.plasticity = plasticity;
    pr->plasticity_bridge.stdp_rule = stdp_rule;
    pr->plasticity_bridge.learning_rate = pr->config.learning_rate;
    return 0;
}

int perirhinal_init_cognitive_bridge(nimcp_perirhinal_t* pr,
    working_memory_t* wm,
    attention_system_t* attention,
    cognitive_integration_hub_t* hub) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->cognitive_bridge.working_memory = wm;
    pr->cognitive_bridge.attention = attention;
    pr->cognitive_bridge.hub = hub;
    return 0;
}

int perirhinal_init_training_bridge(nimcp_perirhinal_t* pr,
    nimcp_training_context_t* training_ctx) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->training_bridge.training_ctx = training_ctx;
    return 0;
}

int perirhinal_init_substrate_bridge(nimcp_perirhinal_t* pr,
    nimcp_neural_substrate_t* substrate) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->substrate_bridge.substrate = substrate;
    pr->substrate_bridge.atp_level = 1.0f;
    pr->substrate_bridge.oxygen_level = 1.0f;
    pr->substrate_bridge.glucose_level = 1.0f;
    return 0;
}

int perirhinal_init_resonance_bridge(nimcp_perirhinal_t* pr,
    nimcp_prime_resonance_t* resonance) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->resonance_bridge.resonance = resonance;
    return 0;
}

int perirhinal_init_thalamic_bridge(nimcp_perirhinal_t* pr,
    thalamus_adapter_t* thalamus) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->thalamic_bridge.thalamus = thalamus;
    pr->thalamic_bridge.relay_gain = 1.0f;
    return 0;
}

int perirhinal_init_hippocampus_bridge(nimcp_perirhinal_t* pr,
    hippocampus_adapter_t* hippocampus) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->hippocampus_bridge.hippocampus = hippocampus;
    return 0;
}

int perirhinal_init_perception_bridge(nimcp_perirhinal_t* pr,
    nimcp_perception_layer_t* perception) {

    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: pr is NULL");
        return -1;
    }
    pr->perception_bridge.perception = perception;
    return 0;
}

int perirhinal_init_all_bridges(nimcp_perirhinal_t* pr,
    nimcp_brain_t* brain) {

    if (!pr || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_bidirectional_update: required parameter is NULL (pr, brain)");
        return -1;
    }
    /* Would initialize all bridges from brain structure */
    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

perirhinal_status_t perirhinal_get_status(const nimcp_perirhinal_t* pr) {
    if (!pr) return PERIRHINAL_STATUS_ERROR;
    return pr->status;
}

perirhinal_error_t perirhinal_get_last_error(const nimcp_perirhinal_t* pr) {
    if (!pr) return PERIRHINAL_ERROR_INTERNAL;
    return pr->last_error;
}

const char* perirhinal_error_string(perirhinal_error_t error) {
    switch (error) {
        case PERIRHINAL_ERROR_NONE: return "No error";
        case PERIRHINAL_ERROR_INVALID_INPUT: return "Invalid input";
        case PERIRHINAL_ERROR_OBJECT_NOT_FOUND: return "Object not found";
        case PERIRHINAL_ERROR_MEMORY_FULL: return "Memory full";
        case PERIRHINAL_ERROR_ENCODING_FAILED: return "Encoding failed";
        case PERIRHINAL_ERROR_RECOGNITION_FAILED: return "Recognition failed";
        case PERIRHINAL_ERROR_SECURITY_VIOLATION: return "Security violation";
        case PERIRHINAL_ERROR_IMMUNE_REJECTION: return "Immune rejection";
        case PERIRHINAL_ERROR_SUBSTRATE_DEPLETED: return "Substrate depleted";
        case PERIRHINAL_ERROR_SYNC_FAILURE: return "Sync failure";
        case PERIRHINAL_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case PERIRHINAL_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* perirhinal_status_string(perirhinal_status_t status) {
    switch (status) {
        case PERIRHINAL_STATUS_IDLE: return "Idle";
        case PERIRHINAL_STATUS_ENCODING: return "Encoding";
        case PERIRHINAL_STATUS_RECOGNIZING: return "Recognizing";
        case PERIRHINAL_STATUS_NOVELTY_DETECTING: return "Detecting novelty";
        case PERIRHINAL_STATUS_FAMILIARITY_COMPUTING: return "Computing familiarity";
        case PERIRHINAL_STATUS_RECENCY_UPDATING: return "Updating recency";
        case PERIRHINAL_STATUS_CONSOLIDATING: return "Consolidating";
        case PERIRHINAL_STATUS_READY: return "Ready";
        case PERIRHINAL_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

int perirhinal_get_stats(const nimcp_perirhinal_t* pr, perirhinal_stats_t* stats) {
    if (!pr || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_get_stats: required parameter is NULL (pr, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(perirhinal_stats_t));

    stats->updates_processed = pr->updates_processed;
    stats->objects_encoded = pr->objects_encoded;
    stats->objects_recognized = pr->objects_recognized;
    stats->novelty_detections = pr->novelty_detections;
    stats->total_stored_objects = pr->num_stored_objects;
    stats->mean_recognition_confidence = pr->mean_recognition_confidence;
    stats->mean_familiarity_signal = pr->mean_familiarity_signal;

    /* Calculate memory utilization */
    stats->memory_utilization =
        (float)pr->num_stored_objects / (float)pr->max_stored_objects;

    /* Calculate mean object activation */
    float total_activation = 0.0f;
    for (uint32_t i = 0; i < pr->num_object_cells; i++) {
        total_activation += pr->object_cells[i].activation;
    }
    stats->mean_object_activation = total_activation / pr->num_object_cells;

    return 0;
}

int perirhinal_get_config(const nimcp_perirhinal_t* pr, perirhinal_config_t* config) {
    if (!pr || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_get_config: required parameter is NULL (pr, config)");
        return -1;
    }
    *config = pr->config;
    return 0;
}

float perirhinal_get_health_status(const nimcp_perirhinal_t* pr) {
    if (!pr) return 0.0f;

    float health = 1.0f;

    /* Factor in memory utilization */
    float utilization = (float)pr->num_stored_objects / (float)pr->max_stored_objects;
    if (utilization > 0.9f) health *= 0.8f;

    /* Factor in error state */
    if (pr->status == PERIRHINAL_STATUS_ERROR) health *= 0.5f;

    return health;
}

int perirhinal_log_diagnostics(const nimcp_perirhinal_t* pr) {
    if (!pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_log_diagnostics: pr is NULL");
        return -1;
    }
    /* Logging would be implemented here */
    return 0;
}

size_t perirhinal_get_object_cell_activity(const nimcp_perirhinal_t* pr,
    float* activity, size_t max_cells) {

    if (!pr || !activity) return 0;

    size_t count = (pr->num_object_cells < max_cells) ?
                   pr->num_object_cells : max_cells;

    for (size_t i = 0; i < count; i++) {
        activity[i] = pr->object_cells[i].activation;
    }

    return count;
}

size_t perirhinal_get_familiarity_cell_activity(const nimcp_perirhinal_t* pr,
    float* activity, size_t max_cells) {

    if (!pr || !activity) return 0;

    size_t count = (pr->num_familiarity_cells < max_cells) ?
                   pr->num_familiarity_cells : max_cells;

    for (size_t i = 0; i < count; i++) {
        activity[i] = pr->familiarity_cells[i].activation;
    }

    return count;
}

float perirhinal_get_current_familiarity(const nimcp_perirhinal_t* pr) {
    if (!pr) return 0.0f;
    return pr->current_familiarity;
}

float perirhinal_get_current_novelty(const nimcp_perirhinal_t* pr) {
    if (!pr) return 1.0f;
    return pr->current_novelty;
}

/*=============================================================================
 * SERIALIZATION API
 *===========================================================================*/

int perirhinal_serialize(const nimcp_perirhinal_t* pr,
    uint8_t* buffer, size_t buffer_size, size_t* bytes_written) {

    if (!pr || !buffer || !bytes_written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_get_current_novelty: required parameter is NULL (pr, buffer, bytes_written)");
        return -1;
    }

    /* Simplified serialization - just store key values */
    size_t needed = sizeof(perirhinal_config_t) + sizeof(uint64_t) * 5;
    if (buffer_size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_get_current_novelty: validation failed");
        return -1;
    }

    size_t offset = 0;
    memcpy(buffer + offset, &pr->config, sizeof(perirhinal_config_t));
    offset += sizeof(perirhinal_config_t);

    memcpy(buffer + offset, &pr->updates_processed, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &pr->objects_encoded, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &pr->objects_recognized, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &pr->novelty_detections, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &pr->num_stored_objects, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    *bytes_written = offset;
    return 0;
}

int perirhinal_deserialize(nimcp_perirhinal_t* pr,
    const uint8_t* buffer, size_t buffer_size) {

    if (!pr || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perirhinal_get_current_novelty: required parameter is NULL (pr, buffer)");
        return -1;
    }

    size_t needed = sizeof(perirhinal_config_t) + sizeof(uint64_t) * 5;
    if (buffer_size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perirhinal_get_current_novelty: validation failed");
        return -1;
    }

    size_t offset = 0;
    memcpy(&pr->config, buffer + offset, sizeof(perirhinal_config_t));
    offset += sizeof(perirhinal_config_t);

    memcpy(&pr->updates_processed, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(&pr->objects_encoded, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(&pr->objects_recognized, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(&pr->novelty_detections, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    return 0;
}

size_t perirhinal_get_serialization_size(const nimcp_perirhinal_t* pr) {
    if (!pr) return 0;
    return sizeof(perirhinal_config_t) + sizeof(uint64_t) * 5;
}
