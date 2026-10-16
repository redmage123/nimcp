//=============================================================================
// nimcp_kuramoto.c - Kuramoto Oscillator Bank Implementation
//=============================================================================
/**
 * @file nimcp_kuramoto.c
 * @brief Implementation of Kuramoto coupled oscillator system
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implements Kuramoto model for NIMCP module synchronization
 * WHY:  Phase coherence enables coordinated information processing
 * HOW:  RK4 integration of coupled phase oscillators with pink noise
 *
 * @author NIMCP Development Team
 */

#include "cognitive/memory/core/nimcp_kuramoto.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(kuramoto, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** Maximum sparse couplings per oscillator (average) */
#define SPARSE_COUPLING_FACTOR 8

/** Module ID mapping table initial size */
#define MODULE_MAP_INITIAL_SIZE 64

/** Error message buffer size */
#define ERROR_MSG_SIZE 256

//=============================================================================
// Thread-Local Error State
//=============================================================================

static _Thread_local char g_kuramoto_error[ERROR_MSG_SIZE] = {0};

/**
 * @brief Set error message
 */
static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_kuramoto_error, ERROR_MSG_SIZE, fmt, args);
    va_end(args);
}

/**
 * @brief Clear error message
 */
static void clear_error(void) {
    g_kuramoto_error[0] = '\0';
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Simple random number generator (XorShift32)
 */
static _Thread_local uint32_t g_kuramoto_rng_state = 0;

static uint32_t xorshift32(void) {
    uint32_t x = g_kuramoto_rng_state;
    if (x == 0) {
        x = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)&x ^ 0xDEADBEEF;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_kuramoto_rng_state = x;
    return x;
}

static void seed_rng(uint32_t seed) {
    g_kuramoto_rng_state = seed ? seed : ((uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)&seed ^ 0xDEADBEEF);
}

/**
 * @brief Generate random float in [0, 1)
 */
static float random_float(void) {
    return (float)(xorshift32() & 0x7FFFFFFF) / (float)0x80000000;
}

/**
 * @brief Generate random phase in [0, 2*pi)
 */
static float random_phase(void) {
    return random_float() * NIMCP_TWO_PI_F;
}

/**
 * @brief Wrap phase to [0, 2*pi)
 */
static float wrap_phase_internal(float phase) {
    while (phase < 0.0f) {
        phase += NIMCP_TWO_PI_F;
    }
    while (phase >= NIMCP_TWO_PI_F) {
        phase -= NIMCP_TWO_PI_F;
    }
    return phase;
}

/**
 * @brief Find oscillator index for module ID
 *
 * @return Index or -1 if not found
 */
static int32_t find_oscillator_index(const kuramoto_system_t* system,
                                      uint32_t module_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wrap_phase_internal: system is NULL");
        return -1;
    }

    /* Check mapping table first */
    if (system->module_to_index && module_id < system->module_map_size) {
        uint32_t idx = system->module_to_index[module_id];
        if (idx < system->num_oscillators &&
            system->oscillators[idx].module_id == module_id &&
            system->oscillators[idx].active) {
            return (int32_t)idx;
        }
    }

    /* Linear search fallback */
    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active &&
            system->oscillators[i].module_id == module_id) {
            return (int32_t)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wrap_phase_internal: operation failed");
    return -1;
}

/**
 * @brief Update module ID to index mapping
 */
static void update_module_mapping(kuramoto_system_t* system,
                                   uint32_t module_id,
                                   uint32_t index) {
    if (!system || !system->module_to_index) return;

    /* Grow mapping table if needed */
    if (module_id >= system->module_map_size) {
        uint32_t new_size = system->module_map_size * 2;
        while (new_size <= module_id) {
            new_size *= 2;
        }
        uint32_t* new_map = nimcp_realloc(system->module_to_index,
                                     new_size * sizeof(uint32_t));
        if (!new_map) {
            /* Realloc failed - cannot grow mapping table */
            return;
        }
        /* Initialize new entries to invalid */
        for (uint32_t i = system->module_map_size; i < new_size; i++) {
            new_map[i] = UINT32_MAX;
        }
        system->module_to_index = new_map;
        system->module_map_size = new_size;
    }

    if (module_id < system->module_map_size) {
        system->module_to_index[module_id] = index;
    }
}

/**
 * @brief Compute derivative for single oscillator
 *
 * f(theta_i) = omega_i + (K/N) * SUM_j[K_ij * sin(theta_j - theta_i)] + noise
 */
static float compute_derivative(const kuramoto_system_t* system,
                                 uint32_t osc_idx,
                                 const float* phases) {
    const kuramoto_oscillator_t* osc = &system->oscillators[osc_idx];

    if (!osc->active) {
        return 0.0f;
    }

    /* Natural frequency plus noise offset */
    float derivative = osc->natural_frequency + osc->frequency_offset;

    /* Count active oscillators for normalization */
    uint32_t active_count = 0;
    for (uint32_t j = 0; j < system->num_oscillators; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(j + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[j].active) {
            active_count++;
        }
    }

    if (active_count <= 1) {
        return derivative;
    }

    float coupling_sum = 0.0f;
    float theta_i = phases[osc_idx];

    if (system->use_sparse_coupling) {
        /* Sparse coupling */
        for (uint32_t e = 0; e < system->num_couplings; e++) {
            /* Phase 8: Loop progress heartbeat */
            if ((e & 0xFF) == 0 && system->num_couplings > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(e + 1) / (float)system->num_couplings);
            }

            const kuramoto_coupling_t* edge = &system->sparse_couplings[e];
            if (edge->to_idx == osc_idx &&
                edge->from_idx < system->num_oscillators &&
                system->oscillators[edge->from_idx].active) {
                float theta_j = phases[edge->from_idx];
                coupling_sum += edge->strength * sinf(theta_j - theta_i);
            }
        }
    } else {
        /* Dense coupling matrix */
        for (uint32_t j = 0; j < system->num_oscillators; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && system->num_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(j + 1) / (float)system->num_oscillators);
            }

            if (j != osc_idx && system->oscillators[j].active) {
                float K_ij = system->coupling_matrix[j * system->max_oscillators + osc_idx];
                float theta_j = phases[j];
                coupling_sum += K_ij * sinf(theta_j - theta_i);
            }
        }
    }

    /* Apply global coupling strength and normalize */
    derivative += (system->config.base_coupling_strength / (float)active_count) * coupling_sum;

    return derivative;
}

/**
 * @brief Compute all derivatives for RK4
 */
static void compute_all_derivatives(const kuramoto_system_t* system,
                                     const float* phases,
                                     float* derivatives) {
    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        derivatives[i] = compute_derivative(system, i, phases);
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT kuramoto_config_t kuramoto_config_default(void) {
    kuramoto_config_t config = {
        .base_coupling_strength = KURAMOTO_DEFAULT_COUPLING,
        .noise_intensity = KURAMOTO_DEFAULT_NOISE_INTENSITY,
        .dt = KURAMOTO_DEFAULT_DT,
        .max_oscillators = KURAMOTO_DEFAULT_OSCILLATORS,
        .use_pink_noise = true,
        .use_adaptive_coupling = false,
        .adaptive_rate = 0.01f,
        .seed = 0
    };
    return config;
}

NIMCP_EXPORT kuramoto_system_t* kuramoto_create(const kuramoto_config_t* config) {
    clear_error();

    kuramoto_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = kuramoto_config_default();
    }

    /* Validate configuration */
    if (cfg.max_oscillators == 0 || cfg.max_oscillators > KURAMOTO_MAX_OSCILLATORS) {
        set_error("Invalid max_oscillators: %u (range 1-%d)",
                  cfg.max_oscillators, KURAMOTO_MAX_OSCILLATORS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_create: cfg.max_oscillators is zero");
        return NULL;
    }
    if (cfg.dt <= 0.0f) {
        set_error("Invalid dt: %f (must be > 0)", cfg.dt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_create: validation failed");
        return NULL;
    }

    /* Allocate system structure */
    kuramoto_system_t* system = nimcp_calloc(1, sizeof(kuramoto_system_t));
    if (!system) {
        set_error("Failed to allocate system structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kuramoto_create: system is NULL");
        return NULL;
    }

    system->config = cfg;
    system->max_oscillators = cfg.max_oscillators;

    /* Seed RNG */
    seed_rng(cfg.seed);

    /* Allocate oscillator array */
    system->oscillators = nimcp_calloc(cfg.max_oscillators, sizeof(kuramoto_oscillator_t));
    if (!system->oscillators) {
        set_error("Failed to allocate oscillator array");
        goto error;
    }

    /* Allocate dense coupling matrix (default) */
    size_t matrix_size = (size_t)cfg.max_oscillators * cfg.max_oscillators;
    system->coupling_matrix = nimcp_calloc(matrix_size, sizeof(float));
    if (!system->coupling_matrix) {
        set_error("Failed to allocate coupling matrix");
        goto error;
    }
    system->use_sparse_coupling = false;

    /* Initialize default all-to-all coupling */
    for (uint32_t i = 0; i < cfg.max_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)cfg.max_oscillators);
        }

        for (uint32_t j = 0; j < cfg.max_oscillators; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && cfg.max_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(j + 1) / (float)cfg.max_oscillators);
            }

            if (i != j) {
                system->coupling_matrix[j * cfg.max_oscillators + i] = 1.0f;
            }
        }
    }

    /* Allocate RK4 workspace */
    system->k1 = nimcp_calloc(cfg.max_oscillators, sizeof(float));
    if (!system->k1) return NULL;
    system->k2 = nimcp_calloc(cfg.max_oscillators, sizeof(float));
    if (!system->k2) return NULL;
    system->k3 = nimcp_calloc(cfg.max_oscillators, sizeof(float));
    if (!system->k3) return NULL;
    system->k4 = nimcp_calloc(cfg.max_oscillators, sizeof(float));
    if (!system->k4) return NULL;
    system->temp_phases = nimcp_calloc(cfg.max_oscillators, sizeof(float));
    if (!system->temp_phases) return NULL;

    if (!system->k1 || !system->k2 || !system->k3 ||
        !system->k4 || !system->temp_phases) {
        set_error("Failed to allocate RK4 workspace");
        goto error;
    }

    /* Allocate module ID mapping */
    system->module_map_size = MODULE_MAP_INITIAL_SIZE;
    system->module_to_index = nimcp_calloc(system->module_map_size, sizeof(uint32_t));
    if (!system->module_to_index) {
        set_error("Failed to allocate module mapping");
        goto error;
    }
    for (uint32_t i = 0; i < system->module_map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->module_map_size > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->module_map_size);
        }

        system->module_to_index[i] = UINT32_MAX;
    }

    /* Allocate pink noise generators if enabled */
    if (cfg.use_pink_noise) {
        system->pink_generators = nimcp_calloc(cfg.max_oscillators,
                                          sizeof(pink_noise_generator_t));
        if (!system->pink_generators) {
            set_error("Failed to allocate pink noise generators");
            goto error;
        }
        system->noise_enabled = true;
        /* Note: Individual generators created when oscillators are added */
    }

    /* Initialize state */
    system->num_oscillators = 0;
    system->num_couplings = 0;
    system->order_valid = false;
    system->step_count = 0;
    system->total_time = 0.0f;

    return system;

error:
    kuramoto_destroy(system);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_create: operation failed");
    return NULL;
}

NIMCP_EXPORT void kuramoto_destroy(kuramoto_system_t* system) {
    if (!system) return;

    /* Free pink noise generators */
    if (system->pink_generators) {
        /* Note: Would call pink_noise_destroy() for each if linked */
        nimcp_free(system->pink_generators);
    }

    /* Free arrays */
    nimcp_free(system->oscillators);
    nimcp_free(system->coupling_matrix);
    nimcp_free(system->sparse_couplings);
    nimcp_free(system->k1);
    nimcp_free(system->k2);
    nimcp_free(system->k3);
    nimcp_free(system->k4);
    nimcp_free(system->temp_phases);
    nimcp_free(system->module_to_index);

    nimcp_free(system);
    system = NULL;
}

NIMCP_EXPORT bool kuramoto_reset(kuramoto_system_t* system) {
    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset: system is NULL");
        return false;
    }

    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active) {
            system->oscillators[i].phase = random_phase();
            system->oscillators[i].frequency_offset = 0.0f;
        }
    }

    system->order_valid = false;
    system->step_count = 0;
    system->total_time = 0.0f;

    clear_error();
    return true;
}

NIMCP_EXPORT bool kuramoto_reset_seeded(kuramoto_system_t* system, uint32_t seed) {
    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset_seeded: system is NULL");
        return false;
    }

    seed_rng(seed);
    return kuramoto_reset(system);
}

//=============================================================================
// Oscillator Management
//=============================================================================

NIMCP_EXPORT int32_t kuramoto_add_oscillator(kuramoto_system_t* system,
                                              uint32_t module_id,
                                              float natural_freq) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset_seeded: system is NULL");
        return -1;
    }

    /* Check if module already exists */
    int32_t existing = find_oscillator_index(system, module_id);
    if (existing >= 0) {
        set_error("Module %u already registered at index %d", module_id, existing);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_reset_seeded: capacity exceeded");
        return -1;
    }

    /* Find free slot */
    uint32_t idx = UINT32_MAX;
    for (uint32_t i = 0; i < system->max_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->max_oscillators);
        }

        if (!system->oscillators[i].active) {
            idx = i;
            break;
        }
    }

    if (idx == UINT32_MAX) {
        set_error("No free oscillator slots (max %u)", system->max_oscillators);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_reset_seeded: validation failed");
        return -1;
    }

    /* Initialize oscillator */
    kuramoto_oscillator_t* osc = &system->oscillators[idx];
    osc->phase = random_phase();
    osc->natural_frequency = natural_freq;
    osc->frequency_offset = 0.0f;
    osc->module_id = module_id;
    osc->active = true;

    /* Update mapping */
    update_module_mapping(system, module_id, idx);

    /* Track count */
    if (idx >= system->num_oscillators) {
        system->num_oscillators = idx + 1;
    }

    system->order_valid = false;

    return (int32_t)idx;
}

NIMCP_EXPORT bool kuramoto_remove_oscillator(kuramoto_system_t* system,
                                              uint32_t module_id) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset_seeded: system is NULL");
        return false;
    }

    int32_t idx = find_oscillator_index(system, module_id);
    if (idx < 0) {
        set_error("Module %u not found", module_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_reset_seeded: validation failed");
        return false;
    }

    /* Deactivate oscillator */
    system->oscillators[idx].active = false;

    /* Clear coupling for this oscillator */
    if (!system->use_sparse_coupling && system->coupling_matrix) {
        for (uint32_t i = 0; i < system->max_oscillators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->max_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(i + 1) / (float)system->max_oscillators);
            }

            system->coupling_matrix[(uint32_t)idx * system->max_oscillators + i] = 0.0f;
            system->coupling_matrix[i * system->max_oscillators + (uint32_t)idx] = 0.0f;
        }
    }

    /* Update mapping */
    if (system->module_to_index && module_id < system->module_map_size) {
        system->module_to_index[module_id] = UINT32_MAX;
    }

    system->order_valid = false;

    return true;
}

NIMCP_EXPORT bool kuramoto_set_frequency(kuramoto_system_t* system,
                                          uint32_t module_id,
                                          float freq) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset_seeded: system is NULL");
        return false;
    }

    int32_t idx = find_oscillator_index(system, module_id);
    if (idx < 0) {
        set_error("Module %u not found", module_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_reset_seeded: validation failed");
        return false;
    }

    system->oscillators[idx].natural_frequency = freq;
    return true;
}

NIMCP_EXPORT float kuramoto_get_phase(const kuramoto_system_t* system,
                                       uint32_t module_id) {
    if (!system) {
        return -1.0f;
    }

    int32_t idx = find_oscillator_index(system, module_id);
    if (idx < 0) {
        return -1.0f;
    }

    return system->oscillators[idx].phase;
}

NIMCP_EXPORT bool kuramoto_set_phase(kuramoto_system_t* system,
                                      uint32_t module_id,
                                      float phase) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset_seeded: system is NULL");
        return false;
    }

    int32_t idx = find_oscillator_index(system, module_id);
    if (idx < 0) {
        set_error("Module %u not found", module_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_reset_seeded: validation failed");
        return false;
    }

    system->oscillators[idx].phase = wrap_phase_internal(phase);
    system->order_valid = false;

    return true;
}

NIMCP_EXPORT const kuramoto_oscillator_t* kuramoto_get_oscillator(
    const kuramoto_system_t* system,
    uint32_t module_id) {

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return NULL;


    }

    int32_t idx = find_oscillator_index(system, module_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_reset_seeded: validation failed");
        return NULL;
    }

    return &system->oscillators[idx];
}

NIMCP_EXPORT int32_t kuramoto_get_oscillator_index(
    const kuramoto_system_t* system,
    uint32_t module_id) {

    return find_oscillator_index(system, module_id);
}

//=============================================================================
// Coupling Management
//=============================================================================

NIMCP_EXPORT bool kuramoto_set_coupling(kuramoto_system_t* system,
                                         uint32_t from_id,
                                         uint32_t to_id,
                                         float strength) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }

    int32_t from_idx = find_oscillator_index(system, from_id);
    int32_t to_idx = find_oscillator_index(system, to_id);

    if (from_idx < 0) {
        set_error("Source module %u not found", from_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }
    if (to_idx < 0) {
        set_error("Target module %u not found", to_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    if (system->use_sparse_coupling) {
        /* Update or add sparse coupling */
        for (uint32_t e = 0; e < system->num_couplings; e++) {
            /* Phase 8: Loop progress heartbeat */
            if ((e & 0xFF) == 0 && system->num_couplings > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(e + 1) / (float)system->num_couplings);
            }

            if (system->sparse_couplings[e].from_idx == (uint32_t)from_idx &&
                system->sparse_couplings[e].to_idx == (uint32_t)to_idx) {
                system->sparse_couplings[e].strength = strength;
                return true;
            }
        }
        /* Add new coupling */
        return kuramoto_add_sparse_coupling(system, from_id, to_id, strength);
    } else {
        /* Dense matrix */
        system->coupling_matrix[(uint32_t)from_idx * system->max_oscillators + (uint32_t)to_idx] = strength;
        return true;
    }
}

NIMCP_EXPORT bool kuramoto_set_global_coupling(kuramoto_system_t* system,
                                                float strength) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }

    if (system->use_sparse_coupling) {
        set_error("Cannot set global coupling in sparse mode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    for (uint32_t i = 0; i < system->max_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->max_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->max_oscillators);
        }

        for (uint32_t j = 0; j < system->max_oscillators; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && system->max_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(j + 1) / (float)system->max_oscillators);
            }

            if (i != j) {
                system->coupling_matrix[i * system->max_oscillators + j] = strength;
            }
        }
    }

    return true;
}

NIMCP_EXPORT bool kuramoto_set_coupling_matrix(kuramoto_system_t* system,
                                                const float* matrix) {
    clear_error();

    if (!system || !matrix) {
        set_error("NULL argument");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, matrix)");
        return false;
    }

    if (system->use_sparse_coupling) {
        set_error("Cannot set matrix in sparse mode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    size_t size = (size_t)system->max_oscillators * system->max_oscillators;
    memcpy(system->coupling_matrix, matrix, size * sizeof(float));

    return true;
}

NIMCP_EXPORT float kuramoto_get_coupling(const kuramoto_system_t* system,
                                          uint32_t from_id,
                                          uint32_t to_id) {
    if (!system) return 0.0f;

    int32_t from_idx = find_oscillator_index(system, from_id);
    int32_t to_idx = find_oscillator_index(system, to_id);

    if (from_idx < 0 || to_idx < 0) {
        return 0.0f;
    }

    if (system->use_sparse_coupling) {
        for (uint32_t e = 0; e < system->num_couplings; e++) {
            /* Phase 8: Loop progress heartbeat */
            if ((e & 0xFF) == 0 && system->num_couplings > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(e + 1) / (float)system->num_couplings);
            }

            if (system->sparse_couplings[e].from_idx == (uint32_t)from_idx &&
                system->sparse_couplings[e].to_idx == (uint32_t)to_idx) {
                return system->sparse_couplings[e].strength;
            }
        }
        return 0.0f;
    } else {
        return system->coupling_matrix[(uint32_t)from_idx * system->max_oscillators + (uint32_t)to_idx];
    }
}

NIMCP_EXPORT bool kuramoto_enable_sparse_coupling(kuramoto_system_t* system,
                                                   uint32_t max_couplings) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }

    if (system->use_sparse_coupling) {
        /* Already in sparse mode, just resize if needed */
        if (max_couplings > system->max_couplings) {
            kuramoto_coupling_t* new_couplings = nimcp_realloc(
                system->sparse_couplings,
                max_couplings * sizeof(kuramoto_coupling_t));
            if (!new_couplings) {
                set_error("Failed to resize sparse couplings");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: new_couplings is NULL");
                return false;
            }
            system->sparse_couplings = new_couplings;
            system->max_couplings = max_couplings;
        }
        return true;
    }

    /* Allocate sparse array */
    system->sparse_couplings = nimcp_calloc(max_couplings, sizeof(kuramoto_coupling_t));
    if (!system->sparse_couplings) {
        set_error("Failed to allocate sparse couplings");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: system->sparse_couplings is NULL");
        return false;
    }
    system->max_couplings = max_couplings;
    system->num_couplings = 0;

    /* Convert existing non-zero couplings to sparse format */
    if (system->coupling_matrix) {
        for (uint32_t i = 0; i < system->num_oscillators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(i + 1) / (float)system->num_oscillators);
            }

            for (uint32_t j = 0; j < system->num_oscillators; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && system->num_oscillators > 256) {
                    kuramoto_heartbeat("kuramoto_loop",
                                     (float)(j + 1) / (float)system->num_oscillators);
                }

                float K_ij = system->coupling_matrix[i * system->max_oscillators + j];
                if (fabsf(K_ij) > 1e-6f && system->num_couplings < max_couplings) {
                    system->sparse_couplings[system->num_couplings].from_idx = i;
                    system->sparse_couplings[system->num_couplings].to_idx = j;
                    system->sparse_couplings[system->num_couplings].strength = K_ij;
                    system->num_couplings++;
                }
            }
        }
    }

    system->use_sparse_coupling = true;
    return true;
}

NIMCP_EXPORT bool kuramoto_add_sparse_coupling(kuramoto_system_t* system,
                                                uint32_t from_id,
                                                uint32_t to_id,
                                                float strength) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }

    if (!system->use_sparse_coupling) {
        set_error("System not in sparse mode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system->use_sparse_coupling is NULL");
        return false;
    }

    int32_t from_idx = find_oscillator_index(system, from_id);
    int32_t to_idx = find_oscillator_index(system, to_id);

    if (from_idx < 0) {
        set_error("Source module %u not found", from_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }
    if (to_idx < 0) {
        set_error("Target module %u not found", to_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    /* Check if coupling already exists */
    for (uint32_t e = 0; e < system->num_couplings; e++) {
        /* Phase 8: Loop progress heartbeat */
        if ((e & 0xFF) == 0 && system->num_couplings > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(e + 1) / (float)system->num_couplings);
        }

        if (system->sparse_couplings[e].from_idx == (uint32_t)from_idx &&
            system->sparse_couplings[e].to_idx == (uint32_t)to_idx) {
            system->sparse_couplings[e].strength = strength;
            return true;
        }
    }

    /* Add new coupling */
    if (system->num_couplings >= system->max_couplings) {
        set_error("Sparse coupling array full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return false;
    }

    system->sparse_couplings[system->num_couplings].from_idx = (uint32_t)from_idx;
    system->sparse_couplings[system->num_couplings].to_idx = (uint32_t)to_idx;
    system->sparse_couplings[system->num_couplings].strength = strength;
    system->num_couplings++;

    return true;
}

//=============================================================================
// Simulation Functions
//=============================================================================

NIMCP_EXPORT bool kuramoto_step(kuramoto_system_t* system, float dt) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_step: system is NULL");
        return false;
    }

    /* Use config dt if not specified */
    if (dt <= 0.0f) {
        dt = system->config.dt;
    }

    uint32_t n = system->num_oscillators;
    if (n == 0) {
        return true; /* Nothing to do */
    }

    /* Update noise if enabled */
    if (system->noise_enabled) {
        kuramoto_update_noise(system);
    }

    /* Copy current phases */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n);
        }

        system->temp_phases[i] = system->oscillators[i].phase;
    }

    /* RK4 Integration */

    /* k1 = f(y, t) */
    compute_all_derivatives(system, system->temp_phases, system->k1);

    /* k2 = f(y + dt/2 * k1, t + dt/2) */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n);
        }

        system->temp_phases[i] = system->oscillators[i].phase + 0.5f * dt * system->k1[i];
    }
    compute_all_derivatives(system, system->temp_phases, system->k2);

    /* k3 = f(y + dt/2 * k2, t + dt/2) */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n);
        }

        system->temp_phases[i] = system->oscillators[i].phase + 0.5f * dt * system->k2[i];
    }
    compute_all_derivatives(system, system->temp_phases, system->k3);

    /* k4 = f(y + dt * k3, t + dt) */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n);
        }

        system->temp_phases[i] = system->oscillators[i].phase + dt * system->k3[i];
    }
    compute_all_derivatives(system, system->temp_phases, system->k4);

    /* Update phases: y_new = y + dt/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n);
        }

        if (system->oscillators[i].active) {
            float delta = (dt / 6.0f) * (system->k1[i] + 2.0f * system->k2[i] +
                                          2.0f * system->k3[i] + system->k4[i]);
            system->oscillators[i].phase = wrap_phase_internal(
                system->oscillators[i].phase + delta);
        }
    }

    /* Invalidate cached order parameter */
    system->order_valid = false;

    /* Update statistics */
    system->step_count++;
    system->total_time += dt;

    return true;
}

NIMCP_EXPORT bool kuramoto_step_n(kuramoto_system_t* system,
                                   float dt,
                                   uint32_t n_steps) {
    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_step: system is NULL");
        return false;
    }

    for (uint32_t i = 0; i < n_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n_steps > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n_steps);
        }

        if (!kuramoto_step(system, dt)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_step: kuramoto_step is NULL");
            return false;
        }
    }

    return true;
}

NIMCP_EXPORT bool kuramoto_evolve(kuramoto_system_t* system, float duration) {
    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_evolve: system is NULL");
        return false;
    }

    if (duration <= 0.0f) {
        return true;
    }

    float dt = system->config.dt;
    uint32_t n_steps = (uint32_t)(duration / (fabsf(dt) > 1e-7f ? dt : 1e-7f) + 0.5f);

    return kuramoto_step_n(system, dt, n_steps);
}

//=============================================================================
// Order Parameter and Coherence
//=============================================================================

NIMCP_EXPORT bool kuramoto_compute_order_parameter(kuramoto_system_t* system) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_compute_order_parameter: system is NULL");
        return false;
    }

    /* Compute r * exp(i*psi) = (1/N) * SUM_j[exp(i*theta_j)] */
    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active) {
            float theta = system->oscillators[i].phase;
            sum_cos += cosf(theta);
            sum_sin += sinf(theta);
            active_count++;
        }
    }

    if (active_count == 0) {
        system->order_parameter_r = 0.0f;
        system->order_parameter_psi = 0.0f;
    } else {
        float mean_cos = sum_cos / (float)active_count;
        float mean_sin = sum_sin / (float)active_count;

        /* r = |mean phasor| */
        system->order_parameter_r = sqrtf(mean_cos * mean_cos + mean_sin * mean_sin);

        /* psi = arg(mean phasor) */
        system->order_parameter_psi = atan2f(mean_sin, mean_cos);
        if (system->order_parameter_psi < 0.0f) {
            system->order_parameter_psi += NIMCP_TWO_PI_F;
        }
    }

    system->order_valid = true;
    return true;
}

NIMCP_EXPORT float kuramoto_get_order_parameter(kuramoto_system_t* system) {
    if (!system) return -1.0f;

    if (!system->order_valid) {
        if (!kuramoto_compute_order_parameter(system)) {
            return -1.0f;
        }
    }

    return system->order_parameter_r;
}

NIMCP_EXPORT float kuramoto_get_mean_phase(kuramoto_system_t* system) {
    if (!system) return -1.0f;

    if (!system->order_valid) {
        if (!kuramoto_compute_order_parameter(system)) {
            return -1.0f;
        }
    }

    return system->order_parameter_psi;
}

NIMCP_EXPORT float kuramoto_coherence(const kuramoto_system_t* system,
                                       uint32_t module_id1,
                                       uint32_t module_id2) {
    if (!system) return 0.0f;

    int32_t idx1 = find_oscillator_index(system, module_id1);
    int32_t idx2 = find_oscillator_index(system, module_id2);

    if (idx1 < 0 || idx2 < 0) {
        return 0.0f;
    }

    float phase1 = system->oscillators[idx1].phase;
    float phase2 = system->oscillators[idx2].phase;

    /* Coherence = cos(phase_difference) */
    return cosf(phase1 - phase2);
}

NIMCP_EXPORT bool kuramoto_is_synchronized(kuramoto_system_t* system,
                                            float threshold) {
    if (!system) {
        return false;
    }

    float r = kuramoto_get_order_parameter(system);
    return (r >= 0.0f && r > threshold);
}

NIMCP_EXPORT bool kuramoto_get_coherence_matrix(const kuramoto_system_t* system,
                                                 float* out) {
    clear_error();

    if (!system || !out) {
        set_error("NULL argument");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_get_mean_phase: required parameter is NULL (system, out)");
        return false;
    }

    uint32_t n = system->num_oscillators;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)n);
        }

        for (uint32_t j = 0; j < n; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && n > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(j + 1) / (float)n);
            }

            if (!system->oscillators[i].active || !system->oscillators[j].active) {
                out[i * n + j] = 0.0f;
            } else if (i == j) {
                out[i * n + j] = 1.0f;
            } else {
                float phase_i = system->oscillators[i].phase;
                float phase_j = system->oscillators[j].phase;
                out[i * n + j] = cosf(phase_i - phase_j);
            }
        }
    }

    return true;
}

//=============================================================================
// Pink Noise Integration
//=============================================================================

NIMCP_EXPORT bool kuramoto_update_noise(kuramoto_system_t* system) {
    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_update_noise: system is NULL");
        return false;
    }

    if (!system->noise_enabled) {
        return true;
    }

    /* Generate pink noise offset for each oscillator */
    /* For now, use simple random walk approximation */
    /* In full implementation, would use pink_noise_next() */
    float intensity = system->config.noise_intensity;

    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active) {
            /* Simple approximation: random walk with decay */
            float noise = (random_float() - 0.5f) * 2.0f * intensity;
            system->oscillators[i].frequency_offset =
                0.95f * system->oscillators[i].frequency_offset + 0.05f * noise;
        }
    }

    return true;
}

NIMCP_EXPORT bool kuramoto_set_noise_intensity(kuramoto_system_t* system,
                                                float intensity) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_update_noise: system is NULL");
        return false;
    }

    system->config.noise_intensity = intensity;
    return true;
}

NIMCP_EXPORT bool kuramoto_set_noise_enabled(kuramoto_system_t* system,
                                              bool enabled) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_update_noise: system is NULL");
        return false;
    }

    system->noise_enabled = enabled;

    /* Reset offsets if disabling */
    if (!enabled) {
        for (uint32_t i = 0; i < system->num_oscillators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(i + 1) / (float)system->num_oscillators);
            }

            system->oscillators[i].frequency_offset = 0.0f;
        }
    }

    return true;
}

//=============================================================================
// Module Integration
//=============================================================================

NIMCP_EXPORT bool kuramoto_register_module(kuramoto_system_t* system,
                                            uint32_t module_id,
                                            float freq) {
    int32_t idx = kuramoto_add_oscillator(system, module_id, freq);
    return (idx >= 0);
}

NIMCP_EXPORT bool kuramoto_unregister_module(kuramoto_system_t* system,
                                              uint32_t module_id) {
    return kuramoto_remove_oscillator(system, module_id);
}

NIMCP_EXPORT float kuramoto_get_module_phase(const kuramoto_system_t* system,
                                              uint32_t module_id) {
    return kuramoto_get_phase(system, module_id);
}

NIMCP_EXPORT bool kuramoto_sync_modules(kuramoto_system_t* system,
                                         uint32_t module_id1,
                                         uint32_t module_id2) {
    clear_error();

    if (!system) {
        set_error("NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_update_noise: system is NULL");
        return false;
    }

    int32_t idx1 = find_oscillator_index(system, module_id1);
    int32_t idx2 = find_oscillator_index(system, module_id2);

    if (idx1 < 0) {
        set_error("Module %u not found", module_id1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_update_noise: validation failed");
        return false;
    }
    if (idx2 < 0) {
        set_error("Module %u not found", module_id2);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kuramoto_update_noise: validation failed");
        return false;
    }

    /* Set both to mean phase */
    float phase1 = system->oscillators[idx1].phase;
    float phase2 = system->oscillators[idx2].phase;

    /* Compute circular mean of two phases */
    float mean_sin = (sinf(phase1) + sinf(phase2)) / 2.0f;
    float mean_cos = (cosf(phase1) + cosf(phase2)) / 2.0f;
    float mean_phase = atan2f(mean_sin, mean_cos);
    if (mean_phase < 0.0f) {
        mean_phase += NIMCP_TWO_PI_F;
    }

    system->oscillators[idx1].phase = mean_phase;
    system->oscillators[idx2].phase = mean_phase;
    system->order_valid = false;

    return true;
}

NIMCP_EXPORT bool kuramoto_modules_coherent(const kuramoto_system_t* system,
                                             uint32_t module_id1,
                                             uint32_t module_id2,
                                             float threshold) {
    float coherence = kuramoto_coherence(system, module_id1, module_id2);
    return (coherence > threshold);
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT bool kuramoto_get_stats(const kuramoto_system_t* system,
                                      kuramoto_stats_t* stats) {
    clear_error();

    if (!system || !stats) {
        set_error("NULL argument");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_update_noise: required parameter is NULL (system, stats)");
        return false;
    }

    memset(stats, 0, sizeof(kuramoto_stats_t));

    /* Count active oscillators and compute frequency statistics */
    uint32_t active = 0;
    float freq_sum = 0.0f;
    float freq_sq_sum = 0.0f;

    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active) {
            active++;
            float freq = system->oscillators[i].natural_frequency;
            freq_sum += freq;
            freq_sq_sum += freq * freq;
        }
    }

    stats->active_oscillators = active;

    if (active > 0) {
        stats->mean_frequency = freq_sum / (float)active;
        float variance = (freq_sq_sum / (float)active) -
                         (stats->mean_frequency * stats->mean_frequency);
        stats->frequency_spread = (variance > 0.0f) ? sqrtf(variance) : 0.0f;
    }

    /* Order parameter (compute if needed) */
    /* Note: Cast away const for computation, but don't modify state */
    kuramoto_system_t* mutable_sys = (kuramoto_system_t*)system;
    if (!system->order_valid) {
        kuramoto_compute_order_parameter(mutable_sys);
    }
    stats->order_parameter_r = system->order_parameter_r;
    stats->order_parameter_psi = system->order_parameter_psi;

    /* Mean coupling (from dense matrix only) */
    if (!system->use_sparse_coupling && system->coupling_matrix) {
        float coupling_sum = 0.0f;
        uint32_t coupling_count = 0;
        for (uint32_t i = 0; i < system->num_oscillators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
                kuramoto_heartbeat("kuramoto_loop",
                                 (float)(i + 1) / (float)system->num_oscillators);
            }

            for (uint32_t j = 0; j < system->num_oscillators; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && system->num_oscillators > 256) {
                    kuramoto_heartbeat("kuramoto_loop",
                                     (float)(j + 1) / (float)system->num_oscillators);
                }

                if (i != j) {
                    coupling_sum += system->coupling_matrix[i * system->max_oscillators + j];
                    coupling_count++;
                }
            }
        }
        if (coupling_count > 0) {
            stats->mean_coupling = coupling_sum / (float)coupling_count;
        }
    }

    /* Time statistics */
    stats->total_steps = system->step_count;
    stats->total_time = system->total_time;

    /* Coupling energy (sum of cos(theta_i - theta_j) for all pairs) */
    float energy = 0.0f;
    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (!system->oscillators[i].active) continue;
        for (uint32_t j = i + 1; j < system->num_oscillators; j++) {
            if (!system->oscillators[j].active) continue;
            float phase_diff = system->oscillators[i].phase - system->oscillators[j].phase;
            energy += cosf(phase_diff);
        }
    }
    stats->coupling_energy = energy;

    return true;
}

NIMCP_EXPORT void kuramoto_print_state(const kuramoto_system_t* system) {
    if (!system) {
        printf("Kuramoto System: NULL\n");
        return;
    }

    printf("=== Kuramoto System State ===\n");
    printf("Oscillators: %u/%u active/max\n",
           system->num_oscillators, system->max_oscillators);
    printf("Coupling: %s (K=%.3f)\n",
           system->use_sparse_coupling ? "sparse" : "dense",
           system->config.base_coupling_strength);
    printf("Noise: %s (intensity=%.3f)\n",
           system->noise_enabled ? "enabled" : "disabled",
           system->config.noise_intensity);
    printf("Steps: %lu, Time: %.3f s\n",
           (unsigned long)system->step_count, system->total_time);

    /* Compute order parameter if needed */
    kuramoto_system_t* mutable_sys = (kuramoto_system_t*)system;
    if (!system->order_valid) {
        kuramoto_compute_order_parameter(mutable_sys);
    }
    printf("Order parameter: r=%.4f, psi=%.4f rad\n",
           system->order_parameter_r, system->order_parameter_psi);

    printf("\nOscillators:\n");
    printf("  %-6s %-8s %-10s %-10s %-8s\n",
           "Index", "ModuleID", "Phase", "Frequency", "Active");
    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        const kuramoto_oscillator_t* osc = &system->oscillators[i];
        if (osc->active) {
            printf("  %-6u %-8u %-10.4f %-10.4f %-8s\n",
                   i, osc->module_id, osc->phase,
                   osc->natural_frequency + osc->frequency_offset,
                   osc->active ? "yes" : "no");
        }
    }
    printf("===========================\n");
}

NIMCP_EXPORT uint32_t kuramoto_get_all_phases(const kuramoto_system_t* system,
                                               float* phases) {
    if (!system || !phases) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active) {
            phases[count++] = system->oscillators[i].phase;
        }
    }
    return count;
}

NIMCP_EXPORT bool kuramoto_set_all_phases(kuramoto_system_t* system,
                                           const float* phases,
                                           uint32_t count) {
    clear_error();

    if (!system || !phases) {
        set_error("NULL argument");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kuramoto_print_state: required parameter is NULL (system, phases)");
        return false;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < system->num_oscillators && idx < count; i++) {
        if (system->oscillators[i].active) {
            system->oscillators[i].phase = wrap_phase_internal(phases[idx++]);
        }
    }

    system->order_valid = false;
    return true;
}

NIMCP_EXPORT uint32_t kuramoto_get_num_oscillators(
    const kuramoto_system_t* system) {

    if (!system) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < system->num_oscillators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_oscillators > 256) {
            kuramoto_heartbeat("kuramoto_loop",
                             (float)(i + 1) / (float)system->num_oscillators);
        }

        if (system->oscillators[i].active) {
            count++;
        }
    }
    return count;
}

NIMCP_EXPORT float kuramoto_wrap_phase(float phase) {
    return wrap_phase_internal(phase);
}

NIMCP_EXPORT float kuramoto_phase_difference(float phase1, float phase2) {
    float diff = phase1 - phase2;

    /* Wrap to [-pi, pi] */
    while (diff > M_PI) {
        diff -= NIMCP_TWO_PI_F;
    }
    while (diff < -M_PI) {
        diff += NIMCP_TWO_PI_F;
    }

    return diff;
}

NIMCP_EXPORT const char* kuramoto_get_last_error(void) {
    return (g_kuramoto_error[0] != '\0') ? g_kuramoto_error : NULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void kuramoto_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_kuramoto_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int kuramoto_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "kuramoto_training_begin: NULL argument");
        return -1;
    }
    kuramoto_heartbeat_instance(NULL, "kuramoto_training_begin", 0.0f);
    return 0;
}

int kuramoto_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "kuramoto_training_end: NULL argument");
        return -1;
    }
    kuramoto_heartbeat_instance(NULL, "kuramoto_training_end", 1.0f);
    return 0;
}

int kuramoto_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "kuramoto_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    kuramoto_heartbeat_instance(NULL, "kuramoto_training_step", progress);
    return 0;
}
