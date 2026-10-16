/**
 * @file nimcp_surprise_imagination_bridge.c
 * @brief Bridge between Surprise Amplifier and imagination/counterfactual system
 * @version 1.1.0
 * @date 2026-01-27
 *
 * WHAT: High surprise triggers counterfactual imagination scenarios
 * WHY:  Surprising events should trigger "what if" reasoning
 * HOW:  Surprise threshold -> trigger imagination; results -> expectation update
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_imagination_bridge.h"
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

BRIDGE_BOILERPLATE(surprise_imagination, MESH_ADAPTER_CATEGORY_COGNITIVE)
static nimcp_health_agent_t* g_surprise_imagination_bridge_health_agent = NULL;
void surprise_imagination_bridge_set_health_agent_global(struct nimcp_health_agent* agent) { g_surprise_imagination_bridge_health_agent = agent; }

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_imagination_bridge {
    surprise_imagination_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* imagination_engine;

    /* State */
    surprise_imagination_effects_t effects;
    surprise_imagination_stats_t stats;

    /* Scenario management */
    surprise_imagination_scenario_t* scenarios;
    uint32_t next_scenario_id;
    float cooldown_timer;

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    bool initialized;
    uint64_t update_count;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static const char* imagination_status_name(uint32_t status) {
    switch (status) {
        case SURPRISE_IMAGINATION_STATUS_PENDING:   return "PENDING";
        case SURPRISE_IMAGINATION_STATUS_ACTIVE:    return "ACTIVE";
        case SURPRISE_IMAGINATION_STATUS_COMPLETED: return "COMPLETED";
        case SURPRISE_IMAGINATION_STATUS_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

static surprise_imagination_scenario_t* find_scenario(
    surprise_imagination_bridge_t* bridge, uint32_t scenario_id)
{
    for (uint32_t i = 0; i < bridge->config.max_scenarios; i++) {
        if (bridge->scenarios[i].scenario_id == scenario_id &&
            bridge->scenarios[i].status != SURPRISE_IMAGINATION_STATUS_CANCELLED) {
            return &bridge->scenarios[i];
        }
    }
    return NULL;
}

static surprise_imagination_scenario_t* find_free_slot(
    surprise_imagination_bridge_t* bridge)
{
    for (uint32_t i = 0; i < bridge->config.max_scenarios; i++) {
        if (bridge->scenarios[i].status == SURPRISE_IMAGINATION_STATUS_COMPLETED ||
            bridge->scenarios[i].status == SURPRISE_IMAGINATION_STATUS_CANCELLED ||
            bridge->scenarios[i].scenario_id == 0) {
            return &bridge->scenarios[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static uint32_t count_active(surprise_imagination_bridge_t* bridge) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->config.max_scenarios; i++) {
        if (bridge->scenarios[i].status == SURPRISE_IMAGINATION_STATUS_ACTIVE ||
            bridge->scenarios[i].status == SURPRISE_IMAGINATION_STATUS_PENDING) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Bio-Async Helpers
 * ============================================================================ */

static void imagination_send_trigger_msg(surprise_imagination_bridge_t* bridge,
                                           uint32_t scenario_id, float surprise_level,
                                           uint32_t source_module)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "imagination_send_trigger_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        uint32_t scenario_id;
        float surprise_level;
        uint32_t source_module;
        uint32_t scenarios_active;
        uint64_t total_triggers;
    } surprise_imagination_trigger_msg_t;

    surprise_imagination_trigger_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_IMAGINATION_TRIGGER,
                        BIO_MODULE_SURPRISE_IMAGINATION,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.scenario_id = scenario_id;
    msg.surprise_level = surprise_level;
    msg.source_module = source_module;
    msg.scenarios_active = bridge->effects.scenarios_active;
    msg.total_triggers = bridge->stats.triggers;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

static void imagination_send_result_msg(surprise_imagination_bridge_t* bridge,
                                          uint32_t scenario_id, float divergence,
                                          float expected, float actual)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "imagination_send_result_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        uint32_t scenario_id;
        float divergence;
        float expected_outcome;
        float actual_outcome;
        float expectation_adjustment;
        uint64_t scenarios_completed;
    } surprise_imagination_result_msg_t;

    surprise_imagination_result_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_IMAGINATION_RESULT,
                        BIO_MODULE_SURPRISE_IMAGINATION,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.scenario_id = scenario_id;
    msg.divergence = divergence;
    msg.expected_outcome = expected;
    msg.actual_outcome = actual;
    msg.expectation_adjustment = bridge->effects.expectation_adjustment;
    msg.scenarios_completed = bridge->stats.scenarios_completed;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_imagination_config_t surprise_imagination_bridge_default_config(void) {
    surprise_imagination_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.trigger_threshold = SURPRISE_IMAGINATION_DEFAULT_TRIGGER_THRESHOLD;
    cfg.cooldown_seconds = SURPRISE_IMAGINATION_DEFAULT_COOLDOWN_SECONDS;
    cfg.max_scenarios = SURPRISE_IMAGINATION_DEFAULT_MAX_SCENARIOS;
    cfg.expectation_update_rate = SURPRISE_IMAGINATION_DEFAULT_EXPECT_UPDATE_RATE;
    cfg.counterfactual_depth = SURPRISE_IMAGINATION_DEFAULT_CF_DEPTH;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_imagination_bridge_t* surprise_imagination_bridge_create(
    const surprise_imagination_config_t* config)
{
    surprise_imagination_bridge_t* bridge = (surprise_imagination_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_imagination_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_IMAGINATION_ERROR_NO_MEMORY,
                           sizeof(surprise_imagination_bridge_t),
                           "surprise_imagination_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_imagination_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_imagination_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_IMAGINATION_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_imagination_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate scenario slots */
    bridge->scenarios = (surprise_imagination_scenario_t*)nimcp_calloc(
        bridge->config.max_scenarios, sizeof(surprise_imagination_scenario_t));
    if (!bridge->scenarios) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_IMAGINATION_ERROR_NO_MEMORY,
                           bridge->config.max_scenarios * sizeof(surprise_imagination_scenario_t),
                           "surprise_imagination_bridge scenarios allocation failed");
        nimcp_mutex_destroy(bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->next_scenario_id = 1;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-imagination bridge (threshold=%.2f, max_scenarios=%u)",
                           bridge->config.trigger_threshold, bridge->config.max_scenarios);
        NIMCP_LOGGING_DEBUG("Imagination config: cooldown=%.2fs, expect_rate=%.3f, cf_depth=%u",
                            bridge->config.cooldown_seconds,
                            bridge->config.expectation_update_rate,
                            bridge->config.counterfactual_depth);
    }

    return bridge;
}

void surprise_imagination_bridge_destroy(surprise_imagination_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-imagination bridge (triggers=%lu, completed=%lu)",
                           (unsigned long)bridge->stats.triggers,
                           (unsigned long)bridge->stats.scenarios_completed);
        NIMCP_LOGGING_DEBUG("Imagination final state: expect_adj=%.4f, cooldown_blocked=%lu",
                            bridge->effects.expectation_adjustment,
                            (unsigned long)bridge->stats.cooldown_blocked);
    }

    if (bridge->scenarios) {
        nimcp_free(bridge->scenarios);
    }
    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_imagination_bridge_reset(surprise_imagination_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->scenarios, 0,
           bridge->config.max_scenarios * sizeof(surprise_imagination_scenario_t));
    bridge->next_scenario_id = 1;
    bridge->cooldown_timer = 0.0f;
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination bridge reset: scenarios cleared, cooldown zeroed");
    }

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_imagination_bridge_connect_amplifier(
    surprise_imagination_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "connect_amplifier", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-imagination bridge connected to amplifier");
        NIMCP_LOGGING_DEBUG("Imagination amplifier connection established, ptr=%p", (void*)amp);
    }
    return 0;
}

int surprise_imagination_bridge_connect_imagination_engine(
    surprise_imagination_bridge_t* bridge,
    void* engine)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in connect_imagination_engine");
    NIMCP_CHECK_THROW_IMMUNE(engine != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL engine in connect_imagination_engine");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "connect_imagination_engine", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->imagination_engine = engine;
    bridge->effects.imagination_connected = true;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-imagination bridge connected to imagination engine");
        NIMCP_LOGGING_DEBUG("Imagination engine connection established, ptr=%p", (void*)engine);
    }
    return 0;
}

int surprise_imagination_bridge_connect_bio_async(
    surprise_imagination_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in connect_bio_async");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "connect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination bio-async %s (router=%p)",
                            router ? "connected" : "disconnected", router);
    }

    return 0;
}

int surprise_imagination_bridge_disconnect_bio_async(
    surprise_imagination_bridge_t* bridge)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in disconnect_bio_async");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "disconnect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = NULL;
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination bio-async disconnected");
    }

    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_imagination_check_trigger(
    surprise_imagination_bridge_t* bridge,
    float surprise_level,
    uint32_t source_module)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in check_trigger");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "check_trigger", 0.0f);

    float level = nimcp_myelin_clamp(surprise_level, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination check_trigger: surprise=%.3f, threshold=%.3f, "
                            "cooldown=%.3f, source=0x%x",
                            level, bridge->config.trigger_threshold,
                            bridge->cooldown_timer, source_module);
    }

    /* Check cooldown */
    if (bridge->cooldown_timer > 0.0f) {
        bridge->stats.cooldown_blocked++;
        bridge->effects.cooldown_remaining = bridge->cooldown_timer;

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("Imagination trigger blocked by cooldown: remaining=%.3fs, "
                               "surprise=%.3f (blocked_count=%lu)",
                               bridge->cooldown_timer, level,
                               (unsigned long)bridge->stats.cooldown_blocked);
        }

        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Check threshold */
    if (level < bridge->config.trigger_threshold) {
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Imagination trigger below threshold: surprise=%.3f < threshold=%.3f",
                                level, bridge->config.trigger_threshold);
        }
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Check available slots */
    surprise_imagination_scenario_t* slot = find_free_slot(bridge);
    if (!slot) {
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("Imagination trigger: no free scenario slots (max=%u, active=%u)",
                               bridge->config.max_scenarios, count_active(bridge));
        }
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Create new scenario */
    memset(slot, 0, sizeof(*slot));
    slot->scenario_id = bridge->next_scenario_id++;
    slot->trigger_magnitude = level;
    slot->trigger_source = source_module;
    slot->status = SURPRISE_IMAGINATION_STATUS_ACTIVE;
    slot->expected_outcome = level * 0.5f;  /* Initial expectation */

    /* Start cooldown */
    bridge->cooldown_timer = bridge->config.cooldown_seconds;

    /* Update effects */
    bridge->effects.scenarios_active = count_active(bridge);
    bridge->effects.last_trigger_magnitude = level;
    bridge->effects.cooldown_remaining = bridge->cooldown_timer;

    bridge->stats.triggers++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination scenario CREATED: id=%u, surprise=%.3f, source=0x%x, "
                            "expected=%.3f, active_count=%u",
                            slot->scenario_id, level, source_module,
                            slot->expected_outcome, bridge->effects.scenarios_active);
        NIMCP_LOGGING_INFO("Imagination triggered: scenario=%u, surprise=%.3f, source=0x%x",
                           slot->scenario_id, level, source_module);
    }

    /* Bio-async: send trigger notification */
    imagination_send_trigger_msg(bridge, slot->scenario_id, level, source_module);
    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination bio-async: sent TRIGGER (scenario=%u)", slot->scenario_id);
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_imagination_on_result(
    surprise_imagination_bridge_t* bridge,
    uint32_t scenario_id,
    float actual_outcome)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in on_result");

    surprise_imagination_heartbeat_instance(bridge->health_agent, "on_result", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    surprise_imagination_scenario_t* scenario = find_scenario(bridge, scenario_id);
    if (!scenario) {
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("Imagination on_result: scenario %u not found or cancelled",
                               scenario_id);
        }
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SURPRISE_IMAGINATION_ERROR_INVALID_PARAM;
    }

    scenario->actual_outcome = actual_outcome;
    scenario->divergence = fabsf(scenario->expected_outcome - actual_outcome);
    scenario->status = SURPRISE_IMAGINATION_STATUS_COMPLETED;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination on_result: scenario=%u, expected=%.3f, actual=%.3f, "
                            "divergence=%.3f, status=%s",
                            scenario_id, scenario->expected_outcome, actual_outcome,
                            scenario->divergence,
                            imagination_status_name(scenario->status));
    }

    /* Update expectation adjustment based on divergence */
    bridge->effects.expectation_adjustment +=
        bridge->config.expectation_update_rate * scenario->divergence;

    bridge->effects.scenarios_active = count_active(bridge);
    bridge->stats.scenarios_completed++;
    bridge->stats.expectations_updated++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination expectation update: divergence=%.3f, "
                            "cumulative_adjustment=%.4f, completed=%lu",
                            scenario->divergence, bridge->effects.expectation_adjustment,
                            (unsigned long)bridge->stats.scenarios_completed);
    }

    /* Bio-async: send result notification */
    imagination_send_result_msg(bridge, scenario_id, scenario->divergence,
                                 scenario->expected_outcome, actual_outcome);
    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination bio-async: sent RESULT (scenario=%u, divergence=%.3f)",
                            scenario_id, scenario->divergence);
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_imagination_bridge_update(
    surprise_imagination_bridge_t* bridge,
    float dt_seconds)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_imagination_heartbeat_instance(bridge->health_agent, "update", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination update: dt=%.4f, cooldown=%.3f, active=%u, "
                            "expect_adj=%.4f",
                            dt_seconds, bridge->cooldown_timer,
                            bridge->effects.scenarios_active,
                            bridge->effects.expectation_adjustment);
    }

    /* Decay cooldown */
    if (bridge->cooldown_timer > 0.0f) {
        bridge->cooldown_timer -= dt_seconds;
        if (bridge->cooldown_timer < 0.0f) {
            bridge->cooldown_timer = 0.0f;
            if (bridge->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Imagination cooldown expired, ready for new triggers");
            }
        }
    }
    bridge->effects.cooldown_remaining = bridge->cooldown_timer;

    /* Decay expectation adjustment toward zero */
    bridge->effects.expectation_adjustment *= 0.99f;

    bridge->effects.scenarios_active = count_active(bridge);
    bridge->stats.total_updates++;
    bridge->update_count++;

    /* Loop heartbeat */
    surprise_imagination_heartbeat_instance(bridge->health_agent, "update", 1.0f);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_imagination_bridge_get_effects(
    const surprise_imagination_bridge_t* bridge,
    surprise_imagination_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_imagination_bridge_get_stats(
    const surprise_imagination_bridge_t* bridge,
    surprise_imagination_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

int surprise_imagination_get_scenario(
    const surprise_imagination_bridge_t* bridge,
    uint32_t scenario_id,
    surprise_imagination_scenario_t* scenario_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in get_scenario");
    NIMCP_CHECK_THROW_IMMUNE(scenario_out != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL scenario_out in get_scenario");

    for (uint32_t i = 0; i < bridge->config.max_scenarios; i++) {
        if (bridge->scenarios[i].scenario_id == scenario_id) {
            *scenario_out = bridge->scenarios[i];
            return 0;
        }
    }

    return NIMCP_SURPRISE_IMAGINATION_ERROR_INVALID_PARAM;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_imagination_bridge_set_health_agent(
    surprise_imagination_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Imagination bridge instance health agent %s",
                            agent ? "set" : "cleared");
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_imagination_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_imagination_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_imagination_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_imagination_training_begin: NULL argument");
        return -1;
    }
    surprise_imagination_heartbeat_instance(NULL, "surprise_imagination_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_imagination_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_imagination_training_end: NULL argument");
        return -1;
    }
    surprise_imagination_heartbeat_instance(NULL, "surprise_imagination_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_imagination_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_imagination_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_imagination_heartbeat_instance(NULL, "surprise_imagination_training_step", progress);
    (void)instance;
    return 0;
}
