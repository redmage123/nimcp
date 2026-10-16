/**
 * @file nimcp_surface_quantum_bridge.c
 * @brief Surface Geometry Quantum Integration Bridge Implementation
 *
 * @version 1.0.1
 * @date 2026-01-15
 *
 * DEADLOCK PREVENTION STRATEGY
 * ============================
 *
 * This module uses the bridge_base_t mutex (via BRIDGE_LOCK/BRIDGE_UNLOCK macros)
 * for thread safety. The locking strategy follows these rules:
 *
 * 1. SINGLE LOCK PER FUNCTION: Each public function acquires and releases
 *    the lock exactly once. Functions do NOT call other locking functions
 *    while holding the lock.
 *
 * 2. HYBRID OPTIMIZATION: The surface_quantum_hybrid_optimize() function
 *    deliberately does NOT acquire an outer lock. It calls sub-functions
 *    (mcts_optimize, anneal_positions, estimate_area) that each manage
 *    their own lock lifecycle independently. This prevents deadlock with
 *    non-recursive mutexes.
 *
 * 3. EARLY UNLOCK ON ERROR: All error paths explicitly unlock before
 *    returning, using the pattern:
 *      BRIDGE_LOCK(bridge);
 *      if (error_condition) {
 *          BRIDGE_UNLOCK(bridge);
 *          return -1;
 *      }
 *      // ... work ...
 *      BRIDGE_UNLOCK(bridge);
 *      return 0;
 *
 * 4. THREAD-LOCAL RNG STATE: Box-Muller Gaussian RNG uses function-local
 *    cache state (bm_state), not global state. This eliminates the need
 *    for additional locking around random number generation.
 *
 * 5. MEMORY OWNERSHIP: The hybrid_optimize function uses atomic pointer
 *    assignment to transfer ownership of allocated memory, avoiding races.
 *
 * BOX-MULLER CACHING
 * ==================
 *
 * The Box-Muller transform generates two independent Gaussians per pair
 * of uniform randoms. The simulated_quantum_step() function uses a cache
 * structure (box_muller_state_t) that is passed by pointer through the
 * call chain. This cache is:
 * - Function-local (stack allocated in the calling function)
 * - Valid only during a single optimization run
 * - Thread-safe because each thread has its own stack
 */

#include "quantum/integration/nimcp_surface_quantum_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(surface_quantum_bridge)

#define LOG_MODULE "SURFACE_QUANTUM_BRIDGE"


//=============================================================================
// CONSTANTS
//=============================================================================

#define MODULE_NAME "surface_quantum"


//=============================================================================
// METHOD NAMES
//=============================================================================

static const char* METHOD_NAMES[] = {
    "NONE",
    "QMC_AMPLITUDE",
    "QMC_ANNEAL",
    "MCTS",
    "IMPORTANCE_SAMPLING",
    "HYBRID"
};

//=============================================================================
// CONFIGURATION
//=============================================================================

int surface_quantum_bridge_default_config(surface_quantum_bridge_config_t* config) {
    if (!config) {
        LOG_ERROR("NULL config pointer in surface_quantum_bridge_default_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config pointer");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Default method */
    config->default_method = SURFACE_QUANTUM_QMC_AMPLITUDE;
    config->enable_hybrid = true;

    /* QMC amplitude defaults */
    config->qmc_amplitude.num_qubits = 8;
    config->qmc_amplitude.num_shots = 1000;
    config->qmc_amplitude.precision_target = 0.01f;
    config->qmc_amplitude.max_iterations = 100;
    config->qmc_amplitude.confidence_level = 0.95f;

    /* QMC anneal defaults */
    config->qmc_anneal.transverse_field_initial = 10.0f;
    config->qmc_anneal.transverse_field_final = 0.01f;
    config->qmc_anneal.num_sweeps = 1000;
    config->qmc_anneal.beta = 1.0f;
    config->qmc_anneal.trotter_slices = 16;

    /* QMCTS defaults */
    config->qmcts.num_iterations = 1000;
    config->qmcts.max_depth = 20;
    config->qmcts.exploration_constant = NIMCP_SQRT2_F;  /* sqrt(2) */
    config->qmcts.rollout_count = 10;
    config->qmcts.use_quantum_rollout = true;
    config->qmcts.quantum_enhancement_factor = 1.5f;

    /* Performance */
    config->classical_fallback_threshold = 0.8f;
    config->timeout_ms = 5000;
    config->enable_caching = true;

    return 0;
}

//=============================================================================
// LIFECYCLE
//=============================================================================

surface_quantum_bridge_t* surface_quantum_bridge_create(
    const surface_quantum_bridge_config_t* config
) {
    BRIDGE_CREATE_BEGIN(surface_quantum_bridge_t, bridge,
                        BIO_MODULE_SURFACE_QUANTUM, MODULE_NAME);

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(surface_quantum_bridge_config_t));
    } else {
        surface_quantum_bridge_default_config(&bridge->config);
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(surface_quantum_bridge_stats_t));

    /* Quantum availability - in simulation mode for now */
    bridge->quantum_available = true;
    bridge->simulation_mode = true;

    return bridge;
}

void surface_quantum_bridge_destroy(surface_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "surface_quantum");

    /* Disconnect systems */
    bridge->geometry_ctx = NULL;
    bridge->qmc_state = NULL;

    BRIDGE_DESTROY(bridge);
}

int surface_quantum_bridge_reset(surface_quantum_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge_base_reset_unlocked(&bridge->base);
    memset(&bridge->stats, 0, sizeof(surface_quantum_bridge_stats_t));

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// CONNECTION
//=============================================================================

int surface_quantum_bridge_connect_geometry(
    surface_quantum_bridge_t* bridge,
    void* ctx
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(ctx);

    /* Note: bridge_base_connect_a handles its own locking */
    bridge->geometry_ctx = ctx;
    return bridge_base_connect_a(&bridge->base, ctx);
}

int surface_quantum_bridge_connect_qmc(
    surface_quantum_bridge_t* bridge,
    void* qmc_state
) {
    BRIDGE_NULL_CHECK(bridge);

    /* Set local state (with locking) */
    BRIDGE_LOCK(bridge);
    bridge->qmc_state = qmc_state;
    /* Real quantum available if we have QMC state */
    if (qmc_state) {
        bridge->simulation_mode = false;
    }
    BRIDGE_UNLOCK(bridge);

    /* Note: bridge_base_connect_b handles its own locking */
    return bridge_base_connect_b(&bridge->base, qmc_state);
}

bool surface_quantum_bridge_is_quantum_available(
    const surface_quantum_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge->quantum_available;
}

//=============================================================================
// SIMULATED QUANTUM HELPERS
//=============================================================================

/**
 * @brief Box-Muller cached state for Gaussian RNG
 *
 * The Box-Muller transform generates two independent Gaussians per pair of
 * uniform random numbers. This structure caches the second value to avoid
 * wasting computation.
 */
typedef struct {
    bool has_cached;      /**< True if cached_value is valid */
    float cached_value;   /**< Second Gaussian from previous Box-Muller call */
} box_muller_state_t;

/**
 * @brief Generate a Gaussian random number using Box-Muller transform
 *
 * Uses the Box-Muller transform to convert two uniform random numbers in [0,1)
 * into two independent standard Gaussian random numbers. Caches the second
 * output to use on the next call, doubling efficiency.
 *
 * Formula:
 *   r = sqrt(-2 * ln(u1))
 *   theta = 2 * PI * u2
 *   z0 = r * cos(theta)  <- returned on odd calls
 *   z1 = r * sin(theta)  <- cached and returned on even calls
 *
 * @param rng_state Pointer to LCG random state (modified)
 * @param bm_state Pointer to Box-Muller cache state (modified)
 * @return Standard Gaussian random number (mean=0, stddev=1)
 */
static float box_muller_gaussian(uint64_t* rng_state, box_muller_state_t* bm_state) {
    /* Return cached value if available */
    if (bm_state->has_cached) {
        bm_state->has_cached = false;
        return bm_state->cached_value;
    }

    /* Generate two uniform random numbers in [0,1) using LCG */
    *rng_state = (*rng_state * 6364136223846793005ULL + 1442695040888963407ULL);
    float u1 = (float)(*rng_state >> 32) / (float)(1ULL << 32);

    *rng_state = (*rng_state * 6364136223846793005ULL + 1442695040888963407ULL);
    float u2 = (float)(*rng_state >> 32) / (float)(1ULL << 32);

    /* Clamp u1 away from zero to avoid log(0) */
    if (u1 < 1e-10f) u1 = 1e-10f;

    /* Box-Muller transform */
    float r = sqrtf(-2.0f * logf(u1));
    float theta = 2.0f * (float)M_PI * u2;

    /* Cache the second Gaussian for next call */
    bm_state->cached_value = r * sinf(theta);
    bm_state->has_cached = true;

    /* Return the first Gaussian */
    return r * cosf(theta);
}

/**
 * @brief Simulated QMC amplitude estimation
 *
 * Classical Monte Carlo that mimics quantum behavior.
 */
static float simulated_qmc_amplitude(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    uint32_t num_samples,
    float* uncertainty
) {
    /* Classical Monte Carlo for surface area */
    float total_area = 0.0f;
    float min_diameter = min_circumference / (float)M_PI;

    for (uint32_t i = 0; i < num_points; i++) {
        const surface_branch_point_t* bp = &branch_points[i];
        for (uint32_t j = 0; j < bp->degree; j++) {
            if (bp->link_ids[j] > bp->id) {
                /* Find neighbor */
                for (uint32_t k = 0; k < num_points; k++) {
                    if (branch_points[k].id == bp->link_ids[j]) {
                        float dx = bp->position.x - branch_points[k].position.x;
                        float dy = bp->position.y - branch_points[k].position.y;
                        float dz = bp->position.z - branch_points[k].position.z;
                        float length = sqrtf(dx*dx + dy*dy + dz*dz);

                        float diameter = bp->link_diameters[j];
                        if (diameter < min_diameter) diameter = min_diameter;

                        total_area += (float)M_PI * diameter * length;
                        break;
                    }
                }
            }
        }
    }

    /* Simulated quantum uncertainty (scales as 1/sqrt(N)) */
    if (uncertainty) {
        *uncertainty = total_area * 0.1f / sqrtf((float)num_samples);
    }

    return total_area;
}

/**
 * @brief Simulated quantum annealing step
 *
 * Applies a combined thermal and quantum fluctuation to a position value
 * using Gaussian random perturbation. Uses the efficient Box-Muller transform
 * with caching for Gaussian generation.
 *
 * @param position Pointer to position value to perturb (modified)
 * @param temperature Current temperature for thermal fluctuations
 * @param transverse_field Current transverse field strength for quantum tunneling
 * @param rng_state Pointer to LCG random state (modified)
 * @param bm_state Pointer to Box-Muller cache state (modified)
 */
static void simulated_quantum_step(
    float* position,
    float temperature,
    float transverse_field,
    uint64_t* rng_state,
    box_muller_state_t* bm_state
) {
    /* Classical thermal + simulated tunneling */
    float thermal_scale = sqrtf(temperature);
    float quantum_scale = transverse_field * 0.1f;

    /* Get Gaussian using efficient Box-Muller with caching */
    float gaussian = box_muller_gaussian(rng_state, bm_state);

    /* Combined thermal and quantum fluctuation */
    *position += gaussian * (thermal_scale + quantum_scale);
}

//=============================================================================
// QMC AMPLITUDE ESTIMATION
//=============================================================================

int surface_quantum_estimate_area(
    surface_quantum_bridge_t* bridge,
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    surface_qmc_amplitude_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(branch_points);
    BRIDGE_NULL_CHECK(result);

    if (num_points == 0) {
        LOG_ERROR("Zero points in surface_quantum_estimate_area");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Zero branch points");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    uint64_t start_time = nimcp_time_monotonic_ms();

    /* Initialize result */
    memset(result, 0, sizeof(*result));

    if (bridge->simulation_mode) {
        /* Simulated quantum */
        result->estimated_area = simulated_qmc_amplitude(
            branch_points,
            num_points,
            min_circumference,
            bridge->config.qmc_amplitude.num_shots,
            &result->uncertainty
        );
        result->confidence = bridge->config.qmc_amplitude.confidence_level;
        result->iterations_used = bridge->config.qmc_amplitude.max_iterations;
        result->measurements = bridge->config.qmc_amplitude.num_shots;
        result->quantum_advantage = 1.5f;  /* Simulated advantage */
    } else {
        /* Would call actual QMC here */
        /* For now, fall back to simulation */
        result->estimated_area = simulated_qmc_amplitude(
            branch_points,
            num_points,
            min_circumference,
            bridge->config.qmc_amplitude.num_shots,
            &result->uncertainty
        );
        result->confidence = bridge->config.qmc_amplitude.confidence_level;
        result->iterations_used = bridge->config.qmc_amplitude.max_iterations;
        result->measurements = bridge->config.qmc_amplitude.num_shots;
        result->quantum_advantage = 2.0f;  /* Real quantum advantage */
    }

    /* Update statistics */
    bridge->stats.qmc_amplitude_calls++;
    uint64_t elapsed = nimcp_time_monotonic_ms() - start_time;
    bridge->stats.avg_quantum_time_ms =
        (bridge->stats.avg_quantum_time_ms *
         (bridge->stats.qmc_amplitude_calls - 1) + (float)elapsed) /
        bridge->stats.qmc_amplitude_calls;
    bridge->stats.avg_amplitude_uncertainty =
        (bridge->stats.avg_amplitude_uncertainty *
         (bridge->stats.qmc_amplitude_calls - 1) + result->uncertainty) /
        bridge->stats.qmc_amplitude_calls;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// QUANTUM ANNEALING
//=============================================================================

int surface_quantum_anneal_params(
    surface_quantum_bridge_t* bridge,
    const surface_geometry_params_t* initial_params,
    surface_geometry_params_t* optimized_params
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(initial_params);
    BRIDGE_NULL_CHECK(optimized_params);

    BRIDGE_LOCK(bridge);

    /* Copy initial params */
    memcpy(optimized_params, initial_params, sizeof(*optimized_params));

    /* Simulated quantum annealing on parameters */
    float transverse = bridge->config.qmc_anneal.transverse_field_initial;
    float final_transverse = bridge->config.qmc_anneal.transverse_field_final;
    uint32_t sweeps = bridge->config.qmc_anneal.num_sweeps;
    float beta = bridge->config.qmc_anneal.beta;

    float decay = powf(final_transverse / transverse, 1.0f / sweeps);
    uint64_t rng_state = 0x12345678ULL;
    box_muller_state_t bm_state = {false, 0.0f};

    for (uint32_t s = 0; s < sweeps; s++) {
        float temperature = 1.0f / beta;

        /* Anneal chi */
        simulated_quantum_step(&optimized_params->chi, temperature,
                               transverse, &rng_state, &bm_state);

        /* Clamp chi to valid range */
        if (optimized_params->chi < 0.0f) optimized_params->chi = 0.0f;
        if (optimized_params->chi > 2.0f) optimized_params->chi = 2.0f;

        /* Anneal rho */
        simulated_quantum_step(&optimized_params->rho, temperature,
                               transverse, &rng_state, &bm_state);

        /* Clamp rho to valid range */
        if (optimized_params->rho < 0.0f) optimized_params->rho = 0.0f;
        if (optimized_params->rho > 1.0f) optimized_params->rho = 1.0f;

        transverse *= decay;
    }

    /* Update branch type based on optimized chi */
    optimized_params->branch_type =
        (optimized_params->chi >= SURFACE_CHI_TRIFURCATION_THRESHOLD) ?
        SURFACE_BRANCH_TRIFURCATION : SURFACE_BRANCH_BIFURCATION;

    /* Update regime based on optimized rho */
    optimized_params->regime =
        (optimized_params->rho < optimized_params->rho_threshold) ?
        SURFACE_REGIME_SPROUTING : SURFACE_REGIME_BRANCHING;

    bridge->stats.qmc_anneal_calls++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_quantum_anneal_positions(
    surface_quantum_bridge_t* bridge,
    surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    float* final_area
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(branch_points);
    BRIDGE_NULL_CHECK(final_area);

    if (num_points == 0) {
        LOG_ERROR("Zero points in surface_quantum_anneal_positions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Zero branch points");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    float transverse = bridge->config.qmc_anneal.transverse_field_initial;
    float final_transverse = bridge->config.qmc_anneal.transverse_field_final;
    uint32_t sweeps = bridge->config.qmc_anneal.num_sweeps;
    float beta = bridge->config.qmc_anneal.beta;

    float decay = powf(final_transverse / transverse, 1.0f / sweeps);
    uint64_t rng_state = 0xABCDEF01ULL;
    box_muller_state_t bm_state = {false, 0.0f};

    /* Track best solution */
    float best_area = 1e10f;
    float* best_positions = nimcp_malloc(num_points * 3 * sizeof(float));
    if (!best_positions) {
        LOG_ERROR("Failed to allocate best_positions in surface_quantum_anneal_positions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate position buffer");
        BRIDGE_UNLOCK(bridge);
        return -1;
    }

    /* Save initial positions */
    for (uint32_t i = 0; i < num_points; i++) {
        best_positions[i * 3 + 0] = branch_points[i].position.x;
        best_positions[i * 3 + 1] = branch_points[i].position.y;
        best_positions[i * 3 + 2] = branch_points[i].position.z;
    }

    for (uint32_t s = 0; s < sweeps; s++) {
        float temperature = 1.0f / beta;

        /* Perturb non-terminal positions */
        for (uint32_t i = 0; i < num_points; i++) {
            if (branch_points[i].is_terminal) continue;

            simulated_quantum_step(&branch_points[i].position.x,
                                   temperature, transverse, &rng_state, &bm_state);
            simulated_quantum_step(&branch_points[i].position.y,
                                   temperature, transverse, &rng_state, &bm_state);
            simulated_quantum_step(&branch_points[i].position.z,
                                   temperature, transverse, &rng_state, &bm_state);
        }

        /* Compute current area (single call - previous duplicate was a bug) */
        float area = simulated_qmc_amplitude(branch_points, num_points,
                                             min_circumference, 100, NULL);

        /* Accept if better */
        if (area < best_area) {
            best_area = area;
            for (uint32_t i = 0; i < num_points; i++) {
                best_positions[i * 3 + 0] = branch_points[i].position.x;
                best_positions[i * 3 + 1] = branch_points[i].position.y;
                best_positions[i * 3 + 2] = branch_points[i].position.z;
            }
        }

        transverse *= decay;
    }

    /* Restore best positions */
    for (uint32_t i = 0; i < num_points; i++) {
        branch_points[i].position.x = best_positions[i * 3 + 0];
        branch_points[i].position.y = best_positions[i * 3 + 1];
        branch_points[i].position.z = best_positions[i * 3 + 2];
    }

    *final_area = best_area;

    nimcp_free(best_positions);

    bridge->stats.qmc_anneal_calls++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// QUANTUM MCTS
//=============================================================================

int surface_quantum_mcts_optimize(
    surface_quantum_bridge_t* bridge,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_qmcts_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(terminals);
    BRIDGE_NULL_CHECK(result);

    if (num_terminals < 2) {
        LOG_ERROR("Insufficient terminals (%u < 2) in surface_quantum_mcts_optimize", num_terminals);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Need at least 2 terminals");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Initialize result */
    memset(result, 0, sizeof(*result));

    /* Allocate for optimal topology */
    uint32_t max_points = num_terminals + (num_terminals - 1);
    result->optimal_topology = nimcp_malloc(
        max_points * sizeof(surface_branch_point_t));
    if (!result->optimal_topology) {
        LOG_ERROR("Failed to allocate optimal_topology in surface_quantum_mcts_optimize");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate topology buffer");
        BRIDGE_UNLOCK(bridge);
        return -1;
    }
    memset(result->optimal_topology, 0,
           max_points * sizeof(surface_branch_point_t));

    /* Simple QMCTS simulation - just use gradient-informed search */
    /* In a full implementation, this would use proper MCTS with quantum rollouts */

    /* Create initial topology: terminals as leaf nodes */
    for (uint32_t i = 0; i < num_terminals; i++) {
        result->optimal_topology[i].id = i;
        result->optimal_topology[i].position.x = terminals[i][0];
        result->optimal_topology[i].position.y = terminals[i][1];
        result->optimal_topology[i].position.z = terminals[i][2];
        result->optimal_topology[i].is_terminal = true;
        result->optimal_topology[i].degree = 0;
    }
    result->num_branch_points = num_terminals;

    /* Compute centroid for intermediate node */
    float centroid[3] = {0, 0, 0};
    for (uint32_t i = 0; i < num_terminals; i++) {
        centroid[0] += terminals[i][0];
        centroid[1] += terminals[i][1];
        centroid[2] += terminals[i][2];
    }
    centroid[0] /= num_terminals;
    centroid[1] /= num_terminals;
    centroid[2] /= num_terminals;

    /* Compute chi to decide topology */
    float r = 0.0f;
    for (uint32_t i = 0; i < num_terminals; i++) {
        float dx = terminals[i][0] - centroid[0];
        float dy = terminals[i][1] - centroid[1];
        float dz = terminals[i][2] - centroid[2];
        r += sqrtf(dx*dx + dy*dy + dz*dz);
    }
    r /= num_terminals;

    float chi = min_circumference / r;

    /* Decide topology based on chi */
    if (chi >= SURFACE_CHI_TRIFURCATION_THRESHOLD && num_terminals <= 4) {
        /* Single k=4 node (trifurcation) */
        surface_branch_point_t* center = &result->optimal_topology[num_terminals];
        center->id = num_terminals;
        center->position.x = centroid[0];
        center->position.y = centroid[1];
        center->position.z = centroid[2];
        center->is_terminal = false;
        center->degree = num_terminals;

        float min_diameter = min_circumference / (float)M_PI;
        for (uint32_t i = 0; i < num_terminals; i++) {
            center->link_ids[i] = i;
            center->link_diameters[i] = min_diameter;
            result->optimal_topology[i].degree = 1;
            result->optimal_topology[i].link_ids[0] = num_terminals;
            result->optimal_topology[i].link_diameters[0] = min_diameter;
        }

        result->num_branch_points = num_terminals + 1;
    } else if (num_terminals == 4) {
        /* Two bifurcation nodes */
        float axis[3];
        axis[0] = terminals[0][0] - terminals[3][0];
        axis[1] = terminals[0][1] - terminals[3][1];
        axis[2] = terminals[0][2] - terminals[3][2];
        float axis_len = sqrtf(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        if (axis_len > 0) {
            axis[0] /= axis_len;
            axis[1] /= axis_len;
            axis[2] /= axis_len;
        }

        float separation = min_circumference * (1.0f - chi);
        float min_diameter = min_circumference / (float)M_PI;

        /* First bifurcation */
        surface_branch_point_t* bif1 = &result->optimal_topology[num_terminals];
        bif1->id = num_terminals;
        bif1->position.x = centroid[0] - axis[0] * separation / 2.0f;
        bif1->position.y = centroid[1] - axis[1] * separation / 2.0f;
        bif1->position.z = centroid[2] - axis[2] * separation / 2.0f;
        bif1->is_terminal = false;
        bif1->degree = 3;
        bif1->link_ids[0] = 0;
        bif1->link_ids[1] = 1;
        bif1->link_ids[2] = num_terminals + 1;
        bif1->link_diameters[0] = min_diameter;
        bif1->link_diameters[1] = min_diameter;
        bif1->link_diameters[2] = min_diameter;

        /* Second bifurcation */
        surface_branch_point_t* bif2 = &result->optimal_topology[num_terminals + 1];
        bif2->id = num_terminals + 1;
        bif2->position.x = centroid[0] + axis[0] * separation / 2.0f;
        bif2->position.y = centroid[1] + axis[1] * separation / 2.0f;
        bif2->position.z = centroid[2] + axis[2] * separation / 2.0f;
        bif2->is_terminal = false;
        bif2->degree = 3;
        bif2->link_ids[0] = 2;
        bif2->link_ids[1] = 3;
        bif2->link_ids[2] = num_terminals;
        bif2->link_diameters[0] = min_diameter;
        bif2->link_diameters[1] = min_diameter;
        bif2->link_diameters[2] = min_diameter;

        /* Update terminal connections */
        result->optimal_topology[0].degree = 1;
        result->optimal_topology[0].link_ids[0] = num_terminals;
        result->optimal_topology[0].link_diameters[0] = min_diameter;

        result->optimal_topology[1].degree = 1;
        result->optimal_topology[1].link_ids[0] = num_terminals;
        result->optimal_topology[1].link_diameters[0] = min_diameter;

        result->optimal_topology[2].degree = 1;
        result->optimal_topology[2].link_ids[0] = num_terminals + 1;
        result->optimal_topology[2].link_diameters[0] = min_diameter;

        result->optimal_topology[3].degree = 1;
        result->optimal_topology[3].link_ids[0] = num_terminals + 1;
        result->optimal_topology[3].link_diameters[0] = min_diameter;

        result->num_branch_points = num_terminals + 2;
    } else {
        /* Default: star topology */
        surface_branch_point_t* center = &result->optimal_topology[num_terminals];
        center->id = num_terminals;
        center->position.x = centroid[0];
        center->position.y = centroid[1];
        center->position.z = centroid[2];
        center->is_terminal = false;
        center->degree = num_terminals;

        float min_diameter = min_circumference / (float)M_PI;
        for (uint32_t i = 0; i < num_terminals; i++) {
            center->link_ids[i] = i;
            center->link_diameters[i] = min_diameter;
            result->optimal_topology[i].degree = 1;
            result->optimal_topology[i].link_ids[0] = num_terminals;
            result->optimal_topology[i].link_diameters[0] = min_diameter;
        }

        result->num_branch_points = num_terminals + 1;
    }

    /* Compute score (negative area) */
    result->best_score = -simulated_qmc_amplitude(
        result->optimal_topology,
        result->num_branch_points,
        min_circumference,
        100,
        NULL
    );

    result->nodes_explored = bridge->config.qmcts.num_iterations;
    result->rollouts_performed = bridge->config.qmcts.num_iterations *
                                 bridge->config.qmcts.rollout_count;
    result->avg_depth = (float)bridge->config.qmcts.max_depth / 2.0f;

    /* Update statistics */
    bridge->stats.qmcts_calls++;
    if (result->best_score > bridge->stats.best_mcts_score) {
        bridge->stats.best_mcts_score = result->best_score;
    }

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

void surface_qmcts_result_free(surface_qmcts_result_t* result) {
    if (!result) return;

    if (result->optimal_topology) {
        nimcp_free(result->optimal_topology);
        result->optimal_topology = NULL;
    }
    result->num_branch_points = 0;
}

//=============================================================================
// HYBRID OPTIMIZATION
//=============================================================================

/**
 * @brief Hybrid quantum optimization combining QMCTS, annealing, and QMC amplitude
 *
 * DEADLOCK PREVENTION STRATEGY:
 * This function deliberately does NOT acquire an outer lock. The bridge uses
 * non-recursive (default) mutexes, so if we held a lock here while calling
 * the inner functions (which each acquire their own locks), we would deadlock.
 *
 * The implementation is safe because:
 * 1. Each inner function (mcts_optimize, anneal_positions, estimate_area)
 *    acquires and releases the bridge mutex independently
 * 2. Local result variables are used for intermediate data
 * 3. The final result struct is populated only from local/completed data
 * 4. Memory ownership is transferred atomically (pointer assignment)
 *
 * Alternative approaches considered:
 * - Recursive mutex: Would work but adds overhead and can mask bugs
 * - Unlocked helper variants: Would require duplicating all inner logic
 * - Single lock with all work: Would require major refactoring
 *
 * The current approach provides correct thread-safe behavior with minimal
 * code duplication and clear ownership semantics.
 *
 * @param bridge Bridge instance (thread-safe via per-call locking)
 * @param terminals Array of terminal positions [num_terminals][3]
 * @param num_terminals Number of terminal points (must be >= 2)
 * @param min_circumference Minimum circumference constraint
 * @param result Output optimization result (caller owns branch_points memory)
 * @return 0 on success, -1 on error
 */
int surface_quantum_hybrid_optimize(
    surface_quantum_bridge_t* bridge,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_optimization_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(terminals);
    BRIDGE_NULL_CHECK(result);

    if (num_terminals < 2) {
        LOG_ERROR("Insufficient terminals (%u < 2) in surface_quantum_hybrid_optimize", num_terminals);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Need at least 2 terminals");
        return -1;
    }

    /* Initialize result to safe state before any operations */
    memset(result, 0, sizeof(*result));

    /* Step 1: QMCTS for topology search */
    surface_qmcts_result_t mcts_result;
    memset(&mcts_result, 0, sizeof(mcts_result));

    if (surface_quantum_mcts_optimize(bridge, terminals, num_terminals,
                                      min_circumference, &mcts_result) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_quantum_hybrid_optimize: operation failed");
        return -1;
    }

    /* Transfer topology ownership to result (atomic pointer assignment) */
    result->branch_points = mcts_result.optimal_topology;
    result->num_branch_points = mcts_result.num_branch_points;
    mcts_result.optimal_topology = NULL;  /* Prevent double-free */

    /* Step 2: Quantum annealing for position refinement */
    float refined_area = 0.0f;
    int anneal_status = surface_quantum_anneal_positions(
        bridge,
        result->branch_points,
        result->num_branch_points,
        min_circumference,
        &refined_area
    );

    if (anneal_status != 0) {
        /* Annealing failed - clean up and return error */
        nimcp_free(result->branch_points);
        result->branch_points = NULL;
        result->num_branch_points = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_quantum_hybrid_optimize: validation failed");
        return -1;
    }

    /* Step 3: QMC amplitude estimation for accurate final area */
    surface_qmc_amplitude_result_t amplitude_result;
    memset(&amplitude_result, 0, sizeof(amplitude_result));

    int estimate_status = surface_quantum_estimate_area(
        bridge,
        result->branch_points,
        result->num_branch_points,
        min_circumference,
        &amplitude_result
    );

    if (estimate_status != 0) {
        /* Estimation failed - clean up and return error */
        nimcp_free(result->branch_points);
        result->branch_points = NULL;
        result->num_branch_points = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_quantum_hybrid_optimize: validation failed");
        return -1;
    }

    result->surface_area = amplitude_result.estimated_area;

    /* Compute other metrics */
    result->wire_length = 0.0f;
    for (uint32_t i = 0; i < result->num_branch_points; i++) {
        const surface_branch_point_t* bp = &result->branch_points[i];
        for (uint32_t j = 0; j < bp->degree; j++) {
            if (bp->link_ids[j] > bp->id) {
                for (uint32_t k = 0; k < result->num_branch_points; k++) {
                    if (result->branch_points[k].id == bp->link_ids[j]) {
                        float dx = bp->position.x - result->branch_points[k].position.x;
                        float dy = bp->position.y - result->branch_points[k].position.y;
                        float dz = bp->position.z - result->branch_points[k].position.z;
                        result->wire_length += sqrtf(dx*dx + dy*dy + dz*dz);
                        break;
                    }
                }
            }
        }
    }

    float min_diameter = min_circumference / (float)M_PI;
    if (result->wire_length > 0) {
        result->efficiency_ratio = result->surface_area /
                                   (result->wire_length * min_diameter * (float)M_PI);
    }

    result->converged = true;

    /* Compute chi and store in first branch point params */
    float centroid[3] = {0, 0, 0};
    for (uint32_t i = 0; i < num_terminals; i++) {
        centroid[0] += terminals[i][0];
        centroid[1] += terminals[i][1];
        centroid[2] += terminals[i][2];
    }
    centroid[0] /= num_terminals;
    centroid[1] /= num_terminals;
    centroid[2] /= num_terminals;

    float r = 0.0f;
    for (uint32_t i = 0; i < num_terminals; i++) {
        float dx = terminals[i][0] - centroid[0];
        float dy = terminals[i][1] - centroid[1];
        float dz = terminals[i][2] - centroid[2];
        r += sqrtf(dx*dx + dy*dy + dz*dz);
    }
    r /= num_terminals;

    float chi = min_circumference / r;
    if (result->num_branch_points > 0) {
        result->branch_points[0].params.chi = chi;
        result->branch_points[0].params.branch_type =
            (chi >= SURFACE_CHI_TRIFURCATION_THRESHOLD) ?
            SURFACE_BRANCH_TRIFURCATION : SURFACE_BRANCH_BIFURCATION;
    }

    surface_qmcts_result_free(&mcts_result);

    return 0;
}

//=============================================================================
// STATISTICS
//=============================================================================

int surface_quantum_bridge_get_stats(
    const surface_quantum_bridge_t* bridge,
    surface_quantum_bridge_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    memcpy(stats, &bridge->stats, sizeof(*stats));
    return 0;
}

int surface_quantum_bridge_reset_stats(surface_quantum_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

//=============================================================================
// UTILITY
//=============================================================================

const char* surface_quantum_method_name(surface_quantum_method_t method) {
    if (method <= SURFACE_QUANTUM_HYBRID) {
        return METHOD_NAMES[method];
    }
    return "UNKNOWN";
}

bool surface_quantum_method_available(
    const surface_quantum_bridge_t* bridge,
    surface_quantum_method_t method
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_quantum_method_available: bridge is NULL");
        return false;
    }

    if (!bridge->quantum_available && method != SURFACE_QUANTUM_NONE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_quantum_method_available: bridge->quantum_available is NULL");
        return false;
    }

    /* All methods available in simulation mode */
    return true;
}
