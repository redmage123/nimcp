/**
 * @file nimcp_amygdala.c
 * @brief Amygdala implementation for emotion processing and fear conditioning
 *
 * WHAT: Biologically-inspired amygdala with nuclei-specific processing
 * WHY:  Enable emotion-driven learning, threat detection, and fear responses
 * HOW:  Implements lateral, basal, central nuclei with fear conditioning circuits
 *
 * Integration with Emotional System:
 * - Amygdala fear/anxiety drives emotional system valence/arousal
 * - Emotional system regulation provides top-down inhibition
 * - Bidirectional synchronization on update
 */

#include "core/brain/subcortical/nimcp_amygdala.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/nimcp_emotional_system.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(amygdala, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cosine similarity between feature vectors
 */
static float compute_cosine_similarity(const float* a, const float* b, uint32_t n) {
    if (!a || !b || n == 0) return 0.0f;

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a < AMYG_SIMILARITY_EPSILON || norm_b < AMYG_SIMILARITY_EPSILON) return 0.0f;

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/**
 * @brief Initialize a single nucleus
 */
static void init_nucleus(amyg_nucleus_t* nucleus, amyg_nucleus_type_t type) {
    if (!nucleus) return;

    nucleus->type = type;
    nucleus->activation = 0.0f;
    nucleus->baseline = AMYG_NUCLEUS_BASELINE;
    nucleus->gain = AMYG_NUCLEUS_GAIN;
    nucleus->plasticity_rate = AMYG_NUCLEUS_PLASTICITY_RATE;
    nucleus->plasticity_enabled = true;
    nucleus->dopamine_level = AMYG_NUCLEUS_DOPAMINE_INIT;
    nucleus->norepinephrine_level = AMYG_NUCLEUS_NE_INIT;
    nucleus->cortisol_level = AMYG_NUCLEUS_CORTISOL_INIT;

    /* Default inter-nucleus weights */
    memset(nucleus->input_weights, 0, sizeof(nucleus->input_weights));

    switch (type) {
        case AMYG_NUCLEUS_LATERAL:
            /* LA receives sensory input, projects to BA and CeA */
            nucleus->input_weights[AMYG_NUCLEUS_BASAL] = AMYG_WEIGHT_LA_TO_BA;
            break;
        case AMYG_NUCLEUS_BASAL:
            /* BA receives from LA and hippocampus, projects to CeA */
            nucleus->input_weights[AMYG_NUCLEUS_LATERAL] = AMYG_WEIGHT_BA_FROM_LA;
            break;
        case AMYG_NUCLEUS_CENTRAL:
            /* CeA receives from LA and BA, output to brainstem */
            nucleus->input_weights[AMYG_NUCLEUS_LATERAL] = AMYG_WEIGHT_LA_TO_CEA;
            nucleus->input_weights[AMYG_NUCLEUS_BASAL] = AMYG_WEIGHT_BA_TO_CEA;
            nucleus->input_weights[AMYG_NUCLEUS_ITC] = AMYG_WEIGHT_ITC_INHIBITION;
            break;
        case AMYG_NUCLEUS_MEDIAL:
            /* MeA for olfactory/social */
            nucleus->input_weights[AMYG_NUCLEUS_LATERAL] = AMYG_WEIGHT_MEA_FROM_LA;
            break;
        case AMYG_NUCLEUS_ITC:
            /* ITC provides inhibition, receives PFC input */
            nucleus->input_weights[AMYG_NUCLEUS_LATERAL] = AMYG_WEIGHT_ITC_FROM_LA;
            nucleus->plasticity_rate = AMYG_NUCLEUS_ITC_PLASTICITY;
            break;
        default:
            break;
    }
}

/**
 * @brief Update nucleus activation based on inputs
 */
static void update_nucleus_activation(amyg_nucleus_t* nucleus,
                                      const amyg_nucleus_t* all_nuclei,
                                      float external_input,
                                      float dt_ms) {
    if (!nucleus || !all_nuclei) return;

    float total_input = external_input;

    /* Sum weighted inputs from other nuclei */
    for (int i = 0; i < AMYG_NUCLEUS_COUNT; i++) {
        if (i != (int)nucleus->type) {
            total_input += nucleus->input_weights[i] * all_nuclei[i].activation;
        }
    }

    /* Apply gain and neuromodulation */
    float modulated_input = total_input * nucleus->gain;
    modulated_input *= (AMYG_NE_MODULATION_BASE + AMYG_NE_MODULATION_SCALE * nucleus->norepinephrine_level);
    modulated_input *= (AMYG_DA_MODULATION_BASE + AMYG_DA_MODULATION_SCALE * nucleus->dopamine_level);

    /* Activation dynamics (simple exponential approach) */
    float tau = AMYG_ACTIVATION_TAU_MS;
    float target = nimcp_clamp01(nucleus->baseline + modulated_input);

    /* P2 fix: Validate dt_ms to prevent NaN/Inf from expf */
    if (dt_ms <= 0.0f || dt_ms > 10000.0f) {
        dt_ms = 1.0f;  /* Default to 1ms if invalid */
    }
    if (tau <= 0.0f) {
        tau = 1.0f;  /* Prevent division by zero */
    }

    float alpha = 1.0f - expf(-dt_ms / tau);

    /* Check for NaN before updating activation */
    float new_activation = nucleus->activation + alpha * (target - nucleus->activation);
    if (isnan(new_activation) || isinf(new_activation)) {
        new_activation = nucleus->baseline;  /* Reset to baseline if corrupted */
    }
    nucleus->activation = nimcp_clamp01(new_activation);
}

/**
 * @brief Convert threat level to fear intensity
 */
static float threat_to_intensity(amyg_threat_level_t threat) {
    switch (threat) {
        case AMYG_THREAT_NONE:     return AMYG_THREAT_NONE_INTENSITY;
        case AMYG_THREAT_LOW:      return AMYG_THREAT_LOW_INTENSITY;
        case AMYG_THREAT_MODERATE: return AMYG_THREAT_MODERATE_INTENSITY;
        case AMYG_THREAT_HIGH:     return AMYG_THREAT_HIGH_INTENSITY;
        case AMYG_THREAT_SEVERE:   return AMYG_THREAT_SEVERE_INTENSITY;
        default:                   return AMYG_THREAT_NONE_INTENSITY;
    }
}

/**
 * @brief Convert fear intensity to threat level
 */
static amyg_threat_level_t intensity_to_threat(float intensity) {
    if (intensity >= AMYG_THREAT_SEVERE_THRESHOLD) return AMYG_THREAT_SEVERE;
    if (intensity >= AMYG_THREAT_HIGH_THRESHOLD) return AMYG_THREAT_HIGH;
    if (intensity >= AMYG_THREAT_MODERATE_THRESHOLD) return AMYG_THREAT_MODERATE;
    if (intensity >= AMYG_THREAT_LOW_THRESHOLD) return AMYG_THREAT_LOW;
    return AMYG_THREAT_NONE;
}

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

int amygdala_default_config(amyg_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->conditioning_rate = AMYG_DEFAULT_CONDITIONING_RATE;
    config->extinction_rate = AMYG_DEFAULT_EXTINCTION_RATE;
    config->reconsolidation_window_ms = AMYG_DEFAULT_RECONSOLIDATION_MS;
    config->spontaneous_recovery_rate = AMYG_DEFAULT_SPONTANEOUS_RECOVERY;

    config->fear_threshold = AMYG_DEFAULT_FEAR_THRESHOLD;
    config->anxiety_threshold = AMYG_DEFAULT_ANXIETY_THRESHOLD;
    config->threat_detection_threshold = AMYG_DEFAULT_THREAT_THRESHOLD;

    config->anxiety_decay_rate = AMYG_DEFAULT_ANXIETY_DECAY;
    config->activation_decay_rate = AMYG_DEFAULT_ACTIVATION_DECAY;

    config->generalization_default = AMYG_DEFAULT_GENERALIZATION;
    config->context_dependent = true;

    config->prefrontal_inhibition_weight = AMYG_DEFAULT_PFC_INHIBITION_WEIGHT;
    config->extinction_enabled = true;

    config->bio_async_enabled = true;
    config->bio_inbox_capacity = AMYG_DEFAULT_BIO_INBOX_CAPACITY;

    return 0;
}

int amygdala_validate_config(const amyg_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    NIMCP_CHECK_THROW(config->conditioning_rate >= 0.0f && config->conditioning_rate <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "conditioning_rate out of range [0,1]");
    NIMCP_CHECK_THROW(config->extinction_rate >= 0.0f && config->extinction_rate <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "extinction_rate out of range [0,1]");
    NIMCP_CHECK_THROW(config->fear_threshold >= 0.0f && config->fear_threshold <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "fear_threshold out of range [0,1]");
    NIMCP_CHECK_THROW(config->anxiety_threshold >= 0.0f && config->anxiety_threshold <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "anxiety_threshold out of range [0,1]");

    return 0;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

amygdala_t* amygdala_create(const amyg_config_t* config) {
    amygdala_t* amyg = (amygdala_t*)nimcp_malloc(sizeof(amygdala_t));
    if (!amyg) {
        NIMCP_LOGGING_ERROR("Failed to allocate amygdala");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

        return NULL;
    }

    memset(amyg, 0, sizeof(amygdala_t));

    /* Apply configuration */
    if (config) {
        if (amygdala_validate_config(config) != 0) {
            NIMCP_LOGGING_ERROR("Invalid amygdala configuration");
            nimcp_free(amyg);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_create: validation failed");
            return NULL;
        }
        amyg->config = *config;
    } else {
        amygdala_default_config(&amyg->config);
    }

    /* Initialize nuclei */
    for (int i = 0; i < AMYG_NUCLEUS_COUNT; i++) {
        init_nucleus(&amyg->nuclei[i], (amyg_nucleus_type_t)i);
    }

    /* Allocate fear memories */
    amyg->fear_memory_capacity = AMYG_MAX_FEAR_MEMORIES;
    amyg->fear_memories = (amyg_fear_memory_t*)nimcp_malloc(
        amyg->fear_memory_capacity * sizeof(amyg_fear_memory_t));
    if (!amyg->fear_memories) {
        NIMCP_LOGGING_ERROR("Failed to allocate fear memories");
        nimcp_free(amyg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "amygdala_create: amyg->fear_memories is NULL");
        return NULL;
    }
    memset(amyg->fear_memories, 0, amyg->fear_memory_capacity * sizeof(amyg_fear_memory_t));
    amyg->fear_memory_count = 0;

    /* Initialize state */
    amyg->current_fear_level = 0.0f;
    amyg->current_anxiety_level = 0.0f;
    amyg->current_threat = AMYG_THREAT_NONE;
    amyg->current_valence = AMYG_VALENCE_NEUTRAL;
    amyg->prefrontal_inhibition = 0.0f;
    amyg->context_valid = false;

    /* Initialize connections */
    amyg->emotion_system = NULL;
    amyg->emotion_system_connected = false;
    amyg->hippocampus = NULL;
    amyg->prefrontal = NULL;
    amyg->hypothalamus = NULL;
    amyg->thalamus = NULL;

    /* Create mutex */
    amyg->mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!amyg->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(amyg->fear_memories);
        nimcp_free(amyg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "amygdala_create: amyg->mutex is NULL");
        return NULL;
    }
    nimcp_mutex_init(amyg->mutex, NULL);

    /* Bio-async */
    amyg->bio_ctx = NULL;
    amyg->bio_async_connected = false;

    /* Statistics */
    amyg->total_fear_events = 0;
    amyg->total_extinction_events = 0;
    amyg->total_conditioning_events = 0;

    NIMCP_LOGGING_INFO("Amygdala created successfully");
    return amyg;
}

void amygdala_destroy(amygdala_t* amyg) {
    if (!amyg) return;

    /* Disconnect from bio-async */
    if (amyg->bio_async_connected) {
        amygdala_disconnect_bio_async(amyg);
    }

    /* Disconnect from emotional system */
    if (amyg->emotion_system_connected) {
        amygdala_disconnect_emotion_system(amyg);
    }

    /* Free resources */
    if (amyg->mutex) {
        nimcp_mutex_destroy(amyg->mutex);
        nimcp_free(amyg->mutex);
    }

    if (amyg->fear_memories) {
        nimcp_free(amyg->fear_memories);
    }

    nimcp_free(amyg);
    NIMCP_LOGGING_INFO("Amygdala destroyed");
}

int amygdala_reset(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);

    /* Reset nuclei */
    for (int i = 0; i < AMYG_NUCLEUS_COUNT; i++) {
        amyg->nuclei[i].activation = amyg->nuclei[i].baseline;
    }

    /* Clear fear memories */
    amyg->fear_memory_count = 0;
    memset(amyg->fear_memories, 0, amyg->fear_memory_capacity * sizeof(amyg_fear_memory_t));

    /* Reset state */
    amyg->current_fear_level = 0.0f;
    amyg->current_anxiety_level = 0.0f;
    amyg->current_threat = AMYG_THREAT_NONE;
    amyg->current_valence = AMYG_VALENCE_NEUTRAL;
    amyg->context_valid = false;

    /* Reset statistics */
    amyg->total_fear_events = 0;
    amyg->total_extinction_events = 0;
    amyg->total_conditioning_events = 0;

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

/* ============================================================================
 * Core Processing Functions
 * ============================================================================ */

int amygdala_process_stimulus(amygdala_t* amyg,
                              const amyg_conditioned_stimulus_t* cs,
                              amyg_fear_response_t* response) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(cs, NIMCP_ERROR_NULL_POINTER, "cs is NULL");

    nimcp_mutex_lock(amyg->mutex);

    float max_match_score = 0.0f;
    uint32_t triggering_memory_id = 0;
    float retrieved_association = 0.0f;

    /* Search for matching fear memories */
    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        amyg_fear_memory_t* mem = &amyg->fear_memories[i];

        /* Compute stimulus similarity */
        uint32_t min_features = (cs->n_features < mem->cs.n_features) ?
                                cs->n_features : mem->cs.n_features;
        float sim = compute_cosine_similarity(cs->features, mem->cs.features, min_features);

        /* Apply generalization */
        float effective_sim = sim;
        if (sim >= (1.0f - mem->generalization_width)) {
            effective_sim = 1.0f; /* Full generalization */
        }

        /* Check context if required */
        float context_factor = 1.0f;
        if (amyg->config.context_dependent && amyg->context_valid) {
            float ctx_sim = amygdala_context_similarity(&amyg->current_context,
                                                        &mem->acquisition_context);
            context_factor = AMYG_CONTEXT_MIN_FACTOR + AMYG_CONTEXT_SCALE_FACTOR * ctx_sim;
        }

        /* Effective association strength */
        float effective_strength = mem->association_strength *
                                   (1.0f - mem->extinction_strength) *
                                   effective_sim *
                                   context_factor;

        if (effective_strength > max_match_score) {
            max_match_score = effective_strength;
            triggering_memory_id = mem->memory_id;
            retrieved_association = mem->association_strength;
        }
    }

    /* Update lateral nucleus (sensory input) */
    float la_input = cs->salience * AMYG_LA_INPUT_CS_WEIGHT +
                     max_match_score * AMYG_LA_INPUT_MEMORY_WEIGHT;
    update_nucleus_activation(&amyg->nuclei[AMYG_NUCLEUS_LATERAL],
                              amyg->nuclei, la_input, AMYG_STIMULUS_DT_MS);

    /* Update basal nucleus (context integration) */
    float ba_input = max_match_score * AMYG_BA_MEMORY_WEIGHT;
    if (amyg->context_valid) {
        ba_input += (1.0f - amyg->current_context.familiarity) * AMYG_BA_UNFAMILIAR_WEIGHT;
    }
    update_nucleus_activation(&amyg->nuclei[AMYG_NUCLEUS_BASAL],
                              amyg->nuclei, ba_input, AMYG_STIMULUS_DT_MS);

    /* Update ITC (inhibitory, receives PFC) */
    float itc_input = amyg->prefrontal_inhibition * amyg->config.prefrontal_inhibition_weight;
    update_nucleus_activation(&amyg->nuclei[AMYG_NUCLEUS_ITC],
                              amyg->nuclei, itc_input, AMYG_STIMULUS_DT_MS);

    /* Update central nucleus (output) */
    update_nucleus_activation(&amyg->nuclei[AMYG_NUCLEUS_CENTRAL],
                              amyg->nuclei, 0.0f, AMYG_STIMULUS_DT_MS);

    /* Compute fear output from CeA */
    float cea_output = amyg->nuclei[AMYG_NUCLEUS_CENTRAL].activation;

    /* Update current state */
    amyg->current_fear_level = cea_output;
    amyg->current_threat = intensity_to_threat(cea_output);

    if (cea_output > amyg->config.fear_threshold) {
        amyg->current_valence = AMYG_VALENCE_NEGATIVE;
        amyg->total_fear_events++;

        /* Increase background anxiety */
        amyg->current_anxiety_level = nimcp_clamp01(amyg->current_anxiety_level + AMYG_ANXIETY_INCREMENT);
    }

    /* Prepare response */
    amyg->last_response.fear_intensity = cea_output;
    amyg->last_response.anxiety_level = amyg->current_anxiety_level;
    amyg->last_response.threat_level = amyg->current_threat;
    amyg->last_response.triggering_memory_id = triggering_memory_id;
    amyg->last_response.memory_match_score = max_match_score;

    /* Set output activations */
    amyg->last_response.outputs[AMYG_OUTPUT_FREEZING] = nimcp_clamp01(cea_output * AMYG_OUTPUT_FREEZING_MULT);
    amyg->last_response.outputs[AMYG_OUTPUT_STARTLE] = nimcp_clamp01(cea_output * AMYG_OUTPUT_STARTLE_MULT);
    amyg->last_response.outputs[AMYG_OUTPUT_AUTONOMIC] = nimcp_clamp01(cea_output * AMYG_OUTPUT_AUTONOMIC_MULT);
    amyg->last_response.outputs[AMYG_OUTPUT_HORMONAL] = nimcp_clamp01(cea_output * AMYG_OUTPUT_HORMONAL_MULT);
    amyg->last_response.outputs[AMYG_OUTPUT_ATTENTION] = nimcp_clamp01(cea_output * AMYG_OUTPUT_ATTENTION_MULT);

    if (response) {
        *response = amyg->last_response;
    }

    /* Sync to emotional system if connected */
    if (amyg->emotion_system_connected) {
        amygdala_sync_to_emotion_system(amyg);
    }

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_step(amygdala_t* amyg, float dt_ms) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(dt_ms > 0.0f, NIMCP_ERROR_INVALID_PARAM, "dt_ms must be positive");

    nimcp_mutex_lock(amyg->mutex);

    /* Decay nuclei activations toward baseline */
    for (int i = 0; i < AMYG_NUCLEUS_COUNT; i++) {
        float decay = amyg->config.activation_decay_rate * dt_ms / 1000.0f;
        float diff = amyg->nuclei[i].activation - amyg->nuclei[i].baseline;
        amyg->nuclei[i].activation -= diff * decay;
        amyg->nuclei[i].activation = nimcp_clamp01(amyg->nuclei[i].activation);
    }

    /* Decay anxiety */
    amyg->current_anxiety_level -= amyg->config.anxiety_decay_rate * dt_ms / 1000.0f;
    amyg->current_anxiety_level = nimcp_clamp01(amyg->current_anxiety_level);

    /* Update fear level from CeA */
    amyg->current_fear_level = amyg->nuclei[AMYG_NUCLEUS_CENTRAL].activation;
    amyg->current_threat = intensity_to_threat(amyg->current_fear_level);

    /* Check for spontaneous recovery */
    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        amyg_fear_memory_t* mem = &amyg->fear_memories[i];
        if (mem->extinction_strength > 0.0f && mem->phase == AMYG_PHASE_EXTINCTION) {
            /* Spontaneous recovery over time */
            float recovery = amyg->config.spontaneous_recovery_rate * dt_ms / 1000.0f;
            mem->extinction_strength -= recovery;
            if (mem->extinction_strength < 0.0f) {
                mem->extinction_strength = 0.0f;
                mem->phase = AMYG_PHASE_SPONTANEOUS_RECOVERY;
            }
        }
    }

    /* Sync from emotional system if connected */
    if (amyg->emotion_system_connected) {
        amygdala_sync_from_emotion_system(amyg);
    }

    amyg->last_update_ms = amyg->current_time_ms;
    amyg->current_time_ms += (uint64_t)dt_ms;

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_get_response(const amygdala_t* amyg, amyg_fear_response_t* response) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(response, NIMCP_ERROR_NULL_POINTER, "response is NULL");

    // Note: Must cast away const to lock mutex for thread-safe read
    // This is safe because we only read response data, not modify amygdala state
    nimcp_mutex_lock(((amygdala_t*)amyg)->mutex);
    *response = amyg->last_response;
    nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);

    return 0;
}

/* ============================================================================
 * Fear Conditioning Functions
 * ============================================================================ */

int amygdala_condition_fear(amygdala_t* amyg,
                            const amyg_conditioned_stimulus_t* cs,
                            const amyg_unconditioned_stimulus_t* us,
                            uint32_t* memory_id) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(cs, NIMCP_ERROR_NULL_POINTER, "cs is NULL");
    NIMCP_CHECK_THROW(us, NIMCP_ERROR_NULL_POINTER, "us is NULL");

    nimcp_mutex_lock(amyg->mutex);

    /* Check if memory already exists for this CS */
    int existing_idx = -1;
    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        uint32_t min_features = (cs->n_features < amyg->fear_memories[i].cs.n_features) ?
                                cs->n_features : amyg->fear_memories[i].cs.n_features;
        float sim = compute_cosine_similarity(cs->features,
                                              amyg->fear_memories[i].cs.features,
                                              min_features);
        if (sim > AMYG_CS_MATCH_THRESHOLD) {
            existing_idx = (int)i;
            break;
        }
    }

    if (existing_idx >= 0) {
        /* Update existing memory */
        amyg_fear_memory_t* mem = &amyg->fear_memories[existing_idx];

        /* Strengthen association */
        float delta = amyg->config.conditioning_rate * us->intensity;
        mem->association_strength = nimcp_clamp01(mem->association_strength + delta);
        mem->us = *us;
        mem->phase = AMYG_PHASE_ACQUISITION;
        mem->last_retrieval_ms = amyg->current_time_ms;
        mem->retrieval_count++;

        if (memory_id) *memory_id = mem->memory_id;
    } else {
        /* Create new memory */
        if (amyg->fear_memory_count >= amyg->fear_memory_capacity) {
            nimcp_mutex_unlock(amyg->mutex);
            return NIMCP_ERROR_OPERATION_FAILED;
        }

        amyg_fear_memory_t* mem = &amyg->fear_memories[amyg->fear_memory_count];
        mem->memory_id = amyg->fear_memory_count + 1;
        mem->cs = *cs;
        mem->us = *us;
        mem->association_strength = amyg->config.conditioning_rate * us->intensity;
        mem->extinction_strength = 0.0f;
        mem->generalization_width = amyg->config.generalization_default;
        mem->acquisition_time_ms = amyg->current_time_ms;
        mem->last_retrieval_ms = amyg->current_time_ms;
        mem->retrieval_count = 1;
        mem->phase = AMYG_PHASE_ACQUISITION;
        mem->is_consolidated = false;

        if (amyg->context_valid) {
            mem->acquisition_context = amyg->current_context;
        }

        if (memory_id) *memory_id = mem->memory_id;
        amyg->fear_memory_count++;
    }

    amyg->total_conditioning_events++;

    /* Boost LA and CeA activation for US */
    amyg->nuclei[AMYG_NUCLEUS_LATERAL].activation =
        nimcp_clamp01(amyg->nuclei[AMYG_NUCLEUS_LATERAL].activation + us->intensity * AMYG_US_LA_BOOST);
    amyg->nuclei[AMYG_NUCLEUS_CENTRAL].activation =
        nimcp_clamp01(amyg->nuclei[AMYG_NUCLEUS_CENTRAL].activation + us->intensity * AMYG_US_CEA_BOOST);

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_extinction_trial(amygdala_t* amyg,
                              const amyg_conditioned_stimulus_t* cs) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(cs, NIMCP_ERROR_NULL_POINTER, "cs is NULL");
    if (!amyg->config.extinction_enabled) return 0;

    nimcp_mutex_lock(amyg->mutex);

    /* Find matching memory */
    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        amyg_fear_memory_t* mem = &amyg->fear_memories[i];

        uint32_t min_features = (cs->n_features < mem->cs.n_features) ?
                                cs->n_features : mem->cs.n_features;
        float sim = compute_cosine_similarity(cs->features, mem->cs.features, min_features);

        if (sim > AMYG_EXTINCTION_MATCH_THRESHOLD) {
            /* Strengthen extinction learning */
            float delta = amyg->config.extinction_rate;

            /* ITC plasticity enhances extinction */
            delta *= (1.0f + amyg->nuclei[AMYG_NUCLEUS_ITC].activation);

            /* PFC inhibition enhances extinction */
            delta *= (1.0f + amyg->prefrontal_inhibition);

            mem->extinction_strength = nimcp_clamp01(mem->extinction_strength + delta);
            mem->phase = AMYG_PHASE_EXTINCTION;
            mem->last_retrieval_ms = amyg->current_time_ms;
            mem->retrieval_count++;

            amyg->total_extinction_events++;
        }
    }

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_retrieve_fear_memory(amygdala_t* amyg,
                                  const amyg_conditioned_stimulus_t* cs,
                                  amyg_fear_memory_t* memory,
                                  float* match_score) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(cs, NIMCP_ERROR_NULL_POINTER, "cs is NULL");

    nimcp_mutex_lock(amyg->mutex);

    float best_score = 0.0f;
    int best_idx = -1;

    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        amyg_fear_memory_t* mem = &amyg->fear_memories[i];

        uint32_t min_features = (cs->n_features < mem->cs.n_features) ?
                                cs->n_features : mem->cs.n_features;
        float sim = compute_cosine_similarity(cs->features, mem->cs.features, min_features);

        if (sim > best_score) {
            best_score = sim;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0 && memory) {
        *memory = amyg->fear_memories[best_idx];
    }
    if (match_score) {
        *match_score = best_score;
    }

    nimcp_mutex_unlock(amyg->mutex);

    return (best_idx >= 0) ? 0 : NIMCP_ERROR_OPERATION_FAILED;
}

int amygdala_add_fear_memory(amygdala_t* amyg,
                             const amyg_fear_memory_t* memory,
                             uint32_t* memory_id) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_NULL_POINTER, "memory is NULL");

    nimcp_mutex_lock(amyg->mutex);

    if (amyg->fear_memory_count >= amyg->fear_memory_capacity) {
        nimcp_mutex_unlock(amyg->mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    amyg->fear_memories[amyg->fear_memory_count] = *memory;
    amyg->fear_memories[amyg->fear_memory_count].memory_id = amyg->fear_memory_count + 1;

    if (memory_id) {
        *memory_id = amyg->fear_memories[amyg->fear_memory_count].memory_id;
    }

    amyg->fear_memory_count++;

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_get_fear_memory(const amygdala_t* amyg,
                             uint32_t memory_id,
                             amyg_fear_memory_t* memory) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(memory, NIMCP_ERROR_NULL_POINTER, "memory is NULL");

    // Note: Must cast away const to lock mutex for thread-safe read
    // This is safe because we only read memory data, not modify amygdala state
    nimcp_mutex_lock(((amygdala_t*)amyg)->mutex);

    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        if (amyg->fear_memories[i].memory_id == memory_id) {
            *memory = amyg->fear_memories[i];
            nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);
    return NIMCP_ERROR_OPERATION_FAILED;
}

int amygdala_clear_fear_memories(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->fear_memory_count = 0;
    memset(amyg->fear_memories, 0, amyg->fear_memory_capacity * sizeof(amyg_fear_memory_t));
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

/* ============================================================================
 * Context Functions
 * ============================================================================ */

int amygdala_set_context(amygdala_t* amyg, const amyg_context_t* context) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->current_context = *context;
    amyg->context_valid = true;
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_get_context(const amygdala_t* amyg, amyg_context_t* context) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");

    nimcp_mutex_lock(((amygdala_t*)amyg)->mutex);
    *context = amyg->current_context;
    nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);

    return amyg->context_valid ? 0 : NIMCP_ERROR_INVALID_STATE;
}

int amygdala_context_match(const amygdala_t* amyg,
                           uint32_t memory_id,
                           float* match_score) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(match_score, NIMCP_ERROR_NULL_POINTER, "match_score is NULL");
    NIMCP_CHECK_THROW(amyg->context_valid, NIMCP_ERROR_INVALID_STATE, "context is not valid");

    nimcp_mutex_lock(((amygdala_t*)amyg)->mutex);

    for (uint32_t i = 0; i < amyg->fear_memory_count; i++) {
        if (amyg->fear_memories[i].memory_id == memory_id) {
            *match_score = amygdala_context_similarity(&amyg->current_context,
                                                       &amyg->fear_memories[i].acquisition_context);
            nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);
    return NIMCP_ERROR_OPERATION_FAILED;
}

/* ============================================================================
 * Regulation Functions
 * ============================================================================ */

int amygdala_set_prefrontal_inhibition(amygdala_t* amyg, float inhibition) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->prefrontal_inhibition = nimcp_clamp01(inhibition);
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_set_neuromodulators(amygdala_t* amyg,
                                 float dopamine,
                                 float norepinephrine,
                                 float cortisol) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);

    for (int i = 0; i < AMYG_NUCLEUS_COUNT; i++) {
        amyg->nuclei[i].dopamine_level = nimcp_clamp01(dopamine);
        amyg->nuclei[i].norepinephrine_level = nimcp_clamp01(norepinephrine);
        amyg->nuclei[i].cortisol_level = nimcp_clamp01(cortisol);
    }

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_set_anxiety(amygdala_t* amyg, float anxiety) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->current_anxiety_level = nimcp_clamp01(anxiety);
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

/* ============================================================================
 * Nucleus Access Functions
 * ============================================================================ */

int amygdala_get_nucleus_activation(const amygdala_t* amyg,
                                    amyg_nucleus_type_t nucleus,
                                    float* activation) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(activation, NIMCP_ERROR_NULL_POINTER, "activation is NULL");
    NIMCP_CHECK_THROW(nucleus < AMYG_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    nimcp_mutex_lock(((amygdala_t*)amyg)->mutex);
    *activation = amyg->nuclei[nucleus].activation;
    nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);

    return 0;
}

int amygdala_set_nucleus_activation(amygdala_t* amyg,
                                    amyg_nucleus_type_t nucleus,
                                    float activation) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(nucleus < AMYG_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    nimcp_mutex_lock(amyg->mutex);
    amyg->nuclei[nucleus].activation = nimcp_clamp01(activation);
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_set_nucleus_plasticity(amygdala_t* amyg,
                                    amyg_nucleus_type_t nucleus,
                                    bool enabled) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    NIMCP_CHECK_THROW(nucleus < AMYG_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    nimcp_mutex_lock(amyg->mutex);
    amyg->nuclei[nucleus].plasticity_enabled = enabled;
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

int amygdala_connect_hippocampus(amygdala_t* amyg, void* hippocampus) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->hippocampus = hippocampus;
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_connect_prefrontal(amygdala_t* amyg, void* prefrontal) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->prefrontal = prefrontal;
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_connect_hypothalamus(amygdala_t* amyg, void* hypothalamus) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->hypothalamus = hypothalamus;
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_connect_thalamus(amygdala_t* amyg, void* thalamus) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->thalamus = thalamus;
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_connect_emotion_system(amygdala_t* amyg, emotional_system_t* emotion_system) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);

    amyg->emotion_system = emotion_system;
    amyg->emotion_system_connected = (emotion_system != NULL);

    if (amyg->emotion_system_connected) {
        NIMCP_LOGGING_INFO("Amygdala connected to emotional system");
    }

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

int amygdala_disconnect_emotion_system(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);

    amyg->emotion_system = NULL;
    amyg->emotion_system_connected = false;

    NIMCP_LOGGING_INFO("Amygdala disconnected from emotional system");

    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

bool amygdala_is_emotion_system_connected(const amygdala_t* amyg) {
    if (!amyg) {
        return false;
    }
    return amyg->emotion_system_connected;
}

int amygdala_sync_to_emotion_system(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    if (!amyg->emotion_system_connected || !amyg->emotion_system) return 0;

    /* Convert amygdala fear/anxiety to emotional system valence/arousal
     * - Fear -> Negative valence + high arousal
     * - Anxiety -> Elevated baseline arousal
     */
    float fear = amyg->current_fear_level;
    float anxiety = amyg->current_anxiety_level;

    /* Valence: fear drives negative valence */
    float valence = -fear; /* More fear = more negative */

    /* Arousal: combination of fear (acute) and anxiety (chronic) */
    float arousal = nimcp_clamp01(fear * AMYG_AROUSAL_FEAR_WEIGHT +
                            anxiety * AMYG_AROUSAL_ANXIETY_WEIGHT);

    /* Update emotional system */
    emotion_system_set_state(amyg->emotion_system, valence, arousal,
                             amyg->current_time_ms);

    return 0;
}

int amygdala_sync_from_emotion_system(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    if (!amyg->emotion_system_connected || !amyg->emotion_system) return 0;

    /* Get regulation state from emotional system */
    emotion_state_t state;
    if (!emotion_system_get_state(amyg->emotion_system, &state)) {
        return 0;
    }

    /* If emotional system is in self-regulation, increase PFC inhibition */
    if (state.in_self_regulation) {
        amyg->prefrontal_inhibition = nimcp_clamp01(amyg->prefrontal_inhibition +
                                               AMYG_PFC_REGULATION_INCREMENT);
    }

    /* Emotional stability reduces anxiety */
    if (state.emotional_stability > AMYG_EMOTIONAL_STABILITY_THRESH) {
        amyg->current_anxiety_level = nimcp_clamp01(amyg->current_anxiety_level -
                                               AMYG_EMOTIONAL_ANXIETY_DECR);
    }

    return 0;
}

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

int amygdala_connect_bio_async(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");
    if (amyg->bio_async_connected) return 0;

    /* Bio-async registration would happen here if router is available */
    amyg->bio_async_connected = false; /* Set to true when actually connected */

    NIMCP_LOGGING_INFO("Amygdala bio-async: router not available");

    return 0;
}

int amygdala_disconnect_bio_async(amygdala_t* amyg) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    amyg->bio_async_connected = false;
    amyg->bio_ctx = NULL;

    return 0;
}

bool amygdala_is_bio_async_connected(const amygdala_t* amyg) {
    if (!amyg) {
        return false;
    }
    return amyg->bio_async_connected;
}

/* ============================================================================
 * Statistics and Debug
 * ============================================================================ */

uint32_t amygdala_get_fear_memory_count(const amygdala_t* amyg) {
    if (!amyg) return 0;
    return amyg->fear_memory_count;
}

float amygdala_get_fear_level(const amygdala_t* amyg) {
    if (!amyg) return 0.0f;
    return amyg->current_fear_level;
}

int amygdala_set_fear_level(amygdala_t* amyg, float fear) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(amyg->mutex);
    amyg->current_fear_level = nimcp_clamp01(fear);
    /* Update threat level based on fear */
    amyg->current_threat = intensity_to_threat(amyg->current_fear_level);
    nimcp_mutex_unlock(amyg->mutex);

    return 0;
}

float amygdala_get_anxiety_level(const amygdala_t* amyg) {
    if (!amyg) return 0.0f;
    return amyg->current_anxiety_level;
}

amyg_threat_level_t amygdala_get_threat_level(const amygdala_t* amyg) {
    if (!amyg) return AMYG_THREAT_NONE;
    return amyg->current_threat;
}

int amygdala_get_statistics(const amygdala_t* amyg,
                            uint64_t* fear_events,
                            uint64_t* extinction_events,
                            uint64_t* conditioning_events) {
    NIMCP_CHECK_THROW(amyg, NIMCP_ERROR_NULL_POINTER, "amyg is NULL");

    nimcp_mutex_lock(((amygdala_t*)amyg)->mutex);

    if (fear_events) *fear_events = amyg->total_fear_events;
    if (extinction_events) *extinction_events = amyg->total_extinction_events;
    if (conditioning_events) *conditioning_events = amyg->total_conditioning_events;

    nimcp_mutex_unlock(((amygdala_t*)amyg)->mutex);

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

float amygdala_stimulus_similarity(const amyg_conditioned_stimulus_t* cs1,
                                   const amyg_conditioned_stimulus_t* cs2) {
    if (!cs1 || !cs2) return 0.0f;

    uint32_t n = (cs1->n_features < cs2->n_features) ? cs1->n_features : cs2->n_features;
    return compute_cosine_similarity(cs1->features, cs2->features, n);
}

float amygdala_context_similarity(const amyg_context_t* ctx1,
                                  const amyg_context_t* ctx2) {
    if (!ctx1 || !ctx2) return 0.0f;

    return compute_cosine_similarity(ctx1->context_vector, ctx2->context_vector,
                                     AMYG_MAX_CONTEXT_DIM);
}

const char* amygdala_nucleus_name(amyg_nucleus_type_t nucleus) {
    switch (nucleus) {
        case AMYG_NUCLEUS_LATERAL:  return "Lateral (LA)";
        case AMYG_NUCLEUS_BASAL:    return "Basal (BA)";
        case AMYG_NUCLEUS_CENTRAL:  return "Central (CeA)";
        case AMYG_NUCLEUS_MEDIAL:   return "Medial (MeA)";
        case AMYG_NUCLEUS_ITC:      return "Intercalated (ITC)";
        default:                    return "Unknown";
    }
}

const char* amygdala_threat_level_name(amyg_threat_level_t threat) {
    switch (threat) {
        case AMYG_THREAT_NONE:      return "None";
        case AMYG_THREAT_LOW:       return "Low";
        case AMYG_THREAT_MODERATE:  return "Moderate";
        case AMYG_THREAT_HIGH:      return "High";
        case AMYG_THREAT_SEVERE:    return "Severe";
        default:                    return "Unknown";
    }
}
