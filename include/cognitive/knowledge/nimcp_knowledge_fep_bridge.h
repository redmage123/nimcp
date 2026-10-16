/**
 * @file nimcp_knowledge_fep_bridge.h
 * @brief Free Energy Principle - Knowledge Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and Knowledge system
 * WHY:  Knowledge = semantic priors in generative model. FEP learning updates
 *       knowledge graph; knowledge constrains FEP inference via semantic priors.
 * HOW:  FEP PE → knowledge updates; knowledge → FEP semantic priors
 *
 * BIOLOGICAL BASIS:
 * - Semantic memory as hierarchical generative model priors
 * - Knowledge acquisition via prediction error minimization
 * - Reference: Hinton (2007) "Learning multiple layers of representation"
 */

#ifndef NIMCP_KNOWLEDGE_FEP_BRIDGE_H
#define NIMCP_KNOWLEDGE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

#define KNOWLEDGE_FEP_UPDATE_THRESHOLD       4.0f
#define KNOWLEDGE_FEP_SEMANTIC_PRIOR_WEIGHT  0.8f

typedef struct knowledge_fep_bridge knowledge_fep_bridge_t;

typedef struct {
    float knowledge_update_threshold;
    float semantic_prior_weight;
    bool enable_knowledge_updates;
    bool enable_semantic_priors;
    float pe_sensitivity;
} knowledge_fep_config_t;

typedef struct {
    float semantic_pe;
    float knowledge_confidence;
} knowledge_fep_effects_t;

typedef struct {
    uint32_t knowledge_updates;
    float current_semantic_prior;
} knowledge_fep_state_t;

typedef struct {
    uint64_t updates_total;
    float avg_semantic_pe;
} knowledge_fep_stats_t;

struct knowledge_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    knowledge_fep_config_t config;
    fep_system_t* fep_system;
    knowledge_system_t knowledge_system;
    knowledge_fep_effects_t effects;
    knowledge_fep_state_t state;
    knowledge_fep_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

int knowledge_fep_bridge_default_config(knowledge_fep_config_t* config);
knowledge_fep_bridge_t* knowledge_fep_bridge_create(const knowledge_fep_config_t* config);
void knowledge_fep_bridge_destroy(knowledge_fep_bridge_t* bridge);

int knowledge_fep_bridge_connect_fep(knowledge_fep_bridge_t* bridge, fep_system_t* fep);
int knowledge_fep_bridge_connect_knowledge(knowledge_fep_bridge_t* bridge, knowledge_system_t knowledge);

int knowledge_fep_update_knowledge(knowledge_fep_bridge_t* bridge, float prediction_error);
int knowledge_fep_apply_semantic_priors(knowledge_fep_bridge_t* bridge);

int knowledge_fep_bridge_update(knowledge_fep_bridge_t* bridge, uint64_t delta_ms);
int knowledge_fep_bridge_get_state(knowledge_fep_bridge_t* bridge, knowledge_fep_state_t* state);
int knowledge_fep_bridge_get_stats(knowledge_fep_bridge_t* bridge, knowledge_fep_stats_t* stats);

int knowledge_fep_bridge_connect_bio_async(knowledge_fep_bridge_t* bridge);
int knowledge_fep_bridge_disconnect_bio_async(knowledge_fep_bridge_t* bridge);
bool knowledge_fep_bridge_is_bio_async_connected(const knowledge_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_FEP_BRIDGE_H */
