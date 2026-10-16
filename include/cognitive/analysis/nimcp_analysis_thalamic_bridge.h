/**
 * @file nimcp_analysis_thalamic_bridge.h
 * @brief Bridge between Analysis system and thalamic router
 *
 * WHAT: Routes analytical processing through attention-gated thalamic pathways
 * WHY: Deep analysis requires conscious attention via thalamic gating
 * HOW: Packages analysis signals, routes via mediodorsal nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - Analysis involves dorsolateral PFC-thalamic circuits
 * - Mediodorsal nucleus relays analytical decisions
 * - Pulvinar coordinates attention during analysis
 * - Thalamic reticular nucleus gates analysis initiation
 *
 * SIGNAL ROUTING:
 * - Analysis requests -> MD -> DLPFC
 * - Decomposition results -> Pulvinar -> Parietal integration
 * - Depth assessments -> MD -> Prefrontal evaluation
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_ANALYSIS_THALAMIC_BRIDGE_H
#define NIMCP_ANALYSIS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Signal types for analysis thalamic routing */
#define ANALYSIS_SIGNAL_DECOMPOSITION  0x2901  /**< Problem decomposition */
#define ANALYSIS_SIGNAL_DEPTH_REQUEST  0x2902  /**< Deep analysis request */
#define ANALYSIS_SIGNAL_PATTERN_FOUND  0x2903  /**< Pattern identified */
#define ANALYSIS_SIGNAL_COMPLETION     0x2904  /**< Analysis complete */

/**
 * @brief Analysis thalamic signal
 */
typedef struct {
    uint32_t signal_type;           /**< Signal type from defines above */
    float analysis_urgency;         /**< Urgency/priority [0-1] */
    float depth_required;           /**< Required analysis depth [0-1] */
    float complexity;               /**< Problem complexity [0-1] */
    void* content;                  /**< Signal content */
    uint32_t content_size;          /**< Content size */
    uint64_t timestamp_us;          /**< Timestamp */
} analysis_thalamic_signal_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_attention_gating;   /**< Gate signals by attention */
    bool enable_complexity_routing; /**< Route complex analyses specially */
    float min_urgency_threshold;    /**< Minimum urgency for routing */
    float complexity_boost;         /**< Boost for complex analyses */
} analysis_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t decompositions_routed; /**< Decompositions processed */
    uint64_t depth_requests;        /**< Deep analysis requests */
    uint64_t patterns_found;        /**< Patterns identified */
    uint64_t completions;           /**< Analyses completed */
    uint64_t signals_gated;         /**< Signals blocked by attention */
    float avg_analysis_urgency;     /**< Average urgency level */
} analysis_thalamic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct analysis_thalamic_bridge analysis_thalamic_bridge_t;

/* Configuration API */
analysis_thalamic_config_t analysis_thalamic_default_config(void);

/* Lifecycle API */
analysis_thalamic_bridge_t* analysis_thalamic_bridge_create(
    void* analysis,
    thalamic_router_t* router,
    const analysis_thalamic_config_t* config
);
void analysis_thalamic_bridge_destroy(analysis_thalamic_bridge_t* bridge);
int analysis_thalamic_bridge_reset(analysis_thalamic_bridge_t* bridge);

/* Signal Routing API */
int analysis_thalamic_route_signal(
    analysis_thalamic_bridge_t* bridge,
    const analysis_thalamic_signal_t* signal
);
int analysis_thalamic_route_decomposition(
    analysis_thalamic_bridge_t* bridge,
    const void* problem,
    uint32_t problem_size,
    float complexity
);
int analysis_thalamic_request_depth(
    analysis_thalamic_bridge_t* bridge,
    float depth_required,
    float urgency
);

/* Attention API */
int analysis_thalamic_set_attention(analysis_thalamic_bridge_t* bridge, float attention);
int analysis_thalamic_get_attention(analysis_thalamic_bridge_t* bridge, float* attention);

/* Statistics API */
int analysis_thalamic_bridge_get_stats(
    const analysis_thalamic_bridge_t* bridge,
    analysis_thalamic_stats_t* stats
);
void analysis_thalamic_bridge_reset_stats(analysis_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANALYSIS_THALAMIC_BRIDGE_H */
