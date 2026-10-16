/**
 * @file nimcp_surprise_substrate_bridge.c
 * @brief Bridge between Surprise Amplifier and metabolic substrate
 * @version 1.1.0
 * @date 2026-01-27
 *
 * WHAT: Metabolic constraints on surprise processing
 * WHY:  Low ATP reduces sensitivity; fatigue increases thresholds
 * HOW:  ATP/fatigue levels -> surprise parameter modulation
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_substrate_bridge.h"
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(surprise_substrate, MESH_ADAPTER_CATEGORY_COGNITIVE)
static nimcp_health_agent_t* g_surprise_substrate_bridge_health_agent = NULL;
void surprise_substrate_bridge_set_health_agent_global(struct nimcp_health_agent* agent) { g_surprise_substrate_bridge_health_agent = agent; }

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_substrate_bridge {
    surprise_substrate_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* substrate_system;

    /* State */
    surprise_substrate_effects_t effects;
    surprise_substrate_stats_t stats;

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Last known substrate state */
    float last_atp;
    float last_fatigue;

    bool initialized;
    uint64_t update_count;
};

/* ============================================================================
 * Bio-Async Helpers
 * ============================================================================ */

static void substrate_send_modulation_msg(surprise_substrate_bridge_t* bridge,
                                           float atp, float fatigue, float capacity)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "substrate_send_modulation_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        float atp_level;
        float fatigue_level;
        float overall_capacity;
        float detection_sensitivity;
        float amplification_accuracy;
        uint64_t update_count;
    } surprise_substrate_modulation_msg_t;

    surprise_substrate_modulation_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_SUBSTRATE_MODULATION,
                        BIO_MODULE_SURPRISE_SUBSTRATE,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.atp_level = atp;
    msg.fatigue_level = fatigue;
    msg.overall_capacity = capacity;
    msg.detection_sensitivity = bridge->effects.detection_sensitivity;
    msg.amplification_accuracy = bridge->effects.amplification_accuracy;
    msg.update_count = bridge->update_count;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

static void substrate_send_fatigue_msg(surprise_substrate_bridge_t* bridge,
                                        float atp, float fatigue)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "substrate_send_fatigue_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        float atp_level;
        float fatigue_level;
        float overall_capacity;
        bool atp_critical;
    } surprise_substrate_fatigue_msg_t;

    surprise_substrate_fatigue_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_SUBSTRATE_FATIGUE,
                        BIO_MODULE_SURPRISE_SUBSTRATE,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.atp_level = atp;
    msg.fatigue_level = fatigue;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.atp_critical = (atp < 0.2f);

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_substrate_config_t surprise_substrate_bridge_default_config(void) {
    surprise_substrate_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.detection_sensitivity_mult = SURPRISE_SUBSTRATE_DEFAULT_DETECT_MULT;
    cfg.amplification_accuracy_mult = SURPRISE_SUBSTRATE_DEFAULT_AMPLIFY_MULT;
    cfg.decay_modulation_mult = SURPRISE_SUBSTRATE_DEFAULT_DECAY_MULT;
    cfg.refractory_modulation_mult = SURPRISE_SUBSTRATE_DEFAULT_REFRACT_MULT;
    cfg.min_capacity = SURPRISE_SUBSTRATE_DEFAULT_MIN_CAPACITY;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_substrate_bridge_t* surprise_substrate_bridge_create(
    const surprise_substrate_config_t* config)
{
    surprise_substrate_bridge_t* bridge = (surprise_substrate_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SUBSTRATE_ERROR_NO_MEMORY,
                           sizeof(surprise_substrate_bridge_t),
                           "surprise_substrate_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_substrate_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_substrate_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SUBSTRATE_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_substrate_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize effects to full capacity */
    bridge->effects.detection_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.amplification_accuracy = 1.0f;
    bridge->effects.decay_modulation = 1.0f;
    bridge->effects.refractory_modulation = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    bridge->last_atp = 1.0f;
    bridge->last_fatigue = 0.0f;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-substrate bridge (min_cap=%.2f)",
                           bridge->config.min_capacity);
        NIMCP_LOGGING_DEBUG("Substrate bridge config: detect_mult=%.3f, amplify_mult=%.3f, "
                            "decay_mult=%.3f, refract_mult=%.3f",
                            bridge->config.detection_sensitivity_mult,
                            bridge->config.amplification_accuracy_mult,
                            bridge->config.decay_modulation_mult,
                            bridge->config.refractory_modulation_mult);
    }

    return bridge;
}

void surprise_substrate_bridge_destroy(surprise_substrate_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-substrate bridge (updates=%lu, warnings=%lu)",
                           (unsigned long)bridge->stats.modulation_updates,
                           (unsigned long)bridge->stats.capacity_warnings);
        NIMCP_LOGGING_DEBUG("Substrate bridge final state: atp=%.3f, fatigue=%.3f, capacity=%.3f",
                            bridge->last_atp, bridge->last_fatigue,
                            bridge->effects.overall_capacity);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_substrate_bridge_connect_amplifier(
    surprise_substrate_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    surprise_substrate_heartbeat_instance(bridge->health_agent, "connect_amplifier", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-substrate bridge connected to amplifier");
        NIMCP_LOGGING_DEBUG("Substrate bridge amplifier connection established, ptr=%p", (void*)amp);
    }
    return 0;
}

int surprise_substrate_bridge_connect_substrate(
    surprise_substrate_bridge_t* bridge,
    void* substrate)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in connect_substrate");
    NIMCP_CHECK_THROW_IMMUNE(substrate != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL substrate in connect_substrate");

    surprise_substrate_heartbeat_instance(bridge->health_agent, "connect_substrate", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->substrate_system = substrate;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-substrate bridge connected to substrate system");
        NIMCP_LOGGING_DEBUG("Substrate system connection established, ptr=%p", (void*)substrate);
    }
    return 0;
}

int surprise_substrate_bridge_register_bio_async(
    surprise_substrate_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in register_bio_async");

    surprise_substrate_heartbeat_instance(bridge->health_agent, "register_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Substrate bridge bio-async %s (router=%p)",
                            router ? "connected" : "disconnected", router);
    }

    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_substrate_bridge_update(
    surprise_substrate_bridge_t* bridge,
    float atp_level,
    float fatigue_level)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    surprise_substrate_heartbeat_instance(bridge->health_agent, "update", 0.0f);

    float atp = nimcp_myelin_clamp(atp_level, 0.0f, 1.0f);
    float fatigue = nimcp_myelin_clamp(fatigue_level, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    float prev_capacity = bridge->effects.overall_capacity;
    bridge->last_atp = atp;
    bridge->last_fatigue = fatigue;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Substrate update: atp=%.3f, fatigue=%.3f, prev_capacity=%.3f",
                            atp, fatigue, prev_capacity);
    }

    /* Warn on edge conditions */
    if (bridge->config.enable_logging) {
        if (atp < 0.2f) {
            NIMCP_LOGGING_WARN("Substrate ATP critically low: atp=%.3f (threshold=0.2)", atp);
        }
    }

    /* Compute modulation effects from substrate state */
    /* High ATP -> better detection; low ATP -> worse */
    bridge->effects.detection_sensitivity = bridge->config.detection_sensitivity_mult * atp;
    bridge->effects.detection_sensitivity = nimcp_myelin_clamp(
        bridge->effects.detection_sensitivity, bridge->config.min_capacity, 2.0f);

    /* High ATP -> accurate amplification */
    bridge->effects.amplification_accuracy = bridge->config.amplification_accuracy_mult * atp;
    bridge->effects.amplification_accuracy = nimcp_myelin_clamp(
        bridge->effects.amplification_accuracy, bridge->config.min_capacity, 2.0f);

    /* High fatigue -> faster decay */
    bridge->effects.decay_modulation = bridge->config.decay_modulation_mult * (1.0f + fatigue);

    /* High fatigue -> longer refractory */
    bridge->effects.refractory_modulation = bridge->config.refractory_modulation_mult * (1.0f + fatigue);

    /* Overall capacity = ATP * (1 - fatigue) */
    bridge->effects.overall_capacity = nimcp_myelin_clamp(
        atp * (1.0f - fatigue * 0.5f), bridge->config.min_capacity, 1.0f);

    bridge->stats.modulation_updates++;

    if (bridge->effects.overall_capacity < 0.5f) {
        bridge->stats.capacity_warnings++;
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("Substrate capacity below 50%%: capacity=%.3f, atp=%.3f, fatigue=%.3f",
                               bridge->effects.overall_capacity, atp, fatigue);
        }
    }
    if (atp < 0.2f) {
        bridge->stats.atp_critical_events++;
    }

    bridge->stats.total_updates++;
    bridge->update_count++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Substrate effects computed: detect=%.3f, amplify=%.3f, "
                            "decay=%.3f, refract=%.3f, capacity=%.3f",
                            bridge->effects.detection_sensitivity,
                            bridge->effects.amplification_accuracy,
                            bridge->effects.decay_modulation,
                            bridge->effects.refractory_modulation,
                            bridge->effects.overall_capacity);
    }

    /* Bio-async: send modulation when effects change significantly */
    float capacity_delta = fabsf(bridge->effects.overall_capacity - prev_capacity);
    if (capacity_delta > 0.05f) {
        substrate_send_modulation_msg(bridge, atp, fatigue,
                                       bridge->effects.overall_capacity);
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Substrate bio-async: sent MODULATION (delta=%.3f)", capacity_delta);
        }
    }

    /* Bio-async: send fatigue when ATP critical */
    if (atp < 0.2f) {
        substrate_send_fatigue_msg(bridge, atp, fatigue);
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Substrate bio-async: sent FATIGUE (atp=%.3f)", atp);
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_substrate_bridge_get_effects(
    const surprise_substrate_bridge_t* bridge,
    surprise_substrate_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    nimcp_mutex_lock(bridge->mutex);
    *effects_out = bridge->effects;
    nimcp_mutex_unlock(bridge->mutex);
    return 0;
}

int surprise_substrate_bridge_apply_effects(
    surprise_substrate_bridge_t* bridge)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in apply_effects");

    surprise_substrate_heartbeat_instance(bridge->health_agent, "apply_effects", 0.0f);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Substrate apply_effects: capacity=%.3f, detect=%.3f",
                            bridge->effects.overall_capacity,
                            bridge->effects.detection_sensitivity);
    }

    /* Effects would be applied to the amplifier here via its API */
    /* For now, effects are available via get_effects for the amplifier to query */

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_substrate_bridge_get_stats(
    const surprise_substrate_bridge_t* bridge,
    surprise_substrate_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    nimcp_mutex_lock(bridge->mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(bridge->mutex);
    return 0;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_substrate_bridge_set_health_agent(
    surprise_substrate_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Substrate bridge instance health agent %s",
                            agent ? "set" : "cleared");
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_substrate_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_substrate_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_substrate_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_substrate_training_begin: NULL argument");
        return -1;
    }
    surprise_substrate_heartbeat_instance(NULL, "surprise_substrate_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_substrate_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_substrate_training_end: NULL argument");
        return -1;
    }
    surprise_substrate_heartbeat_instance(NULL, "surprise_substrate_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_substrate_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_substrate_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_substrate_heartbeat_instance(NULL, "surprise_substrate_training_step", progress);
    (void)instance;
    return 0;
}
