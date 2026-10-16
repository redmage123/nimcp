/**
 * @file nimcp_intuition_thalamic_bridge.h
 * @brief Bridge between intuition system and thalamic router
 *
 * WHAT: Routes intuition signals through attention-gated thalamic pathways
 * WHY: Intuitive insights need to reach conscious awareness via thalamic gating
 * HOW: Packages hunches/insights as signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Thalamus acts as "gateway to consciousness" (Dehaene et al., 2006)
 * - Pulvinar nucleus coordinates attention during insight (Philiastides et al., 2011)
 * - Thalamic reticular nucleus (TRN) gates which insights reach awareness
 * - First-order relay: sensory insights to cortex
 * - Higher-order relay: cortico-cortical insight propagation
 * - Burst vs tonic modes affect insight salience
 *
 * SIGNAL ROUTING:
 * - High-confidence hunches → PRIORITY_HIGH bypass
 * - Insights with emotional valence → enhanced attention weight
 * - Analogical matches → broadcast to multiple cortical targets
 * - Meta-reasoning signals → frontal executive regions
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_INTUITION_THALAMIC_BRIDGE_H
#define NIMCP_INTUITION_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include "cognitive/parietal/nimcp_intuition_integrations.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

/** Maximum destinations for insight broadcast */
#define INTUITION_MAX_BROADCAST_TARGETS 8

/** Signal type identifiers for routed intuition signals */
#define INTUITION_SIGNAL_HUNCH           0x0001
#define INTUITION_SIGNAL_INSIGHT         0x0002
#define INTUITION_SIGNAL_ANALOGY         0x0003
#define INTUITION_SIGNAL_HYPOTHESIS      0x0004
#define INTUITION_SIGNAL_BLEND           0x0005
#define INTUITION_SIGNAL_COUNTERFACTUAL  0x0006
#define INTUITION_SIGNAL_META            0x0007
#define INTUITION_SIGNAL_EXTRAPOLATION   0x0008

/** Default attention weights for different insight types */
#define INTUITION_ATTENTION_HUNCH_DEFAULT        0.6f
#define INTUITION_ATTENTION_INSIGHT_DEFAULT      0.8f
#define INTUITION_ATTENTION_ANALOGY_DEFAULT      0.7f
#define INTUITION_ATTENTION_HYPOTHESIS_DEFAULT   0.5f
#define INTUITION_ATTENTION_BLEND_DEFAULT        0.6f
#define INTUITION_ATTENTION_COUNTERFACTUAL_DEFAULT 0.5f
#define INTUITION_ATTENTION_META_DEFAULT         0.4f

/* ============================================================================
 * Structures
 * ========================================================================== */

/**
 * @struct intuition_signal_t
 * @brief Packaged intuition signal for thalamic routing
 */
typedef struct {
    uint32_t signal_type;            /* INTUITION_SIGNAL_* type */
    uint32_t source_engine;          /* Which Phase 6 engine generated it */
    float confidence;                /* Signal confidence [0-1] */
    float salience;                  /* Perceptual salience [0-1] */
    float emotional_valence;         /* Emotional coloring [-1, +1] */
    float novelty;                   /* How novel is this insight [0-1] */
    float* payload;                  /* Signal data */
    uint32_t payload_size;           /* Size of payload */
    uint64_t timestamp_us;           /* Generation timestamp */
    char description[128];           /* Human-readable description */
} intuition_signal_t;

/**
 * @struct intuition_routing_target_t
 * @brief Destination specification for intuition signals
 */
typedef struct {
    uint32_t target_id;              /* Destination module ID */
    float attention_boost;           /* Additional attention weight */
    bool require_ack;                /* Require acknowledgment */
} intuition_routing_target_t;

/**
 * @struct intuition_thalamic_config_t
 * @brief Configuration for intuition-thalamic bridge
 */
typedef struct {
    /** Enable attention-based signal gating */
    bool enable_attention_gating;

    /** Enable priority routing for high-confidence hunches */
    bool enable_priority_routing;

    /** Enable multi-target broadcast for insights */
    bool enable_broadcast;

    /** Minimum confidence to route signal */
    float min_confidence_threshold;

    /** Minimum attention to pass thalamic gate */
    float min_attention_threshold;

    /** Boost attention for emotionally-valenced signals */
    float emotional_attention_boost;

    /** Boost attention for novel insights */
    float novelty_attention_boost;

    /** Default target IDs for different signal types */
    uint32_t default_hunch_targets[INTUITION_MAX_BROADCAST_TARGETS];
    uint32_t num_hunch_targets;

    uint32_t default_insight_targets[INTUITION_MAX_BROADCAST_TARGETS];
    uint32_t num_insight_targets;

    uint32_t default_meta_targets[INTUITION_MAX_BROADCAST_TARGETS];
    uint32_t num_meta_targets;
} intuition_thalamic_config_t;

/**
 * @struct intuition_thalamic_bridge_t
 * @brief Opaque handle for intuition-thalamic bridge
 */
typedef struct intuition_thalamic_bridge intuition_thalamic_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ========================================================================== */

/**
 * @brief Get default configuration for intuition-thalamic bridge
 * @return Default configuration
 */
intuition_thalamic_config_t intuition_thalamic_default_config(void);

/**
 * @brief Create intuition-thalamic bridge
 *
 * @param intuition Intuition system to connect
 * @param router Thalamic router for signal routing
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
intuition_thalamic_bridge_t* intuition_thalamic_bridge_create(
    intuition_system_t* intuition,
    thalamic_router_t* router,
    const intuition_thalamic_config_t* config
);

/**
 * @brief Destroy intuition-thalamic bridge
 * @param bridge Bridge to destroy
 */
void intuition_thalamic_bridge_destroy(intuition_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_bridge_reset(intuition_thalamic_bridge_t* bridge);

/* ============================================================================
 * Signal Routing Functions
 * ========================================================================== */

/**
 * @brief Route a hunch through thalamic gateway
 *
 * Routes a hunch from intuitive reasoning to appropriate cortical targets.
 * Applies attention gating based on confidence and emotional valence.
 *
 * @param bridge Bridge to use
 * @param hunch Hunch to route
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_hunch(
    intuition_thalamic_bridge_t* bridge,
    const hunch_t* hunch
);

/**
 * @brief Route an insight discovery through thalamic gateway
 *
 * Routes insight/aha moment to frontal and executive regions.
 * High-salience insights get priority routing.
 *
 * @param bridge Bridge to use
 * @param insight Insight data
 * @param novelty How novel is the insight [0-1]
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_insight(
    intuition_thalamic_bridge_t* bridge,
    const void* insight,
    float novelty
);

/**
 * @brief Route an analogical mapping through thalamic gateway
 *
 * Broadcasts analogy to multiple cortical regions for cross-domain
 * integration.
 *
 * @param bridge Bridge to use
 * @param analogy Analogy mapping data
 * @param strength Analogy strength [0-1]
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_analogy(
    intuition_thalamic_bridge_t* bridge,
    const void* analogy,
    float strength
);

/**
 * @brief Route a hypothesis through thalamic gateway
 *
 * Routes generated hypothesis to evaluation regions.
 *
 * @param bridge Bridge to use
 * @param theory Hypothesis/theory to route
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_hypothesis(
    intuition_thalamic_bridge_t* bridge,
    const hypogen_theory_t* theory
);

/**
 * @brief Route a conceptual blend through thalamic gateway
 *
 * @param bridge Bridge to use
 * @param blend Blended concept data
 * @param creativity Creativity score [0-1]
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_blend(
    intuition_thalamic_bridge_t* bridge,
    const void* blend,
    float creativity
);

/**
 * @brief Route meta-reasoning signal through thalamic gateway
 *
 * Routes meta-cognitive signals to executive control regions.
 *
 * @param bridge Bridge to use
 * @param meta_signal Meta-reasoning data
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_meta(
    intuition_thalamic_bridge_t* bridge,
    const void* meta_signal
);

/**
 * @brief Route a generic intuition signal
 *
 * General-purpose routing for any intuition signal type.
 *
 * @param bridge Bridge to use
 * @param signal Signal to route
 * @param targets Explicit targets (NULL for defaults)
 * @param num_targets Number of explicit targets
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_route_signal(
    intuition_thalamic_bridge_t* bridge,
    const intuition_signal_t* signal,
    const intuition_routing_target_t* targets,
    uint32_t num_targets
);

/* ============================================================================
 * Attention Control
 * ========================================================================== */

/**
 * @brief Set attention weight for intuition channel
 *
 * @param bridge Bridge to configure
 * @param attention Attention weight [0-1]
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_set_attention(
    intuition_thalamic_bridge_t* bridge,
    float attention
);

/**
 * @brief Get current attention weight for intuition channel
 *
 * @param bridge Bridge to query
 * @param attention Output attention weight
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_get_attention(
    const intuition_thalamic_bridge_t* bridge,
    float* attention
);

/**
 * @brief Boost attention for a specific signal type
 *
 * @param bridge Bridge to configure
 * @param signal_type INTUITION_SIGNAL_* type
 * @param boost Additional attention boost
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_boost_attention(
    intuition_thalamic_bridge_t* bridge,
    uint32_t signal_type,
    float boost
);

/* ============================================================================
 * Statistics
 * ========================================================================== */

/**
 * @struct intuition_thalamic_stats_t
 * @brief Statistics for intuition-thalamic bridge
 */
typedef struct {
    uint64_t signals_routed;         /* Total signals routed */
    uint64_t signals_dropped;        /* Signals dropped (below threshold) */
    uint64_t signals_bypassed;       /* High-priority bypasses */
    uint64_t hunches_routed;         /* Hunches routed */
    uint64_t insights_routed;        /* Insights routed */
    uint64_t analogies_routed;       /* Analogies routed */
    uint64_t hypotheses_routed;      /* Hypotheses routed */
    uint64_t blends_routed;          /* Blends routed */
    uint64_t meta_signals_routed;    /* Meta-reasoning signals routed */
    float avg_attention_weight;      /* Average attention applied */
    float avg_confidence;            /* Average signal confidence */
} intuition_thalamic_stats_t;

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int intuition_thalamic_bridge_get_stats(
    const intuition_thalamic_bridge_t* bridge,
    intuition_thalamic_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge to reset
 */
void intuition_thalamic_bridge_reset_stats(intuition_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTUITION_THALAMIC_BRIDGE_H */
