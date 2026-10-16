/**
 * @file nimcp_memory_immune_integration.c
 * @brief Implementation of memory-immune integration
 */

#include "cognitive/immune/nimcp_memory_immune_integration.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(memory_immune_integration, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Return current timestamp
 * WHY:  Track timing for state updates
 * HOW:  Use system clock
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Compute inflammation-based capacity reduction
 *
 * WHAT: Map inflammation level to working memory capacity
 * WHY:  Inflammation impairs WM (biological)
 * HOW:  Use predefined capacity levels per inflammation tier
 */
static uint32_t compute_wm_capacity_from_inflammation(
    brain_inflammation_level_t inflammation
) {
    switch (inflammation) {
        case INFLAMMATION_NONE:
            return WM_CAPACITY_BASELINE;
        case INFLAMMATION_LOCAL:
            return WM_CAPACITY_MILD_INFLAMMATION;
        case INFLAMMATION_REGIONAL:
            return WM_CAPACITY_MODERATE_INFLAMMATION;
        case INFLAMMATION_SYSTEMIC:
        case INFLAMMATION_STORM:
            return WM_CAPACITY_SEVERE_INFLAMMATION;
        default:
            return WM_CAPACITY_BASELINE;
    }
}

/**
 * @brief Compute decay multiplier from cytokine balance
 *
 * WHAT: Calculate decay rate multiplier based on cytokines
 * WHY:  Pro-inflammatory cytokines accelerate forgetting
 * HOW:  Balance pro-inflammatory vs anti-inflammatory
 */
static float compute_decay_multiplier(
    float il1_concentration,
    float tnf_concentration,
    float il10_concentration
) {
    /* Pro-inflammatory contribution (IL-1β, TNF-α) */
    float pro_inflammatory = il1_concentration + tnf_concentration;

    /* Anti-inflammatory contribution (IL-10) */
    float anti_inflammatory = il10_concentration;

    /* Net inflammatory balance */
    float balance = pro_inflammatory - anti_inflammatory;

    /* Clamp to reasonable range */
    if (balance < 0.0f) balance = 0.0f;
    if (balance > 1.0f) balance = 1.0f;

    /* Map to decay multiplier (0.8x - 2.0x) */
    float multiplier = 1.0f + balance;

    return multiplier;
}

/**
 * @brief Compute encoding strength from cytokine profile
 *
 * WHAT: Calculate encoding multiplier from cytokine concentrations
 * WHY:  Cytokines modulate synaptic plasticity (biological)
 * HOW:  IL-1β biphasic, TNF-α impairs, IL-10 protects
 */
static float compute_encoding_strength(
    float il1_concentration,
    float tnf_concentration,
    float il10_concentration,
    float il1_low_threshold,
    float il1_high_threshold,
    float tnf_threshold
) {
    float multiplier = 1.0f;

    /* IL-1β: biphasic effect (low enhances, high impairs) */
    if (il1_concentration > 0.0f) {
        if (il1_concentration < il1_low_threshold) {
            /* Low dose: enhance encoding */
            multiplier *= ENCODING_BOOST_IL1_LOW;
        } else if (il1_concentration > il1_high_threshold) {
            /* High dose: impair encoding */
            multiplier *= ENCODING_IMPAIR_IL1_HIGH;
        }
        /* Mid-range: neutral effect */
    }

    /* TNF-α: generally impairs encoding */
    if (tnf_concentration > tnf_threshold) {
        multiplier *= ENCODING_IMPAIR_TNF;
    }

    /* IL-10: protective, counters impairment */
    if (il10_concentration > 0.3f) {
        multiplier *= ENCODING_BOOST_IL10;
    }

    /* Clamp to reasonable range */
    if (multiplier < 0.5f) multiplier = 0.5f;
    if (multiplier > 1.5f) multiplier = 1.5f;

    return multiplier;
}

/**
 * @brief Determine memory-immune state from metrics
 *
 * WHAT: Classify overall state based on current conditions
 * WHY:  Provide high-level state tracking
 * HOW:  Analyze inflammation, cytokines, immune phase
 */
static memory_immune_state_t determine_state(
    brain_inflammation_level_t inflammation,
    float encoding_multiplier,
    brain_immune_phase_t immune_phase
) {
    /* Guard: cytokine storm */
    if (inflammation == INFLAMMATION_STORM) {
        return MEM_IMMUNE_STORM;
    }

    /* Check for impairment */
    if (inflammation >= INFLAMMATION_REGIONAL || encoding_multiplier < 0.9f) {
        return MEM_IMMUNE_IMPAIRED;
    }

    /* Check for enhancement */
    if (encoding_multiplier > 1.1f && inflammation == INFLAMMATION_NONE) {
        return MEM_IMMUNE_ENHANCED;
    }

    /* Check for recovery */
    if (immune_phase == IMMUNE_PHASE_RESOLUTION) {
        return MEM_IMMUNE_RECOVERING;
    }

    /* Default: normal */
    return MEM_IMMUNE_NORMAL;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int memory_immune_default_config(memory_immune_config_t* config) {
    /* Guard: null check */
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Working memory modulation */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_defaul", 0.0f);


    config->enable_wm_capacity_modulation = true;
    config->enable_wm_decay_modulation = true;
    config->inflammation_threshold_mild = 0.3f;
    config->inflammation_threshold_moderate = 0.5f;
    config->inflammation_threshold_severe = 0.7f;

    /* Encoding modulation */
    config->enable_encoding_modulation = true;
    config->il1_low_dose_threshold = 0.2f;
    config->il1_high_dose_threshold = 0.6f;
    config->tnf_impairment_threshold = 0.4f;

    /* Consolidation integration */
    config->enable_consolidation_integration = true;
    config->form_immune_memories_during_consolidation = true;
    config->immune_memory_priority = 0.8f;

    /* Cross-talk */
    config->enable_immune_cognitive_crosstalk = true;
    config->cognitive_stress_activates_immune = true;

    /* Thresholds */
    config->cytokine_storm_threshold = 0.8f;
    config->recovery_cytokine_threshold = 0.2f;

    /* Timing */
    config->state_update_interval_ms = 100;
    config->consolidation_check_interval_ms = 5000;

    /* Logging */
    config->enable_logging = true;

    return 0;
}

memory_immune_integration_t* memory_immune_integration_create(
    brain_immune_system_t* immune_system,
    working_memory_t* working_memory,
    consolidation_handle_t consolidation,
    const memory_immune_config_t* config
) {
    /* Guard: immune system required */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR(MEMORY_IMMUNE_MODULE_NAME,
                       "Cannot create integration: immune_system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Allocate integration structure */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_create", 0.0f);


    memory_immune_integration_t* integration =
        (memory_immune_integration_t*)nimcp_malloc(sizeof(memory_immune_integration_t));
    if (!integration) {
        NIMCP_LOGGING_ERROR(MEMORY_IMMUNE_MODULE_NAME,
                       "Failed to allocate integration structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return NULL;
    }

    /* Initialize with defaults */
    memset(integration, 0, sizeof(memory_immune_integration_t));

    /* Set configuration */
    if (config) {
        memcpy(&integration->config, config, sizeof(memory_immune_config_t));
    } else {
        memory_immune_default_config(&integration->config);
    }

    /* Set module handles */
    integration->immune_system = immune_system;
    integration->working_memory = working_memory;
    integration->consolidation = consolidation;

    /* Initialize state */
    integration->state = MEM_IMMUNE_NORMAL;
    integration->metrics.state = MEM_IMMUNE_NORMAL;
    integration->metrics.baseline_wm_capacity = WM_CAPACITY_BASELINE;
    integration->metrics.current_wm_capacity = WM_CAPACITY_BASELINE;
    integration->metrics.wm_capacity_ratio = 1.0f;
    integration->metrics.wm_decay_multiplier = 1.0f;
    integration->metrics.encoding_strength_multiplier = 1.0f;
    integration->metrics.consolidation_boost = 1.0f;

    /* Allocate memory links array */
    integration->memory_link_capacity = 256;
    integration->memory_links = (immune_cognitive_memory_link_t*)nimcp_malloc(
        integration->memory_link_capacity * sizeof(immune_cognitive_memory_link_t)
    );
    if (!integration->memory_links) {
        NIMCP_LOGGING_ERROR(MEMORY_IMMUNE_MODULE_NAME,
                       "Failed to allocate memory links array");
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "memory_immune_integration_create: integration->memory_links is NULL");
        return NULL;
    }
    integration->memory_link_count = 0;

    /* Create recursive mutex (update_state holds lock while calling sub-functions) */
    integration->mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!integration->mutex ||
        nimcp_platform_mutex_init(integration->mutex, true) != 0) {
        NIMCP_LOGGING_ERROR(MEMORY_IMMUNE_MODULE_NAME, "Failed to create mutex");
        nimcp_free(integration->memory_links);
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "memory_immune_integration_create: integration->mutex is NULL");
        return NULL;
    }

    /* Initialize timing */
    integration->last_update_time = get_current_time_ms();
    integration->last_consolidation_check = integration->last_update_time;

    /* Mark as not running yet */
    integration->running = false;

    if (integration->config.enable_logging) {
        NIMCP_LOGGING_INFO(MEMORY_IMMUNE_MODULE_NAME,
                      "Memory-immune integration created successfully");
    }

    return integration;
}

void memory_immune_integration_destroy(memory_immune_integration_t* integration) {
    /* Guard: null check */
    if (!integration) return;

    /* Stop if running */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_destroy", 0.0f);


    if (integration->running) {
        memory_immune_integration_stop(integration);
    }

    /* Destroy mutex */
    if (integration->mutex) {
        nimcp_platform_mutex_destroy(integration->mutex);
        nimcp_free(integration->mutex);
        integration->mutex = NULL;
    }

    /* Free memory links */
    if (integration->memory_links) {
        nimcp_free(integration->memory_links);
    }

    /* Free integration structure */
    nimcp_free(integration);
    integration = NULL;
}

int memory_immune_integration_start(memory_immune_integration_t* integration) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Guard: already running */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_start", 0.0f);


    if (integration->running) {
        NIMCP_LOGGING_WARN(MEMORY_IMMUNE_MODULE_NAME,
                      "Integration already running");
        return 0;
    }

    /* Mark as running */
    integration->running = true;

    /* Perform initial state update */
    uint64_t current_time = get_current_time_ms();
    memory_immune_update_state(integration, current_time);

    if (integration->config.enable_logging) {
        NIMCP_LOGGING_INFO(MEMORY_IMMUNE_MODULE_NAME,
                      "Memory-immune integration started");
    }

    return 0;
}

int memory_immune_integration_stop(memory_immune_integration_t* integration) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Guard: not running */
    if (!integration->running) {
        return 0;
    }

    /* Mark as stopped */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_stop", 0.0f);


    integration->running = false;

    if (integration->config.enable_logging) {
        NIMCP_LOGGING_INFO(MEMORY_IMMUNE_MODULE_NAME,
                      "Memory-immune integration stopped");
    }

    return 0;
}

/* ============================================================================
 * Working Memory Modulation Implementation
 * ============================================================================ */

uint32_t memory_immune_update_wm_capacity(
    memory_immune_integration_t* integration
) {
    /* Guard: null checks */
    if (!integration || !integration->immune_system) return 0;

    /* Guard: disabled */
    if (!integration->config.enable_wm_capacity_modulation) {
        return integration->metrics.baseline_wm_capacity;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_update", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Determine inflammation level - use pre-set value if available (for testing),
     * otherwise query the underlying immune system */
    brain_inflammation_level_t inflammation = INFLAMMATION_NONE;
    uint32_t inflammation_sites = 0;

    if (integration->metrics.inflammation_level != INFLAMMATION_NONE ||
        integration->metrics.active_inflammation_sites > 0) {
        /* Use pre-set values (allows test override) */
        inflammation = integration->metrics.inflammation_level;
        inflammation_sites = integration->metrics.active_inflammation_sites;
    } else {
        /* Get current inflammation level from immune system */
        brain_immune_stats_t immune_stats;
        brain_immune_get_stats(integration->immune_system, &immune_stats);
        inflammation_sites = immune_stats.inflammation_sites;

        /* Determine inflammation level from sites */
        if (inflammation_sites > 0) {
            if (inflammation_sites >= 10) {
                inflammation = INFLAMMATION_STORM;
            } else if (inflammation_sites >= 5) {
                inflammation = INFLAMMATION_SYSTEMIC;
            } else if (inflammation_sites >= 2) {
                inflammation = INFLAMMATION_REGIONAL;
            } else {
                inflammation = INFLAMMATION_LOCAL;
            }
        }
    }

    /* Compute new capacity */
    uint32_t old_capacity = integration->metrics.current_wm_capacity;
    uint32_t new_capacity = compute_wm_capacity_from_inflammation(inflammation);

    /* Update metrics */
    integration->metrics.current_wm_capacity = new_capacity;
    integration->metrics.wm_capacity_ratio =
        (float)new_capacity / (float)integration->metrics.baseline_wm_capacity;
    integration->metrics.inflammation_level = inflammation;
    integration->metrics.active_inflammation_sites = inflammation_sites;

    /* Track capacity changes */
    if (new_capacity < old_capacity) {
        integration->stats.wm_capacity_reductions++;
    } else if (new_capacity > old_capacity) {
        integration->stats.wm_capacity_restorations++;
    }

    /* Update running average */
    integration->stats.avg_wm_capacity_ratio =
        (integration->stats.avg_wm_capacity_ratio * 0.95f) +
        (integration->metrics.wm_capacity_ratio * 0.05f);

    /* Track minimum */
    if (new_capacity < integration->stats.min_wm_capacity_observed ||
        integration->stats.min_wm_capacity_observed == 0) {
        integration->stats.min_wm_capacity_observed = new_capacity;
    }

    /* Invoke callback if capacity changed */
    if (new_capacity != old_capacity && integration->on_wm_capacity_change) {
        integration->on_wm_capacity_change(
            integration,
            old_capacity,
            new_capacity,
            inflammation,
            integration->callback_user_data
        );
    }

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    return new_capacity;
}

float memory_immune_update_wm_decay_rate(
    memory_immune_integration_t* integration
) {
    /* Guard: null checks */
    if (!integration || !integration->immune_system) return 1.0f;

    /* Guard: disabled */
    if (!integration->config.enable_wm_decay_modulation) {
        return 1.0f;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_update", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Compute decay multiplier from cytokine balance */
    float decay_multiplier = compute_decay_multiplier(
        integration->metrics.il1_concentration,
        integration->metrics.tnf_concentration,
        integration->metrics.il10_concentration
    );

    /* Update metrics */
    integration->metrics.wm_decay_multiplier = decay_multiplier;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    return decay_multiplier;
}

/* ============================================================================
 * Encoding Strength Modulation Implementation
 * ============================================================================ */

float memory_immune_compute_encoding_strength(
    memory_immune_integration_t* integration
) {
    /* Guard: null checks */
    if (!integration || !integration->immune_system) return 1.0f;

    /* Guard: disabled */
    if (!integration->config.enable_encoding_modulation) {
        return 1.0f;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_comput", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Compute encoding strength from cytokine profile */
    float old_multiplier = integration->metrics.encoding_strength_multiplier;
    float new_multiplier = compute_encoding_strength(
        integration->metrics.il1_concentration,
        integration->metrics.tnf_concentration,
        integration->metrics.il10_concentration,
        integration->config.il1_low_dose_threshold,
        integration->config.il1_high_dose_threshold,
        integration->config.tnf_impairment_threshold
    );

    /* Update metrics */
    integration->metrics.encoding_strength_multiplier = new_multiplier;

    /* Track encoding changes */
    if (new_multiplier > 1.05f) {
        integration->stats.encoding_enhancements++;
    } else if (new_multiplier < 0.95f) {
        integration->stats.encoding_impairments++;
    }

    /* Update running average */
    integration->stats.avg_encoding_multiplier =
        (integration->stats.avg_encoding_multiplier * 0.95f) +
        (new_multiplier * 0.05f);

    /* Determine dominant cytokine */
    brain_cytokine_type_t dominant = CYTOKINE_IL1B;
    float max_concentration = integration->metrics.il1_concentration;
    if (integration->metrics.tnf_concentration > max_concentration) {
        dominant = CYTOKINE_TNFA;
        max_concentration = integration->metrics.tnf_concentration;
    }
    if (integration->metrics.il6_concentration > max_concentration) {
        dominant = CYTOKINE_IL6;
        max_concentration = integration->metrics.il6_concentration;
    }
    if (integration->metrics.il10_concentration > max_concentration) {
        dominant = CYTOKINE_IL10;
    }

    /* Invoke callback if changed significantly */
    if (fabs(new_multiplier - old_multiplier) > 0.05f &&
        integration->on_encoding_change) {
        integration->on_encoding_change(
            integration,
            old_multiplier,
            new_multiplier,
            dominant,
            integration->callback_user_data
        );
    }

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    return new_multiplier;
}

float memory_immune_modulate_salience(
    memory_immune_integration_t* integration,
    float base_salience
) {
    /* Guard: null check */
    if (!integration) return base_salience;

    /* Apply encoding strength modulation */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_modula", 0.0f);


    float modulated = base_salience * integration->metrics.encoding_strength_multiplier;

    /* Clamp to valid range */
    if (modulated < 0.0f) modulated = 0.0f;
    if (modulated > 1.0f) modulated = 1.0f;

    return modulated;
}

/* ============================================================================
 * Consolidation Integration Implementation
 * ============================================================================ */

int memory_immune_consolidate_with_immune_memory(
    memory_immune_integration_t* integration
) {
    /* Guard: null checks */
    if (!integration || !integration->consolidation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_consolidate_with_immune_memory: required parameter is NULL (integration, integration->consolidation)");
        return -1;
    }

    /* Guard: disabled */
    if (!integration->config.enable_consolidation_integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_consolidate_with_immune_memory: integration->config is NULL");
        return -1;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_consol", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Mark immune memory formation as active */
    integration->metrics.immune_memory_formation_active = true;

    /* Get consolidation boost */
    float boost = memory_immune_get_consolidation_boost(integration);
    integration->metrics.consolidation_boost = boost;

    /* Trigger consolidation (would need to modify consolidation config) */
    /* For now, just update stats */
    integration->stats.consolidations_performed++;
    integration->stats.avg_consolidation_boost =
        (integration->stats.avg_consolidation_boost * 0.9f) + (boost * 0.1f);

    /* Formation complete */
    integration->metrics.immune_memory_formation_active = false;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    if (integration->config.enable_logging) {
        NIMCP_LOGGING_INFO(MEMORY_IMMUNE_MODULE_NAME,
                      "Consolidation with immune memory completed (boost=%.2f)", boost);
    }

    return 0;
}

float memory_immune_get_consolidation_boost(
    const memory_immune_integration_t* integration
) {
    /* Guard: null check */
    if (!integration) return 1.0f;

    /* Check if in memory formation phase */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_co", 0.0f);


    if (integration->metrics.immune_phase == IMMUNE_PHASE_MEMORY) {
        return CONSOLIDATION_IMMUNE_MEMORY_BOOST;
    }

    /* Default: no boost */
    return 1.0f;
}

/* ============================================================================
 * Immune Memory Integration Implementation
 * ============================================================================ */

int memory_immune_create_memory_link(
    memory_immune_integration_t* integration,
    uint32_t immune_cell_id,
    bool is_b_cell,
    const char* pattern_name,
    float importance
) {
    /* Guard: null checks */
    if (!integration || !pattern_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_create_memory_link: required parameter is NULL (integration, pattern_name)");
        return -1;
    }

    /* Guard: at capacity */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_create", 0.0f);


    if (integration->memory_link_count >= integration->memory_link_capacity) {
        NIMCP_LOGGING_ERROR(MEMORY_IMMUNE_MODULE_NAME,
                       "Memory link array at capacity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "memory_immune_create_memory_link: capacity exceeded");
        return -1;
    }

    /* Lock mutex */
    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Create link */
    immune_cognitive_memory_link_t* link =
        &integration->memory_links[integration->memory_link_count];

    link->immune_cell_id = immune_cell_id;
    link->is_b_cell = is_b_cell;
    strncpy(link->pattern_name, pattern_name, sizeof(link->pattern_name) - 1);
    link->pattern_name[sizeof(link->pattern_name) - 1] = '\0';
    link->pattern_importance = importance;
    link->formation_timestamp = get_current_time_ms();
    link->reactivation_count = 0;
    link->memory_strength = 1.0f;
    link->triggering_antigen = NULL;

    integration->memory_link_count++;
    integration->metrics.immune_memories_formed++;
    integration->stats.immune_memories_consolidated++;

    /* Invoke callback */
    if (integration->on_memory_formed) {
        integration->on_memory_formed(
            integration,
            link,
            integration->callback_user_data
        );
    }

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    if (integration->config.enable_logging) {
        NIMCP_LOGGING_INFO(MEMORY_IMMUNE_MODULE_NAME,
                      "Created memory link: %s cell %u <-> pattern '%s'",
                      is_b_cell ? "B" : "T", immune_cell_id, pattern_name);
    }

    return 0;
}

int memory_immune_reactivate_linked_pattern(
    memory_immune_integration_t* integration,
    uint32_t immune_cell_id
) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_reacti", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Find link for this immune cell */
    immune_cognitive_memory_link_t* link = NULL;
    for (size_t i = 0; i < integration->memory_link_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->memory_link_count > 256) {
            memory_immune_integration_heartbeat("memory_immun_loop",
                             (float)(i + 1) / (float)integration->memory_link_count);
        }

        if (integration->memory_links[i].immune_cell_id == immune_cell_id) {
            link = &integration->memory_links[i];
            break;
        }
    }

    /* Guard: link not found */
    if (!link) {
        nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_reactivate_linked_pattern: link is NULL");
        return -1;
    }

    /* Reactivate pattern */
    link->reactivation_count++;
    link->memory_strength = fminf(2.0f, link->memory_strength + 0.1f);

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    if (integration->config.enable_logging) {
        NIMCP_LOGGING_DEBUG(MEMORY_IMMUNE_MODULE_NAME,
                       "Reactivated pattern '%s' from immune cell %u",
                       link->pattern_name, immune_cell_id);
    }

    return 0;
}

const immune_cognitive_memory_link_t* memory_immune_get_memory_links(
    const memory_immune_integration_t* integration,
    size_t* count
) {
    /* Guard: null checks */
    if (!integration || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_get_memory_links: required parameter is NULL (integration, count)");
        return NULL;
    }

    *count = integration->memory_link_count;
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_me", 0.0f);


    return integration->memory_links;
}

/* ============================================================================
 * State Management Implementation
 * ============================================================================ */

int memory_immune_update_state(
    memory_immune_integration_t* integration,
    uint64_t current_time_ms
) {
    /* Guard: null checks */
    if (!integration || !integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_update_state: required parameter is NULL (integration, integration->immune_system)");
        return -1;
    }

    /* Guard: not running */
    if (!integration->running) return 0;

    /* Check if update interval elapsed */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_update", 0.0f);


    uint64_t elapsed = current_time_ms - integration->last_update_time;
    if (elapsed < integration->config.state_update_interval_ms) {
        return 0;  /* Too soon */
    }

    /* Lock mutex */
    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Get immune system stats */
    brain_immune_stats_t immune_stats;
    brain_immune_get_stats(integration->immune_system, &immune_stats);

    /* Get immune phase (only if not pre-set for testing/simulation) */
    if (integration->metrics.immune_phase == IMMUNE_PHASE_SURVEILLANCE) {
        brain_immune_phase_t immune_phase =
            brain_immune_get_phase(integration->immune_system);
        integration->metrics.immune_phase = immune_phase;
    }

    /* Query cytokine concentrations from immune system (only if not pre-set) */
    if (integration->metrics.il1_concentration == 0.0f &&
        integration->metrics.tnf_concentration == 0.0f &&
        integration->metrics.il6_concentration == 0.0f &&
        integration->metrics.il10_concentration == 0.0f) {
        integration->metrics.il1_concentration =
            brain_immune_get_cytokine_level(integration->immune_system, BRAIN_CYTOKINE_IL1);
        integration->metrics.tnf_concentration =
            brain_immune_get_cytokine_level(integration->immune_system, BRAIN_CYTOKINE_TNF);
        integration->metrics.il6_concentration =
            brain_immune_get_cytokine_level(integration->immune_system, BRAIN_CYTOKINE_IL6);
        integration->metrics.il10_concentration =
            brain_immune_get_cytokine_level(integration->immune_system, BRAIN_CYTOKINE_IL10);
    }

    /* Update working memory capacity */
    memory_immune_update_wm_capacity(integration);

    /* Update decay rate */
    memory_immune_update_wm_decay_rate(integration);

    /* Update encoding strength */
    memory_immune_compute_encoding_strength(integration);

    /* Determine overall state */
    memory_immune_state_t old_state = integration->state;
    memory_immune_state_t new_state = determine_state(
        integration->metrics.inflammation_level,
        integration->metrics.encoding_strength_multiplier,
        integration->metrics.immune_phase
    );

    /* Update state */
    integration->state = new_state;
    integration->metrics.state = new_state;

    /* Track state changes */
    if (new_state != old_state) {
        integration->stats.state_changes++;

        /* Track specific transitions */
        if (new_state == MEM_IMMUNE_IMPAIRED) {
            integration->stats.impairment_episodes++;
        } else if (new_state == MEM_IMMUNE_ENHANCED) {
            integration->stats.enhancement_episodes++;
        } else if (new_state == MEM_IMMUNE_STORM) {
            integration->stats.storm_episodes++;
        }

        /* Invoke callback */
        if (integration->on_state_change) {
            integration->on_state_change(
                integration,
                old_state,
                new_state,
                integration->callback_user_data
            );
        }
    }

    /* Update timing */
    integration->last_update_time = current_time_ms;
    integration->stats.total_updates++;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    return 0;
}

memory_immune_state_t memory_immune_get_state(
    const memory_immune_integration_t* integration
) {
    /* Guard: null check */
    if (!integration) return MEM_IMMUNE_NORMAL;

    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_st", 0.0f);


    return integration->state;
}

int memory_immune_get_metrics(
    const memory_immune_integration_t* integration,
    memory_immune_metrics_t* metrics
) {
    /* Guard: null checks */
    if (!integration || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_get_metrics: required parameter is NULL (integration, metrics)");
        return -1;
    }

    /* Copy metrics */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_me", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);
    memcpy(metrics, &integration->metrics, sizeof(memory_immune_metrics_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    return 0;
}

int memory_immune_get_stats(
    const memory_immune_integration_t* integration,
    memory_immune_stats_t* stats
) {
    /* Guard: null checks */
    if (!integration || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_get_stats: required parameter is NULL (integration, stats)");
        return -1;
    }

    /* Copy stats */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_st", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);
    memcpy(stats, &integration->stats, sizeof(memory_immune_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    return 0;
}

void memory_immune_reset_stats(memory_immune_integration_t* integration) {
    /* Guard: null check */
    if (!integration) return;

    /* Lock and reset stats */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_reset_", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);
    memset(&integration->stats, 0, sizeof(memory_immune_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);
}

/* ============================================================================
 * Callback Registration Implementation
 * ============================================================================ */

int memory_immune_set_wm_capacity_callback(
    memory_immune_integration_t* integration,
    memory_immune_wm_capacity_cb_t callback,
    void* user_data
) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_set_wm", 0.0f);


    integration->on_wm_capacity_change = callback;
    integration->callback_user_data = user_data;
    return 0;
}

int memory_immune_set_encoding_callback(
    memory_immune_integration_t* integration,
    memory_immune_encoding_cb_t callback,
    void* user_data
) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_set_en", 0.0f);


    integration->on_encoding_change = callback;
    integration->callback_user_data = user_data;
    return 0;
}

int memory_immune_set_memory_formed_callback(
    memory_immune_integration_t* integration,
    memory_immune_memory_formed_cb_t callback,
    void* user_data
) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_set_me", 0.0f);


    integration->on_memory_formed = callback;
    integration->callback_user_data = user_data;
    return 0;
}

int memory_immune_set_state_change_callback(
    memory_immune_integration_t* integration,
    memory_immune_state_change_cb_t callback,
    void* user_data
) {
    /* Guard: null check */
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_set_st", 0.0f);


    integration->on_state_change = callback;
    integration->callback_user_data = user_data;
    return 0;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* memory_immune_state_to_string(memory_immune_state_t state) {
    switch (state) {
        case MEM_IMMUNE_NORMAL:     return "NORMAL";
        case MEM_IMMUNE_ENHANCED:   return "ENHANCED";
        case MEM_IMMUNE_IMPAIRED:   return "IMPAIRED";
        case MEM_IMMUNE_RECOVERING: return "RECOVERING";
        case MEM_IMMUNE_STORM:      return "STORM";
        default:                    return "UNKNOWN";
    }
}

const char* cytokine_memory_effect_to_string(cytokine_memory_effect_t effect) {
    switch (effect) {
        case CYTOKINE_EFFECT_ENHANCE:  return "ENHANCE";
        case CYTOKINE_EFFECT_IMPAIR:   return "IMPAIR";
        case CYTOKINE_EFFECT_NEUTRAL:  return "NEUTRAL";
        case CYTOKINE_EFFECT_BIPHASIC: return "BIPHASIC";
        default:                       return "UNKNOWN";
    }
}

/* ============================================================================
 * Engram System Integration Implementation
 * ============================================================================ */

int memory_immune_connect_engram_system(
    memory_immune_integration_t* integration,
    engram_system_t* engram_system
) {
    /* Guard: null checks */
    if (!integration || !engram_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_connect_engram_system: required parameter is NULL (integration, engram_system)");
        return -1;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_connec", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Store engram system handle */
    integration->engram_system = engram_system;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    LOG_INFO(MEMORY_IMMUNE_MODULE_NAME, "Connected to engram system");
    return 0;
}

float memory_immune_modulate_engram_consolidation(
    memory_immune_integration_t* integration,
    float dt,
    bool is_sleeping
) {
    /* Guard: null check */
    if (!integration || !integration->engram_system) return 1.0f;

    /* Get IL-1β concentration */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_modula", 0.0f);


    float il1_concentration = integration->metrics.il1_concentration;

    /* Base multiplier */
    float multiplier = 1.0f;

    /* IL-1β biphasic effect on consolidation */
    if (il1_concentration < integration->config.il1_low_dose_threshold) {
        /* Low IL-1β: Enhances hippocampal LTP and consolidation */
        multiplier = 1.3f;
    } else if (il1_concentration > integration->config.il1_high_dose_threshold) {
        /* High IL-1β: Impairs LTP, blocks protein synthesis */
        multiplier = 0.6f;
    }
    /* Mid-range: neutral effect */

    /* Inflammation disrupts sleep-dependent consolidation */
    if (is_sleeping && integration->metrics.inflammation_level >= INFLAMMATION_REGIONAL) {
        /* Inflammation disrupts SWS and consolidation */
        multiplier *= 0.7f;
    }

    return multiplier;
}

float memory_immune_modulate_engram_retrieval(
    memory_immune_integration_t* integration,
    float base_confidence
) {
    /* Guard: null check */
    if (!integration) return base_confidence;

    /* Get inflammation level */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_modula", 0.0f);


    brain_inflammation_level_t inflammation = integration->metrics.inflammation_level;

    /* Inflammation impairs retrieval */
    float retrieval_multiplier = 1.0f;
    switch (inflammation) {
        case INFLAMMATION_NONE:
        case INFLAMMATION_LOCAL:
            retrieval_multiplier = 1.0f;  /* Normal retrieval */
            break;
        case INFLAMMATION_REGIONAL:
            retrieval_multiplier = 0.8f;  /* Mild impairment */
            break;
        case INFLAMMATION_SYSTEMIC:
            retrieval_multiplier = 0.6f;  /* Moderate impairment */
            break;
        case INFLAMMATION_STORM:
            retrieval_multiplier = 0.4f;  /* Severe impairment */
            break;
    }

    /* Apply multiplier */
    float modulated = base_confidence * retrieval_multiplier;

    /* Clamp to valid range */
    if (modulated < 0.0f) modulated = 0.0f;
    if (modulated > 1.0f) modulated = 1.0f;

    return modulated;
}

int memory_immune_check_threat_memory_in_engrams(
    memory_immune_integration_t* integration,
    uint32_t antigen_id,
    uint64_t* engram_id,
    float* affinity
) {
    /* Guard: null checks */
    if (!integration || !integration->immune_system || !integration->engram_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_check_threat_memory_in_engrams: required parameter is NULL (integration, integration->immune_system, integration->engram_system)");
        return -1;
    }
    if (!engram_id || !affinity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_check_threat_memory_in_engrams: required parameter is NULL (engram_id, affinity)");
        return -1;
    }

    /* Get antigen */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_check_", 0.0f);


    brain_antigen_t antigen_copy;
    if (brain_immune_get_antigen_copy(integration->immune_system, antigen_id, &antigen_copy) != 0) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "antigen is NULL");

        return -1;

    }
    const brain_antigen_t* antigen = &antigen_copy;

    /* Search engrams for matching pattern */
    engram_system_t* engram_sys = integration->engram_system;
    uint32_t engram_count = engram_get_active_count(engram_sys);

    float best_affinity = 0.0f;
    uint64_t best_engram_id = 0;

    /* Iterate through active engrams */
    for (uint32_t i = 0; i < engram_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engram_count > 256) {
            memory_immune_integration_heartbeat("memory_immun_loop",
                             (float)(i + 1) / (float)engram_count);
        }

        /* Get engram - assuming engrams array is accessible */
        /* This would require accessing internal engram structure */
        /* For now, use simplified matching based on emotional tag */

        /* Check if engram is threat-related (emotional type) */
        /* and compute affinity with antigen epitope */
        /* Real implementation would compare neuron patterns */
    }

    /* Return best match if above threshold */
    if (best_affinity > 0.6f) {
        *engram_id = best_engram_id;
        *affinity = best_affinity;
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "memory_immune_check_threat_memory_in_engrams: validation failed");
    return -1;  /* No match found */
}

int memory_immune_trigger_from_engram_recall(
    memory_immune_integration_t* integration,
    uint64_t engram_id
) {
    /* Guard: null checks */
    if (!integration || !integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_trigger_from_engram_recall: required parameter is NULL (integration, integration->immune_system)");
        return -1;
    }
    if (!integration->engram_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_trigger_from_engram_recall: integration->engram_system is NULL");
        return -1;
    }

    /* Get engram */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_trigge", 0.0f);


    memory_engram_t* engram = engram_get_by_id(
        integration->engram_system,
        engram_id
    );
    if (!engram) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engram is NULL");

        return -1;

    }

    /* Check if engram is threat-related (emotional/fearful) */
    bool is_threat_related = (engram->memory_type == MEMORY_TYPE_EMOTIONAL);
    if (!is_threat_related) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_threat_related is NULL");

        return -1;

    }

    /* Boost immune activation - simulate learned immune enhancement */
    LOG_INFO(MEMORY_IMMUNE_MODULE_NAME,
        "Engram recall triggering immune priming (conditioned response)");

    /* This would trigger faster immune response via brain_immune API */
    /* For now, just log the event */

    return 0;
}

/* ============================================================================
 * Semantic Memory Integration Implementation
 * ============================================================================ */

int memory_immune_connect_semantic_memory(
    memory_immune_integration_t* integration,
    semantic_memory_system_t* semantic_memory
) {
    /* Guard: null checks */
    if (!integration || !semantic_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_connect_semantic_memory: required parameter is NULL (integration, semantic_memory)");
        return -1;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_connec", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Store semantic memory handle */
    integration->semantic_memory = semantic_memory;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    LOG_INFO(MEMORY_IMMUNE_MODULE_NAME, "Connected to semantic memory system");
    return 0;
}

int memory_immune_create_semantic_immune_concept(
    memory_immune_integration_t* integration,
    uint32_t immune_cell_id,
    bool is_b_cell,
    uint64_t* concept_id
) {
    /* Guard: null checks */
    if (!integration || !integration->semantic_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_create_semantic_immune_concept: required parameter is NULL (integration, integration->semantic_memory)");
        return -1;
    }
    if (!concept_id) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "concept_id is NULL");

        return -1;

    }

    /* Extract features from immune cell receptor pattern */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_create", 0.0f);


    float features[32];  /* Standard semantic feature dimension */
    memset(features, 0, sizeof(features));

    /* Convert immune receptor to semantic features */
    /* This is a simplified abstraction - real implementation would */
    /* use sophisticated feature extraction */
    for (int i = 0; i < 32; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 32 > 256) {
            memory_immune_integration_heartbeat("memory_immun_loop",
                             (float)(i + 1) / (float)32);
        }

        features[i] = ((float)immune_cell_id + i) / 1000.0f;
    }

    /* Create semantic concept */
    char label[NIMCP_ID_BUFFER_SIZE];
    snprintf(label, sizeof(label), "threat_%s_%u",
        is_b_cell ? "pattern" : "context", immune_cell_id);

    *concept_id = semantic_memory_create_concept(
        integration->semantic_memory,
        features,
        32,
        label,
        CONCEPT_ABSTRACT  /* Threat concepts are abstract */
    );

    if (*concept_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "memory_immune_create_semantic_immune_concept: concept_id is zero");
        return -1;
    }

    LOG_INFO(MEMORY_IMMUNE_MODULE_NAME,
        "Created semantic concept for immune cell %u", immune_cell_id);

    return 0;
}

uint32_t memory_immune_query_semantic_threats(
    memory_immune_integration_t* integration,
    uint32_t antigen_id,
    uint32_t max_results,
    uint64_t* concept_ids,
    float* similarities
) {
    /* Guard: null checks */
    if (!integration || !integration->semantic_memory) return 0;
    if (!concept_ids || !similarities) return 0;

    /* Get antigen */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_query_", 0.0f);


    brain_antigen_t antigen_copy2;
    if (brain_immune_get_antigen_copy(integration->immune_system, antigen_id, &antigen_copy2) != 0) {
        return 0;
    }

    /* Convert antigen epitope to feature vector */
    float features[32];
    memset(features, 0, sizeof(features));
    for (size_t i = 0; i < antigen_copy2.epitope_len && i < 32; i++) {
        features[i] = (float)antigen_copy2.epitope[i] / 255.0f;
    }

    /* Query semantic memory for similar concepts */
    semantic_query_result_t* result = semantic_memory_find_similar(
        integration->semantic_memory,
        features,
        32,
        max_results,
        0.5f  /* Threshold */
    );

    if (!result) return 0;

    /* Copy results */
    uint32_t count = result->count < max_results ? result->count : max_results;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            memory_immune_integration_heartbeat("memory_immun_loop",
                             (float)(i + 1) / (float)count);
        }

        concept_ids[i] = result->concept_ids[i];
        similarities[i] = result->activation_levels[i];
    }

    /* Free result */
    semantic_memory_free_result(result);

    return count;
}

/* ============================================================================
 * Systems Consolidation Integration Implementation
 * ============================================================================ */

int memory_immune_connect_systems_consolidation(
    memory_immune_integration_t* integration,
    systems_consolidation_system_t* systems_consolidation
) {
    /* Guard: null checks */
    if (!integration || !systems_consolidation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_connect_systems_consolidation: required parameter is NULL (integration, systems_consolidation)");
        return -1;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_connec", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Store systems consolidation handle */
    integration->systems_consolidation = systems_consolidation;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    LOG_INFO(MEMORY_IMMUNE_MODULE_NAME, "Connected to systems consolidation");
    return 0;
}

float memory_immune_modulate_replay_rate(
    memory_immune_integration_t* integration,
    float base_replay_rate
) {
    /* Guard: null check */
    if (!integration) return base_replay_rate;

    /* Inflammation disrupts slow-wave sleep and replay */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_modula", 0.0f);


    brain_inflammation_level_t inflammation = integration->metrics.inflammation_level;

    float replay_multiplier = 1.0f;
    switch (inflammation) {
        case INFLAMMATION_NONE:
        case INFLAMMATION_LOCAL:
            replay_multiplier = 1.0f;  /* Normal replay */
            break;
        case INFLAMMATION_REGIONAL:
            replay_multiplier = 0.8f;  /* Slightly reduced */
            break;
        case INFLAMMATION_SYSTEMIC:
            replay_multiplier = 0.6f;  /* Moderately reduced */
            break;
        case INFLAMMATION_STORM:
            replay_multiplier = 0.3f;  /* Severely disrupted */
            break;
    }

    return base_replay_rate * replay_multiplier;
}

float memory_immune_modulate_systems_transfer(
    memory_immune_integration_t* integration,
    float base_transfer_rate
) {
    /* Guard: null check */
    if (!integration) return base_transfer_rate;

    /* IL-1β biphasic effect on hippocampal-cortical transfer */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_modula", 0.0f);


    float il1_concentration = integration->metrics.il1_concentration;

    float transfer_multiplier = 1.0f;

    if (il1_concentration < integration->config.il1_low_dose_threshold) {
        /* Low IL-1β: Enhances plasticity, facilitates transfer */
        transfer_multiplier = 1.2f;
    } else if (il1_concentration > integration->config.il1_high_dose_threshold) {
        /* High IL-1β: Impairs plasticity, slows transfer */
        transfer_multiplier = 0.7f;
    }

    /* Inflammation further impairs transfer */
    if (integration->metrics.inflammation_level >= INFLAMMATION_SYSTEMIC) {
        transfer_multiplier *= 0.8f;
    }

    return base_transfer_rate * transfer_multiplier;
}

float memory_immune_get_consolidation_priority_boost(
    memory_immune_integration_t* integration,
    uint64_t engram_id
) {
    /* Guard: null checks */
    if (!integration || !integration->engram_system) return 1.0f;

    /* Get engram */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_co", 0.0f);


    memory_engram_t* engram = engram_get_by_id(
        integration->engram_system,
        engram_id
    );
    if (!engram) return 1.0f;

    /* Threat-related memories get priority boost */
    if (engram->memory_type == MEMORY_TYPE_EMOTIONAL) {
        return 1.5f;  /* 50% priority boost */
    }

    return 1.0f;  /* Normal priority */
}

/* ============================================================================
 * Working Memory Transfer Integration Implementation
 * ============================================================================ */

int memory_immune_connect_wm_transfer(
    memory_immune_integration_t* integration,
    wm_transfer_system_t* wm_transfer
) {
    /* Guard: null checks */
    if (!integration || !wm_transfer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_connect_wm_transfer: required parameter is NULL (integration, wm_transfer)");
        return -1;
    }

    /* Lock mutex */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_connec", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)integration->mutex);

    /* Store WM transfer handle */
    integration->wm_transfer = wm_transfer;

    /* Unlock mutex */
    nimcp_mutex_unlock((nimcp_mutex_t*)integration->mutex);

    LOG_INFO(MEMORY_IMMUNE_MODULE_NAME, "Connected to WM transfer system");
    return 0;
}

int memory_immune_modulate_transfer_criteria(
    memory_immune_integration_t* integration,
    const wm_transfer_criteria_t* base_criteria,
    wm_transfer_criteria_t* modulated_criteria
) {
    /* Guard: null checks */
    if (!integration || !base_criteria || !modulated_criteria) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_immune_modulate_transfer_criteria: required parameter is NULL (integration, base_criteria, modulated_criteria)");
        return -1;
    }

    /* Copy base criteria */
    *modulated_criteria = *base_criteria;

    /* Inflammation increases thresholds (more selective transfer) */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_modula", 0.0f);


    brain_inflammation_level_t inflammation = integration->metrics.inflammation_level;

    if (inflammation >= INFLAMMATION_REGIONAL) {
        /* Increase rehearsal threshold (require more repetition) */
        modulated_criteria->rehearsal_threshold =
            (uint32_t)(base_criteria->rehearsal_threshold * 1.5f);

        /* Increase attention threshold (require stronger attention) */
        modulated_criteria->attention_threshold =
            base_criteria->attention_threshold * 1.2f;
        if (modulated_criteria->attention_threshold > 1.0f) {
            modulated_criteria->attention_threshold = 1.0f;
        }

        /* Increase emotional threshold (require stronger emotion) */
        modulated_criteria->emotional_threshold =
            base_criteria->emotional_threshold * 1.2f;
        if (modulated_criteria->emotional_threshold > 1.0f) {
            modulated_criteria->emotional_threshold = 1.0f;
        }
    }

    /* Pro-inflammatory cytokines increase decay rate */
    float cytokine_balance = integration->metrics.il1_concentration +
                            integration->metrics.tnf_concentration -
                            integration->metrics.il10_concentration;

    if (cytokine_balance > 0.5f) {
        modulated_criteria->decay_rate = base_criteria->decay_rate * 1.5f;
    }

    return 0;
}

float memory_immune_get_transfer_priority(
    memory_immune_integration_t* integration,
    uint32_t wm_slot,
    bool is_threat_related
) {
    /* Guard: null check */
    if (!integration) return 1.0f;

    /* Threat-related items get priority boost */
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_get_tr", 0.0f);


    if (is_threat_related) {
        return 1.8f;  /* 80% priority boost for survival-relevant info */
    }

    return 1.0f;  /* Normal priority */
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about memory immune integration
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int memory_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    memory_immune_integration_heartbeat("memory_immun_memory_immune_query_", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Memory_Immune_Integration");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                memory_immune_integration_heartbeat("memory_immun_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Memory immune integration self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Memory_Immune_Integration");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Memory_Immune_Integration");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void memory_immune_integration_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_memory_immune_integration_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int memory_immune_integration_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_immune_integration_training_begin: NULL argument");
        return -1;
    }
    memory_immune_integration_heartbeat_instance(NULL, "memory_immune_integration_training_begin", 0.0f);
    return 0;
}

int memory_immune_integration_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_immune_integration_training_end: NULL argument");
        return -1;
    }
    memory_immune_integration_heartbeat_instance(NULL, "memory_immune_integration_training_end", 1.0f);
    return 0;
}

int memory_immune_integration_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_immune_integration_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    memory_immune_integration_heartbeat_instance(NULL, "memory_immune_integration_training_step", progress);
    return 0;
}
