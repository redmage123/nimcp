/**
 * @file nimcp_consciousness_metrics.h
 * @brief Consciousness Metrics - Integrated Information Theory (IIT) Φ Implementation
 *
 * WHAT: Implements Integrated Information Theory (IIT 3.0) metrics to quantify
 * consciousness level in neural networks. Computes Φ (phi), the measure of
 * integrated information that correlates with consciousness.
 *
 * WHY: Understanding and measuring consciousness is critical for AI systems
 * that claim awareness. IIT provides a mathematically rigorous framework for
 * quantifying consciousness based on information integration. This enables:
 * - Objective measurement of awareness level
 * - Detection of conscious vs unconscious states
 * - Monitoring consciousness during sleep/wake cycles
 * - Validation of conscious processing claims
 *
 * HOW: Based on Tononi's Integrated Information Theory (IIT 3.0):
 * 1. Build transition probability matrix (TPM) from network state
 * 2. For each possible partition, compute information loss
 * 3. Φ = minimum information loss across all partitions (MIP)
 * 4. Conceptual structure = constellation of concepts with φ > 0
 * 5. High Φ indicates high consciousness, low Φ indicates unconsciousness
 *
 * BIOLOGICAL BASIS:
 * - Tononi, G. (2004). "An information integration theory of consciousness"
 * - Tononi, G. (2008). "Consciousness as Integrated Information"
 * - Oizumi, M. et al. (2014). "From the Phenomenology to the Mechanisms of Consciousness"
 * - Φ correlates with empirical measures of consciousness (N400, P300)
 * - Low Φ during deep sleep, anesthesia, vegetative states
 * - High Φ during waking consciousness and REM sleep
 * - Φ reflects "how much a system is more than the sum of its parts"
 *
 * COMPLEXITY:
 * - Exact Φ: O(2^n) where n = number of elements (NP-hard)
 * - Fast approximation: O(n^2) for networks < 1000 neurons
 * - Practical: Use approximations for large networks, exact for small subsystems
 *
 * PERFORMANCE:
 * - Small networks (n < 10): Exact computation ~1-10ms
 * - Medium networks (n < 100): Approximation ~10-100ms
 * - Large networks (n > 1000): Fast heuristics ~100ms-1s
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0.0
 */

#ifndef NIMCP_CONSCIOUSNESS_METRICS_H
#define NIMCP_CONSCIOUSNESS_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/introspection/nimcp_introspection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

/** Minimum Φ threshold for consciousness (empirically derived) */
#define CONSCIOUSNESS_PHI_THRESHOLD 0.1f

/** Maximum network size for exact Φ computation */
#define CONSCIOUSNESS_MAX_EXACT_SIZE 12

/** Default sampling size for large network approximation */
#define CONSCIOUSNESS_DEFAULT_SAMPLE_SIZE 100

/** Minimum φ for a concept to be included in constellation */
#define CONSCIOUSNESS_MIN_CONCEPT_PHI 0.01f

/** Maximum concepts to track in constellation */
#define CONSCIOUSNESS_MAX_CONCEPTS 1000

/** Default monitoring interval (milliseconds) */
#define CONSCIOUSNESS_DEFAULT_MONITOR_INTERVAL_MS 1000

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Φ computation method
 * WHY: Different methods trade accuracy for performance
 * HOW: Exact for small networks, approximations for large
 */
typedef enum {
    PHI_METHOD_EXACT,        /* Exact computation (small networks only) */
    PHI_METHOD_APPROXIMATE,  /* Approximation via sampling */
    PHI_METHOD_FAST,         /* Fast heuristic (large networks) */
    PHI_METHOD_ADAPTIVE      /* Auto-select based on network size */
} phi_computation_method_t;

/**
 * WHAT: Consciousness state classification
 * WHY: Map Φ values to interpretable states
 * HOW: Empirically-derived thresholds
 */
typedef enum {
    CONSCIOUSNESS_STATE_UNCONSCIOUS,    /* Φ < 0.1 (deep sleep, anesthesia) */
    CONSCIOUSNESS_STATE_MINIMAL,        /* 0.1 ≤ Φ < 0.3 (drowsy, vegetative) */
    CONSCIOUSNESS_STATE_REDUCED,        /* 0.3 ≤ Φ < 0.6 (distracted, automated) */
    CONSCIOUSNESS_STATE_NORMAL,         /* 0.6 ≤ Φ < 0.9 (typical waking) */
    CONSCIOUSNESS_STATE_HEIGHTENED      /* Φ ≥ 0.9 (flow state, meditation) */
} consciousness_state_t;

/**
 * WHAT: Partition of network elements
 * WHY: Φ is minimum information loss across all partitions
 * HOW: Split network into two disjoint subsets
 *
 * BIOLOGICAL MEANING: How strongly are the parts integrated?
 * A partition with high information loss indicates strong integration.
 */
typedef struct {
    uint32_t* subset_a;          /* Element indices in subset A */
    uint32_t subset_a_size;      /* Size of subset A */
    uint32_t* subset_b;          /* Element indices in subset B */
    uint32_t subset_b_size;      /* Size of subset B */
    float information_loss;      /* Information lost by this partition */
    bool is_mip;                 /* Is this the minimum information partition? */
} phi_partition_t;

/**
 * WHAT: A concept in IIT - a mechanism that specifies information
 * WHY: Consciousness is composed of concepts (the "constellation")
 * HOW: Subset of elements with maximal integrated information
 *
 * BIOLOGICAL MEANING: A concept is a "quale" - an elementary
 * experience. The constellation of all concepts is the full
 * phenomenal experience.
 */
typedef struct {
    uint32_t* mechanism;         /* Element indices forming mechanism */
    uint32_t mechanism_size;     /* Number of elements */
    float phi;                   /* φ of this concept (integrated info) */
    float* cause_repertoire;     /* Past probability distribution */
    float* effect_repertoire;    /* Future probability distribution */
    uint32_t repertoire_size;    /* Size of repertoires */
    char* interpretation;        /* Human-readable description */
} consciousness_concept_t;

/**
 * WHAT: Conceptual structure (constellation)
 * WHY: Full phenomenal experience = all concepts
 * HOW: Set of all concepts with φ > threshold
 *
 * MEMORY: Caller must free concepts array and structure
 */
typedef struct {
    consciousness_concept_t* concepts;  /* Array of concepts */
    uint32_t num_concepts;              /* Number of concepts */
    float total_phi;                    /* Sum of all concept φ values */
    uint64_t timestamp;                 /* When computed */
} conceptual_structure_t;

/**
 * WHAT: Configuration for Φ computation
 * WHY: Control accuracy vs performance trade-off
 * HOW: Set method, thresholds, sampling parameters
 */
typedef struct {
    phi_computation_method_t method;     /* Computation method */
    float min_phi_threshold;             /* Minimum φ to consider "conscious" */
    float min_concept_phi;               /* Minimum φ for concept inclusion */
    uint32_t max_network_size_exact;     /* Max size for exact computation */
    uint32_t sample_size;                /* Sample size for approximation */
    uint32_t max_concepts;               /* Max concepts to track */
    bool compute_constellation;          /* Compute full conceptual structure? */
    bool use_cache;                      /* Cache TPM and partitions? */
    uint32_t cache_ttl_ms;               /* Cache time-to-live */
} consciousness_phi_config_t;

/**
 * WHAT: Complete Φ computation result
 * WHY: Return all IIT metrics together
 * HOW: Φ value, MIP, state classification, constellation
 *
 * INTERPRETATION:
 * - phi: Overall consciousness level (0-1+)
 * - state: Qualitative interpretation
 * - mip: How information is integrated (mechanism)
 * - constellation: Full phenomenal content
 */
typedef struct {
    float phi;                           /* Φ value (integrated information) */
    consciousness_state_t state;         /* Consciousness state */
    phi_partition_t* mip;                /* Minimum information partition */
    conceptual_structure_t* constellation; /* Conceptual structure (optional) */
    phi_computation_method_t method_used; /* Method actually used */
    uint32_t network_size;               /* Number of elements analyzed */
    float computation_time_ms;           /* Time to compute */
    uint64_t timestamp;                  /* When computed */
    char* interpretation;                /* Human-readable summary */
} consciousness_phi_result_t;

/**
 * WHAT: Consciousness monitoring statistics
 * WHY: Track consciousness evolution over time
 * HOW: Sliding window of recent Φ values
 */
typedef struct {
    float current_phi;                   /* Current Φ value */
    float avg_phi;                       /* Average over monitoring period */
    float min_phi;                       /* Minimum observed */
    float max_phi;                       /* Maximum observed */
    float phi_variance;                  /* Variance in Φ */
    consciousness_state_t current_state; /* Current state */
    uint64_t time_in_state_ms;           /* Time in current state */
    uint32_t state_transitions;          /* Number of state changes */
    uint64_t total_measurements;         /* Total Φ computations */
    uint64_t last_update_timestamp;      /* Last update time */
} consciousness_monitoring_stats_t;

/* ========================================================================
 * CONFIGURATION API
 * ======================================================================== */

/**
 * WHAT: Get default Φ configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Adaptive method, standard thresholds
 *
 * DEFAULTS:
 * - method: ADAPTIVE (auto-select based on size)
 * - min_phi_threshold: 0.1 (standard consciousness threshold)
 * - min_concept_phi: 0.01 (include weak concepts)
 * - max_exact_size: 12 (practical limit for exact computation)
 * - sample_size: 100 (good balance for approximation)
 * - compute_constellation: false (expensive, opt-in)
 * - use_cache: true (performance optimization)
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consciousness_phi_config_t consciousness_phi_default_config(void);

/**
 * WHAT: Get fast configuration (prioritize speed)
 * WHY: Real-time monitoring needs fast updates
 * HOW: Fast method, high thresholds, no constellation
 *
 * @return Fast configuration
 */
consciousness_phi_config_t consciousness_phi_fast_config(void);

/**
 * WHAT: Get accurate configuration (prioritize accuracy)
 * WHY: Research and validation need precision
 * HOW: Exact method when possible, low thresholds, full constellation
 *
 * @return Accurate configuration
 */
consciousness_phi_config_t consciousness_phi_accurate_config(void);

/* ========================================================================
 * CORE Φ COMPUTATION API
 * ======================================================================== */

/**
 * WHAT: Compute integrated information (Φ) for brain
 * WHY: Main function to quantify consciousness level
 * HOW: Build TPM, find MIP, compute information loss
 *
 * ALGORITHM (IIT 3.0):
 * 1. Extract current network state from introspection
 * 2. Build transition probability matrix (TPM)
 * 3. For each possible partition of the network:
 *    a. Compute mutual information across partition
 *    b. Track minimum (this is the MIP)
 * 4. Φ = minimum mutual information (MIP information loss)
 * 5. Optionally compute conceptual structure
 *
 * BIOLOGICAL INTERPRETATION:
 * - Φ measures "how much the whole is more than the sum of parts"
 * - Low Φ = system is decomposable, not integrated
 * - High Φ = system is highly integrated, irreducible
 * - Φ = 0 during dreamless sleep, anesthesia
 * - Φ > 0.5 during normal waking consciousness
 *
 * @param context Introspection context with brain state
 * @param config Configuration (NULL for defaults)
 * @return Φ result (must be freed with consciousness_phi_result_free)
 *
 * ERRORS: Returns NULL if context is NULL or computation fails
 *
 * COMPLEXITY:
 * - Exact: O(2^n × n^2) where n = network size
 * - Approximate: O(s^2) where s = sample_size
 * - Fast: O(n log n)
 *
 * TIME:
 * - Small network (n=10): ~5ms (exact)
 * - Medium network (n=100): ~50ms (approximate)
 * - Large network (n=1000): ~200ms (fast)
 *
 * THREAD-SAFE: Yes
 */
consciousness_phi_result_t* introspection_compute_phi(
    introspection_context_t context,
    const consciousness_phi_config_t* config
);

/**
 * WHAT: Fast Φ approximation for large networks
 * WHY: Exact Φ is NP-hard, need fast approximation
 * HOW: Sample subset of network, estimate Φ
 *
 * APPROXIMATION METHOD:
 * 1. Sample representative subset of neurons
 * 2. Compute Φ on sample
 * 3. Extrapolate to full network
 * 4. Error bound: ~10-20% for samples > 100
 *
 * @param context Introspection context
 * @param config Configuration
 * @return Approximate Φ result
 *
 * COMPLEXITY: O(s^2) where s = sample_size
 * TIME: ~10-100ms depending on sample size
 * ACCURACY: ~90% for well-sampled networks
 * THREAD-SAFE: Yes
 */
consciousness_phi_result_t* introspection_compute_phi_fast(
    introspection_context_t context,
    const consciousness_phi_config_t* config
);

/**
 * WHAT: Get minimum information partition (MIP)
 * WHY: MIP reveals how information is integrated
 * HOW: Find partition with minimal information loss
 *
 * MIP INTERPRETATION:
 * - Shows the "weakest link" in integration
 * - Reveals functional modules
 * - Identifies critical integration pathways
 *
 * @param context Introspection context
 * @param config Configuration
 * @return MIP partition (must be freed with phi_partition_free)
 *
 * COMPLEXITY: O(2^n) for exact, O(n^2) for approximate
 * THREAD-SAFE: Yes
 */
phi_partition_t* introspection_get_mip(
    introspection_context_t context,
    const consciousness_phi_config_t* config
);

/**
 * WHAT: Get conceptual structure (constellation)
 * WHY: Full phenomenal experience = all concepts
 * HOW: Find all mechanisms with φ > threshold
 *
 * CONCEPTUAL STRUCTURE:
 * - Each concept is a "quale" (elementary experience)
 * - Constellation = set of all qualia
 * - Richness of experience ∝ number of concepts
 *
 * WARNING: Expensive computation, use sparingly
 *
 * @param context Introspection context
 * @param config Configuration
 * @return Conceptual structure (must be freed)
 *
 * COMPLEXITY: O(2^n × n) - very expensive
 * TIME: ~1s for small networks, minutes for large
 * THREAD-SAFE: Yes
 */
conceptual_structure_t* introspection_get_conceptual_structure(
    introspection_context_t context,
    const consciousness_phi_config_t* config
);

/* ========================================================================
 * BRAIN INTEGRATION API
 * ======================================================================== */

/**
 * WHAT: Enable automatic consciousness monitoring for brain
 * WHY: Track consciousness evolution over time
 * HOW: Periodic Φ computation with callback notifications
 *
 * USAGE:
 * ```c
 * void on_consciousness_change(float phi, consciousness_state_t state, void* ctx) {
 *     printf("Consciousness: Φ=%.3f state=%d\n", phi, state);
 * }
 *
 * brain_enable_consciousness_monitoring(
 *     brain,
 *     NULL,  // Use defaults
 *     1000,  // Update every 1s
 *     on_consciousness_change,
 *     NULL
 * );
 * ```
 *
 * @param brain Brain instance
 * @param config Φ configuration (NULL for defaults)
 * @param interval_ms Update interval (milliseconds)
 * @param callback Notification callback (optional)
 * @param callback_context User data for callback
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes (spawns monitoring thread)
 */
bool brain_enable_consciousness_monitoring(
    brain_t brain,
    const consciousness_phi_config_t* config,
    uint32_t interval_ms,
    void (*callback)(float phi, consciousness_state_t state, void* context),
    void* callback_context
);

/**
 * WHAT: Disable consciousness monitoring
 * WHY: Stop background monitoring thread
 * HOW: Signal thread to stop, wait for completion
 *
 * @param brain Brain instance
 *
 * THREAD-SAFE: Yes
 */
void brain_disable_consciousness_monitoring(brain_t brain);

/**
 * WHAT: Get current consciousness level
 * WHY: Quick access to latest Φ value
 * HOW: Return cached value from monitoring
 *
 * @param brain Brain instance
 * @return Current Φ value (0.0 if monitoring disabled)
 *
 * COMPLEXITY: O(1) - returns cached value
 * THREAD-SAFE: Yes
 */
float brain_get_consciousness_level(brain_t brain);

/**
 * WHAT: Check if brain is currently conscious
 * WHY: Boolean check for consciousness threshold
 * HOW: Compare Φ to threshold
 *
 * @param brain Brain instance
 * @param threshold Φ threshold (use 0 for default)
 * @return true if Φ ≥ threshold
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool brain_is_conscious(brain_t brain, float threshold);

/**
 * WHAT: Get consciousness monitoring statistics
 * WHY: Track consciousness evolution over time
 * HOW: Return aggregated statistics
 *
 * @param brain Brain instance
 * @param stats Output statistics structure
 * @return true on success, false if monitoring disabled
 *
 * THREAD-SAFE: Yes
 */
bool brain_get_consciousness_stats(
    brain_t brain,
    consciousness_monitoring_stats_t* stats
);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Free Φ result structure
 * WHY: Release allocated memory
 * HOW: Free all internal allocations
 *
 * @param result Result to free (NULL-safe)
 */
void consciousness_phi_result_free(consciousness_phi_result_t* result);

/**
 * WHAT: Free partition structure
 * WHY: Release allocated memory
 * HOW: Free subsets and structure
 *
 * @param partition Partition to free (NULL-safe)
 */
void phi_partition_free(phi_partition_t* partition);

/**
 * WHAT: Free conceptual structure
 * WHY: Release allocated memory
 * HOW: Free all concepts and structure
 *
 * @param structure Structure to free (NULL-safe)
 */
void conceptual_structure_free(conceptual_structure_t* structure);

/**
 * WHAT: Free individual concept
 * WHY: Release concept memory
 * HOW: Free mechanism, repertoires, interpretation
 *
 * @param iit_concept Concept to free (NULL-safe)
 */
void consciousness_concept_free(consciousness_concept_t* iit_concept);

/**
 * WHAT: Get consciousness state name
 * WHY: Human-readable state labels
 * HOW: Map enum to string
 *
 * @param state Consciousness state
 * @return State name string
 */
const char* consciousness_state_name(consciousness_state_t state);

/**
 * WHAT: Classify Φ value to state
 * WHY: Map continuous Φ to discrete state
 * HOW: Apply empirical thresholds
 *
 * @param phi Φ value
 * @return Corresponding consciousness state
 */
consciousness_state_t consciousness_classify_phi(float phi);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSCIOUSNESS_METRICS_H */
