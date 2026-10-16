//=============================================================================
// nimcp_pr_pink_noise_bridge.c - Pink Noise Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_pink_noise_bridge.c
 * @brief Implementation of pink noise bridge for Prime Resonant memory system
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implements the bridge between Prime Resonant memory and pink noise module
 * WHY:  Enable biologically-realistic 1/f modulation of all memory dynamics
 * HOW:  Maintains generators per target, fractal timing, correlated quaternion
 *       noise, and comprehensive statistics tracking
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/memory/core/nimcp_pr_pink_noise_bridge.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_pink_noise_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_pink_bridge module (instance-level) */
static inline void pr_pink_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_pink_noise_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_pink_noise_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_pink_noise_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
#define LOG_MODULE "PR_PINK_NOISE_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

/** Magic number for validation */
#define PR_PINK_BRIDGE_MAGIC            0x50494E4B  /* "PINK" */

/** Maximum targets (excluding ALL) */
#define PR_PINK_NUM_TARGETS             7

/** Default buffer size for internal operations */
#define PR_PINK_INTERNAL_BUFFER_SIZE    256

/** Minimum valid amplitude */
#define PR_PINK_MIN_AMPLITUDE           0.0f

/** Maximum valid amplitude */
#define PR_PINK_MAX_AMPLITUDE           1.0f

/** Minimum valid spectral exponent */
#define PR_PINK_MIN_SPECTRAL_EXPONENT   0.0f

/** Maximum valid spectral exponent */
#define PR_PINK_MAX_SPECTRAL_EXPONENT   3.0f

/** Minimum valid correlation time */
#define PR_PINK_MIN_CORRELATION_TIME    0.001f

/** Epsilon for floating-point comparisons */
#define PR_PINK_EPSILON                 NIMCP_EPSILON_NUMERICAL

/** Pi constant */

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static _Thread_local char pr_pink_bridge_error_buffer[NIMCP_ERROR_BUFFER_SIZE] = {0};

/**
 * @brief Set error message (thread-local)
 */
static void pr_pink_bridge_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(pr_pink_bridge_error_buffer, sizeof(pr_pink_bridge_error_buffer),
              fmt, args);
    va_end(args);
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-target generator state
 *
 * WHAT: State for a single modulation target's pink noise
 * WHY:  Each target has independent generator with own parameters
 */
typedef struct {
    pink_noise_generator_t generator;     /**< Base pink noise generator */
    pr_pink_modulation_params_t params;   /**< Modulation parameters */
    float current_noise;                  /**< Last generated noise value */
    uint64_t samples_generated;           /**< Sample count */
    bool initialized;                     /**< Initialization flag */
} pr_target_state_t;

/**
 * @brief Pink noise bridge internal structure
 *
 * WHAT: Complete state for pink noise bridge
 * WHY:  Contains all generators, timing, history, and statistics
 */
struct pr_pink_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Magic number for validation */
    uint32_t magic;

    /* Per-target generators */
    pr_target_state_t targets[PR_PINK_NUM_TARGETS];

    /* Quaternion generator with correlation */
    pr_quat_pink_generators_t quat_generators;

    /* Fractal timing for consolidation */
    pr_fractal_timer_t consolidation_timer;

    /* Fractal timing for promotion */
    pr_fractal_timer_t promotion_timer;

    /* Theta-gamma coupling */
    pr_pink_theta_coupling_t theta_coupling;

    /* Configuration snapshot */
    pr_pink_bridge_config_t config;

    /* History buffer */
    pr_pink_history_t* history;

    /* Statistics (atomic where possible) */
    _Atomic uint64_t total_noise_samples;
    _Atomic uint64_t total_modulations;
    _Atomic uint64_t consolidation_events;
    _Atomic uint64_t mutex_contentions;

    /* Measured spectral quality */
    float measured_exponents[PR_PINK_NUM_TARGETS];
    float spectral_fit_r2[PR_PINK_NUM_TARGETS];

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;

    /* Global amplitude multiplier */
    float global_amplitude;

    /* Initialization timestamp */
    uint64_t created_time_ms;

    /* Instance health agent (B25 Upgrade) */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_pink_bridge, struct pr_pink_bridge_struct)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)(ts.tv_sec * 1000) + (uint64_t)(ts.tv_nsec / 1000000);
}

/**
 * @brief Check if bridge is valid
 */
static inline bool is_valid_bridge(pr_pink_bridge_t bridge) {
    return bridge != NULL && bridge->magic == PR_PINK_BRIDGE_MAGIC;
}

/**
 * @brief Check if target is valid (not ALL)
 */
static inline bool is_valid_target(pr_pink_target_t target) {
    return target >= PR_PINK_TARGET_CONSOLIDATION && target < PR_PINK_TARGET_ALL;
}

/**
 * @brief Lock bridge mutex
 */
static inline bool bridge_lock(pr_pink_bridge_t bridge) {
    if (!bridge->mutex_initialized) return true;
    int result = nimcp_mutex_trylock(&bridge->mutex);
    if (result == 0) return true;
    /* Contention detected */
    atomic_fetch_add(&bridge->mutex_contentions, 1);
    return nimcp_mutex_lock(&bridge->mutex) == 0;
}

/**
 * @brief Unlock bridge mutex
 */
static inline void bridge_unlock(pr_pink_bridge_t bridge) {
    if (bridge->mutex_initialized) {
        nimcp_mutex_unlock(&bridge->mutex);
    }
}

/**
 * @brief Get index for target enum
 */
static inline int target_to_index(pr_pink_target_t target) {
    if (target >= PR_PINK_TARGET_ALL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "target_to_index: capacity exceeded");
        return -1;
    }
    return (int)target;
}

/**
 * @brief Compute Cholesky decomposition of 4x4 correlation matrix
 *
 * WHAT: Decomposes correlation matrix C into L * L^T
 * WHY:  Needed to generate correlated noise from independent streams
 * HOW:  Standard Cholesky-Banachiewicz algorithm
 *
 * @param correlation Input correlation matrix (must be positive-definite)
 * @param cholesky_L Output lower triangular matrix
 * @return true if successful, false if matrix is not positive-definite
 */
static bool compute_cholesky_4x4(
    const float correlation[4][4],
    float cholesky_L[4][4])
{
    /* Initialize output to zero */
    memset(cholesky_L, 0, 16 * sizeof(float));

    /* Cholesky-Banachiewicz algorithm */
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)4);
        }

        for (int j = 0; j <= i; j++) {
            float sum = 0.0f;

            if (j == i) {
                /* Diagonal element */
                for (int k = 0; k < j; k++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((k & 0xFF) == 0 && j > 256) {
                        pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                         (float)(k + 1) / (float)j);
                    }

                    sum += cholesky_L[j][k] * cholesky_L[j][k];
                }
                float diag = correlation[j][j] - sum;
                if (diag <= 0.0f) {
                    /* Matrix not positive-definite */
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_cholesky_4x4: validation failed");
                    return false;
                }
                cholesky_L[j][j] = sqrtf(diag);
            } else {
                /* Off-diagonal element */
                for (int k = 0; k < j; k++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((k & 0xFF) == 0 && j > 256) {
                        pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                         (float)(k + 1) / (float)j);
                    }

                    sum += cholesky_L[i][k] * cholesky_L[j][k];
                }
                if (fabsf(cholesky_L[j][j]) < PR_PINK_EPSILON) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_cholesky_4x4: validation failed");
                    return false;
                }
                cholesky_L[i][j] = (correlation[i][j] - sum) / cholesky_L[j][j];
            }
        }
    }

    return true;
}

/**
 * @brief Build 4x4 correlation matrix from 6 parameters
 *
 * The 6 parameters are: w-x, w-y, w-z, x-y, x-z, y-z
 */
static void build_correlation_matrix(
    const float params[6],
    float matrix[4][4])
{
    /* Diagonal = 1 */
    matrix[0][0] = 1.0f;
    matrix[1][1] = 1.0f;
    matrix[2][2] = 1.0f;
    matrix[3][3] = 1.0f;

    /* Off-diagonal (symmetric) */
    matrix[0][1] = matrix[1][0] = params[0];  /* w-x */
    matrix[0][2] = matrix[2][0] = params[1];  /* w-y */
    matrix[0][3] = matrix[3][0] = params[2];  /* w-z */
    matrix[1][2] = matrix[2][1] = params[3];  /* x-y */
    matrix[1][3] = matrix[3][1] = params[4];  /* x-z */
    matrix[2][3] = matrix[3][2] = params[5];  /* y-z */
}

/**
 * @brief Generate next 4D correlated noise sample
 */
static bool generate_correlated_quat_noise(
    pr_pink_bridge_t bridge,
    float noise_out[4])
{
    pr_quat_pink_generators_t* qg = &bridge->quat_generators;

    if (qg->quat_state == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_correlated_quat_noise: validation failed");
        return false;
    }

    /* Use the quaternionic pink noise generator */
    pr_quat_sample_t sample;
    if (!pr_quat_pink_next(qg->quat_state, &sample)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_correlated_quat_noise: pr_quat_pink_next is NULL");
        return false;
    }

    noise_out[0] = sample.w;
    noise_out[1] = sample.x;
    noise_out[2] = sample.y;
    noise_out[3] = sample.z;

    /* Store for later retrieval */
    memcpy(qg->current_noise, noise_out, 4 * sizeof(float));

    return true;
}

/**
 * @brief Initialize target generator
 */
static bool init_target_generator(
    pr_target_state_t* state,
    const pr_pink_modulation_params_t* params,
    float sample_rate,
    uint32_t seed)
{
    pink_noise_config_t noise_cfg = pink_noise_default_config();
    noise_cfg.alpha = params->spectral_exponent;
    noise_cfg.amplitude = params->amplitude;
    noise_cfg.sample_rate = sample_rate;
    noise_cfg.method = PINK_NOISE_VOSS;
    noise_cfg.seed = seed;

    state->generator = pink_noise_create(&noise_cfg);
    if (state->generator == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_target_generator: validation failed");
        return false;
    }

    state->params = *params;
    state->current_noise = 0.0f;
    state->samples_generated = 0;
    state->initialized = true;

    return true;
}

/**
 * @brief Destroy target generator
 */
static void destroy_target_generator(pr_target_state_t* state) {
    if (state->generator != NULL) {
        pink_noise_destroy(state->generator);
        state->generator = NULL;
    }
    state->initialized = false;
}

/**
 * @brief Generate next noise sample for target
 */
static float generate_target_noise(pr_target_state_t* state) {
    if (!state->initialized || state->generator == NULL) {
        return 0.0f;
    }

    float sample = 0.0f;
    if (!pink_noise_generate_sample(state->generator, &sample)) {
        return 0.0f;
    }

    state->current_noise = sample;
    state->samples_generated++;
    return sample;
}

/**
 * @brief Record sample in history buffer
 */
static void record_history(
    pr_pink_history_t* history,
    float interval,
    float resonance,
    float decay)
{
    if (history == NULL || history->max_samples == 0) {
        return;
    }

    size_t idx = history->write_index;

    if (history->intervals != NULL) {
        history->intervals[idx] = interval;
    }
    if (history->resonance_samples != NULL) {
        history->resonance_samples[idx] = resonance;
    }
    if (history->decay_samples != NULL) {
        history->decay_samples[idx] = decay;
    }

    history->write_index = (idx + 1) % history->max_samples;
    if (history->sample_count < history->max_samples) {
        history->sample_count++;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_pink_bridge_config_t pr_pink_bridge_default_config(void) {
    pr_pink_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* Consolidation: moderate amplitude for timing variability */
    config.consolidation.amplitude = 0.3f;
    config.consolidation.correlation_time = 1.0f;
    config.consolidation.spectral_exponent = 1.0f;
    config.consolidation.enabled = true;

    /* Resonance: small amplitude to preserve ranking */
    config.resonance.amplitude = PR_PINK_BRIDGE_DEFAULT_RESONANCE_AMPLITUDE;
    config.resonance.correlation_time = 0.5f;
    config.resonance.spectral_exponent = 1.0f;
    config.resonance.enabled = true;

    /* Decay: moderate amplitude for retention variability */
    config.decay.amplitude = PR_PINK_BRIDGE_DEFAULT_DECAY_AMPLITUDE;
    config.decay.correlation_time = 2.0f;
    config.decay.spectral_exponent = 1.0f;
    config.decay.enabled = true;

    /* Quaternion: small drift to prevent overfitting */
    config.quaternion.amplitude = PR_PINK_BRIDGE_DEFAULT_QUAT_AMPLITUDE;
    config.quaternion.correlation_time = 1.0f;
    config.quaternion.spectral_exponent = 1.0f;
    config.quaternion.enabled = true;

    /* Entanglement: moderate for association exploration */
    config.entanglement.amplitude = PR_PINK_BRIDGE_DEFAULT_ENTANGLE_AMPLITUDE;
    config.entanglement.correlation_time = 1.0f;
    config.entanglement.spectral_exponent = 1.0f;
    config.entanglement.enabled = true;

    /* Promotion: moderate timing variability */
    config.promotion.amplitude = PR_PINK_BRIDGE_DEFAULT_PROMOTION_AMPLITUDE;
    config.promotion.correlation_time = 1.0f;
    config.promotion.spectral_exponent = 1.0f;
    config.promotion.enabled = true;

    /* Retrieval: moderate latency variability */
    config.retrieval.amplitude = PR_PINK_BRIDGE_DEFAULT_RETRIEVAL_AMPLITUDE;
    config.retrieval.correlation_time = 0.5f;
    config.retrieval.spectral_exponent = 1.0f;
    config.retrieval.enabled = true;

    config.global_amplitude = 1.0f;

    /* Default correlation matrix (from nimcp_pr_pink_noise.h) */
    config.quaternion_correlation[0] = PR_CORR_WX;  /* w-x = -0.3 */
    config.quaternion_correlation[1] = PR_CORR_WY;  /* w-y = +0.5 */
    config.quaternion_correlation[2] = PR_CORR_WZ;  /* w-z = +0.7 */
    config.quaternion_correlation[3] = PR_CORR_XY;  /* x-y = +0.4 */
    config.quaternion_correlation[4] = PR_CORR_XZ;  /* x-z = +0.2 */
    config.quaternion_correlation[5] = PR_CORR_YZ;  /* y-z = +0.6 */

    config.sample_rate_hz = PR_PINK_BRIDGE_DEFAULT_SAMPLE_RATE_HZ;
    config.seed = 0;  /* Time-based */
    config.history_size = PR_PINK_BRIDGE_DEFAULT_HISTORY_SIZE;

    return config;
}

NIMCP_EXPORT pr_pink_modulation_params_t pr_pink_bridge_default_params(
    pr_pink_target_t target)
{
    pr_pink_bridge_config_t cfg = pr_pink_bridge_default_config();

    switch (target) {
        case PR_PINK_TARGET_CONSOLIDATION:
            return cfg.consolidation;
        case PR_PINK_TARGET_RESONANCE:
            return cfg.resonance;
        case PR_PINK_TARGET_DECAY:
            return cfg.decay;
        case PR_PINK_TARGET_QUATERNION:
            return cfg.quaternion;
        case PR_PINK_TARGET_ENTANGLEMENT:
            return cfg.entanglement;
        case PR_PINK_TARGET_PROMOTION:
            return cfg.promotion;
        case PR_PINK_TARGET_RETRIEVAL:
            return cfg.retrieval;
        default: {
            pr_pink_modulation_params_t params = {0};
            params.amplitude = 0.05f;
            params.correlation_time = 1.0f;
            params.spectral_exponent = 1.0f;
            params.enabled = false;
            return params;
        }
    }
}

NIMCP_EXPORT bool pr_pink_bridge_validate_config(
    const pr_pink_bridge_config_t* config)
{
    if (config == NULL) {
        return false;
    }

    /* Validate per-target parameters */
    const pr_pink_modulation_params_t* params[] = {
        &config->consolidation,
        &config->resonance,
        &config->decay,
        &config->quaternion,
        &config->entanglement,
        &config->promotion,
        &config->retrieval
    };

    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        if (params[i]->amplitude < PR_PINK_MIN_AMPLITUDE ||
            params[i]->amplitude > PR_PINK_MAX_AMPLITUDE) {
            return false;
        }
        if (params[i]->correlation_time < PR_PINK_MIN_CORRELATION_TIME) {
            return false;
        }
        if (params[i]->spectral_exponent < PR_PINK_MIN_SPECTRAL_EXPONENT ||
            params[i]->spectral_exponent > PR_PINK_MAX_SPECTRAL_EXPONENT) {
            return false;
        }
    }

    /* Validate global amplitude */
    if (config->global_amplitude < PR_PINK_MIN_AMPLITUDE ||
        config->global_amplitude > PR_PINK_MAX_AMPLITUDE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_validate_config: operation failed");
        return false;
    }

    /* Validate sample rate */
    if (config->sample_rate_hz <= 0.0f) {
        return false;
    }

    /* Validate correlation matrix (build and test Cholesky) */
    float correlation[4][4];
    float cholesky[4][4];
    build_correlation_matrix(config->quaternion_correlation, correlation);
    if (!compute_cholesky_4x4(correlation, cholesky)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_validate_config: compute_cholesky_4x4 is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_pink_bridge_t pr_pink_bridge_create(
    const pr_pink_bridge_config_t* config)
{
    pr_pink_bridge_config_t cfg;
    if (config != NULL) {
        if (!pr_pink_bridge_validate_config(config)) {
            pr_pink_bridge_set_error("Invalid configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_bridge_create: pr_pink_bridge_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_pink_bridge_default_config();
    }

    /* Allocate bridge structure */
    pr_pink_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_pink_bridge_struct));
    if (bridge == NULL) {
        pr_pink_bridge_set_error("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_pink_bridge_create: validation failed");
        return NULL;
    }

    bridge->magic = PR_PINK_BRIDGE_MAGIC;
    bridge->config = cfg;
    bridge->global_amplitude = cfg.global_amplitude;
    bridge->created_time_ms = get_current_time_ms();

    /* Determine seed */
    uint32_t base_seed = cfg.seed;
    if (base_seed == 0) {
        base_seed = (uint32_t)time(NULL);
    }

    /* Initialize mutex - use NIMCP mutex abstraction (P0-6 fix) */
    mutex_attr_t attr = {.type = MUTEX_TYPE_RECURSIVE};
    if (nimcp_mutex_init(&bridge->mutex, &attr) != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        pr_pink_bridge_set_error("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_pink_bridge_create: validation failed");
        return NULL;
    }
    bridge->mutex_initialized = true;

    /* Initialize per-target generators */
    const pr_pink_modulation_params_t* params[] = {
        &cfg.consolidation,
        &cfg.resonance,
        &cfg.decay,
        &cfg.quaternion,
        &cfg.entanglement,
        &cfg.promotion,
        &cfg.retrieval
    };

    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        if (!init_target_generator(&bridge->targets[i], params[i],
                                   cfg.sample_rate_hz, base_seed + i)) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                     (float)(j + 1) / (float)i);
                }

                destroy_target_generator(&bridge->targets[j]);
            }
            nimcp_mutex_destroy(&bridge->mutex);
            nimcp_free(bridge);
            bridge = NULL;
            pr_pink_bridge_set_error("Failed to initialize target %d generator", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_bridge_create: operation failed");
            return NULL;
        }
    }

    /* Initialize quaternion generator with correlation */
    pr_quat_pink_params_t quat_params = pr_quat_pink_default_params();
    quat_params.alpha = cfg.quaternion.spectral_exponent;
    quat_params.amplitude = cfg.quaternion.amplitude;
    quat_params.sample_rate_hz = cfg.sample_rate_hz;
    quat_params.seed = base_seed + 100;

    float correlation[4][4];
    build_correlation_matrix(cfg.quaternion_correlation, correlation);

    bridge->quat_generators.quat_state = pr_quat_pink_create(&quat_params, correlation);
    if (bridge->quat_generators.quat_state == NULL) {
        for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
                pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                 (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
            }

            destroy_target_generator(&bridge->targets[i]);
        }
        nimcp_mutex_destroy(&bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        pr_pink_bridge_set_error("Failed to create quaternion generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_pink_bridge_create: operation failed");
        return NULL;
    }
    memcpy(bridge->quat_generators.correlation_matrix, correlation,
           sizeof(correlation));

    /* Initialize consolidation fractal timer */
    bridge->consolidation_timer.timing = pr_fractal_timing_create(
        PR_PINK_BRIDGE_DEFAULT_CONSOLIDATION_INTERVAL_MS);
    if (bridge->consolidation_timer.timing == NULL) {
        pr_quat_pink_destroy(bridge->quat_generators.quat_state);
        for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
                pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                 (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
            }

            destroy_target_generator(&bridge->targets[i]);
        }
        nimcp_mutex_destroy(&bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        pr_pink_bridge_set_error("Failed to create consolidation timer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_pink_bridge_create: operation failed");
        return NULL;
    }
    bridge->consolidation_timer.base_interval_ms =
        PR_PINK_BRIDGE_DEFAULT_CONSOLIDATION_INTERVAL_MS;
    bridge->consolidation_timer.hurst_exponent =
        PR_PINK_BRIDGE_DEFAULT_HURST_EXPONENT;
    bridge->consolidation_timer.last_event_time_ms = bridge->created_time_ms;
    bridge->consolidation_timer.next_event_time_ms =
        bridge->created_time_ms + (uint64_t)bridge->consolidation_timer.base_interval_ms;

    /* Initialize promotion timer (similar settings) */
    bridge->promotion_timer.timing = pr_fractal_timing_create(1000.0f); /* 1 second base */
    if (bridge->promotion_timer.timing == NULL) {
        pr_fractal_timing_destroy(bridge->consolidation_timer.timing);
        pr_quat_pink_destroy(bridge->quat_generators.quat_state);
        for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
                pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                 (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
            }

            destroy_target_generator(&bridge->targets[i]);
        }
        nimcp_mutex_destroy(&bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        pr_pink_bridge_set_error("Failed to create promotion timer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_pink_bridge_create: operation failed");
        return NULL;
    }
    bridge->promotion_timer.base_interval_ms = 1000.0f;
    bridge->promotion_timer.hurst_exponent = PR_PINK_BRIDGE_DEFAULT_HURST_EXPONENT;
    bridge->promotion_timer.last_event_time_ms = bridge->created_time_ms;

    /* Initialize theta coupling (disabled by default) */
    bridge->theta_coupling.theta_phase = 0.0f;
    bridge->theta_coupling.theta_frequency_hz = 6.0f;
    bridge->theta_coupling.coupling_strength = 0.0f;
    bridge->theta_coupling.enabled = false;

    /* Create history buffer */
    bridge->history = pr_pink_history_create(cfg.history_size);
    /* History creation failure is non-fatal */

    /* Initialize atomic counters */
    atomic_init(&bridge->total_noise_samples, 0);
    atomic_init(&bridge->total_modulations, 0);
    atomic_init(&bridge->consolidation_events, 0);
    atomic_init(&bridge->mutex_contentions, 0);

    return bridge;
}

NIMCP_EXPORT void pr_pink_bridge_destroy(pr_pink_bridge_t bridge) {
    if (bridge == NULL) {
        return;
    }

    if (bridge->magic != PR_PINK_BRIDGE_MAGIC) {
        return;  /* Invalid bridge */
    }

    /* Destroy history */
    if (bridge->history != NULL) {
        pr_pink_history_destroy(bridge->history);
    }

    /* Destroy timers */
    if (bridge->promotion_timer.timing != NULL) {
        pr_fractal_timing_destroy(bridge->promotion_timer.timing);
    }
    if (bridge->consolidation_timer.timing != NULL) {
        pr_fractal_timing_destroy(bridge->consolidation_timer.timing);
    }

    /* Destroy quaternion generator */
    if (bridge->quat_generators.quat_state != NULL) {
        pr_quat_pink_destroy(bridge->quat_generators.quat_state);
    }

    /* Destroy per-target generators */
    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        destroy_target_generator(&bridge->targets[i]);
    }

    /* Destroy mutex */
    if (bridge->mutex_initialized) {
        nimcp_mutex_destroy(&bridge->mutex);
    }

    /* Clear magic and free */
    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT bool pr_pink_bridge_reset(pr_pink_bridge_t bridge, uint32_t new_seed) {
    if (!is_valid_bridge(bridge)) {
        pr_pink_bridge_set_error("Invalid bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_reset: is_valid_bridge is NULL");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_bridge_reset: bridge_lock is NULL");
        return false;
    }

    uint32_t seed = new_seed;
    if (seed == 0) {
        seed = (uint32_t)time(NULL);
    }

    /* Reset all target generators */
    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        if (bridge->targets[i].generator != NULL) {
            pink_noise_reset(bridge->targets[i].generator, seed + i);
            bridge->targets[i].current_noise = 0.0f;
            bridge->targets[i].samples_generated = 0;
        }
    }

    /* Reset quaternion generator */
    if (bridge->quat_generators.quat_state != NULL) {
        pr_quat_pink_reset(bridge->quat_generators.quat_state, seed + 100);
        memset(bridge->quat_generators.current_noise, 0, sizeof(float) * 4);
    }

    /* Reset fractal timers */
    uint64_t now = get_current_time_ms();
    if (bridge->consolidation_timer.timing != NULL) {
        pr_fractal_timing_reset(bridge->consolidation_timer.timing, seed + 200);
        bridge->consolidation_timer.last_event_time_ms = now;
        bridge->consolidation_timer.event_count = 0;
    }
    if (bridge->promotion_timer.timing != NULL) {
        pr_fractal_timing_reset(bridge->promotion_timer.timing, seed + 300);
        bridge->promotion_timer.last_event_time_ms = now;
        bridge->promotion_timer.event_count = 0;
    }

    /* Reset theta phase */
    bridge->theta_coupling.theta_phase = 0.0f;

    /* Clear history */
    if (bridge->history != NULL) {
        bridge->history->sample_count = 0;
        bridge->history->write_index = 0;
    }

    /* Reset statistics */
    atomic_store(&bridge->total_noise_samples, 0);
    atomic_store(&bridge->total_modulations, 0);
    atomic_store(&bridge->consolidation_events, 0);
    atomic_store(&bridge->mutex_contentions, 0);

    memset(bridge->measured_exponents, 0, sizeof(bridge->measured_exponents));
    memset(bridge->spectral_fit_r2, 0, sizeof(bridge->spectral_fit_r2));

    bridge_unlock(bridge);
    return true;
}

//=============================================================================
// Consolidation Timing Functions
//=============================================================================

NIMCP_EXPORT uint64_t pr_pink_next_consolidation_time(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return 0;
    }

    return bridge->consolidation_timer.next_event_time_ms;
}

NIMCP_EXPORT bool pr_pink_should_consolidate_now(
    pr_pink_bridge_t bridge,
    uint64_t current_time_ms)
{
    if (!is_valid_bridge(bridge)) {
        return false;
    }

    return current_time_ms >= bridge->consolidation_timer.next_event_time_ms;
}

NIMCP_EXPORT uint64_t pr_pink_consolidation_complete(
    pr_pink_bridge_t bridge,
    uint64_t current_time_ms)
{
    if (!is_valid_bridge(bridge)) {
        return 0;
    }

    if (!bridge_lock(bridge)) {
        return 0;
    }

    /* Record event */
    bridge->consolidation_timer.last_event_time_ms = current_time_ms;
    bridge->consolidation_timer.event_count++;
    atomic_fetch_add(&bridge->consolidation_events, 1);

    /* Generate next event time using fractal timing */
    float interval = 0.0f;
    if (bridge->consolidation_timer.timing != NULL &&
        bridge->targets[PR_PINK_TARGET_CONSOLIDATION].params.enabled) {

        float next_time = pr_fractal_next_event_time(
            bridge->consolidation_timer.timing, (float)current_time_ms);
        interval = next_time - (float)current_time_ms;

        /* Clamp interval */
        interval = nimcp_myelin_clamp(interval,
                               PR_PINK_BRIDGE_MIN_CONSOLIDATION_INTERVAL_MS,
                               PR_PINK_BRIDGE_MAX_CONSOLIDATION_INTERVAL_MS);
    } else {
        /* Use base interval if timing disabled */
        interval = bridge->consolidation_timer.base_interval_ms;
    }

    bridge->consolidation_timer.current_interval_ms = interval;
    bridge->consolidation_timer.next_event_time_ms =
        current_time_ms + (uint64_t)interval;

    /* Record in history */
    if (bridge->history != NULL) {
        record_history(bridge->history, interval, 0.0f, 0.0f);
    }

    bridge_unlock(bridge);
    return bridge->consolidation_timer.next_event_time_ms;
}

NIMCP_EXPORT bool pr_pink_set_base_consolidation_interval(
    pr_pink_bridge_t bridge,
    float interval_ms)
{
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_base_consolidation_interval: is_valid_bridge is NULL");
        return false;
    }

    if (interval_ms < PR_PINK_BRIDGE_MIN_CONSOLIDATION_INTERVAL_MS ||
        interval_ms > PR_PINK_BRIDGE_MAX_CONSOLIDATION_INTERVAL_MS) {
        pr_pink_bridge_set_error("Invalid interval: %.2f", interval_ms);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_base_consolidation_interval: is_valid_bridge is NULL");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_set_base_consolidation_interval: bridge_lock is NULL");
        return false;
    }

    bridge->consolidation_timer.base_interval_ms = interval_ms;

    bridge_unlock(bridge);
    return true;
}

NIMCP_EXPORT float pr_pink_get_consolidation_interval(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return 0.0f;
    }

    return bridge->consolidation_timer.current_interval_ms;
}

//=============================================================================
// Resonance Modulation Functions
//=============================================================================

NIMCP_EXPORT float pr_pink_bridge_modulate_resonance(
    pr_pink_bridge_t bridge,
    float base_score)
{
    if (!is_valid_bridge(bridge)) {
        return base_score;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_RESONANCE];
    if (!target->params.enabled) {
        return base_score;
    }

    /* Generate noise sample */
    float noise = generate_target_noise(target);
    atomic_fetch_add(&bridge->total_noise_samples, 1);

    /* Apply modulation with global amplitude */
    float amplitude = target->params.amplitude * bridge->global_amplitude;

    /* Apply theta coupling if enabled */
    if (bridge->theta_coupling.enabled) {
        /* Reduce noise at theta trough (retrieval phase) */
        float theta_mod = 0.5f * (1.0f + cosf(bridge->theta_coupling.theta_phase));
        amplitude *= (1.0f - bridge->theta_coupling.coupling_strength * (1.0f - theta_mod));
    }

    float modulated = base_score * (1.0f + amplitude * noise);

    /* Clamp to valid range */
    modulated = nimcp_myelin_clamp(modulated, 0.0f, 1.0f);

    atomic_fetch_add(&bridge->total_modulations, 1);
    return modulated;
}

NIMCP_EXPORT bool pr_pink_bridge_modulate_resonance_batch(
    pr_pink_bridge_t bridge,
    float* scores,
    size_t count)
{
    if (!is_valid_bridge(bridge) || scores == NULL || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_modulate_resonance_batch: is_valid_bridge is NULL");
        return false;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_RESONANCE];
    if (!target->params.enabled || target->generator == NULL) {
        return true;  /* Nothing to do, but not an error */
    }

    float amplitude = target->params.amplitude * bridge->global_amplitude;

    /* Apply theta coupling if enabled */
    if (bridge->theta_coupling.enabled) {
        float theta_mod = 0.5f * (1.0f + cosf(bridge->theta_coupling.theta_phase));
        amplitude *= (1.0f - bridge->theta_coupling.coupling_strength * (1.0f - theta_mod));
    }

    /* Generate batch of noise samples */
    float* noise_batch = nimcp_calloc(count, sizeof(float));
    if (noise_batch == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_modulate_resonance_batch: validation failed");
        return false;
    }

    if (!pink_noise_generate(target->generator, noise_batch, (uint32_t)count)) {
        nimcp_free(noise_batch);
        noise_batch = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_modulate_resonance_batch: pink_noise_generate is NULL");
        return false;
    }

    /* Apply modulation */
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)count);
        }

        scores[i] = scores[i] * (1.0f + amplitude * noise_batch[i]);
        scores[i] = nimcp_myelin_clamp(scores[i], 0.0f, 1.0f);
    }

    target->samples_generated += count;
    atomic_fetch_add(&bridge->total_noise_samples, count);
    atomic_fetch_add(&bridge->total_modulations, count);

    nimcp_free(noise_batch);
    noise_batch = NULL;
    return true;
}

NIMCP_EXPORT float pr_pink_get_resonance_noise(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return 0.0f;
    }

    return bridge->targets[PR_PINK_TARGET_RESONANCE].current_noise;
}

//=============================================================================
// Decay Modulation Functions
//=============================================================================

NIMCP_EXPORT float pr_pink_modulate_decay(
    pr_pink_bridge_t bridge,
    float base_rate,
    pr_memory_tier_t tier)
{
    if (!is_valid_bridge(bridge)) {
        return base_rate;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_DECAY];
    if (!target->params.enabled) {
        return base_rate;
    }

    /* Z3 memories never decay */
    if (tier == PR_MEMORY_TIER_Z3) {
        return 0.0f;
    }

    /* Generate noise sample */
    float noise = generate_target_noise(target);
    atomic_fetch_add(&bridge->total_noise_samples, 1);

    /* Apply modulation with global amplitude */
    float amplitude = target->params.amplitude * bridge->global_amplitude;

    /* Scale amplitude by tier (lower tiers more variable) */
    float tier_scale = 1.0f - (0.2f * (float)tier);
    amplitude *= tier_scale;

    float modulated = base_rate * (1.0f + amplitude * noise);

    /* Ensure non-negative */
    modulated = fmaxf(modulated, 0.0f);

    atomic_fetch_add(&bridge->total_modulations, 1);
    return modulated;
}

NIMCP_EXPORT bool pr_pink_modulate_decay_batch(
    pr_pink_bridge_t bridge,
    pr_memory_node_t** nodes,
    size_t count)
{
    if (!is_valid_bridge(bridge) || nodes == NULL || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_decay_batch: is_valid_bridge is NULL");
        return false;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_DECAY];
    if (!target->params.enabled || target->generator == NULL) {
        return true;
    }

    float amplitude = target->params.amplitude * bridge->global_amplitude;

    /* Generate batch of noise samples */
    float* noise_batch = nimcp_calloc(count, sizeof(float));
    if (noise_batch == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_decay_batch: validation failed");
        return false;
    }

    if (!pink_noise_generate(target->generator, noise_batch, (uint32_t)count)) {
        nimcp_free(noise_batch);
        noise_batch = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_decay_batch: pink_noise_generate is NULL");
        return false;
    }

    /* Apply modulation to each node */
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)count);
        }

        if (nodes[i] == NULL) continue;

        pr_memory_tier_t tier = nodes[i]->tier;
        if (tier == PR_MEMORY_TIER_Z3) continue;  /* Skip permanent memories */

        float tier_scale = 1.0f - (0.2f * (float)tier);
        float mod_amplitude = amplitude * tier_scale;

        nodes[i]->decay_rate = nodes[i]->decay_rate *
                               (1.0f + mod_amplitude * noise_batch[i]);
        nodes[i]->decay_rate = fmaxf(nodes[i]->decay_rate, 0.0f);
    }

    target->samples_generated += count;
    atomic_fetch_add(&bridge->total_noise_samples, count);
    atomic_fetch_add(&bridge->total_modulations, count);

    nimcp_free(noise_batch);
    noise_batch = NULL;
    return true;
}

NIMCP_EXPORT float pr_pink_get_decay_factor(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return 1.0f;
    }

    float noise = bridge->targets[PR_PINK_TARGET_DECAY].current_noise;
    float amplitude = bridge->targets[PR_PINK_TARGET_DECAY].params.amplitude *
                      bridge->global_amplitude;

    return 1.0f + amplitude * noise;
}

//=============================================================================
// Quaternion Drift Functions
//=============================================================================

NIMCP_EXPORT bool pr_pink_drift_quaternion(
    pr_pink_bridge_t bridge,
    nimcp_quaternion_t* quat)
{
    if (!is_valid_bridge(bridge) || quat == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_drift_quaternion: is_valid_bridge is NULL");
        return false;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_QUATERNION];
    if (!target->params.enabled) {
        return true;  /* Disabled, no drift applied */
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_drift_quaternion: bridge_lock is NULL");
        return false;
    }

    /* Generate correlated 4D noise */
    float noise[4];
    if (!generate_correlated_quat_noise(bridge, noise)) {
        bridge_unlock(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_drift_quaternion: generate_correlated_quat_noise is NULL");
        return false;
    }

    /* Apply drift with amplitude */
    float amplitude = target->params.amplitude * bridge->global_amplitude;

    /* Apply theta coupling if enabled */
    if (bridge->theta_coupling.enabled) {
        float theta_mod = 0.5f * (1.0f + cosf(bridge->theta_coupling.theta_phase));
        amplitude *= (1.0f - bridge->theta_coupling.coupling_strength * (1.0f - theta_mod));
    }

    /* Apply perturbations respecting component ranges */
    quat->w = nimcp_myelin_clamp(quat->w + amplitude * noise[0], 0.0f, 1.0f);
    quat->x = nimcp_myelin_clamp(quat->x + amplitude * noise[1], -1.0f, 1.0f);
    quat->y = nimcp_myelin_clamp(quat->y + amplitude * noise[2], 0.0f, 1.0f);
    quat->z = nimcp_myelin_clamp(quat->z + amplitude * noise[3], 0.0f, 1.0f);

    atomic_fetch_add(&bridge->total_noise_samples, 4);
    atomic_fetch_add(&bridge->total_modulations, 1);

    bridge_unlock(bridge);
    return true;
}

NIMCP_EXPORT bool pr_pink_drift_quaternion_correlated(
    pr_pink_bridge_t bridge,
    nimcp_quaternion_t* quat,
    const float correlation[4][4])
{
    if (!is_valid_bridge(bridge) || quat == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_drift_quaternion_correlated: is_valid_bridge is NULL");
        return false;
    }

    /* Use provided correlation or default */
    if (correlation != NULL) {
        /* Temporarily update correlation matrix */
        if (!pr_pink_set_quat_correlation(bridge, correlation)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_drift_quaternion_correlated: pr_pink_set_quat_correlation is NULL");
            return false;
        }
    }

    return pr_pink_drift_quaternion(bridge, quat);
}

NIMCP_EXPORT bool pr_pink_get_quat_noise(
    pr_pink_bridge_t bridge,
    float noise_out[4])
{
    if (!is_valid_bridge(bridge) || noise_out == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_get_quat_noise: is_valid_bridge is NULL");
        return false;
    }

    memcpy(noise_out, bridge->quat_generators.current_noise, 4 * sizeof(float));
    return true;
}

NIMCP_EXPORT bool pr_pink_set_quat_correlation(
    pr_pink_bridge_t bridge,
    const float correlation[4][4])
{
    if (!is_valid_bridge(bridge) || correlation == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_quat_correlation: is_valid_bridge is NULL");
        return false;
    }

    /* Validate matrix by attempting Cholesky decomposition */
    float cholesky[4][4];
    if (!compute_cholesky_4x4(correlation, cholesky)) {
        pr_pink_bridge_set_error("Correlation matrix not positive-definite");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_quat_correlation: compute_cholesky_4x4 is NULL");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_set_quat_correlation: bridge_lock is NULL");
        return false;
    }

    /* Update stored correlation matrix */
    memcpy(bridge->quat_generators.correlation_matrix, correlation,
           sizeof(float) * 16);

    /* Update the quaternionic generator (need to recreate with new correlation) */
    /* For now, just update internal tracking - full implementation would
       recreate the generator with new correlation */

    bridge_unlock(bridge);
    return true;
}

//=============================================================================
// Entanglement Modulation Functions
//=============================================================================

NIMCP_EXPORT bool pr_pink_modulate_edge_weight(
    pr_pink_bridge_t bridge,
    entangle_edge_t* edge)
{
    if (!is_valid_bridge(bridge) || edge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_edge_weight: is_valid_bridge is NULL");
        return false;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_ENTANGLEMENT];
    if (!target->params.enabled) {
        return true;
    }

    /* Generate noise sample */
    float noise = generate_target_noise(target);
    atomic_fetch_add(&bridge->total_noise_samples, 1);

    /* Apply modulation */
    float amplitude = target->params.amplitude * bridge->global_amplitude;
    edge->weight = edge->weight * (1.0f + amplitude * noise);
    edge->weight = nimcp_myelin_clamp(edge->weight, 0.0f, 1.0f);

    atomic_fetch_add(&bridge->total_modulations, 1);
    return true;
}

NIMCP_EXPORT size_t pr_pink_modulate_graph(
    pr_pink_bridge_t bridge,
    entangle_graph_t graph)
{
    if (!is_valid_bridge(bridge) || graph == NULL) {
        return 0;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_ENTANGLEMENT];
    if (!target->params.enabled) {
        return 0;
    }

    /* Get graph statistics to know edge count */
    entangle_stats_t stats;
    if (!entangle_get_stats(graph, &stats)) {
        return 0;
    }

    /* Apply decay to all edges (which effectively modulates weights) */
    /* Note: This is a simplified approach. Full implementation would iterate
       all edges and apply individual modulation */
    float amplitude = target->params.amplitude * bridge->global_amplitude;
    float decay_factor = 1.0f + amplitude * generate_target_noise(target);

    size_t affected = entangle_decay_all(graph, decay_factor);

    atomic_fetch_add(&bridge->total_noise_samples, affected);
    atomic_fetch_add(&bridge->total_modulations, affected);

    return affected;
}

NIMCP_EXPORT float pr_pink_get_edge_noise(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return 0.0f;
    }

    return bridge->targets[PR_PINK_TARGET_ENTANGLEMENT].current_noise;
}

//=============================================================================
// Promotion Timing Functions
//=============================================================================

NIMCP_EXPORT uint64_t pr_pink_next_promotion_check(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return 0;
    }

    if (bridge->promotion_timer.timing == NULL) {
        return get_current_time_ms() + 1000;  /* Default 1 second */
    }

    return bridge->promotion_timer.next_event_time_ms;
}

NIMCP_EXPORT float pr_pink_modulate_eligibility(
    pr_pink_bridge_t bridge,
    float base_eligibility)
{
    if (!is_valid_bridge(bridge)) {
        return base_eligibility;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_PROMOTION];
    if (!target->params.enabled) {
        return base_eligibility;
    }

    float noise = generate_target_noise(target);
    atomic_fetch_add(&bridge->total_noise_samples, 1);

    float amplitude = target->params.amplitude * bridge->global_amplitude;
    float modulated = base_eligibility + amplitude * noise;

    modulated = nimcp_myelin_clamp(modulated, 0.0f, 1.0f);

    atomic_fetch_add(&bridge->total_modulations, 1);
    return modulated;
}

//=============================================================================
// Retrieval Modulation Functions
//=============================================================================

NIMCP_EXPORT float pr_pink_modulate_retrieval_latency(
    pr_pink_bridge_t bridge,
    float base_latency_ms)
{
    if (!is_valid_bridge(bridge)) {
        return base_latency_ms;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_RETRIEVAL];
    if (!target->params.enabled) {
        return base_latency_ms;
    }

    float noise = generate_target_noise(target);
    atomic_fetch_add(&bridge->total_noise_samples, 1);

    float amplitude = target->params.amplitude * bridge->global_amplitude;
    float modulated = base_latency_ms * (1.0f + amplitude * noise);

    /* Ensure non-negative */
    modulated = fmaxf(modulated, 0.0f);

    atomic_fetch_add(&bridge->total_modulations, 1);
    return modulated;
}

NIMCP_EXPORT bool pr_pink_modulate_retrieval_order(
    pr_pink_bridge_t bridge,
    float* scores,
    size_t count)
{
    if (!is_valid_bridge(bridge) || scores == NULL || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_retrieval_order: is_valid_bridge is NULL");
        return false;
    }

    pr_target_state_t* target = &bridge->targets[PR_PINK_TARGET_RETRIEVAL];
    if (!target->params.enabled || target->generator == NULL) {
        return true;
    }

    /* Use smaller amplitude for order perturbation */
    float amplitude = target->params.amplitude * bridge->global_amplitude * 0.3f;

    /* Generate batch of noise samples */
    float* noise_batch = nimcp_calloc(count, sizeof(float));
    if (noise_batch == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_retrieval_order: validation failed");
        return false;
    }

    if (!pink_noise_generate(target->generator, noise_batch, (uint32_t)count)) {
        nimcp_free(noise_batch);
        noise_batch = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_modulate_retrieval_order: pink_noise_generate is NULL");
        return false;
    }

    /* Apply small perturbations */
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)count);
        }

        scores[i] = scores[i] + amplitude * noise_batch[i];
        /* Don't clamp - allow small negative/> 1 for ranking purposes */
    }

    target->samples_generated += count;
    atomic_fetch_add(&bridge->total_noise_samples, count);
    atomic_fetch_add(&bridge->total_modulations, count);

    nimcp_free(noise_batch);
    noise_batch = NULL;
    return true;
}

//=============================================================================
// Fractal Analysis Functions
//=============================================================================

NIMCP_EXPORT int pr_pink_analyze_memory_dynamics(
    pr_pink_bridge_t bridge,
    const pr_pink_history_t* history,
    fractal_result_t* result)
{
    if (!is_valid_bridge(bridge) || result == NULL) {
        return PR_PINK_BRIDGE_ERROR_NULL;
    }

    const pr_pink_history_t* hist = (history != NULL) ? history : bridge->history;
    if (hist == NULL || hist->sample_count < FRACTAL_MIN_SAMPLES) {
        pr_pink_bridge_set_error("Insufficient history samples for analysis");
        return PR_PINK_BRIDGE_ERROR_PARAM;
    }

    /* Analyze interval history */
    if (hist->intervals != NULL && hist->sample_count >= FRACTAL_MIN_SAMPLES) {
        int err = fractal_dfa(hist->intervals, hist->sample_count, NULL, result);
        if (err != FRACTAL_OK) {
            return err;
        }
    }

    return PR_PINK_BRIDGE_OK;
}

NIMCP_EXPORT bool pr_pink_verify_fractal(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target)
{
    if (!is_valid_bridge(bridge) || !is_valid_target(target)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_verify_fractal: required parameter is NULL (is_valid_bridge, is_valid_target)");
        return false;
    }

    int idx = target_to_index(target);
    if (idx < 0 || !bridge->targets[idx].initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_verify_fractal: bridge->targets is NULL");
        return false;
    }

    /* Generate test samples */
    float samples[1024];
    pr_target_state_t* state = &bridge->targets[idx];

    if (!pink_noise_generate(state->generator, samples, 1024)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_verify_fractal: pink_noise_generate is NULL");
        return false;
    }

    /* Validate against expected spectral exponent */
    return pink_noise_validate(samples, 1024, bridge->config.sample_rate_hz,
                               state->params.spectral_exponent,
                               PR_PINK_BRIDGE_VALIDATION_TOLERANCE);
}

NIMCP_EXPORT float pr_pink_get_spectral_exponent(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target)
{
    if (!is_valid_bridge(bridge) || !is_valid_target(target)) {
        return 0.0f;
    }

    int idx = target_to_index(target);
    if (idx < 0) {
        return 0.0f;
    }

    /* Return measured if available, otherwise configured */
    if (bridge->measured_exponents[idx] > 0.0f) {
        return bridge->measured_exponents[idx];
    }

    return bridge->targets[idx].params.spectral_exponent;
}

//=============================================================================
// Generator Control Functions
//=============================================================================

NIMCP_EXPORT bool pr_pink_set_amplitude(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target,
    float amplitude)
{
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_amplitude: is_valid_bridge is NULL");
        return false;
    }

    if (amplitude < PR_PINK_MIN_AMPLITUDE || amplitude > PR_PINK_MAX_AMPLITUDE) {
        pr_pink_bridge_set_error("Invalid amplitude: %.3f", amplitude);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_amplitude: validation failed");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_set_amplitude: bridge_lock is NULL");
        return false;
    }

    if (target == PR_PINK_TARGET_ALL) {
        /* Set all targets */
        for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
                pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                 (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
            }

            bridge->targets[i].params.amplitude = amplitude;
        }
    } else if (is_valid_target(target)) {
        bridge->targets[target].params.amplitude = amplitude;
    } else {
        bridge_unlock(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_amplitude: operation failed");
        return false;
    }

    bridge_unlock(bridge);
    return true;
}

NIMCP_EXPORT float pr_pink_get_amplitude(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target)
{
    if (!is_valid_bridge(bridge) || !is_valid_target(target)) {
        return 0.0f;
    }

    return bridge->targets[target].params.amplitude;
}

NIMCP_EXPORT bool pr_pink_set_enabled(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target,
    bool enabled)
{
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_enabled: is_valid_bridge is NULL");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_set_enabled: bridge_lock is NULL");
        return false;
    }

    if (target == PR_PINK_TARGET_ALL) {
        for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
                pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                 (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
            }

            bridge->targets[i].params.enabled = enabled;
        }
    } else if (is_valid_target(target)) {
        bridge->targets[target].params.enabled = enabled;
    } else {
        bridge_unlock(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_enabled: operation failed");
        return false;
    }

    bridge_unlock(bridge);
    return true;
}

NIMCP_EXPORT bool pr_pink_is_enabled(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target)
{
    if (!is_valid_bridge(bridge) || !is_valid_target(target)) {
        return false;
    }

    return bridge->targets[target].params.enabled;
}

NIMCP_EXPORT bool pr_pink_step_all(pr_pink_bridge_t bridge, float dt_seconds) {
    if (!is_valid_bridge(bridge) || dt_seconds <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_step_all: is_valid_bridge is NULL");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_step_all: bridge_lock is NULL");
        return false;
    }

    /* Advance theta phase if coupled */
    if (bridge->theta_coupling.enabled) {
        bridge->theta_coupling.theta_phase +=
            2.0f * (float)M_PI * bridge->theta_coupling.theta_frequency_hz * dt_seconds;

        /* Keep in [0, 2*pi] */
        while (bridge->theta_coupling.theta_phase >= 2.0f * (float)M_PI) {
            bridge->theta_coupling.theta_phase -= 2.0f * (float)M_PI;
        }
    }

    /* Advance quaternion theta phase */
    if (bridge->quat_generators.quat_state != NULL) {
        pr_quat_pink_advance_theta(bridge->quat_generators.quat_state,
                                   dt_seconds * 1000.0f);
    }

    bridge_unlock(bridge);
    return true;
}

NIMCP_EXPORT bool pr_pink_set_global_amplitude(
    pr_pink_bridge_t bridge,
    float amplitude)
{
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_global_amplitude: is_valid_bridge is NULL");
        return false;
    }

    if (amplitude < PR_PINK_MIN_AMPLITUDE || amplitude > PR_PINK_MAX_AMPLITUDE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_set_global_amplitude: validation failed");
        return false;
    }

    bridge->global_amplitude = amplitude;
    return true;
}

//=============================================================================
// Integration Functions
//=============================================================================

NIMCP_EXPORT bool pr_pink_integrate_with_theta_gamma(
    pr_pink_bridge_t bridge,
    float theta_phase,
    float theta_freq,
    float coupling_strength)
{
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_integrate_with_theta_gamma: is_valid_bridge is NULL");
        return false;
    }

    if (theta_freq <= 0.0f || coupling_strength < 0.0f || coupling_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_integrate_with_theta_gamma: validation failed");
        return false;
    }

    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_integrate_with_theta_gamma: bridge_lock is NULL");
        return false;
    }

    bridge->theta_coupling.theta_phase = theta_phase;
    bridge->theta_coupling.theta_frequency_hz = theta_freq;
    bridge->theta_coupling.coupling_strength = coupling_strength;
    bridge->theta_coupling.enabled = (coupling_strength > 0.0f);

    /* Also update quaternion generator's theta coupling */
    if (bridge->quat_generators.quat_state != NULL) {
        pr_quat_pink_set_theta_coupling(bridge->quat_generators.quat_state,
                                        theta_phase, coupling_strength);
    }

    bridge_unlock(bridge);
    return true;
}

NIMCP_EXPORT float pr_pink_advance_theta_phase(
    pr_pink_bridge_t bridge,
    float dt_seconds)
{
    if (!is_valid_bridge(bridge) || dt_seconds <= 0.0f) {
        return 0.0f;
    }

    if (!bridge->theta_coupling.enabled) {
        return 0.0f;
    }

    bridge->theta_coupling.theta_phase +=
        2.0f * (float)M_PI * bridge->theta_coupling.theta_frequency_hz * dt_seconds;

    /* Keep in [0, 2*pi] */
    while (bridge->theta_coupling.theta_phase >= 2.0f * (float)M_PI) {
        bridge->theta_coupling.theta_phase -= 2.0f * (float)M_PI;
    }

    return bridge->theta_coupling.theta_phase;
}

NIMCP_EXPORT bool pr_pink_integrate_with_z_ladder(
    pr_pink_bridge_t bridge,
    z_ladder_t ladder)
{
    if (!is_valid_bridge(bridge) || ladder == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_integrate_with_z_ladder: is_valid_bridge is NULL");
        return false;
    }

    /* Adjust modulation parameters based on tier usage */
    /* This is a placeholder - full implementation would analyze ladder
       statistics and adjust parameters accordingly */

    z_ladder_stats_t stats;
    if (z_ladder_get_stats(ladder, &stats) != Z_LADDER_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_integrate_with_z_ladder: validation failed");
        return false;
    }

    /* Scale resonance noise based on working memory load */
    float z0_load = (stats.tier_capacities[0] > 0) ?
                    (float)stats.tier_counts[0] / (float)stats.tier_capacities[0] : 0.0f;

    /* Higher load = lower noise (focus on existing items) */
    if (!bridge_lock(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_pink_integrate_with_z_ladder: bridge_lock is NULL");
        return false;
    }

    float base_amplitude = bridge->config.resonance.amplitude;
    bridge->targets[PR_PINK_TARGET_RESONANCE].params.amplitude =
        base_amplitude * (1.0f - 0.3f * z0_load);

    bridge_unlock(bridge);
    return true;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT bool pr_pink_bridge_get_stats(
    pr_pink_bridge_t bridge,
    pr_pink_bridge_stats_t* stats)
{
    if (!is_valid_bridge(bridge) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_get_stats: is_valid_bridge is NULL");
        return false;
    }

    memset(stats, 0, sizeof(*stats));

    /* Consolidation timing stats */
    stats->consolidation_events = atomic_load(&bridge->consolidation_events);
    stats->mean_consolidation_interval_ms = bridge->consolidation_timer.base_interval_ms;

    if (bridge->history != NULL && bridge->history->sample_count > 0 &&
        bridge->history->intervals != NULL) {
        /* Compute mean and variability from history */
        float sum = 0.0f;
        float sum_sq = 0.0f;
        size_t count = bridge->history->sample_count;

        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                                 (float)(i + 1) / (float)count);
            }

            float val = bridge->history->intervals[i];
            sum += val;
            sum_sq += val * val;
        }

        float mean = sum / (float)count;
        float variance = (sum_sq / (float)count) - (mean * mean);
        float stddev = sqrtf(fmaxf(variance, 0.0f));

        stats->mean_consolidation_interval_ms = mean;
        stats->consolidation_variability = (mean > 0.0f) ? stddev / mean : 0.0f;
    }

    /* Modulation stats - compute from target states */
    stats->mean_resonance_modulation =
        bridge->targets[PR_PINK_TARGET_RESONANCE].params.amplitude *
        bridge->global_amplitude;
    stats->mean_decay_modulation =
        bridge->targets[PR_PINK_TARGET_DECAY].params.amplitude *
        bridge->global_amplitude;
    stats->mean_quaternion_drift =
        bridge->targets[PR_PINK_TARGET_QUATERNION].params.amplitude *
        bridge->global_amplitude;
    stats->mean_entanglement_modulation =
        bridge->targets[PR_PINK_TARGET_ENTANGLEMENT].params.amplitude *
        bridge->global_amplitude;

    /* Generator stats */
    stats->total_noise_samples = atomic_load(&bridge->total_noise_samples);
    stats->total_modulations = atomic_load(&bridge->total_modulations);

    /* Spectral quality - use first target's configured value */
    stats->measured_spectral_exponent =
        bridge->targets[PR_PINK_TARGET_CONSOLIDATION].params.spectral_exponent;
    stats->spectral_fit_r2 = 0.95f;  /* Placeholder */

    /* Resource usage estimate */
    stats->memory_bytes = sizeof(struct pr_pink_bridge_struct);
    if (bridge->history != NULL) {
        stats->memory_bytes += bridge->history->max_samples * 3 * sizeof(float);
    }

    stats->mutex_contentions = atomic_load(&bridge->mutex_contentions);

    return true;
}

NIMCP_EXPORT void pr_pink_bridge_reset_stats(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        return;
    }

    atomic_store(&bridge->total_noise_samples, 0);
    atomic_store(&bridge->total_modulations, 0);
    atomic_store(&bridge->consolidation_events, 0);
    atomic_store(&bridge->mutex_contentions, 0);

    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        bridge->targets[i].samples_generated = 0;
    }
}

NIMCP_EXPORT void pr_pink_bridge_print_diagnostics(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        printf("Pink Noise Bridge: INVALID\n");
        return;
    }

    pr_pink_bridge_stats_t stats;
    pr_pink_bridge_get_stats(bridge, &stats);

    printf("\n");
    printf("+===========================================================+\n");
    printf("|           PINK NOISE BRIDGE DIAGNOSTICS                   |\n");
    printf("+===========================================================+\n");
    printf("| Created: %lu ms ago                                       |\n",
           get_current_time_ms() - bridge->created_time_ms);
    printf("+-----------------------------------------------------------+\n");
    printf("| CONSOLIDATION TIMING                                      |\n");
    printf("|   Events: %lu                                             |\n",
           (unsigned long)stats.consolidation_events);
    printf("|   Mean interval: %.2f ms                                  |\n",
           stats.mean_consolidation_interval_ms);
    printf("|   Variability (CV): %.3f                                  |\n",
           stats.consolidation_variability);
    printf("+-----------------------------------------------------------+\n");
    printf("| MODULATION AMPLITUDES (effective)                         |\n");
    printf("|   Resonance:    %.4f                                      |\n",
           stats.mean_resonance_modulation);
    printf("|   Decay:        %.4f                                      |\n",
           stats.mean_decay_modulation);
    printf("|   Quaternion:   %.4f                                      |\n",
           stats.mean_quaternion_drift);
    printf("|   Entanglement: %.4f                                      |\n",
           stats.mean_entanglement_modulation);
    printf("+-----------------------------------------------------------+\n");
    printf("| GENERATOR STATISTICS                                      |\n");
    printf("|   Total noise samples: %lu                                |\n",
           (unsigned long)stats.total_noise_samples);
    printf("|   Total modulations:   %lu                                |\n",
           (unsigned long)stats.total_modulations);
    printf("+-----------------------------------------------------------+\n");
    printf("| SPECTRAL QUALITY                                          |\n");
    printf("|   Target exponent: %.2f (1.0 = pink)                      |\n",
           stats.measured_spectral_exponent);
    printf("+-----------------------------------------------------------+\n");
    printf("| TARGET STATUS                                             |\n");

    const char* target_names[] = {
        "Consolidation", "Resonance", "Decay", "Quaternion",
        "Entanglement", "Promotion", "Retrieval"
    };

    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        printf("|   %-12s: %s  amplitude=%.3f                     |\n",
               target_names[i],
               bridge->targets[i].params.enabled ? "ON " : "OFF",
               bridge->targets[i].params.amplitude);
    }

    printf("+-----------------------------------------------------------+\n");
    printf("| THETA COUPLING                                            |\n");
    printf("|   Enabled: %s                                             |\n",
           bridge->theta_coupling.enabled ? "Yes" : "No");
    if (bridge->theta_coupling.enabled) {
        printf("|   Phase: %.2f rad                                         |\n",
               bridge->theta_coupling.theta_phase);
        printf("|   Frequency: %.1f Hz                                       |\n",
               bridge->theta_coupling.theta_frequency_hz);
        printf("|   Coupling: %.2f                                           |\n",
               bridge->theta_coupling.coupling_strength);
    }
    printf("+-----------------------------------------------------------+\n");
    printf("| RESOURCES                                                 |\n");
    printf("|   Memory: ~%zu bytes                                      |\n",
           stats.memory_bytes);
    printf("|   Mutex contentions: %lu                                  |\n",
           (unsigned long)stats.mutex_contentions);
    printf("+===========================================================+\n\n");
}

//=============================================================================
// History Management Functions
//=============================================================================

NIMCP_EXPORT pr_pink_history_t* pr_pink_history_create(size_t max_samples) {
    if (max_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_pink_history_create: max_samples is zero");
        return NULL;
    }

    pr_pink_history_t* history = nimcp_calloc(1, sizeof(pr_pink_history_t));
    if (history == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate history");

        return NULL;
    }

    history->intervals = nimcp_calloc(max_samples, sizeof(float));
    history->resonance_samples = nimcp_calloc(max_samples, sizeof(float));
    if (!history->resonance_samples) return NULL;
    history->decay_samples = nimcp_calloc(max_samples, sizeof(float));
    if (!history->decay_samples) return NULL;

    if (history->intervals == NULL ||
        history->resonance_samples == NULL ||
        history->decay_samples == NULL) {

        nimcp_free(history->intervals);
        nimcp_free(history->resonance_samples);
        nimcp_free(history->decay_samples);
        nimcp_free(history);
        history = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_history_create: operation failed");
        return NULL;
    }

    history->max_samples = max_samples;
    history->sample_count = 0;
    history->write_index = 0;

    return history;
}

NIMCP_EXPORT void pr_pink_history_destroy(pr_pink_history_t* history) {
    if (history == NULL) {
        return;
    }

    nimcp_free(history->intervals);
    nimcp_free(history->resonance_samples);
    nimcp_free(history->decay_samples);
    nimcp_free(history);
    history = NULL;
}

NIMCP_EXPORT const pr_pink_history_t* pr_pink_bridge_get_history(
    pr_pink_bridge_t bridge)
{
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_bridge_get_history: is_valid_bridge is NULL");
        return NULL;
    }

    return bridge->history;
}

NIMCP_EXPORT void pr_pink_bridge_clear_history(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge) || bridge->history == NULL) {
        return;
    }

    bridge->history->sample_count = 0;
    bridge->history->write_index = 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_pink_target_name(pr_pink_target_t target) {
    switch (target) {
        case PR_PINK_TARGET_CONSOLIDATION: return "Consolidation";
        case PR_PINK_TARGET_RESONANCE:     return "Resonance";
        case PR_PINK_TARGET_DECAY:         return "Decay";
        case PR_PINK_TARGET_QUATERNION:    return "Quaternion";
        case PR_PINK_TARGET_ENTANGLEMENT:  return "Entanglement";
        case PR_PINK_TARGET_PROMOTION:     return "Promotion";
        case PR_PINK_TARGET_RETRIEVAL:     return "Retrieval";
        case PR_PINK_TARGET_ALL:           return "All";
        default:                           return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_pink_bridge_get_last_error(void) {
    if (pr_pink_bridge_error_buffer[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_pink_bridge_get_last_error: validation failed");
        return NULL;
    }
    return pr_pink_bridge_error_buffer;
}

NIMCP_EXPORT uint64_t pr_pink_bridge_current_time_ms(void) {
    return get_current_time_ms();
}

NIMCP_EXPORT bool pr_pink_bridge_validate(pr_pink_bridge_t bridge) {
    if (!is_valid_bridge(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_pink_bridge_validate: is_valid_bridge is NULL");
        return false;
    }

    /* Check all target generators are valid */
    for (int i = 0; i < PR_PINK_NUM_TARGETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_PINK_NUM_TARGETS > 256) {
            pr_pink_noise_bridge_heartbeat("pr_pink_nois_loop",
                             (float)(i + 1) / (float)PR_PINK_NUM_TARGETS);
        }

        if (bridge->targets[i].initialized &&
            bridge->targets[i].generator == NULL) {
            return false;
        }
    }

    /* Check quaternion generator */
    if (bridge->quat_generators.quat_state == NULL) {
        return false;
    }

    /* Check fractal timers */
    if (bridge->consolidation_timer.timing == NULL ||
        bridge->promotion_timer.timing == NULL) {
        return false;
    }

    return true;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_pink_noise_bridge_set_instance_health_agent(
    pr_pink_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_pink_bridge_training_begin(pr_pink_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_pink_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_pink_bridge_heartbeat_instance(bridge->health_agent, "pr_pink_bridge_training_begin", 0.0f);
    return 0;
}

int pr_pink_bridge_training_end(pr_pink_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_pink_bridge_training_end: NULL argument");
        return -1;
    }
    pr_pink_bridge_heartbeat_instance(bridge->health_agent, "pr_pink_bridge_training_end", 1.0f);
    return 0;
}

int pr_pink_bridge_training_step(pr_pink_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_pink_bridge_training_step: NULL argument");
        return -1;
    }
    pr_pink_bridge_heartbeat_instance(bridge->health_agent, "pr_pink_bridge_training_step", progress);
    return 0;
}
