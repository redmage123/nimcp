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
#include "superhuman/nimcp_eagle_vision.h"
#include "superhuman/nimcp_echolocation.h"
#include "superhuman/nimcp_time_dilation.h"
#include "superhuman/nimcp_savant_mode.h"
#include "superhuman/nimcp_synesthesia.h"
#include "embodiment/nimcp_interoceptive_prediction.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "core/brain/nimcp_brain_hearing.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
 * 1. Create each subsystem only if its field is still NULL (idempotent)
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

    /* Create-if-NULL throughout (2026-11-23): this init now also runs from
     * nimcp_brain_eager_init_cognitive on resumed brains (brain_load never
     * ran it, so the 11 cognitive enhancements were NULL in production).
     * allocate_brain() callocs, so fresh brains still start from NULL; the
     * old unconditional NULL-reset here would leak anything already built. */

    /* === SENSOR HUB: Unified sensor interface === */
    if (brain->config.enable_sensor_hub && !brain->sensor_hub) {
        brain->sensor_hub = nimcp_sensor_hub_create(32);  /* max 32 sensors */
        brain->sensor_hub_enabled = (brain->sensor_hub != NULL);
        if (brain->sensor_hub_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Sensor hub created (max 32 sensors)");
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "Sensor hub creation failed — continuing without sensor fusion");
        }
    }

    /* === SAFETY WATCHDOG: Actuator output validation === */
    if (brain->config.enable_safety_watchdog && !brain->safety_watchdog) {
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
    if (brain->config.enable_ros2_bridge && !brain->ros2_bridge) {
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
    if (portia_is_initialized() && !brain->portia_swarm_bridge) {
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
    if (brain->dragonfly_enabled && brain->dragonfly && !brain->swarm_dragonfly_bridge) {
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

    if (brain->config.enable_mavlink_bridge && !brain->mavlink_bridge) {
        nimcp_mavlink_config_t mav_cfg = nimcp_mavlink_config_default();
        brain->mavlink_bridge = nimcp_mavlink_bridge_create(&mav_cfg);
        if (brain->mavlink_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "MAVLink bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "mavlink");
        }
    }

    if (brain->config.enable_dji_bridge && !brain->dji_bridge) {
        nimcp_dji_config_t dji_cfg = nimcp_dji_config_default();
        brain->dji_bridge = nimcp_dji_bridge_create(&dji_cfg);
        if (brain->dji_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "DJI bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "dji");
        }
    }

    if (brain->config.enable_msp_bridge && !brain->msp_bridge) {
        nimcp_msp_config_t msp_cfg = nimcp_msp_config_default();
        brain->msp_bridge = nimcp_msp_bridge_create(&msp_cfg);
        if (brain->msp_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "MSP bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "msp");
        }
    }

    if (brain->config.enable_parrot_bridge && !brain->parrot_bridge) {
        nimcp_parrot_config_t parrot_cfg = nimcp_parrot_config_default();
        brain->parrot_bridge = nimcp_parrot_bridge_create(&parrot_cfg);
        if (brain->parrot_bridge) {
            LOG_MODULE_INFO(LOG_MODULE, "Parrot bridge created");
            if (brain->sensor_hub_enabled)
                _register_drone_sensors(brain->sensor_hub, "parrot");
        }
    }

    /* === NATIVE LANGUAGE: Brain-native text generation === */
    if (brain->config.enable_native_language && !brain->native_language) {
        nimcp_language_config_t lang_cfg = nimcp_language_config_default();
        brain->native_language = nimcp_native_language_create(&lang_cfg);
        brain->native_language_enabled = (brain->native_language != NULL);
        if (brain->native_language_enabled) {
            LOG_MODULE_INFO(LOG_MODULE, "Native language system created (vocab seed=%u)",
                            nimcp_language_get_vocab_size(brain->native_language));
        }
    }

    /* === TOKENIZER: Brain-native vocabulary === */
    if (brain->config.enable_brain_tokenizer && !brain->brain_tokenizer) {
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

        if (!brain->episodic_replay) {
            nimcp_episodic_replay_config_t _er_cfg = nimcp_episodic_replay_config_default();
            brain->episodic_replay = nimcp_episodic_replay_create(&_er_cfg);
        }
        if (!brain->emotional_learning) brain->emotional_learning = nimcp_emotional_learning_create(NULL);
        if (!brain->contrastive_self) brain->contrastive_self = nimcp_contrastive_self_create(NULL);
        if (!brain->self_curriculum) brain->self_curriculum = nimcp_self_curriculum_create(NULL);
        const bool _dyn_new = (brain->dynamic_arch == NULL);
        if (_dyn_new) brain->dynamic_arch = nimcp_dynamic_arch_create(NULL);
        /* Register the two regions whose activations are recorded:
         * - "adaptive" (fed from learning loss in nimcp_brain_learning.c:3404)
         * - "inference" (fed from decision confidence in nimcp_brain_part_core.c:4658)
         * Without this, record_activation silently fails and analyze() sees
         * an empty stat table -- the whole module was a statue. */
        if (brain->dynamic_arch && _dyn_new) {  /* register once, not per re-run */
            extern int nimcp_dynamic_arch_register_region(void*, const char*, uint32_t, uint32_t);
            /* Region neuron-count is only used to compute the advisory delta in
             * nimcp_dynamic_arch_analyze (count * 0.2 for grow, * 0.1 for shrink).
             * We consume `action` only (LR modulation, NOT resizing), so a
             * nominal value is sufficient. */
            const uint32_t nominal_region_size = 1024u;
            nimcp_dynamic_arch_register_region(brain->dynamic_arch, "adaptive",  0, nominal_region_size);
            nimcp_dynamic_arch_register_region(brain->dynamic_arch, "inference", 0, nominal_region_size);
        }
        if (!brain->world_model_trainer) brain->world_model_trainer = nimcp_wmt_create(NULL);
        if (!brain->output_attention) brain->output_attention = nimcp_oa_create(NULL);
        if (!brain->wm_scratchpad) brain->wm_scratchpad = nimcp_wms_create(NULL);
        if (!brain->analogical_transfer) brain->analogical_transfer = nimcp_analogical_create(NULL);
        if (!brain->multiscale_memory) brain->multiscale_memory = nimcp_multiscale_create(NULL);

        /* Inner speech requires native language — create only if language enabled */
        if (brain->native_language_enabled && brain->native_language && !brain->inner_speech) {
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

//=============================================================================
// Portia (process singleton) — 2026-11-23
//=============================================================================
/**
 * @brief Initialize the Portia resource-adaptive singleton if nobody has.
 *
 * portia_init had no caller anywhere, so portia_is_initialized() was always
 * false: the brain_learn_vector portia_update hook and the Portia-Swarm
 * bridge creation above never ran. Portia is process-global (never torn
 * down with a brain). On a mains-powered server it reports AC power and
 * degradation NONE, so the reasoning budget it feeds stays full unless the
 * host actually throttles thermally.
 */
bool nimcp_brain_factory_init_portia_subsystem(brain_t brain) {
    (void)brain;
    if (portia_is_initialized()) return true;
    nimcp_error_t err = portia_init(NULL);
    if (err != NIMCP_SUCCESS && !portia_is_initialized()) {
        LOG_MODULE_WARN(LOG_MODULE, "portia_init failed (%d) — continuing without Portia", (int)err);
        return false;
    }
    LOG_MODULE_INFO(LOG_MODULE, "Portia resource-adaptive system initialized");
    return true;
}

//=============================================================================
// Extended (dog) hearing on the brain's cochlea — 2026-11-23
//=============================================================================
/* Forward-declared: nimcp_cochlea.h's medulla_t/thalamus_t tags collide with
 * nimcp_brain_internal.h. Definitions in src/lib/perception/nimcp_cochlea.c. */
extern bool cochlea_has_extended_hearing(const struct cochlea* cochlea);
extern int cochlea_enable_dog_hearing(struct cochlea* cochlea);
extern uint32_t cochlea_copy_last_input(const struct cochlea* cochlea, float* dst,
                                        uint32_t max_samples, uint64_t* seq_out);

/**
 * @brief Attach dog-mode extended hearing to the brain's (human-mode) cochlea.
 *
 * The brain cochlea is created in BM_MODE_HUMAN, which never creates the
 * extended processor, and nothing called cochlea_enable_extended_mode, so the
 * dog/bat code never ran. Dog mode gives ILD/azimuth localization of what the
 * brain hears (true binaural only via cochlea_process_stereo; the 16 kHz mono
 * hearing stream carries no ultrasonic band). ~1 KB.
 */
bool nimcp_brain_factory_init_ext_hearing_subsystem(brain_t brain) {
    if (!brain || !brain->cochlea) return true;   /* no cochlea: nothing to extend */
    if (cochlea_has_extended_hearing(brain->cochlea)) return true;
    return cochlea_enable_dog_hearing(brain->cochlea) == 0;
}

bool nimcp_brain_ext_hearing_present(brain_t brain) {
    return brain && brain->cochlea && cochlea_has_extended_hearing(brain->cochlea);
}

//=============================================================================
// Non-human / superhuman perception + interoception — 2026-11-23
//=============================================================================
/*
 * The src/superhuman modules (echolocation, eagle vision, time dilation,
 * savant, hyperthymesia, precognition, synesthesia) and the interoceptive
 * predictor had no creator in any brain. They are created here (from eager
 * init, so fresh and resumed brains both get them) and driven by
 * brain_tick_nonhuman() with live brain signals:
 *   echolocation  <- the raw audio block the cochlea last heard
 *   eagle vision  <- the retina ON/OFF map (brain_nonhuman_post_visual)
 *   precognition  <- the learn/decide feature vector (surprise z-score)
 *   time dilation <- NE arousal (threat), feature novelty, surprise
 *   savant        <- labelled feature patterns
 *   hyperthymesia <- periodic autobiographical snapshots (DA/5-HT/NE affect)
 *   synesthesia   <- the training label text (grapheme -> colour)
 *   interoception <- DA / 5-HT / NE levels as endocrine signals
 * It also steps the dragonfly (medulla bridge + tracker + LNN) and Portia.
 *
 * The brain struct has no field for these, so state lives in a small side
 * table keyed by brain pointer; brain_nonhuman_destroy() frees it.
 * Measured create cost (heap): echo 530 KB, eagle 60 KB, time 42 KB,
 * savant 142 KB (+<=256 KB patterns), hyperthymesia 788 KB (+938 B/memory,
 * <=19 MB at the 20k cap, one memory per 1000 ticks), precognition 816 KB
 * (+512 KB history), synesthesia 81 KB, interoception 110 KB: ~3 MB at
 * start, bounded ~24 MB. Independent of neuron count. Tick ~22 us.
 */

/* void*-typed wrappers (the module headers clash with brain_internal). */
extern void*    hyperthymesia_brain_create(uint32_t memory_capacity);
extern void     hyperthymesia_brain_destroy(void* module);
extern uint64_t hyperthymesia_brain_encode(void* module, const float* features, uint32_t n,
                                           float valence, float arousal);
extern uint64_t hyperthymesia_brain_count(const void* module);
extern void*    precognition_brain_create(void);
extern void     precognition_brain_destroy(void* module);
extern float    precognition_brain_observe(void* module, const float* features, uint32_t n);

#define NH_MAX_BRAINS        8u
#define NH_FEAT              64u     /* features kept per pattern / memory */
#define NH_PRECOG_FEAT       128u    /* precognition state_dim */
#define NH_AUDIO_MAX         1024u
#define NH_VIS_MAX           256u    /* sensory-hub visual map cap (16 x 16) */
#define NH_HYPER_CAPACITY    20000u
#define NH_HYPER_EVERY       1000u
#define NH_PATTERN_EVERY     64u
#define NH_INTERO_EVERY      16u
#define NH_PORTIA_EVERY      512u
#define NH_DRAGONFLY_PERIOD_US 33000u

typedef struct {
    brain_t brain;
    nimcp_mutex_t* mu;

    echolocation_system_t*   echo;
    echolocation_output_t*   echo_out;
    eagle_vision_system_t*   eagle;
    eagle_vision_output_t*   eagle_out;
    time_dilation_system_t*  tdil;
    savant_system_t*         savant;
    synesthesia_module_t*    synesthesia;
    void*                    hyper;
    void*                    precog;
    nimcp_intero_context_t*  intero;

    float    prev_feat[NH_PRECOG_FEAT];
    uint32_t prev_n;
    uint64_t audio_seq;
    float    audio[NH_AUDIO_MAX];

    /* latest retina frame, posted by the vision path */
    bool     vis_pending;
    uint32_t vis_w, vis_h;
    float    vis_on[NH_VIS_MAX];
    float    vis_off[NH_VIS_MAX];
    float    vis_rgb[NH_VIS_MAX * 3];

    uint64_t ticks;
    uint64_t last_tick_us;
    uint64_t dragonfly_last_us;

    /* observability */
    uint64_t echo_blocks, echoes_seen, eagle_frames, eagle_targets;
    uint64_t savant_learned, savant_matches, syn_texts, intero_signals, hyper_encoded;
    uint64_t dragonfly_steps, portia_updates;
    float    last_surprise, last_novelty, last_dilation;
} nh_state_t;

static nh_state_t* volatile g_nh[NH_MAX_BRAINS];
static pthread_mutex_t g_nh_mu = PTHREAD_MUTEX_INITIALIZER;

static nh_state_t* nh_find(brain_t brain) {
    for (uint32_t i = 0; i < NH_MAX_BRAINS; i++) {
        nh_state_t* st = __atomic_load_n(&g_nh[i], __ATOMIC_ACQUIRE);
        if (st && st->brain == brain) return st;
    }
    return NULL;
}

static void nh_free(nh_state_t* st) {
    if (!st) return;
    echolocation_output_destroy(st->echo_out);
    echolocation_destroy(st->echo);
    eagle_vision_output_destroy(st->eagle_out);
    eagle_vision_destroy(st->eagle);
    time_dilation_destroy(st->tdil);
    savant_destroy(st->savant);
    synesthesia_destroy(st->synesthesia);
    hyperthymesia_brain_destroy(st->hyper);
    precognition_brain_destroy(st->precog);
    nimcp_intero_destroy(st->intero);
    if (st->mu) nimcp_mutex_free(st->mu);
    free(st);
}

bool nimcp_brain_nonhuman_present(brain_t brain) {
    return brain && nh_find(brain) != NULL;
}

bool nimcp_brain_factory_init_nonhuman_subsystem(brain_t brain) {
    if (!brain) return false;
    if (nh_find(brain)) return true;   /* idempotent */

    nh_state_t* st = (nh_state_t*)calloc(1, sizeof(*st));
    if (!st) return false;
    st->brain = brain;
    st->mu = nimcp_mutex_create(NULL);
    if (!st->mu) { free(st); return false; }

    { echolocation_config_t c; if (echolocation_default_config(&c) == 0) st->echo = echolocation_create(&c);
      if (st->echo) st->echo_out = echolocation_output_create(c.max_echoes, c.max_objects); }
    { eagle_vision_config_t c; if (eagle_vision_default_config(&c) == 0) st->eagle = eagle_vision_create(&c);
      if (st->eagle) st->eagle_out = eagle_vision_output_create(c.max_targets, 64, c.max_patterns); }
    { time_dilation_config_t c; if (time_dilation_default_config(&c) == 0) st->tdil = time_dilation_create(&c); }
    { savant_config_t c; if (savant_default_config(&c) == 0) st->savant = savant_create(&c); }
    { synesthesia_config_t c = synesthesia_default_config(); st->synesthesia = synesthesia_create(&c); }
    st->hyper = hyperthymesia_brain_create(NH_HYPER_CAPACITY);
    st->precog = precognition_brain_create();
    { nimcp_intero_config_t c; nimcp_intero_default_config(&c); st->intero = nimcp_intero_create(&c);
      if (st->intero) (void)nimcp_intero_init_standard_systems(st->intero); }

    pthread_mutex_lock(&g_nh_mu);
    uint32_t slot = NH_MAX_BRAINS;
    for (uint32_t i = 0; i < NH_MAX_BRAINS; i++) if (!g_nh[i]) { slot = i; break; }
    if (slot < NH_MAX_BRAINS) __atomic_store_n(&g_nh[slot], st, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&g_nh_mu);
    if (slot == NH_MAX_BRAINS) { nh_free(st); return false; }

    LOG_MODULE_INFO(LOG_MODULE, "Non-human perception: echo=%d eagle=%d time=%d savant=%d "
                    "synesthesia=%d hyperthymesia=%d precognition=%d interoception=%d",
                    st->echo != NULL, st->eagle != NULL, st->tdil != NULL, st->savant != NULL,
                    st->synesthesia != NULL, st->hyper != NULL, st->precog != NULL,
                    st->intero != NULL);
    return true;
}

void brain_nonhuman_destroy(brain_t brain) {
    if (!brain) return;
    pthread_mutex_lock(&g_nh_mu);
    nh_state_t* st = NULL;
    for (uint32_t i = 0; i < NH_MAX_BRAINS; i++) {
        if (g_nh[i] && g_nh[i]->brain == brain) { st = g_nh[i]; __atomic_store_n(&g_nh[i], NULL, __ATOMIC_RELEASE); break; }
    }
    pthread_mutex_unlock(&g_nh_mu);
    nh_free(st);
}

/**
 * @brief Hand the latest retina ON/OFF ganglion map to eagle vision.
 * Called from the vision path after each frame (maps are rates in [0,1]).
 */
void brain_nonhuman_post_visual(brain_t brain, const float* on_map, const float* off_map,
                                uint32_t w, uint32_t h) {
    nh_state_t* st = nh_find(brain);
    if (!st || !on_map || !off_map || w == 0 || h == 0 || w * h > NH_VIS_MAX) return;
    nimcp_mutex_lock(st->mu);
    memcpy(st->vis_on, on_map, (size_t)w * h * sizeof(float));
    memcpy(st->vis_off, off_map, (size_t)w * h * sizeof(float));
    st->vis_w = w;
    st->vis_h = h;
    st->vis_pending = true;
    nimcp_mutex_unlock(st->mu);
}

static float nh_neuromod(brain_t brain, neuromodulator_type_t t) {
    if (!brain->neuromodulator_system) return 0.0f;
    float v = neuromodulator_get_level(brain->neuromodulator_system, t);
    return isfinite(v) ? v : 0.0f;
}

/**
 * @brief Drive the non-human modules one step from live brain signals.
 *
 * Safe from any thread and on any brain (no-op until eager init created the
 * state); a concurrent call skips rather than waits. Cheap per call: the
 * heavier modules run on sub-sampled cadences (see NH_*_EVERY).
 */
void brain_tick_nonhuman(brain_t brain, const float* features, uint32_t num_features,
                         const char* label) {
    if (!brain) return;
    nh_state_t* st = nh_find(brain);

    /* Wall-clock dt, clamped: learn and decide call at irregular rates. */
    uint64_t now = nimcp_time_now_us();

    if (!st) return;
    if (nimcp_mutex_trylock(st->mu) != 0) return;

    /* Dragonfly: brain_step_dragonfly also steps the medulla bridge
     * (arousal/circadian gating) and the sidecar LNN, which the label hook's
     * raw dragonfly_update bypasses — they never advanced before. The LNN
     * step costs ~0.9 ms regardless of dt, so integrate the elapsed time at
     * ~30 Hz (dragonfly tracking runs on a frame clock) instead of per call. */
    if (brain->dragonfly && brain->dragonfly_enabled) {
        if (!st->dragonfly_last_us) st->dragonfly_last_us = now;
        uint64_t dt_us = now - st->dragonfly_last_us;
        if (dt_us >= NH_DRAGONFLY_PERIOD_US) {
            if (dt_us > 100000u) dt_us = 100000u;
            extern int brain_step_dragonfly(brain_t brain, uint64_t delta_t);
            if (brain_step_dragonfly(brain, dt_us) == 0) st->dragonfly_steps++;
            st->dragonfly_last_us = now;
        }
    }

    const float dt_ms = st->last_tick_us ?
        fminf(fmaxf((float)(now - st->last_tick_us) / 1000.0f, 1.0f), 100.0f) : 16.0f;
    st->last_tick_us = now;
    st->ticks++;

    if (st->ticks % NH_PORTIA_EVERY == 0 && portia_is_initialized()) {
        if (portia_update() == NIMCP_SUCCESS) st->portia_updates++;
    }

    const float da = nh_neuromod(brain, NEUROMOD_DOPAMINE);
    const float sht = nh_neuromod(brain, NEUROMOD_SEROTONIN);
    const float ne = nh_neuromod(brain, NEUROMOD_NOREPINEPHRINE);

    /* Feature-driven modules. */
    bool feats_ok = features && num_features > 0;
    uint32_t nf = feats_ok ? (num_features < NH_PRECOG_FEAT ? num_features : NH_PRECOG_FEAT) : 0;
    if (feats_ok) {
        for (uint32_t i = 0; i < nf; i++) if (!isfinite(features[i])) { feats_ok = false; break; }
    }
    if (feats_ok) {
        float novelty = 0.0f;
        if (st->prev_n == nf) {
            for (uint32_t i = 0; i < nf; i++) novelty += fabsf(features[i] - st->prev_feat[i]);
            novelty /= (float)nf;
        }
        memcpy(st->prev_feat, features, nf * sizeof(float));
        st->prev_n = nf;
        st->last_novelty = novelty;

        if (st->precog) {
            float z = precognition_brain_observe(st->precog, features, nf);
            if (z >= 0.0f) st->last_surprise = z;
        }
        if (st->tdil) {
            float threat = fminf(fmaxf(ne, 0.0f), 1.0f);
            float nov = fminf(novelty, 1.0f);
            float cplx = fminf(st->last_surprise / 5.0f, 1.0f);
            (void)time_dilation_check_triggers(st->tdil, threat, nov, cplx);
            uint32_t done = 0;
            (void)time_dilation_process(st->tdil, dt_ms, &done);
            st->last_dilation = time_dilation_get_factor(st->tdil);
        }
        uint32_t np = nf < NH_FEAT ? nf : NH_FEAT;
        if (st->savant && label && label[0] && st->ticks % NH_PATTERN_EVERY == 0) {
            savant_match_result_t m[4];
            uint32_t nm = 0;
            if (savant_find_patterns(st->savant, features, np, m, 4, &nm) == 0 && nm > 0) {
                st->savant_matches++;
            } else {
                savant_config_t sc;
                uint32_t id = 0;
                if (savant_get_config(st->savant, &sc) == 0 && st->savant_learned < sc.max_patterns &&
                    savant_learn_pattern(st->savant, features, np, SAVANT_PATTERN_SPATIAL, label, &id) == 0)
                    st->savant_learned++;
            }
        }
        if (st->hyper && st->ticks % NH_HYPER_EVERY == 0 &&
            hyperthymesia_brain_count(st->hyper) < NH_HYPER_CAPACITY) {
            float valence = fminf(fmaxf(da - sht * 0.5f, -1.0f), 1.0f);
            if (hyperthymesia_brain_encode(st->hyper, features, np, valence,
                                           fminf(fmaxf(ne, 0.0f), 1.0f)) != 0)
                st->hyper_encoded++;
        }
    }

    if (st->synesthesia && label && label[0] && st->ticks % NH_PATTERN_EVERY == 0) {
        synesthesia_color_t colors[64];
        uint32_t nc = 0;
        if (synesthesia_colorize_text(st->synesthesia, label, colors, 64, &nc)) st->syn_texts++;
    }

    if (st->intero && st->ticks % NH_INTERO_EVERY == 0 && brain->neuromodulator_system) {
        const struct { nimcp_intero_signal_type_t t; float v; } sig[3] = {
            { NIMCP_SIGNAL_DOPAMINE, da }, { NIMCP_SIGNAL_SEROTONIN, sht },
            { NIMCP_SIGNAL_ADRENALINE, ne } };
        for (int i = 0; i < 3; i++) {
            nimcp_intero_signal_t s;
            memset(&s, 0, sizeof(s));
            s.type = sig[i].t;
            s.system = NIMCP_SYSTEM_ENDOCRINE;
            s.value = sig[i].v;
            s.precision = 1.0;
            s.timestamp = now;
            s.is_valid = true;
            if (nimcp_intero_process_signal(st->intero, &s) == NIMCP_INTERO_OK) st->intero_signals++;
        }
    }

    /* Passive echolocation on what the cochlea last heard (new blocks only). */
    if (st->echo && st->echo_out && brain->cochlea) {
        uint64_t seq = 0;
        uint32_t n = cochlea_copy_last_input(brain->cochlea, st->audio, NH_AUDIO_MAX, &seq);
        if (n > 0 && seq != st->audio_seq) {
            st->audio_seq = seq;
            echo_audio_buffer_t ab = { .samples = st->audio, .num_samples = n,
                                       .sample_rate = BRAIN_HEARING_SAMPLE_RATE,
                                       .num_channels = 1, .timestamp_ms = (float)(now / 1000u) };
            if (echolocation_process_audio(st->echo, &ab, st->echo_out) == 0) {
                st->echo_blocks++;
                st->echoes_seen += st->echo_out->num_echoes;
            }
        }
    }

    /* Eagle vision on the latest retina frame. */
    if (st->eagle && st->eagle_out && st->vis_pending) {
        const uint32_t w = st->vis_w, h = st->vis_h;
        for (uint32_t i = 0; i < w * h; i++) {
            st->vis_rgb[i * 3 + 0] = st->vis_on[i];
            st->vis_rgb[i * 3 + 1] = st->vis_off[i];
            st->vis_rgb[i * 3 + 2] = 0.5f * (st->vis_on[i] + st->vis_off[i]);
        }
        st->vis_pending = false;
        eagle_vision_input_t in = { .data = st->vis_rgb, .width = w, .height = h, .channels = 3,
                                    .stride = w * 3, .timestamp_ms = (float)(now / 1000u),
                                    .has_uv_channel = false };
        if (eagle_vision_process_frame(st->eagle, &in, st->eagle_out) == 0) {
            st->eagle_frames++;
            st->eagle_targets += st->eagle_out->num_targets;
        }
    }

    nimcp_mutex_unlock(st->mu);
}

/**
 * @brief Non-human module counters as a JSON object (for an RPC / dashboard).
 * @return bytes written (snprintf semantics), or -1 if the brain has none.
 */
int brain_nonhuman_stats_json(brain_t brain, char* buf, size_t n) {
    nh_state_t* st = nh_find(brain);
    if (!st || !buf || n == 0) return -1;
    nimcp_mutex_lock(st->mu);
    int w = snprintf(buf, n,
        "{\"ticks\":%llu,\"dragonfly_steps\":%llu,\"portia_updates\":%llu,"
        "\"echo_blocks\":%llu,\"echoes\":%llu,\"eagle_frames\":%llu,\"eagle_targets\":%llu,"
        "\"savant_learned\":%llu,\"savant_matches\":%llu,\"synesthesia_texts\":%llu,"
        "\"intero_signals\":%llu,\"hyperthymesia_memories\":%llu,"
        "\"surprise_z\":%.4f,\"novelty\":%.4f,\"time_dilation\":%.4f,\"ext_hearing\":%s}",
        (unsigned long long)st->ticks, (unsigned long long)st->dragonfly_steps,
        (unsigned long long)st->portia_updates, (unsigned long long)st->echo_blocks,
        (unsigned long long)st->echoes_seen, (unsigned long long)st->eagle_frames,
        (unsigned long long)st->eagle_targets, (unsigned long long)st->savant_learned,
        (unsigned long long)st->savant_matches, (unsigned long long)st->syn_texts,
        (unsigned long long)st->intero_signals,
        (unsigned long long)(st->hyper ? hyperthymesia_brain_count(st->hyper) : 0),
        (double)st->last_surprise, (double)st->last_novelty, (double)st->last_dilation,
        nimcp_brain_ext_hearing_present(brain) ? "true" : "false");
    nimcp_mutex_unlock(st->mu);
    return w;
}
