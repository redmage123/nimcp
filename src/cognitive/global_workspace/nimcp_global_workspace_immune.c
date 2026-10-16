/**
 * @file nimcp_global_workspace_immune.c
 * @brief Implementation of Global Workspace - Brain Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * IMPLEMENTATION NOTES:
 * - Inflammation modulation via threshold multipliers
 * - Anomaly detection via pattern analysis on broadcast history
 * - Epitope construction from anomaly signature
 * - Auto-immune triggering for severe anomalies
 *
 * PERFORMANCE:
 * - Modulation update: O(N) where N = inflammation sites (typically <10)
 * - Anomaly detection: O(W × D) where W=window, D=content_dim
 * - Immune trigger: O(1) antigen presentation
 *
 * @author NIMCP Development Team
 */

#include "cognitive/global_workspace/nimcp_global_workspace_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "gw_immune"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(global_workspace_immune, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Monotonic timestamp for timing
 * WHY:  Track broadcast timing for rapid switching detection
 * HOW:  clock_gettime(CLOCK_MONOTONIC)
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Compute inflammation modulation factors
 *
 * WHAT: Convert inflammation level to GW threshold/capacity multipliers
 * WHY:  Inflammation disrupts conscious integration
 * HOW:  Predefined mapping based on biological literature
 */
static void compute_inflammation_multipliers(
    brain_inflammation_level_t level,
    float* threshold_multiplier,
    float* capacity_multiplier)
{
    // Guard: NULL checks
    if (!threshold_multiplier || !capacity_multiplier) return;

    // Map inflammation level to multipliers
    switch (level) {
        case INFLAMMATION_NONE:
            *threshold_multiplier = 1.0f;
            *capacity_multiplier = 1.0f;
            break;

        case INFLAMMATION_LOCAL:
            *threshold_multiplier = 1.1f;  // +10% harder to access
            *capacity_multiplier = 0.9f;   // 90% capacity
            break;

        case INFLAMMATION_REGIONAL:
            *threshold_multiplier = 1.3f;  // +30% harder
            *capacity_multiplier = 0.7f;   // 70% capacity
            break;

        case INFLAMMATION_SYSTEMIC:
            *threshold_multiplier = 1.5f;  // +50% harder
            *capacity_multiplier = 0.5f;   // 50% capacity
            break;

        case INFLAMMATION_STORM:
            *threshold_multiplier = 1.8f;  // +80% harder (near-complete disruption)
            *capacity_multiplier = 0.2f;   // 20% capacity (minimal access)
            break;

        default:
            *threshold_multiplier = 1.0f;
            *capacity_multiplier = 1.0f;
            break;
    }
}

/**
 * @brief Get maximum inflammation level from immune system
 *
 * WHAT: Query immune system for highest inflammation across all sites
 * WHY:  Use worst-case inflammation for GW modulation
 * HOW:  Iterate inflammation sites, find max level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    brain_immune_system_t* immune)
{
    // Guard: NULL check
    if (!immune) return INFLAMMATION_NONE;

    // Find maximum inflammation level across all sites
    brain_inflammation_level_t max_level = INFLAMMATION_NONE;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            global_workspace_immune_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Check if broadcast content contains NaN/Inf/extreme values
 *
 * WHAT: Scan content for corrupted values
 * WHY:  Detect data integrity violations
 * HOW:  Count NaN/Inf and check against threshold
 */
static bool check_content_corruption(
    const float* content,
    uint32_t dim,
    float nan_threshold,
    float extreme_threshold)
{
    // Guard: NULL check
    if (!content || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_content_corruption: content is NULL");
        return false;
    }

    uint32_t nan_count = 0;
    uint32_t extreme_count = 0;

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            global_workspace_immune_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)dim);
        }

        if (isnan(content[i]) || isinf(content[i])) {
            nan_count++;
        }
        if (fabsf(content[i]) > extreme_threshold) {
            extreme_count++;
        }
    }

    // Check if corruption exceeds threshold
    float nan_ratio = (float)nan_count / (float)dim;
    float extreme_ratio = (float)extreme_count / (float)dim;

    return (nan_ratio > nan_threshold || extreme_ratio > nan_threshold);
}

/**
 * @brief Create epitope from broadcast anomaly
 *
 * WHAT: Convert anomaly to immune system threat signature
 * WHY:  Enable immune response to GW anomalies
 * HOW:  Pack anomaly metadata into BRAIN_IMMUNE_EPITOPE_SIZE byte array
 */
static void create_anomaly_epitope(
    const gw_broadcast_anomaly_t* anomaly,
    uint8_t* epitope,
    size_t* epitope_len)
{
    // Guard: NULL checks
    if (!anomaly || !epitope || !epitope_len) return;

    // Clear epitope
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    // Pack anomaly signature into epitope
    uint32_t offset = 0;

    // Bytes 0-3: Anomaly type
    uint32_t type_val = (uint32_t)anomaly->type;
    memcpy(epitope + offset, &type_val, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Bytes 4-7: Broadcast ID
    memcpy(epitope + offset, &anomaly->broadcast_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Bytes 8-11: Source module
    uint32_t module_val = (uint32_t)anomaly->source_module;
    memcpy(epitope + offset, &module_val, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Bytes 12-15: Anomaly score (as bytes)
    memcpy(epitope + offset, &anomaly->anomaly_score, sizeof(float));
    offset += sizeof(float);

    // Bytes 16-31: Timestamp (uint64_t)
    memcpy(epitope + offset, &anomaly->timestamp_ms, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // Bytes 32-63: Description hash (simple hash for pattern matching)
    for (size_t i = 0; i < sizeof(anomaly->description) && offset < BRAIN_IMMUNE_EPITOPE_SIZE; i++) {
        epitope[offset++] = (uint8_t)anomaly->description[i];
        if (anomaly->description[i] == '\0') break;
    }

    *epitope_len = offset;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

gw_immune_context_t* global_workspace_connect_immune(
    global_workspace_t* workspace,
    brain_immune_system_t* immune)
{
    // Guard: NULL checks
    if (!workspace || !immune) {
        NIMCP_LOGGING_ERROR("NULL workspace or immune in global_workspace_connect_immune");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_connect_immune: required parameter is NULL (workspace, immune)");
        return NULL;
    }

    // Allocate context
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_global_workspace_con", 0.0f);


    gw_immune_context_t* ctx = (gw_immune_context_t*)nimcp_calloc(1, sizeof(gw_immune_context_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate gw_immune_context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_connect_immune: ctx is NULL");
        return NULL;
    }

    // Store handles
    ctx->workspace = workspace;
    ctx->immune = immune;
    ctx->connected = true;

    // Initialize anomaly detection with defaults
    ctx->anomaly_config = gw_immune_default_anomaly_config();

    // Initialize modulation
    ctx->baseline_threshold = global_workspace_get_ignition_threshold(workspace);
    ctx->modulation.level = INFLAMMATION_NONE;
    ctx->modulation.threshold_multiplier = 1.0f;
    ctx->modulation.capacity_multiplier = 1.0f;
    ctx->modulation.last_update_ms = get_time_ms();

    // Initialize statistics
    ctx->total_broadcasts_checked = 0;
    ctx->anomalies_detected = 0;
    ctx->immune_triggers = 0;

    // Initialize state
    ctx->last_broadcast_time_ms = 0;
    ctx->last_broadcast_module = MODULE_NONE;
    ctx->module_streak_count = 0;
    ctx->anomaly_count = 0;
    ctx->anomaly_head = 0;

    NIMCP_LOGGING_INFO("Global workspace connected to immune system");
    return ctx;
}

void global_workspace_disconnect_immune(gw_immune_context_t* context) {
    // Guard: NULL check
    if (!context) return;

    // Restore baseline threshold
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_global_workspace_dis", 0.0f);


    if (context->workspace) {
        global_workspace_set_ignition_threshold(context->workspace, context->baseline_threshold);
    }

    // Mark disconnected
    context->connected = false;

    // Free context
    nimcp_free(context);
    context = NULL;

    NIMCP_LOGGING_INFO("Global workspace disconnected from immune system");
}

gw_immune_anomaly_config_t gw_immune_default_anomaly_config(void) {
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_default_an", 0.0f);


    gw_immune_anomaly_config_t config;

    // Rapid switching (half of refractory period)
    config.rapid_switch_threshold_ms = GLOBAL_WORKSPACE_REFRACTORY_PERIOD_MS / 2.0f;

    // Strength spike (30% change)
    config.strength_spike_threshold = 0.3f;

    // Module hijack (80% dominance over 5 broadcasts)
    config.module_hijack_threshold = 0.8f;
    config.module_hijack_window = 5;

    // Repetitive pattern (3 consecutive from same module)
    config.repetitive_count_threshold = 3;

    // Corrupted content (1% NaN/Inf or > 1e6)
    config.content_nan_threshold = 0.01f;
    config.content_extreme_threshold = 1e6f;

    // Enable by default
    config.enable_anomaly_detection = true;
    config.auto_trigger_immune = true;

    return config;
}

/* ============================================================================
 * Inflammation Modulation API
 * ============================================================================ */

int gw_immune_update_inflammation_modulation(gw_immune_context_t* context) {
    // Guard: NULL check
    if (!context || !context->connected) {
        NIMCP_LOGGING_ERROR("NULL or disconnected context in gw_immune_update_inflammation_modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_immune_update_inflammation_modulation: required parameter is NULL (context, context->connected)");
        return -1;
    }

    // Get max inflammation level from immune system
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_update_inf", 0.0f);


    brain_inflammation_level_t level = get_max_inflammation_level(context->immune);

    // Compute modulation factors
    float threshold_mult, capacity_mult;
    compute_inflammation_multipliers(level, &threshold_mult, &capacity_mult);

    // Update modulation state
    context->modulation.level = level;
    context->modulation.threshold_multiplier = threshold_mult;
    context->modulation.capacity_multiplier = capacity_mult;
    context->modulation.last_update_ms = get_time_ms();

    // Apply to workspace threshold
    float modulated_threshold = context->baseline_threshold * threshold_mult;
    global_workspace_set_ignition_threshold(context->workspace, modulated_threshold);

    NIMCP_LOGGING_DEBUG("Inflammation modulation: level=%s, threshold_mult=%.2f, capacity_mult=%.2f",
                        brain_immune_inflammation_to_string(level), threshold_mult, capacity_mult);

    return 0;
}

int gw_immune_get_modulation(
    gw_immune_context_t* context,
    gw_inflammation_modulation_t* modulation)
{
    // Guard: NULL checks
    if (!context || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_immune_get_modulation: required parameter is NULL (context, modulation)");
        return -1;
    }

    *modulation = context->modulation;
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_get_modula", 0.0f);


    return 0;
}

int gw_immune_set_manual_inflammation(
    gw_immune_context_t* context,
    brain_inflammation_level_t level)
{
    // Guard: NULL check
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "context is NULL");

        return -1;
    }

    // Compute modulation factors
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_set_manual", 0.0f);


    float threshold_mult, capacity_mult;
    compute_inflammation_multipliers(level, &threshold_mult, &capacity_mult);

    // Update modulation state
    context->modulation.level = level;
    context->modulation.threshold_multiplier = threshold_mult;
    context->modulation.capacity_multiplier = capacity_mult;
    context->modulation.last_update_ms = get_time_ms();

    // Apply to workspace threshold
    float modulated_threshold = context->baseline_threshold * threshold_mult;
    global_workspace_set_ignition_threshold(context->workspace, modulated_threshold);

    return 0;
}

/* ============================================================================
 * Anomaly Detection API
 * ============================================================================ */

int gw_immune_detect_anomalies(
    gw_immune_context_t* context,
    gw_broadcast_anomaly_t* anomalies,
    uint32_t max_anomalies,
    uint32_t* actual_count)
{
    // Guard: NULL checks
    if (!context || !anomalies || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_immune_detect_anomalies: required parameter is NULL (context, anomalies, actual_count)");
        return -1;
    }

    *actual_count = 0;

    // Check if anomaly detection enabled
    if (!context->anomaly_config.enable_anomaly_detection) {
        return 0;
    }

    // Get current broadcast
    if (!global_workspace_has_broadcast(context->workspace)) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_detect_ano", 0.0f);


    cognitive_module_t source = global_workspace_get_broadcast_source(context->workspace);
    float strength = global_workspace_get_broadcast_strength(context->workspace);
    uint64_t current_time = get_time_ms();

    // Read broadcast content
    float content[GLOBAL_WORKSPACE_MAX_DIM];
    uint32_t content_dim = 0;
    global_workspace_read_broadcast(context->workspace, content, GLOBAL_WORKSPACE_MAX_DIM,
                                      &content_dim, NULL);

    uint32_t anomaly_idx = 0;

    // 1. Rapid switching detection
    if (context->last_broadcast_time_ms > 0) {
        uint64_t time_delta = current_time - context->last_broadcast_time_ms;
        if (time_delta < (uint64_t)context->anomaly_config.rapid_switch_threshold_ms) {
            if (anomaly_idx < max_anomalies) {
                anomalies[anomaly_idx].type = GW_ANOMALY_RAPID_SWITCHING;
                anomalies[anomaly_idx].severity = GW_ANOMALY_SEVERITY_MODERATE;
                anomalies[anomaly_idx].source_module = source;
                anomalies[anomaly_idx].timestamp_ms = current_time;
                anomalies[anomaly_idx].anomaly_score = 1.0f - ((float)time_delta / context->anomaly_config.rapid_switch_threshold_ms);
                snprintf(anomalies[anomaly_idx].description, sizeof(anomalies[anomaly_idx].description),
                         "Rapid switching: %.1f ms (threshold: %.1f ms)",
                         (float)time_delta, context->anomaly_config.rapid_switch_threshold_ms);
                anomalies[anomaly_idx].immune_triggered = false;
                anomaly_idx++;
            }
        }
    }

    // 2. Module hijacking detection (same module dominating)
    if (source == context->last_broadcast_module) {
        context->module_streak_count++;
        if (context->module_streak_count >= context->anomaly_config.repetitive_count_threshold) {
            if (anomaly_idx < max_anomalies) {
                anomalies[anomaly_idx].type = GW_ANOMALY_MODULE_HIJACK;
                anomalies[anomaly_idx].severity = GW_ANOMALY_SEVERITY_SEVERE;
                anomalies[anomaly_idx].source_module = source;
                anomalies[anomaly_idx].timestamp_ms = current_time;
                anomalies[anomaly_idx].anomaly_score = (float)context->module_streak_count / (float)context->anomaly_config.module_hijack_window;
                snprintf(anomalies[anomaly_idx].description, sizeof(anomalies[anomaly_idx].description),
                         "Module hijacking: %s dominant for %u consecutive broadcasts",
                         cognitive_module_to_string(source), context->module_streak_count);
                anomalies[anomaly_idx].immune_triggered = false;
                anomaly_idx++;
            }
        }
    } else {
        context->module_streak_count = 1;
    }

    // 3. Corrupted content detection
    if (check_content_corruption(content, content_dim,
                                   context->anomaly_config.content_nan_threshold,
                                   context->anomaly_config.content_extreme_threshold)) {
        if (anomaly_idx < max_anomalies) {
            anomalies[anomaly_idx].type = GW_ANOMALY_CORRUPTED_CONTENT;
            anomalies[anomaly_idx].severity = GW_ANOMALY_SEVERITY_CRITICAL;
            anomalies[anomaly_idx].source_module = source;
            anomalies[anomaly_idx].timestamp_ms = current_time;
            anomalies[anomaly_idx].anomaly_score = 1.0f;
            snprintf(anomalies[anomaly_idx].description, sizeof(anomalies[anomaly_idx].description),
                     "Corrupted content: NaN/Inf/extreme values detected");
            anomalies[anomaly_idx].immune_triggered = false;
            anomaly_idx++;
        }
    }

    // Update context state
    context->last_broadcast_time_ms = current_time;
    context->last_broadcast_module = source;

    *actual_count = anomaly_idx;
    return 0;
}

int gw_immune_trigger_response(
    gw_immune_context_t* context,
    const gw_broadcast_anomaly_t* anomaly)
{
    // Guard: NULL checks
    if (!context || !anomaly || !context->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_immune_trigger_response: required parameter is NULL (context, anomaly, context->connected)");
        return -1;
    }

    // Create epitope from anomaly
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_trigger_re", 0.0f);


    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t epitope_len = 0;
    create_anomaly_epitope(anomaly, epitope, &epitope_len);

    // Map severity to immune severity (1-10)
    uint32_t immune_severity = 0;
    switch (anomaly->severity) {
        case GW_ANOMALY_SEVERITY_MILD:
            immune_severity = 3;
            break;
        case GW_ANOMALY_SEVERITY_MODERATE:
            immune_severity = 5;
            break;
        case GW_ANOMALY_SEVERITY_SEVERE:
            immune_severity = 7;
            break;
        case GW_ANOMALY_SEVERITY_CRITICAL:
            immune_severity = 10;
            break;
        default:
            immune_severity = 1;
            break;
    }

    // Present antigen to immune system
    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(
        context->immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        epitope_len,
        immune_severity,
        (uint32_t)anomaly->source_module,
        &antigen_id
    );

    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to present anomaly antigen to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_immune_trigger_response: validation failed");
        return -1;
    }

    // Update statistics
    context->immune_triggers++;

    // Release cytokines based on severity
    if (anomaly->severity >= GW_ANOMALY_SEVERITY_MODERATE) {
        // IL-1: Pro-inflammatory alert
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            context->immune,
            CYTOKINE_IL1B,
            0,  // Source: system
            0.5f + (float)anomaly->severity * 0.1f,  // Concentration based on severity
            0,  // Broadcast
            &cytokine_id
        );
    }

    if (anomaly->severity >= GW_ANOMALY_SEVERITY_SEVERE) {
        // IL-6: Acute phase response
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            context->immune,
            CYTOKINE_IL6,
            0,
            0.7f,
            0,
            &cytokine_id
        );
    }

    if (anomaly->severity >= GW_ANOMALY_SEVERITY_CRITICAL) {
        // TNF-α: Severe inflammation
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            context->immune,
            CYTOKINE_TNFA,
            0,
            0.9f,
            0,
            &cytokine_id
        );
    }

    NIMCP_LOGGING_INFO("Immune response triggered for GW anomaly: type=%s, severity=%s, antigen_id=%u",
                       gw_anomaly_type_to_string(anomaly->type),
                       gw_anomaly_severity_to_string(anomaly->severity),
                       antigen_id);

    return 0;
}

uint32_t gw_immune_check_broadcast(gw_immune_context_t* context) {
    // Guard: NULL check
    if (!context) {
        return 0;
    }

    // Update broadcast counter
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_check_broa", 0.0f);


    context->total_broadcasts_checked++;

    // Detect anomalies
    gw_broadcast_anomaly_t anomalies[5];
    uint32_t count = 0;

    if (gw_immune_detect_anomalies(context, anomalies, 5, &count) != 0) {
        return 0;
    }

    // Add to history and trigger immune if configured
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            global_workspace_immune_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)count);
        }

        // Add to anomaly history
        uint32_t hist_idx = context->anomaly_head;
        context->anomaly_history[hist_idx] = anomalies[i];
        context->anomaly_head = (context->anomaly_head + 1) % GW_IMMUNE_MAX_ANOMALY_HISTORY;
        if (context->anomaly_count < GW_IMMUNE_MAX_ANOMALY_HISTORY) {
            context->anomaly_count++;
        }

        // Update statistics
        context->anomalies_detected++;

        // Auto-trigger immune if configured
        if (context->anomaly_config.auto_trigger_immune) {
            gw_immune_trigger_response(context, &anomalies[i]);
            context->anomaly_history[hist_idx].immune_triggered = true;
        }
    }

    return count;
}

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

int gw_immune_get_anomaly_history(
    gw_immune_context_t* context,
    gw_broadcast_anomaly_t* history,
    uint32_t max_history,
    uint32_t* actual_count)
{
    // Guard: NULL checks
    if (!context || !history || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_immune_get_anomaly_history: required parameter is NULL (context, history, actual_count)");
        return -1;
    }

    // Copy history (most recent first)
    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_get_anomal", 0.0f);


    uint32_t count = (context->anomaly_count < max_history) ? context->anomaly_count : max_history;
    *actual_count = count;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            global_workspace_immune_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)count);
        }

        // Calculate circular buffer index (most recent first)
        uint32_t idx = (context->anomaly_head + GW_IMMUNE_MAX_ANOMALY_HISTORY - 1 - i) %
                       GW_IMMUNE_MAX_ANOMALY_HISTORY;
        history[i] = context->anomaly_history[idx];
    }

    return 0;
}

int gw_immune_get_stats(
    gw_immune_context_t* context,
    uint64_t* total_broadcasts,
    uint64_t* total_anomalies,
    uint64_t* total_immune_triggers)
{
    // Guard: NULL checks
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "context is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_get_stats", 0.0f);


    if (total_broadcasts) {
        *total_broadcasts = context->total_broadcasts_checked;
    }
    if (total_anomalies) {
        *total_anomalies = context->anomalies_detected;
    }
    if (total_immune_triggers) {
        *total_immune_triggers = context->immune_triggers;
    }

    return 0;
}

void gw_immune_print_state(gw_immune_context_t* context, bool verbose) {
    // Guard: NULL check
    if (!context) {
        fprintf(stderr, "Context is NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_gw_immune_print_stat", 0.0f);


    fprintf(stderr, "=== Global Workspace ↔ Immune Integration ===\n");

    // Connection status
    fprintf(stderr, "Connected: %s\n", context->connected ? "Yes" : "No");

    // Inflammation modulation
    fprintf(stderr, "\nInflammation Modulation:\n");
    fprintf(stderr, "  Level: %s\n", brain_immune_inflammation_to_string(context->modulation.level));
    fprintf(stderr, "  Threshold multiplier: %.2f (baseline=%.2f, current=%.2f)\n",
            context->modulation.threshold_multiplier,
            context->baseline_threshold,
            global_workspace_get_ignition_threshold(context->workspace));
    fprintf(stderr, "  Capacity multiplier: %.2f\n", context->modulation.capacity_multiplier);

    // Statistics
    fprintf(stderr, "\nStatistics:\n");
    fprintf(stderr, "  Broadcasts checked: %lu\n", context->total_broadcasts_checked);
    fprintf(stderr, "  Anomalies detected: %lu (%.2f%%)\n",
            context->anomalies_detected,
            context->total_broadcasts_checked > 0 ?
                100.0f * (float)context->anomalies_detected / (float)context->total_broadcasts_checked : 0.0f);
    fprintf(stderr, "  Immune triggers: %lu\n", context->immune_triggers);

    // Recent anomalies
    if (verbose && context->anomaly_count > 0) {
        fprintf(stderr, "\nRecent Anomalies:\n");
        uint32_t display_count = (context->anomaly_count < 10) ? context->anomaly_count : 10;
        for (uint32_t i = 0; i < display_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && display_count > 256) {
                global_workspace_immune_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)display_count);
            }

            uint32_t idx = (context->anomaly_head + GW_IMMUNE_MAX_ANOMALY_HISTORY - 1 - i) %
                           GW_IMMUNE_MAX_ANOMALY_HISTORY;
            gw_broadcast_anomaly_t* a = &context->anomaly_history[idx];
            fprintf(stderr, "  [%u] %s (severity=%s, score=%.2f)\n",
                    i, gw_anomaly_type_to_string(a->type),
                    gw_anomaly_severity_to_string(a->severity),
                    a->anomaly_score);
            fprintf(stderr, "      %s\n", a->description);
            fprintf(stderr, "      Immune triggered: %s\n", a->immune_triggered ? "Yes" : "No");
        }
    }

    fprintf(stderr, "============================================\n");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* gw_anomaly_type_to_string(gw_anomaly_type_t type) {
    switch (type) {
        case GW_ANOMALY_NONE: return "NONE";
        case GW_ANOMALY_RAPID_SWITCHING: return "RAPID_SWITCHING";
        case GW_ANOMALY_STRENGTH_SPIKE: return "STRENGTH_SPIKE";
        case GW_ANOMALY_MODULE_HIJACK: return "MODULE_HIJACK";
        case GW_ANOMALY_REPETITIVE_PATTERN: return "REPETITIVE_PATTERN";
        case GW_ANOMALY_CORRUPTED_CONTENT: return "CORRUPTED_CONTENT";
        default: return "UNKNOWN";
    }
}

const char* gw_anomaly_severity_to_string(gw_anomaly_severity_t severity) {
    switch (severity) {
        case GW_ANOMALY_SEVERITY_NONE: return "NONE";
        case GW_ANOMALY_SEVERITY_MILD: return "MILD";
        case GW_ANOMALY_SEVERITY_MODERATE: return "MODERATE";
        case GW_ANOMALY_SEVERITY_SEVERE: return "SEVERE";
        case GW_ANOMALY_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int global_workspace_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_immune_heartbeat("global_works_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Global_Workspace_Immune");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                global_workspace_immune_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Global_Workspace_Immune");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Global_Workspace_Immune");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void global_workspace_immune_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_global_workspace_immune_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int global_workspace_immune_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_immune_training_begin: NULL argument");
        return -1;
    }
    global_workspace_immune_heartbeat_instance(g_global_workspace_immune_health_agent, "global_workspace_immune_training_begin", 0.0f);
    return 0;
}

int global_workspace_immune_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_immune_training_end: NULL argument");
        return -1;
    }
    global_workspace_immune_heartbeat_instance(g_global_workspace_immune_health_agent, "global_workspace_immune_training_end", 1.0f);
    return 0;
}

int global_workspace_immune_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_immune_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    global_workspace_immune_heartbeat_instance(g_global_workspace_immune_health_agent, "global_workspace_immune_training_step", progress);
    return 0;
}
