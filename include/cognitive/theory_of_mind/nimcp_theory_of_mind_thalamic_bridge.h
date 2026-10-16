/**
 * @file nimcp_theory_of_mind_thalamic_bridge.h
 * @brief Bridge between Theory of Mind system and thalamic router
 *
 * WHAT: Routes ToM signals through attention-gated thalamic pathways
 * WHY: Mental state inferences require conscious awareness via thalamic gating
 * HOW: Packages ToM signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - ToM processing involves thalamo-cortical loops for conscious awareness
 * - High-salience social inferences get enhanced routing priority
 * - Attention modulates which mental state inferences reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_THEORY_OF_MIND_THALAMIC_BRIDGE_H
#define NIMCP_THEORY_OF_MIND_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOM_SIGNAL_BELIEF       0x0C01
#define TOM_SIGNAL_DESIRE       0x0C02
#define TOM_SIGNAL_INTENTION    0x0C03
#define TOM_SIGNAL_DECEPTION    0x0C04

typedef struct {
    uint32_t signal_type;
    float social_salience;
    float confidence;
    float urgency;
    void* mental_state_data;
    uint32_t data_size;
    uint64_t timestamp_us;
} tom_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_salience_boost;
    float min_salience_threshold;
    float deception_boost;
} tom_thalamic_config_t;

typedef struct tom_thalamic_bridge tom_thalamic_bridge_t;

tom_thalamic_config_t tom_thalamic_default_config(void);
tom_thalamic_bridge_t* tom_thalamic_bridge_create(void* tom, thalamic_router_t* router, const tom_thalamic_config_t* config);
void tom_thalamic_bridge_destroy(tom_thalamic_bridge_t* bridge);
int tom_thalamic_bridge_reset(tom_thalamic_bridge_t* bridge);
int tom_thalamic_route_inference(tom_thalamic_bridge_t* bridge, const tom_thalamic_signal_t* signal);
int tom_thalamic_route_prediction(tom_thalamic_bridge_t* bridge, const void* prediction, float confidence);
int tom_thalamic_set_attention(tom_thalamic_bridge_t* bridge, float attention);
int tom_thalamic_get_attention(const tom_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t inferences_routed;
    uint64_t predictions_made;
    uint64_t deceptions_detected;
    float avg_confidence;
} tom_thalamic_stats_t;

int tom_thalamic_bridge_get_stats(const tom_thalamic_bridge_t* bridge, tom_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THEORY_OF_MIND_THALAMIC_BRIDGE_H */
