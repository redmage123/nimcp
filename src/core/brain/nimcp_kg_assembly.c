/**
 * @file nimcp_kg_assembly.c
 * @brief Hierarchical Assembly for Brain Knowledge Graph Wiring - Implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implementation of hierarchical assembly from module -> layer -> hemisphere -> brain.
 * Enables brain self-awareness by collecting module wirings and converting to KG.
 */

#include "core/brain/nimcp_kg_assembly.h"
#include "core/brain/nimcp_kg_search.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_assembly, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define INITIAL_MODULE_CAPACITY     16
#define INITIAL_EDGE_CAPACITY       32
#define INITIAL_CALLOSAL_CAPACITY   64

/* ============================================================================
 * Layer Names
 * ============================================================================ */

static const char* layer_names[] = {
    "Layer I (Molecular)",
    "Layer II (External Granular)",
    "Layer III (External Pyramidal)",
    "Layer IV (Internal Granular)",
    "Layer V (Internal Pyramidal)",
    "Layer VI (Multiform)"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds since epoch
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return 0;
}

/**
 * @brief Safe string copy with null termination
 */
static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

/* ============================================================================
 * Layer Assembly API Implementation
 * ============================================================================ */

kg_layer_wiring_t* kg_assembly_create_layer(uint8_t layer_index) {
    if (layer_index >= KG_ASSEMBLY_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_assembly_create_layer: capacity exceeded");
        return NULL;
    }

    kg_layer_wiring_t* layer = nimcp_calloc(1, sizeof(kg_layer_wiring_t));
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "layer is NULL");

        return NULL;
    }

    layer->layer_index = layer_index;
    layer->finalized = false;

    /* Allocate module array */
    layer->module_capacity = INITIAL_MODULE_CAPACITY;
    layer->modules = nimcp_calloc(layer->module_capacity, sizeof(kg_module_wiring_t*));
    if (!layer->modules) {
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_assembly_create_layer: layer->modules is NULL");
        return NULL;
    }
    layer->module_count = 0;

    /* Allocate internal edges array */
    layer->internal_edge_capacity = INITIAL_EDGE_CAPACITY;
    layer->internal_edges = nimcp_calloc(layer->internal_edge_capacity, sizeof(kg_internal_edge_t));
    if (!layer->internal_edges) {
        nimcp_free(layer->modules);
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_assembly_create_layer: layer->internal_edges is NULL");
        return NULL;
    }
    layer->internal_edge_count = 0;

    /* Allocate external edges array */
    layer->external_edge_capacity = INITIAL_EDGE_CAPACITY;
    layer->external_edges = nimcp_calloc(layer->external_edge_capacity, sizeof(kg_external_edge_t));
    if (!layer->external_edges) {
        nimcp_free(layer->internal_edges);
        nimcp_free(layer->modules);
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_assembly_create_layer: layer->external_edges is NULL");
        return NULL;
    }
    layer->external_edge_count = 0;

    layer->metadata = NULL;

    return layer;
}

int kg_assembly_add_module_to_layer(
    kg_layer_wiring_t* layer,
    kg_module_wiring_t* module
) {
    if (!layer || !module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_module_to_layer: required parameter is NULL (layer, module)");
        return -1;
    }

    if (layer->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_module_to_layer: validation failed");
        return -1;  /* Cannot add after finalization */
    }

    /* Check capacity and resize if needed */
    if (layer->module_count >= layer->module_capacity) {
        uint32_t new_capacity = layer->module_capacity * 2;
        kg_module_wiring_t** new_modules = nimcp_realloc(
            layer->modules,
            new_capacity * sizeof(kg_module_wiring_t*)
        );
        if (!new_modules) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_modules is NULL");

            return -1;
        }
        layer->modules = new_modules;
        layer->module_capacity = new_capacity;
    }

    /* Add module pointer (not copying, just storing pointer) */
    layer->modules[layer->module_count++] = module;

    return 0;
}

int kg_assembly_add_internal_edge(
    kg_layer_wiring_t* layer,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    const char* edge_type
) {
    if (!layer || !edge_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_internal_edge: required parameter is NULL (layer, edge_type)");
        return -1;
    }

    if (layer->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_internal_edge: validation failed");
        return -1;
    }

    /* Check capacity and resize if needed */
    if (layer->internal_edge_count >= layer->internal_edge_capacity) {
        uint32_t new_capacity = layer->internal_edge_capacity * 2;
        kg_internal_edge_t* new_edges = nimcp_realloc(
            layer->internal_edges,
            new_capacity * sizeof(kg_internal_edge_t)
        );
        if (!new_edges) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_edges is NULL");

            return -1;
        }
        layer->internal_edges = new_edges;
        layer->internal_edge_capacity = new_capacity;
    }

    /* Add edge */
    kg_internal_edge_t* edge = &layer->internal_edges[layer->internal_edge_count++];
    edge->from = from;
    edge->to = to;
    safe_strcpy(edge->edge_type, edge_type, sizeof(edge->edge_type));

    return 0;
}

int kg_assembly_add_external_edge(
    kg_layer_wiring_t* layer,
    brain_kg_node_id_t from,
    uint8_t target_layer,
    brain_kg_node_id_t to,
    const char* edge_type
) {
    if (!layer || !edge_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_external_edge: required parameter is NULL (layer, edge_type)");
        return -1;
    }

    if (layer->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_external_edge: validation failed");
        return -1;
    }

    if (target_layer >= KG_ASSEMBLY_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_assembly_add_external_edge: capacity exceeded");
        return -1;
    }

    /* Check capacity and resize if needed */
    if (layer->external_edge_count >= layer->external_edge_capacity) {
        uint32_t new_capacity = layer->external_edge_capacity * 2;
        kg_external_edge_t* new_edges = nimcp_realloc(
            layer->external_edges,
            new_capacity * sizeof(kg_external_edge_t)
        );
        if (!new_edges) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_edges is NULL");

            return -1;
        }
        layer->external_edges = new_edges;
        layer->external_edge_capacity = new_capacity;
    }

    /* Add edge */
    kg_external_edge_t* edge = &layer->external_edges[layer->external_edge_count++];
    edge->from = from;
    edge->target_layer = target_layer;
    edge->to = to;
    safe_strcpy(edge->edge_type, edge_type, sizeof(edge->edge_type));

    return 0;
}

int kg_assembly_finalize_layer(kg_layer_wiring_t* layer) {
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer is NULL");

        return -1;
    }

    if (layer->finalized) {
        return 0;  /* Already finalized */
    }

    /* Create layer metadata */
    layer->metadata = kg_layer_metadata_create();
    if (layer->metadata) {
        /* Set layer-specific metadata */
        layer->metadata->layer_index = layer->layer_index;
        kg_metadata_set_string(&layer->metadata->base, "layer_name",
                               kg_assembly_layer_name(layer->layer_index), false);
        kg_metadata_set_int(&layer->metadata->base, "module_count",
                            (int64_t)layer->module_count, true);
        kg_metadata_set_int(&layer->metadata->base, "internal_edge_count",
                            (int64_t)layer->internal_edge_count, true);
        kg_metadata_set_int(&layer->metadata->base, "external_edge_count",
                            (int64_t)layer->external_edge_count, true);

        /* Compute layer health (average of module health) */
        float avg_health = 1.0f;  /* Default healthy */
        /* Would query each module's health here in full implementation */
        layer->metadata->aggregate_health = avg_health;

        layer->metadata->layer_coherence = 1.0f;  /* Default full coherence */
    }

    layer->finalized = true;
    return 0;
}

void kg_assembly_destroy_layer(kg_layer_wiring_t* layer) {
    if (!layer) {
        return;
    }

    /* Free arrays - but NOT the module wiring pointers themselves */
    nimcp_free(layer->modules);
    nimcp_free(layer->internal_edges);
    nimcp_free(layer->external_edges);

    /* Free metadata */
    if (layer->metadata) {
        kg_layer_metadata_destroy(layer->metadata);
    }

    nimcp_free(layer);
}

/* ============================================================================
 * Hemisphere Assembly API Implementation
 * ============================================================================ */

kg_hemisphere_wiring_t* kg_assembly_create_hemisphere(uint8_t hemisphere) {
    if (hemisphere > KG_ASSEMBLY_HEMISPHERE_RIGHT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_assembly_create_hemisphere: validation failed");
        return NULL;
    }

    kg_hemisphere_wiring_t* hemi = nimcp_calloc(1, sizeof(kg_hemisphere_wiring_t));
    if (!hemi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hemi is NULL");

        return NULL;
    }

    hemi->hemisphere = hemisphere;
    hemi->finalized = false;

    /* Initialize all layers */
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        memset(&hemi->layers[i], 0, sizeof(kg_layer_wiring_t));
        hemi->layers[i].layer_index = (uint8_t)i;
        hemi->layers[i].finalized = false;
    }

    hemi->metadata = NULL;
    hemi->feedforward_count = 0;
    hemi->feedback_count = 0;
    hemi->lateral_count = 0;
    hemi->total_modules = 0;

    return hemi;
}

int kg_assembly_add_layer_to_hemisphere(
    kg_hemisphere_wiring_t* hemi,
    kg_layer_wiring_t* layer
) {
    if (!hemi || !layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_layer_to_hemisphere: required parameter is NULL (hemi, layer)");
        return -1;
    }

    if (hemi->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_layer_to_hemisphere: validation failed");
        return -1;
    }

    if (!layer->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_layer_to_hemisphere: layer->finalized is NULL");
        return -1;  /* Layer must be finalized first */
    }

    if (layer->layer_index >= KG_ASSEMBLY_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_assembly_add_layer_to_hemisphere: capacity exceeded");
        return -1;
    }

    /* Copy layer data into hemisphere's layer slot */
    kg_layer_wiring_t* target = &hemi->layers[layer->layer_index];

    /* Copy basic fields */
    target->layer_index = layer->layer_index;
    target->module_count = layer->module_count;
    target->module_capacity = layer->module_capacity;
    target->internal_edge_count = layer->internal_edge_count;
    target->internal_edge_capacity = layer->internal_edge_capacity;
    target->external_edge_count = layer->external_edge_count;
    target->external_edge_capacity = layer->external_edge_capacity;
    target->finalized = layer->finalized;

    /* Transfer ownership of arrays */
    target->modules = layer->modules;
    target->internal_edges = layer->internal_edges;
    target->external_edges = layer->external_edges;
    target->metadata = layer->metadata;

    /* Clear source pointers to prevent double-free */
    layer->modules = NULL;
    layer->internal_edges = NULL;
    layer->external_edges = NULL;
    layer->metadata = NULL;

    return 0;
}

int kg_assembly_finalize_hemisphere(kg_hemisphere_wiring_t* hemi) {
    if (!hemi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemi is NULL");

        return -1;
    }

    if (hemi->finalized) {
        return 0;
    }

    /* Compute statistics */
    hemi->total_modules = 0;
    hemi->feedforward_count = 0;
    hemi->feedback_count = 0;
    hemi->lateral_count = 0;

    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        kg_layer_wiring_t* layer = &hemi->layers[i];

        hemi->total_modules += layer->module_count;
        hemi->lateral_count += layer->internal_edge_count;

        /* Classify external edges as feedforward or feedback */
        for (uint32_t e = 0; e < layer->external_edge_count; e++) {
            kg_external_edge_t* edge = &layer->external_edges[e];

            /* Feedforward: from lower to higher layer index */
            /* In cortex: IV -> II/III -> V (sensory to motor) */
            if (edge->target_layer > layer->layer_index) {
                hemi->feedforward_count++;
            } else if (edge->target_layer < layer->layer_index) {
                hemi->feedback_count++;
            }
        }
    }

    /* Create hemisphere metadata */
    hemi->metadata = kg_hemisphere_metadata_create();
    if (hemi->metadata) {
        const char* hemi_name = kg_assembly_hemisphere_name(hemi->hemisphere);
        hemi->metadata->hemisphere_id = hemi->hemisphere;
        kg_metadata_set_string(&hemi->metadata->base, "hemisphere_name", hemi_name, false);
        kg_metadata_set_int(&hemi->metadata->base, "total_modules",
                            (int64_t)hemi->total_modules, true);
        kg_metadata_set_int(&hemi->metadata->base, "feedforward_connections",
                            (int64_t)hemi->feedforward_count, true);
        kg_metadata_set_int(&hemi->metadata->base, "feedback_connections",
                            (int64_t)hemi->feedback_count, true);
        kg_metadata_set_int(&hemi->metadata->base, "lateral_connections",
                            (int64_t)hemi->lateral_count, true);

        hemi->metadata->total_modules = hemi->total_modules;
    }

    hemi->finalized = true;
    return 0;
}

void kg_assembly_destroy_hemisphere(kg_hemisphere_wiring_t* hemi) {
    if (!hemi) {
        return;
    }

    /* Destroy all layers */
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        kg_layer_wiring_t* layer = &hemi->layers[i];

        nimcp_free(layer->modules);
        nimcp_free(layer->internal_edges);
        nimcp_free(layer->external_edges);

        if (layer->metadata) {
            kg_layer_metadata_destroy(layer->metadata);
        }
    }

    /* Free hemisphere metadata */
    if (hemi->metadata) {
        kg_hemisphere_metadata_destroy(hemi->metadata);
    }

    nimcp_free(hemi);
}

/* ============================================================================
 * Brain Assembly API Implementation
 * ============================================================================ */

kg_brain_wiring_t* kg_assembly_create_brain(void) {
    kg_brain_wiring_t* brain = nimcp_calloc(1, sizeof(kg_brain_wiring_t));
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain is NULL");

        return NULL;
    }

    /* Initialize hemispheres */
    brain->left.hemisphere = KG_ASSEMBLY_HEMISPHERE_LEFT;
    brain->left.finalized = false;
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        memset(&brain->left.layers[i], 0, sizeof(kg_layer_wiring_t));
        brain->left.layers[i].layer_index = (uint8_t)i;
    }

    brain->right.hemisphere = KG_ASSEMBLY_HEMISPHERE_RIGHT;
    brain->right.finalized = false;
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        memset(&brain->right.layers[i], 0, sizeof(kg_layer_wiring_t));
        brain->right.layers[i].layer_index = (uint8_t)i;
    }

    /* Allocate callosal connections array */
    brain->callosal_capacity = INITIAL_CALLOSAL_CAPACITY;
    brain->callosal_connections = nimcp_calloc(
        brain->callosal_capacity,
        sizeof(kg_callosal_connection_t)
    );
    if (!brain->callosal_connections) {
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_assembly_create_brain: brain->callosal_connections is NULL");
        return NULL;
    }
    brain->callosal_count = 0;

    brain->metadata = NULL;
    brain->search_index = NULL;
    brain->finalized = false;
    brain->assembly_timestamp = 0;
    brain->version = 1;
    brain->total_modules = 0;
    brain->total_connections = 0;

    return brain;
}

int kg_assembly_add_hemisphere_to_brain(
    kg_brain_wiring_t* brain,
    kg_hemisphere_wiring_t* hemi
) {
    if (!brain || !hemi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_hemisphere_to_brain: required parameter is NULL (brain, hemi)");
        return -1;
    }

    if (brain->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_hemisphere_to_brain: validation failed");
        return -1;
    }

    if (!hemi->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_add_hemisphere_to_brain: hemi->finalized is NULL");
        return -1;
    }

    /* Determine target based on hemisphere ID */
    kg_hemisphere_wiring_t* target = NULL;
    if (hemi->hemisphere == KG_ASSEMBLY_HEMISPHERE_LEFT) {
        target = &brain->left;
    } else if (hemi->hemisphere == KG_ASSEMBLY_HEMISPHERE_RIGHT) {
        target = &brain->right;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_hemisphere_to_brain: validation failed");
        return -1;
    }

    /* Copy hemisphere data */
    target->hemisphere = hemi->hemisphere;
    target->finalized = hemi->finalized;
    target->feedforward_count = hemi->feedforward_count;
    target->feedback_count = hemi->feedback_count;
    target->lateral_count = hemi->lateral_count;
    target->total_modules = hemi->total_modules;

    /* Transfer layers */
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        kg_layer_wiring_t* src = &hemi->layers[i];
        kg_layer_wiring_t* dst = &target->layers[i];

        dst->layer_index = src->layer_index;
        dst->module_count = src->module_count;
        dst->module_capacity = src->module_capacity;
        dst->internal_edge_count = src->internal_edge_count;
        dst->internal_edge_capacity = src->internal_edge_capacity;
        dst->external_edge_count = src->external_edge_count;
        dst->external_edge_capacity = src->external_edge_capacity;
        dst->finalized = src->finalized;

        /* Transfer ownership of arrays */
        dst->modules = src->modules;
        dst->internal_edges = src->internal_edges;
        dst->external_edges = src->external_edges;
        dst->metadata = src->metadata;

        /* Clear source pointers */
        src->modules = NULL;
        src->internal_edges = NULL;
        src->external_edges = NULL;
        src->metadata = NULL;
    }

    /* Transfer hemisphere metadata */
    target->metadata = hemi->metadata;
    hemi->metadata = NULL;

    return 0;
}

int kg_assembly_add_callosal_connection(
    kg_brain_wiring_t* brain,
    brain_kg_node_id_t left_node,
    brain_kg_node_id_t right_node,
    float bandwidth
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;
    }

    if (brain->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_assembly_add_callosal_connection: validation failed");
        return -1;
    }

    /* Clamp bandwidth */
    if (bandwidth < 0.0f) bandwidth = 0.0f;
    if (bandwidth > 1.0f) bandwidth = 1.0f;

    /* Check capacity and resize if needed */
    if (brain->callosal_count >= brain->callosal_capacity) {
        uint32_t new_capacity = brain->callosal_capacity * 2;
        kg_callosal_connection_t* new_conns = nimcp_realloc(
            brain->callosal_connections,
            new_capacity * sizeof(kg_callosal_connection_t)
        );
        if (!new_conns) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_conns is NULL");

            return -1;
        }
        brain->callosal_connections = new_conns;
        brain->callosal_capacity = new_capacity;
    }

    /* Add connection */
    kg_callosal_connection_t* conn = &brain->callosal_connections[brain->callosal_count++];
    conn->left_node = left_node;
    conn->right_node = right_node;
    conn->bandwidth = bandwidth;

    return 0;
}

int kg_assembly_finalize_brain(kg_brain_wiring_t* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;
    }

    if (brain->finalized) {
        return 0;
    }

    /* Compute totals */
    brain->total_modules = brain->left.total_modules + brain->right.total_modules;

    /* Count all connections */
    uint32_t internal_edges = 0;
    uint32_t external_edges = 0;

    for (int h = 0; h < 2; h++) {
        kg_hemisphere_wiring_t* hemi = (h == 0) ? &brain->left : &brain->right;
        for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
            internal_edges += hemi->layers[i].internal_edge_count;
            external_edges += hemi->layers[i].external_edge_count;
        }
    }

    brain->total_connections = internal_edges + external_edges + brain->callosal_count;

    /* Create system metadata */
    brain->metadata = kg_system_metadata_create();
    if (brain->metadata) {
        kg_metadata_set_string(&brain->metadata->base, "brain_type", "NIMCP_Brain", false);
        kg_metadata_set_int(&brain->metadata->base, "total_modules",
                            (int64_t)brain->total_modules, true);
        kg_metadata_set_int(&brain->metadata->base, "total_connections",
                            (int64_t)brain->total_connections, true);
        kg_metadata_set_int(&brain->metadata->base, "callosal_connections",
                            (int64_t)brain->callosal_count, true);
        kg_metadata_set_int(&brain->metadata->base, "left_modules",
                            (int64_t)brain->left.total_modules, true);
        kg_metadata_set_int(&brain->metadata->base, "right_modules",
                            (int64_t)brain->right.total_modules, true);

        brain->metadata->total_modules = brain->total_modules;
    }

    /* Create search index */
    brain->search_index = kg_search_index_create();
    if (brain->search_index) {
        /* Add system metadata to search index */
        if (brain->metadata) {
            kg_search_index_add_system(brain->search_index, brain->metadata);
        }

        /* Add hemisphere metadata */
        if (brain->left.metadata) {
            kg_search_index_add_hemisphere(brain->search_index, brain->left.metadata);
        }
        if (brain->right.metadata) {
            kg_search_index_add_hemisphere(brain->search_index, brain->right.metadata);
        }

        /* Add layer metadata */
        for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
            if (brain->left.layers[i].metadata) {
                kg_search_index_add_layer(brain->search_index, brain->left.layers[i].metadata);
            }
            if (brain->right.layers[i].metadata) {
                kg_search_index_add_layer(brain->search_index, brain->right.layers[i].metadata);
            }
        }

        /* Rebuild index */
        kg_search_index_rebuild(brain->search_index);
    }

    brain->assembly_timestamp = get_timestamp_ms();
    brain->finalized = true;

    return 0;
}

void kg_assembly_destroy_brain(kg_brain_wiring_t* brain) {
    if (!brain) {
        return;
    }

    /* Destroy left hemisphere layers */
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        kg_layer_wiring_t* layer = &brain->left.layers[i];
        nimcp_free(layer->modules);
        nimcp_free(layer->internal_edges);
        nimcp_free(layer->external_edges);
        if (layer->metadata) {
            kg_layer_metadata_destroy(layer->metadata);
        }
    }
    if (brain->left.metadata) {
        kg_hemisphere_metadata_destroy(brain->left.metadata);
    }

    /* Destroy right hemisphere layers */
    for (int i = 0; i < KG_ASSEMBLY_LAYER_COUNT; i++) {
        kg_layer_wiring_t* layer = &brain->right.layers[i];
        nimcp_free(layer->modules);
        nimcp_free(layer->internal_edges);
        nimcp_free(layer->external_edges);
        if (layer->metadata) {
            kg_layer_metadata_destroy(layer->metadata);
        }
    }
    if (brain->right.metadata) {
        kg_hemisphere_metadata_destroy(brain->right.metadata);
    }

    /* Destroy callosal connections */
    nimcp_free(brain->callosal_connections);

    /* Destroy system metadata */
    if (brain->metadata) {
        kg_system_metadata_destroy(brain->metadata);
    }

    /* Destroy search index */
    if (brain->search_index) {
        kg_search_index_destroy(brain->search_index);
    }

    nimcp_free(brain);
}

/* ============================================================================
 * KG Conversion API Implementation
 * ============================================================================ */

int kg_assembly_write_to_kg(
    const kg_brain_wiring_t* wiring,
    brain_kg_t* kg
) {
    if (!wiring || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_write_to_kg: required parameter is NULL (wiring, kg)");
        return -1;
    }

    if (!wiring->finalized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_assembly_write_to_kg: wiring->finalized is NULL");
        return -1;
    }

    /* Process both hemispheres */
    for (int h = 0; h < 2; h++) {
        const kg_hemisphere_wiring_t* hemi = (h == 0) ? &wiring->left : &wiring->right;

        /* Process each layer */
        for (int l = 0; l < KG_ASSEMBLY_LAYER_COUNT; l++) {
            const kg_layer_wiring_t* layer = &hemi->layers[l];

            /* Add module nodes to KG */
            for (uint32_t m = 0; m < layer->module_count; m++) {
                kg_module_wiring_t* module = layer->modules[m];
                if (!module) continue;

                /* Create node for module */
                brain_kg_node_t node;
                memset(&node, 0, sizeof(brain_kg_node_t));

                /* Node ID is based on module wiring - using a simple hash */
                node.id = (brain_kg_node_id_t)(
                    (uint64_t)h << 48 |
                    (uint64_t)l << 40 |
                    (uint64_t)m
                );

                /* Set node properties from module wiring */
                safe_strcpy(node.name, module->module_name, sizeof(node.name));
                safe_strcpy(node.type, module->module_type, sizeof(node.type));

                /* Add node to KG */
                brain_kg_add_node(kg, &node);

                /* Add module metadata to KG node */
                if (module->metadata.description[0] != '\0') {
                    brain_kg_set_node_property_str(kg, node.id, "description",
                                                    module->metadata.description);
                }
                if (module->metadata.author[0] != '\0') {
                    brain_kg_set_node_property_str(kg, node.id, "author",
                                                    module->metadata.author);
                }
                if (module->metadata.category[0] != '\0') {
                    brain_kg_set_node_property_str(kg, node.id, "category",
                                                    module->metadata.category);
                }

                /* Set layer and hemisphere info */
                brain_kg_set_node_property_int(kg, node.id, "layer", l);
                brain_kg_set_node_property_int(kg, node.id, "hemisphere", h);
            }

            /* Add intra-layer edges */
            for (uint32_t e = 0; e < layer->internal_edge_count; e++) {
                const kg_internal_edge_t* edge = &layer->internal_edges[e];

                /* Determine edge type based on edge_type string */
                brain_kg_edge_type_t edge_type = BRAIN_KG_EDGE_CONNECTS_TO;
                if (strstr(edge->edge_type, "inhibit")) {
                    edge_type = BRAIN_KG_EDGE_INHIBITS;
                } else if (strstr(edge->edge_type, "excit")) {
                    edge_type = BRAIN_KG_EDGE_EXCITES;
                } else if (strstr(edge->edge_type, "modulate")) {
                    edge_type = BRAIN_KG_EDGE_MODULATES;
                }

                brain_kg_add_edge(kg, edge->from, edge->to, edge_type,
                                  edge->edge_type, 1.0f);
            }

            /* Add cross-layer edges */
            for (uint32_t e = 0; e < layer->external_edge_count; e++) {
                const kg_external_edge_t* edge = &layer->external_edges[e];

                /* Determine edge type based on edge_type string */
                brain_kg_edge_type_t edge_type = BRAIN_KG_EDGE_SENDS_TO;
                if (strstr(edge->edge_type, "feedback")) {
                    edge_type = BRAIN_KG_EDGE_RECEIVES_FROM;
                } else if (strstr(edge->edge_type, "feedforward")) {
                    edge_type = BRAIN_KG_EDGE_SENDS_TO;
                }

                brain_kg_edge_id_t edge_id = brain_kg_add_edge(
                    kg, edge->from, edge->to, edge_type,
                    edge->edge_type, 1.0f);

                /* Mark as cross-layer by storing target layer in edge property */
                if (edge_id != BRAIN_KG_INVALID_NODE) {
                    /* Edge property would require additional API - for now just add edge */
                    (void)edge_id;
                }
            }
        }
    }

    /* Add corpus callosum connections */
    for (uint32_t c = 0; c < wiring->callosal_count; c++) {
        const kg_callosal_connection_t* conn = &wiring->callosal_connections[c];

        /* Add bidirectional connections with corpus callosum type */
        brain_kg_add_edge(kg, conn->left_node, conn->right_node,
                          BRAIN_KG_EDGE_COORDINATES_WITH,
                          "corpus_callosum", conn->bandwidth);

        brain_kg_add_edge(kg, conn->right_node, conn->left_node,
                          BRAIN_KG_EDGE_COORDINATES_WITH,
                          "corpus_callosum", conn->bandwidth);
    }

    return 0;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* kg_assembly_layer_name(uint8_t layer_index) {
    if (layer_index >= KG_ASSEMBLY_LAYER_COUNT) {
        return "Unknown Layer";
    }
    return layer_names[layer_index];
}

const char* kg_assembly_hemisphere_name(uint8_t hemisphere) {
    switch (hemisphere) {
        case KG_ASSEMBLY_HEMISPHERE_LEFT:
            return "Left Hemisphere";
        case KG_ASSEMBLY_HEMISPHERE_RIGHT:
            return "Right Hemisphere";
        default:
            return "Unknown Hemisphere";
    }
}

int kg_assembly_validate(const kg_brain_wiring_t* brain) {
    if (!brain) {
        return 1;
    }

    int errors = 0;

    /* Check if finalized */
    if (!brain->finalized) {
        errors++;
    }

    /* Validate hemispheres */
    if (!brain->left.finalized) {
        errors++;
    }
    if (!brain->right.finalized) {
        errors++;
    }

    /* Validate layers */
    for (int h = 0; h < 2; h++) {
        const kg_hemisphere_wiring_t* hemi = (h == 0) ? &brain->left : &brain->right;

        for (int l = 0; l < KG_ASSEMBLY_LAYER_COUNT; l++) {
            const kg_layer_wiring_t* layer = &hemi->layers[l];

            /* Check for NULL module pointers */
            for (uint32_t m = 0; m < layer->module_count; m++) {
                if (!layer->modules[m]) {
                    errors++;
                }
            }

            /* Validate external edge target layers */
            for (uint32_t e = 0; e < layer->external_edge_count; e++) {
                if (layer->external_edges[e].target_layer >= KG_ASSEMBLY_LAYER_COUNT) {
                    errors++;
                }
            }
        }
    }

    /* Validate callosal connections - ensure bandwidth is in range */
    for (uint32_t c = 0; c < brain->callosal_count; c++) {
        float bw = brain->callosal_connections[c].bandwidth;
        if (bw < 0.0f || bw > 1.0f) {
            errors++;
        }
    }

    return errors;
}

int kg_assembly_get_stats(
    const kg_brain_wiring_t* brain,
    uint32_t* total_modules,
    uint32_t* total_internal_edges,
    uint32_t* total_external_edges,
    uint32_t* total_callosal
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;
    }

    uint32_t modules = 0;
    uint32_t internal = 0;
    uint32_t external = 0;

    for (int h = 0; h < 2; h++) {
        const kg_hemisphere_wiring_t* hemi = (h == 0) ? &brain->left : &brain->right;

        for (int l = 0; l < KG_ASSEMBLY_LAYER_COUNT; l++) {
            const kg_layer_wiring_t* layer = &hemi->layers[l];

            modules += layer->module_count;
            internal += layer->internal_edge_count;
            external += layer->external_edge_count;
        }
    }

    if (total_modules) *total_modules = modules;
    if (total_internal_edges) *total_internal_edges = internal;
    if (total_external_edges) *total_external_edges = external;
    if (total_callosal) *total_callosal = brain->callosal_count;

    return 0;
}
