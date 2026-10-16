//=============================================================================
// nimcp_curiosity_fractal.c - Fractal Topology Integration for Curiosity
//=============================================================================
/**
 * @file nimcp_curiosity_fractal.c
 * @brief ENHANCEMENT 2: Hub-based exploration for curiosity module
 *
 * WHAT: Use fractal topology to guide curiosity-driven exploration
 * WHY: Hub neurons are information bottlenecks - exploring via hubs is efficient
 * HOW: Route exploration through hub neurons, use centrality for priority
 *
 * STRATEGY:
 * 1. **Hub Hopping:** Jump between hub neurons for broad exploration
 * 2. **Hub Anchoring:** Use hubs as landmarks for navigation
 * 3. **Centrality Priority:** Weight exploration by betweenness centrality
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include "cognitive/curiosity/nimcp_curiosity.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/nimcp_fractal_cognitive.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <math.h>

#define LOG_MODULE "curiosity_fractal"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(curiosity_fractal, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Hub-Based Exploration
//=============================================================================

/**
 * @brief Guide exploration toward hub neurons
 *
 * WHAT: Modifies exploration priority to favor hub neurons
 * WHY: Hubs are information bottlenecks with high connectivity
 * HOW: Boost priority for neurons near hubs
 *
 * @param cache Fractal cognitive cache
 * @param neuron_index Candidate neuron to explore
 * @param base_priority Base exploration priority
 * @return Boosted priority (0-1)
 */
float curiosity_fractal_boost_hub_priority(const fractal_cognitive_cache_t *cache,
                                           uint32_t neuron_index,
                                           float base_priority) {
    if (!cache || !cache->valid) {
        return base_priority;
    }

    // Check if this is a hub neuron
    /* Phase 8: Heartbeat at operation start */
    curiosity_fractal_heartbeat("curiosity_fr_boost_hub_priority", 0.0f);


    bool is_hub = fractal_is_hub_neuron(cache, neuron_index);
    if (is_hub) {
        return fminf(base_priority * 1.5f, 1.0f);  // 50% boost for hubs
    }

    // Otherwise boost by centrality
    float centrality = fractal_get_centrality(cache, neuron_index);
    float boost = 1.0f + 0.3f * centrality;  // Up to 30% boost
    return fminf(base_priority * boost, 1.0f);
}

/**
 * @brief Find next exploration target using hub-hopping strategy
 *
 * WHAT: Identifies next neuron to explore via hub neurons
 * WHY: Efficient exploration of scale-free networks
 * HOW: Find nearest hub, then explore from hub
 *
 * ALGORITHM:
 * 1. Find nearest hub to current position
 * 2. Get k most central neighbors of that hub
 * 3. Score by exploration priority (including hyperbolic if available)
 * 4. Return highest priority
 *
 * @param network Neural network
 * @param cache Fractal cognitive cache
 * @param current_neuron Current exploration position
 * @param k Number of candidates to consider
 * @return Next neuron index to explore, or UINT32_MAX if none
 */
uint32_t curiosity_fractal_next_exploration_target(neural_network_t network,
                                                   const fractal_cognitive_cache_t *cache,
                                                   uint32_t current_neuron,
                                                   uint32_t k) {
    if (!network || !cache || !cache->valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "curiosity_fractal_next_exploration_target: invalid parameters");
        return UINT32_MAX;
    }

    // Find nearest hub
    /* Phase 8: Heartbeat at operation start */
    curiosity_fractal_heartbeat("curiosity_fr_next_exploration_tar", 0.0f);


    uint32_t nearest_hub = fractal_nearest_hub(network, cache, current_neuron, NULL);
    if (nearest_hub == UINT32_MAX) {
        return UINT32_MAX;
    }

    // Get k most central neighbors of the hub
    uint32_t *candidates = (uint32_t*)nimcp_calloc(k, sizeof(uint32_t));
    if (!candidates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "curiosity_fractal_next_exploration_target: allocation failed");
        return UINT32_MAX;
    }

    uint32_t num_candidates = fractal_get_central_neighbors(network, cache,
                                                            nearest_hub,
                                                            5,  // 5-hop radius
                                                            k,
                                                            candidates);

    if (num_candidates == 0) {
        nimcp_free(candidates);
        candidates = NULL;
        return UINT32_MAX;
    }

    // Score candidates by centrality + hierarchical level diversity
    float current_level = fractal_get_hierarchical_level(cache, current_neuron);
    uint32_t best_candidate = candidates[0];
    float best_score = 0.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_candidates > 256) {
            curiosity_fractal_heartbeat("curiosity_fr_loop",
                             (float)(i + 1) / (float)num_candidates);
        }

        uint32_t candidate = candidates[i];
        float centrality = fractal_get_centrality(cache, candidate);
        float level = fractal_get_hierarchical_level(cache, candidate);
        float level_diversity = fabsf(level - current_level);

        // Score = 70% centrality + 30% level diversity
        float score = 0.7f * centrality + 0.3f * level_diversity;

        if (score > best_score) {
            best_score = score;
            best_candidate = candidate;
        }
    }

    nimcp_free(candidates);
    candidates = NULL;
    return best_candidate;
}

/**
 * @brief Get exploration strategy based on current network position
 *
 * WHAT: Recommends exploration strategy (hub-hop, local, or vertical)
 * WHY: Different positions require different exploration approaches
 * HOW: Analyze centrality and hierarchical level
 *
 * STRATEGIES:
 * - High centrality (hub): **LOCAL** - explore neighbors
 * - Mid centrality: **HUB_HOP** - jump to nearest hub
 * - Low centrality + high level: **UPWARD** - move toward hubs/root
 * - Low centrality + low level: **DOWNWARD** - explore leaves
 *
 * @param cache Fractal cognitive cache
 * @param neuron_index Current position
 * @return Strategy string
 */
const char* curiosity_fractal_recommend_strategy(const fractal_cognitive_cache_t *cache,
                                                uint32_t neuron_index) {
    if (!cache || !cache->valid) {
        return "random";
    }

    float centrality = fractal_get_centrality(cache, neuron_index);
    float level = fractal_get_hierarchical_level(cache, neuron_index);
    bool is_hub = fractal_is_hub_neuron(cache, neuron_index);

    if (is_hub || centrality > 0.7f) {
        return "local";  // High centrality - explore locally
    } else if (centrality > 0.3f) {
        return "hub_hop";  // Mid centrality - hop to hub
    } else if (level < 0.3f) {
        return "downward";  // Near root - go deeper
    } else {
        return "upward";  // Near leaves - go up toward hubs
    }
}

/**
 * @brief Compute exploration radius based on fractal structure
 *
 * WHAT: Determines how far to explore from current position
 * WHY: Scale-free networks have variable local density
 * HOW: Use hierarchical level to set radius
 *
 * FORMULA:
 * radius = base_radius * (1 + 0.5 * (1 - hierarchical_level))
 *
 * Near root (level ≈ 0): larger radius (global exploration)
 * Near leaves (level ≈ 1): smaller radius (local exploration)
 *
 * @param cache Fractal cognitive cache
 * @param neuron_index Current position
 * @param base_radius Base exploration radius (typical: 2.0-5.0)
 * @return Adjusted radius
 */
float curiosity_fractal_exploration_radius(const fractal_cognitive_cache_t *cache,
                                          uint32_t neuron_index,
                                          float base_radius) {
    if (!cache || !cache->valid) {
        return base_radius;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fractal_heartbeat("curiosity_fr_exploration_radius", 0.0f);


    float level = fractal_get_hierarchical_level(cache, neuron_index);

    // Higher level (near leaves) → smaller radius
    // Lower level (near root) → larger radius
    float radius_factor = 1.0f + 0.5f * (1.0f - level);

    return base_radius * radius_factor;
}

//=============================================================================
// Visualization and Debug
//=============================================================================

/**
 * @brief Print curiosity exploration state with fractal context
 *
 * @param cache Fractal cognitive cache
 * @param neuron_index Current exploration position
 */
void curiosity_fractal_print_state(const fractal_cognitive_cache_t *cache,
                                  uint32_t neuron_index) {
    if (!cache || !cache->valid) {
        printf("Fractal state: UNAVAILABLE\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_fractal_heartbeat("curiosity_fr_print_state", 0.0f);


    printf("=== Curiosity Fractal State ===\n");
    printf("Neuron: %u\n", neuron_index);
    printf("Is hub: %s\n", fractal_is_hub_neuron(cache, neuron_index) ? "YES" : "no");
    printf("Centrality: %.4f\n", fractal_get_centrality(cache, neuron_index));
    printf("Degree (norm): %.3f\n", fractal_get_degree_normalized(cache, neuron_index));
    printf("Hierarchical level: %.3f\n", fractal_get_hierarchical_level(cache, neuron_index));
    printf("Strategy: %s\n", curiosity_fractal_recommend_strategy(cache, neuron_index));
    printf("Exploration radius: %.2f\n",
           curiosity_fractal_exploration_radius(cache, neuron_index, 3.0f));
    printf("===============================\n");
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int curiosity_fractal_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_fractal_heartbeat("curiosity_fr_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_Fractal_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_fractal_heartbeat("curiosity_fr_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_Fractal_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_Fractal_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_fractal_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_curiosity_fractal_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_fractal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_fractal_training_begin: NULL argument");
        return -1;
    }
    curiosity_fractal_heartbeat_instance(g_curiosity_fractal_health_agent, "curiosity_fractal_training_begin", 0.0f);
    return 0;
}

int curiosity_fractal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_fractal_training_end: NULL argument");
        return -1;
    }
    curiosity_fractal_heartbeat_instance(g_curiosity_fractal_health_agent, "curiosity_fractal_training_end", 1.0f);
    return 0;
}

int curiosity_fractal_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_fractal_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_fractal_heartbeat_instance(g_curiosity_fractal_health_agent, "curiosity_fractal_training_step", progress);
    return 0;
}
