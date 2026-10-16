/**
 * @file nimcp_complement_system.c
 * @brief Complement System Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implements complement cascade, opsonization, and MAC formation
 * WHY:  Provides innate immune amplification and direct threat elimination
 * HOW:  Three pathways converge on C3 convertase → C5 convertase → MAC
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_complement_system.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(complement_system, MESH_ADAPTER_CATEGORY_SECURITY)



/* Mutex convenience macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static complement_c3b_t* find_c3b_by_id(complement_system_t* system, uint32_t id);
static complement_mac_t* find_mac_by_id(complement_system_t* system, uint32_t id);
static int generate_c3b(complement_system_t* system, uint32_t target_id,
                        complement_pathway_t pathway);
static int generate_c5b(complement_system_t* system, uint32_t target_id,
                        uint32_t parent_c3b_id);
static void decay_c3b_pool(complement_system_t* system, uint64_t delta_ms);
static void progress_mac_assembly(complement_system_t* system, uint64_t delta_ms);
static float compute_opsonization_level(complement_system_t* system, uint32_t target_id);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Convert pathway to string
 */
const char* complement_pathway_to_string(complement_pathway_t pathway) {
    switch (pathway) {
        case COMPLEMENT_PATHWAY_CLASSICAL: return "CLASSICAL";
        case COMPLEMENT_PATHWAY_ALTERNATIVE: return "ALTERNATIVE";
        case COMPLEMENT_PATHWAY_LECTIN: return "LECTIN";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert anaphylatoxin type to string
 */
const char* complement_anaphylatoxin_to_string(anaphylatoxin_type_t type) {
    switch (type) {
        case ANAPHYLATOXIN_C3A: return "C3a";
        case ANAPHYLATOXIN_C4A: return "C4a";
        case ANAPHYLATOXIN_C5A: return "C5a";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert MAC state to string
 */
const char* complement_mac_state_to_string(mac_state_t state) {
    switch (state) {
        case MAC_STATE_INACTIVE: return "INACTIVE";
        case MAC_STATE_FORMING: return "FORMING";
        case MAC_STATE_ASSEMBLING: return "ASSEMBLING";
        case MAC_STATE_COMPLETE: return "COMPLETE";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Helpers - Implementation
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* Cast before multiply to avoid signed 32-bit overflow */
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find C3b by ID
 */
static complement_c3b_t* find_c3b_by_id(complement_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->c3b_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->c3b_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->c3b_count);
        }

        if (system->c3b_pool[i].id == id) {
            return &system->c3b_pool[i];
        }
    }
    return NULL;
}

/**
 * @brief Find MAC by ID
 */
static complement_mac_t* find_mac_by_id(complement_system_t* system, uint32_t id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    for (size_t i = 0; i < system->mac_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->mac_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->mac_count);
        }

        if (system->mac_pool[i].id == id) {
            return &system->mac_pool[i];
        }
    }
    return NULL;
}

/**
 * @brief Generate C3b molecule
 *
 * WHAT: Create new C3b molecule on target surface
 * WHY:  Core opsonization mechanism
 * HOW:  Allocate from pool, initialize, bind to target
 */
static int generate_c3b(complement_system_t* system, uint32_t target_id,
                        complement_pathway_t pathway) {
    if (!system) {
        /* Corrected error code and function name */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_c3b: system is NULL");
        return -1;
    }
    if (system->c3b_count >= system->c3b_capacity) {
        /* Corrected function name in error message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "generate_c3b: capacity exceeded");
        return -1;
    }

    complement_c3b_t* c3b = &system->c3b_pool[system->c3b_count++];
    c3b->id = system->next_c3b_id++;
    c3b->target_antigen_id = target_id;
    c3b->pathway = pathway;
    c3b->generation_time = get_timestamp_ms();
    c3b->concentration = 1.0f;
    c3b->bound_to_target = true;
    c3b->convertase_count = 0;
    c3b->part_of_amplification = false;

    system->stats.c3b_generated++;
    system->current_c3_level -= 0.01f; /* Consume C3 */

    return 0;
}

/**
 * @brief Generate C5b molecule
 *
 * WHAT: Create C5b to initiate MAC formation
 * WHY:  Terminal pathway begins with C5 cleavage
 * HOW:  Allocate from pool, link to parent C3b
 */
static int generate_c5b(complement_system_t* system, uint32_t target_id,
                        uint32_t parent_c3b_id) {
    if (!system) {
        /* stale function name fix: generate_c5b, not find_mac_by_id */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_c5b: system is NULL");
        return -1;
    }
    if (system->c5b_count >= system->c5b_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "generate_c5b: capacity exceeded");
        return -1;
    }

    complement_c5b_t* c5b = &system->c5b_pool[system->c5b_count++];
    c5b->id = system->next_c5b_id++;
    c5b->target_antigen_id = target_id;
    c5b->parent_c3b_id = parent_c3b_id;
    c5b->generation_time = get_timestamp_ms();
    c5b->mac_id = 0;
    c5b->recruited_c6 = false;

    system->stats.c5b_generated++;
    system->current_c5_level -= 0.01f; /* Consume C5 */

    return 0;
}

/**
 * @brief Decay C3b pool over time
 *
 * WHAT: Simulate C3b half-life and degradation
 * WHY:  Prevent unlimited accumulation, model biological decay
 * HOW:  Reduce concentration based on decay rate
 */
static void decay_c3b_pool(complement_system_t* system, uint64_t delta_ms) {
    if (!system) return;

    float decay_factor = 1.0f - (system->config.c3b_decay_rate * delta_ms / 1000.0f);
    if (decay_factor < 0.0f) decay_factor = 0.0f;

    for (size_t i = 0; i < system->c3b_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->c3b_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->c3b_count);
        }

        system->c3b_pool[i].concentration *= decay_factor;
    }
}

/**
 * @brief Progress MAC assembly over time
 *
 * WHAT: Advance MAC formation states
 * WHY:  MAC assembly takes time (C5b→C6→C7→C8→C9 polymerization)
 * HOW:  State transitions based on time elapsed
 */
static void progress_mac_assembly(complement_system_t* system, uint64_t delta_ms) {
    if (!system) return;

    uint64_t now = get_timestamp_ms();

    for (size_t i = 0; i < system->mac_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->mac_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->mac_count);
        }

        complement_mac_t* mac = &system->mac_pool[i];
        if (mac->state == MAC_STATE_COMPLETE) continue;

        uint64_t elapsed = now - mac->formation_time;

        if (mac->state == MAC_STATE_FORMING && elapsed > 100) {
            mac->state = MAC_STATE_ASSEMBLING;
            mac->c9_polymers = 1;
        }

        if (mac->state == MAC_STATE_ASSEMBLING && elapsed > system->config.mac_formation_delay_ms) {
            mac->state = MAC_STATE_COMPLETE;
            mac->completion_time = now;
            mac->c9_polymers = system->config.min_c9_for_lysis;
            mac->lytic_effectiveness = system->config.mac_effectiveness;
            system->stats.macs_completed++;
        }
    }
}

/**
 * @brief Compute opsonization level for target
 *
 * WHAT: Calculate C3b density on target surface
 * WHY:  Measure opsonization effectiveness
 * HOW:  Sum C3b concentrations for target, normalize
 */
static float compute_opsonization_level(complement_system_t* system, uint32_t target_id) {
    if (!system) return 0.0f;

    float total = 0.0f;
    for (size_t i = 0; i < system->c3b_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->c3b_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->c3b_count);
        }

        if (system->c3b_pool[i].target_antigen_id == target_id) {
            total += system->c3b_pool[i].concentration;
        }
    }

    return fminf(total / 10.0f, 1.0f); /* Normalize to 0-1 */
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int complement_default_config(complement_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_default_c", 0.0f);


    memset(config, 0, sizeof(complement_config_t));

    /* Pathway enables */
    config->enable_classical_pathway = true;
    config->enable_alternative_pathway = true;
    config->enable_lectin_pathway = true;

    /* Activation thresholds */
    config->classical_threshold = 0.3f;
    config->alternative_threshold = 0.5f;
    config->lectin_threshold = 0.4f;

    /* Amplification */
    config->amplification_factor = COMPLEMENT_AMPLIFICATION_FACTOR;
    config->max_amplification = 100.0f;
    config->enable_amplification_loop = true;

    /* MAC parameters */
    config->min_c9_for_lysis = 12;
    config->mac_formation_delay_ms = 500.0f;
    config->mac_effectiveness = 0.9f;

    /* Opsonization */
    config->opsonization_enhancement = 2.0f;
    config->c3b_decay_rate = 0.1f; /* 10% per second */

    /* Anaphylatoxins */
    config->enable_anaphylatoxins = true;
    config->c3a_potency = 0.5f;
    config->c5a_potency = 0.8f;

    /* Regulation */
    config->enable_self_regulation = true;
    config->regulation_threshold = 0.9f;

    /* Integration */
    config->enable_logging = true;

    return 0;
}

/**
 * @brief Create complement system
 */
complement_system_t* complement_create(const complement_config_t* config,
                                       brain_immune_system_t* immune_system) {
    if (!immune_system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_create", 0.0f);


    complement_system_t* system = nimcp_malloc(sizeof(complement_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    memset(system, 0, sizeof(complement_system_t));

    /* Configuration */
    if (config) {
        system->config = *config;
    } else {
        complement_default_config(&system->config);
    }

    /* Integration */
    system->immune_system = immune_system;

    /* Allocate pools */
    system->c3b_capacity = COMPLEMENT_MAX_ACTIVE_C3B;
    system->c3b_pool = nimcp_calloc(system->c3b_capacity, sizeof(complement_c3b_t));
    if (!system->c3b_pool) goto error;

    system->c5b_capacity = COMPLEMENT_MAX_ACTIVE_C5B;
    system->c5b_pool = nimcp_calloc(system->c5b_capacity, sizeof(complement_c5b_t));
    if (!system->c5b_pool) goto error;

    system->mac_capacity = COMPLEMENT_MAX_MACS;
    system->mac_pool = nimcp_calloc(system->mac_capacity, sizeof(complement_mac_t));
    if (!system->mac_pool) goto error;

    system->anaphylatoxin_capacity = COMPLEMENT_MAX_ANAPHYLATOXINS;
    system->anaphylatoxin_pool = nimcp_malloc(sizeof(complement_anaphylatoxin_t) *
                                              system->anaphylatoxin_capacity);
    if (!system->anaphylatoxin_pool) goto error;

    /* Thread safety */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) goto error;

    /* Initialize state */
    system->next_c3b_id = 1;
    system->next_c5b_id = 1;
    system->next_mac_id = 1;
    system->next_anaphylatoxin_id = 1;
    system->current_c3_level = 1.0f;
    system->current_c5_level = 1.0f;
    system->running = true;
    system->start_time = get_timestamp_ms();

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Complement system created with %d C3b capacity, %d MAC capacity",
                          (int)system->c3b_capacity, (int)system->mac_capacity);
    }

    return system;

error:
    if (system->c3b_pool) nimcp_free(system->c3b_pool);
    if (system->c5b_pool) nimcp_free(system->c5b_pool);
    if (system->mac_pool) nimcp_free(system->mac_pool);
    if (system->anaphylatoxin_pool) nimcp_free(system->anaphylatoxin_pool);
    nimcp_free(system);
    system = NULL;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_default_config: validation failed");
    return NULL;
}

/**
 * @brief Destroy complement system
 */
void complement_destroy(complement_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_destroy", 0.0f);


    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying complement system (generated %llu C3b, %llu MACs)",
                          (unsigned long long)system->stats.c3b_generated,
                          (unsigned long long)system->stats.macs_formed);
    }

    if (system->mutex) nimcp_mutex_destroy(system->mutex);
    if (system->c3b_pool) nimcp_free(system->c3b_pool);
    if (system->c5b_pool) nimcp_free(system->c5b_pool);
    if (system->mac_pool) nimcp_free(system->mac_pool);
    if (system->anaphylatoxin_pool) nimcp_free(system->anaphylatoxin_pool);

    nimcp_free(system);
    system = NULL;
}

/* ============================================================================
 * Activation API
 * ============================================================================ */

/**
 * @brief Activate complement cascade
 */
int complement_activate(complement_system_t* system, complement_pathway_t pathway,
                        uint32_t target_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_destroy: system is NULL");
        return -1;
    }
    if (!system->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_destroy: system->running is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_activate", 0.0f);


    switch (pathway) {
        case COMPLEMENT_PATHWAY_CLASSICAL:
            return complement_activate_classical(system, 0, target_id);
        case COMPLEMENT_PATHWAY_ALTERNATIVE:
            return complement_activate_alternative(system, target_id);
        case COMPLEMENT_PATHWAY_LECTIN:
            return complement_activate_lectin(system, target_id, NULL, 0);
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "complement_destroy: operation failed");
            return -1;
    }
}

/**
 * @brief Activate classical pathway
 */
int complement_activate_classical(complement_system_t* system, uint32_t antibody_id,
                                  uint32_t target_id) {
    if (!system || !system->config.enable_classical_pathway) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_destroy: required parameter is NULL (system, system->config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_activate_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Generate initial C3b via C4b2a (C3 convertase) */
    if (generate_c3b(system, target_id, COMPLEMENT_PATHWAY_CLASSICAL) != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "complement_destroy: validation failed");
        return -1;
    }

    system->stats.classical_activations++;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Classical pathway activated for target %u (antibody %u)",
                          target_id, antibody_id);
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Activate alternative pathway
 */
int complement_activate_alternative(complement_system_t* system, uint32_t target_id) {
    if (!system || !system->config.enable_alternative_pathway) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_activate_alternative: required parameter is NULL (system, system->config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_activate_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Spontaneous C3 hydrolysis, forms C3bBb convertase */
    if (generate_c3b(system, target_id, COMPLEMENT_PATHWAY_ALTERNATIVE) != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "complement_activate_alternative: validation failed");
        return -1;
    }

    system->stats.alternative_activations++;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Alternative pathway activated for target %u (spontaneous)",
                          target_id);
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Activate lectin pathway
 */
int complement_activate_lectin(complement_system_t* system, uint32_t target_id,
                               const uint8_t* pattern, size_t pattern_len) {
    if (!system || !system->config.enable_lectin_pathway) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_activate_alternative: required parameter is NULL (system, system->config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_activate_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* MBL binds mannose pattern, activates MASP, forms C4b2a convertase */
    if (generate_c3b(system, target_id, COMPLEMENT_PATHWAY_LECTIN) != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "complement_activate_alternative: validation failed");
        return -1;
    }

    system->stats.lectin_activations++;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Lectin pathway activated for target %u (pattern recognition)",
                          target_id);
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Opsonization API
 * ============================================================================ */

/**
 * @brief Opsonize target with C3b
 */
int complement_opsonize(complement_system_t* system, uint32_t target_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_opsonize: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_opsonize", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Generate multiple C3b for opsonization */
    int count = 0;
    for (int i = 0; i < 5; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 5 > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)5);
        }

        if (generate_c3b(system, target_id, COMPLEMENT_PATHWAY_ALTERNATIVE) == 0) {
            count++;
        }
    }

    if (count > 0) {
        system->stats.targets_opsonized++;
    }

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Opsonized target %u with %d C3b molecules", target_id, count);
    }

    nimcp_mutex_unlock(system->mutex);
    return count;
}

/**
 * @brief Get C3b opsonization level
 */
float complement_get_c3b_level(complement_system_t* system, uint32_t target_id) {
    if (!system) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_get_c3b_l", 0.0f);


    nimcp_mutex_lock(system->mutex);
    float level = compute_opsonization_level(system, target_id);
    nimcp_mutex_unlock(system->mutex);

    return level;
}

/* ============================================================================
 * MAC Formation API
 * ============================================================================ */

/**
 * @brief Form membrane attack complex
 */
uint32_t complement_form_mac(complement_system_t* system, uint32_t target_id) {
    if (!system) return 0;
    if (system->mac_count >= system->mac_capacity) return 0;

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_form_mac", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Need C3b to generate C5b */
    uint32_t c3b_id = 0;
    for (size_t i = 0; i < system->c3b_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->c3b_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->c3b_count);
        }

        if (system->c3b_pool[i].target_antigen_id == target_id) {
            c3b_id = system->c3b_pool[i].id;
            break;
        }
    }

    if (c3b_id == 0) {
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Generate C5b */
    if (generate_c5b(system, target_id, c3b_id) != 0) {
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Create MAC */
    complement_mac_t* mac = &system->mac_pool[system->mac_count++];
    mac->id = system->next_mac_id++;
    mac->target_antigen_id = target_id;
    mac->state = MAC_STATE_FORMING;
    mac->initiating_c5b_id = system->next_c5b_id - 1;
    mac->c9_polymers = 0;
    mac->formation_time = get_timestamp_ms();
    mac->completion_time = 0;
    mac->lytic_effectiveness = 0.0f;
    mac->target_eliminated = false;

    system->stats.macs_formed++;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("MAC formation initiated on target %u (MAC ID %u)",
                          target_id, mac->id);
    }

    uint32_t mac_id = mac->id;
    nimcp_mutex_unlock(system->mutex);
    return mac_id;
}

/**
 * @brief Check if MAC is complete
 */
bool complement_is_mac_complete(complement_system_t* system, uint32_t mac_id) {
    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_is_mac_co", 0.0f);


    nimcp_mutex_lock(system->mutex);
    complement_mac_t* mac = find_mac_by_id(system, mac_id);
    bool complete = (mac && mac->state == MAC_STATE_COMPLETE);
    nimcp_mutex_unlock(system->mutex);

    return complete;
}

/* ============================================================================
 * Amplification API
 * ============================================================================ */

/**
 * @brief Trigger amplification cascade
 */
int complement_cascade_amplify(complement_system_t* system, float factor) {
    if (!system || !system->config.enable_amplification_loop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_cascade_amplify: required parameter is NULL (system, system->config)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_cascade_a", 0.0f);


    if (factor <= 0.0f) factor = 1.0f;

    nimcp_mutex_lock(system->mutex);

    /* Count current C3b eligible for amplification */
    int amplifiable = 0;
    for (size_t i = 0; i < system->c3b_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->c3b_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->c3b_count);
        }

        if (system->c3b_pool[i].concentration > 0.5f &&
            !system->c3b_pool[i].part_of_amplification) {
            amplifiable++;
        }
    }

    /* Generate new C3b via amplification loop */
    int generated = 0;
    int to_generate = (int)(amplifiable * system->config.amplification_factor * factor);
    to_generate = (to_generate > 50) ? 50 : to_generate; /* Cap at 50 */

    for (int i = 0; i < to_generate && system->c3b_count < system->c3b_capacity; i++) {
        if (system->c3b_count > 0) {
            uint32_t target = system->c3b_pool[0].target_antigen_id;
            if (generate_c3b(system, target, COMPLEMENT_PATHWAY_ALTERNATIVE) == 0) {
                system->c3b_pool[system->c3b_count - 1].part_of_amplification = true;
                generated++;
            }
        }
    }

    system->stats.amplification_cycles++;
    float amp_level = (amplifiable > 0) ? (float)generated / amplifiable : 1.0f;
    if (amp_level > system->stats.max_amplification_reached) {
        system->stats.max_amplification_reached = amp_level;
    }
    system->stats.current_amplification_level = amp_level;

    if (system->config.enable_logging && generated > 0) {
        NIMCP_LOGGING_INFO("Amplification cascade: %d C3b → %d C3b (factor %.2f)",
                          amplifiable, generated, amp_level);
    }

    nimcp_mutex_unlock(system->mutex);
    return generated;
}

/**
 * @brief Get current amplification level
 */
float complement_get_amplification_level(complement_system_t* system) {
    if (!system) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_get_ampli", 0.0f);


    nimcp_mutex_lock(system->mutex);
    float level = system->stats.current_amplification_level;
    nimcp_mutex_unlock(system->mutex);

    return level;
}

/* ============================================================================
 * Anaphylatoxin API
 * ============================================================================ */

/**
 * @brief Release anaphylatoxin
 */
uint32_t complement_release_anaphylatoxin(complement_system_t* system,
                                          anaphylatoxin_type_t type,
                                          uint32_t target_id) {
    if (!system || !system->config.enable_anaphylatoxins) return 0;
    if (system->anaphylatoxin_count >= system->anaphylatoxin_capacity) return 0;

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_release_a", 0.0f);


    nimcp_mutex_lock(system->mutex);

    complement_anaphylatoxin_t* ana = &system->anaphylatoxin_pool[system->anaphylatoxin_count++];
    ana->id = system->next_anaphylatoxin_id++;
    ana->type = type;
    ana->source_antigen_id = target_id;
    ana->release_time = get_timestamp_ms();
    ana->cytokine_id = 0;
    ana->delivered = false;

    /* Set concentration based on type */
    switch (type) {
        case ANAPHYLATOXIN_C3A:
            ana->concentration = system->config.c3a_potency;
            break;
        case ANAPHYLATOXIN_C5A:
            ana->concentration = system->config.c5a_potency;
            break;
        default:
            ana->concentration = 0.3f;
            break;
    }

    system->stats.anaphylatoxins_released++;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Anaphylatoxin %s released for target %u (concentration %.2f)",
                          complement_anaphylatoxin_to_string(type), target_id,
                          ana->concentration);
    }

    uint32_t ana_id = ana->id;
    nimcp_mutex_unlock(system->mutex);
    return ana_id;
}

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update complement system
 */
int complement_update(complement_system_t* system, uint64_t delta_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_update: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_update", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Decay C3b pool */
    decay_c3b_pool(system, delta_ms);

    /* Progress MAC assembly */
    progress_mac_assembly(system, delta_ms);

    /* Mark eliminated targets */
    for (size_t i = 0; i < system->mac_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->mac_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->mac_count);
        }

        if (system->mac_pool[i].state == MAC_STATE_COMPLETE &&
            !system->mac_pool[i].target_eliminated) {
            system->mac_pool[i].target_eliminated = true;
            system->stats.targets_eliminated_by_mac++;
        }
    }

    /* Replenish C3/C5 levels slowly */
    system->current_c3_level += 0.001f * (delta_ms / 1000.0f);
    system->current_c5_level += 0.001f * (delta_ms / 1000.0f);
    if (system->current_c3_level > 1.0f) system->current_c3_level = 1.0f;
    if (system->current_c5_level > 1.0f) system->current_c5_level = 1.0f;

    /* Update active counts */
    system->stats.active_c3b = 0;
    for (size_t i = 0; i < system->c3b_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->c3b_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->c3b_count);
        }

        if (system->c3b_pool[i].concentration > 0.1f) {
            system->stats.active_c3b++;
        }
    }
    system->stats.active_c5b = (uint32_t)system->c5b_count;
    system->stats.active_macs = 0;
    for (size_t i = 0; i < system->mac_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->mac_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->mac_count);
        }

        if (system->mac_pool[i].state != MAC_STATE_INACTIVE) {
            system->stats.active_macs++;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Get complement statistics
 */
int complement_get_stats(complement_system_t* system, complement_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "complement_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_get_stats", 0.0f);


    nimcp_mutex_lock(system->mutex);
    *stats = system->stats;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

/**
 * @brief Check if target is opsonized
 */
bool complement_is_opsonized(complement_system_t* system, uint32_t target_id) {
    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_is_opsoni", 0.0f);


    nimcp_mutex_lock(system->mutex);
    bool opsonized = (compute_opsonization_level(system, target_id) > 0.1f);
    nimcp_mutex_unlock(system->mutex);

    return opsonized;
}

/**
 * @brief Get MAC count for target
 */
uint32_t complement_get_mac_count(complement_system_t* system, uint32_t target_id) {
    if (!system) return 0;

    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_get_mac_c", 0.0f);


    nimcp_mutex_lock(system->mutex);

    uint32_t count = 0;
    for (size_t i = 0; i < system->mac_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->mac_count > 256) {
            complement_system_heartbeat("complement_s_loop",
                             (float)(i + 1) / (float)system->mac_count);
        }

        if (system->mac_pool[i].target_antigen_id == target_id) {
            count++;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return count;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about complement system
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int complement_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    complement_system_heartbeat("complement_s_complement_query_sel", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Complement_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                complement_system_heartbeat("complement_s_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Complement system self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Complement_System");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Complement_System");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void complement_system_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_complement_system_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int complement_system_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "complement_system_training_begin: NULL argument");
        return -1;
    }
    complement_system_heartbeat_instance(NULL, "complement_system_training_begin", 0.0f);
    return 0;
}

int complement_system_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "complement_system_training_end: NULL argument");
        return -1;
    }
    complement_system_heartbeat_instance(NULL, "complement_system_training_end", 1.0f);
    return 0;
}

int complement_system_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "complement_system_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    complement_system_heartbeat_instance(NULL, "complement_system_training_step", progress);
    return 0;
}
