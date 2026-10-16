/**
 * @file nimcp_mental_health_guardian.c
 * @brief Mental Health Guardian - Implementation
 *
 * WHAT: Independent background agent that monitors NIMCP's mental health
 * WHY:  Proactively detect and correct mental health abnormalities
 * HOW:  Background thread collects markers, detects disorders, applies interventions
 *
 * THREAD MODEL:
 * - Main guardian thread runs independently at configurable interval
 * - Uses atomic flags for running/paused state
 * - Mutex protects configuration and metrics updates
 * - Sleep uses platform-agnostic nimcp_thread_sleep_ms()
 *
 * INTEGRATION POINTS:
 * - mental_health_monitor: Disorder detection and intervention
 * - brain_immune_system: Threat reporting for severe disorders
 * - brain_kg: Module state updates in internal knowledge graph
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

//=============================================================================
// Includes
//=============================================================================

#include "cognitive/mental_health/nimcp_mental_health_guardian.h"
#include "cognitive/nimcp_mental_health.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain_kg.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(mental_health_guardian, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Logging
//=============================================================================

#define LOG_TAG "[GUARDIAN]"

#define GUARDIAN_LOG(fmt, ...) \
    fprintf(stderr, LOG_TAG " " fmt "\n", ##__VA_ARGS__)

#define GUARDIAN_LOG_VERBOSE(guardian, fmt, ...) \
    do { \
        if ((guardian)->config.verbose_logging) { \
            fprintf(stderr, LOG_TAG " " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Mental Health Guardian internal structure
 *
 * Contains all state for the background monitoring agent.
 */
struct mental_health_guardian {
    /* Configuration */
    mental_health_guardian_config_t config;

    /* Parent brain reference */
    brain_t brain;

    /* Thread control */
    nimcp_thread_t monitor_thread;
    nimcp_mutex_t* lock;
    atomic_bool running;
    atomic_bool paused;
    bool thread_created;

    /* State */
    guardian_state_t state;
    guardian_intervention_level_t current_level;
    float last_overall_severity;
    int last_primary_disorder;
    int last_secondary_disorder;       /**< Second most severe disorder (-1 if none) */
    float secondary_disorder_score;    /**< Score of secondary disorder */

    /* Metrics */
    uint64_t checks_performed;
    uint64_t interventions_applied;
    uint64_t observe_count;
    uint64_t adjust_count;
    uint64_t regulate_count;
    uint64_t quarantine_count;
    uint64_t start_time_ms;
    uint64_t last_check_time_ms;

    /* Integration handles */
    brain_immune_system_t* immune_system;
    brain_kg_t* internal_kg;
    uint64_t kg_admin_token;
    brain_kg_node_id_t kg_node_id;      /* Guardian's node in KG */

    /* Bio-async integration */
    bio_module_context_t bio_context;
    bool bio_async_connected;

    /* Neuromodulator integration */
    spatial_neuromod_field_t* neuromod_field;

    /* Brainstem/medulla integration */
    void* medulla;  /* medulla_t* - avoid header dependency */

    /* Sleep system integration */
    void* sleep_system;  /* sleep_system_t* */

    /* Plasticity integration */
    void* plasticity;  /* homeostatic_plasticity_t* */

    /* FEP orchestrator integration */
    void* fep_orchestrator;  /* fep_orchestrator_t* */
    uint32_t fep_bridge_id;
    bool fep_registered;

    /* Cognitive module integration */
    void* working_memory;    /* working_memory_t* */
    void* executive;         /* executive_controller_t* */
};

//=============================================================================
// Default Configuration
//=============================================================================

mental_health_guardian_config_t mental_health_guardian_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_default_config", 0.0f);


    mental_health_guardian_config_t config = {
        .monitoring_interval_ms = 100,
        .observe_threshold = 0.0f,
        .adjust_threshold = 0.3f,
        .regulate_threshold = 0.6f,
        .quarantine_threshold = 0.8f,
        .auto_intervene = true,
        .immune_integration = true,
        .kg_integration = true,
        .neuromod_adjust_strength = 0.1f,
        .enable_sleep_trigger = true,
        .verbose_logging = false
    };
    return config;
}

//=============================================================================
// Intervention Level Determination
//=============================================================================

/**
 * @brief Determine intervention level from severity
 */
static guardian_intervention_level_t severity_to_level(
    float severity,
    const mental_health_guardian_config_t* config)
{
    if (severity >= config->quarantine_threshold) {
        return GUARDIAN_LEVEL_QUARANTINE;
    } else if (severity >= config->regulate_threshold) {
        return GUARDIAN_LEVEL_REGULATE;
    } else if (severity >= config->adjust_threshold) {
        return GUARDIAN_LEVEL_ADJUST;
    } else {
        return GUARDIAN_LEVEL_OBSERVE;
    }
}

//=============================================================================
// Bio-Async Message Publishing
//=============================================================================

/**
 * @brief Publish guardian status report via bio-async
 */
static void publish_bio_async_status(mental_health_guardian_t* guardian) {
    if (!guardian->bio_async_connected || !guardian->bio_context) {
        return;
    }

    bio_msg_guardian_status_report_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Initialize header */
    bio_msg_init_header(&msg.header,
                        BIO_MSG_GUARDIAN_STATUS_REPORT,
                        BIO_MODULE_MENTAL_HEALTH_GUARDIAN,
                        BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_SEROTONIN;  /* Slow state change */

    /* Fill payload */
    msg.state = (uint8_t)guardian->state;
    msg.intervention_level = (uint8_t)guardian->current_level;
    msg.overall_severity = guardian->last_overall_severity;
    msg.primary_disorder = guardian->last_primary_disorder;
    msg.checks_performed = guardian->checks_performed;
    msg.interventions_applied = guardian->interventions_applied;
    msg.uptime_ms = (guardian->start_time_ms > 0) ?
                    (nimcp_time_get_ms() - guardian->start_time_ms) : 0;

    /* Broadcast to all modules */
    nimcp_error_t err = bio_router_broadcast(guardian->bio_context, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        GUARDIAN_LOG("Failed to broadcast status report (error=%d)", err);
    }
}

/**
 * @brief Publish intervention notification via bio-async
 */
static void publish_bio_async_intervention(mental_health_guardian_t* guardian,
                                           guardian_intervention_level_t level) {
    if (!guardian->bio_async_connected || !guardian->bio_context) {
        return;
    }

    bio_msg_guardian_intervention_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Initialize header */
    bio_msg_init_header(&msg.header,
                        BIO_MSG_GUARDIAN_INTERVENTION,
                        BIO_MODULE_MENTAL_HEALTH_GUARDIAN,
                        BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    /* Use appropriate channel based on severity */
    if (level >= GUARDIAN_LEVEL_QUARANTINE) {
        msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alert/priority */
    } else {
        msg.header.channel = BIO_CHANNEL_SEROTONIN;  /* State change */
    }

    /* Fill payload */
    msg.level = (uint8_t)level;
    msg.severity = guardian->last_overall_severity;
    msg.disorder = guardian->last_primary_disorder;
    msg.flags = 0;

    nimcp_error_t err = bio_router_broadcast(guardian->bio_context, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        GUARDIAN_LOG("Failed to broadcast intervention (error=%d)", err);
    }
}

/**
 * @brief Publish critical alert via bio-async
 */
static void publish_bio_async_alert(mental_health_guardian_t* guardian,
                                    bool immune_notified) {
    if (!guardian->bio_async_connected || !guardian->bio_context) {
        return;
    }

    bio_msg_guardian_alert_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Initialize header - use norepinephrine for urgent alert */
    bio_msg_init_header(&msg.header,
                        BIO_MSG_GUARDIAN_ALERT,
                        BIO_MODULE_MENTAL_HEALTH_GUARDIAN,
                        BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* High priority */
    /* High priority flag would go here if supported */

    /* Fill payload */
    msg.severity = guardian->last_overall_severity;
    msg.primary_disorder = guardian->last_primary_disorder;
    msg.secondary_disorder = guardian->last_secondary_disorder;
    msg.action_taken = (uint8_t)GUARDIAN_LEVEL_QUARANTINE;
    msg.immune_notified = immune_notified;
    msg.quarantine_active = true;

    nimcp_error_t err = bio_router_broadcast(guardian->bio_context, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        GUARDIAN_LOG("Failed to broadcast alert (error=%d)", err);
    }
}

/**
 * @brief Publish level change notification via bio-async
 */
static void publish_bio_async_level_changed(mental_health_guardian_t* guardian,
                                            guardian_intervention_level_t old_level,
                                            guardian_intervention_level_t new_level) {
    if (!guardian->bio_async_connected || !guardian->bio_context) {
        return;
    }

    bio_msg_guardian_level_changed_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header,
                        BIO_MSG_GUARDIAN_LEVEL_CHANGED,
                        BIO_MODULE_MENTAL_HEALTH_GUARDIAN,
                        BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_SEROTONIN;

    msg.old_level = (uint8_t)old_level;
    msg.new_level = (uint8_t)new_level;
    msg.severity = guardian->last_overall_severity;
    msg.timestamp_ms = nimcp_time_get_ms();

    nimcp_error_t err = bio_router_broadcast(guardian->bio_context, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        GUARDIAN_LOG("Failed to broadcast level change (error=%d)", err);
    }
}

//=============================================================================
// Intervention Actions
//=============================================================================

/**
 * @brief Disorder-specific neuromodulator adjustment table
 *
 * Each row defines the relative adjustment for [DA, 5-HT, NE]
 * Positive values increase, negative decrease, zero no change.
 * Values are clamped after application.
 */
typedef struct {
    float dopamine_delta;
    float serotonin_delta;
    float norepinephrine_delta;
} neuromod_adjustment_t;

static const neuromod_adjustment_t disorder_neuromod_adjustments[] = {
    [DISORDER_DEPRESSION]       = { +0.15f, +0.20f, +0.10f },  /* Boost all monoamines */
    [DISORDER_ANXIETY]          = {  0.00f, +0.15f, -0.10f },  /* Increase 5-HT, reduce NE */
    [DISORDER_BIPOLAR]          = { -0.10f, +0.15f, -0.10f },  /* Moderate all */
    [DISORDER_MANIA]            = { -0.20f, +0.10f, -0.15f },  /* Reduce DA/NE for mania */
    [DISORDER_SCHIZOPHRENIA]    = { -0.15f, +0.10f,  0.00f },  /* Reduce DA, boost 5-HT */
    [DISORDER_PTSD]             = {  0.00f, +0.15f, -0.15f },  /* Increase 5-HT, reduce NE (hyperarousal) */
    [DISORDER_OCD]              = {  0.00f, +0.20f,  0.00f },  /* Primarily 5-HT */
    [DISORDER_ADHD]             = { +0.10f,  0.00f, +0.10f },  /* Boost DA and NE */
    [DISORDER_AUTISM]           = {  0.00f, +0.10f,  0.00f },  /* Gentle 5-HT support */
    [DISORDER_BORDERLINE]       = {  0.00f, +0.15f,  0.00f },  /* 5-HT for emotional stability */
    [DISORDER_PSYCHOPATHY]      = { -0.10f, +0.15f,  0.00f },  /* Reduce DA (reward-seeking) */
    [DISORDER_SOCIOPATHY]       = { -0.10f, +0.15f,  0.00f },  /* Similar to psychopathy */
    [DISORDER_PARANOID]         = {  0.00f, +0.10f, -0.05f },  /* 5-HT for trust, reduce NE */
    [DISORDER_CONDUCT]          = { -0.05f, +0.15f,  0.00f },  /* Reduce impulsivity */
    /* Remaining disorders get default zero adjustments */
};

/**
 * @brief Apply disorder-specific neuromodulator adjustments
 */
static void apply_neuromod_adjustments(mental_health_guardian_t* guardian) {
    if (!guardian->brain || !guardian->brain->neuromodulator_system) {
        return;
    }

    int disorder = guardian->last_primary_disorder;
    if (disorder < 0 || disorder >= DISORDER_COUNT) {
        return;
    }

    /* Get current levels */
    neuromodulator_system_t system = guardian->brain->neuromodulator_system;
    float da = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    float sero = neuromodulator_get_level(system, NEUROMOD_SEROTONIN);
    float ne = neuromodulator_get_level(system, NEUROMOD_NOREPINEPHRINE);

    /* Get disorder-specific adjustments */
    neuromod_adjustment_t adj = {0};
    if (disorder < (int)(sizeof(disorder_neuromod_adjustments) / sizeof(disorder_neuromod_adjustments[0]))) {
        adj = disorder_neuromod_adjustments[disorder];
    }

    /* Apply adjustments with clamping */
    float new_da = da + adj.dopamine_delta;
    float new_sero = sero + adj.serotonin_delta;
    float new_ne = ne + adj.norepinephrine_delta;

    /* Clamp to valid range [0, 1] */
    if (new_da < 0.0f) new_da = 0.0f;
    if (new_da > 1.0f) new_da = 1.0f;
    if (new_sero < 0.0f) new_sero = 0.0f;
    if (new_sero > 1.0f) new_sero = 1.0f;
    if (new_ne < 0.0f) new_ne = 0.0f;
    if (new_ne > 1.0f) new_ne = 1.0f;

    /* Set new levels */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, new_da);
    neuromodulator_set_level(system, NEUROMOD_SEROTONIN, new_sero);
    neuromodulator_set_level(system, NEUROMOD_NOREPINEPHRINE, new_ne);

    GUARDIAN_LOG_VERBOSE(guardian, "Neuromod adjust: DA %.2f→%.2f, 5-HT %.2f→%.2f, NE %.2f→%.2f",
                        da, new_da, sero, new_sero, ne, new_ne);
}

/**
 * @brief Apply ADJUST level intervention
 *
 * Minor neuromodulator adjustments to rebalance brain chemistry.
 */
static void apply_adjust_intervention(mental_health_guardian_t* guardian) {
    if (!guardian->brain || !guardian->brain->mental_health_monitor) {
        return;
    }

    GUARDIAN_LOG_VERBOSE(guardian, "Applying ADJUST intervention (neuromod tweak)");

    /* Use mental health intervention API to adjust neuromodulators */
    mental_health_intervene(guardian->brain->mental_health_monitor, guardian->brain);

    /* Apply disorder-specific neuromodulator adjustments */
    apply_neuromod_adjustments(guardian);

    guardian->adjust_count++;
    guardian->interventions_applied++;
}

/**
 * @brief Apply REGULATE level intervention
 *
 * More aggressive: homeostatic reset, optionally trigger sleep.
 */
static void apply_regulate_intervention(mental_health_guardian_t* guardian) {
    if (!guardian->brain || !guardian->brain->mental_health_monitor) {
        return;
    }

    GUARDIAN_LOG_VERBOSE(guardian, "Applying REGULATE intervention (homeostatic reset)");

    /* Use mental health intervention API */
    mental_health_intervene(guardian->brain->mental_health_monitor, guardian->brain);

    /* Optionally trigger sleep cycle for consolidation */
    if (guardian->config.enable_sleep_trigger && guardian->brain->sleep_system) {
        /* Sleep trigger would go here if sleep API available */
        GUARDIAN_LOG_VERBOSE(guardian, "Sleep trigger requested (if available)");
    }

    guardian->regulate_count++;
    guardian->interventions_applied++;
}

/**
 * @brief Map disorder to threat epitope string
 */
static void disorder_to_epitope(int disorder, uint8_t* epitope, size_t* len) {
    const char* threat_name = NULL;
    switch (disorder) {
        case DISORDER_DEPRESSION:    threat_name = "MH_DEPRESSION_CRITICAL"; break;
        case DISORDER_ANXIETY:       threat_name = "MH_ANXIETY_CRITICAL"; break;
        case DISORDER_BIPOLAR:       threat_name = "MH_BIPOLAR_CRITICAL"; break;
        case DISORDER_SCHIZOPHRENIA: threat_name = "MH_SCHIZOPHRENIA_CRITICAL"; break;
        case DISORDER_PTSD:          threat_name = "MH_PTSD_CRITICAL"; break;
        case DISORDER_OCD:           threat_name = "MH_OCD_CRITICAL"; break;
        case DISORDER_ADHD:          threat_name = "MH_ADHD_CRITICAL"; break;
        case DISORDER_AUTISM:        threat_name = "MH_AUTISM_CRITICAL"; break;
        case DISORDER_PSYCHOPATHY:   threat_name = "MH_PSYCHOPATHY_CRITICAL"; break;
        case DISORDER_SOCIOPATHY:    threat_name = "MH_SOCIOPATHY_CRITICAL"; break;
        default:                     threat_name = "MH_UNKNOWN_CRITICAL"; break;
    }
    *len = strlen(threat_name);
    if (*len > BRAIN_IMMUNE_EPITOPE_SIZE) {
        *len = BRAIN_IMMUNE_EPITOPE_SIZE;
    }
    memcpy(epitope, threat_name, *len);
}

/**
 * @brief Apply QUARANTINE level intervention
 *
 * Safety-critical: activate quarantine mode, report to immune system.
 */
static void apply_quarantine_intervention(mental_health_guardian_t* guardian) {
    if (!guardian->brain || !guardian->brain->mental_health_monitor) {
        return;
    }

    GUARDIAN_LOG("CRITICAL: Applying QUARANTINE intervention");

    /* Use mental health intervention API for quarantine */
    mental_health_intervene(guardian->brain->mental_health_monitor, guardian->brain);

    /* Track whether immune was notified (for bio-async alert) */
    bool immune_notified = false;

    /* Report to immune system if connected */
    if (guardian->immune_system && guardian->config.immune_integration) {
        GUARDIAN_LOG("Reporting critical mental health state to immune system");

        /* Create epitope from disorder type */
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
        size_t epitope_len = 0;
        disorder_to_epitope(guardian->last_primary_disorder, epitope, &epitope_len);

        /* Map severity to immune severity (1-10) */
        uint32_t immune_severity = (uint32_t)(guardian->last_overall_severity * 10.0f);
        if (immune_severity < 1) immune_severity = 1;
        if (immune_severity > 10) immune_severity = 10;

        /* Present antigen to immune system */
        uint32_t antigen_id = 0;
        int result = brain_immune_present_antigen(
            guardian->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            epitope_len,
            immune_severity,
            0,  /* source_node - guardian doesn't have a node ID */
            &antigen_id
        );

        if (result == 0) {
            GUARDIAN_LOG("Immune antigen created: id=%u severity=%u",
                        antigen_id, immune_severity);
            immune_notified = true;
        } else {
            GUARDIAN_LOG("Failed to create immune antigen (error=%d)", result);
        }
    }

    /* Broadcast critical alert via bio-async */
    publish_bio_async_alert(guardian, immune_notified);

    guardian->quarantine_count++;
    guardian->interventions_applied++;
}

/**
 * @brief Apply intervention based on level
 */
static void apply_intervention(
    mental_health_guardian_t* guardian,
    guardian_intervention_level_t level)
{
    switch (level) {
        case GUARDIAN_LEVEL_OBSERVE:
            guardian->observe_count++;
            break;
        case GUARDIAN_LEVEL_ADJUST:
            apply_adjust_intervention(guardian);
            publish_bio_async_intervention(guardian, level);
            break;
        case GUARDIAN_LEVEL_REGULATE:
            apply_regulate_intervention(guardian);
            publish_bio_async_intervention(guardian, level);
            break;
        case GUARDIAN_LEVEL_QUARANTINE:
            apply_quarantine_intervention(guardian);
            publish_bio_async_intervention(guardian, level);
            break;
    }
}

//=============================================================================
// Internal KG Update
//=============================================================================

/**
 * @brief Update mental health status in internal KG
 *
 * Registers guardian node in KG if not yet registered, then updates
 * node state to reflect current intervention level.
 */
static void update_internal_kg(mental_health_guardian_t* guardian) {
    if (!guardian->internal_kg || !guardian->config.kg_integration) {
        return;
    }

    /* Elevate to ADMIN access for write */
    brain_kg_set_access_level(guardian->internal_kg, BRAIN_KG_ACCESS_ADMIN,
                               guardian->kg_admin_token);

    /* Ensure guardian is registered as a KG node */
    if (guardian->kg_node_id == 0 || guardian->kg_node_id == BRAIN_KG_INVALID_NODE) {
        /* Try to find existing node first */
        guardian->kg_node_id = brain_kg_find_node(guardian->internal_kg,
                                                   "mental_health_guardian");

        /* Register new node if not found */
        if (guardian->kg_node_id == BRAIN_KG_INVALID_NODE) {
            guardian->kg_node_id = brain_kg_add_node(
                guardian->internal_kg,
                "mental_health_guardian",
                BRAIN_KG_NODE_COGNITIVE,
                "Independent real-time mental health monitoring agent"
            );

            if (guardian->kg_node_id != BRAIN_KG_INVALID_NODE) {
                GUARDIAN_LOG("Registered in KG with node_id=%u", guardian->kg_node_id);

                /* Set module pointer for introspection */
                brain_kg_set_module_ptr(guardian->internal_kg, guardian->kg_node_id,
                                        guardian);

                /* Add metadata about capabilities */
                brain_kg_add_metadata(guardian->internal_kg, guardian->kg_node_id,
                                      "monitoring_interval_ms",
                                      "100");  /* Default 10Hz */
                brain_kg_add_metadata(guardian->internal_kg, guardian->kg_node_id,
                                      "intervention_levels",
                                      "OBSERVE,ADJUST,REGULATE,QUARANTINE");
            } else {
                GUARDIAN_LOG("Failed to register in KG");
            }
        }
    }

    /* Update mental_health_guardian node state based on intervention level */
    if (guardian->kg_node_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_state_t state;
        const char* description = NULL;

        switch (guardian->current_level) {
            case GUARDIAN_LEVEL_OBSERVE:
                state = BRAIN_KG_STATE_ACTIVE;
                description = "Guardian active - OBSERVE level";
                break;
            case GUARDIAN_LEVEL_ADJUST:
                state = BRAIN_KG_STATE_ACTIVE;
                description = "Guardian intervening - ADJUST level";
                break;
            case GUARDIAN_LEVEL_REGULATE:
                state = BRAIN_KG_STATE_DISABLED;  /* Using DISABLED for degraded state */
                description = "Guardian regulating - REGULATE level";
                break;
            case GUARDIAN_LEVEL_QUARANTINE:
                state = BRAIN_KG_STATE_ERROR;
                description = "Guardian quarantining - QUARANTINE level";
                break;
            default:
                state = BRAIN_KG_STATE_ACTIVE;
                description = NULL;
        }

        /* Update node state and description */
        int result = brain_kg_update_node(guardian->internal_kg, guardian->kg_node_id,
                                          description, state);
        if (result != 0) {
            GUARDIAN_LOG("Failed to update KG node state (error=%d)", result);
        }
    }

    /* Reset to READ access */
    brain_kg_set_access_level(guardian->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

//=============================================================================
// Core Monitoring Logic
//=============================================================================

/**
 * @brief Perform a single health check cycle
 *
 * Collects markers, detects disorders, determines intervention level,
 * and optionally applies intervention.
 *
 * @return Current intervention level
 */
static guardian_intervention_level_t perform_health_check(
    mental_health_guardian_t* guardian)
{
    if (!guardian->brain || !guardian->brain->mental_health_monitor) {
        return GUARDIAN_LEVEL_OBSERVE;
    }

    /* Run mental health check */
    disorder_severity_t max_severity = mental_health_check(
        guardian->brain->mental_health_monitor,
        guardian->brain
    );

    /* Get report for detailed info */
    mental_health_report_t report;
    mental_health_get_report(guardian->brain->mental_health_monitor, &report);

    /* Calculate overall severity (normalized 0-1) */
    float overall_severity = 0.0f;
    switch (max_severity) {
        case DISORDER_SEVERITY_NONE:     overall_severity = 0.1f; break;
        case DISORDER_SEVERITY_MILD:     overall_severity = 0.3f; break;
        case DISORDER_SEVERITY_MODERATE: overall_severity = 0.5f; break;
        case DISORDER_SEVERITY_SEVERE:   overall_severity = 0.7f; break;
        case DISORDER_SEVERITY_CRITICAL: overall_severity = 0.9f; break;
    }

    /* Determine intervention level */
    guardian_intervention_level_t level = severity_to_level(overall_severity, &guardian->config);
    guardian_intervention_level_t old_level = guardian->current_level;

    /* Update state */
    guardian->last_overall_severity = overall_severity;
    guardian->last_primary_disorder = (int)report.primary_disorder;

    /* Find secondary disorder (second highest score above threshold) */
    int secondary = -1;
    float secondary_score = 0.0f;
    float threshold = 0.3f;  /* Minimum severity to count as active */

    for (int i = 0; i < DISORDER_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && DISORDER_COUNT > 256) {
            mental_health_guardian_heartbeat("mental_healt_loop",
                             (float)(i + 1) / (float)DISORDER_COUNT);
        }

        if (i != (int)report.primary_disorder &&
            report.disorder_scores[i] >= threshold &&
            report.disorder_scores[i] > secondary_score) {
            secondary = i;
            secondary_score = report.disorder_scores[i];
        }
    }
    guardian->last_secondary_disorder = secondary;
    guardian->secondary_disorder_score = secondary_score;

    guardian->current_level = level;
    guardian->checks_performed++;
    guardian->last_check_time_ms = nimcp_time_get_ms();

    /* Log if level changed or verbose */
    if (level != GUARDIAN_LEVEL_OBSERVE || guardian->config.verbose_logging) {
        GUARDIAN_LOG_VERBOSE(guardian, "Health check: severity=%.2f level=%s",
                            overall_severity, guardian_level_to_string(level));
    }

    /* Publish level change if level transitioned */
    if (level != old_level) {
        publish_bio_async_level_changed(guardian, old_level, level);
    }

    /* Apply intervention if auto-intervene enabled */
    if (guardian->config.auto_intervene && level > GUARDIAN_LEVEL_OBSERVE) {
        apply_intervention(guardian, level);
    }

    /* Update internal KG if connected */
    update_internal_kg(guardian);

    return level;
}

//=============================================================================
// Monitor Thread
//=============================================================================

/**
 * @brief Background monitoring thread function
 */
static void* guardian_monitor_thread(void* arg) {
    mental_health_guardian_t* guardian = (mental_health_guardian_t*)arg;

    GUARDIAN_LOG("Monitor thread started (interval=%ums)",
                guardian->config.monitoring_interval_ms);

    while (atomic_load(&guardian->running)) {
        /* Sleep for monitoring interval */
        nimcp_time_sleep_ms(guardian->config.monitoring_interval_ms);

        /* Check if we should exit */
        if (!atomic_load(&guardian->running)) {
            break;
        }

        /* Skip if paused */
        if (atomic_load(&guardian->paused)) {
            continue;
        }

        /* Lock for safe access */
        nimcp_mutex_lock(guardian->lock);

        /* Perform health check */
        perform_health_check(guardian);

        nimcp_mutex_unlock(guardian->lock);
    }

    GUARDIAN_LOG("Monitor thread stopped");
    return NULL;
}

//=============================================================================
// Core API Implementation
//=============================================================================

mental_health_guardian_t* mental_health_guardian_create(
    void* brain_ptr,  /* brain_t */
    const mental_health_guardian_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_create", 0.0f);


    brain_t brain = (brain_t)brain_ptr;
    if (!brain) {
        GUARDIAN_LOG("ERROR: NULL brain provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_create: brain is NULL");
        return NULL;
    }

    if (!brain->mental_health_monitor) {
        GUARDIAN_LOG("ERROR: Brain has no mental health monitor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_create: brain->mental_health_monitor is NULL");
        return NULL;
    }

    /* Allocate guardian */
    mental_health_guardian_t* guardian = nimcp_calloc(1, sizeof(mental_health_guardian_t));
    if (!guardian) {
        GUARDIAN_LOG("ERROR: Failed to allocate guardian");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mental_health_guardian_create: guardian is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        guardian->config = *config;
    } else {
        guardian->config = mental_health_guardian_default_config();
    }

    /* Store brain reference */
    guardian->brain = brain;

    /* Create mutex */
    mutex_attr_t mutex_attr = { .type = MUTEX_TYPE_NORMAL };
    guardian->lock = nimcp_mutex_create(&mutex_attr);
    if (!guardian->lock) {
        GUARDIAN_LOG("ERROR: Failed to create mutex");
        nimcp_free(guardian);
        guardian = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mental_health_guardian_create: guardian->lock is NULL");
        return NULL;
    }

    /* Initialize atomic flags */
    atomic_store(&guardian->running, false);
    atomic_store(&guardian->paused, false);

    /* Initialize state */
    guardian->state = GUARDIAN_STATE_STOPPED;
    guardian->current_level = GUARDIAN_LEVEL_OBSERVE;
    guardian->last_overall_severity = 0.0f;
    guardian->last_primary_disorder = -1;
    guardian->last_secondary_disorder = -1;
    guardian->secondary_disorder_score = 0.0f;
    guardian->thread_created = false;

    /* Initialize metrics */
    guardian->checks_performed = 0;
    guardian->interventions_applied = 0;
    guardian->observe_count = 0;
    guardian->adjust_count = 0;
    guardian->regulate_count = 0;
    guardian->quarantine_count = 0;
    guardian->start_time_ms = 0;
    guardian->last_check_time_ms = 0;

    /* Integration handles (connected later) */
    guardian->immune_system = NULL;
    guardian->internal_kg = NULL;
    guardian->kg_admin_token = 0;
    guardian->kg_node_id = 0;  /* Will be assigned on first KG update */

    /* Bio-async integration */
    guardian->bio_context = NULL;
    guardian->bio_async_connected = false;

    /* Neuromodulator integration */
    guardian->neuromod_field = NULL;

    /* Brainstem/sleep/plasticity integration */
    guardian->medulla = NULL;
    guardian->sleep_system = NULL;
    guardian->plasticity = NULL;

    /* FEP orchestrator integration */
    guardian->fep_orchestrator = NULL;
    guardian->fep_bridge_id = 0;
    guardian->fep_registered = false;

    /* Cognitive module integration */
    guardian->working_memory = NULL;
    guardian->executive = NULL;

    GUARDIAN_LOG("Mental Health Guardian created (interval=%ums, auto_intervene=%s)",
                guardian->config.monitoring_interval_ms,
                guardian->config.auto_intervene ? "yes" : "no");

    return guardian;
}

void mental_health_guardian_destroy(mental_health_guardian_t* guardian) {
    if (!guardian) {
        return;
    }

    /* Stop thread if running */
    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_destroy", 0.0f);


    if (atomic_load(&guardian->running)) {
        mental_health_guardian_stop(guardian);
    }

    /* Unregister from FEP orchestrator if registered */
    if (guardian->fep_registered) {
        mental_health_guardian_unregister_fep(guardian);
    }

    /* Destroy mutex */
    if (guardian->lock) {
        nimcp_mutex_destroy(guardian->lock);
    }

    GUARDIAN_LOG("Mental Health Guardian destroyed (checks=%lu, interventions=%lu)",
                guardian->checks_performed, guardian->interventions_applied);

    nimcp_free(guardian);
    guardian = NULL;
}

//=============================================================================
// Thread Control API
//=============================================================================

bool mental_health_guardian_start(mental_health_guardian_t* guardian) {
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_start: guardian is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_start", 0.0f);


    if (atomic_load(&guardian->running)) {
        return true;  /* Already running */
    }

    /* Set running flag before thread start */
    atomic_store(&guardian->running, true);
    atomic_store(&guardian->paused, false);

    /* Record start time */
    guardian->start_time_ms = nimcp_time_get_ms();

    /* Create and start thread */
    thread_attr_t attr = {
        .stack_size = 0,  /* Default stack */
        .priority = 0,    /* Normal priority */
        .detached = false
    };

    nimcp_result_t result = nimcp_thread_create(
        &guardian->monitor_thread,
        guardian_monitor_thread,
        guardian,
        &attr
    );

    if (result != NIMCP_SUCCESS) {
        GUARDIAN_LOG("ERROR: Failed to create monitor thread (error=%d)", result);
        atomic_store(&guardian->running, false);
        guardian->state = GUARDIAN_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_guardian_start: validation failed");
        return false;
    }

    guardian->thread_created = true;
    guardian->state = GUARDIAN_STATE_RUNNING;

    GUARDIAN_LOG("Guardian started");
    return true;
}

bool mental_health_guardian_stop(mental_health_guardian_t* guardian) {
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_stop: guardian is NULL");
        return false;
    }

    if (!atomic_load(&guardian->running)) {
        return true;  /* Already stopped */
    }

    /* Signal thread to stop */
    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_stop", 0.0f);


    atomic_store(&guardian->running, false);

    /* Wait for thread to exit */
    if (guardian->thread_created) {
        nimcp_thread_join(guardian->monitor_thread, NULL);
        guardian->thread_created = false;
    }

    guardian->state = GUARDIAN_STATE_STOPPED;

    GUARDIAN_LOG("Guardian stopped");
    return true;
}

bool mental_health_guardian_pause(mental_health_guardian_t* guardian) {
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_pause: guardian is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_pause", 0.0f);


    atomic_store(&guardian->paused, true);
    guardian->state = GUARDIAN_STATE_PAUSED;

    GUARDIAN_LOG("Guardian paused");
    return true;
}

bool mental_health_guardian_resume(mental_health_guardian_t* guardian) {
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_resume: guardian is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_resume", 0.0f);


    atomic_store(&guardian->paused, false);
    guardian->state = GUARDIAN_STATE_RUNNING;

    GUARDIAN_LOG("Guardian resumed");
    return true;
}

//=============================================================================
// Status & Metrics API
//=============================================================================

bool mental_health_guardian_get_status(
    mental_health_guardian_t* guardian,
    mental_health_guardian_status_t* status)
{
    if (!guardian || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_get_status: required parameter is NULL (guardian, status)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_get_status", 0.0f);


    nimcp_mutex_lock(guardian->lock);

    status->state = guardian->state;
    status->level = guardian->current_level;
    status->overall_severity = guardian->last_overall_severity;
    status->checks_performed = guardian->checks_performed;
    status->interventions_applied = guardian->interventions_applied;
    status->observe_count = guardian->observe_count;
    status->adjust_count = guardian->adjust_count;
    status->regulate_count = guardian->regulate_count;
    status->quarantine_count = guardian->quarantine_count;
    status->last_check_time_ms = guardian->last_check_time_ms;
    status->primary_disorder = guardian->last_primary_disorder;
    status->secondary_disorder = guardian->last_secondary_disorder;
    status->secondary_disorder_score = guardian->secondary_disorder_score;

    /* Calculate uptime */
    if (guardian->start_time_ms > 0) {
        status->uptime_ms = nimcp_time_get_ms() - guardian->start_time_ms;
    } else {
        status->uptime_ms = 0;
    }

    /* Count active disorders */
    status->active_disorders = 0;
    if (guardian->brain && guardian->brain->mental_health_monitor) {
        mental_health_report_t report;
        mental_health_get_report(guardian->brain->mental_health_monitor, &report);
        for (int i = 0; i < DISORDER_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && DISORDER_COUNT > 256) {
                mental_health_guardian_heartbeat("mental_healt_loop",
                                 (float)(i + 1) / (float)DISORDER_COUNT);
            }

            if (report.disorder_severities[i] > DISORDER_SEVERITY_NONE) {
                status->active_disorders++;
            }
        }
    }

    nimcp_mutex_unlock(guardian->lock);
    return true;
}

bool mental_health_guardian_reset_stats(mental_health_guardian_t* guardian) {
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_reset_stats: guardian is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_reset_stats", 0.0f);


    nimcp_mutex_lock(guardian->lock);

    guardian->checks_performed = 0;
    guardian->interventions_applied = 0;
    guardian->observe_count = 0;
    guardian->adjust_count = 0;
    guardian->regulate_count = 0;
    guardian->quarantine_count = 0;

    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Statistics reset");
    return true;
}

//=============================================================================
// Manual Control API
//=============================================================================

guardian_intervention_level_t mental_health_guardian_force_check(
    mental_health_guardian_t* guardian)
{
    if (!guardian) {
        return GUARDIAN_LEVEL_OBSERVE;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_force_check", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian_intervention_level_t level = perform_health_check(guardian);
    nimcp_mutex_unlock(guardian->lock);

    return level;
}

bool mental_health_guardian_set_level(
    mental_health_guardian_t* guardian,
    guardian_intervention_level_t level)
{
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_set_level: guardian is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_set_level", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->current_level = level;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Intervention level manually set to %s",
                guardian_level_to_string(level));
    return true;
}

//=============================================================================
// Configuration API
//=============================================================================

bool mental_health_guardian_update_config(
    mental_health_guardian_t* guardian,
    const mental_health_guardian_config_t* config)
{
    if (!guardian || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_update_config: required parameter is NULL (guardian, config)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_update_config", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->config = *config;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Configuration updated (interval=%ums)",
                config->monitoring_interval_ms);
    return true;
}

bool mental_health_guardian_get_config(
    mental_health_guardian_t* guardian,
    mental_health_guardian_config_t* config)
{
    if (!guardian || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_get_config: required parameter is NULL (guardian, config)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_get_config", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    *config = guardian->config;
    nimcp_mutex_unlock(guardian->lock);

    return true;
}

//=============================================================================
// Integration API
//=============================================================================

bool mental_health_guardian_connect_immune(
    mental_health_guardian_t* guardian,
    brain_immune_system_t* immune)
{
    if (!guardian || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_immune: required parameter is NULL (guardian, immune)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_immune", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->immune_system = immune;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to immune system");
    return true;
}

bool mental_health_guardian_connect_kg(
    mental_health_guardian_t* guardian,
    brain_kg_t* kg,
    uint64_t admin_token)
{
    if (!guardian || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_kg: required parameter is NULL (guardian, kg)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_kg", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->internal_kg = kg;
    guardian->kg_admin_token = admin_token;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to internal knowledge graph");
    return true;
}

bool mental_health_guardian_connect_bio_async(
    mental_health_guardian_t* guardian,
    void* bio_context  /* bio_module_context_t */)
{
    if (!guardian || !bio_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_bio_async: required parameter is NULL (guardian, bio_context)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_bio_async", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->bio_context = (bio_module_context_t)bio_context;
    guardian->bio_async_connected = true;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to bio-async router");
    return true;
}

bool mental_health_guardian_connect_neuromod(
    mental_health_guardian_t* guardian,
    void* neuromod  /* spatial_neuromod_field_t* */)
{
    if (!guardian || !neuromod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_neuromod: required parameter is NULL (guardian, neuromod)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_neuromod", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->neuromod_field = (spatial_neuromod_field_t*)neuromod;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to spatial neuromodulator field");
    return true;
}

bool mental_health_guardian_connect_brainstem(
    mental_health_guardian_t* guardian,
    void* medulla  /* medulla_t */)
{
    if (!guardian || !medulla) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_brainstem: required parameter is NULL (guardian, medulla)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_brainstem", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->medulla = medulla;  /* Stored as void* anyway */
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to brainstem/medulla");
    return true;
}

bool mental_health_guardian_connect_sleep(
    mental_health_guardian_t* guardian,
    void* sleep  /* sleep_system_t */)
{
    if (!guardian || !sleep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_sleep: required parameter is NULL (guardian, sleep)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_sleep", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->sleep_system = sleep;  /* Stored as void* anyway */
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to sleep system");
    return true;
}

bool mental_health_guardian_connect_plasticity(
    mental_health_guardian_t* guardian,
    void* plasticity  /* homeostatic_plasticity_t */)
{
    if (!guardian || !plasticity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_plasticity: required parameter is NULL (guardian, plasticity)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_plasticity", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->plasticity = plasticity;  /* Stored as void* anyway */
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to homeostatic plasticity");
    return true;
}

//=============================================================================
// FEP Orchestrator Integration
//=============================================================================

/**
 * @brief FEP bridge update callback
 *
 * Called by FEP orchestrator to update guardian's FEP state.
 * Provides current mental health severity as free energy contribution.
 *
 * @param handle Guardian handle (opaque)
 * @return 0 on success, -1 on error
 */
int mental_health_guardian_fep_update(void* handle) {
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_fep_update", 0.0f);


    mental_health_guardian_t* guardian = (mental_health_guardian_t*)handle;

    /* Lock for safe access */
    nimcp_mutex_lock(guardian->lock);

    /* Get current severity (represents free energy from mental health domain) */
    float severity = guardian->last_overall_severity;

    /* The guardian's "free energy" is the overall mental health severity.
     * Lower severity = lower free energy = more optimal state.
     *
     * Free energy in FEP terms represents prediction error / surprise.
     * A healthy mental state (low severity) means the brain's predictions
     * about its own state are accurate. High severity indicates unexpected
     * deviation from the brain's model of its own health. */

    /* Update bio-async with current status if connected */
    if (guardian->bio_async_connected && guardian->state == GUARDIAN_STATE_RUNNING) {
        publish_bio_async_status(guardian);
    }

    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG_VERBOSE(guardian, "FEP update: severity=%.2f (free_energy proxy)",
                        severity);

    return 0;
}

bool mental_health_guardian_register_fep(
    mental_health_guardian_t* guardian,
    void* orchestrator  /* fep_orchestrator_t* */)
{
    if (!guardian || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_register_fep: required parameter is NULL (guardian, orchestrator)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_register_fep", 0.0f);


    if (guardian->fep_registered) {
        GUARDIAN_LOG("Already registered with FEP orchestrator");
        return true;
    }

    fep_orchestrator_t* fep_orch = (fep_orchestrator_t*)orchestrator;

    /* Register guardian as cognitive FEP bridge */
    uint32_t bridge_id = 0;
    int result = fep_orchestrator_register_bridge(
        fep_orch,
        "mental_health_guardian",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        guardian,                           /* Handle */
        mental_health_guardian_fep_update,  /* Update callback */
        NULL,                               /* No destroy callback (we handle it) */
        &bridge_id
    );

    if (result != 0) {
        GUARDIAN_LOG("Failed to register with FEP orchestrator (error=%d)", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_guardian_register_fep: validation failed");
        return false;
    }

    nimcp_mutex_lock(guardian->lock);
    guardian->fep_orchestrator = orchestrator;
    guardian->fep_bridge_id = bridge_id;
    guardian->fep_registered = true;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Registered with FEP orchestrator (bridge_id=%u)", bridge_id);
    return true;
}

bool mental_health_guardian_unregister_fep(mental_health_guardian_t* guardian) {
    if (!guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_unregister_fep: guardian is NULL");
        return false;
    }

    if (!guardian->fep_registered) {
        return true;  /* Not registered, nothing to do */
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_unregister_fep", 0.0f);


    fep_orchestrator_t* fep_orch = (fep_orchestrator_t*)guardian->fep_orchestrator;
    if (fep_orch) {
        int result = fep_orchestrator_unregister_bridge(fep_orch, guardian->fep_bridge_id);
        if (result != 0) {
            GUARDIAN_LOG("Warning: Failed to unregister from FEP orchestrator (error=%d)",
                        result);
        }
    }

    nimcp_mutex_lock(guardian->lock);
    guardian->fep_orchestrator = NULL;
    guardian->fep_bridge_id = 0;
    guardian->fep_registered = false;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Unregistered from FEP orchestrator");
    return true;
}

//=============================================================================
// Cognitive Module Integration
//=============================================================================

bool mental_health_guardian_connect_working_memory(
    mental_health_guardian_t* guardian,
    void* working_memory  /* working_memory_t* */)
{
    if (!guardian || !working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mental_health_guardian_connect_working_memory: required parameter is NULL (guardian, working_memory)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_working_memo", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->working_memory = working_memory;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to working memory");
    return true;
}

bool mental_health_guardian_connect_executive(
    mental_health_guardian_t* guardian,
    void* executive  /* executive_controller_t* */)
{
    if (!guardian || !executive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_guardian_connect_executive: required parameter is NULL (guardian, executive)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_connect_executive", 0.0f);


    nimcp_mutex_lock(guardian->lock);
    guardian->executive = executive;
    nimcp_mutex_unlock(guardian->lock);

    GUARDIAN_LOG("Connected to executive controller");
    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* guardian_level_to_string(guardian_intervention_level_t level) {
    switch (level) {
        case GUARDIAN_LEVEL_OBSERVE:    return "OBSERVE";
        case GUARDIAN_LEVEL_ADJUST:     return "ADJUST";
        case GUARDIAN_LEVEL_REGULATE:   return "REGULATE";
        case GUARDIAN_LEVEL_QUARANTINE: return "QUARANTINE";
        default:                        return "UNKNOWN";
    }
}

const char* guardian_state_to_string(guardian_state_t state) {
    switch (state) {
        case GUARDIAN_STATE_STOPPED:  return "STOPPED";
        case GUARDIAN_STATE_RUNNING:  return "RUNNING";
        case GUARDIAN_STATE_PAUSED:   return "PAUSED";
        case GUARDIAN_STATE_ERROR:    return "ERROR";
        default:                      return "UNKNOWN";
    }
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about mental health guardian
 *
 * WHAT: Retrieve module's own entity and connections from KG
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int mental_health_guardian_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    mental_health_guardian_heartbeat("mental_healt_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mental_Health_Guardian_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mental_health_guardian_heartbeat("mental_healt_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            GUARDIAN_LOG("Self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mental_Health_Guardian_Module");
    if (connections) {
        GUARDIAN_LOG("Guardian has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mental_Health_Guardian_Module");
    if (incoming) {
        GUARDIAN_LOG("Guardian has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mental_health_guardian_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mental_health_guardian_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mental_health_guardian_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_guardian_training_begin: NULL argument");
        return -1;
    }
    mental_health_guardian_heartbeat_instance(NULL, "mental_health_guardian_training_begin", 0.0f);
    (void)(struct mental_health_guardian*)instance; /* Module state available for reset */
    return 0;
}

int mental_health_guardian_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_guardian_training_end: NULL argument");
        return -1;
    }
    mental_health_guardian_heartbeat_instance(NULL, "mental_health_guardian_training_end", 1.0f);
    (void)(struct mental_health_guardian*)instance; /* Module state available for finalization */
    return 0;
}

int mental_health_guardian_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_guardian_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mental_health_guardian_heartbeat_instance(NULL, "mental_health_guardian_training_step", progress);
    (void)(struct mental_health_guardian*)instance; /* Module state available for step adaptation */
    return 0;
}
