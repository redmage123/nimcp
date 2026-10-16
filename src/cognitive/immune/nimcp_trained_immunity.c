/**
 * @file nimcp_trained_immunity.c
 * @brief Trained Immunity (Innate Memory) System Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Implementation of epigenetic-like reprogramming and metabolic shift
 * for enhanced innate immune responses.
 */

#include "cognitive/immune/nimcp_trained_immunity.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(trained_immunity, MESH_ADAPTER_CATEGORY_SECURITY)



/* Platform mutex compatibility macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for decay calculations
 * WHY:  Track training duration and decay
 * HOW:  Use system clock
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Get enhancement factor for stimulus type
 *
 * WHAT: Map stimulus to peak enhancement
 * WHY:  Different stimuli have different potency
 * HOW:  Lookup table by type
 */
static float get_stimulus_enhancement_factor(training_stimulus_t stimulus) {
    switch (stimulus) {
        case TRAINED_STIMULUS_BCG:           return TRAINED_BCG_ENHANCEMENT_PEAK;
        case TRAINED_STIMULUS_BETA_GLUCAN:   return TRAINED_BETA_GLUCAN_ENHANCEMENT;
        case TRAINED_STIMULUS_LPS_LOW_DOSE:  return TRAINED_LPS_ENHANCEMENT;
        case TRAINED_STIMULUS_OXIDIZED_LDL:  return TRAINED_OXLDL_ENHANCEMENT;
        default:                             return 1.0f;
    }
}

/**
 * @brief Get half-life for stimulus type
 *
 * WHAT: Get decay half-life in milliseconds
 * WHY:  Different stimuli decay at different rates
 * HOW:  Lookup table by type
 */
static uint64_t get_stimulus_half_life(training_stimulus_t stimulus) {
    switch (stimulus) {
        case TRAINED_STIMULUS_BCG:           return TRAINED_BCG_HALF_LIFE;
        case TRAINED_STIMULUS_BETA_GLUCAN:   return TRAINED_BETA_GLUCAN_HALF_LIFE;
        case TRAINED_STIMULUS_LPS_LOW_DOSE:  return TRAINED_LPS_HALF_LIFE;
        case TRAINED_STIMULUS_OXIDIZED_LDL:  return TRAINED_OXLDL_HALF_LIFE;
        default:                             return TRAINED_BETA_GLUCAN_HALF_LIFE;
    }
}

/**
 * @brief Calculate exponential decay
 *
 * WHAT: Compute decay factor based on elapsed time
 * WHY:  Trained immunity decays exponentially
 * HOW:  Use half-life formula: factor = initial * (0.5)^(elapsed/half_life)
 */
static float calculate_decay_factor(
    float initial_factor,
    uint64_t elapsed_ms,
    uint64_t half_life_ms
) {
    if (half_life_ms == 0) return initial_factor;

    double time_ratio = (double)elapsed_ms / (double)half_life_ms;
    double decay = pow(0.5, time_ratio);
    return (float)(initial_factor * decay);
}

/**
 * @brief Update metabolic state from enhancement
 *
 * WHAT: Update glycolysis/mTOR/HIF-1α based on training
 * WHY:  Metabolic reprogramming is core to trained immunity
 * HOW:  Enhancement drives metabolic shift
 */
static void update_metabolic_state(
    trained_immunity_t* system,
    float enhancement
) {
    if (!system) return;

    /* Glycolysis increases with training */
    system->metabolic.glycolysis_rate = (enhancement - 1.0f) / 2.0f;
    if (system->metabolic.glycolysis_rate > 1.0f) {
        system->metabolic.glycolysis_rate = 1.0f;
    }

    /* OXPHOS decreases inversely */
    system->metabolic.oxidative_phosphorylation = 1.0f - system->metabolic.glycolysis_rate * 0.5f;

    /* mTOR activation correlates with glycolysis */
    system->metabolic.mtor_activation = system->metabolic.glycolysis_rate * 0.9f;

    /* HIF-1α stabilization with training */
    system->metabolic.hif1a_level = system->metabolic.glycolysis_rate * 0.8f;

    /* Glucose consumption increases */
    system->metabolic.glucose_consumption = 0.5f + system->metabolic.glycolysis_rate * 0.5f;

    /* ATP production capacity increases */
    system->metabolic.atp_production_rate = 0.7f + system->metabolic.glycolysis_rate * 0.3f;

    /* Determine metabolic state */
    if (system->metabolic.glycolysis_rate > TRAINED_GLYCOLYSIS_THRESHOLD) {
        system->metabolic.state = METABOLIC_STATE_GLYCOLYTIC;
    } else if (system->metabolic.glycolysis_rate > 0.3f) {
        system->metabolic.state = METABOLIC_STATE_MIXED;
    } else {
        system->metabolic.state = METABOLIC_STATE_OXIDATIVE;
    }
}

/**
 * @brief Update epigenetic state from enhancement
 *
 * WHAT: Update histone marks and chromatin openness
 * WHY:  Epigenetic changes underlie trained immunity
 * HOW:  Enhancement drives chromatin remodeling
 */
static void update_epigenetic_state(
    trained_immunity_t* system,
    float enhancement
) {
    if (!system) return;

    float training_strength = (enhancement - 1.0f) / 2.0f;
    if (training_strength > 1.0f) training_strength = 1.0f;

    /* H3K4me3 (active promoter mark) */
    system->epigenetic.h3k4me3_level = 0.3f + training_strength * 0.7f;

    /* H3K27ac (active enhancer mark) */
    system->epigenetic.h3k27ac_level = 0.2f + training_strength * 0.8f;

    /* Chromatin openness */
    system->epigenetic.chromatin_openness = 0.4f + training_strength * 0.6f;

    /* Transcriptional readiness */
    system->epigenetic.transcriptional_readiness = 0.3f + training_strength * 0.7f;
}

/**
 * @brief Update PRR sensitivity from enhancement
 *
 * WHAT: Update pattern recognition receptor sensitivity
 * WHY:  Enhanced PRR is key mechanism of trained immunity
 * HOW:  Enhancement drives TLR/NOD upregulation
 */
static void update_prr_sensitivity(
    trained_immunity_t* system,
    float enhancement
) {
    if (!system) return;

    float base_enhancement = (enhancement - 1.0f) / 2.0f;
    if (base_enhancement > 1.0f) base_enhancement = 1.0f;

    /* TLR expression (1.0 to 2.5x) */
    system->prr.tlr_expression = 1.0f + base_enhancement * 1.5f;

    /* NOD-like receptor sensitivity */
    system->prr.nod_sensitivity = 1.0f + base_enhancement * 1.2f;

    /* Recognition speed (faster recognition) */
    system->prr.recognition_speed = base_enhancement;

    /* PAMP sensitivity */
    system->prr.pamp_sensitivity = 1.0f + base_enhancement * 1.3f;

    /* DAMP sensitivity */
    system->prr.damp_sensitivity = 1.0f + base_enhancement * 1.1f;

    /* Update overall PRR sensitivity factor */
    system->prr_sensitivity_factor = system->prr.tlr_expression;
}

/**
 * @brief Recalculate total enhancement from active training
 *
 * WHAT: Sum enhancement from all active training events
 * WHY:  Multiple training events can stack
 * HOW:  Additive enhancement with cap
 */
static float recalculate_total_enhancement(const trained_immunity_t* system) {
    if (!system) return 1.0f;

    float total_enhancement = 1.0f;
    for (size_t i = 0; i < system->training_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->training_count > 256) {
            trained_immunity_heartbeat("trained_immu_loop",
                             (float)(i + 1) / (float)system->training_count);
        }

        if (system->training_history[i].active) {
            total_enhancement += (system->training_history[i].current_enhancement - 1.0f);
        }
    }

    if (total_enhancement > TRAINED_BCG_ENHANCEMENT_PEAK) {
        total_enhancement = TRAINED_BCG_ENHANCEMENT_PEAK;
    }

    return total_enhancement;
}

/**
 * @brief Update cross-protection state
 *
 * WHAT: Calculate cross-protection status from training
 * WHY:  Training provides heterologous immunity
 * HOW:  Check training strength against threshold
 */
static void update_cross_protection(
    trained_immunity_t* system,
    float total_enhancement
) {
    if (!system) return;

    if (system->config.enable_cross_protection) {
        float training_strength = (total_enhancement - 1.0f) / (TRAINED_BCG_ENHANCEMENT_PEAK - 1.0f);
        system->cross_protection_active = training_strength >= TRAINED_CROSS_PROTECTION_THRESHOLD;
        system->cross_protection_level = training_strength;
    } else {
        system->cross_protection_active = false;
        system->cross_protection_level = 0.0f;
    }
}

/**
 * @brief Find slot for new training event
 *
 * WHAT: Find free or oldest slot in training history
 * WHY:  Limited history size requires slot management
 * HOW:  Use next free, or replace oldest inactive/oldest
 */
static size_t find_training_slot(trained_immunity_t* system) {
    size_t slot = system->training_count;

    if (slot >= TRAINED_IMMUNITY_MAX_HISTORY) {
        /* Find oldest inactive entry to replace */
        slot = 0;
        uint64_t oldest_time = system->training_history[0].training_time;

        for (size_t i = 1; i < TRAINED_IMMUNITY_MAX_HISTORY; i++) {
            if (!system->training_history[i].active) {
                return i;
            }
            if (system->training_history[i].training_time < oldest_time) {
                oldest_time = system->training_history[i].training_time;
                slot = i;
            }
        }
    } else {
        system->training_count++;
    }

    return slot;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int trained_immunity_default_config(trained_immunity_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trained_immunity_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_default_config", 0.0f);


    config->max_training_intensity = 1.0f;
    config->decay_rate_multiplier = 1.0f;
    config->enable_cross_protection = true;
    config->min_training_threshold = 0.1f;
    config->prr_sensitivity_multiplier = 1.0f;
    config->enable_bio_async = true;
    config->enable_logging = true;

    return 0;
}

trained_immunity_t* trained_immunity_create(
    const trained_immunity_config_t* config,
    brain_immune_system_t* immune_system
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Null immune system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_create", 0.0f);


    trained_immunity_t* system = (trained_immunity_t*)nimcp_calloc(1, sizeof(trained_immunity_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate trained immunity system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    /* Set configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(trained_immunity_config_t));
    } else {
        trained_immunity_default_config(&system->config);
    }

    /* Create mutex */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trained_immunity_create: system->mutex is NULL");
        return NULL;
    }

    /* Connect to immune system */
    system->immune_system = immune_system;

    /* Initialize baseline states */
    system->epigenetic.h3k4me3_level = 0.3f;
    system->epigenetic.h3k27ac_level = 0.2f;
    system->epigenetic.chromatin_openness = 0.4f;
    system->epigenetic.transcriptional_readiness = 0.3f;

    system->metabolic.state = METABOLIC_STATE_OXIDATIVE;
    system->metabolic.glycolysis_rate = 0.2f;
    system->metabolic.oxidative_phosphorylation = 0.9f;
    system->metabolic.mtor_activation = 0.1f;
    system->metabolic.hif1a_level = 0.1f;
    system->metabolic.glucose_consumption = 0.5f;
    system->metabolic.atp_production_rate = 0.7f;

    system->prr.tlr_expression = TRAINED_PRR_BASE_SENSITIVITY;
    system->prr.nod_sensitivity = TRAINED_PRR_BASE_SENSITIVITY;
    system->prr.recognition_speed = 0.0f;
    system->prr.pamp_sensitivity = TRAINED_PRR_BASE_SENSITIVITY;
    system->prr.damp_sensitivity = TRAINED_PRR_BASE_SENSITIVITY;

    system->current_enhancement_factor = 1.0f;
    system->prr_sensitivity_factor = TRAINED_PRR_BASE_SENSITIVITY;
    system->cross_protection_active = false;
    system->cross_protection_level = 0.0f;

    system->system_start_time = get_current_time_ms();
    system->last_decay_check = system->system_start_time;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Trained immunity system created");
    }

    return system;
}

void trained_immunity_destroy(trained_immunity_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_destroy", 0.0f);


    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying trained immunity system");
    }

    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);
    system = NULL;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int trained_immunity_train(
    trained_immunity_t* system,
    training_stimulus_t stimulus_type,
    float intensity
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    if (stimulus_type >= TRAINED_STIMULUS_COUNT) return -1;
    if (intensity <= 0.0f || intensity > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trained_immunity_train: validation failed");
        return -1;
    }

    /* Check minimum threshold */
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_train", 0.0f);


    if (intensity < system->config.min_training_threshold) {
        return 0;  /* Too weak to train */
    }

    nimcp_mutex_lock(system->mutex);

    /* Find slot and record event */
    size_t slot = find_training_slot(system);
    uint64_t current_time = get_current_time_ms();
    training_history_entry_t* entry = &system->training_history[slot];

    entry->stimulus_type = stimulus_type;
    entry->intensity = intensity * system->config.max_training_intensity;
    entry->training_time = current_time;
    entry->active = true;

    /* Calculate enhancement for this training */
    float peak_enhancement = get_stimulus_enhancement_factor(stimulus_type);
    entry->current_enhancement = 1.0f + (peak_enhancement - 1.0f) * entry->intensity;

    system->last_training_time = current_time;
    system->total_training_events++;

    /* Recalculate and update system state */
    float total_enhancement = recalculate_total_enhancement(system);
    system->current_enhancement_factor = total_enhancement;

    update_metabolic_state(system, total_enhancement);
    update_epigenetic_state(system, total_enhancement);
    update_prr_sensitivity(system, total_enhancement);
    update_cross_protection(system, total_enhancement);

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Trained immunity: stimulus=%s intensity=%.2f enhancement=%.2fx",
            trained_immunity_stimulus_to_string(stimulus_type),
            intensity,
            system->current_enhancement_factor);
    }

    return 0;
}

float trained_immunity_get_enhancement_factor(const trained_immunity_t* system) {
    if (!system) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_enhancement_fact", 0.0f);


    return system->current_enhancement_factor;
}

float trained_immunity_get_prr_sensitivity(const trained_immunity_t* system) {
    if (!system) return TRAINED_PRR_BASE_SENSITIVITY;
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_prr_sensitivity", 0.0f);


    return system->prr_sensitivity_factor;
}

/**
 * @brief Apply decay to training history entries
 *
 * WHAT: Calculate exponential decay for each training event
 * WHY:  Training effects fade over time
 * HOW:  Use half-life based exponential decay
 */
static void apply_decay_to_history(
    trained_immunity_t* system,
    uint64_t current_time
) {
    if (!system) return;

    for (size_t i = 0; i < system->training_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->training_count > 256) {
            trained_immunity_heartbeat("trained_immu_loop",
                             (float)(i + 1) / (float)system->training_count);
        }

        training_history_entry_t* entry = &system->training_history[i];
        if (!entry->active) continue;

        uint64_t elapsed = current_time - entry->training_time;
        uint64_t half_life = get_stimulus_half_life(entry->stimulus_type);
        half_life = (uint64_t)(half_life * system->config.decay_rate_multiplier);

        /* Calculate decay */
        float peak_enhancement = get_stimulus_enhancement_factor(entry->stimulus_type);
        float peak_factor = 1.0f + (peak_enhancement - 1.0f) * entry->intensity;
        float decayed = calculate_decay_factor(peak_factor, elapsed, half_life);

        entry->current_enhancement = decayed;

        /* Mark inactive if decayed below threshold */
        if (decayed < 1.05f) {  /* 5% above baseline */
            entry->active = false;
        }
    }
}

int trained_immunity_decay(
    trained_immunity_t* system,
    uint64_t current_time
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_decay", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Check if decay interval has passed */
    uint64_t elapsed_since_check = current_time - system->last_decay_check;
    if (elapsed_since_check < TRAINED_IMMUNITY_DECAY_INTERVAL) {
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    system->last_decay_check = current_time;

    /* Apply decay and recalculate */
    apply_decay_to_history(system, current_time);

    float total_enhancement = recalculate_total_enhancement(system);
    system->current_enhancement_factor = total_enhancement;

    /* Update derived states */
    update_metabolic_state(system, total_enhancement);
    update_epigenetic_state(system, total_enhancement);
    update_prr_sensitivity(system, total_enhancement);
    update_cross_protection(system, total_enhancement);

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

bool trained_immunity_check_cross_protection(
    const trained_immunity_t* system,
    const brain_antigen_t* antigen
) {
    if (!system || !antigen) {
        return false;
    }
    if (!system->config.enable_cross_protection) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_check_cross_protecti", 0.0f);


    return system->cross_protection_active;
}

metabolic_state_t trained_immunity_get_metabolic_state(
    const trained_immunity_t* system
) {
    if (!system) return METABOLIC_STATE_OXIDATIVE;
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_metabolic_state", 0.0f);


    return system->metabolic.state;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int trained_immunity_get_epigenetic_state(
    const trained_immunity_t* system,
    epigenetic_state_t* state
) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trained_immunity_get_epigenetic_state: required parameter is NULL (system, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_epigenetic_state", 0.0f);


    memcpy(state, &system->epigenetic, sizeof(epigenetic_state_t));
    return 0;
}

int trained_immunity_get_metabolic_reprogramming(
    const trained_immunity_t* system,
    metabolic_reprogramming_t* state
) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trained_immunity_get_metabolic_reprogramming: required parameter is NULL (system, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_metabolic_reprog", 0.0f);


    memcpy(state, &system->metabolic, sizeof(metabolic_reprogramming_t));
    return 0;
}

int trained_immunity_get_prr_state(
    const trained_immunity_t* system,
    prr_sensitivity_t* state
) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trained_immunity_get_prr_state: required parameter is NULL (system, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_prr_state", 0.0f);


    memcpy(state, &system->prr, sizeof(prr_sensitivity_t));
    return 0;
}

bool trained_immunity_is_active(const trained_immunity_t* system) {
    if (!system) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_is_active", 0.0f);


    return system->current_enhancement_factor > 1.05f;
}

size_t trained_immunity_get_history_count(const trained_immunity_t* system) {
    if (!system) return 0;
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_get_history_count", 0.0f);


    return system->training_count;
}

uint64_t trained_immunity_time_since_training(
    const trained_immunity_t* system,
    uint64_t current_time
) {
    if (!system) return 0;
    if (system->last_training_time == 0) return UINT64_MAX;
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_time_since_training", 0.0f);


    return current_time - system->last_training_time;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* trained_immunity_stimulus_to_string(training_stimulus_t stimulus) {
    switch (stimulus) {
        case TRAINED_STIMULUS_NONE:         return "NONE";
        case TRAINED_STIMULUS_BCG:          return "BCG";
        case TRAINED_STIMULUS_BETA_GLUCAN:  return "BETA_GLUCAN";
        case TRAINED_STIMULUS_LPS_LOW_DOSE: return "LPS_LOW_DOSE";
        case TRAINED_STIMULUS_OXIDIZED_LDL: return "OXIDIZED_LDL";
        default:                            return "UNKNOWN";
    }
}

const char* trained_immunity_metabolic_state_to_string(metabolic_state_t state) {
    switch (state) {
        case METABOLIC_STATE_OXIDATIVE:  return "OXIDATIVE";
        case METABOLIC_STATE_MIXED:      return "MIXED";
        case METABOLIC_STATE_GLYCOLYTIC: return "GLYCOLYTIC";
        default:                         return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about trained immunity
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int trained_immunity_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    trained_immunity_heartbeat("trained_immu_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Trained_Immunity");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                trained_immunity_heartbeat("trained_immu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Trained immunity self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Trained_Immunity");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Trained_Immunity");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void trained_immunity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_trained_immunity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int trained_immunity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "trained_immunity_training_begin: NULL argument");
        return -1;
    }
    trained_immunity_heartbeat_instance(NULL, "trained_immunity_training_begin", 0.0f);
    return 0;
}

int trained_immunity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "trained_immunity_training_end: NULL argument");
        return -1;
    }
    trained_immunity_heartbeat_instance(NULL, "trained_immunity_training_end", 1.0f);
    return 0;
}

int trained_immunity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "trained_immunity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    trained_immunity_heartbeat_instance(NULL, "trained_immunity_training_step", progress);
    return 0;
}
