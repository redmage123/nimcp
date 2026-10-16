/**
 * @file nimcp_connectivity_health.h
 * @brief Brain Connectivity Health Assessment - Phase 1.5.4
 *
 * WHAT: Introspection-based connectivity health monitoring with topology analysis
 * WHY:  Enable self-awareness of brain's organizational quality and information flow
 * HOW:  Community detection + hub analysis + Shannon metrics + graph topology
 *
 * FEATURES:
 * - Community structure quality (modularity Q, balance)
 * - Hub neuron identification and health
 * - Graph topology metrics (clustering, path length, small-world)
 * - Shannon information flow efficiency
 * - Integration health between layers
 * - Overall connectivity health score
 *
 * BIOLOGICAL INSPIRATION:
 * - Modular organization in cortex (Bullmore & Sporns, 2012)
 * - Hub neurons in prefrontal-parietal network (Power et al., 2013)
 * - Small-world topology in brain networks (Watts & Strogatz, 1998)
 * - Information integration in conscious processing (Tononi, 2004)
 *
 * PHASE: 1.5.4 - Introspection + Community Detection Health Monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#ifndef NIMCP_CONNECTIVITY_HEALTH_H
#define NIMCP_CONNECTIVITY_HEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations to avoid circular dependency
typedef struct brain_struct* brain_t;
typedef struct introspection_context_struct* introspection_context_t;

// Forward declare brain_region_t (defined in nimcp_brain_regions.h)
// Use int in the API to avoid circular dependency
typedef int brain_region_enum_t;

//=============================================================================
// Constants
//=============================================================================

/** Default minimum modularity for healthy network (Bullmore & Sporns, 2012) */
#define CONNECTIVITY_MIN_MODULARITY 0.3f

/** Default minimum clustering coefficient for healthy network */
#define CONNECTIVITY_MIN_CLUSTERING 0.3f

/** Default maximum path length for healthy network */
#define CONNECTIVITY_MAX_PATH_LENGTH 6.0f

/** Default small-world threshold (sigma > 1.0 indicates small-world) */
#define CONNECTIVITY_SMALL_WORLD_THRESHOLD 1.0f

/** Default minimum information flow efficiency */
#define CONNECTIVITY_MIN_FLOW_EFFICIENCY 0.7f

/** Default hub threshold (standard deviations above mean) */
#define CONNECTIVITY_HUB_THRESHOLD 2.0f

/** Maximum number of hub neurons to track */
#define CONNECTIVITY_MAX_HUBS 256

/** Assessment frequency recommendation (milliseconds) */
#define CONNECTIVITY_DEFAULT_ASSESSMENT_INTERVAL_MS 10000

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for connectivity health assessment
 *
 * WHAT: Thresholds and parameters for health evaluation
 * WHY:  Allow tuning for different network sizes and use cases
 */
typedef struct {
    /* Community structure thresholds */
    float min_modularity;                /**< Minimum acceptable Q (default: 0.3) */
    float max_community_imbalance;       /**< Maximum size imbalance ratio (default: 10.0) */

    /* Hub detection parameters */
    float hub_threshold_stddev;          /**< StdDevs above mean for hub (default: 2.0) */
    bool require_executive_hubs;         /**< Require hubs in executive region */
    bool require_workspace_hubs;         /**< Require hubs in workspace region */

    /* Topology thresholds */
    float min_clustering_coefficient;    /**< Minimum clustering (default: 0.3) */
    float max_path_length;               /**< Maximum avg path length (default: 6.0) */
    float small_world_threshold;         /**< Minimum sigma for small-world (default: 1.0) */

    /* Information flow thresholds */
    float min_flow_efficiency;           /**< Minimum information efficiency (default: 0.7) */
    float min_layer_connectivity;        /**< Minimum layer connection strength (default: 0.5) */

    /* Weight factors for overall score (must sum to 1.0) */
    float weight_modularity;             /**< Weight for modularity score (default: 0.25) */
    float weight_hubs;                   /**< Weight for hub score (default: 0.20) */
    float weight_topology;               /**< Weight for topology score (default: 0.25) */
    float weight_flow;                   /**< Weight for information flow (default: 0.30) */

    /* Assessment control */
    uint32_t assessment_interval_ms;     /**< Recommended interval between assessments */
    bool enable_detailed_hub_analysis;   /**< Compute full hub centrality metrics */
    bool enable_community_balance;       /**< Compute community size entropy */
} connectivity_health_config_t;

//=============================================================================
// Result Structures
//=============================================================================

/**
 * @brief Community structure health metrics
 *
 * WHAT: Quality of modular organization in network
 * WHY:  Healthy brains have Q ~ 0.3-0.5 (modular but integrated)
 */
typedef struct {
    float modularity_q;                  /**< Newman's Q score [-0.5, 1.0] */
    uint32_t num_communities;            /**< Number of detected communities */
    float community_balance;             /**< Size distribution entropy [0, 1] */
    float largest_community_ratio;       /**< Largest / total neurons [0, 1] */
    bool is_healthy;                     /**< Modularity > threshold */
} community_health_t;

/**
 * @brief Hub neuron health metrics
 *
 * WHAT: Analysis of high-centrality connector neurons
 * WHY:  Hubs enable efficient information integration
 */
typedef struct {
    uint32_t num_hubs;                   /**< Total hub neurons detected */
    uint32_t hub_neuron_ids[CONNECTIVITY_MAX_HUBS]; /**< Hub neuron IDs */
    float hub_centrality[CONNECTIVITY_MAX_HUBS];    /**< Centrality scores */

    /* Regional hub distribution */
    bool executive_has_hubs;             /**< Executive region has hubs */
    bool workspace_has_hubs;             /**< Workspace region has hubs */
    bool salience_has_hubs;              /**< Salience region has hubs */
    uint32_t hubs_per_region[16];        /**< Hub count per brain region */

    float avg_hub_centrality;            /**< Mean hub centrality */
    float hub_distribution_entropy;      /**< Hub spread across regions */
    bool is_healthy;                     /**< Required regions have hubs */
} hub_health_t;

/**
 * @brief Graph topology health metrics
 *
 * WHAT: Network structure properties
 * WHY:  Small-world topology enables efficient processing
 */
typedef struct {
    float clustering_coefficient;        /**< Local connectivity [0, 1] */
    float avg_path_length;               /**< Global integration [1, inf) */
    float small_world_sigma;             /**< (C/Crand)/(L/Lrand), >1 is small-world */
    uint32_t network_diameter;           /**< Longest shortest path */
    float assortativity;                 /**< Degree correlation [-1, 1] */

    /* Derived scores */
    float clustering_score;              /**< Normalized to [0, 1] */
    float path_length_score;             /**< Inverted, normalized to [0, 1] */
    float topology_score;                /**< Combined topology health */
    bool is_small_world;                 /**< sigma > threshold */
    bool is_healthy;                     /**< All topology checks pass */
} topology_health_t;

/**
 * @brief Information flow health metrics
 *
 * WHAT: Shannon-based information transfer efficiency
 * WHY:  Efficient information flow enables conscious processing
 */
typedef struct {
    float transfer_efficiency;           /**< I_out / I_in [0, 1] */
    float layer_connectivity;            /**< Middleware-cognitive strength [0, 1] */
    float bottleneck_score;              /**< 1 = no bottlenecks, 0 = severe */
    uint32_t num_bottlenecks;            /**< Count of bottleneck synapses */

    float total_capacity_bits_per_sec;   /**< Network channel capacity */
    float actual_throughput_bits_per_sec;/**< Current information rate */
    float capacity_utilization;          /**< Throughput / capacity [0, 1] */

    bool is_healthy;                     /**< Efficiency > threshold */
} information_flow_health_t;

/**
 * @brief Complete brain connectivity health assessment
 *
 * WHAT: Comprehensive health report combining all metrics
 * WHY:  Single source of truth for network quality
 */
typedef struct {
    /* Component health assessments */
    community_health_t community;        /**< Community structure health */
    hub_health_t hubs;                   /**< Hub neuron health */
    topology_health_t topology;          /**< Graph topology health */
    information_flow_health_t flow;      /**< Information flow health */

    /* Overall health */
    float overall_health;                /**< Combined score [0, 1] */
    bool is_healthy;                     /**< All components healthy */

    /* Diagnostic information */
    uint32_t num_warnings;               /**< Non-critical issues */
    uint32_t num_critical;               /**< Critical issues */
    char primary_issue[256];             /**< Most significant problem */

    /* Timing */
    uint64_t assessment_timestamp_ms;    /**< When assessment was performed */
    uint32_t assessment_duration_ms;     /**< How long assessment took */

    /* Network stats */
    uint32_t total_neurons;              /**< Total neurons analyzed */
    uint32_t total_synapses;             /**< Total synapses analyzed */
} brain_connectivity_health_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default connectivity health configuration
 *
 * WHAT: Returns standard configuration with biological defaults
 * WHY:  Provide sensible starting point for health assessment
 * HOW:  Sets thresholds based on neuroscience literature
 *
 * @return Default configuration structure
 */
connectivity_health_config_t connectivity_health_default_config(void);

//=============================================================================
// Assessment Functions
//=============================================================================

/**
 * @brief Assess complete brain connectivity health
 *
 * WHAT: Full topology analysis with community detection + hub identification
 * WHY:  Self-awareness of brain's organizational quality
 * HOW:  Louvain community detection + degree centrality + graph metrics
 *
 * COMPLEXITY: O(N log N) for community detection, O(N^2) for path length
 * LATENCY: ~10-50ms for N=1000 neurons (periodic, not real-time)
 *
 * @param introspection Introspection context with brain reference
 * @param config Assessment configuration (NULL for defaults)
 * @return Complete connectivity health assessment
 */
brain_connectivity_health_t introspection_assess_connectivity_health(
    introspection_context_t introspection,
    const connectivity_health_config_t* config
);

/**
 * @brief Quick connectivity health check (lightweight)
 *
 * WHAT: Fast health assessment using cached metrics
 * WHY:  Enable frequent monitoring without full recomputation
 * HOW:  Uses last community detection results + current Shannon metrics
 *
 * COMPLEXITY: O(1) if cached, O(N log N) if cache stale
 * LATENCY: ~1-5ms typical
 *
 * @param introspection Introspection context
 * @param[out] is_healthy Quick health status
 * @return Overall health score [0, 1]
 */
float introspection_quick_connectivity_check(
    introspection_context_t introspection,
    bool* is_healthy
);

/**
 * @brief Assess community structure health only
 *
 * WHAT: Evaluate modular organization quality
 * WHY:  Community structure is fundamental to brain organization
 * HOW:  Runs Louvain, computes modularity and balance
 *
 * @param introspection Introspection context
 * @param config Configuration (NULL for defaults)
 * @return Community health metrics
 */
community_health_t introspection_assess_community_health(
    introspection_context_t introspection,
    const connectivity_health_config_t* config
);

/**
 * @brief Assess hub neuron health only
 *
 * WHAT: Evaluate hub neuron presence and distribution
 * WHY:  Hubs are critical for information integration
 * HOW:  Centrality analysis + regional distribution
 *
 * @param introspection Introspection context
 * @param config Configuration (NULL for defaults)
 * @return Hub health metrics
 */
hub_health_t introspection_assess_hub_health(
    introspection_context_t introspection,
    const connectivity_health_config_t* config
);

/**
 * @brief Assess graph topology health only
 *
 * WHAT: Evaluate network structure properties
 * WHY:  Small-world topology enables efficient processing
 * HOW:  Compute clustering, path length, small-world coefficient
 *
 * @param introspection Introspection context
 * @param config Configuration (NULL for defaults)
 * @return Topology health metrics
 */
topology_health_t introspection_assess_topology_health(
    introspection_context_t introspection,
    const connectivity_health_config_t* config
);

/**
 * @brief Assess information flow health only
 *
 * WHAT: Evaluate Shannon information transfer efficiency
 * WHY:  Efficient flow is required for conscious processing
 * HOW:  Query Shannon metrics from brain
 *
 * @param introspection Introspection context
 * @param config Configuration (NULL for defaults)
 * @return Information flow health metrics
 */
information_flow_health_t introspection_assess_flow_health(
    introspection_context_t introspection,
    const connectivity_health_config_t* config
);

//=============================================================================
// Brain Integration Functions
//=============================================================================

/**
 * @brief Enable periodic connectivity health monitoring in brain
 *
 * WHAT: Register periodic health assessment with brain event loop
 * WHY:  Continuous monitoring without manual polling
 * HOW:  Schedules assessment at configured interval
 *
 * @param brain Brain instance
 * @param config Health assessment configuration
 * @param callback Optional callback on health change (can be NULL)
 * @param callback_context Context for callback
 * @return true on success
 */
bool brain_enable_connectivity_monitoring(
    brain_t brain,
    const connectivity_health_config_t* config,
    void (*callback)(const brain_connectivity_health_t*, void*),
    void* callback_context
);

/**
 * @brief Disable connectivity health monitoring
 *
 * @param brain Brain instance
 */
void brain_disable_connectivity_monitoring(brain_t brain);

/**
 * @brief Check if connectivity monitoring is enabled
 *
 * @param brain Brain instance
 * @return true if monitoring is active
 */
bool brain_is_connectivity_monitoring_enabled(brain_t brain);

/**
 * @brief Get last connectivity health assessment from brain
 *
 * WHAT: Retrieve cached health assessment
 * WHY:  Avoid recomputation when recent data is sufficient
 * HOW:  Returns copy of last assessment from brain structure
 *
 * @param brain Brain instance
 * @param[out] health Output health structure
 * @return true if assessment available, false if never assessed
 */
bool brain_get_connectivity_health(
    brain_t brain,
    brain_connectivity_health_t* health
);

/**
 * @brief Force immediate connectivity health assessment
 *
 * WHAT: Run full assessment regardless of interval
 * WHY:  Allow on-demand health checks
 * HOW:  Bypasses interval check, updates cached result
 *
 * @param brain Brain instance
 * @return Fresh connectivity health assessment
 */
brain_connectivity_health_t brain_assess_connectivity_now(brain_t brain);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate community size balance entropy
 *
 * WHAT: Measure evenness of community size distribution
 * WHY:  Extremely imbalanced communities indicate problems
 * HOW:  Shannon entropy of normalized community sizes
 *
 * @param community_sizes Array of community sizes
 * @param num_communities Number of communities
 * @return Balance entropy [0, 1] where 1 = perfectly balanced
 */
float calculate_community_balance(
    const uint32_t* community_sizes,
    uint32_t num_communities
);

/**
 * @brief Check if neuron is in specified brain region
 *
 * @param brain Brain instance
 * @param neuron_id Neuron to check
 * @param region Target region
 * @return true if neuron is in region
 */
bool is_neuron_in_region(
    brain_t brain,
    uint32_t neuron_id,
    int region  /* brain_region_t cast to int to avoid circular dependency */
);

/**
 * @brief Get human-readable health status string
 *
 * @param health Health assessment
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer
 */
const char* connectivity_health_to_string(
    const brain_connectivity_health_t* health,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Free any dynamically allocated health assessment data
 *
 * @param health Health assessment to clean up
 */
void connectivity_health_free(brain_connectivity_health_t* health);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONNECTIVITY_HEALTH_H */
