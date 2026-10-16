/**
 * @file nimcp_sleep_imagination_bridge.c
 * @brief Sleep-Imagination Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/imagination/nimcp_sleep_imagination_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sleep_imagination_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_sleep_imagination_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_sleep_imagination_bridge_mesh_registry = NULL;

nimcp_error_t sleep_imagination_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_sleep_imagination_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "sleep_imagination_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "sleep_imagination_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_sleep_imagination_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_sleep_imagination_bridge_mesh_registry = registry;
    return err;
}

void sleep_imagination_bridge_mesh_unregister(void) {
    if (g_sleep_imagination_bridge_mesh_registry && g_sleep_imagination_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_sleep_imagination_bridge_mesh_registry, g_sleep_imagination_bridge_mesh_id);
        g_sleep_imagination_bridge_mesh_id = 0;
        g_sleep_imagination_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from sleep_imagination_bridge module (instance-level) */
static inline void sleep_imagination_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_sleep_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_sleep_imagination_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_sleep_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SLEEP_IMAGINATION_BRIDGE"


/* Logging macros - map to nimcp logging functions */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* Time function alias */
#define nimcp_time_now_ms() nimcp_time_get_ms()

/* ============================================================================
 * STAGE DESCRIPTORS
 * ============================================================================ */

static const sleep_stage_descriptor_t g_stage_descriptors[SLEEP_STAGE_COUNT] = {
    /* WAKE */
    {
        .name = "Wake",
        .vividness_factor = 1.0f,
        .control_factor = 1.0f,
        .creativity_factor = 1.0f,
        .imagery_active = true,
        .consolidation_active = false
    },
    /* NREM1 */
    {
        .name = "NREM1",
        .vividness_factor = 0.4f,
        .control_factor = 0.3f,
        .creativity_factor = 1.2f,
        .imagery_active = true,
        .consolidation_active = false
    },
    /* NREM2 */
    {
        .name = "NREM2",
        .vividness_factor = 0.1f,
        .control_factor = 0.0f,
        .creativity_factor = 0.5f,
        .imagery_active = false,
        .consolidation_active = true
    },
    /* NREM3 */
    {
        .name = "NREM3",
        .vividness_factor = 0.0f,
        .control_factor = 0.0f,
        .creativity_factor = 0.0f,
        .imagery_active = false,
        .consolidation_active = true
    },
    /* REM */
    {
        .name = "REM",
        .vividness_factor = 0.9f,
        .control_factor = 0.1f,
        .creativity_factor = 1.8f,
        .imagery_active = true,
        .consolidation_active = false
    }
};

const sleep_stage_descriptor_t* sleep_imagination_get_stage_descriptor(sleep_stage_t stage) {
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    if (stage >= SLEEP_STAGE_COUNT) {
        stage = SLEEP_STAGE_WAKE;
    }
    return &g_stage_descriptors[stage];
}

const char* sleep_imagination_stage_name(sleep_stage_t stage) {
    return sleep_imagination_get_stage_descriptor(stage)->name;
}

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

int sleep_imagination_default_config(sleep_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_de", 0.0f);


    config->rem_vividness = SLEEP_IMAG_DEFAULT_REM_VIVIDNESS;
    config->rem_creativity_boost = SLEEP_IMAG_DEFAULT_REM_CREATIVITY;
    config->nrem1_vividness = SLEEP_IMAG_DEFAULT_NREM1_VIVIDNESS;

    config->enable_spontaneous_dreams = true;
    config->enable_lucid_mode = false;
    config->lucid_control_threshold = 0.7f;

    config->enable_consolidation_signals = true;
    config->consolidation_threshold = 0.5f;

    config->enable_nightmare_detection = true;
    config->nightmare_intensity_threshold = 0.8f;
    config->enable_nightmare_interruption = true;

    config->update_interval_ms = 16.0f;  /* ~60 Hz */
    config->enable_bio_async = true;

    return 0;
}

int sleep_imagination_validate_config(const sleep_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_va", 0.0f);


    if (config->rem_vividness < 0.0f || config->rem_vividness > 1.0f) {
        return -1;
    }
    if (config->rem_creativity_boost < 0.0f || config->rem_creativity_boost > 5.0f) {
        return -1;
    }
    if (config->nrem1_vividness < 0.0f || config->nrem1_vividness > 1.0f) {
        return -1;
    }
    if (config->lucid_control_threshold < 0.0f || config->lucid_control_threshold > 1.0f) {
        return -1;
    }
    if (config->consolidation_threshold < 0.0f || config->consolidation_threshold > 1.0f) {
        return -1;
    }
    if (config->nightmare_intensity_threshold < 0.0f ||
        config->nightmare_intensity_threshold > 1.0f) {
        return -1;
    }
    if (config->update_interval_ms < 1.0f) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

sleep_imagination_bridge_t* sleep_imagination_bridge_create(
    const sleep_imagination_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_create", 0.0f);


    sleep_imagination_bridge_t* bridge = nimcp_calloc(
        1, sizeof(sleep_imagination_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR("Failed to allocate sleep-imagination bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_IMAGINATION_SLEEP;
    bridge->base.module_name = "sleep_imagination_bridge";
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "sleep_imagination") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_imagination_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (sleep_imagination_validate_config(config) != 0) {
            NIMCP_LOG_ERROR("Invalid bridge configuration");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_imagination_bridge_create: validation failed");
            return NULL;
        }
        bridge->config = *config;
    } else {
        sleep_imagination_default_config(&bridge->config);
    }

    /* Initialize effects */
    memset(&bridge->sleep_to_imag, 0, sizeof(bridge->sleep_to_imag));
    memset(&bridge->imag_to_sleep, 0, sizeof(bridge->imag_to_sleep));

    /* Initialize state */
    bridge->current_stage = SLEEP_STAGE_WAKE;
    bridge->previous_stage = SLEEP_STAGE_WAKE;
    bridge->stage_entry_time_ms = nimcp_time_now_ms();
    bridge->num_dream_scenarios = 0;
    bridge->in_rem_period = false;
    bridge->rem_start_time_ms = 0;
    bridge->accumulated_rem_time_ms = 0.0f;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize timing */
    bridge->last_update_time_ms = nimcp_time_now_ms();

    NIMCP_LOG_INFO("Created sleep-imagination bridge");
    return bridge;
}

void sleep_imagination_bridge_destroy(sleep_imagination_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sleep_imagination");

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        sleep_imagination_disconnect_bio_async(bridge);
    }

    /* Free dream embedding */
    if (bridge->imag_to_sleep.dream_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_sleep.dream_embedding);
    }

    /* Free replay cue */
    if (bridge->imag_to_sleep.replay_cue) {
        nimcp_tensor_destroy(bridge->imag_to_sleep.replay_cue);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOG_INFO("Destroyed sleep-imagination bridge");
}

int sleep_imagination_reset(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free tensor pointers in effects BEFORE memset zeroes them */
    if (bridge->imag_to_sleep.dream_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_sleep.dream_embedding);
    }
    if (bridge->imag_to_sleep.replay_cue) {
        nimcp_tensor_destroy(bridge->imag_to_sleep.replay_cue);
    }

    /* Clear effects (now safe - tensor pointers already freed) */
    memset(&bridge->sleep_to_imag, 0, sizeof(bridge->sleep_to_imag));
    memset(&bridge->imag_to_sleep, 0, sizeof(bridge->imag_to_sleep));

    /* Reset state to wake */
    bridge->current_stage = SLEEP_STAGE_WAKE;
    bridge->previous_stage = SLEEP_STAGE_WAKE;
    bridge->stage_entry_time_ms = nimcp_time_now_ms();
    bridge->num_dream_scenarios = 0;
    bridge->in_rem_period = false;
    bridge->accumulated_rem_time_ms = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

int sleep_imagination_connect_sleep(
    sleep_imagination_bridge_t* bridge,
    struct sleep_system* sleep)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->sleep = sleep;
    bridge->base.system_a = sleep;
    bridge->base.system_a_connected = (sleep != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Sleep system %s to bridge",
                   sleep ? "connected" : "disconnected");
    return 0;
}

int sleep_imagination_connect_imagination(
    sleep_imagination_bridge_t* bridge,
    struct imagination_engine* imagination)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->imagination = imagination;
    bridge->base.system_b = imagination;
    bridge->base.system_b_connected = (imagination != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Imagination engine %s to bridge",
                   imagination ? "connected" : "disconnected");
    return 0;
}

int sleep_imagination_disconnect_sleep(sleep_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_di", 0.0f);


    return sleep_imagination_connect_sleep(bridge, NULL);
}

int sleep_imagination_disconnect_imagination(sleep_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_di", 0.0f);


    return sleep_imagination_connect_imagination(bridge, NULL);
}

bool sleep_imagination_is_connected(const sleep_imagination_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_is", 0.0f);


    return bridge->base.bridge_active;
}

/* ============================================================================
 * UPDATE
 * ============================================================================ */

int sleep_imagination_update(
    sleep_imagination_bridge_t* bridge,
    float delta_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bridge_active) return 0;  /* Nothing to do */

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check update interval */
    uint64_t now = nimcp_time_now_ms();
    float elapsed = (float)(now - bridge->last_update_time_ms);
    if (elapsed < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->last_update_time_ms = now;

    /* Update REM time tracking */
    if (bridge->in_rem_period) {
        bridge->accumulated_rem_time_ms += elapsed;
        bridge->stats.total_rem_time_ms += elapsed;
    }

    /* Compute effects in both directions */
    sleep_imagination_compute_sleep_effects(bridge);
    sleep_imagination_compute_imag_effects(bridge);

    /* Apply effects */
    sleep_imagination_apply_effects(bridge);

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_imagination_compute_sleep_effects(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_co", 0.0f);


    const sleep_stage_descriptor_t* desc =
        sleep_imagination_get_stage_descriptor(bridge->current_stage);

    /* Set stage info */
    bridge->sleep_to_imag.current_stage = bridge->current_stage;
    bridge->sleep_to_imag.stage_stability = 0.8f;  /* Could be computed from sleep system */

    /* Compute stage depth based on time in stage */
    uint64_t now = nimcp_time_now_ms();
    float time_in_stage = (float)(now - bridge->stage_entry_time_ms);
    bridge->sleep_to_imag.stage_depth = fminf(1.0f, time_in_stage / 60000.0f);  /* Max at 1 min */

    /* Apply stage-based modulation */
    bridge->sleep_to_imag.vividness_modulation = desc->vividness_factor;
    bridge->sleep_to_imag.control_modulation = desc->control_factor;
    bridge->sleep_to_imag.creativity_boost = desc->creativity_factor;

    /* Override with config for specific stages */
    if (bridge->current_stage == SLEEP_STAGE_REM) {
        bridge->sleep_to_imag.vividness_modulation = bridge->config.rem_vividness;
        bridge->sleep_to_imag.creativity_boost = bridge->config.rem_creativity_boost;
        bridge->sleep_to_imag.enable_creative_mode = true;
    } else if (bridge->current_stage == SLEEP_STAGE_NREM1) {
        bridge->sleep_to_imag.vividness_modulation = bridge->config.nrem1_vividness;
        bridge->sleep_to_imag.enable_creative_mode = false;
    } else {
        bridge->sleep_to_imag.enable_creative_mode = false;
    }

    /* Lucid mode increases control */
    if (bridge->config.enable_lucid_mode &&
        bridge->current_stage == SLEEP_STAGE_REM) {
        bridge->sleep_to_imag.control_modulation =
            fmaxf(bridge->sleep_to_imag.control_modulation,
                  bridge->config.lucid_control_threshold);
    }

    /* Content weights vary by stage */
    if (bridge->current_stage == SLEEP_STAGE_REM) {
        bridge->sleep_to_imag.emotional_salience_weight = 0.7f;
        bridge->sleep_to_imag.recency_weight = 0.5f;
        bridge->sleep_to_imag.novelty_preference = 0.6f;
    } else {
        bridge->sleep_to_imag.emotional_salience_weight = 0.3f;
        bridge->sleep_to_imag.recency_weight = 0.8f;
        bridge->sleep_to_imag.novelty_preference = 0.2f;
    }

    /* Consolidation signals during NREM */
    bridge->sleep_to_imag.trigger_consolidation = desc->consolidation_active;
    bridge->sleep_to_imag.consolidation_strength =
        desc->consolidation_active ? 0.8f : 0.0f;

    return 0;
}

int sleep_imagination_compute_imag_effects(sleep_imagination_bridge_t* bridge) {
    if (!bridge || !bridge->imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_imagination_compute_imag_effects: required parameter is NULL (bridge, bridge->imagination)");
        return -1;
    }

    /* Default state */
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_co", 0.0f);


    bridge->imag_to_sleep.dream_activity_level = 0.0f;
    bridge->imag_to_sleep.emotional_intensity = 0.0f;
    bridge->imag_to_sleep.nightmare_detected = false;
    bridge->imag_to_sleep.request_replay = false;
    bridge->imag_to_sleep.suggest_stage_change = false;
    bridge->imag_to_sleep.encode_dream_memory = false;

    /* During REM with active dreams */
    if (bridge->in_rem_period && bridge->num_dream_scenarios > 0) {
        bridge->imag_to_sleep.dream_activity_level = 0.7f;  /* Would query imagination */

        /* Check for nightmares */
        if (bridge->config.enable_nightmare_detection) {
            /* In full implementation, would check emotional intensity from imagination */
            float emotional_intensity = 0.3f;  /* Placeholder */
            bridge->imag_to_sleep.emotional_intensity = emotional_intensity;

            if (emotional_intensity > bridge->config.nightmare_intensity_threshold) {
                bridge->imag_to_sleep.nightmare_detected = true;
                bridge->stats.nightmares_detected++;
            }
        }
    }

    return 0;
}

int sleep_imagination_apply_effects(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply sleep effects to imagination */
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ap", 0.0f);


    if (bridge->imagination) {
        /* In full implementation, would call imagination APIs to set:
         * - Vividness level
         * - Control level
         * - Creative mode
         */
    }

    /* Apply imagination effects to sleep */
    if (bridge->sleep) {
        /* Handle nightmare interruption */
        if (bridge->imag_to_sleep.nightmare_detected &&
            bridge->config.enable_nightmare_interruption) {
            /* Would transition to lighter sleep or wake briefly */
            bridge->stats.nightmares_interrupted++;
        }
    }

    return 0;
}

/* ============================================================================
 * SLEEP STAGE
 * ============================================================================ */

int sleep_imagination_set_sleep_stage(
    sleep_imagination_bridge_t* bridge,
    sleep_stage_t stage)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (stage >= SLEEP_STAGE_COUNT) return -1;

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->current_stage != stage) {
        bridge->previous_stage = bridge->current_stage;
        bridge->current_stage = stage;
        bridge->stage_entry_time_ms = nimcp_time_now_ms();
        bridge->stats.stage_transitions++;

        /* Track REM periods */
        if (stage == SLEEP_STAGE_REM && !bridge->in_rem_period) {
            bridge->in_rem_period = true;
            bridge->rem_start_time_ms = bridge->stage_entry_time_ms;
            bridge->stats.rem_periods++;

            /* Start spontaneous dream if enabled */
            if (bridge->config.enable_spontaneous_dreams && bridge->imagination) {
                /* In full implementation, would start a dream scenario */
                if (bridge->num_dream_scenarios < SLEEP_IMAG_MAX_DREAM_SCENARIOS) {
                    bridge->dream_scenarios[bridge->num_dream_scenarios++] = 1;
                    bridge->stats.dreams_generated++;
                }
            }
        } else if (stage != SLEEP_STAGE_REM && bridge->in_rem_period) {
            bridge->in_rem_period = false;
        }

        NIMCP_LOG_DEBUG("Sleep stage transition: %s -> %s",
                       sleep_imagination_stage_name(bridge->previous_stage),
                       sleep_imagination_stage_name(bridge->current_stage));
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

sleep_stage_t sleep_imagination_get_sleep_stage(
    const sleep_imagination_bridge_t* bridge)
{
    if (!bridge) return SLEEP_STAGE_WAKE;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    return bridge->current_stage;
}

uint64_t sleep_imagination_get_stage_duration(
    const sleep_imagination_bridge_t* bridge)
{
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    return nimcp_time_now_ms() - bridge->stage_entry_time_ms;
}

/* ============================================================================
 * DREAM GENERATION
 * ============================================================================ */

uint32_t sleep_imagination_start_dream(
    sleep_imagination_bridge_t* bridge,
    const nimcp_tensor_t* emotional_seed,
    struct imagination_goal* goal)
{
    if (!bridge) return 0;
    if (!bridge->base.bridge_active) return 0;

    /* Dreams require REM or NREM1 */
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_st", 0.0f);


    if (bridge->current_stage != SLEEP_STAGE_REM &&
        bridge->current_stage != SLEEP_STAGE_NREM1) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_dream_scenarios >= SLEEP_IMAG_MAX_DREAM_SCENARIOS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* In full implementation, would:
     * 1. Create imagination scenario with emotional seed
     * 2. Set dream-specific parameters
     * 3. Start scenario execution
     */

    uint32_t scenario_id = bridge->num_dream_scenarios + 1;  /* Placeholder */
    bridge->dream_scenarios[bridge->num_dream_scenarios++] = scenario_id;
    bridge->stats.dreams_generated++;

    bridge->imag_to_sleep.active_scenario_id = scenario_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Started dream scenario %u", scenario_id);
    return scenario_id;
}

int sleep_imagination_end_dream(
    sleep_imagination_bridge_t* bridge,
    bool encode_as_memory)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->num_dream_scenarios == 0) return -1;

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* End the most recent dream */
    bridge->num_dream_scenarios--;
    bridge->imag_to_sleep.active_scenario_id = 0;

    if (encode_as_memory) {
        bridge->imag_to_sleep.encode_dream_memory = true;
        bridge->stats.dreams_encoded++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Ended dream scenario, encode=%d", encode_as_memory);
    return 0;
}

int sleep_imagination_trigger_replay(
    sleep_imagination_bridge_t* bridge,
    const nimcp_tensor_t* memory_cue)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bridge_active) return 0;

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_tr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->sleep_to_imag.replay_active = true;
    bridge->sleep_to_imag.replay_content_influence = 0.6f;
    bridge->imag_to_sleep.request_replay = true;

    /* Store replay cue if provided */
    if (memory_cue) {
        if (bridge->imag_to_sleep.replay_cue) {
            nimcp_tensor_destroy(bridge->imag_to_sleep.replay_cue);
        }
        bridge->imag_to_sleep.replay_cue = nimcp_tensor_clone(memory_cue);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Triggered memory replay for dream integration");
    return 0;
}

int sleep_imagination_interrupt_nightmare(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_nightmare_interruption) return -1;

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_in", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->imag_to_sleep.nightmare_detected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_imagination_interrupt_nightmare: bridge->imag_to_sleep is NULL");
        return -1;
    }

    /* End all current dreams */
    bridge->num_dream_scenarios = 0;
    bridge->imag_to_sleep.nightmare_detected = false;
    bridge->imag_to_sleep.active_scenario_id = 0;
    bridge->stats.nightmares_interrupted++;

    /* Suggest transition to lighter sleep */
    bridge->imag_to_sleep.suggest_stage_change = true;
    bridge->imag_to_sleep.suggested_stage = SLEEP_STAGE_NREM1;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Interrupted nightmare, suggesting stage transition");
    return 0;
}

/* ============================================================================
 * EFFECTS ACCESS
 * ============================================================================ */

int sleep_imagination_get_sleep_effects(
    const sleep_imagination_bridge_t* bridge,
    sleep_to_imagination_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_imagination_get_sleep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->sleep_to_imag;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    return 0;
}

int sleep_imagination_get_imag_effects(
    const sleep_imagination_bridge_t* bridge,
    imagination_to_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_imagination_get_imag_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->imag_to_sleep;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    return 0;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int sleep_imagination_get_stats(
    const sleep_imagination_bridge_t* bridge,
    sleep_imagination_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_imagination_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);

    nimcp_mutex_lock(((sleep_imagination_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((sleep_imagination_bridge_t*)bridge)->base.mutex);
    return 0;
}

int sleep_imagination_reset_stats(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t sleep_imagination_get_dream_count(const sleep_imagination_bridge_t* bridge) {
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    return bridge->num_dream_scenarios;
}

bool sleep_imagination_is_rem(const sleep_imagination_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_is", 0.0f);


    return bridge->in_rem_period;
}

float sleep_imagination_get_rem_time(const sleep_imagination_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_ge", 0.0f);


    return bridge->accumulated_rem_time_ms;
}

/* ============================================================================
 * BIO-ASYNC
 * ============================================================================ */

int sleep_imagination_connect_bio_async(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_co", 0.0f);


    int result = bridge_base_connect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOG_INFO("Sleep-imagination bridge connected to bio-async");
    }

    return result;
}

int sleep_imagination_disconnect_bio_async(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Already disconnected */

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_di", 0.0f);


    int result = bridge_base_disconnect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOG_INFO("Sleep-imagination bridge disconnected from bio-async");
    }

    return result;
}

bool sleep_imagination_is_bio_async_connected(
    const sleep_imagination_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_is", 0.0f);


    return bridge->base.bio_async_enabled;
}

int sleep_imagination_process_messages(sleep_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process pending bio-async messages */
    /* In full implementation, would handle:
     * - BIO_MSG_SLEEP_STAGE_CHANGE
     * - BIO_MSG_DREAM_START_REQUEST
     * - BIO_MSG_DREAM_END_REQUEST
     * - BIO_MSG_NIGHTMARE_ALERT
     * - BIO_MSG_REPLAY_REQUEST
     */

    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_sleep_imagination_pr", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Sleep Imagination Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int sleep_imagination_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    sleep_imagination_bridge_heartbeat("sleep_imagin_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Sleep_Imagination_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                sleep_imagination_bridge_heartbeat("sleep_imagin_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOG_DEBUG("Sleep Imagination Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Sleep_Imagination_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Sleep_Imagination_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_imagination_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_sleep_imagination_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_imagination_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_imagination_bridge_training_begin: NULL argument");
        return -1;
    }
    sleep_imagination_bridge_heartbeat("sleep_imagination_bridge_training_begin", 0.0f);
    return 0;
}

int sleep_imagination_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_imagination_bridge_training_end: NULL argument");
        return -1;
    }
    sleep_imagination_bridge_heartbeat("sleep_imagination_bridge_training_end", 1.0f);
    return 0;
}

int sleep_imagination_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_imagination_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sleep_imagination_bridge_heartbeat("sleep_imagination_bridge_training_step", progress);
    return 0;
}
