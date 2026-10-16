/**
 * @file nimcp_glial_integration.c
 * @brief Implementation of glial-neuron integration
 *
 * FEATURES:
 * - Bio-async messaging for glial-neural coordination
 * - Comprehensive logging for debugging and monitoring
 * - Tripartite synapse integration
 * - Myelination coordination
 * - Synaptic surveillance and pruning
 *
 * @version 2.0.0
 * @date 2025-11-28
 */

#include "glial/integration/nimcp_glial_integration.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "GLIAL_INTEGRATION"

#include <stddef.h>  /* for NULL */
// ============================================================================
// Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
// ============================================================================
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(glial_integration)

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Dynamic array for storing lists of IDs in reverse mappings
 */
/**
 * G7: synapse_id is now uint64 to survive 2M-scale neuron counts.
 * Prior `pre*10000+post` overflowed uint32 at pre >= 429,497 and collided
 * deterministically when post >= 10000 (e.g. (pre=1,post=20000) ==
 * (pre=3,post=0)). The new packing `(pre << 32) | post` is bijective over
 * (uint32, uint32) pairs — zero collisions.
 *
 * id_list_t stores uint64 too so it can back both the synapse-reverse
 * lookups (uint64 synapse_ids) and the neuron-reverse lookup (uint32
 * neuron_ids trivially widen to uint64). 4 bytes/entry memory overhead
 * is negligible at these scales.
 */
typedef struct {
    uint64_t* ids;      // Array of IDs (uint64 holds synapse_ids and neuron_ids)
    uint32_t count;     // Number of IDs
    uint32_t capacity;  // Allocated capacity
} id_list_t;

static id_list_t* id_list_create(void) {
    id_list_t* list = (id_list_t*)nimcp_malloc(sizeof(id_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->capacity = 8;  // Initial capacity
    list->count = 0;
    list->ids = (uint64_t*)nimcp_malloc(sizeof(uint64_t) * list->capacity);
    if (!list->ids) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "id_list_create: list->ids is NULL");
        return NULL;
    }

    return list;
}

static void id_list_destroy(id_list_t* list) {
    if (!list) return;
    if (list->ids) nimcp_free(list->ids);
    nimcp_free(list);
}

// Value destructor for hash tables storing id_list_t*
static void id_list_value_destructor(void* value, size_t value_size) {
    (void)value_size;  // Unused parameter
    if (value) {
        id_list_t* list = *(id_list_t**)value;
        id_list_destroy(list);
    }
}


static bool id_list_add(id_list_t* list, uint64_t id) {
    if (!list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "id_list_add: list is NULL");
        return false;
    }

    // Check if already exists
    for (uint32_t i = 0; i < list->count; i++) {
        if (list->ids[i] == id) {
            return true; // Already exists, not an error
        }
    }

    // Expand if needed
    if (list->count >= list->capacity) {
        uint32_t new_capacity = list->capacity * 2;
        uint64_t* new_ids = (uint64_t*)nimcp_realloc(list->ids, sizeof(uint64_t) * new_capacity);
        if (!new_ids) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "id_list_add: new_ids is NULL");
            return false;
        }

        list->ids = new_ids;
        list->capacity = new_capacity;
    }

    list->ids[list->count++] = id;
    return true;
}

/**
 * @brief Generate synapse ID from pre/post neuron IDs.
 *
 * G7: Bit-packed uint64 `(pre << 32) | post`. Bijective over (uint32,
 * uint32) pairs — zero collisions at any brain scale. Replaces the old
 * `pre*10000 + post` which overflowed + collided at 2M neurons.
 */
static inline uint64_t make_synapse_id(uint32_t pre_neuron_id, uint32_t post_neuron_id) {
    return ((uint64_t)pre_neuron_id << 32) | (uint64_t)post_neuron_id;
}

// ============================================================================
// CREATION & DESTRUCTION
// ============================================================================

glial_integration_t* glial_integration_create(neural_network_t network, uint32_t max_mappings) {
    // Validate parameters - max_mappings must be non-zero
    // Network can be NULL (glial cells can exist independently)
    if (max_mappings == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "glial_integration_create: max_mappings is zero");
        return NULL;
    }

    glial_integration_t* gi = (glial_integration_t*)nimcp_malloc(sizeof(glial_integration_t));
    NIMCP_API_CHECK_ALLOC(gi, "glial_integration_create: Failed to allocate structure");

    memset(gi, 0, sizeof(glial_integration_t));

    gi->network = network;

    // G7: synapse-keyed tables use UINT64 (synapse_id = packed (pre,post)).
    // Neuron-keyed tables stay UINT32 (neuron_id fits).
    hash_table_config_t forward_config_u32 = {  // neuron_id → glial_id
        .initial_buckets = max_mappings > 256 ? max_mappings : 256,
        .key_type = HASH_KEY_UINT32,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .custom_hash_fn = NULL,
        .custom_compare_fn = NULL,
        .value_destructor = NULL,
        .case_insensitive = false,
        .thread_safe = false
    };
    hash_table_config_t forward_config_u64 = {  // synapse_id → glial_id
        .initial_buckets = max_mappings > 256 ? max_mappings : 256,
        .key_type = HASH_KEY_UINT64,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .custom_hash_fn = NULL,
        .custom_compare_fn = NULL,
        .value_destructor = NULL,
        .case_insensitive = false,
        .thread_safe = false
    };

    // Config for reverse mappings (glial_id → id_list_t*). Keys are
    // glial cell IDs which are uint32.
    hash_table_config_t reverse_config = {
        .initial_buckets = max_mappings > 256 ? max_mappings : 256,
        .key_type = HASH_KEY_UINT32,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .custom_hash_fn = NULL,
        .custom_compare_fn = NULL,
        .value_destructor = id_list_value_destructor,  // Auto-cleanup
        .case_insensitive = false,
        .thread_safe = false
    };

    // Forward mappings (G7: synapse tables now uint64-keyed)
    gi->synapse_to_astrocyte = hash_table_create(&forward_config_u64);
    gi->neuron_to_oligodendrocyte = hash_table_create(&forward_config_u32);
    gi->synapse_to_microglia = hash_table_create(&forward_config_u64);

    // Reverse mappings
    gi->astrocyte_to_synapses = hash_table_create(&reverse_config);
    gi->oligodendrocyte_to_neurons = hash_table_create(&reverse_config);
    gi->microglia_to_synapses = hash_table_create(&reverse_config);

    if (!gi->synapse_to_astrocyte || !gi->neuron_to_oligodendrocyte ||
        !gi->synapse_to_microglia || !gi->astrocyte_to_synapses ||
        !gi->oligodendrocyte_to_neurons || !gi->microglia_to_synapses) {
        glial_integration_destroy(gi);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_integration_create: operation failed");
        return NULL;
    }

    // Initialize statistics
    gi->total_astrocyte_modulations = 0;
    gi->total_oligodendrocyte_myelinations = 0;
    gi->total_microglia_prunings = 0;

    // Initialize timing
    gi->last_update_timestamp_us = 0;

    // Default: all features disabled
    gi->enable_astrocyte_modulation = false;
    gi->enable_oligodendrocyte_myelination = false;
    gi->enable_microglia_pruning = false;
    gi->enable_bio_async = false;

    // Bio-async initialization
    gi->bio_ctx = NULL;
    gi->bio_async_enabled = false;

    // Initialize lock
    nimcp_mutex_init(&gi->lock, NULL);

    LOG_INFO(LOG_MODULE, "Glial integration created with max_mappings=%u", max_mappings);

    return gi;
}

void glial_integration_destroy(glial_integration_t* gi) {
    if (!gi) return;

    LOG_INFO(LOG_MODULE, "Destroying glial integration");

    // Unregister bio-async
    if (gi->bio_async_enabled && gi->bio_ctx) {
        bio_router_unregister_module(gi->bio_ctx);
        gi->bio_ctx = NULL;
        gi->bio_async_enabled = false;
        LOG_INFO(LOG_MODULE, "Bio-async unregistered");
    }

    if (gi->synapse_to_astrocyte) {
        hash_table_destroy(gi->synapse_to_astrocyte);
    }
    if (gi->neuron_to_oligodendrocyte) {
        hash_table_destroy(gi->neuron_to_oligodendrocyte);
    }
    if (gi->synapse_to_microglia) {
        hash_table_destroy(gi->synapse_to_microglia);
    }
    if (gi->astrocyte_to_synapses) {
        hash_table_destroy(gi->astrocyte_to_synapses);
    }
    if (gi->oligodendrocyte_to_neurons) {
        hash_table_destroy(gi->oligodendrocyte_to_neurons);
    }
    if (gi->microglia_to_synapses) {
        hash_table_destroy(gi->microglia_to_synapses);
    }

    // Phase C2.1: Cleanup spatial neuromod system
    if (gi->spatial_neuromod) {
        spatial_neuromod_system_destroy(gi->spatial_neuromod);
    }

    nimcp_mutex_destroy(&gi->lock);
    nimcp_free(gi);
}

// ============================================================================
// GLIAL NETWORK ASSIGNMENT
// ============================================================================

nimcp_result_t glial_integration_set_astrocyte_network(glial_integration_t* gi,
                                                       astrocyte_network_t* astrocyte_network) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    gi->astrocyte_network = astrocyte_network;
    nimcp_mutex_unlock(&gi->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t glial_integration_set_oligodendrocyte_network(
    glial_integration_t* gi, oligodendrocyte_network_t* oligo_network) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    gi->oligodendrocyte_network = oligo_network;
    nimcp_mutex_unlock(&gi->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t glial_integration_set_microglia_network(glial_integration_t* gi,
                                                       microglia_network_t* microglia_network) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    gi->microglia_network = microglia_network;
    nimcp_mutex_unlock(&gi->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t glial_integration_set_spatial_neuromod_system(
    glial_integration_t* gi, spatial_neuromod_system_t* spatial_neuromod) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);

    // Cleanup old system if exists
    if (gi->spatial_neuromod) {
        spatial_neuromod_system_destroy(gi->spatial_neuromod);
    }

    gi->spatial_neuromod = spatial_neuromod;
    gi->enable_spatial_neuromod = (spatial_neuromod != NULL);

    nimcp_mutex_unlock(&gi->lock);

    return NIMCP_SUCCESS;
}

// ============================================================================
// GLIAL CELL ASSIGNMENT
// ============================================================================

nimcp_result_t glial_integration_assign_astrocyte_to_synapse(glial_integration_t* gi,
                                                             uint32_t astrocyte_id,
                                                             uint64_t synapse_id) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);

    // Forward mapping: synapse → astrocyte (G7: uint64 key)
    hash_table_insert_uint64(gi->synapse_to_astrocyte, synapse_id, &astrocyte_id, sizeof(uint32_t));

    // Reverse mapping: astrocyte → list of synapses
    id_list_t** list_ptr = (id_list_t**)hash_table_lookup_uint32(gi->astrocyte_to_synapses, astrocyte_id);
    id_list_t* list;

    if (!list_ptr) {
        // Create new list for this astrocyte
        list = id_list_create();
        if (!list) {
            nimcp_mutex_unlock(&gi->lock);
            return NIMCP_ERROR_MEMORY;
        }

        // Store pointer to list in hash table (hash table will copy the pointer value)
        hash_table_insert_uint32(gi->astrocyte_to_synapses, astrocyte_id, &list, sizeof(id_list_t*));
    } else {
        list = *list_ptr;
    }

    // Add synapse to list
    if (!id_list_add(list, synapse_id)) {
        nimcp_mutex_unlock(&gi->lock);
        return NIMCP_ERROR_MEMORY;
    }

    nimcp_mutex_unlock(&gi->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t glial_integration_assign_oligodendrocyte_to_neuron(glial_integration_t* gi,
                                                                  uint32_t oligo_id,
                                                                  uint32_t neuron_id) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);

    // Forward mapping: neuron → oligodendrocyte
    // Store the oligo_id directly (not a pointer to it)
    hash_table_insert_uint32(gi->neuron_to_oligodendrocyte, neuron_id, &oligo_id, sizeof(uint32_t));

    // Reverse mapping: oligodendrocyte → list of neurons
    id_list_t** list_ptr = (id_list_t**)hash_table_lookup_uint32(gi->oligodendrocyte_to_neurons, oligo_id);
    id_list_t* list;

    if (!list_ptr) {
        list = id_list_create();
        if (!list) {
            nimcp_mutex_unlock(&gi->lock);
            return NIMCP_ERROR_MEMORY;
        }

        // Store pointer to list in hash table (hash table will copy the pointer value)
        hash_table_insert_uint32(gi->oligodendrocyte_to_neurons, oligo_id, &list, sizeof(id_list_t*));
    } else {
        list = *list_ptr;
    }

    if (!id_list_add(list, neuron_id)) {
        nimcp_mutex_unlock(&gi->lock);
        return NIMCP_ERROR_MEMORY;
    }

    nimcp_mutex_unlock(&gi->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t glial_integration_assign_microglia_to_synapse(glial_integration_t* gi,
                                                             uint32_t microglia_id,
                                                             uint64_t synapse_id) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);

    // Forward mapping: synapse → microglia (G7: uint64 key)
    hash_table_insert_uint64(gi->synapse_to_microglia, synapse_id, &microglia_id, sizeof(uint32_t));

    // Reverse mapping: microglia → list of synapses
    id_list_t** list_ptr = (id_list_t**)hash_table_lookup_uint32(gi->microglia_to_synapses, microglia_id);
    id_list_t* list;

    if (!list_ptr) {
        list = id_list_create();
        if (!list) {
            nimcp_mutex_unlock(&gi->lock);
            return NIMCP_ERROR_MEMORY;
        }

        // Store pointer to list in hash table (hash table will copy the pointer value)
        hash_table_insert_uint32(gi->microglia_to_synapses, microglia_id, &list, sizeof(id_list_t*));
    } else {
        list = *list_ptr;
    }

    if (!id_list_add(list, synapse_id)) {
        nimcp_mutex_unlock(&gi->lock);
        return NIMCP_ERROR_MEMORY;
    }

    // Also add to microglia's monitored synapse list
    if (gi->microglia_network && microglia_id < gi->microglia_network->num_microglia) {
        microglia_t* mg = gi->microglia_network->microglia[microglia_id];
        if (mg) {
            microglia_monitor_synapse(mg, synapse_id);
        }
    }

    nimcp_mutex_unlock(&gi->lock);
    return NIMCP_SUCCESS;
}

// ============================================================================
// AUTOMATIC SPATIAL ASSIGNMENT
// ============================================================================

uint32_t glial_integration_auto_assign_spatial(glial_integration_t* gi) {
    /**
     * @brief WHAT: Automatically assign glial cells to neurons/synapses by spatial proximity
     * @brief WHY:  Setup tripartite synapses without manual assignment
     * @brief HOW:  For each astrocyte, find synapses within coverage radius and assign
     *
     * ALGORITHM:
     * 1. For each astrocyte in network:
     *    a. Get its spatial position (x, y, z) and coverage radius
     *    b. Find all neurons within radius (using spatial queries)
     *    c. Assign synapses connected to those neurons → this astrocyte
     * 2. For each oligodendrocyte:
     *    a. Find neurons within myelination radius
     *    b. Assign neurons → this oligodendrocyte (up to capacity limit)
     * 3. For each microglia:
     *    a. Find synapses within surveillance radius
     *    b. Assign synapses → this microglia for monitoring
     *
     * BIOLOGICAL RATIONALE:
     * - Astrocytes cover local synaptic domains (~100,000 synapses, 50-100 µm radius)
     * - Oligodendrocytes myelinate nearby axons (capacity: ~40-50 axons each)
     * - Microglia patrol local territories (surveillance radius: ~20-50 µm)
     */
    if (!gi) return 0;

    uint32_t total_assignments = 0;

    nimcp_mutex_lock(&gi->lock);

    // ========================================================================
    // STEP 1: Assign Astrocytes to Synapses (Tripartite Synapse Model)
    // ========================================================================
    if (gi->astrocyte_network && gi->astrocyte_network->num_astrocytes > 0) {
        astrocyte_network_t* astro_net = gi->astrocyte_network;

        for (uint32_t i = 0; i < astro_net->num_astrocytes; i++) {
            astrocyte_t* astro = astro_net->astrocytes[i];
            if (!astro) continue;

            // For now, use simple round-robin assignment
            // TODO: Once spatial index is available, use proximity-based assignment
            //
            // In a full implementation, this would:
            // 1. Query spatial index for all neurons within astro->coverage_radius of (astro->x, astro->y, astro->z)
            // 2. For each nearby neuron, get its synapses
            // 3. Assign those synapses to this astrocyte
            //
            // Current stub: Assign synapse IDs based on coverage area estimate
            // Assume network has ~10,000 synapses, each astrocyte covers ~100 synapses
            uint32_t synapses_per_astrocyte = 100;
            uint32_t start_synapse = i * synapses_per_astrocyte;
            uint32_t end_synapse = start_synapse + synapses_per_astrocyte;

            for (uint32_t syn_id = start_synapse; syn_id < end_synapse; syn_id++) {
                nimcp_result_t result = glial_integration_assign_astrocyte_to_synapse(gi, i, syn_id);
                if (result == NIMCP_SUCCESS) {
                    total_assignments++;
                }
            }
        }
    }

    // ========================================================================
    // STEP 2: Assign Oligodendrocytes to Neurons (Myelination)
    // ========================================================================
    if (gi->oligodendrocyte_network && gi->oligodendrocyte_network->num_oligodendrocytes > 0) {
        oligodendrocyte_network_t* oligo_net = gi->oligodendrocyte_network;

        for (uint32_t i = 0; i < oligo_net->num_oligodendrocytes; i++) {
            // Each oligodendrocyte can myelinate ~40-50 neurons (biological limit)
            uint32_t neurons_per_oligo = 45;
            uint32_t start_neuron = i * neurons_per_oligo;
            uint32_t end_neuron = start_neuron + neurons_per_oligo;

            for (uint32_t neuron_id = start_neuron; neuron_id < end_neuron; neuron_id++) {
                nimcp_result_t result = glial_integration_assign_oligodendrocyte_to_neuron(gi, i, neuron_id);
                if (result == NIMCP_SUCCESS) {
                    total_assignments++;
                }
            }
        }
    }

    // ========================================================================
    // STEP 3: Assign Microglia to Synapses (Synaptic Surveillance)
    // ========================================================================
    if (gi->microglia_network && gi->microglia_network->num_microglia > 0) {
        microglia_network_t* micro_net = gi->microglia_network;

        for (uint32_t i = 0; i < micro_net->num_microglia; i++) {
            // Each microglia monitors ~50-200 synapses in its territory
            uint32_t synapses_per_microglia = 100;
            uint32_t start_synapse = i * synapses_per_microglia;
            uint32_t end_synapse = start_synapse + synapses_per_microglia;

            for (uint32_t syn_id = start_synapse; syn_id < end_synapse; syn_id++) {
                nimcp_result_t result = glial_integration_assign_microglia_to_synapse(gi, i, syn_id);
                if (result == NIMCP_SUCCESS) {
                    total_assignments++;
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);

    return total_assignments;
}

// ============================================================================
// EVENT NOTIFICATIONS
// ============================================================================

void glial_integration_on_synapse_fired(glial_integration_t* gi, uint32_t pre_neuron_id,
                                        uint32_t post_neuron_id, float synaptic_weight,
                                        uint64_t timestamp) {
    if (!gi) return;

    uint64_t synapse_id = make_synapse_id(pre_neuron_id, post_neuron_id);

    nimcp_mutex_lock(&gi->lock);

    // Find assigned astrocyte
    if (gi->enable_astrocyte_modulation && gi->astrocyte_network) {
        uint32_t* astrocyte_id_ptr = (uint32_t*)hash_table_lookup_uint64(
            gi->synapse_to_astrocyte, synapse_id);

        if (astrocyte_id_ptr) {
            uint32_t astrocyte_id = *astrocyte_id_ptr;
            if (astrocyte_id < gi->astrocyte_network->num_astrocytes) {
                astrocyte_t* ast = gi->astrocyte_network->astrocytes[astrocyte_id];
                if (ast) {
                    // Increase calcium with external stimulus
                    // dt = 0.001s (1ms), stimulus = synaptic_weight * 10
                    astrocyte_update_calcium(ast, 0.001F, synaptic_weight * 10.0F);
                }
            }
        }
    }

    // Notify microglia if assigned
    if (gi->enable_microglia_pruning && gi->microglia_network) {
        uint32_t* microglia_id_ptr = (uint32_t*)hash_table_lookup_uint64(
            gi->synapse_to_microglia, synapse_id);

        if (microglia_id_ptr) {
            uint32_t microglia_id = *microglia_id_ptr;
            if (microglia_id < gi->microglia_network->num_microglia) {
                microglia_t* mg = gi->microglia_network->microglia[microglia_id];
                if (mg) {
                    // Track synapse activity
                    microglia_track_synapse_activity(mg, synapse_id, synaptic_weight, timestamp);
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
}

void glial_integration_on_neuron_fired(glial_integration_t* gi, uint32_t neuron_id,
                                       uint64_t timestamp) {
    if (!gi || !gi->enable_oligodendrocyte_myelination) return;

    nimcp_mutex_lock(&gi->lock);

    // Find assigned oligodendrocyte
    if (gi->oligodendrocyte_network) {
        uint32_t* oligo_id_ptr = (uint32_t*)hash_table_lookup_uint32(
            gi->neuron_to_oligodendrocyte, neuron_id);

        if (oligo_id_ptr) {
            uint32_t oligo_id = *oligo_id_ptr;
            if (oligo_id < gi->oligodendrocyte_network->num_oligodendrocytes) {
                oligodendrocyte_t* oligo = gi->oligodendrocyte_network->oligodendrocytes[oligo_id];
                if (oligo) {
                    // Track axon activity for adaptive myelination
                    oligodendrocyte_track_activity(oligo, neuron_id, 1.0F, timestamp);
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
}

// ============================================================================
// GLIAL MODULATION QUERIES
// ============================================================================

float glial_integration_get_synaptic_modulation(glial_integration_t* gi, uint32_t pre_neuron_id,
                                                 uint32_t post_neuron_id) {
    if (!gi || !gi->enable_astrocyte_modulation) {
        return 1.0F; // Neutral modulation
    }

    uint64_t synapse_id = make_synapse_id(pre_neuron_id, post_neuron_id);

    nimcp_mutex_lock(&gi->lock);

    // Find assigned astrocyte
    if (gi->astrocyte_network) {
        uint32_t* astrocyte_id_ptr = (uint32_t*)hash_table_lookup_uint64(
            gi->synapse_to_astrocyte, synapse_id);

        if (astrocyte_id_ptr) {
            uint32_t astrocyte_id = *astrocyte_id_ptr;
            if (astrocyte_id < gi->astrocyte_network->num_astrocytes) {
                astrocyte_t* ast = gi->astrocyte_network->astrocytes[astrocyte_id];
                if (ast) {
                    // Get modulation factor based on glutamate pool
                    float glutamate = ast->glutamate_pool;
                    float modulation = 1.0F + (glutamate / 100.0F); // Simple linear mapping

                    // Clamp to range
                    if (modulation < 0.8F) modulation = 0.8F;
                    if (modulation > 1.2F) modulation = 1.2F;

                    nimcp_mutex_unlock(&gi->lock);
                    return modulation;
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
    return 1.0F; // No astrocyte → neutral
}

float glial_integration_get_myelination_factor(glial_integration_t* gi, uint32_t neuron_id) {
    if (!gi || !gi->enable_oligodendrocyte_myelination) {
        return 0.0F; // No myelination
    }

    nimcp_mutex_lock(&gi->lock);

    // Find assigned oligodendrocyte
    if (gi->oligodendrocyte_network) {
        uint32_t* oligo_id_ptr = (uint32_t*)hash_table_lookup_uint32(
            gi->neuron_to_oligodendrocyte, neuron_id);

        if (oligo_id_ptr) {
            uint32_t oligo_id = *oligo_id_ptr;
            if (oligo_id < gi->oligodendrocyte_network->num_oligodendrocytes) {
                oligodendrocyte_t* oligo = gi->oligodendrocyte_network->oligodendrocytes[oligo_id];
                if (oligo) {
                    // Get myelination level for this neuron/axon
                    float myelination = oligodendrocyte_get_myelination_level(oligo, neuron_id);
                    nimcp_mutex_unlock(&gi->lock);
                    return myelination;
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
    return 0.0F; // No oligodendrocyte
}

bool glial_integration_should_prune_synapse(glial_integration_t* gi, uint32_t pre_neuron_id,
                                            uint32_t post_neuron_id) {
    if (!gi) {
        return false;
    }
    if (!gi->enable_microglia_pruning) {
        return false;  /* Pruning disabled - normal config */
    }

    uint64_t synapse_id = make_synapse_id(pre_neuron_id, post_neuron_id);

    nimcp_mutex_lock(&gi->lock);

    // Find assigned microglia
    if (gi->microglia_network) {
        uint32_t* microglia_id_ptr = (uint32_t*)hash_table_lookup_uint64(
            gi->synapse_to_microglia, synapse_id);

        if (microglia_id_ptr) {
            uint32_t microglia_id = *microglia_id_ptr;
            if (microglia_id < gi->microglia_network->num_microglia) {
                microglia_t* mg = gi->microglia_network->microglia[microglia_id];
                if (mg) {
                    // Check if activity score is below pruning threshold
                    float activity_score = microglia_get_synapse_activity_score(mg, synapse_id);
                    bool should_prune = (activity_score < mg->pruning_threshold);
                    nimcp_mutex_unlock(&gi->lock);
                    return should_prune;
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
    return false;  /* No microglia found for synapse - normal search miss */
}

// ============================================================================
// SIMULATION STEP
// ============================================================================

void glial_integration_step(glial_integration_t* gi, uint64_t timestamp) {
    if (!gi) return;

    /* Phase 8: Send heartbeat at start of integration step */
    glial_integration_heartbeat("glial_integration_step", 0.0f);

    nimcp_mutex_lock(&gi->lock);

    // Compute timestep in milliseconds (convert from microseconds)
    float dt_ms;
    if (gi->last_update_timestamp_us == 0 || timestamp <= gi->last_update_timestamp_us) {
        dt_ms = 1.0F;  // Assume 1ms for first call or backward timestamp
    } else {
        uint64_t dt_us = timestamp - gi->last_update_timestamp_us;
        dt_ms = (float)dt_us / 1000.0F;  // Convert µs to ms
    }
    if (timestamp > gi->last_update_timestamp_us) {
        gi->last_update_timestamp_us = timestamp;
    }

    // Synchronize network time with glial time (for STP and other timing-dependent features)
    if (gi->network) {
        neural_network_set_time(gi->network, timestamp);
    }

    /* Phase 8: Send heartbeat after setup (20% progress) */
    glial_integration_heartbeat("glial_integration_step", 0.2f);

    // Step astrocyte network (calcium dynamics, glutamate release)
    if (gi->astrocyte_network && gi->enable_astrocyte_modulation) {
        astrocyte_network_step(gi->astrocyte_network, dt_ms);
    }

    /* Phase 8: Send heartbeat after astrocyte step (40% progress) */
    glial_integration_heartbeat("glial_integration_step", 0.4f);

    // Step oligodendrocyte network (adaptive myelination)
    if (gi->oligodendrocyte_network && gi->enable_oligodendrocyte_myelination) {
        oligodendrocyte_network_step(gi->oligodendrocyte_network, dt_ms);
    }

    /* Phase 8: Send heartbeat after oligodendrocyte step (60% progress) */
    glial_integration_heartbeat("glial_integration_step", 0.6f);

    // Step microglia network (activity scoring, pruning)
    if (gi->microglia_network && gi->enable_microglia_pruning) {
        microglia_network_step(gi->microglia_network, timestamp);
    }

    /* Phase 8: Send heartbeat after microglia step (80% progress) */
    glial_integration_heartbeat("glial_integration_step", 0.8f);

    // Part A2.1: Step spatial neuromodulator diffusion (DA, 5-HT, ACh, NE)
    // PHASE C4.6: Multi-objective optimization integrated here
    if (gi->spatial_neuromod && gi->enable_spatial_neuromod && gi->network) {
        // Use same dt_ms as astrocyte/oligodendrocyte systems (already computed above)
        // Update all enabled neuromodulator fields in one call
        // Supports all Phase C4.x features: quantum-Shannon, adaptive routing,
        // dynamic adaptation, and multi-objective Pareto optimization
        spatial_neuromod_system_update(gi->spatial_neuromod, gi->network, dt_ms);
        gi->total_neuromod_updates++;
    }

    nimcp_mutex_unlock(&gi->lock);
}

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

nimcp_result_t glial_integration_get_stats(glial_integration_t* gi,
                                           glial_integration_stats_t* stats) {
    if (!gi || !stats) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(glial_integration_stats_t));

    nimcp_mutex_lock(&gi->lock);

    // Count glial cells
    if (gi->astrocyte_network) {
        stats->num_astrocytes = gi->astrocyte_network->num_astrocytes;
    }
    if (gi->oligodendrocyte_network) {
        stats->num_oligodendrocytes = gi->oligodendrocyte_network->num_oligodendrocytes;
    }
    if (gi->microglia_network) {
        stats->num_microglia = gi->microglia_network->num_microglia;
    }

    // Count assignments from forward mappings
    stats->num_tripartite_synapses = (uint32_t)hash_table_size(gi->synapse_to_astrocyte);
    stats->num_myelinated_neurons = (uint32_t)hash_table_size(gi->neuron_to_oligodendrocyte);
    stats->num_monitored_synapses = (uint32_t)hash_table_size(gi->synapse_to_microglia);

    // Copy statistics
    stats->total_modulations = gi->total_astrocyte_modulations;
    stats->total_myelinations = gi->total_oligodendrocyte_myelinations;
    stats->total_prunings = gi->total_microglia_prunings;

    nimcp_mutex_unlock(&gi->lock);

    return NIMCP_SUCCESS;
}

uint32_t glial_integration_get_astrocyte_synapse_count(glial_integration_t* gi,
                                                       uint32_t astrocyte_id) {
    if (!gi) return 0;

    nimcp_mutex_lock(&gi->lock);

    id_list_t** list_ptr = (id_list_t**)hash_table_lookup_uint32(gi->astrocyte_to_synapses, astrocyte_id);
    uint32_t count = 0;

    if (list_ptr && *list_ptr) {
        count = (*list_ptr)->count;
    }

    nimcp_mutex_unlock(&gi->lock);
    return count;
}

uint32_t glial_integration_get_oligodendrocyte_neuron_count(glial_integration_t* gi,
                                                            uint32_t oligo_id) {
    if (!gi) return 0;

    nimcp_mutex_lock(&gi->lock);

    id_list_t** list_ptr = (id_list_t**)hash_table_lookup_uint32(gi->oligodendrocyte_to_neurons, oligo_id);
    uint32_t count = 0;

    if (list_ptr && *list_ptr) {
        count = (*list_ptr)->count;
    }

    nimcp_mutex_unlock(&gi->lock);
    return count;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void glial_integration_set_astrocyte_modulation_enabled(glial_integration_t* gi, bool enable) {
    if (!gi) return;

    nimcp_mutex_lock(&gi->lock);
    gi->enable_astrocyte_modulation = enable;
    nimcp_mutex_unlock(&gi->lock);
}

void glial_integration_set_oligodendrocyte_myelination_enabled(glial_integration_t* gi,
                                                               bool enable) {
    if (!gi) return;

    nimcp_mutex_lock(&gi->lock);
    gi->enable_oligodendrocyte_myelination = enable;
    nimcp_mutex_unlock(&gi->lock);
}

void glial_integration_set_microglia_pruning_enabled(glial_integration_t* gi, bool enable) {
    if (!gi) return;

    nimcp_mutex_lock(&gi->lock);
    gi->enable_microglia_pruning = enable;
    nimcp_mutex_unlock(&gi->lock);
}

// ============================================================================
// MYELIN SHEATH INTEGRATION
// ============================================================================

nimcp_result_t glial_integration_set_myelin_sheath_network(
    glial_integration_t* gi, myelin_sheath_network_t* myelin_network) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    gi->myelin_sheath_network = myelin_network;
    gi->enable_myelin_sheath = (myelin_network != NULL);
    nimcp_mutex_unlock(&gi->lock);

    return NIMCP_SUCCESS;
}

void glial_integration_set_myelin_sheath_enabled(glial_integration_t* gi, bool enable) {
    if (!gi) return;

    nimcp_mutex_lock(&gi->lock);
    gi->enable_myelin_sheath = enable;
    nimcp_mutex_unlock(&gi->lock);
}

float glial_integration_get_myelin_velocity(glial_integration_t* gi, uint32_t axon_id) {
    if (!gi || !gi->enable_myelin_sheath || !gi->myelin_sheath_network) {
        return NIMCP_MYELIN_BASE_VELOCITY_MS;
    }

    nimcp_mutex_lock(&gi->lock);
    float velocity = myelin_network_get_velocity(gi->myelin_sheath_network, axon_id);
    nimcp_mutex_unlock(&gi->lock);

    return velocity;
}

float glial_integration_get_myelin_delay(glial_integration_t* gi, uint32_t axon_id) {
    if (!gi || !gi->enable_myelin_sheath || !gi->myelin_sheath_network) {
        return 0.0F;
    }

    nimcp_mutex_lock(&gi->lock);
    float delay = myelin_network_get_delay(gi->myelin_sheath_network, axon_id);
    nimcp_mutex_unlock(&gi->lock);

    return delay;
}

myelin_sheath_t* glial_integration_create_myelin_sheath(
    glial_integration_t* gi,
    uint32_t axon_id,
    uint32_t oligo_id,
    float axon_length,
    float axon_diameter) {
    if (!gi || !gi->myelin_sheath_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_integration_create_myelin_sheath: required parameter is NULL (gi, gi->myelin_sheath_network)");
        return NULL;
    }

    nimcp_mutex_lock(&gi->lock);
    myelin_sheath_t* sheath = myelin_network_create_sheath_for_axon(
        gi->myelin_sheath_network, axon_id, oligo_id, axon_length, axon_diameter, 0.0F);
    nimcp_mutex_unlock(&gi->lock);

    return sheath;
}

void glial_integration_apply_axon_activity_to_myelin(
    glial_integration_t* gi,
    uint32_t axon_id,
    float activity_level,
    float dt) {
    if (!gi || !gi->enable_myelin_sheath || !gi->myelin_sheath_network) {
        return;
    }

    nimcp_mutex_lock(&gi->lock);
    myelin_network_apply_activity(gi->myelin_sheath_network, axon_id, activity_level, dt);
    nimcp_mutex_unlock(&gi->lock);
}
