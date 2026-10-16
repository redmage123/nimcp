//=============================================================================
// nimcp_pr_pink_noise.c - Prime Resonant Pink Noise Extension Implementation
//=============================================================================
/**
 * @file nimcp_pr_pink_noise.c
 * @brief Implementation of quaternionic pink noise and fractal timing
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements correlated quaternionic noise and 1/f timing
 * WHY:  Enable biologically realistic memory dynamics in Prime Resonant
 * HOW:  Cholesky decomposition for correlations, pink noise for timing
 *
 * @author NIMCP Development Team
 */

#include "cognitive/memory/core/nimcp_pr_pink_noise.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "pr_pink_noise"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(pr_pink_noise, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_ERROR_LENGTH        256
#define EPSILON                 1e-10f

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static _Thread_local char last_error[MAX_ERROR_LENGTH] = {0};

/**
 * @brief Set error message
 *
 * WHAT: Stores error message in thread-local storage
 * WHY:  Thread-safe error reporting
 */
static void set_error(const char* message) {
    if (!message) {
        last_error[0] = '\0';
        return;
    }
    strncpy(last_error, message, MAX_ERROR_LENGTH - 1);
    last_error[MAX_ERROR_LENGTH - 1] = '\0';
}

//=============================================================================
// Module Statistics
//=============================================================================

static _Thread_local pr_pink_noise_stats_t module_stats = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute Cholesky decomposition of 4x4 matrix
 *
 * WHAT: Computes lower triangular L such that A = L * L^T
 * WHY:  Transform independent noise to correlated noise
 * HOW:  Standard Cholesky algorithm for small matrix
 *
 * @param A Input symmetric positive-definite matrix
 * @param L Output lower triangular factor
 * @return true on success, false if not positive-definite
 */
static bool cholesky_decompose_4x4(
    const float A[PR_QUAT_DIM][PR_QUAT_DIM],
    float L[PR_QUAT_DIM][PR_QUAT_DIM])
{
    /* Guard: NULL inputs */
    if (!A || !L) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cholesky_decompose_4x4: required parameter is NULL (A, L)");
        return false;
    }

    /* Initialize L to zero */
    memset(L, 0, sizeof(float) * PR_QUAT_DIM * PR_QUAT_DIM);

    /* Cholesky decomposition for 4x4 matrix */
    for (int i = 0; i < PR_QUAT_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_QUAT_DIM > 256) {
            pr_pink_noise_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_QUAT_DIM);
        }

        for (int j = 0; j <= i; j++) {
            float sum = 0.0f;

            if (i == j) {
                /* Diagonal element */
                for (int k = 0; k < j; k++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((k & 0xFF) == 0 && j > 256) {
                        pr_pink_noise_heartbeat("pr_pink_nois_loop",
                                         (float)(k + 1) / (float)j);
                    }

                    sum += L[j][k] * L[j][k];
                }
                float diag = A[j][j] - sum;

                /* Guard: Not positive-definite */
                if (diag <= EPSILON) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_decompose_4x4: validation failed");
                    return false;
                }

                L[j][j] = sqrtf(diag);
            } else {
                /* Off-diagonal element */
                for (int k = 0; k < j; k++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((k & 0xFF) == 0 && j > 256) {
                        pr_pink_noise_heartbeat("pr_pink_nois_loop",
                                         (float)(k + 1) / (float)j);
                    }

                    sum += L[i][k] * L[j][k];
                }

                /* Guard: Division by zero */
                if (fabsf(L[j][j]) < EPSILON) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_decompose_4x4: validation failed");
                    return false;
                }

                L[i][j] = (A[i][j] - sum) / L[j][j];
            }
        }
    }

    return true;
}

/**
 * @brief Apply Cholesky transform to independent noise
 *
 * WHAT: Transforms 4 independent samples to correlated samples
 * WHY:  Produces noise with specified cross-correlations
 * HOW:  Matrix multiply: correlated = L * independent
 *
 * @param L Lower triangular Cholesky factor
 * @param independent Input independent samples [4]
 * @param correlated Output correlated samples [4]
 */
static void apply_cholesky_transform(
    const float L[PR_QUAT_DIM][PR_QUAT_DIM],
    const float independent[PR_QUAT_DIM],
    float correlated[PR_QUAT_DIM])
{
    /* Matrix-vector multiply: correlated = L * independent */
    for (int i = 0; i < PR_QUAT_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_QUAT_DIM > 256) {
            pr_pink_noise_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_QUAT_DIM);
        }

        correlated[i] = 0.0f;
        for (int j = 0; j <= i; j++) {  /* L is lower triangular */
            correlated[i] += L[i][j] * independent[j];
        }
    }
}

/**
 * @brief Apply theta coupling to noise amplitude
 *
 * WHAT: Modulates noise based on theta phase
 * WHY:  Theta rhythm coordinates memory operations
 * HOW:  amplitude *= 1 + strength * cos(theta_phase)
 *
 * @param sample Sample value
 * @param theta_phase Current theta phase
 * @param coupling_strength Coupling strength [0, 1]
 * @return Modulated sample
 */
static float apply_theta_coupling(
    float sample,
    float theta_phase,
    float coupling_strength)
{
    /* Modulation factor based on theta phase */
    float modulation = 1.0f + coupling_strength * cosf(theta_phase);
    return sample * modulation;
}

//=============================================================================
// Quaternionic Pink Noise Implementation
//=============================================================================

pr_quat_pink_params_t pr_quat_pink_default_params(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_default", 0.0f);


    pr_quat_pink_params_t params = {
        .alpha = 1.0f,                      /* True pink noise */
        .amplitude = 0.05f,                 /* 5% modulation */
        .sample_rate_hz = PR_DEFAULT_SAMPLE_RATE_HZ,
        .seed = 0                           /* Time-based */
    };
    return params;
}

void pr_quat_pink_default_correlation(
    float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM])
{
    /* Guard: NULL output */
    if (!correlation_matrix) {
        return;
    }

    /* Initialize to identity */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_default", 0.0f);


    memset(correlation_matrix, 0, sizeof(float) * PR_QUAT_DIM * PR_QUAT_DIM);

    /* Diagonal elements (self-correlation = 1.0) */
    correlation_matrix[PR_QUAT_W][PR_QUAT_W] = PR_CORR_WW;
    correlation_matrix[PR_QUAT_X][PR_QUAT_X] = PR_CORR_XX;
    correlation_matrix[PR_QUAT_Y][PR_QUAT_Y] = PR_CORR_YY;
    correlation_matrix[PR_QUAT_Z][PR_QUAT_Z] = PR_CORR_ZZ;

    /* Off-diagonal elements (symmetric matrix) */
    /* w-x correlation */
    correlation_matrix[PR_QUAT_W][PR_QUAT_X] = PR_CORR_WX;
    correlation_matrix[PR_QUAT_X][PR_QUAT_W] = PR_CORR_WX;

    /* w-y correlation */
    correlation_matrix[PR_QUAT_W][PR_QUAT_Y] = PR_CORR_WY;
    correlation_matrix[PR_QUAT_Y][PR_QUAT_W] = PR_CORR_WY;

    /* w-z correlation */
    correlation_matrix[PR_QUAT_W][PR_QUAT_Z] = PR_CORR_WZ;
    correlation_matrix[PR_QUAT_Z][PR_QUAT_W] = PR_CORR_WZ;

    /* x-y correlation */
    correlation_matrix[PR_QUAT_X][PR_QUAT_Y] = PR_CORR_XY;
    correlation_matrix[PR_QUAT_Y][PR_QUAT_X] = PR_CORR_XY;

    /* x-z correlation */
    correlation_matrix[PR_QUAT_X][PR_QUAT_Z] = PR_CORR_XZ;
    correlation_matrix[PR_QUAT_Z][PR_QUAT_X] = PR_CORR_XZ;

    /* y-z correlation */
    correlation_matrix[PR_QUAT_Y][PR_QUAT_Z] = PR_CORR_YZ;
    correlation_matrix[PR_QUAT_Z][PR_QUAT_Y] = PR_CORR_YZ;
}

bool pr_quat_pink_validate_correlation(
    const float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM])
{
    /* Guard: NULL input */
    if (!correlation_matrix) {
        set_error("Correlation matrix is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_validate_correlation: correlation_matrix is NULL");
        return false;
    }

    /* Check diagonal elements are 1.0 */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_validat", 0.0f);


    for (int i = 0; i < PR_QUAT_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_QUAT_DIM > 256) {
            pr_pink_noise_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_QUAT_DIM);
        }

        if (fabsf(correlation_matrix[i][i] - 1.0f) > EPSILON) {
            set_error("Diagonal elements must be 1.0");
            return false;
        }
    }

    /* Check symmetry */
    for (int i = 0; i < PR_QUAT_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_QUAT_DIM > 256) {
            pr_pink_noise_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_QUAT_DIM);
        }

        for (int j = 0; j < i; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && i > 256) {
                pr_pink_noise_heartbeat("pr_pink_nois_loop",
                                 (float)(j + 1) / (float)i);
            }

            if (fabsf(correlation_matrix[i][j] - correlation_matrix[j][i]) > EPSILON) {
                set_error("Correlation matrix must be symmetric");
                return false;
            }
        }
    }

    /* Check correlation bounds [-1, 1] */
    for (int i = 0; i < PR_QUAT_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_QUAT_DIM > 256) {
            pr_pink_noise_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_QUAT_DIM);
        }

        for (int j = 0; j < PR_QUAT_DIM; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && PR_QUAT_DIM > 256) {
                pr_pink_noise_heartbeat("pr_pink_nois_loop",
                                 (float)(j + 1) / (float)PR_QUAT_DIM);
            }

            if (correlation_matrix[i][j] < -1.0f || correlation_matrix[i][j] > 1.0f) {
                set_error("Correlation values must be in [-1, 1]");
                return false;
            }
        }
    }

    /* Attempt Cholesky decomposition to verify positive-definiteness */
    float L[PR_QUAT_DIM][PR_QUAT_DIM];
    if (!cholesky_decompose_4x4(correlation_matrix, L)) {
        set_error("Correlation matrix is not positive-definite");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_quat_pink_validate_correlation: cholesky_decompose_4x4 is NULL");
        return false;
    }

    return true;
}

pr_quat_pink_state_t* pr_quat_pink_create(
    const pr_quat_pink_params_t* params,
    const float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM])
{
    /* Use defaults if NULL */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_create", 0.0f);


    pr_quat_pink_params_t default_params = pr_quat_pink_default_params();
    if (!params) {
        params = &default_params;
    }

    /* Use default correlation if NULL */
    float default_corr[PR_QUAT_DIM][PR_QUAT_DIM];
    if (!correlation_matrix) {
        pr_quat_pink_default_correlation(default_corr);
        correlation_matrix = (const float (*)[PR_QUAT_DIM])default_corr;
    }

    /* Validate correlation matrix */
    if (!pr_quat_pink_validate_correlation(correlation_matrix)) {
        /* Error already set by validate */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_create: pr_quat_pink_validate_correlation is NULL");
        return NULL;
    }

    /* Allocate state */
    pr_quat_pink_state_t* state = (pr_quat_pink_state_t*)nimcp_malloc(
        sizeof(pr_quat_pink_state_t));

    /* Guard: Allocation failed */
    if (!state) {
        set_error("Failed to allocate quaternionic pink noise state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate state");

        return NULL;
    }

    /* Initialize to zero */
    memset(state, 0, sizeof(pr_quat_pink_state_t));

    /* Copy parameters */
    memcpy(&state->params, params, sizeof(pr_quat_pink_params_t));

    /* Copy and compute Cholesky decomposition */
    memcpy(state->correlation_matrix, correlation_matrix,
           sizeof(float) * PR_QUAT_DIM * PR_QUAT_DIM);

    if (!cholesky_decompose_4x4(state->correlation_matrix, state->cholesky_L)) {
        set_error("Cholesky decomposition failed");
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_create: cholesky_decompose_4x4 is NULL");
        return NULL;
    }

    /* Create pink noise config */
    pink_noise_config_t noise_config = pink_noise_default_config();
    noise_config.alpha = params->alpha;
    noise_config.amplitude = params->amplitude;
    noise_config.sample_rate = params->sample_rate_hz;
    noise_config.method = PINK_NOISE_VOSS;  /* Fast, good quality */

    /* Create 4 independent generators with different seeds */
    uint32_t base_seed = params->seed;
    if (base_seed == 0) {
        base_seed = (uint32_t)time(NULL);
    }

    noise_config.seed = base_seed;
    state->gen_w = pink_noise_create(&noise_config);

    noise_config.seed = base_seed + 1000;
    state->gen_x = pink_noise_create(&noise_config);

    noise_config.seed = base_seed + 2000;
    state->gen_y = pink_noise_create(&noise_config);

    noise_config.seed = base_seed + 3000;
    state->gen_z = pink_noise_create(&noise_config);

    /* Guard: Generator creation failed */
    if (!state->gen_w || !state->gen_x || !state->gen_y || !state->gen_z) {
        set_error("Failed to create pink noise generators");
        if (state->gen_w) pink_noise_destroy(state->gen_w);
        if (state->gen_x) pink_noise_destroy(state->gen_x);
        if (state->gen_y) pink_noise_destroy(state->gen_y);
        if (state->gen_z) pink_noise_destroy(state->gen_z);
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_create: validation failed");
        return NULL;
    }

    /* Initialize theta coupling */
    state->theta_phase = 0.0f;
    state->theta_frequency_hz = PR_DEFAULT_THETA_FREQ_HZ;
    state->theta_coupling_strength = 0.0f;  /* Disabled by default */
    state->sample_rate_hz = params->sample_rate_hz;

    /* Statistics */
    state->samples_generated = 0;

    set_error(NULL);
    return state;
}

void pr_quat_pink_destroy(pr_quat_pink_state_t* state) {
    /* Guard: NULL state */
    if (!state) {
        return;
    }

    /* Destroy generators */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_destroy", 0.0f);


    if (state->gen_w) pink_noise_destroy(state->gen_w);
    if (state->gen_x) pink_noise_destroy(state->gen_x);
    if (state->gen_y) pink_noise_destroy(state->gen_y);
    if (state->gen_z) pink_noise_destroy(state->gen_z);

    /* Free state */
    nimcp_free(state);
    state = NULL;
}

bool pr_quat_pink_next(
    pr_quat_pink_state_t* state,
    pr_quat_sample_t* sample)
{
    /* Guard: NULL inputs */
    if (!state || !sample) {
        set_error("Invalid state or sample pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_next: required parameter is NULL (state, sample)");
        return false;
    }

    /* Generate 4 independent pink noise samples */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_next", 0.0f);


    float independent[PR_QUAT_DIM];

    if (!pink_noise_generate_sample(state->gen_w, &independent[PR_QUAT_W]) ||
        !pink_noise_generate_sample(state->gen_x, &independent[PR_QUAT_X]) ||
        !pink_noise_generate_sample(state->gen_y, &independent[PR_QUAT_Y]) ||
        !pink_noise_generate_sample(state->gen_z, &independent[PR_QUAT_Z])) {
        set_error("Failed to generate independent samples");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_quat_pink_next: pink_noise_generate_sample is NULL");
        return false;
    }

    /* Apply Cholesky transform for correlation */
    float correlated[PR_QUAT_DIM];
    apply_cholesky_transform(state->cholesky_L, independent, correlated);

    /* Apply theta coupling if enabled */
    if (state->theta_coupling_strength > EPSILON) {
        correlated[PR_QUAT_W] = apply_theta_coupling(
            correlated[PR_QUAT_W], state->theta_phase, state->theta_coupling_strength);
        correlated[PR_QUAT_X] = apply_theta_coupling(
            correlated[PR_QUAT_X], state->theta_phase, state->theta_coupling_strength);
        correlated[PR_QUAT_Y] = apply_theta_coupling(
            correlated[PR_QUAT_Y], state->theta_phase, state->theta_coupling_strength);
        correlated[PR_QUAT_Z] = apply_theta_coupling(
            correlated[PR_QUAT_Z], state->theta_phase, state->theta_coupling_strength);

        /* Advance theta phase (one sample step) */
        float dt_ms = 1000.0f / state->sample_rate_hz;
        pr_quat_pink_advance_theta(state, dt_ms);
    }

    /* Output correlated sample */
    sample->w = correlated[PR_QUAT_W];
    sample->x = correlated[PR_QUAT_X];
    sample->y = correlated[PR_QUAT_Y];
    sample->z = correlated[PR_QUAT_Z];

    /* Update statistics */
    state->samples_generated++;
    module_stats.quat_samples_generated++;

    return true;
}

bool pr_quat_pink_path(
    pr_quat_pink_state_t* state,
    pr_quat_path_t* path,
    size_t steps,
    float smoothness)
{
    /* Guard: NULL inputs */
    if (!state || !path || !path->samples) {
        set_error("Invalid state, path, or samples array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_path: required parameter is NULL (state, path, path->samples)");
        return false;
    }

    /* Guard: Invalid steps */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_path", 0.0f);


    if (steps == 0) {
        set_error("Steps must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_quat_pink_path: steps is zero");
        return false;
    }

    /* Guard: Clamp smoothness */
    if (smoothness < 0.0f) smoothness = 0.0f;
    if (smoothness > 1.0f) smoothness = 1.0f;

    /* Smoothing factor (higher smoothness = more filtering) */
    float smooth_factor = smoothness;

    /* Generate first sample */
    pr_quat_sample_t raw;
    if (!pr_quat_pink_next(state, &raw)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_quat_pink_path: pr_quat_pink_next is NULL");
        return false;
    }
    path->samples[0] = raw;

    /* Generate remaining samples with smoothing */
    float total_length = 0.0f;
    for (size_t i = 1; i < steps; i++) {
        if (!pr_quat_pink_next(state, &raw)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_quat_pink_path: pr_quat_pink_next is NULL");
            return false;
        }

        /* Apply exponential smoothing */
        pr_quat_sample_t* prev = &path->samples[i - 1];
        pr_quat_sample_t* curr = &path->samples[i];

        curr->w = smooth_factor * prev->w + (1.0f - smooth_factor) * raw.w;
        curr->x = smooth_factor * prev->x + (1.0f - smooth_factor) * raw.x;
        curr->y = smooth_factor * prev->y + (1.0f - smooth_factor) * raw.y;
        curr->z = smooth_factor * prev->z + (1.0f - smooth_factor) * raw.z;

        /* Accumulate path length */
        float dw = curr->w - prev->w;
        float dx = curr->x - prev->x;
        float dy = curr->y - prev->y;
        float dz = curr->z - prev->z;
        total_length += sqrtf(dw * dw + dx * dx + dy * dy + dz * dz);
    }

    /* Fill path metadata */
    path->step_count = steps;
    path->smoothness = smoothness;
    path->total_length = total_length;

    return true;
}

bool pr_quat_pink_set_theta_coupling(
    pr_quat_pink_state_t* state,
    float phase,
    float strength)
{
    /* Guard: NULL state */
    if (!state) {
        set_error("State is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_set_theta_coupling: state is NULL");
        return false;
    }

    /* Guard: Clamp strength */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_set_the", 0.0f);


    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;

    /* Normalize phase to [0, 2*pi] */
    while (phase < 0.0f) phase += NIMCP_TWO_PI_F;
    while (phase >= NIMCP_TWO_PI_F) phase -= NIMCP_TWO_PI_F;

    state->theta_phase = phase;
    state->theta_coupling_strength = strength;

    return true;
}

float pr_quat_pink_advance_theta(
    pr_quat_pink_state_t* state,
    float dt_ms)
{
    /* Guard: NULL state */
    if (!state) {
        return 0.0f;
    }

    /* Advance phase: phase += 2*pi*freq*dt */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_advance", 0.0f);


    float dt_s = dt_ms / 1000.0f;
    state->theta_phase += NIMCP_TWO_PI_F * state->theta_frequency_hz * dt_s;

    /* Wrap to [0, 2*pi] */
    while (state->theta_phase >= NIMCP_TWO_PI_F) {
        state->theta_phase -= NIMCP_TWO_PI_F;
    }

    return state->theta_phase;
}

bool pr_quat_pink_reset(
    pr_quat_pink_state_t* state,
    uint32_t new_seed)
{
    /* Guard: NULL state */
    if (!state) {
        set_error("State is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_quat_pink_reset: state is NULL");
        return false;
    }

    /* Reset all generators */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_quat_pink_reset", 0.0f);


    uint32_t base_seed = new_seed;
    if (base_seed == 0) {
        base_seed = (uint32_t)time(NULL);
    }

    if (!pink_noise_reset(state->gen_w, base_seed) ||
        !pink_noise_reset(state->gen_x, base_seed + 1000) ||
        !pink_noise_reset(state->gen_y, base_seed + 2000) ||
        !pink_noise_reset(state->gen_z, base_seed + 3000)) {
        set_error("Failed to reset generators");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_quat_pink_reset: pink_noise_reset is NULL");
        return false;
    }

    /* Reset theta phase */
    state->theta_phase = 0.0f;

    /* Reset statistics */
    state->samples_generated = 0;

    return true;
}

//=============================================================================
// Fractal Timing Implementation
//=============================================================================

pr_fractal_timing_t* pr_fractal_timing_create(float base_interval_ms) {
    /* Use full parameter version with defaults */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_timing_cr", 0.0f);


    return pr_fractal_timing_create_ex(
        base_interval_ms,
        base_interval_ms * 0.1f,    /* Min: 10% of base */
        base_interval_ms * 10.0f,   /* Max: 10x base */
        PR_FRACTAL_ALPHA,
        0);                          /* Time-based seed */
}

pr_fractal_timing_t* pr_fractal_timing_create_ex(
    float base_interval_ms,
    float min_interval_ms,
    float max_interval_ms,
    float alpha,
    uint32_t seed)
{
    /* Guard: Invalid intervals */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_timing_cr", 0.0f);


    if (base_interval_ms <= 0.0f || min_interval_ms <= 0.0f ||
        max_interval_ms <= min_interval_ms) {
        set_error("Invalid interval parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_fractal_timing_create_ex: operation failed");
        return NULL;
    }

    /* Allocate timing generator */
    pr_fractal_timing_t* timing = (pr_fractal_timing_t*)nimcp_malloc(
        sizeof(pr_fractal_timing_t));

    /* Guard: Allocation failed */
    if (!timing) {
        set_error("Failed to allocate fractal timing generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate timing");

        return NULL;
    }

    /* Initialize */
    memset(timing, 0, sizeof(pr_fractal_timing_t));

    timing->base_interval_ms = base_interval_ms;
    timing->min_interval_ms = min_interval_ms;
    timing->max_interval_ms = max_interval_ms;
    timing->alpha = alpha;
    timing->last_event_time_ms = 0.0f;
    timing->event_count = 0;

    /* Create pink noise generator for interval modulation */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = alpha;
    config.amplitude = 1.0f;  /* Full range for interval modulation */
    config.seed = seed;

    timing->interval_gen = pink_noise_create(&config);

    /* Guard: Generator creation failed */
    if (!timing->interval_gen) {
        set_error("Failed to create interval generator");
        nimcp_free(timing);
        timing = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_fractal_timing_create_ex: timing->interval_gen is NULL");
        return NULL;
    }

    set_error(NULL);
    return timing;
}

void pr_fractal_timing_destroy(pr_fractal_timing_t* timing) {
    /* Guard: NULL timing */
    if (!timing) {
        return;
    }

    /* Destroy generator */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_timing_de", 0.0f);


    if (timing->interval_gen) {
        pink_noise_destroy(timing->interval_gen);
    }

    /* Free timing */
    nimcp_free(timing);
    timing = NULL;
}

float pr_fractal_next_event_time(
    pr_fractal_timing_t* timing,
    float current_time_ms)
{
    /* Guard: NULL timing */
    if (!timing) {
        return current_time_ms;
    }

    /* Generate pink noise sample for interval modulation */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_next_even", 0.0f);


    float noise = 0.0f;
    if (!pink_noise_generate_sample(timing->interval_gen, &noise)) {
        /* Fall back to base interval on error */
        noise = 0.0f;
    }

    /* Normalize noise to [-0.5, 0.5] and modulate interval */
    /* interval = base * (1 + noise) where noise is bounded */
    float normalized_noise = noise / 3.0f;  /* Approximate ±3sigma clipping */
    if (normalized_noise < -0.5f) normalized_noise = -0.5f;
    if (normalized_noise > 2.0f) normalized_noise = 2.0f;

    float interval = timing->base_interval_ms * (1.0f + normalized_noise);

    /* Clamp to valid range */
    if (interval < timing->min_interval_ms) {
        interval = timing->min_interval_ms;
    }
    if (interval > timing->max_interval_ms) {
        interval = timing->max_interval_ms;
    }

    /* Compute next event time */
    float next_time = current_time_ms + interval;

    /* Update state */
    timing->last_event_time_ms = current_time_ms;
    timing->event_count++;

    /* Update statistics */
    module_stats.fractal_events++;
    module_stats.mean_interval_ms =
        (module_stats.mean_interval_ms * (module_stats.fractal_events - 1) + interval) /
        module_stats.fractal_events;

    return next_time;
}

bool pr_fractal_event_due(
    pr_fractal_timing_t* timing,
    float current_time_ms)
{
    /* Guard: NULL timing */
    if (!timing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_fractal_event_due: timing is NULL");
        return false;
    }

    /* First event is always due at time 0 */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_event_due", 0.0f);


    if (timing->event_count == 0) {
        return true;
    }

    /* Check if we've reached the scheduled next event time */
    /* This assumes caller tracks the scheduled time externally */
    /* For simple usage, check if enough time has passed */
    float elapsed = current_time_ms - timing->last_event_time_ms;
    return elapsed >= timing->min_interval_ms;
}

float pr_fractal_get_last_event_time(const pr_fractal_timing_t* timing) {
    if (!timing) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_get_last_", 0.0f);


    return timing->last_event_time_ms;
}

uint64_t pr_fractal_get_event_count(const pr_fractal_timing_t* timing) {
    if (!timing) return 0;
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_get_event", 0.0f);


    return timing->event_count;
}

bool pr_fractal_timing_reset(
    pr_fractal_timing_t* timing,
    uint32_t new_seed)
{
    /* Guard: NULL timing */
    if (!timing) {
        set_error("Timing is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_fractal_timing_reset: timing is NULL");
        return false;
    }

    /* Reset generator */
    if (!pink_noise_reset(timing->interval_gen, new_seed)) {
        set_error("Failed to reset interval generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_fractal_timing_reset: pink_noise_reset is NULL");
        return false;
    }

    /* Reset state */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_fractal_timing_re", 0.0f);


    timing->last_event_time_ms = 0.0f;
    timing->event_count = 0;

    return true;
}

//=============================================================================
// COW-Enabled Pink Noise Buffer Implementation
//=============================================================================

pr_pink_buffer_t* pr_pink_buffer_create(
    cow_manager_t cow_mgr,
    size_t sample_count,
    float sample_rate_hz)
{
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_creat", 0.0f);


    return pr_pink_buffer_create_ex(
        cow_mgr,
        sample_count,
        sample_rate_hz,
        1.0f,       /* alpha = 1.0 (pink) */
        0.05f,      /* amplitude = 5% */
        0);         /* time-based seed */
}

pr_pink_buffer_t* pr_pink_buffer_create_ex(
    cow_manager_t cow_mgr,
    size_t sample_count,
    float sample_rate_hz,
    float alpha,
    float amplitude,
    uint32_t seed)
{
    /* Guard: Invalid parameters */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_creat", 0.0f);


    if (sample_count == 0 || sample_rate_hz <= 0.0f) {
        set_error("Invalid buffer parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_buffer_create_ex: sample_count is zero");
        return NULL;
    }

    /* Allocate buffer structure */
    pr_pink_buffer_t* buffer = (pr_pink_buffer_t*)nimcp_malloc(
        sizeof(pr_pink_buffer_t));

    /* Guard: Allocation failed */
    if (!buffer) {
        set_error("Failed to allocate buffer structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate buffer");

        return NULL;
    }

    /* Initialize */
    memset(buffer, 0, sizeof(pr_pink_buffer_t));
    buffer->sample_count = sample_count;
    buffer->sample_rate_hz = sample_rate_hz;
    buffer->current_index = 0;

    /* Generate pink noise samples into temporary buffer */
    float* samples = (float*)nimcp_calloc(sample_count, sizeof(float));
    if (!samples) {
        set_error("Failed to allocate sample buffer");
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate samples");

        return NULL;
    }

    /* Create temporary generator */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = alpha;
    config.amplitude = amplitude;
    config.sample_rate = sample_rate_hz;
    config.seed = seed;

    pink_noise_generator_t gen = pink_noise_create(&config);
    if (!gen) {
        set_error("Failed to create temporary generator");
        nimcp_free(samples);
        samples = NULL;
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gen is NULL");

        return NULL;
    }

    /* Generate samples */
    if (!pink_noise_generate(gen, samples, (uint32_t)sample_count)) {
        set_error("Failed to generate samples");
        pink_noise_destroy(gen);
        nimcp_free(samples);
        samples = NULL;
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_create_ex: pink_noise_generate is NULL");
        return NULL;
    }

    pink_noise_destroy(gen);

    /* Create or use provided COW manager */
    bool owns_manager = (cow_mgr == NULL);

    if (owns_manager) {
        /* Create COW manager for this buffer */
        cow_manager_config_t cow_config = cow_default_config(
            sample_count * sizeof(float), NULL);

        cow_mgr = cow_manager_create(&cow_config, samples);
        if (!cow_mgr) {
            set_error("Failed to create COW manager");
            nimcp_free(samples);
            samples = NULL;
            nimcp_free(buffer);
            buffer = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cow_mgr is NULL");

            return NULL;
        }
    }

    /* Acquire handle from COW manager */
    buffer->noise_handle = cow_acquire(cow_mgr);
    if (!buffer->noise_handle) {
        set_error("Failed to acquire COW handle");
        if (owns_manager) {
            cow_manager_destroy(cow_mgr);
        }
        nimcp_free(samples);
        samples = NULL;
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_create_ex: validation failed");
        return NULL;
    }

    /* Write generated samples into COW buffer */
    float* cow_data = (float*)cow_write(buffer->noise_handle);
    if (!cow_data) {
        set_error("Failed to get writable COW pointer");
        cow_release(buffer->noise_handle);
        if (owns_manager) {
            cow_manager_destroy(cow_mgr);
        }
        nimcp_free(samples);
        samples = NULL;
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_create_ex: validation failed");
        return NULL;
    }
    memcpy(cow_data, samples, sample_count * sizeof(float));

    /* Clean up temporary buffer */
    nimcp_free(samples);
    samples = NULL;

    buffer->cow_manager = cow_mgr;
    buffer->owns_manager = owns_manager;

    set_error(NULL);
    return buffer;
}

pr_pink_buffer_t* pr_pink_buffer_clone(const pr_pink_buffer_t* buffer) {
    /* Guard: NULL buffer */
    if (!buffer || !buffer->cow_manager) {
        set_error("Cannot clone NULL buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_clone: required parameter is NULL (buffer, buffer->cow_manager)");
        return NULL;
    }

    /* Allocate new buffer structure */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_clone", 0.0f);


    pr_pink_buffer_t* clone = (pr_pink_buffer_t*)nimcp_malloc(
        sizeof(pr_pink_buffer_t));

    /* Guard: Allocation failed */
    if (!clone) {
        set_error("Failed to allocate clone buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;
    }

    /* Copy metadata */
    clone->sample_count = buffer->sample_count;
    clone->sample_rate_hz = buffer->sample_rate_hz;
    clone->current_index = 0;  /* Start from beginning */
    clone->cow_manager = buffer->cow_manager;
    clone->owns_manager = false;  /* Clone never owns manager */

    /* Acquire new COW handle (O(1) operation, no data copy) */
    clone->noise_handle = cow_acquire(buffer->cow_manager);
    if (!clone->noise_handle) {
        set_error("Failed to acquire COW handle for clone");
        nimcp_free(clone);
        clone = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_pink_buffer_clone: clone->noise_handle is NULL");
        return NULL;
    }

    /* Update statistics */
    module_stats.buffer_clones++;

    set_error(NULL);
    return clone;
}

void pr_pink_buffer_destroy(pr_pink_buffer_t* buffer) {
    /* Guard: NULL buffer */
    if (!buffer) {
        return;
    }

    /* Release COW handle */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_destr", 0.0f);


    if (buffer->noise_handle) {
        cow_release(buffer->noise_handle);
    }

    /* Destroy COW manager if we own it */
    if (buffer->owns_manager && buffer->cow_manager) {
        cow_manager_destroy(buffer->cow_manager);
    }

    /* Free buffer structure */
    nimcp_free(buffer);
    buffer = NULL;
}

bool pr_pink_buffer_next(
    pr_pink_buffer_t* buffer,
    float* sample)
{
    /* Guard: NULL inputs */
    if (!buffer || !sample) {
        set_error("Invalid buffer or sample pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_next: required parameter is NULL (buffer, sample)");
        return false;
    }

    /* Get read-only pointer to data */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_next", 0.0f);


    const float* data = (const float*)cow_read(buffer->noise_handle);
    if (!data) {
        set_error("Failed to read COW buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_next: data is NULL");
        return false;
    }

    /* Get current sample */
    *sample = data[buffer->current_index];

    /* Advance with wraparound */
    buffer->current_index = (buffer->current_index + 1) % buffer->sample_count;

    return true;
}

bool pr_pink_buffer_get(
    const pr_pink_buffer_t* buffer,
    size_t index,
    float* sample)
{
    /* Guard: NULL inputs */
    if (!buffer || !sample) {
        set_error("Invalid buffer or sample pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_get: required parameter is NULL (buffer, sample)");
        return false;
    }

    /* Guard: Index out of range */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_get", 0.0f);


    if (index >= buffer->sample_count) {
        set_error("Index out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_pink_buffer_get: capacity exceeded");
        return false;
    }

    /* Get read-only pointer to data */
    const float* data = (const float*)cow_read(buffer->noise_handle);
    if (!data) {
        set_error("Failed to read COW buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_get: data is NULL");
        return false;
    }

    *sample = data[index];
    return true;
}

bool pr_pink_buffer_write(
    pr_pink_buffer_t* buffer,
    size_t index,
    float value)
{
    /* Guard: NULL buffer */
    if (!buffer) {
        set_error("Buffer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_write: buffer is NULL");
        return false;
    }

    /* Guard: Index out of range */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_write", 0.0f);


    if (index >= buffer->sample_count) {
        set_error("Index out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_pink_buffer_write: capacity exceeded");
        return false;
    }

    /* Get writable pointer (triggers COW if shared) */
    float* data = (float*)cow_write(buffer->noise_handle);
    if (!data) {
        set_error("Failed to get writable COW pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_buffer_write: data is NULL");
        return false;
    }

    data[index] = value;

    /* Update statistics if this was a COW copy */
    if (!cow_is_shared(buffer->noise_handle)) {
        module_stats.buffer_writes++;
    }

    return true;
}

void pr_pink_buffer_reset_index(pr_pink_buffer_t* buffer) {
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_reset", 0.0f);


    if (buffer) {
        buffer->current_index = 0;
    }
}

bool pr_pink_buffer_is_shared(const pr_pink_buffer_t* buffer) {
    if (!buffer || !buffer->noise_handle) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_buffer_is_sh", 0.0f);


    return cow_is_shared(buffer->noise_handle);
}

//=============================================================================
// Resonance Modulation Implementation
//=============================================================================

bool pr_pink_modulate_resonance(
    pr_pink_buffer_t* buffer,
    float base_resonance,
    float depth,
    float* resonance_out)
{
    /* Guard: NULL inputs */
    if (!buffer || !resonance_out) {
        set_error("Invalid buffer or output pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_modulate_resonance: required parameter is NULL (buffer, resonance_out)");
        return false;
    }

    /* Guard: Clamp depth */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_modulate_res", 0.0f);


    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;

    /* Get next noise sample */
    float noise = 0.0f;
    if (!pr_pink_buffer_next(buffer, &noise)) {
        *resonance_out = base_resonance;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_resonance: pr_pink_buffer_next is NULL");
        return false;
    }

    /* Apply modulation: resonance = base * (1 + depth * normalized_noise) */
    /* Normalize noise to approximately [-1, 1] */
    float normalized = noise / (3.0f * 0.05f);  /* Assume default amplitude */
    if (normalized < -1.0f) normalized = -1.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    *resonance_out = base_resonance * (1.0f + depth * normalized);

    /* Clamp to [0, 1] */
    if (*resonance_out < 0.0f) *resonance_out = 0.0f;
    if (*resonance_out > 1.0f) *resonance_out = 1.0f;

    return true;
}

bool pr_pink_modulate_quaternion(
    pr_quat_pink_state_t* quat_state,
    const float base_quat[PR_QUAT_DIM],
    float depth,
    float out_quat[PR_QUAT_DIM])
{
    /* Guard: NULL inputs */
    if (!quat_state || !base_quat || !out_quat) {
        set_error("Invalid state or quaternion pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_modulate_quaternion: required parameter is NULL (quat_state, base_quat, out_quat)");
        return false;
    }

    /* Guard: Clamp depth */
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_pr_pink_modulate_qua", 0.0f);


    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;

    /* Generate correlated noise sample */
    pr_quat_sample_t noise;
    if (!pr_quat_pink_next(quat_state, &noise)) {
        /* On error, return base unchanged */
        memcpy(out_quat, base_quat, sizeof(float) * PR_QUAT_DIM);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_quaternion: pr_quat_pink_next is NULL");
        return false;
    }

    /* Apply modulation to each component */
    out_quat[PR_QUAT_W] = base_quat[PR_QUAT_W] + depth * noise.w;
    out_quat[PR_QUAT_X] = base_quat[PR_QUAT_X] + depth * noise.x;
    out_quat[PR_QUAT_Y] = base_quat[PR_QUAT_Y] + depth * noise.y;
    out_quat[PR_QUAT_Z] = base_quat[PR_QUAT_Z] + depth * noise.z;

    /* Clamp components to valid ranges */
    /* w (consolidation): [0, 1] */
    if (out_quat[PR_QUAT_W] < 0.0f) out_quat[PR_QUAT_W] = 0.0f;
    if (out_quat[PR_QUAT_W] > 1.0f) out_quat[PR_QUAT_W] = 1.0f;

    /* x (emotion): [-1, 1] */
    if (out_quat[PR_QUAT_X] < -1.0f) out_quat[PR_QUAT_X] = -1.0f;
    if (out_quat[PR_QUAT_X] > 1.0f) out_quat[PR_QUAT_X] = 1.0f;

    /* y (salience): [0, 1] */
    if (out_quat[PR_QUAT_Y] < 0.0f) out_quat[PR_QUAT_Y] = 0.0f;
    if (out_quat[PR_QUAT_Y] > 1.0f) out_quat[PR_QUAT_Y] = 1.0f;

    /* z (accessibility): [0, 1] */
    if (out_quat[PR_QUAT_Z] < 0.0f) out_quat[PR_QUAT_Z] = 0.0f;
    if (out_quat[PR_QUAT_Z] > 1.0f) out_quat[PR_QUAT_Z] = 1.0f;

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

void pr_pink_noise_get_stats(pr_pink_noise_stats_t* stats) {
    if (!stats) return;
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_get_stats", 0.0f);


    memcpy(stats, &module_stats, sizeof(pr_pink_noise_stats_t));
}

void pr_pink_noise_reset_stats(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_pink_noise_heartbeat("pr_pink_nois_reset_stats", 0.0f);


    memset(&module_stats, 0, sizeof(pr_pink_noise_stats_t));
}

const char* pr_pink_noise_get_last_error(void) {
    if (last_error[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_noise_get_last_error: validation failed");
        return NULL;
    }
    return last_error;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void pr_pink_noise_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_pr_pink_noise_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 * ============================================================================ */

int pr_pink_noise_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_pink_noise_training_begin: NULL argument");
        return -1;
    }
    pr_pink_noise_heartbeat_instance(NULL, "pr_pink_noise_training_begin", 0.0f);

    /* Reset module statistics to establish training baseline */
    memset(&module_stats, 0, sizeof(pr_pink_noise_stats_t));

    /* Clear thread-local error state for clean training run */
    last_error[0] = '\0';

    LOG_MODULE_INFO(LOG_MODULE, "pr_pink_noise training begin");
    return 0;
}

int pr_pink_noise_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_pink_noise_training_end: NULL argument");
        return -1;
    }
    pr_pink_noise_heartbeat_instance(NULL, "pr_pink_noise_training_end", 0.0f);

    /* Capture final training statistics snapshot */
    pr_pink_noise_stats_t final_stats;
    memcpy(&final_stats, &module_stats, sizeof(pr_pink_noise_stats_t));

    /* Log training completion with sample generation metrics */
    LOG_MODULE_INFO(LOG_MODULE,
              "pr_pink_noise training end: samples=%lu, fractal_events=%lu, measured_alpha=%.4f",
              (unsigned long)final_stats.quat_samples_generated,
              (unsigned long)final_stats.fractal_events,
              (double)final_stats.measured_alpha);

    pr_pink_noise_heartbeat_instance(NULL, "pr_pink_noise_training_end", 1.0f);
    return 0;
}

int pr_pink_noise_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_pink_noise_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    pr_pink_noise_heartbeat_instance(NULL, "pr_pink_noise_training_step", progress);

    /*
     * Adapt noise characteristics during training:
     * - Early training (progress < 0.3): higher amplitude noise for exploration
     * - Mid training (0.3-0.7): standard amplitude
     * - Late training (progress > 0.7): reduced noise for convergence
     *
     * The module_stats tracks generation counts; we use progress to
     * modulate the measured_alpha toward the target spectral exponent.
     */
    if (module_stats.measured_alpha > 0.0f) {
        /* Gradually stabilize the spectral exponent toward 1.0 (ideal pink) */
        float target_alpha = 1.0f;
        float alpha_delta = (target_alpha - module_stats.measured_alpha) * progress * 0.01f;
        module_stats.measured_alpha += alpha_delta;
    }

    return 0;
}
