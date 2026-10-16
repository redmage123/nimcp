//=============================================================================
// nimcp_language_config.h - Language Layer Configuration
//=============================================================================
/**
 * @file nimcp_language_config.h
 * @brief Configuration structures for the Language Layer
 *
 * WHAT: Configuration types for language orchestrator and bridges
 * WHY:  Centralized configuration for consistent initialization
 * HOW:  Default configs + override capability for customization
 *
 * @version 1.0.0 - Phase L1: Language Layer Core Infrastructure
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_CONFIG_H
#define NIMCP_LANGUAGE_CONFIG_H

#include "language/nimcp_language_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Orchestrator Configuration
//=============================================================================

/**
 * @brief Language orchestrator configuration
 *
 * WHAT: Configuration for the central language layer orchestrator
 * WHY:  Control which subsystems and bridges are enabled
 * HOW:  Set flags and parameters before creating orchestrator
 */
typedef struct {
    /* Processing mode */
    language_mode_t default_mode;     /**< Default processing mode */

    /* Subsystem enables */
    bool enable_wernicke;             /**< Enable comprehension (Wernicke) */
    bool enable_broca;                /**< Enable production (Broca) */
    bool enable_nlp_core;             /**< Enable NLP processing */
    bool enable_spike_nlp;            /**< Enable spike-based NLP */
    bool enable_multimodal;           /**< Enable multimodal fusion */

    /* Bridge enables */
    bool enable_perception_bridge;    /**< Connect to perception layer */
    bool enable_cognitive_bridge;     /**< Connect to cognitive layer */
    bool enable_training_bridge;      /**< Connect to training layer */
    bool enable_omni_bridge;          /**< Connect to omni inference */
    bool enable_immune_bridge;        /**< Connect to immune system */
    bool enable_gpu_bridge;           /**< Enable GPU acceleration */

    /* Processing settings */
    uint32_t max_utterance_words;     /**< Max words per utterance */
    uint32_t phoneme_buffer_size;     /**< Phoneme buffer capacity */
    uint32_t semantic_dim;            /**< Semantic vector dimension */
    float comprehension_threshold;    /**< Min confidence for comprehension */
    float production_threshold;       /**< Min confidence for production */

    /* Timing settings */
    uint32_t update_interval_ms;      /**< Update cycle interval */
    uint32_t comprehension_timeout_ms;/**< Max comprehension time */
    uint32_t production_timeout_ms;   /**< Max production time */

    /* Bio-async settings */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t message_inbox_capacity;  /**< Message inbox size */

    /* Logging */
    bool enable_logging;              /**< Enable detailed logging */
    bool enable_stats;                /**< Enable statistics tracking */
} language_orchestrator_config_t;

//=============================================================================
// Perception Bridge Configuration
//=============================================================================

/**
 * @brief Language-perception bridge configuration
 */
typedef struct {
    /* Connection enables */
    bool enable_speech_cortex;        /**< Connect to speech cortex */
    bool enable_audio_cortex;         /**< Connect to audio cortex */
    bool enable_visual_cortex;        /**< Connect to visual cortex */
    bool enable_omni_sensory;         /**< Connect to omni sensory bridge */

    /* Speech processing */
    float speech_confidence_threshold;/**< Min confidence for speech input */
    uint32_t phoneme_lookahead;       /**< Phoneme prediction lookahead */
    bool enable_coarticulation;       /**< Model coarticulation effects */

    /* Visual processing (reading) */
    float visual_text_threshold;      /**< Min confidence for text detection */
    bool enable_lip_reading;          /**< Enable lip-reading features */
    float mcgurk_weight;              /**< McGurk effect strength [0-1] */

    /* Cross-modal */
    bool enable_audiovisual_binding;  /**< Enable AV cross-modal binding */
    float audiovisual_coherence_threshold;  /**< Binding coherence threshold */

    /* Attention */
    bool enable_attention_feedback;   /**< Send attention to perception */
    float attention_boost_magnitude;  /**< Attention boost strength */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t update_interval_ms;      /**< Update cycle interval */
} language_perception_config_t;

//=============================================================================
// Cognitive Bridge Configuration
//=============================================================================

/**
 * @brief Language-cognitive bridge configuration
 */
typedef struct {
    /* Connection enables */
    bool enable_working_memory;       /**< Connect to working memory */
    bool enable_attention;            /**< Connect to attention module */
    bool enable_semantic_memory;      /**< Connect to semantic memory */
    bool enable_reasoning;            /**< Connect to reasoning engine */
    bool enable_executive;            /**< Connect to executive controller */

    /* Working memory */
    uint32_t phonological_buffer_size;/**< Phonological loop size (7±2) */
    float rehearsal_decay_rate;       /**< Subvocal rehearsal decay */
    bool enable_articulatory_rehearsal;/**< Enable subvocal rehearsal */

    /* Semantic memory */
    float spreading_activation_decay; /**< Spreading activation decay */
    uint32_t max_spreading_depth;     /**< Max spreading hops */
    float concept_activation_threshold;/**< Min activation for reporting */

    /* Attention */
    float linguistic_attention_base;  /**< Base linguistic attention */
    bool enable_attention_modulation; /**< Modulate attention from language */

    /* Reasoning */
    bool enable_inference_feedback;   /**< Use reasoning for interpretation */
    float inference_confidence_weight;/**< Weight of reasoning confidence */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t update_interval_ms;      /**< Update cycle interval */
} language_cognitive_config_t;

//=============================================================================
// Training Bridge Configuration
//=============================================================================

/**
 * @brief Language-training bridge configuration
 */
typedef struct {
    /* Connection enables */
    bool enable_training_context;     /**< Connect to training context */
    bool enable_cognitive_training;   /**< Connect to cognitive training bridge */
    bool enable_perception_training;  /**< Connect to perception training bridge */
    bool enable_plasticity;           /**< Connect to plasticity system */

    /* Vocabulary learning */
    float vocabulary_learning_rate;   /**< Vocabulary LR */
    float vocabulary_decay_rate;      /**< Forgetting rate for unused words */
    bool enable_vocabulary_expansion; /**< Allow new word learning */

    /* Grammar learning */
    float grammar_learning_rate;      /**< Grammar pattern LR */
    bool enable_grammar_induction;    /**< Allow grammar rule induction */

    /* Phoneme learning (STDP) */
    float stdp_lr_potentiation;       /**< STDP LTP rate */
    float stdp_lr_depression;         /**< STDP LTD rate */
    float stdp_time_window_ms;        /**< STDP time window */
    bool enable_stdp;                 /**< Enable spike timing plasticity */

    /* Semantic binding */
    float semantic_binding_rate;      /**< Word-concept binding strength */
    bool enable_semantic_learning;    /**< Allow semantic association learning */

    /* Error feedback */
    bool enable_comprehension_feedback;/**< Report comprehension errors */
    bool enable_production_feedback;  /**< Report production errors */
    float error_signal_scale;         /**< Scale factor for error signals */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t update_interval_ms;      /**< Update cycle interval */
} language_training_config_t;

//=============================================================================
// Omni Bridge Configuration
//=============================================================================

/**
 * @brief Language-omni inference bridge configuration
 */
typedef struct {
    /* Connection enables */
    bool enable_jepa;                 /**< Connect to JEPA bidirectional */
    bool enable_predictive_hierarchy; /**< Connect to predictive hierarchy */
    bool enable_hopfield;             /**< Connect to Hopfield memory */
    bool enable_fep;                  /**< Connect to FEP orchestrator */

    /* Prediction settings */
    uint32_t phoneme_prediction_horizon;  /**< Phonemes to predict ahead */
    uint32_t word_prediction_horizon;     /**< Words to predict ahead */
    float prediction_confidence_threshold;/**< Min confidence for predictions */

    /* Precision */
    float default_precision;          /**< Default precision weight */
    bool enable_precision_modulation; /**< Allow precision updates */
    float precision_update_rate;      /**< Precision learning rate */

    /* Prediction error */
    bool enable_pe_reporting;         /**< Report prediction errors */
    float pe_threshold_phoneme;       /**< PE threshold for phonemes */
    float pe_threshold_word;          /**< PE threshold for words */
    float pe_threshold_semantic;      /**< PE threshold for semantics */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t update_interval_ms;      /**< Update cycle interval */
} language_omni_config_t;

//=============================================================================
// Immune Bridge Configuration
//=============================================================================

/**
 * @brief Language-immune bridge configuration
 *
 * BIOLOGICAL BASIS:
 * - Models effects of neuroinflammation on language
 * - Wernicke's aphasia symptoms from inflammation
 * - Cytokine effects on comprehension and production
 */
typedef struct {
    /* Connection enables */
    bool enable_brain_immune;         /**< Connect to brain immune system */
    bool enable_wernicke_immune;      /**< Wernicke inflammation effects */
    bool enable_broca_immune;         /**< Broca inflammation effects */
    bool enable_nlp_immune;           /**< NLP network immune effects */

    /* Cytokine sensitivity */
    float il1b_sensitivity;           /**< IL-1beta sensitivity */
    float il6_sensitivity;            /**< IL-6 sensitivity */
    float tnfa_sensitivity;           /**< TNF-alpha sensitivity */
    float il10_sensitivity;           /**< IL-10 sensitivity (recovery) */

    /* Inflammation thresholds */
    float mild_inflammation_threshold;/**< Threshold for mild effects */
    float moderate_inflammation_threshold;/**< Threshold for moderate effects */
    float severe_inflammation_threshold;/**< Threshold for severe effects */

    /* Effects */
    float comprehension_impairment_scale;  /**< Scale for comprehension impact */
    float production_impairment_scale;     /**< Scale for production impact */
    bool enable_aphasia_modeling;     /**< Enable aphasia symptom modeling */

    /* Recovery */
    float recovery_rate;              /**< Recovery rate when inflammation drops */
    bool enable_immune_memory;        /**< Remember inflammation patterns */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t update_interval_ms;      /**< Update cycle interval */
} language_immune_config_t;

//=============================================================================
// GPU Bridge Configuration
//=============================================================================

/**
 * @brief Language-GPU bridge configuration
 */
typedef struct {
    /* Enable flags */
    bool enable_gpu;                  /**< Enable GPU acceleration */
    bool enable_phoneme_gpu;          /**< GPU phoneme recognition */
    bool enable_lexical_gpu;          /**< GPU lexical access */
    bool enable_semantic_gpu;         /**< GPU semantic spreading */
    bool enable_embedding_gpu;        /**< GPU embedding operations */

    /* GPU settings */
    uint32_t device_id;               /**< GPU device to use */
    uint32_t batch_size;              /**< Processing batch size */
    bool enable_async_transfer;       /**< Async CPU-GPU transfer */
    bool enable_tensor_cores;         /**< Use tensor cores if available */

    /* Memory */
    size_t max_gpu_memory_mb;         /**< Max GPU memory allocation */
    bool enable_memory_pool;          /**< Use memory pooling */

    /* Batching */
    uint32_t phoneme_batch_threshold; /**< Min phonemes to batch */
    uint32_t word_batch_threshold;    /**< Min words to batch */
    float batch_timeout_ms;           /**< Max wait time for batching */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
} language_gpu_config_t;

//=============================================================================
// Thalamic Bridge Configuration
//=============================================================================

/**
 * @brief Language-thalamic bridge configuration
 *
 * Integration with thalamic router for signal gating and relay.
 */
#ifndef LANGUAGE_THALAMIC_CONFIG_T_DEFINED
#define LANGUAGE_THALAMIC_CONFIG_T_DEFINED
typedef struct {
    /* Enable flags */
    bool enable_attention_gating;      /**< Gate by attention level */
    bool enable_motor_priority;        /**< Prioritize motor commands */
    bool enable_semantic_routing;      /**< Route semantic to MD */
    bool enable_multimodal_routing;    /**< Route via pulvinar */

    /* Thresholds */
    float attention_threshold;         /**< Minimum attention for routing */
    float motor_priority_boost;        /**< Priority boost for motor */
    float gating_decay_rate;           /**< Gating decay per second */

    /* Timing */
    uint32_t relay_latency_us;         /**< Simulated relay latency */
    uint32_t gating_window_ms;         /**< Attention gating window */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    uint32_t update_interval_ms;       /**< Update cycle interval */
} language_thalamic_config_t;
#endif /* LANGUAGE_THALAMIC_CONFIG_T_DEFINED */

//=============================================================================
// Substrate Bridge Configuration
//=============================================================================

/**
 * @brief Language-substrate bridge configuration
 *
 * Metabolic modulation of language processing based on neural substrate state.
 */
#ifndef LANGUAGE_SUBSTRATE_CONFIG_T_DEFINED
#define LANGUAGE_SUBSTRATE_CONFIG_T_DEFINED
typedef struct {
    /* Enable flags */
    bool enable_atp_modulation;        /**< Modulate by ATP level */
    bool enable_fatigue_effects;       /**< Apply fatigue effects */
    bool enable_stress_effects;        /**< Apply stress effects */
    bool enable_neurotransmitter_effects; /**< Apply NT effects */

    /* Sensitivity settings */
    float atp_sensitivity;             /**< ATP effect strength [0-2] */
    float fatigue_sensitivity;         /**< Fatigue effect strength [0-2] */
    float stress_sensitivity;          /**< Stress effect strength [0-2] */

    /* Thresholds */
    float critical_atp_threshold;      /**< ATP level causing impairment */
    float high_fatigue_threshold;      /**< Fatigue level causing impairment */
    float optimal_stress_level;        /**< Optimal stress (Yerkes-Dodson) */

    /* Timing */
    uint32_t update_interval_ms;       /**< How often to update */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */
} language_substrate_config_t;
#endif /* LANGUAGE_SUBSTRATE_CONFIG_T_DEFINED */

//=============================================================================
// Logic Bridge Configuration
//=============================================================================

/**
 * @brief Language-logic bridge configuration
 *
 * Integration with symbolic logic module for reasoning about language.
 */
#ifndef LANGUAGE_LOGIC_CONFIG_T_DEFINED
#define LANGUAGE_LOGIC_CONFIG_T_DEFINED
typedef struct {
    /* Enable flags */
    bool enable_entailment_checking;   /**< Check semantic entailment */
    bool enable_consistency_checking;  /**< Check discourse consistency */
    bool enable_presupposition_check;  /**< Verify presuppositions */
    bool enable_reference_resolution;  /**< Logical reference binding */
    bool enable_implicature_reasoning; /**< Reason about implicatures */

    /* Reasoning parameters */
    uint32_t max_inference_depth;      /**< Maximum reasoning depth */
    uint32_t default_timeout_ms;       /**< Default reasoning timeout */
    float min_confidence_threshold;    /**< Minimum confidence to accept */

    /* Performance */
    bool enable_caching;               /**< Cache inference results */
    uint32_t cache_size;               /**< Inference cache size */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    uint32_t update_interval_ms;       /**< Update cycle interval */
} language_logic_config_t;
#endif /* LANGUAGE_LOGIC_CONFIG_T_DEFINED */

//=============================================================================
// Default Configuration Functions
//=============================================================================

/**
 * @brief Get default orchestrator configuration
 * @param config Output configuration structure
 */
void language_orchestrator_default_config(language_orchestrator_config_t* config);

/**
 * @brief Get default perception bridge configuration
 * @param config Output configuration structure
 */
void language_perception_default_config(language_perception_config_t* config);

/**
 * @brief Get default cognitive bridge configuration
 * @param config Output configuration structure
 */
void language_cognitive_default_config(language_cognitive_config_t* config);

/**
 * @brief Get default training bridge configuration
 * @param config Output configuration structure
 */
void language_training_default_config(language_training_config_t* config);

/**
 * @brief Get default omni bridge configuration
 * @param config Output configuration structure
 */
void language_omni_default_config(language_omni_config_t* config);

/**
 * @brief Get default immune bridge configuration
 * @param config Output configuration structure
 */
void language_immune_default_config(language_immune_config_t* config);

/**
 * @brief Get default GPU bridge configuration
 * @param config Output configuration structure
 */
void language_gpu_default_config(language_gpu_config_t* config);

/**
 * @brief Get default thalamic bridge configuration
 * @param config Output configuration structure
 */
void language_thalamic_default_config(language_thalamic_config_t* config);

/**
 * @brief Get default substrate bridge configuration
 * @param config Output configuration structure
 */
void language_substrate_default_config(language_substrate_config_t* config);

/**
 * @brief Get default logic bridge configuration
 * @param config Output configuration structure
 */
void language_logic_default_config(language_logic_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_CONFIG_H */
