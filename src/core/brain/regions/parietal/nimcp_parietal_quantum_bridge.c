/**
 * @file nimcp_parietal_quantum_bridge.c
 * @brief Implementation of quantum-inspired spatial reasoning optimization
 *
 * WHAT: Integrates quantum algorithms with parietal cortex functions
 * WHY: Explore multiple spatial configurations simultaneously
 * HOW: Quantum reasoning for attention, transforms, motor planning
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/parietal/nimcp_parietal_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(parietal_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "PARIETAL_QUANTUM_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define PARIETAL_Q_LOG_MODULE "PARIETAL_QUANTUM"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum state for spatial location
 */
typedef struct {
    uint32_t location_id;
    float amplitude_real;            /**< Real part of amplitude */
    float amplitude_imag;            /**< Imaginary part of amplitude */
    parietal_cortex_position_t position;
    float salience;
} quantum_location_state_t;

/**
 * @brief Quantum walk state
 */
typedef struct {
    parietal_cortex_position_t position;
    float amplitude_real;
    float amplitude_imag;
    uint8_t coin_state;              /**< 0 = left, 1 = right, 2 = up, 3 = down */
} quantum_walk_state_t;

/**
 * @brief Internal bridge structure
 */
struct parietal_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    parietal_region_quantum_config_t config;

    /* Parietal adapter reference */
    parietal_adapter_t* parietal;

    /* Quantum states */
    quantum_location_state_t* spatial_states;
    uint32_t spatial_state_count;
    uint32_t spatial_state_capacity;

    /* Attention superposition */
    quantum_location_state_t* attention_superposition;
    uint32_t attention_state_count;
    bool attention_collapsed;

    /* Frame superposition */
    quantum_frame_superposition_t current_frame_super;

    /* Trajectory superposition */
    quantum_trajectory_candidate_t* trajectory_states;
    uint32_t trajectory_state_count;
    uint32_t trajectory_state_capacity;
    bool trajectory_collapsed;

    /* Quantum walk state */
    quantum_walk_state_t* walk_states;
    uint32_t walk_state_count;
    uint32_t walk_graph_size;
    bool walk_initialized;

    /* Random state for quantum measurements */
    uint32_t rng_state;

    /* Statistics */
    parietal_quantum_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple pseudo-random number generator
 */
static float quantum_random(parietal_quantum_bridge_t* bridge) {
    bridge->rng_state = bridge->rng_state * 1103515245 + 12345;
    return (float)(bridge->rng_state % 10000) / 10000.0f;
}

/**
 * @brief Compute probability from amplitude
 */
static float amplitude_to_prob(float real, float imag) {
    return real * real + imag * imag;
}

/**
 * @brief Normalize amplitudes in state array
 */
static void normalize_amplitudes(quantum_location_state_t* states, uint32_t count) {
    float total_prob = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        total_prob += amplitude_to_prob(states[i].amplitude_real, states[i].amplitude_imag);
    }

    if (total_prob > 0.0f) {
        float norm = sqrtf(total_prob);
        for (uint32_t i = 0; i < count; i++) {
            states[i].amplitude_real /= norm;
            states[i].amplitude_imag /= norm;
        }
    }
}

/**
 * @brief Apply Grover diffusion operator
 */
static void apply_diffusion(quantum_location_state_t* states, uint32_t count) {
    /* Compute mean amplitude */
    float mean_real = 0.0f, mean_imag = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        mean_real += states[i].amplitude_real;
        mean_imag += states[i].amplitude_imag;
    }
    mean_real /= (float)count;
    mean_imag /= (float)count;

    /* Inversion about mean: 2*mean - amplitude */
    for (uint32_t i = 0; i < count; i++) {
        states[i].amplitude_real = 2.0f * mean_real - states[i].amplitude_real;
        states[i].amplitude_imag = 2.0f * mean_imag - states[i].amplitude_imag;
    }
}

/**
 * @brief Apply oracle to mark target states (high salience)
 */
static void apply_salience_oracle(quantum_location_state_t* states, uint32_t count,
                                   float threshold) {
    /* Flip phase of states above salience threshold */
    for (uint32_t i = 0; i < count; i++) {
        if (states[i].salience >= threshold) {
            states[i].amplitude_real = -states[i].amplitude_real;
            states[i].amplitude_imag = -states[i].amplitude_imag;
        }
    }
}

/**
 * @brief Compute optimal Grover iterations
 */
static uint32_t compute_grover_iterations(uint32_t n, uint32_t num_marked) {
    if (n == 0 || num_marked == 0) return 1;
    float theta = asinf(sqrtf((float)num_marked / (float)n));
    if (theta < 0.0001f) return 1;
    uint32_t optimal = (uint32_t)(M_PI / (4.0f * theta) - 0.5f);
    return optimal > 0 ? optimal : 1;
}

/**
 * @brief Measure quantum state (collapse to one outcome)
 */
static uint32_t measure_state(parietal_quantum_bridge_t* bridge,
                               quantum_location_state_t* states,
                               uint32_t count) {
    float r = quantum_random(bridge);
    float cumulative = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        cumulative += amplitude_to_prob(states[i].amplitude_real, states[i].amplitude_imag);
        if (r <= cumulative) {
            return i;
        }
    }
    return count > 0 ? count - 1 : 0;
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

parietal_region_quantum_config_t parietal_region_quantum_default_config(void) {
    parietal_region_quantum_config_t config;
    config.enabled = true;
    config.spatial_grid_size = NIMCP_DEFAULT_GRID_DIM;
    config.max_targets = 128;
    config.max_grover_iterations = 10;
    config.min_salience_threshold = 0.1f;
    config.enable_superposition = true;
    config.enable_quantum_walk = true;
    config.enable_entanglement = true;
    config.seed = 42;
    return config;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

parietal_quantum_bridge_t* parietal_region_quantum_bridge_create(
    parietal_adapter_t* parietal,
    const parietal_region_quantum_config_t* config) {

    LOG_INFO("[%s] Creating quantum parietal bridge", PARIETAL_Q_LOG_MODULE);

    parietal_quantum_bridge_t* bridge = (parietal_quantum_bridge_t*)nimcp_calloc(
        1, sizeof(parietal_quantum_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", PARIETAL_Q_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Store configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = parietal_region_quantum_default_config();
    }

    bridge->parietal = parietal;
    bridge->rng_state = bridge->config.seed;

    /* Allocate spatial states */
    bridge->spatial_state_capacity = bridge->config.spatial_grid_size * bridge->config.spatial_grid_size;
    bridge->spatial_states = (quantum_location_state_t*)nimcp_calloc(
        bridge->spatial_state_capacity, sizeof(quantum_location_state_t));
    if (!bridge->spatial_states) {
        LOG_ERROR("[%s] Failed to allocate spatial states", PARIETAL_Q_LOG_MODULE);
        parietal_region_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_region_quantum_default_config: bridge->spatial_states is NULL");
        return NULL;
    }

    /* Allocate attention superposition */
    bridge->attention_superposition = (quantum_location_state_t*)nimcp_calloc(
        bridge->config.max_targets, sizeof(quantum_location_state_t));
    if (!bridge->attention_superposition) {
        LOG_ERROR("[%s] Failed to allocate attention states", PARIETAL_Q_LOG_MODULE);
        parietal_region_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_region_quantum_default_config: bridge->attention_superposition is NULL");
        return NULL;
    }

    /* Allocate trajectory states */
    bridge->trajectory_state_capacity = 32;
    bridge->trajectory_states = (quantum_trajectory_candidate_t*)nimcp_calloc(
        bridge->trajectory_state_capacity, sizeof(quantum_trajectory_candidate_t));
    if (!bridge->trajectory_states) {
        LOG_ERROR("[%s] Failed to allocate trajectory states", PARIETAL_Q_LOG_MODULE);
        parietal_region_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_region_quantum_default_config: bridge->trajectory_states is NULL");
        return NULL;
    }

    /* Allocate quantum walk states */
    bridge->walk_states = (quantum_walk_state_t*)nimcp_calloc(
        bridge->spatial_state_capacity * 4, sizeof(quantum_walk_state_t));  /* 4 coin states */
    if (!bridge->walk_states) {
        LOG_ERROR("[%s] Failed to allocate walk states", PARIETAL_Q_LOG_MODULE);
        parietal_region_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_region_quantum_default_config: bridge->walk_states is NULL");
        return NULL;
    }

    /* Initialize frame superposition */
    memset(&bridge->current_frame_super, 0, sizeof(bridge->current_frame_super));
    bridge->current_frame_super.collapsed_frame = -1;

    LOG_INFO("[%s] Quantum parietal bridge created successfully", PARIETAL_Q_LOG_MODULE);
    return bridge;
}

void parietal_region_quantum_bridge_destroy(parietal_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "parietal_quantum");

    LOG_DEBUG("[%s] Destroying quantum parietal bridge", PARIETAL_Q_LOG_MODULE);

    if (bridge->spatial_states) nimcp_free(bridge->spatial_states);
    if (bridge->attention_superposition) nimcp_free(bridge->attention_superposition);
    if (bridge->trajectory_states) nimcp_free(bridge->trajectory_states);
    if (bridge->walk_states) nimcp_free(bridge->walk_states);

    nimcp_free(bridge);
}

bool parietal_region_quantum_bridge_is_enabled(const parietal_quantum_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->config.enabled;
}

void parietal_region_quantum_bridge_set_enabled(parietal_quantum_bridge_t* bridge, bool enabled) {
    if (!bridge) return;
    bridge->config.enabled = enabled;
    LOG_INFO("[%s] Quantum optimization %s", PARIETAL_Q_LOG_MODULE,
             enabled ? "enabled" : "disabled");
}

int parietal_region_quantum_bridge_reset(parietal_quantum_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->spatial_state_count = 0;
    bridge->attention_state_count = 0;
    bridge->attention_collapsed = false;
    bridge->trajectory_state_count = 0;
    bridge->trajectory_collapsed = false;
    bridge->walk_state_count = 0;
    bridge->walk_initialized = false;

    memset(&bridge->current_frame_super, 0, sizeof(bridge->current_frame_super));
    bridge->current_frame_super.collapsed_frame = -1;

    LOG_DEBUG("[%s] Bridge reset", PARIETAL_Q_LOG_MODULE);
    return 0;
}

/*=============================================================================
 * SPATIAL ATTENTION API
 *===========================================================================*/

int parietal_quantum_search_spatial(
    parietal_quantum_bridge_t* bridge,
    const float* salience_map,
    uint32_t grid_size,
    quantum_spatial_result_t* result) {

    if (!bridge || !salience_map || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_bridge_reset: required parameter is NULL (bridge, salience_map, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        LOG_DEBUG("[%s] Quantum disabled, using classical search", PARIETAL_Q_LOG_MODULE);
        /* Fall back to linear search */
        float max_salience = 0.0f;
        uint32_t max_idx = 0;
        uint32_t n = grid_size * grid_size;
        for (uint32_t i = 0; i < n; i++) {
            if (salience_map[i] > max_salience) {
                max_salience = salience_map[i];
                max_idx = i;
            }
        }
        result->locations_evaluated = n;
        result->grover_iterations_used = 0;
        result->satisfaction_probability = max_salience;
        result->search_speedup = 1.0f;
        return 0;
    }

    LOG_DEBUG("[%s] Quantum spatial search, grid size %u", PARIETAL_Q_LOG_MODULE, grid_size);

    uint32_t n = grid_size * grid_size;
    if (n > bridge->spatial_state_capacity) {
        n = bridge->spatial_state_capacity;
    }

    /* Initialize uniform superposition */
    float initial_amp = 1.0f / sqrtf((float)n);
    uint32_t num_marked = 0;

    for (uint32_t i = 0; i < n; i++) {
        bridge->spatial_states[i].location_id = i;
        bridge->spatial_states[i].amplitude_real = initial_amp;
        bridge->spatial_states[i].amplitude_imag = 0.0f;
        bridge->spatial_states[i].position.x = (float)(i % grid_size) / (float)grid_size - 0.5f;
        bridge->spatial_states[i].position.y = (float)(i / grid_size) / (float)grid_size - 0.5f;
        bridge->spatial_states[i].position.z = 0.0f;
        bridge->spatial_states[i].salience = salience_map[i];

        if (salience_map[i] >= bridge->config.min_salience_threshold) {
            num_marked++;
        }
    }
    bridge->spatial_state_count = n;

    /* Compute optimal iterations */
    uint32_t iterations = compute_grover_iterations(n, num_marked);
    if (iterations > bridge->config.max_grover_iterations) {
        iterations = bridge->config.max_grover_iterations;
    }

    /* Apply Grover iterations */
    for (uint32_t iter = 0; iter < iterations; iter++) {
        apply_salience_oracle(bridge->spatial_states, n, bridge->config.min_salience_threshold);
        apply_diffusion(bridge->spatial_states, n);
    }

    /* Measure to find result */
    uint32_t selected_idx = measure_state(bridge, bridge->spatial_states, n);

    /* Populate result */
    static quantum_spatial_candidate_t best_candidate;
    best_candidate.location_id = bridge->spatial_states[selected_idx].location_id;
    best_candidate.position = bridge->spatial_states[selected_idx].position;
    best_candidate.amplitude = sqrtf(amplitude_to_prob(
        bridge->spatial_states[selected_idx].amplitude_real,
        bridge->spatial_states[selected_idx].amplitude_imag));
    best_candidate.salience = bridge->spatial_states[selected_idx].salience;
    best_candidate.distance_from_focus = 0.0f;  /* Computed relative to current focus */
    best_candidate.combined_score = best_candidate.salience * best_candidate.amplitude;

    result->best_location = &best_candidate;
    result->locations_evaluated = n;
    result->grover_iterations_used = iterations;
    result->satisfaction_probability = best_candidate.amplitude;
    result->search_speedup = sqrtf((float)n);  /* Theoretical Grover speedup */

    /* Update statistics */
    bridge->stats.spatial_searches++;
    bridge->stats.avg_spatial_speedup = (bridge->stats.avg_spatial_speedup *
        (bridge->stats.spatial_searches - 1) + result->search_speedup) /
        bridge->stats.spatial_searches;

    if (result->satisfaction_probability >= 0.5f) {
        bridge->stats.successful_searches++;
    } else {
        bridge->stats.failed_searches++;
    }

    LOG_DEBUG("[%s] Spatial search complete: location=%u, salience=%.3f",
              PARIETAL_Q_LOG_MODULE, selected_idx, best_candidate.salience);

    return 0;
}

int parietal_quantum_superpose_attention(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_spatial_target_t* targets,
    uint32_t num_targets,
    const float* amplitudes) {

    if (!bridge || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_bridge_reset: required parameter is NULL (bridge, targets)");
        return -1;
    }
    if (!bridge->config.enable_superposition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_bridge_reset: bridge->config is NULL");
        return -1;
    }

    uint32_t count = num_targets;
    if (count > bridge->config.max_targets) {
        count = bridge->config.max_targets;
    }

    LOG_DEBUG("[%s] Creating attention superposition with %u targets",
              PARIETAL_Q_LOG_MODULE, count);

    float total_amp = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        bridge->attention_superposition[i].location_id = targets[i].target_id;
        bridge->attention_superposition[i].position = targets[i].position;
        bridge->attention_superposition[i].salience = targets[i].salience;

        if (amplitudes) {
            bridge->attention_superposition[i].amplitude_real = amplitudes[i];
        } else {
            bridge->attention_superposition[i].amplitude_real = 1.0f / sqrtf((float)count);
        }
        bridge->attention_superposition[i].amplitude_imag = 0.0f;
        total_amp += amplitude_to_prob(
            bridge->attention_superposition[i].amplitude_real,
            bridge->attention_superposition[i].amplitude_imag);
    }

    bridge->attention_state_count = count;
    bridge->attention_collapsed = false;

    /* Normalize */
    if (total_amp > 0.0f) {
        normalize_amplitudes(bridge->attention_superposition, count);
    }

    return 0;
}

int parietal_quantum_collapse_attention(
    parietal_quantum_bridge_t* bridge,
    parietal_cortex_spatial_target_t* selected_target) {

    if (!bridge || !selected_target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_bridge_reset: required parameter is NULL (bridge, selected_target)");
        return -1;
    }
    if (bridge->attention_state_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_region_quantum_bridge_reset: bridge->attention_state_count is zero");
        return -1;
    }

    LOG_DEBUG("[%s] Collapsing attention superposition", PARIETAL_Q_LOG_MODULE);

    uint32_t selected_idx = measure_state(bridge, bridge->attention_superposition,
                                           bridge->attention_state_count);

    selected_target->target_id = bridge->attention_superposition[selected_idx].location_id;
    selected_target->position = bridge->attention_superposition[selected_idx].position;
    selected_target->salience = bridge->attention_superposition[selected_idx].salience;
    selected_target->is_active = true;

    bridge->attention_collapsed = true;

    LOG_DEBUG("[%s] Attention collapsed to target %u",
              PARIETAL_Q_LOG_MODULE, selected_target->target_id);

    return 0;
}

/*=============================================================================
 * COORDINATE TRANSFORM API
 *===========================================================================*/

int parietal_quantum_frame_superposition(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* position,
    parietal_cortex_spatial_frame_t base_frame,
    quantum_frame_superposition_t* superposition) {

    if (!bridge || !position || !superposition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_frame_superposition: required parameter is NULL (bridge, position, superposition)");
        return -1;
    }
    if (!bridge->config.enable_superposition) {
        LOG_DEBUG("parietal_quantum_frame_superposition: superposition not enabled");
        return -1;
    }

    LOG_DEBUG("[%s] Creating frame superposition from frame %d", PARIETAL_Q_LOG_MODULE, base_frame);

    memset(superposition, 0, sizeof(quantum_frame_superposition_t));

    /* Create superposition across all frames */
    float amp = 1.0f / sqrtf((float)PARIETAL_CORTEX_SPATIAL_FRAME_COUNT);

    for (int i = 0; i < PARIETAL_CORTEX_SPATIAL_FRAME_COUNT; i++) {
        superposition->states[i].frame = (parietal_cortex_spatial_frame_t)i;
        superposition->states[i].amplitude = amp;

        /* Transform position to each frame using parietal adapter */
        if (bridge->parietal && i != (int)base_frame) {
            parietal_cortex_transform_coordinates(bridge->parietal, position, base_frame,
                                           (parietal_cortex_spatial_frame_t)i,
                                           &superposition->states[i].position);
        } else {
            superposition->states[i].position = *position;
        }
    }

    superposition->active_frames = PARIETAL_CORTEX_SPATIAL_FRAME_COUNT;
    superposition->collapsed_frame = (parietal_cortex_spatial_frame_t)(-1);
    superposition->total_amplitude = 1.0f;

    /* Store in bridge */
    bridge->current_frame_super = *superposition;

    bridge->stats.frame_transforms++;
    return 0;
}

int parietal_quantum_collapse_frame(
    parietal_quantum_bridge_t* bridge,
    const quantum_frame_superposition_t* superposition,
    parietal_cortex_spatial_frame_t target_frame,
    parietal_cortex_position_t* result) {

    if (!bridge || !superposition || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_collapse_frame: required parameter is NULL (bridge, superposition, result)");
        return -1;
    }
    if (target_frame >= PARIETAL_CORTEX_SPATIAL_FRAME_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_quantum_collapse_frame: target_frame out of range");
        return -1;
    }

    LOG_DEBUG("[%s] Collapsing frame superposition to frame %d",
              PARIETAL_Q_LOG_MODULE, target_frame);

    *result = superposition->states[target_frame].position;
    return 0;
}

int parietal_quantum_transform(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* position,
    parietal_cortex_spatial_frame_t from_frame,
    parietal_cortex_spatial_frame_t to_frame,
    parietal_cortex_position_t* result) {

    if (!bridge || !position || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_transform: required parameter is NULL (bridge, position, result)");
        return -1;
    }

    if (!bridge->config.enabled || !bridge->config.enable_superposition) {
        /* Direct transform without superposition */
        if (bridge->parietal) {
            return parietal_cortex_transform_coordinates(bridge->parietal, position,
                                                   from_frame, to_frame, result) ? 0 : -1;
        }
        *result = *position;
        return 0;
    }

    /* Create superposition, then collapse to target frame */
    quantum_frame_superposition_t super;
    int rc = parietal_quantum_frame_superposition(bridge, position, from_frame, &super);
    if (rc != 0) return rc;

    return parietal_quantum_collapse_frame(bridge, &super, to_frame, result);
}

/*=============================================================================
 * TRAJECTORY OPTIMIZATION API
 *===========================================================================*/

int parietal_quantum_optimize_trajectory(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    const parietal_cortex_position_t* target,
    const parietal_cortex_position_t* obstacles,
    uint32_t num_obstacles,
    quantum_trajectory_result_t* result) {

    if (!bridge || !start || !target || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_optimize_trajectory: required parameter is NULL (bridge, start, target, result)");
        return -1;
    }

    LOG_DEBUG("[%s] Optimizing trajectory with %u obstacles",
              PARIETAL_Q_LOG_MODULE, num_obstacles);

    /* Generate trajectory candidates */
    uint32_t num_candidates = 16;
    if (num_candidates > bridge->trajectory_state_capacity) {
        num_candidates = bridge->trajectory_state_capacity;
    }

    float initial_amp = 1.0f / sqrtf((float)num_candidates);

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_trajectory_candidate_t* traj = &bridge->trajectory_states[i];
        traj->trajectory_id = i;
        traj->start = *start;
        traj->end = *target;
        traj->amplitude = initial_amp;

        /* Generate waypoints with variation */
        traj->waypoint_count = 1 + (i % 3);  /* 1-3 waypoints */
        for (uint32_t w = 0; w < traj->waypoint_count; w++) {
            float t = (float)(w + 1) / (float)(traj->waypoint_count + 1);
            float noise = (quantum_random(bridge) - 0.5f) * 0.2f;
            traj->waypoints[w].x = start->x + t * (target->x - start->x) + noise;
            traj->waypoints[w].y = start->y + t * (target->y - start->y) + noise;
            traj->waypoints[w].z = start->z + t * (target->z - start->z);
        }

        /* Compute path length */
        traj->path_length = 0.0f;
        parietal_cortex_position_t prev = *start;
        for (uint32_t w = 0; w < traj->waypoint_count; w++) {
            float dx = traj->waypoints[w].x - prev.x;
            float dy = traj->waypoints[w].y - prev.y;
            float dz = traj->waypoints[w].z - prev.z;
            traj->path_length += sqrtf(dx*dx + dy*dy + dz*dz);
            prev = traj->waypoints[w];
        }
        float dx = target->x - prev.x;
        float dy = target->y - prev.y;
        float dz = target->z - prev.z;
        traj->path_length += sqrtf(dx*dx + dy*dy + dz*dz);

        /* Compute collision risk */
        traj->collision_risk = 0.0f;
        for (uint32_t o = 0; o < num_obstacles && obstacles; o++) {
            /* Simple proximity check */
            for (uint32_t w = 0; w < traj->waypoint_count; w++) {
                dx = traj->waypoints[w].x - obstacles[o].x;
                dy = traj->waypoints[w].y - obstacles[o].y;
                dz = traj->waypoints[w].z - obstacles[o].z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                if (dist < 0.1f) {
                    traj->collision_risk += 0.5f;
                } else if (dist < 0.2f) {
                    traj->collision_risk += 0.2f;
                }
            }
        }
        if (traj->collision_risk > 1.0f) traj->collision_risk = 1.0f;

        /* Compute smoothness (fewer waypoints = smoother) */
        traj->smoothness = 1.0f - (float)traj->waypoint_count / 5.0f;

        /* Energy cost (path length + collision penalty) */
        traj->energy_cost = traj->path_length * (1.0f + traj->collision_risk);
    }

    bridge->trajectory_state_count = num_candidates;

    /* Apply amplitude amplification based on quality */
    for (uint32_t i = 0; i < num_candidates; i++) {
        float quality = (1.0f - bridge->trajectory_states[i].energy_cost / 2.0f) *
                       (1.0f - bridge->trajectory_states[i].collision_risk) *
                       bridge->trajectory_states[i].smoothness;
        if (quality < 0.0f) quality = 0.0f;
        bridge->trajectory_states[i].amplitude *= (1.0f + quality);
    }

    /* Find best trajectory */
    float max_amp = 0.0f;
    uint32_t best_idx = 0;
    for (uint32_t i = 0; i < num_candidates; i++) {
        if (bridge->trajectory_states[i].amplitude > max_amp) {
            max_amp = bridge->trajectory_states[i].amplitude;
            best_idx = i;
        }
    }

    result->best_trajectory = &bridge->trajectory_states[best_idx];
    result->trajectories_evaluated = num_candidates;
    result->optimization_score = max_amp;
    result->grover_iterations_used = 1;  /* Simplified */

    bridge->stats.trajectory_optimizations++;

    LOG_DEBUG("[%s] Trajectory optimization complete: best=%u, score=%.3f",
              PARIETAL_Q_LOG_MODULE, best_idx, max_amp);

    return 0;
}

int parietal_quantum_superpose_trajectories(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    const parietal_cortex_position_t* target,
    uint32_t num_candidates) {

    if (!bridge || !start || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_superpose_trajectories: required parameter is NULL (bridge, start, target)");
        return -1;
    }

    quantum_trajectory_result_t result;
    int rc = parietal_quantum_optimize_trajectory(bridge, start, target, NULL, 0, &result);
    if (rc != 0) return rc;

    bridge->trajectory_collapsed = false;
    return 0;
}

int parietal_quantum_collapse_trajectory(
    parietal_quantum_bridge_t* bridge,
    quantum_trajectory_candidate_t* trajectory) {

    if (!bridge || !trajectory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_collapse_trajectory: required parameter is NULL (bridge, trajectory)");
        return -1;
    }
    if (bridge->trajectory_state_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "parietal_quantum_collapse_trajectory: trajectory_state_count is zero");
        return -1;
    }

    /* Select based on amplitude */
    float max_amp = 0.0f;
    uint32_t best_idx = 0;
    for (uint32_t i = 0; i < bridge->trajectory_state_count; i++) {
        if (bridge->trajectory_states[i].amplitude > max_amp) {
            max_amp = bridge->trajectory_states[i].amplitude;
            best_idx = i;
        }
    }

    *trajectory = bridge->trajectory_states[best_idx];
    bridge->trajectory_collapsed = true;

    return 0;
}

/*=============================================================================
 * QUANTUM WALK API
 *===========================================================================*/

int parietal_quantum_walk_init(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    uint32_t graph_size) {

    if (!bridge || !start) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_walk_init: required parameter is NULL (bridge, start)");
        return -1;
    }
    if (!bridge->config.enable_quantum_walk) {
        LOG_DEBUG("parietal_quantum_walk_init: quantum walk not enabled");
        return -1;
    }

    LOG_DEBUG("[%s] Initializing quantum walk, graph size %u",
              PARIETAL_Q_LOG_MODULE, graph_size);

    bridge->walk_graph_size = graph_size;
    uint32_t total_states = graph_size * graph_size * 4;  /* 4 coin states */

    if (total_states > bridge->spatial_state_capacity * 4) {
        total_states = bridge->spatial_state_capacity * 4;
    }

    /* Initialize walker at start position with coin in superposition */
    memset(bridge->walk_states, 0, total_states * sizeof(quantum_walk_state_t));

    /* Find start grid cell */
    uint32_t start_x = (uint32_t)((start->x + 0.5f) * graph_size);
    uint32_t start_y = (uint32_t)((start->y + 0.5f) * graph_size);
    if (start_x >= graph_size) start_x = graph_size - 1;
    if (start_y >= graph_size) start_y = graph_size - 1;

    uint32_t start_idx = start_y * graph_size + start_x;

    /* Initialize with equal superposition of coin states */
    float amp = 0.5f;  /* 1/sqrt(4) = 0.5 */
    for (int c = 0; c < 4; c++) {
        bridge->walk_states[start_idx * 4 + c].position = *start;
        bridge->walk_states[start_idx * 4 + c].amplitude_real = amp;
        bridge->walk_states[start_idx * 4 + c].amplitude_imag = 0.0f;
        bridge->walk_states[start_idx * 4 + c].coin_state = (uint8_t)c;
    }

    bridge->walk_state_count = 4;
    bridge->walk_initialized = true;

    bridge->stats.quantum_walks++;
    return 0;
}

int parietal_quantum_walk_step(
    parietal_quantum_bridge_t* bridge,
    uint32_t steps) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;


    }
    if (!bridge->walk_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "parietal_quantum_walk_step: walk not initialized");
        return -1;
    }

    LOG_DEBUG("[%s] Quantum walk step, %u steps", PARIETAL_Q_LOG_MODULE, steps);

    uint32_t n = bridge->walk_graph_size;

    for (uint32_t s = 0; s < steps; s++) {
        /* Apply Hadamard coin to each position */
        for (uint32_t i = 0; i < n * n; i++) {
            /* Get amplitudes for all coin states at this position */
            float a0_r = bridge->walk_states[i * 4 + 0].amplitude_real;
            float a1_r = bridge->walk_states[i * 4 + 1].amplitude_real;
            float a2_r = bridge->walk_states[i * 4 + 2].amplitude_real;
            float a3_r = bridge->walk_states[i * 4 + 3].amplitude_real;

            /* Apply 4D Hadamard (simplified Grover diffusion coin) */
            float mean = (a0_r + a1_r + a2_r + a3_r) / 4.0f;
            bridge->walk_states[i * 4 + 0].amplitude_real = 2.0f * mean - a0_r;
            bridge->walk_states[i * 4 + 1].amplitude_real = 2.0f * mean - a1_r;
            bridge->walk_states[i * 4 + 2].amplitude_real = 2.0f * mean - a2_r;
            bridge->walk_states[i * 4 + 3].amplitude_real = 2.0f * mean - a3_r;
        }

        /* Apply shift operator (would need temp buffer in real implementation) */
        /* Simplified: just update positions based on coin state */
        for (uint32_t i = 0; i < n * n * 4; i++) {
            float step_size = 1.0f / (float)n;
            switch (bridge->walk_states[i].coin_state) {
                case 0: bridge->walk_states[i].position.x -= step_size; break;
                case 1: bridge->walk_states[i].position.x += step_size; break;
                case 2: bridge->walk_states[i].position.y -= step_size; break;
                case 3: bridge->walk_states[i].position.y += step_size; break;
            }
        }
    }

    return 0;
}

int parietal_quantum_walk_measure(
    parietal_quantum_bridge_t* bridge,
    parietal_cortex_position_t* position,
    float* probability) {

    if (!bridge || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_quantum_walk_measure: required parameter is NULL (bridge, position)");
        return -1;
    }
    if (!bridge->walk_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "parietal_quantum_walk_measure: walk not initialized");
        return -1;
    }

    uint32_t n = bridge->walk_graph_size;
    uint32_t total = n * n * 4;

    /* Compute total probability and select */
    float total_prob = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        total_prob += amplitude_to_prob(
            bridge->walk_states[i].amplitude_real,
            bridge->walk_states[i].amplitude_imag);
    }

    float r = quantum_random(bridge) * total_prob;
    float cumulative = 0.0f;
    uint32_t selected = 0;

    for (uint32_t i = 0; i < total; i++) {
        cumulative += amplitude_to_prob(
            bridge->walk_states[i].amplitude_real,
            bridge->walk_states[i].amplitude_imag);
        if (r <= cumulative) {
            selected = i;
            break;
        }
    }

    *position = bridge->walk_states[selected].position;
    if (probability) {
        float amp = amplitude_to_prob(
            bridge->walk_states[selected].amplitude_real,
            bridge->walk_states[selected].amplitude_imag);
        *probability = (fabsf(total_prob) > 1e-10f) ? (amp / total_prob) : 0.0f;
    }

    return 0;
}

int parietal_region_quantum_walk_search(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    const parietal_cortex_position_t* target,
    uint32_t max_steps,
    quantum_spatial_result_t* result) {

    if (!bridge || !start || !target || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_walk_search: required parameter is NULL (bridge, start, target, result)");
        return -1;
    }

    LOG_DEBUG("[%s] Quantum walk search", PARIETAL_Q_LOG_MODULE);

    int rc = parietal_quantum_walk_init(bridge, start, bridge->config.spatial_grid_size);
    if (rc != 0) return rc;

    /* Compute optimal steps (sqrt(N)) */
    uint32_t n = bridge->walk_graph_size * bridge->walk_graph_size;
    uint32_t optimal_steps = (uint32_t)sqrtf((float)n);
    if (optimal_steps > max_steps) optimal_steps = max_steps;

    /* Mark target in walk */
    uint32_t target_x = (uint32_t)((target->x + 0.5f) * bridge->walk_graph_size);
    uint32_t target_y = (uint32_t)((target->y + 0.5f) * bridge->walk_graph_size);
    if (target_x >= bridge->walk_graph_size) target_x = bridge->walk_graph_size - 1;
    if (target_y >= bridge->walk_graph_size) target_y = bridge->walk_graph_size - 1;

    /* Apply walk steps with target marking */
    for (uint32_t s = 0; s < optimal_steps; s++) {
        parietal_quantum_walk_step(bridge, 1);

        /* Apply phase flip at target (marking) */
        uint32_t target_idx = target_y * bridge->walk_graph_size + target_x;
        for (int c = 0; c < 4; c++) {
            bridge->walk_states[target_idx * 4 + c].amplitude_real *= -1.0f;
        }
    }

    /* Measure result */
    parietal_cortex_position_t measured_pos;
    float prob;
    parietal_quantum_walk_measure(bridge, &measured_pos, &prob);

    static quantum_spatial_candidate_t candidate;
    candidate.position = measured_pos;
    candidate.amplitude = sqrtf(prob);
    candidate.salience = 1.0f - sqrtf(
        (measured_pos.x - target->x) * (measured_pos.x - target->x) +
        (measured_pos.y - target->y) * (measured_pos.y - target->y));
    if (candidate.salience < 0.0f) candidate.salience = 0.0f;

    result->best_location = &candidate;
    result->locations_evaluated = n;
    result->grover_iterations_used = optimal_steps;
    result->satisfaction_probability = prob;
    result->search_speedup = sqrtf((float)n);

    return 0;
}

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

int parietal_region_quantum_get_stats(
    const parietal_quantum_bridge_t* bridge,
    parietal_quantum_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void parietal_region_quantum_reset_stats(parietal_quantum_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

int parietal_region_quantum_get_config(
    const parietal_quantum_bridge_t* bridge,
    parietal_region_quantum_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_region_quantum_reset_stats: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
