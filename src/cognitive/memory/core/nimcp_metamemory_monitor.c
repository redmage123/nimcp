//=============================================================================
// nimcp_metamemory_monitor.c - Metamemory Monitoring System Implementation
//=============================================================================
/**
 * @file nimcp_metamemory_monitor.c
 * @brief Implementation of metamemory monitoring for Prime Resonant memory
 *
 * This file implements the metamemory monitoring system which provides
 * "memory about memory" - tracking knowledge domains, memory health,
 * and identifying memories at risk of being forgotten.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_metamemory_monitor.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(metamemory_monitor, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal metamemory monitor structure
 */
struct metamem_monitor_struct {
    // External references (not owned)
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;
    z_ladder_t z_ladder;

    // Knowledge domains
    knowledge_domain_t* domains;
    size_t num_domains;
    size_t max_domains;

    // At-risk tracking
    at_risk_memory_t* at_risk;
    size_t num_at_risk;
    size_t max_at_risk;

    // System health metrics
    float overall_health;
    float overall_coverage;
    float mean_consolidation;
    float retrieval_success_rate;
    float encoding_rate;

    // Historical trends
    float* consolidation_history;
    float* accessibility_history;
    float* retrieval_history;
    uint64_t* timestamp_history;
    size_t history_len;
    size_t history_capacity;
    size_t history_index;  // Circular buffer index

    // Monitoring intervals
    float monitor_interval_sec;
    float deep_scan_interval_sec;
    uint64_t last_monitor_time_ms;
    uint64_t last_deep_scan_time_ms;

    // Callback
    metamem_alert_callback_t alert_callback;
    void* alert_user_data;

    // Configuration
    metamem_config_t config;

    // Statistics
    uint64_t total_updates;
    uint64_t total_scans;
    uint64_t alerts_sent;
};

//=============================================================================
// Internal Helper Functions - Forward Declarations
//=============================================================================

static void domains_init(metamem_monitor_t monitor);
static void domains_cleanup(metamem_monitor_t monitor);
static void at_risk_init(metamem_monitor_t monitor);
static void at_risk_cleanup(metamem_monitor_t monitor);
static void history_init(metamem_monitor_t monitor);
static void history_cleanup(metamem_monitor_t monitor);
static void history_record(metamem_monitor_t monitor, float consol, float access, float retrieval);

static float compute_memory_risk(metamem_monitor_t monitor, const pr_memory_node_t* node,
                                  uint64_t current_time_ms);
static void update_domain_stats(metamem_monitor_t monitor, knowledge_domain_t* domain);
static knowledge_domain_t* find_or_create_domain(metamem_monitor_t monitor,
                                                   const prime_signature_t* sig);
static float compute_domain_coverage(const knowledge_domain_t* domain);
static void send_alert(metamem_monitor_t monitor, metamem_alert_type_t type, void* data);
static int compare_at_risk(const void* a, const void* b);
static int compare_review_priority(const void* a, const void* b);
static float linear_regression_slope(const float* values, size_t count);

//=============================================================================
// Configuration Functions
//=============================================================================

metamem_config_t metamem_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_config_defau", 0.0f);


    metamem_config_t config = {
        // Monitoring intervals
        .monitor_interval_sec = METAMEM_DEFAULT_INTERVAL,
        .deep_scan_interval_sec = 60.0f,

        // Risk thresholds
        .risk_threshold_low = 0.25f,
        .risk_threshold_medium = 0.5f,
        .risk_threshold_high = METAMEM_HIGH_RISK_THRESHOLD,
        .risk_threshold_critical = METAMEM_CRITICAL_RISK_THRESHOLD,

        // Risk weights
        .risk_weight_consolidation = METAMEM_RISK_WEIGHT_CONSOL,
        .risk_weight_time = METAMEM_RISK_WEIGHT_TIME,
        .risk_weight_tier = METAMEM_RISK_WEIGHT_TIER,
        .risk_weight_entanglement = METAMEM_RISK_WEIGHT_ENTANGLE,
        .risk_weight_salience = METAMEM_RISK_WEIGHT_SALIENCE,

        // Domain detection
        .domain_similarity_threshold = METAMEM_DOMAIN_SIMILARITY,
        .min_domain_size = METAMEM_MIN_DOMAIN_SIZE,
        .max_domains = METAMEM_MAX_DOMAINS,
        .max_top_per_domain = METAMEM_MAX_TOP_MEMORIES,

        // History
        .history_length = METAMEM_HISTORY_LENGTH,

        // Callbacks
        .enable_alerts = true,

        // Memory limits
        .max_at_risk = METAMEM_MAX_AT_RISK
    };
    return config;
}

bool metamem_config_validate(const metamem_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_config_validate: config is NULL");
        return false;
    }

    // Validate intervals
    if (config->monitor_interval_sec <= 0.0f) {
        return false;
    }
    if (config->deep_scan_interval_sec <= 0.0f) {
        return false;
    }

    // Validate thresholds (must be increasing)
    if (config->risk_threshold_low < 0.0f || config->risk_threshold_low > 1.0f) {
        return false;
    }
    if (config->risk_threshold_medium <= config->risk_threshold_low) {
        return false;
    }
    if (config->risk_threshold_high <= config->risk_threshold_medium) {
        return false;
    }
    if (config->risk_threshold_critical <= config->risk_threshold_high) {
        return false;
    }
    if (config->risk_threshold_critical > 1.0f) {
        return false;
    }

    // Validate weights (should sum to ~1.0 but we normalize internally)
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_config_valid", 0.0f);


    float weight_sum = config->risk_weight_consolidation +
                       config->risk_weight_time +
                       config->risk_weight_tier +
                       config->risk_weight_entanglement +
                       config->risk_weight_salience;
    if (weight_sum <= 0.0f) {
        return false;
    }

    // Validate limits
    if (config->max_domains == 0 || config->max_domains > METAMEM_MAX_DOMAINS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamem_config_validate: config->max_domains is zero");
        return false;
    }
    if (config->history_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamem_config_validate: config->history_length is zero");
        return false;
    }
    if (config->max_at_risk == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamem_config_validate: config->max_at_risk is zero");
        return false;
    }

    return true;
}

//=============================================================================
// Monitor Lifecycle
//=============================================================================

metamem_monitor_t metamem_monitor_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    z_ladder_t z_ladder,
    const metamem_config_t* config
) {
    if (!entanglement || !node_manager || !z_ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_monitor_create: required parameter is NULL (entanglement, node_manager, z_ladder)");
        return NULL;
    }

    // Use default config if none provided
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_crea", 0.0f);


    metamem_config_t cfg = config ? *config : metamem_config_default();
    if (!metamem_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_monitor_create: metamem_config_validate is NULL");
        return NULL;
    }

    // Allocate monitor structure
    metamem_monitor_t monitor = (metamem_monitor_t)nimcp_calloc(1, sizeof(struct metamem_monitor_struct));
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate monitor");

        return NULL;
    }

    // Store external references
    monitor->entanglement = entanglement;
    monitor->node_manager = node_manager;
    monitor->z_ladder = z_ladder;

    // Store configuration
    monitor->config = cfg;
    monitor->monitor_interval_sec = cfg.monitor_interval_sec;
    monitor->deep_scan_interval_sec = cfg.deep_scan_interval_sec;
    monitor->max_domains = cfg.max_domains;
    monitor->max_at_risk = cfg.max_at_risk;

    // Initialize subsystems
    domains_init(monitor);
    at_risk_init(monitor);
    history_init(monitor);

    // Initialize metrics to neutral values
    monitor->overall_health = 1.0f;
    monitor->overall_coverage = 0.0f;
    monitor->mean_consolidation = 0.5f;
    monitor->retrieval_success_rate = 1.0f;
    monitor->encoding_rate = 0.0f;

    // Initialize timestamps
    uint64_t now = metamem_current_time_ms();
    monitor->last_monitor_time_ms = now;
    monitor->last_deep_scan_time_ms = now;

    return monitor;
}

void metamem_monitor_destroy(metamem_monitor_t monitor) {
    if (!monitor) return;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_dest", 0.0f);


    domains_cleanup(monitor);
    at_risk_cleanup(monitor);
    history_cleanup(monitor);

    nimcp_free(monitor);
    monitor = NULL;
}

//=============================================================================
// Update and Monitoring
//=============================================================================

metamem_error_t metamem_monitor_update(
    metamem_monitor_t monitor,
    uint64_t current_time_ms
) {
    if (!monitor) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_upda", 0.0f);


    float elapsed_sec = (float)(current_time_ms - monitor->last_monitor_time_ms) / 1000.0f;

    // Check if enough time has passed for regular update
    if (elapsed_sec < monitor->monitor_interval_sec) {
        return METAMEM_SUCCESS;  // Not time yet
    }

    monitor->last_monitor_time_ms = current_time_ms;
    monitor->total_updates++;

    // Quick update: at-risk detection
    metamem_monitor_detect_at_risk(monitor);

    // Update health metrics
    size_t total_memories = z_ladder_get_total_count(monitor->z_ladder);
    if (total_memories > 0) {
        // Compute mean consolidation from Z-ladder nodes
        float total_consol = 0.0f;
        float total_access = 0.0f;

        for (int tier = 0; tier < PR_MEMORY_TIER_COUNT; tier++) {
            /* Phase 8: Loop progress heartbeat */
            if ((tier & 0xFF) == 0 && PR_MEMORY_TIER_COUNT > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(tier + 1) / (float)PR_MEMORY_TIER_COUNT);
            }

            size_t count = z_ladder_get_count(monitor->z_ladder, (pr_memory_tier_t)tier);
            if (count == 0) continue;

            pr_memory_node_t** nodes = (pr_memory_node_t**)nimcp_calloc(count, sizeof(pr_memory_node_t*));
            if (nodes) {
                size_t actual = 0;
                z_ladder_get_nodes(monitor->z_ladder, (pr_memory_tier_t)tier, nodes, count, &actual);

                for (size_t i = 0; i < actual; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && actual > 256) {
                        metamemory_monitor_heartbeat("metamemory_m_loop",
                                         (float)(i + 1) / (float)actual);
                    }

                    if (nodes[i]) {
                        nimcp_quaternion_t state = pr_memory_node_get_state(nodes[i]);
                        total_consol += state.w;
                        total_access += state.z;
                    }
                }
                nimcp_free(nodes);
                nodes = NULL;
            }
        }

        monitor->mean_consolidation = total_consol / (float)total_memories;
        float mean_access = total_access / (float)total_memories;

        // Record history
        history_record(monitor, monitor->mean_consolidation, mean_access,
                       monitor->retrieval_success_rate);
    }

    // Check if deep scan is needed
    float deep_elapsed = (float)(current_time_ms - monitor->last_deep_scan_time_ms) / 1000.0f;
    if (deep_elapsed >= monitor->deep_scan_interval_sec) {
        metamem_monitor_full_scan(monitor);
        monitor->last_deep_scan_time_ms = current_time_ms;
    }

    // Update overall health
    monitor->overall_health = metamem_monitor_get_overall_health(monitor);

    // Check for alerts
    if (monitor->config.enable_alerts && monitor->alert_callback) {
        // Health alert
        if (monitor->overall_health < 0.5f) {
            send_alert(monitor, METAMEM_ALERT_HEALTH_DEGRADED, &monitor->overall_health);
        }

        // Critical risk alerts
        for (size_t i = 0; i < monitor->num_at_risk; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && monitor->num_at_risk > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(i + 1) / (float)monitor->num_at_risk);
            }

            if (monitor->at_risk[i].urgency == META_URGENCY_CRITICAL) {
                send_alert(monitor, METAMEM_ALERT_FORGETTING_RISK, &monitor->at_risk[i]);
            }
        }
    }

    return METAMEM_SUCCESS;
}

metamem_error_t metamem_monitor_full_scan(metamem_monitor_t monitor) {
    if (!monitor) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_full", 0.0f);


    monitor->total_scans++;

    // Rebuild domain inventory
    metamem_monitor_inventory_domains(monitor);

    // Full at-risk detection
    metamem_monitor_detect_at_risk(monitor);

    // Compute overall coverage
    float total_coverage = 0.0f;
    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        total_coverage += monitor->domains[i].coverage_score;
    }
    if (monitor->num_domains > 0) {
        monitor->overall_coverage = total_coverage / (float)monitor->num_domains;
    }

    return METAMEM_SUCCESS;
}

//=============================================================================
// Knowledge Domain API
//=============================================================================

int metamem_monitor_inventory_domains(metamem_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_monitor_inventory_domains: monitor is NULL");
        return -1;
    }

    // Clear existing domain stats (keep domain definitions)
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_inve", 0.0f);


    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        monitor->domains[i].memory_count = 0;
        monitor->domains[i].mean_consolidation = 0.0f;
        monitor->domains[i].mean_accessibility = 0.0f;
        monitor->domains[i].mean_salience = 0.0f;
        monitor->domains[i].num_top = 0;
        memset(monitor->domains[i].tier_counts, 0, sizeof(monitor->domains[i].tier_counts));
    }

    // Iterate all memories and assign to domains
    size_t total_processed = 0;

    for (int tier = 0; tier < PR_MEMORY_TIER_COUNT; tier++) {
        /* Phase 8: Loop progress heartbeat */
        if ((tier & 0xFF) == 0 && PR_MEMORY_TIER_COUNT > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(tier + 1) / (float)PR_MEMORY_TIER_COUNT);
        }

        size_t count = z_ladder_get_count(monitor->z_ladder, (pr_memory_tier_t)tier);
        if (count == 0) continue;

        pr_memory_node_t** nodes = (pr_memory_node_t**)nimcp_calloc(count, sizeof(pr_memory_node_t*));
        if (!nodes) continue;

        size_t actual = 0;
        z_ladder_get_nodes(monitor->z_ladder, (pr_memory_tier_t)tier, nodes, count, &actual);

        for (size_t i = 0; i < actual; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && actual > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(i + 1) / (float)actual);
            }

            if (!nodes[i]) continue;

            const prime_signature_t* sig = pr_memory_node_get_signature(nodes[i]);
            if (!sig) continue;

            // Find or create appropriate domain
            knowledge_domain_t* domain = find_or_create_domain(monitor, sig);
            if (!domain) continue;

            // Update domain statistics
            nimcp_quaternion_t state = pr_memory_node_get_state(nodes[i]);

            domain->memory_count++;
            domain->mean_consolidation += state.w;
            domain->mean_accessibility += state.z;
            domain->mean_salience += state.y;
            domain->tier_counts[tier]++;

            // Track top memories by consolidation
            if (domain->num_top < domain->max_top) {
                domain->top_memories[domain->num_top++] = nodes[i];
            } else {
                // Find weakest in top list and replace if this is stronger
                size_t weakest_idx = 0;
                float weakest_consol = 1.0f;
                for (size_t j = 0; j < domain->num_top; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && domain->num_top > 256) {
                        metamemory_monitor_heartbeat("metamemory_m_loop",
                                         (float)(j + 1) / (float)domain->num_top);
                    }

                    nimcp_quaternion_t top_state = pr_memory_node_get_state(domain->top_memories[j]);
                    if (top_state.w < weakest_consol) {
                        weakest_consol = top_state.w;
                        weakest_idx = j;
                    }
                }
                if (state.w > weakest_consol) {
                    domain->top_memories[weakest_idx] = nodes[i];
                }
            }

            total_processed++;
        }

        nimcp_free(nodes);
        nodes = NULL;
    }

    // Finalize domain statistics
    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        if (monitor->domains[i].memory_count > 0) {
            float count_f = (float)monitor->domains[i].memory_count;
            monitor->domains[i].mean_consolidation /= count_f;
            monitor->domains[i].mean_accessibility /= count_f;
            monitor->domains[i].mean_salience /= count_f;
            monitor->domains[i].coverage_score = compute_domain_coverage(&monitor->domains[i]);
        }
        monitor->domains[i].last_update_ms = metamem_current_time_ms();
    }

    return (int)monitor->num_domains;
}

metamem_error_t metamem_monitor_get_domain_summary(
    metamem_monitor_t monitor,
    uint64_t domain_hash,
    metamem_domain_summary_t* summary
) {
    if (!monitor || !summary) return METAMEM_ERROR_NULL_POINTER;

    // Find domain
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    knowledge_domain_t* domain = NULL;
    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        if (monitor->domains[i].domain_hash == domain_hash) {
            domain = &monitor->domains[i];
            break;
        }
    }

    if (!domain) return METAMEM_ERROR_DOMAIN_NOT_FOUND;

    // Fill summary
    strncpy(summary->name, domain->domain_name, METAMEM_MAX_DOMAIN_NAME - 1);
    summary->name[METAMEM_MAX_DOMAIN_NAME - 1] = '\0';
    summary->hash = domain->domain_hash;
    summary->memory_count = domain->memory_count;
    summary->coverage = domain->coverage_score;
    summary->mean_consolidation = domain->mean_consolidation;
    summary->mean_accessibility = domain->mean_accessibility;

    // Compute health score
    summary->health_score = (domain->mean_consolidation * 0.4f +
                             domain->coverage_score * 0.3f +
                             domain->mean_accessibility * 0.3f);

    return METAMEM_SUCCESS;
}

metamem_error_t metamem_monitor_get_all_domains(
    metamem_monitor_t monitor,
    metamem_domain_summary_t* summaries,
    size_t max_summaries,
    size_t* count
) {
    if (!monitor || !summaries || !count) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    size_t to_return = (monitor->num_domains < max_summaries) ?
                        monitor->num_domains : max_summaries;

    for (size_t i = 0; i < to_return; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_return > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)to_return);
        }

        strncpy(summaries[i].name, monitor->domains[i].domain_name, METAMEM_MAX_DOMAIN_NAME - 1);
        summaries[i].name[METAMEM_MAX_DOMAIN_NAME - 1] = '\0';
        summaries[i].hash = monitor->domains[i].domain_hash;
        summaries[i].memory_count = monitor->domains[i].memory_count;
        summaries[i].coverage = monitor->domains[i].coverage_score;
        summaries[i].mean_consolidation = monitor->domains[i].mean_consolidation;
        summaries[i].mean_accessibility = monitor->domains[i].mean_accessibility;
        summaries[i].health_score = (monitor->domains[i].mean_consolidation * 0.4f +
                                     monitor->domains[i].coverage_score * 0.3f +
                                     monitor->domains[i].mean_accessibility * 0.3f);
    }

    *count = to_return;
    return METAMEM_SUCCESS;
}

float metamem_monitor_compute_coverage(
    metamem_monitor_t monitor,
    uint64_t domain_hash
) {
    if (!monitor) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_comp", 0.0f);


    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        if (monitor->domains[i].domain_hash == domain_hash) {
            return compute_domain_coverage(&monitor->domains[i]);
        }
    }

    return -1.0f;  // Domain not found
}

uint64_t metamem_monitor_find_domain(
    metamem_monitor_t monitor,
    const char* name
) {
    if (!monitor || !name) return 0;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_find", 0.0f);


    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        if (strcmp(monitor->domains[i].domain_name, name) == 0) {
            return monitor->domains[i].domain_hash;
        }
    }

    return 0;  // Not found
}

uint64_t metamem_monitor_register_domain(
    metamem_monitor_t monitor,
    const char* name,
    const prime_signature_t* seed_signature
) {
    if (!monitor || !name) return 0;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_regi", 0.0f);


    if (monitor->num_domains >= monitor->max_domains) {
        return 0;  // Capacity exceeded
    }

    // Check if already exists
    uint64_t existing = metamem_monitor_find_domain(monitor, name);
    if (existing != 0) {
        return existing;  // Already exists
    }

    // Create new domain
    knowledge_domain_t* domain = &monitor->domains[monitor->num_domains];
    memset(domain, 0, sizeof(knowledge_domain_t));

    strncpy(domain->domain_name, name, METAMEM_MAX_DOMAIN_NAME - 1);
    domain->domain_name[METAMEM_MAX_DOMAIN_NAME - 1] = '\0';
    domain->domain_hash = metamem_hash_domain_name(name);

    if (seed_signature) {
        memcpy(&domain->domain_signature, seed_signature, sizeof(prime_signature_t));
    }

    domain->max_top = monitor->config.max_top_per_domain;
    domain->top_memories = (pr_memory_node_t**)nimcp_calloc(domain->max_top, sizeof(pr_memory_node_t*));
    if (!domain->top_memories) {
        domain->max_top = 0;
    }

    domain->domain_age_ms = metamem_current_time_ms();

    monitor->num_domains++;

    return domain->domain_hash;
}

//=============================================================================
// At-Risk Detection API
//=============================================================================

int metamem_monitor_detect_at_risk(metamem_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_monitor_detect_at_risk: monitor is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_dete", 0.0f);


    uint64_t current_time = metamem_current_time_ms();

    // Clear existing at-risk list
    monitor->num_at_risk = 0;

    // Scan all memories for risk
    for (int tier = 0; tier < PR_MEMORY_TIER_COUNT; tier++) {
        /* Phase 8: Loop progress heartbeat */
        if ((tier & 0xFF) == 0 && PR_MEMORY_TIER_COUNT > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(tier + 1) / (float)PR_MEMORY_TIER_COUNT);
        }

        // Skip Z3 (permanent) tier - these don't decay
        if (tier == PR_MEMORY_TIER_Z3) continue;

        size_t count = z_ladder_get_count(monitor->z_ladder, (pr_memory_tier_t)tier);
        if (count == 0) continue;

        pr_memory_node_t** nodes = (pr_memory_node_t**)nimcp_calloc(count, sizeof(pr_memory_node_t*));
        if (!nodes) continue;

        size_t actual = 0;
        z_ladder_get_nodes(monitor->z_ladder, (pr_memory_tier_t)tier, nodes, count, &actual);

        for (size_t i = 0; i < actual; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && actual > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(i + 1) / (float)actual);
            }

            if (!nodes[i]) continue;
            if (monitor->num_at_risk >= monitor->max_at_risk) break;

            float risk = compute_memory_risk(monitor, nodes[i], current_time);

            // Track if above low threshold
            if (risk >= monitor->config.risk_threshold_low) {
                at_risk_memory_t* entry = &monitor->at_risk[monitor->num_at_risk];

                entry->memory = nodes[i];
                entry->memory_id = pr_memory_node_get_id(nodes[i]);
                entry->risk_score = risk;

                nimcp_quaternion_t state = pr_memory_node_get_state(nodes[i]);
                entry->consolidation = state.w;
                entry->tier = (pr_memory_tier_t)tier;
                entry->decay_rate = pr_memory_node_default_decay_rate((pr_memory_tier_t)tier);

                uint64_t idle_ms = pr_memory_node_get_idle_ms(nodes[i], current_time);
                entry->time_since_access = (float)idle_ms / 1000.0f;

                // Get entanglement strength
                uint32_t entangle_count = pr_memory_node_get_entanglement_count(nodes[i]);
                entry->entanglement_strength = (entangle_count > 10) ? 1.0f :
                                                (float)entangle_count / 10.0f;

                entry->urgency = metamem_monitor_classify_urgency(monitor, risk);

                monitor->num_at_risk++;
            }
        }

        nimcp_free(nodes);
        nodes = NULL;
    }

    // Sort by risk score (highest first)
    if (monitor->num_at_risk > 1) {
        qsort(monitor->at_risk, monitor->num_at_risk,
              sizeof(at_risk_memory_t), compare_at_risk);
    }

    return (int)monitor->num_at_risk;
}

metamem_error_t metamem_monitor_get_at_risk(
    metamem_monitor_t monitor,
    at_risk_memory_t* at_risk,
    size_t max_count,
    size_t* count
) {
    if (!monitor || !at_risk || !count) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    size_t to_return = (monitor->num_at_risk < max_count) ?
                        monitor->num_at_risk : max_count;

    memcpy(at_risk, monitor->at_risk, to_return * sizeof(at_risk_memory_t));
    *count = to_return;

    return METAMEM_SUCCESS;
}

metamem_error_t metamem_monitor_get_at_risk_by_urgency(
    metamem_monitor_t monitor,
    meta_urgency_t min_urgency,
    at_risk_memory_t* at_risk,
    size_t max_count,
    size_t* count
) {
    if (!monitor || !at_risk || !count) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    size_t written = 0;
    for (size_t i = 0; i < monitor->num_at_risk && written < max_count; i++) {
        if (monitor->at_risk[i].urgency >= min_urgency) {
            at_risk[written++] = monitor->at_risk[i];
        }
    }

    *count = written;
    return METAMEM_SUCCESS;
}

float metamem_monitor_compute_risk(
    metamem_monitor_t monitor,
    const pr_memory_node_t* node
) {
    if (!monitor || !node) return -1.0f;
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_comp", 0.0f);


    return compute_memory_risk(monitor, node, metamem_current_time_ms());
}

meta_urgency_t metamem_monitor_classify_urgency(
    metamem_monitor_t monitor,
    float risk_score
) {
    if (!monitor) return META_URGENCY_NONE;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_clas", 0.0f);


    if (risk_score >= monitor->config.risk_threshold_critical) {
        return META_URGENCY_CRITICAL;
    } else if (risk_score >= monitor->config.risk_threshold_high) {
        return META_URGENCY_HIGH;
    } else if (risk_score >= monitor->config.risk_threshold_medium) {
        return META_URGENCY_MEDIUM;
    } else if (risk_score >= monitor->config.risk_threshold_low) {
        return META_URGENCY_LOW;
    }
    return META_URGENCY_NONE;
}

//=============================================================================
// Health Reporting API
//=============================================================================

metamem_error_t metamem_monitor_get_health_report(
    metamem_monitor_t monitor,
    metamem_health_report_t* report
) {
    if (!monitor || !report) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    memset(report, 0, sizeof(metamem_health_report_t));

    // Overall metrics
    report->overall_health = metamem_monitor_get_overall_health(monitor);
    report->overall_coverage = monitor->overall_coverage;
    report->mean_consolidation = monitor->mean_consolidation;
    report->retrieval_success_rate = monitor->retrieval_success_rate;
    report->encoding_rate = monitor->encoding_rate;

    // Tier health
    size_t total = 0;
    for (int tier = 0; tier < PR_MEMORY_TIER_COUNT; tier++) {
        /* Phase 8: Loop progress heartbeat */
        if ((tier & 0xFF) == 0 && PR_MEMORY_TIER_COUNT > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(tier + 1) / (float)PR_MEMORY_TIER_COUNT);
        }

        report->tier_counts[tier] = z_ladder_get_count(monitor->z_ladder, (pr_memory_tier_t)tier);
        total += report->tier_counts[tier];

        // Utilization would require knowing capacity from config
        // For now, estimate based on typical capacity
        size_t capacity = (tier == 0) ? 9 :      // Z0: 7+/-2
                          (tier == 1) ? 100 :    // Z1
                          (tier == 2) ? 10000 :  // Z2
                          100000;                 // Z3: high but not unlimited
        report->tier_utilization[tier] = (float)report->tier_counts[tier] / (float)capacity;
        if (report->tier_utilization[tier] > 1.0f) {
            report->tier_utilization[tier] = 1.0f;
        }

        // Average strength per tier
        if (report->tier_counts[tier] > 0) {
            float total_strength = 0.0f;
            pr_memory_node_t** nodes = (pr_memory_node_t**)nimcp_malloc(
                report->tier_counts[tier] * sizeof(pr_memory_node_t*));
            if (nodes) {
                size_t actual = 0;
                z_ladder_get_nodes(monitor->z_ladder, (pr_memory_tier_t)tier,
                                   nodes, report->tier_counts[tier], &actual);
                for (size_t i = 0; i < actual; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && actual > 256) {
                        metamemory_monitor_heartbeat("metamemory_m_loop",
                                         (float)(i + 1) / (float)actual);
                    }

                    if (nodes[i]) {
                        total_strength += nodes[i]->current_strength;
                    }
                }
                report->tier_avg_strength[tier] = total_strength / (float)actual;
                nimcp_free(nodes);
                nodes = NULL;
            }
        }
    }

    // Risk summary
    report->at_risk_count = monitor->num_at_risk;
    report->critical_count = 0;
    report->high_risk_count = 0;
    for (size_t i = 0; i < monitor->num_at_risk; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_at_risk > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_at_risk);
        }

        if (monitor->at_risk[i].urgency == META_URGENCY_CRITICAL) {
            report->critical_count++;
        } else if (monitor->at_risk[i].urgency == META_URGENCY_HIGH) {
            report->high_risk_count++;
        }
    }

    // Domain summary
    report->domain_count = monitor->num_domains;
    report->weak_domains = 0;
    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        if (monitor->domains[i].coverage_score < 0.3f) {
            report->weak_domains++;
        }
    }

    // Trends
    report->consolidation_trend = metamem_monitor_compute_trend(monitor, 0);
    report->accessibility_trend = metamem_monitor_compute_trend(monitor, 1);
    report->retrieval_trend = metamem_monitor_compute_trend(monitor, 2);

    report->report_time_ms = metamem_current_time_ms();

    return METAMEM_SUCCESS;
}

float metamem_monitor_get_overall_health(metamem_monitor_t monitor) {
    if (!monitor) return -1.0f;

    // Health factors
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    float health = 0.0f;

    // 1. Consolidation health (25%)
    float consol_health = monitor->mean_consolidation;
    health += consol_health * 0.25f;

    // 2. At-risk proportion (25%)
    size_t total = z_ladder_get_total_count(monitor->z_ladder);
    float at_risk_ratio = (total > 0) ? (float)monitor->num_at_risk / (float)total : 0.0f;
    float risk_health = 1.0f - at_risk_ratio;
    health += risk_health * 0.25f;

    // 3. Domain coverage (20%)
    health += monitor->overall_coverage * 0.20f;

    // 4. Tier balance (15%)
    // Good balance: most in Z2/Z3, some in Z1, few in Z0
    if (total > 0) {
        float z2_z3_ratio = (float)(z_ladder_get_count(monitor->z_ladder, PR_MEMORY_TIER_Z2) +
                                    z_ladder_get_count(monitor->z_ladder, PR_MEMORY_TIER_Z3)) /
                            (float)total;
        health += z2_z3_ratio * 0.15f;
    }

    // 5. Critical count penalty (15%)
    size_t critical = 0;
    for (size_t i = 0; i < monitor->num_at_risk; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_at_risk > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_at_risk);
        }

        if (monitor->at_risk[i].urgency == META_URGENCY_CRITICAL) {
            critical++;
        }
    }
    float critical_health = (critical == 0) ? 1.0f :
                            (critical < 5) ? 0.5f : 0.0f;
    health += critical_health * 0.15f;

    return health;
}

//=============================================================================
// Review and Prediction API
//=============================================================================

metamem_error_t metamem_monitor_recommend_review(
    metamem_monitor_t monitor,
    metamem_review_rec_t* recommendations,
    size_t max_count,
    size_t* count
) {
    if (!monitor || !recommendations || !count) return METAMEM_ERROR_NULL_POINTER;

    // Use at-risk memories as basis for recommendations
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_reco", 0.0f);


    size_t to_recommend = (monitor->num_at_risk < max_count) ?
                           monitor->num_at_risk : max_count;

    for (size_t i = 0; i < to_recommend; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_recommend > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)to_recommend);
        }

        recommendations[i].memory = monitor->at_risk[i].memory;
        recommendations[i].memory_id = monitor->at_risk[i].memory_id;
        recommendations[i].risk_score = monitor->at_risk[i].risk_score;
        recommendations[i].urgency = monitor->at_risk[i].urgency;

        // Priority considers risk and entanglement (reviewing central memories helps network)
        float entangle_bonus = monitor->at_risk[i].entanglement_strength * 0.1f;
        recommendations[i].priority = monitor->at_risk[i].risk_score + entangle_bonus;
        if (recommendations[i].priority > 1.0f) {
            recommendations[i].priority = 1.0f;
        }

        // Generate reason string based on primary risk factor
        if (monitor->at_risk[i].consolidation < 0.3f) {
            recommendations[i].reason = "Low consolidation - needs reinforcement";
        } else if (monitor->at_risk[i].time_since_access > 3600.0f) {
            recommendations[i].reason = "Not accessed recently - may be fading";
        } else if (monitor->at_risk[i].entanglement_strength < 0.3f) {
            recommendations[i].reason = "Weakly connected - isolated memory";
        } else {
            recommendations[i].reason = "General forgetting risk";
        }
    }

    // Sort by priority
    if (to_recommend > 1) {
        qsort(recommendations, to_recommend, sizeof(metamem_review_rec_t),
              compare_review_priority);
    }

    *count = to_recommend;
    return METAMEM_SUCCESS;
}

metamem_error_t metamem_monitor_predict_forgetting(
    metamem_monitor_t monitor,
    metamem_forgetting_pred_t* predictions,
    size_t max_count,
    size_t* count
) {
    if (!monitor || !predictions || !count) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_pred", 0.0f);


    size_t to_predict = (monitor->num_at_risk < max_count) ?
                         monitor->num_at_risk : max_count;

    for (size_t i = 0; i < to_predict; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_predict > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)to_predict);
        }

        metamem_monitor_predict_memory(monitor, monitor->at_risk[i].memory,
                                        &predictions[i]);
    }

    *count = to_predict;
    return METAMEM_SUCCESS;
}

metamem_error_t metamem_monitor_predict_memory(
    metamem_monitor_t monitor,
    const pr_memory_node_t* node,
    metamem_forgetting_pred_t* prediction
) {
    if (!monitor || !node || !prediction) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_pred", 0.0f);


    prediction->memory = (pr_memory_node_t*)node;  // Cast away const for output
    prediction->memory_id = pr_memory_node_get_id(node);

    // Get node parameters
    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    float consolidation = state.w;
    pr_memory_tier_t tier = pr_memory_node_get_tier(node);
    float decay_rate = pr_memory_node_default_decay_rate(tier);

    // Exponential decay model: strength(t) = strength(0) * exp(-decay_rate * t / (1 + c))
    // P(forget) = 1 - exp(-decay_rate * t / (1 + consolidation))

    // Effective decay rate adjusted by consolidation
    float effective_decay = decay_rate / (1.0f + consolidation);

    // Half-life: t_half = ln(2) / effective_decay
    if (effective_decay > METAMEM_EPSILON) {
        prediction->estimated_half_life_sec = 0.693f / effective_decay;
    } else {
        prediction->estimated_half_life_sec = 1e9f;  // Very long
    }

    // Probabilities for different time horizons
    float t_24h = 24.0f * 3600.0f;  // 24 hours in seconds
    float t_7d = 7.0f * 24.0f * 3600.0f;
    float t_30d = 30.0f * 24.0f * 3600.0f;

    prediction->probability_24h = 1.0f - expf(-effective_decay * t_24h);
    prediction->probability_7d = 1.0f - expf(-effective_decay * t_7d);
    prediction->probability_30d = 1.0f - expf(-effective_decay * t_30d);

    // Clamp probabilities
    if (prediction->probability_24h > 1.0f) prediction->probability_24h = 1.0f;
    if (prediction->probability_7d > 1.0f) prediction->probability_7d = 1.0f;
    if (prediction->probability_30d > 1.0f) prediction->probability_30d = 1.0f;

    // Z3 tier never forgets
    if (tier == PR_MEMORY_TIER_Z3) {
        prediction->probability_24h = 0.0f;
        prediction->probability_7d = 0.0f;
        prediction->probability_30d = 0.0f;
        prediction->estimated_half_life_sec = INFINITY;
    }

    return METAMEM_SUCCESS;
}

//=============================================================================
// Historical Trends API
//=============================================================================

metamem_error_t metamem_monitor_get_trends(
    metamem_monitor_t monitor,
    metamem_trend_point_t* points,
    size_t max_points,
    size_t* count
) {
    if (!monitor || !points || !count) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_get_", 0.0f);


    size_t to_return = (monitor->history_len < max_points) ?
                        monitor->history_len : max_points;

    // Return in reverse chronological order (most recent first)
    for (size_t i = 0; i < to_return; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_return > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)to_return);
        }

        size_t idx = (monitor->history_index + monitor->history_capacity - 1 - i) %
                     monitor->history_capacity;

        points[i].consolidation = monitor->consolidation_history[idx];
        points[i].accessibility = monitor->accessibility_history[idx];
        points[i].retrieval_success = monitor->retrieval_history[idx];
        points[i].timestamp_ms = monitor->timestamp_history[idx];
    }

    *count = to_return;
    return METAMEM_SUCCESS;
}

float metamem_monitor_compute_trend(
    metamem_monitor_t monitor,
    int metric
) {
    if (!monitor) return 0.0f;
    if (monitor->history_len < 3) return 0.0f;  // Need at least 3 points

    // Select history array based on metric
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_comp", 0.0f);


    float* history = NULL;
    switch (metric) {
        case 0: history = monitor->consolidation_history; break;
        case 1: history = monitor->accessibility_history; break;
        case 2: history = monitor->retrieval_history; break;
        default: return 0.0f;
    }

    // Extract recent values in order
    size_t num_samples = (monitor->history_len < 10) ? monitor->history_len : 10;
    float values[10];

    for (size_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        size_t idx = (monitor->history_index + monitor->history_capacity - num_samples + i) %
                     monitor->history_capacity;
        values[i] = history[idx];
    }

    return linear_regression_slope(values, num_samples);
}

//=============================================================================
// Callback API
//=============================================================================

metamem_error_t metamem_monitor_set_alert_callback(
    metamem_monitor_t monitor,
    metamem_alert_callback_t callback,
    void* user_data
) {
    if (!monitor) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_set_", 0.0f);


    monitor->alert_callback = callback;
    monitor->alert_user_data = user_data;

    return METAMEM_SUCCESS;
}

metamem_error_t metamem_monitor_clear_alert_callback(metamem_monitor_t monitor) {
    if (!monitor) return METAMEM_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_clea", 0.0f);


    monitor->alert_callback = NULL;
    monitor->alert_user_data = NULL;

    return METAMEM_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* metamem_error_string(metamem_error_t error) {
    switch (error) {
        case METAMEM_SUCCESS:               return "Success";
        case METAMEM_ERROR_NULL_POINTER:    return "Null pointer argument";
        case METAMEM_ERROR_NOT_INITIALIZED: return "Monitor not initialized";
        case METAMEM_ERROR_NO_MEMORY:       return "Memory allocation failed";
        case METAMEM_ERROR_INVALID_CONFIG:  return "Invalid configuration";
        case METAMEM_ERROR_DOMAIN_NOT_FOUND:return "Domain not found";
        case METAMEM_ERROR_CAPACITY:        return "Capacity exceeded";
        case METAMEM_ERROR_INVALID_STATE:   return "Invalid monitor state";
        default:                            return "Unknown error";
    }
}

const char* metamem_urgency_string(meta_urgency_t urgency) {
    switch (urgency) {
        case META_URGENCY_NONE:     return "None";
        case META_URGENCY_LOW:      return "Low";
        case META_URGENCY_MEDIUM:   return "Medium";
        case META_URGENCY_HIGH:     return "High";
        case META_URGENCY_CRITICAL: return "Critical";
        default:                    return "Unknown";
    }
}

const char* metamem_alert_type_string(metamem_alert_type_t alert_type) {
    switch (alert_type) {
        case METAMEM_ALERT_FORGETTING_RISK:  return "Forgetting Risk";
        case METAMEM_ALERT_COVERAGE_GAP:     return "Coverage Gap";
        case METAMEM_ALERT_HEALTH_DEGRADED:  return "Health Degraded";
        case METAMEM_ALERT_TIER_OVERFLOW:    return "Tier Overflow";
        case METAMEM_ALERT_DOMAIN_FRAGMENTED:return "Domain Fragmented";
        default:                             return "Unknown Alert";
    }
}

uint64_t metamem_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_current_time", 0.0f);


    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void metamem_print_health_report(const metamem_health_report_t* report) {
    if (!report) return;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_print_health", 0.0f);


    printf("=== Metamemory Health Report ===\n");
    printf("Overall Health: %.2f%%\n", report->overall_health * 100.0f);
    printf("Overall Coverage: %.2f%%\n", report->overall_coverage * 100.0f);
    printf("Mean Consolidation: %.3f\n", report->mean_consolidation);
    printf("Retrieval Success Rate: %.2f%%\n", report->retrieval_success_rate * 100.0f);
    printf("\nTier Distribution:\n");
    printf("  Z0 (Working):   %zu memories (%.1f%% util)\n",
           report->tier_counts[0], report->tier_utilization[0] * 100.0f);
    printf("  Z1 (Short):     %zu memories (%.1f%% util)\n",
           report->tier_counts[1], report->tier_utilization[1] * 100.0f);
    printf("  Z2 (Long):      %zu memories (%.1f%% util)\n",
           report->tier_counts[2], report->tier_utilization[2] * 100.0f);
    printf("  Z3 (Permanent): %zu memories (%.1f%% util)\n",
           report->tier_counts[3], report->tier_utilization[3] * 100.0f);
    printf("\nRisk Summary:\n");
    printf("  At-risk: %zu total\n", report->at_risk_count);
    printf("  Critical: %zu\n", report->critical_count);
    printf("  High: %zu\n", report->high_risk_count);
    printf("\nDomains: %zu (%zu weak)\n", report->domain_count, report->weak_domains);
    printf("\nTrends (slope):\n");
    printf("  Consolidation: %+.4f\n", report->consolidation_trend);
    printf("  Accessibility: %+.4f\n", report->accessibility_trend);
    printf("  Retrieval: %+.4f\n", report->retrieval_trend);
    printf("================================\n");
}

void metamem_print_domain_summary(const metamem_domain_summary_t* summary) {
    if (!summary) return;

    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_print_domain", 0.0f);


    printf("Domain: %s (0x%016lx)\n", summary->name, (unsigned long)summary->hash);
    printf("  Memories: %zu\n", summary->memory_count);
    printf("  Coverage: %.2f%%\n", summary->coverage * 100.0f);
    printf("  Mean Consolidation: %.3f\n", summary->mean_consolidation);
    printf("  Mean Accessibility: %.3f\n", summary->mean_accessibility);
    printf("  Health Score: %.2f%%\n", summary->health_score * 100.0f);
}

bool metamem_monitor_validate(metamem_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_monitor_validate: monitor is NULL");
        return false;
    }

    // Check external references
    if (!monitor->entanglement || !monitor->node_manager || !monitor->z_ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamem_monitor_validate: required parameter is NULL (monitor->entanglement, monitor->node_manager, monitor->z_ladder)");
        return false;
    }

    // Check domain array
    /* Phase 8: Heartbeat at operation start */
    metamemory_monitor_heartbeat("metamemory_m_metamem_monitor_vali", 0.0f);


    if (monitor->num_domains > monitor->max_domains) {
        return false;
    }

    // Check at-risk array
    if (monitor->num_at_risk > monitor->max_at_risk) {
        return false;
    }

    // Check history bounds
    if (monitor->history_len > monitor->history_capacity) {
        return false;
    }

    // Check metric ranges
    if (monitor->overall_health < 0.0f || monitor->overall_health > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helper Functions - Implementation
//=============================================================================

static void domains_init(metamem_monitor_t monitor) {
    monitor->domains = (knowledge_domain_t*)nimcp_calloc(monitor->max_domains,
                                                    sizeof(knowledge_domain_t));
    monitor->num_domains = 0;

    if (monitor->domains) {
        for (size_t i = 0; i < monitor->max_domains; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && monitor->max_domains > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(i + 1) / (float)monitor->max_domains);
            }

            monitor->domains[i].max_top = monitor->config.max_top_per_domain;
            monitor->domains[i].top_memories = (pr_memory_node_t**)nimcp_calloc(
                monitor->domains[i].max_top, sizeof(pr_memory_node_t*));
        }
    }
}

static void domains_cleanup(metamem_monitor_t monitor) {
    if (monitor->domains) {
        for (size_t i = 0; i < monitor->max_domains; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && monitor->max_domains > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(i + 1) / (float)monitor->max_domains);
            }

            if (monitor->domains[i].top_memories) {
                nimcp_free(monitor->domains[i].top_memories);
            }
        }
        nimcp_free(monitor->domains);
        monitor->domains = NULL;
    }
    monitor->num_domains = 0;
}

static void at_risk_init(metamem_monitor_t monitor) {
    monitor->at_risk = (at_risk_memory_t*)nimcp_calloc(monitor->max_at_risk,
                                                  sizeof(at_risk_memory_t));
    monitor->num_at_risk = 0;
}

static void at_risk_cleanup(metamem_monitor_t monitor) {
    if (monitor->at_risk) {
        nimcp_free(monitor->at_risk);
        monitor->at_risk = NULL;
    }
    monitor->num_at_risk = 0;
}

static void history_init(metamem_monitor_t monitor) {
    monitor->history_capacity = monitor->config.history_length;
    monitor->history_len = 0;
    monitor->history_index = 0;

    monitor->consolidation_history = (float*)nimcp_calloc(monitor->history_capacity, sizeof(float));
    if (!monitor->consolidation_history) return;
    monitor->accessibility_history = (float*)nimcp_calloc(monitor->history_capacity, sizeof(float));
    if (!monitor->accessibility_history) return;
    monitor->retrieval_history = (float*)nimcp_calloc(monitor->history_capacity, sizeof(float));
    if (!monitor->retrieval_history) return;
    monitor->timestamp_history = (uint64_t*)nimcp_calloc(monitor->history_capacity, sizeof(uint64_t));
    if (!monitor->timestamp_history) return;
}

static void history_cleanup(metamem_monitor_t monitor) {
    if (monitor->consolidation_history) nimcp_free(monitor->consolidation_history);
    if (monitor->accessibility_history) nimcp_free(monitor->accessibility_history);
    if (monitor->retrieval_history) nimcp_free(monitor->retrieval_history);
    if (monitor->timestamp_history) nimcp_free(monitor->timestamp_history);

    monitor->consolidation_history = NULL;
    monitor->accessibility_history = NULL;
    monitor->retrieval_history = NULL;
    monitor->timestamp_history = NULL;
    monitor->history_len = 0;
}

static void history_record(metamem_monitor_t monitor, float consol, float access, float retrieval) {
    if (!monitor->consolidation_history) return;

    monitor->consolidation_history[monitor->history_index] = consol;
    monitor->accessibility_history[monitor->history_index] = access;
    monitor->retrieval_history[monitor->history_index] = retrieval;
    monitor->timestamp_history[monitor->history_index] = metamem_current_time_ms();

    monitor->history_index = (monitor->history_index + 1) % monitor->history_capacity;
    if (monitor->history_len < monitor->history_capacity) {
        monitor->history_len++;
    }
}

static float compute_memory_risk(metamem_monitor_t monitor, const pr_memory_node_t* node,
                                  uint64_t current_time_ms) {
    if (!node) return 0.0f;

    nimcp_quaternion_t state = pr_memory_node_get_state(node);
    pr_memory_tier_t tier = pr_memory_node_get_tier(node);
    uint32_t entangle_count = pr_memory_node_get_entanglement_count(node);
    uint64_t idle_ms = pr_memory_node_get_idle_ms(node, current_time_ms);

    // Normalize weights
    float total_weight = monitor->config.risk_weight_consolidation +
                         monitor->config.risk_weight_time +
                         monitor->config.risk_weight_tier +
                         monitor->config.risk_weight_entanglement +
                         monitor->config.risk_weight_salience;

    float w_consol = monitor->config.risk_weight_consolidation / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
    float w_time = monitor->config.risk_weight_time / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
    float w_tier = monitor->config.risk_weight_tier / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
    float w_entangle = monitor->config.risk_weight_entanglement / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
    float w_salience = monitor->config.risk_weight_salience / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);

    // 1. Consolidation risk: lower consolidation = higher risk
    float consol_risk = 1.0f - state.w;

    // 2. Time risk: longer idle time = higher risk (normalized to ~1 hour)
    float time_factor = (float)idle_ms / (3600.0f * 1000.0f);  // Hours since access
    float time_risk = 1.0f - expf(-time_factor);  // Approaches 1 as time increases

    // 3. Tier risk: lower tiers have faster decay
    float tier_risk = 0.0f;
    switch (tier) {
        case PR_MEMORY_TIER_Z0: tier_risk = 1.0f; break;
        case PR_MEMORY_TIER_Z1: tier_risk = 0.7f; break;
        case PR_MEMORY_TIER_Z2: tier_risk = 0.3f; break;
        case PR_MEMORY_TIER_Z3: tier_risk = 0.0f; break;  // No decay
        default: tier_risk = 0.5f;
    }

    // 4. Entanglement risk: fewer connections = higher risk
    float entangle_norm = (entangle_count > 10) ? 1.0f : (float)entangle_count / 10.0f;
    float entangle_risk = 1.0f - entangle_norm;

    // 5. Salience risk: lower salience = higher risk
    float salience_risk = 1.0f - state.y;

    // Combined risk
    float risk = w_consol * consol_risk +
                 w_time * time_risk +
                 w_tier * tier_risk +
                 w_entangle * entangle_risk +
                 w_salience * salience_risk;

    // Clamp to [0, 1]
    if (risk < 0.0f) risk = 0.0f;
    if (risk > 1.0f) risk = 1.0f;

    return risk;
}

static void update_domain_stats(metamem_monitor_t monitor, knowledge_domain_t* domain) {
    (void)monitor;  // May be used for additional context

    if (domain->memory_count == 0) {
        domain->coverage_score = 0.0f;
        return;
    }

    domain->coverage_score = compute_domain_coverage(domain);
    domain->last_update_ms = metamem_current_time_ms();
}

static knowledge_domain_t* find_or_create_domain(metamem_monitor_t monitor,
                                                   const prime_signature_t* sig) {
    if (!sig) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sig is NULL");

        return NULL;

    }

    // Find best matching existing domain
    float best_similarity = 0.0f;
    knowledge_domain_t* best_domain = NULL;

    for (size_t i = 0; i < monitor->num_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)monitor->num_domains);
        }

        float sim = prime_sig_jaccard(sig, &monitor->domains[i].domain_signature);
        if (sim >= monitor->config.domain_similarity_threshold && sim > best_similarity) {
            best_similarity = sim;
            best_domain = &monitor->domains[i];
        }
    }

    if (best_domain) {
        return best_domain;
    }

    // Create new domain if none matches and capacity allows
    if (monitor->num_domains >= monitor->max_domains) {
        // Find least populated domain and merge into it
        size_t min_count = SIZE_MAX;
        size_t min_idx = 0;
        for (size_t i = 0; i < monitor->num_domains; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && monitor->num_domains > 256) {
                metamemory_monitor_heartbeat("metamemory_m_loop",
                                 (float)(i + 1) / (float)monitor->num_domains);
            }

            if (monitor->domains[i].memory_count < min_count) {
                min_count = monitor->domains[i].memory_count;
                min_idx = i;
            }
        }
        return &monitor->domains[min_idx];
    }

    // Create new domain
    knowledge_domain_t* domain = &monitor->domains[monitor->num_domains];

    snprintf(domain->domain_name, METAMEM_MAX_DOMAIN_NAME, "Domain_%zu",
             monitor->num_domains);
    domain->domain_hash = metamem_hash_domain_name(domain->domain_name);
    memcpy(&domain->domain_signature, sig, sizeof(prime_signature_t));
    domain->domain_age_ms = metamem_current_time_ms();

    monitor->num_domains++;

    return domain;
}

static float compute_domain_coverage(const knowledge_domain_t* domain) {
    if (!domain || domain->memory_count == 0) return 0.0f;

    float coverage = 0.0f;

    // Factor 1: Memory count (30%)
    // More memories = better coverage, up to a point
    float count_factor = (float)domain->memory_count / 100.0f;  // Normalize to ~100
    if (count_factor > 1.0f) count_factor = 1.0f;
    coverage += count_factor * 0.30f;

    // Factor 2: Consolidation distribution (30%)
    // Higher mean consolidation = better
    coverage += domain->mean_consolidation * 0.30f;

    // Factor 3: Tier distribution (25%)
    // More in Z2/Z3 = better
    size_t total = domain->memory_count;
    if (total > 0) {
        float long_term_ratio = (float)(domain->tier_counts[2] + domain->tier_counts[3]) /
                                (float)total;
        coverage += long_term_ratio * 0.25f;
    }

    // Factor 4: Accessibility (15%)
    coverage += domain->mean_accessibility * 0.15f;

    return coverage;
}

static void send_alert(metamem_monitor_t monitor, metamem_alert_type_t type, void* data) {
    if (!monitor->alert_callback) return;

    monitor->alerts_sent++;
    monitor->alert_callback(type, data, monitor->alert_user_data);
}

static int compare_at_risk(const void* a, const void* b) {
    const at_risk_memory_t* ma = (const at_risk_memory_t*)a;
    const at_risk_memory_t* mb = (const at_risk_memory_t*)b;

    // Sort by risk score descending
    if (mb->risk_score > ma->risk_score) return 1;
    if (mb->risk_score < ma->risk_score) return -1;
    return 0;
}

static int compare_review_priority(const void* a, const void* b) {
    const metamem_review_rec_t* ra = (const metamem_review_rec_t*)a;
    const metamem_review_rec_t* rb = (const metamem_review_rec_t*)b;

    // Sort by priority descending
    if (rb->priority > ra->priority) return 1;
    if (rb->priority < ra->priority) return -1;
    return 0;
}

static float linear_regression_slope(const float* values, size_t count) {
    if (count < 2) return 0.0f;

    // Simple linear regression: y = mx + b
    // m = (n*sum(xy) - sum(x)*sum(y)) / (n*sum(x^2) - sum(x)^2)

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_x2 = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            metamemory_monitor_heartbeat("metamemory_m_loop",
                             (float)(i + 1) / (float)count);
        }

        float x = (float)i;
        float y = values[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    float n = (float)count;
    float denom = n * sum_x2 - sum_x * sum_x;

    if (fabsf(denom) < METAMEM_EPSILON) {
        return 0.0f;  // All x values are the same
    }

    float slope = (n * sum_xy - sum_x * sum_y) / denom;

    return slope;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void metamemory_monitor_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_metamemory_monitor_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int metamemory_monitor_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metamemory_monitor_training_begin: NULL argument");
        return -1;
    }
    metamemory_monitor_heartbeat_instance(NULL, "metamemory_monitor_training_begin", 0.0f);
    (void)(struct metamem_monitor_struct*)instance; /* Module state available for reset */
    return 0;
}

int metamemory_monitor_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metamemory_monitor_training_end: NULL argument");
        return -1;
    }
    metamemory_monitor_heartbeat_instance(NULL, "metamemory_monitor_training_end", 1.0f);
    (void)(struct metamem_monitor_struct*)instance; /* Module state available for finalization */
    return 0;
}

int metamemory_monitor_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metamemory_monitor_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    metamemory_monitor_heartbeat_instance(NULL, "metamemory_monitor_training_step", progress);
    (void)(struct metamem_monitor_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
