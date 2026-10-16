/**
 * @file nimcp_hippocampus.c
 * @brief Hippocampus Implementation - Central Hub for Memory and Navigation
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"
#include <stddef.h>  /* for NULL */
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"


/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *============================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/geometry/nimcp_differential_geometry.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hippo, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static float randf(void) {
    static __thread unsigned int tl_seed = 0;
    if (tl_seed == 0) tl_seed = (unsigned int)(uintptr_t)&tl_seed;
    return (float)rand_r(&tl_seed) / (float)RAND_MAX;
}

static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a < 1e-9f || norm_b < 1e-9f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

static float euclidean_distance(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/**
 * @brief Riemannian distance between two points using a spatial metric
 *
 * When a Riemannian metric is provided, computes the proper geodesic distance
 * which accounts for non-Euclidean environment geometry (e.g., curved surfaces,
 * variable-scale regions). Falls back to Euclidean if metric is NULL.
 */
static float riemannian_spatial_distance(const float* a, const float* b,
                                          uint32_t dim,
                                          const riemannian_metric_t* metric) {
    if (!metric || dim > DIFFGEO_MAX_DIM) {
        return euclidean_distance(a, b, dim);
    }

    /* Compute displacement vector */
    float diff[DIFFGEO_MAX_DIM];
    for (uint32_t i = 0; i < dim; i++) {
        diff[i] = b[i] - a[i];
    }

    /* ||diff||_g = sqrt(g_ij * diff^i * diff^j) */
    return riemannian_norm(metric, diff);
}

static float gaussian_activation(float distance, float sigma) {
    if (fabsf(sigma) < 1e-10f) return 0.0f;
    return expf(-(distance * distance) / (2.0f * sigma * sigma));
}

static void normalize_vector(float* v, uint32_t dim) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) norm += v[i] * v[i];
    if (norm > 1e-9f) {
        norm = sqrtf(norm);
        for (uint32_t i = 0; i < dim; i++) v[i] /= norm;
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hippo_config_t hippo_default_config(void) {
    hippo_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_dg_cells = HIPPO_DEFAULT_DG_CELLS;
    config.num_ca3_cells = HIPPO_DEFAULT_CA3_CELLS;
    config.num_ca1_cells = HIPPO_DEFAULT_CA1_CELLS;
    config.num_subiculum_cells = HIPPO_DEFAULT_SUBICULUM_CELLS;
    config.num_place_cells = HIPPO_DEFAULT_PLACE_CELLS;
    config.max_episodes = HIPPO_MAX_EPISODES;

    config.dg_sparsity = 0.02f;
    config.ca3_recurrence_density = HIPPO_CA3_RECURRENCE_PROB;
    config.pattern_completion_threshold = 0.3f;

    config.theta_frequency = HIPPO_THETA_FREQUENCY;
    config.gamma_frequency = HIPPO_GAMMA_FREQUENCY;
    config.enable_theta_gamma_coupling = true;
    config.enable_sharp_wave_ripples = true;

    config.default_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.ltp_threshold = 0.5f;
    config.ltd_threshold = 0.3f;
    config.enable_neurogenesis = true;

    config.consolidation_rate = 0.001f;
    config.enable_sleep_replay = true;
    config.enable_awake_replay = true;

    config.enable_prime_resonance = true;
    config.enable_all_bridges = true;

    return config;
}

nimcp_hippocampus_t* hippo_create(const hippo_config_t* config) {
    hippo_config_t cfg = config ? *config : hippo_default_config();

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)nimcp_calloc(1, sizeof(nimcp_hippocampus_t));
    if (!hippo) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo is NULL");

        return NULL;

    }

    hippo->config = cfg;
    hippo->status = HIPPO_STATUS_IDLE;
    hippo->last_error = HIPPO_ERROR_NONE;

    /* Allocate DG cells */
    hippo->num_dg_cells = cfg.num_dg_cells;
    hippo->dg_cells = (nimcp_dg_cell_t*)nimcp_calloc(cfg.num_dg_cells, sizeof(nimcp_dg_cell_t));
    if (!hippo->dg_cells) goto cleanup;

    for (uint32_t i = 0; i < cfg.num_dg_cells; i++) {
        hippo->dg_cells[i].cell_id = i;
        hippo->dg_cells[i].sparsity = cfg.dg_sparsity;
        hippo->dg_cells[i].threshold = 0.5f + randf() * 0.3f;
        hippo->dg_cells[i].snn_neuron_id = i;
        hippo->dg_cells[i].spike_threshold = 0.5f;
        hippo->dg_cells[i].learning_rate = cfg.default_learning_rate;
        hippo->dg_cells[i].maturity = 1.0f;
    }

    /* Allocate CA3 cells */
    hippo->num_ca3_cells = cfg.num_ca3_cells;
    hippo->ca3_cells = (nimcp_ca3_cell_t*)nimcp_calloc(cfg.num_ca3_cells, sizeof(nimcp_ca3_cell_t));
    if (!hippo->ca3_cells) goto cleanup;

    for (uint32_t i = 0; i < cfg.num_ca3_cells; i++) {
        hippo->ca3_cells[i].cell_id = i;
        hippo->ca3_cells[i].attractor_strength = 0.5f;
        hippo->ca3_cells[i].completion_threshold = cfg.pattern_completion_threshold;
        hippo->ca3_cells[i].snn_neuron_id = cfg.num_dg_cells + i;
    }

    /* Allocate CA1 cells */
    hippo->num_ca1_cells = cfg.num_ca1_cells;
    hippo->ca1_cells = (nimcp_ca1_cell_t*)nimcp_calloc(cfg.num_ca1_cells, sizeof(nimcp_ca1_cell_t));
    if (!hippo->ca1_cells) goto cleanup;

    for (uint32_t i = 0; i < cfg.num_ca1_cells; i++) {
        hippo->ca1_cells[i].cell_id = i;
        hippo->ca1_cells[i].snn_neuron_id = cfg.num_dg_cells + cfg.num_ca3_cells + i;
        hippo->ca1_cells[i].cortical_output_weight = 0.5f;
        hippo->ca1_cells[i].subiculum_output_weight = 0.5f;
        hippo->ca1_cells[i].theta_modulation = 1.0f;  /* Default modulation so propagation works */
    }

    /* Allocate subiculum cells */
    hippo->num_subiculum_cells = cfg.num_subiculum_cells;
    hippo->subiculum_cells = (nimcp_subiculum_cell_t*)nimcp_calloc(cfg.num_subiculum_cells, sizeof(nimcp_subiculum_cell_t));
    if (!hippo->subiculum_cells) goto cleanup;

    for (uint32_t i = 0; i < cfg.num_subiculum_cells; i++) {
        hippo->subiculum_cells[i].cell_id = i;
        hippo->subiculum_cells[i].snn_neuron_id = cfg.num_dg_cells + cfg.num_ca3_cells + cfg.num_ca1_cells + i;
    }

    /* Allocate place cells */
    hippo->num_place_cells = cfg.num_place_cells;
    hippo->place_cells = (nimcp_place_cell_t*)nimcp_calloc(cfg.num_place_cells, sizeof(nimcp_place_cell_t));
    if (!hippo->place_cells) goto cleanup;

    for (uint32_t i = 0; i < cfg.num_place_cells; i++) {
        hippo->place_cells[i].cell_id = i;
        hippo->place_cells[i].place_field_radius = 5.0f + randf() * 10.0f;
        hippo->place_cells[i].peak_firing_rate = 20.0f + randf() * 30.0f;
        hippo->place_cells[i].field_stability = 0.0f;  /* Start unassigned */
        hippo->place_cells[i].remap_threshold = 0.5f;
        /* Random initial place field centers */
        for (int d = 0; d < 3; d++) {
            hippo->place_cells[i].place_field_center[d] = randf() * 100.0f;
        }
    }

    /* Allocate episodes */
    hippo->max_episodes = cfg.max_episodes;
    hippo->episodes = (nimcp_episode_t*)nimcp_calloc(cfg.max_episodes, sizeof(nimcp_episode_t));
    if (!hippo->episodes) goto cleanup;

    /* Allocate replay buffer */
    hippo->replay_buffer = (nimcp_ripple_event_t*)nimcp_calloc(HIPPO_MAX_REPLAY_BUFFER, sizeof(nimcp_ripple_event_t));
    if (!hippo->replay_buffer) goto cleanup;
    hippo->replay_buffer_size = HIPPO_MAX_REPLAY_BUFFER;

    /* Allocate activation patterns */
    hippo->dg_activation_pattern = (float*)nimcp_calloc(cfg.num_dg_cells, sizeof(float));
    hippo->ca3_activation_pattern = (float*)nimcp_calloc(cfg.num_ca3_cells, sizeof(float));
    hippo->ca1_activation_pattern = (float*)nimcp_calloc(cfg.num_ca1_cells, sizeof(float));
    hippo->subiculum_pattern = (float*)nimcp_calloc(cfg.num_subiculum_cells, sizeof(float));

    if (!hippo->dg_activation_pattern || !hippo->ca3_activation_pattern ||
        !hippo->ca1_activation_pattern || !hippo->subiculum_pattern) goto cleanup;

    /* Initialize oscillation state */
    hippo->oscillation_state = OSCILLATION_THETA;
    hippo->theta_phase = 0.0f;
    hippo->gamma_phase = 0.0f;
    hippo->theta_power = 1.0f;
    hippo->gamma_power = 0.5f;

    /* Initialize bridge defaults */
    hippo->prime_resonance_bridge.resonance_frequency = 40.0f;
    hippo->prime_resonance_bridge.memory_enhancement_factor = 1.5f;

    hippo->immune_bridge.health_score = 1.0f;
    hippo->substrate_bridge.atp_level = 1.0f;
    hippo->substrate_bridge.oxygen_saturation = 1.0f;
    hippo->substrate_bridge.glucose_level = 1.0f;

    hippo->plasticity_bridge.ltp_magnitude = 0.1f;
    hippo->plasticity_bridge.ltd_magnitude = 0.05f;
    hippo->plasticity_bridge.hebbian_learning = true;
    hippo->plasticity_bridge.stdp_enabled = true;

    hippo->creation_time = get_timestamp_ms();
    hippo->status = HIPPO_STATUS_READY;

    return hippo;

cleanup:
    hippo_destroy(hippo);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_create: operation failed");
    return NULL;
}

void hippo_destroy(nimcp_hippocampus_t* hippo) {
    if (!hippo) return;

    /* Free episode contents */
    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];
        if (ep->what_content) nimcp_free(ep->what_content);
        if (ep->where_content) nimcp_free(ep->where_content);
        if (ep->when_content) nimcp_free(ep->when_content);
        if (ep->bound_representation) nimcp_free(ep->bound_representation);
        if (ep->dg_pattern) nimcp_free(ep->dg_pattern);
        if (ep->ca3_pattern) nimcp_free(ep->ca3_pattern);
        if (ep->ca1_pattern) nimcp_free(ep->ca1_pattern);
    }

    /* Free DG cell weights */
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        if (hippo->dg_cells[i].input_weights) nimcp_free(hippo->dg_cells[i].input_weights);
        if (hippo->dg_cells[i].eligibility_traces) nimcp_free(hippo->dg_cells[i].eligibility_traces);
    }

    /* Free CA3 cell weights */
    for (uint32_t i = 0; i < hippo->num_ca3_cells; i++) {
        if (hippo->ca3_cells[i].mossy_fiber_weights) nimcp_free(hippo->ca3_cells[i].mossy_fiber_weights);
        if (hippo->ca3_cells[i].perforant_weights) nimcp_free(hippo->ca3_cells[i].perforant_weights);
        if (hippo->ca3_cells[i].recurrent_weights) nimcp_free(hippo->ca3_cells[i].recurrent_weights);
    }

    /* Free CA1 cell weights */
    for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) {
        if (hippo->ca1_cells[i].schaffer_weights) nimcp_free(hippo->ca1_cells[i].schaffer_weights);
        if (hippo->ca1_cells[i].perforant_weights) nimcp_free(hippo->ca1_cells[i].perforant_weights);
    }

    /* Free subiculum cell weights */
    for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) {
        if (hippo->subiculum_cells[i].ca1_weights) nimcp_free(hippo->subiculum_cells[i].ca1_weights);
    }

    /* Free place cell associated episodes */
    for (uint32_t i = 0; i < hippo->num_place_cells; i++) {
        if (hippo->place_cells[i].associated_episodes) nimcp_free(hippo->place_cells[i].associated_episodes);
    }

    /* Free replay buffer episode sequences */
    for (uint32_t i = 0; i < hippo->replay_buffer_size; i++) {
        if (hippo->replay_buffer[i].episode_sequence) nimcp_free(hippo->replay_buffer[i].episode_sequence);
    }

    /* Free bridge perception buffer */
    if (hippo->perception_bridge.current_percept) nimcp_free(hippo->perception_bridge.current_percept);

    /* Free entorhinal grid input */
    if (hippo->entorhinal_bridge.grid_cell_input) nimcp_free(hippo->entorhinal_bridge.grid_cell_input);

    /* Free perirhinal object representation */
    if (hippo->perirhinal_bridge.object_representation) nimcp_free(hippo->perirhinal_bridge.object_representation);

    /* Free parahippocampal representations */
    if (hippo->parahippocampal_bridge.scene_representation) nimcp_free(hippo->parahippocampal_bridge.scene_representation);
    if (hippo->parahippocampal_bridge.spatial_context) nimcp_free(hippo->parahippocampal_bridge.spatial_context);

    /* Free main arrays */
    if (hippo->dg_cells) nimcp_free(hippo->dg_cells);
    if (hippo->ca3_cells) nimcp_free(hippo->ca3_cells);
    if (hippo->ca1_cells) nimcp_free(hippo->ca1_cells);
    if (hippo->subiculum_cells) nimcp_free(hippo->subiculum_cells);
    if (hippo->place_cells) nimcp_free(hippo->place_cells);
    if (hippo->episodes) nimcp_free(hippo->episodes);
    if (hippo->replay_buffer) nimcp_free(hippo->replay_buffer);
    if (hippo->dg_activation_pattern) nimcp_free(hippo->dg_activation_pattern);
    if (hippo->ca3_activation_pattern) nimcp_free(hippo->ca3_activation_pattern);
    if (hippo->ca1_activation_pattern) nimcp_free(hippo->ca1_activation_pattern);
    if (hippo->subiculum_pattern) nimcp_free(hippo->subiculum_pattern);

    nimcp_free(hippo);
}

int hippo_reset(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_reset: hippo is NULL");
        return -1;
    }

    /* Reset cell activations */
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        hippo->dg_cells[i].activation = 0.0f;
        hippo->dg_cells[i].membrane_potential = 0.0f;
    }
    for (uint32_t i = 0; i < hippo->num_ca3_cells; i++) {
        hippo->ca3_cells[i].activation = 0.0f;
        hippo->ca3_cells[i].membrane_potential = 0.0f;
    }
    for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) {
        hippo->ca1_cells[i].activation = 0.0f;
        hippo->ca1_cells[i].membrane_potential = 0.0f;
    }
    for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) {
        hippo->subiculum_cells[i].activation = 0.0f;
        hippo->subiculum_cells[i].membrane_potential = 0.0f;
    }
    for (uint32_t i = 0; i < hippo->num_place_cells; i++) {
        hippo->place_cells[i].current_rate = 0.0f;
    }

    /* Clear activation patterns */
    memset(hippo->dg_activation_pattern, 0, hippo->num_dg_cells * sizeof(float));
    memset(hippo->ca3_activation_pattern, 0, hippo->num_ca3_cells * sizeof(float));
    memset(hippo->ca1_activation_pattern, 0, hippo->num_ca1_cells * sizeof(float));
    memset(hippo->subiculum_pattern, 0, hippo->num_subiculum_cells * sizeof(float));

    /* Clear episodes */
    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];
        if (ep->what_content) { nimcp_free(ep->what_content); ep->what_content = NULL; }
        if (ep->where_content) { nimcp_free(ep->where_content); ep->where_content = NULL; }
        if (ep->when_content) { nimcp_free(ep->when_content); ep->when_content = NULL; }
        if (ep->bound_representation) { nimcp_free(ep->bound_representation); ep->bound_representation = NULL; }
        if (ep->dg_pattern) { nimcp_free(ep->dg_pattern); ep->dg_pattern = NULL; }
        if (ep->ca3_pattern) { nimcp_free(ep->ca3_pattern); ep->ca3_pattern = NULL; }
        if (ep->ca1_pattern) { nimcp_free(ep->ca1_pattern); ep->ca1_pattern = NULL; }
    }
    hippo->num_episodes = 0;

    /* Reset oscillations */
    hippo->theta_phase = 0.0f;
    hippo->gamma_phase = 0.0f;
    hippo->oscillation_state = OSCILLATION_THETA;

    /* Reset statistics */
    hippo->updates_processed = 0;
    hippo->encodings_performed = 0;
    hippo->retrievals_performed = 0;
    hippo->replays_performed = 0;
    hippo->active_place_cells = 0;
    hippo->replay_head = 0;

    hippo->status = HIPPO_STATUS_READY;
    hippo->last_error = HIPPO_ERROR_NONE;

    return 0;
}

int hippo_update(nimcp_hippocampus_t* hippo, float dt) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_update: hippo is NULL");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of hippocampus update */
    hippo_heartbeat("hippo_update", 0.0f);

    hippo->last_update_time = get_timestamp_ms();

    /* Update oscillations */
    hippo_update_theta(hippo, dt);
    hippo_update_gamma(hippo, dt);

    /* Update place cells based on current position
     * Uses Riemannian metric if available for non-Euclidean environments */
    hippo->active_place_cells = 0;
    const riemannian_metric_t* metric = (const riemannian_metric_t*)hippo->spatial_metric;
    for (uint32_t i = 0; i < hippo->num_place_cells; i++) {
        float dist = riemannian_spatial_distance(hippo->current_position,
                                                  hippo->place_cells[i].place_field_center, 3,
                                                  metric);
        float rate = hippo->place_cells[i].peak_firing_rate *
                     gaussian_activation(dist, hippo->place_cells[i].place_field_radius);
        hippo->place_cells[i].current_rate = rate;
        if (rate > 1.0f) hippo->active_place_cells++;
    }

    /* Apply theta modulation to CA1 cells */
    float theta_mod = 0.5f + 0.5f * cosf(hippo->theta_phase);
    for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) {
        hippo->ca1_cells[i].theta_modulation = theta_mod;
    }

    /* Decay activations */
    float decay = expf(-dt * 5.0f);
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        hippo->dg_cells[i].activation *= decay;
    }
    for (uint32_t i = 0; i < hippo->num_ca3_cells; i++) {
        hippo->ca3_cells[i].activation *= decay;
    }
    for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) {
        hippo->ca1_cells[i].activation *= decay;
    }

    /* Update consolidation */
    if (hippo->config.enable_sleep_replay || hippo->config.enable_awake_replay) {
        hippo_consolidate_memories(hippo, dt);
    }

    /* Process modulation from hypothalamus */
    if (hippo->hypothalamus_bridge.initialized) {
        float stress_mod = 1.0f - hippo->hypothalamus_bridge.stress_level * 0.3f;
        /* Use local effective rate - don't permanently mutate config */
        float effective_learning_rate = hippo->config.default_learning_rate * stress_mod;
        (void)effective_learning_rate;  /* Available for future use in this scope */
    }

    /* Process immune modulation */
    if (hippo->immune_bridge.initialized && hippo->immune_bridge.neuroinflammation) {
        /* Don't permanently degrade theta_power each cycle - compute locally */
        float effective_theta_power = hippo->theta_power * 0.9f;
        (void)effective_theta_power;  /* Available for future use in this scope */
    }

    hippo->updates_processed++;
    return 0;
}

/*=============================================================================
 * EPISODIC MEMORY FUNCTIONS
 *===========================================================================*/

int hippo_encode_episode(nimcp_hippocampus_t* hippo,
                          const float* what, uint32_t what_dim,
                          const float* where, uint32_t where_dim,
                          const float* when, uint32_t when_dim,
                          float emotional_valence,
                          float emotional_arousal,
                          uint32_t* episode_id_out) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_encode_episode: hippo is NULL");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of episode encoding */
    hippo_heartbeat("hippo_encode_episode", 0.0f);

    /* Validate input - at least 'what' content is required with non-zero dimension */
    if (!what || what_dim == 0) {
        hippo->last_error = HIPPO_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippo_encode_episode: what is NULL");
        return -1;
    }

    if (hippo->num_episodes >= hippo->max_episodes) {
        hippo->last_error = HIPPO_ERROR_MEMORY_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hippo_encode_episode: capacity exceeded");
        return -1;
    }

    hippo->status = HIPPO_STATUS_ENCODING;

    uint32_t id = hippo->num_episodes;
    nimcp_episode_t* ep = &hippo->episodes[id];
    memset(ep, 0, sizeof(nimcp_episode_t));

    ep->episode_id = id;

    /* Store what content */
    if (what && what_dim > 0) {
        ep->what_content = (float*)nimcp_malloc(what_dim * sizeof(float));
        if (!ep->what_content) {
            hippo->last_error = HIPPO_ERROR_ENCODING_FAILED;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippo_encode_episode: ep->what_content alloc failed");
            return -1;
        }
        memcpy(ep->what_content, what, what_dim * sizeof(float));
        ep->what_dim = what_dim;
    }

    /* Store where content */
    if (where && where_dim > 0) {
        ep->where_content = (float*)nimcp_malloc(where_dim * sizeof(float));
        if (!ep->where_content) {
            nimcp_free(ep->what_content);
            ep->what_content = NULL;
            ep->what_dim = 0;
            hippo->last_error = HIPPO_ERROR_ENCODING_FAILED;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippo_encode_episode: ep->where_content alloc failed");
            return -1;
        }
        memcpy(ep->where_content, where, where_dim * sizeof(float));
        ep->where_dim = where_dim;
        ep->has_spatial_context = true;
        if (where_dim >= 3) {
            ep->associated_position[0] = where[0];
            ep->associated_position[1] = where[1];
            ep->associated_position[2] = where[2];
        }
    }

    /* Store when content */
    if (when && when_dim > 0) {
        ep->when_content = (float*)nimcp_malloc(when_dim * sizeof(float));
        if (!ep->when_content) {
            nimcp_free(ep->where_content);
            ep->where_content = NULL;
            ep->where_dim = 0;
            nimcp_free(ep->what_content);
            ep->what_content = NULL;
            ep->what_dim = 0;
            hippo->last_error = HIPPO_ERROR_ENCODING_FAILED;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippo_encode_episode: ep->when_content alloc failed");
            return -1;
        }
        memcpy(ep->when_content, when, when_dim * sizeof(float));
        ep->when_dim = when_dim;
    }

    /* Create bound representation */
    uint32_t bound_dim = (what_dim > 0 ? what_dim : 0) +
                         (where_dim > 0 ? where_dim : 0) +
                         (when_dim > 0 ? when_dim : 0);
    if (bound_dim > 0) {
        ep->bound_representation = (float*)nimcp_calloc(bound_dim, sizeof(float));
        if (ep->bound_representation) {
            uint32_t offset = 0;
            if (what && what_dim > 0) {
                memcpy(ep->bound_representation + offset, what, what_dim * sizeof(float));
                offset += what_dim;
            }
            if (where && where_dim > 0) {
                memcpy(ep->bound_representation + offset, where, where_dim * sizeof(float));
                offset += where_dim;
            }
            if (when && when_dim > 0) {
                memcpy(ep->bound_representation + offset, when, when_dim * sizeof(float));
            }
            ep->bound_dim = bound_dim;
        }
    }

    /* Set metadata */
    ep->emotional_valence = emotional_valence;
    ep->emotional_arousal = emotional_arousal;
    ep->encoding_timestamp = get_timestamp_ms();
    ep->recency = 1.0f;
    ep->consolidation_level = 0.0f;

    /* Calculate encoding strength based on arousal and attention */
    float base_strength = 0.5f + emotional_arousal * 0.3f;
    if (hippo->cognitive_bridge.initialized) {
        base_strength += hippo->cognitive_bridge.attention_level * 0.2f;
    }
    if (hippo->prime_resonance_bridge.initialized && hippo->prime_resonance_bridge.resonance_active) {
        base_strength *= hippo->prime_resonance_bridge.memory_enhancement_factor;
    }
    ep->encoding_strength = fminf(1.0f, base_strength);

    /* Determine episode type */
    if (where_dim > 0) {
        ep->type = EPISODE_TYPE_SPATIAL;
    } else if (fabsf(emotional_valence) > 0.5f) {
        ep->type = EPISODE_TYPE_EMOTIONAL;
    } else {
        ep->type = EPISODE_TYPE_SEMANTIC;
    }

    /* Create DG pattern via pattern separation */
    if (ep->bound_representation && ep->bound_dim > 0) {
        hippo_activate_dg(hippo, ep->bound_representation, ep->bound_dim);

        /* Store sparse DG pattern */
        uint32_t active_count = 0;
        for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
            if (hippo->dg_cells[i].activation > hippo->dg_cells[i].threshold) {
                active_count++;
            }
        }

        if (active_count > 0) {
            ep->dg_pattern = (uint32_t*)nimcp_malloc(active_count * sizeof(uint32_t));
            if (ep->dg_pattern) {
                uint32_t idx = 0;
                for (uint32_t i = 0; i < hippo->num_dg_cells && idx < active_count; i++) {
                    if (hippo->dg_cells[i].activation > hippo->dg_cells[i].threshold) {
                        ep->dg_pattern[idx++] = i;
                    }
                }
                ep->dg_pattern_size = active_count;
            }
        }

        /* Propagate through trisynaptic loop */
        hippo_propagate_trisynaptic(hippo);
    }

    hippo->num_episodes++;
    hippo->encodings_performed++;

    if (episode_id_out) *episode_id_out = id;

    hippo->status = HIPPO_STATUS_READY;
    return 0;
}

int hippo_retrieve_episode(nimcp_hippocampus_t* hippo,
                            const float* cue, uint32_t cue_dim,
                            retrieval_mode_t mode,
                            uint32_t* episode_id_out,
                            float* match_confidence) {
    if (!hippo || !cue || cue_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hippo, cue)");
        return -1;
    }

    hippo->status = HIPPO_STATUS_RETRIEVING;

    float best_match = 0.0f;
    uint32_t best_id = UINT32_MAX;

    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];
        if (!ep->bound_representation) continue;

        float similarity = 0.0f;

        switch (mode) {
            case RETRIEVAL_FREE_RECALL:
                /* Match against full representation */
                if (ep->bound_dim >= cue_dim) {
                    similarity = cosine_similarity(cue, ep->bound_representation,
                                                   cue_dim < ep->bound_dim ? cue_dim : ep->bound_dim);
                }
                break;

            case RETRIEVAL_CUED_RECALL:
                /* Match against what content primarily */
                if (ep->what_content && ep->what_dim > 0) {
                    similarity = cosine_similarity(cue, ep->what_content,
                                                   cue_dim < ep->what_dim ? cue_dim : ep->what_dim);
                }
                break;

            case RETRIEVAL_RECOGNITION:
                /* High threshold matching */
                if (ep->bound_dim >= cue_dim) {
                    similarity = cosine_similarity(cue, ep->bound_representation,
                                                   cue_dim < ep->bound_dim ? cue_dim : ep->bound_dim);
                    if (similarity < 0.8f) similarity = 0.0f;
                }
                break;

            case RETRIEVAL_PATTERN_COMPLETION:
                /* Use CA3 pattern completion */
                hippo_pattern_complete(hippo, cue, cue_dim, NULL, NULL, &similarity);
                break;
        }

        /* Weight by encoding strength and recency */
        similarity *= ep->encoding_strength;
        similarity *= (0.5f + 0.5f * ep->recency);

        /* Prime resonance boost */
        if (hippo->prime_resonance_bridge.initialized &&
            ep->resonance_signature == hippo->prime_resonance_bridge.last_resonance_tag) {
            similarity *= 1.2f;
        }

        if (similarity > best_match) {
            best_match = similarity;
            best_id = i;
        }
    }

    if (best_id == UINT32_MAX) {
        hippo->last_error = HIPPO_ERROR_RETRIEVAL_FAILED;
        hippo->status = HIPPO_STATUS_READY;
        return -1;
    }

    /* Strengthen retrieved episode */
    hippo->episodes[best_id].retrieval_count++;
    hippo->episodes[best_id].recency = 1.0f;

    if (episode_id_out) *episode_id_out = best_id;
    if (match_confidence) *match_confidence = best_match;

    hippo->retrievals_performed++;
    hippo->status = HIPPO_STATUS_READY;
    return 0;
}

const nimcp_episode_t* hippo_get_episode(nimcp_hippocampus_t* hippo, uint32_t episode_id) {
    if (!hippo || episode_id >= hippo->num_episodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_episode: hippo is NULL");
        return NULL;
    }
    if (!hippo->episodes[episode_id].bound_representation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_episode: hippo->episodes is NULL");
        return NULL;
    }
    return &hippo->episodes[episode_id];
}

int hippo_strengthen_episode(nimcp_hippocampus_t* hippo, uint32_t episode_id, float amount) {
    if (!hippo || episode_id >= hippo->num_episodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippo_strengthen_episode: hippo is NULL");
        return -1;
    }

    nimcp_episode_t* ep = &hippo->episodes[episode_id];
    if (!ep->bound_representation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_strengthen_episode: ep->bound_representation is NULL");
        return -1;
    }

    ep->encoding_strength = fminf(1.0f, ep->encoding_strength + amount);
    ep->retrieval_count++;
    ep->recency = 1.0f;

    return 0;
}

int hippo_forget_episode(nimcp_hippocampus_t* hippo, uint32_t episode_id) {
    if (!hippo || episode_id >= hippo->num_episodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippo_forget_episode: hippo is NULL");
        return -1;
    }

    nimcp_episode_t* ep = &hippo->episodes[episode_id];

    if (ep->what_content) { nimcp_free(ep->what_content); ep->what_content = NULL; }
    if (ep->where_content) { nimcp_free(ep->where_content); ep->where_content = NULL; }
    if (ep->when_content) { nimcp_free(ep->when_content); ep->when_content = NULL; }
    if (ep->bound_representation) { nimcp_free(ep->bound_representation); ep->bound_representation = NULL; }
    if (ep->dg_pattern) { nimcp_free(ep->dg_pattern); ep->dg_pattern = NULL; }
    if (ep->ca3_pattern) { nimcp_free(ep->ca3_pattern); ep->ca3_pattern = NULL; }
    if (ep->ca1_pattern) { nimcp_free(ep->ca1_pattern); ep->ca1_pattern = NULL; }

    memset(ep, 0, sizeof(nimcp_episode_t));
    return 0;
}

int hippo_find_similar_episodes(nimcp_hippocampus_t* hippo,
                                 const float* query, uint32_t query_dim,
                                 uint32_t* episode_ids,
                                 float* similarities,
                                 uint32_t max_results,
                                 uint32_t* num_found) {
    if (!hippo || !query || !episode_ids || !similarities || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_find_similar_episodes: required parameter is NULL (hippo, query, episode_ids, similarities, num_found)");
        return -1;
    }

    uint32_t count = 0;

    for (uint32_t i = 0; i < hippo->num_episodes && count < max_results; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];
        if (!ep->bound_representation) continue;

        float sim = cosine_similarity(query, ep->bound_representation,
                                      query_dim < ep->bound_dim ? query_dim : ep->bound_dim);

        if (sim > 0.1f) {
            /* Insert sorted */
            uint32_t pos = count;
            for (uint32_t j = 0; j < count; j++) {
                if (sim > similarities[j]) {
                    pos = j;
                    break;
                }
            }

            if (pos < max_results) {
                for (uint32_t j = (count < max_results ? count : max_results - 1); j > pos; j--) {
                    episode_ids[j] = episode_ids[j - 1];
                    similarities[j] = similarities[j - 1];
                }
                episode_ids[pos] = i;
                similarities[pos] = sim;
                if (count < max_results) count++;
            }
        }
    }

    *num_found = count;
    return 0;
}

int hippo_get_episodes_by_type(nimcp_hippocampus_t* hippo,
                                episode_type_t type,
                                uint32_t* episode_ids,
                                uint32_t max_episodes,
                                uint32_t* num_found) {
    if (!hippo || !episode_ids || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_episodes_by_type: required parameter is NULL (hippo, episode_ids, num_found)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < hippo->num_episodes && count < max_episodes; i++) {
        if (hippo->episodes[i].bound_representation && hippo->episodes[i].type == type) {
            episode_ids[count++] = i;
        }
    }

    *num_found = count;
    return 0;
}

int hippo_get_recent_episodes(nimcp_hippocampus_t* hippo,
                               uint32_t* episode_ids,
                               uint32_t max_episodes,
                               uint32_t* num_found) {
    if (!hippo || !episode_ids || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_recent_episodes: required parameter is NULL (hippo, episode_ids, num_found)");
        return -1;
    }

    /* Simple: return most recent by encoding timestamp */
    uint32_t count = 0;
    for (uint32_t i = hippo->num_episodes; i > 0 && count < max_episodes; i--) {
        if (hippo->episodes[i - 1].bound_representation) {
            episode_ids[count++] = i - 1;
        }
    }

    *num_found = count;
    return 0;
}

/*=============================================================================
 * PATTERN SEPARATION/COMPLETION
 *===========================================================================*/

int hippo_pattern_separate(nimcp_hippocampus_t* hippo,
                            const float* input, uint32_t input_dim,
                            float* separated_output, uint32_t* output_dim) {
    if (!hippo || !input || input_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_pattern_separate: required parameter is NULL (hippo, input)");
        return -1;
    }

    hippo->status = HIPPO_STATUS_PATTERN_SEPARATING;

    /* Activate DG with sparse coding */
    hippo_activate_dg(hippo, input, input_dim);

    /* Copy sparse DG pattern to output */
    if (separated_output && output_dim) {
        uint32_t max_out = *output_dim;
        uint32_t count = 0;
        for (uint32_t i = 0; i < hippo->num_dg_cells && count < max_out; i++) {
            separated_output[i] = hippo->dg_activation_pattern[i];
            count++;
        }
        *output_dim = count;
    }

    hippo->status = HIPPO_STATUS_READY;
    return 0;
}

int hippo_pattern_complete(nimcp_hippocampus_t* hippo,
                            const float* partial_cue, uint32_t cue_dim,
                            float* completed_pattern, uint32_t* pattern_dim,
                            float* completion_confidence) {
    if (!hippo || !partial_cue || cue_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_pattern_complete: required parameter is NULL (hippo, partial_cue)");
        return -1;
    }

    hippo->status = HIPPO_STATUS_PATTERN_COMPLETING;

    /* Compute effective threshold - dragonfly rapid retrieval lowers it */
    float effective_threshold = hippo->config.pattern_completion_threshold;
    if (hippo->dragonfly_bridge.initialized && hippo->dragonfly_bridge.rapid_retrieval_mode) {
        effective_threshold = 0.2f;
    }

    /* Find best matching stored pattern in CA3 */
    float best_match = 0.0f;
    uint32_t best_episode = UINT32_MAX;

    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        if (!hippo->episodes[i].bound_representation) continue;

        float sim = cosine_similarity(partial_cue, hippo->episodes[i].bound_representation,
                                      cue_dim < hippo->episodes[i].bound_dim ?
                                      cue_dim : hippo->episodes[i].bound_dim);

        if (sim > best_match && sim > effective_threshold) {
            best_match = sim;
            best_episode = i;
        }
    }

    if (completion_confidence) *completion_confidence = best_match;

    if (best_episode != UINT32_MAX && completed_pattern && pattern_dim) {
        nimcp_episode_t* ep = &hippo->episodes[best_episode];
        uint32_t copy_size = (*pattern_dim < ep->bound_dim) ? *pattern_dim : ep->bound_dim;
        memcpy(completed_pattern, ep->bound_representation, copy_size * sizeof(float));
        *pattern_dim = copy_size;
    }

    hippo->status = HIPPO_STATUS_READY;
    return 0;
}

int hippo_assess_novelty(nimcp_hippocampus_t* hippo,
                          const float* input, uint32_t input_dim,
                          float* novelty_score,
                          bool* requires_separation) {
    if (!hippo || !input || input_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_forget_episode: required parameter is NULL (hippo, input)");
        return -1;
    }

    float max_similarity = 0.0f;

    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        if (!hippo->episodes[i].bound_representation) continue;

        float sim = cosine_similarity(input, hippo->episodes[i].bound_representation,
                                      input_dim < hippo->episodes[i].bound_dim ?
                                      input_dim : hippo->episodes[i].bound_dim);
        if (sim > max_similarity) max_similarity = sim;
    }

    float novelty = 1.0f - max_similarity;
    if (novelty_score) *novelty_score = novelty;
    if (requires_separation) *requires_separation = (novelty > 0.3f);

    return 0;
}

/*=============================================================================
 * SPATIAL NAVIGATION
 *===========================================================================*/

int hippo_update_position(nimcp_hippocampus_t* hippo, const float* position, uint32_t dim) {
    if (!hippo || !position || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_update_position: required parameter is NULL (hippo, position)");
        return -1;
    }

    for (uint32_t i = 0; i < dim && i < 3; i++) {
        hippo->current_position[i] = position[i];
    }

    /* Update place cell activations using Riemannian metric if available */
    hippo->active_place_cells = 0;
    const riemannian_metric_t* metric = (const riemannian_metric_t*)hippo->spatial_metric;
    for (uint32_t i = 0; i < hippo->num_place_cells; i++) {
        float dist = riemannian_spatial_distance(hippo->current_position,
                                                  hippo->place_cells[i].place_field_center, 3,
                                                  metric);
        float rate = hippo->place_cells[i].peak_firing_rate *
                     gaussian_activation(dist, hippo->place_cells[i].place_field_radius);
        hippo->place_cells[i].current_rate = rate;
        if (rate > 1.0f) hippo->active_place_cells++;
    }

    return 0;
}

int hippo_get_active_place_cells(nimcp_hippocampus_t* hippo,
                                  uint32_t* cell_ids,
                                  float* firing_rates,
                                  uint32_t max_cells,
                                  uint32_t* num_active) {
    if (!hippo || !cell_ids || !firing_rates || !num_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_active_place_cells: required parameter is NULL (hippo, cell_ids, firing_rates, num_active)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < hippo->num_place_cells && count < max_cells; i++) {
        if (hippo->place_cells[i].current_rate > 1.0f) {
            cell_ids[count] = hippo->place_cells[i].cell_id;
            firing_rates[count] = hippo->place_cells[i].current_rate;
            count++;
        }
    }

    *num_active = count;
    return 0;
}

int hippo_decode_position(nimcp_hippocampus_t* hippo,
                           float* decoded_position,
                           float* confidence) {
    if (!hippo || !decoded_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_decode_position: required parameter is NULL (hippo, decoded_position)");
        return -1;
    }

    float weighted_sum[3] = {0, 0, 0};
    float total_rate = 0.0f;

    for (uint32_t i = 0; i < hippo->num_place_cells; i++) {
        float rate = hippo->place_cells[i].current_rate;
        if (rate > 0.1f) {
            for (int d = 0; d < 3; d++) {
                weighted_sum[d] += rate * hippo->place_cells[i].place_field_center[d];
            }
            total_rate += rate;
        }
    }

    if (total_rate > 0.1f) {
        for (int d = 0; d < 3; d++) {
            decoded_position[d] = weighted_sum[d] / total_rate;
        }
        if (confidence) *confidence = fminf(1.0f, total_rate / 100.0f);
    } else {
        for (int d = 0; d < 3; d++) decoded_position[d] = 0.0f;
        if (confidence) *confidence = 0.0f;
    }

    return 0;
}

int hippo_create_place_field(nimcp_hippocampus_t* hippo,
                              const float* center, float radius,
                              uint32_t* cell_id_out) {
    if (!hippo || !center) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_create_place_field: required parameter is NULL (hippo, center)");
        return -1;
    }

    /* Find inactive place cell to assign */
    for (uint32_t i = 0; i < hippo->num_place_cells; i++) {
        if (hippo->place_cells[i].field_stability < 0.1f) {
            for (int d = 0; d < 3; d++) {
                hippo->place_cells[i].place_field_center[d] = center[d];
            }
            hippo->place_cells[i].place_field_radius = radius;
            hippo->place_cells[i].field_stability = 0.5f;
            if (cell_id_out) *cell_id_out = i;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hippo_create_place_field: no available cells");
    return -1;  /* No available cells */
}

int hippo_link_episode_to_place(nimcp_hippocampus_t* hippo,
                                 uint32_t episode_id,
                                 const float* position) {
    if (!hippo || episode_id >= hippo->num_episodes || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_link_episode_to_place: required parameter is NULL (hippo, position)");
        return -1;
    }

    nimcp_episode_t* ep = &hippo->episodes[episode_id];
    ep->associated_position[0] = position[0];
    ep->associated_position[1] = position[1];
    ep->associated_position[2] = position[2];
    ep->has_spatial_context = true;

    return 0;
}

int hippo_get_episodes_at_location(nimcp_hippocampus_t* hippo,
                                    const float* position,
                                    float radius,
                                    uint32_t* episode_ids,
                                    uint32_t max_episodes,
                                    uint32_t* num_found) {
    if (!hippo || !position || !episode_ids || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_episodes_at_location: required parameter is NULL (hippo, position, episode_ids, num_found)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < hippo->num_episodes && count < max_episodes; i++) {
        if (!hippo->episodes[i].has_spatial_context) continue;

        float dist = euclidean_distance(position, hippo->episodes[i].associated_position, 3);
        if (dist <= radius) {
            episode_ids[count++] = i;
        }
    }

    *num_found = count;
    return 0;
}

/*=============================================================================
 * OSCILLATIONS AND RHYTHM
 *===========================================================================*/

int hippo_update_theta(nimcp_hippocampus_t* hippo, float dt) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_update_theta: hippo is NULL");
        return -1;
    }

    hippo->theta_phase += 2.0f * M_PI * hippo->config.theta_frequency * dt;
    while (hippo->theta_phase >= 2.0f * M_PI) hippo->theta_phase -= 2.0f * M_PI;

    return 0;
}

int hippo_update_gamma(nimcp_hippocampus_t* hippo, float dt) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_update_gamma: hippo is NULL");
        return -1;
    }

    hippo->gamma_phase += 2.0f * M_PI * hippo->config.gamma_frequency * dt;
    while (hippo->gamma_phase >= 2.0f * M_PI) hippo->gamma_phase -= 2.0f * M_PI;

    return 0;
}

float hippo_get_theta_phase(nimcp_hippocampus_t* hippo) {
    if (!hippo) return 0.0f;
    return hippo->theta_phase;
}

float hippo_get_gamma_phase(nimcp_hippocampus_t* hippo) {
    if (!hippo) return 0.0f;
    return hippo->gamma_phase;
}

int hippo_get_oscillation_power(nimcp_hippocampus_t* hippo,
                                 float* theta_power,
                                 float* gamma_power,
                                 float* ripple_power) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_gamma_phase: hippo is NULL");
        return -1;
    }

    if (theta_power) *theta_power = hippo->theta_power;
    if (gamma_power) *gamma_power = hippo->gamma_power;
    if (ripple_power) {
        *ripple_power = (hippo->oscillation_state == OSCILLATION_SHARP_WAVE_RIPPLE) ? 1.0f : 0.0f;
    }

    return 0;
}

int hippo_set_oscillation_state(nimcp_hippocampus_t* hippo, oscillation_state_t state) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_set_oscillation_state: hippo is NULL");
        return -1;
    }
    hippo->oscillation_state = state;
    return 0;
}

/*=============================================================================
 * MEMORY REPLAY AND CONSOLIDATION
 *===========================================================================*/

int hippo_trigger_replay(nimcp_hippocampus_t* hippo, replay_state_t direction) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_trigger_replay: hippo is NULL");
        return -1;
    }

    hippo->status = HIPPO_STATUS_REPLAYING;
    hippo->oscillation_state = OSCILLATION_SHARP_WAVE_RIPPLE;

    /* Create replay event */
    uint32_t ripple_idx = hippo->replay_head;
    nimcp_ripple_event_t* ripple = &hippo->replay_buffer[ripple_idx];

    /* Free old sequence if exists */
    if (ripple->episode_sequence) {
        nimcp_free(ripple->episode_sequence);
        ripple->episode_sequence = NULL;
    }

    ripple->ripple_id = ripple_idx;
    ripple->timestamp = get_timestamp_ms();
    ripple->amplitude = 0.8f + randf() * 0.2f;
    ripple->frequency = 150.0f + randf() * 100.0f;
    ripple->duration_ms = 50.0f + randf() * 100.0f;
    ripple->replay_direction = direction;
    ripple->compression_factor = 20.0f;

    /* Select episodes to replay - recent and strong ones */
    uint32_t max_replay = 10;
    ripple->episode_sequence = (uint32_t*)nimcp_malloc(max_replay * sizeof(uint32_t));
    if (ripple->episode_sequence) {
        uint32_t count = 0;
        for (uint32_t i = 0; i < hippo->num_episodes && count < max_replay; i++) {
            uint32_t idx = (direction == REPLAY_REVERSE) ?
                           (hippo->num_episodes - 1 - i) : i;
            if (hippo->episodes[idx].bound_representation &&
                hippo->episodes[idx].encoding_strength > 0.3f) {
                ripple->episode_sequence[count++] = idx;
            }
        }
        ripple->sequence_length = count;
    }

    hippo->replay_head = (hippo->replay_head + 1) % hippo->replay_buffer_size;
    hippo->replays_performed++;

    return 0;
}

int hippo_process_replay(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_process_replay: hippo is NULL");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of replay processing */
    hippo_heartbeat("hippo_replay", 0.0f);

    if (hippo->oscillation_state != OSCILLATION_SHARP_WAVE_RIPPLE) return 0;

    /* Strengthen replayed episodes */
    uint32_t prev_idx = (hippo->replay_head == 0) ?
                        hippo->replay_buffer_size - 1 : hippo->replay_head - 1;
    nimcp_ripple_event_t* ripple = &hippo->replay_buffer[prev_idx];

    if (ripple->episode_sequence) {
        for (uint32_t i = 0; i < ripple->sequence_length; i++) {
            uint32_t ep_id = ripple->episode_sequence[i];
            if (ep_id < hippo->num_episodes) {
                hippo->episodes[ep_id].encoding_strength =
                    fminf(1.0f, hippo->episodes[ep_id].encoding_strength + 0.05f);
                hippo->episodes[ep_id].consolidation_level =
                    fminf(1.0f, hippo->episodes[ep_id].consolidation_level + 0.01f);
            }
        }
    }

    /* End ripple after processing */
    hippo->oscillation_state = OSCILLATION_THETA;
    hippo->status = HIPPO_STATUS_READY;

    return 0;
}

const nimcp_ripple_event_t* hippo_get_last_ripple(nimcp_hippocampus_t* hippo) {
    if (!hippo) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo is NULL");

        return NULL;

    }

    uint32_t prev_idx = (hippo->replay_head == 0) ?
                        hippo->replay_buffer_size - 1 : hippo->replay_head - 1;
    return &hippo->replay_buffer[prev_idx];
}

int hippo_consolidate_memories(nimcp_hippocampus_t* hippo, float dt) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_consolidate_memories: hippo is NULL");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of memory consolidation */
    hippo_heartbeat("hippo_consolidate", 0.0f);

    float consolidation_step = hippo->config.consolidation_rate * dt;

    /* Thalamus spindle coupling enhances consolidation */
    if (hippo->thalamus_bridge.initialized) {
        consolidation_step *= (1.0f + hippo->thalamus_bridge.spindle_coupling);
    }

    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];
        if (!ep->bound_representation) continue;

        /* Stronger memories consolidate faster */
        float episode_rate = consolidation_step * ep->encoding_strength;

        /* Emotional memories consolidate faster */
        episode_rate *= (1.0f + ep->emotional_arousal * 0.5f);

        ep->consolidation_level = fminf(1.0f, ep->consolidation_level + episode_rate);

        /* Decay recency */
        ep->recency *= (1.0f - 0.001f * dt);
    }

    return 0;
}

float hippo_get_consolidation_level(nimcp_hippocampus_t* hippo, uint32_t episode_id) {
    if (!hippo || episode_id >= hippo->num_episodes) return 0.0f;
    return hippo->episodes[episode_id].consolidation_level;
}

/*=============================================================================
 * SUBREGION ACCESS
 *===========================================================================*/

int hippo_get_dg_pattern(nimcp_hippocampus_t* hippo,
                          float* pattern, uint32_t max_size,
                          uint32_t* actual_size) {
    if (!hippo || !pattern || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_dg_pattern: required parameter is NULL (hippo, pattern, actual_size)");
        return -1;
    }

    uint32_t count = (hippo->num_dg_cells < max_size) ? hippo->num_dg_cells : max_size;
    memcpy(pattern, hippo->dg_activation_pattern, count * sizeof(float));
    *actual_size = count;

    return 0;
}

int hippo_get_ca3_pattern(nimcp_hippocampus_t* hippo,
                           float* pattern, uint32_t max_size,
                           uint32_t* actual_size) {
    if (!hippo || !pattern || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_ca3_pattern: required parameter is NULL (hippo, pattern, actual_size)");
        return -1;
    }

    uint32_t count = (hippo->num_ca3_cells < max_size) ? hippo->num_ca3_cells : max_size;
    memcpy(pattern, hippo->ca3_activation_pattern, count * sizeof(float));
    *actual_size = count;

    return 0;
}

int hippo_get_ca1_pattern(nimcp_hippocampus_t* hippo,
                           float* pattern, uint32_t max_size,
                           uint32_t* actual_size) {
    if (!hippo || !pattern || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_ca1_pattern: required parameter is NULL (hippo, pattern, actual_size)");
        return -1;
    }

    uint32_t count = (hippo->num_ca1_cells < max_size) ? hippo->num_ca1_cells : max_size;
    memcpy(pattern, hippo->ca1_activation_pattern, count * sizeof(float));
    *actual_size = count;

    return 0;
}

int hippo_get_subiculum_output(nimcp_hippocampus_t* hippo,
                                float* output, uint32_t max_size,
                                uint32_t* actual_size) {
    if (!hippo || !output || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_subiculum_output: required parameter is NULL (hippo, output, actual_size)");
        return -1;
    }

    uint32_t count = (hippo->num_subiculum_cells < max_size) ? hippo->num_subiculum_cells : max_size;
    memcpy(output, hippo->subiculum_pattern, count * sizeof(float));
    *actual_size = count;

    return 0;
}

int hippo_activate_dg(nimcp_hippocampus_t* hippo,
                       const float* input, uint32_t input_dim) {
    if (!hippo || !input || input_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_activate_dg: required parameter is NULL (hippo, input)");
        return -1;
    }

    /* Sparse activation via competitive inhibition */
    float max_activation = 0.0f;

    /* First pass: compute raw activations */
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        float activation = 0.0f;

        /* Simple random projection for now */
        for (uint32_t j = 0; j < input_dim; j++) {
            /* Use cell_id to create deterministic but varied weights */
            float weight = sinf((float)(i * input_dim + j) * 0.01f);
            activation += input[j] * weight;
        }

        activation = activation / sqrtf((float)input_dim);
        hippo->dg_cells[i].activation = fmaxf(0.0f, activation);
        hippo->dg_activation_pattern[i] = hippo->dg_cells[i].activation;

        if (hippo->dg_cells[i].activation > max_activation) {
            max_activation = hippo->dg_cells[i].activation;
        }
    }

    /* Second pass: enforce sparsity via winner-take-all */
    if (max_activation > 0.0f) {
        float threshold = max_activation * (1.0f - hippo->config.dg_sparsity);
        for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
            if (hippo->dg_cells[i].activation < threshold) {
                hippo->dg_cells[i].activation = 0.0f;
                hippo->dg_activation_pattern[i] = 0.0f;
            } else {
                /* Normalize surviving activations */
                hippo->dg_cells[i].activation /= max_activation;
                hippo->dg_activation_pattern[i] = hippo->dg_cells[i].activation;
            }
        }
    }

    return 0;
}

int hippo_propagate_trisynaptic(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_propagate_trisynaptic: hippo is NULL");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of trisynaptic propagation */
    hippo_heartbeat("hippo_trisynaptic", 0.0f);

    /* DG -> CA3 (via mossy fibers) */
    float dg_sum = 0.0f;
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        dg_sum += hippo->dg_cells[i].activation;
    }

    for (uint32_t i = 0; i < hippo->num_ca3_cells; i++) {
        float activation = 0.0f;

        /* Mossy fiber input (sparse, strong) */
        for (uint32_t j = 0; j < hippo->num_dg_cells; j++) {
            if (hippo->dg_cells[j].activation > 0.1f) {
                float weight = sinf((float)(i * hippo->num_dg_cells + j) * 0.1f) * 0.5f + 0.5f;
                activation += hippo->dg_cells[j].activation * weight;
            }
        }

        /* Recurrent CA3 connections */
        for (uint32_t j = 0; j < hippo->num_ca3_cells; j++) {
            if (i != j && randf() < hippo->config.ca3_recurrence_density) {
                activation += hippo->ca3_cells[j].activation * 0.1f;
            }
        }

        hippo->ca3_cells[i].activation = fmaxf(0.0f, fminf(1.0f, activation));
        hippo->ca3_activation_pattern[i] = hippo->ca3_cells[i].activation;
    }

    /* CA3 -> CA1 (via Schaffer collaterals) */
    for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) {
        float activation = 0.0f;

        for (uint32_t j = 0; j < hippo->num_ca3_cells; j++) {
            float weight = cosf((float)(i * hippo->num_ca3_cells + j) * 0.1f) * 0.5f + 0.5f;
            activation += hippo->ca3_cells[j].activation * weight;
        }

        activation /= sqrtf((float)hippo->num_ca3_cells);
        activation *= hippo->ca1_cells[i].theta_modulation;

        hippo->ca1_cells[i].activation = fmaxf(0.0f, fminf(1.0f, activation));
        hippo->ca1_activation_pattern[i] = hippo->ca1_cells[i].activation;
    }

    /* CA1 -> Subiculum */
    for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) {
        float activation = 0.0f;

        for (uint32_t j = 0; j < hippo->num_ca1_cells; j++) {
            float weight = sinf((float)(i * hippo->num_ca1_cells + j) * 0.1f) * 0.5f + 0.5f;
            activation += hippo->ca1_cells[j].activation * weight;
        }

        activation /= sqrtf((float)hippo->num_ca1_cells);

        hippo->subiculum_cells[i].activation = fmaxf(0.0f, fminf(1.0f, activation));
        hippo->subiculum_pattern[i] = hippo->subiculum_cells[i].activation;
    }

    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW
 *===========================================================================*/

int hippo_process_incoming(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_process_incoming: hippo is NULL");
        return -1;
    }

    /* Process entorhinal grid cell input */
    if (hippo->entorhinal_bridge.initialized && hippo->entorhinal_bridge.grid_cell_input) {
        /* Grid cells provide spatial context for encoding */
        hippo->entorhinal_bridge.temporal_context_signal =
            hippo->theta_phase / (2.0f * M_PI);
    }

    /* Process perirhinal familiarity signal */
    if (hippo->perirhinal_bridge.initialized) {
        /* High novelty triggers encoding mode */
        if (hippo->perirhinal_bridge.novelty_signal > 0.5f) {
            hippo->status = HIPPO_STATUS_ENCODING;
        }
    }

    /* Process parahippocampal scene context */
    if (hippo->parahippocampal_bridge.initialized &&
        hippo->parahippocampal_bridge.scene_representation) {
        /* Use scene as context for encoding */
    }

    /* Process hypothalamus emotional/stress modulation */
    if (hippo->hypothalamus_bridge.initialized) {
        float stress = hippo->hypothalamus_bridge.stress_level;
        /* Yerkes-Dodson: moderate stress enhances, high stress impairs */
        float stress_mod = (stress < 0.5f) ? (1.0f + stress) : (2.0f - stress);
        /* Use local effective rate - don't permanently mutate config */
        float effective_learning_rate = hippo->config.default_learning_rate * stress_mod;
        (void)effective_learning_rate;  /* Available for future use in this scope */
    }

    /* Process perception salience */
    if (hippo->perception_bridge.initialized) {
        if (hippo->perception_bridge.salience_level > 0.7f) {
            /* Salient events get preferential encoding */
            hippo->prime_resonance_bridge.memory_enhancement_factor = 2.0f;
        }
    }

    /* Process prime resonance synchronization */
    if (hippo->prime_resonance_bridge.initialized &&
        hippo->prime_resonance_bridge.resonance_active) {
        /* Align theta to resonance phase for enhanced encoding */
        float phase_diff = hippo->prime_resonance_bridge.phase_alignment - hippo->theta_phase;
        hippo->theta_phase += phase_diff * 0.1f;
    }

    /* Process cognitive attention modulation */
    if (hippo->cognitive_bridge.initialized) {
        /* Don't permanently degrade theta_power each cycle - compute locally */
        float effective_theta_power = hippo->theta_power * (0.5f + 0.5f * hippo->cognitive_bridge.attention_level);
        (void)effective_theta_power;  /* Available for future use in this scope */
    }

    return 0;
}

int hippo_send_outgoing(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_send_outgoing: hippo is NULL");
        return -1;
    }

    /* Send to mammillary bodies via fornix */
    if (hippo->mammillary_bridge.initialized) {
        /* Compute fornix output from subiculum */
        float fornix_signal = 0.0f;
        for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) {
            fornix_signal += hippo->subiculum_cells[i].activation;
        }
        hippo->mammillary_bridge.fornix_output_strength =
            (hippo->num_subiculum_cells > 0) ? (fornix_signal / hippo->num_subiculum_cells) : 0.0f;
    }

    /* Send to thalamus */
    if (hippo->thalamus_bridge.initialized) {
        hippo->thalamus_bridge.anterior_nucleus_activity = hippo->theta_power;
    }

    /* Send to entorhinal (feedback) */
    if (hippo->entorhinal_bridge.initialized) {
        /* CA1 sends retrieval signals back */
        float ca1_output = 0.0f;
        for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) {
            ca1_output += hippo->ca1_cells[i].activation;
        }
        /* This would modulate entorhinal processing */
    }

    /* Send to Portia for strategic planning */
    if (hippo->portia_bridge.initialized) {
        hippo->portia_bridge.memory_utilization = (float)hippo->retrievals_performed / 100.0f;
    }

    /* Send to Dragonfly for rapid retrieval */
    if (hippo->dragonfly_bridge.initialized && hippo->dragonfly_bridge.rapid_retrieval_mode) {
        /* Note: rapid retrieval lowers threshold - but don't permanently overwrite config.
         * Pattern completion uses effective_threshold computed locally where needed. */
    }

    return 0;
}

int hippo_bidirectional_update(nimcp_hippocampus_t* hippo, float dt) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_bidirectional_update: hippo is NULL");
        return -1;
    }

    hippo_process_incoming(hippo);
    hippo_update(hippo, dt);
    hippo_send_outgoing(hippo);

    return 0;
}

int hippo_sync_entorhinal(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_sync_entorhinal: hippo is NULL");
        return -1;
    }
    if (hippo->entorhinal_bridge.initialized) {
        hippo->entorhinal_bridge.temporal_context_signal = hippo->theta_phase / (2.0f * M_PI);
    }
    return 0;
}

int hippo_sync_perirhinal(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_sync_perirhinal: hippo is NULL");
        return -1;
    }
    return 0;
}

int hippo_sync_parahippocampal(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_sync_parahippocampal: hippo is NULL");
        return -1;
    }
    return 0;
}

int hippo_sync_mammillary(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_sync_mammillary: hippo is NULL");
        return -1;
    }
    if (hippo->mammillary_bridge.initialized) {
        float output = 0.0f;
        for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) {
            output += hippo->subiculum_cells[i].activation;
        }
        hippo->mammillary_bridge.fornix_output_strength =
            (hippo->num_subiculum_cells > 0) ? (output / hippo->num_subiculum_cells) : 0.0f;
    }
    return 0;
}

int hippo_sync_thalamus(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_sync_thalamus: hippo is NULL");
        return -1;
    }
    if (hippo->thalamus_bridge.initialized) {
        hippo->thalamus_bridge.anterior_nucleus_activity = hippo->theta_power;
    }
    return 0;
}

int hippo_sync_cortical(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_sync_cortical: hippo is NULL");
        return -1;
    }
    /* Cortical sync via CA1 output */
    return 0;
}

/*=============================================================================
 * BRIDGE INITIALIZATION FUNCTIONS
 *===========================================================================*/

int hippo_init_prime_resonance_bridge(nimcp_hippocampus_t* hippo, void* pr_memory) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_prime_resonance_bridge: hippo is NULL");
        return -1;
    }
    hippo->prime_resonance_bridge.initialized = true;
    hippo->prime_resonance_bridge.pr_memory_ctx = pr_memory;
    hippo->prime_resonance_bridge.resonance_frequency = 40.0f;
    hippo->prime_resonance_bridge.resonance_amplitude = 0.5f;
    hippo->prime_resonance_bridge.memory_enhancement_factor = 1.5f;
    hippo->prime_resonance_bridge.coherence_level = 0.8f;
    return 0;
}

int hippo_init_immune_bridge(nimcp_hippocampus_t* hippo, void* immune) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_immune_bridge: hippo is NULL");
        return -1;
    }
    hippo->immune_bridge.initialized = true;
    hippo->immune_bridge.immune_system = immune;
    hippo->immune_bridge.health_score = 1.0f;
    hippo->immune_bridge.inflammation_level = 0.0f;
    hippo->immune_bridge.microglial_activity = 0.1f;
    return 0;
}

int hippo_init_bio_async_bridge(nimcp_hippocampus_t* hippo, void* runtime) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_bio_async_bridge: hippo is NULL");
        return -1;
    }
    hippo->bio_async_bridge.initialized = true;
    hippo->bio_async_bridge.runtime = runtime;
    hippo->bio_async_bridge.async_efficiency = 1.0f;
    hippo->bio_async_bridge.background_processing = true;
    return 0;
}

int hippo_init_brain_init_bridge(nimcp_hippocampus_t* hippo, void* brain_init) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_brain_init_bridge: hippo is NULL");
        return -1;
    }
    hippo->brain_init_bridge.initialized = true;
    hippo->brain_init_bridge.brain_init_ctx = brain_init;
    hippo->brain_init_bridge.initialization_progress = 1.0f;
    hippo->brain_init_bridge.full_integration_complete = true;
    return 0;
}

int hippo_init_security_bridge(nimcp_hippocampus_t* hippo, void* security_ctx, void* security_ops) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_security_bridge: hippo is NULL");
        return -1;
    }
    hippo->security_bridge.initialized = true;
    hippo->security_bridge.security_ctx = security_ctx;
    hippo->security_bridge.security_ops = security_ops;
    hippo->security_bridge.access_level = 1;
    hippo->security_bridge.integrity_checking = true;
    return 0;
}

int hippo_init_logging_bridge(nimcp_hippocampus_t* hippo, void* logger) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_logging_bridge: hippo is NULL");
        return -1;
    }
    hippo->logging_bridge.initialized = true;
    hippo->logging_bridge.logger = logger;
    hippo->logging_bridge.log_level = 2;
    strncpy(hippo->logging_bridge.log_prefix, "HIPPOCAMPUS", 31);
    return 0;
}

int hippo_init_cognitive_bridge(nimcp_hippocampus_t* hippo, void* cognitive) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_cognitive_bridge: hippo is NULL");
        return -1;
    }
    hippo->cognitive_bridge.initialized = true;
    hippo->cognitive_bridge.cognitive_ctx = cognitive;
    hippo->cognitive_bridge.attention_level = 0.5f;
    hippo->cognitive_bridge.working_memory_load = 0.0f;
    hippo->cognitive_bridge.cognitive_control = 0.5f;
    return 0;
}

int hippo_init_training_bridge(nimcp_hippocampus_t* hippo, void* training) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_training_bridge: hippo is NULL");
        return -1;
    }
    hippo->training_bridge.initialized = true;
    hippo->training_bridge.training_ctx = training;
    hippo->training_bridge.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    return 0;
}

int hippo_init_omni_bridge(nimcp_hippocampus_t* hippo, void* omni) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_omni_bridge: hippo is NULL");
        return -1;
    }
    hippo->omni_bridge.initialized = true;
    hippo->omni_bridge.omni_ctx = omni;
    hippo->omni_bridge.integration_weight = 0.5f;
    hippo->omni_bridge.synchronization_quality = 1.0f;
    hippo->omni_bridge.bidirectional_active = true;
    return 0;
}

int hippo_init_hypothalamus_bridge(nimcp_hippocampus_t* hippo, void* hypothalamus) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_hypothalamus_bridge: hippo is NULL");
        return -1;
    }
    hippo->hypothalamus_bridge.initialized = true;
    hippo->hypothalamus_bridge.hypothalamus = hypothalamus;
    hippo->hypothalamus_bridge.stress_level = 0.0f;
    hippo->hypothalamus_bridge.arousal_level = 0.5f;
    return 0;
}

int hippo_init_substrate_bridge(nimcp_hippocampus_t* hippo, void* substrate) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_substrate_bridge: hippo is NULL");
        return -1;
    }
    hippo->substrate_bridge.initialized = true;
    hippo->substrate_bridge.substrate = substrate;
    hippo->substrate_bridge.atp_level = 1.0f;
    hippo->substrate_bridge.oxygen_saturation = 1.0f;
    hippo->substrate_bridge.glucose_level = 1.0f;
    hippo->substrate_bridge.ltp_threshold = 0.5f;
    hippo->substrate_bridge.ltd_threshold = 0.3f;
    return 0;
}

int hippo_init_thalamus_bridge(nimcp_hippocampus_t* hippo, void* thalamus) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_thalamus_bridge: hippo is NULL");
        return -1;
    }
    hippo->thalamus_bridge.initialized = true;
    hippo->thalamus_bridge.thalamus = thalamus;
    hippo->thalamus_bridge.relay_gain = 1.0f;
    hippo->thalamus_bridge.spindle_coupling = 0.5f;
    return 0;
}

int hippo_init_portia_bridge(nimcp_hippocampus_t* hippo, void* portia) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_portia_bridge: hippo is NULL");
        return -1;
    }
    hippo->portia_bridge.initialized = true;
    hippo->portia_bridge.portia_ctx = portia;
    hippo->portia_bridge.planning_depth = 3.0f;
    hippo->portia_bridge.prospective_memory_active = true;
    return 0;
}

int hippo_init_dragonfly_bridge(nimcp_hippocampus_t* hippo, void* dragonfly) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_dragonfly_bridge: hippo is NULL");
        return -1;
    }
    hippo->dragonfly_bridge.initialized = true;
    hippo->dragonfly_bridge.dragonfly_ctx = dragonfly;
    hippo->dragonfly_bridge.reaction_threshold = 0.3f;
    hippo->dragonfly_bridge.rapid_retrieval_mode = false;
    return 0;
}

int hippo_init_perception_bridge(nimcp_hippocampus_t* hippo, void* perception) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_perception_bridge: hippo is NULL");
        return -1;
    }
    hippo->perception_bridge.initialized = true;
    hippo->perception_bridge.perception_ctx = perception;
    hippo->perception_bridge.salience_level = 0.5f;
    hippo->perception_bridge.attention_weight = 0.5f;
    return 0;
}

int hippo_init_snn_bridge(nimcp_hippocampus_t* hippo, void* snn) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_snn_bridge: hippo is NULL");
        return -1;
    }
    hippo->snn_bridge.initialized = true;
    hippo->snn_bridge.snn_network = snn;
    hippo->snn_bridge.total_neurons = hippo->num_dg_cells + hippo->num_ca3_cells +
                                       hippo->num_ca1_cells + hippo->num_subiculum_cells;
    return 0;
}

int hippo_init_plasticity_bridge(nimcp_hippocampus_t* hippo, void* plasticity, void* stdp) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_plasticity_bridge: hippo is NULL");
        return -1;
    }
    hippo->plasticity_bridge.initialized = true;
    hippo->plasticity_bridge.plasticity_ctx = plasticity;
    hippo->plasticity_bridge.stdp_ctx = stdp;
    hippo->plasticity_bridge.ltp_magnitude = 0.1f;
    hippo->plasticity_bridge.ltd_magnitude = 0.05f;
    hippo->plasticity_bridge.hebbian_learning = true;
    hippo->plasticity_bridge.stdp_enabled = true;
    return 0;
}

int hippo_init_entorhinal_bridge(nimcp_hippocampus_t* hippo, void* entorhinal) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_entorhinal_bridge: hippo is NULL");
        return -1;
    }
    hippo->entorhinal_bridge.initialized = true;
    hippo->entorhinal_bridge.entorhinal = entorhinal;
    hippo->entorhinal_bridge.perforant_path_strength = 0.8f;
    hippo->entorhinal_bridge.mec_input_weight = 0.5f;
    hippo->entorhinal_bridge.lec_input_weight = 0.5f;
    return 0;
}

int hippo_init_perirhinal_bridge(nimcp_hippocampus_t* hippo, void* perirhinal) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_perirhinal_bridge: hippo is NULL");
        return -1;
    }
    hippo->perirhinal_bridge.initialized = true;
    hippo->perirhinal_bridge.perirhinal = perirhinal;
    hippo->perirhinal_bridge.familiarity_signal = 0.0f;
    hippo->perirhinal_bridge.novelty_signal = 1.0f;
    return 0;
}

int hippo_init_parahippocampal_bridge(nimcp_hippocampus_t* hippo, void* parahippocampal) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_parahippocampal_bridge: hippo is NULL");
        return -1;
    }
    hippo->parahippocampal_bridge.initialized = true;
    hippo->parahippocampal_bridge.parahippocampal = parahippocampal;
    hippo->parahippocampal_bridge.place_recognition = 0.0f;
    hippo->parahippocampal_bridge.context_stability = 1.0f;
    return 0;
}

int hippo_init_mammillary_bridge(nimcp_hippocampus_t* hippo, void* mammillary) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_mammillary_bridge: hippo is NULL");
        return -1;
    }
    hippo->mammillary_bridge.initialized = true;
    hippo->mammillary_bridge.mammillary = mammillary;
    hippo->mammillary_bridge.fornix_output_strength = 0.0f;
    hippo->mammillary_bridge.papez_circuit_activity = 0.0f;
    return 0;
}

int hippo_init_cerebellum_bridge(nimcp_hippocampus_t* hippo, void* cerebellum) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_cerebellum_bridge: hippo is NULL");
        return -1;
    }
    hippo->cerebellum_bridge.initialized = true;
    hippo->cerebellum_bridge.cerebellum = cerebellum;
    hippo->cerebellum_bridge.timing_signal = 0.0f;
    return 0;
}

int hippo_init_medulla_bridge(nimcp_hippocampus_t* hippo, void* medulla) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_medulla_bridge: hippo is NULL");
        return -1;
    }
    hippo->medulla_bridge.initialized = true;
    hippo->medulla_bridge.medulla = medulla;
    hippo->medulla_bridge.autonomic_state = 0.5f;
    return 0;
}

int hippo_init_swarm_bridge(nimcp_hippocampus_t* hippo, void* swarm) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_swarm_bridge: hippo is NULL");
        return -1;
    }
    hippo->swarm_bridge.initialized = true;
    hippo->swarm_bridge.swarm_ctx = swarm;
    hippo->swarm_bridge.swarm_coherence = 0.8f;
    return 0;
}

int hippo_init_all_bridges(nimcp_hippocampus_t* hippo, void** bridge_contexts) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_all_bridges: hippo is NULL");
        return -1;
    }

    /* Initialize all bridges with NULL contexts (defaults) */
    hippo_init_prime_resonance_bridge(hippo, bridge_contexts ? bridge_contexts[0] : NULL);
    hippo_init_immune_bridge(hippo, bridge_contexts ? bridge_contexts[1] : NULL);
    hippo_init_bio_async_bridge(hippo, bridge_contexts ? bridge_contexts[2] : NULL);
    hippo_init_brain_init_bridge(hippo, bridge_contexts ? bridge_contexts[3] : NULL);
    hippo_init_security_bridge(hippo, bridge_contexts ? bridge_contexts[4] : NULL, NULL);
    hippo_init_logging_bridge(hippo, bridge_contexts ? bridge_contexts[5] : NULL);
    hippo_init_cognitive_bridge(hippo, bridge_contexts ? bridge_contexts[6] : NULL);
    hippo_init_training_bridge(hippo, bridge_contexts ? bridge_contexts[7] : NULL);
    hippo_init_omni_bridge(hippo, bridge_contexts ? bridge_contexts[8] : NULL);
    hippo_init_hypothalamus_bridge(hippo, bridge_contexts ? bridge_contexts[9] : NULL);
    hippo_init_substrate_bridge(hippo, bridge_contexts ? bridge_contexts[10] : NULL);
    hippo_init_thalamus_bridge(hippo, bridge_contexts ? bridge_contexts[11] : NULL);
    hippo_init_portia_bridge(hippo, bridge_contexts ? bridge_contexts[12] : NULL);
    hippo_init_dragonfly_bridge(hippo, bridge_contexts ? bridge_contexts[13] : NULL);
    hippo_init_perception_bridge(hippo, bridge_contexts ? bridge_contexts[14] : NULL);
    hippo_init_snn_bridge(hippo, bridge_contexts ? bridge_contexts[15] : NULL);
    hippo_init_plasticity_bridge(hippo, bridge_contexts ? bridge_contexts[16] : NULL, NULL);
    hippo_init_entorhinal_bridge(hippo, bridge_contexts ? bridge_contexts[17] : NULL);
    hippo_init_perirhinal_bridge(hippo, bridge_contexts ? bridge_contexts[18] : NULL);
    hippo_init_parahippocampal_bridge(hippo, bridge_contexts ? bridge_contexts[19] : NULL);
    hippo_init_mammillary_bridge(hippo, bridge_contexts ? bridge_contexts[20] : NULL);
    hippo_init_cerebellum_bridge(hippo, bridge_contexts ? bridge_contexts[21] : NULL);
    hippo_init_medulla_bridge(hippo, bridge_contexts ? bridge_contexts[22] : NULL);
    hippo_init_swarm_bridge(hippo, bridge_contexts ? bridge_contexts[23] : NULL);

    return 0;
}

/*=============================================================================
 * PRIME RESONANCE INTEGRATION
 *===========================================================================*/

int hippo_tag_with_resonance(nimcp_hippocampus_t* hippo,
                              uint32_t episode_id,
                              uint64_t resonance_signature) {
    if (!hippo || episode_id >= hippo->num_episodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippo_init_all_bridges: hippo is NULL");
        return -1;
    }

    hippo->episodes[episode_id].resonance_signature = resonance_signature;
    hippo->episodes[episode_id].resonance_strength = 1.0f;

    return 0;
}

int hippo_find_by_resonance(nimcp_hippocampus_t* hippo,
                             uint64_t resonance_signature,
                             uint32_t* episode_ids,
                             uint32_t max_episodes,
                             uint32_t* num_found) {
    if (!hippo || !episode_ids || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_all_bridges: required parameter is NULL (hippo, episode_ids, num_found)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < hippo->num_episodes && count < max_episodes; i++) {
        if (hippo->episodes[i].bound_representation &&
            hippo->episodes[i].resonance_signature == resonance_signature) {
            episode_ids[count++] = i;
        }
    }

    *num_found = count;
    return 0;
}

int hippo_resonance_enhanced_encode(nimcp_hippocampus_t* hippo,
                                     const float* content, uint32_t dim,
                                     uint32_t* episode_id_out) {
    if (!hippo || !content || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_all_bridges: required parameter is NULL (hippo, content)");
        return -1;
    }

    /* Enable resonance enhancement */
    float original_factor = hippo->prime_resonance_bridge.memory_enhancement_factor;
    if (hippo->prime_resonance_bridge.initialized) {
        hippo->prime_resonance_bridge.resonance_active = true;
        hippo->prime_resonance_bridge.memory_enhancement_factor = 2.0f;
    }

    /* Encode with enhanced parameters */
    uint32_t episode_id;
    int result = hippo_encode_episode(hippo, content, dim, NULL, 0, NULL, 0, 0.0f, 0.8f, &episode_id);

    /* Tag with current resonance */
    if (result == 0 && hippo->prime_resonance_bridge.initialized) {
        hippo->episodes[episode_id].resonance_signature =
            hippo->prime_resonance_bridge.last_resonance_tag;
        hippo->episodes[episode_id].resonance_strength =
            hippo->prime_resonance_bridge.coherence_level;
    }

    /* Restore original factor */
    hippo->prime_resonance_bridge.memory_enhancement_factor = original_factor;
    hippo->prime_resonance_bridge.resonance_active = false;

    if (episode_id_out) *episode_id_out = episode_id;
    return result;
}

int hippo_resonance_guided_retrieve(nimcp_hippocampus_t* hippo,
                                     const float* cue, uint32_t cue_dim,
                                     uint32_t* episode_id_out,
                                     float* confidence) {
    if (!hippo || !cue || cue_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_init_all_bridges: required parameter is NULL (hippo, cue)");
        return -1;
    }

    /* First try resonance-tagged episodes */
    if (hippo->prime_resonance_bridge.initialized &&
        hippo->prime_resonance_bridge.last_resonance_tag != 0) {

        uint32_t resonance_ids[10];
        uint32_t num_resonance;
        hippo_find_by_resonance(hippo, hippo->prime_resonance_bridge.last_resonance_tag,
                                resonance_ids, 10, &num_resonance);

        float best_match = 0.0f;
        uint32_t best_id = UINT32_MAX;

        for (uint32_t i = 0; i < num_resonance; i++) {
            nimcp_episode_t* ep = &hippo->episodes[resonance_ids[i]];
            if (!ep->bound_representation) continue;

            float sim = cosine_similarity(cue, ep->bound_representation,
                                          cue_dim < ep->bound_dim ? cue_dim : ep->bound_dim);
            /* Boost by resonance strength */
            sim *= (1.0f + ep->resonance_strength * 0.5f);

            if (sim > best_match) {
                best_match = sim;
                best_id = resonance_ids[i];
            }
        }

        if (best_id != UINT32_MAX && best_match > 0.3f) {
            if (episode_id_out) *episode_id_out = best_id;
            if (confidence) *confidence = best_match;
            hippo->retrievals_performed++;
            return 0;
        }
    }

    /* Fall back to regular retrieval */
    return hippo_retrieve_episode(hippo, cue, cue_dim, RETRIEVAL_CUED_RECALL,
                                   episode_id_out, confidence);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

hippo_status_t hippo_get_status(nimcp_hippocampus_t* hippo) {
    if (!hippo) return HIPPO_STATUS_ERROR;
    return hippo->status;
}

hippo_error_t hippo_get_last_error(nimcp_hippocampus_t* hippo) {
    if (!hippo) return HIPPO_ERROR_INTERNAL;
    return hippo->last_error;
}

const char* hippo_error_string(hippo_error_t error) {
    switch (error) {
        case HIPPO_ERROR_NONE: return "No error";
        case HIPPO_ERROR_INVALID_INPUT: return "Invalid input";
        case HIPPO_ERROR_MEMORY_FULL: return "Memory full";
        case HIPPO_ERROR_EPISODE_NOT_FOUND: return "Episode not found";
        case HIPPO_ERROR_ENCODING_FAILED: return "Encoding failed";
        case HIPPO_ERROR_RETRIEVAL_FAILED: return "Retrieval failed";
        case HIPPO_ERROR_PATTERN_MISMATCH: return "Pattern mismatch";
        case HIPPO_ERROR_RHYTHM_DISRUPTED: return "Rhythm disrupted";
        case HIPPO_ERROR_CONSOLIDATION_FAILED: return "Consolidation failed";
        case HIPPO_ERROR_BRIDGE_ERROR: return "Bridge error";
        case HIPPO_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* hippo_status_string(hippo_status_t status) {
    switch (status) {
        case HIPPO_STATUS_IDLE: return "Idle";
        case HIPPO_STATUS_READY: return "Ready";
        case HIPPO_STATUS_ENCODING: return "Encoding";
        case HIPPO_STATUS_RETRIEVING: return "Retrieving";
        case HIPPO_STATUS_CONSOLIDATING: return "Consolidating";
        case HIPPO_STATUS_REPLAYING: return "Replaying";
        case HIPPO_STATUS_NAVIGATING: return "Navigating";
        case HIPPO_STATUS_PATTERN_SEPARATING: return "Pattern separating";
        case HIPPO_STATUS_PATTERN_COMPLETING: return "Pattern completing";
        case HIPPO_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

int hippo_get_stats(nimcp_hippocampus_t* hippo, hippo_stats_t* stats) {
    if (!hippo || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_stats: required parameter is NULL (hippo, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(hippo_stats_t));

    stats->total_episodes = hippo->num_episodes;
    stats->episodes_encoded = hippo->encodings_performed;
    stats->episodes_retrieved = hippo->retrievals_performed;
    stats->replay_events = hippo->replays_performed;
    stats->place_cells_active = hippo->active_place_cells;
    stats->theta_power = hippo->theta_power;
    stats->gamma_power = hippo->gamma_power;
    stats->last_update_time = hippo->last_update_time;
    stats->updates_processed = hippo->updates_processed;

    /* Calculate averages */
    float total_strength = 0.0f;
    float total_consolidation = 0.0f;
    uint32_t valid_episodes = 0;

    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        if (hippo->episodes[i].bound_representation) {
            total_strength += hippo->episodes[i].encoding_strength;
            total_consolidation += hippo->episodes[i].consolidation_level;
            valid_episodes++;
        }
    }

    if (valid_episodes > 0) {
        stats->avg_encoding_strength = total_strength / valid_episodes;
        stats->consolidation_progress = total_consolidation / valid_episodes;
    }

    return 0;
}

int hippo_get_config(nimcp_hippocampus_t* hippo, hippo_config_t* config) {
    if (!hippo || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_get_config: required parameter is NULL (hippo, config)");
        return -1;
    }
    *config = hippo->config;
    return 0;
}

float hippo_get_health_status(nimcp_hippocampus_t* hippo) {
    if (!hippo) return 0.0f;

    float health = 1.0f;

    /* Consider immune status */
    if (hippo->immune_bridge.initialized) {
        health *= hippo->immune_bridge.health_score;
        if (hippo->immune_bridge.neuroinflammation) health *= 0.7f;
    }

    /* Consider substrate status */
    if (hippo->substrate_bridge.initialized) {
        health *= (hippo->substrate_bridge.atp_level * 0.4f +
                   hippo->substrate_bridge.oxygen_saturation * 0.3f +
                   hippo->substrate_bridge.glucose_level * 0.3f);
    }

    /* Consider theta rhythm health */
    health *= (0.5f + 0.5f * hippo->theta_power);

    /* Consider error state */
    if (hippo->status == HIPPO_STATUS_ERROR) health *= 0.5f;

    return fminf(1.0f, fmaxf(0.0f, health));
}

int hippo_log_diagnostics(nimcp_hippocampus_t* hippo) {
    if (!hippo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_log_diagnostics: hippo is NULL");
        return -1;
    }

    /* Would log via logging bridge if available */
    return 0;
}

/*=============================================================================
 * CELL ACTIVITY QUERIES
 *===========================================================================*/

size_t hippo_get_dg_cell_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max) {
    if (!hippo || !activity) return 0;

    size_t count = (hippo->num_dg_cells < max) ? hippo->num_dg_cells : max;
    for (size_t i = 0; i < count; i++) {
        activity[i] = hippo->dg_cells[i].activation;
    }
    return count;
}

size_t hippo_get_ca3_cell_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max) {
    if (!hippo || !activity) return 0;

    size_t count = (hippo->num_ca3_cells < max) ? hippo->num_ca3_cells : max;
    for (size_t i = 0; i < count; i++) {
        activity[i] = hippo->ca3_cells[i].activation;
    }
    return count;
}

size_t hippo_get_ca1_cell_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max) {
    if (!hippo || !activity) return 0;

    size_t count = (hippo->num_ca1_cells < max) ? hippo->num_ca1_cells : max;
    for (size_t i = 0; i < count; i++) {
        activity[i] = hippo->ca1_cells[i].activation;
    }
    return count;
}

size_t hippo_get_subiculum_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max) {
    if (!hippo || !activity) return 0;

    size_t count = (hippo->num_subiculum_cells < max) ? hippo->num_subiculum_cells : max;
    for (size_t i = 0; i < count; i++) {
        activity[i] = hippo->subiculum_cells[i].activation;
    }
    return count;
}

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

size_t hippo_get_serialization_size(nimcp_hippocampus_t* hippo) {
    if (!hippo) return 0;

    size_t size = sizeof(hippo_config_t);
    size += sizeof(hippo_status_t);
    size += sizeof(uint32_t);  /* num_episodes */
    size += sizeof(float) * 4;  /* Oscillation state */
    size += sizeof(float) * 3;  /* Position */

    /* Episode data */
    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];
        size += sizeof(uint32_t);  /* episode_id */
        size += sizeof(episode_type_t);
        size += sizeof(uint32_t) + ep->what_dim * sizeof(float);  /* what */
        size += sizeof(uint32_t) + ep->where_dim * sizeof(float); /* where */
        size += sizeof(uint32_t) + ep->when_dim * sizeof(float);  /* when */
        size += sizeof(uint32_t) + ep->bound_dim * sizeof(float); /* bound_representation */
        size += sizeof(float) * 4;  /* encoding_strength, emotional_valence, emotional_arousal, consolidation_level */
        size += sizeof(uint64_t);   /* encoding_timestamp */
    }

    return size;
}

int hippo_serialize(nimcp_hippocampus_t* hippo, uint8_t* buffer, size_t size, size_t* written) {
    if (!hippo || !buffer || !written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_serialize: required parameter is NULL (hippo, buffer, written)");
        return -1;
    }

    size_t required = hippo_get_serialization_size(hippo);
    if (size < required) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippo_serialize: validation failed");
        return -1;
    }

    size_t offset = 0;

    /* Write config */
    memcpy(buffer + offset, &hippo->config, sizeof(hippo_config_t));
    offset += sizeof(hippo_config_t);

    /* Write status */
    memcpy(buffer + offset, &hippo->status, sizeof(hippo_status_t));
    offset += sizeof(hippo_status_t);

    /* Write num_episodes */
    memcpy(buffer + offset, &hippo->num_episodes, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    /* Write oscillation state */
    memcpy(buffer + offset, &hippo->theta_phase, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &hippo->gamma_phase, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &hippo->theta_power, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &hippo->gamma_power, sizeof(float));
    offset += sizeof(float);

    /* Write position */
    memcpy(buffer + offset, hippo->current_position, sizeof(float) * 3);
    offset += sizeof(float) * 3;

    /* Write episodes */
    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        nimcp_episode_t* ep = &hippo->episodes[i];

        memcpy(buffer + offset, &ep->episode_id, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(buffer + offset, &ep->type, sizeof(episode_type_t));
        offset += sizeof(episode_type_t);

        /* What content */
        memcpy(buffer + offset, &ep->what_dim, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->what_dim > 0 && ep->what_content) {
            memcpy(buffer + offset, ep->what_content, ep->what_dim * sizeof(float));
            offset += ep->what_dim * sizeof(float);
        }

        /* Where content */
        memcpy(buffer + offset, &ep->where_dim, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->where_dim > 0 && ep->where_content) {
            memcpy(buffer + offset, ep->where_content, ep->where_dim * sizeof(float));
            offset += ep->where_dim * sizeof(float);
        }

        /* When content */
        memcpy(buffer + offset, &ep->when_dim, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->when_dim > 0 && ep->when_content) {
            memcpy(buffer + offset, ep->when_content, ep->when_dim * sizeof(float));
            offset += ep->when_dim * sizeof(float);
        }

        /* Bound representation array */
        memcpy(buffer + offset, &ep->bound_dim, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->bound_dim > 0 && ep->bound_representation) {
            memcpy(buffer + offset, ep->bound_representation, ep->bound_dim * sizeof(float));
            offset += ep->bound_dim * sizeof(float);
        }

        /* Episode metadata */
        memcpy(buffer + offset, &ep->encoding_strength, sizeof(float));
        offset += sizeof(float);
        memcpy(buffer + offset, &ep->emotional_valence, sizeof(float));
        offset += sizeof(float);
        memcpy(buffer + offset, &ep->emotional_arousal, sizeof(float));
        offset += sizeof(float);
        memcpy(buffer + offset, &ep->consolidation_level, sizeof(float));
        offset += sizeof(float);
        memcpy(buffer + offset, &ep->encoding_timestamp, sizeof(uint64_t));
        offset += sizeof(uint64_t);
    }

    *written = offset;
    return 0;
}

nimcp_hippocampus_t* hippo_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read) {
    if (!buffer || size < sizeof(hippo_config_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippo_deserialize: buffer is NULL");
        return NULL;
    }

    size_t offset = 0;

    /* Read config */
    hippo_config_t config;
    memcpy(&config, buffer + offset, sizeof(hippo_config_t));
    offset += sizeof(hippo_config_t);

    /* Create instance */
    nimcp_hippocampus_t* hippo = hippo_create(&config);
    if (!hippo) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo is NULL");

        return NULL;

    }

    /* Read status */
    memcpy(&hippo->status, buffer + offset, sizeof(hippo_status_t));
    offset += sizeof(hippo_status_t);

    /* Read num_episodes */
    uint32_t num_episodes;
    memcpy(&num_episodes, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    /* Read oscillation state */
    memcpy(&hippo->theta_phase, buffer + offset, sizeof(float));
    offset += sizeof(float);
    memcpy(&hippo->gamma_phase, buffer + offset, sizeof(float));
    offset += sizeof(float);
    memcpy(&hippo->theta_power, buffer + offset, sizeof(float));
    offset += sizeof(float);
    memcpy(&hippo->gamma_power, buffer + offset, sizeof(float));
    offset += sizeof(float);

    /* Read position */
    memcpy(hippo->current_position, buffer + offset, sizeof(float) * 3);
    offset += sizeof(float) * 3;

    /* Read episodes with bounds checking */
    #define SAFE_READ(dest, src_off, sz) do { \
        if ((src_off) + (sz) > size) goto deserialize_error; \
        memcpy((dest), buffer + (src_off), (sz)); \
    } while(0)

    for (uint32_t i = 0; i < num_episodes && i < hippo->max_episodes; i++) {
        if (offset >= size) break;

        nimcp_episode_t* ep = &hippo->episodes[i];

        SAFE_READ(&ep->episode_id, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        SAFE_READ(&ep->type, offset, sizeof(episode_type_t));
        offset += sizeof(episode_type_t);

        /* What content */
        SAFE_READ(&ep->what_dim, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->what_dim > 0 && ep->what_dim < 10000) {  /* Sanity check */
            if (offset + ep->what_dim * sizeof(float) > size) goto deserialize_error;
            ep->what_content = (float*)nimcp_malloc(ep->what_dim * sizeof(float));
            if (ep->what_content) {
                memcpy(ep->what_content, buffer + offset, ep->what_dim * sizeof(float));
                offset += ep->what_dim * sizeof(float);
            }
        }

        /* Where content */
        SAFE_READ(&ep->where_dim, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->where_dim > 0 && ep->where_dim < 10000) {
            if (offset + ep->where_dim * sizeof(float) > size) goto deserialize_error;
            ep->where_content = (float*)nimcp_malloc(ep->where_dim * sizeof(float));
            if (ep->where_content) {
                memcpy(ep->where_content, buffer + offset, ep->where_dim * sizeof(float));
                offset += ep->where_dim * sizeof(float);
            }
        }

        /* When content */
        SAFE_READ(&ep->when_dim, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->when_dim > 0 && ep->when_dim < 10000) {
            if (offset + ep->when_dim * sizeof(float) > size) goto deserialize_error;
            ep->when_content = (float*)nimcp_malloc(ep->when_dim * sizeof(float));
            if (ep->when_content) {
                memcpy(ep->when_content, buffer + offset, ep->when_dim * sizeof(float));
                offset += ep->when_dim * sizeof(float);
            }
        }

        /* Bound representation array */
        SAFE_READ(&ep->bound_dim, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (ep->bound_dim > 0 && ep->bound_dim < 100000) {
            if (offset + ep->bound_dim * sizeof(float) > size) goto deserialize_error;
            ep->bound_representation = (float*)nimcp_malloc(ep->bound_dim * sizeof(float));
            if (ep->bound_representation) {
                memcpy(ep->bound_representation, buffer + offset, ep->bound_dim * sizeof(float));
                offset += ep->bound_dim * sizeof(float);
            }
        }

        /* Episode metadata */
        SAFE_READ(&ep->encoding_strength, offset, sizeof(float));
        offset += sizeof(float);
        SAFE_READ(&ep->emotional_valence, offset, sizeof(float));
        offset += sizeof(float);
        SAFE_READ(&ep->emotional_arousal, offset, sizeof(float));
        offset += sizeof(float);
        SAFE_READ(&ep->consolidation_level, offset, sizeof(float));
        offset += sizeof(float);
        SAFE_READ(&ep->encoding_timestamp, offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        hippo->num_episodes++;
    }

    #undef SAFE_READ

    if (bytes_read) *bytes_read = offset;
    return hippo;

deserialize_error:
    hippo_destroy(hippo);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippo_deserialize: validation failed");
    return NULL;
}
