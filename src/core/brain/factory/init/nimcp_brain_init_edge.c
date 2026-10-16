//=============================================================================
// nimcp_brain_init_edge.c - Edge/Robot Integration Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_edge.c
 * @brief Sensor hub, safety watchdog, and Portia/Dragonfly swarm bridge init
 *
 * WHAT: Initializes edge/robot integration subsystems during brain creation
 * WHY:  Enable embodied deployment on robots, drones, and edge devices
 * HOW:  Conditionally create sensor hub, safety watchdog, and swarm bridges
 *       based on brain configuration flags
 *
 * SUBSYSTEMS:
 * - Sensor Hub: Unified interface for heterogeneous sensors (LIDAR, IMU, etc.)
 * - Safety Watchdog: Heartbeat-based deadman switch + output validation
 * - Portia-Swarm Bridge: Resource-adaptive intelligence ↔ collective decisions
 * - Dragonfly-Swarm Bridge: Target tracking ↔ multi-drone pursuit coordination
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-21
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "edge/nimcp_sensor.h"
#include "edge/nimcp_safety_watchdog.h"
#include "edge/nimcp_ros2_bridge.h"
#include "edge/nimcp_mavlink_bridge.h"
#include "edge/nimcp_dji_bridge.h"
#include "edge/nimcp_msp_bridge.h"
#include "edge/nimcp_parrot_bridge.h"
#include "cognitive/language/nimcp_native_language.h"
#include "cognitive/language/nimcp_tokenizer.h"
#include "portia/nimcp_portia_swarm_bridge.h"
#include "swarm/nimcp_swarm_dragonfly_bridge.h"
#include "cognitive/memory/nimcp_episodic_replay.h"
#include "portia/nimcp_portia.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_edge, MESH_ADAPTER_CATEGORY_SYSTEM)

#define LOG_MODULE "BRAIN_INIT_EDGE"


/* Sensor IDs for drone telemetry auto-registration */
#define DRONE_SENSOR_ID_IMU       100
#define DRONE_SENSOR_ID_GPS       101
#define DRONE_SENSOR_ID_BATTERY   102
#define DRONE_SENSOR_ID_ATTITUDE  103

/**
 * @brief Auto-register standard drone sensors with the sensor hub.
 */
static void _register_drone_sensors(void* hub_ptr, const char* bridge_name) {
    if (!hub_ptr || !bridge_name) return;
    nimcp_sensor_hub_t* hub = (nimcp_sensor_hub_t*)hub_ptr;

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));

    desc.sensor_id = DRONE_SENSOR_ID_IMU;
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_FLOAT_ARRAY;
    snprintf(desc.name, sizeof(desc.name), "%s_imu", bridge_name);
    desc.sample_rate_hz = 50.0f;
    desc.max_data_count = 9;
    desc.noise_stddev = 0.01f;
    nimcp_sensor_register(hub, &desc);

    desc.sensor_id = DRONE_SENSOR_ID_GPS;
    desc.type = NIMCP_SENSOR_GPS;
    desc.format = NIMCP_SENSOR_FMT_FLOAT_ARRAY;
    snprintf(desc.name, sizeof(desc.name), "%s_gps", bridge_name);
    desc.sample_rate_hz = 10.0f;
    desc.max_data_count = 7;
    desc.noise_stddev = 1.0f;
    nimcp_sensor_register(hub, &desc);

    desc.sensor_id = DRONE_SENSOR_ID_BATTERY;
    desc.type = NIMCP_SENSOR_BAROMETER;
    desc.format = NIMCP_SENSOR_FMT_FLOAT_ARRAY;
    snprintf(desc.name, sizeof(desc.name), "%s_battery", bridge_name);
    desc.sample_rate_hz = 1.0f;
    desc.max_data_count = 3;
    desc.noise_stddev = 0.1f;
    nimcp_sensor_register(hub, &desc);

    desc.sensor_id = DRONE_SENSOR_ID_ATTITUDE;
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_VECTOR6;
    snprintf(desc.name, sizeof(desc.name), "%s_attitude", bridge_name);
    desc.sample_rate_hz = 50.0f;
    desc.max_data_count = 6;
    desc.noise_stddev = 0.005f;
    nimcp_sensor_register(hub, &desc);

    LOG_MODULE_INFO(LOG_MODULE, "Registered 4 drone sensors for bridge '%s'", bridge_name);
}

/**
 * @brief Initialize edge/robot integration subsystems
 *
 * WHAT: Creates sensor hub, safety watchdog, and swarm bridges as configured
 * WHY:  Enable embodied deployment on physical platforms
 * HOW:  Check brain config flags and conditionally create each subsystem
 *
 * @param brain Brain instance to initialize edge subsystems for
 * @return true on success (including when subsystems are disabled), false on critical failure
 *
 * PROCESS:
 * 1. Initialize all edge fields to safe defaults
 * 2. Create sensor hub if enable_sensor_hub is set
 * 3. Create safety watchdog if enable_safety_watchdog is set
 * 4. Create Portia-Swarm bridge if Portia is initialized and swarm is available
 * 5. Create Dragonfly-Swarm bridge if dragonfly is enabled
 */
bool nimcp_brain_factory_init_edge_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "nimcp_brain_factory_init_edge_subsystem: brain is NULL");
        return false;
    }

    /* Initialize all edge fields to safe defaults */
    brain->sensor_hub = NULL;
    brain->safety_watchdog = NULL;
    brain->ros2_bridge = NULL;
    brain->sensor_hub_enabled = false;
    brain->safety_watchdog_enabled = false;
    brain->ros2_bridge_enabled = false;
    brain->portia_swarm_bridge = NULL;
    brain->portia_swarm_bridge_enabled = false;
    brain->swarm_dragonfly_bridge = NULL;
    brain->swarm_dragonfly_bridge_enabled = false;
    brain->mavlink_bridge = NULL;
    brain->dji_bridge = NULL;
    brain->msp_bridge = NULL;
    brain->parrot_bridge = NULL;
    brain->sensorimotor = NULL;
    brain->native_language = NULL;
    brain->brain_tokenizer = NULL;
    brain->native_language_enabled = false;
    brain->brain_tokenizer_enabled = false;
    brain->inner_speech = NULL;
    brain->episodic_replay = NULL;
    brain->world_model_trainer = NULL;
    brain->output_attention = NULL;
    brain->wm_scratchpad = NULL;
    brain->analogical_transfer = NULL;
    brain->multiscale_memory = NULL;
    brain->emotional_learning = NULL;
    brain->contrastive_self = NULL;
    brain->self_curriculum = NULL;
    brain->dynamic_arch = NULL;

    /* === SENSOR HUB: Unified sensor interface === */
    if (brain->config.enable_sensor_hub) {
        brain->sensor_hub = nimcp_sensor_hub_create(32);  /* max 32 sensors */
        brain->sensor_hub_enabled = (brain->sensor_hub != NULL);
        if (brain->sensor_hub_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Sensor hub created (max 32 sensors)");
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "Sensor hub creation failed — continuing without sensor fusion");
        }
    }

    /* === SAFETY WATCHDOG: Actuator output validation === */
    if (brain->config.enable_safety_watchdog) {
        nimcp_watchdog_config_t wdog_cfg = nimcp_watchdog_config_default();
        brain->safety_watchdog = nimcp_watchdog_create(&wdog_cfg);
        brain->safety_watchdog_enabled = (brain->safety_watchdog != NULL);
        if (brain->safety_watchdog_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Safety watchdog created (timeout: %u ms)",
                            wdog_cfg.timeout_ms);
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "Safety watchdog creation failed — "
                            "continuing without actuator safety");
        }
    }

    /* === ROS 2 BRIDGE: Robot Operating System integration === */
    if (brain->config.enable_ros2_bridge) {
        nimcp_ros2_config_t ros2_cfg = nimcp_ros2_config_default();
        /* Pass NULL brain handle — bridge stores it but doesn't dereference at creation.
         * The brain handle is set later via the public API if needed. */
        brain->ros2_bridge = nimcp_ros2_bridge_create(NULL, &ros2_cfg);
        brain->ros2_bridge_enabled = (brain->ros2_bridge != NULL);
        if (brain->ros2_bridge_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "ROS 2 bridge created (stub mode)");
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "ROS 2 bridge creation failed — "
                            "continuing without ROS 2 integration");
        }
    }

    /* === PORTIA-SWARM BRIDGE: Resource-adaptive collective intelligence === */
    /* Only create if Portia is globally initialized — Portia is a singleton */
    if (portia_is_initialized()) {
        portia_swarm_config_t psb_config = {0};
        portia_swarm_default_config(&psb_config);

        portia_context_t* portia_ctx = portia_get_context();
        if (portia_ctx) {
            brain->portia_swarm_bridge = portia_swarm_bridge_create(&psb_config, portia_ctx);
            brain->portia_swarm_bridge_enabled = (brain->portia_swarm_bridge != NULL);
            if (brain->portia_swarm_bridge_enabled) {
                LOG_MODULE_INFO(LOG_MODULE, "Portia-Swarm bridge created");
            }
        }
    }

    /* === DRAGONFLY-SWARM BRIDGE: Coordinated multi-drone pursuit === */
    /* Only create if dragonfly is enabled and available */
    if (brain->dragonfly_enabled && brain->dragonfly) {
        swarm_dragonfly_bridge_config_t sdb_config = swarm_dragonfly_bridge_default_config();
        /* Collective workspace and task scheduler are optional (NULL = standalone mode) */
        brain->swarm_dragonfly_bridge = swarm_dragonfly_bridge_create(
            brain->dragonfly, NULL /* workspace */, NULL /* scheduler */, &sdb_config);
        brain->swarm_dragonfly_bridge_enabled = (brain->swarm_dragonfly_bridge != NULL);
        if (brain->swarm_dragonfly_bridge_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Dragonfly-Swarm bridge created");
        }
    }

    /* === FLIGHT CONTROLLER BRIDGES: Drone platform integration === */
    /* Each bridge auto-registers its sensors with the sensor hub so telemetry
     * flows automatically: FC → bridge → sensor hub → brain input */

    if (brain->config.enable_mavlink_bridge) {
        nimcp_mavlink_config_t mav_cfg = nimcp_mavlink_config_default();
        brain->mavlink_bridge = nimcp_mavlink_bridge_create(&mav_cfg);
        if (brain->mavlink_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "MAVLink bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "mavlink");
        }
    }

    if (brain->config.enable_dji_bridge) {
        nimcp_dji_config_t dji_cfg = nimcp_dji_config_default();
        brain->dji_bridge = nimcp_dji_bridge_create(&dji_cfg);
        if (brain->dji_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "DJI bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "dji");
        }
    }

    if (brain->config.enable_msp_bridge) {
        nimcp_msp_config_t msp_cfg = nimcp_msp_config_default();
        brain->msp_bridge = nimcp_msp_bridge_create(&msp_cfg);
        if (brain->msp_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "MSP bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "msp");
        }
    }

    if (brain->config.enable_parrot_bridge) {
        nimcp_parrot_config_t parrot_cfg = nimcp_parrot_config_default();
        brain->parrot_bridge = nimcp_parrot_bridge_create(&parrot_cfg);
        if (brain->parrot_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "Parrot bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "parrot");
        }
    }

    /* === NATIVE LANGUAGE: Brain-native text generation === */
    if (brain->config.enable_native_language) {
        nimcp_language_config_t lang_cfg = nimcp_language_config_default();
        brain->native_language = nimcp_native_language_create(&lang_cfg);
        brain->native_language_enabled = (brain->native_language != NULL);
        if (brain->native_language_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Native language system created (vocab seed=%u)",
                            nimcp_language_get_vocab_size(brain->native_language));
        }
    }

    /* === TOKENIZER: Brain-native vocabulary === */
    if (brain->config.enable_brain_tokenizer) {
        nimcp_tokenizer_config_t tok_cfg = nimcp_tokenizer_config_default();
        brain->brain_tokenizer = nimcp_tokenizer_create(&tok_cfg);
        brain->brain_tokenizer_enabled = (brain->brain_tokenizer != NULL);
        if (brain->brain_tokenizer_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Tokenizer created (initial vocab=%u)",
                            nimcp_tokenizer_get_vocab_size(brain->brain_tokenizer));
        }
    }

    /* === COGNITIVE ENHANCEMENTS (always created, lightweight) === */
    {
        /* Episodic replay's public API rejects NULL config (logs
         * "[EPISODIC_REPLAY] NULL config" and returns NULL), so
         * construct the default config and pass it explicitly.
         * The other modules below accept NULL and self-default. */
        extern void* nimcp_emotional_learning_create(const void*);
        extern void* nimcp_contrastive_self_create(const void*);
        extern void* nimcp_self_curriculum_create(const void*);
        extern void* nimcp_dynamic_arch_create(const void*);
        extern void* nimcp_wmt_create(const void*);
        extern void* nimcp_oa_create(const void*);
        extern void* nimcp_wms_create(const void*);
        extern void* nimcp_analogical_create(const void*);
        extern void* nimcp_multiscale_create(const void*);

        nimcp_episodic_replay_config_t _er_cfg = nimcp_episodic_replay_config_default();
        brain->episodic_replay = nimcp_episodic_replay_create(&_er_cfg);
        brain->emotional_learning = nimcp_emotional_learning_create(NULL);
        brain->contrastive_self = nimcp_contrastive_self_create(NULL);
        brain->self_curriculum = nimcp_self_curriculum_create(NULL);
        brain->dynamic_arch = nimcp_dynamic_arch_create(NULL);
        /* Register the two regions whose activations are recorded:
         * - "adaptive" (fed from learning loss in nimcp_brain_learning.c:3404)
         * - "inference" (fed from decision confidence in nimcp_brain_part_core.c:4658)
         * Without this, record_activation silently fails and analyze() sees
         * an empty stat table -- the whole module was a statue. */
        if (brain->dynamic_arch) {
            extern int nimcp_dynamic_arch_register_region(void*, const char*, uint32_t, uint32_t);
            /* Region neuron-count is only used to compute the advisory delta in
             * nimcp_dynamic_arch_analyze (count * 0.2 for grow, * 0.1 for shrink).
             * We consume `action` only (LR modulation, NOT resizing), so a
             * nominal value is sufficient. */
            const uint32_t nominal_region_size = 1024u;
            nimcp_dynamic_arch_register_region(brain->dynamic_arch, "adaptive",  0, nominal_region_size);
            nimcp_dynamic_arch_register_region(brain->dynamic_arch, "inference", 0, nominal_region_size);
        }
        brain->world_model_trainer = nimcp_wmt_create(NULL);
        brain->output_attention = nimcp_oa_create(NULL);
        brain->wm_scratchpad = nimcp_wms_create(NULL);
        brain->analogical_transfer = nimcp_analogical_create(NULL);
        brain->multiscale_memory = nimcp_multiscale_create(NULL);

        /* Inner speech requires native language — create only if language enabled */
        if (brain->native_language_enabled && brain->native_language) {
            extern void* nimcp_inner_speech_create(const void*);
            brain->inner_speech = nimcp_inner_speech_create(NULL);
        }

        int created = (brain->episodic_replay ? 1 : 0) + (brain->emotional_learning ? 1 : 0) +
                      (brain->contrastive_self ? 1 : 0) + (brain->self_curriculum ? 1 : 0) +
                      (brain->dynamic_arch ? 1 : 0) + (brain->world_model_trainer ? 1 : 0) +
                      (brain->output_attention ? 1 : 0) + (brain->wm_scratchpad ? 1 : 0) +
                      (brain->analogical_transfer ? 1 : 0) + (brain->multiscale_memory ? 1 : 0) +
                      (brain->inner_speech ? 1 : 0);
        LOG_MODULE_INFO(LOG_MODULE, "Cognitive enhancements: %d/11 created", created);
    }

    return true;  /* Non-fatal — individual subsystem failures don't prevent brain creation */
}
