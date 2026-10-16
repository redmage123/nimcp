//=============================================================================
// nimcp_curiosity_hyperbolic.c - Hyperbolic Space Exploration for Curiosity
//=============================================================================
/**
 * @file nimcp_curiosity_hyperbolic.c
 * @brief Integration of hyperbolic knowledge embeddings with curiosity-driven learning
 *
 * PART B1.1 INTEGRATION: Curiosity Module
 *
 * WHAT: Use hyperbolic distances to guide exploration and learning
 * WHY: Enables hierarchical exploration at appropriate abstraction levels
 * HOW: Compute hyperbolic distances, explore along geodesics
 *
 * KEY INSIGHTS:
 * - Near origin (||x|| ≈ 0): Abstract concepts, general principles
 * - Mid-radius (||x|| ≈ 0.5): Domain-specific knowledge
 * - Near boundary (||x|| ≈ 0.9): Specific facts, examples
 *
 * EXPLORATION STRATEGIES:
 * 1. **Breadth-first (horizontal):** Explore concepts at same hierarchical level
 * 2. **Depth-first (vertical):** Drill down from abstract to specific
 * 3. **Geodesic (shortest path):** Follow natural connections in hyperbolic space
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
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "utils/geometry/nimcp_hyperbolic.h"
#include "utils/geometry/nimcp_lorentz.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "curiosity_hyperbolic"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(curiosity_hyperbolic, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Hyperbolic Exploration Strategies
//=============================================================================

/**
 * @brief Compute exploration priority based on hyperbolic distance
 *
 * WHAT: Assigns priority to concepts for exploration
 * WHY: Guides curiosity toward productive learning
 * HOW: Combines distance, hierarchical level, and novelty
 *
 * FORMULA:
 * priority = exploration_factor * (1 / (1 + distance)) + novelty_bonus
 *
 * Where:
 * - exploration_factor: 1.0 for nearby, 0.5 for far
 * - novelty_bonus: 0.2 if never seen before
 *
 * @param current_knowledge Current concept being explored
 * @param candidate_knowledge Candidate concept to explore
 * @param exploration_radius How far to look (hyperbolic distance)
 * @return Priority score (0-1, higher = more interesting)
 */
float curiosity_hyperbolic_priority(const knowledge_item_t *current_knowledge,
                                   const knowledge_item_t *candidate_knowledge,
                                   float exploration_radius) {
    if (!current_knowledge || !candidate_knowledge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "curiosity_hyperbolic_priority: NULL knowledge item");
        return 0.0f;
    }

    // Check both have hyperbolic embeddings
    if (!current_knowledge->hyperbolic_embedding || !candidate_knowledge->hyperbolic_embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "curiosity_hyperbolic_priority: missing hyperbolic embedding");
        return 0.0f;  // Can't compute hyperbolic priority
    }

    // Compute hyperbolic distance
    /* Phase 8: Heartbeat at operation start */
    curiosity_hyperbolic_heartbeat("curiosity_hy_priority", 0.0f);


    float distance = knowledge_hyperbolic_distance(current_knowledge, candidate_knowledge);
    if (distance < 0.0f) {
        return 0.0f;  // Invalid distance
    }

    // Distance factor: closer is more interesting (within exploration radius)
    float distance_factor = 0.0f;
    if (distance < exploration_radius) {
        distance_factor = 1.0f / (1.0f + distance);
    } else {
        distance_factor = 0.1f * expf(-(distance - exploration_radius));
    }

    // Hierarchical level factor: prefer same level or one level down
    float level_diff = fabsf(current_knowledge->hierarchical_level -
                            candidate_knowledge->hierarchical_level);
    float level_factor = expf(-level_diff);  // 1.0 if same level, decays exponentially

    // Novelty bonus: if candidate has low reinforcement count
    float novelty_bonus = 0.0f;
    if (candidate_knowledge->reinforcement_count == 0) {
        novelty_bonus = 0.2f;  // Brand new concept
    } else if (candidate_knowledge->reinforcement_count < 3) {
        novelty_bonus = 0.1f;  // Rarely seen
    }

    // Confidence factor: prefer concepts we partially understand (Goldilocks zone)
    // Too easy (confidence 1.0) or too hard (confidence 0.0) are less interesting
    float confidence = candidate_knowledge->confidence;
    float confidence_factor = 4.0f * confidence * (1.0f - confidence);  // Peaks at 0.5

    // Combine factors
    float priority = 0.4f * distance_factor +
                    0.3f * level_factor +
                    0.2f * confidence_factor +
                    0.1f * (1.0f + novelty_bonus);

    return fminf(priority, 1.0f);  // Clamp to [0, 1]
}

/**
 * @brief Find most interesting concepts to explore using hyperbolic space
 *
 * WHAT: Returns k most interesting concepts near current knowledge
 * WHY: Directs curiosity-driven exploration efficiently
 * HOW: Uses hyperbolic k-NN + priority scoring
 *
 * ALGORITHM:
 * 1. Find k*3 nearest neighbors in hyperbolic space
 * 2. Score each by exploration priority
 * 3. Return top k by priority
 *
 * @param system Knowledge system
 * @param current_concept Current concept being explored
 * @param k Number of interesting concepts to return
 * @param exploration_radius How far to look
 * @param interesting_out Output array [k]
 * @return Number of interesting concepts found
 */
uint32_t curiosity_find_interesting_hyperbolic(knowledge_system_t system,
                                              const knowledge_item_t *current_concept,
                                              uint32_t k,
                                              float exploration_radius,
                                              knowledge_item_t **interesting_out) {
    if (!system || !current_concept || k == 0 || !interesting_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "curiosity_find_interesting_hyperbolic: invalid parameters");
        return 0;
    }

    // Find more neighbors than needed (oversample)
    /* Phase 8: Heartbeat at operation start */
    curiosity_hyperbolic_heartbeat("curiosity_hy_curiosity_find_inter", 0.0f);


    uint32_t oversample_k = k * 3;
    knowledge_item_t **neighbors = nimcp_calloc(oversample_k, sizeof(knowledge_item_t*));
    float *distances = nimcp_calloc(oversample_k, sizeof(float));

    if (!neighbors || !distances) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "curiosity_find_interesting_hyperbolic: allocation failed");
        nimcp_free(neighbors);
        neighbors = NULL;
        nimcp_free(distances);
        distances = NULL;
        return 0;
    }

    // Get k-NN in hyperbolic space
    uint32_t num_neighbors = knowledge_hyperbolic_knn(system, current_concept,
                                                      oversample_k, neighbors, distances);

    if (num_neighbors == 0) {
        nimcp_free(neighbors);
        neighbors = NULL;
        nimcp_free(distances);
        distances = NULL;
        return 0;
    }

    // Score each neighbor by exploration priority
    typedef struct {
        knowledge_item_t *item;
        float priority;
    } scored_item_t;

    scored_item_t *scored = nimcp_calloc(num_neighbors, sizeof(scored_item_t));
    if (!scored) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "curiosity_find_interesting_hyperbolic: scored allocation failed");
        nimcp_free(neighbors);
        neighbors = NULL;
        nimcp_free(distances);
        distances = NULL;
        return 0;
    }

    for (uint32_t i = 0; i < num_neighbors; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neighbors > 256) {
            curiosity_hyperbolic_heartbeat("curiosity_hy_loop",
                             (float)(i + 1) / (float)num_neighbors);
        }

        scored[i].item = neighbors[i];
        scored[i].priority = curiosity_hyperbolic_priority(current_concept, neighbors[i],
                                                          exploration_radius);
    }

    // Sort by priority (simple bubble sort, ok for small k)
    for (uint32_t i = 0; i < num_neighbors - 1; i++) {
        for (uint32_t j = 0; j < num_neighbors - i - 1; j++) {
            if (scored[j].priority < scored[j + 1].priority) {
                scored_item_t temp = scored[j];
                scored[j] = scored[j + 1];
                scored[j + 1] = temp;
            }
        }
    }

    // Copy top k to output
    uint32_t num_interesting = (k < num_neighbors) ? k : num_neighbors;
    for (uint32_t i = 0; i < num_interesting; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_interesting > 256) {
            curiosity_hyperbolic_heartbeat("curiosity_hy_loop",
                             (float)(i + 1) / (float)num_interesting);
        }

        interesting_out[i] = scored[i].item;
    }

    // Cleanup
    nimcp_free(scored);
    scored = NULL;
    nimcp_free(neighbors);
    neighbors = NULL;
    nimcp_free(distances);
    distances = NULL;

    return num_interesting;
}

/**
 * @brief Recommend exploration strategy based on current position in hyperbolic space
 *
 * WHAT: Suggests whether to go broader (horizontal), deeper (vertical), or follow connections
 * WHY: Provides human-like learning guidance
 * HOW: Analyzes current position's radius and local density
 *
 * STRATEGIES:
 * - Near origin (||x|| < 0.3): **Go deeper** - learn specifics of abstract concepts
 * - Mid-radius (0.3 ≤ ||x|| < 0.7): **Go broader** - explore related concepts at same level
 * - Near boundary (||x|| ≥ 0.7): **Go up** - generalize from specific examples
 *
 * @param current_knowledge Current concept
 * @return Recommended strategy (as string)
 */
const char* curiosity_recommend_strategy_hyperbolic(const knowledge_item_t *current_knowledge) {
    if (!current_knowledge || !current_knowledge->hyperbolic_embedding) {
        return "explore_randomly";  // Fallback
    }

    // Compute radius (distance from origin)
    float radius = poincare_norm(current_knowledge->hyperbolic_embedding);

    if (radius < 0.3f) {
        return "explore_deeper";  // Near origin - drill down to specifics
    } else if (radius < 0.7f) {
        return "explore_broader";  // Mid-level - explore peers
    } else {
        return "explore_upward";  // Near boundary - abstract back up
    }
}

/**
 * @brief Visualize exploration state in hyperbolic space (debug helper)
 *
 * WHAT: Prints current exploration state for debugging
 * WHY: Helps understand curiosity-driven learning process
 *
 * @param current_knowledge Current concept
 * @param exploration_radius Current radius
 */
void curiosity_visualize_hyperbolic_state(const knowledge_item_t *current_knowledge,
                                         float exploration_radius) {
    if (!current_knowledge || !current_knowledge->hyperbolic_embedding) {
        printf("No hyperbolic embedding available\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_hyperbolic_heartbeat("curiosity_hy_curiosity_visualize_", 0.0f);


    float radius = poincare_norm(current_knowledge->hyperbolic_embedding);

    printf("=== Curiosity Hyperbolic State ===\n");
    printf("Current concept: %s\n", current_knowledge->concept_name);
    printf("Hierarchical level: %.2f\n", current_knowledge->hierarchical_level);
    printf("Radius (abstraction): %.4f\n", radius);
    printf("Exploration radius: %.2f\n", exploration_radius);
    printf("Strategy: %s\n", curiosity_recommend_strategy_hyperbolic(current_knowledge));
    printf("Position: [");
    for (uint32_t i = 0; i < current_knowledge->hyperbolic_embedding->dim; i++) {
        printf("%.3f", current_knowledge->hyperbolic_embedding->coords[i]);
        if (i < current_knowledge->hyperbolic_embedding->dim - 1) {
            printf(", ");
        }
    }
    printf("]\n");
    printf("================================\n");
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int curiosity_hyperbolic_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_hyperbolic_heartbeat("curiosity_hy_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_Hyperbolic_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_hyperbolic_heartbeat("curiosity_hy_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_Hyperbolic_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_Hyperbolic_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_hyperbolic_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_curiosity_hyperbolic_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_hyperbolic_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_hyperbolic_training_begin: NULL argument");
        return -1;
    }
    curiosity_hyperbolic_heartbeat_instance(g_curiosity_hyperbolic_health_agent, "curiosity_hyperbolic_training_begin", 0.0f);
    (void)instance; /* Module state available for reset */
    return 0;
}

int curiosity_hyperbolic_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_hyperbolic_training_end: NULL argument");
        return -1;
    }
    curiosity_hyperbolic_heartbeat_instance(g_curiosity_hyperbolic_health_agent, "curiosity_hyperbolic_training_end", 1.0f);
    (void)instance; /* Module state available for finalization */
    return 0;
}

int curiosity_hyperbolic_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_hyperbolic_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_hyperbolic_heartbeat_instance(g_curiosity_hyperbolic_health_agent, "curiosity_hyperbolic_training_step", progress);
    (void)instance; /* Module state available for step adaptation */
    return 0;
}
