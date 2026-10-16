/**
 * @file nimcp_pattern_db_immune_bridge.c
 * @brief Pattern Database-Immune System Integration Bridge Implementation
 */

#include "security/immune/nimcp_pattern_db_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pattern_db_immune_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cytokine effects on pattern weights
 */
static void compute_cytokine_effects(
    pattern_db_immune_bridge_t* bridge,
    const brain_immune_system_t* immune
) {
    if (!bridge || !immune) return;

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(pattern_db_cytokine_effects_t));

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_weight_boost = CYTOKINE_IL1_WEIGHT_BOOST;
    bridge->cytokine_effects.il6_weight_boost = CYTOKINE_IL6_WEIGHT_BOOST;
    bridge->cytokine_effects.tnf_weight_boost = CYTOKINE_TNF_WEIGHT_BOOST;
    bridge->cytokine_effects.ifn_gamma_weight_boost = CYTOKINE_IFN_GAMMA_WEIGHT_BOOST;
    bridge->cytokine_effects.il10_weight_reduction = CYTOKINE_IL10_WEIGHT_REDUCTION;

    /* Aggregate modulation */
    bridge->cytokine_effects.total_weight_modulation =
        bridge->cytokine_effects.il1_weight_boost +
        bridge->cytokine_effects.il6_weight_boost +
        bridge->cytokine_effects.tnf_weight_boost +
        bridge->cytokine_effects.ifn_gamma_weight_boost +
        bridge->cytokine_effects.il10_weight_reduction;

    /* Compute effective multiplier */
    bridge->cytokine_effects.effective_weight_multiplier =
        1.0f + bridge->cytokine_effects.total_weight_modulation;

    /* Clamp to config limits */
    if (bridge->cytokine_effects.effective_weight_multiplier > bridge->config.max_weight_multiplier) {
        bridge->cytokine_effects.effective_weight_multiplier = bridge->config.max_weight_multiplier;
    }
    if (bridge->cytokine_effects.effective_weight_multiplier < bridge->config.min_weight_multiplier) {
        bridge->cytokine_effects.effective_weight_multiplier = bridge->config.min_weight_multiplier;
    }

    /* Hypervigilant matching if very high multiplier */
    bridge->cytokine_effects.hypervigilant_matching =
        (bridge->cytokine_effects.effective_weight_multiplier > 1.5f);
}

/**
 * @brief Compute inflammation effects on pattern database
 */
static void compute_inflammation_effects(
    pattern_db_immune_bridge_t* bridge,
    const brain_immune_system_t* immune
) {
    if (!bridge || !immune) return;

    /* Get current inflammation level */
    brain_immune_phase_t phase = brain_immune_get_phase((brain_immune_system_t*)immune);

    /* Map phase to inflammation */
    brain_inflammation_level_t level = INFLAMMATION_NONE;
    if (phase >= IMMUNE_PHASE_ACTIVATION) {
        level = INFLAMMATION_LOCAL;
    }
    if (phase >= IMMUNE_PHASE_EFFECTOR) {
        level = INFLAMMATION_REGIONAL;
    }

    bridge->inflammation_state.current_level = level;

    /* Set weight multiplier based on level */
    switch (level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.weight_multiplier = INFLAMMATION_NONE_WEIGHT_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.weight_multiplier = INFLAMMATION_LOCAL_WEIGHT_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.weight_multiplier = INFLAMMATION_REGIONAL_WEIGHT_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.weight_multiplier = INFLAMMATION_SYSTEMIC_WEIGHT_FACTOR;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.weight_multiplier = INFLAMMATION_STORM_WEIGHT_FACTOR;
            break;
    }

    /* Set mode flags */
    bridge->inflammation_state.aggressive_matching = (level >= INFLAMMATION_REGIONAL);
    bridge->inflammation_state.emergency_mode = (level == INFLAMMATION_STORM);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int pattern_db_immune_default_config(pattern_db_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    memset(config, 0, sizeof(pattern_db_immune_config_t));

    /* Feature enables */
    config->enable_cytokine_weight_modulation = true;
    config->enable_inflammation_priority_boost = true;
    config->enable_pattern_match_antigen_presentation = true;
    config->enable_memory_cell_pattern_sync = true;
    config->enable_affinity_based_refinement = true;
    config->enable_auto_pattern_pruning = true;

    /* Pattern modulation config */
    config->max_weight_multiplier = 3.0f; /* Max 3x boost */
    config->min_weight_multiplier = 0.5f; /* Min 0.5x */
    config->max_patterns_from_memory = 100;

    /* Antigen presentation config */
    config->min_match_score_for_antigen = PATTERN_MATCH_ANTIGEN_THRESHOLD;
    config->severity_multiplier = PATTERN_MATCH_SEVERITY_MULTIPLIER;

    /* Pattern pruning config */
    config->auto_prune_unused = PATTERN_PRUNE_UNUSED;
    config->unused_threshold_sec = PATTERN_UNUSED_THRESHOLD_SEC;
    config->min_patterns_to_keep = 10;

    /* Learning feedback */
    config->refine_from_neutralization = true;
    config->learn_from_false_positives = true;

    return 0;
}

pattern_db_immune_bridge_t* pattern_db_immune_create(
    const pattern_db_immune_config_t* config,
    nimcp_pattern_db_t pattern_db,
    brain_immune_system_t* immune_system
) {
    /* Guard clauses */
    if (!pattern_db || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for pattern DB immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_immune_create: required parameter is NULL (pattern_db, immune_system)");
        return NULL;
    }

    /* Allocate bridge */
    pattern_db_immune_bridge_t* bridge = (pattern_db_immune_bridge_t*)
        nimcp_malloc(sizeof(pattern_db_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate pattern DB immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_db_immune_create: failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(pattern_db_immune_bridge_t));

    /* Set handles */
    bridge->pattern_db = pattern_db;
    bridge->immune_system = immune_system;

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        pattern_db_immune_default_config(&bridge->config);
    }

    /* Allocate mappings array */
    bridge->mapping_capacity = 64;
    bridge->mappings = (pattern_immune_mapping_t*)
        nimcp_malloc(sizeof(pattern_immune_mapping_t) * bridge->mapping_capacity);
    if (!bridge->mappings) {
        NIMCP_LOGGING_ERROR("Failed to allocate pattern mappings");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_db_immune_create: bridge->mappings is NULL");
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->mappings, 0, sizeof(pattern_immune_mapping_t) * bridge->mapping_capacity);

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "pattern_db_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for pattern DB immune bridge");
        nimcp_free(bridge->mappings);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_db_immune_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created pattern DB immune bridge");
    return bridge;
}

void pattern_db_immune_destroy(pattern_db_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        pattern_db_immune_disconnect_bio_async(bridge);
    }

    /* Free mappings */
    if (bridge->mappings) {
        nimcp_free(bridge->mappings);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed pattern DB immune bridge");
}

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

int pattern_db_immune_update(pattern_db_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    if (bridge->config.enable_cytokine_weight_modulation) {
        compute_cytokine_effects(bridge, bridge->immune_system);
        bridge->weight_modulations++;
    }

    /* Compute inflammation effects */
    if (bridge->config.enable_inflammation_priority_boost) {
        compute_inflammation_effects(bridge, bridge->immune_system);
    }

    bridge->total_updates++;
    bridge->last_update_time = 0; /* Would use actual timestamp */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pattern_db_immune_apply_modulation(pattern_db_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute effective weight multiplier combining cytokine and inflammation */
    float effective_multiplier = bridge->inflammation_state.weight_multiplier;
    effective_multiplier *= bridge->cytokine_effects.effective_weight_multiplier;

    /* Would update pattern weights in DB here */
    /* For each pattern, multiply weight by effective_multiplier */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pattern_db_immune_sync_memory_to_patterns(pattern_db_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->config.enable_memory_cell_pattern_sync) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Would iterate immune memory cells and create patterns here */
    /* This is a placeholder for the actual sync logic */

    bridge->patterns_synced++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pattern_db_immune_present_match(
    pattern_db_immune_bridge_t* bridge,
    const nimcp_pattern_match_result_t* match_result,
    uint32_t* antigen_id
) {
    if (!bridge || !match_result || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_immune_present_match: required parameter is NULL (bridge, match_result, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_pattern_match_antigen_presentation) return 0;

    /* Check if score is high enough */
    if (match_result->threat_score < bridge->config.min_match_score_for_antigen) {
        return 0; /* Not presented */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute severity from match score */
    uint32_t severity = (uint32_t)(match_result->threat_score * bridge->config.severity_multiplier);
    if (severity > 10) severity = 10;

    /* Create epitope from pattern ID and category */
    uint8_t epitope[32];
    memset(epitope, 0, sizeof(epitope));
    uint32_t* id_ptr = (uint32_t*)epitope;
    *id_ptr = match_result->pattern_id;
    epitope[4] = (uint8_t)match_result->category;

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(epitope),
        severity,
        0, /* source node */
        antigen_id
    );

    if (ret == 0) {
        bridge->antigens_presented++;
        bridge->immune_modulation.antigens_presented++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int pattern_db_immune_refine_from_affinity(
    pattern_db_immune_bridge_t* bridge,
    nimcp_pattern_id_t pattern_id,
    uint32_t antibody_id,
    float affinity_score
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->config.enable_affinity_based_refinement) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Would refine pattern weight based on affinity here */
    /* Higher affinity → boost pattern weight */

    bridge->immune_modulation.patterns_refined++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint32_t pattern_db_immune_prune_unused(pattern_db_immune_bridge_t* bridge) {
    if (!bridge) return 0;
    if (!bridge->config.auto_prune_unused) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint32_t pruned = 0;

    /* Would check pattern last match times and prune old ones here */
    /* This is a placeholder for actual pruning logic */

    bridge->patterns_pruned += pruned;
    bridge->last_prune_time = 0; /* Would use actual timestamp */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return pruned;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int pattern_db_immune_connect_bio_async(pattern_db_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_PATTERN_DB,
        .module_name = "pattern_db_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Pattern DB immune bridge connected to bio-async router");
    }

    return bridge->base.bio_ctx ? 0 : -1;
}

int pattern_db_immune_disconnect_bio_async(pattern_db_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Pattern DB immune bridge disconnected from bio-async router");
    return 0;
}

bool pattern_db_immune_is_bio_async_connected(const pattern_db_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float pattern_db_immune_get_weight_multiplier(const pattern_db_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    float multiplier = bridge->inflammation_state.weight_multiplier;
    multiplier *= bridge->cytokine_effects.effective_weight_multiplier;
    return multiplier;
}

int pattern_db_immune_get_mapping(
    const pattern_db_immune_bridge_t* bridge,
    nimcp_pattern_id_t pattern_id,
    pattern_immune_mapping_t* mapping
) {
    if (!bridge || !mapping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_immune_get_mapping: required parameter is NULL (bridge, mapping)");
        return -1;
    }

    /* Search mappings array for pattern ID */
    for (size_t i = 0; i < bridge->mapping_count; i++) {
        if (bridge->mappings[i].pattern_id == pattern_id) {
            *mapping = bridge->mappings[i];
            return 0;
        }
    }

    return -1; /* Not found */
}
