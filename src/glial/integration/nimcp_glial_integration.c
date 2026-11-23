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
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

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

    // Initialize the lock first: the error path below calls
    // glial_integration_destroy(), which destroys it.
    nimcp_mutex_init(&gi->lock, NULL);

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
//
// The *_locked helpers require gi->lock to be held. The public functions take
// the (non-recursive) lock and call them; glial_integration_auto_assign_spatial
// holds the lock once and calls the helpers directly (it used to call the
// public functions, which re-locked the mutex and self-deadlocked as soon as
// any glial cell existed).

/* Append id to the reverse list of glial cell `glial_id` in `reverse`. */
static nimcp_result_t reverse_list_add_locked(hash_table_t* reverse, uint32_t glial_id, uint64_t id) {
    id_list_t** list_ptr = (id_list_t**)hash_table_lookup_uint32(reverse, glial_id);
    id_list_t* list;

    if (!list_ptr || !*list_ptr) {
        list = id_list_create();
        if (!list) {
            return NIMCP_ERROR_MEMORY;
        }
        // Store pointer to list in hash table (hash table copies the pointer value)
        if (!hash_table_insert_uint32(reverse, glial_id, &list, sizeof(id_list_t*))) {
            id_list_destroy(list);
            return NIMCP_ERROR_MEMORY;
        }
    } else {
        list = *list_ptr;
    }

    return id_list_add(list, id) ? NIMCP_SUCCESS : NIMCP_ERROR_MEMORY;
}

static nimcp_result_t assign_astrocyte_to_synapse_locked(glial_integration_t* gi,
                                                         uint32_t astrocyte_id,
                                                         uint64_t synapse_id) {
    uint32_t* prev = (uint32_t*)hash_table_lookup_uint64(gi->synapse_to_astrocyte, synapse_id);
    bool is_new = (prev == NULL);
    if (prev && *prev == astrocyte_id) {
        return NIMCP_SUCCESS;  // already mapped to this astrocyte
    }

    // Forward mapping: synapse → astrocyte (G7: uint64 key)
    if (!hash_table_insert_uint64(gi->synapse_to_astrocyte, synapse_id, &astrocyte_id, sizeof(uint32_t))) {
        return NIMCP_ERROR_MEMORY;
    }

    // Reverse mapping: astrocyte → list of synapses
    nimcp_result_t r = reverse_list_add_locked(gi->astrocyte_to_synapses, astrocyte_id, synapse_id);
    if (r != NIMCP_SUCCESS) {
        return r;
    }

    // Count the synapse into the astrocyte's drive normaliser
    if (is_new && gi->astrocyte_network && astrocyte_id < gi->astrocyte_network->num_astrocytes) {
        astrocyte_t* ast = gi->astrocyte_network->astrocytes[astrocyte_id];
        if (ast) {
            nimcp_spinlock_lock(&ast->lock);
            ast->drive_synapses++;
            nimcp_spinlock_unlock(&ast->lock);
        }
    }
    return NIMCP_SUCCESS;
}

static nimcp_result_t assign_oligodendrocyte_to_neuron_locked(glial_integration_t* gi,
                                                              uint32_t oligo_id,
                                                              uint32_t neuron_id) {
    // Register the axon with the oligodendrocyte itself; without this,
    // oligodendrocyte_get_myelination_level() never finds it (always 0).
    if (gi->oligodendrocyte_network && oligo_id < gi->oligodendrocyte_network->num_oligodendrocytes) {
        oligodendrocyte_t* oligo = gi->oligodendrocyte_network->oligodendrocytes[oligo_id];
        if (oligo) {
            nimcp_result_t ar = oligodendrocyte_assign_neuron(oligo, neuron_id);
            if (ar != NIMCP_SUCCESS) {
                return ar;  // cell full (max_axons) or invalid
            }
        }
    }

    // Forward mapping: neuron → oligodendrocyte
    if (!hash_table_insert_uint32(gi->neuron_to_oligodendrocyte, neuron_id, &oligo_id, sizeof(uint32_t))) {
        return NIMCP_ERROR_MEMORY;
    }

    // Reverse mapping: oligodendrocyte → list of neurons
    return reverse_list_add_locked(gi->oligodendrocyte_to_neurons, oligo_id, neuron_id);
}

static nimcp_result_t assign_microglia_to_synapse_locked(glial_integration_t* gi,
                                                         uint32_t microglia_id,
                                                         uint64_t synapse_id) {
    // Forward mapping: synapse → microglia (G7: uint64 key)
    if (!hash_table_insert_uint64(gi->synapse_to_microglia, synapse_id, &microglia_id, sizeof(uint32_t))) {
        return NIMCP_ERROR_MEMORY;
    }

    // Reverse mapping: microglia → list of synapses
    nimcp_result_t r = reverse_list_add_locked(gi->microglia_to_synapses, microglia_id, synapse_id);
    if (r != NIMCP_SUCCESS) {
        return r;
    }

    // Also add to microglia's monitored synapse list
    if (gi->microglia_network && microglia_id < gi->microglia_network->num_microglia) {
        microglia_t* mg = gi->microglia_network->microglia[microglia_id];
        if (mg) {
            microglia_monitor_synapse(mg, synapse_id);
        }
    }
    return NIMCP_SUCCESS;
}

nimcp_result_t glial_integration_assign_astrocyte_to_synapse(glial_integration_t* gi,
                                                             uint32_t astrocyte_id,
                                                             uint64_t synapse_id) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    nimcp_result_t r = assign_astrocyte_to_synapse_locked(gi, astrocyte_id, synapse_id);
    nimcp_mutex_unlock(&gi->lock);
    return r;
}

nimcp_result_t glial_integration_assign_oligodendrocyte_to_neuron(glial_integration_t* gi,
                                                                  uint32_t oligo_id,
                                                                  uint32_t neuron_id) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    nimcp_result_t r = assign_oligodendrocyte_to_neuron_locked(gi, oligo_id, neuron_id);
    nimcp_mutex_unlock(&gi->lock);
    return r;
}

nimcp_result_t glial_integration_assign_microglia_to_synapse(glial_integration_t* gi,
                                                             uint32_t microglia_id,
                                                             uint64_t synapse_id) {
    NIMCP_CHECK_THROW(gi, NIMCP_ERROR_INVALID_PARAM, "gi is NULL");

    nimcp_mutex_lock(&gi->lock);
    nimcp_result_t r = assign_microglia_to_synapse_locked(gi, microglia_id, synapse_id);
    nimcp_mutex_unlock(&gi->lock);
    return r;
}

// ============================================================================
// AUTOMATIC SPATIAL ASSIGNMENT
// ============================================================================

static inline uint32_t saturating_add_u32(uint32_t a, uint64_t b) {
    uint64_t s = (uint64_t)a + b;
    return s > UINT32_MAX ? UINT32_MAX : (uint32_t)s;
}

uint32_t glial_integration_auto_assign_spatial(glial_integration_t* gi) {
    /**
     * WHAT: Assign glial cells to the real neurons/synapses of gi->network
     * WHY:  Set up tripartite synapses and myelination without manual calls
     * HOW:  Neuron-id locality (no coordinates exist): astrocyte territories
     *       over postsynaptic neuron blocks (all real synapses covered, O(1)
     *       lookup, no per-synapse memory); oligodendrocytes take neuron
     *       blocks up to their axon capacity. Microglia are left to explicit
     *       assignment. See header for details.
     */
    if (!gi) return 0;

    uint32_t total_assignments = 0;

    nimcp_mutex_lock(&gi->lock);

    uint32_t n_neurons = gi->network ? neural_network_get_num_neurons(gi->network) : 0;
    if (n_neurons == 0) {
        nimcp_mutex_unlock(&gi->lock);
        return 0;
    }

    // ========================================================================
    // STEP 1: Astrocyte territories (tripartite synapses)
    // ========================================================================
    if (gi->astrocyte_network && gi->astrocyte_network->num_astrocytes > 0 &&
        gi->astrocyte_network->astrocytes) {
        astrocyte_network_t* astro_net = gi->astrocyte_network;
        uint32_t n_astro = astro_net->num_astrocytes;

        // Count real incoming synapses per territory
        uint32_t* counts = (uint32_t*)nimcp_calloc(n_astro, sizeof(uint32_t));
        uint64_t covered = 0;
        if (counts) {
            for (uint32_t n = 0; n < n_neurons; n++) {
                neuron_t* neuron = neural_network_get_neuron(gi->network, n);
                if (!neuron) continue;
                uint32_t in = NEURON_IN_COUNT(neuron);
                uint32_t a = (uint32_t)(((uint64_t)n * n_astro) / n_neurons);
                uint64_t c = (uint64_t)counts[a] + in;
                counts[a] = c > UINT32_MAX ? UINT32_MAX : (uint32_t)c;
                covered += in;
            }
            for (uint32_t a = 0; a < n_astro; a++) {
                astrocyte_t* ast = astro_net->astrocytes[a];
                if (!ast) continue;
                nimcp_spinlock_lock(&ast->lock);
                ast->drive_synapses = counts[a];
                nimcp_spinlock_unlock(&ast->lock);
            }
            nimcp_free(counts);

            gi->territory_neurons = n_neurons;
            gi->territory_astrocytes = n_astro;
            gi->territory_synapses = covered;
            total_assignments = saturating_add_u32(total_assignments, covered);
        } else {
            LOG_WARN(LOG_MODULE, "auto_assign_spatial: territory count allocation failed");
        }
    }

    // ========================================================================
    // STEP 2: Oligodendrocytes → neurons (axons), capacity-limited
    // ========================================================================
    if (gi->oligodendrocyte_network && gi->oligodendrocyte_network->num_oligodendrocytes > 0 &&
        gi->oligodendrocyte_network->oligodendrocytes) {
        oligodendrocyte_network_t* oligo_net = gi->oligodendrocyte_network;
        uint32_t n_oligo = oligo_net->num_oligodendrocytes;

        for (uint32_t n = 0; n < n_neurons; n++) {
            uint32_t o = (uint32_t)(((uint64_t)n * n_oligo) / n_neurons);
            oligodendrocyte_t* oligo = oligo_net->oligodendrocytes[o];
            if (!oligo) continue;
            if (hash_table_lookup_uint32(gi->neuron_to_oligodendrocyte, n)) {
                continue;  // already assigned (idempotent re-run)
            }
            if (oligo->num_myelinated_axons >= oligo->max_axons) {
                continue;  // this oligodendrocyte is full
            }
            if (assign_oligodendrocyte_to_neuron_locked(gi, o, n) == NIMCP_SUCCESS) {
                total_assignments = saturating_add_u32(total_assignments, 1);
            }
        }
    }

    // STEP 3 (microglia): intentionally not automatic — see header.

    nimcp_mutex_unlock(&gi->lock);

    LOG_INFO(LOG_MODULE, "auto_assign_spatial: %u assignments (%llu territory synapses, %u neurons)",
             total_assignments, (unsigned long long)gi->territory_synapses, n_neurons);
    return total_assignments;
}

uint32_t glial_integration_populate(glial_integration_t* gi, uint32_t n_neurons, float density) {
    if (!gi) {
        return 0;
    }
    if (!(density > 0.0F) || !isfinite(density)) {
        density = 1.0F;
    }
    if (density > 1.0F) {
        density = 1.0F;
    }
    if (n_neurons == 0 && gi->network) {
        n_neurons = neural_network_get_num_neurons(gi->network);
    }
    if (n_neurons == 0) {
        return 0;
    }

    const float spacing_um = 50.0F;

    nimcp_mutex_lock(&gi->lock);

    // Astrocytes
    astrocyte_network_t* an = gi->astrocyte_network;
    if (an && an->num_astrocytes == 0 && an->capacity > 0) {
        uint32_t count = (uint32_t)lroundf((float)an->capacity * density);
        if (count < 1) count = 1;
        uint32_t side = (uint32_t)ceilf(cbrtf((float)count));
        for (uint32_t i = 0; i < count; i++) {
            float x = (float)(i % side) * spacing_um;
            float y = (float)((i / side) % side) * spacing_um;
            float z = (float)(i / (side * side)) * spacing_um;
            astrocyte_t* ast = astrocyte_create(i, ASTROCYTE_TYPE_GENERIC, x, y, z, spacing_um);
            if (!ast) break;
            if (astrocyte_network_add(an, ast) != NIMCP_SUCCESS) {
                astrocyte_destroy(ast);
                break;
            }
        }
    }

    // Oligodendrocytes: only as many as needed to cover n_neurons axons
    oligodendrocyte_network_t* on = gi->oligodendrocyte_network;
    if (on && on->num_oligodendrocytes == 0 && on->capacity > 0) {
        uint32_t count = (uint32_t)lroundf((float)on->capacity * density);
        if (count < 1) count = 1;
        uint32_t per = (n_neurons + count - 1) / count;
        if (per > NIMCP_OLIGO_MAX_AXONS) per = NIMCP_OLIGO_MAX_AXONS;
        if (per < 1) per = 1;
        uint32_t needed = (n_neurons + per - 1) / per;
        if (count > needed) count = needed;
        uint32_t side = (uint32_t)ceilf(cbrtf((float)count));
        for (uint32_t i = 0; i < count; i++) {
            float x = (float)(i % side) * spacing_um;
            float y = (float)((i / side) % side) * spacing_um;
            float z = (float)(i / (side * side)) * spacing_um;
            oligodendrocyte_t* ol = oligodendrocyte_create(i, x, y, z, per);
            if (!ol) break;
            if (oligodendrocyte_network_add(on, ol) != NIMCP_SUCCESS) {
                oligodendrocyte_destroy(ol);
                break;
            }
        }
    }

    // Microglia: bounded (each preallocates its monitoring slots)
    microglia_network_t* mn = gi->microglia_network;
    if (mn && mn->num_microglia == 0 && mn->capacity > 0) {
        uint32_t count = (uint32_t)lroundf((float)mn->capacity * density);
        if (count < 1) count = 1;
        if (count > 256) count = 256;
        uint32_t side = (uint32_t)ceilf(cbrtf((float)count));
        for (uint32_t i = 0; i < count; i++) {
            float x = (float)(i % side) * spacing_um;
            float y = (float)((i / side) % side) * spacing_um;
            float z = (float)(i / (side * side)) * spacing_um;
            microglia_t* mg = microglia_create(i, x, y, z, spacing_um);
            if (!mg) break;
            if (microglia_network_add(mn, mg) != NIMCP_SUCCESS) {
                microglia_destroy(mg);
                break;
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);

    return glial_integration_auto_assign_spatial(gi);
}

// ============================================================================
// LOOKUP HELPERS (caller holds gi->lock)
// ============================================================================

/* Astrocyte serving synapse (pre, post): explicit mapping first, then the
 * territory of the postsynaptic neuron. NULL if none. */
static astrocyte_t* lookup_astrocyte_locked(glial_integration_t* gi, uint32_t pre, uint32_t post) {
    astrocyte_network_t* an = gi->astrocyte_network;
    if (!an || !an->astrocytes || an->num_astrocytes == 0) {
        return NULL;
    }
    uint32_t astrocyte_id = UINT32_MAX;
    if (hash_table_size(gi->synapse_to_astrocyte) > 0) {
        uint32_t* p = (uint32_t*)hash_table_lookup_uint64(gi->synapse_to_astrocyte,
                                                          make_synapse_id(pre, post));
        if (p) astrocyte_id = *p;
    }
    if (astrocyte_id == UINT32_MAX && gi->territory_neurons > 0 && post < gi->territory_neurons) {
        astrocyte_id = (uint32_t)(((uint64_t)post * gi->territory_astrocytes) / gi->territory_neurons);
    }
    if (astrocyte_id >= an->num_astrocytes) {
        return NULL;
    }
    return an->astrocytes[astrocyte_id];
}

// ============================================================================
// EVENT NOTIFICATIONS
// ============================================================================

void glial_integration_on_synapse_fired(glial_integration_t* gi, uint32_t pre_neuron_id,
                                        uint32_t post_neuron_id, float synaptic_weight,
                                        uint64_t timestamp) {
    if (!gi) return;
    if (!gi->enable_astrocyte_modulation && !gi->enable_microglia_pruning) return;

    nimcp_mutex_lock(&gi->lock);

    // Astrocyte: accumulate synaptic drive. astrocyte_network_step() converts
    // it into an IP3-producing stimulus (mean weight x events/s per mapped
    // synapse). (Was: a full calcium ODE step plus a bio-router publication
    // per synaptic event, which also time-compressed the dynamics.)
    if (gi->enable_astrocyte_modulation) {
        astrocyte_t* ast = lookup_astrocyte_locked(gi, pre_neuron_id, post_neuron_id);
        if (ast) {
            float w = fabsf(synaptic_weight);
            if (isfinite(w)) {
                nimcp_spinlock_lock(&ast->lock);
                ast->synaptic_drive += w;
                nimcp_spinlock_unlock(&ast->lock);
            }
            gi->total_astrocyte_modulations++;
        }
    }

    // Notify microglia if assigned
    if (gi->enable_microglia_pruning && gi->microglia_network &&
        hash_table_size(gi->synapse_to_microglia) > 0) {
        uint64_t synapse_id = make_synapse_id(pre_neuron_id, post_neuron_id);
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
    (void)timestamp;
    if (!gi || !gi->enable_oligodendrocyte_myelination) return;

    nimcp_mutex_lock(&gi->lock);

    // Find assigned oligodendrocyte
    if (gi->oligodendrocyte_network && hash_table_size(gi->neuron_to_oligodendrocyte) > 0) {
        uint32_t* oligo_id_ptr = (uint32_t*)hash_table_lookup_uint32(
            gi->neuron_to_oligodendrocyte, neuron_id);

        if (oligo_id_ptr) {
            uint32_t oligo_id = *oligo_id_ptr;
            if (oligo_id < gi->oligodendrocyte_network->num_oligodendrocytes) {
                oligodendrocyte_t* oligo = gi->oligodendrocyte_network->oligodendrocytes[oligo_id];
                if (oligo) {
                    // Count the spike; oligodendrocyte_network_step() turns the
                    // counts into a firing rate (Hz) that drives myelination.
                    // (Was track_activity(..., 1.0): an EMA of the constant 1.0,
                    // i.e. "1 Hz" for any neuron that ever fired.)
                    oligodendrocyte_record_spike(oligo, neuron_id);
                    gi->total_oligodendrocyte_myelinations++;
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

    float modulation = 1.0F;  // No astrocyte -> neutral

    nimcp_mutex_lock(&gi->lock);

    astrocyte_t* ast = lookup_astrocyte_locked(gi, pre_neuron_id, post_neuron_id);
    if (ast) {
        nimcp_spinlock_lock(&ast->lock);
        float ca = ast->calcium_concentration;
        float base = ast->calcium_baseline;
        float pool = ast->glutamate_pool;
        float atp = ast->atp_level;
        nimcp_spinlock_unlock(&ast->lock);

        // Ca2+-dependent gliotransmitter potentiation (limited by the
        // releasable pool) minus depression when the astrocyte's energy
        // supply is depleted. Neutral (1.0) at resting Ca2+ and full ATP.
        // (Was 1 + glutamate_pool/100: range [1.00, 1.01].)
        float activation = (ca - base) / 1.0F;  // 1 uM above rest = full
        activation = activation < 0.0F ? 0.0F : (activation > 1.0F ? 1.0F : activation);
        float depression = 1.0F - atp;
        depression = depression < 0.0F ? 0.0F : (depression > 1.0F ? 1.0F : depression);
        modulation = 1.0F + 0.2F * (activation * pool - depression);

        if (!isfinite(modulation)) modulation = 1.0F;
        if (modulation < 0.8F) modulation = 0.8F;
        if (modulation > 1.2F) modulation = 1.2F;
    }

    nimcp_mutex_unlock(&gi->lock);
    return modulation;
}

float glial_integration_get_myelination_factor(glial_integration_t* gi, uint32_t neuron_id) {
    if (!gi || !gi->enable_oligodendrocyte_myelination) {
        return 0.0F; // No myelination
    }

    float myelination = 0.0F;  // No oligodendrocyte

    nimcp_mutex_lock(&gi->lock);

    if (gi->oligodendrocyte_network && hash_table_size(gi->neuron_to_oligodendrocyte) > 0) {
        uint32_t* oligo_id_ptr = (uint32_t*)hash_table_lookup_uint32(
            gi->neuron_to_oligodendrocyte, neuron_id);

        if (oligo_id_ptr) {
            uint32_t oligo_id = *oligo_id_ptr;
            if (oligo_id < gi->oligodendrocyte_network->num_oligodendrocytes) {
                oligodendrocyte_t* oligo = gi->oligodendrocyte_network->oligodendrocytes[oligo_id];
                if (oligo) {
                    myelination = oligodendrocyte_get_myelination_level(oligo, neuron_id);
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
    return myelination;
}

bool glial_integration_should_prune_synapse(glial_integration_t* gi, uint32_t pre_neuron_id,
                                            uint32_t post_neuron_id) {
    if (!gi) {
        return false;
    }
    if (!gi->enable_microglia_pruning) {
        return false;  /* Pruning disabled - normal config */
    }

    bool should_prune = false;

    nimcp_mutex_lock(&gi->lock);

    if (gi->microglia_network && hash_table_size(gi->synapse_to_microglia) > 0) {
        uint64_t synapse_id = make_synapse_id(pre_neuron_id, post_neuron_id);
        uint32_t* microglia_id_ptr = (uint32_t*)hash_table_lookup_uint64(
            gi->synapse_to_microglia, synapse_id);

        if (microglia_id_ptr) {
            uint32_t microglia_id = *microglia_id_ptr;
            if (microglia_id < gi->microglia_network->num_microglia) {
                microglia_t* mg = gi->microglia_network->microglia[microglia_id];
                if (mg) {
                    // Pruned = the microglia removed it from surveillance
                    // (microglia_prune_weak_synapses), or it is C3-tagged and
                    // still below the pruning threshold. (Was: activity_score
                    // < threshold, which is true for every freshly monitored
                    // synapse (score 0); a pruned synapse never transmits, so
                    // its score could never rise: instant permanent pruning.)
                    bool monitored = false;
                    bool c3 = false;
                    nimcp_spinlock_lock(&mg->lock);
                    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
                        if (mg->synapses[i].synapse_id == synapse_id) {
                            monitored = true;
                            c3 = (mg->synapses[i].complement.tag == COMPLEMENT_C3);
                            break;
                        }
                    }
                    nimcp_spinlock_unlock(&mg->lock);
                    if (!monitored) {
                        should_prune = true;
                    } else if (c3) {
                        should_prune = microglia_should_prune_synapse(mg, synapse_id);
                    }
                    if (should_prune) {
                        gi->total_microglia_prunings++;
                    }
                }
            }
        }
    }

    nimcp_mutex_unlock(&gi->lock);
    return should_prune;
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

    // Astrocyte and oligodendrocyte steps take SECONDS (their headers say
    // so). Passing milliseconds made astrocyte_network_step() skip every call
    // (dt > 1 returned early) and made oligodendrocytes integrate 1000x too
    // much time. Timestamps from different clocks (sim steps vs wall clock)
    // can produce huge gaps; anything beyond 2 s is steady state anyway.
    float dt_s = dt_ms / 1000.0F;
    if (!isfinite(dt_s) || dt_s <= 0.0F) dt_s = 0.001F;
    if (dt_s > 2.0F) dt_s = 2.0F;

    // Step astrocyte network (calcium dynamics, glutamate release)
    if (gi->astrocyte_network && gi->enable_astrocyte_modulation) {
        astrocyte_network_step(gi->astrocyte_network, dt_s);
    }

    /* Phase 8: Send heartbeat after astrocyte step (40% progress) */
    glial_integration_heartbeat("glial_integration_step", 0.4f);

    // Step oligodendrocyte network (adaptive myelination)
    if (gi->oligodendrocyte_network && gi->enable_oligodendrocyte_myelination) {
        oligodendrocyte_network_step(gi->oligodendrocyte_network, dt_s);
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
    {
        // Explicit mappings + synapses covered by astrocyte territories
        uint64_t tri = (uint64_t)hash_table_size(gi->synapse_to_astrocyte) + gi->territory_synapses;
        stats->num_tripartite_synapses = tri > UINT32_MAX ? UINT32_MAX : (uint32_t)tri;
    }
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
