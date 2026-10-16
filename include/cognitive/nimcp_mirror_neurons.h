/**
 * @file nimcp_mirror_neurons.h
 * @brief Mirror Neuron System - Observation-based learning and social cognition
 * @version 1.0.0
 * @date 2025-01-09
 *
 * WHAT: Biologically-inspired mirror neuron system for imitation learning
 * WHY:  Enable learning by observation, social cognition, and action understanding
 * HOW:  Neurons that fire for both self-action and observed-action, with associative learning
 *
 * Mirror neurons are a class of neurons that activate both when:
 * 1. An agent performs an action (motor execution)
 * 2. An agent observes another performing the same action (motor observation)
 *
 * This dual representation enables:
 * - Imitation learning (learn by watching)
 * - Action understanding (predict others' goals)
 * - Empathy and theory of mind (understand others' mental states)
 * - Reduced training time (observation > trial-and-error)
 *
 * Key Features:
 * - Action representation in shared observation/execution space
 * - Association learning between observed and performed actions
 * - Integration with working memory for action replay
 * - Connection to theory of mind for intent understanding
 * - Multi-agent learning support
 *
 * Integration Points:
 * - Working Memory: Store observed action sequences
 * - Theory of Mind: Understand agent intentions
 * - Executive Functions: Plan imitation strategies
 * - Predictive Processing: Predict action outcomes
 *
 * @see Phase 10.11 - Mirror Neurons & Social Cognition
 */

#ifndef NIMCP_MIRROR_NEURONS_H
#define NIMCP_MIRROR_NEURONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * @brief Opaque handle to mirror neuron system
 */
typedef struct mirror_neurons_system mirror_neurons_system_t;
typedef mirror_neurons_system_t* mirror_neurons_t;

/**
 * @brief Forward declaration of brain handle
 */
typedef struct brain_struct* brain_t;

//=============================================================================
// Configuration & Parameters
//=============================================================================

/**
 * @brief Mirror neuron configuration
 *
 * WHAT: Configuration for mirror neuron system creation
 * WHY:  Allow customization of mirror neuron behavior and capacity
 */
typedef struct {
    // System capacity
    uint32_t num_mirror_neurons;      /**< Number of mirror neurons (default: 1000) */
    uint32_t max_actions;             /**< Max distinct actions to track (default: 100) */
    uint32_t max_agents;              /**< Max agents to observe (default: 10) */

    // Learning parameters
    float learning_rate;              /**< Association learning rate (0.0-1.0, default: 0.01) */
    float decay_rate;                 /**< Activation decay rate (0.0-1.0, default: 0.05) */
    float match_threshold;            /**< Action matching threshold (0.0-1.0, default: 0.7) */

    // Integration flags
    bool enable_working_memory;       /**< Store observations in working memory (default: true) */
    bool enable_theory_of_mind;       /**< Infer agent intentions (default: true) */
    bool enable_prediction;           /**< Predict action outcomes (default: true) */

    // Performance tuning
    uint32_t observation_window;      /**< Time window for observation (ms, default: 1000) */
    uint32_t replay_capacity;         /**< Max stored observations for replay (default: 100) */

    // Glial cell support (Phase 10.11.1)
    bool enable_glial_modulation;     /**< Enable glial cell modulation (default: true) */
    bool enable_astrocytes;           /**< Astrocytes modulate association strength (default: true) */
    bool enable_oligodendrocytes;     /**< Oligodendrocytes speed recognition (default: true) */
    bool enable_microglia;            /**< Microglia prune weak associations (default: true) */

    // Phase 10.11.2: Substrate integration
    bool enable_substrate;            /**< Enable biological substrate backing (default: false) */
    bool enable_myelination;          /**< Use myelin sheath for timing (default: true if substrate) */
    bool enable_dendrite_plasticity;  /**< Use dendritic spine plasticity (default: true if substrate) */
    bool enable_axon_timing;          /**< Use axon propagation delays (default: true if substrate) */
    bool enable_substrate_pool;       /**< Use memory pool for substrate (default: true if substrate) */
    uint32_t substrate_pool_size;     /**< Substrate pool capacity (default: 4096) */

} mirror_neuron_config_t;

/**
 * @brief Action representation
 *
 * WHAT: Represents an action in the mirror neuron system
 * WHY:  Common representation for both observed and executed actions
 */
typedef struct {
    uint32_t action_id;               /**< Unique action identifier */
    char action_name[64];             /**< Human-readable action name */
    float features[32];               /**< Action feature vector */
    uint32_t num_features;            /**< Number of valid features */
    uint64_t timestamp;               /**< When action occurred */
    uint32_t agent_id;                /**< Which agent performed it (0 = self) */
    float confidence;                 /**< Confidence in action recognition (0.0-1.0) */
} action_t;

/**
 * @brief Mirror neuron activation record
 *
 * WHAT: Records activation pattern for observation-execution pair
 * WHY:  Track associations between observed and executed actions
 */
typedef struct {
    uint32_t action_id;               /**< Action that triggered activation */
    float observation_activation;     /**< Activation from observation */
    float execution_activation;       /**< Activation from execution */
    float association_strength;       /**< Learned association (0.0-1.0) */
    uint32_t observation_count;       /**< Times observed */
    uint32_t execution_count;         /**< Times executed */
    uint64_t last_activation;         /**< Last activation timestamp */
} mirror_activation_t;

/**
 * @brief Mirror neuron system statistics
 *
 * WHAT: Runtime statistics for mirror neuron system
 * WHY:  Monitor performance and learning progress
 */
typedef struct {
    // Capacity metrics
    uint32_t num_active_neurons;      /**< Currently active mirror neurons */
    uint32_t num_learned_actions;     /**< Number of distinct learned actions */
    uint32_t num_observed_agents;     /**< Number of agents observed */

    // Learning metrics
    uint32_t total_observations;      /**< Total observations processed */
    uint32_t total_executions;        /**< Total self-actions executed */
    uint32_t successful_matches;      /**< Successful observation-execution matches */
    float avg_match_quality;          /**< Average matching quality (0.0-1.0) */

    // Performance metrics
    float avg_observation_latency_ms; /**< Average observation processing time */
    float avg_execution_latency_ms;   /**< Average execution processing time */
    uint64_t last_update_time;        /**< Last system update timestamp */

    // Integration metrics
    uint32_t working_memory_stores;   /**< Observations stored in working memory */
    uint32_t theory_of_mind_inferences; /**< Intent inferences made */
    uint32_t predictions_made;        /**< Action predictions made */

} mirror_neuron_stats_t;

//=============================================================================
// Core API - Lifecycle Management
//=============================================================================

/**
 * @brief Create mirror neuron system
 *
 * WHAT: Initialize mirror neuron system with configuration
 * WHY:  Enable observation-based learning and social cognition
 * HOW:  Allocate neurons, initialize learning parameters, setup tracking
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: Yes (creates new instance)
 *
 * @param config Configuration parameters (NULL = use defaults)
 * @return Mirror neuron system handle or NULL on error
 */
mirror_neurons_t mirror_neurons_create(const mirror_neuron_config_t* config);

/**
 * @brief Destroy mirror neuron system
 *
 * WHAT: Free all resources associated with mirror neuron system
 * WHY:  Prevent memory leaks
 * HOW:  Release neurons, clear observations, free memory
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param mirror Mirror neuron system to destroy (NULL-safe)
 */
void mirror_neurons_destroy(mirror_neurons_t mirror);

/**
 * @brief Set brain reference for neuromodulator integration
 *
 * WHAT: Associate mirror neurons with brain for ACh modulation
 * WHY:  Enable acetylcholine-gated social learning
 * HOW:  Store brain reference in system structure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param brain Brain handle (can be NULL to disable neuromodulation)
 */
void mirror_neurons_set_brain(mirror_neurons_t mirror, brain_t brain);

//=============================================================================
// Core API - Action Processing
//=============================================================================

/**
 * @brief Process observed action from another agent
 *
 * WHAT: Activate mirror neurons in response to observing an action
 * WHY:  Learn from observation, build action associations
 * HOW:  Match action features to neurons, update activations, strengthen associations
 *
 * This is the core observation pathway. Mirror neurons will:
 * 1. Recognize the observed action
 * 2. Activate corresponding mirror neurons
 * 3. Update observation statistics
 * 4. Store in working memory (if enabled)
 * 5. Trigger theory of mind inference (if enabled)
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param action Observed action
 * @return true on success, false on error
 */
bool mirror_neurons_observe_action(mirror_neurons_t mirror, const action_t* action);

/**
 * @brief Process self-executed action
 *
 * WHAT: Activate mirror neurons when agent performs an action
 * WHY:  Build execution representation, strengthen observation-execution associations
 * HOW:  Match action to neurons, update execution statistics, learn associations
 *
 * This is the core execution pathway. Mirror neurons will:
 * 1. Recognize the executed action
 * 2. Activate corresponding mirror neurons
 * 3. Update execution statistics
 * 4. Strengthen associations with previously observed similar actions
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param action Executed action
 * @return true on success, false on error
 */
bool mirror_neurons_execute_action(mirror_neurons_t mirror, const action_t* action);

/**
 * @brief Get mirror neuron activation for specific action
 *
 * WHAT: Query current activation level for an action
 * WHY:  Determine how strongly an action is represented
 * HOW:  Sum activations across all neurons responding to action
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: Yes (read-only)
 *
 * @param mirror Mirror neuron system
 * @param action_id Action to query
 * @return Activation level (0.0-1.0), or -1.0 on error
 */
float mirror_neurons_get_activation(mirror_neurons_t mirror, uint32_t action_id);

/**
 * @brief Match observed action to executed action
 *
 * WHAT: Determine if observed and executed actions are similar
 * WHY:  Enable action recognition and imitation learning
 * HOW:  Compare feature vectors, check association strength
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param mirror Mirror neuron system
 * @param observed_action Observed action
 * @param executed_action Executed action
 * @param out_similarity Pointer to store similarity score (0.0-1.0, can be NULL)
 * @return true if actions match (above threshold), false otherwise
 */
bool mirror_neurons_match_actions(
    mirror_neurons_t mirror,
    const action_t* observed_action,
    const action_t* executed_action,
    float* out_similarity
);

//=============================================================================
// Learning & Adaptation API
//=============================================================================

/**
 * @brief Learn from action demonstration
 *
 * WHAT: Process a complete action demonstration for imitation learning
 * WHY:  Enable one-shot or few-shot learning from demonstration
 * HOW:  Observe action sequence, build internal representation, prepare for reproduction
 *
 * This high-level function processes a demonstration by:
 * 1. Observing each action in sequence
 * 2. Building associations between actions
 * 3. Storing in working memory for replay
 * 4. Creating executable representation
 *
 * COMPLEXITY: O(m*n) where m = sequence length, n = num_mirror_neurons
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param actions Array of actions in demonstration
 * @param num_actions Number of actions in sequence
 * @param demonstrator_id ID of demonstrating agent
 * @return true on success, false on error
 */
bool mirror_neurons_learn_demonstration(
    mirror_neurons_t mirror,
    const action_t* actions,
    uint32_t num_actions,
    uint32_t demonstrator_id
);

/**
 * @brief Update association strengths through learning
 *
 * WHAT: Apply learning rule to update observation-execution associations
 * WHY:  Strengthen associations based on repeated co-activation
 * HOW:  Hebbian-like learning: neurons that fire together, wire together
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @return true on success, false on error
 */
bool mirror_neurons_update_associations(mirror_neurons_t mirror);

/**
 * @brief Decay inactive activations
 *
 * WHAT: Reduce activation levels of inactive neurons
 * WHY:  Prevent saturation, maintain dynamic responsiveness
 * HOW:  Apply exponential decay to activation levels
 *
 * COMPLEXITY: O(n) where n = num_mirror_neurons
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param delta_time_ms Time since last decay (milliseconds)
 * @return true on success, false on error
 */
bool mirror_neurons_decay_activations(mirror_neurons_t mirror, uint32_t delta_time_ms);

//=============================================================================
// Query & Analysis API
//=============================================================================

/**
 * @brief Get system statistics
 *
 * WHAT: Retrieve runtime statistics for mirror neuron system
 * WHY:  Monitor performance, learning progress, integration health
 * HOW:  Aggregate metrics from internal state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only with atomic reads)
 *
 * @param mirror Mirror neuron system
 * @param stats Pointer to store statistics
 * @return true on success, false on error
 */
bool mirror_neurons_get_stats(mirror_neurons_t mirror, mirror_neuron_stats_t* stats);

/**
 * @brief Get activation record for specific action
 *
 * WHAT: Retrieve detailed activation information for an action
 * WHY:  Analyze observation-execution associations
 * HOW:  Look up activation record by action ID
 *
 * COMPLEXITY: O(1) average (hash table lookup)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param mirror Mirror neuron system
 * @param action_id Action to query
 * @param activation Pointer to store activation record
 * @return true if action found, false otherwise
 */
bool mirror_neurons_get_activation_record(
    mirror_neurons_t mirror,
    uint32_t action_id,
    mirror_activation_t* activation
);

/**
 * @brief Predict next action in sequence
 *
 * WHAT: Predict likely next action based on observed sequence
 * WHY:  Enable proactive behavior, action anticipation
 * HOW:  Use learned action sequences and associations
 *
 * COMPLEXITY: O(n) where n = num_learned_actions
 * THREAD-SAFE: Yes (read-only)
 *
 * @param mirror Mirror neuron system
 * @param previous_actions Recent action sequence
 * @param num_previous Number of previous actions
 * @param predicted_action Pointer to store predicted action
 * @param confidence Pointer to store prediction confidence (can be NULL)
 * @return true if prediction made, false otherwise
 */
bool mirror_neurons_predict_next_action(
    mirror_neurons_t mirror,
    const action_t* previous_actions,
    uint32_t num_previous,
    action_t* predicted_action,
    float* confidence
);

//=============================================================================
// Integration API - Cognitive Pipeline
//=============================================================================

/**
 * @brief Integrate with working memory
 *
 * WHAT: Store observed actions in working memory for replay
 * WHY:  Enable offline learning, action replay, sequence learning
 * HOW:  Convert observations to working memory items
 *
 * COMPLEXITY: O(1) - just stores pointer
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param working_memory Working memory system handle
 * @return true on success, false on error
 */
bool mirror_neurons_integrate_working_memory(
    mirror_neurons_t mirror,
    void* working_memory  // working_memory_t* (avoid circular dependency)
);

/**
 * @brief Integrate with theory of mind
 *
 * WHAT: Use mirror neurons to infer agent intentions
 * WHY:  Understand why agents perform actions (goal inference)
 * HOW:  Map observed actions to likely goals via theory of mind
 *
 * COMPLEXITY: O(1) - just stores pointer
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param theory_of_mind Theory of mind system handle
 * @return true on success, false on error
 */
bool mirror_neurons_integrate_theory_of_mind(
    mirror_neurons_t mirror,
    void* theory_of_mind  // tom_t* (avoid circular dependency)
);

/**
 * @brief Integrate with predictive processing
 *
 * WHAT: Use mirror neurons for action prediction
 * WHY:  Anticipate others' actions, generate predictions
 * HOW:  Feed mirror neuron activations to predictive network
 *
 * COMPLEXITY: O(1) - just stores pointer
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param mirror Mirror neuron system
 * @param predictive_network Predictive network handle
 * @return true on success, false on error
 */
bool mirror_neurons_integrate_predictive(
    mirror_neurons_t mirror,
    void* predictive_network  // predictive_t* (avoid circular dependency)
);

/**
 * @brief Integrate with glial cell system
 *
 * WHAT: Enable glial cell modulation of mirror neurons
 * WHY:
 * - Astrocytes modulate association strength (stronger/weaker learning)
 * - Oligodendrocytes speed up action recognition (faster matching)
 * - Microglia prune rarely-used mirror neuron associations (memory optimization)
 * HOW:  Create glial integration layer and assign glial cells to mirror neurons
 *
 * Biological basis:
 * - Mirror neurons have dense astrocyte coverage for plasticity modulation
 * - High-frequency mirror neurons benefit from oligodendrocyte myelination
 * - Microglia maintain mirror neuron network efficiency by pruning
 *
 * @param mirror Mirror neuron system
 * @param glial_integration Glial integration system handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1) - just stores pointer
 * THREAD-SAFE: No (requires external synchronization)
 */
bool mirror_neurons_integrate_glial(
    mirror_neurons_t mirror,
    void* glial_integration  // glial_integration_t* (avoid circular dependency)
);

/**
 * @brief Integrate with brain immune system
 *
 * WHAT: Enable bidirectional coupling between mirror neurons and immune system
 * WHY:  Model biological interactions:
 *       - Inflammation reduces empathic resonance (sickness behavior)
 *       - Social isolation triggers inflammatory response
 * HOW:  Create integration layer that modulates resonance based on cytokines
 *       and releases cytokines based on social activity
 *
 * Biological basis:
 * - Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) reduce social motivation
 * - Social isolation increases inflammatory markers (IL-6, CRP)
 * - Mirror neuron activity decreases during illness-induced withdrawal
 * - Social success releases anti-inflammatory cytokines (IL-10)
 *
 * Integration effects:
 * Immune → Mirror:
 *   - Cytokine levels modulate motor resonance suppression
 *   - Inflammation raises empathy threshold (reduces automatic imitation)
 *   - Sickness behavior suppresses observation mode
 *
 * Mirror → Immune:
 *   - Social isolation (>5min no observation) → IL-6 release
 *   - Failed imitation (rejection) → stress cytokines
 *   - Successful imitation → IL-10 (anti-inflammatory)
 *
 * @param mirror Mirror neuron system
 * @param immune_system Brain immune system handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1) - creates integration layer
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @see nimcp_mirror_immune_integration.h for detailed integration API
 * @see Phase 10.11.7 - Mirror Neuron Immune Integration
 */
bool mirror_neurons_connect_immune(
    mirror_neurons_t mirror,
    void* immune_system  // brain_immune_system_t* (avoid circular dependency)
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Retrieve default mirror neuron configuration
 * WHY:  Provide sensible defaults for most use cases
 * HOW:  Return pre-configured struct
 *
 * COMPLEXITY: O(1) - returns stack-allocated struct
 * THREAD-SAFE: Yes (no shared state)
 *
 * @return Default configuration
 */
mirror_neuron_config_t mirror_neurons_get_default_config(void);

/**
 * @brief Create action from features
 *
 * WHAT: Helper to create action_t from feature vector
 * WHY:  Simplify action creation
 * HOW:  Fill in action_t struct with provided data
 *
 * COMPLEXITY: O(n) where n = num_features (for copying)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @param action_id Unique action identifier
 * @param action_name Human-readable name
 * @param features Feature vector
 * @param num_features Number of features (max 32)
 * @param agent_id Performing agent (0 = self)
 * @return action_t struct
 */
action_t mirror_neurons_create_action(
    uint32_t action_id,
    const char* action_name,
    const float* features,
    uint32_t num_features,
    uint32_t agent_id
);

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get social salience for current context
 *
 * WHAT: Query importance of social cues
 * WHY:  Visual cortex can boost attention to social stimuli
 * HOW:  Return agent detection confidence × observation activation
 *
 * BIOLOGY: STS (superior temporal sulcus) modulates V1 for social stimuli
 *
 * @param mirror Mirror neuron system
 * @return Social salience [0, 1]
 */
float mirror_neurons_get_social_salience(mirror_neurons_t mirror);

/**
 * @brief Activate observation mode
 *
 * WHAT: Signal that agent detected, prepare for observation learning
 * WHY:  Visual detection of agents triggers mirror neuron activation
 * HOW:  Prime observation pathways for incoming action features
 *
 * @param mirror Mirror neuron system
 */
void mirror_neurons_activate_observation_mode(mirror_neurons_t mirror);

/**
 * @brief Check if mirror neurons have recent observations
 *
 * WHAT: Determine if other agents observed recently
 * WHY:  Enable Theory of Mind predictions based on observations
 * HOW:  Check observation timestamp against current time
 *
 * @param mirror Mirror neuron system
 * @return true if observations within last 5 seconds, false otherwise
 *
 * COMPLEXITY: O(1)
 */
bool mirror_neurons_has_recent_observations(mirror_neurons_t mirror);

/**
 * @brief Get all mirror neuron activations for Theory of Mind integration
 *
 * WHAT: Extract current activation pattern across all mirror neurons
 * WHY:  Enable ToM to infer agent intentions from mirror neuron activity
 * HOW:  Aggregate observation and execution activations into output array
 *
 * BIOLOGICAL RATIONALE:
 * Mirror neurons fire when observing actions, enabling understanding of others'
 * intentions (Rizzolatti & Craighero, 2004). ToM uses these activations to
 * infer mental states: "What action are they performing, and why?"
 *
 * @param mirror Mirror neuron system
 * @param activations Output buffer for activation values (must have space for max_size floats)
 * @param max_size Maximum number of activations to return (buffer size)
 * @param out_size Output: actual number of activations written
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_actions
 * THREAD-SAFE: Yes (read-only)
 */
bool mirror_neurons_get_all_activations(
    mirror_neurons_t mirror,
    float* activations,
    uint32_t max_size,
    uint32_t* out_size
);

//=============================================================================
// Substrate Integration API (Phase 10.11.2)
//=============================================================================

/**
 * @brief Enable substrate integration for mirror neurons
 *
 * WHAT: Enable biological substrate backing for biologically-realistic behavior
 * WHY:  Provides myelination-dependent timing, dendrite plasticity, glial modulation
 * HOW:  Creates substrate backings for each mirror neuron unit
 *
 * COMPLEXITY: O(num_neurons) for full substrate creation
 * THREAD-SAFE: No (requires exclusive access)
 *
 * @param mirror Mirror neuron system
 * @return true on success, false on error
 */
bool mirror_neurons_enable_substrate(mirror_neurons_t mirror);

/**
 * @brief Connect substrate to axon network
 *
 * WHAT: Wire substrate layer to axon network for propagation delays
 * WHY:  Enable myelination-dependent recognition speed
 * HOW:  Create axons for observation/execution pathways
 *
 * @param mirror Mirror neuron system
 * @param axon_network Axon network handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: No
 */
bool mirror_neurons_connect_axon_network(
    mirror_neurons_t mirror,
    void* axon_network);

/**
 * @brief Connect substrate to dendrite network
 *
 * WHAT: Wire substrate layer to dendrite network for spine plasticity
 * WHY:  Enable structural plasticity for association learning
 * HOW:  Create dendrites with spines for each mirror unit
 *
 * @param mirror Mirror neuron system
 * @param dendrite_network Dendrite network handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: No
 */
bool mirror_neurons_connect_dendrite_network(
    mirror_neurons_t mirror,
    void* dendrite_network);

/**
 * @brief Connect substrate to myelin sheath network
 *
 * WHAT: Wire substrate layer to myelin network for myelination effects
 * WHY:  Enable detailed myelination structural modeling
 * HOW:  Create myelin sheaths for active pathways
 *
 * @param mirror Mirror neuron system
 * @param myelin_network Myelin sheath network handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(num_neurons)
 * THREAD-SAFE: No
 */
bool mirror_neurons_connect_myelin_network(
    mirror_neurons_t mirror,
    void* myelin_network);

/**
 * @brief Get recognition delay for action (substrate-aware)
 *
 * WHAT: Calculate delay for action recognition including substrate effects
 * WHY:  Myelination and axon properties affect recognition speed
 * HOW:  Query substrate backing for delay, fall back to base if no substrate
 *
 * @param mirror Mirror neuron system
 * @param action_id Action to query
 * @return Recognition delay in milliseconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
float mirror_neurons_get_recognition_delay(mirror_neurons_t mirror, uint32_t action_id);

/**
 * @brief Get association strength from spine weights
 *
 * WHAT: Query observation-execution association based on spine plasticity
 * WHY:  Spines encode learned associations in substrate mode
 * HOW:  Sum spine weights for the action's mirror units
 *
 * @param mirror Mirror neuron system
 * @param action_id Action to query
 * @return Association strength (0 to num_spines per unit)
 *
 * COMPLEXITY: O(neurons_for_action * spines_per_neuron)
 * THREAD-SAFE: Yes (read-only)
 */
float mirror_neurons_get_spine_association(mirror_neurons_t mirror, uint32_t action_id);

/**
 * @brief Step substrate simulation forward
 *
 * WHAT: Advance all substrate states by one timestep
 * WHY:  Keep substrate synchronized with simulation time
 * HOW:  Update myelination, spine plasticity, glial states
 *
 * @param mirror Mirror neuron system
 * @param dt_ms Time step in milliseconds
 * @return true on success, false on error
 *
 * COMPLEXITY: O(num_neurons * spines_per_neuron)
 * THREAD-SAFE: No (requires external synchronization)
 */
bool mirror_neurons_step_substrate(mirror_neurons_t mirror, float dt_ms);

/**
 * @brief Check if substrate is enabled
 *
 * @param mirror Mirror neuron system
 * @return true if substrate mode is active
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool mirror_neurons_has_substrate(mirror_neurons_t mirror);

//=============================================================================
// Enhanced Mirror Neuron Systems (Phase 10.11.4-6)
//=============================================================================

/**
 * @brief Forward declarations for enhancement systems
 */
typedef struct mirror_stdp_system* mirror_stdp_t;
typedef struct motor_resonance_system* motor_resonance_t;
typedef struct mirror_hierarchy_system* mirror_hierarchy_t;

/**
 * @brief Enable STDP learning for mirror neurons
 *
 * WHAT: Activate spike-timing dependent plasticity for learning
 * WHY:  Replace simple Hebbian learning with biologically accurate timing rules
 * HOW:  Create STDP system and connect to observation/execution pathways
 *
 * When enabled:
 * - Observation spikes followed by execution spikes = LTP (strengthen)
 * - Execution spikes followed by observation spikes = LTD (weaken)
 * - Supports triplet STDP, homeostasis, metaplasticity, neuromodulator gating
 *
 * @param mirror Mirror neuron system
 * @param max_synapses Maximum STDP-managed synapses (0 = use num_actions)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(max_synapses) for allocation
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @see Phase 10.11.4 - STDP Learning Enhancement
 */
bool mirror_neurons_enable_stdp(mirror_neurons_t mirror, uint32_t max_synapses);

/**
 * @brief Get STDP system handle
 *
 * WHAT: Access the STDP learning system
 * WHY:  Allow direct configuration and monitoring of STDP
 *
 * @param mirror Mirror neuron system
 * @return STDP system handle or NULL if not enabled
 */
mirror_stdp_t mirror_neurons_get_stdp(mirror_neurons_t mirror);

/**
 * @brief Set dopamine level for STDP learning
 *
 * WHAT: Update dopamine modulation of STDP
 * WHY:  Reward signals modulate learning (three-factor rule)
 *
 * @param mirror Mirror neuron system
 * @param level Dopamine level (0-1, 0.5 = baseline)
 */
void mirror_neurons_set_stdp_dopamine(mirror_neurons_t mirror, float level);

/**
 * @brief Enable motor resonance for mirror neurons
 *
 * WHAT: Activate motor resonance with suppression circuits
 * WHY:  Model automatic imitation tendency and its behavioral control
 * HOW:  Create resonance system with BG/PFC suppression
 *
 * When enabled:
 * - Observations automatically prime motor representations
 * - Basal ganglia provides tonic suppression to prevent unwanted imitation
 * - Learning contexts release suppression to allow appropriate imitation
 * - Conflict detection prevents incompatible motor commands
 *
 * @param mirror Mirror neuron system
 * @param max_channels Maximum motor channels (0 = use max_actions)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(max_channels) for allocation
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @see Phase 10.11.5 - Motor Resonance Enhancement
 */
bool mirror_neurons_enable_resonance(mirror_neurons_t mirror, uint32_t max_channels);

/**
 * @brief Get motor resonance system handle
 *
 * WHAT: Access the motor resonance system
 * WHY:  Allow direct configuration of suppression and release
 *
 * @param mirror Mirror neuron system
 * @return Resonance system handle or NULL if not enabled
 */
motor_resonance_t mirror_neurons_get_resonance(mirror_neurons_t mirror);

/**
 * @brief Set learning context for motor resonance
 *
 * WHAT: Signal that current context supports imitation learning
 * WHY:  Release BG suppression when learning by imitation is appropriate
 *
 * @param mirror Mirror neuron system
 * @param learning_strength Learning context strength (0-1)
 */
void mirror_neurons_set_learning_context(mirror_neurons_t mirror, float learning_strength);

/**
 * @brief Set social context for motor resonance
 *
 * WHAT: Signal social interaction context
 * WHY:  Release suppression for appropriate social mirroring
 *
 * @param mirror Mirror neuron system
 * @param social_strength Social context strength (0-1)
 */
void mirror_neurons_set_social_context(mirror_neurons_t mirror, float social_strength);

/**
 * @brief Check if action is above execution threshold
 *
 * WHAT: Query if motor resonance is strong enough for imitation
 * WHY:  Determine when automatic imitation should trigger
 *
 * @param mirror Mirror neuron system
 * @param action_id Action to check
 * @return true if resonance above threshold
 */
bool mirror_neurons_should_imitate(mirror_neurons_t mirror, uint32_t action_id);

/**
 * @brief Enable hierarchical goal representation
 *
 * WHAT: Activate IPL/F5 goal-motor hierarchy
 * WHY:  Separate goal-level from motor-level representation
 * HOW:  Create hierarchy system with goal/motor layers
 *
 * When enabled:
 * - Goals (IPL level): "What is the intention?"
 * - Motors (F5 level): "What are the motor details?"
 * - Bindings connect goals to possible motor implementations
 * - Bottom-up inference: observe motor -> infer goal
 * - Top-down prediction: select goal -> predict motor
 *
 * @param mirror Mirror neuron system
 * @return true on success, false on error
 *
 * COMPLEXITY: O(max_goals + max_motors) for allocation
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @see Phase 10.11.6 - Hierarchical Goals Enhancement
 */
bool mirror_neurons_enable_hierarchy(mirror_neurons_t mirror);

/**
 * @brief Get hierarchy system handle
 *
 * WHAT: Access the goal-motor hierarchy system
 * WHY:  Allow direct configuration and goal/motor management
 *
 * @param mirror Mirror neuron system
 * @return Hierarchy system handle or NULL if not enabled
 */
mirror_hierarchy_t mirror_neurons_get_hierarchy(mirror_neurons_t mirror);

/**
 * @brief Infer goal from observed action
 *
 * WHAT: Determine likely goal given observed motor action
 * WHY:  Understand action intention, not just motor details
 *
 * @param mirror Mirror neuron system
 * @param action_id Observed action
 * @param out_goal Output: inferred goal ID
 * @param out_confidence Output: inference confidence
 * @return true if goal inferred, false if no inference
 */
bool mirror_neurons_infer_goal(mirror_neurons_t mirror, uint32_t action_id,
                                uint32_t* out_goal, float* out_confidence);

/**
 * @brief Select goal for top-down motor control
 *
 * WHAT: Set current intention goal
 * WHY:  Enable goal-directed motor preparation
 *
 * @param mirror Mirror neuron system
 * @param goal_id Goal to select (-1 to clear)
 */
void mirror_neurons_select_goal(mirror_neurons_t mirror, int32_t goal_id);

/**
 * @brief Step all enhancement systems
 *
 * WHAT: Advance STDP, resonance, and hierarchy by one timestep
 * WHY:  Keep enhancement systems synchronized with simulation
 *
 * @param mirror Mirror neuron system
 * @param dt_ms Time step in milliseconds
 * @return true on success
 */
bool mirror_neurons_step_enhancements(mirror_neurons_t mirror, float dt_ms);

//=============================================================================
// Persistence API (Save/Load) - Phase 10.11
//=============================================================================

/**
 * @brief Save mirror neuron system state to file
 *
 * WHAT: Serialize mirror neuron system to binary file
 * WHY:  Enable persistence of learned action associations and statistics
 * HOW:  Write version, config, neurons, actions, agents, and statistics
 *
 * Note: Integration handles (working_memory, theory_of_mind, etc.) are not saved
 *       They must be re-established after loading
 *
 * @param mirror Mirror neuron system
 * @param file Open file handle for writing
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n + a + g) where n=neurons, a=actions, g=agents
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
bool mirror_neurons_save(mirror_neurons_t mirror, FILE* file);

/**
 * @brief Load mirror neuron system state from file
 *
 * WHAT: Deserialize mirror neuron system from binary file
 * WHY:  Restore saved action associations and learning state
 * HOW:  Read version, validate, reconstruct state
 *
 * Note: Integration handles must be set separately via integration functions
 * Note: Brain reference must be set via mirror_neurons_set_brain()
 *
 * @param file Open file handle for reading
 * @return Mirror neuron system handle or NULL on error
 *
 * COMPLEXITY: O(n + a + g) where n=neurons, a=actions, g=agents
 * THREAD-SAFE: Yes (creates new instance)
 */
mirror_neurons_t mirror_neurons_load(FILE* file);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIRROR_NEURONS_H
