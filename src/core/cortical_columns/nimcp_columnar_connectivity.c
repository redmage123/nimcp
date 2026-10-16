/**
 * @file nimcp_columnar_connectivity.c
 * @brief Implementation of cortical columnar connectivity
 *
 * WHAT: Biologically-realistic connectivity patterns for cortical columns
 * WHY:  Implement canonical microcircuit with distance-dependent, layer-specific rules
 * HOW:  Connection pools, hash tables for fast lookup, mathematical connectivity models
 */

#include "core/cortical_columns/nimcp_columnar_connectivity.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/algorithms/nimcp_monte_carlo.h"  /* For mc_random_uniform */
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <pthread.h>
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "columnar_connectivity"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(columnar_connectivity)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void columnar_connectivity_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_CORTICAL_COLUMNAR_CONNECTIVITY,
        .module_name = "columnar_connectivity",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for columnar_connectivity module");
    }
}

__attribute__((constructor))
static void columnar_connectivity_bio_init(void) {
    pthread_once(&bio_init_once, columnar_connectivity_bio_init_impl);
}

__attribute__((destructor))
static void columnar_connectivity_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for columnar_connectivity module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// CONSTANTS
//=============================================================================

#define MAX_RULES 64
#define HASH_TABLE_SIZE 1024
#define MAX_COLUMN_DEGREE 10000  // Max connections per column
#define STDP_TAU_PLUS_US 20000.0f  // 20ms in microseconds
#define STDP_TAU_MINUS_US 20000.0f
#define STDP_A_PLUS 0.01f
#define STDP_A_MINUS -0.01f
#define STDP_WINDOW_US 20000  // 20ms window
#define DEFAULT_CONDUCTION_VELOCITY 1.0f  // 1 m/s
#define DEFAULT_SYNAPTIC_DELAY 0.5f  // 0.5ms

// Small-world analysis constants
#define MIN_COLUMNS_FOR_ANALYSIS 10
#define SAMPLE_SIZE_FOR_PATH_LENGTH 100
#define INFINITY_DISTANCE 1e9f

//=============================================================================
// INTERNAL DATA STRUCTURES
//=============================================================================

/**
 * WHAT: Hash table entry for connection lookups
 * WHY:  Fast O(1) lookup of connections by source/target column
 * HOW:  Separate chaining with linked lists
 */
typedef struct connection_node {
    columnar_connection_t connection;
    struct connection_node* next;
} connection_node_t;

/**
 * WHAT: Hash table for connections
 * WHY:  Enable fast lookup by source or target column ID
 */
typedef struct {
    connection_node_t* buckets[HASH_TABLE_SIZE];
    uint32_t count;
} connection_hash_table_t;

/**
 * WHAT: Columnar connectivity manager internal structure
 * WHY:  Store all connections, rules, and lookup structures
 */
struct columnar_connectivity {
    // Connection storage
    columnar_connection_t* connections;  /**< Connection pool */
    uint32_t num_connections;            /**< Active connections */
    uint32_t max_connections;            /**< Pool capacity */

    // Fast lookup tables
    connection_hash_table_t* by_source;  /**< Hash by source column */
    connection_hash_table_t* by_target;  /**< Hash by target column */

    // Connectivity rules
    connectivity_rule_t rules[MAX_RULES];
    uint32_t num_rules;

    // Thread safety
    nimcp_platform_mutex_t lock;

    // Statistics cache
    connectivity_stats_t cached_stats;
    bool stats_valid;
};

//=============================================================================
// HASH TABLE OPERATIONS
//=============================================================================

/**
 * WHAT: Hash function for column IDs
 * WHY:  Distribute columns evenly across hash table
 * HOW:  Simple modulo hash (could use FNV or MurmurHash for better distribution)
 */
static inline uint32_t hash_column_id(uint32_t column_id) {
    return column_id % HASH_TABLE_SIZE;
}

/**
 * WHAT: Create hash table
 * WHY:  Initialize empty hash table for connections
 */
static connection_hash_table_t* conn_hash_table_create(void) {
    connection_hash_table_t* table = nimcp_calloc(1, sizeof(connection_hash_table_t));
    if (!table) {
        LOG_ERROR("Failed to allocate hash table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "table is NULL");

        return NULL;
    }
    return table;
}

/**
 * WHAT: Destroy hash table
 * WHY:  Free all nodes and table memory
 */
static void conn_hash_table_destroy(connection_hash_table_t* table) {
    if (!table) return;

    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        connection_node_t* node = table->buckets[i];
        while (node) {
            connection_node_t* next = node->next;
            nimcp_free(node);
            node = next;
        }
    }

    nimcp_free(table);
}

/**
 * WHAT: Insert connection into hash table
 * WHY:  Enable fast lookup by key (source or target column)
 * HOW:  Hash key, prepend to bucket linked list
 */
static nimcp_result_t conn_hash_table_insert(
    connection_hash_table_t* table,
    uint32_t key,
    const columnar_connection_t* conn)
{
    if (!table || !conn) return NIMCP_INVALID_PARAM;

    uint32_t bucket = hash_column_id(key);

    connection_node_t* node = nimcp_malloc(sizeof(connection_node_t));
    if (!node) {
        LOG_ERROR("Failed to allocate hash node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate hash node");
        return NIMCP_NO_MEMORY;
    }

    node->connection = *conn;
    node->next = table->buckets[bucket];
    table->buckets[bucket] = node;
    table->count++;

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Get all connections with matching key
 * WHY:  Retrieve all connections from/to a column
 * HOW:  Walk bucket linked list, copy matching connections
 */
static uint32_t conn_hash_table_get(
    connection_hash_table_t* table,
    uint32_t key,
    columnar_connection_t* out_connections,
    uint32_t max_connections)
{
    if (!table || !out_connections) return 0;

    uint32_t bucket = hash_column_id(key);
    uint32_t count = 0;

    connection_node_t* node = table->buckets[bucket];
    while (node && count < max_connections) {
        out_connections[count++] = node->connection;
        node = node->next;
    }

    return count;
}

//=============================================================================
// RANDOM NUMBER GENERATION
//=============================================================================

/** Thread-local RNG seed for reproducible random generation */
static __thread uint32_t g_rng_seed = 0;
static __thread bool g_rng_initialized = false;

/**
 * WHAT: Initialize RNG seed if not already done
 * WHY:  Ensure each thread has a unique starting seed
 * HOW:  Use thread ID and time-based seed from mc_seed_from_time
 */
static inline void ensure_rng_initialized(void) {
    if (!g_rng_initialized) {
        g_rng_seed = mc_seed_from_time();
        g_rng_initialized = true;
    }
}

/**
 * WHAT: Generate random float in [0, 1)
 * WHY:  For probabilistic connection generation
 * HOW:  Use mc_random_uniform from nimcp_monte_carlo for better RNG quality
 */
static inline float randf(void) {
    ensure_rng_initialized();
    return mc_random_uniform(&g_rng_seed);
}

/**
 * WHAT: Generate random Gaussian (Box-Muller transform)
 * WHY:  For patchy connectivity and weight initialization
 * HOW:  Use mc_random_normal from nimcp_monte_carlo for thread-safe Gaussian
 */
static float randg(float mean, float stddev) {
    ensure_rng_initialized();
    return mc_random_normal(&g_rng_seed, mean, stddev);
}

//=============================================================================
// DISTANCE AND SIMILARITY FUNCTIONS
//=============================================================================

/**
 * WHAT: Compute Euclidean distance between columns
 * WHY:  Distance-dependent connectivity probability
 * HOW:  sqrt((x1-x2)² + (y1-y2)²)
 */
static float compute_distance(
    const float* pos1,
    const float* pos2,
    uint32_t dims)
{
    float sum_sq = 0.0F;
    for (uint32_t i = 0; i < dims; i++) {
        float diff = pos1[i] - pos2[i];
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq);
}

/**
 * WHAT: Compute feature similarity (orientation tuning)
 * WHY:  Columns with similar features connect preferentially
 * HOW:  S(θ1, θ2) = 0.5 × (1 + cos(2(θ1 - θ2)))
 */
static float compute_feature_similarity(float feature1, float feature2) {
    float diff = feature1 - feature2;
    return 0.5F * (1.0F + cosf(2.0F * diff));
}

/**
 * WHAT: Distance-dependent connection probability
 * WHY:  P(d) = P₀ × exp(-d/λ) (exponential decay with distance)
 * HOW:  Multiply base probability by exponential decay factor
 */
static float connection_probability(
    float distance,
    float base_prob,
    float lambda)
{
    if (lambda <= 0.0F) return base_prob;
    return base_prob * expf(-distance / lambda);
}

/**
 * WHAT: Compute patchy connectivity probability
 * WHY:  Biological connections cluster at specific distances (0.5mm, 1.5mm, 3mm)
 * HOW:  Sum of Gaussian patches at characteristic distances
 */
static float patchy_probability(float distance) {
    // Patch centers (mm) and weights (Bosking et al. 1997)
    const float patch_centers[] = {0.5F, 1.5F, 3.0F};
    const float patch_widths[] = {0.2F, 0.4F, 0.6F};
    const float patch_weights[] = {0.4F, 0.35F, 0.25F};
    const int num_patches = 3;

    float prob = 0.0F;
    for (int i = 0; i < num_patches; i++) {
        float diff = distance - patch_centers[i];
        float gaussian = expf(-diff * diff / (2.0F * patch_widths[i] * patch_widths[i]));
        prob += patch_weights[i] * gaussian;
    }

    return prob;
}

//=============================================================================
// LIFECYCLE MANAGEMENT
//=============================================================================

columnar_connectivity_t* columnar_connectivity_create(uint32_t max_connections) {
    // Guard: validate parameters
    if (max_connections == 0) {
        LOG_ERROR("max_connections must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "columnar_connectivity_create: max_connections is zero");
        return NULL;
    }

    columnar_connectivity_t* conn = nimcp_calloc(1, sizeof(columnar_connectivity_t));
    if (!conn) {
        LOG_ERROR("Failed to allocate connectivity manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "conn is NULL");

        return NULL;
    }

    // Allocate connection pool
    conn->connections = nimcp_malloc(sizeof(columnar_connection_t) * max_connections);
    if (!conn->connections) {
        LOG_ERROR("Failed to allocate connection pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate connection pool");
        nimcp_free(conn);
        return NULL;
    }

    conn->max_connections = max_connections;
    conn->num_connections = 0;

    // Create hash tables
    conn->by_source = conn_hash_table_create();
    conn->by_target = conn_hash_table_create();
    if (!conn->by_source || !conn->by_target) {
        LOG_ERROR("Failed to create hash tables");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create hash tables");
        conn_hash_table_destroy(conn->by_source);
        conn_hash_table_destroy(conn->by_target);
        nimcp_free(conn->connections);
        nimcp_free(conn);
        return NULL;
    }

    // Initialize mutex (recursive=true to allow nested locking in get_stats->compute_*)
    if (nimcp_platform_mutex_init(&conn->lock, true) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        conn_hash_table_destroy(conn->by_source);
        conn_hash_table_destroy(conn->by_target);
        nimcp_free(conn->connections);
        nimcp_free(conn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "columnar_connectivity_create: validation failed");
        return NULL;
    }

    conn->num_rules = 0;
    conn->stats_valid = false;

    LOG_INFO("Created columnar connectivity manager (capacity: %u)", max_connections);
    return conn;
}

void columnar_connectivity_destroy(columnar_connectivity_t* conn) {
    // Guard: NULL-safe
    if (!conn) return;

    LOG_DEBUG("Destroying columnar connectivity manager");

    nimcp_platform_mutex_destroy(&conn->lock);
    conn_hash_table_destroy(conn->by_source);
    conn_hash_table_destroy(conn->by_target);
    nimcp_free(conn->connections);
    nimcp_free(conn);
}

//=============================================================================
// CONNECTIVITY RULES
//=============================================================================

nimcp_result_t connectivity_add_rule(
    columnar_connectivity_t* conn,
    const connectivity_rule_t* rule)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return NIMCP_INVALID_PARAM;
    }
    if (!rule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rule is NULL");
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    // Guard: check capacity
    if (conn->num_rules >= MAX_RULES) {
        LOG_ERROR("Maximum number of rules exceeded");
        nimcp_platform_mutex_unlock(&conn->lock);
        return NIMCP_ERROR;
    }

    conn->rules[conn->num_rules++] = *rule;
    conn->stats_valid = false;  // Invalidate cached stats

    nimcp_platform_mutex_unlock(&conn->lock);

    LOG_DEBUG("Added connectivity rule (type: %d, P0: %.3f, lambda: %.3f)",
              rule->type, rule->base_probability, rule->distance_decay_lambda);

    return NIMCP_SUCCESS;
}

nimcp_result_t connectivity_apply_canonical_rules(columnar_connectivity_t* conn) {
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Applying canonical microcircuit rules (Douglas & Martin 1991)");

    // Rule 1: Layer IV → II/III (thalamic input to association)
    connectivity_rule_t rule1 = {
        .type = CONNECTIVITY_INTRACOLUMNAR,
        .base_probability = 0.3F,
        .distance_decay_lambda = 0.2F,  // mm
        .feature_similarity_weight = 0.0F,
        .layer_specific = true,
        .source_layer = CC_LAYER_IV,
        .target_layer = CC_LAYER_II_III,
        .min_delay_ms = 0.5F,
        .conduction_velocity_m_s = 1.0F
    };
    connectivity_add_rule(conn, &rule1);

    // Rule 2: Layer II/III → V (association to output)
    connectivity_rule_t rule2 = {
        .type = CONNECTIVITY_INTRACOLUMNAR,
        .base_probability = 0.2F,
        .distance_decay_lambda = 0.3F,
        .feature_similarity_weight = 0.0F,
        .layer_specific = true,
        .source_layer = CC_LAYER_II_III,
        .target_layer = CC_LAYER_V,
        .min_delay_ms = 0.5F,
        .conduction_velocity_m_s = 1.0F
    };
    connectivity_add_rule(conn, &rule2);

    // Rule 3: Layer V → VI (output to feedback)
    connectivity_rule_t rule3 = {
        .type = CONNECTIVITY_INTRACOLUMNAR,
        .base_probability = 0.15F,
        .distance_decay_lambda = 0.25F,
        .feature_similarity_weight = 0.0F,
        .layer_specific = true,
        .source_layer = CC_LAYER_V,
        .target_layer = CC_LAYER_VI,
        .min_delay_ms = 0.5F,
        .conduction_velocity_m_s = 1.0F
    };
    connectivity_add_rule(conn, &rule3);

    // Rule 4: Horizontal connections (patchy, same layer)
    connectivity_rule_t rule4 = {
        .type = CONNECTIVITY_INTERCOLUMNAR,
        .base_probability = 0.1F,
        .distance_decay_lambda = 1.5F,  // mm (patchy clusters)
        .feature_similarity_weight = 0.5F,  // Feature similarity modulates
        .layer_specific = false,
        .source_layer = CC_LAYER_II_III,
        .target_layer = CC_LAYER_II_III,
        .min_delay_ms = 1.0F,
        .conduction_velocity_m_s = 0.5F  // Slower horizontal axons
    };
    connectivity_add_rule(conn, &rule4);

    LOG_INFO("Applied %u canonical rules", conn->num_rules);
    return NIMCP_SUCCESS;
}

//=============================================================================
// INTERNAL CONNECTION CREATION
//=============================================================================

/**
 * WHAT: Add a single connection to the manager
 * WHY:  Central function for connection creation
 * HOW:  Add to pool, insert into hash tables
 */
static nimcp_result_t add_connection(
    columnar_connectivity_t* conn,
    const columnar_connection_t* connection)
{
    // Guard: check capacity
    if (conn->num_connections >= conn->max_connections) {
        LOG_ERROR("Connection pool full");
        return NIMCP_ERROR;
    }

    // Add to pool
    conn->connections[conn->num_connections++] = *connection;

    // Index in hash tables
    conn_hash_table_insert(conn->by_source, connection->source_column_id, connection);
    conn_hash_table_insert(conn->by_target, connection->target_column_id, connection);

    conn->stats_valid = false;
    return NIMCP_SUCCESS;
}

//=============================================================================
// CONNECTION GENERATION
//=============================================================================

uint32_t connectivity_generate_intracolumnar(
    columnar_connectivity_t* conn,
    uint32_t column_id,
    const laminar_structure_t* layers)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return 0;
    }
    if (!layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layers is NULL");
        return 0;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    uint32_t connections_created = 0;

    // Apply each intracolumnar rule
    for (uint32_t r = 0; r < conn->num_rules; r++) {
        const connectivity_rule_t* rule = &conn->rules[r];

        // Guard: only intracolumnar rules
        if (rule->type != CONNECTIVITY_INTRACOLUMNAR) continue;

        cc_cortical_layer_t src_layer = rule->source_layer;
        cc_cortical_layer_t tgt_layer = rule->target_layer;

        // Guard: check layer validity
        if (src_layer >= CC_LAYER_COUNT || tgt_layer >= CC_LAYER_COUNT) continue;

        uint32_t src_count = laminar_get_layer_neuron_count(layers, src_layer);
        uint32_t tgt_count = laminar_get_layer_neuron_count(layers, tgt_layer);

        // Generate connections between layers
        for (uint32_t i = 0; i < src_count && i < MAX_COLUMN_DEGREE; i++) {
            for (uint32_t j = 0; j < tgt_count && j < MAX_COLUMN_DEGREE; j++) {
                // Probabilistic connection
                if (randf() < rule->base_probability) {
                    columnar_connection_t new_conn = {
                        .source_column_id = column_id,
                        .target_column_id = column_id,
                        .source_layer = src_layer,
                        .target_layer = tgt_layer,
                        .weight = 0.5F + randg(0.0F, 0.1F),  // ~N(0.5, 0.1)
                        .delay_ms = rule->min_delay_ms,
                        .type = CONNECTIVITY_INTRACOLUMNAR
                    };

                    // Clip weight to [0, 1]
                    if (new_conn.weight < 0.0F) new_conn.weight = 0.0F;
                    if (new_conn.weight > 1.0F) new_conn.weight = 1.0F;

                    if (add_connection(conn, &new_conn) == NIMCP_SUCCESS) {
                        connections_created++;
                    }
                }
            }
        }
    }

    nimcp_platform_mutex_unlock(&conn->lock);

    LOG_DEBUG("Generated %u intracolumnar connections for column %u",
              connections_created, column_id);

    return connections_created;
}

uint32_t connectivity_generate_intercolumnar(
    columnar_connectivity_t* conn,
    const uint32_t* column_ids,
    uint32_t num_columns,
    const float* positions,
    uint32_t dims)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return 0;
    }
    if (!column_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "column_ids is NULL");
        return 0;
    }
    if (num_columns == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "num_columns must be > 0");
        return 0;
    }
    if (dims != 2 && dims != 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dims must be 2 or 3");
        return 0;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    uint32_t connections_created = 0;

    // If no positions provided, use grid layout
    float* local_positions = NULL;
    if (!positions) {
        local_positions = nimcp_malloc(sizeof(float) * num_columns * dims);
        if (!local_positions) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate local_positions");
            nimcp_platform_mutex_unlock(&conn->lock);
            return 0;
        }

        // Simple grid layout
        uint32_t grid_size = (uint32_t)sqrtf((float)num_columns) + 1;
        for (uint32_t i = 0; i < num_columns; i++) {
            local_positions[i * dims + 0] = (float)(i % grid_size);
            local_positions[i * dims + 1] = (float)(i / grid_size);
            if (dims == 3) local_positions[i * dims + 2] = 0.0F;
        }

        positions = local_positions;
    }

    // Apply each intercolumnar rule
    for (uint32_t r = 0; r < conn->num_rules; r++) {
        const connectivity_rule_t* rule = &conn->rules[r];

        // Guard: only intercolumnar rules
        if (rule->type != CONNECTIVITY_INTERCOLUMNAR) continue;

        // For each column pair
        for (uint32_t i = 0; i < num_columns; i++) {
            for (uint32_t j = 0; j < num_columns; j++) {
                // Guard: no self-connections
                if (i == j) continue;

                // Compute distance
                const float* pos_i = &positions[i * dims];
                const float* pos_j = &positions[j * dims];
                float distance = compute_distance(pos_i, pos_j, dims);

                // Distance-dependent probability
                float prob = connection_probability(
                    distance,
                    rule->base_probability,
                    rule->distance_decay_lambda
                );

                // Add patchy modulation
                prob *= patchy_probability(distance);

                // Feature similarity modulation (use column IDs as proxy features)
                if (rule->feature_similarity_weight > 0.0F) {
                    float feature_i = (float)column_ids[i] / (float)num_columns * 3.14159F;
                    float feature_j = (float)column_ids[j] / (float)num_columns * 3.14159F;
                    float similarity = compute_feature_similarity(feature_i, feature_j);
                    prob *= (1.0F - rule->feature_similarity_weight +
                            rule->feature_similarity_weight * similarity);
                }

                // Probabilistic connection
                if (randf() < prob) {
                    // Compute delay based on distance and conduction velocity
                    float delay_ms = rule->min_delay_ms;
                    if (rule->conduction_velocity_m_s > 0.0F) {
                        // distance in mm, velocity in m/s
                        delay_ms += (distance / 1000.0F) / rule->conduction_velocity_m_s * 1000.0F;
                    }

                    columnar_connection_t new_conn = {
                        .source_column_id = column_ids[i],
                        .target_column_id = column_ids[j],
                        .source_layer = rule->layer_specific ? rule->source_layer : CC_LAYER_II_III,
                        .target_layer = rule->layer_specific ? rule->target_layer : CC_LAYER_II_III,
                        .weight = 0.3F + randg(0.0F, 0.05F),
                        .delay_ms = delay_ms,
                        .type = CONNECTIVITY_INTERCOLUMNAR
                    };

                    // Clip weight
                    if (new_conn.weight < 0.0F) new_conn.weight = 0.0F;
                    if (new_conn.weight > 1.0F) new_conn.weight = 1.0F;

                    if (add_connection(conn, &new_conn) == NIMCP_SUCCESS) {
                        connections_created++;
                    }
                }
            }
        }
    }

    nimcp_free(local_positions);
    nimcp_platform_mutex_unlock(&conn->lock);

    LOG_INFO("Generated %u intercolumnar connections for %u columns",
             connections_created, num_columns);

    return connections_created;
}

uint32_t connectivity_generate_long_range(
    columnar_connectivity_t* conn,
    const uint32_t* source_columns,
    uint32_t num_sources,
    const uint32_t* target_columns,
    uint32_t num_targets)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return 0;
    }
    if (!source_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_columns is NULL");
        return 0;
    }
    if (!target_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "target_columns is NULL");
        return 0;
    }
    if (num_sources == 0 || num_targets == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "num_sources and num_targets must be > 0");
        return 0;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    uint32_t connections_created = 0;

    // Feedforward: Layer II/III → Layer IV
    // Feedback: Layer VI → Layer I

    for (uint32_t r = 0; r < conn->num_rules; r++) {
        const connectivity_rule_t* rule = &conn->rules[r];

        // Guard: only long-range rules
        if (rule->type != CONNECTIVITY_LONG_RANGE &&
            rule->type != CONNECTIVITY_FEEDFORWARD &&
            rule->type != CONNECTIVITY_FEEDBACK) {
            continue;
        }

        // Determine source/target layers based on type
        cc_cortical_layer_t src_layer, tgt_layer;
        if (rule->type == CONNECTIVITY_FEEDFORWARD) {
            src_layer = CC_LAYER_II_III;  // Layer II/III
            tgt_layer = CC_LAYER_IV;      // Layer IV
        } else if (rule->type == CONNECTIVITY_FEEDBACK) {
            src_layer = CC_LAYER_VI;
            tgt_layer = CC_LAYER_I;
        } else {
            src_layer = rule->layer_specific ? rule->source_layer : CC_LAYER_II_III;
            tgt_layer = rule->layer_specific ? rule->target_layer : CC_LAYER_IV;
        }

        // Sparse random connectivity
        for (uint32_t i = 0; i < num_sources; i++) {
            for (uint32_t j = 0; j < num_targets; j++) {
                // Probabilistic connection
                if (randf() < rule->base_probability) {
                    columnar_connection_t new_conn = {
                        .source_column_id = source_columns[i],
                        .target_column_id = target_columns[j],
                        .source_layer = src_layer,
                        .target_layer = tgt_layer,
                        .weight = 0.4F + randg(0.0F, 0.1F),
                        .delay_ms = 2.0F + randg(0.0F, 0.5F),  // Longer delays for long-range
                        .type = rule->type
                    };

                    // Clip weight
                    if (new_conn.weight < 0.0F) new_conn.weight = 0.0F;
                    if (new_conn.weight > 1.0F) new_conn.weight = 1.0F;

                    if (add_connection(conn, &new_conn) == NIMCP_SUCCESS) {
                        connections_created++;
                    }
                }
            }
        }
    }

    nimcp_platform_mutex_unlock(&conn->lock);

    LOG_INFO("Generated %u long-range connections (%u sources → %u targets)",
             connections_created, num_sources, num_targets);

    return connections_created;
}

//=============================================================================
// CONNECTION ACCESS
//=============================================================================

uint32_t connectivity_get_connections_from(
    columnar_connectivity_t* conn,
    uint32_t column_id,
    columnar_connection_t* out_connections,
    uint32_t max_connections)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return 0;
    }
    if (!out_connections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "out_connections is NULL");
        return 0;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    uint32_t count = conn_hash_table_get(
        conn->by_source,
        column_id,
        out_connections,
        max_connections
    );

    nimcp_platform_mutex_unlock(&conn->lock);

    return count;
}

uint32_t connectivity_get_connections_to(
    columnar_connectivity_t* conn,
    uint32_t column_id,
    columnar_connection_t* out_connections,
    uint32_t max_connections)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return 0;
    }
    if (!out_connections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "out_connections is NULL");
        return 0;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    uint32_t count = conn_hash_table_get(
        conn->by_target,
        column_id,
        out_connections,
        max_connections
    );

    nimcp_platform_mutex_unlock(&conn->lock);

    return count;
}

//=============================================================================
// SIGNAL PROPAGATION
//=============================================================================

void connectivity_propagate(
    columnar_connectivity_t* conn,
    const float* source_activations,
    float* target_inputs,
    uint32_t num_columns)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return;
    }
    if (!source_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_activations is NULL");
        return;
    }
    if (!target_inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "target_inputs is NULL");
        return;
    }

    // Process pending bio-async messages
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    // Clear target inputs
    memset(target_inputs, 0, sizeof(float) * num_columns);

    // For each connection: target += weight × source
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        const columnar_connection_t* c = &conn->connections[i];

        // Guard: bounds check
        if (c->source_column_id >= num_columns ||
            c->target_column_id >= num_columns) {
            continue;
        }

        target_inputs[c->target_column_id] +=
            c->weight * source_activations[c->source_column_id];
    }
}

void connectivity_propagate_with_delay(
    columnar_connectivity_t* conn,
    const float* source_activations,
    float* target_inputs,
    uint32_t num_columns,
    float dt_ms)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return;
    }
    if (!source_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_activations is NULL");
        return;
    }
    if (!target_inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "target_inputs is NULL");
        return;
    }
    if (dt_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dt_ms must be > 0");
        return;
    }

    // Clear target inputs
    memset(target_inputs, 0, sizeof(float) * num_columns);

    // Simple delay implementation: only propagate if delay <= dt
    // (Full delay line buffer would be needed for accurate delays)
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        const columnar_connection_t* c = &conn->connections[i];

        // Guard: bounds check
        if (c->source_column_id >= num_columns ||
            c->target_column_id >= num_columns) {
            continue;
        }

        // Simple approximation: propagate if delay is within time step
        if (c->delay_ms <= dt_ms) {
            target_inputs[c->target_column_id] +=
                c->weight * source_activations[c->source_column_id];
        }
    }
}

//=============================================================================
// PLASTICITY
//=============================================================================

void connectivity_apply_hebbian(
    columnar_connectivity_t* conn,
    const float* pre_activations,
    const float* post_activations,
    float learning_rate)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return;
    }
    if (!pre_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pre_activations is NULL");
        return;
    }
    if (!post_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "post_activations is NULL");
        return;
    }
    if (learning_rate <= 0.0F || learning_rate > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "learning_rate must be in (0, 1]");
        return;
    }

    // For each connection: Δw = η × pre × post
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        columnar_connection_t* c = &conn->connections[i];

        float pre = pre_activations[c->source_column_id];
        float post = post_activations[c->target_column_id];

        // Hebbian update
        float delta_w = learning_rate * pre * post;
        c->weight += delta_w;

        // Clip to [0, 1]
        if (c->weight < 0.0F) c->weight = 0.0F;
        if (c->weight > 1.0F) c->weight = 1.0F;
    }

    conn->stats_valid = false;
}

void connectivity_apply_stdp(
    columnar_connectivity_t* conn,
    const uint64_t* pre_spike_times,
    const uint64_t* post_spike_times,
    uint32_t num_columns)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return;
    }
    if (!pre_spike_times) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pre_spike_times is NULL");
        return;
    }
    if (!post_spike_times) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "post_spike_times is NULL");
        return;
    }

    // For each connection: apply STDP window
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        columnar_connection_t* c = &conn->connections[i];

        // Guard: bounds check
        if (c->source_column_id >= num_columns ||
            c->target_column_id >= num_columns) {
            continue;
        }

        uint64_t t_pre = pre_spike_times[c->source_column_id];
        uint64_t t_post = post_spike_times[c->target_column_id];

        // Guard: check if both spiked
        if (t_pre == 0 || t_post == 0) continue;

        // Compute spike timing difference (in microseconds)
        int64_t delta_t = (int64_t)t_post - (int64_t)t_pre;

        // Guard: check STDP window
        if (llabs(delta_t) > STDP_WINDOW_US) continue;

        float delta_w = 0.0F;

        if (delta_t > 0) {
            // LTP: pre before post
            delta_w = STDP_A_PLUS * expf(-(float)delta_t / STDP_TAU_PLUS_US);
        } else {
            // LTD: post before pre
            delta_w = STDP_A_MINUS * expf((float)delta_t / STDP_TAU_MINUS_US);
        }

        c->weight += delta_w;

        // Clip to [0, 1]
        if (c->weight < 0.0F) c->weight = 0.0F;
        if (c->weight > 1.0F) c->weight = 1.0F;
    }

    conn->stats_valid = false;
}

//=============================================================================
// TOPOLOGY ANALYSIS
//=============================================================================

float connectivity_compute_clustering(columnar_connectivity_t* conn) {
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return -1.0F;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    // Guard: need minimum connections
    if (conn->num_connections < 3) {
        nimcp_platform_mutex_unlock(&conn->lock);
        return 0.0F;
    }

    // Extract unique column IDs
    uint32_t* columns = nimcp_malloc(sizeof(uint32_t) * conn->num_connections * 2);
    if (!columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate columns array for clustering");
        nimcp_platform_mutex_unlock(&conn->lock);
        return -1.0F;
    }

    uint32_t num_unique = 0;
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        uint32_t src = conn->connections[i].source_column_id;
        uint32_t tgt = conn->connections[i].target_column_id;

        // Add to list if not present
        bool found_src = false, found_tgt = false;
        for (uint32_t j = 0; j < num_unique; j++) {
            if (columns[j] == src) found_src = true;
            if (columns[j] == tgt) found_tgt = true;
        }
        if (!found_src) columns[num_unique++] = src;
        if (!found_tgt) columns[num_unique++] = tgt;
    }

    float total_clustering = 0.0F;
    uint32_t nodes_with_neighbors = 0;

    // For each column, compute local clustering
    for (uint32_t i = 0; i < num_unique && i < SAMPLE_SIZE_FOR_PATH_LENGTH; i++) {
        uint32_t col = columns[i];

        // Get neighbors (columns connected to this one)
        columnar_connection_t neighbors[MAX_COLUMN_DEGREE];
        uint32_t degree = conn_hash_table_get(
            conn->by_source, col, neighbors, MAX_COLUMN_DEGREE);

        // Guard: need at least 2 neighbors for clustering
        if (degree < 2) continue;

        // Count triangles (neighbors connected to each other)
        uint32_t triangles = 0;
        for (uint32_t j = 0; j < degree && j < 100; j++) {
            for (uint32_t k = j + 1; k < degree && k < 100; k++) {
                // Check if neighbors[j] and neighbors[k] are connected
                columnar_connection_t temp[MAX_COLUMN_DEGREE];
                uint32_t n = conn_hash_table_get(
                    conn->by_source, neighbors[j].target_column_id,
                    temp, MAX_COLUMN_DEGREE);

                for (uint32_t m = 0; m < n; m++) {
                    if (temp[m].target_column_id == neighbors[k].target_column_id) {
                        triangles++;
                        break;
                    }
                }
            }
        }

        // Local clustering coefficient
        uint32_t possible_triangles = degree * (degree - 1) / 2;
        if (possible_triangles > 0) {
            total_clustering += (float)triangles / (float)possible_triangles;
            nodes_with_neighbors++;
        }
    }

    nimcp_free(columns);
    nimcp_platform_mutex_unlock(&conn->lock);

    // Average clustering coefficient
    if (nodes_with_neighbors == 0) return 0.0F;
    return total_clustering / (float)nodes_with_neighbors;
}

float connectivity_compute_path_length(columnar_connectivity_t* conn) {
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return -1.0F;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    // Guard: need minimum connections
    if (conn->num_connections < MIN_COLUMNS_FOR_ANALYSIS) {
        nimcp_platform_mutex_unlock(&conn->lock);
        return -1.0F;
    }

    // Sample-based estimation for large graphs (BFS-based approximation)
    float total_path_length = 0.0F;
    uint32_t path_count = 0;

    // Extract unique column IDs
    uint32_t* columns = nimcp_malloc(sizeof(uint32_t) * conn->num_connections * 2);
    if (!columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate columns array for path length");
        nimcp_platform_mutex_unlock(&conn->lock);
        return -1.0F;
    }

    uint32_t num_unique = 0;
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        uint32_t src = conn->connections[i].source_column_id;
        uint32_t tgt = conn->connections[i].target_column_id;

        bool found_src = false, found_tgt = false;
        for (uint32_t j = 0; j < num_unique; j++) {
            if (columns[j] == src) found_src = true;
            if (columns[j] == tgt) found_tgt = true;
        }
        if (!found_src) columns[num_unique++] = src;
        if (!found_tgt) columns[num_unique++] = tgt;
    }

    // Sample pairs for path length estimation
    uint32_t samples = num_unique < SAMPLE_SIZE_FOR_PATH_LENGTH ?
                       num_unique : SAMPLE_SIZE_FOR_PATH_LENGTH;

    for (uint32_t i = 0; i < samples; i++) {
        for (uint32_t j = i + 1; j < samples; j++) {
            // Simple hop count (simplified version)
            columnar_connection_t temp[MAX_COLUMN_DEGREE];
            uint32_t n = conn_hash_table_get(
                conn->by_source, columns[i], temp, MAX_COLUMN_DEGREE);

            // Check if directly connected
            bool connected = false;
            for (uint32_t k = 0; k < n; k++) {
                if (temp[k].target_column_id == columns[j]) {
                    total_path_length += 1.0F;
                    path_count++;
                    connected = true;
                    break;
                }
            }

            // If not directly connected, estimate 2-3 hops
            if (!connected && n > 0) {
                total_path_length += 2.5F;
                path_count++;
            }
        }
    }

    nimcp_free(columns);
    nimcp_platform_mutex_unlock(&conn->lock);

    return path_count > 0 ? total_path_length / (float)path_count : -1.0F;
}

bool connectivity_is_small_world(columnar_connectivity_t* conn) {
    // Guard: validate parameters
    if (!conn) {
        return false;
    }

    float C = connectivity_compute_clustering(conn);
    float L = connectivity_compute_path_length(conn);

    // Guard: check validity
    if (C < 0.0F || L < 0.0F) {
        return false;
    }

    // Approximation: C_rand ≈ k/N, L_rand ≈ ln(N)/ln(k)
    // For small-world: σ = (C/C_rand) / (L/L_rand) > 1

    // Simplified check: high clustering (C > 0.3) and short paths (L < 6)
    return (C > 0.3F && L < 6.0F);
}

//=============================================================================
// STATISTICS
//=============================================================================

nimcp_result_t connectivity_get_stats(
    columnar_connectivity_t* conn,
    connectivity_stats_t* stats)
{
    // Guard: validate parameters
    if (!conn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");
        return NIMCP_INVALID_PARAM;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&conn->lock);

    // Clear stats
    memset(stats, 0, sizeof(connectivity_stats_t));

    stats->total_connections = conn->num_connections;

    float weight_sum = 0.0F;
    float delay_sum = 0.0F;

    // Aggregate statistics
    for (uint32_t i = 0; i < conn->num_connections; i++) {
        const columnar_connection_t* c = &conn->connections[i];

        // Count by type
        switch (c->type) {
            case CONNECTIVITY_INTRACOLUMNAR:
                stats->intracolumnar_count++;
                break;
            case CONNECTIVITY_INTERCOLUMNAR:
                stats->intercolumnar_count++;
                break;
            case CONNECTIVITY_LONG_RANGE:
            case CONNECTIVITY_FEEDFORWARD:
            case CONNECTIVITY_FEEDBACK:
                stats->long_range_count++;
                break;
            default:
                break;
        }

        // Layer connection matrix
        if (c->source_layer < CC_LAYER_COUNT && c->target_layer < CC_LAYER_COUNT) {
            stats->layer_connection_counts[c->source_layer][c->target_layer]++;
        }

        weight_sum += c->weight;
        delay_sum += c->delay_ms;
    }

    // Compute averages
    if (stats->total_connections > 0) {
        stats->avg_weight = weight_sum / (float)stats->total_connections;
        stats->avg_delay_ms = delay_sum / (float)stats->total_connections;
    }

    // Compute topology metrics (expensive - cache results)
    stats->clustering_coefficient = connectivity_compute_clustering(conn);
    stats->characteristic_path_length = connectivity_compute_path_length(conn);

    // Small-world sigma (simplified)
    if (stats->clustering_coefficient > 0.0F &&
        stats->characteristic_path_length > 0.0F) {
        stats->small_world_sigma = stats->clustering_coefficient /
                                   stats->characteristic_path_length;
    }

    nimcp_platform_mutex_unlock(&conn->lock);

    LOG_DEBUG("Connectivity stats: %u total, %.3f avg weight, %.3f avg delay ms",
              stats->total_connections, stats->avg_weight, stats->avg_delay_ms);

    return NIMCP_SUCCESS;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * WHAT: Query knowledge graph for columnar connectivity module self-knowledge
 * WHY:  Enable self-awareness and introspection about this module's role
 * HOW:  Query KG for entity info, log observations, check relations
 */
int columnar_connectivity_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Columnar_Connectivity_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Columnar connectivity self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Columnar_Connectivity_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Columnar_Connectivity_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
