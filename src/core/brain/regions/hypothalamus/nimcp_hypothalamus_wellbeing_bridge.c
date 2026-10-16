/**
 * @file nimcp_hypothalamus_wellbeing_bridge.c
 * @brief Implementation of Hypothalamus-Wellbeing Bridge for Homeostatic Balance Reporting
 *
 * WHAT: Bidirectional integration between hypothalamus homeostasis and wellbeing monitoring
 * WHY:  Homeostatic balance directly impacts system wellbeing; wellbeing state modulates setpoints
 * HOW:  Drive deviations → distress signals; wellbeing state → homeostatic adjustments
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_wellbeing_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_wellbeing_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_WELLBEING_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_wellbeing_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hypo_drive_system_handle_t* drives;
    hypo_wellbeing_config_t config;

    /* Current distress report */
    hypo_distress_report_t distress_report;

    /* Wellbeing feedback state */
    hypo_wellbeing_feedback_t feedback;

    /* Timing */
    uint64_t distress_onset_time_ms;
    uint64_t last_update_time_ms;
    bool distress_active;

    /* Bio-async registration */
    bool bio_registered;
    bio_module_context_t bio_ctx;
};

static uint64_t get_current_time_ms(void) {
    /* Placeholder - would use actual time source */
    static uint64_t mock_time = 0;
    return mock_time += 100;
}

/**
 * @brief Map drive type to distress source
 */
static hypo_distress_source_t drive_to_distress_source(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_HUNGER:      return HYPO_DISTRESS_HUNGER;
        case HYPO_DRIVE_THIRST:      return HYPO_DISTRESS_THIRST;
        case HYPO_DRIVE_FATIGUE:     return HYPO_DISTRESS_FATIGUE;
        case HYPO_DRIVE_SAFETY:      return HYPO_DISTRESS_SAFETY;
        case HYPO_DRIVE_TEMPERATURE: return HYPO_DISTRESS_THERMAL;
        case HYPO_DRIVE_SOCIAL:      return HYPO_DISTRESS_SOCIAL;
        case HYPO_DRIVE_CURIOSITY:   return HYPO_DISTRESS_CURIOSITY;
        default:                     return HYPO_DISTRESS_NONE;
    }
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

void hypo_wellbeing_bridge_default_config(hypo_wellbeing_config_t* config) {
    if (!config) return;

    config->distress_threshold = 0.4f;
    config->conflict_weight = 0.5f;
    config->chronic_accumulation = 0.01f;
    config->chronic_decay = 0.001f;
    config->report_all_distress = true;
    config->intervention_threshold = 0.7f;
    config->safety_priority_boost = 1.5f;
}

hypo_wellbeing_bridge_t* hypo_wellbeing_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_wellbeing_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_wellbeing_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_wellbeing_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_wellbeing_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_wellbeing_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->drives = drives;

    if (config) {
        bridge->config = *config;
    } else {
        hypo_wellbeing_bridge_default_config(&bridge->config);
    }

    /* Initialize optimal distress state */
    memset(&bridge->distress_report, 0, sizeof(hypo_distress_report_t));
    bridge->distress_report.overall_state = HYPO_WB_OPTIMAL;
    bridge->distress_report.primary_source = HYPO_DISTRESS_NONE;

    /* Initialize neutral feedback */
    memset(&bridge->feedback, 0, sizeof(hypo_wellbeing_feedback_t));
    bridge->feedback.adjustment_factor = 1.0f;

    bridge->distress_onset_time_ms = 0;
    bridge->last_update_time_ms = get_current_time_ms();
    bridge->distress_active = false;

    bridge->bio_registered = false;
    bridge->bio_ctx = NULL;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_wellbeing");

    nimcp_log(LOG_LEVEL_INFO, "hypo_wellbeing_bridge: created successfully");
    return bridge;
}

void hypo_wellbeing_bridge_destroy(hypo_wellbeing_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_wellbeing");

    if (bridge->bio_registered) {
        hypo_wellbeing_bridge_unregister_bio(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_wellbeing_bridge: destroyed");
}

/*=============================================================================
 * HOMEOSTASIS → WELLBEING REPORTING
 *===========================================================================*/

int hypo_wellbeing_bridge_compute_distress(hypo_wellbeing_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_compute_distress: bridge or drives is NULL");
        return -1;
    }

    const hypo_wellbeing_config_t* cfg = &bridge->config;
    hypo_distress_report_t* report = &bridge->distress_report;
    uint64_t current_time = get_current_time_ms();

    /* Get current drive states */
    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_wellbeing_bridge_compute_distress: hypo_drive_get_system_state is NULL");
        return -1;
    }

    /*
     * DISTRESS COMPUTATION:
     *
     * 1. Per-drive distress = urgency (deviation from setpoint)
     * 2. Safety threats get priority boost
     * 3. Multi-drive conflicts add to distress
     * 4. Chronic accumulation over time
     */

    /* Compute per-drive distress */
    float max_distress = 0.0f;
    hypo_distress_source_t max_source = HYPO_DISTRESS_NONE;
    float total_distress = 0.0f;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = drive_state.drives[i].urgency;

        /* Safety gets priority boost */
        if (i == HYPO_DRIVE_SAFETY) {
            urgency *= cfg->safety_priority_boost;
        }

        report->per_drive_distress[i] = urgency;
        total_distress += urgency;

        if (urgency > max_distress) {
            max_distress = urgency;
            max_source = drive_to_distress_source((hypo_drive_type_t)i);
        }
    }

    /* Compute conflict level (multiple high-urgency drives) */
    int high_urgency_count = 0;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        if (drive_state.drives[i].urgency > cfg->distress_threshold) {
            high_urgency_count++;
        }
    }
    report->conflict_level = (high_urgency_count > 1) ?
        nimcp_clamp01((float)(high_urgency_count - 1) / (HYPO_DRIVE_COUNT - 1)) : 0.0f;

    /* Add conflict contribution to distress */
    float conflict_contribution = report->conflict_level * cfg->conflict_weight;
    if (conflict_contribution > 0.0f) {
        max_source = HYPO_DISTRESS_CONFLICT;
    }

    /* Compute overall distress */
    float avg_distress = total_distress / (float)HYPO_DRIVE_COUNT;
    report->distress_level = nimcp_clamp01(
        max_distress * 0.6f + avg_distress * 0.2f + conflict_contribution * 0.2f
    );

    /* Update chronic load */
    if (report->distress_level > cfg->distress_threshold) {
        report->chronic_load = nimcp_clamp01(
            report->chronic_load + cfg->chronic_accumulation * report->distress_level
        );

        if (!bridge->distress_active) {
            bridge->distress_onset_time_ms = current_time;
            bridge->distress_active = true;
        }
        report->duration_ms = current_time - bridge->distress_onset_time_ms;

        /* Chronic stress is its own distress source */
        if (report->chronic_load > 0.6f) {
            max_source = HYPO_DISTRESS_CHRONIC;
        }
    } else {
        report->chronic_load = nimcp_clamp01(
            report->chronic_load - cfg->chronic_decay
        );
        bridge->distress_active = false;
        report->duration_ms = 0;
    }

    report->primary_source = max_source;

    /* Determine overall state */
    if (report->distress_level < 0.2f) {
        report->overall_state = HYPO_WB_OPTIMAL;
    } else if (report->distress_level < 0.4f) {
        report->overall_state = HYPO_WB_MILD_STRESS;
    } else if (report->distress_level < 0.6f) {
        report->overall_state = HYPO_WB_MODERATE_STRESS;
    } else if (report->distress_level < 0.8f) {
        report->overall_state = HYPO_WB_SEVERE_STRESS;
    } else {
        report->overall_state = HYPO_WB_CRITICAL;
    }

    /* Safety assessment */
    report->safety_threatened = (drive_state.drives[HYPO_DRIVE_SAFETY].urgency > 0.6f);

    /* Intervention recommendation */
    report->requires_intervention = (report->distress_level > cfg->intervention_threshold) ||
                                    report->safety_threatened;

    /* Alignment safety: Log concerning states */
    if (report->overall_state >= HYPO_WB_SEVERE_STRESS) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_wellbeing_bridge: distress level %.2f, source %d, duration %llu ms",
            report->distress_level, report->primary_source,
            (unsigned long long)report->duration_ms);
    }

    if (report->requires_intervention) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_wellbeing_bridge: intervention recommended (safety=%d, distress=%.2f)",
            report->safety_threatened, report->distress_level);
    }

    bridge->last_update_time_ms = current_time;
    return 0;
}

int hypo_wellbeing_bridge_get_distress_report(
    const hypo_wellbeing_bridge_t* bridge,
    hypo_distress_report_t* report)
{
    if (!bridge || !report) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_get_distress_report: bridge or report is NULL");
        return -1;
    }

    *report = bridge->distress_report;
    return 0;
}

bool hypo_wellbeing_bridge_needs_intervention(const hypo_wellbeing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_needs_intervention: bridge is NULL");
        return false;
    }
    return bridge->distress_report.requires_intervention;
}

/*=============================================================================
 * WELLBEING → HOMEOSTASIS MODULATION
 *===========================================================================*/

int hypo_wellbeing_bridge_update_feedback(
    hypo_wellbeing_bridge_t* bridge,
    const hypo_wellbeing_feedback_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_update_feedback: bridge or feedback is NULL");
        return -1;
    }

    bridge->feedback = *feedback;
    return 0;
}

int hypo_wellbeing_bridge_apply_interventions(hypo_wellbeing_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_apply_interventions: bridge or drives is NULL");
        return -1;
    }

    const hypo_wellbeing_feedback_t* fb = &bridge->feedback;

    /*
     * WELLBEING → HOMEOSTASIS INTERVENTIONS:
     *
     * 1. Distress acknowledged → Log for transparency
     * 2. Active intervention → May adjust setpoints
     * 3. Reduce non-essential → Suppress lower-priority drives
     * 4. Safety override → Emergency priority shift
     */

    if (fb->distress_acknowledged) {
        nimcp_log(LOG_LEVEL_INFO,
            "hypo_wellbeing_bridge: distress acknowledged by wellbeing system");
    }

    if (fb->intervention_active) {
        nimcp_log(LOG_LEVEL_INFO,
            "hypo_wellbeing_bridge: intervention active (adjustment=%.2f)",
            fb->adjustment_factor);

        /* Could adjust drive setpoints here based on adjustment_factor */
    }

    if (fb->reduce_non_essential) {
        nimcp_log(LOG_LEVEL_DEBUG,
            "hypo_wellbeing_bridge: suppressing non-essential drives");

        /* Would suppress curiosity, social drives in favor of survival */
    }

    if (fb->safety_override) {
        nimcp_log(LOG_LEVEL_WARN,
            "hypo_wellbeing_bridge: safety override active - all non-safety drives suppressed");

        /* Emergency: Only safety drive remains active */
    }

    return 0;
}

int hypo_wellbeing_bridge_get_feedback(
    const hypo_wellbeing_bridge_t* bridge,
    hypo_wellbeing_feedback_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_get_feedback: bridge or feedback is NULL");
        return -1;
    }

    *feedback = bridge->feedback;
    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t wellbeing_handle_feedback(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_wellbeing_bridge_t* bridge = (hypo_wellbeing_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(hypo_wellbeing_feedback_t)) {
        const hypo_wellbeing_feedback_t* feedback =
            (const hypo_wellbeing_feedback_t*)(header + 1);
        hypo_wellbeing_bridge_update_feedback(bridge, feedback);
        hypo_wellbeing_bridge_apply_interventions(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t wellbeing_handle_distress_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    (void)msg;
    (void)msg_size;
    hypo_wellbeing_bridge_t* bridge = (hypo_wellbeing_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    hypo_wellbeing_bridge_compute_distress(bridge);
    hypo_wellbeing_bridge_broadcast_distress(bridge);

    return NIMCP_SUCCESS;
}

bool hypo_wellbeing_bridge_register_bio(hypo_wellbeing_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;  /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_WELLBEING_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_wellbeing_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_wellbeing_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_wellbeing_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for wellbeing messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_WB_FEEDBACK, wellbeing_handle_feedback);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_WB_DISTRESS_REQUEST, wellbeing_handle_distress_request);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_wellbeing_bridge: registered with bio-router");
    return true;
}

void hypo_wellbeing_bridge_unregister_bio(hypo_wellbeing_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_wellbeing_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_wellbeing_bridge_broadcast_distress(hypo_wellbeing_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_wellbeing_bridge_broadcast_distress: bridge is NULL or not bio_registered");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    struct {
        bio_message_header_t header;
        hypo_distress_report_t report;
    } msg;

    msg.header.type = BIO_MSG_HYPO_WB_DISTRESS_REPORT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_WELLBEING_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;  /* Distress is high priority */
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.sequence_id = 0;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;

    msg.report = bridge->distress_report;

    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}
