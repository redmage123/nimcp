/**
 * @file nimcp_hypothalamus_perception_bridge.c
 * @brief Implementation of Hypothalamus-Perception Bridge for Sensory Modulation
 *
 * WHAT: Bidirectional integration between hypothalamus drives and sensory cortices
 * WHY:  Arousal and drives modulate sensory processing - hungry animals see food more
 * HOW:  Arousal → sensory gain; drive urgency → stimulus salience boost
 *
 * ENHANCED FEATURES (Phase 17.5):
 * 1. INTEROCEPTION: Internal body state sensing (hunger pangs, heart rate, etc.)
 * 2. OLFACTORY/GUSTATORY: Smell/taste integration for food/water detection
 * 3. PREDICTIVE CODING: Top-down predictions to perception for active inference
 * 4. PAIN MODULATION: Stress-induced analgesia and hyperalgesia
 * 5. SLEEP GATING: Sleep state modulates perception sensitivity
 * 6. THERMAL SALIENCE: Temperature drive enhances thermal stimulus processing
 *
 * @version Phase 17.5: Enhanced Perception Integration
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.h"
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
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_perception_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_PERCEPTION_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_perception_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hypo_drive_system_handle_t* drives;
    hypo_perception_config_t config;

    /* Current modulation state */
    hypo_perception_modulation_t modulation;

    /* Arousal from external source (brainstem/LC) */
    float external_arousal;

    /* Anticipation state per drive */
    float anticipation[HYPO_DRIVE_COUNT];

    /* Recent detection history */
    hypo_sensory_detection_t last_detection;
    bool has_recent_detection;

    /* Bio-async registration */
    bool bio_registered;
    bio_module_context_t bio_ctx;

    /* Enhancement 1: Interoception state */
    hypo_interoceptive_state_t interoceptive;
    float interoceptive_accuracy;
    bool interoception_enabled;

    /* Enhancement 2: Chemosensory state */
    hypo_chemosensory_state_t chemosensory;
    bool chemosensory_enabled;
    float hunger_smell_boost;
    float thirst_salt_boost;

    /* Enhancement 3: Predictive coding state */
    hypo_predictive_state_t predictive;
    bool predictive_coding_enabled;
    float prediction_precision_default;
    float free_energy_weight;

    /* Enhancement 4: Pain modulation state */
    hypo_pain_modulation_state_t pain;
    bool pain_modulation_enabled;
    float acute_stress_analgesia;
    float chronic_stress_hyperalgesia;

    /* Enhancement 5: Sleep gating state */
    hypo_sleep_gating_state_t sleep_gating;
    bool sleep_gating_enabled;
    float deep_sleep_gate;
    float rem_sleep_gate;

    /* Enhancement 6: Thermal state */
    hypo_thermal_state_t thermal;
    bool thermal_salience_enabled;
    float thermal_setpoint;
    float thermal_tolerance;

    /* Statistics */
    hypo_perception_bridge_stats_t stats;
    uint64_t creation_time_us;
};

/**
 * @brief Map stimulus category to corresponding drive
 */
static hypo_drive_type_t category_to_drive(hypo_stim_category_t category) {
    switch (category) {
        case HYPO_STIM_FOOD:    return HYPO_DRIVE_HUNGER;
        case HYPO_STIM_WATER:   return HYPO_DRIVE_THIRST;
        case HYPO_STIM_THREAT:  return HYPO_DRIVE_SAFETY;
        case HYPO_STIM_SOCIAL:  return HYPO_DRIVE_SOCIAL;
        case HYPO_STIM_NOVEL:   return HYPO_DRIVE_CURIOSITY;
        case HYPO_STIM_THERMAL: return HYPO_DRIVE_TEMPERATURE;
        case HYPO_STIM_PAIN:    return HYPO_DRIVE_SAFETY;
        case HYPO_STIM_REWARD:  return HYPO_DRIVE_COMPETENCE;
        default:                return HYPO_DRIVE_COUNT;  /* Invalid */
    }
}

/**
 * @brief Map drive to primary stimulus category
 */
static hypo_stim_category_t drive_to_category(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_HUNGER:      return HYPO_STIM_FOOD;
        case HYPO_DRIVE_THIRST:      return HYPO_STIM_WATER;
        case HYPO_DRIVE_SAFETY:      return HYPO_STIM_THREAT;
        case HYPO_DRIVE_SOCIAL:      return HYPO_STIM_SOCIAL;
        case HYPO_DRIVE_CURIOSITY:   return HYPO_STIM_NOVEL;
        case HYPO_DRIVE_TEMPERATURE: return HYPO_STIM_THERMAL;
        case HYPO_DRIVE_COMPETENCE:  return HYPO_STIM_REWARD;
        default:                     return HYPO_STIM_NEUTRAL;
    }
}

/**
 * @brief Map interoceptive signal to corresponding drive
 */
static hypo_drive_type_t intero_to_drive(hypo_interoceptive_type_t type) {
    switch (type) {
        case HYPO_INTERO_HUNGER_PANGS: return HYPO_DRIVE_HUNGER;
        case HYPO_INTERO_THIRST:       return HYPO_DRIVE_THIRST;
        case HYPO_INTERO_TEMPERATURE:  return HYPO_DRIVE_TEMPERATURE;
        case HYPO_INTERO_FATIGUE:      return HYPO_DRIVE_FATIGUE;
        case HYPO_INTERO_PAIN:         return HYPO_DRIVE_SAFETY;
        default:                       return HYPO_DRIVE_COUNT;
    }
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

void hypo_perception_bridge_default_config(hypo_perception_config_t* config) {
    if (!config) return;

    config->arousal_gain_min = 0.7f;
    config->arousal_gain_max = 1.5f;
    config->drive_salience_weight = 0.8f;
    config->threat_priority_threshold = 0.5f;
    config->visual_weight = 1.0f;
    config->auditory_weight = 1.0f;
    config->enable_anticipation = true;
    config->anticipation_decay = 0.05f;
}

void hypo_perception_bridge_default_enhanced_config(
    hypo_perception_enhanced_config_t* config)
{
    if (!config) return;

    /* Base config */
    hypo_perception_bridge_default_config(&config->base);

    /* Interoception defaults */
    config->enable_interoception = true;
    config->interoceptive_accuracy = 0.7f;

    /* Chemosensory defaults */
    config->enable_chemosensory = true;
    config->hunger_smell_boost = 0.5f;
    config->thirst_salt_boost = 0.4f;

    /* Predictive coding defaults */
    config->enable_predictive_coding = true;
    config->prediction_precision_default = 0.5f;
    config->free_energy_weight = 1.0f;

    /* Pain modulation defaults */
    config->enable_pain_modulation = true;
    config->acute_stress_analgesia = 0.5f;
    config->chronic_stress_hyperalgesia = 0.3f;

    /* Sleep gating defaults */
    config->enable_sleep_gating = true;
    config->deep_sleep_gate = 0.1f;
    config->rem_sleep_gate = 0.2f;

    /* Thermal defaults */
    config->enable_thermal_salience = true;
    config->thermal_setpoint = 0.5f;
    config->thermal_tolerance = 0.1f;
}

hypo_perception_bridge_t* hypo_perception_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_perception_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_perception_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_perception_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_perception_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_perception_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_perception") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_perception_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->drives = drives;
    bridge->creation_time_us = nimcp_time_get_us();

    if (config) {
        bridge->config = *config;
    } else {
        hypo_perception_bridge_default_config(&bridge->config);
    }

    /* Initialize modulation to baseline */
    bridge->modulation.global_gain = 1.0f;
    bridge->modulation.arousal_level = 0.5f;
    for (int i = 0; i < HYPO_SENSE_COUNT; i++) {
        bridge->modulation.modality_gains[i] = 1.0f;
    }
    for (int i = 0; i < HYPO_STIM_COUNT; i++) {
        bridge->modulation.category_salience[i] = 1.0f;
    }
    bridge->modulation.threat_priority = false;
    bridge->modulation.survival_mode = false;

    bridge->external_arousal = 0.5f;

    /* Initialize anticipation to zero */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->anticipation[i] = 0.0f;
    }

    bridge->has_recent_detection = false;
    bridge->bio_registered = false;
    bridge->bio_ctx = NULL;

    /* Enhancement 1: Interoception defaults */
    bridge->interoception_enabled = true;
    bridge->interoceptive_accuracy = 0.7f;
    bridge->interoceptive.global_interoceptive_accuracy = 0.7f;
    for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
        bridge->interoceptive.signals[i] = 0.0f;
        bridge->interoceptive.prediction_errors[i] = 0.0f;
        bridge->interoceptive.salience[i] = 0.0f;
    }

    /* Enhancement 2: Chemosensory defaults */
    bridge->chemosensory_enabled = true;
    bridge->hunger_smell_boost = 0.5f;
    bridge->thirst_salt_boost = 0.4f;
    for (int i = 0; i < HYPO_OLFACT_COUNT; i++) {
        bridge->chemosensory.olfactory_intensity[i] = 0.0f;
    }
    for (int i = 0; i < HYPO_GUST_COUNT; i++) {
        bridge->chemosensory.gustatory_intensity[i] = 0.0f;
    }
    bridge->chemosensory.hunger_modulation = 1.0f;
    bridge->chemosensory.thirst_modulation = 1.0f;
    bridge->chemosensory.food_detected = false;
    bridge->chemosensory.toxin_detected = false;

    /* Enhancement 3: Predictive coding defaults */
    bridge->predictive_coding_enabled = true;
    bridge->prediction_precision_default = 0.5f;
    bridge->free_energy_weight = 1.0f;
    for (int i = 0; i < HYPO_PRED_COUNT; i++) {
        bridge->predictive.predictions[i] = 0.5f;
        bridge->predictive.prediction_errors[i] = 0.0f;
        bridge->predictive.precision[i] = 0.5f;
    }
    bridge->predictive.free_energy = 0.0f;
    bridge->predictive.active_inference_enabled = true;

    /* Enhancement 4: Pain modulation defaults */
    bridge->pain_modulation_enabled = true;
    bridge->acute_stress_analgesia = 0.5f;
    bridge->chronic_stress_hyperalgesia = 0.3f;
    bridge->pain.pain_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->pain.stress_level = 0.0f;
    bridge->pain.analgesia_level = 0.0f;
    bridge->pain.hyperalgesia_level = 0.0f;
    bridge->pain.in_acute_stress = false;
    bridge->pain.in_chronic_stress = false;
    bridge->pain.endorphin_level = 0.0f;

    /* Enhancement 5: Sleep gating defaults */
    bridge->sleep_gating_enabled = true;
    bridge->deep_sleep_gate = 0.1f;
    bridge->rem_sleep_gate = 0.2f;
    bridge->sleep_gating.current_stage = HYPO_SLEEP_AWAKE;
    bridge->sleep_gating.perception_gate = 1.0f;
    bridge->sleep_gating.arousal_threshold = 0.0f;
    bridge->sleep_gating.threat_bypass = 0.9f;  /* Threats strongly bypass gating */
    bridge->sleep_gating.name_bypass = 0.85f;   /* Own name also strongly bypasses */
    bridge->sleep_gating.gating_enabled = true;
    bridge->sleep_gating.sleep_onset_us = 0;

    /* Enhancement 6: Thermal defaults */
    bridge->thermal_salience_enabled = true;
    bridge->thermal_setpoint = 0.5f;
    bridge->thermal_tolerance = 0.1f;
    bridge->thermal.core_temperature = NIMCP_TEMPERATURE_LOW;
    bridge->thermal.ambient_temperature = NIMCP_TEMPERATURE_LOW;
    bridge->thermal.thermal_discomfort = 0.0f;
    bridge->thermal.thermal_salience_boost = 1.0f;
    bridge->thermal.warm_seeking = 0.0f;
    bridge->thermal.cool_seeking = 0.0f;
    bridge->thermal.hypothermic_alert = false;
    bridge->thermal.hyperthermic_alert = false;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    nimcp_log(LOG_LEVEL_INFO, "hypo_perception_bridge: created successfully with enhancements");
    return bridge;
}

void hypo_perception_bridge_destroy(hypo_perception_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_perception");

    if (bridge->bio_registered) {
        hypo_perception_bridge_unregister_bio(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_perception_bridge: destroyed");
}

int hypo_perception_bridge_apply_enhanced_config(
    hypo_perception_bridge_t* bridge,
    const hypo_perception_enhanced_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_apply_enhanced_config: bridge or config is NULL");
        return -1;
    }

    bridge->config = config->base;

    /* Interoception */
    bridge->interoception_enabled = config->enable_interoception;
    bridge->interoceptive_accuracy = config->interoceptive_accuracy;
    bridge->interoceptive.global_interoceptive_accuracy = config->interoceptive_accuracy;

    /* Chemosensory */
    bridge->chemosensory_enabled = config->enable_chemosensory;
    bridge->hunger_smell_boost = config->hunger_smell_boost;
    bridge->thirst_salt_boost = config->thirst_salt_boost;

    /* Predictive coding */
    bridge->predictive_coding_enabled = config->enable_predictive_coding;
    bridge->prediction_precision_default = config->prediction_precision_default;
    bridge->free_energy_weight = config->free_energy_weight;

    /* Pain modulation */
    bridge->pain_modulation_enabled = config->enable_pain_modulation;
    bridge->acute_stress_analgesia = config->acute_stress_analgesia;
    bridge->chronic_stress_hyperalgesia = config->chronic_stress_hyperalgesia;

    /* Sleep gating */
    bridge->sleep_gating_enabled = config->enable_sleep_gating;
    bridge->deep_sleep_gate = config->deep_sleep_gate;
    bridge->rem_sleep_gate = config->rem_sleep_gate;

    /* Thermal */
    bridge->thermal_salience_enabled = config->enable_thermal_salience;
    bridge->thermal_setpoint = config->thermal_setpoint;
    bridge->thermal_tolerance = config->thermal_tolerance;

    nimcp_log(LOG_LEVEL_DEBUG, "hypo_perception_bridge: applied enhanced config");
    return 0;
}

/*=============================================================================
 * DRIVE → PERCEPTION MODULATION
 *===========================================================================*/

int hypo_perception_bridge_compute_modulation(hypo_perception_bridge_t* bridge) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_compute_modulation: bridge or drives is NULL");
        return -1;
    }

    const hypo_perception_config_t* cfg = &bridge->config;
    hypo_perception_modulation_t* mod = &bridge->modulation;

    /* Get current drive states */
    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_compute_modulation: hypo_drive_get_system_state is NULL");
        return -1;
    }

    /*
     * AROUSAL → GLOBAL SENSORY GAIN:
     *
     * Arousal (from LC norepinephrine) increases overall sensory responsiveness.
     * Low arousal = reduced sensitivity (drowsy state)
     * High arousal = heightened sensitivity (alert state)
     */
    float arousal = nimcp_clamp01(bridge->external_arousal);
    mod->arousal_level = arousal;

    /* Linear interpolation between min and max gain */
    mod->global_gain = cfg->arousal_gain_min +
                       arousal * (cfg->arousal_gain_max - cfg->arousal_gain_min);

    /*
     * DRIVE URGENCY → CATEGORY SALIENCE:
     *
     * High hunger → food stimuli more salient
     * High thirst → water stimuli more salient
     * High safety → threat stimuli more salient (and prioritized)
     * etc.
     */

    /* Reset category salience to baseline */
    for (int i = 0; i < HYPO_STIM_COUNT; i++) {
        mod->category_salience[i] = 1.0f;
    }

    /* Boost salience for each drive's relevant category */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float urgency = drive_state.drives[d].urgency;
        hypo_stim_category_t cat = drive_to_category((hypo_drive_type_t)d);

        if (cat < HYPO_STIM_COUNT) {
            /* Salience boost proportional to urgency */
            float boost = 1.0f + urgency * cfg->drive_salience_weight;
            mod->category_salience[cat] = nimcp_clampf(boost, 1.0f, 2.0f);
        }
    }

    /* Safety drive special handling: threat priority */
    /* Consider both drive urgency and local anticipation from threat detections */
    float safety_urgency = drive_state.drives[HYPO_DRIVE_SAFETY].urgency;
    float safety_anticipation = bridge->anticipation[HYPO_DRIVE_SAFETY];
    float effective_safety = fmaxf(safety_urgency, safety_anticipation);
    mod->threat_priority = (effective_safety > cfg->threat_priority_threshold);

    /* Threat salience boosted by effective safety level */
    if (effective_safety > 0.3f) {
        mod->category_salience[HYPO_STIM_THREAT] = nimcp_clampf(
            1.0f + effective_safety * cfg->drive_salience_weight, 1.0f, 2.0f);
    }

    /* Pain stimuli also get boosted when safety is high */
    if (effective_safety > 0.3f) {
        mod->category_salience[HYPO_STIM_PAIN] = nimcp_clampf(
            1.0f + effective_safety * 1.5f, 1.0f, 2.5f);
    }

    /*
     * SURVIVAL MODE:
     *
     * When any critical drive is extremely urgent, enter survival mode
     * with maximum sensory gain for relevant stimuli.
     */
    bool survival = false;
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        if (drive_state.drives[d].urgency > 0.9f) {
            /* Critical drive = physiological or safety */
            if (d == HYPO_DRIVE_HUNGER || d == HYPO_DRIVE_THIRST ||
                d == HYPO_DRIVE_SAFETY || d == HYPO_DRIVE_TEMPERATURE) {
                survival = true;
                break;
            }
        }
    }
    mod->survival_mode = survival;

    if (survival) {
        /* Amplify global gain in survival mode */
        mod->global_gain = cfg->arousal_gain_max;
        nimcp_log(LOG_LEVEL_DEBUG,
            "hypo_perception_bridge: survival mode active, max sensory gain");
    }

    /*
     * MODALITY-SPECIFIC GAINS:
     *
     * Different modalities may be weighted differently based on context.
     * Visual and auditory are primary for most threat/food detection.
     */
    mod->modality_gains[HYPO_SENSE_VISUAL] = mod->global_gain * cfg->visual_weight;
    mod->modality_gains[HYPO_SENSE_AUDITORY] = mod->global_gain * cfg->auditory_weight;
    mod->modality_gains[HYPO_SENSE_SOMATOSENSORY] = mod->global_gain;
    mod->modality_gains[HYPO_SENSE_OLFACTORY] = mod->global_gain;
    mod->modality_gains[HYPO_SENSE_GUSTATORY] = mod->global_gain;

    /* Hunger boosts gustatory and olfactory */
    float hunger_urgency = drive_state.drives[HYPO_DRIVE_HUNGER].urgency;
    if (hunger_urgency > 0.3f) {
        mod->modality_gains[HYPO_SENSE_GUSTATORY] *= (1.0f + hunger_urgency * 0.5f);
        mod->modality_gains[HYPO_SENSE_OLFACTORY] *= (1.0f + hunger_urgency * 0.3f);
    }

    /* Update chemosensory modulation from drives */
    if (bridge->chemosensory_enabled) {
        bridge->chemosensory.hunger_modulation = 1.0f + hunger_urgency * bridge->hunger_smell_boost;
        float thirst_urgency = drive_state.drives[HYPO_DRIVE_THIRST].urgency;
        bridge->chemosensory.thirst_modulation = 1.0f + thirst_urgency * bridge->thirst_salt_boost;
    }

    /* Decay anticipation over time */
    if (cfg->enable_anticipation) {
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            bridge->anticipation[i] *= (1.0f - cfg->anticipation_decay);
        }
    }

    mod->timestamp_us = nimcp_time_get_us();
    bridge->stats.modulations_computed++;

    return 0;
}

int hypo_perception_bridge_get_modulation(
    const hypo_perception_bridge_t* bridge,
    hypo_perception_modulation_t* modulation)
{
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_modulation: bridge or modulation is NULL");
        return -1;
    }

    *modulation = bridge->modulation;
    return 0;
}

int hypo_perception_bridge_set_arousal(
    hypo_perception_bridge_t* bridge,
    float arousal)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->external_arousal = nimcp_clamp01(arousal);
    return 0;
}

/*=============================================================================
 * PERCEPTION → DRIVE FEEDBACK
 *===========================================================================*/

int hypo_perception_bridge_process_detection(
    hypo_perception_bridge_t* bridge,
    const hypo_sensory_detection_t* detection)
{
    if (!bridge || !detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_process_detection: bridge or detection is NULL");
        return -1;
    }

    bridge->last_detection = *detection;
    bridge->has_recent_detection = true;
    bridge->stats.detections_processed++;

    /*
     * SENSORY DETECTION → DRIVE ANTICIPATION:
     *
     * Seeing food increases hunger anticipation (wanting)
     * Detecting threat boosts safety drive (fear response)
     * etc.
     */

    if (!bridge->config.enable_anticipation) {
        return 0;
    }

    hypo_drive_type_t relevant_drive = category_to_drive(detection->detected_category);
    if (relevant_drive >= HYPO_DRIVE_COUNT) {
        return 0;  /* No relevant drive for this category */
    }

    /* Boost anticipation based on detection confidence and intensity */
    float anticipation_boost = detection->confidence * detection->intensity * 0.5f;
    bridge->anticipation[relevant_drive] = nimcp_clamp01(
        bridge->anticipation[relevant_drive] + anticipation_boost
    );

    /* Threat detection gets immediate safety drive boost */
    if (detection->is_threat) {
        bridge->anticipation[HYPO_DRIVE_SAFETY] = nimcp_clamp01(
            bridge->anticipation[HYPO_DRIVE_SAFETY] + 0.3f
        );
        nimcp_log(LOG_LEVEL_DEBUG,
            "hypo_perception_bridge: threat detected, safety anticipation boosted");
    }

    return 0;
}

float hypo_perception_bridge_get_anticipation(
    const hypo_perception_bridge_t* bridge,
    hypo_drive_type_t drive_type)
{
    if (!bridge || drive_type >= HYPO_DRIVE_COUNT) {
        return 0.0f;
    }

    return bridge->anticipation[drive_type];
}

/*=============================================================================
 * ENHANCEMENT 1: INTEROCEPTION
 *===========================================================================*/

int hypo_perception_bridge_process_interoceptive(
    hypo_perception_bridge_t* bridge,
    hypo_interoceptive_type_t signal_type,
    float intensity)
{
    if (!bridge || signal_type >= HYPO_INTERO_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_process_interoceptive: bridge is NULL or invalid signal_type");
        return -1;
    }
    if (!bridge->interoception_enabled) return 0;

    float clamped_intensity = nimcp_clamp01(intensity);

    /* Calculate prediction error (deviation from expected) */
    float expected = bridge->interoceptive.signals[signal_type];
    float prediction_error = fabsf(clamped_intensity - expected);

    /* Update signal */
    bridge->interoceptive.signals[signal_type] = clamped_intensity;
    bridge->interoceptive.prediction_errors[signal_type] = prediction_error;

    /* Compute salience based on intensity and prediction error */
    float salience = clamped_intensity * 0.5f + prediction_error * 0.5f;
    salience *= bridge->interoceptive_accuracy;
    bridge->interoceptive.salience[signal_type] = nimcp_clamp01(salience);

    bridge->interoceptive.timestamp_us = nimcp_time_get_us();
    bridge->stats.interoceptive_signals++;

    /* Map interoceptive signal to drive if applicable */
    hypo_drive_type_t drive = intero_to_drive(signal_type);
    if (drive < HYPO_DRIVE_COUNT && clamped_intensity > 0.5f) {
        /* Strong interoceptive signal boosts drive anticipation */
        bridge->anticipation[drive] = nimcp_clamp01(
            bridge->anticipation[drive] + clamped_intensity * 0.2f
        );
    }

    return 0;
}

int hypo_perception_bridge_get_interoceptive_state(
    const hypo_perception_bridge_t* bridge,
    hypo_interoceptive_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_interoceptive_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->interoceptive;
    return 0;
}

int hypo_perception_bridge_set_interoceptive_accuracy(
    hypo_perception_bridge_t* bridge,
    float accuracy)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->interoceptive_accuracy = nimcp_clamp01(accuracy);
    bridge->interoceptive.global_interoceptive_accuracy = bridge->interoceptive_accuracy;
    return 0;
}

/*=============================================================================
 * ENHANCEMENT 2: OLFACTORY/GUSTATORY
 *===========================================================================*/

int hypo_perception_bridge_process_olfactory(
    hypo_perception_bridge_t* bridge,
    hypo_olfactory_type_t odor_type,
    float intensity)
{
    if (!bridge || odor_type >= HYPO_OLFACT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_process_olfactory: bridge is NULL or invalid odor_type");
        return -1;
    }
    if (!bridge->chemosensory_enabled) return 0;

    float clamped_intensity = nimcp_clamp01(intensity);

    /* Apply hunger modulation to food odors */
    float modulated_intensity = clamped_intensity;
    if (odor_type == HYPO_OLFACT_FOOD_PLEASANT) {
        modulated_intensity *= bridge->chemosensory.hunger_modulation;
        modulated_intensity = nimcp_clamp01(modulated_intensity);
        if (clamped_intensity > 0.3f) {
            bridge->chemosensory.food_detected = true;
        }
    } else if (odor_type == HYPO_OLFACT_FOOD_UNPLEASANT) {
        if (clamped_intensity > 0.5f) {
            bridge->chemosensory.toxin_detected = true;
        }
    } else if (odor_type == HYPO_OLFACT_DANGER) {
        /* Danger odors boost safety anticipation */
        if (clamped_intensity > 0.3f) {
            bridge->anticipation[HYPO_DRIVE_SAFETY] = nimcp_clamp01(
                bridge->anticipation[HYPO_DRIVE_SAFETY] + clamped_intensity * 0.3f
            );
        }
    }

    bridge->chemosensory.olfactory_intensity[odor_type] = modulated_intensity;
    bridge->chemosensory.timestamp_us = nimcp_time_get_us();
    bridge->stats.olfactory_stimuli++;

    return 0;
}

int hypo_perception_bridge_process_gustatory(
    hypo_perception_bridge_t* bridge,
    hypo_gustatory_type_t taste_type,
    float intensity)
{
    if (!bridge || taste_type >= HYPO_GUST_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_process_gustatory: bridge is NULL or invalid taste_type");
        return -1;
    }
    if (!bridge->chemosensory_enabled) return 0;

    float clamped_intensity = nimcp_clamp01(intensity);

    /* Apply drive modulation to specific tastes */
    float modulated_intensity = clamped_intensity;

    switch (taste_type) {
        case HYPO_GUST_SWEET:
        case HYPO_GUST_UMAMI:
        case HYPO_GUST_FAT:
            /* Hunger boosts rewarding tastes */
            modulated_intensity *= bridge->chemosensory.hunger_modulation;
            modulated_intensity = nimcp_clamp01(modulated_intensity);
            if (clamped_intensity > 0.3f) {
                bridge->chemosensory.food_detected = true;
            }
            break;

        case HYPO_GUST_SALTY:
            /* Thirst boosts salt taste */
            modulated_intensity *= bridge->chemosensory.thirst_modulation;
            modulated_intensity = nimcp_clamp01(modulated_intensity);
            break;

        case HYPO_GUST_BITTER:
            /* Bitter = potential toxin */
            if (clamped_intensity > 0.4f) {
                bridge->chemosensory.toxin_detected = true;
            }
            break;

        default:
            break;
    }

    bridge->chemosensory.gustatory_intensity[taste_type] = modulated_intensity;
    bridge->chemosensory.timestamp_us = nimcp_time_get_us();
    bridge->stats.gustatory_stimuli++;

    return 0;
}

int hypo_perception_bridge_get_chemosensory_state(
    const hypo_perception_bridge_t* bridge,
    hypo_chemosensory_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_chemosensory_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->chemosensory;
    return 0;
}

/*=============================================================================
 * ENHANCEMENT 3: PREDICTIVE CODING
 *===========================================================================*/

int hypo_perception_bridge_generate_prediction(
    hypo_perception_bridge_t* bridge,
    hypo_prediction_type_t pred_type,
    float probability,
    float precision)
{
    if (!bridge || pred_type >= HYPO_PRED_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_generate_prediction: bridge is NULL or invalid pred_type");
        return -1;
    }
    if (!bridge->predictive_coding_enabled) return 0;

    bridge->predictive.predictions[pred_type] = nimcp_clamp01(probability);
    bridge->predictive.precision[pred_type] = nimcp_clamp01(precision);
    bridge->predictive.timestamp_us = nimcp_time_get_us();
    bridge->stats.predictions_generated++;

    return 0;
}

int hypo_perception_bridge_update_prediction_error(
    hypo_perception_bridge_t* bridge,
    hypo_prediction_type_t pred_type,
    float actual_value)
{
    if (!bridge || pred_type >= HYPO_PRED_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_update_prediction_error: bridge is NULL or invalid pred_type");
        return -1;
    }
    if (!bridge->predictive_coding_enabled) return 0;

    float clamped_actual = nimcp_clamp01(actual_value);
    float predicted = bridge->predictive.predictions[pred_type];
    float precision = bridge->predictive.precision[pred_type];

    /* Precision-weighted prediction error */
    float raw_error = clamped_actual - predicted;
    float weighted_error = raw_error * precision;

    bridge->predictive.prediction_errors[pred_type] = weighted_error;
    bridge->predictive.timestamp_us = nimcp_time_get_us();
    bridge->stats.prediction_errors_updated++;

    /* Recompute free energy */
    float fe;
    hypo_perception_bridge_compute_free_energy(bridge, &fe);

    return 0;
}

int hypo_perception_bridge_get_predictive_state(
    const hypo_perception_bridge_t* bridge,
    hypo_predictive_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_predictive_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->predictive;
    return 0;
}

int hypo_perception_bridge_compute_free_energy(
    hypo_perception_bridge_t* bridge,
    float* free_energy)
{
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_compute_free_energy: bridge or free_energy is NULL");
        return -1;
    }

    /* Free energy = sum of precision-weighted squared prediction errors */
    float fe = 0.0f;
    for (int i = 0; i < HYPO_PRED_COUNT; i++) {
        float pe = bridge->predictive.prediction_errors[i];
        float precision = bridge->predictive.precision[i];
        fe += precision * pe * pe;
    }

    fe *= bridge->free_energy_weight;
    bridge->predictive.free_energy = fe;
    *free_energy = fe;

    /* Update average in stats */
    float prev_avg = bridge->stats.avg_free_energy;
    float count = (float)(bridge->stats.predictions_generated + 1);
    if (count > 0.0f) {
        float new_avg_fe = prev_avg + (fe - prev_avg) / count;
        if (isfinite(new_avg_fe)) {
            bridge->stats.avg_free_energy = new_avg_fe;
        }
    }

    return 0;
}

/*=============================================================================
 * ENHANCEMENT 4: PAIN MODULATION
 *===========================================================================*/

int hypo_perception_bridge_set_stress_for_pain(
    hypo_perception_bridge_t* bridge,
    float stress_level,
    bool is_chronic)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->pain_modulation_enabled) return 0;

    float clamped_stress = nimcp_clamp01(stress_level);
    bridge->pain.stress_level = clamped_stress;
    bridge->pain.in_chronic_stress = is_chronic;
    bridge->pain.in_acute_stress = !is_chronic && (clamped_stress > 0.3f);

    if (bridge->pain.in_acute_stress) {
        /* Acute stress → stress-induced analgesia */
        bridge->pain.analgesia_level = clamped_stress * bridge->acute_stress_analgesia;
        bridge->pain.hyperalgesia_level = 0.0f;
        bridge->pain.pain_sensitivity = nimcp_clampf(
            1.0f - bridge->pain.analgesia_level, 0.5f, 1.0f);
    } else if (bridge->pain.in_chronic_stress) {
        /* Chronic stress → hyperalgesia */
        bridge->pain.hyperalgesia_level = clamped_stress * bridge->chronic_stress_hyperalgesia;
        bridge->pain.analgesia_level = 0.0f;
        bridge->pain.pain_sensitivity = nimcp_clampf(
            1.0f + bridge->pain.hyperalgesia_level, 1.0f, 2.0f);
    } else {
        bridge->pain.analgesia_level = 0.0f;
        bridge->pain.hyperalgesia_level = 0.0f;
        bridge->pain.pain_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    }

    bridge->pain.timestamp_us = nimcp_time_get_us();
    return 0;
}

int hypo_perception_bridge_modulate_pain(
    hypo_perception_bridge_t* bridge,
    float raw_pain,
    float* modulated_pain)
{
    if (!bridge || !modulated_pain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_modulate_pain: bridge or modulated_pain is NULL");
        return -1;
    }

    float clamped_raw = nimcp_clamp01(raw_pain);

    if (!bridge->pain_modulation_enabled) {
        *modulated_pain = clamped_raw;
        return 0;
    }

    /* Apply pain sensitivity modifier */
    float modulated = clamped_raw * bridge->pain.pain_sensitivity;

    /* Apply endorphin-based analgesia */
    modulated *= (1.0f - bridge->pain.endorphin_level * 0.5f);

    *modulated_pain = nimcp_clamp01(modulated);
    bridge->stats.pain_stimuli_modulated++;

    /* Update average in stats */
    float ratio = clamped_raw > 0.001f ? (*modulated_pain / clamped_raw) : 1.0f;
    float prev_avg_pain = bridge->stats.avg_pain_modulation;
    float pain_count = (float)bridge->stats.pain_stimuli_modulated;
    if (pain_count > 0.0f) {
        float new_avg_pain = prev_avg_pain + (ratio - prev_avg_pain) / pain_count;
        if (isfinite(new_avg_pain)) {
            bridge->stats.avg_pain_modulation = new_avg_pain;
        }
    }

    return 0;
}

int hypo_perception_bridge_get_pain_state(
    const hypo_perception_bridge_t* bridge,
    hypo_pain_modulation_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_pain_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->pain;
    return 0;
}

int hypo_perception_bridge_release_endorphins(
    hypo_perception_bridge_t* bridge,
    float amount)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    float new_level = bridge->pain.endorphin_level + nimcp_clamp01(amount);
    bridge->pain.endorphin_level = nimcp_clamp01(new_level);
    bridge->pain.timestamp_us = nimcp_time_get_us();

    return 0;
}

/*=============================================================================
 * ENHANCEMENT 5: SLEEP GATING
 *===========================================================================*/

int hypo_perception_bridge_set_sleep_stage(
    hypo_perception_bridge_t* bridge,
    hypo_sleep_stage_t stage)
{
    if (!bridge || stage >= HYPO_SLEEP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_perception_bridge_set_sleep_stage: bridge is NULL or invalid stage");
        return -1;
    }

    hypo_sleep_stage_t prev_stage = bridge->sleep_gating.current_stage;
    bridge->sleep_gating.current_stage = stage;

    /* Update perception gate based on sleep stage
     *
     * Gate: How much sensory input gets through (1.0 = full, 0.0 = none)
     * Threshold: Minimum gated intensity needed to trigger perception
     *
     * The relationship: gated_intensity = raw_intensity * gate
     * Passes if: gated_intensity >= threshold
     */
    switch (stage) {
        case HYPO_SLEEP_AWAKE:
            bridge->sleep_gating.perception_gate = 1.0f;
            bridge->sleep_gating.arousal_threshold = 0.0f;
            break;

        case HYPO_SLEEP_DROWSY:
            /* Still quite alert, most stimuli should pass */
            bridge->sleep_gating.perception_gate = 0.8f;
            bridge->sleep_gating.arousal_threshold = 0.1f;
            break;

        case HYPO_SLEEP_LIGHT:
            /* Only stronger stimuli wake you */
            bridge->sleep_gating.perception_gate = 0.5f;
            bridge->sleep_gating.arousal_threshold = 0.3f;
            break;

        case HYPO_SLEEP_DEEP:
            /* Very hard to wake, only threat/name bypass */
            bridge->sleep_gating.perception_gate = bridge->deep_sleep_gate;
            bridge->sleep_gating.arousal_threshold = 0.5f;
            break;

        case HYPO_SLEEP_REM:
            /* Similar to light but with some internal gating */
            bridge->sleep_gating.perception_gate = bridge->rem_sleep_gate;
            bridge->sleep_gating.arousal_threshold = 0.4f;
            break;

        default:
            break;
    }

    /* Track sleep onset */
    if (prev_stage == HYPO_SLEEP_AWAKE && stage != HYPO_SLEEP_AWAKE) {
        bridge->sleep_gating.sleep_onset_us = nimcp_time_get_us();
    } else if (stage == HYPO_SLEEP_AWAKE) {
        bridge->sleep_gating.sleep_onset_us = 0;
    }

    bridge->sleep_gating.timestamp_us = nimcp_time_get_us();
    return 0;
}

int hypo_perception_bridge_check_sleep_gate(
    hypo_perception_bridge_t* bridge,
    float stimulus_intensity,
    bool is_threat,
    bool is_name,
    bool* passes)
{
    if (!bridge || !passes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_check_sleep_gate: bridge or passes is NULL");
        return -1;
    }

    bridge->stats.sleep_gate_checks++;

    if (!bridge->sleep_gating_enabled ||
        bridge->sleep_gating.current_stage == HYPO_SLEEP_AWAKE) {
        *passes = true;
        bridge->stats.sleep_gate_passes++;
        return 0;
    }

    float clamped_intensity = nimcp_clamp01(stimulus_intensity);
    float threshold = bridge->sleep_gating.arousal_threshold;

    /* Threat stimuli have lower threshold (bypass) */
    if (is_threat) {
        threshold *= (1.0f - bridge->sleep_gating.threat_bypass);
    }

    /* Own name has special bypass */
    if (is_name) {
        threshold *= (1.0f - bridge->sleep_gating.name_bypass);
    }

    /* Apply gate attenuation to stimulus */
    float gated_intensity = clamped_intensity * bridge->sleep_gating.perception_gate;

    /* Add special handling for threat/name */
    if (is_threat || is_name) {
        gated_intensity += clamped_intensity * 0.3f;  /* Partial bypass */
    }

    *passes = (gated_intensity >= threshold);

    if (*passes) {
        bridge->stats.sleep_gate_passes++;
    }

    return 0;
}

int hypo_perception_bridge_get_sleep_gating_state(
    const hypo_perception_bridge_t* bridge,
    hypo_sleep_gating_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_sleep_gating_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->sleep_gating;
    return 0;
}

/*=============================================================================
 * ENHANCEMENT 6: THERMAL SALIENCE
 *===========================================================================*/

int hypo_perception_bridge_set_core_temperature(
    hypo_perception_bridge_t* bridge,
    float temperature)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->thermal.core_temperature = nimcp_clamp01(temperature);

    /* Compute thermal discomfort */
    float deviation = fabsf(temperature - bridge->thermal_setpoint);
    bridge->thermal.thermal_discomfort = nimcp_clamp01(deviation / bridge->thermal_tolerance);

    /* Compute seeking motivations */
    if (temperature < bridge->thermal_setpoint - bridge->thermal_tolerance) {
        bridge->thermal.warm_seeking = bridge->thermal.thermal_discomfort;
        bridge->thermal.cool_seeking = 0.0f;
        bridge->thermal.hypothermic_alert = (temperature < 0.3f);
        bridge->thermal.hyperthermic_alert = false;  /* Clear opposite alert */
    } else if (temperature > bridge->thermal_setpoint + bridge->thermal_tolerance) {
        bridge->thermal.cool_seeking = bridge->thermal.thermal_discomfort;
        bridge->thermal.warm_seeking = 0.0f;
        bridge->thermal.hyperthermic_alert = (temperature > 0.7f);
        bridge->thermal.hypothermic_alert = false;  /* Clear opposite alert */
    } else {
        bridge->thermal.warm_seeking = 0.0f;
        bridge->thermal.cool_seeking = 0.0f;
        bridge->thermal.hypothermic_alert = false;
        bridge->thermal.hyperthermic_alert = false;
    }

    bridge->thermal.timestamp_us = nimcp_time_get_us();
    bridge->stats.thermal_updates++;

    return 0;
}

int hypo_perception_bridge_set_ambient_temperature(
    hypo_perception_bridge_t* bridge,
    float temperature)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->thermal.ambient_temperature = nimcp_clamp01(temperature);
    bridge->thermal.timestamp_us = nimcp_time_get_us();

    return 0;
}

int hypo_perception_bridge_get_thermal_state(
    const hypo_perception_bridge_t* bridge,
    hypo_thermal_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_thermal_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->thermal;
    return 0;
}

int hypo_perception_bridge_compute_thermal_salience(
    hypo_perception_bridge_t* bridge,
    float* boost)
{
    if (!bridge || !boost) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_compute_thermal_salience: bridge or boost is NULL");
        return -1;
    }

    if (!bridge->thermal_salience_enabled) {
        *boost = 1.0f;
        return 0;
    }

    /* Thermal salience boost = 1.0 to 3.0 based on discomfort */
    float salience = 1.0f + bridge->thermal.thermal_discomfort * 2.0f;
    bridge->thermal.thermal_salience_boost = nimcp_clampf(salience, 1.0f, 3.0f);
    *boost = bridge->thermal.thermal_salience_boost;

    return 0;
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

int hypo_perception_bridge_get_stats(
    const hypo_perception_bridge_t* bridge,
    hypo_perception_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    stats->uptime_us = nimcp_time_get_us() - bridge->creation_time_us;
    return 0;
}

int hypo_perception_bridge_reset_stats(hypo_perception_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

void hypo_perception_bridge_print_summary(const hypo_perception_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_log(LOG_LEVEL_INFO, "=== Hypothalamus Perception Bridge Summary ===");
    nimcp_log(LOG_LEVEL_INFO, "Global gain: %.2f, Arousal: %.2f",
              bridge->modulation.global_gain, bridge->modulation.arousal_level);
    nimcp_log(LOG_LEVEL_INFO, "Threat priority: %s, Survival mode: %s",
              bridge->modulation.threat_priority ? "yes" : "no",
              bridge->modulation.survival_mode ? "yes" : "no");

    if (bridge->interoception_enabled) {
        nimcp_log(LOG_LEVEL_INFO, "Interoception: accuracy=%.2f",
                  bridge->interoceptive_accuracy);
    }

    if (bridge->predictive_coding_enabled) {
        nimcp_log(LOG_LEVEL_INFO, "Predictive coding: FE=%.3f",
                  bridge->predictive.free_energy);
    }

    if (bridge->pain_modulation_enabled) {
        nimcp_log(LOG_LEVEL_INFO, "Pain: sensitivity=%.2f, stress=%.2f, endorphins=%.2f",
                  bridge->pain.pain_sensitivity, bridge->pain.stress_level,
                  bridge->pain.endorphin_level);
    }

    if (bridge->sleep_gating_enabled) {
        nimcp_log(LOG_LEVEL_INFO, "Sleep: stage=%s, gate=%.2f",
                  hypo_sleep_stage_name(bridge->sleep_gating.current_stage),
                  bridge->sleep_gating.perception_gate);
    }

    if (bridge->thermal_salience_enabled) {
        nimcp_log(LOG_LEVEL_INFO, "Thermal: core=%.2f, discomfort=%.2f, salience_boost=%.2f",
                  bridge->thermal.core_temperature, bridge->thermal.thermal_discomfort,
                  bridge->thermal.thermal_salience_boost);
    }

    nimcp_log(LOG_LEVEL_INFO, "Stats: modulations=%u, detections=%u, intero=%u, olfact=%u, gust=%u",
              bridge->stats.modulations_computed, bridge->stats.detections_processed,
              bridge->stats.interoceptive_signals, bridge->stats.olfactory_stimuli,
              bridge->stats.gustatory_stimuli);
}

int hypo_perception_bridge_reset(hypo_perception_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Reset modulation */
    bridge->modulation.global_gain = 1.0f;
    bridge->modulation.arousal_level = 0.5f;
    for (int i = 0; i < HYPO_SENSE_COUNT; i++) {
        bridge->modulation.modality_gains[i] = 1.0f;
    }
    for (int i = 0; i < HYPO_STIM_COUNT; i++) {
        bridge->modulation.category_salience[i] = 1.0f;
    }
    bridge->modulation.threat_priority = false;
    bridge->modulation.survival_mode = false;

    bridge->external_arousal = 0.5f;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->anticipation[i] = 0.0f;
    }

    bridge->has_recent_detection = false;

    /* Reset interoception */
    for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
        bridge->interoceptive.signals[i] = 0.0f;
        bridge->interoceptive.prediction_errors[i] = 0.0f;
        bridge->interoceptive.salience[i] = 0.0f;
    }

    /* Reset chemosensory */
    for (int i = 0; i < HYPO_OLFACT_COUNT; i++) {
        bridge->chemosensory.olfactory_intensity[i] = 0.0f;
    }
    for (int i = 0; i < HYPO_GUST_COUNT; i++) {
        bridge->chemosensory.gustatory_intensity[i] = 0.0f;
    }
    bridge->chemosensory.food_detected = false;
    bridge->chemosensory.toxin_detected = false;

    /* Reset predictive */
    for (int i = 0; i < HYPO_PRED_COUNT; i++) {
        bridge->predictive.predictions[i] = 0.5f;
        bridge->predictive.prediction_errors[i] = 0.0f;
        bridge->predictive.precision[i] = 0.5f;
    }
    bridge->predictive.free_energy = 0.0f;

    /* Reset pain */
    bridge->pain.pain_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->pain.stress_level = 0.0f;
    bridge->pain.analgesia_level = 0.0f;
    bridge->pain.hyperalgesia_level = 0.0f;
    bridge->pain.in_acute_stress = false;
    bridge->pain.in_chronic_stress = false;
    bridge->pain.endorphin_level = 0.0f;

    /* Reset sleep gating */
    bridge->sleep_gating.current_stage = HYPO_SLEEP_AWAKE;
    bridge->sleep_gating.perception_gate = 1.0f;
    bridge->sleep_gating.arousal_threshold = 0.0f;
    bridge->sleep_gating.sleep_onset_us = 0;

    /* Reset thermal */
    bridge->thermal.core_temperature = NIMCP_TEMPERATURE_LOW;
    bridge->thermal.ambient_temperature = NIMCP_TEMPERATURE_LOW;
    bridge->thermal.thermal_discomfort = 0.0f;
    bridge->thermal.thermal_salience_boost = 1.0f;
    bridge->thermal.warm_seeking = 0.0f;
    bridge->thermal.cool_seeking = 0.0f;
    bridge->thermal.hypothermic_alert = false;
    bridge->thermal.hyperthermic_alert = false;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

int hypo_perception_bridge_update(
    hypo_perception_bridge_t* bridge,
    uint64_t delta_us)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    (void)delta_us;  /* May use for time-based decay in future */

    /* Decay endorphins over time */
    if (bridge->pain.endorphin_level > 0.0f) {
        float decay = 0.001f * (delta_us / 1000.0f);  /* Slow decay */
        bridge->pain.endorphin_level = nimcp_clamp01(
            bridge->pain.endorphin_level - decay);
    }

    /* Recompute modulation */
    hypo_perception_bridge_compute_modulation(bridge);

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t perception_handle_arousal_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_perception_bridge_t* bridge = (hypo_perception_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(float)) {
        const float* arousal = (const float*)(header + 1);
        hypo_perception_bridge_set_arousal(bridge, *arousal);
        hypo_perception_bridge_compute_modulation(bridge);
        hypo_perception_bridge_broadcast_modulation(bridge);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t perception_handle_detection(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_perception_bridge_t* bridge = (hypo_perception_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    if (msg_size >= sizeof(bio_message_header_t) + sizeof(hypo_sensory_detection_t)) {
        const hypo_sensory_detection_t* detection =
            (const hypo_sensory_detection_t*)(header + 1);
        hypo_perception_bridge_process_detection(bridge, detection);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t perception_handle_modulation_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    (void)msg;
    (void)msg_size;
    hypo_perception_bridge_t* bridge = (hypo_perception_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_bridge_broadcast_modulation(bridge);

    return NIMCP_SUCCESS;
}

bool hypo_perception_bridge_register_bio(hypo_perception_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;  /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_PERCEPTION_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_perception_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_perception_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_perception_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for perception messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_PERCEPTION_AROUSAL_UPDATE, perception_handle_arousal_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_PERCEPTION_DETECTION, perception_handle_detection);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_HYPO_PERCEPTION_MODULATION_REQUEST, perception_handle_modulation_request);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_perception_bridge: registered with bio-router");
    return true;
}

void hypo_perception_bridge_unregister_bio(hypo_perception_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_perception_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_perception_bridge_broadcast_modulation(hypo_perception_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        hypo_perception_modulation_t modulation;
    } msg;

    msg.header.type = BIO_MSG_HYPO_PERCEPTION_MODULATION_OUTPUT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_PERCEPTION_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.sequence_id = 0;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Arousal channel */

    msg.modulation = bridge->modulation;

    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * STRING UTILITIES
 *===========================================================================*/

const char* hypo_interoceptive_type_name(hypo_interoceptive_type_t type) {
    switch (type) {
        case HYPO_INTERO_HUNGER_PANGS: return "hunger_pangs";
        case HYPO_INTERO_HEART_RATE:   return "heart_rate";
        case HYPO_INTERO_BREATHING:    return "breathing";
        case HYPO_INTERO_TEMPERATURE:  return "temperature";
        case HYPO_INTERO_FATIGUE:      return "fatigue";
        case HYPO_INTERO_PAIN:         return "pain";
        case HYPO_INTERO_THIRST:       return "thirst";
        case HYPO_INTERO_BLADDER:      return "bladder";
        default:                       return "unknown";
    }
}

const char* hypo_olfactory_type_name(hypo_olfactory_type_t type) {
    switch (type) {
        case HYPO_OLFACT_FOOD_PLEASANT:   return "food_pleasant";
        case HYPO_OLFACT_FOOD_UNPLEASANT: return "food_unpleasant";
        case HYPO_OLFACT_PHEROMONE_SOCIAL: return "pheromone_social";
        case HYPO_OLFACT_DANGER:          return "danger";
        case HYPO_OLFACT_NEUTRAL:         return "neutral";
        default:                          return "unknown";
    }
}

const char* hypo_gustatory_type_name(hypo_gustatory_type_t type) {
    switch (type) {
        case HYPO_GUST_SWEET:  return "sweet";
        case HYPO_GUST_SALTY:  return "salty";
        case HYPO_GUST_SOUR:   return "sour";
        case HYPO_GUST_BITTER: return "bitter";
        case HYPO_GUST_UMAMI:  return "umami";
        case HYPO_GUST_FAT:    return "fat";
        default:              return "unknown";
    }
}

const char* hypo_prediction_type_name(hypo_prediction_type_t type) {
    switch (type) {
        case HYPO_PRED_FOOD_PRESENCE:      return "food_presence";
        case HYPO_PRED_THREAT_PRESENCE:    return "threat_presence";
        case HYPO_PRED_SOCIAL_INTERACTION: return "social_interaction";
        case HYPO_PRED_TEMPERATURE_CHANGE: return "temperature_change";
        case HYPO_PRED_REWARD_AVAILABLE:   return "reward_available";
        default:                           return "unknown";
    }
}

const char* hypo_sleep_stage_name(hypo_sleep_stage_t stage) {
    switch (stage) {
        case HYPO_SLEEP_AWAKE:  return "awake";
        case HYPO_SLEEP_DROWSY: return "drowsy";
        case HYPO_SLEEP_LIGHT:  return "light";
        case HYPO_SLEEP_DEEP:   return "deep";
        case HYPO_SLEEP_REM:    return "rem";
        default:               return "unknown";
    }
}
