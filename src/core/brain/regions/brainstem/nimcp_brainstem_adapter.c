/**
 * @file nimcp_brainstem_adapter.c
 * @brief Implementation of Brainstem region brain adapter
 *
 * WHAT: Unified adapter connecting brainstem sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers and autonomic control
 * HOW:  Orchestrates midbrain, pons, medulla, and reticular formation
 *
 * @version Phase BS-1: Brainstem Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"
#include "core/medulla/nimcp_medulla.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_neural_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brainstem_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define BRAINSTEM_LOG_MODULE "BRAINSTEM"

/*=============================================================================
 * INTERNAL SUB-MODULE STRUCTURES
 *===========================================================================*/

/**
 * @brief Midbrain processor (superior/inferior colliculus, PAG)
 */
struct midbrain_processor {
    midbrain_config_t config;

    /* Superior Colliculus (saccade/orienting) */
    float target_x;
    float target_y;
    float target_salience;
    bool saccade_pending;

    /* Inferior Colliculus (sound localization) */
    float sound_azimuth;
    float sound_elevation;
    float sound_intensity;

    /* Periaqueductal Gray (defensive behaviors) */
    float defensive_activation;
    float fear_level;

    /* Statistics */
    uint64_t saccades_generated;
    uint64_t sounds_processed;
    uint64_t defensive_activations;
};

/**
 * @brief Pons processor (relay, respiratory, sleep-wake)
 */
struct pons_processor {
    pons_config_t config;

    /* Relay state */
    float relay_gain;
    uint64_t relay_count;

    /* Respiratory center analog */
    float respiratory_rate;
    float respiratory_depth;

    /* Locus Coeruleus (norepinephrine) */
    float lc_activity;              /**< Arousal drive */

    /* Raphe Nuclei (serotonin) */
    float raphe_activity;           /**< Mood/sleep modulation */

    /* Sleep-wake transition */
    float sleep_pressure;
    bool sleep_promoting;
};

/**
 * @brief Reticular formation (arousal, attention, motor tone)
 */
struct reticular_formation {
    reticular_config_t config;

    /* Ascending Reticular Activating System */
    float current_arousal;
    float target_arousal;
    brainstem_arousal_level_t arousal_level;

    /* Descending modulation */
    float motor_tone;

    /* Attention filtering */
    float attention_level;
    float habituation_level;

    /* Statistics */
    uint64_t arousal_updates;
    uint64_t attention_shifts;
};

/**
 * @brief Registered reflex
 */
typedef struct reflex_entry {
    brainstem_reflex_t reflex;
    bool registered;
} reflex_entry_t;

/**
 * @brief Internal adapter structure
 */
struct brainstem_adapter {
    /* Configuration */
    brainstem_config_t config;

    /* Sub-modules */
    midbrain_processor_t* midbrain;
    pons_processor_t* pons;
    reticular_formation_t* reticular;
    medulla_t medulla;
    bool owns_medulla;               /**< Did we create the medulla? */

    /* Reflex registry */
    reflex_entry_t* reflexes;
    uint32_t reflex_count;
    uint32_t reflex_capacity;

    /* Callbacks */
    brainstem_reflex_callback_t reflex_callback;
    void* reflex_user_data;
    brainstem_arousal_callback_t arousal_callback;
    void* arousal_user_data;
    brainstem_vital_callback_t vital_callback;
    void* vital_user_data;
    brainstem_orienting_callback_t orienting_callback;
    void* orienting_user_data;

    /* State */
    brainstem_status_t status;
    brainstem_error_t last_error;
    double current_time_ms;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    brainstem_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(brainstem_adapter_t* adapter, brainstem_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != BRAINSTEM_ERROR_NONE) {
        adapter->status = BRAINSTEM_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", BRAINSTEM_LOG_MODULE, error);
    }
}

/**
 * @brief Convert arousal value to level
 */
static brainstem_arousal_level_t arousal_to_level(float arousal) {
    if (arousal < 0.05f) return BRAINSTEM_AROUSAL_COMA;
    if (arousal < 0.15f) return BRAINSTEM_AROUSAL_DEEP_SLEEP;
    if (arousal < 0.30f) return BRAINSTEM_AROUSAL_LIGHT_SLEEP;
    if (arousal < 0.45f) return BRAINSTEM_AROUSAL_DROWSY;
    if (arousal < 0.70f) return BRAINSTEM_AROUSAL_AWAKE;
    if (arousal < 0.85f) return BRAINSTEM_AROUSAL_ALERT;
    return BRAINSTEM_AROUSAL_HYPERAROUSED;
}

/**
 * @brief Create midbrain processor
 */
static midbrain_processor_t* midbrain_create(const midbrain_config_t* config) {
    midbrain_processor_t* mb = nimcp_calloc(1, sizeof(midbrain_processor_t));
    if (!mb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mb is NULL");

        return NULL;

    }

    if (config) {
        mb->config = *config;
    } else {
        /* Defaults */
        mb->config.enable_superior_colliculus = true;
        mb->config.saccade_threshold = 0.3f;
        mb->config.max_saccade_targets = 4;
        mb->config.enable_inferior_colliculus = true;
        mb->config.sound_localization_precision = 0.8f;
        mb->config.enable_pag = true;
        mb->config.defensive_threshold = 0.6f;
    }

    return mb;
}

/**
 * @brief Destroy midbrain processor
 */
static void midbrain_destroy(midbrain_processor_t* mb) {
    if (mb) nimcp_free(mb);
}

/**
 * @brief Create pons processor
 */
static pons_processor_t* pons_create(const pons_config_t* config) {
    pons_processor_t* pons = nimcp_calloc(1, sizeof(pons_processor_t));
    if (!pons) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pons is NULL");

        return NULL;

    }

    if (config) {
        pons->config = *config;
    } else {
        /* Defaults */
        pons->config.enable_corticopontine_relay = true;
        pons->config.relay_latency_ms = 5.0f;
        pons->config.enable_pneumotaxic_center = true;
        pons->config.base_respiratory_rate = 0.5f;
        pons->config.enable_locus_coeruleus = true;
        pons->config.enable_raphe_nuclei = true;
    }

    pons->relay_gain = 1.0f;
    pons->respiratory_rate = pons->config.base_respiratory_rate;
    pons->respiratory_depth = 0.5f;
    pons->lc_activity = 0.5f;
    pons->raphe_activity = 0.5f;

    return pons;
}

/**
 * @brief Destroy pons processor
 */
static void pons_destroy(pons_processor_t* pons) {
    if (pons) nimcp_free(pons);
}

/**
 * @brief Create reticular formation
 */
static reticular_formation_t* reticular_create(const reticular_config_t* config) {
    reticular_formation_t* rf = nimcp_calloc(1, sizeof(reticular_formation_t));
    if (!rf) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rf is NULL");

        return NULL;

    }

    if (config) {
        rf->config = *config;
    } else {
        /* Defaults */
        rf->config.enable_aras = true;
        rf->config.baseline_arousal = BRAINSTEM_DEFAULT_AROUSAL_BASELINE;
        rf->config.arousal_decay_rate = 0.05f;
        rf->config.enable_descending_modulation = true;
        rf->config.motor_tone_baseline = NIMCP_NOREPINEPHRINE_BASELINE;
        rf->config.enable_attention_filter = true;
        rf->config.habituation_rate = 0.1f;
    }

    rf->current_arousal = rf->config.baseline_arousal;
    rf->target_arousal = rf->config.baseline_arousal;
    rf->arousal_level = arousal_to_level(rf->current_arousal);
    rf->motor_tone = rf->config.motor_tone_baseline;
    rf->attention_level = 0.5f;
    rf->habituation_level = 0.0f;

    return rf;
}

/**
 * @brief Destroy reticular formation
 */
static void reticular_destroy(reticular_formation_t* rf) {
    if (rf) nimcp_free(rf);
}

/**
 * @brief Update midbrain processing
 */
static void midbrain_update(midbrain_processor_t* mb, float dt) {
    if (!mb) return;

    /* Superior colliculus: decay pending saccade */
    if (mb->saccade_pending) {
        mb->target_salience *= (1.0f - dt);
        if (mb->target_salience < 0.1f) {
            mb->saccade_pending = false;
        }
    }

    /* PAG: decay defensive activation */
    mb->defensive_activation *= (1.0f - 0.2f * dt);
    if (mb->defensive_activation < 0.01f) {
        mb->defensive_activation = 0.0f;
    }
}

/**
 * @brief Update pons processing
 */
static void pons_update(pons_processor_t* pons, float dt) {
    if (!pons) return;

    /* Locus coeruleus: maintain arousal drive with some noise */
    /* (In real implementation, this would be modulated by inputs) */

    /* Raphe: slow modulation toward baseline */
    float raphe_target = 0.5f;
    pons->raphe_activity += (raphe_target - pons->raphe_activity) * 0.1f * dt;
}

/**
 * @brief Update reticular formation
 */
static void reticular_update(reticular_formation_t* rf, float dt) {
    if (!rf) return;

    /* Move current arousal toward target */
    float diff = rf->target_arousal - rf->current_arousal;
    float rate = (diff > 0) ? 0.5f : rf->config.arousal_decay_rate;
    rf->current_arousal += diff * rate * dt;

    /* Clamp arousal */
    if (rf->current_arousal < 0.0f) rf->current_arousal = 0.0f;
    if (rf->current_arousal > 1.0f) rf->current_arousal = 1.0f;

    /* Natural decay toward baseline */
    float baseline_diff = rf->config.baseline_arousal - rf->current_arousal;
    rf->current_arousal += baseline_diff * rf->config.arousal_decay_rate * dt * 0.1f;

    /* Update level */
    rf->arousal_level = arousal_to_level(rf->current_arousal);

    /* Update motor tone based on arousal */
    float tone_target = rf->config.motor_tone_baseline +
                        (rf->current_arousal - 0.5f) * 0.4f;
    if (tone_target < 0.1f) tone_target = 0.1f;
    if (tone_target > 0.9f) tone_target = 0.9f;
    rf->motor_tone += (tone_target - rf->motor_tone) * 0.3f * dt;

    /* Habituation decay */
    rf->habituation_level *= (1.0f - rf->config.habituation_rate * dt);

    rf->arousal_updates++;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

brainstem_config_t brainstem_default_config(void) {
    brainstem_config_t config;
    memset(&config, 0, sizeof(config));

    /* Midbrain defaults */
    config.midbrain.enable_superior_colliculus = true;
    config.midbrain.saccade_threshold = 0.3f;
    config.midbrain.max_saccade_targets = 4;
    config.midbrain.enable_inferior_colliculus = true;
    config.midbrain.sound_localization_precision = 0.8f;
    config.midbrain.enable_pag = true;
    config.midbrain.defensive_threshold = 0.6f;

    /* Pons defaults */
    config.pons.enable_corticopontine_relay = true;
    config.pons.relay_latency_ms = 5.0f;
    config.pons.enable_pneumotaxic_center = true;
    config.pons.base_respiratory_rate = 0.5f;
    config.pons.enable_locus_coeruleus = true;
    config.pons.enable_raphe_nuclei = true;

    /* Reticular defaults */
    config.reticular.enable_aras = true;
    config.reticular.baseline_arousal = BRAINSTEM_DEFAULT_AROUSAL_BASELINE;
    config.reticular.arousal_decay_rate = 0.05f;
    config.reticular.enable_descending_modulation = true;
    config.reticular.motor_tone_baseline = NIMCP_NOREPINEPHRINE_BASELINE;
    config.reticular.enable_attention_filter = true;
    config.reticular.habituation_rate = 0.1f;

    /* Main adapter defaults */
    config.use_external_medulla = false;
    config.enable_reflexes = true;
    config.enable_vital_monitoring = true;
    config.enable_arousal_control = true;
    config.enable_events = true;
    config.update_interval_ms = BRAINSTEM_DEFAULT_UPDATE_INTERVAL_MS;
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_NOREPINEPHRINE;

    return config;
}

brainstem_adapter_t* brainstem_create(const brainstem_config_t* config,
                                       medulla_t external_medulla) {
    LOG_INFO("[%s] Creating brainstem adapter", BRAINSTEM_LOG_MODULE);

    brainstem_adapter_t* adapter = nimcp_calloc(1, sizeof(brainstem_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", BRAINSTEM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_default_config: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = brainstem_default_config();
    }

    /* Create midbrain */
    adapter->midbrain = midbrain_create(&adapter->config.midbrain);
    if (!adapter->midbrain) {
        LOG_ERROR("[%s] Failed to create midbrain processor", BRAINSTEM_LOG_MODULE);
        brainstem_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_default_config: adapter->midbrain is NULL");
        return NULL;
    }

    /* Create pons */
    adapter->pons = pons_create(&adapter->config.pons);
    if (!adapter->pons) {
        LOG_ERROR("[%s] Failed to create pons processor", BRAINSTEM_LOG_MODULE);
        brainstem_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_default_config: adapter->pons is NULL");
        return NULL;
    }

    /* Create reticular formation */
    adapter->reticular = reticular_create(&adapter->config.reticular);
    if (!adapter->reticular) {
        LOG_ERROR("[%s] Failed to create reticular formation", BRAINSTEM_LOG_MODULE);
        brainstem_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_default_config: adapter->reticular is NULL");
        return NULL;
    }

    /* Set up medulla */
    if (external_medulla && adapter->config.use_external_medulla) {
        adapter->medulla = external_medulla;
        adapter->owns_medulla = false;
        LOG_DEBUG("[%s] Using external medulla", BRAINSTEM_LOG_MODULE);
    } else {
        /* Create internal medulla */
        medulla_config_t med_cfg = medulla_default_config();
        med_cfg.enable_bio_async = adapter->config.enable_bio_async;
        adapter->medulla = medulla_create(&med_cfg);
        if (!adapter->medulla) {
            LOG_WARN("[%s] Failed to create medulla (continuing without)", BRAINSTEM_LOG_MODULE);
        } else {
            adapter->owns_medulla = true;
            medulla_start(adapter->medulla);
        }
    }

    /* Initialize reflex registry */
    adapter->reflex_capacity = BRAINSTEM_DEFAULT_MAX_REFLEXES;
    adapter->reflexes = nimcp_calloc(adapter->reflex_capacity, sizeof(reflex_entry_t));
    if (!adapter->reflexes) {
        LOG_ERROR("[%s] Failed to allocate reflex registry", BRAINSTEM_LOG_MODULE);
        brainstem_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brainstem_default_config: adapter->reflexes is NULL");
        return NULL;
    }

    /* Initialize bio-async */
    adapter->bio_ctx = NULL;
    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", BRAINSTEM_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_BRAINSTEM,
            .module_name = "brainstem_region",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            LOG_INFO("[%s] Bio-async registered successfully", BRAINSTEM_LOG_MODULE);
        } else {
            LOG_WARN("[%s] Failed to register with bio-async router", BRAINSTEM_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = BRAINSTEM_STATUS_ACTIVE;
    adapter->last_error = BRAINSTEM_ERROR_NONE;
    adapter->current_time_ms = 0.0;

    LOG_INFO("[%s] Brainstem adapter created successfully", BRAINSTEM_LOG_MODULE);
    return adapter;
}

void brainstem_destroy(brainstem_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying brainstem adapter", BRAINSTEM_LOG_MODULE);

    /* Unregister from bio-async */
    if (adapter->bio_ctx) {
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Destroy sub-modules */
    if (adapter->midbrain) {
        midbrain_destroy(adapter->midbrain);
    }
    if (adapter->pons) {
        pons_destroy(adapter->pons);
    }
    if (adapter->reticular) {
        reticular_destroy(adapter->reticular);
    }
    if (adapter->medulla && adapter->owns_medulla) {
        medulla_stop(adapter->medulla);
        medulla_destroy(adapter->medulla);
    }

    /* Free reflex registry */
    if (adapter->reflexes) {
        nimcp_free(adapter->reflexes);
    }

    nimcp_free(adapter);
    LOG_DEBUG("[%s] Brainstem adapter destroyed", BRAINSTEM_LOG_MODULE);
}

bool brainstem_reset(brainstem_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", BRAINSTEM_LOG_MODULE);

    /* Reset midbrain */
    if (adapter->midbrain) {
        adapter->midbrain->target_x = 0.0f;
        adapter->midbrain->target_y = 0.0f;
        adapter->midbrain->target_salience = 0.0f;
        adapter->midbrain->saccade_pending = false;
        adapter->midbrain->defensive_activation = 0.0f;
    }

    /* Reset pons */
    if (adapter->pons) {
        adapter->pons->relay_gain = 1.0f;
        adapter->pons->lc_activity = 0.5f;
        adapter->pons->raphe_activity = 0.5f;
        adapter->pons->sleep_pressure = 0.0f;
        adapter->pons->sleep_promoting = false;
    }

    /* Reset reticular formation */
    if (adapter->reticular) {
        adapter->reticular->current_arousal = adapter->reticular->config.baseline_arousal;
        adapter->reticular->target_arousal = adapter->reticular->config.baseline_arousal;
        adapter->reticular->arousal_level = arousal_to_level(adapter->reticular->current_arousal);
        adapter->reticular->motor_tone = adapter->reticular->config.motor_tone_baseline;
        adapter->reticular->attention_level = 0.5f;
        adapter->reticular->habituation_level = 0.0f;
    }

    /* Reset state */
    adapter->status = BRAINSTEM_STATUS_ACTIVE;
    adapter->last_error = BRAINSTEM_ERROR_NONE;

    return true;
}

/*=============================================================================
 * REFLEX MANAGEMENT
 *===========================================================================*/

bool brainstem_register_reflex(brainstem_adapter_t* adapter,
                                const brainstem_reflex_t* reflex) {
    if (!adapter || !reflex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_reset: required parameter is NULL (adapter, reflex)");
        return false;
    }

    if (adapter->reflex_count >= adapter->reflex_capacity) {
        LOG_WARN("[%s] Reflex registry full", BRAINSTEM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "brainstem_reset: capacity exceeded");
        return false;
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < adapter->reflex_capacity; i++) {
        if (!adapter->reflexes[i].registered) {
            adapter->reflexes[i].reflex = *reflex;
            adapter->reflexes[i].registered = true;
            adapter->reflex_count++;
            LOG_DEBUG("[%s] Registered reflex: %s (id=%u)",
                      BRAINSTEM_LOG_MODULE, reflex->name, reflex->reflex_id);
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brainstem_reset: operation failed");
    return false;
}

bool brainstem_trigger_reflex(brainstem_adapter_t* adapter,
                               uint32_t reflex_id,
                               float stimulus_intensity,
                               brainstem_motor_output_t* output) {
    if (!adapter || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_reset: required parameter is NULL (adapter, output)");
        return false;
    }

    /* Find reflex */
    for (uint32_t i = 0; i < adapter->reflex_capacity; i++) {
        if (adapter->reflexes[i].registered &&
            adapter->reflexes[i].reflex.reflex_id == reflex_id) {

            brainstem_reflex_t* reflex = &adapter->reflexes[i].reflex;

            /* Check threshold */
            if (stimulus_intensity < reflex->threshold) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brainstem_reset: validation failed");
                return false;
            }

            /* Generate output */
            output->pathway_id = reflex_id;
            output->activation = stimulus_intensity * reflex->gain;
            if (output->activation > 1.0f) output->activation = 1.0f;
            output->urgency = stimulus_intensity;
            output->timestamp_ms = adapter->current_time_ms + reflex->latency_ms;
            output->is_reflex = true;

            /* Update stats */
            adapter->stats.reflexes_triggered++;

            /* Invoke callback */
            if (adapter->reflex_callback) {
                adapter->reflex_callback(reflex, output, adapter->reflex_user_data);
            }

            LOG_DEBUG("[%s] Triggered reflex: %s (intensity=%.2f)",
                      BRAINSTEM_LOG_MODULE, reflex->name, stimulus_intensity);
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brainstem_reset: operation failed");
    return false;
}

bool brainstem_set_reflex_callback(brainstem_adapter_t* adapter,
                                    brainstem_reflex_callback_t callback,
                                    void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_reset: adapter is NULL");
        return false;
    }
    adapter->reflex_callback = callback;
    adapter->reflex_user_data = user_data;
    return true;
}

/*=============================================================================
 * SENSORY PROCESSING (MIDBRAIN)
 *===========================================================================*/

bool brainstem_process_sensory(brainstem_adapter_t* adapter,
                                const brainstem_sensory_input_t* input,
                                brainstem_orienting_response_t* response) {
    if (!adapter || !input || !response || !adapter->midbrain) {
        set_error(adapter, BRAINSTEM_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_reset: required parameter is NULL (adapter, input, response, adapter->midbrain)");
        return false;
    }

    memset(response, 0, sizeof(brainstem_orienting_response_t));
    midbrain_processor_t* mb = adapter->midbrain;

    /* Superior Colliculus: Visual orienting */
    if (mb->config.enable_superior_colliculus && input->visual_salience > 0.0f) {
        if (input->visual_salience > mb->config.saccade_threshold) {
            mb->target_x = input->visual_target_x;
            mb->target_y = input->visual_target_y;
            mb->target_salience = input->visual_salience;
            mb->saccade_pending = true;

            response->saccade_x = input->visual_target_x;
            response->saccade_y = input->visual_target_y;
            response->attention_shift = input->visual_salience;

            mb->saccades_generated++;
            adapter->stats.saccades_generated++;
        }
    }

    /* Inferior Colliculus: Auditory orienting */
    if (mb->config.enable_inferior_colliculus && input->sound_intensity > 0.0f) {
        mb->sound_azimuth = input->sound_azimuth;
        mb->sound_elevation = input->sound_elevation;
        mb->sound_intensity = input->sound_intensity;

        /* Convert azimuth to head turn */
        float azimuth_norm = input->sound_azimuth / 180.0f; /* -1 to 1 */
        response->head_turn = azimuth_norm * input->sound_intensity;

        if (input->sudden_sound) {
            /* Startle reflex via PAG */
            mb->defensive_activation = fminf(1.0f, mb->defensive_activation + 0.3f);
            response->reflex_triggered = true;

            /* Boost arousal */
            if (adapter->reticular) {
                adapter->reticular->current_arousal =
                    fminf(1.0f, adapter->reticular->current_arousal + 0.2f);
            }
        }

        mb->sounds_processed++;
    }

    /* PAG: Defensive behavior */
    if (mb->config.enable_pag && mb->defensive_activation > mb->config.defensive_threshold) {
        /* Freeze or fight-or-flight preparation */
        if (adapter->reticular) {
            adapter->reticular->target_arousal = fminf(1.0f, 0.8f + mb->defensive_activation * 0.2f);
        }
        mb->defensive_activations++;
    }

    adapter->stats.midbrain_activations++;

    /* Invoke callback */
    if (adapter->orienting_callback && (response->saccade_x != 0.0f ||
                                         response->saccade_y != 0.0f ||
                                         response->head_turn != 0.0f)) {
        adapter->orienting_callback(response, adapter->orienting_user_data);
    }

    return true;
}

bool brainstem_generate_saccade(brainstem_adapter_t* adapter,
                                 float target_x,
                                 float target_y,
                                 float urgency) {
    if (!adapter || !adapter->midbrain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, adapter->midbrain)");
        return false;
    }

    midbrain_processor_t* mb = adapter->midbrain;
    mb->target_x = target_x;
    mb->target_y = target_y;
    mb->target_salience = urgency;
    mb->saccade_pending = true;
    mb->saccades_generated++;
    adapter->stats.saccades_generated++;

    return true;
}

bool brainstem_set_orienting_callback(brainstem_adapter_t* adapter,
                                       brainstem_orienting_callback_t callback,
                                       void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }
    adapter->orienting_callback = callback;
    adapter->orienting_user_data = user_data;
    return true;
}

/*=============================================================================
 * AROUSAL CONTROL (RETICULAR FORMATION)
 *===========================================================================*/

brainstem_arousal_level_t brainstem_get_arousal_level(const brainstem_adapter_t* adapter) {
    if (!adapter || !adapter->reticular) return BRAINSTEM_AROUSAL_AWAKE;
    return adapter->reticular->arousal_level;
}

float brainstem_get_arousal_value(const brainstem_adapter_t* adapter) {
    if (!adapter || !adapter->reticular) return 0.5f;
    return adapter->reticular->current_arousal;
}

bool brainstem_boost_arousal(brainstem_adapter_t* adapter, float amount) {
    if (!adapter || !adapter->reticular) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_boost_arousal: required parameter is NULL (adapter, adapter->reticular)");
        return false;
    }

    brainstem_arousal_level_t old_level = adapter->reticular->arousal_level;

    adapter->reticular->current_arousal += amount;
    if (adapter->reticular->current_arousal > 1.0f) {
        adapter->reticular->current_arousal = 1.0f;
    }
    adapter->reticular->target_arousal = adapter->reticular->current_arousal;
    adapter->reticular->arousal_level = arousal_to_level(adapter->reticular->current_arousal);

    adapter->stats.arousal_modulations++;

    /* Notify via callback */
    if (adapter->arousal_callback && old_level != adapter->reticular->arousal_level) {
        adapter->arousal_callback(old_level, adapter->reticular->arousal_level,
                                   adapter->reticular->current_arousal,
                                   adapter->arousal_user_data);
    }

    return true;
}

bool brainstem_reduce_arousal(brainstem_adapter_t* adapter, float amount) {
    if (!adapter || !adapter->reticular) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_reduce_arousal: required parameter is NULL (adapter, adapter->reticular)");
        return false;
    }

    brainstem_arousal_level_t old_level = adapter->reticular->arousal_level;

    adapter->reticular->current_arousal -= amount;
    if (adapter->reticular->current_arousal < 0.0f) {
        adapter->reticular->current_arousal = 0.0f;
    }
    adapter->reticular->target_arousal = adapter->reticular->current_arousal;
    adapter->reticular->arousal_level = arousal_to_level(adapter->reticular->current_arousal);

    adapter->stats.arousal_modulations++;

    /* Notify via callback */
    if (adapter->arousal_callback && old_level != adapter->reticular->arousal_level) {
        adapter->arousal_callback(old_level, adapter->reticular->arousal_level,
                                   adapter->reticular->current_arousal,
                                   adapter->arousal_user_data);
    }

    return true;
}

bool brainstem_set_target_arousal(brainstem_adapter_t* adapter, float target) {
    if (!adapter || !adapter->reticular) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_set_target_arousal: required parameter is NULL (adapter, adapter->reticular)");
        return false;
    }

    if (target < 0.0f) target = 0.0f;
    if (target > 1.0f) target = 1.0f;

    adapter->reticular->target_arousal = target;
    return true;
}

bool brainstem_set_arousal_callback(brainstem_adapter_t* adapter,
                                     brainstem_arousal_callback_t callback,
                                     void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_set_target_arousal: adapter is NULL");
        return false;
    }
    adapter->arousal_callback = callback;
    adapter->arousal_user_data = user_data;
    return true;
}

/*=============================================================================
 * VITAL FUNCTIONS (MEDULLA)
 *===========================================================================*/

bool brainstem_get_vitals(const brainstem_adapter_t* adapter,
                           brainstem_vitals_t* vitals) {
    if (!adapter || !vitals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_set_target_arousal: required parameter is NULL (adapter, vitals)");
        return false;
    }

    memset(vitals, 0, sizeof(brainstem_vitals_t));

    if (adapter->medulla) {
        /* Get arousal from medulla if available */
        float arousal = medulla_get_arousal_level(adapter->medulla);
        if (arousal >= 0.0f) {
            vitals->heart_rate_analog = 0.3f + arousal * 0.4f;
            vitals->respiratory_rate = 0.3f + arousal * 0.4f;
        } else {
            vitals->heart_rate_analog = 0.5f;
            vitals->respiratory_rate = 0.5f;
        }
        vitals->blood_pressure_analog = 0.5f;
        vitals->temperature_analog = 0.5f;

        /* Check for protection level */
        protection_level_t prot = medulla_get_protection_level(adapter->medulla);
        if (prot >= PROTECTION_LEVEL_CRITICAL) {
            vitals->vital_alarm = true;
        }
    } else {
        /* No medulla - use defaults */
        vitals->heart_rate_analog = 0.5f;
        vitals->respiratory_rate = 0.5f;
        vitals->blood_pressure_analog = 0.5f;
        vitals->temperature_analog = 0.5f;
    }

    return true;
}

bool brainstem_set_vital_callback(brainstem_adapter_t* adapter,
                                   brainstem_vital_callback_t callback,
                                   void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_set_target_arousal: adapter is NULL");
        return false;
    }
    adapter->vital_callback = callback;
    adapter->vital_user_data = user_data;
    return true;
}

bool brainstem_trigger_protection(brainstem_adapter_t* adapter, float severity) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_trigger_protection: adapter is NULL");
        return false;
    }

    if (adapter->medulla) {
        if (severity > 0.9f) {
            medulla_emergency_shutdown(adapter->medulla, "High severity protection trigger");
        }
    }

    /* Reduce arousal in protective response */
    if (adapter->reticular) {
        adapter->reticular->target_arousal = 0.2f;
    }

    adapter->status = BRAINSTEM_STATUS_PROTECTIVE;
    return true;
}

/*=============================================================================
 * RELAY FUNCTIONS (PONS)
 *===========================================================================*/

bool brainstem_relay_signal(brainstem_adapter_t* adapter,
                             const float* signal,
                             uint32_t signal_size,
                             float* output) {
    if (!adapter || !signal || !output || !adapter->pons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_trigger_protection: required parameter is NULL (adapter, signal, output, adapter->pons)");
        return false;
    }

    pons_processor_t* pons = adapter->pons;

    /* Apply relay gain and copy */
    for (uint32_t i = 0; i < signal_size; i++) {
        output[i] = signal[i] * pons->relay_gain;
    }

    pons->relay_count++;
    adapter->stats.pons_relays++;

    return true;
}

bool brainstem_modulate_sleep(brainstem_adapter_t* adapter, float sleep_pressure) {
    if (!adapter || !adapter->pons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_modulate_sleep: required parameter is NULL (adapter, adapter->pons)");
        return false;
    }

    pons_processor_t* pons = adapter->pons;
    pons->sleep_pressure = sleep_pressure;
    pons->sleep_promoting = (sleep_pressure > 0.6f);

    /* Modulate reticular arousal based on sleep pressure */
    if (adapter->reticular && pons->sleep_promoting) {
        adapter->reticular->target_arousal =
            adapter->reticular->config.baseline_arousal * (1.0f - sleep_pressure * 0.5f);
    }

    return true;
}

/*=============================================================================
 * UPDATE AND STATE
 *===========================================================================*/

bool brainstem_update(brainstem_adapter_t* adapter, float dt) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_update: adapter is NULL");
        return false;
    }

    adapter->current_time_ms += dt * 1000.0;

    /* Update sub-modules */
    midbrain_update(adapter->midbrain, dt);
    pons_update(adapter->pons, dt);
    reticular_update(adapter->reticular, dt);

    /* Update medulla if we own it */
    if (adapter->medulla && adapter->owns_medulla) {
        medulla_update(adapter->medulla, dt);
        adapter->stats.medulla_updates++;
    }

    adapter->stats.reticular_updates++;
    adapter->stats.updates_processed++;

    /* Update average arousal */
    if (adapter->reticular) {
        float alpha = 0.01f;
        adapter->stats.avg_arousal = adapter->stats.avg_arousal * (1.0f - alpha) +
                                      adapter->reticular->current_arousal * alpha;
    }

    /* Update status based on arousal */
    if (adapter->status != BRAINSTEM_STATUS_ERROR &&
        adapter->status != BRAINSTEM_STATUS_PROTECTIVE) {
        if (adapter->reticular) {
            if (adapter->reticular->arousal_level <= BRAINSTEM_AROUSAL_LIGHT_SLEEP) {
                adapter->status = BRAINSTEM_STATUS_SLEEP;
                adapter->stats.sleep_episodes++;
            } else if (adapter->reticular->arousal_level >= BRAINSTEM_AROUSAL_ALERT) {
                adapter->status = BRAINSTEM_STATUS_ALERT;
                adapter->stats.alert_episodes++;
            } else {
                adapter->status = BRAINSTEM_STATUS_ACTIVE;
            }
        }
    }

    return true;
}

bool brainstem_get_state(const brainstem_adapter_t* adapter,
                          brainstem_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_update: required parameter is NULL (adapter, state)");
        return false;
    }

    memset(state, 0, sizeof(brainstem_state_t));

    state->status = adapter->status;

    if (adapter->reticular) {
        state->arousal_level = adapter->reticular->arousal_level;
        state->arousal_value = adapter->reticular->current_arousal;
        state->motor_tone = adapter->reticular->motor_tone;
    } else {
        state->arousal_level = BRAINSTEM_AROUSAL_AWAKE;
        state->arousal_value = 0.5f;
        state->motor_tone = 0.5f;
    }

    brainstem_get_vitals(adapter, &state->vitals);

    if (adapter->midbrain) {
        state->orienting.saccade_x = adapter->midbrain->target_x;
        state->orienting.saccade_y = adapter->midbrain->target_y;
    }

    if (adapter->pons) {
        state->sleep_pressure_high = adapter->pons->sleep_promoting;
    }

    if (adapter->medulla) {
        circadian_phase_t phase = medulla_get_circadian_phase(adapter->medulla);
        state->circadian_phase = (float)phase * 3.0f; /* Approximate hours */
    }

    return true;
}

brainstem_status_t brainstem_get_status(const brainstem_adapter_t* adapter) {
    if (!adapter) return BRAINSTEM_STATUS_ERROR;
    return adapter->status;
}

brainstem_error_t brainstem_get_last_error(const brainstem_adapter_t* adapter) {
    if (!adapter) return BRAINSTEM_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* brainstem_error_string(brainstem_error_t error) {
    switch (error) {
        case BRAINSTEM_ERROR_NONE: return "No error";
        case BRAINSTEM_ERROR_INVALID_INPUT: return "Invalid input";
        case BRAINSTEM_ERROR_MIDBRAIN_FAILURE: return "Midbrain processing failed";
        case BRAINSTEM_ERROR_PONS_FAILURE: return "Pons processing failed";
        case BRAINSTEM_ERROR_MEDULLA_FAILURE: return "Medulla processing failed";
        case BRAINSTEM_ERROR_RETICULAR_FAILURE: return "Reticular formation failed";
        case BRAINSTEM_ERROR_REFLEX_FAILURE: return "Reflex activation failed";
        case BRAINSTEM_ERROR_AROUSAL_FAILURE: return "Arousal modulation failed";
        case BRAINSTEM_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* brainstem_status_string(brainstem_status_t status) {
    switch (status) {
        case BRAINSTEM_STATUS_OFFLINE: return "Offline";
        case BRAINSTEM_STATUS_INITIALIZING: return "Initializing";
        case BRAINSTEM_STATUS_ACTIVE: return "Active";
        case BRAINSTEM_STATUS_ALERT: return "Alert";
        case BRAINSTEM_STATUS_PROTECTIVE: return "Protective";
        case BRAINSTEM_STATUS_SLEEP: return "Sleep";
        case BRAINSTEM_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

const char* brainstem_arousal_string(brainstem_arousal_level_t level) {
    switch (level) {
        case BRAINSTEM_AROUSAL_COMA: return "Coma";
        case BRAINSTEM_AROUSAL_DEEP_SLEEP: return "Deep Sleep";
        case BRAINSTEM_AROUSAL_LIGHT_SLEEP: return "Light Sleep";
        case BRAINSTEM_AROUSAL_DROWSY: return "Drowsy";
        case BRAINSTEM_AROUSAL_AWAKE: return "Awake";
        case BRAINSTEM_AROUSAL_ALERT: return "Alert";
        case BRAINSTEM_AROUSAL_HYPERAROUSED: return "Hyperaroused";
        default: return "Unknown";
    }
}

bool brainstem_get_stats(const brainstem_adapter_t* adapter, brainstem_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool brainstem_get_config(const brainstem_adapter_t* adapter, brainstem_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_get_config: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

midbrain_processor_t* brainstem_get_midbrain(brainstem_adapter_t* adapter) {
    return adapter ? adapter->midbrain : NULL;
}

pons_processor_t* brainstem_get_pons(brainstem_adapter_t* adapter) {
    return adapter ? adapter->pons : NULL;
}

reticular_formation_t* brainstem_get_reticular(brainstem_adapter_t* adapter) {
    return adapter ? adapter->reticular : NULL;
}

medulla_t brainstem_get_medulla(brainstem_adapter_t* adapter) {
    return adapter ? adapter->medulla : NULL;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t brainstem_get_bio_context(brainstem_adapter_t* adapter) {
    return adapter ? adapter->bio_ctx : NULL;
}

uint32_t brainstem_process_bio_messages(brainstem_adapter_t* adapter,
                                         uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;
    return bio_router_process_inbox(adapter->bio_ctx, max_messages);
}

nimcp_error_t brainstem_broadcast_arousal_change(brainstem_adapter_t* adapter,
                                                   brainstem_arousal_level_t new_level) {
    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS; /* Not an error if bio-async disabled */

    /* Would broadcast arousal state change here */
    LOG_DEBUG("[%s] Broadcasting arousal change: %s",
              BRAINSTEM_LOG_MODULE, brainstem_arousal_string(new_level));

    return NIMCP_SUCCESS;
}

nimcp_error_t brainstem_broadcast_reflex(brainstem_adapter_t* adapter,
                                          const brainstem_reflex_t* reflex) {
    if (!adapter || !reflex) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;

    LOG_DEBUG("[%s] Broadcasting reflex activation: %s",
              BRAINSTEM_LOG_MODULE, reflex->name);

    return NIMCP_SUCCESS;
}
