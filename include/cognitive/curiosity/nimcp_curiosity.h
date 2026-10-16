//=============================================================================
// nimcp_curiosity.h - Curiosity-Driven Learning System
//=============================================================================

#ifndef NIMCP_CURIOSITY_H
#define NIMCP_CURIOSITY_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/imagination/nimcp_imagination_callbacks.h"

/**
 * @file nimcp_curiosity.h
 * @brief Curiosity-driven learning system for human-like knowledge acquisition
 *
 * NIMCP Philosophy: Learn like a human infant
 * - Start with minimal knowledge
 * - Curiosity drives exploration
 * - Learn incrementally from experiences
 * - No massive pre-training required
 * - CPU-friendly, no GPUs needed
 *
 * Example usage:
 * ```c
 * // Create curiosity engine (like an infant's drive to explore)
 * curiosity_engine_t engine = curiosity_engine_create("infant_learner");
 *
 * // Infant encounters something new
 * knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "What is a cat?");
 *
 * // Curiosity generates questions
 * const char* questions[10];
 * uint32_t num_q = curiosity_generate_questions(engine, &gap, questions, 10);
 * // Output: "What does a cat look like?", "What sounds does it make?",
 * //         "How does it move?", "What does it eat?"
 *
 * // Learn from answers (incrementally)
 * curiosity_learn_answer(engine, questions[0], "A cat is a small furry animal");
 * curiosity_learn_answer(engine, questions[1], "Cats meow and purr");
 *
 * // Knowledge accumulates like an infant learning
 * ```
 */

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Curiosity engine handle
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct curiosity_engine_struct* curiosity_engine_t;

/**
 * @brief Knowledge gap detection
 */
typedef struct {
    char topic[256];           /**< What we don't know about */
    float gap_size;            /**< How much we don't know (0-1) */
    float curiosity_intensity; /**< How curious we are about it (0-1) */
    float learning_potential;  /**< Expected learning value (0-1) */
    uint32_t related_concepts; /**< Number of related known concepts */
    char** prerequisites;      /**< What we need to know first */
    uint32_t num_prerequisites;
} knowledge_gap_t;

/**
 * @brief Question types for knowledge seeking
 */
typedef enum {
    QUESTION_WHAT,  /**< "What is X?" - definitional */
    QUESTION_WHY,   /**< "Why does X happen?" - causal */
    QUESTION_HOW,   /**< "How does X work?" - mechanistic */
    QUESTION_WHEN,  /**< "When did X happen?" - temporal */
    QUESTION_WHERE, /**< "Where is X?" - spatial */
    QUESTION_WHO,   /**< "Who did X?" - agentive */
    QUESTION_WHICH  /**< "Which X?" - discriminative */
} question_type_t;

/**
 * @brief Generated question for knowledge seeking
 */
typedef struct {
    char question[512];   /**< The question text */
    question_type_t type; /**< Question type */
    float priority;       /**< How important to answer (0-1) */
    float difficulty;     /**< Expected difficulty (0-1) */
    char** search_terms;  /**< Suggested search terms */
    uint32_t num_search_terms;
} generated_question_t;

/**
 * @brief Learning motivation sources
 */
typedef enum {
    MOTIVATION_INTRINSIC, /**< Pure curiosity */
    MOTIVATION_GOAL,      /**< Needed for a goal */
    MOTIVATION_SOCIAL,    /**< Others find it important */
    MOTIVATION_SURVIVAL,  /**< Needed for safety/wellbeing */
    MOTIVATION_AESTHETIC  /**< Beautiful/interesting */
} motivation_source_t;

/**
 * @brief Learning motivation state
 */
typedef struct {
    float intrinsic_curiosity; /**< Pure desire to know (0-1) */
    float goal_relevance;      /**< Helps achieve goals (0-1) */
    float social_importance;   /**< Others value it (0-1) */
    float survival_value;      /**< Survival/safety importance (0-1) */
    float aesthetic_appeal;    /**< Beauty/interest (0-1) */
    float overall_motivation;  /**< Combined motivation (0-1) */
} motivation_state_t;

//=============================================================================
// Curiosity Engine Creation
//=============================================================================

/**
 * @brief Create curiosity engine
 *
 * BREAKING CHANGE: Now requires parent brain reference (module pattern)
 * Curiosity is a cognitive module that uses the parent brain's neuromodulator system.
 *
 * @param parent_brain Parent brain that owns this curiosity module
 * @param learner_name Name for this learner (e.g., "infant", "student")
 * @return Curiosity engine or NULL on error
 */
curiosity_engine_t curiosity_engine_create(brain_t parent_brain, const char* learner_name);

/**
 * @brief Destroy curiosity engine
 *
 * @param engine Engine to destroy
 */
void curiosity_engine_destroy(curiosity_engine_t engine);

//=============================================================================
// Knowledge Gap Detection
//=============================================================================

/**
 * @brief Detect knowledge gap
 *
 * When encountering new information, assess what we don't know
 *
 * @param engine Curiosity engine
 * @param concept Concept or topic encountered
 * @return Knowledge gap assessment
 */
knowledge_gap_t curiosity_detect_knowledge_gap(curiosity_engine_t engine, const char* concept_str);

/**
 * @brief Check if concept is known
 *
 * @param engine Curiosity engine
 * @param concept Concept to check
 * @return Familiarity score (0=unknown, 1=fully known)
 */
float curiosity_check_familiarity(curiosity_engine_t engine, const char* concept_str);

/**
 * @brief Get related concepts
 *
 * Find concepts related to current knowledge gap
 *
 * @param engine Curiosity engine
 * @param concept Central concept
 * @param related Output array for related concepts
 * @param max_related Maximum concepts to return
 * @return Number of related concepts found
 */
uint32_t curiosity_get_related_concepts(curiosity_engine_t engine, const char* concept_str,
                                        char** related, uint32_t max_related);

//=============================================================================
// Question Generation
//=============================================================================

/**
 * @brief Generate questions about knowledge gap
 *
 * Like an infant asking "What's that?" "Why?" "How?"
 *
 * @param engine Curiosity engine
 * @param gap Knowledge gap to explore
 * @param questions Output array for questions
 * @param max_questions Maximum questions to generate
 * @return Number of questions generated
 */
uint32_t curiosity_generate_questions(curiosity_engine_t engine, const knowledge_gap_t* gap,
                                      generated_question_t* questions, uint32_t max_questions);

/**
 * @brief Generate follow-up question
 *
 * After learning something, ask deeper questions
 *
 * @param engine Curiosity engine
 * @param previous_answer Previous answer learned
 * @return Follow-up question or NULL
 */
const char* curiosity_generate_followup(curiosity_engine_t engine, const char* previous_answer);

//=============================================================================
// Motivation & Drive
//=============================================================================

/**
 * @brief Assess learning motivation
 *
 * Why do we want to learn this? (intrinsic, goal, social, survival)
 *
 * @param engine Curiosity engine
 * @param concept Concept to learn
 * @return Motivation state
 */
motivation_state_t curiosity_assess_motivation(curiosity_engine_t engine, const char* concept_str);

/**
 * @brief Set intrinsic curiosity level
 *
 * Adjust the baseline drive to explore (like infant curiosity)
 *
 * @param engine Curiosity engine
 * @param level Curiosity level (0-1, typically 0.7-0.9 for infants)
 */
void curiosity_set_baseline(curiosity_engine_t engine, float level);

/**
 * @brief Get current motivation to learn
 *
 * @param engine Curiosity engine
 * @return Current overall motivation (0-1)
 */
float curiosity_get_drive(curiosity_engine_t engine);

//=============================================================================
// Incremental Learning
//=============================================================================

/**
 * @brief Learn from answer to question
 *
 * Incremental learning like an infant acquiring knowledge piece by piece
 *
 * @param engine Curiosity engine
 * @param question Question that was asked
 * @param answer Answer received
 * @return true on success
 */
bool curiosity_learn_answer(curiosity_engine_t engine, const char* question, const char* answer);

/**
 * @brief Learn from experience
 *
 * Learn from direct interaction (not just text)
 *
 * @param engine Curiosity engine
 * @param experience_description Description of experience
 * @param sensory_data Optional sensory features
 * @param num_features Number of features
 * @return true on success
 */
bool curiosity_learn_experience(curiosity_engine_t engine, const char* experience_description,
                                const float* sensory_data, uint32_t num_features);

/**
 * @brief Learn from observation
 *
 * Learn by watching (social learning)
 *
 * @param engine Curiosity engine
 * @param what_observed What was observed
 * @param context Context of observation
 * @return true on success
 */
bool curiosity_learn_observation(curiosity_engine_t engine, const char* what_observed,
                                 const char* context);

//=============================================================================
// Knowledge Search & Acquisition
//=============================================================================

/**
 * @brief Search callback for external knowledge sources
 *
 * Function signature for searching external sources (web, books, etc.)
 */
typedef char** (*knowledge_search_fn_t)(const char* query, void* context, uint32_t max_results,
                                        uint32_t* num_results);

/**
 * @brief Register knowledge source
 *
 * Connect to external knowledge (Wikipedia, books, web, etc.)
 *
 * @param engine Curiosity engine
 * @param source_name Name of knowledge source
 * @param search_fn Search function callback
 * @param context Context for search function
 * @return true on success
 */
bool curiosity_register_knowledge_source(curiosity_engine_t engine, const char* source_name,
                                         knowledge_search_fn_t search_fn, void* context);

/**
 * @brief Seek knowledge actively
 *
 * Given a knowledge gap, actively search for answers
 *
 * @param engine Curiosity engine
 * @param gap Knowledge gap to fill
 * @param results Output array for search results
 * @param max_results Maximum results
 * @return Number of results found
 */
uint32_t curiosity_seek_knowledge(curiosity_engine_t engine, const knowledge_gap_t* gap,
                                  char** results, uint32_t max_results);

//=============================================================================
// Learning Progress
//=============================================================================

/**
 * @brief Learning progress tracking
 */
typedef struct {
    uint64_t total_questions_asked;
    uint64_t total_answers_learned;
    uint64_t total_experiences;
    uint64_t concepts_learned;
    float knowledge_growth_rate; /**< Recent learning rate */
    float avg_curiosity;         /**< Average curiosity level */
    char current_focus[256];     /**< Current learning focus */
    bool enable_bio_async;       /**< Enable bio-async communication */
} learning_progress_t;

/**
 * @brief Get learning progress
 *
 * Track how much has been learned (like developmental milestones)
 *
 * @param engine Curiosity engine
 * @param progress Output progress structure
 * @return true on success
 */
bool curiosity_get_progress(curiosity_engine_t engine, learning_progress_t* progress);

/**
 * @brief Get knowledge domain coverage
 *
 * How much is known in different domains (literature, science, art, etc.)
 *
 * @param engine Curiosity engine
 * @param domain Domain name (e.g., "literature", "science", "ethics")
 * @return Coverage percentage (0-1)
 */
float curiosity_get_domain_coverage(curiosity_engine_t engine, const char* domain);

//=============================================================================
// Developmental Stages
//=============================================================================

/**
 * @brief Learning stage (like human development)
 */
typedef enum {
    STAGE_INFANT,     /**< Basic concepts, sensory learning */
    STAGE_TODDLER,    /**< Language acquisition, basic reasoning */
    STAGE_CHILD,      /**< Structured learning, reading, writing */
    STAGE_ADOLESCENT, /**< Abstract reasoning, complex domains */
    STAGE_ADULT,      /**< Specialized knowledge, expertise */
    STAGE_EXPERT      /**< Deep expertise in domains */
} learning_stage_t;

/**
 * @brief Get current learning stage
 *
 * @param engine Curiosity engine
 * @return Current developmental stage
 */
learning_stage_t curiosity_get_stage(curiosity_engine_t engine);

/**
 * @brief Set learning stage constraints
 *
 * Limit learning to age-appropriate concepts
 *
 * @param engine Curiosity engine
 * @param stage Target learning stage
 */
void curiosity_set_stage(curiosity_engine_t engine, learning_stage_t stage);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print knowledge gap summary
 */
void curiosity_print_gap(const knowledge_gap_t* gap);

/**
 * @brief Print generated question
 */
void curiosity_print_question(const generated_question_t* question);

/**
 * @brief Print learning progress
 */
void curiosity_print_progress(const learning_progress_t* progress);

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Set exploration rate based on cognitive load
 *
 * WHAT: Adjust exploration vs exploitation based on executive load
 * WHY:  Prevent exploration when cognitively overloaded
 * HOW:  Modulate curiosity intensity and exploration rate
 *
 * BIOLOGY: Prefrontal cortex regulates exploration/exploitation trade-off
 *
 * @param engine Curiosity engine
 * @param exploration_rate Exploration rate [0, 1] (0=exploit, 1=explore)
 */
void curiosity_set_exploration_rate(curiosity_engine_t engine, float exploration_rate);

/**
 * @brief Get information gain from recent learning
 *
 * WHAT: Query expected information gain from exploring
 * WHY:  Executive can prioritize exploratory tasks
 * HOW:  Return recent learning value
 *
 * @param engine Curiosity engine
 * @return Information gain [0, 1]
 */
float curiosity_get_information_gain(curiosity_engine_t engine);

//=============================================================================
// Immune System Integration (Brain Immune System)
//=============================================================================

/* Forward declarations to avoid circular dependencies */
struct brain_immune_system;
struct imagination_engine;
struct imagination_scenario;

typedef struct imagination_engine imagination_engine_t;
typedef struct imagination_scenario imagination_scenario_t;

/**
 * @brief Connect curiosity engine to brain immune system
 *
 * WHAT: Establish bidirectional immune-curiosity coupling
 * WHY:  Model sickness behavior (cytokines suppress exploration) and novelty vigilance
 * HOW:  Create curiosity_immune_bridge, register callbacks with immune system
 *
 * BIOLOGICAL BASIS:
 * - Immune → Curiosity: Cytokines (IL-1, IL-6, TNF-α) reduce dopaminergic exploration
 * - Curiosity → Immune: Novel stimuli trigger immune vigilance (stress response)
 *
 * @param engine Curiosity engine
 * @param immune_system Brain immune system to connect
 * @return 0 on success, -1 on error
 */
int curiosity_connect_immune(curiosity_engine_t engine, struct brain_immune_system* immune_system);

/**
 * @brief Disconnect from brain immune system
 *
 * WHAT: Tear down immune-curiosity coupling
 * WHY:  Clean shutdown, restore original curiosity levels
 * HOW:  Destroy bridge, unregister callbacks
 *
 * @param engine Curiosity engine
 * @return 0 on success, -1 on error
 */
int curiosity_disconnect_immune(curiosity_engine_t engine);

/**
 * @brief Get current sickness behavior suppression level
 *
 * WHAT: Query immune-induced curiosity suppression
 * WHY:  Diagnostic visibility into sickness behavior effects
 * HOW:  Return suppression factor from immune bridge
 *
 * @param engine Curiosity engine
 * @return Suppression factor (0-1, where 0=max suppression, 1=no suppression)
 */
float curiosity_get_immune_suppression(curiosity_engine_t engine);

/**
 * @brief Get current immune vigilance boost from novelty
 *
 * WHAT: Query curiosity-induced immune alertness
 * WHY:  Diagnostic visibility into novelty-immune coupling
 * HOW:  Return vigilance boost from immune bridge
 *
 * @param engine Curiosity engine
 * @return Immune vigilance boost (1.0-1.5x)
 */
float curiosity_get_novelty_vigilance_boost(curiosity_engine_t engine);

//=============================================================================
// Imagination Engine Integration
//=============================================================================

/**
 * @brief Connect curiosity engine to imagination engine
 *
 * WHAT: Establish bidirectional curiosity-imagination coupling
 * WHY:  Model exploratory imagination triggered by curiosity
 * HOW:  Create internal bridge, register callbacks with imagination engine
 *
 * BIOLOGICAL BASIS:
 * - Curiosity -> Imagination: Novel stimuli trigger exploratory mental simulation
 * - Imagination -> Curiosity: Imagined scenarios generate new questions/gaps
 * - Default Mode Network (imagination) activated during curiosity-driven exploration
 *
 * @param engine Curiosity engine
 * @param imag   Imagination engine to connect
 * @return 0 on success, -1 on error
 */
int curiosity_connect_imagination(curiosity_engine_t engine, imagination_engine_t* imag);

/**
 * @brief Set callback for imagination results
 *
 * WHAT: Register callback to receive imagination scenario results
 * WHY:  Allows curiosity module to evaluate imagined content for novelty
 * HOW:  Store callback and user_data, invoke when imagination completes
 *
 * @param engine    Curiosity engine
 * @param cb        Callback function for imagination results
 * @param user_data User context passed to callback
 * @return 0 on success, -1 on error
 */
int curiosity_set_imagination_callback(curiosity_engine_t engine,
                                       imagination_result_callback_t cb,
                                       void* user_data);

/**
 * @brief Evaluate novelty of an imagined scenario
 *
 * WHAT: Assess how novel/surprising an imagination scenario is
 * WHY:  Drive curiosity-based exploration of interesting mental simulations
 * HOW:  Compare scenario content against known concepts and experiences
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic prediction error from novel imagined content
 * - High novelty increases motivation to explore further
 *
 * @param engine   Curiosity engine
 * @param scenario Imagination scenario to evaluate
 * @return Novelty score [0.0-1.0] (0=familiar, 1=highly novel)
 */
float curiosity_evaluate_imagined_novelty(curiosity_engine_t engine,
                                          imagination_scenario_t* scenario);

/**
 * @brief Request exploratory imagination from connected engine
 *
 * WHAT: Trigger imagination to explore current knowledge gaps
 * WHY:  Proactive exploration through mental simulation
 * HOW:  Send request to imagination engine based on curiosity state
 *
 * BIOLOGICAL BASIS:
 * - Curiosity triggers Default Mode Network for hypothetical exploration
 * - Mental simulation of "what would happen if..."
 *
 * @param engine Curiosity engine
 * @return 0 on success, -1 on error (e.g., no imagination connected)
 */
int curiosity_request_exploratory_imagination(curiosity_engine_t engine);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/**
 * @brief Select exploration action using epsilon-greedy MC strategy
 *
 * @param engine Curiosity engine
 * @param epsilon Exploration probability [0, 1]
 * @return true if should explore, false if should exploit
 */
bool curiosity_should_explore_mc(curiosity_engine_t engine, float epsilon);

/**
 * @brief Sample topic to explore using novelty-weighted MC sampling
 *
 * @param engine Curiosity engine
 * @param concepts Array of candidate concepts
 * @param num_concepts Number of candidates
 * @return Index of selected concept
 */
uint32_t curiosity_sample_exploration_target_mc(
    curiosity_engine_t engine,
    const char** concepts,
    uint32_t num_concepts);

/**
 * @brief Estimate information gain via MC simulation
 *
 * @param engine Curiosity engine
 * @param topic Topic to evaluate
 * @param num_simulations Number of MC simulations
 * @return Expected information gain [0, 1]
 */
float curiosity_estimate_info_gain_mc(
    curiosity_engine_t engine,
    const char* topic,
    uint32_t num_simulations);

/**
 * @brief Select question using softmax MC sampling
 *
 * @param engine Curiosity engine
 * @param questions Array of generated questions
 * @param num_questions Number of questions
 * @param temperature Softmax temperature (higher = more random)
 * @return Index of selected question
 */
uint32_t curiosity_sample_question_mc(
    curiosity_engine_t engine,
    const generated_question_t* questions,
    uint32_t num_questions,
    float temperature);

/**
 * @brief Add exploration noise to curiosity intensity
 *
 * @param intensity Base curiosity intensity
 * @param noise_scale Scale of noise (std dev)
 * @return Noisy intensity clamped to [0, 1]
 */
float curiosity_add_exploration_noise_mc(float intensity, float noise_scale);

/**
 * @brief Get thread-local MC seed for curiosity module
 *
 * @return Pointer to thread-local seed
 */
uint32_t* curiosity_get_mc_seed(void);

//=============================================================================
// Empowerment QMC API (Step 10 MC Integration)
//=============================================================================

/**
 * @brief Empowerment result for curiosity decisions
 *
 * BIOLOGICAL: Empowerment measures the agent's capacity to influence
 * future states through its actions - a key intrinsic motivation signal.
 */
typedef struct {
    float empowerment;              /**< Empowerment in bits */
    float empowerment_normalized;   /**< Normalized to [0,1] */
    float entropy_current;          /**< Current state entropy */
    float entropy_reachable;        /**< Reachable states entropy */
    uint32_t action_count;          /**< Number of available actions */
} curiosity_empowerment_t;

/**
 * @brief Compute empowerment for knowledge state
 *
 * WHAT: Measure capacity to influence future knowledge states
 * WHY:  Empowerment drives exploration toward controllable regions
 * HOW:  MC estimation of mutual information I(A; S')
 *
 * BIOLOGICAL: Models the intrinsic motivation to seek controllable outcomes
 *
 * @param engine Curiosity engine
 * @param concept_name Concept to evaluate empowerment for
 * @param horizon Number of steps to look ahead
 * @param result Output empowerment result
 * @return 0 on success, -1 on error
 */
int curiosity_compute_empowerment(
    curiosity_engine_t engine,
    const char* concept_name,
    uint32_t horizon,
    curiosity_empowerment_t* result
);

/**
 * @brief Select concept by empowerment-weighted sampling
 *
 * WHAT: Sample concept proportional to empowerment
 * WHY:  Balance curiosity with sense of control
 * HOW:  Softmax sampling with empowerment as temperature
 *
 * @param engine Curiosity engine
 * @param concepts Array of candidate concepts
 * @param num_concepts Number of candidates
 * @param temperature Sampling temperature (higher = more random)
 * @return Index of selected concept
 */
uint32_t curiosity_sample_by_empowerment(
    curiosity_engine_t engine,
    const char** concepts,
    uint32_t num_concepts,
    float temperature
);

/**
 * @brief Compute intrinsic reward from empowerment
 *
 * WHAT: Map empowerment to intrinsic motivation signal
 * WHY:  Drive learning toward empowering states
 * HOW:  r_intrinsic = alpha * empowerment + beta * novelty
 *
 * @param engine Curiosity engine
 * @param concept_name Concept just explored
 * @param alpha Weight for empowerment component
 * @param beta Weight for novelty component
 * @return Intrinsic reward signal [0, 1]
 */
float curiosity_compute_intrinsic_reward(
    curiosity_engine_t engine,
    const char* concept_name,
    float alpha,
    float beta
);

/**
 * @brief Estimate expected empowerment change
 *
 * WHAT: Predict how exploring a concept will change empowerment
 * WHY:  Prioritize actions that increase future control
 * HOW:  MC simulation of empowerment before/after exploration
 *
 * @param engine Curiosity engine
 * @param concept_name Concept to evaluate
 * @param num_simulations Number of MC simulations
 * @return Expected empowerment change (can be negative)
 */
float curiosity_estimate_empowerment_change(
    curiosity_engine_t engine,
    const char* concept_name,
    uint32_t num_simulations
);

#ifdef __cplusplus
}
#endif
#endif  // NIMCP_CURIOSITY_H
