/**
 * @file nimcp_parahippocampal.c
 * @brief Parahippocampal Cortex - Scene and Context Processing
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(parahippocampal, MESH_ADAPTER_CATEGORY_COGNITIVE)


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

static float compute_distance(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

static float gaussian(float x, float mean, float sigma) {
    if (fabsf(sigma) < 1e-10f) return 0.0f;
    float diff = x - mean;
    return expf(-(diff * diff) / (2.0f * sigma * sigma));
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

parahipp_config_t parahipp_default_config(void) {
    parahipp_config_t config = {0};

    /* Place cell parameters */
    config.num_place_cells = PARAHIPP_DEFAULT_PLACE_CELLS;
    config.place_field_radius_min = 0.5f;
    config.place_field_radius_max = 5.0f;

    /* Scene cell parameters */
    config.num_scene_cells = PARAHIPP_DEFAULT_SCENE_CELLS;
    config.scene_selectivity = 0.8f;
    config.view_invariance_target = 0.7f;

    /* Layout cell parameters */
    config.num_layout_cells = PARAHIPP_DEFAULT_LAYOUT_CELLS;
    config.boundary_angles = 360;

    /* Context cell parameters */
    config.num_context_cells = PARAHIPP_DEFAULT_CONTEXT_CELLS;
    config.context_binding_rate = 0.1f;
    config.context_decay_rate = 0.01f;

    /* Landmark cell parameters */
    config.num_landmark_cells = PARAHIPP_DEFAULT_LANDMARK_CELLS;
    config.max_landmarks = PARAHIPP_DEFAULT_MAX_LANDMARKS;

    /* Memory parameters */
    config.max_stored_scenes = PARAHIPP_DEFAULT_MAX_STORED_SCENES;
    config.scene_dim = PARAHIPP_DEFAULT_SCENE_DIM;
    config.layout_dim = PARAHIPP_DEFAULT_LAYOUT_DIM;
    config.context_dim = PARAHIPP_DEFAULT_CONTEXT_DIM;
    config.max_views_per_scene = 8;

    /* Recognition parameters */
    config.scene_match_threshold = PARAHIPP_SCENE_MATCH_THRESHOLD;
    config.context_match_threshold = PARAHIPP_CONTEXT_BINDING_THRESHOLD;
    config.max_alternatives = 5;

    /* Enable all integrations by default */
    config.enable_entorhinal = true;
    config.enable_perirhinal = true;
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
    config.enable_omni = true;
    config.enable_hypothalamus = true;
    config.enable_logic = true;
    config.enable_kg = true;

    /* Processing options */
    config.enable_view_invariance = true;
    config.enable_layout_processing = true;
    config.enable_landmark_tracking = true;
    config.enable_context_binding = true;

    /* Learning parameters */
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
    config.eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;

    /* Oscillation parameters */
    config.theta_frequency = 8.0f;
    config.gamma_frequency = 40.0f;

    return config;
}

nimcp_parahippocampal_t* parahipp_create(const parahipp_config_t* config) {
    nimcp_parahippocampal_t* ph = (nimcp_parahippocampal_t*)nimcp_calloc(1, sizeof(nimcp_parahippocampal_t));
    if (!ph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ph is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        ph->config = *config;
    } else {
        ph->config = parahipp_default_config();
    }

    /* Allocate place cells */
    ph->num_place_cells = ph->config.num_place_cells;
    ph->place_cells = (nimcp_place_cell_t*)nimcp_calloc(ph->num_place_cells, sizeof(nimcp_place_cell_t));
    if (!ph->place_cells) goto error;

    /* Initialize place cells with random place fields */
    for (uint32_t i = 0; i < ph->num_place_cells; i++) {
        ph->place_cells[i].cell_id = i;
        ph->place_cells[i].place_field_center[0] = ((float)nimcp_tl_rand() / RAND_MAX) * 100.0f;
        ph->place_cells[i].place_field_center[1] = ((float)nimcp_tl_rand() / RAND_MAX) * 100.0f;
        ph->place_cells[i].place_field_center[2] = 0.0f;
        ph->place_cells[i].place_field_radius = ph->config.place_field_radius_min +
            ((float)nimcp_tl_rand() / RAND_MAX) * (ph->config.place_field_radius_max - ph->config.place_field_radius_min);
        ph->place_cells[i].peak_rate = 1.0f;
        ph->place_cells[i].learning_rate = ph->config.learning_rate;
    }

    /* Allocate scene cells */
    ph->num_scene_cells = ph->config.num_scene_cells;
    ph->scene_cells = (nimcp_scene_cell_t*)nimcp_calloc(ph->num_scene_cells, sizeof(nimcp_scene_cell_t));
    if (!ph->scene_cells) goto error;

    /* Initialize scene cells */
    for (uint32_t i = 0; i < ph->num_scene_cells; i++) {
        ph->scene_cells[i].cell_id = i;
        ph->scene_cells[i].selectivity = ph->config.scene_selectivity;
        ph->scene_cells[i].view_invariance = ph->config.view_invariance_target;
        ph->scene_cells[i].scene_dim = ph->config.scene_dim;
        ph->scene_cells[i].scene_weights = (float*)nimcp_calloc(ph->config.scene_dim, sizeof(float));
        if (!ph->scene_cells[i].scene_weights) goto error;

        /* Random initialization */
        for (uint32_t j = 0; j < ph->config.scene_dim; j++) {
            ph->scene_cells[i].scene_weights[j] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        ph->scene_cells[i].preferred_category = (scene_category_t)(i % SCENE_CATEGORY_COUNT);
    }

    /* Allocate layout cells */
    ph->num_layout_cells = ph->config.num_layout_cells;
    ph->layout_cells = (nimcp_layout_cell_t*)nimcp_calloc(ph->num_layout_cells, sizeof(nimcp_layout_cell_t));
    if (!ph->layout_cells) goto error;

    /* Initialize layout cells */
    for (uint32_t i = 0; i < ph->num_layout_cells; i++) {
        ph->layout_cells[i].cell_id = i;
        ph->layout_cells[i].preferred_layout = (layout_type_t)(i % LAYOUT_TYPE_COUNT);
        ph->layout_cells[i].num_angles = ph->config.boundary_angles;
        ph->layout_cells[i].boundary_distances = (float*)nimcp_calloc(ph->config.boundary_angles, sizeof(float));
        if (!ph->layout_cells[i].boundary_distances) goto error;
    }

    /* Allocate context cells */
    ph->num_context_cells = ph->config.num_context_cells;
    ph->context_cells = (nimcp_context_cell_t*)nimcp_calloc(ph->num_context_cells, sizeof(nimcp_context_cell_t));
    if (!ph->context_cells) goto error;

    /* Initialize context cells */
    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        ph->context_cells[i].cell_id = i;
        ph->context_cells[i].context_dim = ph->config.context_dim;
        ph->context_cells[i].context_vector = (float*)nimcp_calloc(ph->config.context_dim, sizeof(float));
        if (!ph->context_cells[i].context_vector) goto error;
        ph->context_cells[i].temporal_stability = 1.0f;
        ph->context_cells[i].spatial_weight = 0.4f;
        ph->context_cells[i].object_weight = 0.3f;
        ph->context_cells[i].temporal_weight = 0.3f;
    }

    /* Allocate landmark cells */
    ph->num_landmark_cells = ph->config.num_landmark_cells;
    ph->landmark_cells = (nimcp_landmark_cell_t*)nimcp_calloc(ph->num_landmark_cells, sizeof(nimcp_landmark_cell_t));
    if (!ph->landmark_cells) goto error;

    /* Initialize landmark cells */
    for (uint32_t i = 0; i < ph->num_landmark_cells; i++) {
        ph->landmark_cells[i].cell_id = i;
        ph->landmark_cells[i].landmark_id = UINT32_MAX;  /* Unassigned */
    }

    /* Allocate stored scenes */
    ph->max_stored_scenes = ph->config.max_stored_scenes;
    ph->stored_scenes = (nimcp_stored_scene_t*)nimcp_calloc(ph->max_stored_scenes, sizeof(nimcp_stored_scene_t));
    if (!ph->stored_scenes) goto error;

    for (uint32_t i = 0; i < ph->max_stored_scenes; i++) {
        ph->stored_scenes[i].scene_id = UINT32_MAX;  /* Invalid/empty */
    }
    ph->num_stored_scenes = 0;

    /* Allocate stored landmarks */
    ph->max_landmarks = ph->config.max_landmarks;
    ph->stored_landmarks = (nimcp_stored_landmark_t*)nimcp_calloc(ph->max_landmarks, sizeof(nimcp_stored_landmark_t));
    if (!ph->stored_landmarks) goto error;

    for (uint32_t i = 0; i < ph->max_landmarks; i++) {
        ph->stored_landmarks[i].landmark_id = UINT32_MAX;
    }
    ph->num_stored_landmarks = 0;

    /* Allocate current processing buffers */
    ph->current_input_dim = ph->config.scene_dim;
    ph->current_scene_input = (float*)nimcp_calloc(ph->current_input_dim, sizeof(float));
    if (!ph->current_scene_input) goto error;

    ph->current_context_dim = ph->config.context_dim;
    ph->current_context = (float*)nimcp_calloc(ph->current_context_dim, sizeof(float));
    if (!ph->current_context) goto error;

    /* Initialize layout */
    ph->current_layout.geometric_features = (float*)nimcp_calloc(ph->config.layout_dim, sizeof(float));
    if (!ph->current_layout.geometric_features) goto error;
    ph->current_layout.feature_dim = ph->config.layout_dim;

    /* Initialize timing */
    ph->creation_time_ms = get_current_time_ms();
    ph->last_update_ms = ph->creation_time_ms;

    /* Set initial status */
    ph->status = PARAHIPP_STATUS_READY;
    ph->last_error = PARAHIPP_ERROR_NONE;

    return ph;

error:
    parahipp_destroy(ph);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_create: operation failed");
    return NULL;
}

void parahipp_destroy(nimcp_parahippocampal_t* ph) {
    if (!ph) return;

    /* Free place cells */
    nimcp_free(ph->place_cells);

    /* Free scene cells */
    if (ph->scene_cells) {
        for (uint32_t i = 0; i < ph->num_scene_cells; i++) {
            nimcp_free(ph->scene_cells[i].scene_weights);
        }
        nimcp_free(ph->scene_cells);
    }

    /* Free layout cells */
    if (ph->layout_cells) {
        for (uint32_t i = 0; i < ph->num_layout_cells; i++) {
            nimcp_free(ph->layout_cells[i].boundary_distances);
        }
        nimcp_free(ph->layout_cells);
    }

    /* Free context cells */
    if (ph->context_cells) {
        for (uint32_t i = 0; i < ph->num_context_cells; i++) {
            nimcp_free(ph->context_cells[i].context_vector);
        }
        nimcp_free(ph->context_cells);
    }

    /* Free landmark cells */
    nimcp_free(ph->landmark_cells);

    /* Free stored scenes */
    if (ph->stored_scenes) {
        for (uint32_t i = 0; i < ph->max_stored_scenes; i++) {
            if (ph->stored_scenes[i].scene_id != UINT32_MAX) {
                nimcp_free(ph->stored_scenes[i].scene_features);
                nimcp_free(ph->stored_scenes[i].context_vector);
                nimcp_free(ph->stored_scenes[i].landmark_ids);
                nimcp_free(ph->stored_scenes[i].object_ids);
                nimcp_free(ph->stored_scenes[i].layout.geometric_features);
                if (ph->stored_scenes[i].view_features) {
                    for (uint32_t v = 0; v < ph->stored_scenes[i].num_views; v++) {
                        nimcp_free(ph->stored_scenes[i].view_features[v]);
                    }
                    nimcp_free(ph->stored_scenes[i].view_features);
                }
                nimcp_free(ph->stored_scenes[i].view_headings);
            }
        }
        nimcp_free(ph->stored_scenes);
    }

    /* Free stored landmarks */
    if (ph->stored_landmarks) {
        for (uint32_t i = 0; i < ph->max_landmarks; i++) {
            if (ph->stored_landmarks[i].landmark_id != UINT32_MAX) {
                nimcp_free(ph->stored_landmarks[i].visual_features);
            }
        }
        nimcp_free(ph->stored_landmarks);
    }

    /* Free processing buffers */
    nimcp_free(ph->current_scene_input);
    nimcp_free(ph->current_context);
    nimcp_free(ph->current_layout.geometric_features);

    nimcp_free(ph);
}

int parahipp_reset(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_reset: ph is NULL");
        return -1;
    }

    /* Reset statistics */
    ph->updates_processed = 0;
    ph->scenes_encoded = 0;
    ph->scenes_recognized = 0;
    ph->context_switches = 0;
    ph->mean_recognition_confidence = 0.0f;
    ph->total_processing_time_ms = 0.0;

    /* Reset cell activations */
    for (uint32_t i = 0; i < ph->num_place_cells; i++) {
        ph->place_cells[i].activation = 0.0f;
        ph->place_cells[i].eligibility_trace = 0.0f;
    }

    for (uint32_t i = 0; i < ph->num_scene_cells; i++) {
        ph->scene_cells[i].activation = 0.0f;
    }

    for (uint32_t i = 0; i < ph->num_layout_cells; i++) {
        ph->layout_cells[i].activation = 0.0f;
        ph->layout_cells[i].openness = 0.0f;
    }

    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        ph->context_cells[i].activation = 0.0f;
        ph->context_cells[i].binding_strength = 0.0f;
        memset(ph->context_cells[i].context_vector, 0,
               ph->config.context_dim * sizeof(float));
    }

    for (uint32_t i = 0; i < ph->num_landmark_cells; i++) {
        ph->landmark_cells[i].activation = 0.0f;
    }

    /* Reset current state */
    memset(ph->current_scene_input, 0, ph->current_input_dim * sizeof(float));
    memset(ph->current_context, 0, ph->current_context_dim * sizeof(float));
    memset(ph->current_position, 0, sizeof(ph->current_position));
    ph->current_heading = 0.0f;

    ph->status = PARAHIPP_STATUS_READY;
    ph->last_error = PARAHIPP_ERROR_NONE;

    return 0;
}

int parahipp_update(nimcp_parahippocampal_t* ph, float dt) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_update: ph is NULL");
        return -1;
    }

    uint64_t start_time = get_current_time_ms();

    /* Decay context cell activations */
    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        ph->context_cells[i].activation *= (1.0f - ph->config.context_decay_rate * dt);
    }

    /* Decay eligibility traces */
    for (uint32_t i = 0; i < ph->num_place_cells; i++) {
        ph->place_cells[i].eligibility_trace *= ph->config.eligibility_decay;
    }

    /* Update context stability */
    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        /* If no change detected, increase stability */
        if (ph->context_cells[i].change_detection < 0.1f) {
            ph->context_cells[i].temporal_stability =
                fminf(1.0f, ph->context_cells[i].temporal_stability + 0.01f * dt);
        }
    }

    ph->updates_processed++;
    ph->last_update_ms = get_current_time_ms();
    ph->total_processing_time_ms += (double)(ph->last_update_ms - start_time);
    ph->simulation_dt_ms = dt * 1000.0f;

    return 0;
}

/*=============================================================================
 * SCENE ENCODING/RECOGNITION API
 *===========================================================================*/

int parahipp_encode_scene(nimcp_parahippocampal_t* ph,
    const float* scene_features, uint32_t feature_dim,
    const float* position, float heading,
    const char* name, uint32_t* scene_id_out) {

    if (!ph || !scene_features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_update: required parameter is NULL (ph, scene_features)");
        return -1;
    }

    ph->status = PARAHIPP_STATUS_SCENE_ENCODING;

    /* Find empty slot */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < ph->max_stored_scenes; i++) {
        if (ph->stored_scenes[i].scene_id == UINT32_MAX) {
            slot = i;
            break;
        }
    }

    if (slot == UINT32_MAX) {
        ph->last_error = PARAHIPP_ERROR_MEMORY_FULL;
        ph->status = PARAHIPP_STATUS_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_update: validation failed");
        return -1;
    }

    nimcp_stored_scene_t* scene = &ph->stored_scenes[slot];
    scene->scene_id = slot;

    /* Copy name */
    if (name) {
        strncpy(scene->name, name, sizeof(scene->name) - 1);
        scene->name[sizeof(scene->name) - 1] = '\0';
    }

    /* Allocate and copy scene features */
    scene->feature_dim = feature_dim;
    scene->scene_features = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!scene->scene_features) {
        scene->scene_id = UINT32_MAX;
        ph->last_error = PARAHIPP_ERROR_ENCODING_FAILED;
        ph->status = PARAHIPP_STATUS_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parahipp_update: scene->scene_features is NULL");
        return -1;
    }
    memcpy(scene->scene_features, scene_features, feature_dim * sizeof(float));

    /* Classify scene category based on features */
    scene->category = SCENE_CATEGORY_UNKNOWN;

    /* Initialize layout */
    scene->layout.type = LAYOUT_TYPE_UNKNOWN;
    scene->layout.feature_dim = ph->config.layout_dim;
    scene->layout.geometric_features = (float*)nimcp_calloc(ph->config.layout_dim, sizeof(float));

    /* Copy position and heading */
    if (position) {
        memcpy(scene->position, position, 3 * sizeof(float));
    }
    scene->heading = heading;

    /* Initialize context */
    scene->context_dim = ph->config.context_dim;
    scene->context_vector = (float*)nimcp_calloc(scene->context_dim, sizeof(float));
    if (ph->current_context) {
        memcpy(scene->context_vector, ph->current_context,
               fminf(scene->context_dim, ph->current_context_dim) * sizeof(float));
    }
    scene->context_stability = 0.5f;

    /* Initialize first view */
    scene->num_views = 1;
    scene->view_features = (float**)nimcp_malloc(sizeof(float*));
    scene->view_headings = (float*)nimcp_malloc(sizeof(float));
    if (scene->view_features && scene->view_headings) {
        scene->view_features[0] = (float*)nimcp_malloc(feature_dim * sizeof(float));
        if (scene->view_features[0]) {
            memcpy(scene->view_features[0], scene_features, feature_dim * sizeof(float));
        }
        scene->view_headings[0] = heading;
    }

    /* Initialize memory properties */
    scene->familiarity = 0.1f;
    scene->recency = 1.0f;
    scene->first_encoded_ms = get_current_time_ms();
    scene->last_visited_ms = scene->first_encoded_ms;
    scene->visit_count = 1;

    /* Initialize associations */
    scene->landmark_ids = NULL;
    scene->num_landmarks = 0;
    scene->object_ids = NULL;
    scene->num_objects = 0;

    ph->num_stored_scenes++;
    ph->scenes_encoded++;

    if (scene_id_out) {
        *scene_id_out = scene->scene_id;
    }

    ph->status = PARAHIPP_STATUS_READY;
    return 0;
}

int parahipp_recognize_scene(nimcp_parahippocampal_t* ph,
    const float* scene_features, uint32_t feature_dim,
    parahipp_recognition_result_t* result) {

    if (!ph || !scene_features || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_update: required parameter is NULL (ph, scene_features, result)");
        return -1;
    }

    ph->status = PARAHIPP_STATUS_SCENE_RECOGNIZING;
    memset(result, 0, sizeof(parahipp_recognition_result_t));

    float best_match = 0.0f;
    uint32_t best_id = UINT32_MAX;

    /* Search stored scenes */
    for (uint32_t i = 0; i < ph->max_stored_scenes; i++) {
        if (ph->stored_scenes[i].scene_id == UINT32_MAX) continue;

        nimcp_stored_scene_t* scene = &ph->stored_scenes[i];
        float similarity = 0.0f;

        /* Compare with main scene features */
        if (scene->scene_features && scene->feature_dim == feature_dim) {
            similarity = compute_cosine_similarity(
                scene_features, scene->scene_features, feature_dim);
        }

        /* Also check views for view-invariant matching */
        if (ph->config.enable_view_invariance && scene->view_features) {
            for (uint32_t v = 0; v < scene->num_views; v++) {
                if (scene->view_features[v]) {
                    float view_sim = compute_cosine_similarity(
                        scene_features, scene->view_features[v], feature_dim);
                    if (view_sim > similarity) {
                        similarity = view_sim;
                    }
                }
            }
        }

        if (similarity > best_match) {
            best_match = similarity;
            best_id = scene->scene_id;
        }
    }

    /* Fill result */
    result->scene_id = best_id;
    result->match_confidence = best_match;

    if (best_id != UINT32_MAX) {
        nimcp_stored_scene_t* scene = &ph->stored_scenes[best_id];
        result->category = scene->category;
        result->layout_type = scene->layout.type;
        result->familiarity = scene->familiarity;
        memcpy(result->estimated_position, scene->position, 3 * sizeof(float));
        result->position_confidence = best_match;
    }

    /* Determine novelty */
    result->is_novel = (best_match < ph->config.scene_match_threshold);

    /* Determine context state */
    if (result->is_novel) {
        result->context_state = CONTEXT_STATE_NOVEL;
    } else if (best_match > 0.9f) {
        result->context_state = CONTEXT_STATE_STABLE;
    } else if (best_match > 0.6f) {
        result->context_state = CONTEXT_STATE_FAMILIAR;
    } else {
        result->context_state = CONTEXT_STATE_CHANGED;
    }

    ph->scenes_recognized++;
    ph->mean_recognition_confidence =
        (ph->mean_recognition_confidence * (ph->scenes_recognized - 1) + best_match)
        / ph->scenes_recognized;

    ph->status = PARAHIPP_STATUS_READY;
    return 0;
}

int parahipp_add_scene_view(nimcp_parahippocampal_t* ph,
    uint32_t scene_id, const float* view_features, uint32_t feature_dim,
    float heading) {

    if (!ph || !view_features || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (ph, view_features)");
        return -1;
    }

    nimcp_stored_scene_t* scene = &ph->stored_scenes[scene_id];
    if (scene->scene_id == UINT32_MAX) {
        ph->last_error = PARAHIPP_ERROR_SCENE_NOT_FOUND;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    if (scene->num_views >= ph->config.max_views_per_scene) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "unknown: capacity exceeded");
        return -1;  /* Max views reached */
    }

    /* Reallocate view arrays */
    float** new_features = (float**)nimcp_realloc(scene->view_features,
        (scene->num_views + 1) * sizeof(float*));
    float* new_headings = (float*)nimcp_realloc(scene->view_headings,
        (scene->num_views + 1) * sizeof(float));

    if (!new_features || !new_headings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: required parameter is NULL (new_features, new_headings)");
        return -1;
    }

    scene->view_features = new_features;
    scene->view_headings = new_headings;

    scene->view_features[scene->num_views] = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!scene->view_features[scene->num_views]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: scene->view_features is NULL");
        return -1;
    }

    memcpy(scene->view_features[scene->num_views], view_features, feature_dim * sizeof(float));
    scene->view_headings[scene->num_views] = heading;
    scene->num_views++;

    return 0;
}

const nimcp_stored_scene_t* parahipp_get_scene(const nimcp_parahippocampal_t* ph,
    uint32_t scene_id) {

    if (!ph || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "unknown: ph is NULL");
        return NULL;
    }
    if (ph->stored_scenes[scene_id].scene_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }
    return &ph->stored_scenes[scene_id];
}

int parahipp_update_scene_visit(nimcp_parahippocampal_t* ph, uint32_t scene_id) {
    if (!ph || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "parahipp_update_scene_visit: ph is NULL");
        return -1;
    }

    nimcp_stored_scene_t* scene = &ph->stored_scenes[scene_id];
    if (scene->scene_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_update_scene_visit: validation failed");
        return -1;
    }

    scene->familiarity += (1.0f - scene->familiarity) * 0.1f;
    scene->recency = 1.0f;
    scene->last_visited_ms = get_current_time_ms();
    scene->visit_count++;

    return 0;
}

int parahipp_forget_scene(nimcp_parahippocampal_t* ph, uint32_t scene_id) {
    if (!ph || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "parahipp_forget_scene: ph is NULL");
        return -1;
    }

    nimcp_stored_scene_t* scene = &ph->stored_scenes[scene_id];
    if (scene->scene_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_forget_scene: validation failed");
        return -1;
    }

    /* Free allocated memory */
    nimcp_free(scene->scene_features);
    nimcp_free(scene->context_vector);
    nimcp_free(scene->landmark_ids);
    nimcp_free(scene->object_ids);
    nimcp_free(scene->layout.geometric_features);

    if (scene->view_features) {
        for (uint32_t v = 0; v < scene->num_views; v++) {
            nimcp_free(scene->view_features[v]);
        }
        nimcp_free(scene->view_features);
    }
    nimcp_free(scene->view_headings);

    memset(scene, 0, sizeof(nimcp_stored_scene_t));
    scene->scene_id = UINT32_MAX;

    ph->num_stored_scenes--;
    return 0;
}

/*=============================================================================
 * PLACE CELL API
 *===========================================================================*/

int parahipp_update_place_cells(nimcp_parahippocampal_t* ph,
    const float* position, uint32_t dim) {

    if (!ph || !position || dim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_forget_scene: required parameter is NULL (ph, position)");
        return -1;
    }

    memcpy(ph->current_position, position, fminf(dim, 3) * sizeof(float));

    /* Update each place cell based on distance from place field center */
    for (uint32_t i = 0; i < ph->num_place_cells; i++) {
        float dist = compute_distance(position, ph->place_cells[i].place_field_center,
                                      fminf(dim, 3));
        float radius = ph->place_cells[i].place_field_radius;

        /* Gaussian activation based on distance */
        ph->place_cells[i].activation = gaussian(dist, 0.0f, radius) *
                                        ph->place_cells[i].peak_rate;

        /* Update eligibility trace */
        if (ph->place_cells[i].activation > 0.1f) {
            ph->place_cells[i].eligibility_trace = 1.0f;
        }
    }

    return 0;
}

int parahipp_get_place_population_vector(const nimcp_parahippocampal_t* ph,
    float* vector_out, uint32_t* dim) {

    if (!ph || !vector_out || !dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_forget_scene: required parameter is NULL (ph, vector_out, dim)");
        return -1;
    }

    /* Weighted sum of place field centers */
    float weighted_sum[3] = {0};
    float total_weight = 0.0f;

    for (uint32_t i = 0; i < ph->num_place_cells; i++) {
        float weight = ph->place_cells[i].activation;
        for (int j = 0; j < 3; j++) {
            weighted_sum[j] += weight * ph->place_cells[i].place_field_center[j];
        }
        total_weight += weight;
    }

    if (total_weight > 1e-6f) {
        for (int j = 0; j < 3; j++) {
            vector_out[j] = weighted_sum[j] / total_weight;
        }
    } else {
        memset(vector_out, 0, 3 * sizeof(float));
    }

    *dim = 3;
    return 0;
}

int parahipp_decode_position(const nimcp_parahippocampal_t* ph,
    float* position_out, float* confidence_out) {

    if (!ph || !position_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_forget_scene: required parameter is NULL (ph, position_out)");
        return -1;
    }

    uint32_t dim;
    int result = parahipp_get_place_population_vector(ph, position_out, &dim);

    if (confidence_out) {
        /* Confidence based on total activation */
        float total_activation = 0.0f;
        for (uint32_t i = 0; i < ph->num_place_cells; i++) {
            total_activation += ph->place_cells[i].activation;
        }
        *confidence_out = fminf(1.0f, total_activation / 10.0f);
    }

    return result;
}

int parahipp_get_active_place_cells(const nimcp_parahippocampal_t* ph,
    uint32_t* cell_ids, float* activations, uint32_t max_cells, uint32_t* num_active) {

    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_forget_scene: ph is NULL");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < ph->num_place_cells && count < max_cells; i++) {
        if (ph->place_cells[i].activation > 0.1f) {
            if (cell_ids) cell_ids[count] = i;
            if (activations) activations[count] = ph->place_cells[i].activation;
            count++;
        }
    }

    if (num_active) *num_active = count;
    return 0;
}

/*=============================================================================
 * SPATIAL LAYOUT API
 *===========================================================================*/

int parahipp_process_layout(nimcp_parahippocampal_t* ph,
    const float* boundary_distances, uint32_t num_angles) {

    if (!ph || !boundary_distances) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_forget_scene: required parameter is NULL (ph, boundary_distances)");
        return -1;
    }

    ph->status = PARAHIPP_STATUS_LAYOUT_PROCESSING;

    /* Copy boundary distances */
    uint32_t copy_angles = fminf(num_angles, 360);
    memcpy(ph->current_layout.boundaries, boundary_distances, copy_angles * sizeof(float));

    /* Compute openness (average boundary distance) */
    float total_dist = 0.0f;
    float min_dist = boundary_distances[0];
    float max_dist = boundary_distances[0];

    for (uint32_t i = 0; i < copy_angles; i++) {
        total_dist += boundary_distances[i];
        if (boundary_distances[i] < min_dist) min_dist = boundary_distances[i];
        if (boundary_distances[i] > max_dist) max_dist = boundary_distances[i];
    }

    ph->current_layout.openness = fminf(1.0f, total_dist / (copy_angles * 20.0f));

    /* Compute aspect ratio and symmetry */
    float ns_dist = (boundary_distances[0] + boundary_distances[copy_angles/2]) / 2.0f;
    float ew_dist = (boundary_distances[copy_angles/4] + boundary_distances[3*copy_angles/4]) / 2.0f;

    if (ew_dist > 0.0f) {
        ph->current_layout.boundaries[0] = ns_dist / ew_dist;  /* Aspect ratio stored here temporarily */
    }

    /* Classify layout type */
    float ratio = max_dist / (min_dist + 1e-6f);
    if (ratio < 1.5f) {
        ph->current_layout.type = LAYOUT_TYPE_CIRCULAR;
    } else if (ratio < 3.0f) {
        ph->current_layout.type = LAYOUT_TYPE_RECTANGULAR;
    } else {
        ph->current_layout.type = LAYOUT_TYPE_CORRIDOR;
    }

    /* Compute navigability */
    ph->current_layout.navigability = fminf(1.0f, min_dist / 2.0f);

    /* Update layout cells */
    for (uint32_t i = 0; i < ph->num_layout_cells; i++) {
        /* Activation based on match to preferred layout */
        if (ph->layout_cells[i].preferred_layout == ph->current_layout.type) {
            ph->layout_cells[i].activation = 0.8f;
        } else {
            ph->layout_cells[i].activation = 0.2f;
        }
        ph->layout_cells[i].openness = ph->current_layout.openness;
    }

    ph->status = PARAHIPP_STATUS_READY;
    return 0;
}

layout_type_t parahipp_get_layout_type(const nimcp_parahippocampal_t* ph) {
    if (!ph) return LAYOUT_TYPE_UNKNOWN;
    return ph->current_layout.type;
}

int parahipp_get_layout_features(const nimcp_parahippocampal_t* ph,
    float* features_out, uint32_t max_dim) {

    if (!ph || !features_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_get_layout_type: required parameter is NULL (ph, features_out)");
        return -1;
    }

    uint32_t copy_dim = fminf(ph->current_layout.feature_dim, max_dim);
    if (ph->current_layout.geometric_features) {
        memcpy(features_out, ph->current_layout.geometric_features, copy_dim * sizeof(float));
    }

    return (int)copy_dim;
}

float parahipp_get_openness(const nimcp_parahippocampal_t* ph) {
    if (!ph) return 0.0f;
    return ph->current_layout.openness;
}

float parahipp_get_navigability(const nimcp_parahippocampal_t* ph) {
    if (!ph) return 0.0f;
    return ph->current_layout.navigability;
}

/*=============================================================================
 * CONTEXT API
 *===========================================================================*/

int parahipp_get_current_context(const nimcp_parahippocampal_t* ph,
    float* context_out, uint32_t max_dim) {

    if (!ph || !context_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_get_navigability: required parameter is NULL (ph, context_out)");
        return -1;
    }

    uint32_t copy_dim = fminf(ph->current_context_dim, max_dim);
    memcpy(context_out, ph->current_context, copy_dim * sizeof(float));

    return (int)copy_dim;
}

int parahipp_set_context(nimcp_parahippocampal_t* ph,
    const float* context, uint32_t dim) {

    if (!ph || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_get_navigability: required parameter is NULL (ph, context)");
        return -1;
    }

    ph->status = PARAHIPP_STATUS_CONTEXT_BINDING;

    /* Detect context change */
    float old_context_norm = 0.0f;
    for (uint32_t i = 0; i < ph->current_context_dim; i++) {
        old_context_norm += ph->current_context[i] * ph->current_context[i];
    }

    if (old_context_norm > 1e-6f) {
        float similarity = compute_cosine_similarity(
            context, ph->current_context, fminf(dim, ph->current_context_dim));
        if (similarity < 0.8f) {
            ph->context_switches++;
        }
    }

    /* Update context */
    uint32_t copy_dim = fminf(dim, ph->current_context_dim);
    memcpy(ph->current_context, context, copy_dim * sizeof(float));

    /* Update context cells */
    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        memcpy(ph->context_cells[i].context_vector, context,
               fminf(dim, ph->context_cells[i].context_dim) * sizeof(float));
        ph->context_cells[i].binding_strength = 1.0f;
        ph->context_cells[i].activation = 1.0f;
    }

    ph->status = PARAHIPP_STATUS_READY;
    return 0;
}

bool parahipp_detect_context_change(const nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_detect_context_change: ph is NULL");
        return false;
    }

    float total_change = 0.0f;
    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        total_change += ph->context_cells[i].change_detection;
    }

    return (total_change / ph->num_context_cells) > 0.3f;
}

float parahipp_get_context_stability(const nimcp_parahippocampal_t* ph) {
    if (!ph) return 0.0f;

    float total_stability = 0.0f;
    for (uint32_t i = 0; i < ph->num_context_cells; i++) {
        total_stability += ph->context_cells[i].temporal_stability;
    }

    return total_stability / ph->num_context_cells;
}

context_state_t parahipp_get_context_state(const nimcp_parahippocampal_t* ph) {
    if (!ph) return CONTEXT_STATE_NOVEL;

    float stability = parahipp_get_context_stability(ph);

    if (stability > 0.8f) return CONTEXT_STATE_STABLE;
    if (stability > 0.5f) return CONTEXT_STATE_FAMILIAR;
    if (parahipp_detect_context_change(ph)) return CONTEXT_STATE_CHANGED;
    if (stability < 0.2f) return CONTEXT_STATE_NOVEL;

    return CONTEXT_STATE_TRANSITIONING;
}

int parahipp_bind_context_to_scene(nimcp_parahippocampal_t* ph, uint32_t scene_id) {
    if (!ph || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "parahipp_bind_context_to_scene: ph is NULL");
        return -1;
    }

    nimcp_stored_scene_t* scene = &ph->stored_scenes[scene_id];
    if (scene->scene_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_bind_context_to_scene: validation failed");
        return -1;
    }

    /* Copy current context to scene */
    if (scene->context_vector) {
        memcpy(scene->context_vector, ph->current_context,
               fminf(scene->context_dim, ph->current_context_dim) * sizeof(float));
        scene->context_stability = parahipp_get_context_stability(ph);
    }

    return 0;
}

/*=============================================================================
 * LANDMARK API
 *===========================================================================*/

int parahipp_add_landmark(nimcp_parahippocampal_t* ph,
    const float* visual_features, uint32_t feature_dim,
    const float* position, const char* name, uint32_t* landmark_id_out) {

    if (!ph || !visual_features || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bind_context_to_scene: required parameter is NULL (ph, visual_features, position)");
        return -1;
    }

    /* Find empty slot */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < ph->max_landmarks; i++) {
        if (ph->stored_landmarks[i].landmark_id == UINT32_MAX) {
            slot = i;
            break;
        }
    }

    if (slot == UINT32_MAX) {
        ph->last_error = PARAHIPP_ERROR_MEMORY_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_bind_context_to_scene: validation failed");
        return -1;
    }

    nimcp_stored_landmark_t* lm = &ph->stored_landmarks[slot];
    lm->landmark_id = slot;

    if (name) {
        strncpy(lm->name, name, sizeof(lm->name) - 1);
    }

    memcpy(lm->position, position, 3 * sizeof(float));

    lm->feature_dim = feature_dim;
    lm->visual_features = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!lm->visual_features) {
        lm->landmark_id = UINT32_MAX;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parahipp_bind_context_to_scene: lm->visual_features is NULL");
        return -1;
    }
    memcpy(lm->visual_features, visual_features, feature_dim * sizeof(float));

    lm->salience = 1.0f;
    lm->reliability = 0.5f;
    lm->first_seen_ms = get_current_time_ms();
    lm->last_seen_ms = lm->first_seen_ms;
    lm->encounter_count = 1;

    ph->num_stored_landmarks++;

    if (landmark_id_out) {
        *landmark_id_out = lm->landmark_id;
    }

    return 0;
}

int parahipp_recognize_landmarks(nimcp_parahippocampal_t* ph,
    const float* visual_features, uint32_t feature_dim,
    uint32_t* landmark_ids, float* confidences,
    uint32_t max_landmarks, uint32_t* num_recognized) {

    if (!ph || !visual_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bind_context_to_scene: required parameter is NULL (ph, visual_features)");
        return -1;
    }

    ph->status = PARAHIPP_STATUS_LANDMARK_PROCESSING;

    uint32_t found = 0;
    for (uint32_t i = 0; i < ph->max_landmarks && found < max_landmarks; i++) {
        if (ph->stored_landmarks[i].landmark_id == UINT32_MAX) continue;

        nimcp_stored_landmark_t* lm = &ph->stored_landmarks[i];
        if (!lm->visual_features || lm->feature_dim != feature_dim) continue;

        float similarity = compute_cosine_similarity(
            visual_features, lm->visual_features, feature_dim);

        if (similarity > 0.5f) {
            if (landmark_ids) landmark_ids[found] = lm->landmark_id;
            if (confidences) confidences[found] = similarity;
            found++;
        }
    }

    if (num_recognized) *num_recognized = found;

    ph->status = PARAHIPP_STATUS_READY;
    return 0;
}

const nimcp_stored_landmark_t* parahipp_get_landmark(const nimcp_parahippocampal_t* ph,
    uint32_t landmark_id) {

    if (!ph || landmark_id >= ph->max_landmarks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "parahipp_bind_context_to_scene: ph is NULL");
        return NULL;
    }
    if (ph->stored_landmarks[landmark_id].landmark_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bind_context_to_scene: validation failed");
        return NULL;
    }
    return &ph->stored_landmarks[landmark_id];
}

int parahipp_update_landmark_cells(nimcp_parahippocampal_t* ph,
    const float* current_position) {

    if (!ph || !current_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bind_context_to_scene: required parameter is NULL (ph, current_position)");
        return -1;
    }

    for (uint32_t i = 0; i < ph->num_landmark_cells; i++) {
        if (ph->landmark_cells[i].landmark_id == UINT32_MAX) {
            ph->landmark_cells[i].activation = 0.0f;
            continue;
        }

        uint32_t lm_id = ph->landmark_cells[i].landmark_id;
        if (lm_id >= ph->max_landmarks ||
            ph->stored_landmarks[lm_id].landmark_id == UINT32_MAX) {
            ph->landmark_cells[i].activation = 0.0f;
            continue;
        }

        nimcp_stored_landmark_t* lm = &ph->stored_landmarks[lm_id];

        /* Compute distance and bearing */
        float dx = lm->position[0] - current_position[0];
        float dy = lm->position[1] - current_position[1];
        float distance = sqrtf(dx * dx + dy * dy);
        float bearing = atan2f(dy, dx);

        ph->landmark_cells[i].distance = distance;
        ph->landmark_cells[i].bearing = bearing;

        /* Activation based on visibility */
        float visibility = fmaxf(0.0f, 1.0f - distance / lm->visibility_range);
        ph->landmark_cells[i].activation = visibility * lm->salience;
    }

    return 0;
}

float parahipp_get_landmark_bearing(const nimcp_parahippocampal_t* ph,
    uint32_t landmark_id, const float* from_position) {

    if (!ph || !from_position || landmark_id >= ph->max_landmarks) return 0.0f;

    const nimcp_stored_landmark_t* lm = &ph->stored_landmarks[landmark_id];
    if (lm->landmark_id == UINT32_MAX) return 0.0f;

    float dx = lm->position[0] - from_position[0];
    float dy = lm->position[1] - from_position[1];

    return atan2f(dy, dx);
}

/*=============================================================================
 * SCENE-OBJECT BINDING API
 *===========================================================================*/

int parahipp_bind_objects_to_scene(nimcp_parahippocampal_t* ph,
    uint32_t scene_id, const uint32_t* object_ids, uint32_t num_objects) {

    if (!ph || !object_ids || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bind_context_to_scene: required parameter is NULL (ph, object_ids)");
        return -1;
    }

    nimcp_stored_scene_t* scene = &ph->stored_scenes[scene_id];
    if (scene->scene_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_bind_context_to_scene: validation failed");
        return -1;
    }

    /* Free old objects */
    nimcp_free(scene->object_ids);

    scene->object_ids = (uint32_t*)nimcp_malloc(num_objects * sizeof(uint32_t));
    if (!scene->object_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parahipp_bind_context_to_scene: scene->object_ids is NULL");
        return -1;
    }

    memcpy(scene->object_ids, object_ids, num_objects * sizeof(uint32_t));
    scene->num_objects = num_objects;

    return 0;
}

int parahipp_get_scene_objects(const nimcp_parahippocampal_t* ph,
    uint32_t scene_id, uint32_t* object_ids, uint32_t max_objects,
    uint32_t* num_objects) {

    if (!ph || scene_id >= ph->max_stored_scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "parahipp_bind_context_to_scene: ph is NULL");
        return -1;
    }

    const nimcp_stored_scene_t* scene = &ph->stored_scenes[scene_id];
    if (scene->scene_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_bind_context_to_scene: validation failed");
        return -1;
    }

    uint32_t copy_count = fminf(scene->num_objects, max_objects);
    if (object_ids && scene->object_ids) {
        memcpy(object_ids, scene->object_ids, copy_count * sizeof(uint32_t));
    }

    if (num_objects) *num_objects = copy_count;
    return 0;
}

int parahipp_find_scenes_with_object(const nimcp_parahippocampal_t* ph,
    uint32_t object_id, uint32_t* scene_ids, uint32_t max_scenes,
    uint32_t* num_scenes) {

    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: ph is NULL");
        return -1;
    }

    uint32_t found = 0;
    for (uint32_t i = 0; i < ph->max_stored_scenes && found < max_scenes; i++) {
        const nimcp_stored_scene_t* scene = &ph->stored_scenes[i];
        if (scene->scene_id == UINT32_MAX) continue;

        for (uint32_t j = 0; j < scene->num_objects; j++) {
            if (scene->object_ids && scene->object_ids[j] == object_id) {
                if (scene_ids) scene_ids[found] = scene->scene_id;
                found++;
                break;
            }
        }
    }

    if (num_scenes) *num_scenes = found;
    return 0;
}

/*=============================================================================
 * BRIDGE INTEGRATION API
 *===========================================================================*/

int parahipp_send_to_entorhinal(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_send_to_entorhinal: ph is NULL");
        return -1;
    }
    if (!ph->entorhinal_bridge.entorhinal) return 0;
    ph->entorhinal_bridge.items_transferred++;
    return 0;
}

int parahipp_receive_from_entorhinal(nimcp_parahippocampal_t* ph,
    const float* grid_input, uint32_t dim) {
    if (!ph || !grid_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_send_to_entorhinal: required parameter is NULL (ph, grid_input)");
        return -1;
    }
    (void)dim;
    return 0;
}

int parahipp_send_to_perirhinal(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_send_to_perirhinal: ph is NULL");
        return -1;
    }
    if (!ph->perirhinal_bridge.perirhinal) return 0;
    ph->perirhinal_bridge.items_transferred++;
    return 0;
}

int parahipp_receive_from_perirhinal(nimcp_parahippocampal_t* ph,
    const uint32_t* object_ids, uint32_t num_objects) {
    if (!ph || !object_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_send_to_perirhinal: required parameter is NULL (ph, object_ids)");
        return -1;
    }
    (void)num_objects;
    return 0;
}

int parahipp_sync_entorhinal(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_sync_entorhinal: ph is NULL");
        return -1;
    }
    return 0;
}

int parahipp_sync_perirhinal(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_sync_perirhinal: ph is NULL");
        return -1;
    }
    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

int parahipp_process_incoming(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_process_incoming: ph is NULL");
        return -1;
    }

    if (ph->perception_bridge.perception && ph->perception_bridge.visual_input) {
        parahipp_process_visual_input(ph,
            ph->perception_bridge.visual_input,
            ph->perception_bridge.visual_dim);
    }

    return 0;
}

int parahipp_send_outgoing(nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_send_outgoing: ph is NULL");
        return -1;
    }

    parahipp_send_to_entorhinal(ph);
    parahipp_send_to_perirhinal(ph);

    if (ph->cognitive_bridge.hub) {
        ph->cognitive_bridge.cognitive_events_sent++;
    }

    return 0;
}

int parahipp_bidirectional_update(nimcp_parahippocampal_t* ph, float dt) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bidirectional_update: ph is NULL");
        return -1;
    }

    parahipp_process_incoming(ph);
    parahipp_update(ph, dt);
    parahipp_send_outgoing(ph);

    return 0;
}

int parahipp_process_visual_input(nimcp_parahippocampal_t* ph,
    const float* visual_features, uint32_t feature_dim) {

    if (!ph || !visual_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_bidirectional_update: required parameter is NULL (ph, visual_features)");
        return -1;
    }

    /* Store current input */
    if (feature_dim != ph->current_input_dim) {
        float* new_input = (float*)nimcp_realloc(ph->current_scene_input,
            feature_dim * sizeof(float));
        if (!new_input) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parahipp_bidirectional_update: new_input is NULL");
            return -1;
        }
        ph->current_scene_input = new_input;
        ph->current_input_dim = feature_dim;
    }
    memcpy(ph->current_scene_input, visual_features, feature_dim * sizeof(float));

    /* Update scene cells */
    for (uint32_t i = 0; i < ph->num_scene_cells; i++) {
        float activation = 0.0f;
        if (ph->scene_cells[i].scene_weights) {
            for (uint32_t j = 0; j < feature_dim &&
                 j < ph->scene_cells[i].scene_dim; j++) {
                activation += visual_features[j] * ph->scene_cells[i].scene_weights[j];
            }
        }
        ph->scene_cells[i].activation = 1.0f / (1.0f + expf(-activation));
    }

    return 0;
}

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

int parahipp_init_entorhinal_bridge(nimcp_parahippocampal_t* ph, nimcp_entorhinal_t* ec) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_entorhinal_bridge: ph is NULL");
        return -1;
    }
    ph->entorhinal_bridge.entorhinal = ec;
    ph->entorhinal_bridge.grid_cell_input_weight = 0.5f;
    ph->entorhinal_bridge.spatial_context_weight = 0.6f;
    return 0;
}

int parahipp_init_perirhinal_bridge(nimcp_parahippocampal_t* ph, nimcp_perirhinal_t* pr) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_perirhinal_bridge: ph is NULL");
        return -1;
    }
    ph->perirhinal_bridge.perirhinal = pr;
    ph->perirhinal_bridge.object_context_weight = 0.5f;
    ph->perirhinal_bridge.scene_object_binding = 0.7f;
    return 0;
}

int parahipp_init_security_bridge(nimcp_parahippocampal_t* ph, nimcp_security_context_t* ctx, nimcp_access_control_t* ac) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_security_bridge: ph is NULL");
        return -1;
    }
    ph->security_bridge.security_ctx = ctx;
    ph->security_bridge.access_control = ac;
    ph->security_bridge.access_level = 1;
    return 0;
}

int parahipp_init_immune_bridge(nimcp_parahippocampal_t* ph, brain_immune_system_t* immune) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_immune_bridge: ph is NULL");
        return -1;
    }
    ph->immune_bridge.immune = immune;
    ph->immune_bridge.health_score = 1.0f;
    return 0;
}

int parahipp_init_bio_async_bridge(nimcp_parahippocampal_t* ph, nimcp_bio_router_t* router) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_bio_async_bridge: ph is NULL");
        return -1;
    }
    ph->bio_async_bridge.router = router;
    return 0;
}

int parahipp_init_snn_bridge(nimcp_parahippocampal_t* ph, nimcp_snn_network_t* snn) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_snn_bridge: ph is NULL");
        return -1;
    }
    ph->snn_bridge.snn = snn;
    return 0;
}

int parahipp_init_plasticity_bridge(nimcp_parahippocampal_t* ph, nimcp_plasticity_manager_t* plasticity, nimcp_stdp_rule_t* stdp) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_plasticity_bridge: ph is NULL");
        return -1;
    }
    ph->plasticity_bridge.plasticity = plasticity;
    ph->plasticity_bridge.stdp_rule = stdp;
    ph->plasticity_bridge.learning_rate = ph->config.learning_rate;
    return 0;
}

int parahipp_init_cognitive_bridge(nimcp_parahippocampal_t* ph, working_memory_t* wm, attention_system_t* attention, cognitive_integration_hub_t* hub) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_cognitive_bridge: ph is NULL");
        return -1;
    }
    ph->cognitive_bridge.working_memory = wm;
    ph->cognitive_bridge.attention = attention;
    ph->cognitive_bridge.hub = hub;
    return 0;
}

int parahipp_init_training_bridge(nimcp_parahippocampal_t* ph, nimcp_training_context_t* ctx) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_training_bridge: ph is NULL");
        return -1;
    }
    ph->training_bridge.training_ctx = ctx;
    return 0;
}

int parahipp_init_substrate_bridge(nimcp_parahippocampal_t* ph, nimcp_neural_substrate_t* substrate) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_substrate_bridge: ph is NULL");
        return -1;
    }
    ph->substrate_bridge.substrate = substrate;
    ph->substrate_bridge.atp_level = 1.0f;
    ph->substrate_bridge.oxygen_level = 1.0f;
    ph->substrate_bridge.glucose_level = 1.0f;
    return 0;
}

int parahipp_init_resonance_bridge(nimcp_parahippocampal_t* ph, nimcp_prime_resonance_t* resonance) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_resonance_bridge: ph is NULL");
        return -1;
    }
    ph->resonance_bridge.resonance = resonance;
    return 0;
}

int parahipp_init_thalamic_bridge(nimcp_parahippocampal_t* ph, thalamus_adapter_t* thalamus) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_thalamic_bridge: ph is NULL");
        return -1;
    }
    ph->thalamic_bridge.thalamus = thalamus;
    ph->thalamic_bridge.relay_gain = 1.0f;
    return 0;
}

int parahipp_init_hippocampus_bridge(nimcp_parahippocampal_t* ph, hippocampus_adapter_t* hippocampus) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_hippocampus_bridge: ph is NULL");
        return -1;
    }
    ph->hippocampus_bridge.hippocampus = hippocampus;
    ph->hippocampus_bridge.place_cell_coupling = 0.8f;
    return 0;
}

int parahipp_init_perception_bridge(nimcp_parahippocampal_t* ph, nimcp_perception_layer_t* perception) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_perception_bridge: ph is NULL");
        return -1;
    }
    ph->perception_bridge.perception = perception;
    return 0;
}

int parahipp_init_omni_bridge(nimcp_parahippocampal_t* ph, nimcp_omnidirectional_system_t* omni) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_omni_bridge: ph is NULL");
        return -1;
    }
    ph->omni_bridge.omni = omni;
    return 0;
}

int parahipp_init_hypothalamus_bridge(nimcp_parahippocampal_t* ph, hypothalamus_adapter_t* hypothalamus) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_hypothalamus_bridge: ph is NULL");
        return -1;
    }
    ph->hypothalamus_bridge.hypothalamus = hypothalamus;
    return 0;
}

int parahipp_init_all_bridges(nimcp_parahippocampal_t* ph, nimcp_brain_t* brain) {
    if (!ph || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_init_all_bridges: required parameter is NULL (ph, brain)");
        return -1;
    }
    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

parahipp_status_t parahipp_get_status(const nimcp_parahippocampal_t* ph) {
    if (!ph) return PARAHIPP_STATUS_ERROR;
    return ph->status;
}

parahipp_error_t parahipp_get_last_error(const nimcp_parahippocampal_t* ph) {
    if (!ph) return PARAHIPP_ERROR_INTERNAL;
    return ph->last_error;
}

const char* parahipp_error_string(parahipp_error_t error) {
    switch (error) {
        case PARAHIPP_ERROR_NONE: return "No error";
        case PARAHIPP_ERROR_INVALID_INPUT: return "Invalid input";
        case PARAHIPP_ERROR_SCENE_NOT_FOUND: return "Scene not found";
        case PARAHIPP_ERROR_MEMORY_FULL: return "Memory full";
        case PARAHIPP_ERROR_ENCODING_FAILED: return "Encoding failed";
        case PARAHIPP_ERROR_RECOGNITION_FAILED: return "Recognition failed";
        case PARAHIPP_ERROR_SECURITY_VIOLATION: return "Security violation";
        case PARAHIPP_ERROR_IMMUNE_REJECTION: return "Immune rejection";
        case PARAHIPP_ERROR_SUBSTRATE_DEPLETED: return "Substrate depleted";
        case PARAHIPP_ERROR_SYNC_FAILURE: return "Sync failure";
        case PARAHIPP_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case PARAHIPP_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* parahipp_status_string(parahipp_status_t status) {
    switch (status) {
        case PARAHIPP_STATUS_IDLE: return "Idle";
        case PARAHIPP_STATUS_SCENE_ENCODING: return "Encoding scene";
        case PARAHIPP_STATUS_SCENE_RECOGNIZING: return "Recognizing scene";
        case PARAHIPP_STATUS_LAYOUT_PROCESSING: return "Processing layout";
        case PARAHIPP_STATUS_CONTEXT_BINDING: return "Binding context";
        case PARAHIPP_STATUS_LANDMARK_PROCESSING: return "Processing landmarks";
        case PARAHIPP_STATUS_CONSOLIDATING: return "Consolidating";
        case PARAHIPP_STATUS_READY: return "Ready";
        case PARAHIPP_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

int parahipp_get_stats(const nimcp_parahippocampal_t* ph, parahipp_stats_t* stats) {
    if (!ph || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_get_stats: required parameter is NULL (ph, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(parahipp_stats_t));

    stats->updates_processed = ph->updates_processed;
    stats->scenes_encoded = ph->scenes_encoded;
    stats->scenes_recognized = ph->scenes_recognized;
    stats->context_switches = ph->context_switches;
    stats->total_stored_scenes = ph->num_stored_scenes;
    stats->total_stored_landmarks = ph->num_stored_landmarks;
    stats->mean_recognition_confidence = ph->mean_recognition_confidence;

    stats->memory_utilization = (float)ph->num_stored_scenes / (float)ph->max_stored_scenes;

    /* Calculate mean activations */
    float total_place = 0.0f, total_scene = 0.0f;
    uint32_t active_place = 0;

    for (uint32_t i = 0; i < ph->num_place_cells; i++) {
        total_place += ph->place_cells[i].activation;
        if (ph->place_cells[i].activation > 0.1f) active_place++;
    }

    for (uint32_t i = 0; i < ph->num_scene_cells; i++) {
        total_scene += ph->scene_cells[i].activation;
    }

    stats->mean_place_activation = total_place / ph->num_place_cells;
    stats->active_place_cells = active_place;
    stats->mean_scene_activation = total_scene / ph->num_scene_cells;
    stats->mean_context_stability = parahipp_get_context_stability(ph);

    return 0;
}

int parahipp_get_config(const nimcp_parahippocampal_t* ph, parahipp_config_t* config) {
    if (!ph || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_get_config: required parameter is NULL (ph, config)");
        return -1;
    }
    *config = ph->config;
    return 0;
}

float parahipp_get_health_status(const nimcp_parahippocampal_t* ph) {
    if (!ph) return 0.0f;

    float health = 1.0f;

    float utilization = (float)ph->num_stored_scenes / (float)ph->max_stored_scenes;
    if (utilization > 0.9f) health *= 0.8f;

    if (ph->status == PARAHIPP_STATUS_ERROR) health *= 0.5f;

    return health;
}

int parahipp_log_diagnostics(const nimcp_parahippocampal_t* ph) {
    if (!ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_log_diagnostics: ph is NULL");
        return -1;
    }
    return 0;
}

size_t parahipp_get_place_cell_activity(const nimcp_parahippocampal_t* ph,
    float* activity, size_t max_cells) {

    if (!ph || !activity) return 0;

    size_t count = (ph->num_place_cells < max_cells) ? ph->num_place_cells : max_cells;
    for (size_t i = 0; i < count; i++) {
        activity[i] = ph->place_cells[i].activation;
    }

    return count;
}

size_t parahipp_get_scene_cell_activity(const nimcp_parahippocampal_t* ph,
    float* activity, size_t max_cells) {

    if (!ph || !activity) return 0;

    size_t count = (ph->num_scene_cells < max_cells) ? ph->num_scene_cells : max_cells;
    for (size_t i = 0; i < count; i++) {
        activity[i] = ph->scene_cells[i].activation;
    }

    return count;
}

size_t parahipp_get_context_cell_activity(const nimcp_parahippocampal_t* ph,
    float* activity, size_t max_cells) {

    if (!ph || !activity) return 0;

    size_t count = (ph->num_context_cells < max_cells) ? ph->num_context_cells : max_cells;
    for (size_t i = 0; i < count; i++) {
        activity[i] = ph->context_cells[i].activation;
    }

    return count;
}

/*=============================================================================
 * SERIALIZATION API
 *===========================================================================*/

int parahipp_serialize(const nimcp_parahippocampal_t* ph,
    uint8_t* buffer, size_t buffer_size, size_t* bytes_written) {

    if (!ph || !buffer || !bytes_written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_log_diagnostics: required parameter is NULL (ph, buffer, bytes_written)");
        return -1;
    }

    size_t needed = sizeof(parahipp_config_t) + sizeof(uint64_t) * 4;
    if (buffer_size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_log_diagnostics: validation failed");
        return -1;
    }

    size_t offset = 0;
    memcpy(buffer + offset, &ph->config, sizeof(parahipp_config_t));
    offset += sizeof(parahipp_config_t);

    memcpy(buffer + offset, &ph->updates_processed, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &ph->scenes_encoded, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &ph->scenes_recognized, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(buffer + offset, &ph->num_stored_scenes, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    *bytes_written = offset;
    return 0;
}

int parahipp_deserialize(nimcp_parahippocampal_t* ph,
    const uint8_t* buffer, size_t buffer_size) {

    if (!ph || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parahipp_log_diagnostics: required parameter is NULL (ph, buffer)");
        return -1;
    }

    size_t needed = sizeof(parahipp_config_t) + sizeof(uint64_t) * 4;
    if (buffer_size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parahipp_log_diagnostics: validation failed");
        return -1;
    }

    size_t offset = 0;
    memcpy(&ph->config, buffer + offset, sizeof(parahipp_config_t));
    offset += sizeof(parahipp_config_t);

    memcpy(&ph->updates_processed, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(&ph->scenes_encoded, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    memcpy(&ph->scenes_recognized, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    return 0;
}

size_t parahipp_get_serialization_size(const nimcp_parahippocampal_t* ph) {
    if (!ph) return 0;
    return sizeof(parahipp_config_t) + sizeof(uint64_t) * 4;
}
