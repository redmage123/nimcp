/**
 * @file nimcp_intuition_integrations.h
 * @brief Integration & Extrapolation Framework for Phase 6 Intuitive Reasoning
 *
 * WHAT: Cross-system integration layer connecting intuitive reasoning engines
 *       to training, memory, attention, and other cognitive systems
 * WHY:  Enable genuine knowledge creation by leveraging all cognitive resources
 * HOW:  Bridge patterns connecting Phase 6 engines to existing NIMCP systems
 *
 * BIOLOGICAL BASIS:
 * Human intuition emerges from the integration of multiple brain systems:
 * - Prefrontal cortex (executive control) guides reasoning strategy
 * - Hippocampus (episodic memory) provides experiential context
 * - Amygdala (emotion) contributes "gut feelings"
 * - Basal ganglia (procedural memory) provides learned heuristics
 * - Default mode network enables incubation and insight
 *
 * CAPABILITIES:
 * - Training integration: Learn from successful/failed intuitions
 * - Memory integration: Draw from episodic and semantic memory
 * - Attention integration: Focus intuitive processing
 * - Extrapolation: Predict beyond known data
 * - Knowledge synthesis: Combine multiple knowledge sources
 *
 * USAGE:
 * ```c
 * intuition_system_t* system = intuition_system_create();
 *
 * // Attach to working memory
 * intuition_attach_working_memory(system, working_mem);
 *
 * // Extrapolate from known data
 * extrapolation_t* ext = intuition_extrapolate(system, data, count, range);
 *
 * // Synthesize knowledge from fragments
 * synthesis_t* synth = intuition_synthesize_knowledge(system, fragments, count);
 *
 * intuition_system_destroy(system);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_INTUITION_INTEGRATIONS_H
#define NIMCP_INTUITION_INTEGRATIONS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

/* Include Phase 6 engine headers */
#include "cognitive/parietal/nimcp_intuitive_reasoning.h"
#include "cognitive/parietal/nimcp_analogical_reasoning.h"
#include "cognitive/parietal/nimcp_insight_discovery.h"
#include "cognitive/parietal/nimcp_hypothesis_generation.h"
#include "cognitive/parietal/nimcp_conceptual_blending.h"
#include "cognitive/parietal/nimcp_counterfactual.h"
#include "cognitive/parietal/nimcp_meta_reasoning.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum extrapolation range */
#define INTUITION_MAX_EXTRAPOLATION_RANGE   1000

/** Maximum data points for extrapolation */
#define INTUITION_MAX_DATA_POINTS           4096

/** Maximum knowledge fragments for synthesis */
#define INTUITION_MAX_KNOWLEDGE_FRAGMENTS   256

/** Maximum identified gaps */
#define INTUITION_MAX_GAPS                  64

/** Maximum generated questions */
#define INTUITION_MAX_QUESTIONS             32

/** Maximum predictions */
#define INTUITION_MAX_PREDICTIONS           64

/** Maximum trend description length */
#define INTUITION_MAX_TREND_DESC            256

/** Bio-async module ID for intuition integrations */
#define BIO_MODULE_INTUITION_INTEGRATIONS   0x03A7

/* ============================================================================
 * FORWARD DECLARATIONS FOR EXTERNAL SYSTEMS
 * ============================================================================ */

/* Memory systems */
struct working_memory;
typedef struct working_memory working_memory_t;

struct semantic_memory_system;
typedef struct semantic_memory_system semantic_memory_t;

struct episodic_memory_system;
typedef struct episodic_memory_system episodic_memory_t;

/* Cognitive systems */
struct attention_system;
typedef struct attention_system attention_system_t;

struct executive_function;
typedef struct executive_function executive_function_t;

struct emotion_system;
typedef struct emotion_system emotion_system_t;

/* Logic and reasoning */
#ifndef NIMCP_LOGIC_GATE_NETWORK_T_DEFINED
#define NIMCP_LOGIC_GATE_NETWORK_T_DEFINED
typedef struct logic_gate_network_struct* logic_gate_network_t;
#endif

/* Training */
#ifndef NIMCP_TRAINING_ENGINE_T_DEFINED
#define NIMCP_TRAINING_ENGINE_T_DEFINED
typedef struct training_engine_struct* training_engine_t;
#endif

/* ============================================================================
 * OPAQUE HANDLES
 * ============================================================================ */

/** Opaque handle for integrated intuition system */
typedef struct intuition_system intuition_system_t;

/* ============================================================================
 * DATA POINT & RANGE TYPES
 * ============================================================================ */

/**
 * @brief Data point for extrapolation
 */
typedef struct {
    float* values;          /**< Feature values */
    uint32_t dim;           /**< Dimensionality */
    float timestamp;        /**< Temporal coordinate */
    float confidence;       /**< Data point reliability [0,1] */
    char label[64];         /**< Optional label */
} intuition_data_point_t;

/**
 * @brief Range specification for extrapolation
 */
typedef struct {
    float start;            /**< Range start value */
    float end;              /**< Range end value */
    uint32_t num_samples;   /**< Number of points to predict */
    bool is_temporal;       /**< Is this a time-based range? */
} intuition_range_t;

/**
 * @brief Detected trend in data
 */
typedef struct {
    uint32_t id;
    char description[INTUITION_MAX_TREND_DESC];
    float slope;            /**< Overall trend direction */
    float intercept;        /**< Trend baseline */
    float curvature;        /**< Second-order trend */
    float period;           /**< Periodicity if detected */
    float r_squared;        /**< Fit quality [0,1] */
    bool is_monotonic;      /**< True if consistently increasing/decreasing */
    bool has_seasonality;   /**< True if periodic pattern detected */
} intuition_trend_t;

/**
 * @brief Validity boundary for extrapolation
 */
typedef struct {
    float lower_bound;      /**< Lower safe extrapolation limit */
    float upper_bound;      /**< Upper safe extrapolation limit */
    float confidence_decay; /**< How fast confidence drops beyond bounds */
    char warning[128];      /**< Warning message for boundary violations */
} intuition_boundary_t;

/* ============================================================================
 * EXTRAPOLATION TYPES
 * ============================================================================ */

/**
 * @brief Extrapolation result
 *
 * Contains predictions beyond the known data range along with
 * confidence measures and validity bounds.
 */
typedef struct {
    uint32_t id;

    /* Source data */
    intuition_data_point_t** known_data;    /**< Existing observations */
    uint32_t num_known;

    /* Predictions */
    intuition_data_point_t** extrapolated;  /**< Predicted beyond known range */
    uint32_t num_extrapolated;

    /* Analysis */
    intuition_trend_t* detected_trend;      /**< Underlying pattern */
    float extrapolation_confidence;         /**< Decreases with distance */
    intuition_boundary_t* validity_bounds;  /**< Where extrapolation is reasonable */

    /* Metadata */
    float max_reliable_distance;            /**< How far extrapolation is reliable */
    bool uses_ensemble;                     /**< Multiple models combined */
    uint32_t ensemble_size;                 /**< Number of models if ensemble */
} extrapolation_t;

/* ============================================================================
 * KNOWLEDGE SYNTHESIS TYPES
 * ============================================================================ */

/**
 * @brief Knowledge fragment from various sources
 */
typedef struct {
    uint32_t id;
    char description[256];      /**< What this fragment represents */
    float* content;             /**< Encoded content vector */
    uint32_t content_dim;
    float confidence;           /**< Source reliability [0,1] */
    float relevance;            /**< Task relevance [0,1] */
    uint64_t source_id;         /**< ID of source system */
    uint32_t source_type;       /**< Type of source (memory, perception, etc.) */
    uint64_t timestamp;         /**< When acquired */
} knowledge_fragment_t;

/**
 * @brief Identified gap in intuitive knowledge
 *
 * Note: Named intuition_gap_t to avoid conflict with curiosity module's knowledge_gap_t
 */
typedef struct {
    uint32_t id;
    char description[256];      /**< What's missing */
    float importance;           /**< How critical is this gap [0,1] */
    float fillability;          /**< Can we acquire this? [0,1] */
    uint32_t* related_fragments;/**< Fragments that touch this gap */
    uint32_t num_related;
} intuition_gap_t;

/**
 * @brief Contradiction between knowledge sources
 */
typedef struct {
    uint32_t id;
    uint32_t fragment1_id;      /**< First conflicting fragment */
    uint32_t fragment2_id;      /**< Second conflicting fragment */
    char description[256];      /**< Nature of contradiction */
    float severity;             /**< How serious is this conflict [0,1] */
    float resolution_confidence;/**< Can we resolve this? [0,1] */
    char suggested_resolution[256];
} knowledge_contradiction_t;

/**
 * @brief Synthesized unified knowledge
 */
typedef struct {
    float* unified_representation; /**< Combined knowledge vector */
    uint32_t dim;
    float coherence;              /**< Internal consistency [0,1] */
    float coverage;               /**< How much of fragments captured [0,1] */
    char summary[512];            /**< Human-readable summary */
} unified_knowledge_t;

/**
 * @brief Knowledge synthesis result
 */
typedef struct {
    uint32_t id;

    /* Input */
    knowledge_fragment_t** sources; /**< Input knowledge pieces */
    uint32_t num_sources;

    /* Output */
    unified_knowledge_t* synthesized; /**< Unified understanding */

    /* Analysis */
    intuition_gap_t** identified_gaps;      /**< What's still unknown */
    uint32_t num_gaps;
    knowledge_contradiction_t** conflicts;  /**< Inconsistencies found */
    uint32_t num_conflicts;

    /* Quality metrics */
    float synthesis_confidence;  /**< Overall synthesis quality [0,1] */
    float novelty_score;         /**< New insights from synthesis [0,1] */
} synthesis_t;

/* ============================================================================
 * GENERATED QUESTION TYPES
 * ============================================================================ */

/**
 * @brief Generated question to fill intuition gap
 *
 * Note: Named intuition_question_t to avoid conflict with curiosity module's generated_question_t
 */
typedef struct {
    uint32_t id;
    char question[256];         /**< The question text */
    uint32_t gap_id;            /**< Which gap this addresses */
    float priority;             /**< How important to answer [0,1] */
    float answerability;        /**< Likelihood of getting answer [0,1] */
    char suggested_source[128]; /**< Where to look for answer */
} intuition_question_t;

/* ============================================================================
 * NOVEL PREDICTION TYPES
 * ============================================================================ */

/**
 * @brief Domain specification for novel predictions
 */
typedef struct {
    uint32_t id;
    char name[64];              /**< Domain name */
    float* feature_bounds;      /**< Min/max for each dimension */
    uint32_t dim;
    float exploration_factor;   /**< How far outside training to go [0,1] */
} prediction_domain_t;

/**
 * @brief Novel prediction result
 */
typedef struct {
    uint32_t id;
    float* prediction;          /**< Predicted values */
    uint32_t dim;
    float confidence;           /**< Prediction confidence [0,1] */
    float novelty;              /**< How novel is this [0,1] */
    bool is_out_of_distribution;/**< Beyond training distribution? */
    char rationale[256];        /**< Why this prediction was made */
} novel_prediction_t;

/* ============================================================================
 * EXPERIENCE TYPES FOR TRAINING
 * ============================================================================ */

/**
 * @brief Experience record for learning from intuitions
 */
typedef struct {
    uint32_t id;
    hunch_t* hunch;             /**< The intuition */
    float predicted_outcome;    /**< What was expected */
    float actual_outcome;       /**< What happened */
    float timestamp;            /**< When this occurred */
    bool was_successful;        /**< Did intuition prove correct? */
} intuition_experience_t;

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Configuration for intuition integration system
 */
typedef struct {
    /* Sub-engine enablement */
    bool enable_intuitive_engine;
    bool enable_analogical_engine;
    bool enable_insight_engine;
    bool enable_hypothesis_engine;
    bool enable_blending_engine;
    bool enable_counterfactual_engine;
    bool enable_meta_engine;

    /* Integration enablement */
    bool enable_training_integration;
    bool enable_memory_integration;
    bool enable_attention_integration;
    bool enable_emotion_integration;
    bool enable_logic_validation;

    /* Extrapolation settings */
    float extrapolation_confidence_decay;   /**< How fast confidence drops */
    float min_extrapolation_confidence;     /**< Threshold for predictions */
    uint32_t max_extrapolation_steps;       /**< Maximum steps beyond data */

    /* Synthesis settings */
    float synthesis_coherence_threshold;    /**< Min coherence for synthesis */
    float gap_importance_threshold;         /**< Min importance to report */
    float contradiction_severity_threshold; /**< Min severity to report */

    /* Biological modulation */
    float inflammation_sensitivity;         /**< Response to inflammation [0,2] */
    float fatigue_sensitivity;              /**< Response to fatigue [0,2] */
} intuition_system_config_t;

/**
 * @brief Statistics for intuition integration system
 */
typedef struct {
    /* Extrapolation stats */
    uint32_t extrapolations_performed;
    uint32_t successful_extrapolations;
    float avg_extrapolation_accuracy;

    /* Synthesis stats */
    uint32_t syntheses_performed;
    uint32_t gaps_identified;
    uint32_t contradictions_found;
    uint32_t contradictions_resolved;

    /* Learning stats */
    uint32_t intuitions_trained;
    uint32_t intuitions_confirmed;
    uint32_t intuitions_refuted;
    float avg_intuition_accuracy;

    /* Integration stats */
    uint32_t memory_queries;
    uint32_t attention_focuses;
    uint32_t logic_validations;
    uint32_t logic_validation_passes;

    /* Novel prediction stats */
    uint32_t novel_predictions_made;
    uint32_t novel_predictions_confirmed;
} intuition_system_stats_t;

/* ============================================================================
 * LIFECYCLE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @return Default configuration values
 */
intuition_system_config_t intuition_system_default_config(void);

/**
 * @brief Create integrated intuition system
 * @return New system instance or NULL on failure
 */
intuition_system_t* intuition_system_create(void);

/**
 * @brief Create integrated intuition system with custom config
 * @param config Configuration parameters
 * @return New system instance or NULL on failure
 */
intuition_system_t* intuition_system_create_custom(const intuition_system_config_t* config);

/**
 * @brief Destroy intuition system
 * @param system System to destroy
 */
void intuition_system_destroy(intuition_system_t* system);

/* ============================================================================
 * ATTACHMENT FUNCTIONS - TRAINING
 * ============================================================================ */

/**
 * @brief Attach training engine for learning from intuitions
 * @param system Intuition system
 * @param training Training engine to attach
 * @return 0 on success, -1 on failure
 */
int intuition_attach_training(intuition_system_t* system, training_engine_t* training);

/**
 * @brief Learn from successful/failed intuitions
 * @param system Intuition system
 * @param hunch The hunch that was tested
 * @param actual_outcome What actually happened [0,1]
 * @return 0 on success, -1 on failure
 */
int intuition_feedback_success(intuition_system_t* system, const hunch_t* hunch,
                               float actual_outcome);

/**
 * @brief Update priors based on confirmed theory
 * @param system Intuition system
 * @param confirmed_theory Theory that was confirmed
 * @return 0 on success, -1 on failure
 */
int intuition_update_priors(intuition_system_t* system,
                            const hypogen_theory_t* confirmed_theory);

/**
 * @brief Train intuition from batch of experiences
 * @param system Intuition system
 * @param experiences Array of past experiences
 * @param count Number of experiences
 * @return 0 on success, -1 on failure
 */
int intuition_train_from_experience(intuition_system_t* system,
                                    const intuition_experience_t** experiences,
                                    uint32_t count);

/* ============================================================================
 * ATTACHMENT FUNCTIONS - MEMORY SYSTEMS
 * ============================================================================ */

/**
 * @brief Attach working memory for maintaining active hunches
 * @param system Intuition system
 * @param wm Working memory system
 * @return 0 on success, -1 on failure
 */
int intuition_attach_working_memory(intuition_system_t* system, working_memory_t* wm);

/**
 * @brief Attach episodic memory for drawing from past experiences
 * @param system Intuition system
 * @param episodic Episodic memory system
 * @return 0 on success, -1 on failure
 */
int intuition_attach_episodic_memory(intuition_system_t* system, episodic_memory_t* episodic);

/**
 * @brief Attach semantic memory for accessing conceptual knowledge
 * @param system Intuition system
 * @param semantic Semantic memory system
 * @return 0 on success, -1 on failure
 */
int intuition_attach_semantic_memory(intuition_system_t* system, semantic_memory_t* semantic);

/* ============================================================================
 * ATTACHMENT FUNCTIONS - COGNITIVE SYSTEMS
 * ============================================================================ */

/**
 * @brief Attach attention system for focus control
 * @param system Intuition system
 * @param attention Attention system
 * @return 0 on success, -1 on failure
 */
int intuition_attach_attention(intuition_system_t* system, attention_system_t* attention);

/**
 * @brief Attach executive function for strategy guidance
 * @param system Intuition system
 * @param executive Executive function system
 * @return 0 on success, -1 on failure
 */
int intuition_attach_executive(intuition_system_t* system, executive_function_t* executive);

/**
 * @brief Attach emotion system for gut feelings
 * @param system Intuition system
 * @param emotion Emotion system
 * @return 0 on success, -1 on failure
 */
int intuition_attach_emotion(intuition_system_t* system, emotion_system_t* emotion);

/**
 * @brief Attach logic gates for formal validation
 * @param system Intuition system
 * @param logic Logic gate network
 * @return 0 on success, -1 on failure
 */
int intuition_attach_logic_gates(intuition_system_t* system, logic_gate_network_t* logic);

/* ============================================================================
 * LOGIC VALIDATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Validate an intuitive leap with formal logic
 * @param system Intuition system
 * @param hunch Hunch to validate
 * @return true if logically consistent, false otherwise
 */
bool intuition_validate_with_logic(intuition_system_t* system, const hunch_t* hunch);

/**
 * @brief Refine intuitive hypothesis using logic
 * @param system Intuition system
 * @param hunch Hunch to refine
 * @return Refined hypothesis or NULL if cannot refine
 *
 * @note Caller must free returned hypothesis with hypothesis_free_theory()
 */
hypogen_theory_t* intuition_logic_refine(intuition_system_t* system, const hunch_t* hunch);

/* ============================================================================
 * EXTRAPOLATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Extrapolate from existing knowledge
 * @param system Intuition system
 * @param known Array of known data points
 * @param count Number of known points
 * @param target_range Range to extrapolate into
 * @return Extrapolation result or NULL on failure
 *
 * @note Caller must free with extrapolation_free()
 */
extrapolation_t* intuition_extrapolate(intuition_system_t* system,
                                       const intuition_data_point_t** known,
                                       uint32_t count,
                                       const intuition_range_t* target_range);

/**
 * @brief Incrementally update extrapolation with new data
 * @param system Intuition system
 * @param previous Previous extrapolation result
 * @param new_data New data points
 * @param new_count Number of new points
 * @return Updated extrapolation or NULL on failure
 *
 * @note Caller must free with extrapolation_free()
 */
extrapolation_t* intuition_extrapolate_incremental(intuition_system_t* system,
                                                   const extrapolation_t* previous,
                                                   const intuition_data_point_t** new_data,
                                                   uint32_t new_count);

/**
 * @brief Detect when extrapolation breaks down
 * @param system Intuition system
 * @param extrapolation The extrapolation to check
 * @param actual Actual observed data point
 * @return true if extrapolation failed significantly
 */
bool intuition_detect_extrapolation_failure(intuition_system_t* system,
                                            const extrapolation_t* extrapolation,
                                            const intuition_data_point_t* actual);

/**
 * @brief Free extrapolation result
 * @param ext Extrapolation to free
 */
void extrapolation_free(extrapolation_t* ext);

/* ============================================================================
 * NOVEL PREDICTION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Generate novel predictions beyond training distribution
 * @param system Intuition system
 * @param domain Domain constraints
 * @param num_predictions Output: number of predictions generated
 * @return Array of novel predictions or NULL on failure
 *
 * @note Caller must free with novel_prediction_free()
 */
novel_prediction_t** intuition_predict_novel(intuition_system_t* system,
                                             const prediction_domain_t* domain,
                                             uint32_t* num_predictions);

/**
 * @brief Free novel prediction
 * @param pred Prediction to free
 */
void novel_prediction_free(novel_prediction_t* pred);

/* ============================================================================
 * KNOWLEDGE SYNTHESIS FUNCTIONS
 * ============================================================================ */

/**
 * @brief Synthesize knowledge from multiple sources
 * @param system Intuition system
 * @param fragments Input knowledge pieces
 * @param count Number of fragments
 * @return Synthesis result or NULL on failure
 *
 * @note Caller must free with synthesis_free()
 */
synthesis_t* intuition_synthesize_knowledge(intuition_system_t* system,
                                            const knowledge_fragment_t** fragments,
                                            uint32_t count);

/**
 * @brief Identify gaps in current understanding
 * @param system Intuition system
 * @param domain Domain to analyze
 * @param num_gaps Output: number of gaps found
 * @return Array of intuition gaps or NULL on failure
 *
 * @note Caller must free each gap with intuition_gap_free()
 */
intuition_gap_t** intuition_identify_knowledge_gaps(intuition_system_t* system,
                                                    const prediction_domain_t* domain,
                                                    uint32_t* num_gaps);

/**
 * @brief Generate questions to fill intuition gaps
 * @param system Intuition system
 * @param gaps Identified gaps
 * @param num_gaps Number of gaps
 * @param num_questions Output: number of questions generated
 * @return Array of questions or NULL on failure
 *
 * @note Caller must free with intuition_question_free()
 */
intuition_question_t** intuition_generate_questions(intuition_system_t* system,
                                                    const intuition_gap_t** gaps,
                                                    uint32_t num_gaps,
                                                    uint32_t* num_questions);

/**
 * @brief Free synthesis result
 * @param synth Synthesis to free
 */
void synthesis_free(synthesis_t* synth);

/**
 * @brief Free intuition gap
 * @param gap Gap to free
 */
void intuition_gap_free(intuition_gap_t* gap);

/**
 * @brief Free intuition question
 * @param question Question to free
 */
void intuition_question_free(intuition_question_t* question);

/* ============================================================================
 * DATA POINT HELPERS
 * ============================================================================ */

/**
 * @brief Create data point
 * @param values Feature values (copied)
 * @param dim Dimensionality
 * @param timestamp Temporal coordinate
 * @param confidence Reliability [0,1]
 * @return New data point or NULL on failure
 */
intuition_data_point_t* intuition_data_point_create(const float* values, uint32_t dim,
                                                    float timestamp, float confidence);

/**
 * @brief Free data point
 * @param point Point to free
 */
void intuition_data_point_free(intuition_data_point_t* point);

/**
 * @brief Create knowledge fragment
 * @param description Description text
 * @param content Encoded content (copied)
 * @param dim Content dimensionality
 * @param confidence Source reliability [0,1]
 * @return New fragment or NULL on failure
 */
knowledge_fragment_t* knowledge_fragment_create(const char* description,
                                                const float* content, uint32_t dim,
                                                float confidence);

/**
 * @brief Free knowledge fragment
 * @param fragment Fragment to free
 */
void knowledge_fragment_free(knowledge_fragment_t* fragment);

/* ============================================================================
 * SUB-ENGINE ACCESS
 * ============================================================================ */

/**
 * @brief Get intuitive reasoning engine
 * @param system Intuition system
 * @return Intuitive engine or NULL if not enabled
 */
intuitive_engine_t* intuition_get_intuitive_engine(intuition_system_t* system);

/**
 * @brief Get analogical reasoning engine
 * @param system Intuition system
 * @return Analogical engine or NULL if not enabled
 */
analogical_engine_t* intuition_get_analogical_engine(intuition_system_t* system);

/**
 * @brief Get insight discovery engine
 * @param system Intuition system
 * @return Insight engine or NULL if not enabled
 */
insight_engine_t* intuition_get_insight_engine(intuition_system_t* system);

/**
 * @brief Get hypothesis generation engine
 * @param system Intuition system
 * @return Hypothesis engine or NULL if not enabled
 */
hypothesis_engine_t* intuition_get_hypothesis_engine(intuition_system_t* system);

/**
 * @brief Get conceptual blending engine
 * @param system Intuition system
 * @return Blending engine or NULL if not enabled
 */
blending_engine_t* intuition_get_blending_engine(intuition_system_t* system);

/**
 * @brief Get counterfactual reasoning engine
 * @param system Intuition system
 * @return Counterfactual engine or NULL if not enabled
 */
counterfactual_engine_t* intuition_get_counterfactual_engine(intuition_system_t* system);

/**
 * @brief Get meta-reasoning engine
 * @param system Intuition system
 * @return Meta engine or NULL if not enabled
 */
meta_engine_t* intuition_get_meta_engine(intuition_system_t* system);

/* ============================================================================
 * BIOLOGICAL MODULATION
 * ============================================================================ */

/**
 * @brief Set inflammation level
 * @param system Intuition system
 * @param level Inflammation level [0,1]
 * @return 0 on success, -1 on failure
 */
int intuition_system_set_inflammation(intuition_system_t* system, float level);

/**
 * @brief Set fatigue level
 * @param system Intuition system
 * @param level Fatigue level [0,1]
 * @return 0 on success, -1 on failure
 */
int intuition_system_set_fatigue(intuition_system_t* system, float level);

/* ============================================================================
 * STATISTICS & DIAGNOSTICS
 * ============================================================================ */

/**
 * @brief Get system statistics
 * @param system Intuition system
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int intuition_system_get_stats(const intuition_system_t* system,
                               intuition_system_stats_t* stats);

/**
 * @brief Reset statistics
 * @param system Intuition system
 */
void intuition_system_reset_stats(intuition_system_t* system);

/**
 * @brief Get last error message
 * @return Error message string (thread-local)
 */
const char* intuition_system_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTUITION_INTEGRATIONS_H */
