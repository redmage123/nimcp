/**
 * @file nimcp_medulla_cerebellum_bridge.c
 * @brief Implementation of Medulla-Cerebellum Bridge
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/medulla/nimcp_medulla.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(medulla_cerebellum_bridge)

#define LOG_MODULE "MEDULLA_CEREBELLUM_BRIDGE"



/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Bridge internal state
 */
struct med_cereb_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    med_cereb_bridge_config_t config;

    /* Connected systems */
    medulla_t medulla;
    cerebellum_adapter_t* cerebellum;
    bio_router_t router;

    /* Inferior olive model */
    med_cereb_inferior_olive_t io;

    /* Error queue */
    med_cereb_pending_error_t error_queue[MED_CEREB_MAX_ERROR_QUEUE];
    uint32_t error_queue_head;
    uint32_t error_queue_tail;
    uint32_t error_queue_count;

    /* Current effects (cached) */
    med_cereb_arousal_effects_t arousal_effects;
    med_cereb_protection_effects_t protection_effects;
    med_cereb_circadian_effects_t circadian_effects;

    /* Emergency state */
    bool emergency_stop_active;

    /* Statistics */
    med_cereb_bridge_stats_t stats;

    /* Last update time */
    uint64_t last_update_us;
};

/* ============================================================================
 * ERROR TYPE NAMES
 * ============================================================================ */

static const char* ERROR_TYPE_NAMES[MED_CEREB_ERROR_COUNT] = {
    "Timing",
    "Amplitude",
    "Trajectory",
    "Coordination",
    "Prediction",
    "Protection",
    "Sequence"
};

/* ============================================================================
 * HELPER: AROUSAL LEVEL CONVERSION
 * ============================================================================ */

/**
 * @brief Convert medulla arousal level to normalized value [0, 1]
 */
static float arousal_to_normalized(arousal_level_t level) {
    switch (level) {
        case AROUSAL_LEVEL_COMA:        return 0.0f;
        case AROUSAL_LEVEL_DEEP_SLEEP:  return 0.15f;
        case AROUSAL_LEVEL_LIGHT_SLEEP: return 0.3f;
        case AROUSAL_LEVEL_DROWSY:      return 0.45f;
        case AROUSAL_LEVEL_AWAKE:       return 0.6f;
        case AROUSAL_LEVEL_ALERT:       return 0.8f;
        case AROUSAL_LEVEL_HYPERAROUSAL:return 1.0f;
        default:                        return 0.5f;
    }
}

/**
 * @brief Compute arousal effects using inverted-U model (Yerkes-Dodson)
 */
static void compute_arousal_effects(float arousal_normalized,
                                     float optimal_arousal,
                                     float sensitivity,
                                     med_cereb_arousal_effects_t* effects) {
    /* Motor gain increases with arousal (linear) */
    effects->motor_gain = 0.4f + arousal_normalized * 1.2f;
    if (effects->motor_gain > 2.0f) effects->motor_gain = 2.0f;
    if (effects->motor_gain < 0.2f) effects->motor_gain = 0.2f;

    /* Reaction time decreases with arousal */
    effects->reaction_time_factor = 2.0f - arousal_normalized * 1.5f;
    if (effects->reaction_time_factor < 0.5f) effects->reaction_time_factor = 0.5f;
    if (effects->reaction_time_factor > 2.0f) effects->reaction_time_factor = 2.0f;

    /* Deep nuclei excitability */
    effects->nuclei_excitability = arousal_normalized;

    /* Fine motor precision follows inverted-U (Yerkes-Dodson law) */
    float deviation = fabsf(arousal_normalized - optimal_arousal);
    effects->fine_motor_precision = 1.0f - (deviation * deviation * sensitivity);
    if (effects->fine_motor_precision < 0.2f) effects->fine_motor_precision = 0.2f;
    if (effects->fine_motor_precision > 1.0f) effects->fine_motor_precision = 1.0f;

    /* Tremor increases at extremes */
    if (arousal_normalized < 0.3f) {
        effects->tremor_amplitude = 0.3f - arousal_normalized;
    } else if (arousal_normalized > 0.85f) {
        effects->tremor_amplitude = (arousal_normalized - 0.85f) * 2.0f;
    } else {
        effects->tremor_amplitude = 0.0f;
    }
    if (effects->tremor_amplitude > 1.0f) effects->tremor_amplitude = 1.0f;
}

/**
 * @brief Compute protection effects
 */
static void compute_protection_effects(protection_level_t level,
                                        uint8_t non_essential_cutoff,
                                        uint8_t voluntary_cutoff,
                                        med_cereb_protection_effects_t* effects) {
    memset(effects, 0, sizeof(*effects));

    /* Output scaling based on protection level */
    switch (level) {
        case PROTECTION_LEVEL_NORMAL:
            effects->output_scale = 1.0f;
            break;
        case PROTECTION_LEVEL_CAUTIOUS:
            effects->output_scale = 0.9f;
            break;
        case PROTECTION_LEVEL_GUARDED:
            effects->output_scale = 0.7f;
            break;
        case PROTECTION_LEVEL_DEFENSIVE:
            effects->output_scale = 0.5f;
            effects->non_essential_disabled = true;
            break;
        case PROTECTION_LEVEL_CRITICAL:
            effects->output_scale = 0.2f;
            effects->non_essential_disabled = true;
            effects->voluntary_disabled = true;
            effects->reflexes_only = true;
            break;
        case PROTECTION_LEVEL_SHUTDOWN:
            effects->output_scale = 0.0f;
            effects->non_essential_disabled = true;
            effects->voluntary_disabled = true;
            effects->emergency_stop = true;
            effects->reflexes_only = true;
            break;
        default:
            effects->output_scale = 1.0f;
            break;
    }

    /* Apply cutoff levels */
    if ((uint8_t)level >= non_essential_cutoff) {
        effects->non_essential_disabled = true;
    }
    if ((uint8_t)level >= voluntary_cutoff) {
        effects->voluntary_disabled = true;
    }
}

/**
 * @brief Compute circadian learning effects
 */
static void compute_circadian_effects(circadian_phase_t phase,
                                       float max_boost,
                                       float min_factor,
                                       med_cereb_circadian_effects_t* effects) {
    /* Learning rates vary by circadian phase */
    /* Peak learning during morning and late afternoon */
    float base_multiplier;

    switch (phase) {
        case CIRCADIAN_PHASE_EARLY_MORNING:
            base_multiplier = 0.7f;  /* Rising */
            break;
        case CIRCADIAN_PHASE_MORNING:
            base_multiplier = 1.0f;  /* Peak */
            break;
        case CIRCADIAN_PHASE_AFTERNOON:
            base_multiplier = 0.6f;  /* Post-lunch dip */
            break;
        case CIRCADIAN_PHASE_EVENING:
            base_multiplier = 0.9f;  /* Second peak */
            break;
        case CIRCADIAN_PHASE_LATE_EVENING:
            base_multiplier = 0.5f;  /* Declining */
            break;
        case CIRCADIAN_PHASE_NIGHT:
            base_multiplier = 0.4f;  /* Low */
            break;
        case CIRCADIAN_PHASE_DEEP_NIGHT:
            base_multiplier = min_factor;  /* Minimum */
            break;
        case CIRCADIAN_PHASE_PRE_DAWN:
            base_multiplier = 0.5f;  /* Rising */
            break;
        default:
            base_multiplier = 0.7f;
            break;
    }

    /* Scale to configured range */
    float range = max_boost - min_factor;
    effects->ltd_rate_multiplier = min_factor + base_multiplier * range;
    effects->ltp_rate_multiplier = min_factor + base_multiplier * range;

    /* Consolidation is highest during sleep */
    if (phase == CIRCADIAN_PHASE_DEEP_NIGHT ||
        phase == CIRCADIAN_PHASE_NIGHT) {
        effects->consolidation_rate = 0.9f;
    } else if (phase == CIRCADIAN_PHASE_PRE_DAWN ||
               phase == CIRCADIAN_PHASE_LATE_EVENING) {
        effects->consolidation_rate = 0.6f;
    } else {
        effects->consolidation_rate = 0.3f;
    }

    /* Retrieval follows learning curve */
    effects->retrieval_efficiency = base_multiplier;
}

/* ============================================================================
 * INFERIOR OLIVE MODEL
 * ============================================================================ */

/**
 * @brief Initialize inferior olive model
 */
static void init_inferior_olive(med_cereb_inferior_olive_t* io,
                                 const med_cereb_bridge_config_t* config) {
    memset(io, 0, sizeof(*io));

    io->num_neurons = config->num_io_neurons;
    if (io->num_neurons > MED_CEREB_MAX_IO_NEURONS) {
        io->num_neurons = MED_CEREB_MAX_IO_NEURONS;
    }

    io->oscillation_freq = config->io_oscillation_freq;
    io->coupling_strength = config->io_coupling_strength;
    io->refractory_period_us = (uint64_t)(config->io_refractory_ms * 1000.0f);
    io->firing_threshold = config->io_firing_threshold;

    /* Initialize neurons */
    for (uint32_t i = 0; i < io->num_neurons; i++) {
        med_cereb_io_neuron_t* neuron = &io->neurons[i];
        neuron->neuron_id = i;
        neuron->activation = 0.0f;
        neuron->error_accumulator = 0.0f;
        neuron->refractory_remaining_us = 0;
        neuron->is_refractory = false;

        /* Assign error type (distribute across types) */
        neuron->error_type = (med_cereb_error_type_t)(i % MED_CEREB_ERROR_COUNT);

        /* Random initial oscillation phase */
        neuron->oscillation_phase = (float)(i % 10) * 0.628f;  /* Spread phases */

        /* Assign Purkinje targets (10 per IO neuron) */
        neuron->num_targets = 10;
        for (uint32_t j = 0; j < neuron->num_targets; j++) {
            /* Each IO neuron contacts Purkinje cells in a strip */
            neuron->target_purkinje[j] = i * 10 + j;
        }
    }
}

/**
 * @brief Update inferior olive oscillations
 */
static void update_io_oscillations(med_cereb_inferior_olive_t* io, uint64_t delta_us) {
    float dt_sec = (float)delta_us / 1000000.0f;
    float phase_increment = 2.0f * (float)M_PI * io->oscillation_freq * dt_sec;

    /* Update each neuron */
    for (uint32_t i = 0; i < io->num_neurons; i++) {
        med_cereb_io_neuron_t* neuron = &io->neurons[i];

        /* Update oscillation phase */
        neuron->oscillation_phase += phase_increment;
        if (neuron->oscillation_phase > 2.0f * (float)M_PI) {
            neuron->oscillation_phase -= 2.0f * (float)M_PI;
        }

        /* Update refractory period */
        if (neuron->is_refractory) {
            if (neuron->refractory_remaining_us > delta_us) {
                neuron->refractory_remaining_us -= delta_us;
            } else {
                neuron->refractory_remaining_us = 0;
                neuron->is_refractory = false;
            }
        }

        /* Apply gap junction coupling (neighbors influence phase) */
        if (io->coupling_strength > 0.0f && i > 0 && i < io->num_neurons - 1) {
            float neighbor_avg = (io->neurons[i-1].oscillation_phase +
                                  io->neurons[i+1].oscillation_phase) / 2.0f;
            float phase_diff = neighbor_avg - neuron->oscillation_phase;
            neuron->oscillation_phase += phase_diff * io->coupling_strength * dt_sec;
        }
    }

    io->current_time_us += delta_us;
}

/**
 * @brief Distribute error to appropriate IO neurons
 */
static void distribute_error_to_io(med_cereb_inferior_olive_t* io,
                                    med_cereb_error_type_t error_type,
                                    float magnitude) {
    for (uint32_t i = 0; i < io->num_neurons; i++) {
        med_cereb_io_neuron_t* neuron = &io->neurons[i];

        if (neuron->error_type == error_type) {
            /* Primary responders get full error */
            neuron->error_accumulator += magnitude;
        } else {
            /* Neighbors get partial error (lateral spread) */
            neuron->error_accumulator += magnitude * 0.1f;
        }

        /* Clamp accumulator */
        if (neuron->error_accumulator > 1.0f) neuron->error_accumulator = 1.0f;
        if (neuron->error_accumulator < -1.0f) neuron->error_accumulator = -1.0f;
    }
}

/**
 * @brief Process IO neurons and generate spikes
 * @return Number of spikes generated
 */
static uint32_t process_io_spikes(struct med_cereb_bridge_struct* bridge) {
    med_cereb_inferior_olive_t* io = &bridge->io;
    uint32_t spikes = 0;

    for (uint32_t i = 0; i < io->num_neurons; i++) {
        med_cereb_io_neuron_t* neuron = &io->neurons[i];

        if (neuron->is_refractory) continue;

        /* Subthreshold oscillation modulates threshold */
        float osc_mod = sinf(neuron->oscillation_phase) * 0.2f;
        float effective_threshold = io->firing_threshold - osc_mod;

        /* Check for spike */
        if (fabsf(neuron->error_accumulator) >= effective_threshold) {
            /* Generate climbing fiber signal */
            if (bridge->cerebellum) {
                climbing_fiber_signal_t cf_signal;
                cf_signal.fiber_id = neuron->neuron_id;
                cf_signal.error_signal = neuron->error_accumulator;
                cf_signal.timestamp_ms = (float)io->current_time_us / 1000.0f;
                cf_signal.error_type = (uint8_t)neuron->error_type;

                /* Send to each target Purkinje cell */
                for (uint32_t t = 0; t < neuron->num_targets; t++) {
                    cf_signal.target_purkinje_id = neuron->target_purkinje[t];
                    cerebellum_process_climbing_signal(bridge->cerebellum, &cf_signal);
                }

                bridge->stats.climbing_signals_sent++;
                bridge->stats.signals_per_type[neuron->error_type]++;
            }

            /* Enter refractory period */
            neuron->is_refractory = true;
            neuron->refractory_remaining_us = io->refractory_period_us;

            /* Decay error accumulator */
            neuron->error_accumulator *= 0.5f;

            spikes++;
        } else {
            /* Slow decay of error accumulator */
            neuron->error_accumulator *= 0.99f;
        }
    }

    bridge->stats.io_spikes += spikes;
    return spikes;
}

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

int med_cereb_bridge_default_config(med_cereb_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->num_io_neurons = 50;  /* 50 IO neurons → 500 Purkinje targets */

    config->enable_arousal_modulation = true;
    config->enable_protection_gating = true;
    config->enable_circadian_learning = true;
    config->enable_io_signaling = true;

    /* IO parameters based on biological values */
    config->io_oscillation_freq = 10.0f;     /* ~10 Hz subthreshold oscillations */
    config->io_coupling_strength = 0.3f;     /* Moderate gap junction coupling */
    config->io_refractory_ms = 100.0f;       /* ~100ms refractory period */
    config->io_firing_threshold = 0.5f;      /* Moderate threshold */

    /* Arousal parameters */
    config->arousal_gain_sensitivity = 2.0f;
    config->optimal_arousal_level = 0.7f;    /* Optimal for fine motor */

    /* Protection parameters */
    config->non_essential_cutoff_level = PROTECTION_LEVEL_DEFENSIVE;
    config->voluntary_cutoff_level = PROTECTION_LEVEL_CRITICAL;

    /* Circadian parameters */
    config->max_circadian_learning_boost = 1.5f;
    config->min_circadian_learning_factor = 0.3f;

    /* Bio-async */
    config->enable_bio_async = false;
    config->update_interval_us = 10000;  /* 10ms */

    return 0;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

med_cereb_bridge_t med_cereb_bridge_create(const med_cereb_bridge_config_t* config) {
    struct med_cereb_bridge_struct* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "med_cereb_bridge_create: failed to allocate bridge structure");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        med_cereb_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "medulla_cerebellum") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "med_cereb_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize inferior olive */
    init_inferior_olive(&bridge->io, &bridge->config);

    /* Initialize default effects */
    compute_arousal_effects(0.6f, bridge->config.optimal_arousal_level,
                            bridge->config.arousal_gain_sensitivity,
                            &bridge->arousal_effects);

    compute_protection_effects(PROTECTION_LEVEL_NORMAL,
                               bridge->config.non_essential_cutoff_level,
                               bridge->config.voluntary_cutoff_level,
                               &bridge->protection_effects);

    compute_circadian_effects(CIRCADIAN_PHASE_MORNING,
                              bridge->config.max_circadian_learning_boost,
                              bridge->config.min_circadian_learning_factor,
                              &bridge->circadian_effects);

    NIMCP_LOGGING_INFO("Created %s bridge", "medulla_cerebellum");
    return bridge;
}

void med_cereb_bridge_destroy(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_destroy: bridge is NULL");
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "medulla_cerebellum");
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int med_cereb_bridge_reset(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset IO */
    init_inferior_olive(&bridge->io, &bridge->config);

    /* Clear error queue */
    bridge->error_queue_head = 0;
    bridge->error_queue_tail = 0;
    bridge->error_queue_count = 0;

    /* Reset emergency state */
    bridge->emergency_stop_active = false;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

int med_cereb_bridge_connect_medulla(med_cereb_bridge_t bridge, medulla_t medulla) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_connect_medulla: bridge is NULL");
        return -1;
    }
    if (!medulla) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_connect_medulla: medulla is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->medulla = medulla;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int med_cereb_bridge_connect_cerebellum(med_cereb_bridge_t bridge,
                                         cerebellum_adapter_t* cerebellum) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_connect_cerebellum: bridge is NULL");
        return -1;
    }
    if (!cerebellum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_connect_cerebellum: cerebellum is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->cerebellum = cerebellum;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int med_cereb_bridge_connect_bio_async(med_cereb_bridge_t bridge,
                                        bio_router_t router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->router = router;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool med_cereb_bridge_is_connected(med_cereb_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->medulla != NULL && bridge->cerebellum != NULL;
}

/* ============================================================================
 * INFERIOR OLIVE ERROR SIGNALING
 * ============================================================================ */

int med_cereb_bridge_queue_error(med_cereb_bridge_t bridge,
                                  med_cereb_error_type_t error_type,
                                  float magnitude,
                                  uint32_t source_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_queue_error: bridge is NULL");
        return -1;
    }
    if (error_type >= MED_CEREB_ERROR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "med_cereb_bridge_queue_error: error_type out of range");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check queue capacity */
    if (bridge->error_queue_count >= MED_CEREB_MAX_ERROR_QUEUE) {
        bridge->stats.errors_dropped++;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "med_cereb_bridge_is_connected: capacity exceeded");
        return -1;
    }

    /* Add to queue */
    med_cereb_pending_error_t* error = &bridge->error_queue[bridge->error_queue_tail];
    error->error_type = error_type;
    error->magnitude = magnitude;
    error->timestamp_us = nimcp_time_get_us();
    error->source_id = source_id;
    error->processed = false;

    bridge->error_queue_tail = (bridge->error_queue_tail + 1) % MED_CEREB_MAX_ERROR_QUEUE;
    bridge->error_queue_count++;

    /* Update stats */
    if (fabsf(magnitude) > bridge->stats.peak_error_magnitude) {
        bridge->stats.peak_error_magnitude = fabsf(magnitude);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int med_cereb_bridge_send_climbing_signal(med_cereb_bridge_t bridge,
                                           med_cereb_error_type_t error_type,
                                           float magnitude,
                                           uint32_t target_purkinje) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_send_climbing_signal: bridge is NULL");
        return -1;
    }
    if (!bridge->cerebellum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_send_climbing_signal: cerebellum is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    climbing_fiber_signal_t cf_signal;
    cf_signal.fiber_id = 0;  /* Direct signal, no specific fiber */
    cf_signal.error_signal = magnitude;
    cf_signal.timestamp_ms = (float)nimcp_time_get_us() / 1000.0f;
    cf_signal.target_purkinje_id = target_purkinje;
    cf_signal.error_type = (uint8_t)error_type;

    bool result = cerebellum_process_climbing_signal(bridge->cerebellum, &cf_signal);

    if (result) {
        bridge->stats.climbing_signals_sent++;
        bridge->stats.signals_per_type[error_type]++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return result ? 0 : -1;
}

int med_cereb_bridge_broadcast_error(med_cereb_bridge_t bridge,
                                      med_cereb_error_type_t error_type,
                                      float magnitude) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_broadcast_error: bridge is NULL");
        return -1;
    }
    if (!bridge->cerebellum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_broadcast_error: cerebellum is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool result = cerebellum_broadcast_error(bridge->cerebellum,
                                              magnitude,
                                              (uint8_t)error_type);

    if (result) {
        bridge->stats.climbing_signals_sent++;
        bridge->stats.signals_per_type[error_type]++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return result ? 0 : -1;
}

/* ============================================================================
 * AROUSAL MODULATION
 * ============================================================================ */

int med_cereb_bridge_get_arousal_effects(med_cereb_bridge_t bridge,
                                          med_cereb_arousal_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_arousal_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_arousal_effects: effects is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->arousal_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int med_cereb_bridge_modulate_motor(med_cereb_bridge_t bridge,
                                     const float* motor_command,
                                     float* modulated_command,
                                     uint32_t num_dimensions) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_modulate_motor: bridge is NULL");
        return -1;
    }
    if (!motor_command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_modulate_motor: motor_command is NULL");
        return -1;
    }
    if (!modulated_command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_modulate_motor: modulated_command is NULL");
        return -1;
    }
    if (num_dimensions == 0 || num_dimensions > 8) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "med_cereb_bridge_modulate_motor: num_dimensions out of range");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if motor is allowed */
    if (bridge->emergency_stop_active ||
        bridge->protection_effects.emergency_stop) {
        for (uint32_t i = 0; i < num_dimensions; i++) {
            modulated_command[i] = 0.0f;
        }
        bridge->stats.protection_gates++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Apply arousal gain */
    float gain = bridge->arousal_effects.motor_gain;

    /* Apply protection scaling */
    float scale = bridge->protection_effects.output_scale;

    /* Apply fine motor precision (adds noise at low precision) */
    float precision = bridge->arousal_effects.fine_motor_precision;

    for (uint32_t i = 0; i < num_dimensions; i++) {
        modulated_command[i] = motor_command[i] * gain * scale;

        /* Add tremor noise */
        if (bridge->arousal_effects.tremor_amplitude > 0.01f) {
            float tremor = bridge->arousal_effects.tremor_amplitude *
                           sinf((float)i * 1.5f + (float)bridge->io.current_time_us * 0.00001f);
            modulated_command[i] += tremor * 0.1f;
        }
    }

    bridge->stats.motor_commands_modulated++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * PROTECTION GATING
 * ============================================================================ */

int med_cereb_bridge_get_protection_effects(med_cereb_bridge_t bridge,
                                             med_cereb_protection_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_protection_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_protection_effects: effects is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->protection_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool med_cereb_bridge_motor_allowed(med_cereb_bridge_t bridge,
                                     bool is_essential,
                                     bool is_reflexive) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_motor_allowed: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool allowed = true;

    /* Emergency stop blocks everything including reflexes */
    if (bridge->emergency_stop_active) {
        allowed = false;
    } else if (bridge->protection_effects.emergency_stop) {
        allowed = false;
    } else if (is_reflexive) {
        /* Reflexes are always allowed unless emergency stop */
        allowed = true;
    } else if (bridge->protection_effects.voluntary_disabled) {
        /* Voluntary movements blocked */
        allowed = false;
    } else if (bridge->protection_effects.non_essential_disabled && !is_essential) {
        /* Non-essential movements blocked */
        allowed = false;
    } else if (bridge->protection_effects.reflexes_only) {
        /* Only reflexes allowed */
        allowed = false;
    }

    if (!allowed) {
        bridge->stats.protection_gates++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return allowed;
}

int med_cereb_bridge_emergency_stop(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_emergency_stop: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->emergency_stop_active = true;
    bridge->protection_effects.emergency_stop = true;
    bridge->stats.protection_gates++;

    /* Send protection error to cerebellum */
    if (bridge->cerebellum) {
        cerebellum_broadcast_error(bridge->cerebellum, 1.0f, MED_CEREB_ERROR_PROTECTION);
        bridge->stats.climbing_signals_sent++;
        bridge->stats.signals_per_type[MED_CEREB_ERROR_PROTECTION]++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int med_cereb_bridge_release_emergency(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_release_emergency: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->emergency_stop_active = false;
    bridge->protection_effects.emergency_stop = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * CIRCADIAN LEARNING MODULATION
 * ============================================================================ */

int med_cereb_bridge_get_circadian_effects(med_cereb_bridge_t bridge,
                                            med_cereb_circadian_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_circadian_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_circadian_effects: effects is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->circadian_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float med_cereb_bridge_get_learning_multiplier(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_learning_multiplier: bridge is NULL");
        return 1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Combine circadian and arousal effects */
    float circadian_mult = bridge->circadian_effects.ltd_rate_multiplier;
    float arousal_mult = bridge->arousal_effects.fine_motor_precision;

    float combined = circadian_mult * arousal_mult;
    if (combined < 0.1f) combined = 0.1f;
    if (combined > 2.0f) combined = 2.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return combined;
}

int med_cereb_bridge_apply_circadian_learning(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_apply_circadian_learning: bridge is NULL");
        return -1;
    }
    if (!bridge->cerebellum) return 0;  /* No-op if not connected */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Note: This would update cerebellum's learning rates if the API supported it */
    /* For now, we track the adjustment in stats */
    bridge->stats.learning_rate_adjustments++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * UPDATE
 * ============================================================================ */

int med_cereb_bridge_update(med_cereb_bridge_t bridge, uint64_t delta_us) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update arousal effects from medulla */
    if (bridge->medulla && bridge->config.enable_arousal_modulation) {
        /* medulla_get_arousal_level returns float 0.0-1.0, already normalized */
        float arousal = medulla_get_arousal_level(bridge->medulla);
        compute_arousal_effects(arousal,
                                bridge->config.optimal_arousal_level,
                                bridge->config.arousal_gain_sensitivity,
                                &bridge->arousal_effects);
    }

    /* Update protection effects from medulla */
    if (bridge->medulla && bridge->config.enable_protection_gating) {
        protection_level_t protection = medulla_get_protection_level(bridge->medulla);
        med_cereb_protection_effects_t old_effects = bridge->protection_effects;
        compute_protection_effects(protection,
                                   bridge->config.non_essential_cutoff_level,
                                   bridge->config.voluntary_cutoff_level,
                                   &bridge->protection_effects);

        /* Track protection gating events */
        bool was_gating = old_effects.non_essential_disabled ||
                          old_effects.voluntary_disabled ||
                          old_effects.reflexes_only ||
                          old_effects.emergency_stop;
        bool is_gating = bridge->protection_effects.non_essential_disabled ||
                         bridge->protection_effects.voluntary_disabled ||
                         bridge->protection_effects.reflexes_only ||
                         bridge->protection_effects.emergency_stop;

        if (is_gating && !was_gating) {
            bridge->stats.protection_gates++;
        }
    }

    /* Update circadian effects from medulla */
    if (bridge->medulla && bridge->config.enable_circadian_learning) {
        circadian_phase_t phase = medulla_get_circadian_phase(bridge->medulla);
        compute_circadian_effects(phase,
                                  bridge->config.max_circadian_learning_boost,
                                  bridge->config.min_circadian_learning_factor,
                                  &bridge->circadian_effects);
    }

    /* Process error queue through inferior olive */
    if (bridge->config.enable_io_signaling) {
        /* Process pending errors */
        while (bridge->error_queue_count > 0) {
            med_cereb_pending_error_t* error = &bridge->error_queue[bridge->error_queue_head];

            /* Distribute error to IO neurons */
            distribute_error_to_io(&bridge->io, error->error_type, error->magnitude);

            /* Update stats */
            float current_avg = bridge->stats.avg_error_magnitude;
            uint64_t count = bridge->stats.climbing_signals_sent;
            bridge->stats.avg_error_magnitude =
                (current_avg * count + fabsf(error->magnitude)) / (count + 1);

            /* Remove from queue */
            bridge->error_queue_head = (bridge->error_queue_head + 1) % MED_CEREB_MAX_ERROR_QUEUE;
            bridge->error_queue_count--;
        }

        /* Update IO oscillations */
        update_io_oscillations(&bridge->io, delta_us);

        /* Process IO spikes and generate climbing fiber signals */
        process_io_spikes(bridge);
    }

    bridge->last_update_us = nimcp_time_get_us();

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int med_cereb_bridge_process_messages(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_process_messages: bridge is NULL");
        return -1;
    }
    if (!bridge->router) return 0;

    /* Bio-async message processing would go here */
    return 0;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int med_cereb_bridge_get_stats(med_cereb_bridge_t bridge,
                                med_cereb_bridge_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int med_cereb_bridge_reset_stats(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int med_cereb_bridge_get_io_state(med_cereb_bridge_t bridge,
                                   med_cereb_inferior_olive_t* io_state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_io_state: bridge is NULL");
        return -1;
    }
    if (!io_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_get_io_state: io_state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *io_state = bridge->io;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t med_cereb_bridge_pending_error_count(med_cereb_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "med_cereb_bridge_pending_error_count: bridge is NULL");
        return 0;
    }
    return bridge->error_queue_count;
}

/* ============================================================================
 * DEBUG / DIAGNOSTICS
 * ============================================================================ */

void med_cereb_bridge_print_state(med_cereb_bridge_t bridge) {
    if (!bridge) {
        printf("Medulla-Cerebellum Bridge: NULL\n");
        return;
    }

    printf("=== Medulla-Cerebellum Bridge ===\n");
    printf("Connected: medulla=%s cerebellum=%s\n",
           bridge->medulla ? "yes" : "no",
           bridge->cerebellum ? "yes" : "no");
    printf("Emergency Stop: %s\n", bridge->emergency_stop_active ? "ACTIVE" : "inactive");
    printf("Pending Errors: %u\n", bridge->error_queue_count);

    printf("\nArousal Effects:\n");
    printf("  Motor Gain: %.2f\n", bridge->arousal_effects.motor_gain);
    printf("  Reaction Factor: %.2f\n", bridge->arousal_effects.reaction_time_factor);
    printf("  Fine Motor Precision: %.2f\n", bridge->arousal_effects.fine_motor_precision);
    printf("  Tremor: %.2f\n", bridge->arousal_effects.tremor_amplitude);

    printf("\nProtection Effects:\n");
    printf("  Output Scale: %.2f\n", bridge->protection_effects.output_scale);
    printf("  Non-Essential Disabled: %s\n",
           bridge->protection_effects.non_essential_disabled ? "yes" : "no");
    printf("  Voluntary Disabled: %s\n",
           bridge->protection_effects.voluntary_disabled ? "yes" : "no");

    printf("\nCircadian Effects:\n");
    printf("  LTD Multiplier: %.2f\n", bridge->circadian_effects.ltd_rate_multiplier);
    printf("  LTP Multiplier: %.2f\n", bridge->circadian_effects.ltp_rate_multiplier);
    printf("  Consolidation Rate: %.2f\n", bridge->circadian_effects.consolidation_rate);

    printf("\nStatistics:\n");
    printf("  Climbing Signals Sent: %lu\n", (unsigned long)bridge->stats.climbing_signals_sent);
    printf("  IO Spikes: %lu\n", (unsigned long)bridge->stats.io_spikes);
    printf("  Motor Commands Modulated: %lu\n", (unsigned long)bridge->stats.motor_commands_modulated);
    printf("  Protection Gates: %lu\n", (unsigned long)bridge->stats.protection_gates);
    printf("  Avg Error Magnitude: %.3f\n", bridge->stats.avg_error_magnitude);
}

void med_cereb_bridge_print_io_state(med_cereb_bridge_t bridge) {
    if (!bridge) {
        printf("Inferior Olive: NULL\n");
        return;
    }

    med_cereb_inferior_olive_t* io = &bridge->io;

    printf("=== Inferior Olive State ===\n");
    printf("Neurons: %u\n", io->num_neurons);
    printf("Oscillation Freq: %.1f Hz\n", io->oscillation_freq);
    printf("Coupling Strength: %.2f\n", io->coupling_strength);
    printf("Firing Threshold: %.2f\n", io->firing_threshold);

    printf("\nNeuron States (first 10):\n");
    for (uint32_t i = 0; i < io->num_neurons && i < 10; i++) {
        med_cereb_io_neuron_t* n = &io->neurons[i];
        printf("  [%u] type=%s accum=%.3f refractory=%s\n",
               n->neuron_id,
               ERROR_TYPE_NAMES[n->error_type],
               n->error_accumulator,
               n->is_refractory ? "yes" : "no");
    }
}

const char* med_cereb_error_type_name(med_cereb_error_type_t error_type) {
    if (error_type >= MED_CEREB_ERROR_COUNT) return "Unknown";
    return ERROR_TYPE_NAMES[error_type];
}
