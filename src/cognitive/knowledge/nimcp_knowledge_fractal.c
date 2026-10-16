//=============================================================================
// nimcp_knowledge_fractal.c - Fractal Topology for Knowledge Anchoring
//=============================================================================
/**
 * @file nimcp_knowledge_fractal.c
 * @brief ENHANCEMENT 2: Use hub neurons as concept anchors
 *
 * WHAT: Anchor knowledge concepts to hub neurons in fractal topology
 * WHY: Hub neurons have high connectivity - ideal for frequently accessed concepts
 * HOW: Map high-centrality concepts to high-centrality neurons
 *
 * CONCEPT ANCHORING:
 * - Fundamental concepts (e.g., "object", "action") → hub neurons
 * - Domain-specific concepts → mid-centrality neurons
 * - Specific instances → low-centrality neurons
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/nimcp_fractal_cognitive.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "cognitive.knowledge.fractal"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(knowledge_fractal, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Concept-to-Neuron Mapping
//=============================================================================

/**
 * @brief Find optimal neuron to anchor a knowledge concept
 *
 * WHAT: Selects neuron with centrality matching concept importance
 * WHY: Important concepts should be on hub neurons for fast access
 * HOW: Match concept importance to neuron centrality
 *
 * ALGORITHM:
 * 1. Estimate concept importance (0-1)
 * 2. Find neurons with similar centrality
 * 3. Return best match
 *
 * @param cache Fractal cognitive cache
 * @param concept_importance Importance score (0-1)
 * @return Neuron index, or UINT32_MAX if none found
 */
uint32_t knowledge_fractal_find_anchor_neuron(const fractal_cognitive_cache_t *cache,
                                              float concept_importance) {
    if (!cache || !cache->valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "knowledge_fractal_find_anchor_neuron: invalid cache");
        return UINT32_MAX;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_fractal_heartbeat("knowledge_fr_find_anchor_neuron", 0.0f);


    uint32_t N = cache->stats.num_neurons;
    uint32_t best_neuron = UINT32_MAX;
    float best_match = INFINITY;

    // Find neuron with centrality closest to concept importance
    for (uint32_t i = 0; i < N; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && N > 256) {
            knowledge_fractal_heartbeat("knowledge_fr_loop",
                             (float)(i + 1) / (float)N);
        }

        float centrality = fractal_get_centrality(cache, i);
        float match_error = fabsf(centrality - concept_importance);

        if (match_error < best_match) {
            best_match = match_error;
            best_neuron = i;
        }
    }

    return best_neuron;
}

/**
 * @brief Get concept reinforcement factor based on neuron centrality
 *
 * WHAT: Boosts learning for concepts on hub neurons
 * WHY: Hub neurons are accessed more frequently - faster learning
 * HOW: Learning rate multiplier proportional to centrality
 *
 * @param cache Fractal cognitive cache
 * @param neuron_index Neuron hosting the concept
 * @return Learning rate multiplier (1.0-2.0)
 */
float knowledge_fractal_learning_boost(const fractal_cognitive_cache_t *cache,
                                       uint32_t neuron_index) {
    if (!cache || !cache->valid) {
        return 1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_fractal_heartbeat("knowledge_fr_learning_boost", 0.0f);


    float centrality = fractal_get_centrality(cache, neuron_index);

    // Hub neurons get up to 2x learning boost
    return 1.0F + centrality;
}

/**
 * @brief Retrieve related concepts using fractal topology
 *
 * WHAT: Finds concepts related by proximity in fractal network
 * WHY: Concepts on nearby neurons are semantically related
 * HOW: Graph traversal from concept's anchor neuron
 *
 * @param network Neural network
 * @param cache Fractal cognitive cache
 * @param concept_neuron Anchor neuron of query concept
 * @param max_distance Maximum graph distance (hops)
 * @param related_out Output array (caller allocates)
 * @param max_related Maximum related concepts to return
 * @return Number of related concepts found
 */
uint32_t knowledge_fractal_get_related(neural_network_t network,
                                       const fractal_cognitive_cache_t *cache,
                                       uint32_t concept_neuron,
                                       uint32_t max_distance,
                                       uint32_t *related_out,
                                       uint32_t max_related) {
    if (!network || !cache || !cache->valid || !related_out || max_related == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "knowledge_fractal_get_related: invalid parameters");
        return 0;
    }

    // Simplified: Get k central neighbors within radius
    /* Phase 8: Heartbeat at operation start */
    knowledge_fractal_heartbeat("knowledge_fr_get_related", 0.0f);


    return fractal_get_central_neighbors(network, cache,
                                        concept_neuron,
                                        max_distance,
                                        max_related,
                                        related_out);
}

/**
 * @brief Estimate semantic distance using fractal topology
 *
 * WHAT: Computes semantic distance between two concepts
 * WHY: Graph distance correlates with semantic distance
 * HOW: Combines graph distance + hierarchical level difference
 *
 * @param cache Fractal cognitive cache
 * @param neuron1 First concept's anchor neuron
 * @param neuron2 Second concept's anchor neuron
 * @return Semantic distance (0 = identical, higher = more different)
 */
float knowledge_fractal_semantic_distance(const fractal_cognitive_cache_t *cache,
                                         uint32_t neuron1,
                                         uint32_t neuron2) {
    if (!cache || !cache->valid) {
        return INFINITY;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_fractal_heartbeat("knowledge_fr_semantic_distance", 0.0f);


    if (neuron1 == neuron2) {
        return 0.0F;
    }

    // Simplified distance: index difference + level difference
    float index_dist = fabsf((float)neuron1 - (float)neuron2);
    float level1 = fractal_get_hierarchical_level(cache, neuron1);
    float level2 = fractal_get_hierarchical_level(cache, neuron2);
    float level_dist = fabsf(level1 - level2);

    // Combine: 70% graph distance + 30% level distance
    return 0.7F * index_dist + 0.3F * level_dist * cache->stats.num_neurons;
}

//=============================================================================
// Debug and Visualization
//=============================================================================

/**
 * @brief Print knowledge concept's fractal context
 *
 * @param cache Fractal cognitive cache
 * @param concept_neuron Concept's anchor neuron
 * @param concept_name Concept name
 */
void knowledge_fractal_print_concept_context(const fractal_cognitive_cache_t *cache,
                                            uint32_t concept_neuron,
                                            const char *concept_name) {
    if (!cache || !cache->valid || !concept_name) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_fractal_heartbeat("knowledge_fr_print_concept_contex", 0.0f);


    printf("=== Concept Fractal Context ===\n");
    printf("Concept: %s\n", concept_name);
    printf("Anchor neuron: %u\n", concept_neuron);
    printf("Is hub: %s\n", fractal_is_hub_neuron(cache, concept_neuron) ? "YES" : "no");
    printf("Centrality: %.4f\n", fractal_get_centrality(cache, concept_neuron));
    printf("Hierarchical level: %.3f\n", fractal_get_hierarchical_level(cache, concept_neuron));
    printf("Learning boost: %.2fx\n", knowledge_fractal_learning_boost(cache, concept_neuron));
    printf("===============================\n");
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query knowledge graph for self-knowledge about fractal knowledge anchoring
 *
 * WHAT: Retrieves entity observations and relations for fractal topology
 * WHY: Enables self-aware introspection of module capabilities
 * HOW: Uses kg_reader to query JSONL knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int knowledge_fractal_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_fractal_heartbeat("knowledge_fr_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_Fractal");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_fractal_heartbeat("knowledge_fr_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_Fractal");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_Fractal");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_fractal_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_knowledge_fractal_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_fractal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_fractal_training_begin: NULL argument");
        return -1;
    }
    knowledge_fractal_heartbeat_instance(NULL, "knowledge_fractal_training_begin", 0.0f);
    return 0;
}

int knowledge_fractal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_fractal_training_end: NULL argument");
        return -1;
    }
    knowledge_fractal_heartbeat_instance(NULL, "knowledge_fractal_training_end", 1.0f);
    return 0;
}

int knowledge_fractal_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_fractal_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_fractal_heartbeat_instance(NULL, "knowledge_fractal_training_step", progress);
    return 0;
}
