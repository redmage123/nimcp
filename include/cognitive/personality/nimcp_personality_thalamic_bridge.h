/**
 * @file nimcp_personality_thalamic_bridge.h
 * @brief Bridge between Personality system and thalamic router
 *
 * WHAT: Routes personality expressions through attention-gated thalamic pathways
 * WHY: Personality modulates behavior via conscious and unconscious pathways
 * HOW: Packages trait signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Personality involves prefrontal regulatory circuits
 * - Thalamus modulates trait expression through attention
 * - Trait-relevant stimuli get enhanced processing
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PERSONALITY_THALAMIC_BRIDGE_H
#define NIMCP_PERSONALITY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PERSONALITY_SIGNAL_TRAIT      0x0601
#define PERSONALITY_SIGNAL_STATE      0x0602
#define PERSONALITY_SIGNAL_REGULATION 0x0603
#define PERSONALITY_SIGNAL_EXPRESSION 0x0604

typedef struct {
    uint32_t signal_type;
    float trait_activation;
    float state_intensity;
    float regulation_effort;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} personality_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_state_modulation;
    float min_trait_activation;
    float regulation_threshold;
} personality_thalamic_config_t;

typedef struct personality_thalamic_bridge personality_thalamic_bridge_t;

personality_thalamic_config_t personality_thalamic_default_config(void);
personality_thalamic_bridge_t* personality_thalamic_bridge_create(void* personality, thalamic_router_t* router, const personality_thalamic_config_t* config);
void personality_thalamic_bridge_destroy(personality_thalamic_bridge_t* bridge);
int personality_thalamic_bridge_reset(personality_thalamic_bridge_t* bridge);
int personality_thalamic_route_trait(personality_thalamic_bridge_t* bridge, const personality_thalamic_signal_t* signal);
int personality_thalamic_route_regulation(personality_thalamic_bridge_t* bridge, const void* regulation, float effort);
int personality_thalamic_set_attention(personality_thalamic_bridge_t* bridge, float attention);
int personality_thalamic_get_attention(personality_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t traits_expressed;
    uint64_t regulations_applied;
    uint64_t state_changes;
    float avg_trait_activation;
} personality_thalamic_stats_t;

int personality_thalamic_bridge_get_stats(personality_thalamic_bridge_t* bridge, personality_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERSONALITY_THALAMIC_BRIDGE_H */
