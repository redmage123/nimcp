/**
 * @file nimcp_epistemic_thalamic_bridge.h
 * @brief Bridge between Epistemic system and thalamic router
 *
 * WHAT: Routes epistemic (knowledge-seeking) signals through thalamic pathways
 * WHY: Epistemic processing requires attention for conscious knowledge seeking
 * HOW: Packages epistemic signals, routes via mediodorsal/pulvinar pathways
 *
 * BIOLOGICAL BASIS:
 * - Epistemic curiosity involves prefrontal-thalamic circuits
 * - Mediodorsal nucleus supports metacognitive awareness
 * - Pulvinar coordinates epistemic attention
 * - Uncertainty signals route through anterior thalamus
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EPISTEMIC_THALAMIC_BRIDGE_H
#define NIMCP_EPISTEMIC_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPISTEMIC_SIGNAL_UNCERTAINTY    0x2E01  /**< Uncertainty detection */
#define EPISTEMIC_SIGNAL_INQUIRY        0x2E02  /**< Knowledge inquiry */
#define EPISTEMIC_SIGNAL_BELIEF_UPDATE  0x2E03  /**< Belief update */
#define EPISTEMIC_SIGNAL_CONFIDENCE     0x2E04  /**< Confidence assessment */

typedef struct {
    uint32_t signal_type;
    float epistemic_urgency;
    float uncertainty_level;
    float confidence;
    float information_gain;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} epistemic_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_uncertainty_boost;
    float min_urgency_threshold;
    float uncertainty_boost_factor;
} epistemic_thalamic_config_t;

typedef struct {
    uint64_t uncertainties_routed;
    uint64_t inquiries_routed;
    uint64_t belief_updates;
    uint64_t confidence_assessments;
    uint64_t signals_gated;
    float avg_uncertainty;
    float avg_information_gain;
} epistemic_thalamic_stats_t;

typedef struct epistemic_thalamic_bridge epistemic_thalamic_bridge_t;

epistemic_thalamic_config_t epistemic_thalamic_default_config(void);
epistemic_thalamic_bridge_t* epistemic_thalamic_bridge_create(
    void* epistemic, thalamic_router_t* router,
    const epistemic_thalamic_config_t* config);
void epistemic_thalamic_bridge_destroy(epistemic_thalamic_bridge_t* bridge);
int epistemic_thalamic_bridge_reset(epistemic_thalamic_bridge_t* bridge);

int epistemic_thalamic_route_signal(
    epistemic_thalamic_bridge_t* bridge,
    const epistemic_thalamic_signal_t* signal);
int epistemic_thalamic_route_uncertainty(
    epistemic_thalamic_bridge_t* bridge,
    float uncertainty, float urgency);
int epistemic_thalamic_route_inquiry(
    epistemic_thalamic_bridge_t* bridge,
    float expected_gain, float urgency);

int epistemic_thalamic_set_attention(epistemic_thalamic_bridge_t* bridge, float attention);
int epistemic_thalamic_get_attention(epistemic_thalamic_bridge_t* bridge, float* attention);
int epistemic_thalamic_bridge_get_stats(
    const epistemic_thalamic_bridge_t* bridge,
    epistemic_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPISTEMIC_THALAMIC_BRIDGE_H */
