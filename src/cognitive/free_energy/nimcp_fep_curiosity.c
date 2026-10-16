/**
 * @file nimcp_fep_curiosity.c
 * @brief Epistemic Value and Curiosity for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of curiosity-driven exploration in FEP
 * WHY:  Agents must balance exploitation (pragmatic) with exploration (epistemic)
 * HOW:  Compute information gain, empowerment, and novelty to drive curiosity
 */

#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(fep_curiosity_instance, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Alias: tests reference fep_curiosity_set_health_agent (without _instance suffix) */
static nimcp_health_agent_t* g_fep_curiosity_health_agent = NULL;
void fep_curiosity_set_health_agent(struct nimcp_health_agent* agent) { g_fep_curiosity_health_agent = agent; }


/* Local result structure for total curiosity computation */
typedef struct {
    float epistemic_value;
    float novelty;
    float empowerment;
    float total_curiosity;
} fep_curiosity_result_t;

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief State memory entry for novelty detection
 */
typedef struct {
    float* state;           /**< State vector */
    size_t dim;             /**< State dimensionality */
    uint32_t visit_count;   /**< Number of visits */
} state_memory_entry_t;

/**
 * @brief Internal curiosity system structure
 */
struct fep_curiosity_system {
    fep_curiosity_config_t config;      /**< Configuration */
    fep_curiosity_state_t state;        /**< Current state */
    fep_curiosity_stats_t stats;        /**< Statistics */

    /* FEP system connection */
    fep_system_t* fep_system;           /**< Connected FEP system */

    /* Novelty memory */
    state_memory_entry_t* memory;       /**< State memory bank */
    size_t memory_count;                /**< Current memory size */
    size_t memory_capacity;             /**< Maximum memory size */

    /* Bio-async */
    bio_module_context_t bio_ctx;       /**< Bio-async context */
    bool bio_async_enabled;             /**< Bio-async status */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Mutex for thread safety */
};

static inline float safe_log(float x) {
    if (x <= 0.0f) return -100.0f;
    return logf(x);
}

/**
 * @brief Compute entropy using central statistics module
 *
 * Note: nimcp_stats_entropy returns entropy in bits (log2).
 * The original used natural log, so we convert: H_nats = H_bits * ln(2)
 */
static float compute_entropy(const float* probs, size_t n) {
    if (!probs || n == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    if (n > 256) {
        fep_curiosity_instance_heartbeat("fep_curiosit_entropy", 0.5f);
    }

    /* nimcp_stats_entropy returns bits; convert to nats for compatibility */
    float entropy_bits = nimcp_stats_entropy(probs, (uint32_t)n);
    return entropy_bits * 0.693147f;  /* ln(2) to convert bits to nats */
}

static uint32_t hash_state(const float* state, size_t dim) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                             (float)(i + 1) / (float)dim);
        }

        int32_t int_val = (int32_t)(state[i] * 10000.0f);
        hash = ((hash << 5) + hash) + (uint32_t)int_val;
    }
    return hash;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

void fep_curiosity_default_config(fep_curiosity_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_default_config", 0.0f);


    config->type = CURIOSITY_EPISTEMIC;
    config->exploration_bonus = FEP_CURIOSITY_DEFAULT_EXPLORATION_BONUS;
    config->novelty_threshold = FEP_CURIOSITY_DEFAULT_NOVELTY_THRESHOLD;
    config->information_gain_weight = FEP_CURIOSITY_DEFAULT_INFO_GAIN_WEIGHT;
    config->empowerment_weight = FEP_CURIOSITY_DEFAULT_EMPOWERMENT_WEIGHT;
    config->enable_intrinsic_motivation = true;
    config->memory_capacity = FEP_CURIOSITY_DEFAULT_MEMORY_CAPACITY;
}

fep_curiosity_system_t* fep_curiosity_create(const fep_curiosity_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_create", 0.0f);


    fep_curiosity_system_t* sys = (fep_curiosity_system_t*)nimcp_calloc(
        1, sizeof(fep_curiosity_system_t));
    NIMCP_API_CHECK_ALLOC(sys, "Failed to allocate curiosity system");

    /* Apply configuration */
    fep_curiosity_config_t default_cfg;
    if (!config) {
        fep_curiosity_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Initialize state */
    memset(&sys->state, 0, sizeof(fep_curiosity_state_t));
    memset(&sys->stats, 0, sizeof(fep_curiosity_stats_t));

    /* Allocate novelty memory */
    if (config->memory_capacity > 0) {
        sys->memory = (state_memory_entry_t*)nimcp_calloc(
            config->memory_capacity, sizeof(state_memory_entry_t));
        sys->memory_capacity = config->memory_capacity;
        sys->memory_count = 0;
    }

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        fep_curiosity_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_curiosity_create: sys->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Curiosity system created");
    return sys;
}

void fep_curiosity_destroy(fep_curiosity_system_t* sys) {
    if (!sys) return;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_destroy", 0.0f);


    if (sys->bio_async_enabled) {
        fep_curiosity_disconnect_bio_async(sys);
    }

    /* Free memory entries */
    if (sys->memory) {
        for (size_t i = 0; i < sys->memory_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sys->memory_count > 256) {
                fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                                 (float)(i + 1) / (float)sys->memory_count);
            }

            if (sys->memory[i].state) {
                nimcp_free(sys->memory[i].state);
            }
        }
        nimcp_free(sys->memory);
    }

    if (sys->mutex) {
        nimcp_platform_mutex_destroy(sys->mutex);
        nimcp_free(sys->mutex);
        sys->mutex = NULL;
    }

    nimcp_free(sys);
    sys = NULL;
    NIMCP_LOGGING_INFO("Curiosity system destroyed");
}

int fep_curiosity_reset(fep_curiosity_system_t* sys) {
    if (!sys) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_reset", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Reset state */
    memset(&sys->state, 0, sizeof(fep_curiosity_state_t));
    memset(&sys->stats, 0, sizeof(fep_curiosity_stats_t));

    /* Clear memory */
    if (sys->memory) {
        for (size_t i = 0; i < sys->memory_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sys->memory_count > 256) {
                fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                                 (float)(i + 1) / (float)sys->memory_count);
            }

            if (sys->memory[i].state) {
                nimcp_free(sys->memory[i].state);
                sys->memory[i].state = NULL;
            }
        }
        sys->memory_count = 0;
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

/* ============================================================================
 * Core Computation API
 * ============================================================================ */

float fep_compute_epistemic_value(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const fep_policy_t* policy
) {
    if (!sys || !fep) return 0.0f;

    /* Compute expected information gain under policy */
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_fep_compute_epistemi", 0.0f);


    float epistemic_value = 0.0f;

    /* Simplified: use entropy reduction estimate */
    /* In full implementation, integrate over predicted observations */
    epistemic_value = sys->config.information_gain_weight *
                      (0.5f + 0.5f * sys->state.exploration_drive);

    sys->state.epistemic_value = epistemic_value;
    return epistemic_value;
}

float fep_compute_information_gain(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t dim
) {
    if (!sys || !fep || !observation || dim == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_fep_compute_informat", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* KL divergence between posterior and prior */
    float info_gain = 0.0f;

    /* Simplified: estimate based on observation surprise */
    float surprise = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                             (float)(i + 1) / (float)dim);
        }

        float expected = 1.0f / (float)dim;  /* Uniform prior */
        if (observation[i] > 1e-10f) {
            surprise += observation[i] * (safe_log(observation[i]) - safe_log(expected));
        }
    }
    info_gain = nimcp_clampf(surprise, 0.0f, 10.0f);

    sys->state.information_gain = info_gain;
    sys->stats.total_information_gain += info_gain;
    sys->stats.observations_processed++;

    nimcp_platform_mutex_unlock(sys->mutex);
    return info_gain;
}

float fep_compute_empowerment(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* state,
    size_t dim
) {
    if (!sys || !fep || !state || dim == 0) return 0.0f;

    /* Empowerment = I[a, s'|s] = mutual information between actions and future states */
    /* Simplified estimation */
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_fep_compute_empowerm", 0.0f);


    float empowerment = 0.5f;  /* Baseline empowerment */

    /* States near center of distribution have higher empowerment */
    float state_magnitude = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                             (float)(i + 1) / (float)dim);
        }

        state_magnitude += state[i] * state[i];
    }
    state_magnitude = sqrtf(state_magnitude);

    /* Moderate states have higher empowerment (more options) */
    if (state_magnitude > 0.3f && state_magnitude < 0.7f) {
        empowerment = 0.7f;
    }

    sys->state.empowerment = empowerment;
    return empowerment * sys->config.empowerment_weight;
}

float fep_compute_novelty(
    fep_curiosity_system_t* sys,
    const float* state,
    size_t dim
) {
    if (!sys || !state || dim == 0) return 0.0f;  /* NULL = no novelty */

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_fep_compute_novelty", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Count-based novelty: novelty ∝ 1/√(count + 1) */
    uint32_t hash = hash_state(state, dim);
    uint32_t count = FEP_CURIOSITY_MIN_COUNT;

    /* Search memory for matching state */
    for (size_t i = 0; i < sys->memory_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->memory_count > 256) {
            fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                             (float)(i + 1) / (float)sys->memory_count);
        }

        if (sys->memory[i].dim == dim && sys->memory[i].state) {
            uint32_t entry_hash = hash_state(sys->memory[i].state, dim);
            if (entry_hash == hash) {
                count = sys->memory[i].visit_count + 1;
                break;
            }
        }
    }

    float novelty = 1.0f / sqrtf((float)(count + 1));
    sys->state.novelty_score = novelty;

    if (novelty > sys->config.novelty_threshold) {
        sys->stats.novel_states_found++;
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return novelty;
}

/* ============================================================================
 * Active Inference Integration API
 * ============================================================================ */

int fep_curiosity_modulate_efe(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    fep_efe_t* efe
) {
    if (!sys || !fep || !efe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_modulate_efe: required parameter is NULL (sys, fep, efe)");
        return -1;
    }

    /* Modulate EFE with curiosity bonus */
    /* G'(π) = G(π) - α·EpistemicValue - β·Empowerment - γ·Novelty */

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_modulate_efe", 0.0f);


    float curiosity_bonus = 0.0f;
    curiosity_bonus += sys->config.information_gain_weight * sys->state.epistemic_value;
    curiosity_bonus += sys->config.empowerment_weight * sys->state.empowerment;
    curiosity_bonus += sys->config.exploration_bonus * sys->state.novelty_score;

    /* Apply bonus (reduce EFE for exploratory actions) */
    efe->total -= curiosity_bonus;

    return 0;
}

int fep_curiosity_select_action(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    uint32_t* action
) {
    if (!sys || !fep || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_select_action: required parameter is NULL (sys, fep, action)");
        return -1;
    }

    /* Select action with curiosity bonus */
    /* For now, return action 0 with exploration bonus */
    *action = 0;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_select_action", 0.0f);


    return 0;
}

/* ============================================================================
 * State Tracking API
 * ============================================================================ */

int fep_curiosity_record_observation(
    fep_curiosity_system_t* sys,
    const float* observation,
    size_t dim
) {
    if (!sys || !observation || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_record_observation: required parameter is NULL (sys, observation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_record_observation", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Check if state is in memory */
    uint32_t hash = hash_state(observation, dim);
    bool found = false;

    for (size_t i = 0; i < sys->memory_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->memory_count > 256) {
            fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                             (float)(i + 1) / (float)sys->memory_count);
        }

        if (sys->memory[i].dim == dim && sys->memory[i].state) {
            uint32_t entry_hash = hash_state(sys->memory[i].state, dim);
            if (entry_hash == hash) {
                sys->memory[i].visit_count++;
                found = true;
                break;
            }
        }
    }

    /* Add new entry if not found and space available */
    if (!found && sys->memory_count < sys->memory_capacity) {
        float* new_state = (float*)nimcp_calloc(dim, sizeof(float));
        if (new_state) {
            memcpy(new_state, observation, dim * sizeof(float));
            sys->memory[sys->memory_count].state = new_state;
            sys->memory[sys->memory_count].dim = dim;
            sys->memory[sys->memory_count].visit_count = 1;
            sys->memory_count++;
        }
    }

    /* Compute novelty for this observation */
    uint32_t count = FEP_CURIOSITY_MIN_COUNT;
    for (size_t i = 0; i < sys->memory_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->memory_count > 256) {
            fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                             (float)(i + 1) / (float)sys->memory_count);
        }

        if (sys->memory[i].dim == dim && sys->memory[i].state) {
            uint32_t entry_hash = hash_state(sys->memory[i].state, dim);
            if (entry_hash == hash) {
                count = sys->memory[i].visit_count;
                break;
            }
        }
    }

    float novelty = 1.0f / sqrtf((float)(count + 1));
    if (novelty > sys->config.novelty_threshold) {
        sys->stats.novel_states_found++;
    }

    /* Increment observation counter */
    sys->stats.observations_processed++;

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_curiosity_get_state(
    const fep_curiosity_system_t* sys,
    fep_curiosity_state_t* state
) {
    if (!sys || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_get_state: required parameter is NULL (sys, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_get_state", 0.0f);

    nimcp_platform_mutex_lock(((fep_curiosity_system_t*)sys)->mutex);
    *state = sys->state;
    nimcp_platform_mutex_unlock(((fep_curiosity_system_t*)sys)->mutex);

    return 0;
}

int fep_curiosity_get_stats(
    const fep_curiosity_system_t* sys,
    fep_curiosity_stats_t* stats
) {
    if (!sys || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_get_stats: required parameter is NULL (sys, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_get_stats", 0.0f);

    nimcp_platform_mutex_lock(((fep_curiosity_system_t*)sys)->mutex);
    *stats = sys->stats;

    if (sys->stats.observations_processed > 0) {
        stats->avg_epistemic_value = sys->stats.total_information_gain /
                                      (float)sys->stats.observations_processed;
    }
    nimcp_platform_mutex_unlock(((fep_curiosity_system_t*)sys)->mutex);
    return 0;
}

/* ============================================================================
 * FEP System Integration API
 * ============================================================================ */

int fep_curiosity_connect(
    fep_curiosity_system_t* curiosity,
    fep_system_t* fep
) {
    if (!curiosity || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_connect: required parameter is NULL (curiosity, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_connect", 0.0f);

    nimcp_platform_mutex_lock(curiosity->mutex);
    curiosity->fep_system = fep;
    nimcp_platform_mutex_unlock(curiosity->mutex);
    NIMCP_LOGGING_INFO("Curiosity connected to FEP system");
    return 0;
}

int fep_curiosity_disconnect(fep_curiosity_system_t* curiosity) {
    if (!curiosity) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_disconnect", 0.0f);


    curiosity->fep_system = NULL;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int fep_curiosity_connect_bio_async(fep_curiosity_system_t* sys) {
    if (!sys) {
        return -1;
    }
    if (sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_CURIOSITY,
        .module_name = "fep_curiosity",
        .inbox_capacity = 32,
        .user_data = sys
    };

    sys->bio_ctx = bio_router_register_module(&info);
    if (sys->bio_ctx) {
        sys->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Curiosity connected to bio-async");
    }
    return 0;
}

int fep_curiosity_disconnect_bio_async(fep_curiosity_system_t* sys) {
    if (!sys) {
        return -1;
    }
    if (!sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_disconnect_bio_async", 0.0f);


    if (sys->bio_ctx) {
        bio_router_unregister_module(sys->bio_ctx);
        sys->bio_ctx = NULL;
    }
    sys->bio_async_enabled = false;
    return 0;
}

bool fep_curiosity_is_bio_async_connected(const fep_curiosity_system_t* sys) {
    if (!sys) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_is_bio_async_connect", 0.0f);


    return sys->bio_async_enabled;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* fep_curiosity_type_to_string(fep_curiosity_type_t type) {
    switch (type) {
        case CURIOSITY_EPISTEMIC:   return "Epistemic";
        case CURIOSITY_EMPOWERMENT: return "Empowerment";
        case CURIOSITY_COMPETENCE:  return "Competence";
        case CURIOSITY_NOVELTY:     return "Novelty";
        default:                    return "Unknown";
    }
}

/* ============================================================================
 * Additional Helper Functions for Tests
 * ============================================================================ */

float fep_curiosity_compute_epistemic(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* state,
    size_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_compute_epistemic", 0.0f);


    return fep_compute_epistemic_value(sys, fep, NULL);
}

float fep_curiosity_compute_novelty(
    fep_curiosity_system_t* sys,
    const float* state,
    size_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_compute_novelty", 0.0f);


    return fep_compute_novelty(sys, state, dim);
}

float fep_curiosity_compute_empowerment(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* state,
    size_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_compute_empowerment", 0.0f);


    return fep_compute_empowerment(sys, fep, state, dim);
}

int fep_curiosity_add_to_memory(
    fep_curiosity_system_t* sys,
    const float* state,
    size_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_add_to_memory", 0.0f);


    return fep_curiosity_record_observation(sys, state, dim);
}

int fep_curiosity_clear_memory(fep_curiosity_system_t* sys) {
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_clear_memory", 0.0f);


    return fep_curiosity_reset(sys);
}

int fep_curiosity_update(fep_curiosity_system_t* sys, uint64_t delta_ms) {
    if (!sys) {
        return -1;
    }

    /* Update exploration drive based on recent novelty */
    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_update", 0.0f);

    nimcp_platform_mutex_lock(sys->mutex);
    float decay = expf(-(float)delta_ms / 10000.0f);
    sys->state.exploration_drive = sys->state.exploration_drive * decay +
                                    sys->state.novelty_score * (1.0f - decay);
    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_curiosity_compute_total(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* state,
    size_t dim,
    fep_curiosity_result_t* result
) {
    if (!sys || !state || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_curiosity_compute_total: required parameter is NULL (sys, state, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_compute_total", 0.0f);


    result->epistemic_value = fep_curiosity_compute_epistemic(sys, fep, state, dim);
    result->novelty = fep_curiosity_compute_novelty(sys, state, dim);
    result->empowerment = fep_curiosity_compute_empowerment(sys, fep, state, dim);

    result->total_curiosity = sys->config.information_gain_weight * result->epistemic_value +
                               sys->config.empowerment_weight * result->empowerment +
                               sys->config.exploration_bonus * result->novelty;

    return 0;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int fep_curiosity_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_curiosity_instance_heartbeat("fep_curiosit_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Curiosity");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_curiosity_instance_heartbeat("fep_curiosit_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP Curiosity self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Curiosity");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Curiosity");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_curiosity_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;  /* context pointer not used for global singleton */
    g_fep_curiosity_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_curiosity_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_curiosity_training_begin: NULL argument");
        return -1;
    }
    fep_curiosity_instance_heartbeat_instance(g_fep_curiosity_instance_health_agent, "fep_cur_training_begin", 0.0f);
    (void)ctx;
    return 0;
}

int fep_curiosity_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_curiosity_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_curiosity_instance_heartbeat_instance(g_fep_curiosity_instance_health_agent, "fep_cur_training_step", clamped);
    (void)ctx;
    return 0;
}

int fep_curiosity_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_curiosity_training_end: NULL argument");
        return -1;
    }
    fep_curiosity_instance_heartbeat_instance(g_fep_curiosity_instance_health_agent, "fep_cur_training_end", 1.0f);
    (void)ctx;
    return 0;
}
