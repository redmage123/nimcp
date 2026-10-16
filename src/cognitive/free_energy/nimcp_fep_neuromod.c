/**
 * @file nimcp_fep_neuromod.c
 * @brief Neuromodulator Precision Weighting for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of neuromodulator-based precision modulation
 * WHY:  Biological realism - neuromodulators control gain on prediction errors
 * HOW:  Model 4 neuromodulatory systems (ACh, NE, DA, 5-HT) that weight precision
 */

#include "cognitive/free_energy/nimcp_fep_neuromod.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(fep_neuromod, MESH_ADAPTER_CATEGORY_COGNITIVE)

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void update_precision_multiplier(fep_neuromod_system_t* sys) {
    float ach_level = sys->state.levels[FEP_NEUROMOD_ACH];
    float ne_level = sys->state.levels[FEP_NEUROMOD_NE];

    /* Precision multiplier: Π = (1 + ACh*gain_ach) * (1 + NE*gain_ne) */
    float pi_ach = 1.0f + ach_level * sys->config.precision_gain_ach;
    float pi_ne = 1.0f + ne_level * sys->config.precision_gain_ne;
    sys->state.precision_multiplier = nimcp_clampf(
        pi_ach * pi_ne,
        FEP_NEUROMOD_MIN_PRECISION,
        FEP_NEUROMOD_MAX_PRECISION
    );
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int fep_neuromod_default_config(fep_neuromod_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_default_config", 0.0f);


    config->ach_baseline = FEP_NEUROMOD_DEFAULT_ACH_BASELINE;
    config->ne_baseline = FEP_NEUROMOD_DEFAULT_NE_BASELINE;
    config->da_baseline = FEP_NEUROMOD_DEFAULT_DA_BASELINE;
    config->serotonin_baseline = FEP_NEUROMOD_DEFAULT_5HT_BASELINE;

    config->ach_decay_rate = FEP_NEUROMOD_DEFAULT_ACH_DECAY;
    config->ne_decay_rate = FEP_NEUROMOD_DEFAULT_NE_DECAY;
    config->da_decay_rate = FEP_NEUROMOD_DEFAULT_DA_DECAY;
    config->serotonin_decay_rate = FEP_NEUROMOD_DEFAULT_5HT_DECAY;

    config->precision_gain_ach = FEP_NEUROMOD_DEFAULT_PRECISION_GAIN_ACH;
    config->precision_gain_ne = FEP_NEUROMOD_DEFAULT_PRECISION_GAIN_NE;
    config->exploration_rate_da = FEP_NEUROMOD_DEFAULT_EXPLORATION_RATE;
    config->temporal_discount_5ht = FEP_NEUROMOD_DEFAULT_TEMPORAL_DISCOUNT;

    config->enable_adaptive_gain = false;

    return 0;
}

fep_neuromod_system_t* fep_neuromod_create(const fep_neuromod_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_create", 0.0f);


    fep_neuromod_system_t* sys = (fep_neuromod_system_t*)nimcp_calloc(
        1, sizeof(fep_neuromod_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate neuromod system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sys");

        return NULL;
    }

    /* Apply configuration */
    fep_neuromod_config_t default_cfg;
    if (!config) {
        fep_neuromod_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Initialize neuromodulator levels to baselines */
    sys->state.levels[FEP_NEUROMOD_ACH] = config->ach_baseline;
    sys->state.levels[FEP_NEUROMOD_NE] = config->ne_baseline;
    sys->state.levels[FEP_NEUROMOD_DA] = config->da_baseline;
    sys->state.levels[FEP_NEUROMOD_5HT] = config->serotonin_baseline;

    sys->state.precision_multiplier = 1.0f;
    sys->state.exploration_bonus = 0.0f;
    sys->state.temporal_horizon = 1.0f;

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_neuromod_create: sys->mutex is NULL");
        return NULL;
    }

    sys->last_update_ms = get_time_ms();

    NIMCP_LOGGING_INFO("Neuromod system created");
    return sys;
}

void fep_neuromod_destroy(fep_neuromod_system_t* sys) {
    if (!sys) return;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_destroy", 0.0f);


    if (sys->bio_async_enabled) {
        fep_neuromod_disconnect_bio_async(sys);
    }

    if (sys->mutex) {
        nimcp_platform_mutex_destroy(sys->mutex);
        nimcp_free(sys->mutex);
        sys->mutex = NULL;
    }

    nimcp_free(sys);
    sys = NULL;
    NIMCP_LOGGING_INFO("Neuromod system destroyed");
}

/* ============================================================================
 * Neuromodulator Dynamics Implementation
 * ============================================================================ */

int fep_neuromod_update(fep_neuromod_system_t* sys, uint64_t delta_ms) {
    if (!sys) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_update", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    float dt = (float)delta_ms / 1000.0f;  /* Convert to seconds */

    /* Decay each neuromodulator toward baseline */
    /* level(t+1) = baseline + (level(t) - baseline) * exp(-decay * dt) */
    float ach = sys->state.levels[FEP_NEUROMOD_ACH];
    float ne = sys->state.levels[FEP_NEUROMOD_NE];
    float da = sys->state.levels[FEP_NEUROMOD_DA];
    float serotonin = sys->state.levels[FEP_NEUROMOD_5HT];

    float ach_decay = expf(-sys->config.ach_decay_rate * dt);
    float ne_decay = expf(-sys->config.ne_decay_rate * dt);
    float da_decay = expf(-sys->config.da_decay_rate * dt);
    float serotonin_decay = expf(-sys->config.serotonin_decay_rate * dt);

    sys->state.levels[FEP_NEUROMOD_ACH] =
        sys->config.ach_baseline + (ach - sys->config.ach_baseline) * ach_decay;
    sys->state.levels[FEP_NEUROMOD_NE] =
        sys->config.ne_baseline + (ne - sys->config.ne_baseline) * ne_decay;
    sys->state.levels[FEP_NEUROMOD_DA] =
        sys->config.da_baseline + (da - sys->config.da_baseline) * da_decay;
    sys->state.levels[FEP_NEUROMOD_5HT] =
        sys->config.serotonin_baseline + (serotonin - sys->config.serotonin_baseline) * serotonin_decay;

    /* Update computed effects */
    float da_level = sys->state.levels[FEP_NEUROMOD_DA];
    float serotonin_level = sys->state.levels[FEP_NEUROMOD_5HT];

    /* Update precision multiplier (ACh, NE) */
    update_precision_multiplier(sys);

    /* Exploration bonus from DA */
    sys->state.exploration_bonus = da_level * sys->config.exploration_rate_da;

    /* Temporal horizon from 5-HT */
    sys->state.temporal_horizon = 1.0f + serotonin_level * sys->config.temporal_discount_5ht;

    sys->total_updates++;
    sys->last_update_ms = get_time_ms();

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_neuromod_release(
    fep_neuromod_system_t* sys,
    fep_neuromod_type_t type,
    float amount
) {
    if (!sys) {
        return -1;
    }
    if (type >= FEP_NEUROMOD_COUNT) return -1;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_release", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    sys->state.levels[type] = nimcp_clampf(
        sys->state.levels[type] + amount,
        FEP_NEUROMOD_MIN_LEVEL,
        FEP_NEUROMOD_MAX_LEVEL
    );

    /* Update precision multiplier if ACh or NE changed */
    if (type == FEP_NEUROMOD_ACH || type == FEP_NEUROMOD_NE) {
        update_precision_multiplier(sys);
    }

    /* Update release statistics */
    switch (type) {
        case FEP_NEUROMOD_ACH: sys->ach_releases++; break;
        case FEP_NEUROMOD_NE: sys->ne_releases++; break;
        case FEP_NEUROMOD_DA: sys->da_releases++; break;
        case FEP_NEUROMOD_5HT: sys->serotonin_releases++; break;
        default: break;
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_neuromod_set_level(
    fep_neuromod_system_t* sys,
    fep_neuromod_type_t type,
    float level
) {
    if (!sys) {
        return -1;
    }
    if (type >= FEP_NEUROMOD_COUNT) return -1;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_set_level", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);
    sys->state.levels[type] = nimcp_clampf(level, FEP_NEUROMOD_MIN_LEVEL, FEP_NEUROMOD_MAX_LEVEL);

    /* Update precision multiplier if ACh or NE changed */
    if (type == FEP_NEUROMOD_ACH || type == FEP_NEUROMOD_NE) {
        update_precision_multiplier(sys);
    }

    nimcp_platform_mutex_unlock(sys->mutex);

    return 0;
}

float fep_neuromod_get_level(
    const fep_neuromod_system_t* sys,
    fep_neuromod_type_t type
) {
    if (!sys) return -1.0f;
    if (type >= FEP_NEUROMOD_COUNT) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_get_level", 0.0f);

    nimcp_platform_mutex_lock(((fep_neuromod_system_t*)sys)->mutex);
    float level = sys->state.levels[type];
    nimcp_platform_mutex_unlock(((fep_neuromod_system_t*)sys)->mutex);
    return level;
}

/* ============================================================================
 * Precision Modulation Implementation
 * ============================================================================ */

float fep_neuromod_compute_precision(
    fep_neuromod_system_t* sys,
    float base_precision
) {
    if (!sys) return base_precision;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_compute_precision", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Apply neuromodulator effects */
    float modulated = base_precision * sys->state.precision_multiplier;

    nimcp_platform_mutex_unlock(sys->mutex);

    return nimcp_clampf(modulated, FEP_NEUROMOD_MIN_PRECISION, FEP_NEUROMOD_MAX_PRECISION);
}

int fep_neuromod_apply_to_fep(
    fep_neuromod_system_t* neuromod,
    fep_system_t* fep
) {
    if (!neuromod || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_neuromod_apply_to_fep: required parameter is NULL (neuromod, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_apply_to_fep", 0.0f);


    nimcp_platform_mutex_lock(neuromod->mutex);

    /* Apply precision modulation to FEP levels */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            fep_neuromod_heartbeat("fep_neuromod_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];

        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                fep_neuromod_heartbeat("fep_neuromod_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            float base_precision = level->errors.precision[i];
            float modulated = base_precision * neuromod->state.precision_multiplier;
            level->errors.precision[i] = nimcp_clampf(
                modulated,
                FEP_NEUROMOD_MIN_PRECISION,
                FEP_NEUROMOD_MAX_PRECISION
            );
        }
    }

    nimcp_platform_mutex_unlock(neuromod->mutex);
    return 0;
}

/* ============================================================================
 * Event-Driven Release Implementation
 * ============================================================================ */

int fep_neuromod_on_prediction_error(
    fep_neuromod_system_t* sys,
    float error_magnitude
) {
    if (!sys) {
        return -1;
    }

    /* Large errors indicate unreliable predictions → decrease ACh */
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_on_prediction_error", 0.0f);


    float normalized_error = nimcp_clampf(error_magnitude / 10.0f, 0.0f, 1.0f);
    float ach_change = -0.2f * normalized_error;

    return fep_neuromod_release(sys, FEP_NEUROMOD_ACH, ach_change);
}

int fep_neuromod_on_surprise(
    fep_neuromod_system_t* sys,
    float surprise
) {
    if (!sys) {
        return -1;
    }

    /* Surprise → NE release */
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_on_surprise", 0.0f);


    float normalized_surprise = nimcp_clampf(surprise / 10.0f, 0.0f, 0.5f);
    return fep_neuromod_release(sys, FEP_NEUROMOD_NE, normalized_surprise);
}

int fep_neuromod_on_reward(
    fep_neuromod_system_t* sys,
    float reward
) {
    if (!sys) {
        return -1;
    }

    /* Reward prediction error → DA release (can be negative) */
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_on_reward", 0.0f);


    float da_change = nimcp_clampf(reward * 0.3f, -0.3f, 0.3f);
    return fep_neuromod_release(sys, FEP_NEUROMOD_DA, da_change);
}

int fep_neuromod_on_uncertainty(
    fep_neuromod_system_t* sys,
    float uncertainty
) {
    if (!sys) {
        return -1;
    }

    /* High uncertainty → decrease ACh (predictions unreliable) */
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_on_uncertainty", 0.0f);


    float ach_change = 0.2f * (1.0f - uncertainty);
    return fep_neuromod_release(sys, FEP_NEUROMOD_ACH, ach_change - 0.1f);
}

/* ============================================================================
 * State Query Implementation
 * ============================================================================ */

int fep_neuromod_get_state(
    const fep_neuromod_system_t* sys,
    fep_neuromod_state_t* state
) {
    if (!sys || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_neuromod_get_state: required parameter is NULL (sys, state)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_get_state", 0.0f);

    nimcp_platform_mutex_lock(((fep_neuromod_system_t*)sys)->mutex);
    *state = sys->state;
    nimcp_platform_mutex_unlock(((fep_neuromod_system_t*)sys)->mutex);

    return 0;
}

/* ============================================================================
 * FEP Integration Implementation
 * ============================================================================ */

int fep_neuromod_connect(
    fep_neuromod_system_t* neuromod,
    fep_system_t* fep
) {
    if (!neuromod || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_neuromod_connect: required parameter is NULL (neuromod, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_connect", 0.0f);


    nimcp_platform_mutex_lock(neuromod->mutex);
    neuromod->fep_system = fep;
    nimcp_platform_mutex_unlock(neuromod->mutex);

    NIMCP_LOGGING_INFO("Neuromod connected to FEP system");
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int fep_neuromod_connect_bio_async(fep_neuromod_system_t* sys) {
    if (!sys) {
        return -1;
    }
    if (sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_NEUROMOD,
        .module_name = "fep_neuromod",
        .inbox_capacity = 32,
        .user_data = sys
    };

    sys->bio_ctx = bio_router_register_module(&info);
    if (sys->bio_ctx) {
        sys->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Neuromod connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int fep_neuromod_disconnect_bio_async(fep_neuromod_system_t* sys) {
    if (!sys) {
        return -1;
    }
    if (!sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_disconnect_bio_async", 0.0f);


    if (sys->bio_ctx) {
        bio_router_unregister_module(sys->bio_ctx);
        sys->bio_ctx = NULL;
    }
    sys->bio_async_enabled = false;
    return 0;
}

bool fep_neuromod_is_bio_async_connected(const fep_neuromod_system_t* sys) {
    if (!sys) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_is_bio_async_connect", 0.0f);


    return sys->bio_async_enabled;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* fep_neuromod_type_to_string(fep_neuromod_type_t type) {
    switch (type) {
        case FEP_NEUROMOD_ACH:  return "ACH";
        case FEP_NEUROMOD_NE:   return "NE";
        case FEP_NEUROMOD_DA:   return "DA";
        case FEP_NEUROMOD_5HT:  return "5HT";
        default:            return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for FEP Neuromodulator self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int fep_neuromod_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    fep_neuromod_heartbeat("fep_neuromod_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Neuromodulation");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_neuromod_heartbeat("fep_neuromod_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP Neuromod self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Neuromodulation");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Neuromodulation");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fep_neuromod_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fep_neuromod_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fep_neuromod_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_neuromod_training_begin: NULL argument");
        return -1;
    }
    fep_neuromod_heartbeat_instance(g_fep_neuromod_health_agent, "fep_neuromod_training_begin", 0.0f);
    return 0;
}

int fep_neuromod_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_neuromod_training_end: NULL argument");
        return -1;
    }
    fep_neuromod_heartbeat_instance(g_fep_neuromod_health_agent, "fep_neuromod_training_end", 1.0f);
    return 0;
}

int fep_neuromod_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_neuromod_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fep_neuromod_heartbeat_instance(g_fep_neuromod_health_agent, "fep_neuromod_training_step", progress);
    return 0;
}
