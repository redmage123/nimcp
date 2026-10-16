/**
 * @file nimcp_occipital_quantum_bridge.c
 * @brief Quantum Occipital Bridge Implementation
 *
 * Integrates quantum algorithms with Occipital Cortex for
 * optimized visual search, feature binding, scene segmentation,
 * and motion integration.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct occipital_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* occipital;                      /**< Occipital adapter handle */
    occipital_quantum_config_t config;    /**< Configuration */
    qreason_t quantum_reasoner;           /**< Quantum reasoning engine */
    occipital_quantum_stats_t stats;      /**< Statistics */

    /* Candidate tracking */
    quantum_search_candidate_t* search_candidates;
    quantum_binding_hypothesis_t* binding_hypotheses;
    quantum_segment_candidate_t* segment_candidates;
    quantum_motion_candidate_t* motion_candidates;
    uint32_t max_candidates;

    /* Feature binding scratch space */
    uint32_t* binding_feature_ids;
    uint32_t max_binding_features;

    /* RNG state */
    uint32_t rng_state;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint32_t quantum_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float quantum_randf(uint32_t* state) {
    return (float)quantum_rand(state) / 32767.0f;
}

static float color_distance(float h1, float s1, float h2, float s2) {
    /* Simple hue-saturation distance */
    float dh = fabsf(h1 - h2);
    if (dh > 180.0f) dh = 360.0f - dh;
    dh /= 180.0f;  /* Normalize to 0-1 */
    float ds = fabsf(s1 - s2);
    return sqrtf(dh * dh + ds * ds);
}

static float orientation_distance(float o1, float o2) {
    float d = fabsf(o1 - o2);
    /* Orientation is periodic with period pi */
    while (d > NIMCP_PI_F / 2.0f) d -= NIMCP_PI_F;
    return fabsf(d) / (NIMCP_PI_F / 2.0f);
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

occipital_quantum_config_t occipital_quantum_default_config(void) {
    return (occipital_quantum_config_t){
        .enabled = true,
        .visual_search_depth = 1024,
        .binding_alternatives = 16,
        .max_grover_iterations = 15,
        .min_detection_confidence = 0.5f,
        .enable_interference = true,
        .use_superposition = true,
        .seed = 42
    };
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

occipital_quantum_bridge_t* occipital_quantum_bridge_create(
    void* occipital,
    const occipital_quantum_config_t* config
) {
    occipital_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(occipital_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (bridge_base_init(&bridge->base, 0, "occipital_quantum") != 0) { nimcp_free(bridge); return NULL; }

    bridge->occipital = occipital;
    bridge->config = config ? *config : occipital_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_detection_confidence;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate candidate arrays */
    bridge->max_candidates = bridge->config.binding_alternatives;

    bridge->search_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_search_candidate_t));
    bridge->binding_hypotheses = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_binding_hypothesis_t));
    bridge->segment_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_segment_candidate_t));
    bridge->motion_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_motion_candidate_t));

    if (!bridge->search_candidates || !bridge->binding_hypotheses ||
        !bridge->segment_candidates || !bridge->motion_candidates) {
        occipital_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_quantum_bridge_create: operation failed");
        return NULL;
    }

    /* Allocate binding scratch space */
    bridge->max_binding_features = 32;
    bridge->binding_feature_ids = nimcp_calloc(
        bridge->max_binding_features * bridge->max_candidates, sizeof(uint32_t));
    if (!bridge->binding_feature_ids) {
        occipital_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_quantum_bridge_create: bridge->binding_feature_ids is NULL");
        return NULL;
    }

    /* Initialize hypotheses feature ID pointers */
    for (uint32_t i = 0; i < bridge->max_candidates; i++) {
        bridge->binding_hypotheses[i].feature_ids =
            &bridge->binding_feature_ids[i * bridge->max_binding_features];
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void occipital_quantum_bridge_destroy(occipital_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_quantum");

    if (bridge->search_candidates) nimcp_free(bridge->search_candidates);
    if (bridge->binding_hypotheses) nimcp_free(bridge->binding_hypotheses);
    if (bridge->segment_candidates) nimcp_free(bridge->segment_candidates);
    if (bridge->motion_candidates) nimcp_free(bridge->motion_candidates);
    if (bridge->binding_feature_ids) nimcp_free(bridge->binding_feature_ids);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

bool occipital_quantum_bridge_is_enabled(const occipital_quantum_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->config.enabled;
}

void occipital_quantum_bridge_set_enabled(occipital_quantum_bridge_t* bridge, bool enabled) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_bridge_set_enabled: bridge is NULL");
        return;
    }
    bridge->config.enabled = enabled;
}

/*=============================================================================
 * VISUAL SEARCH API
 *===========================================================================*/

int occipital_quantum_visual_search(
    occipital_quantum_bridge_t* bridge,
    const visual_search_target_t* target,
    uint32_t num_locations,
    quantum_search_result_t* result
) {
    if (!bridge || !target || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_visual_search: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for visual search problem */
    qreason_cnf_t search_cnf = {0};
    search_cnf.n_variables = (num_locations < QREASON_MAX_VARIABLES) ?
                             num_locations : QREASON_MAX_VARIABLES;

    /* Clause: at least one location contains target */
    search_cnf.n_clauses = 1;
    search_cnf.clauses[0].n_literals = (search_cnf.n_variables < QREASON_MAX_LITERALS) ?
                                       search_cnf.n_variables : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < search_cnf.clauses[0].n_literals; i++) {
        search_cnf.clauses[0].literals[i].variable = i;
        search_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum Grover search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &search_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_visual_search: validation failed");
        return -1;
    }

    /* Generate search candidates */
    uint32_t num_candidates = (bridge->max_candidates < search_cnf.n_variables) ?
                              bridge->max_candidates : search_cnf.n_variables;

    quantum_search_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_search_candidate_t* cand = &bridge->search_candidates[i];

        cand->location_id = i;
        /* Distribute locations across visual field */
        uint32_t grid_size = (uint32_t)sqrtf((float)num_locations) + 1;
        cand->x = (float)(i % grid_size) / (float)grid_size;
        cand->y = (float)(i / grid_size) / (float)grid_size;
        cand->amplitude = qresult.confidences[i];

        /* Compute feature match based on target specification */
        float feature_match = 1.0f;
        if (target->search_by_color) {
            float loc_hue = quantum_randf(&bridge->rng_state) * 360.0f;
            float loc_sat = quantum_randf(&bridge->rng_state);
            float color_dist = color_distance(target->target_color_h, target->target_color_s,
                                               loc_hue, loc_sat);
            feature_match *= 1.0f - color_dist;
        }
        if (target->search_by_orientation) {
            float loc_ori = quantum_randf(&bridge->rng_state) * NIMCP_PI_F;
            float ori_dist = orientation_distance(target->target_orientation, loc_ori);
            feature_match *= 1.0f - ori_dist;
        }
        if (target->search_by_size) {
            float loc_size = quantum_randf(&bridge->rng_state);
            float size_dist = fabsf(target->target_size - loc_size);
            feature_match *= 1.0f - size_dist;
        }

        cand->feature_match = feature_match;
        cand->saliency = quantum_randf(&bridge->rng_state);

        /* Conjunction search is harder - requires all features to match */
        if (target->conjunction_search) {
            cand->combined_score = cand->amplitude * 0.3f +
                                   cand->feature_match * 0.5f +
                                   cand->saliency * 0.2f;
        } else {
            /* Feature search benefits more from saliency (pop-out) */
            cand->combined_score = cand->amplitude * 0.4f +
                                   cand->feature_match * 0.4f +
                                   cand->saliency * 0.2f;
        }

        cand->is_target = (cand->combined_score > bridge->config.min_detection_confidence);

        if (cand->combined_score > best_score) {
            best_score = cand->combined_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_candidate = best;
    result->locations_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;
    result->target_found = (best && best->is_target);

    /* Compute speedup (Grover provides quadratic speedup) */
    float classical_cost = (float)num_locations;
    float quantum_cost = sqrtf((float)num_locations);
    result->search_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.visual_searches++;
    float new_speedup =
        (bridge->stats.avg_search_speedup * (bridge->stats.visual_searches - 1) +
         result->search_speedup) / bridge->stats.visual_searches;
    if (isfinite(new_speedup)) bridge->stats.avg_search_speedup = new_speedup;
    float new_sat_prob =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.visual_searches - 1) +
         result->satisfaction_probability) / bridge->stats.visual_searches;
    if (isfinite(new_sat_prob)) bridge->stats.avg_satisfaction_prob = new_sat_prob;

    if (result->target_found) {
        bridge->stats.successful_searches++;
    } else {
        bridge->stats.failed_searches++;
    }

    return 0;
}

/*=============================================================================
 * FEATURE BINDING API
 *===========================================================================*/

int occipital_quantum_feature_binding(
    occipital_quantum_bridge_t* bridge,
    const float* feature_locations,
    const uint32_t* feature_types,
    uint32_t num_features,
    quantum_binding_result_t* result
) {
    if (!bridge || !feature_locations || !feature_types || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_feature_binding: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for feature binding */
    qreason_cnf_t binding_cnf = {0};
    binding_cnf.n_variables = (bridge->config.binding_alternatives < QREASON_MAX_VARIABLES) ?
                              bridge->config.binding_alternatives : QREASON_MAX_VARIABLES;

    /* Each variable represents a binding hypothesis */
    binding_cnf.n_clauses = 1;
    binding_cnf.clauses[0].n_literals = binding_cnf.n_variables;

    for (uint32_t i = 0; i < binding_cnf.n_variables; i++) {
        binding_cnf.clauses[0].literals[i].variable = i;
        binding_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &binding_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_feature_binding: validation failed");
        return -1;
    }

    /* Generate binding hypotheses */
    quantum_binding_hypothesis_t* best = NULL;
    float best_coherence = -1.0f;

    for (uint32_t i = 0; i < binding_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_binding_hypothesis_t* hyp = &bridge->binding_hypotheses[i];

        hyp->binding_id = i;
        hyp->amplitude = qresult.confidences[i];

        /* Randomly assign features to this binding hypothesis */
        hyp->num_features = 0;
        float cx = 0.0f, cy = 0.0f;

        for (uint32_t f = 0; f < num_features && hyp->num_features < bridge->max_binding_features; f++) {
            /* Probabilistically include feature based on spatial proximity and type */
            float px = feature_locations[f * 2];
            float py = feature_locations[f * 2 + 1];

            /* Features closer to hypothesis centroid more likely to be included */
            if (quantum_randf(&bridge->rng_state) < 0.5f + hyp->amplitude * 0.3f) {
                hyp->feature_ids[hyp->num_features++] = f;
                cx += px;
                cy += py;
            }
        }

        if (hyp->num_features > 0) {
            hyp->x_centroid = cx / (float)hyp->num_features;
            hyp->y_centroid = cy / (float)hyp->num_features;
        }

        /* Compute coherence: features in the same binding should be spatially close */
        float coherence = 1.0f;
        if (hyp->num_features > 1) {
            float max_dist = 0.0f;
            for (uint32_t j = 0; j < hyp->num_features; j++) {
                uint32_t fj = hyp->feature_ids[j];
                float dx = feature_locations[fj * 2] - hyp->x_centroid;
                float dy = feature_locations[fj * 2 + 1] - hyp->y_centroid;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > max_dist) max_dist = dist;
            }
            coherence = 1.0f - max_dist;  /* More compact = more coherent */
        }

        hyp->coherence = coherence;
        hyp->probability = hyp->amplitude * hyp->coherence;

        if (hyp->probability > best_coherence && hyp->num_features > 0) {
            best_coherence = hyp->probability;
            best = hyp;
        }
    }

    /* Fill result */
    result->best_binding = best;
    result->hypotheses_evaluated = binding_cnf.n_variables;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->num_objects = best ? 1 : 0;

    /* Update statistics */
    bridge->stats.binding_operations++;
    float new_coherence =
        (bridge->stats.avg_binding_coherence * (bridge->stats.binding_operations - 1) +
         (best ? best->coherence : 0.0f)) / bridge->stats.binding_operations;
    if (isfinite(new_coherence)) bridge->stats.avg_binding_coherence = new_coherence;

    return 0;
}

/*=============================================================================
 * SCENE SEGMENTATION API
 *===========================================================================*/

int occipital_quantum_segment_scene(
    occipital_quantum_bridge_t* bridge,
    const float* edge_map,
    uint32_t width,
    uint32_t height,
    quantum_segmentation_result_t* result
) {
    if (!bridge || !edge_map || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_segment_scene: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for segmentation optimization */
    qreason_cnf_t seg_cnf = {0};
    seg_cnf.n_variables = (bridge->config.binding_alternatives < QREASON_MAX_VARIABLES) ?
                          bridge->config.binding_alternatives : QREASON_MAX_VARIABLES;

    seg_cnf.n_clauses = 1;
    seg_cnf.clauses[0].n_literals = seg_cnf.n_variables;

    for (uint32_t i = 0; i < seg_cnf.n_variables; i++) {
        seg_cnf.clauses[0].literals[i].variable = i;
        seg_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &seg_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_segment_scene: validation failed");
        return -1;
    }

    /* Generate segmentation candidates */
    quantum_segment_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < seg_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_segment_candidate_t* seg = &bridge->segment_candidates[i];

        seg->segment_id = i;
        seg->amplitude = qresult.confidences[i];
        seg->mask_width = width;
        seg->mask_height = height;

        /* Compute boundary cost using edge map */
        float boundary_cost = 0.0f;
        float region_sum = 0.0f;
        uint32_t region_count = 0;

        /* Sample edge map at random locations */
        for (uint32_t s = 0; s < 100; s++) {
            uint32_t sx = quantum_rand(&bridge->rng_state) % width;
            uint32_t sy = quantum_rand(&bridge->rng_state) % height;
            float edge_val = edge_map[sy * width + sx];
            boundary_cost += edge_val;
            region_sum += 1.0f - edge_val;
            region_count++;
        }

        seg->boundary_cost = boundary_cost / (float)region_count;
        seg->region_homogeneity = region_sum / (float)region_count;

        /* Better segmentations have low boundary cost (align with edges) and high homogeneity */
        float score = seg->amplitude * 0.4f +
                      (1.0f - seg->boundary_cost) * 0.3f +
                      seg->region_homogeneity * 0.3f;

        seg->is_figure = (seg->amplitude > 0.5f);

        if (score > best_score) {
            best_score = score;
            best = seg;
        }
    }

    /* Fill result */
    result->best_segmentation = best;
    result->segmentations_evaluated = seg_cnf.n_variables;
    result->optimization_score = best_score;

    /* Update statistics */
    bridge->stats.segmentation_operations++;

    return 0;
}

/*=============================================================================
 * MOTION INTEGRATION API
 *===========================================================================*/

int occipital_quantum_integrate_motion(
    occipital_quantum_bridge_t* bridge,
    const float* local_dx,
    const float* local_dy,
    uint32_t num_motions,
    quantum_motion_result_t* result
) {
    if (!bridge || !local_dx || !local_dy || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_integrate_motion: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enabled) return -1;
    if (num_motions == 0) return -1;

    memset(result, 0, sizeof(*result));

    /* Build CNF for motion integration */
    qreason_cnf_t motion_cnf = {0};
    motion_cnf.n_variables = (bridge->config.binding_alternatives < QREASON_MAX_VARIABLES) ?
                             bridge->config.binding_alternatives : QREASON_MAX_VARIABLES;

    motion_cnf.n_clauses = 1;
    motion_cnf.clauses[0].n_literals = motion_cnf.n_variables;

    for (uint32_t i = 0; i < motion_cnf.n_variables; i++) {
        motion_cnf.clauses[0].literals[i].variable = i;
        motion_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &motion_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_integrate_motion: validation failed");
        return -1;
    }

    /* Compute mean local motion as baseline */
    float mean_dx = 0.0f, mean_dy = 0.0f;
    for (uint32_t i = 0; i < num_motions; i++) {
        mean_dx += local_dx[i];
        mean_dy += local_dy[i];
    }
    mean_dx /= (float)num_motions;
    mean_dy /= (float)num_motions;

    /* Generate motion integration candidates */
    quantum_motion_candidate_t* best = NULL;
    float best_coherence = -1.0f;

    for (uint32_t i = 0; i < motion_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_motion_candidate_t* mot = &bridge->motion_candidates[i];

        mot->integration_id = i;
        mot->amplitude = qresult.confidences[i];

        /* Hypothesize global motion (perturbed from mean) */
        float perturbation = (quantum_randf(&bridge->rng_state) - 0.5f) * 2.0f;
        mot->global_dx = mean_dx + perturbation * mot->amplitude;
        mot->global_dy = mean_dy + perturbation * mot->amplitude;

        /* Compute residual error: how well does global motion explain local motions? */
        float residual = 0.0f;
        for (uint32_t j = 0; j < num_motions; j++) {
            float edx = local_dx[j] - mot->global_dx;
            float edy = local_dy[j] - mot->global_dy;
            residual += sqrtf(edx * edx + edy * edy);
        }
        mot->residual_error = residual / (float)num_motions;

        /* Coherence: inverse of residual error */
        mot->coherence = 1.0f / (1.0f + mot->residual_error);

        float score = mot->amplitude * 0.4f + mot->coherence * 0.6f;

        if (score > best_coherence) {
            best_coherence = score;
            best = mot;
        }
    }

    /* Fill result */
    result->best_integration = best;
    result->integrations_evaluated = motion_cnf.n_variables;
    result->coherence_score = best ? best->coherence : 0.0f;

    /* Update statistics */
    bridge->stats.motion_integrations++;

    return 0;
}

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

int occipital_quantum_get_stats(
    const occipital_quantum_bridge_t* bridge,
    occipital_quantum_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_get_stats: required parameter is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void occipital_quantum_reset_stats(occipital_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_reset_stats: bridge is NULL");
        return;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

int occipital_quantum_get_config(
    const occipital_quantum_bridge_t* bridge,
    occipital_quantum_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_quantum_get_config: required parameter is NULL");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
