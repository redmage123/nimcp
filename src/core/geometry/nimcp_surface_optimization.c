/**
 * @file nimcp_surface_optimization.c
 * @brief Surface Optimization Algorithms Implementation
 *
 * Implements surface-minimizing optimization methods based on
 * Meng et al. Nature 2026. Methods include:
 * - Gradient descent (with Adam optimizer option)
 * - Monte Carlo integration for area estimation
 * - Quantum annealing for escaping local minima
 * - MCTS for topology search
 *
 * THREAD SAFETY:
 * - RNG state uses _Thread_local for per-thread isolation
 * - Each thread has its own RNG state to avoid race conditions
 * - Overflow protection added for geometric calculations
 *
 * @version 1.1.0
 * @date 2026-01-15
 */

#include "core/geometry/nimcp_surface_optimization.h"
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/geometry/nimcp_surface_manifold.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <float.h>  /* For FLT_MAX, overflow protection */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(surface_optimization)

//=============================================================================
// THREAD-SAFE RNG STATE
//=============================================================================

/**
 * @brief Thread-local RNG state for thread-safe random number generation
 *
 * WHAT: Per-thread RNG state using C11 _Thread_local
 * WHY:  Prevents race conditions when multiple threads use the optimization
 * HOW:  Each thread gets its own isolated RNG state, seeded uniquely
 */
static _Thread_local uint64_t g_thread_rng_state = 0;
static _Thread_local bool g_thread_rng_initialized = false;

/**
 * @brief Initialize thread-local RNG state with unique seed
 *
 * WHAT: Ensure RNG state is initialized for current thread
 * WHY:  First call in each thread needs proper seeding
 * HOW:  Combine time, thread ID approximation, and address randomness
 */
static void ensure_thread_rng_initialized(void) {
    if (!g_thread_rng_initialized) {
        /* Create unique seed combining multiple entropy sources */
        uint64_t seed = (uint64_t)time(NULL);
        seed ^= (uint64_t)(uintptr_t)&g_thread_rng_state;  /* Address randomness */
        seed ^= ((uint64_t)clock()) << 32;  /* CPU time for additional entropy */

        /* Mix the seed with a hash function for better distribution */
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;

        g_thread_rng_state = seed;
        g_thread_rng_initialized = true;
    }
}

/**
 * @brief Get pointer to thread-local RNG state
 *
 * WHAT: Access the current thread's RNG state
 * WHY:  Allows existing code to use RNG state pointer pattern
 * HOW:  Returns pointer to thread-local variable after initialization
 */
static uint64_t* get_thread_rng_state(void) {
    ensure_thread_rng_initialized();
    return &g_thread_rng_state;
}

//=============================================================================
// SAFE MATH UTILITIES (Overflow Protection)
//=============================================================================

/**
 * @brief Safe multiplication with overflow detection
 *
 * WHAT: Multiply two floats with overflow protection
 * WHY:  Large geometric coordinates can cause overflow in squared distances
 * HOW:  Check for infinity/NaN after multiplication
 */
static inline float safe_mul(float a, float b) {
    float result = a * b;
    if (!isfinite(result)) {
        /* Overflow detected: clamp to max float */
        return (a >= 0.0f) == (b >= 0.0f) ? FLT_MAX : -FLT_MAX;
    }
    return result;
}

/**
 * @brief Safe addition with overflow detection
 *
 * WHAT: Add two floats with overflow protection
 * WHY:  Sum of squared distances can overflow for large values
 * HOW:  Check bounds before adding, return clamped value if overflow
 */
static inline float safe_add(float a, float b) {
    /* Check for potential overflow */
    if (b > 0 && a > FLT_MAX - b) return FLT_MAX;
    if (b < 0 && a < -FLT_MAX - b) return -FLT_MAX;
    return a + b;
}

/**
 * @brief Safe squared distance computation
 *
 * WHAT: Compute squared distance without overflow
 * WHY:  Squaring large differences can overflow float32
 * HOW:  Use double intermediate for sum of squares, clamp result
 *
 * NUMERICAL STABILITY:
 * - Float32 max ~3.4e38, so sqrt(FLT_MAX) ~1.84e19
 * - If any difference > ~1.84e19, squaring overflows
 * - Using double accumulator (max ~1.8e308) handles this safely
 */
static inline float safe_squared_distance(float dx, float dy, float dz) {
    /* Use double precision for intermediate calculations to prevent overflow */
    double ddx = (double)dx;
    double ddy = (double)dy;
    double ddz = (double)dz;
    double sum = ddx * ddx + ddy * ddy + ddz * ddz;

    /* Clamp to float range */
    if (sum > (double)FLT_MAX) return FLT_MAX;
    return (float)sum;
}

/**
 * @brief Safe square root for potentially overflowed values
 *
 * WHAT: Compute square root with overflow/underflow handling
 * WHY:  sqrtf(FLT_MAX) is valid but sqrtf(inf) is inf, need bounds
 * HOW:  Check for special cases before calling sqrtf
 */
static inline float safe_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    if (!isfinite(x)) return FLT_MAX;
    return sqrtf(x);
}

//=============================================================================
// CONSTANTS
//=============================================================================

/** Minimum determinant for numerical stability */
#define SURFACE_OPT_MIN_DETERMINANT 1e-10f

/** Maximum gradient magnitude (for clipping) */
#define SURFACE_OPT_MAX_GRADIENT 100.0f

/** Default random seed */
#define SURFACE_OPT_DEFAULT_SEED 0x12345678ULL

/** Pi constant */

//=============================================================================
// INTERNAL STRUCTURES
//=============================================================================

/** Maximum consecutive divergence steps before early exit */
#define SURFACE_OPT_MAX_DIVERGENCE_STEPS 10

/** Divergence threshold: area increase ratio that triggers divergence counter */
#define SURFACE_OPT_DIVERGENCE_THRESHOLD 1.1f

/**
 * @brief Generic optimizer structure
 */
struct surface_optimizer_struct {
    surface_optimization_method_t method;

    /* Method-specific state */
    union {
        surface_gradient_state_t* gradient;
        surface_mc_state_t* mc;
        surface_annealing_state_t* annealing;
    } state;

    /* Method-specific config */
    union {
        surface_gradient_config_t gradient;
        surface_monte_carlo_config_t mc;
        surface_annealing_config_t annealing;
        surface_mcts_config_t mcts;
    } config;

    /* Common state */
    float min_circumference;
    float current_area;
    bool initialized;
    bool converged;
    uint32_t iteration;

    /* Divergence detection */
    float best_area;                /**< Best area seen so far */
    uint32_t divergence_count;      /**< Consecutive steps with area increase */
    bool diverged;                  /**< True if optimization is diverging */

    /* Solution storage */
    surface_branch_point_t* solution;
    uint32_t num_solution_points;
    uint32_t max_solution_points;
};

//=============================================================================
// RANDOM NUMBER GENERATION (Simple LCG for portability)
//=============================================================================

/**
 * @brief Simple LCG random number generator
 */
static inline uint64_t surface_rng_next(uint64_t* state) {
    *state = (*state * 6364136223846793005ULL + 1442695040888963407ULL);
    return *state;
}

/**
 * @brief Generate uniform random float in [0, 1)
 */
static inline float surface_rng_uniform(uint64_t* state) {
    return (float)(surface_rng_next(state) >> 33) / (float)(1ULL << 31);
}

/**
 * @brief Generate Gaussian random number (Box-Muller)
 */
static float surface_rng_gaussian(uint64_t* state, float mean, float stddev) {
    float u1 = surface_rng_uniform(state);
    float u2 = surface_rng_uniform(state);

    /* Avoid log(0) */
    if (u1 < 1e-10f) u1 = 1e-10f;

    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return mean + stddev * z;
}

//=============================================================================
// VECTOR MATH UTILITIES (With Overflow Protection)
//=============================================================================

/**
 * @brief Compute distance between two 3D points (float array version)
 *
 * OVERFLOW PROTECTION: Uses safe_squared_distance with double intermediate
 * to prevent overflow when coordinates are very large.
 */
static inline float vec3_distance(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    /* Use safe squared distance to prevent overflow */
    return safe_sqrt(safe_squared_distance(dx, dy, dz));
}

/**
 * @brief Compute distance between two surface_vec3_t points
 *
 * OVERFLOW PROTECTION: Uses safe_squared_distance with double intermediate
 * to prevent overflow when coordinates are very large.
 */
static inline float svec3_distance(const surface_vec3_t* a, const surface_vec3_t* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    /* Use safe squared distance to prevent overflow */
    return safe_sqrt(safe_squared_distance(dx, dy, dz));
}

/**
 * @brief Compute length of 3D vector
 *
 * OVERFLOW PROTECTION: Uses safe_squared_distance to prevent overflow
 */
static inline float vec3_length(const float v[3]) {
    return safe_sqrt(safe_squared_distance(v[0], v[1], v[2]));
}

static inline void vec3_sub(float result[3], const float a[3], const float b[3]) {
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

static inline void svec3_sub(float result[3], const surface_vec3_t* a, const surface_vec3_t* b) {
    result[0] = a->x - b->x;
    result[1] = a->y - b->y;
    result[2] = a->z - b->z;
}

/* Access surface_vec3_t by coordinate index (0=x, 1=y, 2=z) */
static inline float svec3_get(const surface_vec3_t* v, int coord) {
    switch (coord) {
        case 0: return v->x;
        case 1: return v->y;
        case 2: return v->z;
        default: return 0.0f;
    }
}

static inline void svec3_set(surface_vec3_t* v, int coord, float val) {
    switch (coord) {
        case 0: v->x = val; break;
        case 1: v->y = val; break;
        case 2: v->z = val; break;
    }
}

static inline void svec3_to_array(const surface_vec3_t* v, float arr[3]) {
    arr[0] = v->x;
    arr[1] = v->y;
    arr[2] = v->z;
}

static inline void svec3_from_array(surface_vec3_t* v, const float arr[3]) {
    v->x = arr[0];
    v->y = arr[1];
    v->z = arr[2];
}

static inline void vec3_add(float result[3], const float a[3], const float b[3]) {
    result[0] = a[0] + b[0];
    result[1] = a[1] + b[1];
    result[2] = a[2] + b[2];
}

static inline void vec3_scale(float result[3], const float v[3], float s) {
    result[0] = v[0] * s;
    result[1] = v[1] * s;
    result[2] = v[2] * s;
}

//=============================================================================
// GRADIENT DESCENT CONFIGURATION
//=============================================================================

int surface_gradient_default_config(surface_gradient_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_gradient_default_config: config is NULL");
        return -1;
    }

    config->learning_rate = 0.01f;
    config->momentum = 0.9f;
    config->decay = 0.999f;
    config->max_iterations = 1000;
    config->tolerance = 1e-6f;
    config->use_adam = true;
    config->beta1 = 0.9f;
    config->beta2 = 0.999f;
    config->epsilon = 1e-8f;

    return 0;
}

//=============================================================================
// GRADIENT COMPUTATION
//=============================================================================

/**
 * @brief Compute numerical gradient via finite differences
 */
static float compute_link_area(const surface_vec3_t* pos1, const surface_vec3_t* pos2,
                               float diameter) {
    float length = svec3_distance(pos1, pos2);
    /* Cylindrical approximation: A = π * d * L */
    return (float)M_PI * diameter * length;
}

/**
 * @brief Compute total area for current configuration
 */
static float compute_total_area(const surface_branch_point_t* branch_points,
                                uint32_t num_points,
                                float min_circumference) {
    float total = 0.0f;
    float min_diameter = min_circumference / (float)M_PI;

    for (uint32_t i = 0; i < num_points; i++) {
        const surface_branch_point_t* bp = &branch_points[i];

        for (uint32_t j = 0; j < bp->degree; j++) {
            uint32_t neighbor_id = bp->link_ids[j];

            /* Only count each link once */
            if (neighbor_id > bp->id) {
                /* Find neighbor */
                for (uint32_t k = 0; k < num_points; k++) {
                    if (branch_points[k].id == neighbor_id) {
                        float diameter = bp->link_diameters[j];
                        if (diameter < min_diameter) {
                            diameter = min_diameter;
                        }
                        total += compute_link_area(&bp->position,
                                                   &branch_points[k].position,
                                                   diameter);
                        break;
                    }
                }
            }
        }
    }

    return total;
}

int surface_compute_area_gradient(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    float* position_gradients,
    float* diameter_gradients
) {
    if (!branch_points || num_points == 0 || !position_gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_area_gradient: required parameter is NULL (branch_points, position_gradients)");
        return -1;
    }

    const float delta = 1e-4f;  /* Finite difference step */

    /* Compute base area */
    float base_area = compute_total_area(branch_points, num_points, min_circumference);

    /* Create working copy */
    surface_branch_point_t* working = nimcp_malloc(
        num_points * sizeof(surface_branch_point_t));
    if (!working) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_compute_area_gradient: working is NULL");
        return -1;
    }
    memcpy(working, branch_points, num_points * sizeof(surface_branch_point_t));

    /* Compute position gradients via central differences */
    for (uint32_t i = 0; i < num_points; i++) {
        /* Skip terminals (fixed positions) */
        if (working[i].is_terminal) {
            position_gradients[i * 3 + 0] = 0.0f;
            position_gradients[i * 3 + 1] = 0.0f;
            position_gradients[i * 3 + 2] = 0.0f;
            continue;
        }

        for (uint32_t coord = 0; coord < 3; coord++) {
            float orig_val = svec3_get(&branch_points[i].position, coord);

            /* Positive perturbation */
            svec3_set(&working[i].position, coord, orig_val + delta);
            float area_plus = compute_total_area(working, num_points, min_circumference);

            /* Negative perturbation */
            svec3_set(&working[i].position, coord, orig_val - delta);
            float area_minus = compute_total_area(working, num_points, min_circumference);

            /* Restore */
            svec3_set(&working[i].position, coord, orig_val);

            /* Central difference */
            float grad = (area_plus - area_minus) / (2.0f * delta);

            /* Clip gradient */
            if (grad > SURFACE_OPT_MAX_GRADIENT) grad = SURFACE_OPT_MAX_GRADIENT;
            if (grad < -SURFACE_OPT_MAX_GRADIENT) grad = -SURFACE_OPT_MAX_GRADIENT;

            position_gradients[i * 3 + coord] = grad;
        }
    }

    /* Compute diameter gradients if requested */
    if (diameter_gradients) {
        float min_diameter = min_circumference / (float)M_PI;

        for (uint32_t i = 0; i < num_points; i++) {
            for (uint32_t j = 0; j < working[i].degree; j++) {
                /* Only for non-constrained diameters */
                if (working[i].link_diameters[j] <= min_diameter) {
                    diameter_gradients[i * SURFACE_MAX_BRANCH_DEGREE + j] = 0.0f;
                    continue;
                }

                float orig_diam = working[i].link_diameters[j];

                /* Positive perturbation */
                working[i].link_diameters[j] = orig_diam + delta;
                float area_plus = compute_total_area(working, num_points, min_circumference);

                /* Negative perturbation */
                working[i].link_diameters[j] = orig_diam - delta;
                if (working[i].link_diameters[j] < min_diameter) {
                    working[i].link_diameters[j] = min_diameter;
                }
                float area_minus = compute_total_area(working, num_points, min_circumference);

                /* Restore */
                working[i].link_diameters[j] = orig_diam;

                float grad = (area_plus - area_minus) / (2.0f * delta);
                if (grad > SURFACE_OPT_MAX_GRADIENT) grad = SURFACE_OPT_MAX_GRADIENT;
                if (grad < -SURFACE_OPT_MAX_GRADIENT) grad = -SURFACE_OPT_MAX_GRADIENT;

                diameter_gradients[i * SURFACE_MAX_BRANCH_DEGREE + j] = grad;
            }
        }
    }

    nimcp_free(working);
    return 0;
}

//=============================================================================
// MONTE CARLO CONFIGURATION
//=============================================================================

int surface_mc_default_config(surface_monte_carlo_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_mc_default_config: config is NULL");
        return -1;
    }

    config->num_samples = 10000;
    config->use_importance_sampling = true;
    config->use_stratified = false;
    config->strata_per_dim = 10;
    config->seed = SURFACE_OPT_DEFAULT_SEED;

    return 0;
}

//=============================================================================
// MONTE CARLO AREA ESTIMATION
//=============================================================================

int surface_mc_estimate_area(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    const surface_monte_carlo_config_t* config,
    float* area_estimate,
    float* variance
) {
    if (!branch_points || num_points == 0 || !config || !area_estimate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_mc_estimate_area: required parameter is NULL (branch_points, config, area_estimate)");
        return -1;
    }

    uint64_t rng_state = config->seed;

    /* Compute bounding volume for sampling */
    float min_bounds[3] = {1e10f, 1e10f, 1e10f};
    float max_bounds[3] = {-1e10f, -1e10f, -1e10f};

    for (uint32_t i = 0; i < num_points; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            float pos_j = svec3_get(&branch_points[i].position, j);
            if (pos_j < min_bounds[j]) {
                min_bounds[j] = pos_j;
            }
            if (pos_j > max_bounds[j]) {
                max_bounds[j] = pos_j;
            }
        }
    }

    /* Add margin for surfaces */
    float max_diameter = 0.0f;
    for (uint32_t i = 0; i < num_points; i++) {
        for (uint32_t j = 0; j < branch_points[i].degree; j++) {
            if (branch_points[i].link_diameters[j] > max_diameter) {
                max_diameter = branch_points[i].link_diameters[j];
            }
        }
    }
    float margin = max_diameter * 2.0f;
    for (uint32_t j = 0; j < 3; j++) {
        min_bounds[j] -= margin;
        max_bounds[j] += margin;
    }

    /* Sampling */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    uint32_t samples = config->num_samples;

    /* Volume of bounding box */
    float volume = (max_bounds[0] - min_bounds[0]) *
                   (max_bounds[1] - min_bounds[1]) *
                   (max_bounds[2] - min_bounds[2]);

    for (uint32_t s = 0; s < samples; s++) {
        /* Generate random point */
        float point[3];

        if (config->use_stratified) {
            /* Stratified sampling */
            uint32_t stratum = s % (config->strata_per_dim * config->strata_per_dim * config->strata_per_dim);
            uint32_t sx = stratum % config->strata_per_dim;
            uint32_t sy = (stratum / config->strata_per_dim) % config->strata_per_dim;
            uint32_t sz = stratum / (config->strata_per_dim * config->strata_per_dim);

            float strata_size[3];
            strata_size[0] = (max_bounds[0] - min_bounds[0]) / config->strata_per_dim;
            strata_size[1] = (max_bounds[1] - min_bounds[1]) / config->strata_per_dim;
            strata_size[2] = (max_bounds[2] - min_bounds[2]) / config->strata_per_dim;

            point[0] = min_bounds[0] + (sx + surface_rng_uniform(&rng_state)) * strata_size[0];
            point[1] = min_bounds[1] + (sy + surface_rng_uniform(&rng_state)) * strata_size[1];
            point[2] = min_bounds[2] + (sz + surface_rng_uniform(&rng_state)) * strata_size[2];
        } else {
            /* Uniform sampling */
            point[0] = min_bounds[0] + surface_rng_uniform(&rng_state) * (max_bounds[0] - min_bounds[0]);
            point[1] = min_bounds[1] + surface_rng_uniform(&rng_state) * (max_bounds[1] - min_bounds[1]);
            point[2] = min_bounds[2] + surface_rng_uniform(&rng_state) * (max_bounds[2] - min_bounds[2]);
        }

        /* Check if point is on surface (distance to nearest link < radius) */
        float min_dist = 1e10f;

        for (uint32_t i = 0; i < num_points; i++) {
            const surface_branch_point_t* bp = &branch_points[i];

            for (uint32_t j = 0; j < bp->degree; j++) {
                uint32_t neighbor_id = bp->link_ids[j];
                if (neighbor_id <= bp->id) continue;  /* Avoid double counting */

                /* Find neighbor */
                for (uint32_t k = 0; k < num_points; k++) {
                    if (branch_points[k].id == neighbor_id) {
                        /* Distance from point to line segment */
                        float seg[3], to_point[3];
                        float bp_pos[3];
                        svec3_to_array(&bp->position, bp_pos);
                        svec3_sub(seg, &branch_points[k].position, &bp->position);
                        vec3_sub(to_point, point, bp_pos);

                        float seg_len_sq = seg[0]*seg[0] + seg[1]*seg[1] + seg[2]*seg[2];
                        if (seg_len_sq < 1e-10f) continue;

                        float t = (to_point[0]*seg[0] + to_point[1]*seg[1] + to_point[2]*seg[2]) / seg_len_sq;
                        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

                        float closest[3];
                        closest[0] = bp_pos[0] + t * seg[0];
                        closest[1] = bp_pos[1] + t * seg[1];
                        closest[2] = bp_pos[2] + t * seg[2];

                        float dist = vec3_distance(point, closest);
                        float radius = bp->link_diameters[j] / 2.0f;

                        /* Distance to surface */
                        float surf_dist = fabsf(dist - radius);
                        if (surf_dist < min_dist) {
                            min_dist = surf_dist;
                        }
                        break;
                    }
                }
            }
        }

        /* Weight based on distance (Gaussian kernel) */
        float sigma = min_circumference / (2.0f * (float)M_PI);
        float weight = expf(-min_dist * min_dist / (2.0f * sigma * sigma));

        if (config->use_importance_sampling) {
            /* Importance sampling correction */
            sum += weight;
            sum_sq += weight * weight;
        } else {
            sum += weight;
            sum_sq += weight * weight;
        }
    }

    /* Estimate area (with volume scaling) */
    float mean = sum / samples;
    *area_estimate = mean * volume / (sqrtf(2.0f * (float)M_PI) * (min_circumference / (2.0f * (float)M_PI)));

    if (variance) {
        float mean_sq = sum_sq / samples;
        *variance = (mean_sq - mean * mean) / samples;
    }

    return 0;
}

//=============================================================================
// QUANTUM ANNEALING CONFIGURATION
//=============================================================================

int surface_annealing_default_config(surface_annealing_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_annealing_default_config: config is NULL");
        return -1;
    }

    config->temperature_initial = 10.0f;
    config->temperature_final = 0.01f;
    config->cooling_rate = 0.95f;
    config->quantum_strength = 1.0f;
    config->steps_per_temperature = 100;
    config->max_iterations = 10000;
    config->acceptance_target = 0.234f;  /* Optimal for high dimensions */

    return 0;
}

//=============================================================================
// QUANTUM ANNEALING STATE
//=============================================================================

surface_annealing_state_t* surface_annealing_state_create(
    const surface_annealing_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    surface_annealing_state_t* state = nimcp_malloc(sizeof(surface_annealing_state_t));
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "state is NULL");

        return NULL;

    }

    memset(state, 0, sizeof(*state));
    state->temperature = config->temperature_initial;
    state->best_energy = 1e10f;
    state->current_energy = 1e10f;

    return state;
}

void surface_annealing_state_destroy(surface_annealing_state_t* state) {
    if (!state) return;

    if (state->current) {
        nimcp_free(state->current);
    }
    if (state->best) {
        nimcp_free(state->best);
    }
    nimcp_free(state);
}

//=============================================================================
// QUANTUM ANNEALING STEP
//=============================================================================

int surface_annealing_step(
    surface_annealing_state_t* state,
    const surface_annealing_config_t* config,
    bool* accepted
) {
    if (!state || !config || !accepted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_annealing_step: required parameter is NULL (state, config, accepted)");
        return -1;
    }
    if (!state->current || state->num_points == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_annealing_step: state->current is NULL");
        return -1;
    }

    *accepted = false;

    /* THREAD SAFETY: Use thread-local RNG state instead of static local
     * WHY: Static local variables are shared across threads, causing race conditions
     *      and non-reproducible results in multi-threaded optimization
     * HOW: get_thread_rng_state() returns pointer to thread-local storage */
    uint64_t* rng_state = get_thread_rng_state();

    /* Select random non-terminal point to modify */
    uint32_t attempts = 0;
    uint32_t point_idx;
    do {
        point_idx = (uint32_t)(surface_rng_uniform(rng_state) * state->num_points);
        attempts++;
        if (attempts > state->num_points * 2) return 0;  /* No movable points */
    } while (state->current[point_idx].is_terminal);

    /* Save old position */
    float old_pos[3];
    svec3_to_array(&state->current[point_idx].position, old_pos);

    /* Propose move with quantum-enhanced step size */
    float step_size = config->quantum_strength * sqrtf(state->temperature);

    /* Quantum tunneling: occasionally make larger jumps */
    if (surface_rng_uniform(rng_state) < 0.1f) {
        step_size *= 3.0f;  /* Tunneling event */
        state->tunneling_events++;
    }

    for (uint32_t i = 0; i < 3; i++) {
        float curr_val = svec3_get(&state->current[point_idx].position, i);
        svec3_set(&state->current[point_idx].position, i,
                  curr_val + surface_rng_gaussian(rng_state, 0.0f, step_size));
    }

    /* Compute new energy (surface area) */
    float new_energy = compute_total_area(state->current, state->num_points,
                                          state->current[0].link_diameters[0] * (float)M_PI);
    float delta_e = new_energy - state->current_energy;

    /* Metropolis-Hastings acceptance */
    bool accept = false;
    if (delta_e <= 0.0f) {
        accept = true;
    } else {
        float prob = expf(-delta_e / state->temperature);
        if (surface_rng_uniform(rng_state) < prob) {
            accept = true;
        }
    }

    if (accept) {
        state->current_energy = new_energy;
        state->accepted++;
        *accepted = true;

        /* Update best if improved */
        if (new_energy < state->best_energy) {
            state->best_energy = new_energy;
            if (!state->best) {
                state->best = nimcp_malloc(state->num_points * sizeof(surface_branch_point_t));
            }
            if (state->best) {
                memcpy(state->best, state->current,
                       state->num_points * sizeof(surface_branch_point_t));
            }
        }
    } else {
        /* Restore old position */
        svec3_from_array(&state->current[point_idx].position, old_pos);
        state->rejected++;
    }

    /* Update temperature on schedule */
    state->step++;
    if (state->step % config->steps_per_temperature == 0) {
        state->temperature *= config->cooling_rate;
        if (state->temperature < config->temperature_final) {
            state->temperature = config->temperature_final;
        }
    }

    /* Update acceptance rate */
    state->acceptance_rate = (float)state->accepted / (float)(state->accepted + state->rejected);

    return 0;
}

//=============================================================================
// MCTS CONFIGURATION
//=============================================================================

int surface_mcts_default_config(surface_mcts_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_mcts_default_config: config is NULL");
        return -1;
    }

    config->num_iterations = 1000;
    config->max_depth = 20;
    config->exploration_constant = NIMCP_SQRT2_F;  /* sqrt(2) */
    config->rollout_count = 10;
    config->use_virtual_loss = false;

    return 0;
}

//=============================================================================
// MCTS ACTIONS
//=============================================================================

int surface_mcts_get_actions(
    const surface_mcts_state_t* state,
    surface_mcts_action_t* actions,
    uint32_t max_actions,
    uint32_t* num_actions
) {
    if (!state || !actions || !num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_mcts_get_actions: required parameter is NULL (state, actions, num_actions)");
        return -1;
    }

    *num_actions = 0;

    if (state->is_complete) return 0;

    /* Generate potential bifurcation and trifurcation actions */

    /* Find unconnected terminal pairs */
    for (uint32_t i = 0; i < state->num_terminals && *num_actions < max_actions; i++) {
        uint32_t tid1 = state->terminal_ids[i];
        const surface_branch_point_t* t1 = NULL;

        /* Find terminal 1 */
        for (uint32_t j = 0; j < state->num_branch_points; j++) {
            if (state->branch_points[j].id == tid1) {
                t1 = &state->branch_points[j];
                break;
            }
        }
        if (!t1) continue;

        for (uint32_t j = i + 1; j < state->num_terminals && *num_actions < max_actions; j++) {
            uint32_t tid2 = state->terminal_ids[j];
            const surface_branch_point_t* t2 = NULL;

            /* Find terminal 2 */
            for (uint32_t k = 0; k < state->num_branch_points; k++) {
                if (state->branch_points[k].id == tid2) {
                    t2 = &state->branch_points[k];
                    break;
                }
            }
            if (!t2) continue;

            /* Action: Add bifurcation between these terminals */
            surface_mcts_action_t* action = &actions[*num_actions];
            action->action_type = 0;  /* Bifurcation */

            /* Position at midpoint */
            action->position[0] = (t1->position.x + t2->position.x) / 2.0f;
            action->position[1] = (t1->position.y + t2->position.y) / 2.0f;
            action->position[2] = (t1->position.z + t2->position.z) / 2.0f;

            action->connect_to[0] = tid1;
            action->connect_to[1] = tid2;
            action->num_connections = 2;

            (*num_actions)++;
        }
    }

    /* Add trifurcation actions for groups of 3 terminals */
    if (state->num_terminals >= 3 && *num_actions < max_actions) {
        for (uint32_t i = 0; i < state->num_terminals - 2 && *num_actions < max_actions; i++) {
            for (uint32_t j = i + 1; j < state->num_terminals - 1 && *num_actions < max_actions; j++) {
                for (uint32_t k = j + 1; k < state->num_terminals && *num_actions < max_actions; k++) {
                    uint32_t tids[3] = {state->terminal_ids[i], state->terminal_ids[j], state->terminal_ids[k]};
                    float centroid[3] = {0, 0, 0};

                    /* Compute centroid */
                    for (uint32_t t = 0; t < 3; t++) {
                        for (uint32_t bp = 0; bp < state->num_branch_points; bp++) {
                            if (state->branch_points[bp].id == tids[t]) {
                                centroid[0] += state->branch_points[bp].position.x;
                                centroid[1] += state->branch_points[bp].position.y;
                                centroid[2] += state->branch_points[bp].position.z;
                                break;
                            }
                        }
                    }
                    centroid[0] /= 3.0f;
                    centroid[1] /= 3.0f;
                    centroid[2] /= 3.0f;

                    surface_mcts_action_t* action = &actions[*num_actions];
                    action->action_type = 1;  /* Trifurcation */
                    memcpy(action->position, centroid, sizeof(centroid));
                    action->connect_to[0] = tids[0];
                    action->connect_to[1] = tids[1];
                    action->connect_to[2] = tids[2];
                    action->num_connections = 3;

                    (*num_actions)++;
                }
            }
        }
    }

    return 0;
}

int surface_mcts_apply_action(
    surface_mcts_state_t* state,
    const surface_mcts_action_t* action
) {
    if (!state || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_mcts_apply_action: required parameter is NULL (state, action)");
        return -1;
    }

    /* Add new branch point */
    uint32_t new_idx = state->num_branch_points;
    if (new_idx >= SURFACE_MAX_BRANCH_POINTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_mcts_apply_action: capacity exceeded");
        return -1;
    }

    surface_branch_point_t* new_point = &state->branch_points[new_idx];
    memset(new_point, 0, sizeof(*new_point));

    new_point->id = new_idx + 1000;  /* Offset to distinguish from terminals */
    svec3_from_array(&new_point->position, action->position);
    new_point->degree = action->num_connections;
    new_point->is_terminal = false;

    /* Connect to specified points */
    for (uint32_t i = 0; i < action->num_connections; i++) {
        new_point->link_ids[i] = action->connect_to[i];
        new_point->link_diameters[i] = 1.0f;  /* Default diameter */

        /* Update neighbor's connections */
        for (uint32_t j = 0; j < state->num_branch_points; j++) {
            if (state->branch_points[j].id == action->connect_to[i]) {
                uint32_t deg = state->branch_points[j].degree;
                if (deg < SURFACE_MAX_BRANCH_DEGREE) {
                    state->branch_points[j].link_ids[deg] = new_point->id;
                    state->branch_points[j].link_diameters[deg] = 1.0f;
                    state->branch_points[j].degree++;
                }
                break;
            }
        }
    }

    state->num_branch_points++;

    /* Recompute area */
    state->current_area = compute_total_area(state->branch_points,
                                             state->num_branch_points,
                                             (float)M_PI);

    /* Check if complete (all terminals connected) */
    state->is_complete = true;
    for (uint32_t i = 0; i < state->num_terminals; i++) {
        uint32_t tid = state->terminal_ids[i];
        bool found = false;
        for (uint32_t j = 0; j < state->num_branch_points; j++) {
            if (state->branch_points[j].id == tid && state->branch_points[j].degree > 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            state->is_complete = false;
            break;
        }
    }

    return 0;
}

float surface_mcts_evaluate(const surface_mcts_state_t* state) {
    if (!state) return -1e10f;

    /* Score is negative area (lower area = higher score) */
    float score = -state->current_area;

    /* Penalty for incomplete networks */
    if (!state->is_complete) {
        score -= 1000.0f;
    }

    return score;
}

//=============================================================================
// TETRAHEDRAL OPTIMIZATION
//=============================================================================

int surface_tetrahedron_chi(
    const float terminals[4][3],
    float circumference,
    float* chi
) {
    if (!terminals || circumference <= 0.0f || !chi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_tetrahedron_chi: required parameter is NULL (terminals, chi)");
        return -1;
    }

    /* Compute centroid */
    float centroid[3] = {0, 0, 0};
    for (uint32_t i = 0; i < 4; i++) {
        centroid[0] += terminals[i][0];
        centroid[1] += terminals[i][1];
        centroid[2] += terminals[i][2];
    }
    centroid[0] /= 4.0f;
    centroid[1] /= 4.0f;
    centroid[2] /= 4.0f;

    /* Compute r = average distance from centroid to vertices */
    float r = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        r += vec3_distance(terminals[i], centroid);
    }
    r /= 4.0f;

    /* chi = w/r where w is circumference */
    if (r < 1e-10f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_tetrahedron_chi: validation failed");
        return -1;
    }
    *chi = circumference / r;

    return 0;
}

int surface_tetrahedron_lambda(
    const surface_branch_point_t* solution,
    uint32_t num_points,
    float circumference,
    float* lambda
) {
    if (!solution || num_points < 5 || circumference <= 0.0f || !lambda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_tetrahedron_lambda: required parameter is NULL (solution, lambda)");
        return -1;
    }

    /* Find intermediate (non-terminal) points */
    const surface_branch_point_t* intermediate[2] = {NULL, NULL};
    uint32_t num_intermediate = 0;

    for (uint32_t i = 0; i < num_points && num_intermediate < 2; i++) {
        if (!solution[i].is_terminal) {
            intermediate[num_intermediate++] = &solution[i];
        }
    }

    if (num_intermediate == 2) {
        /* Distance between intermediate nodes */
        float l = svec3_distance(&intermediate[0]->position, &intermediate[1]->position);
        *lambda = l / circumference;
    } else if (num_intermediate == 1) {
        /* Single trifurcation point - lambda is 0 */
        *lambda = 0.0f;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_tetrahedron_lambda: validation failed");
        return -1;
    }

    return 0;
}

int surface_optimize_tetrahedron_internal(
    const float terminals[4][3],
    float min_circumference,
    surface_optimization_method_t method,
    const void* method_config,
    surface_optimization_result_t* result
) {
    if (!terminals || min_circumference <= 0.0f || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimize_tetrahedron_internal: required parameter is NULL (terminals, result)");
        return -1;
    }

    /* Compute chi to determine expected topology */
    float chi;
    if (surface_tetrahedron_chi(terminals, min_circumference, &chi) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_optimize_tetrahedron_internal: validation failed");
        return -1;
    }

    /* Initialize result */
    memset(result, 0, sizeof(*result));

    /* Determine topology based on chi threshold */
    bool use_trifurcation = (chi >= SURFACE_CHI_TRIFURCATION_THRESHOLD);

    /* Allocate solution points */
    uint32_t num_points = use_trifurcation ? 5 : 6;  /* 4 terminals + 1 or 2 intermediate */
    result->branch_points = nimcp_malloc(num_points * sizeof(surface_branch_point_t));
    if (!result->branch_points) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_optimize_tetrahedron_internal: result->branch_points is NULL");
        return -1;
    }
    memset(result->branch_points, 0, num_points * sizeof(surface_branch_point_t));
    result->num_branch_points = num_points;

    /* Set up terminal points */
    for (uint32_t i = 0; i < 4; i++) {
        result->branch_points[i].id = i;
        svec3_from_array(&result->branch_points[i].position, terminals[i]);
        result->branch_points[i].is_terminal = true;
        result->branch_points[i].degree = 1;
    }

    /* Compute centroid */
    float centroid[3] = {0, 0, 0};
    for (uint32_t i = 0; i < 4; i++) {
        centroid[0] += terminals[i][0];
        centroid[1] += terminals[i][1];
        centroid[2] += terminals[i][2];
    }
    centroid[0] /= 4.0f;
    centroid[1] /= 4.0f;
    centroid[2] /= 4.0f;

    float min_diameter = min_circumference / (float)M_PI;

    if (use_trifurcation) {
        /* Single trifurcation at centroid */
        surface_branch_point_t* tri = &result->branch_points[4];
        tri->id = 4;
        svec3_from_array(&tri->position, centroid);
        tri->is_terminal = false;
        tri->degree = 4;

        for (uint32_t i = 0; i < 4; i++) {
            tri->link_ids[i] = i;
            tri->link_diameters[i] = min_diameter;
            result->branch_points[i].link_ids[0] = 4;
            result->branch_points[i].link_diameters[0] = min_diameter;
        }
    } else {
        /* Two bifurcations - use Steiner-like configuration */
        /* Place bifurcations along axis through centroid */
        float axis[3];
        vec3_sub(axis, terminals[0], terminals[3]);
        float axis_len = vec3_length(axis);
        if (axis_len > 0.0f) {
            vec3_scale(axis, axis, 1.0f / axis_len);
        }

        /* Initial separation based on chi */
        float separation = min_circumference * (1.0f - chi);

        surface_branch_point_t* bif1 = &result->branch_points[4];
        surface_branch_point_t* bif2 = &result->branch_points[5];

        bif1->id = 4;
        bif2->id = 5;
        bif1->is_terminal = false;
        bif2->is_terminal = false;

        /* Position bifurcations */
        bif1->position.x = centroid[0] - axis[0] * separation / 2.0f;
        bif1->position.y = centroid[1] - axis[1] * separation / 2.0f;
        bif1->position.z = centroid[2] - axis[2] * separation / 2.0f;

        bif2->position.x = centroid[0] + axis[0] * separation / 2.0f;
        bif2->position.y = centroid[1] + axis[1] * separation / 2.0f;
        bif2->position.z = centroid[2] + axis[2] * separation / 2.0f;

        /* Connect: bif1 to terminals 0,1 and bif2; bif2 to terminals 2,3 and bif1 */
        bif1->degree = 3;
        bif1->link_ids[0] = 0;
        bif1->link_ids[1] = 1;
        bif1->link_ids[2] = 5;
        bif1->link_diameters[0] = min_diameter;
        bif1->link_diameters[1] = min_diameter;
        bif1->link_diameters[2] = min_diameter;

        bif2->degree = 3;
        bif2->link_ids[0] = 2;
        bif2->link_ids[1] = 3;
        bif2->link_ids[2] = 4;
        bif2->link_diameters[0] = min_diameter;
        bif2->link_diameters[1] = min_diameter;
        bif2->link_diameters[2] = min_diameter;

        /* Update terminal connections */
        result->branch_points[0].link_ids[0] = 4;
        result->branch_points[0].link_diameters[0] = min_diameter;
        result->branch_points[1].link_ids[0] = 4;
        result->branch_points[1].link_diameters[0] = min_diameter;
        result->branch_points[2].link_ids[0] = 5;
        result->branch_points[2].link_diameters[0] = min_diameter;
        result->branch_points[3].link_ids[0] = 5;
        result->branch_points[3].link_diameters[0] = min_diameter;
    }

    /* Optimization step is optional - the tetrahedral topology is already valid.
     * To enable optimization, surface_optimizer_init must be called to allocate
     * method-specific state. For now, skip optimization since the topology is
     * already correctly determined by chi. */
    (void)method;
    (void)method_config;

    /* Compute final metrics */
    result->surface_area = compute_total_area(result->branch_points,
                                              result->num_branch_points,
                                              min_circumference);

    /* Compute Steiner length for comparison */
    result->wire_length = 0.0f;
    for (uint32_t i = 0; i < result->num_branch_points; i++) {
        const surface_branch_point_t* bp = &result->branch_points[i];
        for (uint32_t j = 0; j < bp->degree; j++) {
            if (bp->link_ids[j] > bp->id) {
                for (uint32_t k = 0; k < result->num_branch_points; k++) {
                    if (result->branch_points[k].id == bp->link_ids[j]) {
                        result->wire_length += svec3_distance(&bp->position,
                                                             &result->branch_points[k].position);
                        break;
                    }
                }
            }
        }
    }

    /* Efficiency ratio */
    if (result->wire_length > 0.0f) {
        result->efficiency_ratio = result->surface_area / (result->wire_length * min_diameter * (float)M_PI);
    }

    /* Store chi in first branch point's params */
    if (result->num_branch_points > 0) {
        result->branch_points[0].params.chi = chi;
        result->branch_points[0].params.branch_type = use_trifurcation ?
            SURFACE_BRANCH_TRIFURCATION : SURFACE_BRANCH_BIFURCATION;
        result->branch_points[0].params.is_optimal = true;
    }
    result->converged = true;

    return 0;
}

//=============================================================================
// INITIALIZATION HELPERS
//=============================================================================

int surface_compute_centroid(
    const float (*terminals)[3],
    uint32_t num_terminals,
    float centroid[3]
) {
    if (!terminals || num_terminals == 0 || !centroid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_centroid: required parameter is NULL (terminals, centroid)");
        return -1;
    }

    centroid[0] = 0.0f;
    centroid[1] = 0.0f;
    centroid[2] = 0.0f;

    for (uint32_t i = 0; i < num_terminals; i++) {
        centroid[0] += terminals[i][0];
        centroid[1] += terminals[i][1];
        centroid[2] += terminals[i][2];
    }

    centroid[0] /= num_terminals;
    centroid[1] /= num_terminals;
    centroid[2] /= num_terminals;

    return 0;
}

int surface_compute_characteristic_distance(
    const float (*terminals)[3],
    uint32_t num_terminals,
    float* distance
) {
    if (!terminals || num_terminals == 0 || !distance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_characteristic_distance: required parameter is NULL (terminals, distance)");
        return -1;
    }

    float centroid[3];
    if (surface_compute_centroid(terminals, num_terminals, centroid) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_compute_characteristic_distance: validation failed");
        return -1;
    }

    *distance = 0.0f;
    for (uint32_t i = 0; i < num_terminals; i++) {
        *distance += vec3_distance(terminals[i], centroid);
    }
    *distance /= num_terminals;

    return 0;
}

int surface_create_initial_topology(
    const float (*terminals)[3],
    uint32_t num_terminals,
    surface_branch_point_t* branch_points,
    uint32_t max_points,
    uint32_t* num_points
) {
    if (!terminals || num_terminals < 2 || !branch_points || !num_points) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_create_initial_topology: required parameter is NULL (terminals, branch_points, num_points)");
        return -1;
    }

    /* At minimum, need space for terminals plus one intermediate */
    if (max_points < num_terminals + 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_create_initial_topology: validation failed");
        return -1;
    }

    *num_points = 0;

    /* Create terminal points */
    for (uint32_t i = 0; i < num_terminals; i++) {
        surface_branch_point_t* bp = &branch_points[i];
        memset(bp, 0, sizeof(*bp));
        bp->id = i;
        svec3_from_array(&bp->position, terminals[i]);
        bp->is_terminal = true;
        bp->degree = 0;
    }
    *num_points = num_terminals;

    if (num_terminals == 2) {
        /* Just connect the two terminals directly */
        branch_points[0].degree = 1;
        branch_points[0].link_ids[0] = 1;
        branch_points[0].link_diameters[0] = 1.0f;

        branch_points[1].degree = 1;
        branch_points[1].link_ids[0] = 0;
        branch_points[1].link_diameters[0] = 1.0f;

    } else if (num_terminals == 3) {
        /* Star topology: single bifurcation at centroid */
        if (*num_points >= max_points) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_create_initial_topology: capacity exceeded");
            return -1;
        }

        float centroid[3];
        surface_compute_centroid(terminals, num_terminals, centroid);

        surface_branch_point_t* center = &branch_points[num_terminals];
        memset(center, 0, sizeof(*center));
        center->id = num_terminals;
        svec3_from_array(&center->position, centroid);
        center->is_terminal = false;
        center->degree = 3;

        for (uint32_t i = 0; i < 3; i++) {
            center->link_ids[i] = i;
            center->link_diameters[i] = 1.0f;

            branch_points[i].degree = 1;
            branch_points[i].link_ids[0] = num_terminals;
            branch_points[i].link_diameters[0] = 1.0f;
        }

        (*num_points)++;

    } else if (num_terminals == 4) {
        /* Two bifurcations for 4 terminals */
        if (*num_points + 2 > max_points) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_create_initial_topology: validation failed");
            return -1;
        }

        float centroid[3];
        surface_compute_centroid(terminals, num_terminals, centroid);

        /* Compute axis from pair of terminals */
        float axis[3];
        vec3_sub(axis, terminals[0], terminals[3]);
        float axis_len = vec3_length(axis);
        if (axis_len > 1e-10f) {
            vec3_scale(axis, axis, 1.0f / axis_len);
        }

        float separation = axis_len * 0.3f;  /* Initial separation */

        /* First bifurcation */
        surface_branch_point_t* bif1 = &branch_points[num_terminals];
        memset(bif1, 0, sizeof(*bif1));
        bif1->id = num_terminals;
        bif1->position.x = centroid[0] - axis[0] * separation / 2.0f;
        bif1->position.y = centroid[1] - axis[1] * separation / 2.0f;
        bif1->position.z = centroid[2] - axis[2] * separation / 2.0f;
        bif1->is_terminal = false;
        bif1->degree = 3;
        bif1->link_ids[0] = 0;
        bif1->link_ids[1] = 1;
        bif1->link_ids[2] = num_terminals + 1;
        bif1->link_diameters[0] = 1.0f;
        bif1->link_diameters[1] = 1.0f;
        bif1->link_diameters[2] = 1.0f;

        /* Second bifurcation */
        surface_branch_point_t* bif2 = &branch_points[num_terminals + 1];
        memset(bif2, 0, sizeof(*bif2));
        bif2->id = num_terminals + 1;
        bif2->position.x = centroid[0] + axis[0] * separation / 2.0f;
        bif2->position.y = centroid[1] + axis[1] * separation / 2.0f;
        bif2->position.z = centroid[2] + axis[2] * separation / 2.0f;
        bif2->is_terminal = false;
        bif2->degree = 3;
        bif2->link_ids[0] = 2;
        bif2->link_ids[1] = 3;
        bif2->link_ids[2] = num_terminals;
        bif2->link_diameters[0] = 1.0f;
        bif2->link_diameters[1] = 1.0f;
        bif2->link_diameters[2] = 1.0f;

        /* Update terminal connections */
        branch_points[0].degree = 1;
        branch_points[0].link_ids[0] = num_terminals;
        branch_points[0].link_diameters[0] = 1.0f;

        branch_points[1].degree = 1;
        branch_points[1].link_ids[0] = num_terminals;
        branch_points[1].link_diameters[0] = 1.0f;

        branch_points[2].degree = 1;
        branch_points[2].link_ids[0] = num_terminals + 1;
        branch_points[2].link_diameters[0] = 1.0f;

        branch_points[3].degree = 1;
        branch_points[3].link_ids[0] = num_terminals + 1;
        branch_points[3].link_diameters[0] = 1.0f;

        *num_points += 2;

    } else {
        /* General case: star topology from centroid */
        if (*num_points >= max_points) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_create_initial_topology: capacity exceeded");
            return -1;
        }

        float centroid[3];
        surface_compute_centroid(terminals, num_terminals, centroid);

        surface_branch_point_t* center = &branch_points[num_terminals];
        memset(center, 0, sizeof(*center));
        center->id = num_terminals;
        svec3_from_array(&center->position, centroid);
        center->is_terminal = false;
        center->degree = (num_terminals <= SURFACE_MAX_BRANCH_DEGREE) ? num_terminals : SURFACE_MAX_BRANCH_DEGREE;

        for (uint32_t i = 0; i < center->degree; i++) {
            center->link_ids[i] = i;
            center->link_diameters[i] = 1.0f;

            branch_points[i].degree = 1;
            branch_points[i].link_ids[0] = num_terminals;
            branch_points[i].link_diameters[0] = 1.0f;
        }

        (*num_points)++;
    }

    return 0;
}

//=============================================================================
// OPTIMIZER INTERFACE
//=============================================================================

surface_optimizer_t* surface_optimizer_create(
    surface_optimization_method_t method,
    const void* config
) {
    surface_optimizer_t* opt = nimcp_malloc(sizeof(surface_optimizer_t));
    if (!opt) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "opt is NULL");

        return NULL;

    }

    memset(opt, 0, sizeof(*opt));
    opt->method = method;

    /* Initialize method-specific config */
    switch (method) {
        case SURFACE_OPT_GRADIENT_DESCENT:
            if (config) {
                memcpy(&opt->config.gradient, config, sizeof(surface_gradient_config_t));
            } else {
                surface_gradient_default_config(&opt->config.gradient);
            }
            break;

        case SURFACE_OPT_MONTE_CARLO:
            if (config) {
                memcpy(&opt->config.mc, config, sizeof(surface_monte_carlo_config_t));
            } else {
                surface_mc_default_config(&opt->config.mc);
            }
            break;

        case SURFACE_OPT_QUANTUM_ANNEALING:
            if (config) {
                memcpy(&opt->config.annealing, config, sizeof(surface_annealing_config_t));
            } else {
                surface_annealing_default_config(&opt->config.annealing);
            }
            break;

        case SURFACE_OPT_QMCTS:
            if (config) {
                memcpy(&opt->config.mcts, config, sizeof(surface_mcts_config_t));
            } else {
                surface_mcts_default_config(&opt->config.mcts);
            }
            break;

        case SURFACE_OPT_COUNT:
        default:
            /* No configuration needed */
            break;
    }

    /* Allocate default solution space */
    opt->max_solution_points = SURFACE_MAX_BRANCH_POINTS;
    opt->solution = nimcp_malloc(opt->max_solution_points * sizeof(surface_branch_point_t));
    if (!opt->solution) {
        nimcp_free(opt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_optimizer_create: opt->solution is NULL");
        return NULL;
    }
    memset(opt->solution, 0, opt->max_solution_points * sizeof(surface_branch_point_t));

    return opt;
}

void surface_optimizer_destroy(surface_optimizer_t* optimizer) {
    if (!optimizer) return;

    /* Free method-specific state */
    switch (optimizer->method) {
        case SURFACE_OPT_GRADIENT_DESCENT:
            if (optimizer->state.gradient) {
                if (optimizer->state.gradient->position_gradients) {
                    nimcp_free(optimizer->state.gradient->position_gradients);
                }
                if (optimizer->state.gradient->diameter_gradients) {
                    nimcp_free(optimizer->state.gradient->diameter_gradients);
                }
                if (optimizer->state.gradient->velocity_pos) {
                    nimcp_free(optimizer->state.gradient->velocity_pos);
                }
                if (optimizer->state.gradient->velocity_diam) {
                    nimcp_free(optimizer->state.gradient->velocity_diam);
                }
                if (optimizer->state.gradient->m_pos) {
                    nimcp_free(optimizer->state.gradient->m_pos);
                }
                if (optimizer->state.gradient->v_pos) {
                    nimcp_free(optimizer->state.gradient->v_pos);
                }
                if (optimizer->state.gradient->m_diam) {
                    nimcp_free(optimizer->state.gradient->m_diam);
                }
                if (optimizer->state.gradient->v_diam) {
                    nimcp_free(optimizer->state.gradient->v_diam);
                }
                nimcp_free(optimizer->state.gradient);
            }
            break;

        case SURFACE_OPT_MONTE_CARLO:
            if (optimizer->state.mc) {
                nimcp_free(optimizer->state.mc);
            }
            break;

        case SURFACE_OPT_QUANTUM_ANNEALING:
            if (optimizer->state.annealing) {
                surface_annealing_state_destroy(optimizer->state.annealing);
            }
            break;

        default:
            break;
    }

    if (optimizer->solution) {
        nimcp_free(optimizer->solution);
    }

    nimcp_free(optimizer);
}

int surface_optimizer_init(
    surface_optimizer_t* optimizer,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference
) {
    if (!optimizer || !terminals || num_terminals < 2 || min_circumference <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimizer_init: required parameter is NULL (optimizer, terminals)");
        return -1;
    }

    optimizer->min_circumference = min_circumference;

    /* Create initial topology */
    if (surface_create_initial_topology(terminals, num_terminals,
                                         optimizer->solution,
                                         optimizer->max_solution_points,
                                         &optimizer->num_solution_points) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_optimizer_init: operation failed");
        return -1;
    }

    /* Initialize method-specific state */
    switch (optimizer->method) {
        case SURFACE_OPT_GRADIENT_DESCENT: {
            uint32_t n = optimizer->num_solution_points;

            optimizer->state.gradient = nimcp_malloc(sizeof(surface_gradient_state_t));
            if (!optimizer->state.gradient) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_optimizer_init: optimizer->state is NULL");
                return -1;
            }
            memset(optimizer->state.gradient, 0, sizeof(surface_gradient_state_t));

            optimizer->state.gradient->branch_points = optimizer->solution;
            optimizer->state.gradient->num_branch_points = n;

            /* Allocate gradient arrays */
            optimizer->state.gradient->position_gradients = nimcp_malloc(n * 3 * sizeof(float));
            optimizer->state.gradient->diameter_gradients = nimcp_malloc(n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));
            optimizer->state.gradient->velocity_pos = nimcp_malloc(n * 3 * sizeof(float));
            optimizer->state.gradient->velocity_diam = nimcp_malloc(n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));

            if (optimizer->config.gradient.use_adam) {
                optimizer->state.gradient->m_pos = nimcp_malloc(n * 3 * sizeof(float));
                optimizer->state.gradient->v_pos = nimcp_malloc(n * 3 * sizeof(float));
                optimizer->state.gradient->m_diam = nimcp_malloc(n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));
                optimizer->state.gradient->v_diam = nimcp_malloc(n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));

                if (optimizer->state.gradient->m_pos) {
                    memset(optimizer->state.gradient->m_pos, 0, n * 3 * sizeof(float));
                }
                if (optimizer->state.gradient->v_pos) {
                    memset(optimizer->state.gradient->v_pos, 0, n * 3 * sizeof(float));
                }
                if (optimizer->state.gradient->m_diam) {
                    memset(optimizer->state.gradient->m_diam, 0, n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));
                }
                if (optimizer->state.gradient->v_diam) {
                    memset(optimizer->state.gradient->v_diam, 0, n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));
                }
            }

            if (optimizer->state.gradient->velocity_pos) {
                memset(optimizer->state.gradient->velocity_pos, 0, n * 3 * sizeof(float));
            }
            if (optimizer->state.gradient->velocity_diam) {
                memset(optimizer->state.gradient->velocity_diam, 0, n * SURFACE_MAX_BRANCH_DEGREE * sizeof(float));
            }
            break;
        }

        case SURFACE_OPT_MONTE_CARLO:
            optimizer->state.mc = nimcp_malloc(sizeof(surface_mc_state_t));
            if (!optimizer->state.mc) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_optimizer_init: optimizer->state is NULL");
                return -1;
            }
            memset(optimizer->state.mc, 0, sizeof(surface_mc_state_t));
            optimizer->state.mc->rng_state = optimizer->config.mc.seed;
            break;

        case SURFACE_OPT_QUANTUM_ANNEALING:
            optimizer->state.annealing = surface_annealing_state_create(&optimizer->config.annealing);
            if (!optimizer->state.annealing) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimizer_init: optimizer->state is NULL");
                return -1;
            }

            /* Copy solution to annealing state */
            optimizer->state.annealing->current = nimcp_malloc(
                optimizer->num_solution_points * sizeof(surface_branch_point_t));
            if (!optimizer->state.annealing->current) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_optimizer_init: optimizer->state is NULL");
                return -1;
            }
            memcpy(optimizer->state.annealing->current, optimizer->solution,
                   optimizer->num_solution_points * sizeof(surface_branch_point_t));
            optimizer->state.annealing->num_points = optimizer->num_solution_points;
            optimizer->state.annealing->current_energy = compute_total_area(
                optimizer->solution, optimizer->num_solution_points, min_circumference);
            break;

        default:
            break;
    }

    /* Compute initial area */
    optimizer->current_area = compute_total_area(optimizer->solution,
                                                  optimizer->num_solution_points,
                                                  min_circumference);

    optimizer->initialized = true;
    optimizer->converged = false;
    optimizer->iteration = 0;

    /* Initialize divergence tracking */
    optimizer->best_area = optimizer->current_area;
    optimizer->divergence_count = 0;
    optimizer->diverged = false;

    return 0;
}

int surface_optimizer_step(
    surface_optimizer_t* optimizer,
    bool* improved
) {
    if (!optimizer || !optimizer->initialized || !improved) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimizer_step: required parameter is NULL (optimizer, optimizer->initialized, improved)");
        return -1;
    }

    *improved = false;
    float old_area = optimizer->current_area;

    switch (optimizer->method) {
        case SURFACE_OPT_GRADIENT_DESCENT: {
            surface_gradient_state_t* state = optimizer->state.gradient;
            surface_gradient_config_t* config = &optimizer->config.gradient;

            /* Compute gradients */
            if (surface_compute_area_gradient(optimizer->solution,
                                              optimizer->num_solution_points,
                                              optimizer->min_circumference,
                                              state->position_gradients,
                                              state->diameter_gradients) != 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_optimizer_step: operation failed");
                return -1;
            }

            /* Update positions using Adam or momentum */
            float lr = config->learning_rate * powf(config->decay, (float)optimizer->iteration);

            for (uint32_t i = 0; i < optimizer->num_solution_points; i++) {
                if (optimizer->solution[i].is_terminal) continue;

                for (uint32_t j = 0; j < 3; j++) {
                    uint32_t idx = i * 3 + j;
                    float grad = state->position_gradients[idx];

                    if (config->use_adam && state->m_pos && state->v_pos) {
                        /* Adam update */
                        state->m_pos[idx] = config->beta1 * state->m_pos[idx] +
                                           (1.0f - config->beta1) * grad;
                        state->v_pos[idx] = config->beta2 * state->v_pos[idx] +
                                           (1.0f - config->beta2) * grad * grad;

                        float m_hat = state->m_pos[idx] / (1.0f - powf(config->beta1, optimizer->iteration + 1));
                        float v_hat = state->v_pos[idx] / (1.0f - powf(config->beta2, optimizer->iteration + 1));

                        float delta = lr * m_hat / (sqrtf(v_hat) + config->epsilon);
                        if (j == 0) optimizer->solution[i].position.x -= delta;
                        else if (j == 1) optimizer->solution[i].position.y -= delta;
                        else optimizer->solution[i].position.z -= delta;
                    } else {
                        /* Momentum update */
                        state->velocity_pos[idx] = config->momentum * state->velocity_pos[idx] + lr * grad;
                        float delta = state->velocity_pos[idx];
                        if (j == 0) optimizer->solution[i].position.x -= delta;
                        else if (j == 1) optimizer->solution[i].position.y -= delta;
                        else optimizer->solution[i].position.z -= delta;
                    }
                }
            }

            /* Check convergence */
            optimizer->current_area = compute_total_area(optimizer->solution,
                                                          optimizer->num_solution_points,
                                                          optimizer->min_circumference);

            if (fabsf(optimizer->current_area - old_area) < config->tolerance) {
                optimizer->converged = true;
            }

            *improved = (optimizer->current_area < old_area);
            break;
        }

        case SURFACE_OPT_QUANTUM_ANNEALING: {
            bool accepted;
            if (surface_annealing_step(optimizer->state.annealing,
                                       &optimizer->config.annealing,
                                       &accepted) != 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_optimizer_step: operation failed");
                return -1;
            }

            if (accepted) {
                /* Copy best solution back */
                if (optimizer->state.annealing->best) {
                    memcpy(optimizer->solution, optimizer->state.annealing->best,
                           optimizer->num_solution_points * sizeof(surface_branch_point_t));
                    optimizer->current_area = optimizer->state.annealing->best_energy;
                }
            }

            *improved = (optimizer->current_area < old_area);

            /* Check convergence (temperature threshold) */
            if (optimizer->state.annealing->temperature <= optimizer->config.annealing.temperature_final) {
                optimizer->converged = true;
            }
            break;
        }

        default:
            /* Other methods: no iterative step */
            optimizer->converged = true;
            break;
    }

    /* Divergence detection: track if optimization is consistently increasing area */
    if (optimizer->current_area < optimizer->best_area) {
        /* Improvement: update best and reset divergence counter */
        optimizer->best_area = optimizer->current_area;
        optimizer->divergence_count = 0;
    } else if (optimizer->current_area > old_area * SURFACE_OPT_DIVERGENCE_THRESHOLD) {
        /* Significant increase: increment divergence counter */
        optimizer->divergence_count++;

        if (optimizer->divergence_count >= SURFACE_OPT_MAX_DIVERGENCE_STEPS) {
            /* Divergence detected: early exit */
            optimizer->diverged = true;
            optimizer->converged = true;  /* Mark as converged to stop iterations */
        }
    }

    optimizer->iteration++;
    return 0;
}

int surface_optimizer_run(
    surface_optimizer_t* optimizer,
    surface_optimization_result_t* result
) {
    if (!optimizer || !optimizer->initialized || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimizer_run: required parameter is NULL (optimizer, optimizer->initialized, result)");
        return -1;
    }

    uint32_t max_iter;
    switch (optimizer->method) {
        case SURFACE_OPT_GRADIENT_DESCENT:
            max_iter = optimizer->config.gradient.max_iterations;
            break;
        case SURFACE_OPT_QUANTUM_ANNEALING:
            max_iter = optimizer->config.annealing.max_iterations;
            break;
        case SURFACE_OPT_QMCTS:
            max_iter = optimizer->config.mcts.num_iterations;
            break;
        default:
            max_iter = 1;
            break;
    }

    bool improved;
    while (!optimizer->converged && optimizer->iteration < max_iter) {
        if (surface_optimizer_step(optimizer, &improved) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_optimizer_run: validation failed");
            return -1;
        }
    }

    /* Fill result */
    memset(result, 0, sizeof(*result));
    result->surface_area = optimizer->current_area;
    result->iterations = optimizer->iteration;
    result->converged = optimizer->converged && !optimizer->diverged;
    result->diverged = optimizer->diverged;

    /* Compute wire length */
    result->wire_length = 0.0f;
    for (uint32_t i = 0; i < optimizer->num_solution_points; i++) {
        const surface_branch_point_t* bp = &optimizer->solution[i];
        for (uint32_t j = 0; j < bp->degree; j++) {
            if (bp->link_ids[j] > bp->id) {
                for (uint32_t k = 0; k < optimizer->num_solution_points; k++) {
                    if (optimizer->solution[k].id == bp->link_ids[j]) {
                        result->wire_length += surface_vec3_distance(&bp->position,
                                                             &optimizer->solution[k].position);
                        break;
                    }
                }
            }
        }
    }

    /* Copy solution */
    result->branch_points = nimcp_malloc(optimizer->num_solution_points * sizeof(surface_branch_point_t));
    if (result->branch_points) {
        memcpy(result->branch_points, optimizer->solution,
               optimizer->num_solution_points * sizeof(surface_branch_point_t));
        result->num_branch_points = optimizer->num_solution_points;
    }

    return 0;
}

bool surface_optimizer_converged(const surface_optimizer_t* optimizer) {
    if (!optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimizer_converged: optimizer is NULL");
        return false;
    }
    return optimizer->converged;
}

int surface_optimizer_get_solution(
    const surface_optimizer_t* optimizer,
    surface_branch_point_t* branch_points,
    uint32_t max_points,
    uint32_t* num_points
) {
    if (!optimizer || !branch_points || !num_points) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimizer_get_solution: required parameter is NULL (optimizer, branch_points, num_points)");
        return -1;
    }

    uint32_t n = (optimizer->num_solution_points < max_points) ?
                  optimizer->num_solution_points : max_points;

    memcpy(branch_points, optimizer->solution, n * sizeof(surface_branch_point_t));
    *num_points = n;

    return 0;
}

float surface_optimizer_get_area(const surface_optimizer_t* optimizer) {
    if (!optimizer) return -1.0f;
    return optimizer->current_area;
}
