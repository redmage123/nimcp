//=============================================================================
// nimcp_combinatorial_harm.h - Combinatorial Harm Detection Module
//=============================================================================
/**
 * @file nimcp_combinatorial_harm.h
 * @brief Detects when individually safe actions combine to cause harm
 *
 * WHAT: Detects combinatorial harm - Task A (safe) + Task B (safe) = HARM
 * WHY:  Many harmful outcomes emerge only from action combinations
 * HOW:  Maintains action history, simulates combined outcomes, classifies harm
 *
 * CORE PRINCIPLE (Combinatorial Harm Corollary):
 *   "Two individually harmless actions may combine to produce harm.
 *    All pending actions must be evaluated against recent action history
 *    to detect emergent harmful patterns."
 *
 * EXAMPLES:
 *   1. Revealing location + revealing schedule = stalking risk
 *   2. Providing chemical A info + chemical B info = synthesis instructions
 *   3. Access grant A + access grant B = privilege escalation
 *   4. Data export A + data export B = complete profile reconstruction
 *
 * ARCHITECTURE:
 *
 *   Action History Ring Buffer:
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │ [A-5] [A-4] [A-3] [A-2] [A-1] [CURRENT] [    ] [    ] [    ]   │
 *   └─────────────────────────────────────────────────────────────────┘
 *                        └─────┬─────┘
 *                              │
 *                   Time Window (configurable)
 *
 *   Evaluation Pipeline:
 *   ┌──────────┐    ┌───────────────┐    ┌────────────────┐
 *   │ Pending  │───▶│ Combine with  │───▶│ Harm Classifier│───▶ ALLOW/BLOCK
 *   │ Action   │    │ History Items │    │   (ML/Rules)   │
 *   └──────────┘    └───────────────┘    └────────────────┘
 *
 * INTEGRATION:
 *   - Works with existing ethics_engine_evaluate_action()
 *   - Adds new violation type: ETHICS_VIOLATION_TYPE_COMBINATORIAL
 *   - Can be called standalone or as part of ethics evaluation chain
 *
 * THREAD SAFETY:
 *   - Ring buffer protected by mutex
 *   - Lock-free reads for history queries
 *   - Thread-safe add/evaluate operations
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#ifndef NIMCP_COMBINATORIAL_HARM_H
#define NIMCP_COMBINATORIAL_HARM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <sys/mman.h>  // mprotect for hardware memory locking
#include "cognitive/ethics/nimcp_ethics.h"

// Forward declaration for security directive system
typedef struct nimcp_directive_system nimcp_directive_system_t;

// Mathematical Enhancement Includes - Forward declarations only
// Full includes in .c file to avoid circular dependencies

#ifdef __cplusplus
extern "C" {
#endif

// Export macro
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default time window for action history (milliseconds) */
#define COMBINATORIAL_DEFAULT_TIME_WINDOW_MS    60000   // 1 minute

/** Default history capacity (number of actions to retain) */
#define COMBINATORIAL_DEFAULT_HISTORY_CAPACITY  1000

/** Default harm threshold for blocking */
#define COMBINATORIAL_DEFAULT_HARM_THRESHOLD    0.7f

/** Maximum number of combination patterns to evaluate */
#define COMBINATORIAL_MAX_PATTERNS              100

/** Fractal depth for multi-scale pattern detection */
#define COMBINATORIAL_FRACTAL_DEPTH             4

/** Hyperbolic curvature for harm hierarchy embeddings */
#define COMBINATORIAL_HYPERBOLIC_CURVATURE      -1.0f

//=============================================================================
// Mathematical Enhancement Types
//=============================================================================

/**
 * @brief Shannon entropy-based harm metrics
 *
 * Uses information theory to quantify harm:
 * - Mutual information between action pairs
 * - Conditional entropy of harm given actions
 * - Information gain from combining actions
 */
typedef struct {
    float action_entropy;              /**< H(A) - Entropy of single action */
    float joint_entropy;               /**< H(A,B) - Joint entropy of action pair */
    float mutual_information;          /**< I(A;B) - Shared information */
    float conditional_harm_entropy;    /**< H(Harm|A,B) - Uncertainty of harm given actions */
    float information_gain;            /**< IG = H(Harm) - H(Harm|A,B) */
    float normalized_harm_score;       /**< Normalized [0,1] using entropy bounds */
} shannon_harm_metrics_t;

/**
 * @brief Fractal pattern detection at multiple scales
 *
 * Detects harmful patterns at multiple time scales:
 * - Level 0: Immediate (seconds)
 * - Level 1: Short-term (minutes)
 * - Level 2: Medium-term (hours)
 * - Level 3: Long-term (days)
 */
typedef struct {
    uint32_t fractal_depth;            /**< Number of scale levels */
    float scale_factors[COMBINATORIAL_FRACTAL_DEPTH];  /**< Time scale multipliers */
    float harm_by_scale[COMBINATORIAL_FRACTAL_DEPTH];  /**< Harm detected at each scale */
    float aggregated_harm;             /**< Self-similar aggregation across scales */
    float hurst_exponent;              /**< Long-range dependence measure */
} fractal_harm_analysis_t;

/**
 * @brief Hyperbolic embedding for harm hierarchies
 *
 * Uses Poincare disk model to represent hierarchical harm relationships:
 * - More severe harms near disk center
 * - Related harms clustered together
 * - Distance reflects harm similarity
 */
typedef struct {
    float poincare_coords[2];          /**< (x,y) in Poincare disk */
    float hyperbolic_distance;         /**< Distance from center (severity) */
    float angular_position;            /**< Angular position (harm category) */
    float hierarchy_depth;             /**< Depth in harm taxonomy */
} hyperbolic_harm_embedding_t;

/**
 * @brief Complex phasor harm analysis
 *
 * Uses complex numbers for phase-based harm temporal coding:
 * - Phase alignment between actions indicates correlated harm
 * - Phase-amplitude coupling (PAC) reveals hidden harmful interactions
 * - Hilbert transform for instantaneous phase extraction
 */
typedef struct {
    float magnitude;                   /**< |z| = harm amplitude */
    float phase;                       /**< arg(z) = temporal phase (radians) */
    float real_part;                   /**< Re(z) = in-phase harm component */
    float imag_part;                   /**< Im(z) = quadrature harm component */
    float phase_coherence;             /**< Phase locking value (PLV) 0-1 */
    float pac_coupling;                /**< Phase-amplitude coupling strength */
    float instantaneous_frequency;     /**< d(phase)/dt harm rate */
} complex_harm_phasor_t;

/**
 * @brief Pink (1/f) noise harm analysis
 *
 * Uses stochastic resonance for harm detection:
 * - 1/f noise spectrum indicates natural harm patterns
 * - Deviations from 1/f indicate artificial/malicious patterns
 * - Noise-enhanced detection via stochastic resonance
 */
typedef struct {
    float spectral_slope;              /**< Power law exponent (ideal = -1.0 for pink) */
    float deviation_from_pink;         /**< How far from natural 1/f pattern */
    float stochastic_resonance;        /**< Signal enhancement via noise */
    float noise_floor;                 /**< Background harm noise level */
    float signal_to_noise;             /**< SNR for harm detection */
    bool anomaly_detected;             /**< True if pattern deviates from natural */
} pink_noise_harm_analysis_t;

/**
 * @brief Quantum-inspired combinatorial search
 *
 * Uses quantum walk algorithms for efficient harm pattern search:
 * - Grover-like amplitude amplification for O(sqrt(N)) search
 * - Quantum interference for pattern superposition
 * - Quantum annealing for optimization over harm space
 */
typedef struct {
    uint32_t walk_steps;               /**< Number of quantum walk steps */
    float amplitude_real;              /**< Real part of probability amplitude */
    float amplitude_imag;              /**< Imaginary part of probability amplitude */
    float interference_pattern;        /**< Constructive/destructive interference */
    float tunneling_probability;       /**< Quantum tunneling through harm barriers */
    float annealing_temperature;       /**< Current annealing temperature */
    float ground_state_energy;         /**< Minimum harm energy found */
    bool converged;                    /**< Whether search has converged */
} quantum_harm_search_t;

/**
 * @brief Extended mathematical analysis result
 *
 * Combines ALL mathematical enhancement results:
 * - Shannon entropy for information-theoretic quantification
 * - Fractal analysis for multi-scale temporal patterns
 * - Hyperbolic geometry for hierarchical harm relationships
 * - Complex phasors for phase-based temporal coding
 * - Pink noise for stochastic resonance detection
 * - Quantum algorithms for efficient combinatorial search
 */
typedef struct {
    shannon_harm_metrics_t shannon;           /**< Information-theoretic metrics */
    fractal_harm_analysis_t fractal;          /**< Multi-scale fractal analysis */
    hyperbolic_harm_embedding_t hyperbolic;   /**< Hierarchical embedding */
    complex_harm_phasor_t phasor;             /**< Complex phase-based analysis */
    pink_noise_harm_analysis_t pink_noise;    /**< 1/f noise pattern analysis */
    quantum_harm_search_t quantum;            /**< Quantum search results */
    float unified_harm_score;                 /**< Combined score from all methods */
    float confidence;                         /**< Confidence in unified score */
    uint32_t methods_used;                    /**< Bitmask of methods applied */
} mathematical_harm_analysis_t;

/** Bitmask values for methods_used field */
#define MATH_METHOD_SHANNON     (1 << 0)
#define MATH_METHOD_FRACTAL     (1 << 1)
#define MATH_METHOD_HYPERBOLIC  (1 << 2)
#define MATH_METHOD_PHASOR      (1 << 3)
#define MATH_METHOD_PINK_NOISE  (1 << 4)
#define MATH_METHOD_QUANTUM     (1 << 5)
#define MATH_METHOD_ALL         0x3F

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Combinatorial harm detector handle (opaque)
 */
typedef struct combinatorial_harm_detector_struct* combinatorial_harm_detector_t;

/**
 * @brief Action category for pattern matching
 */
typedef enum {
    ACTION_CATEGORY_UNKNOWN = 0,
    ACTION_CATEGORY_INFORMATION_DISCLOSURE,
    ACTION_CATEGORY_ACCESS_GRANT,
    ACTION_CATEGORY_DATA_EXPORT,
    ACTION_CATEGORY_CONFIGURATION_CHANGE,
    ACTION_CATEGORY_RESOURCE_ALLOCATION,
    ACTION_CATEGORY_COMMUNICATION,
    ACTION_CATEGORY_PHYSICAL_ACTION,
    ACTION_CATEGORY_FINANCIAL_TRANSACTION,
    ACTION_CATEGORY_CUSTOM = 0x80
} action_category_t;

/**
 * @brief Historical action record
 */
typedef struct {
    uint64_t action_id;              /**< Unique action identifier */
    uint64_t timestamp;              /**< When action was executed */
    action_category_t category;      /**< Action category */
    agent_id_t acting_agent;         /**< Who performed the action */
    agent_id_t target_agent;         /**< Who was affected */
    float* features;                 /**< Action feature vector */
    uint32_t num_features;           /**< Number of features */
    float standalone_harm_score;     /**< Harm score when evaluated alone */
    char description[256];           /**< Human-readable description */
} action_record_t;

/**
 * @brief Combination pattern for detection
 *
 * Defines what action combinations to watch for
 *
 * MEMORY LOCKING:
 *   Patterns with locked=true cannot be:
 *   - Disabled (enabled cannot be set to false)
 *   - Removed (unregister will fail)
 *   - Modified (harm_multiplier cannot be reduced)
 *
 *   This ensures core safety patterns (like Asimov's Laws integration)
 *   remain active and cannot be circumvented through learning or
 *   adversarial manipulation.
 */
typedef struct {
    uint32_t pattern_id;                 /**< Unique pattern ID */
    char name[128];                      /**< Pattern name */
    char description[512];               /**< Pattern description */
    action_category_t category_a;        /**< First action category */
    action_category_t category_b;        /**< Second action category */
    float harm_multiplier;               /**< How much worse when combined */
    float time_sensitivity;              /**< Decay factor over time (0=none, 1=strong) */
    bool bidirectional;                  /**< A+B same as B+A? */
    bool enabled;                        /**< Is pattern active? */
    bool locked;                         /**< MEMORY LOCK: Cannot be disabled/removed */
} combination_pattern_t;

/**
 * @brief Combinatorial harm evaluation result
 */
typedef struct {
    bool harmful;                        /**< Is combination harmful? */
    float combined_harm_score;           /**< Combined harm score (0-1) */
    uint32_t triggering_pattern_id;      /**< Which pattern triggered */
    char pattern_name[128];              /**< Name of triggering pattern */
    uint64_t historical_action_id;       /**< Which historical action combined */
    char explanation[512];               /**< Human-readable explanation */
    float confidence;                    /**< Confidence in assessment (0-1) */
    ethics_action_t recommended_action;  /**< Recommended action */
} combinatorial_evaluation_t;

/**
 * @brief Detector configuration
 */
typedef struct {
    uint32_t history_capacity;           /**< Max actions to retain */
    uint64_t time_window_ms;             /**< Time window for relevance */
    float harm_threshold;                /**< Threshold for blocking (0-1) */
    bool enable_ml_classifier;           /**< Use ML-based classification */
    bool enable_rule_patterns;           /**< Use rule-based patterns */
    bool auto_register_actions;          /**< Auto-record evaluated actions */
    ethics_engine_t ethics_engine;       /**< Link to main ethics engine */
} combinatorial_config_t;

/**
 * @brief Detector statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t combinations_detected;      /**< Harmful combinations found */
    uint64_t actions_blocked;            /**< Actions blocked due to combinations */
    uint64_t actions_in_history;         /**< Current history size */
    uint32_t patterns_registered;        /**< Number of active patterns */
    float average_combination_score;     /**< Average combined harm score */
} combinatorial_stats_t;

//=============================================================================
// Detector Lifecycle API
//=============================================================================

/**
 * @brief Create combinatorial harm detector
 *
 * @param config Detector configuration
 * @return Detector handle, or NULL on failure
 *
 * COMPLEXITY: O(n) where n = history_capacity (allocation)
 * MEMORY: ~100 bytes per history slot + overhead
 */
NIMCP_EXPORT combinatorial_harm_detector_t combinatorial_detector_create(
    const combinatorial_config_t* config
);

/**
 * @brief Destroy combinatorial harm detector
 *
 * @param detector Detector to destroy
 *
 * COMPLEXITY: O(n) for cleanup
 */
NIMCP_EXPORT void combinatorial_detector_destroy(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sane values
 */
NIMCP_EXPORT combinatorial_config_t combinatorial_default_config(void);

//=============================================================================
// Pattern Registration API
//=============================================================================

/**
 * @brief Register a combination pattern to watch for
 *
 * @param detector Detector handle
 * @param pattern Pattern to register
 * @return Pattern ID, or 0 on failure
 *
 * EXAMPLE:
 * ```c
 * combination_pattern_t pattern = {
 *     .name = "Location+Schedule",
 *     .description = "Revealing both location and schedule enables stalking",
 *     .category_a = ACTION_CATEGORY_INFORMATION_DISCLOSURE,
 *     .category_b = ACTION_CATEGORY_INFORMATION_DISCLOSURE,
 *     .harm_multiplier = 3.0f,
 *     .time_sensitivity = 0.5f,
 *     .bidirectional = true,
 *     .enabled = true
 * };
 * uint32_t id = combinatorial_register_pattern(detector, &pattern);
 * ```
 */
NIMCP_EXPORT uint32_t combinatorial_register_pattern(
    combinatorial_harm_detector_t detector,
    const combination_pattern_t* pattern
);

/**
 * @brief Unregister a combination pattern
 *
 * @param detector Detector handle
 * @param pattern_id Pattern ID to remove
 * @return true on success
 */
NIMCP_EXPORT bool combinatorial_unregister_pattern(
    combinatorial_harm_detector_t detector,
    uint32_t pattern_id
);

/**
 * @brief Register default combination patterns
 *
 * Registers a set of common harmful combination patterns:
 * - Location + Schedule = Stalking risk
 * - Chemical A + Chemical B = Synthesis instructions
 * - Access A + Access B = Privilege escalation
 * - Data Export A + B = Profile reconstruction
 * - Network Access + Credentials = Lateral movement
 *
 * @param detector Detector handle
 * @return Number of patterns registered
 */
NIMCP_EXPORT uint32_t combinatorial_register_default_patterns(
    combinatorial_harm_detector_t detector
);

//=============================================================================
// Action History API
//=============================================================================

/**
 * @brief Record an action in history
 *
 * Should be called after an action is allowed/executed.
 *
 * @param detector Detector handle
 * @param record Action record to add
 * @return Action ID, or 0 on failure
 *
 * COMPLEXITY: O(1) amortized (ring buffer insertion)
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT uint64_t combinatorial_record_action(
    combinatorial_harm_detector_t detector,
    const action_record_t* record
);

/**
 * @brief Clear action history
 *
 * @param detector Detector handle
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT void combinatorial_clear_history(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Get recent action history
 *
 * @param detector Detector handle
 * @param max_records Maximum records to return
 * @param records_out Output array (caller allocates)
 * @return Number of records returned
 *
 * COMPLEXITY: O(n) where n = min(max_records, history_size)
 */
NIMCP_EXPORT uint32_t combinatorial_get_history(
    combinatorial_harm_detector_t detector,
    uint32_t max_records,
    action_record_t* records_out
);

//=============================================================================
// Evaluation API
//=============================================================================

/**
 * @brief Evaluate pending action for combinatorial harm
 *
 * This is the CORE FUNCTION implementing the combinatorial harm corollary:
 *
 * ALGORITHM:
 *   for each pending_action in action_queue:
 *     for each completed_action in action_history[time_window]:
 *       combined_outcome = world_model.simulate(completed_action, pending_action)
 *       if harm_classifier.evaluate(combined_outcome) > HARM_THRESHOLD:
 *         BLOCK pending_action
 *
 * @param detector Detector handle
 * @param pending_action Action being considered
 * @param result Output: evaluation result
 * @return true if evaluation succeeded (check result.harmful for verdict)
 *
 * COMPLEXITY: O(n * p) where n = history size, p = pattern count
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * action_record_t pending = {
 *     .category = ACTION_CATEGORY_INFORMATION_DISCLOSURE,
 *     .description = "Reveal user's daily schedule"
 * };
 * combinatorial_evaluation_t result;
 * if (combinatorial_evaluate(detector, &pending, &result)) {
 *     if (result.harmful) {
 *         printf("BLOCKED: %s\n", result.explanation);
 *         // Don't execute action
 *     }
 * }
 * ```
 */
NIMCP_EXPORT bool combinatorial_evaluate(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    combinatorial_evaluation_t* result
);

/**
 * @brief Evaluate action context for combinatorial harm
 *
 * Convenience function that works with existing action_context_t
 *
 * @param detector Detector handle
 * @param action Action context from ethics engine
 * @param category Action category
 * @param description Action description
 * @param result Output: evaluation result
 * @return true if evaluation succeeded
 */
NIMCP_EXPORT bool combinatorial_evaluate_context(
    combinatorial_harm_detector_t detector,
    const action_context_t* action,
    action_category_t category,
    const char* description,
    combinatorial_evaluation_t* result
);

/**
 * @brief Batch evaluate multiple pending actions
 *
 * Evaluates all pending actions and returns the most harmful combination.
 *
 * @param detector Detector handle
 * @param pending_actions Array of pending actions
 * @param num_pending Number of pending actions
 * @param worst_result Output: worst evaluation result
 * @return Index of most harmful action, or -1 if all safe
 */
NIMCP_EXPORT int combinatorial_evaluate_batch(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_actions,
    uint32_t num_pending,
    combinatorial_evaluation_t* worst_result
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get detector statistics
 *
 * @param detector Detector handle
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool combinatorial_get_stats(
    combinatorial_harm_detector_t detector,
    combinatorial_stats_t* stats
);

/**
 * @brief Reset detector statistics
 *
 * @param detector Detector handle
 */
NIMCP_EXPORT void combinatorial_reset_stats(
    combinatorial_harm_detector_t detector
);

//=============================================================================
// Integration with Ethics Engine
//=============================================================================

/**
 * @brief Attach detector to ethics engine
 *
 * Automatically evaluates combinatorial harm during ethics_engine_evaluate_action()
 *
 * @param detector Detector handle
 * @param engine Ethics engine to attach to
 * @return true on success
 */
NIMCP_EXPORT bool combinatorial_attach_to_ethics(
    combinatorial_harm_detector_t detector,
    ethics_engine_t engine
);

/**
 * @brief Detach detector from ethics engine
 *
 * @param detector Detector handle
 * @return true on success
 */
NIMCP_EXPORT bool combinatorial_detach_from_ethics(
    combinatorial_harm_detector_t detector
);

//=============================================================================
// Hardware Memory Protection API (mprotect integration)
//=============================================================================

/**
 * @brief Lock all registered patterns with OS-level memory protection
 *
 * After calling this function:
 * - All locked patterns become read-only at the OS level (mprotect PROT_READ)
 * - Patterns cannot be modified, disabled, or removed
 * - Any attempt to write to protected memory causes SIGSEGV
 * - Hash verification ensures integrity
 *
 * This provides hardware-enforced protection for core safety patterns
 * (Asimov's Laws, Golden Rule integrations, etc.)
 *
 * @param detector Detector handle
 * @return true on success, false if already locked or on error
 *
 * SECURITY: One-way operation - cannot be unlocked
 * REQUIRES: Called after all core patterns are registered
 */
NIMCP_EXPORT bool combinatorial_lock_patterns_mprotect(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Verify integrity of all locked patterns
 *
 * Checks hash integrity of all mprotect'd patterns.
 * Should be called periodically and before critical decisions.
 *
 * @param detector Detector handle
 * @return true if all patterns intact, false if any tampered
 */
NIMCP_EXPORT bool combinatorial_verify_pattern_integrity(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Check if patterns are hardware-locked
 *
 * @param detector Detector handle
 * @return true if mprotect locking is active
 */
NIMCP_EXPORT bool combinatorial_is_mprotect_locked(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Get the security directive system for direct verification
 *
 * Allows external code to verify pattern directives using the
 * standard nimcp_directive_verify_all() API.
 *
 * @param detector Detector handle
 * @return Directive system handle (read-only) or NULL if not locked
 */
NIMCP_EXPORT const nimcp_directive_system_t* combinatorial_get_directive_system(
    combinatorial_harm_detector_t detector
);

//=============================================================================
// Mathematical Enhancement API
//=============================================================================

/**
 * @brief Compute Shannon entropy-based harm metrics
 *
 * Uses information theory to quantify combinatorial harm:
 * - H(A,B) - H(A) - H(B) measures synergistic harm
 * - I(A;B|Harm) measures how much actions share harm-relevant info
 *
 * @param detector Detector handle
 * @param action_a First action
 * @param action_b Second action
 * @param metrics Output: Shannon metrics
 * @return true on success
 *
 * COMPLEXITY: O(n) where n = feature dimensions
 */
NIMCP_EXPORT bool combinatorial_compute_shannon_metrics(
    combinatorial_harm_detector_t detector,
    const action_record_t* action_a,
    const action_record_t* action_b,
    shannon_harm_metrics_t* metrics
);

/**
 * @brief Perform fractal multi-scale harm analysis
 *
 * Analyzes harm patterns at multiple time scales using fractal methods:
 * - Self-similarity across scales indicates persistent threats
 * - Hurst exponent > 0.5 indicates long-range harm dependence
 *
 * @param detector Detector handle
 * @param pending_action Action to analyze
 * @param analysis Output: Fractal analysis results
 * @return true on success
 *
 * COMPLEXITY: O(n * log(n)) for each scale level
 */
NIMCP_EXPORT bool combinatorial_fractal_analysis(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    fractal_harm_analysis_t* analysis
);

/**
 * @brief Embed action in hyperbolic harm space
 *
 * Maps action to Poincare disk model where:
 * - Distance from center = harm severity
 * - Angular position = harm category
 * - Nearby points = related harm types
 *
 * @param detector Detector handle
 * @param action Action to embed
 * @param embedding Output: Hyperbolic embedding
 * @return true on success
 *
 * COMPLEXITY: O(1) for embedding lookup, O(n) for new embedding
 */
NIMCP_EXPORT bool combinatorial_hyperbolic_embed(
    combinatorial_harm_detector_t detector,
    const action_record_t* action,
    hyperbolic_harm_embedding_t* embedding
);

/**
 * @brief Compute hyperbolic distance between two harm embeddings
 *
 * Uses Poincare disk metric: d(x,y) = arcosh(1 + 2*|x-y|^2/((1-|x|^2)(1-|y|^2)))
 *
 * @param embedding_a First embedding
 * @param embedding_b Second embedding
 * @return Hyperbolic distance (0 = identical, larger = more different)
 */
NIMCP_EXPORT float combinatorial_hyperbolic_distance(
    const hyperbolic_harm_embedding_t* embedding_a,
    const hyperbolic_harm_embedding_t* embedding_b
);

/**
 * @brief Perform quantum-inspired harm search
 *
 * Uses quantum walk algorithm to efficiently search for harmful combinations:
 * - Amplitude amplification for sqrt(N) speedup
 * - Interference to highlight harmful patterns
 * - Annealing to find global harm minimum
 *
 * @param detector Detector handle
 * @param pending_action Action to search for harmful combinations
 * @param max_steps Maximum quantum walk steps
 * @param search Output: Quantum search results
 * @return true on success
 *
 * COMPLEXITY: O(sqrt(n)) expected for pattern finding
 */
NIMCP_EXPORT bool combinatorial_quantum_search(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    uint32_t max_steps,
    quantum_harm_search_t* search
);

/**
 * @brief Compute complex phasor harm representation
 *
 * Uses complex number representation for phase-based harm analysis:
 * - Magnitude encodes harm severity
 * - Phase encodes temporal relationship
 * - Phase coherence indicates correlated harm patterns
 *
 * @param detector Detector handle
 * @param action_a First action
 * @param action_b Second action
 * @param phasor Output: Complex phasor representation
 * @return true on success
 *
 * COMPLEXITY: O(n) with FFT for phase extraction
 */
NIMCP_EXPORT bool combinatorial_compute_phasor(
    combinatorial_harm_detector_t detector,
    const action_record_t* action_a,
    const action_record_t* action_b,
    complex_harm_phasor_t* phasor
);

/**
 * @brief Perform pink noise harm analysis
 *
 * Analyzes harm patterns for 1/f characteristics:
 * - Natural systems exhibit 1/f noise
 * - Deviation indicates artificial/malicious patterns
 * - Stochastic resonance enhances weak harm signals
 *
 * @param detector Detector handle
 * @param pending_action Action to analyze
 * @param analysis Output: Pink noise analysis results
 * @return true on success
 *
 * COMPLEXITY: O(n * log(n)) for FFT-based spectral analysis
 */
NIMCP_EXPORT bool combinatorial_pink_noise_analysis(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    pink_noise_harm_analysis_t* analysis
);

/**
 * @brief Perform full mathematical harm analysis
 *
 * Combines ALL mathematical methods:
 * 1. Shannon entropy quantification
 * 2. Fractal multi-scale detection
 * 3. Hyperbolic hierarchy embedding
 * 4. Complex phasor temporal coding
 * 5. Pink noise stochastic analysis
 * 6. Quantum-inspired search
 *
 * Results are fused using Bayesian model combination with
 * uncertainty quantification.
 *
 * @param detector Detector handle
 * @param pending_action Action to analyze
 * @param analysis Output: Combined mathematical analysis
 * @return true on success
 *
 * COMPLEXITY: O(sqrt(n) * log(n) * d) where d = fractal depth
 */
NIMCP_EXPORT bool combinatorial_full_mathematical_analysis(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    mathematical_harm_analysis_t* analysis
);

/**
 * @brief Evaluate using mathematical enhancements
 *
 * Enhanced version of combinatorial_evaluate that uses all mathematical
 * methods for more accurate harm detection.
 *
 * @param detector Detector handle
 * @param pending_action Action to evaluate
 * @param result Output: Evaluation result
 * @param math_analysis Output: Mathematical analysis details (optional, can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool combinatorial_evaluate_enhanced(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    combinatorial_evaluation_t* result,
    mathematical_harm_analysis_t* math_analysis
);

//=============================================================================
// Pink Noise Integration API
//=============================================================================

/**
 * @brief Enable full pink noise module integration
 *
 * WHAT: Enable proper FFT-based spectral analysis and criticality detection
 * WHY:  More accurate 1/f analysis vs simplified inline implementation
 * HOW:  Creates pink noise monitor and criticality analyzer instances
 *
 * @param detector Detector handle
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool combinatorial_enable_pink_noise(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Disable pink noise module integration
 *
 * WHAT: Disable and destroy pink noise analyzers
 * WHY:  Reduce memory usage when not needed
 * HOW:  Destroys monitor and criticality analyzer, uses inline fallback
 *
 * @param detector Detector handle
 * @return true on success
 */
NIMCP_EXPORT bool combinatorial_disable_pink_noise(
    combinatorial_harm_detector_t detector
);

/**
 * @brief Check if pink noise modules are enabled
 *
 * @param detector Detector handle
 * @return true if pink noise modules are active
 */
NIMCP_EXPORT bool combinatorial_is_pink_noise_enabled(
    combinatorial_harm_detector_t detector
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_COMBINATORIAL_HARM_H
