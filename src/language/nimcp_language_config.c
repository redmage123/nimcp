#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_config.c - Language Layer Configuration Defaults
//=============================================================================
/**
 * @file nimcp_language_config.c
 * @brief Default configuration functions for Language Layer
 *
 * @version 1.0.0 - Phase L5: Orchestrator Implementation
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/nimcp_language_config.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_config)

//=============================================================================
// Default Configuration Functions
//=============================================================================

void language_perception_default_config(language_perception_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Connection enables */
    config->enable_speech_cortex = true;
    config->enable_audio_cortex = true;
    config->enable_visual_cortex = false;  /* Opt-in */
    config->enable_omni_sensory = true;

    /* Speech processing */
    config->speech_confidence_threshold = 0.5f;
    config->phoneme_lookahead = 3;
    config->enable_coarticulation = true;

    /* Visual processing (reading) */
    config->visual_text_threshold = 0.7f;
    config->enable_lip_reading = false;
    config->mcgurk_weight = 0.3f;

    /* Cross-modal */
    config->enable_audiovisual_binding = true;
    config->audiovisual_coherence_threshold = 0.6f;

    /* Attention */
    config->enable_attention_feedback = true;
    config->attention_boost_magnitude = 0.2f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 10;
}

void language_cognitive_default_config(language_cognitive_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Connection enables */
    config->enable_working_memory = true;
    config->enable_attention = true;
    config->enable_semantic_memory = true;
    config->enable_reasoning = true;
    config->enable_executive = true;

    /* Working memory (Baddeley model) */
    config->phonological_buffer_size = 7;  /* Miller's 7±2 */
    config->rehearsal_decay_rate = 0.95f;
    config->enable_articulatory_rehearsal = true;

    /* Semantic memory */
    config->spreading_activation_decay = 0.8f;
    config->max_spreading_depth = 3;
    config->concept_activation_threshold = 0.1f;

    /* Attention */
    config->linguistic_attention_base = 0.5f;
    config->enable_attention_modulation = true;

    /* Reasoning */
    config->enable_inference_feedback = true;
    config->inference_confidence_weight = 0.7f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 20;
}

void language_training_default_config(language_training_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Connection enables */
    config->enable_training_context = true;
    config->enable_cognitive_training = true;
    config->enable_perception_training = true;
    config->enable_plasticity = true;

    /* Vocabulary learning */
    config->vocabulary_learning_rate = 0.01f;
    config->vocabulary_decay_rate = 0.001f;
    config->enable_vocabulary_expansion = true;

    /* Grammar learning */
    config->grammar_learning_rate = 0.005f;
    config->enable_grammar_induction = true;

    /* Phoneme learning (STDP) */
    config->stdp_lr_potentiation = 0.01f;
    config->stdp_lr_depression = 0.012f;
    config->stdp_time_window_ms = 20.0f;
    config->enable_stdp = true;

    /* Semantic binding */
    config->semantic_binding_rate = 0.01f;
    config->enable_semantic_learning = true;

    /* Error feedback */
    config->enable_comprehension_feedback = true;
    config->enable_production_feedback = true;
    config->error_signal_scale = 1.0f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 50;
}

void language_omni_default_config(language_omni_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Connection enables */
    config->enable_jepa = true;
    config->enable_predictive_hierarchy = true;
    config->enable_hopfield = true;
    config->enable_fep = true;

    /* Prediction settings */
    config->phoneme_prediction_horizon = 5;
    config->word_prediction_horizon = 3;
    config->prediction_confidence_threshold = 0.3f;

    /* Precision */
    config->default_precision = 0.8f;
    config->enable_precision_modulation = true;
    config->precision_update_rate = 0.01f;

    /* Prediction error */
    config->enable_pe_reporting = true;
    config->pe_threshold_phoneme = 0.3f;
    config->pe_threshold_word = 0.4f;
    config->pe_threshold_semantic = 0.5f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 20;
}

void language_immune_default_config(language_immune_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Connection enables */
    config->enable_brain_immune = true;
    config->enable_wernicke_immune = true;
    config->enable_broca_immune = true;
    config->enable_nlp_immune = true;

    /* Cytokine sensitivity */
    config->il1b_sensitivity = 1.0f;
    config->il6_sensitivity = 0.8f;
    config->tnfa_sensitivity = 1.2f;
    config->il10_sensitivity = 0.5f;

    /* Inflammation thresholds */
    config->mild_inflammation_threshold = 0.3f;
    config->moderate_inflammation_threshold = 0.5f;
    config->severe_inflammation_threshold = 0.7f;

    /* Effects */
    config->comprehension_impairment_scale = 1.0f;
    config->production_impairment_scale = 1.0f;
    config->enable_aphasia_modeling = true;

    /* Recovery */
    config->recovery_rate = 0.01f;
    config->enable_immune_memory = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 100;
}

void language_gpu_default_config(language_gpu_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_gpu_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Enable flags */
    config->enable_gpu = false;  /* Opt-in */
    config->enable_phoneme_gpu = true;
    config->enable_lexical_gpu = true;
    config->enable_semantic_gpu = true;
    config->enable_embedding_gpu = true;

    /* GPU settings */
    config->device_id = 0;
    config->batch_size = 32;
    config->enable_async_transfer = true;
    config->enable_tensor_cores = true;

    /* Memory */
    config->max_gpu_memory_mb = 256;
    config->enable_memory_pool = true;

    /* Batching */
    config->phoneme_batch_threshold = 16;
    config->word_batch_threshold = 8;
    config->batch_timeout_ms = 5.0f;

    /* Bio-async */
    config->enable_bio_async = true;
}

void language_thalamic_default_config(language_thalamic_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Enable flags */
    config->enable_attention_gating = true;
    config->enable_motor_priority = true;
    config->enable_semantic_routing = true;
    config->enable_multimodal_routing = true;

    /* Thresholds */
    config->attention_threshold = 0.3f;
    config->motor_priority_boost = 0.2f;
    config->gating_decay_rate = 0.1f;

    /* Timing */
    config->relay_latency_us = 5000;   /* 5ms relay latency */
    config->gating_window_ms = 50;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 10;
}

void language_substrate_default_config(language_substrate_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_substrate_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Enable flags */
    config->enable_atp_modulation = true;
    config->enable_fatigue_effects = true;
    config->enable_stress_effects = true;
    config->enable_neurotransmitter_effects = true;

    /* Sensitivity settings */
    config->atp_sensitivity = 1.0f;
    config->fatigue_sensitivity = 1.0f;
    config->stress_sensitivity = 0.8f;

    /* Thresholds */
    config->critical_atp_threshold = 0.3f;
    config->high_fatigue_threshold = 0.7f;
    config->optimal_stress_level = 0.4f;  /* Yerkes-Dodson optimal */

    /* Timing */
    config->update_interval_ms = 50;

    /* Bio-async */
    config->enable_bio_async = true;
}

void language_logic_default_config(language_logic_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_default_config: NULL config");
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Enable flags */
    config->enable_entailment_checking = true;
    config->enable_consistency_checking = true;
    config->enable_presupposition_check = true;
    config->enable_reference_resolution = true;
    config->enable_implicature_reasoning = false;  /* Opt-in (slower) */

    /* Reasoning parameters */
    config->max_inference_depth = 5;
    config->default_timeout_ms = 100;
    config->min_confidence_threshold = 0.7f;

    /* Performance */
    config->enable_caching = true;
    config->cache_size = 256;

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 20;
}

//=============================================================================
// Version Information
//=============================================================================

const char* nimcp_language_layer_version(void)
{
    return "1.0.0";
}

const char* nimcp_language_layer_build_info(void)
{
    return "Language Layer v1.0.0 - Phase L5 Implementation";
}
