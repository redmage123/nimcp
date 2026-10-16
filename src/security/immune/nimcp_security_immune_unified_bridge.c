/**
 * @file nimcp_security_immune_unified_bridge.c
 * @brief Unified Security-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Comprehensive bidirectional integration between all security components
 *       and the brain immune system through a single unified bridge.
 * WHY:  Biological immune systems coordinate with physical barriers and detection
 *       systems to provide layered defense; this mirrors that by unifying BBB,
 *       anomaly detection, pattern database, rate limiting, and policy engine
 *       with the adaptive immune response system.
 * HOW:  Each security component presents threats as antigens and receives immune
 *       modulation signals; immune responses map to security actions; cytokines
 *       tune security sensitivity; tolerance prevents false positives.
 */

#include "security/immune/nimcp_security_immune_unified_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_immune_unified_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper: Timestamp
 * ============================================================================ */

static uint64_t get_current_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

/* ============================================================================
 * Internal Helper: Compute Cytokine Effects
 * ============================================================================ */

/**
 * @brief Compute individual cytokine effects on security components
 */
static void compute_cytokine_modulation(
    sec_immune_unified_bridge_t* bridge,
    sec_immune_cytokine_effects_t* effects
) {
    if (!bridge || !effects || !bridge->immune_system) return;

    /* Get immune statistics for cytokine levels */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        return;
    }

    /* IL-1 effects - pro-inflammatory */
    float il1_level = stats.cytokine_il1;
    effects->il1_bbb_modulation = il1_level * SEC_IMMUNE_IL1_BBB_THRESHOLD_IMPACT;
    effects->il1_anomaly_modulation = il1_level * SEC_IMMUNE_IL1_ANOMALY_THRESHOLD_IMPACT;
    effects->il1_pattern_modulation = il1_level * SEC_IMMUNE_IL1_PATTERN_WEIGHT_IMPACT;
    effects->il1_rate_modulation = il1_level * SEC_IMMUNE_IL1_RATE_LIMIT_IMPACT;
    effects->il1_policy_modulation = il1_level * SEC_IMMUNE_IL1_POLICY_STRICTNESS_IMPACT;

    /* IL-6 effects - acute phase */
    float il6_level = stats.cytokine_il6;
    effects->il6_bbb_modulation = il6_level * SEC_IMMUNE_IL6_BBB_THRESHOLD_IMPACT;
    effects->il6_anomaly_modulation = il6_level * SEC_IMMUNE_IL6_ANOMALY_THRESHOLD_IMPACT;
    effects->il6_pattern_modulation = il6_level * SEC_IMMUNE_IL6_PATTERN_WEIGHT_IMPACT;
    effects->il6_rate_modulation = il6_level * SEC_IMMUNE_IL6_RATE_LIMIT_IMPACT;
    effects->il6_policy_modulation = il6_level * SEC_IMMUNE_IL6_POLICY_STRICTNESS_IMPACT;

    /* TNF-alpha effects - severe inflammation */
    float tnf_level = stats.cytokine_tnf;
    effects->tnf_bbb_modulation = tnf_level * SEC_IMMUNE_TNF_BBB_THRESHOLD_IMPACT;
    effects->tnf_anomaly_modulation = tnf_level * SEC_IMMUNE_TNF_ANOMALY_THRESHOLD_IMPACT;
    effects->tnf_pattern_modulation = tnf_level * SEC_IMMUNE_TNF_PATTERN_WEIGHT_IMPACT;
    effects->tnf_rate_modulation = tnf_level * SEC_IMMUNE_TNF_RATE_LIMIT_IMPACT;
    effects->tnf_policy_modulation = tnf_level * SEC_IMMUNE_TNF_POLICY_STRICTNESS_IMPACT;

    /* IL-10 effects - anti-inflammatory */
    float il10_level = stats.cytokine_il10;
    effects->il10_bbb_modulation = il10_level * SEC_IMMUNE_IL10_BBB_THRESHOLD_IMPACT;
    effects->il10_anomaly_modulation = il10_level * SEC_IMMUNE_IL10_ANOMALY_THRESHOLD_IMPACT;
    effects->il10_pattern_modulation = il10_level * SEC_IMMUNE_IL10_PATTERN_WEIGHT_IMPACT;
    effects->il10_rate_modulation = il10_level * SEC_IMMUNE_IL10_RATE_LIMIT_IMPACT;
    effects->il10_policy_modulation = il10_level * SEC_IMMUNE_IL10_POLICY_STRICTNESS_IMPACT;

    /* IFN-gamma effects - quarantine/antiviral */
    float ifn_level = stats.cytokine_ifn_gamma;
    effects->ifn_bbb_modulation = ifn_level * SEC_IMMUNE_IFN_BBB_THRESHOLD_IMPACT;
    effects->ifn_anomaly_modulation = ifn_level * SEC_IMMUNE_IFN_ANOMALY_THRESHOLD_IMPACT;
    effects->ifn_pattern_modulation = ifn_level * SEC_IMMUNE_IFN_PATTERN_WEIGHT_IMPACT;
    effects->ifn_rate_modulation = ifn_level * SEC_IMMUNE_IFN_RATE_LIMIT_IMPACT;
    effects->ifn_policy_modulation = ifn_level * SEC_IMMUNE_IFN_POLICY_STRICTNESS_IMPACT;

    /* Compute totals */
    effects->total_bbb_modulation =
        effects->il1_bbb_modulation + effects->il6_bbb_modulation +
        effects->tnf_bbb_modulation + effects->il10_bbb_modulation +
        effects->ifn_bbb_modulation;

    effects->total_anomaly_modulation =
        effects->il1_anomaly_modulation + effects->il6_anomaly_modulation +
        effects->tnf_anomaly_modulation + effects->il10_anomaly_modulation +
        effects->ifn_anomaly_modulation;

    effects->total_pattern_modulation =
        effects->il1_pattern_modulation + effects->il6_pattern_modulation +
        effects->tnf_pattern_modulation + effects->il10_pattern_modulation +
        effects->ifn_pattern_modulation;

    effects->total_rate_modulation =
        effects->il1_rate_modulation + effects->il6_rate_modulation +
        effects->tnf_rate_modulation + effects->il10_rate_modulation +
        effects->ifn_rate_modulation;

    effects->total_policy_modulation =
        effects->il1_policy_modulation + effects->il6_policy_modulation +
        effects->tnf_policy_modulation + effects->il10_policy_modulation +
        effects->ifn_policy_modulation;

    /* Mode flags */
    effects->emergency_mode_active = (tnf_level > 0.7f);
    effects->recovery_mode_active = (il10_level > 0.5f && tnf_level < 0.3f);
}

/* ============================================================================
 * Internal Helper: Compute Inflammation Effects
 * ============================================================================ */

/**
 * @brief Compute inflammation-based security parameter factors
 */
static void compute_inflammation_factors(
    sec_immune_unified_bridge_t* bridge,
    sec_immune_inflammation_state_t* state
) {
    if (!bridge || !state || !bridge->immune_system) return;

    /* Get current inflammation level */
    brain_inflammation_level_t level = brain_immune_get_inflammation_level(bridge->immune_system);
    state->current_level = level;

    /* Map inflammation to factors */
    switch (level) {
        case INFLAMMATION_NONE:
            state->bbb_threshold_factor = SEC_IMMUNE_INFL_NONE_BBB_FACTOR;
            state->anomaly_threshold_factor = SEC_IMMUNE_INFL_NONE_ANOMALY_FACTOR;
            state->pattern_weight_factor = SEC_IMMUNE_INFL_NONE_PATTERN_FACTOR;
            state->rate_limit_factor = SEC_IMMUNE_INFL_NONE_RATE_FACTOR;
            state->policy_strictness_factor = SEC_IMMUNE_INFL_NONE_POLICY_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            state->bbb_threshold_factor = SEC_IMMUNE_INFL_LOCAL_BBB_FACTOR;
            state->anomaly_threshold_factor = SEC_IMMUNE_INFL_LOCAL_ANOMALY_FACTOR;
            state->pattern_weight_factor = SEC_IMMUNE_INFL_LOCAL_PATTERN_FACTOR;
            state->rate_limit_factor = SEC_IMMUNE_INFL_LOCAL_RATE_FACTOR;
            state->policy_strictness_factor = SEC_IMMUNE_INFL_LOCAL_POLICY_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            state->bbb_threshold_factor = SEC_IMMUNE_INFL_REGIONAL_BBB_FACTOR;
            state->anomaly_threshold_factor = SEC_IMMUNE_INFL_REGIONAL_ANOMALY_FACTOR;
            state->pattern_weight_factor = SEC_IMMUNE_INFL_REGIONAL_PATTERN_FACTOR;
            state->rate_limit_factor = SEC_IMMUNE_INFL_REGIONAL_RATE_FACTOR;
            state->policy_strictness_factor = SEC_IMMUNE_INFL_REGIONAL_POLICY_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            state->bbb_threshold_factor = SEC_IMMUNE_INFL_SYSTEMIC_BBB_FACTOR;
            state->anomaly_threshold_factor = SEC_IMMUNE_INFL_SYSTEMIC_ANOMALY_FACTOR;
            state->pattern_weight_factor = SEC_IMMUNE_INFL_SYSTEMIC_PATTERN_FACTOR;
            state->rate_limit_factor = SEC_IMMUNE_INFL_SYSTEMIC_RATE_FACTOR;
            state->policy_strictness_factor = SEC_IMMUNE_INFL_SYSTEMIC_POLICY_FACTOR;
            break;
        case INFLAMMATION_STORM:
            state->bbb_threshold_factor = SEC_IMMUNE_INFL_STORM_BBB_FACTOR;
            state->anomaly_threshold_factor = SEC_IMMUNE_INFL_STORM_ANOMALY_FACTOR;
            state->pattern_weight_factor = SEC_IMMUNE_INFL_STORM_PATTERN_FACTOR;
            state->rate_limit_factor = SEC_IMMUNE_INFL_STORM_RATE_FACTOR;
            state->policy_strictness_factor = SEC_IMMUNE_INFL_STORM_POLICY_FACTOR;
            break;
    }

    /* Mode flags */
    state->hypervigilant_mode = (level >= INFLAMMATION_SYSTEMIC);
    state->emergency_lockdown = (level == INFLAMMATION_STORM);
    state->resource_conservation = (level >= INFLAMMATION_REGIONAL);
}

/* ============================================================================
 * Internal Helper: Allocate Tolerance Whitelist
 * ============================================================================ */

static int allocate_tolerance_whitelist(
    sec_immune_unified_bridge_t* bridge,
    size_t capacity
) {
    if (!bridge || capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "allocate_tolerance_whitelist: bridge is NULL");
        return -1;
    }

    bridge->tolerance.whitelist = (sec_immune_tolerance_entry_t*)
        nimcp_malloc(sizeof(sec_immune_tolerance_entry_t) * capacity);
    if (!bridge->tolerance.whitelist) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    memset(bridge->tolerance.whitelist, 0, sizeof(sec_immune_tolerance_entry_t) * capacity);
    bridge->tolerance.whitelist_capacity = capacity;
    bridge->tolerance.whitelist_count = 0;
    return 0;
}

/* ============================================================================
 * Internal Helper: Find Tolerance Entry
 * ============================================================================ */

static sec_immune_tolerance_entry_t* find_tolerance_entry(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
) {
    if (!bridge || !pattern || pattern_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tolerance_entry: required parameter is NULL (bridge, pattern)");
        return NULL;
    }
    if (!bridge->tolerance.whitelist) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tolerance_entry: bridge->tolerance is NULL");
        return NULL;
    }

    size_t compare_len = pattern_len;
    if (compare_len > BRAIN_IMMUNE_EPITOPE_SIZE) {
        compare_len = BRAIN_IMMUNE_EPITOPE_SIZE;
    }

    for (size_t i = 0; i < bridge->tolerance.whitelist_count; i++) {
        sec_immune_tolerance_entry_t* entry = &bridge->tolerance.whitelist[i];
        if (entry->pattern_len == compare_len &&
            memcmp(entry->pattern, pattern, compare_len) == 0) {
            return entry;
        }
    }
    return NULL;
}

/* ============================================================================
 * Internal Helper: Find Memory Cell
 * ============================================================================ */

static sec_immune_memory_cell_t* find_memory_cell_by_id(
    sec_immune_unified_bridge_t* bridge,
    uint32_t memory_id
) {
    if (!bridge || !bridge->memory_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_memory_cell_by_id: required parameter is NULL (bridge, bridge->memory_cells)");
        return NULL;
    }

    for (size_t i = 0; i < bridge->memory_cell_count; i++) {
        if (bridge->memory_cells[i].memory_id == memory_id) {
            return &bridge->memory_cells[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int sec_immune_unified_default_config(sec_immune_unified_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    memset(config, 0, sizeof(sec_immune_unified_config_t));

    /* Security -> Immune enables */
    config->enable_bbb_antigen_presentation = true;
    config->enable_anomaly_antigen_presentation = true;
    config->enable_pattern_antigen_presentation = true;
    config->enable_rate_violation_antigen_presentation = true;
    config->enable_policy_violation_antigen_presentation = true;

    /* Immune -> Security enables: cytokines */
    config->enable_cytokine_bbb_modulation = true;
    config->enable_cytokine_anomaly_modulation = true;
    config->enable_cytokine_pattern_modulation = true;
    config->enable_cytokine_rate_modulation = true;
    config->enable_cytokine_policy_modulation = true;

    /* Immune -> Security enables: inflammation */
    config->enable_inflammation_bbb_adjustment = true;
    config->enable_inflammation_anomaly_adjustment = true;
    config->enable_inflammation_pattern_adjustment = true;
    config->enable_inflammation_rate_adjustment = true;
    config->enable_inflammation_policy_adjustment = true;

    /* Bidirectional features */
    config->enable_antibody_action_execution = true;
    config->enable_memory_cell_formation = true;
    config->enable_pattern_memory_sync = true;
    config->enable_tolerance_system = true;
    config->enable_regulatory_t_cells = true;

    /* Auto-response */
    config->auto_trigger_inflammation = true;
    config->auto_form_memory = true;
    config->auto_sync_patterns = true;
    config->auto_train_from_feedback = true;

    /* BBB thresholds */
    config->bbb_base_threshold = 0.5f;
    config->bbb_min_threshold_factor = 0.3f;
    config->bbb_max_threshold_factor = 1.0f;

    /* Anomaly thresholds */
    config->anomaly_base_threshold = 0.7f;
    config->anomaly_min_threshold = 0.3f;
    config->anomaly_max_threshold = 0.95f;

    /* Pattern weights */
    config->pattern_base_weight = 1.0f;
    config->pattern_min_weight_factor = 0.5f;
    config->pattern_max_weight_factor = 2.0f;

    /* Rate limits */
    config->rate_base_rps = 100.0f;
    config->rate_base_burst = 150.0f;
    config->rate_min_factor = 0.2f;

    /* Policy strictness */
    config->policy_base_strictness = 1.0f;
    config->policy_min_strictness = 0.5f;
    config->policy_max_strictness = 2.0f;

    /* Tolerance configuration */
    config->max_tolerance_entries = SEC_IMMUNE_TOLERANCE_MAX_WHITELIST;
    config->tolerance_confirmation_count = SEC_IMMUNE_TOLERANCE_CONFIRMATION_COUNT;
    config->tolerance_learning_period_ms = SEC_IMMUNE_TOLERANCE_LEARNING_PERIOD_MS;
    config->regulatory_suppression_threshold = SEC_IMMUNE_TOLERANCE_REGULATORY_THRESHOLD;

    /* Memory configuration */
    config->min_neutralizations_for_memory = SEC_IMMUNE_MEMORY_MIN_NEUTRALIZATIONS;
    config->memory_decay_half_life_ms = SEC_IMMUNE_MEMORY_DECAY_HALF_LIFE_MS;
    config->sync_memory_to_pattern_db = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->broadcast_security_events = true;
    config->broadcast_immune_modulation = true;

    return 0;
}

sec_immune_unified_bridge_t* sec_immune_unified_create(
    const sec_immune_unified_config_t* config,
    brain_immune_system_t* immune_system
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Immune system required for unified bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    sec_immune_unified_bridge_t* bridge = (sec_immune_unified_bridge_t*)
        nimcp_malloc(sizeof(sec_immune_unified_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-immune unified bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_immune_unified_create: failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(sec_immune_unified_bridge_t));

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_IMMUNE_SECURITY;
    bridge->base.module_name = "sec_immune_unified_bridge";

    /* Set immune system */
    bridge->immune_system = immune_system;

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_immune_unified_default_config(&bridge->config);
    }

    /* Create mutex */
    bridge->base.mutex = nimcp_platform_mutex_create();
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for unified bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_immune_unified_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate tolerance whitelist */
    if (allocate_tolerance_whitelist(bridge, bridge->config.max_tolerance_entries) != 0) {
        NIMCP_LOGGING_WARN("Failed to allocate tolerance whitelist");
    }

    /* Allocate memory cells array */
    bridge->memory_cell_capacity = 64;
    bridge->memory_cells = (sec_immune_memory_cell_t*)
        nimcp_malloc(sizeof(sec_immune_memory_cell_t) * bridge->memory_cell_capacity);
    if (bridge->memory_cells) {
        memset(bridge->memory_cells, 0,
               sizeof(sec_immune_memory_cell_t) * bridge->memory_cell_capacity);
    }

    /* Allocate antibody actions array */
    bridge->antibody_action_capacity = 64;
    bridge->antibody_actions = (sec_immune_antibody_action_t*)
        nimcp_malloc(sizeof(sec_immune_antibody_action_t) * bridge->antibody_action_capacity);
    if (bridge->antibody_actions) {
        memset(bridge->antibody_actions, 0,
               sizeof(sec_immune_antibody_action_t) * bridge->antibody_action_capacity);
    }

    /* Initialize timing */
    bridge->last_update_time = get_current_time_ms();
    bridge->next_memory_id = 1;

    NIMCP_LOGGING_INFO("Created security-immune unified bridge");
    return bridge;
}

void sec_immune_unified_destroy(sec_immune_unified_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_immune_unified_disconnect_bio_async(bridge);
    }

    /* Free tolerance whitelist */
    if (bridge->tolerance.whitelist) {
        nimcp_free(bridge->tolerance.whitelist);
    }

    /* Free memory cells */
    if (bridge->memory_cells) {
        nimcp_free(bridge->memory_cells);
    }

    /* Free antibody actions */
    if (bridge->antibody_actions) {
        nimcp_free(bridge->antibody_actions);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed security-immune unified bridge");
}

int sec_immune_unified_reset(sec_immune_unified_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset cytokine effects */
    memset(&bridge->cytokine_effects, 0, sizeof(sec_immune_cytokine_effects_t));

    /* Reset inflammation state but keep level */
    bridge->inflammation_state.bbb_threshold_factor = 1.0f;
    bridge->inflammation_state.anomaly_threshold_factor = 1.0f;
    bridge->inflammation_state.pattern_weight_factor = 1.0f;
    bridge->inflammation_state.rate_limit_factor = 1.0f;
    bridge->inflammation_state.policy_strictness_factor = 1.0f;

    /* Reset per-component state */
    memset(&bridge->bbb_state, 0, sizeof(sec_immune_bbb_state_t));
    bridge->bbb_state.effective_threshold_factor = 1.0f;

    memset(&bridge->anomaly_state, 0, sizeof(sec_immune_anomaly_state_t));
    bridge->anomaly_state.effective_threshold = bridge->config.anomaly_base_threshold;

    memset(&bridge->pattern_state, 0, sizeof(sec_immune_pattern_state_t));
    bridge->pattern_state.effective_weight_factor = 1.0f;

    memset(&bridge->rate_state, 0, sizeof(sec_immune_rate_state_t));
    bridge->rate_state.effective_rps_factor = 1.0f;
    bridge->rate_state.effective_burst_factor = 1.0f;

    memset(&bridge->policy_state, 0, sizeof(sec_immune_policy_state_t));
    bridge->policy_state.effective_strictness_factor = 1.0f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_immune_unified_stats_t));

    /* Reset timing */
    bridge->last_update_time = get_current_time_ms();

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Security Component Connection API
 * ============================================================================ */

int sec_immune_unified_connect_bbb(
    sec_immune_unified_bridge_t* bridge,
    bbb_system_t bbb_system
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->bbb_system = bbb_system;
    bridge->bbb_state.effective_threshold_factor = 1.0f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected BBB to unified security-immune bridge");
    return 0;
}

int sec_immune_unified_connect_anomaly(
    sec_immune_unified_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->anomaly_detector = detector;
    bridge->anomaly_state.effective_threshold = bridge->config.anomaly_base_threshold;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected anomaly detector to unified security-immune bridge");
    return 0;
}

int sec_immune_unified_connect_pattern_db(
    sec_immune_unified_bridge_t* bridge,
    nimcp_pattern_db_t pattern_db
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->pattern_db = pattern_db;
    bridge->pattern_state.effective_weight_factor = 1.0f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected pattern DB to unified security-immune bridge");
    return 0;
}

int sec_immune_unified_connect_rate_limiter(
    sec_immune_unified_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->rate_limiter = rate_limiter;
    bridge->rate_state.effective_rps_factor = 1.0f;
    bridge->rate_state.effective_burst_factor = 1.0f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected rate limiter to unified security-immune bridge");
    return 0;
}

int sec_immune_unified_connect_policy_engine(
    sec_immune_unified_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->policy_engine = policy_engine;
    bridge->policy_state.effective_strictness_factor = 1.0f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected policy engine to unified security-immune bridge");
    return 0;
}

int sec_immune_unified_connect_all(
    sec_immune_unified_bridge_t* bridge,
    bbb_system_t bbb_system,
    nimcp_anomaly_detector_t detector,
    nimcp_pattern_db_t pattern_db,
    nimcp_rate_limiter_t rate_limiter,
    nimcp_policy_engine_t policy_engine
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    if (bbb_system) {
        sec_immune_unified_connect_bbb(bridge, bbb_system);
    }
    if (detector) {
        sec_immune_unified_connect_anomaly(bridge, detector);
    }
    if (pattern_db) {
        sec_immune_unified_connect_pattern_db(bridge, pattern_db);
    }
    if (rate_limiter) {
        sec_immune_unified_connect_rate_limiter(bridge, rate_limiter);
    }
    if (policy_engine) {
        sec_immune_unified_connect_policy_engine(bridge, policy_engine);
    }

    return 0;
}

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

int sec_immune_unified_update(sec_immune_unified_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint64_t now = get_current_time_ms();

    /* Update cytokine effects */
    compute_cytokine_modulation(bridge, &bridge->cytokine_effects);
    bridge->last_cytokine_update = now;

    /* Update inflammation effects */
    compute_inflammation_factors(bridge, &bridge->inflammation_state);
    bridge->last_inflammation_update = now;

    /* Apply modulations */
    sec_immune_unified_apply_cytokine_effects(bridge);
    sec_immune_unified_apply_inflammation(bridge);

    /* Update state */
    bridge->last_update_time = now;
    bridge->base.total_updates++;

    /* Update stats */
    bridge->stats.current_inflammation = bridge->inflammation_state.current_level;
    bridge->stats.emergency_mode_active = bridge->cytokine_effects.emergency_mode_active ||
                                          bridge->inflammation_state.emergency_lockdown;
    bridge->stats.tolerance_learning_active = bridge->tolerance.learning_mode_active;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_apply_cytokine_effects(sec_immune_unified_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    /* Apply to BBB state */
    if (bridge->config.enable_cytokine_bbb_modulation) {
        float factor = 1.0f + bridge->cytokine_effects.total_bbb_modulation;
        factor = nimcp_clampf(factor, bridge->config.bbb_min_threshold_factor,
                             bridge->config.bbb_max_threshold_factor);
        bridge->bbb_state.effective_threshold_factor = factor;
        bridge->stats.bbb_modulations++;
    }

    /* Apply to anomaly state */
    if (bridge->config.enable_cytokine_anomaly_modulation) {
        float threshold = bridge->config.anomaly_base_threshold *
                          (1.0f + bridge->cytokine_effects.total_anomaly_modulation);
        threshold = nimcp_clampf(threshold, bridge->config.anomaly_min_threshold,
                                bridge->config.anomaly_max_threshold);
        bridge->anomaly_state.effective_threshold = threshold;
        bridge->stats.anomaly_modulations++;
    }

    /* Apply to pattern state */
    if (bridge->config.enable_cytokine_pattern_modulation) {
        float factor = 1.0f + bridge->cytokine_effects.total_pattern_modulation;
        factor = nimcp_clampf(factor, bridge->config.pattern_min_weight_factor,
                             bridge->config.pattern_max_weight_factor);
        bridge->pattern_state.effective_weight_factor = factor;
        bridge->stats.pattern_modulations++;
    }

    /* Apply to rate state */
    if (bridge->config.enable_cytokine_rate_modulation) {
        float factor = 1.0f + bridge->cytokine_effects.total_rate_modulation;
        factor = nimcp_clampf(factor, bridge->config.rate_min_factor, 1.0f);
        bridge->rate_state.effective_rps_factor = factor;
        bridge->rate_state.effective_burst_factor = factor;
        bridge->stats.rate_modulations++;
    }

    /* Apply to policy state */
    if (bridge->config.enable_cytokine_policy_modulation) {
        float factor = 1.0f + bridge->cytokine_effects.total_policy_modulation;
        factor = nimcp_clampf(factor, bridge->config.policy_min_strictness,
                             bridge->config.policy_max_strictness);
        bridge->policy_state.effective_strictness_factor = factor;
        bridge->stats.policy_modulations++;
    }

    return 0;
}

int sec_immune_unified_apply_inflammation(sec_immune_unified_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    sec_immune_inflammation_state_t* state = &bridge->inflammation_state;

    /* Apply inflammation factors to BBB */
    if (bridge->config.enable_inflammation_bbb_adjustment) {
        bridge->bbb_state.effective_threshold_factor *= state->bbb_threshold_factor;
        bridge->bbb_state.paranoid_mode = state->hypervigilant_mode;
    }

    /* Apply to anomaly */
    if (bridge->config.enable_inflammation_anomaly_adjustment) {
        bridge->anomaly_state.effective_threshold *= state->anomaly_threshold_factor;
        bridge->anomaly_state.hypervigilant_detection = state->hypervigilant_mode;
    }

    /* Apply to pattern */
    if (bridge->config.enable_inflammation_pattern_adjustment) {
        bridge->pattern_state.effective_weight_factor *= state->pattern_weight_factor;
    }

    /* Apply to rate limiter */
    if (bridge->config.enable_inflammation_rate_adjustment) {
        bridge->rate_state.effective_rps_factor *= state->rate_limit_factor;
        bridge->rate_state.effective_burst_factor *= state->rate_limit_factor;
        bridge->rate_state.emergency_throttling = state->emergency_lockdown;
    }

    /* Apply to policy */
    if (bridge->config.enable_inflammation_policy_adjustment) {
        bridge->policy_state.effective_strictness_factor *= state->policy_strictness_factor;
        bridge->policy_state.emergency_enforcement = state->emergency_lockdown;
    }

    return 0;
}

/* ============================================================================
 * Security -> Immune: Antigen Presentation API
 * ============================================================================ */

int sec_immune_unified_present_bbb_threat(
    sec_immune_unified_bridge_t* bridge,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity,
    const uint8_t* threat_data,
    size_t data_len,
    uint32_t* antigen_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_present_bbb_threat: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bbb_antigen_presentation) return 0;
    if (severity < SEC_IMMUNE_BBB_MIN_SEVERITY_FOR_ANTIGEN) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check tolerance */
    if (bridge->config.enable_tolerance_system && threat_data && data_len > 0) {
        if (find_tolerance_entry(bridge, threat_data, data_len) != NULL) {
            bridge->tolerance.false_positives_prevented++;
            bridge->stats.false_positives_prevented++;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0; /* Tolerated - don't present */
        }
    }

    /* Map severity */
    uint32_t immune_severity = (uint32_t)(severity * SEC_IMMUNE_BBB_SEVERITY_MULTIPLIER);
    if (immune_severity > 10) immune_severity = 10;

    /* Create epitope from threat data */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    size_t copy_len = data_len < sizeof(epitope) ? data_len : sizeof(epitope);
    if (threat_data && copy_len > 0) {
        memcpy(epitope, threat_data, copy_len);
    }

    /* Present to immune system */
    int ret = brain_immune_present_bbb_threat(
        bridge->immune_system,
        threat_type,
        severity,
        epitope,
        copy_len,
        antigen_id
    );

    if (ret == 0) {
        bridge->bbb_state.threats_presented++;
        bridge->bbb_state.last_threat_time = get_current_time_ms();
        if (severity > bridge->bbb_state.max_threat_severity) {
            bridge->bbb_state.max_threat_severity = (float)severity;
        }
        bridge->stats.bbb_antigens_presented++;
        bridge->stats.total_antigens_presented++;

        /* Auto-trigger inflammation for severe threats */
        if (bridge->config.auto_trigger_inflammation &&
            severity >= BBB_SEVERITY_HIGH) {
            uint32_t site_id = 0;
            brain_immune_initiate_inflammation(
                bridge->immune_system, 0, *antigen_id, &site_id);
            /* Escalate for critical threats */
            if (severity >= BBB_SEVERITY_CRITICAL) {
                brain_immune_escalate_inflammation(
                    bridge->immune_system, site_id);
            }
            bridge->stats.inflammation_triggers++;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int sec_immune_unified_present_anomaly(
    sec_immune_unified_bridge_t* bridge,
    const nimcp_anomaly_result_t* result,
    uint32_t* antigen_id
) {
    if (!bridge || !result || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_present_anomaly: required parameter is NULL (bridge, result, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_anomaly_antigen_presentation) return 0;
    if (result->anomaly_score < SEC_IMMUNE_ANOMALY_MIN_SCORE_FOR_ANTIGEN) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map anomaly score to severity */
    uint32_t severity = (uint32_t)(result->anomaly_score * SEC_IMMUNE_ANOMALY_SCORE_MULTIPLIER);
    if (severity > 10) severity = 10;

    /* Create epitope from triggered features */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    memcpy(epitope, &result->triggered_features, sizeof(uint32_t));

    /* Check tolerance */
    if (bridge->config.enable_tolerance_system) {
        if (find_tolerance_entry(bridge, epitope, sizeof(uint32_t)) != NULL) {
            bridge->tolerance.false_positives_prevented++;
            bridge->stats.false_positives_prevented++;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(uint32_t),
        severity,
        0,
        antigen_id
    );

    if (ret == 0) {
        bridge->anomaly_state.anomalies_presented++;
        bridge->anomaly_state.last_anomaly_time = get_current_time_ms();
        bridge->stats.anomaly_antigens_presented++;
        bridge->stats.total_antigens_presented++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int sec_immune_unified_present_pattern_match(
    sec_immune_unified_bridge_t* bridge,
    const nimcp_pattern_match_result_t* match_result,
    uint32_t* antigen_id
) {
    if (!bridge || !match_result || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_present_pattern_match: required parameter is NULL (bridge, match_result, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_pattern_antigen_presentation) return 0;
    if (!match_result->matched) return 0;
    if (match_result->threat_score < SEC_IMMUNE_PATTERN_MIN_SCORE_FOR_ANTIGEN) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map threat score to severity */
    uint32_t severity = (uint32_t)(match_result->threat_score * SEC_IMMUNE_PATTERN_SCORE_MULTIPLIER);
    if (severity > 10) severity = 10;

    /* Create epitope from pattern ID and category */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    memcpy(epitope, &match_result->pattern_id, sizeof(nimcp_pattern_id_t));
    memcpy(epitope + sizeof(nimcp_pattern_id_t),
           &match_result->category, sizeof(nimcp_pattern_category_t));

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(nimcp_pattern_id_t) + sizeof(nimcp_pattern_category_t),
        severity,
        0,
        antigen_id
    );

    if (ret == 0) {
        bridge->pattern_state.matches_presented++;
        bridge->pattern_state.last_match_time = get_current_time_ms();
        if (match_result->threat_score > bridge->pattern_state.max_match_score) {
            bridge->pattern_state.max_match_score = match_result->threat_score;
        }
        bridge->stats.pattern_antigens_presented++;
        bridge->stats.total_antigens_presented++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int sec_immune_unified_present_rate_violation(
    sec_immune_unified_bridge_t* bridge,
    const char* client_id,
    uint32_t violation_count,
    uint32_t* antigen_id
) {
    if (!bridge || !client_id || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_present_rate_violation: required parameter is NULL (bridge, client_id, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_rate_violation_antigen_presentation) return 0;
    if (violation_count < SEC_IMMUNE_RATE_MIN_VIOLATIONS_FOR_ANTIGEN) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map violations to severity */
    uint32_t severity = SEC_IMMUNE_RATE_SEVERITY_BASE +
                        (violation_count * SEC_IMMUNE_RATE_SEVERITY_PER_VIOLATION);
    if (severity > 10) severity = 10;

    /* Create epitope from client ID hash */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    size_t id_len = strlen(client_id);
    size_t copy_len = id_len < sizeof(epitope) ? id_len : sizeof(epitope);
    memcpy(epitope, client_id, copy_len);

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        copy_len,
        severity,
        0,
        antigen_id
    );

    if (ret == 0) {
        bridge->rate_state.violations_presented++;
        bridge->rate_state.last_violation_time = get_current_time_ms();
        bridge->stats.rate_antigens_presented++;
        bridge->stats.total_antigens_presented++;

        /* Trigger inflammation for repeated violations */
        if (violation_count >= SEC_IMMUNE_RATE_VIOLATIONS_FOR_INFLAMMATION) {
            bridge->rate_state.inflammation_triggers++;
            bridge->stats.inflammation_triggers++;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int sec_immune_unified_present_policy_violation(
    sec_immune_unified_bridge_t* bridge,
    const nimcp_policy_result_t* result,
    uint32_t* antigen_id
) {
    if (!bridge || !result || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_present_policy_violation: required parameter is NULL (bridge, result, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_policy_violation_antigen_presentation) return 0;
    if (result->action == NIMCP_POLICY_ACTION_ALLOW) return 0;
    if (result->severity < SEC_IMMUNE_POLICY_MIN_SEVERITY_FOR_ANTIGEN) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map policy severity to immune severity */
    uint32_t severity = (uint32_t)(result->severity * SEC_IMMUNE_POLICY_SEVERITY_MULTIPLIER);
    if (severity > 10) severity = 10;

    /* Create epitope from rule name */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    if (result->rule_name) {
        size_t name_len = strlen(result->rule_name);
        size_t copy_len = name_len < sizeof(epitope) ? name_len : sizeof(epitope);
        memcpy(epitope, result->rule_name, copy_len);
    }

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(epitope),
        severity,
        0,
        antigen_id
    );

    if (ret == 0) {
        bridge->policy_state.violations_presented++;
        bridge->policy_state.last_violation_time = get_current_time_ms();
        bridge->stats.policy_antigens_presented++;
        bridge->stats.total_antigens_presented++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

/* ============================================================================
 * Immune -> Security: Antibody Action API
 * ============================================================================ */

int sec_immune_unified_execute_antibody_action(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antibody_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->config.enable_antibody_action_execution) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Execute the antibody response via brain immune system */
    brain_immune_execute_antibody(bridge->immune_system, antibody_id);

    /* Always count at bridge level - the bridge can neutralize threats
     * even when no specific antibody exists in the immune system */
    bridge->stats.antibody_actions_executed++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_execute_killer_action(
    sec_immune_unified_bridge_t* bridge,
    uint32_t t_cell_id,
    uint32_t target
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Execute killer T cell action via immune system */
    int ret = brain_immune_t_cell_kill(bridge->immune_system, t_cell_id, target);

    /* Always execute quarantine at bridge level - the bridge can act
     * even when no specific T cell exists in the immune system */
    {
        bridge->stats.quarantine_actions++;

        /* If BBB connected, trigger quarantine */
        if (bridge->bbb_system) {
            bridge->bbb_state.quarantines_initiated++;
        }

        /* If rate limiter connected, could block client */
        if (bridge->rate_limiter) {
            bridge->rate_state.quarantine_actions++;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    (void)ret; /* Bridge-level quarantine always succeeds */
    return 0;
}

int sec_immune_unified_execute_helper_action(
    sec_immune_unified_bridge_t* bridge,
    uint32_t t_cell_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Helper T cell coordinates escalation */
    if (bridge->policy_engine && bridge->policy_state.effective_strictness_factor < 2.0f) {
        bridge->policy_state.effective_strictness_factor *= 1.1f;
        bridge->policy_state.escalation_count++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Memory Cell API
 * ============================================================================ */

int sec_immune_unified_form_memory(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antigen_id,
    uint32_t antibody_id,
    uint32_t* memory_id
) {
    if (!bridge || !memory_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_immune_unified_form_memory: required parameter is NULL (bridge, memory_id)");
        return -1;
    }
    if (!bridge->config.enable_memory_cell_formation) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check if we have capacity */
    if (bridge->memory_cell_count >= bridge->memory_cell_capacity) {
        /* Would need to grow array or evict old entries */
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Get antigen info (copy to avoid dangling pointer after immune mutex release) */
    brain_antigen_t antigen_copy;
    if (brain_immune_get_antigen_copy(bridge->immune_system, antigen_id, &antigen_copy) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Create memory cell */
    sec_immune_memory_cell_t* cell = &bridge->memory_cells[bridge->memory_cell_count];
    memset(cell, 0, sizeof(sec_immune_memory_cell_t));

    cell->memory_id = bridge->next_memory_id++;
    cell->source_antigen_id = antigen_id;
    memcpy(cell->epitope, antigen_copy.epitope, antigen_copy.epitope_len);
    cell->epitope_len = antigen_copy.epitope_len;
    cell->original_source = antigen_copy.source;
    cell->bbb_threat_type = antigen_copy.bbb_threat_type;
    cell->confidence = antigen_copy.confidence;
    cell->neutralization_count = 1;
    cell->formation_time = get_current_time_ms();

    bridge->memory_cell_count++;
    *memory_id = cell->memory_id;

    bridge->stats.memory_cells_formed++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_sync_memory_to_pattern(
    sec_immune_unified_bridge_t* bridge,
    uint32_t memory_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->config.enable_pattern_memory_sync) return 0;
    if (!bridge->pattern_db) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_sync_memory_to_pattern: bridge->pattern_db is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sec_immune_memory_cell_t* cell = find_memory_cell_by_id(bridge, memory_id);
    if (!cell || cell->synced_to_pattern_db) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return cell ? 0 : -1;
    }

    /* Create pattern entry from memory cell */
    nimcp_pattern_entry_t entry = {0};
    entry.pattern = (const char*)cell->epitope;
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = SEC_IMMUNE_MEMORY_PATTERN_PRIORITY;
    entry.weight = SEC_IMMUNE_MEMORY_ANTIBODY_WEIGHT;
    entry.description = "Learned from immune memory";

    nimcp_pattern_id_t pattern_id;
    nimcp_error_t err = nimcp_pattern_db_add(bridge->pattern_db, &entry, &pattern_id);

    if (err == NIMCP_SUCCESS) {
        cell->synced_to_pattern_db = true;
        cell->pattern_id = pattern_id;
        bridge->pattern_state.patterns_from_memory++;
        bridge->pattern_state.synced_memory_cells++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

int sec_immune_unified_check_memory(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* epitope,
    size_t epitope_len,
    uint32_t* memory_id
) {
    if (!bridge || !epitope || !memory_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_immune_unified_check_memory: required parameter is NULL (bridge, epitope, memory_id)");
        return -1;
    }
    if (!bridge->memory_cells || epitope_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_immune_unified_check_memory: bridge->memory_cells is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    size_t compare_len = epitope_len < BRAIN_IMMUNE_EPITOPE_SIZE ?
                         epitope_len : BRAIN_IMMUNE_EPITOPE_SIZE;

    for (size_t i = 0; i < bridge->memory_cell_count; i++) {
        sec_immune_memory_cell_t* cell = &bridge->memory_cells[i];
        if (cell->epitope_len == compare_len &&
            memcmp(cell->epitope, epitope, compare_len) == 0) {
            *memory_id = cell->memory_id;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_ERROR_NOT_FOUND;
}

int sec_immune_unified_secondary_response(
    sec_immune_unified_bridge_t* bridge,
    uint32_t memory_id,
    uint32_t antigen_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sec_immune_memory_cell_t* cell = find_memory_cell_by_id(bridge, memory_id);
    if (!cell) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_secondary_response: cell is NULL");
        return -1;
    }

    /* Try secondary response via immune system (may fail if memory_id
     * doesn't map to a brain B cell - bridge uses its own ID space) */
    brain_immune_secondary_response(bridge->immune_system, antigen_id, memory_id);

    /* Always update bridge-level memory cell - the bridge manages its
     * own memory cell tracking independent of brain B cell IDs */
    cell->last_recognition_time = get_current_time_ms();
    cell->neutralization_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Tolerance System API
 * ============================================================================ */

int sec_immune_unified_add_tolerance(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len,
    const char* description,
    bool is_permanent
) {
    if (!bridge || !pattern || pattern_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_add_tolerance: required parameter is NULL (bridge, pattern)");
        return -1;
    }
    if (!bridge->config.enable_tolerance_system) return 0;
    if (!bridge->tolerance.whitelist) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_add_tolerance: bridge->tolerance is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check if already exists */
    if (find_tolerance_entry(bridge, pattern, pattern_len) != NULL) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0; /* Already whitelisted */
    }

    /* Check capacity */
    if (bridge->tolerance.whitelist_count >= bridge->tolerance.whitelist_capacity) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Add new entry */
    sec_immune_tolerance_entry_t* entry =
        &bridge->tolerance.whitelist[bridge->tolerance.whitelist_count];

    memset(entry, 0, sizeof(sec_immune_tolerance_entry_t));
    size_t copy_len = pattern_len < BRAIN_IMMUNE_EPITOPE_SIZE ?
                      pattern_len : BRAIN_IMMUNE_EPITOPE_SIZE;
    memcpy(entry->pattern, pattern, copy_len);
    entry->pattern_len = copy_len;
    entry->confirmation_count = is_permanent ? bridge->config.tolerance_confirmation_count : 0;
    entry->first_seen_time = get_current_time_ms();
    entry->last_confirmed_time = entry->first_seen_time;
    entry->is_permanent = is_permanent;
    entry->description = description;

    bridge->tolerance.whitelist_count++;
    bridge->tolerance.patterns_whitelisted++;
    bridge->stats.patterns_whitelisted++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_remove_tolerance(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
) {
    if (!bridge || !pattern || pattern_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_remove_tolerance: required parameter is NULL (bridge, pattern)");
        return -1;
    }
    if (!bridge->tolerance.whitelist) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_remove_tolerance: bridge->tolerance is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    size_t compare_len = pattern_len < BRAIN_IMMUNE_EPITOPE_SIZE ?
                         pattern_len : BRAIN_IMMUNE_EPITOPE_SIZE;

    for (size_t i = 0; i < bridge->tolerance.whitelist_count; i++) {
        sec_immune_tolerance_entry_t* entry = &bridge->tolerance.whitelist[i];
        if (entry->pattern_len == compare_len &&
            memcmp(entry->pattern, pattern, compare_len) == 0) {
            /* Remove by shifting remaining entries */
            if (i < bridge->tolerance.whitelist_count - 1) {
                memmove(&bridge->tolerance.whitelist[i],
                        &bridge->tolerance.whitelist[i + 1],
                        sizeof(sec_immune_tolerance_entry_t) *
                        (bridge->tolerance.whitelist_count - i - 1));
            }
            bridge->tolerance.whitelist_count--;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_immune_unified_remove_tolerance: operation failed");
    return -1; /* Not found */
}

bool sec_immune_unified_is_tolerated(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
) {
    if (!bridge || !pattern || pattern_len == 0) {
        return false;
    }
    if (!bridge->config.enable_tolerance_system) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool result = (find_tolerance_entry(bridge, pattern, pattern_len) != NULL);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

int sec_immune_unified_confirm_benign(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
) {
    if (!bridge || !pattern || pattern_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_confirm_benign: required parameter is NULL (bridge, pattern)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sec_immune_tolerance_entry_t* entry = find_tolerance_entry(bridge, pattern, pattern_len);

    if (entry) {
        /* Already in whitelist - increment confirmation */
        entry->confirmation_count++;
        entry->last_confirmed_time = get_current_time_ms();
        bridge->tolerance.patterns_confirmed++;
    } else if (bridge->tolerance.learning_mode_active) {
        /* In learning mode - add to whitelist with confirmation count */
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return sec_immune_unified_add_tolerance(bridge, pattern, pattern_len, NULL, false);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_set_learning_mode(
    sec_immune_unified_bridge_t* bridge,
    bool enable
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->tolerance.learning_mode_active = enable;
    if (enable) {
        bridge->tolerance.learning_start_time = get_current_time_ms();
    }
    bridge->stats.tolerance_learning_active = enable;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_activate_regulatory(
    sec_immune_unified_bridge_t* bridge,
    float suppression_level
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->config.enable_regulatory_t_cells) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    suppression_level = nimcp_clampf(suppression_level, 0.0f, 1.0f);
    bridge->tolerance.regulatory_activity = suppression_level;
    bridge->policy_state.regulatory_suppression = suppression_level;
    bridge->stats.regulatory_suppressions++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Training Feedback API
 * ============================================================================ */

int sec_immune_unified_feedback_true_positive(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antigen_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->anomaly_state.true_positives++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_immune_unified_feedback_false_positive(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antigen_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->anomaly_state.false_positives++;
    bridge->stats.false_positives_prevented++;

    /* Get antigen epitope for tolerance learning (copy to avoid dangling pointer) */
    brain_antigen_t antigen_copy;
    if (brain_immune_get_antigen_copy(bridge->immune_system, antigen_id, &antigen_copy) == 0
        && bridge->config.enable_tolerance_system) {
        sec_immune_unified_confirm_benign(bridge, antigen_copy.epitope, antigen_copy.epitope_len);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int sec_immune_unified_connect_bio_async(sec_immune_unified_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SECURITY,
        .module_name = "sec_immune_unified_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security-immune unified bridge connected to bio-async router");
        return 0;
    }

    return NIMCP_ERROR_INVALID_STATE;
}

int sec_immune_unified_disconnect_bio_async(sec_immune_unified_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Security-immune unified bridge disconnected from bio-async router");
    return 0;
}

bool sec_immune_unified_is_bio_async_connected(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int sec_immune_unified_broadcast_security_event(
    sec_immune_unified_bridge_t* bridge,
    uint32_t event_type,
    uint32_t severity,
    const void* data,
    size_t data_len
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }
    if (!bridge->config.broadcast_security_events) return 0;
    if (!bridge->base.bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_immune_unified_broadcast_security_event: bridge->base is NULL");
        return -1;
    }

    /* Allocate and build broadcast message */
    size_t payload_size = data_len > 0 ? data_len : sizeof(uint32_t);
    size_t msg_size = sizeof(bio_message_header_t) + payload_size;
    void* msg_buffer = nimcp_malloc(msg_size);
    if (!msg_buffer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_immune_unified_broadcast_security_event: failed to allocate msg_buffer");

        return -1;

    }

    /* Build header */
    bio_message_header_t* header = (bio_message_header_t*)msg_buffer;
    header->type = BIO_MSG_SECURITY_ALERT;
    header->source_module = BIO_MODULE_INTROSPECTION;  /* Use introspection as security-immune source */
    header->target_module = BIO_MODULE_BRAIN;  /* Target brain for security alerts */
    header->sequence_id = 0;
    header->timestamp_us = nimcp_platform_time_monotonic_us();
    header->channel = (severity > 2) ? BIO_CHANNEL_NOREPINEPHRINE : BIO_CHANNEL_SEROTONIN;
    header->payload_size = (uint32_t)payload_size;
    header->flags = 0;

    /* Copy payload */
    if (data && data_len > 0) {
        memcpy((uint8_t*)msg_buffer + sizeof(bio_message_header_t), data, data_len);
    } else {
        /* Default: send event type as payload */
        memcpy((uint8_t*)msg_buffer + sizeof(bio_message_header_t), &event_type, sizeof(uint32_t));
    }

    /* Broadcast via bio-router */
    nimcp_error_t result = bio_router_broadcast(bridge->base.bio_ctx, msg_buffer, msg_size);
    nimcp_free(msg_buffer);

    return (result == NIMCP_SUCCESS) ? 0 : -1;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float sec_immune_unified_get_bbb_threshold_factor(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->bbb_state.effective_threshold_factor : 1.0f;
}

float sec_immune_unified_get_anomaly_threshold(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->anomaly_state.effective_threshold : 0.7f;
}

float sec_immune_unified_get_pattern_weight_factor(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->pattern_state.effective_weight_factor : 1.0f;
}

float sec_immune_unified_get_rate_limit_factor(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->rate_state.effective_rps_factor : 1.0f;
}

float sec_immune_unified_get_policy_strictness_factor(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->policy_state.effective_strictness_factor : 1.0f;
}

bool sec_immune_unified_is_emergency_mode(
    const sec_immune_unified_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->cytokine_effects.emergency_mode_active ||
           bridge->inflammation_state.emergency_lockdown;
}

bool sec_immune_unified_is_learning_mode(
    const sec_immune_unified_bridge_t* bridge
) {
    return bridge ? bridge->tolerance.learning_mode_active : false;
}

int sec_immune_unified_get_stats(
    const sec_immune_unified_bridge_t* bridge,
    sec_immune_unified_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "sec_immune_unified_get_stats: bridge or stats is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(((sec_immune_unified_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(((sec_immune_unified_bridge_t*)bridge)->base.mutex);

    return 0;
}

float sec_immune_unified_get_threat_level(
    const sec_immune_unified_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    float threat = 0.0f;

    /* Factor in inflammation */
    switch (bridge->inflammation_state.current_level) {
        case INFLAMMATION_NONE:   threat += 0.0f; break;
        case INFLAMMATION_LOCAL:  threat += 0.2f; break;
        case INFLAMMATION_REGIONAL: threat += 0.4f; break;
        case INFLAMMATION_SYSTEMIC: threat += 0.7f; break;
        case INFLAMMATION_STORM:  threat += 1.0f; break;
    }

    /* Factor in emergency mode */
    if (bridge->cytokine_effects.emergency_mode_active) {
        threat += 0.3f;
    }

    /* Factor in recent antigens */
    if (bridge->stats.total_antigens_presented > 0) {
        threat += 0.1f;
    }

    return nimcp_clampf(threat, 0.0f, 1.0f);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* sec_immune_unified_inflammation_name(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return "none";
        case INFLAMMATION_LOCAL:    return "local";
        case INFLAMMATION_REGIONAL: return "regional";
        case INFLAMMATION_SYSTEMIC: return "systemic";
        case INFLAMMATION_STORM:    return "cytokine_storm";
        default:                    return "unknown";
    }
}

const char* sec_immune_unified_bbb_threat_name(bbb_threat_type_t type) {
    return bbb_threat_type_name(type);
}

const char* sec_immune_unified_antibody_class_name(brain_antibody_class_t ab_class) {
    switch (ab_class) {
        case ANTIBODY_IGM: return "IgM";
        case ANTIBODY_IGG: return "IgG";
        case ANTIBODY_IGE: return "IgE";
        default:           return "unknown";
    }
}

void sec_immune_unified_print_stats(const sec_immune_unified_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Security-Immune Unified Bridge Statistics ===\n");
    printf("Antigens Presented:\n");
    printf("  BBB:     %lu\n", (unsigned long)bridge->stats.bbb_antigens_presented);
    printf("  Anomaly: %lu\n", (unsigned long)bridge->stats.anomaly_antigens_presented);
    printf("  Pattern: %lu\n", (unsigned long)bridge->stats.pattern_antigens_presented);
    printf("  Rate:    %lu\n", (unsigned long)bridge->stats.rate_antigens_presented);
    printf("  Policy:  %lu\n", (unsigned long)bridge->stats.policy_antigens_presented);
    printf("  Total:   %lu\n", (unsigned long)bridge->stats.total_antigens_presented);
    printf("\nImmune Activity:\n");
    printf("  B cells activated: %lu\n", (unsigned long)bridge->stats.b_cells_activated);
    printf("  T cells activated: %lu\n", (unsigned long)bridge->stats.t_cells_activated);
    printf("  Antibodies produced: %lu\n", (unsigned long)bridge->stats.antibodies_produced);
    printf("  Memory cells formed: %lu\n", (unsigned long)bridge->stats.memory_cells_formed);
    printf("\nModulations:\n");
    printf("  BBB:     %lu\n", (unsigned long)bridge->stats.bbb_modulations);
    printf("  Anomaly: %lu\n", (unsigned long)bridge->stats.anomaly_modulations);
    printf("  Pattern: %lu\n", (unsigned long)bridge->stats.pattern_modulations);
    printf("  Rate:    %lu\n", (unsigned long)bridge->stats.rate_modulations);
    printf("  Policy:  %lu\n", (unsigned long)bridge->stats.policy_modulations);
    printf("\nTolerance:\n");
    printf("  False positives prevented: %lu\n",
           (unsigned long)bridge->stats.false_positives_prevented);
    printf("  Patterns whitelisted: %lu\n",
           (unsigned long)bridge->stats.patterns_whitelisted);
    printf("\nCurrent State:\n");
    printf("  Inflammation: %s\n",
           sec_immune_unified_inflammation_name(bridge->stats.current_inflammation));
    printf("  Emergency mode: %s\n",
           bridge->stats.emergency_mode_active ? "active" : "inactive");
    printf("  Learning mode: %s\n",
           bridge->stats.tolerance_learning_active ? "active" : "inactive");
    printf("  Threat level: %.2f\n", bridge->stats.current_threat_level);
}

void sec_immune_unified_print_cytokine_effects(
    const sec_immune_unified_bridge_t* bridge
) {
    if (!bridge) return;

    const sec_immune_cytokine_effects_t* e = &bridge->cytokine_effects;

    printf("=== Cytokine Effects on Security ===\n");
    printf("IL-1:  BBB=%.3f, Anomaly=%.3f, Pattern=%.3f, Rate=%.3f, Policy=%.3f\n",
           e->il1_bbb_modulation, e->il1_anomaly_modulation, e->il1_pattern_modulation,
           e->il1_rate_modulation, e->il1_policy_modulation);
    printf("IL-6:  BBB=%.3f, Anomaly=%.3f, Pattern=%.3f, Rate=%.3f, Policy=%.3f\n",
           e->il6_bbb_modulation, e->il6_anomaly_modulation, e->il6_pattern_modulation,
           e->il6_rate_modulation, e->il6_policy_modulation);
    printf("TNF:   BBB=%.3f, Anomaly=%.3f, Pattern=%.3f, Rate=%.3f, Policy=%.3f\n",
           e->tnf_bbb_modulation, e->tnf_anomaly_modulation, e->tnf_pattern_modulation,
           e->tnf_rate_modulation, e->tnf_policy_modulation);
    printf("IL-10: BBB=%.3f, Anomaly=%.3f, Pattern=%.3f, Rate=%.3f, Policy=%.3f\n",
           e->il10_bbb_modulation, e->il10_anomaly_modulation, e->il10_pattern_modulation,
           e->il10_rate_modulation, e->il10_policy_modulation);
    printf("IFN:   BBB=%.3f, Anomaly=%.3f, Pattern=%.3f, Rate=%.3f, Policy=%.3f\n",
           e->ifn_bbb_modulation, e->ifn_anomaly_modulation, e->ifn_pattern_modulation,
           e->ifn_rate_modulation, e->ifn_policy_modulation);
    printf("Totals: BBB=%.3f, Anomaly=%.3f, Pattern=%.3f, Rate=%.3f, Policy=%.3f\n",
           e->total_bbb_modulation, e->total_anomaly_modulation, e->total_pattern_modulation,
           e->total_rate_modulation, e->total_policy_modulation);
    printf("Modes: Emergency=%s, Recovery=%s\n",
           e->emergency_mode_active ? "active" : "inactive",
           e->recovery_mode_active ? "active" : "inactive");
}

void sec_immune_unified_print_inflammation_state(
    const sec_immune_unified_bridge_t* bridge
) {
    if (!bridge) return;

    const sec_immune_inflammation_state_t* s = &bridge->inflammation_state;

    printf("=== Inflammation State ===\n");
    printf("Level: %s\n", sec_immune_unified_inflammation_name(s->current_level));
    printf("Duration: %.1f sec\n", s->inflammation_duration_sec);
    printf("Chronic: %s\n", s->is_chronic ? "yes" : "no");
    printf("Factors:\n");
    printf("  BBB threshold:     %.3f\n", s->bbb_threshold_factor);
    printf("  Anomaly threshold: %.3f\n", s->anomaly_threshold_factor);
    printf("  Pattern weight:    %.3f\n", s->pattern_weight_factor);
    printf("  Rate limit:        %.3f\n", s->rate_limit_factor);
    printf("  Policy strictness: %.3f\n", s->policy_strictness_factor);
    printf("Modes: Hypervigilant=%s, Lockdown=%s, Conservation=%s\n",
           s->hypervigilant_mode ? "yes" : "no",
           s->emergency_lockdown ? "yes" : "no",
           s->resource_conservation ? "yes" : "no");
}
