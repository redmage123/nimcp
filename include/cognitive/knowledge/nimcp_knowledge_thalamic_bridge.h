/**
 * @file nimcp_knowledge_thalamic_bridge.h
 * @brief Bridge between Knowledge system and thalamic router
 *
 * WHAT: Routes knowledge retrieval through attention-gated thalamic pathways
 * WHY: Semantic access requires attention-gated conscious retrieval
 * HOW: Packages knowledge signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Semantic retrieval involves temporal-thalamic circuits
 * - Thalamus gates which memories reach conscious access
 * - Attention modulates retrieval fluency
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_KNOWLEDGE_THALAMIC_BRIDGE_H
#define NIMCP_KNOWLEDGE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KNOWLEDGE_SIGNAL_RETRIEVAL   0x0301
#define KNOWLEDGE_SIGNAL_UPDATE      0x0302
#define KNOWLEDGE_SIGNAL_CONFLICT    0x0303
#define KNOWLEDGE_SIGNAL_INFERENCE   0x0304

typedef struct {
    uint32_t signal_type;
    float relevance;
    float confidence;
    float retrieval_strength;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} knowledge_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_relevance_boost;
    float min_retrieval_strength;
    float inference_threshold;
} knowledge_thalamic_config_t;

typedef struct knowledge_thalamic_bridge knowledge_thalamic_bridge_t;

knowledge_thalamic_config_t knowledge_thalamic_default_config(void);
knowledge_thalamic_bridge_t* knowledge_thalamic_bridge_create(void* knowledge, thalamic_router_t* router, const knowledge_thalamic_config_t* config);
void knowledge_thalamic_bridge_destroy(knowledge_thalamic_bridge_t* bridge);
int knowledge_thalamic_bridge_reset(knowledge_thalamic_bridge_t* bridge);
int knowledge_thalamic_route_retrieval(knowledge_thalamic_bridge_t* bridge, const knowledge_thalamic_signal_t* signal);
int knowledge_thalamic_route_inference(knowledge_thalamic_bridge_t* bridge, const void* inference, float confidence);
int knowledge_thalamic_set_attention(knowledge_thalamic_bridge_t* bridge, float attention);
int knowledge_thalamic_get_attention(knowledge_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t retrievals_routed;
    uint64_t inferences_routed;
    uint64_t conflicts_detected;
    float avg_retrieval_strength;
} knowledge_thalamic_stats_t;

int knowledge_thalamic_bridge_get_stats(knowledge_thalamic_bridge_t* bridge, knowledge_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_THALAMIC_BRIDGE_H */
