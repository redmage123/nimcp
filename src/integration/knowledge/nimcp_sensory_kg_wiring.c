/**
 * @file nimcp_sensory_kg_wiring.c
 * @brief Sensory Module Knowledge Graph Wiring Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Implementation of knowledge graph registration for sensory modules.
 *
 * WHY: Enables the KG system to reason about sensory processing, support
 *      cross-modal queries, and provide security validation for sensory data.
 *
 * HOW: Manages node/edge storage, registration APIs, and query traversal.
 *
 * @author NIMCP Development Team
 */

#include "integration/knowledge/nimcp_sensory_kg_wiring.h"
#include "constants/nimcp_buffer_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sensory_kg_wiring)

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal KG wiring structure
 */
struct sensory_kg_wiring_struct {
    /* Configuration */
    sensory_kg_config_t config;

    /* Nodes */
    sensory_kg_node_t* nodes;
    uint32_t num_nodes;
    uint32_t next_node_id;

    /* Edges */
    sensory_kg_edge_t* edges;
    uint32_t num_edges;
    uint32_t next_edge_id;

    /* Module references */
    nimcp_somatosensory_t* soma;
    nimcp_olfactory_t* olfact;
    nimcp_gustatory_t* gust;

    /* Root nodes for each module */
    uint32_t soma_root_id;
    uint32_t olfact_root_id;
    uint32_t gust_root_id;
    uint32_t cross_modal_root_id;

    /* Statistics */
    sensory_kg_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp
 */
static uint64_t get_timestamp(void) {
    static uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Find node by ID
 */
static sensory_kg_node_t* find_node(sensory_kg_wiring_t* wiring, uint32_t node_id) {
    if (!wiring) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wiring is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < wiring->num_nodes; i++) {
        if (wiring->nodes[i].node_id == node_id) {
            return &wiring->nodes[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Allocate new node
 */
static sensory_kg_node_t* allocate_node(sensory_kg_wiring_t* wiring) {
    if (!wiring || wiring->num_nodes >= wiring->config.max_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_node: wiring is NULL");
        return NULL;
    }
    return &wiring->nodes[wiring->num_nodes++];
}

/**
 * @brief Allocate new edge
 */
static sensory_kg_edge_t* allocate_edge(sensory_kg_wiring_t* wiring) {
    if (!wiring || wiring->num_edges >= wiring->config.max_edges) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_edge: wiring is NULL");
        return NULL;
    }
    return &wiring->edges[wiring->num_edges++];
}

/**
 * @brief Create a generic node
 */
static int create_node(sensory_kg_wiring_t* wiring, sensory_kg_node_type_t type,
                       const char* name, const char* description,
                       uint32_t module_id, uint32_t* out_node_id) {
    if (!wiring || !name || !out_node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_node: required parameter is NULL (wiring, name, out_node_id)");
        return -1;
    }

    sensory_kg_node_t* node = allocate_node(wiring);
    if (!node) {
        wiring->stats.registration_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_node: node is NULL");
        return -1;
    }

    node->node_id = wiring->next_node_id++;
    node->type = type;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
    if (description) {
        strncpy(node->description, description, sizeof(node->description) - 1);
        node->description[sizeof(node->description) - 1] = '\0';
    }
    node->module_id = module_id;
    node->importance = 0.5f;
    node->activation = 0.0f;
    node->is_active = true;
    node->security_level = wiring->config.default_security_level;
    node->validated = false;
    node->created_time = get_timestamp();
    node->last_updated = node->created_time;

    *out_node_id = node->node_id;
    wiring->stats.total_nodes++;

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int sensory_kg_default_config(sensory_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(sensory_kg_config_t));

    config->max_nodes = SENSORY_KG_MAX_NODES_PER_MODULE * 4;  /* 4 modules worth */
    config->max_edges = SENSORY_KG_MAX_EDGES;

    config->enable_somatosensory = true;
    config->enable_olfactory = true;
    config->enable_gustatory = true;
    config->enable_cross_modal = true;

    config->enable_bbb_validation = false;
    config->default_security_level = 1;

    config->enable_caching = true;
    config->cache_ttl_ms = 5000;

    config->enable_logging = false;

    return 0;
}

sensory_kg_wiring_t* sensory_kg_wiring_create(const sensory_kg_config_t* config) {
    sensory_kg_wiring_t* wiring = (sensory_kg_wiring_t*)nimcp_calloc(1, sizeof(sensory_kg_wiring_t));
    NIMCP_API_CHECK_ALLOC(wiring, "Failed to allocate sensory KG wiring");

    if (config) {
        memcpy(&wiring->config, config, sizeof(sensory_kg_config_t));
    } else {
        sensory_kg_default_config(&wiring->config);
    }

    /* Allocate nodes */
    wiring->nodes = (sensory_kg_node_t*)nimcp_calloc(wiring->config.max_nodes, sizeof(sensory_kg_node_t));
    if (!wiring->nodes) {
        nimcp_free(wiring);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_kg_wiring_create: wiring->nodes is NULL");
        return NULL;
    }

    /* Allocate edges */
    wiring->edges = (sensory_kg_edge_t*)nimcp_calloc(wiring->config.max_edges, sizeof(sensory_kg_edge_t));
    if (!wiring->edges) {
        nimcp_free(wiring->nodes);
        nimcp_free(wiring);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_kg_wiring_create: wiring->edges is NULL");
        return NULL;
    }

    /* Initialize ID counters with base offsets */
    wiring->next_node_id = SENSORY_KG_SOMATOSENSORY_BASE;
    wiring->next_edge_id = 1;

    /* Create root nodes */
    uint32_t root_id;
    if (wiring->config.enable_somatosensory) {
        wiring->next_node_id = SENSORY_KG_SOMATOSENSORY_BASE;
        create_node(wiring, SENSORY_KG_NODE_CORTEX, "Somatosensory_Cortex",
                   "Primary somatosensory cortex (S1)", 0x3100, &root_id);
        wiring->soma_root_id = root_id;
        wiring->stats.somatosensory_nodes++;
    }

    if (wiring->config.enable_olfactory) {
        wiring->next_node_id = SENSORY_KG_OLFACTORY_BASE;
        create_node(wiring, SENSORY_KG_NODE_CORTEX, "Olfactory_Cortex",
                   "Primary olfactory cortex (piriform)", 0x3200, &root_id);
        wiring->olfact_root_id = root_id;
        wiring->stats.olfactory_nodes++;
    }

    if (wiring->config.enable_gustatory) {
        wiring->next_node_id = SENSORY_KG_GUSTATORY_BASE;
        create_node(wiring, SENSORY_KG_NODE_CORTEX, "Gustatory_Cortex",
                   "Primary gustatory cortex (insula)", 0x3300, &root_id);
        wiring->gust_root_id = root_id;
        wiring->stats.gustatory_nodes++;
    }

    if (wiring->config.enable_cross_modal) {
        wiring->next_node_id = SENSORY_KG_CROSS_MODAL_BASE;
        create_node(wiring, SENSORY_KG_NODE_ROOT, "Cross_Modal_Integration",
                   "Cross-modal sensory integration", 0, &root_id);
        wiring->cross_modal_root_id = root_id;
        wiring->stats.cross_modal_nodes++;
    }

    return wiring;
}

void sensory_kg_wiring_destroy(sensory_kg_wiring_t* wiring) {
    if (!wiring) return;

    if (wiring->nodes) nimcp_free(wiring->nodes);
    if (wiring->edges) nimcp_free(wiring->edges);
    nimcp_free(wiring);
}

/* ============================================================================
 * Somatosensory Registration Implementation
 * ============================================================================ */

int sensory_kg_register_somatosensory(sensory_kg_wiring_t* wiring, nimcp_somatosensory_t* soma) {
    if (!wiring || !soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_somatosensory: required parameter is NULL (wiring, soma)");
        return -1;
    }
    if (!wiring->config.enable_somatosensory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_somatosensory: wiring->config is NULL");
        return -1;
    }

    wiring->soma = soma;

    /* Register default body regions */
    uint32_t node_id;

    sensory_kg_register_body_region(wiring, "Hand_Region", BODY_SEG_HAND_L, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->soma_root_id, 1.0f);

    sensory_kg_register_body_region(wiring, "Face_Region", BODY_SEG_FACE, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->soma_root_id, 1.0f);

    sensory_kg_register_body_region(wiring, "Trunk_Region", BODY_SEG_CHEST, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->soma_root_id, 1.0f);

    sensory_kg_register_body_region(wiring, "Foot_Region", BODY_SEG_FOOT_L, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->soma_root_id, 1.0f);

    return 0;
}

int sensory_kg_register_body_region(sensory_kg_wiring_t* wiring, const char* name,
                                    body_segment_t region, uint32_t* node_id) {
    if (!wiring || !name || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_body_region: required parameter is NULL (wiring, name, node_id)");
        return -1;
    }

    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc), "Body region: %s (type %d)", name, region);

    int result = create_node(wiring, SENSORY_KG_NODE_BODY_REGION, name, desc,
                             0x3100, node_id);
    if (result == 0) {
        wiring->stats.somatosensory_nodes++;
        find_node(wiring, *node_id)->local_id = (uint32_t)region;
    }

    return result;
}

int sensory_kg_register_mechanoreceptor(sensory_kg_wiring_t* wiring, const char* name,
                                        soma_receptor_type_t type, uint32_t* node_id) {
    if (!wiring || !name || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_mechanoreceptor: required parameter is NULL (wiring, name, node_id)");
        return -1;
    }

    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc), "Mechanoreceptor: %s (type %d)", name, type);

    int result = create_node(wiring, SENSORY_KG_NODE_MECHANORECEPTOR, name, desc,
                             0x3100, node_id);
    if (result == 0) {
        wiring->stats.somatosensory_nodes++;
    }

    return result;
}

int sensory_kg_register_touch_pathway(sensory_kg_wiring_t* wiring, uint32_t source_id, uint32_t target_id) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_touch_pathway: wiring is NULL");
        return -1;
    }
    return sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PROJECTS_TO, source_id, target_id, 1.0f);
}

/* ============================================================================
 * Olfactory Registration Implementation
 * ============================================================================ */

int sensory_kg_register_olfactory(sensory_kg_wiring_t* wiring, nimcp_olfactory_t* olfact) {
    if (!wiring || !olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_olfactory: required parameter is NULL (wiring, olfact)");
        return -1;
    }
    if (!wiring->config.enable_olfactory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_olfactory: wiring->config is NULL");
        return -1;
    }

    wiring->olfact = olfact;

    /* Register default odor categories */
    uint32_t node_id;

    sensory_kg_register_odor_category(wiring, "Floral", ODOR_CAT_FLORAL, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->olfact_root_id, 1.0f);

    sensory_kg_register_odor_category(wiring, "Fruity", ODOR_CAT_FRUITY, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->olfact_root_id, 1.0f);

    sensory_kg_register_odor_category(wiring, "Woody", ODOR_CAT_WOODY, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->olfact_root_id, 1.0f);

    sensory_kg_register_odor_category(wiring, "Chemical", ODOR_CAT_CHEMICAL, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->olfact_root_id, 1.0f);

    return 0;
}

int sensory_kg_register_glomerulus(sensory_kg_wiring_t* wiring, uint32_t glom_id, uint32_t* node_id) {
    if (!wiring || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_glomerulus: required parameter is NULL (wiring, node_id)");
        return -1;
    }

    char name[NIMCP_ID_BUFFER_SIZE];
    snprintf(name, sizeof(name), "Glomerulus_%u", glom_id);

    int result = create_node(wiring, SENSORY_KG_NODE_GLOMERULUS, name,
                             "Olfactory bulb glomerulus", 0x3200, node_id);
    if (result == 0) {
        wiring->stats.olfactory_nodes++;
        find_node(wiring, *node_id)->local_id = glom_id;
    }

    return result;
}

int sensory_kg_register_odor_category(sensory_kg_wiring_t* wiring, const char* name,
                                      odor_category_t category, uint32_t* node_id) {
    if (!wiring || !name || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_odor_category: required parameter is NULL (wiring, name, node_id)");
        return -1;
    }

    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc), "Odor category: %s", name);

    int result = create_node(wiring, SENSORY_KG_NODE_ODOR_CATEGORY, name, desc,
                             0x3200, node_id);
    if (result == 0) {
        wiring->stats.olfactory_nodes++;
        find_node(wiring, *node_id)->local_id = (uint32_t)category;
    }

    return result;
}

int sensory_kg_register_odor_memory(sensory_kg_wiring_t* wiring, uint32_t odor_node, uint32_t memory_id) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_odor_memory: wiring is NULL");
        return -1;
    }

    char name[NIMCP_ID_BUFFER_SIZE];
    snprintf(name, sizeof(name), "Odor_Memory_%u", memory_id);

    uint32_t mem_node_id;
    int result = create_node(wiring, SENSORY_KG_NODE_ODOR_MEMORY, name,
                             "Odor-triggered memory association", 0x3200, &mem_node_id);
    if (result != 0) return result;

    wiring->stats.olfactory_nodes++;

    return sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_ASSOCIATED_MEMORY,
                               odor_node, mem_node_id, 1.0f);
}

/* ============================================================================
 * Gustatory Registration Implementation
 * ============================================================================ */

int sensory_kg_register_gustatory(sensory_kg_wiring_t* wiring, nimcp_gustatory_t* gust) {
    if (!wiring || !gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_gustatory: required parameter is NULL (wiring, gust)");
        return -1;
    }
    if (!wiring->config.enable_gustatory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_gustatory: wiring->config is NULL");
        return -1;
    }

    wiring->gust = gust;

    /* Register basic taste qualities */
    uint32_t node_id;

    sensory_kg_register_taste_quality(wiring, TASTE_SWEET, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->gust_root_id, 1.0f);

    sensory_kg_register_taste_quality(wiring, TASTE_SALTY, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->gust_root_id, 1.0f);

    sensory_kg_register_taste_quality(wiring, TASTE_SOUR, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->gust_root_id, 1.0f);

    sensory_kg_register_taste_quality(wiring, TASTE_BITTER, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->gust_root_id, 1.0f);

    sensory_kg_register_taste_quality(wiring, TASTE_UMAMI, &node_id);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, node_id, wiring->gust_root_id, 1.0f);

    return 0;
}

int sensory_kg_register_taste_quality(sensory_kg_wiring_t* wiring, basic_taste_t taste, uint32_t* node_id) {
    if (!wiring || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_taste_quality: required parameter is NULL (wiring, node_id)");
        return -1;
    }

    const char* taste_names[] = {"Sweet", "Salty", "Sour", "Bitter", "Umami"};
    const char* name = (taste < TASTE_COUNT) ? taste_names[taste] : "Unknown";

    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc), "Basic taste quality: %s", name);

    int result = create_node(wiring, SENSORY_KG_NODE_TASTE_QUALITY, name, desc,
                             0x3300, node_id);
    if (result == 0) {
        wiring->stats.gustatory_nodes++;
        find_node(wiring, *node_id)->local_id = (uint32_t)taste;
    }

    return result;
}

int sensory_kg_register_tongue_region(sensory_kg_wiring_t* wiring, tongue_region_t region, uint32_t* node_id) {
    if (!wiring || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_tongue_region: required parameter is NULL (wiring, node_id)");
        return -1;
    }

    const char* region_names[] = {"Tip", "Front_Sides", "Back_Sides", "Back", "Center"};
    const char* name = (region < TONGUE_REGION_COUNT) ? region_names[region] : "Unknown";

    char full_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(full_name, sizeof(full_name), "Tongue_%s", name);

    int result = create_node(wiring, SENSORY_KG_NODE_TASTE_BUD, full_name,
                             "Tongue region with taste receptors", 0x3300, node_id);
    if (result == 0) {
        wiring->stats.gustatory_nodes++;
    }

    return result;
}

int sensory_kg_register_food_category(sensory_kg_wiring_t* wiring, const char* name,
                                      food_category_t category, uint32_t* node_id) {
    if (!wiring || !name || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_food_category: required parameter is NULL (wiring, name, node_id)");
        return -1;
    }

    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc), "Food category: %s", name);

    int result = create_node(wiring, SENSORY_KG_NODE_FOOD_CATEGORY, name, desc,
                             0x3300, node_id);
    if (result == 0) {
        wiring->stats.gustatory_nodes++;
        find_node(wiring, *node_id)->local_id = (uint32_t)category;
    }

    return result;
}

/* ============================================================================
 * Cross-Modal Registration Implementation
 * ============================================================================ */

int sensory_kg_register_flavor(sensory_kg_wiring_t* wiring, uint32_t taste_node,
                               uint32_t odor_node, uint32_t* flavor_node_id) {
    if (!wiring || !flavor_node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_flavor: required parameter is NULL (wiring, flavor_node_id)");
        return -1;
    }
    if (!wiring->config.enable_cross_modal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_flavor: wiring->config is NULL");
        return -1;
    }

    /* Get taste and odor names for the flavor name */
    sensory_kg_node_t* taste = find_node(wiring, taste_node);
    sensory_kg_node_t* odor = find_node(wiring, odor_node);
    if (!taste || !odor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_flavor: required parameter is NULL (taste, odor)");
        return -1;
    }

    char name[NIMCP_ID_BUFFER_SIZE];
    snprintf(name, sizeof(name), "Flavor_%s_%s", taste->name, odor->name);

    int result = create_node(wiring, SENSORY_KG_NODE_FLAVOR, name,
                             "Cross-modal flavor percept", 0, flavor_node_id);
    if (result != 0) return result;

    wiring->stats.cross_modal_nodes++;

    /* Create binding edges */
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_BINDS_WITH, *flavor_node_id, taste_node, 0.5f);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_BINDS_WITH, *flavor_node_id, odor_node, 0.5f);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_PART_OF, *flavor_node_id, wiring->cross_modal_root_id, 1.0f);

    wiring->stats.integration_edges += 3;

    return 0;
}

int sensory_kg_register_chemosensory(sensory_kg_wiring_t* wiring, uint32_t olfact_node,
                                     uint32_t gust_node, uint32_t* node_id) {
    if (!wiring || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_chemosensory: required parameter is NULL (wiring, node_id)");
        return -1;
    }
    if (!wiring->config.enable_cross_modal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_register_chemosensory: wiring->config is NULL");
        return -1;
    }

    int result = create_node(wiring, SENSORY_KG_NODE_CHEMOSENSORY, "Chemosensory_Unit",
                             "Combined chemical sense processing", 0, node_id);
    if (result != 0) return result;

    wiring->stats.cross_modal_nodes++;

    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_INTEGRATES_WITH, *node_id, olfact_node, 0.5f);
    sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_INTEGRATES_WITH, *node_id, gust_node, 0.5f);

    wiring->stats.integration_edges += 2;

    return 0;
}

int sensory_kg_create_integration_edge(sensory_kg_wiring_t* wiring, uint32_t node_a,
                                       uint32_t node_b, float weight) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_create_integration_edge: wiring is NULL");
        return -1;
    }

    int result = sensory_kg_add_edge(wiring, SENSORY_KG_EDGE_INTEGRATES_WITH, node_a, node_b, weight);
    if (result == 0) {
        wiring->stats.integration_edges++;
    }
    return result;
}

/* ============================================================================
 * Edge Management Implementation
 * ============================================================================ */

int sensory_kg_add_edge(sensory_kg_wiring_t* wiring, sensory_kg_edge_type_t type,
                        uint32_t source, uint32_t target, float weight) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_add_edge: wiring is NULL");
        return -1;
    }

    /* Verify nodes exist */
    if (!find_node(wiring, source) || !find_node(wiring, target)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sensory_kg_add_edge: required parameter is NULL (find_node, find_node)");
        return -1;
    }

    sensory_kg_edge_t* edge = allocate_edge(wiring);
    if (!edge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_kg_add_edge: edge is NULL");
        return -1;
    }

    edge->edge_id = wiring->next_edge_id++;
    edge->type = type;
    edge->source_node_id = source;
    edge->target_node_id = target;
    edge->weight = weight;
    edge->confidence = 1.0f;
    edge->bidirectional = false;
    edge->active = true;
    edge->created_time = get_timestamp();
    edge->last_used = edge->created_time;

    wiring->stats.total_edges++;

    /* Categorize edge */
    if (type <= SENSORY_KG_EDGE_INHIBITS) {
        wiring->stats.processing_edges++;
    } else if (type <= SENSORY_KG_EDGE_ENHANCES) {
        wiring->stats.integration_edges++;
    } else if (type <= SENSORY_KG_EDGE_LEARNED_FROM) {
        wiring->stats.memory_edges++;
    }

    return 0;
}

int sensory_kg_remove_edge(sensory_kg_wiring_t* wiring, uint32_t edge_id) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_remove_edge: wiring is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < wiring->num_edges; i++) {
        if (wiring->edges[i].edge_id == edge_id) {
            wiring->edges[i].active = false;
            wiring->stats.total_edges--;
            return 0;
        }
    }

    return -1;  /* Edge not found */
}

int sensory_kg_update_edge_weight(sensory_kg_wiring_t* wiring, uint32_t edge_id, float new_weight) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_update_edge_weight: wiring is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < wiring->num_edges; i++) {
        if (wiring->edges[i].edge_id == edge_id && wiring->edges[i].active) {
            wiring->edges[i].weight = new_weight;
            return 0;
        }
    }

    return -1;  /* Edge not found */
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int sensory_kg_query_node(sensory_kg_wiring_t* wiring, uint32_t node_id, sensory_kg_node_t* node) {
    if (!wiring || !node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_query_node: required parameter is NULL (wiring, node)");
        return -1;
    }

    sensory_kg_node_t* found = find_node(wiring, node_id);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_query_node: found is NULL");
        return -1;
    }

    memcpy(node, found, sizeof(sensory_kg_node_t));
    wiring->stats.queries_processed++;

    return 0;
}

int sensory_kg_query_by_type(sensory_kg_wiring_t* wiring, sensory_kg_node_type_t type,
                             sensory_kg_query_result_t* result) {
    if (!wiring || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_query_by_type: required parameter is NULL (wiring, result)");
        return -1;
    }

    /* Count matching nodes */
    uint32_t count = 0;
    for (uint32_t i = 0; i < wiring->num_nodes; i++) {
        if (wiring->nodes[i].type == type && wiring->nodes[i].is_active) {
            count++;
        }
    }

    if (count == 0) {
        result->nodes = NULL;
        result->num_nodes = 0;
        result->edges = NULL;
        result->num_edges = 0;
        result->relevance_score = 0.0f;
        return 0;
    }

    /* Allocate result */
    result->nodes = (sensory_kg_node_t**)nimcp_calloc(count, sizeof(sensory_kg_node_t*));
    if (!result->nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_kg_query_by_type: result->nodes is NULL");
        return -1;
    }

    /* Fill result */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < wiring->num_nodes && idx < count; i++) {
        if (wiring->nodes[i].type == type && wiring->nodes[i].is_active) {
            result->nodes[idx++] = &wiring->nodes[i];
        }
    }

    result->num_nodes = count;
    result->edges = NULL;
    result->num_edges = 0;
    result->relevance_score = 1.0f;

    wiring->stats.queries_processed++;

    return 0;
}

int sensory_kg_query_connected(sensory_kg_wiring_t* wiring, uint32_t node_id,
                               sensory_kg_edge_type_t edge_type,
                               sensory_kg_query_result_t* result) {
    if (!wiring || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_query_connected: required parameter is NULL (wiring, result)");
        return -1;
    }

    /* Count connected nodes */
    uint32_t count = 0;
    for (uint32_t i = 0; i < wiring->num_edges; i++) {
        if (wiring->edges[i].active &&
            wiring->edges[i].source_node_id == node_id &&
            wiring->edges[i].type == edge_type) {
            count++;
        }
    }

    if (count == 0) {
        result->nodes = NULL;
        result->num_nodes = 0;
        result->edges = NULL;
        result->num_edges = 0;
        result->relevance_score = 0.0f;
        return 0;
    }

    result->nodes = (sensory_kg_node_t**)nimcp_calloc(count, sizeof(sensory_kg_node_t*));
    result->edges = (sensory_kg_edge_t**)nimcp_calloc(count, sizeof(sensory_kg_edge_t*));
    if (!result->nodes || !result->edges) {
        nimcp_free(result->nodes);
        nimcp_free(result->edges);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_kg_query_connected: required parameter is NULL (result->nodes, result->edges)");
        return -1;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < wiring->num_edges && idx < count; i++) {
        if (wiring->edges[i].active &&
            wiring->edges[i].source_node_id == node_id &&
            wiring->edges[i].type == edge_type) {
            result->edges[idx] = &wiring->edges[i];
            result->nodes[idx] = find_node(wiring, wiring->edges[i].target_node_id);
            idx++;
        }
    }

    result->num_nodes = count;
    result->num_edges = count;
    result->relevance_score = 1.0f;

    wiring->stats.queries_processed++;

    return 0;
}

int sensory_kg_query_path(sensory_kg_wiring_t* wiring, uint32_t source_id,
                          uint32_t target_id, sensory_kg_query_result_t* result) {
    if (!wiring || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_query_path: required parameter is NULL (wiring, result)");
        return -1;
    }

    /* Simple BFS path finding - placeholder implementation */
    result->nodes = NULL;
    result->num_nodes = 0;
    result->edges = NULL;
    result->num_edges = 0;
    result->relevance_score = 0.0f;

    wiring->stats.queries_processed++;

    return 0;
}

void sensory_kg_free_query_result(sensory_kg_query_result_t* result) {
    if (!result) return;

    if (result->nodes) nimcp_free(result->nodes);
    if (result->edges) nimcp_free(result->edges);

    result->nodes = NULL;
    result->num_nodes = 0;
    result->edges = NULL;
    result->num_edges = 0;
}

/* ============================================================================
 * Activation API Implementation
 * ============================================================================ */

int sensory_kg_activate_node(sensory_kg_wiring_t* wiring, uint32_t node_id, float activation) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_activate_node: wiring is NULL");
        return -1;
    }

    sensory_kg_node_t* node = find_node(wiring, node_id);
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_activate_node: node is NULL");
        return -1;
    }

    node->activation = (activation < 0.0f) ? 0.0f : (activation > 1.0f) ? 1.0f : activation;
    node->last_updated = get_timestamp();

    return 0;
}

int sensory_kg_propagate_activation(sensory_kg_wiring_t* wiring, uint32_t source_id, float decay_rate) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_propagate_activation: wiring is NULL");
        return -1;
    }

    sensory_kg_node_t* source = find_node(wiring, source_id);
    if (!source) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_propagate_activation: source is NULL");
        return -1;
    }

    /* Propagate to connected nodes */
    for (uint32_t i = 0; i < wiring->num_edges; i++) {
        sensory_kg_edge_t* edge = &wiring->edges[i];
        if (edge->active && edge->source_node_id == source_id) {
            sensory_kg_node_t* target = find_node(wiring, edge->target_node_id);
            if (target) {
                float propagated = source->activation * edge->weight * decay_rate;
                target->activation += propagated;
                if (target->activation > 1.0f) target->activation = 1.0f;
                target->last_updated = get_timestamp();
            }
            edge->last_used = get_timestamp();
        }
    }

    return 0;
}

int sensory_kg_decay_activations(sensory_kg_wiring_t* wiring, float decay_factor) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_decay_activations: wiring is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < wiring->num_nodes; i++) {
        if (wiring->nodes[i].is_active) {
            wiring->nodes[i].activation *= decay_factor;
            if (wiring->nodes[i].activation < 0.001f) {
                wiring->nodes[i].activation = 0.0f;
            }
        }
    }

    return 0;
}

/* ============================================================================
 * Security API Implementation
 * ============================================================================ */

int sensory_kg_validate_node(sensory_kg_wiring_t* wiring, uint32_t node_id, void* bbb_context) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_validate_node: wiring is NULL");
        return -1;
    }
    (void)bbb_context;  /* Would integrate with BBB in full implementation */

    sensory_kg_node_t* node = find_node(wiring, node_id);
    if (!node) {
        wiring->stats.validation_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_validate_node: node is NULL");
        return -1;
    }

    node->validated = true;
    return 0;
}

int sensory_kg_set_security_level(sensory_kg_wiring_t* wiring, uint32_t node_id, uint32_t level) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_set_security_level: wiring is NULL");
        return -1;
    }

    sensory_kg_node_t* node = find_node(wiring, node_id);
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_set_security_level: node is NULL");
        return -1;
    }

    node->security_level = level;
    return 0;
}

bool sensory_kg_check_access(sensory_kg_wiring_t* wiring, uint32_t node_id, uint32_t requester_level) {
    if (!wiring) {
        return false;
    }

    sensory_kg_node_t* node = find_node(wiring, node_id);
    if (!node) {
        return false;
    }

    return requester_level >= node->security_level;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int sensory_kg_get_stats(const sensory_kg_wiring_t* wiring, sensory_kg_stats_t* stats) {
    if (!wiring || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_get_stats: required parameter is NULL (wiring, stats)");
        return -1;
    }
    memcpy(stats, &wiring->stats, sizeof(sensory_kg_stats_t));
    return 0;
}

int sensory_kg_reset_stats(sensory_kg_wiring_t* wiring) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_reset_stats: wiring is NULL");
        return -1;
    }

    /* Preserve structural counts */
    uint32_t total_nodes = wiring->stats.total_nodes;
    uint32_t total_edges = wiring->stats.total_edges;

    memset(&wiring->stats, 0, sizeof(sensory_kg_stats_t));

    wiring->stats.total_nodes = total_nodes;
    wiring->stats.total_edges = total_edges;

    return 0;
}

const char* sensory_kg_node_type_name(sensory_kg_node_type_t type) {
    switch (type) {
        case SENSORY_KG_NODE_ROOT:           return "ROOT";
        case SENSORY_KG_NODE_CORTEX:         return "CORTEX";
        case SENSORY_KG_NODE_PATHWAY:        return "PATHWAY";
        case SENSORY_KG_NODE_RECEPTOR:       return "RECEPTOR";
        case SENSORY_KG_NODE_BODY_REGION:    return "BODY_REGION";
        case SENSORY_KG_NODE_MECHANORECEPTOR: return "MECHANORECEPTOR";
        case SENSORY_KG_NODE_THERMORECEPTOR: return "THERMORECEPTOR";
        case SENSORY_KG_NODE_NOCICEPTOR:     return "NOCICEPTOR";
        case SENSORY_KG_NODE_PROPRIOCEPTOR:  return "PROPRIOCEPTOR";
        case SENSORY_KG_NODE_GLOMERULUS:     return "GLOMERULUS";
        case SENSORY_KG_NODE_ODOR_CATEGORY:  return "ODOR_CATEGORY";
        case SENSORY_KG_NODE_ODOR_MEMORY:    return "ODOR_MEMORY";
        case SENSORY_KG_NODE_TASTE_BUD:      return "TASTE_BUD";
        case SENSORY_KG_NODE_TASTE_QUALITY:  return "TASTE_QUALITY";
        case SENSORY_KG_NODE_FOOD_CATEGORY:  return "FOOD_CATEGORY";
        case SENSORY_KG_NODE_FLAVOR:         return "FLAVOR";
        case SENSORY_KG_NODE_TEXTURE:        return "TEXTURE";
        case SENSORY_KG_NODE_CHEMOSENSORY:   return "CHEMOSENSORY";
        default:                              return "UNKNOWN";
    }
}

const char* sensory_kg_edge_type_name(sensory_kg_edge_type_t type) {
    switch (type) {
        case SENSORY_KG_EDGE_PROCESSES:       return "PROCESSES";
        case SENSORY_KG_EDGE_PROJECTS_TO:     return "PROJECTS_TO";
        case SENSORY_KG_EDGE_MODULATES:       return "MODULATES";
        case SENSORY_KG_EDGE_INHIBITS:        return "INHIBITS";
        case SENSORY_KG_EDGE_INTEGRATES_WITH: return "INTEGRATES_WITH";
        case SENSORY_KG_EDGE_BINDS_WITH:      return "BINDS_WITH";
        case SENSORY_KG_EDGE_ENHANCES:        return "ENHANCES";
        case SENSORY_KG_EDGE_ASSOCIATED_MEMORY: return "ASSOCIATED_MEMORY";
        case SENSORY_KG_EDGE_TRIGGERS_MEMORY: return "TRIGGERS_MEMORY";
        case SENSORY_KG_EDGE_LEARNED_FROM:    return "LEARNED_FROM";
        case SENSORY_KG_EDGE_TRIGGERS_RESPONSE: return "TRIGGERS_RESPONSE";
        case SENSORY_KG_EDGE_INDICATES_DANGER: return "INDICATES_DANGER";
        case SENSORY_KG_EDGE_INDICATES_REWARD: return "INDICATES_REWARD";
        case SENSORY_KG_EDGE_LOCATED_IN:      return "LOCATED_IN";
        case SENSORY_KG_EDGE_PART_OF:         return "PART_OF";
        case SENSORY_KG_EDGE_INNERVATES:      return "INNERVATES";
        default:                               return "UNKNOWN";
    }
}

void sensory_kg_print_summary(const sensory_kg_wiring_t* wiring) {
    if (!wiring) return;

    printf("=== Sensory KG Wiring Summary ===\n");
    printf("Total Nodes: %u\n", wiring->stats.total_nodes);
    printf("  Somatosensory: %u\n", wiring->stats.somatosensory_nodes);
    printf("  Olfactory: %u\n", wiring->stats.olfactory_nodes);
    printf("  Gustatory: %u\n", wiring->stats.gustatory_nodes);
    printf("  Cross-Modal: %u\n", wiring->stats.cross_modal_nodes);
    printf("\n");

    printf("Total Edges: %u\n", wiring->stats.total_edges);
    printf("  Processing: %u\n", wiring->stats.processing_edges);
    printf("  Integration: %u\n", wiring->stats.integration_edges);
    printf("  Memory: %u\n", wiring->stats.memory_edges);
    printf("\n");

    printf("Queries Processed: %lu\n", (unsigned long)wiring->stats.queries_processed);
    printf("Registration Errors: %lu\n", (unsigned long)wiring->stats.registration_errors);
    printf("Validation Errors: %lu\n", (unsigned long)wiring->stats.validation_errors);
    printf("=================================\n");
}

/* ============================================================================
 * Serialization Implementation (Stubs)
 * ============================================================================ */

size_t sensory_kg_get_serialization_size(const sensory_kg_wiring_t* wiring) {
    if (!wiring) return 0;

    return sizeof(sensory_kg_config_t) +
           (wiring->num_nodes * sizeof(sensory_kg_node_t)) +
           (wiring->num_edges * sizeof(sensory_kg_edge_t)) +
           sizeof(sensory_kg_stats_t) +
           64;  /* Header overhead */
}

int sensory_kg_serialize(const sensory_kg_wiring_t* wiring, uint8_t* buffer,
                         size_t size, size_t* written) {
    if (!wiring || !buffer || !written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_serialize: required parameter is NULL (wiring, buffer, written)");
        return -1;
    }

    size_t required = sensory_kg_get_serialization_size(wiring);
    if (size < required) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sensory_kg_serialize: validation failed");
        return -1;
    }

    /* Placeholder - full implementation would serialize all data */
    *written = 0;

    return 0;
}

sensory_kg_wiring_t* sensory_kg_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read) {
    if (!buffer || !bytes_read) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_deserialize: required parameter is NULL (buffer, bytes_read)");
        return NULL;
    }
    (void)size;

    /* Placeholder - full implementation would deserialize all data */
    *bytes_read = 0;

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_kg_deserialize: required parameter is NULL (buffer, bytes_read)");
    return NULL;
}
