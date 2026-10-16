#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_routing_table.c - Dynamic Neural Routing Rules
//=============================================================================

#include "middleware/routing/nimcp_routing_table.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_routing_table"
#define LOG_MODULE_ID 0x052A
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(routing_table)

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct route_node {
    routing_rule_t rule;
    struct route_node* next;
} route_node_t;

#define HASH_TABLE_SIZE 256

struct routing_table {
    // Configuration
    routing_table_config_t config;

    // Route storage (hash table by source ID)
    route_node_t* hash_table[HASH_TABLE_SIZE];

    // Statistics
    uint32_t num_routes;
    uint64_t total_usage;
    uint64_t total_prunes;

    // Note: Memory pools not applicable here - query results are returned to caller
    // and freed via routing_table_free_query which doesn't have pool access.
    // Variable-size allocations (1-16 entries) also don't fit fixed-block pools well.
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static uint32_t hash_source_id(uint32_t source_id) {
    return source_id % HASH_TABLE_SIZE;
}

static route_node_t* find_route(const routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id) {
    uint32_t hash = hash_source_id(source_id);
    route_node_t* node = table->hash_table[hash];

    while (node) {
        if (node->rule.source_id == source_id &&
            node->rule.dest_id == dest_id) {
            return node;
        }
        node = node->next;
    }

    return NULL;  /* Route not found - normal return, not an error */
}

static bool create_route(routing_table_t* table,
                        uint32_t source_id,
                        uint32_t dest_id,
                        float strength) {
    if (table->num_routes >= table->config.max_routes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hash_source_id: capacity exceeded");
        return false;
    }

    route_node_t* node = (route_node_t*)nimcp_calloc(1, sizeof(route_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hash_source_id: node is NULL");
        return false;
    }

    node->rule.source_id = source_id;
    node->rule.dest_id = dest_id;
    node->rule.strength = strength;
    node->rule.priority = 0;
    node->rule.usage_count = 0;
    node->rule.last_used_ms = 0;

    uint32_t hash = hash_source_id(source_id);
    node->next = table->hash_table[hash];
    table->hash_table[hash] = node;

    table->num_routes++;

    return true;
}

static void apply_decay_to_all(routing_table_t* table) {
    if (!table->config.enable_learning) return;

    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        route_node_t* node = table->hash_table[i];
        while (node) {
            node->rule.strength *= table->config.strength_decay;
            node = node->next;
        }
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

routing_table_config_t routing_table_default_config(void) {
    routing_table_config_t config;
    config.max_routes = ROUTING_TABLE_MAX_ROUTES;
    config.max_paths_per_source = ROUTING_TABLE_MAX_PATHS;
    config.min_strength = ROUTING_MIN_STRENGTH;
    config.strength_decay = ROUTING_STRENGTH_DECAY;
    config.enable_learning = true;
    config.enable_pruning = true;
    config.enable_multi_path = true;
    config.learning_rate = 0.1F;
    return config;
}

routing_table_t* routing_table_create(const routing_table_config_t* config) {
    if (!config || config->max_routes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_table_create: config is NULL or max_routes is 0");
        return NULL;
    }

    routing_table_t* table = (routing_table_t*)nimcp_calloc(1, sizeof(routing_table_t));
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "routing_table_create: failed to allocate table");
        return NULL;
    }

    table->config = *config;

    // Initialize hash table
    memset(table->hash_table, 0, sizeof(table->hash_table));

    // Initialize statistics
    table->num_routes = 0;
    table->total_usage = 0;
    table->total_prunes = 0;

    return table;
}

void routing_table_destroy(routing_table_t* table) {
    if (!table) return;

    // Free all routes
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        route_node_t* node = table->hash_table[i];
        while (node) {
            route_node_t* next = node->next;
            nimcp_free(node);
            node = next;
        }
    }

    nimcp_free(table);
}

bool routing_table_add_route(routing_table_t* table,
                             uint32_t source_id,
                             uint32_t dest_id,
                             float initial_strength) {
    if (!table || initial_strength < 0.0F || initial_strength > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "routing_table_destroy: table is NULL");
        return false;
    }

    route_node_t* existing = find_route(table, source_id, dest_id);

    if (existing) {
        // Update existing route
        if (table->config.enable_learning) {
            float lr = table->config.learning_rate;
            existing->rule.strength += lr * (1.0F - existing->rule.strength);
            if (existing->rule.strength > 1.0F) {
                existing->rule.strength = 1.0F;
            }
        }
        return true;
    }

    // Create new route
    return create_route(table, source_id, dest_id, initial_strength);
}

bool routing_table_query_routes(const routing_table_t* table,
                                uint32_t source_id,
                                route_query_t* result) {
    if (!table || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: required parameter is NULL (table, result)");
        return false;
    }

    // Count routes for this source
    uint32_t hash = hash_source_id(source_id);
    route_node_t* node = table->hash_table[hash];

    uint32_t count = 0;
    while (node) {
        if (node->rule.source_id == source_id &&
            node->rule.strength >= table->config.min_strength) {
            count++;
        }
        node = node->next;
    }

    if (count == 0) {
        result->dest_ids = NULL;
        result->strengths = NULL;
        result->num_dests = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_table_destroy: count is zero");
        return false;
    }

    // Allocate result arrays
    result->dest_ids = (uint32_t*)nimcp_malloc(count * sizeof(uint32_t));
    result->strengths = (float*)nimcp_malloc(count * sizeof(float));

    if (!result->dest_ids || !result->strengths) {
        nimcp_free(result->dest_ids);
        nimcp_free(result->strengths);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "routing_table_destroy: required parameter is NULL (result->dest_ids, result->strengths)");
        return false;
    }

    // Populate results
    node = table->hash_table[hash];
    uint32_t idx = 0;

    while (node && idx < count) {
        if (node->rule.source_id == source_id &&
            node->rule.strength >= table->config.min_strength) {
            result->dest_ids[idx] = node->rule.dest_id;
            result->strengths[idx] = node->rule.strength;
            idx++;
        }
        node = node->next;
    }

    result->num_dests = idx;

    // Sort by strength (descending)
    for (uint32_t i = 0; i < result->num_dests; i++) {
        for (uint32_t j = i + 1; j < result->num_dests; j++) {
            if (result->strengths[j] > result->strengths[i]) {
                float temp_s = result->strengths[i];
                result->strengths[i] = result->strengths[j];
                result->strengths[j] = temp_s;

                uint32_t temp_d = result->dest_ids[i];
                result->dest_ids[i] = result->dest_ids[j];
                result->dest_ids[j] = temp_d;
            }
        }
    }

    // Limit to max paths if configured
    if (!table->config.enable_multi_path && result->num_dests > 0) {
        result->num_dests = 1;
    } else if (result->num_dests > table->config.max_paths_per_source) {
        result->num_dests = table->config.max_paths_per_source;
    }

    return true;
}

bool routing_table_get_strength(const routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id,
                                float* strength) {
    if (!table || !strength) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: required parameter is NULL (table, strength)");
        return false;
    }

    const route_node_t* node = find_route(table, source_id, dest_id);

    if (!node) {
        *strength = 0.0F;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: node is NULL");
        return false;
    }

    *strength = node->rule.strength;
    return true;
}

bool routing_table_set_priority(routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id,
                                uint32_t priority) {
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: table is NULL");
        return false;
    }

    route_node_t* node = find_route(table, source_id, dest_id);

    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: node is NULL");
        return false;
    }

    node->rule.priority = priority;
    return true;
}

bool routing_table_use_route(routing_table_t* table,
                             uint32_t source_id,
                             uint32_t dest_id) {
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: table is NULL");
        return false;
    }

    route_node_t* node = find_route(table, source_id, dest_id);

    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: node is NULL");
        return false;
    }

    node->rule.usage_count++;
    node->rule.last_used_ms++;  // Placeholder timestamp

    table->total_usage++;

    // Hebbian learning
    if (table->config.enable_learning) {
        float lr = table->config.learning_rate;
        node->rule.strength += lr * (1.0F - node->rule.strength);
        if (node->rule.strength > 1.0F) {
            node->rule.strength = 1.0F;
        }

        // Apply decay to all other routes
        apply_decay_to_all(table);
    }

    return true;
}

bool routing_table_remove_route(routing_table_t* table,
                                uint32_t source_id,
                                uint32_t dest_id) {
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_destroy: table is NULL");
        return false;
    }

    uint32_t hash = hash_source_id(source_id);
    route_node_t** prev = &table->hash_table[hash];
    route_node_t* node = table->hash_table[hash];

    while (node) {
        if (node->rule.source_id == source_id &&
            node->rule.dest_id == dest_id) {
            *prev = node->next;
            nimcp_free(node);
            table->num_routes--;
            return true;
        }
        prev = &node->next;
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_table_destroy: operation failed");
    return false;
}

bool routing_table_prune(routing_table_t* table, uint32_t* num_pruned) {
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_prune: table is NULL");
        return false;
    }

    uint32_t pruned = 0;

    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        route_node_t** prev = &table->hash_table[i];
        route_node_t* node = table->hash_table[i];

        while (node) {
            if (node->rule.strength < table->config.min_strength) {
                *prev = node->next;
                route_node_t* to_free = node;
                node = node->next;
                nimcp_free(to_free);
                table->num_routes--;
                pruned++;
            } else {
                prev = &node->next;
                node = node->next;
            }
        }
    }

    table->total_prunes += pruned;

    if (num_pruned) *num_pruned = pruned;

    return true;
}

bool routing_table_get_stats(const routing_table_t* table,
                             uint32_t* num_routes,
                             float* avg_strength,
                             uint64_t* total_usage) {
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_table_prune: table is NULL");
        return false;
    }

    if (num_routes) *num_routes = table->num_routes;
    if (total_usage) *total_usage = table->total_usage;

    if (avg_strength) {
        if (table->num_routes == 0) {
            *avg_strength = 0.0F;
        } else {
            float sum = 0.0F;
            uint32_t count = 0;

            for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
                route_node_t* node = table->hash_table[i];
                while (node) {
                    sum += node->rule.strength;
                    count++;
                    node = node->next;
                }
            }

            *avg_strength = sum / (float)count;
        }
    }

    return true;
}

void routing_table_clear(routing_table_t* table) {
    if (!table) return;

    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        route_node_t* node = table->hash_table[i];
        while (node) {
            route_node_t* next = node->next;
            nimcp_free(node);
            node = next;
        }
        table->hash_table[i] = NULL;
    }

    table->num_routes = 0;
}

void routing_table_free_query(route_query_t* result) {
    if (!result) return;

    nimcp_free(result->dest_ids);
    nimcp_free(result->strengths);

    result->dest_ids = NULL;
    result->strengths = NULL;
    result->num_dests = 0;
}
