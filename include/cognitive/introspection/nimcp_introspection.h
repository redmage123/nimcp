/**
 * @file nimcp_introspection.h
 * @brief Brain introspection and internal state access API
 *
 * WHAT: This module provides APIs to query and inspect the internal state
 * of a neural brain, including neuron activity, learned patterns, uncertainty
 * estimates, and state representations.
 *
 * WHY: Active consciousness requires self-awareness - the ability to examine
 * one's own internal state, understand what patterns are active, measure
 * uncertainty in decisions, and access the neural representation. This is
 * essential for metacognition, explanation, and conscious reasoning.
 *
 * HOW: Provides structured access to internal neural network state:
 * - Active neuron populations (which neurons are firing)
 * - Internal state vectors (compressed brain state)
 * - Uncertainty estimates (epistemic + aleatoric)
 * - Pattern activity queries (is pattern X active?)
 * - Network topology inspection
 * - Activity history and statistics
 *
 * DESIGN PATTERNS:
 * - Factory: Create/destroy introspection handles
 * - Strategy: Different state extraction strategies (fast vs detailed)
 * - Observer: Callbacks for state change events
 * - Memento: Capture/restore brain state snapshots
 * - Facade: Simplified interface to complex neural internals
 *
 * THREAD SAFETY: All functions are thread-safe. Multiple threads can
 * query introspection state concurrently. Mutex protection for internal
 * data structures.
 *
 * PERFORMANCE:
 * - brain_get_active_population: O(n) where n = network size, ~0.1-1ms
 * - brain_get_internal_state: O(n), ~0.5-2ms depending on strategy
 * - brain_get_uncertainty: O(k) where k = ensemble size, ~1-5ms
 * - brain_is_pattern_active: O(1), ~1μs (hash lookup)
 * - brain_get_neuron_activity: O(1), ~1μs (direct access)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_INTROSPECTION_H
#define NIMCP_INTROSPECTION_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_kg_helpers.h"

/* Forward declaration for immune system integration */
struct brain_immune_system;

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Opaque handle to introspection context
 * WHY: Encapsulation - hide implementation details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct introspection_context_struct* introspection_context_t;

/**
 * WHAT: Strategy for state extraction
 * WHY: Trade-off between speed and detail
 * HOW: Fast (~0.5ms) uses sampling, Detailed (~2ms) uses full scan
 */
typedef enum {
    STATE_STRATEGY_FAST,     /* Sample 10% of neurons, ~0.5ms */
    STATE_STRATEGY_BALANCED, /* Sample 30% of neurons, ~1ms */
    STATE_STRATEGY_DETAILED  /* Full scan all neurons, ~2ms */
} state_extraction_strategy_t;

/**
 * WHAT: Type of uncertainty being measured
 * WHY: Different sources of uncertainty require different handling
 * HOW: Epistemic = model uncertainty, Aleatoric = data uncertainty
 */
typedef enum {
    UNCERTAINTY_TYPE_EPISTEMIC, /* Model doesn't know (lack of data) */
    UNCERTAINTY_TYPE_ALEATORIC, /* Inherent randomness in data */
    UNCERTAINTY_TYPE_TOTAL      /* Combined uncertainty */
} uncertainty_type_t;

/**
 * WHAT: Active neuron population data structure
 * WHY: Applications need to know which neurons are firing
 * HOW: Arrays of neuron IDs and their activation levels
 *
 * MEMORY: Caller must free neuron_ids and activation_levels arrays
 */
typedef struct {
    uint32_t* neuron_ids;     /* Array of active neuron IDs */
    float* activation_levels; /* Array of activation values (0-1) */
    uint32_t num_active;      /* Number of active neurons */
    uint32_t total_neurons;   /* Total network size */
    float activity_threshold; /* Threshold used for "active" */
    uint64_t timestamp;       /* When this was captured */
} neuron_population_t;

/**
 * WHAT: Internal brain state representation
 * WHY: Compressed representation for state comparison, logging, transfer
 * HOW: State vector encodes neural activity in compact form
 *
 * MEMORY: Caller must free state_vector and interpretation strings
 */
typedef struct {
    float* state_vector;       /* Compressed state representation */
    uint32_t dimension;        /* Dimensionality of state vector */
    char* interpretation;      /* Human-readable description */
    float compression_ratio;   /* Original size / compressed size */
    float information_content; /* Entropy of state (bits) */
    uint64_t timestamp;        /* When this was captured */
} brain_state_t;

/**
 * WHAT: Uncertainty estimate for a decision
 * WHY: Knowing uncertainty is critical for metacognition
 * HOW: Epistemic (model) + Aleatoric (data) + Total uncertainty
 *
 * INTERPRETATION:
 * - High epistemic → Need more training data
 * - High aleatoric → Inherently uncertain input
 * - Low total → Confident decision
 */
typedef struct {
    float epistemic;             /* Model uncertainty (0-1) */
    float aleatoric;             /* Data uncertainty (0-1) */
    float total;                 /* Combined uncertainty (0-1) */
    float confidence;            /* 1.0 - total uncertainty */
    uint32_t ensemble_size;      /* Number of models used */
    float* ensemble_predictions; /* Individual predictions (optional) */
} brain_uncertainty_t;

/**
 * WHAT: Information about a learned pattern
 * WHY: Track what patterns the brain has learned
 * HOW: Metadata about pattern including activity, strength, age
 */
typedef struct {
    char* pattern_name;        /* Unique pattern identifier */
    float current_activity;    /* Current activation (0-1) */
    float average_activity;    /* Historical average */
    float pattern_strength;    /* How well learned (0-1) */
    uint32_t activation_count; /* Times this pattern activated */
    uint64_t first_learned;    /* Timestamp first learned */
    uint64_t last_activated;   /* Timestamp last activated */
} pattern_info_t;

/**
 * WHAT: Detailed neuron activity information
 * WHY: Deep inspection of individual neuron state
 * HOW: Activation, gradient, connections, contribution to decision
 */
typedef struct {
    uint32_t neuron_id;          /* Neuron identifier */
    float activation;            /* Current activation (0-1) */
    float gradient;              /* Current gradient (backprop) */
    uint32_t num_connections;    /* Number of synapses */
    float total_weight;          /* Sum of connection weights */
    float decision_contribution; /* Contribution to last decision */
    bool is_active;              /* Above activity threshold? */
} neuron_activity_t;

/**
 * WHAT: Network topology statistics
 * WHY: Understanding network structure aids debugging
 * HOW: Connection counts, sparsity, clustering coefficient, etc.
 */
typedef struct {
    uint32_t total_neurons;           /* Total neuron count */
    uint32_t total_connections;       /* Total synapse count */
    float avg_connections_per_neuron; /* Average degree */
    float connection_sparsity;        /* 1.0 - (actual / possible) */
    float clustering_coefficient;     /* Local clustering measure */
    uint32_t num_layers;              /* Network depth */
    uint32_t* neurons_per_layer;      /* Neurons in each layer */
} network_topology_t;

/**
 * WHAT: Activity history entry
 * WHY: Track how brain state evolves over time
 * HOW: Sliding window of recent activity snapshots
 */
typedef struct {
    uint64_t timestamp;       /* When this was recorded */
    float avg_activation;     /* Average neuron activation */
    float max_activation;     /* Maximum neuron activation */
    uint32_t num_active;      /* Count above threshold */
    float energy_consumption; /* Computational energy estimate */
} activity_history_entry_t;

/**
 * WHAT: Introspection statistics
 * WHY: Monitor introspection performance and usage
 * HOW: Counters for API calls, timing, memory usage
 */
typedef struct {
    uint64_t queries_total;             /* Total introspection queries */
    uint64_t queries_active_population; /* Active population queries */
    uint64_t queries_internal_state;    /* Internal state queries */
    uint64_t queries_uncertainty;       /* Uncertainty queries */
    uint64_t queries_pattern;           /* Pattern queries */
    float avg_query_time_ms;            /* Average query time */
    size_t memory_used_bytes;           /* Memory used by introspection */
} introspection_stats_t;

/**
 * WHAT: Callback type for activity sampling events
 * WHY: Allow external observers to react to activity snapshots
 * HOW: Invoked with each sampled activity entry and user context
 *
 * @param entry Activity snapshot data
 * @param user_data User-provided context pointer
 */
typedef void (*activity_sample_callback_t)(const activity_history_entry_t* entry, void* user_data);

/**
 * WHAT: Configuration for introspection context
 * WHY: Customize introspection behavior
 * HOW: Set thresholds, strategies, history size, callbacks
 */
typedef struct {
    state_extraction_strategy_t default_strategy; /* Default state strategy */
    float activity_threshold;                     /* Threshold for "active" neuron */
    uint32_t history_size;                        /* Activity history buffer size */
    bool enable_pattern_tracking;                 /* Track learned patterns? */
    bool enable_uncertainty_estimation;           /* Enable uncertainty? */
    uint32_t uncertainty_ensemble_size;           /* Models for uncertainty */
    bool enable_bio_async;                        /* Enable bio-async communication */
    void (*on_state_change)(brain_state_t* state, void* context); /* Observer */
    void* callback_context;                                       /* Context for callbacks */

    /* Auto activity history configuration */
    bool enable_auto_history;                     /* Auto-populate history? */
    uint32_t history_sample_interval_ms;          /* Sampling rate (ms) */
    float history_change_threshold;               /* Min change to record */
} introspection_config_t;

/* ========================================================================
 * CORE INTROSPECTION API
 * ======================================================================== */

/**
 * WHAT: Get default introspection configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Returns pre-configured struct with balanced settings
 *
 * DEFAULT SETTINGS:
 * - Strategy: BALANCED (30% sampling, ~1ms)
 * - Activity threshold: 0.3
 * - History size: 100 entries
 * - Pattern tracking: enabled
 * - Uncertainty estimation: enabled
 * - Ensemble size: 5 models
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
introspection_config_t introspection_default_config(void);

/**
 * WHAT: Create introspection context for a brain
 * WHY: Initialize introspection subsystem
 * HOW: Allocate context, attach to brain, configure tracking
 *
 * DESIGN PATTERN: Factory
 *
 * @param brain The brain to introspect
 * @param config Configuration (NULL for defaults)
 * @return Introspection context handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if brain is NULL
 * - Returns NULL if allocation fails
 *
 * MEMORY: Caller must call introspection_context_destroy() when done
 *
 * COMPLEXITY: O(n) where n = network size (for topology analysis)
 * THREAD-SAFE: Yes
 */
introspection_context_t introspection_context_create(brain_t brain,
                                                     const introspection_config_t* config);

/**
 * WHAT: Destroy introspection context and free resources
 * WHY: Prevent memory leaks
 * HOW: Free all allocations, detach from brain, clear callbacks
 *
 * DESIGN PATTERN: Factory (destruction)
 *
 * @param context Context to destroy
 *
 * SAFETY: Safe to call with NULL context
 * MEMORY: Frees all internal allocations
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller must ensure no concurrent access)
 */
void introspection_context_destroy(introspection_context_t context);

/* ========================================================================
 * NEURON POPULATION QUERIES
 * ======================================================================== */

/**
 * WHAT: Get currently active neuron population
 * WHY: See which neurons are firing for given input
 * HOW: Scan network for neurons above activity threshold
 *
 * USAGE:
 * ```c
 * neuron_population_t pop = brain_get_active_population(context, 0.5);
 * printf("Active neurons: %u / %u\n", pop.num_active, pop.total_neurons);
 * for (uint32_t i = 0; i < pop.num_active; i++) {
 *     printf("  Neuron %u: %.2f\n", pop.neuron_ids[i], pop.activation_levels[i]);
 * }
 * neuron_population_free(&pop);
 * ```
 *
 * @param context Introspection context
 * @param threshold Minimum activation to be considered "active" (0-1)
 * @return Population structure (must be freed with neuron_population_free)
 *
 * ERRORS: Returns empty population (num_active=0) on error
 *
 * COMPLEXITY: O(n) where n = network size
 * THREAD-SAFE: Yes
 */
neuron_population_t brain_get_active_population(introspection_context_t context, float threshold);

/**
 * WHAT: Free neuron population structure
 * WHY: Release allocated memory
 * HOW: Free arrays, zero struct
 *
 * @param population Population to free
 *
 * SAFETY: Safe to call with NULL or already-freed population
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void neuron_population_free(neuron_population_t* population);

/**
 * WHAT: Get detailed activity for a specific neuron
 * WHY: Deep inspection of individual neuron behavior
 * HOW: Access neuron structure and compute statistics
 *
 * @param context Introspection context
 * @param neuron_id ID of neuron to query
 * @return Neuron activity structure
 *
 * ERRORS: Returns zeroed struct with is_active=false on invalid ID
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
neuron_activity_t brain_get_neuron_activity(introspection_context_t context, uint32_t neuron_id);

/* ========================================================================
 * INTERNAL STATE QUERIES
 * ======================================================================== */

/**
 * WHAT: Get compressed internal state representation
 * WHY: State vector for logging, comparison, transfer, analysis
 * HOW: Extract neural activations and compress to vector
 *
 * DESIGN PATTERN: Strategy (uses configured extraction strategy)
 *
 * STRATEGIES:
 * - FAST: Sample 10% of neurons → 10% size, ~0.5ms, 90% accuracy
 * - BALANCED: Sample 30% → 30% size, ~1ms, 95% accuracy
 * - DETAILED: Full scan → 100% size, ~2ms, 100% accuracy
 *
 * USAGE:
 * ```c
 * brain_state_t state = brain_get_internal_state(context, STATE_STRATEGY_BALANCED);
 * printf("State dimension: %u\n", state.dimension);
 * printf("Interpretation: %s\n", state.interpretation);
 * printf("Information content: %.2f bits\n", state.information_content);
 * brain_state_free(&state);
 * ```
 *
 * @param context Introspection context
 * @param strategy Extraction strategy (or use config default if < 0)
 * @return Brain state structure (must be freed with brain_state_free)
 *
 * ERRORS: Returns empty state (dimension=0) on error
 *
 * COMPLEXITY: O(n*s) where n=network size, s=sampling rate
 * THREAD-SAFE: Yes
 */
brain_state_t brain_get_internal_state(introspection_context_t context,
                                       state_extraction_strategy_t strategy);

/**
 * WHAT: Free brain state structure
 * WHY: Release allocated memory
 * HOW: Free vector, interpretation string, zero struct
 *
 * @param state State to free
 *
 * SAFETY: Safe to call with NULL or already-freed state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void brain_state_free(brain_state_t* state);

/**
 * WHAT: Compare two brain states for similarity
 * WHY: Detect state changes, measure drift, verify consistency
 * HOW: Cosine similarity between state vectors
 *
 * @param state1 First state
 * @param state2 Second state
 * @return Similarity score (0=completely different, 1=identical)
 *
 * ERRORS: Returns 0.0 if states have different dimensions
 *
 * COMPLEXITY: O(d) where d = state dimension
 * THREAD-SAFE: Yes
 */
float brain_state_similarity(const brain_state_t* state1, const brain_state_t* state2);

/* ========================================================================
 * UNCERTAINTY ESTIMATION
 * ======================================================================== */

/**
 * WHAT: Estimate uncertainty for a decision
 * WHY: Metacognition requires knowing when brain is uncertain
 * HOW: Ensemble method - train multiple models, measure variance
 *
 * DESIGN PATTERN: Strategy (ensemble-based uncertainty estimation)
 *
 * METHOD:
 * 1. Train small ensemble of models (default 5) on same data
 * 2. Get predictions from all models for input
 * 3. Variance = epistemic uncertainty (model doesn't know)
 * 4. Entropy of each prediction = aleatoric uncertainty (data is noisy)
 * 5. Total = epistemic + aleatoric
 *
 * USAGE:
 * ```c
 * float features[13] = {...};
 * brain_uncertainty_t unc = brain_get_uncertainty(context, features, 13);
 * if (unc.epistemic > 0.7) {
 *     printf("Model uncertainty high - need more training data\n");
 * }
 * if (unc.aleatoric > 0.7) {
 *     printf("Data uncertainty high - input is ambiguous\n");
 * }
 * if (unc.confidence > 0.9) {
 *     printf("High confidence - safe to act\n");
 * }
 * brain_uncertainty_free(&unc);
 * ```
 *
 * @param context Introspection context
 * @param features Input features
 * @param num_features Number of features
 * @return Uncertainty structure (must be freed with brain_uncertainty_free)
 *
 * ERRORS: Returns zeroed struct on error
 *
 * COMPLEXITY: O(k*m) where k=ensemble size, m=model complexity
 * TIME: ~1-5ms depending on ensemble size
 * THREAD-SAFE: Yes
 */
brain_uncertainty_t brain_get_uncertainty(introspection_context_t context, const float* features,
                                          uint32_t num_features);

/**
 * WHAT: Free uncertainty structure
 * WHY: Release ensemble predictions array
 * HOW: Free array, zero struct
 *
 * @param uncertainty Uncertainty to free
 *
 * SAFETY: Safe to call with NULL or already-freed uncertainty
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void brain_uncertainty_free(brain_uncertainty_t* uncertainty);

/* ========================================================================
 * PATTERN QUERIES
 * ======================================================================== */

/**
 * WHAT: Check if a named pattern is currently active
 * WHY: Query specific learned patterns by name
 * HOW: Hash lookup in pattern registry
 *
 * PATTERNS: Named groups of neurons that fire together
 * Example patterns: "face_detected", "threat_response", "goal_achieved"
 *
 * USAGE:
 * ```c
 * if (brain_is_pattern_active(context, "threat_response")) {
 *     printf("Threat pattern active - triggering safety response\n");
 * }
 * ```
 *
 * @param context Introspection context
 * @param pattern_name Name of pattern to query
 * @return true if pattern is currently active, false otherwise
 *
 * ERRORS: Returns false if pattern doesn't exist or context is NULL
 *
 * COMPLEXITY: O(1) - hash lookup
 * THREAD-SAFE: Yes
 */
bool brain_is_pattern_active(introspection_context_t context, const char* pattern_name);

/**
 * WHAT: Get detailed information about a pattern
 * WHY: Inspect pattern metadata and history
 * HOW: Lookup pattern in registry, return info struct
 *
 * @param context Introspection context
 * @param pattern_name Pattern to query
 * @return Pattern info structure (must be freed with pattern_info_free)
 *
 * ERRORS: Returns NULL if pattern doesn't exist
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
pattern_info_t* brain_get_pattern_info(introspection_context_t context, const char* pattern_name);

/**
 * WHAT: Free pattern info structure
 * WHY: Release allocated memory
 * HOW: Free name string, free struct
 *
 * @param info Pattern info to free
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void pattern_info_free(pattern_info_t* info);

/**
 * WHAT: Get list of all registered patterns
 * WHY: Discover what patterns brain has learned
 * HOW: Return array of pattern names
 *
 * @param context Introspection context
 * @param num_patterns Output: number of patterns
 * @return Array of pattern names (must be freed with pattern_list_free)
 *
 * ERRORS: Returns NULL if context is NULL
 *
 * COMPLEXITY: O(p) where p = number of patterns
 * THREAD-SAFE: Yes
 */
char** brain_list_patterns(introspection_context_t context, uint32_t* num_patterns);

/**
 * WHAT: Free pattern list
 * WHY: Release memory from brain_list_patterns
 * HOW: Free each string, free array
 *
 * @param pattern_list List to free
 * @param num_patterns Number of patterns
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(p) where p = number of patterns
 * THREAD-SAFE: Yes
 */
void pattern_list_free(char** pattern_list, uint32_t num_patterns);

/* ========================================================================
 * NETWORK TOPOLOGY
 * ======================================================================== */

/**
 * WHAT: Get network topology statistics
 * WHY: Understand network structure
 * HOW: Analyze connection graph, compute metrics
 *
 * @param context Introspection context
 * @return Topology structure (must be freed with network_topology_free)
 *
 * ERRORS: Returns zeroed struct on error
 *
 * COMPLEXITY: O(n + e) where n=neurons, e=edges
 * THREAD-SAFE: Yes
 */
network_topology_t brain_get_topology(introspection_context_t context);

/**
 * WHAT: Free topology structure
 * WHY: Release neurons_per_layer array
 * HOW: Free array, zero struct
 *
 * @param topology Topology to free
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void network_topology_free(network_topology_t* topology);

/* ========================================================================
 * ACTIVITY HISTORY
 * ======================================================================== */

/**
 * WHAT: Get recent activity history
 * WHY: Track how brain state evolves over time
 * HOW: Return sliding window of recent snapshots
 *
 * @param context Introspection context
 * @param num_entries Output: number of history entries
 * @return Array of history entries (must be freed)
 *
 * ERRORS: Returns NULL if context is NULL
 *
 * COMPLEXITY: O(h) where h = history size
 * THREAD-SAFE: Yes
 */
activity_history_entry_t* brain_get_activity_history(introspection_context_t context,
                                                     uint32_t* num_entries);

/**
 * WHAT: Sample current brain activity and add to history
 * WHY: Enable auto-population of activity history from brain processing loop
 * HOW: Computes avg/max activation, active count, energy, adds to queue
 *
 * DESIGN PATTERN: Observer (samples state at regular intervals)
 *
 * WHAT IT DOES:
 * - Computes average activation across network
 * - Finds maximum activation value
 * - Counts neurons above activity threshold
 * - Estimates energy consumption based on activity
 * - Enqueues entry to activity history queue
 * - Invokes registered callback if present
 * - Sends bio-async BIO_MSG_ACTIVITY_SAMPLE message
 *
 * @param context Introspection context
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context is NULL or network unavailable
 *
 * COMPLEXITY: O(n) where n = network size
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * ```c
 * // In brain processing loop
 * if (should_sample_activity(brain)) {
 *     introspection_sample_activity(brain->introspection);
 * }
 * ```
 */
bool introspection_sample_activity(introspection_context_t context);

/**
 * WHAT: Set activity sampling interval
 * WHY: Allow runtime adjustment of sampling rate
 * HOW: Updates interval_ms in context configuration
 *
 * @param context Introspection context
 * @param interval_ms New sampling interval in milliseconds
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_set_sample_interval(introspection_context_t context, uint32_t interval_ms);

/**
 * WHAT: Get activity history buffer statistics
 * WHY: Monitor history usage and performance
 * HOW: Returns current size, capacity, utilization
 *
 * @param context Introspection context
 * @param current_size Output: current number of entries
 * @param capacity Output: maximum capacity
 * @param utilization Output: percentage full (0.0-1.0)
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context is NULL or outputs are NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_get_history_stats(introspection_context_t context,
                                     uint32_t* current_size,
                                     uint32_t* capacity,
                                     float* utilization);

/**
 * WHAT: Clear activity history queue
 * WHY: Reset history tracking (e.g., after major brain state change)
 * HOW: Clears all entries from activity queue
 *
 * @param context Introspection context
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context is NULL
 *
 * COMPLEXITY: O(h) where h = current history size
 * THREAD-SAFE: Yes
 */
bool introspection_clear_history(introspection_context_t context);

/**
 * WHAT: Register callback for activity sampling events
 * WHY: Allow external observers to react to each activity snapshot
 * HOW: Stores callback and context, invokes on each sample
 *
 * DESIGN PATTERN: Observer (callback registration)
 *
 * @param context Introspection context
 * @param callback Callback function (NULL to unregister)
 * @param user_data User context passed to callback
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * ```c
 * void my_activity_callback(const activity_history_entry_t* entry, void* data) {
 *     printf("Activity: avg=%.2f max=%.2f active=%u energy=%.2f\n",
 *            entry->avg_activation, entry->max_activation,
 *            entry->num_active, entry->energy_consumption);
 * }
 * introspection_set_activity_callback(ctx, my_activity_callback, NULL);
 * ```
 */
bool introspection_set_activity_callback(introspection_context_t context,
                                         activity_sample_callback_t callback,
                                         void* user_data);

/* ========================================================================
 * STATISTICS AND MONITORING
 * ======================================================================== */

/**
 * WHAT: Get introspection statistics
 * WHY: Monitor performance and usage
 * HOW: Return counters and metrics
 *
 * @param context Introspection context
 * @param stats Output: statistics structure
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_get_stats(introspection_context_t context, introspection_stats_t* stats);

/**
 * WHAT: Reset introspection statistics
 * WHY: Clear counters for new measurement period
 * HOW: Zero all counters except memory usage
 *
 * @param context Introspection context
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void introspection_reset_stats(introspection_context_t context);

/* ========================================================================
 * IMMUNE SYSTEM INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Connect brain immune system to introspection module
 * WHY: Enable immune state to influence consciousness metrics and metacognition
 * HOW: Store immune system reference, register for immune event callbacks
 *
 * INTEGRATION EFFECTS:
 * - Inflammation reduces consciousness phi (IIT metric)
 * - Threat patterns appear in temporal pattern detection
 * - Antibody effectiveness contributes to ensemble uncertainty
 * - Immune phase visible in state reporting
 * - Memory cell formation triggers metacognitive callbacks
 *
 * BIOLOGICAL BASIS:
 * - Systemic inflammation reduces consciousness (cytokine-induced fatigue)
 * - Immune activation affects cognitive performance
 * - Fever states alter consciousness levels
 * - Immune memory formation is a metacognitive learning event
 *
 * @param context Introspection context
 * @param immune_system Brain immune system to connect
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context or immune_system is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * ```c
 * brain_immune_system_t* immune = brain_immune_create(NULL);
 * introspection_context_t intro = brain_get_introspection(brain);
 * introspection_connect_immune(intro, immune);
 * ```
 */
bool introspection_connect_immune(introspection_context_t context,
                                   struct brain_immune_system* immune_system);

/**
 * WHAT: Get immune system from introspection context
 * WHY: Access immune system for external queries
 * HOW: Return immune system reference
 *
 * @param context Introspection context
 * @return Immune system or NULL if none connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
struct brain_immune_system* introspection_get_immune(introspection_context_t context);

/* ========================================================================
 * INTERNAL KNOWLEDGE GRAPH INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Connect introspection to internal brain Knowledge Graph
 * WHY: Enable module topology awareness for self-inspection
 * HOW: Initialize KG context, cache own node ID
 *
 * INTEGRATION EFFECTS:
 * - Can query which modules are currently ACTIVE in KG
 * - Can traverse module relationships via KG topology
 * - Can discover module capabilities from KG node types
 *
 * @param context Introspection context
 * @param brain Brain instance containing internal KG
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_connect_internal_kg(introspection_context_t context, brain_t brain);

/**
 * WHAT: Disconnect from internal KG
 * WHY: Proper cleanup before shutdown
 * HOW: Clear KG context
 *
 * @param context Introspection context
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void introspection_disconnect_internal_kg(introspection_context_t context);

/**
 * WHAT: Get all currently ACTIVE modules from KG
 * WHY: Self-awareness of what modules are running
 * HOW: Query KG for nodes in BRAIN_KG_STATE_ACTIVE state
 *
 * @param context Introspection context
 * @return Node list of active modules (caller must free), or NULL
 *
 * COMPLEXITY: O(n) where n = KG node count
 * THREAD-SAFE: Yes
 */
brain_kg_node_list_t* introspection_get_active_modules(introspection_context_t context);

/**
 * WHAT: Get module topology from a center point
 * WHY: Understand which modules are reachable from a given module
 * HOW: Use KG traversal to find connected nodes
 *
 * @param context Introspection context
 * @param center_name Module name to start from
 * @param max_depth Maximum traversal depth
 * @return Node list of reachable modules (caller must free), or NULL
 *
 * COMPLEXITY: O(n) where n = reachable nodes
 * THREAD-SAFE: Yes
 */
brain_kg_node_list_t* introspection_get_module_topology(
    introspection_context_t context,
    const char* center_name,
    uint32_t max_depth
);

/**
 * WHAT: Check if a set of modules are all active in KG
 * WHY: Pattern matching - check if module combination is running
 * HOW: Query KG for each module's state
 *
 * @param context Introspection context
 * @param module_names Array of module names to check
 * @param count Number of modules
 * @return true if all are active, false otherwise
 *
 * COMPLEXITY: O(k) where k = module count
 * THREAD-SAFE: Yes
 */
bool introspection_is_module_set_active(
    introspection_context_t context,
    const char** module_names,
    uint32_t count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_H */
