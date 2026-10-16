/**
 * @file nimcp_thalamic_router_fep_bridge.h
 * @brief Free Energy Principle - Thalamic Router Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and thalamic routing
 * WHY:  Thalamus routes prediction errors up (bottom-up) and predictions down (top-down);
 *       FEP precision modulates routing gain and attention
 * HOW:  FEP precision → routing attention weights; routing patterns → FEP observations;
 *       prediction errors boost routing priority
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → THALAMIC ROUTING PATHWAYS:
 * ---------------------------------
 * 1. Precision as Thalamic Gain:
 *    - High FEP precision → increased thalamic gain (attention)
 *    - Low precision → reduced gain (ignored/filtered)
 *    - Implements precision-weighting via routing modulation
 *    - Reference: Sherman & Guillery (2013) "Functional connections of cortical areas"
 *
 * 2. Predictions as Top-Down Routing:
 *    - FEP predictions routed from higher to lower cortex via thalamus
 *    - Pulvinar coordinates prediction distribution
 *    - Top-down modulation of sensory processing
 *    - Reference: Saalmann & Kastner (2011) "Cognitive and perceptual functions of
 *      the visual thalamus"
 *
 * 3. Prediction Errors Boost Priority:
 *    - High PE signals routed with high priority
 *    - Critical for updating beliefs
 *    - Attention to surprising/unexpected inputs
 *    - Reference: Friston (2005) "A theory of cortical responses"
 *
 * THALAMIC ROUTING → FEP PATHWAYS:
 * ---------------------------------
 * 1. Routed Signals as Observations:
 *    - Bottom-up routing → sensory observations
 *    - Higher-order relay → contextual observations
 *    - Signal strength indicates confidence
 *    - Reference: Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * 2. Routing Synchrony as Confidence:
 *    - Synchronized routing → high confidence
 *    - Thalamic bursts → salient/urgent signals
 *    - Tonic firing → regular information transfer
 *    - Reference: Steriade (2006) "Grouping of brain rhythms in corticothalamic systems"
 *
 * 3. Routing Bottlenecks as Capacity Constraints:
 *    - Queue saturation → limited processing capacity
 *    - Dropped signals → information loss
 *    - Feeds back to FEP as uncertainty source
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THALAMIC_ROUTER_FEP_BRIDGE_H
#define NIMCP_THALAMIC_ROUTER_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "middleware/routing/nimcp_thalamic_router.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define FEP_PRECISION_HIGH_GAIN          1.5f   /**< High precision gain boost */
#define FEP_PRECISION_LOW_GAIN           0.5f   /**< Low precision gain reduction */
#define FEP_PE_HIGH_PRIORITY_THRESHOLD   5.0f   /**< PE threshold for high priority */

typedef struct thalamic_router_fep_bridge thalamic_router_fep_bridge_t;

typedef struct {
    bool enable_precision_gain;
    bool enable_prediction_routing;
    bool enable_pe_priority_boost;
    bool enable_synchrony_confidence;
    float gain_sensitivity;
    float priority_sensitivity;
} thalamic_router_fep_config_t;

typedef struct {
    float routing_gain_modifier;
    float prediction_routing_weight;
    signal_priority_t pe_priority_level;
} thalamic_router_fep_effects_t;

typedef struct {
    float current_precision;
    float current_pe;
    uint32_t signals_routed;
    float routing_synchrony;
} thalamic_router_fep_state_t;

typedef struct {
    uint64_t precision_adjustments;
    uint64_t pe_priority_boosts;
    float avg_routing_gain;
    float avg_pe;
} thalamic_router_fep_stats_t;

struct thalamic_router_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    thalamic_router_fep_config_t config;
    thalamic_router_t* thalamic_router;
    fep_system_t* fep_system;
    thalamic_router_fep_effects_t effects;
    thalamic_router_fep_state_t state;
    thalamic_router_fep_stats_t stats;
};

/* Lifecycle */
int thalamic_router_fep_bridge_default_config(thalamic_router_fep_config_t* config);
thalamic_router_fep_bridge_t* thalamic_router_fep_bridge_create(const thalamic_router_fep_config_t* config);
void thalamic_router_fep_bridge_destroy(thalamic_router_fep_bridge_t* bridge);

/* Connection */
int thalamic_router_fep_bridge_connect_router(thalamic_router_fep_bridge_t* bridge, thalamic_router_t* router);
int thalamic_router_fep_bridge_connect_fep(thalamic_router_fep_bridge_t* bridge, fep_system_t* fep);
int thalamic_router_fep_bridge_disconnect(thalamic_router_fep_bridge_t* bridge);

/* FEP → Router */
int thalamic_router_fep_apply_precision_gain(thalamic_router_fep_bridge_t* bridge, float precision);
int thalamic_router_fep_route_prediction(thalamic_router_fep_bridge_t* bridge, float prediction);
int thalamic_router_fep_boost_pe_priority(thalamic_router_fep_bridge_t* bridge, float prediction_error);

/* Router → FEP */
int thalamic_router_fep_report_routed_signal(thalamic_router_fep_bridge_t* bridge, const routed_signal_t* signal);
int thalamic_router_fep_update_confidence_from_routing(thalamic_router_fep_bridge_t* bridge, float synchrony);

/* Update */
int thalamic_router_fep_bridge_update(thalamic_router_fep_bridge_t* bridge, uint64_t delta_ms);

/* State/Stats */
int thalamic_router_fep_bridge_get_state(const thalamic_router_fep_bridge_t* bridge, thalamic_router_fep_state_t* state);
int thalamic_router_fep_bridge_get_stats(const thalamic_router_fep_bridge_t* bridge, thalamic_router_fep_stats_t* stats);

/* Bio-Async */
int thalamic_router_fep_bridge_connect_bio_async(thalamic_router_fep_bridge_t* bridge);
int thalamic_router_fep_bridge_disconnect_bio_async(thalamic_router_fep_bridge_t* bridge);
bool thalamic_router_fep_bridge_is_bio_async_connected(const thalamic_router_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THALAMIC_ROUTER_FEP_BRIDGE_H */
