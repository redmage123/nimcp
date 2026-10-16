/**
 * @file nimcp_curiosity_enhanced.h
 * @brief Enhanced Curiosity System with 10 Advanced Features
 * @version 1.0.0
 * @date 2025-12-20
 *
 * WHAT: Comprehensive curiosity enhancement module with biologically-inspired features
 * WHY:  Model complete curiosity dynamics including boredom, satiation, anxiety,
 *       social curiosity, meta-cognition, and counterfactual reasoning
 * HOW:  Modular design with bridges to emotion, anxiety, social, and learning systems
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ENHANCEMENT 1 - BOREDOM & UNDERSTIMULATION:
 * - Dopaminergic system shows reduced activity during monotony
 * - Locus coeruleus signals novelty-seeking when understimulated
 * - Reference: Westgate & Wilson (2018) "Boring thoughts and bored minds"
 *
 * ENHANCEMENT 2 - INTEREST DECAY & SATIATION:
 * - Habituation via synaptic depression
 * - Sensory-specific satiety in reward circuits
 * - Hyperbolic discounting of familiar stimuli
 * - Reference: Gottlieb et al. (2013) "Information-seeking, curiosity, and attention"
 *
 * ENHANCEMENT 3 - CURIOSITY TYPE DIFFERENTIATION:
 * - Diversive: LC-NE driven broad scanning
 * - Specific: VTA-DA driven focused investigation
 * - Perceptual: Sensory novelty processing
 * - Epistemic: Prefrontal knowledge-seeking
 * - Reference: Litman (2005) "Curiosity and the pleasures of learning"
 *
 * ENHANCEMENT 4 - CURIOSITY-ANXIETY BALANCE:
 * - Amygdala-prefrontal competition
 * - Approach-avoidance conflict resolution
 * - BIS/BAS system integration
 * - Reference: Spielberger & Starr (1994) "Curiosity and exploratory behavior"
 *
 * ENHANCEMENT 5 - SOCIAL CURIOSITY & GOSSIP:
 * - Theory of Mind integration
 * - Social information foraging
 * - Reputation and coalition tracking
 * - Reference: Dunbar (2004) "Gossip in evolutionary perspective"
 *
 * ENHANCEMENT 6 - META-CURIOSITY:
 * - Prefrontal metacognition
 * - Self-reflection on epistemic states
 * - Blind spot identification
 * - Reference: Metcalfe & Finn (2008) "Metacognition and curiosity"
 *
 * ENHANCEMENT 7 - CURIOSITY CONTAGION:
 * - Mirror neuron systems
 * - Social facilitation of exploration
 * - Observational learning of interests
 * - Reference: Herrmann et al. (2007) "Humans have evolved specialized skills"
 *
 * ENHANCEMENT 8 - SURPRISE-DRIVEN LEARNING RATE:
 * - Dopaminergic prediction error signals
 * - Enhanced synaptic plasticity under surprise
 * - Priority encoding of unexpected events
 * - Reference: Schultz (2016) "Dopamine reward prediction error coding"
 *
 * ENHANCEMENT 9 - CURIOSITY FATIGUE & RECOVERY:
 * - Prefrontal depletion model
 * - Consolidation period requirements
 * - Recovery dynamics
 * - Reference: Baumeister et al. (1998) "Ego depletion"
 *
 * ENHANCEMENT 10 - COUNTERFACTUAL CURIOSITY:
 * - Prefrontal simulation of alternatives
 * - Regret-based learning
 * - "What if" reasoning
 * - Reference: Roese (1997) "Counterfactual thinking"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CURIOSITY_ENHANCED_H
#define NIMCP_CURIOSITY_ENHANCED_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/curiosity/nimcp_curiosity.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CURIOSITY_ENH_VERSION_MAJOR    1
#define CURIOSITY_ENH_VERSION_MINOR    0
#define CURIOSITY_ENH_VERSION_PATCH    0

/* Boredom thresholds */
#define BOREDOM_THRESHOLD_DEFAULT      0.3f
#define BOREDOM_MONOTONY_DECAY         0.01f
#define BOREDOM_NOVELTY_SEEK_BOOST     1.5f

/* Interest decay parameters */
#define INTEREST_DECAY_RATE_DEFAULT    0.02f
#define SATIATION_THRESHOLD            0.8f
#define INTEREST_HALF_LIFE_MS          300000  /* 5 minutes */

/* Curiosity types */
#define CURIOSITY_TYPE_COUNT           6

/* Anxiety balance */
#define ANXIETY_SUPPRESS_THRESHOLD     0.7f
#define APPROACH_AVOIDANCE_NEUTRAL     0.5f

/* Social curiosity */
#define MAX_SOCIAL_TARGETS             64
#define GOSSIP_INTEREST_DECAY          0.05f

/* Meta-curiosity */
#define MAX_BLIND_SPOTS                32
#define META_AWARENESS_THRESHOLD       0.6f

/* Curiosity contagion */
#define CONTAGION_SUSCEPTIBILITY_DEFAULT  0.5f
#define CONTAGION_DECAY_RATE           0.1f

/* Surprise learning */
#define SURPRISE_LR_BOOST_MAX          3.0f
#define SURPRISE_THRESHOLD_DEFAULT     0.5f

/* Fatigue parameters */
#define FATIGUE_ACCUMULATION_RATE      0.001f
#define FATIGUE_RECOVERY_RATE          0.005f
#define FATIGUE_REST_THRESHOLD         0.7f

/* Counterfactual */
#define MAX_COUNTERFACTUALS            16
#define COUNTERFACTUAL_EXPLORATION_COST  0.1f

/* Bio-async module IDs */
#define BIO_MODULE_CURIOSITY_BOREDOM       0x0700
#define BIO_MODULE_CURIOSITY_INTEREST      0x0701
#define BIO_MODULE_CURIOSITY_TYPES         0x0702
#define BIO_MODULE_CURIOSITY_ANXIETY       0x0703
#define BIO_MODULE_CURIOSITY_SOCIAL        0x0704
#define BIO_MODULE_CURIOSITY_META          0x0705
#define BIO_MODULE_CURIOSITY_CONTAGION     0x0706
#define BIO_MODULE_CURIOSITY_SURPRISE      0x0707
#define BIO_MODULE_CURIOSITY_FATIGUE       0x0708
#define BIO_MODULE_CURIOSITY_COUNTERFACT   0x0709

/* ============================================================================
 * Enhancement 1: Boredom & Understimulation
 * ============================================================================ */

/**
 * @brief Boredom state tracking
 *
 * BIOLOGICAL: Models dopaminergic hypoactivity during monotony
 */
typedef struct curiosity_boredom_state_s {
    float monotony_level;               /**< Repetitive stimuli detection [0,1] */
    float understimulation_duration_ms; /**< Time since last novelty */
    float boredom_threshold;            /**< Threshold for boredom trigger */
    float novelty_seeking_boost;        /**< Exploration boost when bored */
    float last_stimulus_novelty;        /**< Novelty of last stimulus */
    uint64_t last_novel_event_ms;       /**< Timestamp of last novel event */
    uint32_t repetition_count;          /**< Count of repeated stimuli */
    bool is_bored;                      /**< Current boredom state */
} curiosity_boredom_state_t;

/**
 * @brief Boredom detection configuration
 */
typedef struct curiosity_boredom_config_s {
    float boredom_threshold;            /**< Level to trigger boredom */
    float monotony_decay_rate;          /**< How fast monotony accumulates */
    float novelty_boost_factor;         /**< Boost when seeking novelty */
    float understimulation_timeout_ms;  /**< Time before understimulation */
    bool enable_auto_novelty_seek;      /**< Auto-trigger exploration */
} curiosity_boredom_config_t;

/* ============================================================================
 * Enhancement 2: Interest Decay & Satiation
 * ============================================================================ */

/**
 * @brief Topic interest tracking with decay
 *
 * BIOLOGICAL: Models habituation and sensory-specific satiety
 */
typedef struct curiosity_topic_interest_s {
    char topic[256];                    /**< Topic identifier */
    float initial_interest;             /**< Interest when first encountered */
    float current_interest;             /**< Current interest level */
    float decay_rate;                   /**< Topic-specific decay rate */
    float satiation_level;              /**< How "full" of knowledge */
    float novelty_score;                /**< Novelty score for quantum exploration */
    uint64_t exposure_count;            /**< Number of exposures */
    uint64_t last_exposure_ms;          /**< Last exposure timestamp */
    uint64_t first_exposure_ms;         /**< First encounter timestamp */
    float peak_interest;                /**< Maximum interest reached */
} curiosity_topic_interest_t;

/**
 * @brief Interest decay configuration
 */
typedef struct curiosity_interest_config_s {
    float base_decay_rate;              /**< Default decay rate */
    float satiation_threshold;          /**< When topic is "saturated" */
    uint64_t half_life_ms;              /**< Time for interest to halve */
    float residual_interest_min;        /**< Minimum residual interest */
    bool enable_hyperbolic_decay;       /**< Use hyperbolic discounting */
} curiosity_interest_config_t;

/* ============================================================================
 * Enhancement 3: Curiosity Type Differentiation
 * ============================================================================ */

/**
 * @brief Types of curiosity
 *
 * BIOLOGICAL: Different neural circuits for different curiosity modes
 */
typedef enum {
    CURIOSITY_TYPE_DIVERSIVE = 0,       /**< Broad exploration (LC-NE) */
    CURIOSITY_TYPE_SPECIFIC,            /**< Focused investigation (VTA-DA) */
    CURIOSITY_TYPE_PERCEPTUAL,          /**< Sensory novelty */
    CURIOSITY_TYPE_EPISTEMIC,           /**< Knowledge-seeking */
    CURIOSITY_TYPE_SOCIAL,              /**< People/relationship curiosity */
    CURIOSITY_TYPE_MORBID               /**< Threat-related (controlled exposure) */
} curiosity_type_t;

/**
 * @brief Curiosity type profile
 */
typedef struct curiosity_type_profile_s {
    curiosity_type_t dominant_type;     /**< Currently dominant type */
    float type_intensities[CURIOSITY_TYPE_COUNT]; /**< Intensity per type */
    float transition_matrix[CURIOSITY_TYPE_COUNT][CURIOSITY_TYPE_COUNT]; /**< Transition probs */
    uint64_t type_durations_ms[CURIOSITY_TYPE_COUNT]; /**< Time spent per type */
    uint64_t last_transition_ms;        /**< Last type change */
} curiosity_type_profile_t;

/**
 * @brief Curiosity type configuration
 */
typedef struct curiosity_type_config_s {
    float type_weights[CURIOSITY_TYPE_COUNT]; /**< Base weights per type */
    float transition_threshold;         /**< Threshold for type change */
    bool enable_dynamic_switching;      /**< Allow automatic switching */
} curiosity_type_config_t;

/* ============================================================================
 * Enhancement 4: Curiosity-Anxiety Balance
 * ============================================================================ */

/**
 * @brief Approach-avoidance conflict state
 *
 * BIOLOGICAL: Models amygdala-prefrontal competition
 */
typedef struct curiosity_approach_avoidance_s {
    float approach_tendency;            /**< Curiosity drive [0,1] */
    float avoidance_tendency;           /**< Fear/anxiety [0,1] */
    float conflict_level;               /**< When both high [0,1] */
    float net_motivation;               /**< approach - avoidance */
    float resolution_bias;              /**< Tendency to approach vs avoid */
    bool in_conflict;                   /**< Currently in conflict */
    uint64_t conflict_start_ms;         /**< When conflict began */
    uint32_t approach_wins;             /**< Times approach won */
    uint32_t avoidance_wins;            /**< Times avoidance won */
} curiosity_approach_avoidance_t;

/**
 * @brief Anxiety bridge configuration
 */
typedef struct curiosity_anxiety_config_s {
    float anxiety_suppress_threshold;   /**< Anxiety level to suppress curiosity */
    float approach_bias;                /**< Baseline approach bias */
    float conflict_resolution_rate;     /**< How fast conflict resolves */
    bool enable_gradual_exposure;       /**< Allow cautious exploration */
} curiosity_anxiety_config_t;

/* Forward declaration for anxiety system */
typedef struct anxiety_system_s anxiety_system_t;

/* ============================================================================
 * Enhancement 5: Social Curiosity & Gossip
 * ============================================================================ */

/**
 * @brief Social curiosity target
 */
typedef struct curiosity_social_target_s {
    char agent_id[64];                  /**< Target agent identifier */
    float social_interest;              /**< Interest in this agent [0,1] */
    float trust_level;                  /**< Trust in agent [0,1] */
    float information_value;            /**< Expected info from agent */
    float relationship_depth;           /**< Relationship closeness */
    uint64_t last_interaction_ms;       /**< Last interaction time */
    uint32_t interaction_count;         /**< Number of interactions */
    bool is_tracked;                    /**< Actively tracking this agent */
} curiosity_social_target_t;

/**
 * @brief Social curiosity state
 */
typedef struct curiosity_social_state_s {
    float gossip_interest;              /**< Interest in social info [0,1] */
    float reputation_tracking;          /**< Curiosity about reputations */
    float coalition_mapping;            /**< Who is allied with whom */
    float deception_detection;          /**< Curiosity about hidden motives */
    curiosity_social_target_t targets[MAX_SOCIAL_TARGETS]; /**< Tracked agents */
    uint32_t num_targets;               /**< Number of tracked agents */
    uint32_t gossip_events_received;    /**< Gossip events processed */
    uint32_t gossip_events_shared;      /**< Gossip events shared */
} curiosity_social_state_t;

/**
 * @brief Social curiosity configuration
 */
typedef struct curiosity_social_config_s {
    float gossip_decay_rate;            /**< Interest decay for social info */
    float trust_influence;              /**< How trust affects curiosity */
    float coalition_update_rate;        /**< Coalition model update rate */
    bool enable_deception_vigilance;    /**< Track potential deception */
} curiosity_social_config_t;

/* Forward declaration for ToM system - NOTE: theory_of_mind_t is already defined in nimcp_brain.h as a pointer type */

/* Forward declaration for quantum bridge */
typedef struct curiosity_quantum_bridge_s curiosity_quantum_bridge_t;

/* ============================================================================
 * Quantum Monte Carlo Integration for Curiosity
 * ============================================================================ */

/**
 * @brief QMC configuration for curiosity uncertainty estimation
 *
 * WHAT: Configure QMC-based uncertainty quantification
 * WHY:  Better decision-making with uncertainty-aware exploration
 * HOW:  Use MC sampling to estimate confidence in curiosity assessments
 */
typedef struct curiosity_qmc_config_s {
    uint32_t num_samples;           /**< MC samples for estimation (default: 1000) */
    uint32_t burnin;                /**< Burn-in for MCMC (default: 100) */
    float uncertainty_threshold;     /**< Threshold for high uncertainty [0,1] */
    float exploration_bonus;         /**< Bonus for high-uncertainty topics [0,1] */
    bool enable_empowerment;         /**< Enable empowerment calculation */
    uint32_t empowerment_horizon;    /**< Steps for empowerment lookahead */
    float temperature;               /**< Temperature for Boltzmann sampling */
    uint32_t seed;                   /**< RNG seed (0 = time-based) */
} curiosity_qmc_config_t;

/**
 * @brief Result of QMC uncertainty estimation
 */
typedef struct curiosity_qmc_uncertainty_s {
    float mean_interest;            /**< Mean interest estimate */
    float variance;                 /**< Interest variance */
    float std_error;                /**< Standard error */
    float confidence_95_lower;      /**< 95% CI lower bound */
    float confidence_95_upper;      /**< 95% CI upper bound */
    float epistemic_uncertainty;    /**< Model uncertainty (reducible) */
    float aleatoric_uncertainty;    /**< Inherent randomness (irreducible) */
    uint32_t effective_samples;     /**< Effective sample size */
} curiosity_qmc_uncertainty_t;

/**
 * @brief Result of empowerment calculation
 *
 * BIOLOGICAL: Empowerment measures the agent's capacity to influence
 * future states - a key intrinsic motivation signal
 */
typedef struct curiosity_empowerment_result_s {
    float empowerment;              /**< Total empowerment (bits) */
    float empowerment_normalized;   /**< Normalized to [0,1] */
    float* action_empowerment;      /**< Empowerment per action */
    uint32_t num_actions;           /**< Number of actions */
    float entropy_current;          /**< Current state entropy */
    float entropy_reachable;        /**< Reachable states entropy */
    float channel_capacity;         /**< Information channel capacity */
} curiosity_empowerment_result_t;

/**
 * @brief QMC statistics for curiosity module
 */
typedef struct curiosity_qmc_stats_s {
    uint64_t uncertainty_estimations;   /**< Total uncertainty estimates */
    uint64_t empowerment_calculations;  /**< Total empowerment calcs */
    uint64_t mc_samples_total;          /**< Total MC samples drawn */
    uint64_t high_uncertainty_topics;   /**< Topics with high uncertainty */
    float avg_epistemic_uncertainty;    /**< Average epistemic uncertainty */
    float avg_empowerment;              /**< Average empowerment */
    float total_exploration_bonus;      /**< Total bonus from uncertainty */
} curiosity_qmc_stats_t;

/* ============================================================================
 * Enhancement 6: Meta-Curiosity
 * ============================================================================ */

/**
 * @brief Meta-curiosity state (curiosity about curiosity)
 *
 * BIOLOGICAL: Prefrontal metacognition
 */
typedef struct curiosity_meta_state_s {
    float self_awareness_of_interests;  /**< Awareness of own curiosity */
    float curiosity_pattern_recognition;/**< Recognition of curiosity patterns */
    float learning_strategy_awareness;  /**< Awareness of learning approach */
    char* identified_blind_spots[MAX_BLIND_SPOTS]; /**< Known blind spots */
    uint32_t num_blind_spots;           /**< Number of identified blind spots */
    float meta_curiosity_level;         /**< Curiosity about own curiosity */
    uint64_t last_introspection_ms;     /**< Last self-reflection */
    float introspection_depth;          /**< Depth of self-analysis */
} curiosity_meta_state_t;

/**
 * @brief Meta-curiosity configuration
 */
typedef struct curiosity_meta_config_s {
    float introspection_frequency_ms;   /**< How often to introspect */
    float blind_spot_detection_threshold; /**< Threshold for blind spots */
    float meta_awareness_growth_rate;   /**< Growth of meta-awareness */
    bool enable_automatic_introspection;/**< Auto-trigger introspection */
} curiosity_meta_config_t;

/* ============================================================================
 * Enhancement 7: Curiosity Contagion
 * ============================================================================ */

/**
 * @brief Curiosity contagion event
 *
 * BIOLOGICAL: Mirror neuron systems, social facilitation
 */
typedef struct curiosity_contagion_event_s {
    char observed_agent[64];            /**< Who was observed */
    char topic_of_interest[256];        /**< What they were curious about */
    float observed_curiosity_intensity; /**< Their curiosity level */
    float contagion_strength;           /**< How much it affects self */
    uint64_t observation_time_ms;       /**< When observed */
    bool was_adopted;                   /**< Did we adopt this curiosity */
} curiosity_contagion_event_t;

/**
 * @brief Curiosity contagion state
 */
typedef struct curiosity_contagion_state_s {
    float contagion_susceptibility;     /**< How susceptible to contagion */
    float accumulated_contagion;        /**< Accumulated contagion effects */
    curiosity_contagion_event_t recent_events[16]; /**< Recent events */
    uint32_t num_recent_events;         /**< Number of recent events */
    uint32_t total_contagion_events;    /**< Total events observed */
    uint32_t curiosities_adopted;       /**< Curiosities adopted from others */
} curiosity_contagion_state_t;

/**
 * @brief Contagion configuration
 */
typedef struct curiosity_contagion_config_s {
    float base_susceptibility;          /**< Default susceptibility */
    float trust_susceptibility_factor;  /**< How trust affects susceptibility */
    float contagion_decay_rate;         /**< Decay of contagion effects */
    bool enable_selective_contagion;    /**< Only from trusted sources */
} curiosity_contagion_config_t;

/* ============================================================================
 * Enhancement 8: Surprise-Driven Learning Rate
 * ============================================================================ */

/**
 * @brief Surprise-based learning modulation
 *
 * BIOLOGICAL: Dopaminergic prediction error signals
 */
typedef struct curiosity_surprise_learning_s {
    float surprise_magnitude;           /**< How unexpected [0,1] */
    float learning_rate_boost;          /**< Proportional LR increase */
    float memory_consolidation_priority;/**< Priority for memory */
    float attention_capture_strength;   /**< How much attention captured */
    float prediction_error;             /**< Difference from expected */
    uint64_t last_surprise_ms;          /**< Last surprise event */
    uint32_t surprise_count;            /**< Total surprise events */
    float avg_surprise;                 /**< Running average */
} curiosity_surprise_learning_t;

/**
 * @brief Surprise learning configuration
 */
typedef struct curiosity_surprise_config_s {
    float surprise_threshold;           /**< Threshold to trigger boost */
    float max_lr_boost;                 /**< Maximum learning rate boost */
    float surprise_decay_rate;          /**< How fast surprise decays */
    float memory_priority_factor;       /**< Memory consolidation factor */
} curiosity_surprise_config_t;

/* ============================================================================
 * Enhancement 9: Curiosity Fatigue & Recovery
 * ============================================================================ */

/**
 * @brief Curiosity fatigue state
 *
 * BIOLOGICAL: Prefrontal depletion model
 */
typedef struct curiosity_fatigue_state_s {
    float exploration_fatigue;          /**< Accumulated exploration cost */
    float cognitive_depletion;          /**< Mental energy drain */
    float recovery_rate;                /**< Current recovery rate */
    float rest_threshold;               /**< When to stop exploring */
    uint64_t last_rest_period_ms;       /**< Last rest start */
    uint64_t total_exploration_ms;      /**< Total exploration time */
    uint64_t total_rest_ms;             /**< Total rest time */
    bool is_resting;                    /**< Currently in recovery */
    bool needs_rest;                    /**< Rest is recommended */
} curiosity_fatigue_state_t;

/**
 * @brief Fatigue configuration
 */
typedef struct curiosity_fatigue_config_s {
    float fatigue_accumulation_rate;    /**< How fast fatigue builds */
    float base_recovery_rate;           /**< Base recovery speed */
    float rest_threshold;               /**< Fatigue level for rest */
    float min_rest_duration_ms;         /**< Minimum rest period */
    bool enable_auto_rest;              /**< Auto-initiate rest */
} curiosity_fatigue_config_t;

/* ============================================================================
 * Enhancement 10: Counterfactual Curiosity
 * ============================================================================ */

/**
 * @brief Counterfactual curiosity item
 *
 * BIOLOGICAL: Prefrontal simulation of alternatives
 */
typedef struct curiosity_counterfactual_s {
    char actual_outcome[512];           /**< What actually happened */
    char counterfactual_question[512];  /**< "What if..." question */
    char alternative_action[256];       /**< Alternative that could have been taken */
    float regret_intensity;             /**< Regret about not taking alternative */
    float learning_value;               /**< Potential learning from exploring */
    float exploration_cost;             /**< Cost to investigate */
    bool is_explored;                   /**< Has this been explored */
    uint64_t decision_time_ms;          /**< When decision was made */
} curiosity_counterfactual_t;

/**
 * @brief Counterfactual curiosity state
 */
typedef struct curiosity_counterfactual_state_s {
    curiosity_counterfactual_t items[MAX_COUNTERFACTUALS]; /**< Tracked counterfactuals */
    uint32_t num_items;                 /**< Number of tracked items */
    float counterfactual_curiosity;     /**< Overall "what if" curiosity */
    float regret_sensitivity;           /**< How sensitive to regret */
    uint32_t counterfactuals_explored;  /**< Total explored */
    float avg_learning_value;           /**< Average learning from exploration */
} curiosity_counterfactual_state_t;

/**
 * @brief Counterfactual configuration
 */
typedef struct curiosity_counterfactual_config_s {
    float regret_threshold;             /**< Regret level to trigger curiosity */
    float exploration_cost_limit;       /**< Max cost for exploration */
    float learning_value_threshold;     /**< Min value to explore */
    bool enable_automatic_generation;   /**< Auto-generate counterfactuals */
} curiosity_counterfactual_config_t;

/* ============================================================================
 * Enhanced Curiosity System
 * ============================================================================ */

/**
 * @brief Unified enhanced curiosity configuration
 */
typedef struct curiosity_enhanced_config_s {
    /* Sub-system configurations */
    curiosity_boredom_config_t boredom;
    curiosity_interest_config_t interest;
    curiosity_type_config_t types;
    curiosity_anxiety_config_t anxiety;
    curiosity_social_config_t social;
    curiosity_meta_config_t meta;
    curiosity_contagion_config_t contagion;
    curiosity_surprise_config_t surprise;
    curiosity_fatigue_config_t fatigue;
    curiosity_counterfactual_config_t counterfactual;

    /* Global settings */
    float update_interval_ms;           /**< Update frequency */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    bool enable_all_enhancements;       /**< Enable all features */
    bool enable_quantum_curiosity;      /**< Enable quantum exploration (default: true) */
} curiosity_enhanced_config_t;

/**
 * @brief Enhanced curiosity system state
 */
typedef struct curiosity_enhanced_state_s {
    curiosity_boredom_state_t boredom;
    curiosity_type_profile_t types;
    curiosity_approach_avoidance_t anxiety_balance;
    curiosity_social_state_t social;
    curiosity_meta_state_t meta;
    curiosity_contagion_state_t contagion;
    curiosity_surprise_learning_t surprise;
    curiosity_fatigue_state_t fatigue;
    curiosity_counterfactual_state_t counterfactual;

    /* Aggregated metrics */
    float overall_curiosity_drive;      /**< Combined curiosity level */
    float effective_exploration_rate;   /**< After all modulations */
    uint64_t last_update_ms;            /**< Last update timestamp */
} curiosity_enhanced_state_t;

/**
 * @brief Enhanced curiosity statistics
 */
typedef struct curiosity_enhanced_stats_s {
    uint64_t boredom_episodes;
    uint64_t novelty_events;
    uint64_t interest_satiation_events;
    uint64_t type_transitions;
    uint64_t approach_avoidance_conflicts;
    uint64_t social_curiosity_events;
    uint64_t introspection_events;
    uint64_t contagion_events;
    uint64_t surprise_events;
    uint64_t fatigue_rest_periods;
    uint64_t counterfactuals_generated;
    uint64_t quantum_explorations;
    float avg_curiosity_level;
    float avg_boredom_level;
    float avg_fatigue_level;
    float avg_quantum_speedup;
} curiosity_enhanced_stats_t;

/**
 * @brief Enhanced curiosity system handle
 */
typedef struct curiosity_enhanced_system_s curiosity_enhanced_system_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize default configuration
 *
 * WHAT: Set all config values to sensible defaults
 * WHY:  Ensure valid configuration before use
 * HOW:  Initialize all sub-configs with defaults
 *
 * @param config Configuration to initialize
 */
void curiosity_enhanced_config_default(curiosity_enhanced_config_t* config);

/**
 * @brief Create enhanced curiosity system
 *
 * WHAT: Instantiate complete enhanced curiosity system
 * WHY:  Create fully-functional curiosity enhancement
 * HOW:  Allocate memory, initialize all sub-systems
 *
 * @param config Configuration (NULL for defaults)
 * @param base_engine Base curiosity engine to enhance
 * @return System handle or NULL on failure
 */
curiosity_enhanced_system_t* curiosity_enhanced_create(
    const curiosity_enhanced_config_t* config,
    curiosity_engine_t base_engine
);

/**
 * @brief Destroy enhanced curiosity system
 *
 * WHAT: Clean up all resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all sub-systems, disconnect bridges
 *
 * @param system System to destroy (NULL-safe)
 */
void curiosity_enhanced_destroy(curiosity_enhanced_system_t* system);

/**
 * @brief Update enhanced curiosity system
 *
 * WHAT: Process one update cycle
 * WHY:  Evolve all curiosity states
 * HOW:  Update each sub-system, compute aggregates
 *
 * @param system System handle
 * @param dt_ms Time delta in milliseconds
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_update(curiosity_enhanced_system_t* system, float dt_ms);

/* ============================================================================
 * Enhancement 1: Boredom Functions
 * ============================================================================ */

/**
 * @brief Detect boredom state
 *
 * WHAT: Check if system is bored/understimulated
 * WHY:  Trigger novelty-seeking when needed
 * HOW:  Analyze monotony, time since last novelty
 *
 * @param system System handle
 * @param state Output state (optional)
 * @return true if bored
 */
bool curiosity_enhanced_is_bored(
    const curiosity_enhanced_system_t* system,
    curiosity_boredom_state_t* state
);

/**
 * @brief Report stimulus for monotony tracking
 *
 * WHAT: Record stimulus for boredom detection
 * WHY:  Track repetition and novelty
 * HOW:  Compare with recent stimuli
 *
 * @param system System handle
 * @param stimulus_hash Hash of stimulus
 * @param novelty Novelty level [0,1]
 * @return 0 on success
 */
int curiosity_enhanced_report_stimulus(
    curiosity_enhanced_system_t* system,
    uint64_t stimulus_hash,
    float novelty
);

/**
 * @brief Get novelty-seeking boost from boredom
 *
 * WHAT: Query exploration boost from boredom
 * WHY:  Modulate exploration rate
 * HOW:  Return boost factor based on boredom
 *
 * @param system System handle
 * @return Boost factor [1.0, BOREDOM_NOVELTY_SEEK_BOOST]
 */
float curiosity_enhanced_get_boredom_boost(
    const curiosity_enhanced_system_t* system
);

/* ============================================================================
 * Enhancement 2: Interest Decay Functions
 * ============================================================================ */

/**
 * @brief Get current interest in topic
 *
 * WHAT: Query current interest level
 * WHY:  Determine if topic is still interesting
 * HOW:  Apply decay and satiation model
 *
 * @param system System handle
 * @param topic Topic to query
 * @return Interest level [0,1]
 */
float curiosity_enhanced_get_topic_interest(
    const curiosity_enhanced_system_t* system,
    const char* topic
);

/**
 * @brief Record topic exposure
 *
 * WHAT: Note exposure to topic
 * WHY:  Update interest decay model
 * HOW:  Increment count, update timestamps
 *
 * @param system System handle
 * @param topic Topic exposed to
 * @param learning_value How much was learned [0,1]
 * @return 0 on success
 */
int curiosity_enhanced_record_exposure(
    curiosity_enhanced_system_t* system,
    const char* topic,
    float learning_value
);

/**
 * @brief Compute satiation level for topic
 *
 * WHAT: Get how "full" of knowledge on topic
 * WHY:  Determine if more learning is valuable
 * HOW:  Compute based on exposures and learning
 *
 * @param system System handle
 * @param topic Topic to check
 * @return Satiation level [0,1]
 */
float curiosity_enhanced_compute_satiation(
    const curiosity_enhanced_system_t* system,
    const char* topic
);

/**
 * @brief Get residual interest after satiation
 *
 * WHAT: Query minimum interest that remains
 * WHY:  Some topics retain baseline interest
 * HOW:  Apply satiation and residual models
 *
 * @param system System handle
 * @param topic Topic to check
 * @return Residual interest [0,1]
 */
float curiosity_enhanced_get_residual_interest(
    const curiosity_enhanced_system_t* system,
    const char* topic
);

/* ============================================================================
 * Enhancement 3: Curiosity Type Functions
 * ============================================================================ */

/**
 * @brief Get dominant curiosity type
 *
 * WHAT: Query currently dominant curiosity mode
 * WHY:  Adapt exploration strategy
 * HOW:  Return type with highest intensity
 *
 * @param system System handle
 * @return Dominant curiosity type
 */
curiosity_type_t curiosity_enhanced_get_dominant_type(
    const curiosity_enhanced_system_t* system
);

/**
 * @brief Get curiosity type profile
 *
 * WHAT: Query full type distribution
 * WHY:  Understand curiosity composition
 * HOW:  Return all type intensities
 *
 * @param system System handle
 * @param profile Output profile
 * @return 0 on success
 */
int curiosity_enhanced_get_type_profile(
    const curiosity_enhanced_system_t* system,
    curiosity_type_profile_t* profile
);

/**
 * @brief Set curiosity type intensity
 *
 * WHAT: Modulate specific curiosity type
 * WHY:  External influence on curiosity mode
 * HOW:  Set intensity for type
 *
 * @param system System handle
 * @param type Type to modulate
 * @param intensity New intensity [0,1]
 * @return 0 on success
 */
int curiosity_enhanced_set_type_intensity(
    curiosity_enhanced_system_t* system,
    curiosity_type_t type,
    float intensity
);

/**
 * @brief Trigger type transition
 *
 * WHAT: Force transition to new curiosity type
 * WHY:  Context-driven type changes
 * HOW:  Update dominant type, record transition
 *
 * @param system System handle
 * @param new_type Target type
 * @return 0 on success
 */
int curiosity_enhanced_transition_type(
    curiosity_enhanced_system_t* system,
    curiosity_type_t new_type
);

/* ============================================================================
 * Enhancement 4: Curiosity-Anxiety Functions
 * ============================================================================ */

/**
 * @brief Connect to anxiety system
 *
 * WHAT: Establish anxiety-curiosity coupling
 * WHY:  Model approach-avoidance dynamics
 * HOW:  Register callbacks, share state
 *
 * @param system System handle
 * @param anxiety Anxiety system
 * @return 0 on success
 */
int curiosity_enhanced_connect_anxiety(
    curiosity_enhanced_system_t* system,
    anxiety_system_t* anxiety
);

/**
 * @brief Get net motivation (approach - avoidance)
 *
 * WHAT: Query net exploratory drive
 * WHY:  Determine if exploration is advisable
 * HOW:  Compute approach - avoidance
 *
 * @param system System handle
 * @return Net motivation [-1, 1]
 */
float curiosity_enhanced_get_net_motivation(
    const curiosity_enhanced_system_t* system
);

/**
 * @brief Check if should explore given threat
 *
 * WHAT: Decision to explore vs avoid
 * WHY:  Safety-aware exploration
 * HOW:  Compare approach vs avoidance + threat
 *
 * @param system System handle
 * @param threat_level Current threat [0,1]
 * @return true if exploration advisable
 */
bool curiosity_enhanced_should_explore(
    const curiosity_enhanced_system_t* system,
    float threat_level
);

/**
 * @brief Report approach-avoidance conflict resolution
 *
 * WHAT: Record conflict outcome
 * WHY:  Learn from conflict resolutions
 * HOW:  Update approach/avoidance wins
 *
 * @param system System handle
 * @param approach_won true if approach won
 * @return 0 on success
 */
int curiosity_enhanced_report_conflict_resolution(
    curiosity_enhanced_system_t* system,
    bool approach_won
);

/* ============================================================================
 * Enhancement 5: Social Curiosity Functions
 * ============================================================================ */

/**
 * @brief Connect to Theory of Mind system
 *
 * WHAT: Establish ToM-curiosity coupling
 * WHY:  Enable social cognition integration
 * HOW:  Register callbacks, share state
 *
 * @param system System handle
 * @param tom Theory of Mind system
 * @return 0 on success
 */
int curiosity_enhanced_connect_tom(
    curiosity_enhanced_system_t* system,
    theory_of_mind_t* tom
);

/**
 * @brief Assess social curiosity about agent
 *
 * WHAT: Query curiosity about specific agent
 * WHY:  Determine social interest
 * HOW:  Compute based on relationship, trust, info value
 *
 * @param system System handle
 * @param agent_id Agent to assess
 * @param target Output target info (optional)
 * @return Social curiosity level [0,1]
 */
float curiosity_enhanced_assess_social_target(
    curiosity_enhanced_system_t* system,
    const char* agent_id,
    curiosity_social_target_t* target
);

/**
 * @brief Record social interaction
 *
 * WHAT: Note interaction with agent
 * WHY:  Update social curiosity model
 * HOW:  Update target tracking
 *
 * @param system System handle
 * @param agent_id Agent interacted with
 * @param info_gained Information gained [0,1]
 * @return 0 on success
 */
int curiosity_enhanced_record_social_interaction(
    curiosity_enhanced_system_t* system,
    const char* agent_id,
    float info_gained
);

/**
 * @brief Get gossip interest level
 *
 * WHAT: Query interest in social information
 * WHY:  Modulate social info seeking
 * HOW:  Return current gossip interest
 *
 * @param system System handle
 * @return Gossip interest [0,1]
 */
float curiosity_enhanced_get_gossip_interest(
    const curiosity_enhanced_system_t* system
);

/* ============================================================================
 * Enhancement 6: Meta-Curiosity Functions
 * ============================================================================ */

/**
 * @brief Introspect on own curiosity
 *
 * WHAT: Self-reflect on curiosity patterns
 * WHY:  Improve self-awareness
 * HOW:  Analyze curiosity history, patterns
 *
 * @param system System handle
 * @param state Output meta state
 * @return 0 on success
 */
int curiosity_enhanced_introspect(
    curiosity_enhanced_system_t* system,
    curiosity_meta_state_t* state
);

/**
 * @brief Identify curiosity blind spots
 *
 * WHAT: Find topics of low curiosity
 * WHY:  Reveal knowledge gaps
 * HOW:  Analyze topic distribution
 *
 * @param system System handle
 * @return Number of blind spots identified
 */
uint32_t curiosity_enhanced_identify_blind_spots(
    curiosity_enhanced_system_t* system
);

/**
 * @brief Get meta-curiosity level
 *
 * WHAT: Query curiosity about own curiosity
 * WHY:  Determine metacognitive engagement
 * HOW:  Return meta-curiosity intensity
 *
 * @param system System handle
 * @return Meta-curiosity level [0,1]
 */
float curiosity_enhanced_get_meta_curiosity(
    const curiosity_enhanced_system_t* system
);

/* ============================================================================
 * Enhancement 7: Curiosity Contagion Functions
 * ============================================================================ */

/**
 * @brief Observe another's curiosity
 *
 * WHAT: Process observation of other's curiosity
 * WHY:  Enable curiosity contagion
 * HOW:  Potentially adopt observed curiosity
 *
 * @param system System handle
 * @param event Observation event
 * @return true if curiosity was adopted
 */
bool curiosity_enhanced_observe_curiosity(
    curiosity_enhanced_system_t* system,
    const curiosity_contagion_event_t* event
);

/**
 * @brief Get contagion susceptibility
 *
 * WHAT: Query susceptibility to contagion
 * WHY:  Modulate social influence
 * HOW:  Return current susceptibility
 *
 * @param system System handle
 * @return Susceptibility [0,1]
 */
float curiosity_enhanced_get_contagion_susceptibility(
    const curiosity_enhanced_system_t* system
);

/**
 * @brief Set contagion susceptibility
 *
 * WHAT: Modulate susceptibility to contagion
 * WHY:  External control over social influence
 * HOW:  Set susceptibility value
 *
 * @param system System handle
 * @param susceptibility New susceptibility [0,1]
 * @return 0 on success
 */
int curiosity_enhanced_set_contagion_susceptibility(
    curiosity_enhanced_system_t* system,
    float susceptibility
);

/* ============================================================================
 * Enhancement 8: Surprise Learning Functions
 * ============================================================================ */

/**
 * @brief Report surprise event
 *
 * WHAT: Note surprising observation
 * WHY:  Boost learning rate
 * HOW:  Compute surprise-based modulation
 *
 * @param system System handle
 * @param prediction_error How wrong prediction was [0,1]
 * @param context Context of surprise
 * @return Learning rate boost factor
 */
float curiosity_enhanced_report_surprise(
    curiosity_enhanced_system_t* system,
    float prediction_error,
    const char* context
);

/**
 * @brief Get surprise-based learning boost
 *
 * WHAT: Query current LR boost from surprise
 * WHY:  Modulate learning rate
 * HOW:  Return accumulated boost
 *
 * @param system System handle
 * @return Learning rate boost [1.0, max]
 */
float curiosity_enhanced_get_surprise_boost(
    const curiosity_enhanced_system_t* system
);

/**
 * @brief Prioritize surprising experience for memory
 *
 * WHAT: Mark experience as high priority
 * WHY:  Enhanced consolidation of surprises
 * HOW:  Set memory priority
 *
 * @param system System handle
 * @param experience Experience description
 * @return Memory priority [0,1]
 */
float curiosity_enhanced_prioritize_surprise(
    curiosity_enhanced_system_t* system,
    const char* experience
);

/* ============================================================================
 * Enhancement 9: Fatigue Functions
 * ============================================================================ */

/**
 * @brief Check fatigue state
 *
 * WHAT: Query current fatigue level
 * WHY:  Determine if rest is needed
 * HOW:  Return fatigue state
 *
 * @param system System handle
 * @param state Output fatigue state (optional)
 * @return Fatigue level [0,1]
 */
float curiosity_enhanced_check_fatigue(
    const curiosity_enhanced_system_t* system,
    curiosity_fatigue_state_t* state
);

/**
 * @brief Initiate recovery period
 *
 * WHAT: Start rest/recovery period
 * WHY:  Recover from exploration fatigue
 * HOW:  Enter rest mode
 *
 * @param system System handle
 * @param rest_duration_ms How long to rest
 * @return 0 on success
 */
int curiosity_enhanced_initiate_recovery(
    curiosity_enhanced_system_t* system,
    float rest_duration_ms
);

/**
 * @brief Check if rest is needed
 *
 * WHAT: Query if rest is recommended
 * WHY:  Prevent exhaustion
 * HOW:  Compare fatigue to threshold
 *
 * @param system System handle
 * @return true if rest needed
 */
bool curiosity_enhanced_needs_rest(
    const curiosity_enhanced_system_t* system
);

/**
 * @brief End recovery period
 *
 * WHAT: Exit rest mode
 * WHY:  Resume exploration
 * HOW:  Clear rest flag
 *
 * @param system System handle
 * @return 0 on success
 */
int curiosity_enhanced_end_recovery(
    curiosity_enhanced_system_t* system
);

/* ============================================================================
 * Enhancement 10: Counterfactual Functions
 * ============================================================================ */

/**
 * @brief Generate counterfactual question
 *
 * WHAT: Create "what if" question
 * WHY:  Enable counterfactual reasoning
 * HOW:  Generate based on decision point
 *
 * @param system System handle
 * @param decision_point Decision that was made
 * @param actual_outcome What happened
 * @param counterfactual Output counterfactual
 * @return 0 on success
 */
int curiosity_enhanced_generate_counterfactual(
    curiosity_enhanced_system_t* system,
    const char* decision_point,
    const char* actual_outcome,
    curiosity_counterfactual_t* counterfactual
);

/**
 * @brief Explore counterfactual
 *
 * WHAT: Investigate "what if" scenario
 * WHY:  Learn from alternatives
 * HOW:  Simulate alternative outcome
 *
 * @param system System handle
 * @param counterfactual Counterfactual to explore
 * @param learning_outcome What was learned
 * @return 0 on success
 */
int curiosity_enhanced_explore_counterfactual(
    curiosity_enhanced_system_t* system,
    curiosity_counterfactual_t* counterfactual,
    float* learning_outcome
);

/**
 * @brief Get counterfactual curiosity level
 *
 * WHAT: Query "what if" curiosity
 * WHY:  Determine interest in alternatives
 * HOW:  Return counterfactual curiosity
 *
 * @param system System handle
 * @return Counterfactual curiosity [0,1]
 */
float curiosity_enhanced_get_counterfactual_curiosity(
    const curiosity_enhanced_system_t* system
);

/* ============================================================================
 * State and Statistics Functions
 * ============================================================================ */

/**
 * @brief Get full enhanced state
 *
 * WHAT: Query complete enhanced state
 * WHY:  External monitoring
 * HOW:  Copy all state data
 *
 * @param system System handle
 * @param state Output state
 * @return 0 on success
 */
int curiosity_enhanced_get_state(
    const curiosity_enhanced_system_t* system,
    curiosity_enhanced_state_t* state
);

/**
 * @brief Get enhanced statistics
 *
 * WHAT: Query accumulated statistics
 * WHY:  Performance monitoring
 * HOW:  Copy stats data
 *
 * @param system System handle
 * @param stats Output statistics
 * @return 0 on success
 */
int curiosity_enhanced_get_stats(
    const curiosity_enhanced_system_t* system,
    curiosity_enhanced_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero all counters
 *
 * @param system System handle
 */
void curiosity_enhanced_reset_stats(
    curiosity_enhanced_system_t* system
);

/**
 * @brief Get overall curiosity drive
 *
 * WHAT: Query aggregated curiosity level
 * WHY:  Single metric for curiosity state
 * HOW:  Combine all sub-systems
 *
 * @param system System handle
 * @return Overall drive [0,1]
 */
float curiosity_enhanced_get_overall_drive(
    const curiosity_enhanced_system_t* system
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Inter-module communication
 * HOW:  Register with router
 *
 * @param system System handle
 * @return 0 on success
 */
int curiosity_enhanced_connect_bio_async(
    curiosity_enhanced_system_t* system
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Disable bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 *
 * @param system System handle
 * @return 0 on success
 */
int curiosity_enhanced_disconnect_bio_async(
    curiosity_enhanced_system_t* system
);

/**
 * @brief Check bio-async connection
 *
 * WHAT: Query bio-async connection state
 * WHY:  Verify communication
 * HOW:  Return connection status
 *
 * @param system System handle
 * @return true if connected
 */
bool curiosity_enhanced_is_bio_async_connected(
    const curiosity_enhanced_system_t* system
);

/* ============================================================================
 * String Conversion Functions
 * ============================================================================ */

/**
 * @brief Convert curiosity type to string
 */
const char* curiosity_type_to_string(curiosity_type_t type);

/**
 * @brief Convert string to curiosity type
 */
curiosity_type_t curiosity_type_from_string(const char* str);

/* ============================================================================
 * Quantum Bridge Integration
 * ============================================================================ */

/**
 * @brief Get quantum bridge handle
 *
 * WHAT: Access quantum exploration bridge
 * WHY:  Allow direct quantum exploration queries
 * HOW:  Return bridge pointer
 *
 * @param system System handle
 * @return Quantum bridge, or NULL if not enabled
 */
curiosity_quantum_bridge_t* curiosity_enhanced_get_quantum_bridge(
    curiosity_enhanced_system_t* system
);

/**
 * @brief Perform quantum exploration
 *
 * WHAT: Use quantum walk to find novel topics
 * WHY:  Quadratic speedup over classical exploration
 * HOW:  Delegate to quantum bridge
 *
 * @param system System handle
 * @param start_topic Starting topic (NULL for current)
 * @param novel_topic Output: discovered novel topic (256 bytes min)
 * @return Novelty score, or -1.0f on failure
 */
float curiosity_enhanced_quantum_explore(
    curiosity_enhanced_system_t* system,
    const char* start_topic,
    char* novel_topic
);

/**
 * @brief Add topic to quantum exploration graph
 *
 * WHAT: Register topic for quantum exploration
 * WHY:  Expand explorable novelty space
 * HOW:  Add to quantum bridge graph
 *
 * @param system System handle
 * @param topic Topic identifier
 * @param curiosity_level Initial curiosity [0,1]
 * @param novelty_score Novelty score [0,1]
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_add_quantum_topic(
    curiosity_enhanced_system_t* system,
    const char* topic,
    float curiosity_level,
    float novelty_score
);

/**
 * @brief Evaluate topic novelty using quantum walk
 *
 * WHAT: Quantum assessment of topic novelty
 * WHY:  Use quantum walk entropy for novelty
 * HOW:  Run quantum walk, compute distribution entropy
 *
 * @param system System handle
 * @param topic Topic to evaluate
 * @return Novelty score [0,1], or -1.0f on error
 */
float curiosity_enhanced_quantum_evaluate_novelty(
    curiosity_enhanced_system_t* system,
    const char* topic
);

/* ============================================================================
 * Quantum Monte Carlo Functions
 * ============================================================================ */

/**
 * @brief Get default QMC configuration
 *
 * WHAT: Initialize QMC config with sensible defaults
 * WHY:  Ensure valid configuration before use
 * HOW:  Set samples=1000, burnin=100, threshold=0.3
 *
 * @param config Configuration to initialize
 */
void curiosity_enhanced_qmc_default_config(curiosity_qmc_config_t* config);

/**
 * @brief Set QMC configuration for curiosity system
 *
 * WHAT: Configure QMC parameters
 * WHY:  Customize uncertainty estimation behavior
 * HOW:  Store config in system
 *
 * @param system System handle
 * @param config QMC configuration
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_set_qmc_config(
    curiosity_enhanced_system_t* system,
    const curiosity_qmc_config_t* config
);

/**
 * @brief Estimate uncertainty in topic interest
 *
 * WHAT: Quantify confidence in interest assessment
 * WHY:  Uncertainty-aware exploration (explore high-uncertainty topics)
 * HOW:  Bootstrap sampling with interest variance estimation
 *
 * BIOLOGICAL: Norepinephrine signals uncertainty, drives exploration
 *
 * @param system System handle
 * @param topic Topic to assess
 * @param result Output uncertainty result
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_estimate_uncertainty(
    curiosity_enhanced_system_t* system,
    const char* topic,
    curiosity_qmc_uncertainty_t* result
);

/**
 * @brief Batch uncertainty estimation for multiple topics
 *
 * WHAT: Estimate uncertainty for many topics efficiently
 * WHY:  Amortize MC overhead across batch
 * HOW:  Parallel MC sampling with shared random state
 *
 * @param system System handle
 * @param topics Array of topic names
 * @param num_topics Number of topics
 * @param results Output array of uncertainty results
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_estimate_uncertainty_batch(
    curiosity_enhanced_system_t* system,
    const char** topics,
    uint32_t num_topics,
    curiosity_qmc_uncertainty_t* results
);

/**
 * @brief Compute empowerment for topic exploration
 *
 * WHAT: Measure capacity to influence future knowledge states
 * WHY:  Empowerment is a key intrinsic motivation signal
 * HOW:  Estimate mutual information I(A; S') between actions and future states
 *
 * BIOLOGICAL: Models the "sense of control" that drives exploration
 *
 * @param system System handle
 * @param topic Starting topic
 * @param horizon Number of steps to look ahead
 * @param result Output empowerment result
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_compute_empowerment(
    curiosity_enhanced_system_t* system,
    const char* topic,
    uint32_t horizon,
    curiosity_empowerment_result_t* result
);

/**
 * @brief Free empowerment result resources
 *
 * @param result Result to free
 */
void curiosity_empowerment_result_free(curiosity_empowerment_result_t* result);

/**
 * @brief Select exploration target using uncertainty-based sampling
 *
 * WHAT: Sample topic proportional to uncertainty × interest
 * WHY:  Balance curiosity with epistemic exploration
 * HOW:  Thompson sampling with uncertainty estimates
 *
 * @param system System handle
 * @param topics Array of candidate topics
 * @param num_topics Number of topics
 * @param selected_topic Output: selected topic name (256 bytes min)
 * @return Uncertainty score of selected topic
 */
float curiosity_enhanced_sample_by_uncertainty(
    curiosity_enhanced_system_t* system,
    const char** topics,
    uint32_t num_topics,
    char* selected_topic
);

/**
 * @brief Compute uncertainty-weighted exploration bonus
 *
 * WHAT: Bonus for exploring uncertain topics
 * WHY:  UCB-style exploration in curiosity space
 * HOW:  bonus = exploration_bonus * sqrt(2 * ln(N) / n_topic)
 *
 * @param system System handle
 * @param topic Topic to evaluate
 * @return Exploration bonus [0, exploration_bonus]
 */
float curiosity_enhanced_get_exploration_bonus(
    curiosity_enhanced_system_t* system,
    const char* topic
);

/**
 * @brief Update topic interest with MC sampling
 *
 * WHAT: Update interest using MC-estimated posterior
 * WHY:  Bayesian update of interest beliefs
 * HOW:  Importance sampling for posterior estimation
 *
 * @param system System handle
 * @param topic Topic to update
 * @param observed_interest Observed interest signal [0,1]
 * @param observation_noise Noise in observation [0,1]
 * @return Updated interest estimate
 */
float curiosity_enhanced_update_interest_mc(
    curiosity_enhanced_system_t* system,
    const char* topic,
    float observed_interest,
    float observation_noise
);

/**
 * @brief Estimate information gain using MC simulation
 *
 * WHAT: Expected KL divergence from exploring topic
 * WHY:  Information-theoretic exploration bonus
 * HOW:  Sample knowledge states, compute expected KL
 *
 * @param system System handle
 * @param topic Topic to evaluate
 * @return Expected information gain (bits)
 */
float curiosity_enhanced_estimate_info_gain_qmc(
    curiosity_enhanced_system_t* system,
    const char* topic
);

/**
 * @brief Run MCTS for exploration planning
 *
 * WHAT: Plan exploration sequence using MCTS
 * WHY:  Long-horizon exploration optimization
 * HOW:  Build tree of topic transitions, select best sequence
 *
 * @param system System handle
 * @param start_topic Starting topic
 * @param horizon Planning horizon (steps)
 * @param exploration_path Output: planned topics (array of 256-byte strings)
 * @param max_path_length Maximum path length
 * @return Number of topics in path
 */
uint32_t curiosity_enhanced_plan_exploration_mcts(
    curiosity_enhanced_system_t* system,
    const char* start_topic,
    uint32_t horizon,
    char** exploration_path,
    uint32_t max_path_length
);

/**
 * @brief Get QMC statistics
 *
 * WHAT: Query accumulated QMC statistics
 * WHY:  Monitor MC performance and exploration
 * HOW:  Copy stats data
 *
 * @param system System handle
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int curiosity_enhanced_get_qmc_stats(
    const curiosity_enhanced_system_t* system,
    curiosity_qmc_stats_t* stats
);

/**
 * @brief Reset QMC statistics
 *
 * @param system System handle
 */
void curiosity_enhanced_reset_qmc_stats(
    curiosity_enhanced_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_ENHANCED_H */
