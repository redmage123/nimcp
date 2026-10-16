/**
 * @file nimcp_genius_traits.h
 * @brief Genius profile trait structures and region configurations
 *
 * WHAT: Defines cognitive traits, region configs, connectivity, and eidetic memory
 * WHY:  Provides biologically-grounded parameters for genius brain configurations
 * HOW:  Based on neuroscience research into domain-specific cognitive abilities
 *
 * @version 1.0.0
 * @date 2026-02-03
 */

#ifndef NIMCP_GENIUS_TRAITS_H
#define NIMCP_GENIUS_TRAITS_H

#include "nimcp_genius_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * COGNITIVE TRAITS STRUCTURE
 * ============================================================================ */

/**
 * @brief Cognitive trait parameters for genius profiles
 *
 * All multipliers use 1.0 as baseline (normal human level).
 * Values > 1.0 indicate enhancement, < 1.0 indicate reduction.
 *
 * Biological basis:
 *   - Working memory: Prefrontal cortex dorsolateral region
 *   - Attention: Prefrontal-parietal network
 *   - Pattern recognition: Temporal and parietal cortex
 *   - Creativity: Default mode + executive network interaction
 */
typedef struct genius_traits_t {
    /* === Working Memory === */
    uint32_t working_memory_capacity;       /**< Items held (default 7, max 15) */
    float working_memory_decay_factor;      /**< Decay rate (1.0=normal, 0.1=slow) */
    float working_memory_refresh_efficiency;/**< Rehearsal effectiveness (1.0-3.0) */

    /* === Attention === */
    float sustained_attention_duration;     /**< Focus duration multiplier (1.0-5.0) */
    float attention_switching_cost;         /**< Switch cost (1.0=normal, 0.5=fast) */
    float multi_focus_capacity;             /**< Parallel attention streams (1.0-3.0) */
    float selective_attention_strength;     /**< Filter distractions (1.0-2.0) */

    /* === Pattern Recognition === */
    float pattern_sensitivity;              /**< Detect patterns (1.0-3.0) */
    float anomaly_detection;                /**< Notice irregularities (1.0-3.0) */
    float abstraction_level;                /**< Abstract thinking (1.0-3.0) */
    float association_strength;             /**< Cross-domain connections (1.0-3.0) */

    /* === Creativity === */
    float divergent_thinking;               /**< Novel idea generation (1.0-3.0) */
    float convergent_thinking;              /**< Solution finding (1.0-3.0) */
    float imagination_fluidity;             /**< Idea flow rate (1.0-3.0) */
    float originality;                      /**< Uniqueness of ideas (1.0-3.0) */

    /* === Mental Imagery === */
    float mental_imagery_vividness;         /**< Internal visualization (1.0-3.0) */
    float mental_rotation_speed;            /**< 3D manipulation speed (1.0-3.0) */
    float mental_simulation_accuracy;       /**< Simulation fidelity (1.0-3.0) */

    /* === Learning === */
    float skill_acquisition_rate;           /**< How fast skills are learned (1.0-3.0) */
    float memory_consolidation_rate;        /**< Memory strengthening (1.0-3.0) */
    float transfer_learning_ability;        /**< Cross-domain application (1.0-3.0) */
    float insight_frequency;                /**< Aha! moment frequency (1.0-3.0) */

    /* === Emotional/Motivational === */
    float stress_resilience;                /**< Performance under pressure (1.0-3.0) */
    float intrinsic_motivation;             /**< Self-driven persistence (1.0-3.0) */
    float flow_state_threshold;             /**< Ease of entering flow (1.0=normal, 0.3=easy) */
    float frustration_tolerance;            /**< Tolerance for setbacks (1.0-3.0) */

    /* === Financial-specific traits === */
    float temporal_discounting_factor;      /**< Patience (1.0=normal, 0.3=very patient) */
    float risk_calibration;                 /**< Probability accuracy (1.0-3.0) */
    float contrarian_threshold;             /**< Act against consensus (1.0-3.0) */
    float loss_aversion_resistance;         /**< Resist loss aversion (1.0-3.0) */

    /* === Eidetic Memory traits === */
    float eidetic_visual_strength;          /**< Visual memory (0.0-3.0) */
    float eidetic_auditory_strength;        /**< Auditory memory (0.0-3.0) */
    float eidetic_spatial_strength;         /**< Spatial memory (0.0-3.0) */
    float eidetic_verbal_strength;          /**< Verbal memory (0.0-3.0) */
    float memory_decay_resistance;          /**< Slower forgetting (1.0-10.0) */
    float detail_resolution;                /**< Fine-grained recall (1.0-3.0) */
    float mental_simulation_fidelity;       /**< Mental rehearsal accuracy (1.0-3.0) */
} genius_traits_t;

/**
 * @brief Initialize traits to baseline (normal human) values
 */
static inline void genius_traits_init_baseline(genius_traits_t* traits) {
    if (!traits) return;

    traits->working_memory_capacity = 7;
    traits->working_memory_decay_factor = 1.0f;
    traits->working_memory_refresh_efficiency = 1.0f;

    traits->sustained_attention_duration = 1.0f;
    traits->attention_switching_cost = 1.0f;
    traits->multi_focus_capacity = 1.0f;
    traits->selective_attention_strength = 1.0f;

    traits->pattern_sensitivity = 1.0f;
    traits->anomaly_detection = 1.0f;
    traits->abstraction_level = 1.0f;
    traits->association_strength = 1.0f;

    traits->divergent_thinking = 1.0f;
    traits->convergent_thinking = 1.0f;
    traits->imagination_fluidity = 1.0f;
    traits->originality = 1.0f;

    traits->mental_imagery_vividness = 1.0f;
    traits->mental_rotation_speed = 1.0f;
    traits->mental_simulation_accuracy = 1.0f;

    traits->skill_acquisition_rate = 1.0f;
    traits->memory_consolidation_rate = 1.0f;
    traits->transfer_learning_ability = 1.0f;
    traits->insight_frequency = 1.0f;

    traits->stress_resilience = 1.0f;
    traits->intrinsic_motivation = 1.0f;
    traits->flow_state_threshold = 1.0f;
    traits->frustration_tolerance = 1.0f;

    traits->temporal_discounting_factor = 1.0f;
    traits->risk_calibration = 1.0f;
    traits->contrarian_threshold = 1.0f;
    traits->loss_aversion_resistance = 1.0f;

    traits->eidetic_visual_strength = 0.0f;
    traits->eidetic_auditory_strength = 0.0f;
    traits->eidetic_spatial_strength = 0.0f;
    traits->eidetic_verbal_strength = 0.0f;
    traits->memory_decay_resistance = 1.0f;
    traits->detail_resolution = 1.0f;
    traits->mental_simulation_fidelity = 1.0f;
}

/* ============================================================================
 * REGION CONFIGURATION STRUCTURE
 * ============================================================================ */

/**
 * @brief Region-specific enhancement configuration
 *
 * Applied to brain region adapters (parietal, occipital, temporal, etc.)
 * Multipliers scale from baseline configuration.
 */
typedef struct genius_region_config_t {
    /* === Capacity Scaling === */
    float size_multiplier;                  /**< Neuron count scale (0.5-3.0) */
    float feature_capacity_multiplier;      /**< Feature detection capacity (0.5-3.0) */

    /* === Processing Enhancement === */
    float processing_speed_multiplier;      /**< Speed/latency (0.5-3.0) */
    float precision_multiplier;             /**< Accuracy/resolution (0.5-3.0) */
    float learning_rate_multiplier;         /**< Plasticity rate (0.5-3.0) */

    /* === Layer-specific Scaling === */
    float layer_2_3_multiplier;             /**< Associative layers */
    float layer_4_multiplier;               /**< Input layer (sensory) */
    float layer_5_multiplier;               /**< Output layer (motor) */
    float layer_6_multiplier;               /**< Feedback layer */

    /* === Region-specific Parameters === */
    float custom_params[GENIUS_MAX_REGION_PARAMS];  /**< Region-specific parameters */
    uint32_t custom_param_count;            /**< Number of custom params used */

    /* === Feature Flags === */
    uint32_t enable_flags;                  /**< Bitmask of enabled features */

    /* === SNN Configuration Overrides === */
    float tau_mem_override;                 /**< Membrane time constant (0=use default) */
    float tau_syn_override;                 /**< Synaptic time constant (0=use default) */
    float v_thresh_override;                /**< Spike threshold (0=use default) */
    float stdp_a_plus_multiplier;           /**< LTP strength multiplier */
    float stdp_a_minus_multiplier;          /**< LTD strength multiplier */
} genius_region_config_t;

/**
 * @brief Region feature enable flags
 */
typedef enum {
    GENIUS_REGION_ENABLE_ENHANCED_PLASTICITY    = (1 << 0),
    GENIUS_REGION_ENABLE_QUANTUM_OPTIMIZATION   = (1 << 1),
    GENIUS_REGION_ENABLE_EIDETIC_BUFFER         = (1 << 2),
    GENIUS_REGION_ENABLE_FAST_LEARNING          = (1 << 3),
    GENIUS_REGION_ENABLE_HIGH_PRECISION         = (1 << 4),
    GENIUS_REGION_ENABLE_IMMUNE_MODULATION      = (1 << 5),
    GENIUS_REGION_ENABLE_BIO_ASYNC              = (1 << 6),
    GENIUS_REGION_ENABLE_MESH_COORDINATION      = (1 << 7)
} genius_region_enable_flags_t;

/**
 * @brief Initialize region config to baseline
 */
static inline void genius_region_config_init_baseline(genius_region_config_t* config) {
    if (!config) return;

    config->size_multiplier = 1.0f;
    config->feature_capacity_multiplier = 1.0f;
    config->processing_speed_multiplier = 1.0f;
    config->precision_multiplier = 1.0f;
    config->learning_rate_multiplier = 1.0f;

    config->layer_2_3_multiplier = 1.0f;
    config->layer_4_multiplier = 1.0f;
    config->layer_5_multiplier = 1.0f;
    config->layer_6_multiplier = 1.0f;

    config->custom_param_count = 0;
    config->enable_flags = 0;

    config->tau_mem_override = 0.0f;
    config->tau_syn_override = 0.0f;
    config->v_thresh_override = 0.0f;
    config->stdp_a_plus_multiplier = 1.0f;
    config->stdp_a_minus_multiplier = 1.0f;
}

/* ============================================================================
 * CONNECTIVITY CONFIGURATION
 * ============================================================================ */

/**
 * @brief Inter-region connectivity enhancement configuration
 *
 * Multipliers for white matter tract strengths between brain regions.
 * Based on neuroscience research into genius brain connectivity patterns.
 */
typedef struct genius_connectivity_t {
    /* === Major Pathways === */
    float parietal_prefrontal;              /**< Number sense to executive */
    float parietal_occipital;               /**< Visuospatial integration */
    float temporal_prefrontal;              /**< Semantic to planning */
    float temporal_parietal;                /**< Auditory-spatial integration */
    float occipital_temporal;               /**< Ventral "what" stream */
    float occipital_parietal;               /**< Dorsal "where" stream */
    float prefrontal_hippocampus;           /**< Goal-directed memory */
    float prefrontal_amygdala;              /**< Emotional regulation */

    /* === Motor Pathways === */
    float cerebellum_motor;                 /**< Motor coordination */
    float cerebellum_prefrontal;            /**< Cognitive timing */
    float basal_ganglia_motor;              /**< Motor automation */
    float basal_ganglia_prefrontal;         /**< Habit-goal integration */

    /* === Corpus Callosum (Interhemispheric) === */
    float callosum_motor_gain;              /**< Motor transfer */
    float callosum_sensory_gain;            /**< Sensory transfer */
    float callosum_cognitive_gain;          /**< Cognitive transfer */

    /* === Memory Pathways === */
    float hippocampus_occipital;            /**< Visual memory encoding */
    float hippocampus_temporal;             /**< Auditory memory encoding */
    float hippocampus_parietal;             /**< Spatial memory encoding */
    float prefrontal_hippocampus_recall;    /**< Top-down memory retrieval */
    float hippocampus_cortical_consolidation;/**< Memory consolidation */

    /* === Language Pathways (for Literary genius) === */
    float broca_wernicke;                   /**< Language production-comprehension */
    float angular_gyrus_connection;         /**< Reading/writing */
    float semantic_network_strength;        /**< Word association */

    /* === Social Pathways (for Strategic genius) === */
    float prefrontal_insula;                /**< Social-emotional integration */
    float temporal_amygdala;                /**< Face-emotion processing */
    float theory_of_mind_network;           /**< Mentalizing network */
} genius_connectivity_t;

/**
 * @brief Initialize connectivity to baseline
 */
static inline void genius_connectivity_init_baseline(genius_connectivity_t* conn) {
    if (!conn) return;

    conn->parietal_prefrontal = 1.0f;
    conn->parietal_occipital = 1.0f;
    conn->temporal_prefrontal = 1.0f;
    conn->temporal_parietal = 1.0f;
    conn->occipital_temporal = 1.0f;
    conn->occipital_parietal = 1.0f;
    conn->prefrontal_hippocampus = 1.0f;
    conn->prefrontal_amygdala = 1.0f;

    conn->cerebellum_motor = 1.0f;
    conn->cerebellum_prefrontal = 1.0f;
    conn->basal_ganglia_motor = 1.0f;
    conn->basal_ganglia_prefrontal = 1.0f;

    conn->callosum_motor_gain = 1.0f;
    conn->callosum_sensory_gain = 1.0f;
    conn->callosum_cognitive_gain = 1.0f;

    conn->hippocampus_occipital = 1.0f;
    conn->hippocampus_temporal = 1.0f;
    conn->hippocampus_parietal = 1.0f;
    conn->prefrontal_hippocampus_recall = 1.0f;
    conn->hippocampus_cortical_consolidation = 1.0f;

    conn->broca_wernicke = 1.0f;
    conn->angular_gyrus_connection = 1.0f;
    conn->semantic_network_strength = 1.0f;

    conn->prefrontal_insula = 1.0f;
    conn->temporal_amygdala = 1.0f;
    conn->theory_of_mind_network = 1.0f;
}

/* ============================================================================
 * LATERALIZATION PROFILE
 * ============================================================================ */

/**
 * @brief Hemispheric lateralization profile for genius types
 *
 * Values: 0.0 = fully right-dominant, 0.5 = bilateral, 1.0 = fully left-dominant
 */
typedef struct genius_lateralization_t {
    float language_dominance;               /**< Language processing */
    float spatial_dominance;                /**< Spatial processing */
    float motor_fine_dominance;             /**< Fine motor control */
    float motor_gross_dominance;            /**< Gross motor control */
    float emotion_processing_dominance;     /**< Emotional processing */
    float attention_global_dominance;       /**< Global attention */
    float attention_local_dominance;        /**< Local/detail attention */
    float music_melody_dominance;           /**< Melody processing */
    float music_rhythm_dominance;           /**< Rhythm processing */
    float face_recognition_dominance;       /**< Face processing */
    float logical_reasoning_dominance;      /**< Logical/analytical */
    float creative_thinking_dominance;      /**< Creative/holistic */
    float mathematical_dominance;           /**< Mathematical reasoning */
    float verbal_memory_dominance;          /**< Verbal memory */
    float visual_memory_dominance;          /**< Visual memory */
} genius_lateralization_t;

/**
 * @brief Initialize lateralization to balanced (bilateral)
 */
static inline void genius_lateralization_init_balanced(genius_lateralization_t* lat) {
    if (!lat) return;

    lat->language_dominance = 0.7f;         /* Left-biased for most */
    lat->spatial_dominance = 0.4f;          /* Right-biased */
    lat->motor_fine_dominance = 0.6f;       /* Slight left */
    lat->motor_gross_dominance = 0.5f;      /* Bilateral */
    lat->emotion_processing_dominance = 0.4f; /* Right-biased */
    lat->attention_global_dominance = 0.4f; /* Right-biased */
    lat->attention_local_dominance = 0.6f;  /* Left-biased */
    lat->music_melody_dominance = 0.4f;     /* Right-biased */
    lat->music_rhythm_dominance = 0.6f;     /* Left-biased */
    lat->face_recognition_dominance = 0.3f; /* Right-biased */
    lat->logical_reasoning_dominance = 0.7f; /* Left-biased */
    lat->creative_thinking_dominance = 0.4f; /* Right-biased */
    lat->mathematical_dominance = 0.6f;     /* Left-biased */
    lat->verbal_memory_dominance = 0.7f;    /* Left-biased */
    lat->visual_memory_dominance = 0.4f;    /* Right-biased */
}

/* ============================================================================
 * EIDETIC MEMORY CONFIGURATION
 * ============================================================================ */

/**
 * @brief Per-system eidetic memory configuration
 *
 * Configures enhancements to each NIMCP memory subsystem.
 */

/** Working memory eidetic enhancements */
typedef struct {
    uint32_t capacity_boost;                /**< Additional items (0-8) */
    float decay_multiplier;                 /**< Decay slowdown (0.1-1.0) */
    float refresh_efficiency;               /**< Rehearsal boost (1.0-3.0) */
    bool enable_parallel_buffers;           /**< Multiple WM streams */
    uint32_t visual_buffer_size;            /**< Visual WM buffer */
    uint32_t auditory_buffer_size;          /**< Auditory WM buffer */
    uint32_t spatial_buffer_size;           /**< Spatial WM buffer */
} eidetic_working_memory_config_t;

/** Autobiographical memory eidetic enhancements */
typedef struct {
    uint32_t capacity_multiplier;           /**< Memory count multiplier (1-10) */
    float detail_preservation;              /**< Detail level (0.0-1.0) */
    float temporal_precision;               /**< Time recall accuracy (1.0-10.0) */
    float emotional_vividness;              /**< Emotion re-experience (1.0-3.0) */
    bool enable_flashbulb_mode;             /**< All memories vivid */
    float forgetting_resistance;            /**< Forgetting slowdown (1.0-10.0) */
} eidetic_autobiographical_config_t;

/** Semantic memory eidetic enhancements */
typedef struct {
    uint32_t concept_capacity_multiplier;   /**< Concept count (1-8) */
    uint32_t feature_dimension_boost;       /**< Feature richness (1-4) */
    float association_strength_boost;       /**< Link strength (1.0-3.0) */
    float cross_domain_activation;          /**< Cross-connection (1.0-3.0) */
    bool enable_instant_learning;           /**< One-shot concept formation */
    float spreading_activation_boost;       /**< Activation spread (1.0-3.0) */
} eidetic_semantic_config_t;

/** Hippocampus eidetic enhancements */
typedef struct {
    float dg_size_multiplier;               /**< Dentate gyrus scale (1.0-4.0) */
    float ca3_size_multiplier;              /**< CA3 scale (1.0-4.0) */
    float ca1_size_multiplier;              /**< CA1 scale (1.0-4.0) */
    float place_cell_multiplier;            /**< Place cell count (1.0-4.0) */
    float pattern_separation_ratio;         /**< Separation strength (5.0-20.0) */
    float pattern_completion_threshold;     /**< Completion ease (0.1-0.5) */
    float recurrence_strength;              /**< CA3 recurrence (1.0-3.0) */
    float encoding_speed;                   /**< Encoding rate (1.0-5.0) */
    float encoding_fidelity;                /**< Encoding accuracy (0.5-1.0) */
    bool single_exposure_learning;          /**< No repetition needed */
    float replay_fidelity;                  /**< Replay accuracy (0.5-1.0) */
    float ripple_frequency_boost;           /**< Ripple rate (1.0-3.0) */
} eidetic_hippocampus_config_t;

/** Consolidation eidetic enhancements */
typedef struct {
    float consolidation_speed;              /**< Speed multiplier (1.0-5.0) */
    float retention_boost;                  /**< Retention improvement (1.0-3.0) */
    float pruning_resistance;               /**< Resist pruning (1.0-5.0) */
    float replay_accuracy;                  /**< Replay fidelity (0.5-1.0) */
    bool enable_awake_consolidation;        /**< Consolidate while awake */
    float synaptic_scaling_resistance;      /**< Preserve strong memories (1.0-3.0) */
} eidetic_consolidation_config_t;

/** Systems consolidation (Phase M2) eidetic enhancements */
typedef struct {
    float cortical_transfer_rate;           /**< Transfer speed (1.0-5.0) */
    float semantic_extraction_boost;        /**< Abstraction rate (1.0-3.0) */
    float cortical_capacity_multiplier;     /**< Cortical storage (1-8) */
    float forgetting_resistance;            /**< Forgetting slowdown (1.0-10.0) */
    bool preserve_episodic_detail;          /**< Keep sensory details */
} eidetic_systems_consolidation_config_t;

/** Hopfield associative memory eidetic enhancements */
typedef struct {
    uint32_t pattern_capacity_multiplier;   /**< Pattern count (1-8) */
    uint32_t pattern_dimension_boost;       /**< Pattern size (1-4) */
    float inverse_temperature;              /**< Retrieval sharpness (1.0-20.0) */
    float retrieval_speed;                  /**< Retrieval rate (1.0-5.0) */
    bool enable_one_shot_storage;           /**< Single exposure storage */
} eidetic_hopfield_config_t;

/** Engram system eidetic enhancements */
typedef struct {
    uint32_t engram_capacity_multiplier;    /**< Engram count (1-8) */
    uint32_t neurons_per_engram_boost;      /**< Engram size (1-4) */
    float consolidation_speed;              /**< Consolidation rate (1.0-6.0) */
    float tagging_window_extension;         /**< Tag window (1.0-4.0) */
    float reactivation_threshold;           /**< Easier recall (0.1-0.5) */
    bool instant_consolidation;             /**< Skip labile state */
} eidetic_engram_config_t;

/** Procedural memory eidetic enhancements */
typedef struct {
    float acquisition_speed;                /**< Learning rate (1.0-5.0) */
    uint32_t chunk_size_boost;              /**< Chunk capacity (1-3) */
    float automation_speed;                 /**< Automatization (1.0-10.0) */
    float motor_precision_boost;            /**< Motor accuracy (1.0-3.0) */
    bool enable_mental_practice;            /**< Learn by visualization */
    float skill_retention;                  /**< Skill memory (1.0-10.0) */
} eidetic_procedural_config_t;

/** Prospective memory eidetic enhancements */
typedef struct {
    uint32_t intention_capacity_boost;      /**< Intention count (1-4) */
    float temporal_precision;               /**< Time accuracy (1.0-30.0) */
    float cue_sensitivity;                  /**< Cue detection (0.1-1.0) */
    float intention_retention;              /**< Retention (1.0-10.0) */
    bool automatic_scheduling;              /**< Self-organizing */
} eidetic_prospective_config_t;

/** Prime resonance memory eidetic enhancements */
typedef struct {
    float resonance_amplification;          /**< Pattern strength (1.0-5.0) */
    float coherence_maintenance;            /**< Coherence time (1.0-5.0) */
    uint32_t entanglement_boost;            /**< Entanglement count (1-4) */
    bool enable_superposition_memory;       /**< Multiple states */
} eidetic_prime_resonance_config_t;

/**
 * @brief Master eidetic memory configuration
 *
 * Combines all per-system eidetic configurations into a single structure.
 */
typedef struct eidetic_memory_config_t {
    /* === Modality Strengths (0.0-3.0) === */
    float visual_eidetic;                   /**< Visual memory strength */
    float auditory_eidetic;                 /**< Auditory memory strength */
    float spatial_eidetic;                  /**< Spatial memory strength */
    float verbal_eidetic;                   /**< Verbal memory strength */
    float numerical_eidetic;                /**< Numerical memory strength */
    float kinesthetic_eidetic;              /**< Motor memory strength */

    /* === Global Memory Characteristics === */
    float encoding_speed;                   /**< Global encoding rate (1.0-5.0) */
    float decay_resistance;                 /**< Global decay slowdown (1.0-10.0) */
    float retrieval_accuracy;               /**< Global retrieval (0.5-1.0) */
    float detail_granularity;               /**< Detail level (1.0-3.0) */

    /* === Mental Simulation === */
    float simulation_duration_sec;          /**< Image persistence (1.0-60.0) */
    float manipulation_capability;          /**< Transform images (1.0-3.0) */

    /* === Per-System Configurations === */
    eidetic_working_memory_config_t working_memory;
    eidetic_autobiographical_config_t autobiographical;
    eidetic_semantic_config_t semantic;
    eidetic_hippocampus_config_t hippocampus;
    eidetic_consolidation_config_t consolidation;
    eidetic_systems_consolidation_config_t systems_consolidation;
    eidetic_hopfield_config_t hopfield;
    eidetic_engram_config_t engram;
    eidetic_procedural_config_t procedural;
    eidetic_prospective_config_t prospective;
    eidetic_prime_resonance_config_t prime_resonance;

    /* === Integration Flags === */
    bool enable_cross_system_enhancement;   /**< Enhance all systems together */
    bool enable_sleep_independence;         /**< Less reliance on sleep */
    bool enable_instant_recall;             /**< No retrieval delay */
    bool enable_immune_modulation;          /**< Allow immune effects */
} eidetic_memory_config_t;

/**
 * @brief Initialize eidetic config to baseline (no enhancement)
 */
void eidetic_memory_config_init_baseline(eidetic_memory_config_t* config);

/**
 * @brief Get predefined eidetic configuration for Tesla (visual-spatial)
 */
const eidetic_memory_config_t* eidetic_config_tesla(void);

/**
 * @brief Get predefined eidetic configuration for Mozart (auditory)
 */
const eidetic_memory_config_t* eidetic_config_mozart(void);

/**
 * @brief Get predefined eidetic configuration for von Neumann (numerical)
 */
const eidetic_memory_config_t* eidetic_config_vonneumann(void);

/**
 * @brief Get predefined eidetic configuration for Kim Peek (encyclopedic)
 */
const eidetic_memory_config_t* eidetic_config_kim_peek(void);

/**
 * @brief Get predefined eidetic configuration for Wiltshire (visual-artistic)
 */
const eidetic_memory_config_t* eidetic_config_wiltshire(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_TRAITS_H */
