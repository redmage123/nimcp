/**
 * @file nimcp_attention_thalamic_bridge.h
 * @brief Bridge between Attention system and thalamic router
 *
 * WHAT: Routes attention signals through thalamic gating pathways
 * WHY: Attention itself requires thalamic mediation for focus/shifting
 * HOW: Packages attention signals, routes via thalamic reticular nucleus
 *
 * BIOLOGICAL BASIS:
 * - Thalamic reticular nucleus (TRN) gates attention
 * - Pulvinar coordinates spatial attention
 * - Mediodorsal nucleus supports executive attention
 * - TRN inhibitory surround creates attention focus
 *
 * SIGNAL ROUTING:
 * - Focus requests -> TRN -> Cortical suppression
 * - Shift signals -> Pulvinar -> Spatial reorienting
 * - Filter commands -> TRN -> Distractor inhibition
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_ATTENTION_THALAMIC_BRIDGE_H
#define NIMCP_ATTENTION_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Signal types for attention thalamic routing */
#define ATTENTION_SIGNAL_FOCUS       0x2A01  /**< Focus establishment */
#define ATTENTION_SIGNAL_SHIFT       0x2A02  /**< Attention shift request */
#define ATTENTION_SIGNAL_FILTER      0x2A03  /**< Distractor filtering */
#define ATTENTION_SIGNAL_RELEASE     0x2A04  /**< Attention release */
#define ATTENTION_SIGNAL_VIGILANCE   0x2A05  /**< Vigilance update */

/**
 * @brief Attention thalamic signal
 */
typedef struct {
    uint32_t signal_type;           /**< Signal type from defines above */
    float attention_priority;       /**< Priority level [0-1] */
    float target_salience;          /**< Target salience [0-1] */
    float shift_cost;               /**< Cost of shifting [0-1] */
    void* content;                  /**< Signal content */
    uint32_t content_size;          /**< Content size */
    uint64_t timestamp_us;          /**< Timestamp */
} attention_thalamic_signal_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_priority_gating;    /**< Gate by priority level */
    bool enable_shift_cost;         /**< Consider shift cost */
    bool enable_vigilance_boost;    /**< Boost vigilance signals */
    float min_priority_threshold;   /**< Minimum priority for routing */
    float shift_cost_penalty;       /**< Penalty for frequent shifts */
} attention_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t focus_requests;        /**< Focus establishments */
    uint64_t shifts_executed;       /**< Attention shifts */
    uint64_t filter_activations;    /**< Filter activations */
    uint64_t releases;              /**< Attention releases */
    uint64_t vigilance_updates;     /**< Vigilance updates */
    uint64_t signals_gated;         /**< Signals blocked */
    float avg_shift_cost;           /**< Average shift cost */
} attention_thalamic_stats_t;

typedef struct attention_thalamic_bridge attention_thalamic_bridge_t;

/* Configuration API */
attention_thalamic_config_t attention_thalamic_default_config(void);

/* Lifecycle API */
attention_thalamic_bridge_t* attention_thalamic_bridge_create(
    void* attention,
    thalamic_router_t* router,
    const attention_thalamic_config_t* config
);
void attention_thalamic_bridge_destroy(attention_thalamic_bridge_t* bridge);
int attention_thalamic_bridge_reset(attention_thalamic_bridge_t* bridge);

/* Signal Routing API */
int attention_thalamic_route_signal(
    attention_thalamic_bridge_t* bridge,
    const attention_thalamic_signal_t* signal
);
int attention_thalamic_request_focus(
    attention_thalamic_bridge_t* bridge,
    float priority,
    float target_salience
);
int attention_thalamic_request_shift(
    attention_thalamic_bridge_t* bridge,
    float new_priority,
    float shift_cost
);
int attention_thalamic_activate_filter(
    attention_thalamic_bridge_t* bridge,
    float filter_strength
);

/* Attention API */
int attention_thalamic_set_attention(attention_thalamic_bridge_t* bridge, float attention);
int attention_thalamic_get_attention(attention_thalamic_bridge_t* bridge, float* attention);

/* Statistics API */
int attention_thalamic_bridge_get_stats(
    const attention_thalamic_bridge_t* bridge,
    attention_thalamic_stats_t* stats
);
void attention_thalamic_bridge_reset_stats(attention_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_THALAMIC_BRIDGE_H */
