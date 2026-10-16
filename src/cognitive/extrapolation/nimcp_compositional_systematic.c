/**
 * @file nimcp_compositional_systematic.c
 * @brief Compositional Systematic Generalization Module Implementation
 *
 * Implements systematic compositionality for combining known primitives
 * into novel combinations following grammatical rules.
 */

#include "cognitive/extrapolation/nimcp_compositional_systematic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(compositional_systematic, MESH_ADAPTER_CATEGORY_COGNITIVE)



/*=============================================================================
 * STATIC HELPERS - MEMORY
 *===========================================================================*/

static cs_comp_node_t* alloc_comp_node(void) {
    cs_comp_node_t* node = nimcp_calloc(1, sizeof(cs_comp_node_t));
    return node;
}

static void free_comp_node(cs_comp_node_t* node) {
    if (!node) return;

    /* Recursively free children */
    for (uint32_t i = 0; i < node->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && node->num_children > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)node->num_children);
        }

        free_comp_node(node->children[i]);
    }

    nimcp_free(node);
    node = NULL;
}

static cs_comp_node_t* copy_comp_node(const cs_comp_node_t* src) {
    if (!src) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;

    }

    cs_comp_node_t* dst = alloc_comp_node();
    if (!dst) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dst is NULL");

        return NULL;

    }

    dst->primitive_id = src->primitive_id;
    dst->num_bindings = src->num_bindings;
    memcpy(dst->bindings, src->bindings, src->num_bindings * sizeof(cs_binding_t));
    dst->depth = src->depth;
    dst->confidence = src->confidence;
    dst->num_children = src->num_children;

    /* Recursively copy children */
    for (uint32_t i = 0; i < src->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && src->num_children > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)src->num_children);
        }

        dst->children[i] = copy_comp_node(src->children[i]);
    }

    return dst;
}

/*=============================================================================
 * STATIC HELPERS - PRIMITIVE LOOKUP
 *===========================================================================*/

static cs_primitive_t* find_primitive_by_id(
    nimcp_compositional_t* cs,
    uint32_t id)
{
    for (uint32_t i = 0; i < cs->num_primitives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_primitives > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_primitives);
        }

        if (cs->primitives[i].id == id) {
            return &cs->primitives[i];
        }
    }
    return NULL;
}

static cs_primitive_t* find_primitive_by_name(
    nimcp_compositional_t* cs,
    const char* name)
{
    for (uint32_t i = 0; i < cs->num_primitives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_primitives > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_primitives);
        }

        if (strcmp(cs->primitives[i].name, name) == 0) {
            return &cs->primitives[i];
        }
    }
    return NULL;
}

static cs_rule_t* find_rule_by_id(
    nimcp_compositional_t* cs,
    uint32_t id)
{
    for (uint32_t i = 0; i < cs->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_rules > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_rules);
        }

        if (cs->rules[i].id == id) {
            return &cs->rules[i];
        }
    }
    return NULL;
}

/*=============================================================================
 * STATIC HELPERS - EMBEDDING OPERATIONS
 *===========================================================================*/

static void combine_embeddings(
    float* output,
    const float* emb1,
    const float* emb2,
    float weight1,
    float weight2,
    uint32_t dim)
{
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)dim);
        }

        output[i] = weight1 * emb1[i] + weight2 * emb2[i];
    }
}

static float embedding_similarity(
    const float* emb1,
    const float* emb2,
    uint32_t dim)
{
    float dot = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)dim);
        }

        dot += emb1[i] * emb2[i];
        norm1 += emb1[i] * emb1[i];
        norm2 += emb2[i] * emb2[i];
    }

    float denom = sqrtf(norm1) * sqrtf(norm2);
    if (denom < 1e-8f) return 0.0f;

    return dot / denom;
}

static void normalize_embedding(float* emb, uint32_t dim) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)dim);
        }

        norm += emb[i] * emb[i];
    }
    norm = sqrtf(norm);

    if (norm > 1e-8f) {
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                compositional_systematic_heartbeat("compositiona_loop",
                                 (float)(i + 1) / (float)dim);
            }

            emb[i] /= norm;
        }
    }
}

/*=============================================================================
 * STATIC HELPERS - COMPOSITION
 *===========================================================================*/

static void compute_combined_embedding(
    nimcp_compositional_t* cs,
    cs_composition_t* comp)
{
    memset(comp->combined_embedding, 0, CS_EMBEDDING_DIM * sizeof(float));

    if (!comp->root) return;

    /* Start with head primitive embedding */
    cs_primitive_t* head = find_primitive_by_id(cs, comp->root->primitive_id);
    if (head) {
        memcpy(comp->combined_embedding, head->embedding,
               cs->config.embedding_dim * sizeof(float));
    }

    /* Combine with bound primitives */
    for (uint32_t i = 0; i < comp->root->num_bindings; i++) {
        cs_binding_t* binding = &comp->root->bindings[i];
        if (!binding->is_variable) {
            cs_primitive_t* bound = find_primitive_by_id(cs, binding->primitive_id);
            if (bound) {
                /* Weight by binding strength and slot importance */
                float weight = binding->strength * 0.3f;
                combine_embeddings(
                    comp->combined_embedding,
                    comp->combined_embedding,
                    bound->embedding,
                    1.0f - weight,
                    weight,
                    cs->config.embedding_dim);
            }
        }
    }

    normalize_embedding(comp->combined_embedding, cs->config.embedding_dim);
}

static uint32_t count_tree_primitives(const cs_comp_node_t* node) {
    if (!node) return 0;

    uint32_t count = 1; /* This node's primitive */

    /* Add bound primitives */
    for (uint32_t i = 0; i < node->num_bindings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && node->num_bindings > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)node->num_bindings);
        }

        if (!node->bindings[i].is_variable) {
            count++;
        }
    }

    /* Add children recursively */
    for (uint32_t i = 0; i < node->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && node->num_children > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)node->num_children);
        }

        count += count_tree_primitives(node->children[i]);
    }

    return count;
}

static uint32_t compute_tree_depth(const cs_comp_node_t* node) {
    if (!node) return 0;

    uint32_t max_child_depth = 0;
    for (uint32_t i = 0; i < node->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && node->num_children > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)node->num_children);
        }

        uint32_t child_depth = compute_tree_depth(node->children[i]);
        if (child_depth > max_child_depth) {
            max_child_depth = child_depth;
        }
    }

    return 1 + max_child_depth;
}

static bool check_rule_applicable(
    const cs_rule_t* rule,
    const cs_primitive_t* head,
    const cs_binding_t* bindings,
    uint32_t num_bindings)
{
    /* Check head type matches */
    if (head->type != rule->head_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_rule_applicable: validation failed");
        return false;
    }

    /* Check we have enough bindings */
    if (num_bindings < rule->num_args) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_rule_applicable: validation failed");
        return false;
    }

    return true;
}

static float compute_grammaticality(
    nimcp_compositional_t* cs,
    const cs_composition_t* comp)
{
    float score = 1.0f;

    if (!comp->root) return 0.0f;

    /* Check head primitive validity */
    cs_primitive_t* head = find_primitive_by_id(cs, comp->root->primitive_id);
    if (!head) return 0.0f;

    /* Penalize if head type doesn't match composition type expectations */
    if (comp->type == CS_COMPOSE_SEQUENTIAL && head->type != CS_PRIM_ACTION) {
        score *= 0.5f;
    }

    /* Check binding validity */
    for (uint32_t i = 0; i < comp->root->num_bindings; i++) {
        cs_binding_t* binding = &comp->root->bindings[i];
        if (!binding->is_variable) {
            cs_primitive_t* bound = find_primitive_by_id(cs, binding->primitive_id);
            if (!bound) {
                score *= 0.8f;
                continue;
            }

            /* Check if binding slot is valid for primitive */
            if (!(head->valid_bindings & (1 << binding->slot))) {
                score *= 0.7f;
            }
        }
    }

    /* Check recursion is allowed */
    if (comp->type == CS_COMPOSE_RECURSIVE && !cs->config.enable_recursive) {
        score *= 0.3f;
    }

    /* Penalize excessive depth */
    if (comp->max_depth > cs->config.max_depth) {
        score *= 0.5f;
    }

    return score;
}

/*=============================================================================
 * STATIC HELPERS - BASE RULES
 *===========================================================================*/

static void init_base_rules(nimcp_compositional_t* cs) {
    /* Rule 1: ACTION + OBJECT (e.g., "push ball") */
    cs_rule_t rule1 = {
        .id = cs->next_rule_id++,
        .name = "action_object",
        .comp_type = CS_COMPOSE_SEQUENTIAL,
        .head_type = CS_PRIM_ACTION,
        .arg_types = {CS_PRIM_OBJECT},
        .num_args = 1,
        .slot_order = {CS_BIND_PATIENT},
        .direction = CS_RULE_BIDIRECTIONAL,
        .confidence = 1.0f,
        .application_count = 0,
        .success_rate = 1.0f
    };
    strncpy(rule1.name, "action_object", CS_MAX_NAME_LEN - 1);
    cs->rules[cs->num_rules++] = rule1;

    /* Rule 2: ACTION + AGENT + OBJECT (e.g., "robot pushes ball") */
    cs_rule_t rule2 = {
        .id = cs->next_rule_id++,
        .name = "agent_action_object",
        .comp_type = CS_COMPOSE_SEQUENTIAL,
        .head_type = CS_PRIM_ACTION,
        .arg_types = {CS_PRIM_OBJECT, CS_PRIM_OBJECT},
        .num_args = 2,
        .slot_order = {CS_BIND_AGENT, CS_BIND_PATIENT},
        .direction = CS_RULE_BIDIRECTIONAL,
        .confidence = 1.0f,
        .application_count = 0,
        .success_rate = 1.0f
    };
    strncpy(rule2.name, "agent_action_object", CS_MAX_NAME_LEN - 1);
    cs->rules[cs->num_rules++] = rule2;

    /* Rule 3: MODIFIER + OBJECT (e.g., "red ball") */
    cs_rule_t rule3 = {
        .id = cs->next_rule_id++,
        .name = "modifier_object",
        .comp_type = CS_COMPOSE_PARALLEL,
        .head_type = CS_PRIM_OBJECT,
        .arg_types = {CS_PRIM_MODIFIER},
        .num_args = 1,
        .slot_order = {CS_BIND_MANNER},
        .direction = CS_RULE_BIDIRECTIONAL,
        .confidence = 1.0f,
        .application_count = 0,
        .success_rate = 1.0f
    };
    strncpy(rule3.name, "modifier_object", CS_MAX_NAME_LEN - 1);
    cs->rules[cs->num_rules++] = rule3;

    /* Rule 4: RELATION + OBJECT + OBJECT (e.g., "ball on table") */
    cs_rule_t rule4 = {
        .id = cs->next_rule_id++,
        .name = "relation_objects",
        .comp_type = CS_COMPOSE_HIERARCHICAL,
        .head_type = CS_PRIM_RELATION,
        .arg_types = {CS_PRIM_OBJECT, CS_PRIM_OBJECT},
        .num_args = 2,
        .slot_order = {CS_BIND_AGENT, CS_BIND_LOCATION},
        .direction = CS_RULE_BIDIRECTIONAL,
        .confidence = 1.0f,
        .application_count = 0,
        .success_rate = 1.0f
    };
    strncpy(rule4.name, "relation_objects", CS_MAX_NAME_LEN - 1);
    cs->rules[cs->num_rules++] = rule4;

    /* Rule 5: ACTION + MODIFIER (e.g., "run quickly") */
    cs_rule_t rule5 = {
        .id = cs->next_rule_id++,
        .name = "action_modifier",
        .comp_type = CS_COMPOSE_PARALLEL,
        .head_type = CS_PRIM_ACTION,
        .arg_types = {CS_PRIM_MODIFIER},
        .num_args = 1,
        .slot_order = {CS_BIND_MANNER},
        .direction = CS_RULE_BIDIRECTIONAL,
        .confidence = 1.0f,
        .application_count = 0,
        .success_rate = 1.0f
    };
    strncpy(rule5.name, "action_modifier", CS_MAX_NAME_LEN - 1);
    cs->rules[cs->num_rules++] = rule5;

    cs->stats.active_rules = cs->num_rules;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

cs_config_t cs_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_default_config", 0.0f);


    cs_config_t config = {
        .max_primitives = CS_MAX_PRIMITIVES,
        .max_compositions = CS_MAX_COMPOSITIONS,
        .max_rules = CS_MAX_RULES,
        .max_depth = CS_MAX_DEPTH,
        .embedding_dim = CS_EMBEDDING_DIM,
        .novelty_threshold = NIMCP_NOVELTY_THRESHOLD,
        .grammaticality_threshold = 0.5f,
        .binding_decay = 0.001f,
        .learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .enable_recursive = true,
        .enable_learning = true,
        .enable_logging = true
    };
    return config;
}

nimcp_compositional_t* cs_create(const cs_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_create", 0.0f);


    nimcp_compositional_t* cs = nimcp_calloc(1, sizeof(nimcp_compositional_t));
    if (!cs) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cs");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        cs->config = *config;
    } else {
        cs->config = cs_default_config();
    }

    /* Allocate primitive storage */
    cs->primitive_capacity = cs->config.max_primitives;
    cs->primitives = nimcp_calloc(cs->primitive_capacity, sizeof(cs_primitive_t));
    if (!cs->primitives) {
        nimcp_free(cs);
        cs = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cs_create: cs->primitives is NULL");
        return NULL;
    }
    cs->num_primitives = 0;
    cs->next_primitive_id = 1;

    /* Allocate composition storage */
    cs->composition_capacity = cs->config.max_compositions;
    cs->compositions = nimcp_calloc(cs->composition_capacity, sizeof(cs_composition_t));
    if (!cs->compositions) {
        nimcp_free(cs->primitives);
        nimcp_free(cs);
        cs = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cs_create: cs->compositions is NULL");
        return NULL;
    }
    cs->num_compositions = 0;
    cs->next_composition_id = 1;

    /* Allocate rule storage */
    cs->rule_capacity = cs->config.max_rules;
    cs->rules = nimcp_calloc(cs->rule_capacity, sizeof(cs_rule_t));
    if (!cs->rules) {
        nimcp_free(cs->compositions);
        nimcp_free(cs->primitives);
        nimcp_free(cs);
        cs = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cs_create: cs->rules is NULL");
        return NULL;
    }
    cs->num_rules = 0;
    cs->next_rule_id = 1;

    /* Initialize workspace */
    cs->workspace = NULL;
    cs->workspace_depth = 0;

    /* Set initial state */
    cs->status = CS_STATUS_IDLE;
    cs->last_error = CS_OK;
    cs->initialized = false;

    return cs;
}

cs_error_t cs_init(nimcp_compositional_t* cs) {
    if (!cs) return CS_ERR_NULL_PTR;

    /* Initialize statistics */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_init", 0.0f);


    memset(&cs->stats, 0, sizeof(cs_stats_t));

    /* Initialize base grammar rules */
    init_base_rules(cs);

    cs->initialized = true;
    return CS_OK;
}

cs_error_t cs_reset(nimcp_compositional_t* cs) {
    if (!cs) return CS_ERR_NULL_PTR;

    /* Free all composition trees */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_reset", 0.0f);


    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        free_comp_node(cs->compositions[i].root);
        cs->compositions[i].root = NULL;
    }
    cs->num_compositions = 0;

    /* Free workspace */
    free_comp_node(cs->workspace);
    cs->workspace = NULL;
    cs->workspace_depth = 0;

    /* Reset status */
    cs->status = CS_STATUS_IDLE;
    cs->last_error = CS_OK;

    /* Update stats */
    cs->stats.active_compositions = 0;

    return CS_OK;
}

void cs_destroy(nimcp_compositional_t* cs) {
    if (!cs) return;

    /* Free all composition trees */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_destroy", 0.0f);


    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        free_comp_node(cs->compositions[i].root);
    }

    /* Free workspace */
    free_comp_node(cs->workspace);

    /* Free arrays */
    nimcp_free(cs->primitives);
    nimcp_free(cs->compositions);
    nimcp_free(cs->rules);

    nimcp_free(cs);
    cs = NULL;
}

/*=============================================================================
 * PRIMITIVE API
 *===========================================================================*/

cs_error_t cs_register_primitive(
    nimcp_compositional_t* cs,
    const cs_primitive_t* primitive,
    uint32_t* id_out)
{
    if (!cs || !primitive || !id_out) return CS_ERR_NULL_PTR;
    if (!cs->initialized) return CS_ERR_NOT_INITIALIZED;
    if (cs->num_primitives >= cs->primitive_capacity) return CS_ERR_CAPACITY_EXCEEDED;

    /* Copy primitive data */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_register_primitiv", 0.0f);


    cs_primitive_t* new_prim = &cs->primitives[cs->num_primitives];
    *new_prim = *primitive;
    new_prim->id = cs->next_primitive_id++;
    new_prim->use_count = 0;
    new_prim->last_used = (uint64_t)time(NULL);

    *id_out = new_prim->id;
    cs->num_primitives++;
    cs->stats.primitives_registered++;
    cs->stats.active_primitives = cs->num_primitives;

    return CS_OK;
}

cs_error_t cs_get_primitive(
    nimcp_compositional_t* cs,
    uint32_t id,
    cs_primitive_t* primitive_out)
{
    if (!cs || !primitive_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_primitive", 0.0f);


    cs_primitive_t* found = find_primitive_by_id(cs, id);
    if (!found) return CS_ERR_PRIMITIVE_NOT_FOUND;

    *primitive_out = *found;
    return CS_OK;
}

cs_error_t cs_get_primitive_by_name(
    nimcp_compositional_t* cs,
    const char* name,
    cs_primitive_t* primitive_out)
{
    if (!cs || !name || !primitive_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_primitive_by_", 0.0f);


    cs_primitive_t* found = find_primitive_by_name(cs, name);
    if (!found) return CS_ERR_PRIMITIVE_NOT_FOUND;

    *primitive_out = *found;
    return CS_OK;
}

cs_error_t cs_remove_primitive(
    nimcp_compositional_t* cs,
    uint32_t id)
{
    if (!cs) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_remove_primitive", 0.0f);


    for (uint32_t i = 0; i < cs->num_primitives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_primitives > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_primitives);
        }

        if (cs->primitives[i].id == id) {
            /* Shift remaining primitives */
            for (uint32_t j = i; j < cs->num_primitives - 1; j++) {
                cs->primitives[j] = cs->primitives[j + 1];
            }
            cs->num_primitives--;
            cs->stats.active_primitives = cs->num_primitives;
            return CS_OK;
        }
    }

    return CS_ERR_PRIMITIVE_NOT_FOUND;
}

cs_error_t cs_find_primitives_by_type(
    nimcp_compositional_t* cs,
    cs_primitive_type_t type,
    cs_primitive_t* primitives_out,
    uint32_t* count_out,
    uint32_t max_count)
{
    if (!cs || !primitives_out || !count_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_find_primitives_b", 0.0f);


    uint32_t found = 0;
    for (uint32_t i = 0; i < cs->num_primitives && found < max_count; i++) {
        if (cs->primitives[i].type == type) {
            primitives_out[found++] = cs->primitives[i];
        }
    }

    *count_out = found;
    return CS_OK;
}

/*=============================================================================
 * COMPOSITION API
 *===========================================================================*/

cs_error_t cs_compose(
    nimcp_compositional_t* cs,
    cs_composition_type_t comp_type,
    uint32_t head_id,
    const cs_binding_t* bindings,
    uint32_t num_bindings,
    cs_composition_t* composition_out)
{
    if (!cs || !composition_out) return CS_ERR_NULL_PTR;
    if (!cs->initialized) return CS_ERR_NOT_INITIALIZED;
    if (cs->num_compositions >= cs->composition_capacity) return CS_ERR_CAPACITY_EXCEEDED;

    /* Validate head primitive */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_compose", 0.0f);


    cs_primitive_t* head = find_primitive_by_id(cs, head_id);
    if (!head) return CS_ERR_PRIMITIVE_NOT_FOUND;

    cs->status = CS_STATUS_COMPOSING;

    /* Create composition tree */
    cs_comp_node_t* root = alloc_comp_node();
    if (!root) {
        cs->last_error = CS_ERR_MEMORY_ALLOC;
        cs->status = CS_STATUS_ERROR;
        return CS_ERR_MEMORY_ALLOC;
    }

    root->primitive_id = head_id;
    root->depth = 0;
    root->confidence = 1.0f;

    /* Add bindings */
    uint32_t valid_bindings = 0;
    for (uint32_t i = 0; i < num_bindings && i < CS_MAX_BINDINGS; i++) {
        root->bindings[valid_bindings] = bindings[i];

        /* Validate bound primitive exists (if not variable) */
        if (!bindings[i].is_variable) {
            cs_primitive_t* bound = find_primitive_by_id(cs, bindings[i].primitive_id);
            if (!bound) {
                free_comp_node(root);
                cs->last_error = CS_ERR_BINDING_FAILED;
                cs->status = CS_STATUS_ERROR;
                return CS_ERR_BINDING_FAILED;
            }
            bound->use_count++;
            bound->last_used = (uint64_t)time(NULL);
        }
        valid_bindings++;
    }
    root->num_bindings = valid_bindings;

    /* Update head primitive usage */
    head->use_count++;
    head->last_used = (uint64_t)time(NULL);

    /* Build composition */
    cs_composition_t* comp = &cs->compositions[cs->num_compositions];
    comp->id = cs->next_composition_id++;
    comp->type = comp_type;
    comp->root = root;
    comp->total_primitives = count_tree_primitives(root);
    comp->max_depth = compute_tree_depth(root);
    comp->created_time = (uint64_t)time(NULL);

    /* Compute combined embedding */
    compute_combined_embedding(cs, comp);

    /* Compute grammaticality */
    comp->grammaticality = compute_grammaticality(cs, comp);
    comp->is_valid = (comp->grammaticality >= cs->config.grammaticality_threshold);

    /* Compute novelty */
    float novelty = 1.0f;
    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        float sim = embedding_similarity(
            comp->combined_embedding,
            cs->compositions[i].combined_embedding,
            cs->config.embedding_dim);
        if (sim > (1.0f - novelty)) {
            novelty = 1.0f - sim;
        }
    }
    comp->novelty_score = novelty;

    /* Copy to output */
    *composition_out = *comp;
    composition_out->root = copy_comp_node(root);

    cs->num_compositions++;
    cs->stats.compositions_created++;
    cs->stats.active_compositions = cs->num_compositions;
    cs->stats.mean_composition_depth =
        (cs->stats.mean_composition_depth * (cs->num_compositions - 1) + comp->max_depth) /
        cs->num_compositions;
    cs->stats.mean_novelty =
        (cs->stats.mean_novelty * (cs->num_compositions - 1) + comp->novelty_score) /
        cs->num_compositions;
    cs->stats.mean_grammaticality =
        (cs->stats.mean_grammaticality * (cs->num_compositions - 1) + comp->grammaticality) /
        cs->num_compositions;

    cs->status = CS_STATUS_IDLE;
    return CS_OK;
}

cs_error_t cs_compose_hierarchical(
    nimcp_compositional_t* cs,
    cs_composition_type_t comp_type,
    uint32_t head_id,
    const uint32_t* children,
    uint32_t num_children,
    cs_composition_t* composition_out)
{
    if (!cs || !composition_out) return CS_ERR_NULL_PTR;
    if (!cs->initialized) return CS_ERR_NOT_INITIALIZED;
    if (num_children > CS_MAX_CHILDREN) return CS_ERR_CAPACITY_EXCEEDED;

    /* Validate head primitive */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_compose_hierarchi", 0.0f);


    cs_primitive_t* head = find_primitive_by_id(cs, head_id);
    if (!head) return CS_ERR_PRIMITIVE_NOT_FOUND;

    cs->status = CS_STATUS_COMPOSING;

    /* Create root node */
    cs_comp_node_t* root = alloc_comp_node();
    if (!root) {
        cs->last_error = CS_ERR_MEMORY_ALLOC;
        cs->status = CS_STATUS_ERROR;
        return CS_ERR_MEMORY_ALLOC;
    }

    root->primitive_id = head_id;
    root->depth = 0;
    root->confidence = 1.0f;

    /* Attach children compositions */
    uint32_t max_child_depth = 0;
    for (uint32_t i = 0; i < num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_children > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)num_children);
        }

        /* Find child composition */
        cs_composition_t* child_comp = NULL;
        for (uint32_t j = 0; j < cs->num_compositions; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && cs->num_compositions > 256) {
                compositional_systematic_heartbeat("compositiona_loop",
                                 (float)(j + 1) / (float)cs->num_compositions);
            }

            if (cs->compositions[j].id == children[i]) {
                child_comp = &cs->compositions[j];
                break;
            }
        }

        if (!child_comp) {
            free_comp_node(root);
            cs->last_error = CS_ERR_INVALID_COMPOSITION;
            cs->status = CS_STATUS_ERROR;
            return CS_ERR_INVALID_COMPOSITION;
        }

        /* Check depth limit */
        if (child_comp->max_depth + 1 > cs->config.max_depth) {
            free_comp_node(root);
            cs->last_error = CS_ERR_DEPTH_EXCEEDED;
            cs->status = CS_STATUS_ERROR;
            return CS_ERR_DEPTH_EXCEEDED;
        }

        /* Copy child tree */
        root->children[i] = copy_comp_node(child_comp->root);
        if (root->children[i]) {
            /* Update child depths */
            root->children[i]->depth = 1;
            if (child_comp->max_depth > max_child_depth) {
                max_child_depth = child_comp->max_depth;
            }
        }
        root->num_children++;
    }

    /* Build composition */
    if (cs->num_compositions >= cs->composition_capacity) {
        free_comp_node(root);
        cs->last_error = CS_ERR_CAPACITY_EXCEEDED;
        cs->status = CS_STATUS_ERROR;
        return CS_ERR_CAPACITY_EXCEEDED;
    }

    cs_composition_t* comp = &cs->compositions[cs->num_compositions];
    comp->id = cs->next_composition_id++;
    comp->type = comp_type;
    comp->root = root;
    comp->total_primitives = count_tree_primitives(root);
    comp->max_depth = 1 + max_child_depth;
    comp->created_time = (uint64_t)time(NULL);

    /* Compute combined embedding */
    compute_combined_embedding(cs, comp);

    /* Compute grammaticality */
    comp->grammaticality = compute_grammaticality(cs, comp);
    comp->is_valid = (comp->grammaticality >= cs->config.grammaticality_threshold);

    /* Compute novelty */
    float novelty = 1.0f;
    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        float sim = embedding_similarity(
            comp->combined_embedding,
            cs->compositions[i].combined_embedding,
            cs->config.embedding_dim);
        if (sim > (1.0f - novelty)) {
            novelty = 1.0f - sim;
        }
    }
    comp->novelty_score = novelty;

    /* Copy to output */
    *composition_out = *comp;
    composition_out->root = copy_comp_node(root);

    cs->num_compositions++;
    cs->stats.compositions_created++;
    cs->stats.active_compositions = cs->num_compositions;

    cs->status = CS_STATUS_IDLE;
    return CS_OK;
}

cs_error_t cs_decompose(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    cs_primitive_t* primitives_out,
    cs_binding_t* bindings_out,
    uint32_t* num_primitives_out,
    uint32_t* num_bindings_out)
{
    if (!cs || !composition || !primitives_out || !bindings_out ||
        !num_primitives_out || !num_bindings_out) {
        return CS_ERR_NULL_PTR;
    }

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_decompose", 0.0f);

    if (!composition->root) return CS_ERR_INVALID_COMPOSITION;

    cs->status = CS_STATUS_DECOMPOSING;

    uint32_t prim_count = 0;
    uint32_t bind_count = 0;

    /* Extract head primitive */
    cs_primitive_t* head = find_primitive_by_id(cs, composition->root->primitive_id);
    if (head) {
        primitives_out[prim_count++] = *head;
    }

    /* Extract bindings and bound primitives */
    for (uint32_t i = 0; i < composition->root->num_bindings; i++) {
        cs_binding_t* binding = &composition->root->bindings[i];
        bindings_out[bind_count++] = *binding;

        if (!binding->is_variable) {
            cs_primitive_t* bound = find_primitive_by_id(cs, binding->primitive_id);
            if (bound) {
                primitives_out[prim_count++] = *bound;
            }
        }
    }

    *num_primitives_out = prim_count;
    *num_bindings_out = bind_count;

    cs->stats.decompositions++;
    cs->status = CS_STATUS_IDLE;

    return CS_OK;
}

cs_error_t cs_validate_composition(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    float* grammaticality_out)
{
    if (!cs || !composition || !grammaticality_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_validate_composit", 0.0f);


    cs->status = CS_STATUS_VALIDATING;

    float gram = compute_grammaticality(cs, composition);
    *grammaticality_out = gram;

    cs->stats.compositions_validated++;
    cs->status = CS_STATUS_IDLE;

    if (gram < cs->config.grammaticality_threshold) {
        return CS_ERR_VALIDATION_FAILED;
    }

    return CS_OK;
}

cs_error_t cs_get_composition(
    nimcp_compositional_t* cs,
    uint32_t id,
    cs_composition_t* composition_out)
{
    if (!cs || !composition_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_composition", 0.0f);


    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        if (cs->compositions[i].id == id) {
            *composition_out = cs->compositions[i];
            composition_out->root = copy_comp_node(cs->compositions[i].root);
            return CS_OK;
        }
    }

    return CS_ERR_INVALID_COMPOSITION;
}

cs_error_t cs_compute_novelty(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    float* novelty_out)
{
    if (!cs || !composition || !novelty_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_compute_novelty", 0.0f);


    float novelty = 1.0f;

    /* Compare to all existing compositions */
    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        if (cs->compositions[i].id == composition->id) continue;

        float sim = embedding_similarity(
            composition->combined_embedding,
            cs->compositions[i].combined_embedding,
            cs->config.embedding_dim);

        if (sim > (1.0f - novelty)) {
            novelty = 1.0f - sim;
        }
    }

    *novelty_out = novelty;
    return CS_OK;
}

/*=============================================================================
 * RULE API
 *===========================================================================*/

cs_error_t cs_apply_rule(
    nimcp_compositional_t* cs,
    uint32_t rule_id,
    const cs_primitive_t* primitives,
    uint32_t num_primitives,
    cs_composition_t* composition_out)
{
    if (!cs || !primitives || !composition_out) return CS_ERR_NULL_PTR;
    if (!cs->initialized) return CS_ERR_NOT_INITIALIZED;
    if (num_primitives == 0) return CS_ERR_INVALID_PRIMITIVE;

    /* Find rule */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_apply_rule", 0.0f);


    cs_rule_t* rule = find_rule_by_id(cs, rule_id);
    if (!rule) return CS_ERR_INVALID_RULE;

    /* Find head primitive (first primitive matching rule's head type) */
    const cs_primitive_t* head = NULL;
    for (uint32_t i = 0; i < num_primitives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_primitives > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)num_primitives);
        }

        if (primitives[i].type == rule->head_type) {
            head = &primitives[i];
            break;
        }
    }
    if (!head) return CS_ERR_RULE_MISMATCH;

    /* Build bindings from remaining primitives */
    cs_binding_t bindings[CS_MAX_BINDINGS];
    uint32_t num_bindings = 0;

    uint32_t arg_idx = 0;
    for (uint32_t i = 0; i < num_primitives && arg_idx < rule->num_args; i++) {
        if (&primitives[i] == head) continue;

        /* Check type matches expected */
        if (primitives[i].type == rule->arg_types[arg_idx]) {
            bindings[num_bindings].slot = rule->slot_order[arg_idx];
            bindings[num_bindings].primitive_id = primitives[i].id;
            bindings[num_bindings].strength = 1.0f;
            bindings[num_bindings].is_variable = false;
            num_bindings++;
            arg_idx++;
        }
    }

    /* Check we have enough arguments */
    if (num_bindings < rule->num_args) {
        return CS_ERR_RULE_MISMATCH;
    }

    /* Create composition */
    cs_error_t err = cs_compose(cs, rule->comp_type, head->id,
                                  bindings, num_bindings, composition_out);

    if (err == CS_OK) {
        rule->application_count++;
        rule->success_rate = (rule->success_rate * (rule->application_count - 1) + 1.0f) /
                            rule->application_count;
        cs->stats.rules_applied++;
    } else {
        rule->success_rate = (rule->success_rate * rule->application_count) /
                            (rule->application_count + 1);
        rule->application_count++;
    }

    return err;
}

cs_error_t cs_infer_rule(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    cs_rule_t* rule_out)
{
    if (!cs || !composition || !rule_out) return CS_ERR_NULL_PTR;
    if (!composition->root) return CS_ERR_INVALID_COMPOSITION;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_infer_rule", 0.0f);


    cs->status = CS_STATUS_LEARNING;

    /* Extract pattern from composition */
    cs_primitive_t* head = find_primitive_by_id(cs, composition->root->primitive_id);
    if (!head) {
        cs->status = CS_STATUS_ERROR;
        return CS_ERR_PRIMITIVE_NOT_FOUND;
    }

    /* Build rule */
    cs_rule_t new_rule = {0};
    new_rule.id = 0; /* Will be assigned on add */
    new_rule.comp_type = composition->type;
    new_rule.head_type = head->type;
    new_rule.direction = CS_RULE_FORWARD;
    new_rule.confidence = composition->grammaticality;
    new_rule.application_count = 1;
    new_rule.success_rate = composition->is_valid ? 1.0f : 0.0f;

    /* Extract argument types from bindings */
    new_rule.num_args = 0;
    for (uint32_t i = 0; i < composition->root->num_bindings && i < CS_MAX_BINDINGS; i++) {
        cs_binding_t* binding = &composition->root->bindings[i];
        if (!binding->is_variable) {
            cs_primitive_t* bound = find_primitive_by_id(cs, binding->primitive_id);
            if (bound) {
                new_rule.arg_types[new_rule.num_args] = bound->type;
                new_rule.slot_order[new_rule.num_args] = binding->slot;
                new_rule.num_args++;
            }
        }
    }

    /* Generate rule name */
    snprintf(new_rule.name, CS_MAX_NAME_LEN, "inferred_%s_%u",
             cs_primitive_type_string(head->type), cs->next_rule_id);

    *rule_out = new_rule;

    cs->stats.rules_learned++;
    cs->status = CS_STATUS_IDLE;

    return CS_OK;
}

cs_error_t cs_add_rule(
    nimcp_compositional_t* cs,
    const cs_rule_t* rule,
    uint32_t* id_out)
{
    if (!cs || !rule || !id_out) return CS_ERR_NULL_PTR;
    if (cs->num_rules >= cs->rule_capacity) return CS_ERR_CAPACITY_EXCEEDED;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_add_rule", 0.0f);


    cs_rule_t* new_rule = &cs->rules[cs->num_rules];
    *new_rule = *rule;
    new_rule->id = cs->next_rule_id++;

    *id_out = new_rule->id;
    cs->num_rules++;
    cs->stats.active_rules = cs->num_rules;

    return CS_OK;
}

cs_error_t cs_get_rule(
    nimcp_compositional_t* cs,
    uint32_t id,
    cs_rule_t* rule_out)
{
    if (!cs || !rule_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_rule", 0.0f);


    cs_rule_t* found = find_rule_by_id(cs, id);
    if (!found) return CS_ERR_INVALID_RULE;

    *rule_out = *found;
    return CS_OK;
}

cs_error_t cs_find_applicable_rules(
    nimcp_compositional_t* cs,
    const cs_primitive_t* primitives,
    uint32_t num_primitives,
    cs_rule_t* rules_out,
    uint32_t* count_out,
    uint32_t max_count)
{
    if (!cs || !primitives || !rules_out || !count_out) return CS_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_find_applicable_r", 0.0f);


    uint32_t found = 0;

    for (uint32_t r = 0; r < cs->num_rules && found < max_count; r++) {
        cs_rule_t* rule = &cs->rules[r];

        /* Check if we have a matching head primitive */
        bool has_head = false;
        for (uint32_t p = 0; p < num_primitives; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_primitives > 256) {
                compositional_systematic_heartbeat("compositiona_loop",
                                 (float)(p + 1) / (float)num_primitives);
            }

            if (primitives[p].type == rule->head_type) {
                has_head = true;
                break;
            }
        }
        if (!has_head) continue;

        /* Check if we have enough matching argument primitives */
        uint32_t matched_args = 0;
        for (uint32_t a = 0; a < rule->num_args; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && rule->num_args > 256) {
                compositional_systematic_heartbeat("compositiona_loop",
                                 (float)(a + 1) / (float)rule->num_args);
            }

            for (uint32_t p = 0; p < num_primitives; p++) {
                /* Phase 8: Loop progress heartbeat */
                if ((p & 0xFF) == 0 && num_primitives > 256) {
                    compositional_systematic_heartbeat("compositiona_loop",
                                     (float)(p + 1) / (float)num_primitives);
                }

                if (primitives[p].type == rule->arg_types[a]) {
                    matched_args++;
                    break;
                }
            }
        }

        if (matched_args >= rule->num_args) {
            rules_out[found++] = *rule;
        }
    }

    *count_out = found;
    return CS_OK;
}

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

cs_error_t cs_update(
    nimcp_compositional_t* cs,
    float dt_ms)
{
    if (!cs) return CS_ERR_NULL_PTR;
    if (!cs->initialized) return CS_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_update", 0.0f);


    float decay_factor = expf(-cs->config.binding_decay * dt_ms);

    /* Decay binding strengths in compositions */
    for (uint32_t i = 0; i < cs->num_compositions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_compositions > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_compositions);
        }

        cs_composition_t* comp = &cs->compositions[i];
        if (comp->root) {
            for (uint32_t j = 0; j < comp->root->num_bindings; j++) {
                comp->root->bindings[j].strength *= decay_factor;
            }
            comp->root->confidence *= decay_factor;
        }
    }

    /* Decay primitive activations */
    for (uint32_t i = 0; i < cs->num_primitives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cs->num_primitives > 256) {
            compositional_systematic_heartbeat("compositiona_loop",
                             (float)(i + 1) / (float)cs->num_primitives);
        }

        cs->primitives[i].activation *= decay_factor;
    }

    return CS_OK;
}

cs_error_t cs_get_stats(
    nimcp_compositional_t* cs,
    cs_stats_t* stats_out)
{
    if (!cs || !stats_out) return CS_ERR_NULL_PTR;

    *stats_out = cs->stats;
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_stats", 0.0f);


    return CS_OK;
}

cs_error_t cs_reset_stats(nimcp_compositional_t* cs) {
    if (!cs) return CS_ERR_NULL_PTR;

    /* Preserve active counts */
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_reset_stats", 0.0f);


    uint32_t active_prims = cs->stats.active_primitives;
    uint32_t active_comps = cs->stats.active_compositions;
    uint32_t active_rules = cs->stats.active_rules;

    memset(&cs->stats, 0, sizeof(cs_stats_t));

    cs->stats.active_primitives = active_prims;
    cs->stats.active_compositions = active_comps;
    cs->stats.active_rules = active_rules;

    return CS_OK;
}

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

cs_status_t cs_get_status(nimcp_compositional_t* cs) {
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_status", 0.0f);


    return cs ? cs->status : CS_STATUS_ERROR;
}

cs_error_t cs_get_last_error(nimcp_compositional_t* cs) {
    /* Phase 8: Heartbeat at operation start */
    compositional_systematic_heartbeat("compositiona_cs_get_last_error", 0.0f);


    return cs ? cs->last_error : CS_ERR_NULL_PTR;
}

const char* cs_error_string(cs_error_t error) {
    switch (error) {
        case CS_OK: return "OK";
        case CS_ERR_NULL_PTR: return "Null pointer";
        case CS_ERR_NOT_INITIALIZED: return "Not initialized";
        case CS_ERR_MEMORY_ALLOC: return "Memory allocation failed";
        case CS_ERR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case CS_ERR_PRIMITIVE_NOT_FOUND: return "Primitive not found";
        case CS_ERR_INVALID_PRIMITIVE: return "Invalid primitive";
        case CS_ERR_INVALID_COMPOSITION: return "Invalid composition";
        case CS_ERR_INVALID_RULE: return "Invalid rule";
        case CS_ERR_RULE_MISMATCH: return "Rule mismatch";
        case CS_ERR_BINDING_FAILED: return "Binding failed";
        case CS_ERR_DEPTH_EXCEEDED: return "Depth exceeded";
        case CS_ERR_VALIDATION_FAILED: return "Validation failed";
        case CS_ERR_DECOMPOSITION_FAILED: return "Decomposition failed";
        default: return "Unknown error";
    }
}

const char* cs_status_string(cs_status_t status) {
    switch (status) {
        case CS_STATUS_IDLE: return "Idle";
        case CS_STATUS_COMPOSING: return "Composing";
        case CS_STATUS_DECOMPOSING: return "Decomposing";
        case CS_STATUS_VALIDATING: return "Validating";
        case CS_STATUS_LEARNING: return "Learning";
        case CS_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

const char* cs_composition_type_string(cs_composition_type_t type) {
    switch (type) {
        case CS_COMPOSE_SEQUENTIAL: return "Sequential";
        case CS_COMPOSE_PARALLEL: return "Parallel";
        case CS_COMPOSE_HIERARCHICAL: return "Hierarchical";
        case CS_COMPOSE_RECURSIVE: return "Recursive";
        default: return "Unknown";
    }
}

const char* cs_primitive_type_string(cs_primitive_type_t type) {
    switch (type) {
        case CS_PRIM_ACTION: return "Action";
        case CS_PRIM_OBJECT: return "Object";
        case CS_PRIM_RELATION: return "Relation";
        case CS_PRIM_MODIFIER: return "Modifier";
        default: return "Unknown";
    }
}

const char* cs_binding_type_string(cs_binding_type_t type) {
    switch (type) {
        case CS_BIND_AGENT: return "Agent";
        case CS_BIND_PATIENT: return "Patient";
        case CS_BIND_INSTRUMENT: return "Instrument";
        case CS_BIND_LOCATION: return "Location";
        case CS_BIND_TIME: return "Time";
        case CS_BIND_MANNER: return "Manner";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void compositional_systematic_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_compositional_systematic_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int compositional_systematic_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "compositional_systematic_training_begin: NULL argument");
        return -1;
    }
    compositional_systematic_heartbeat_instance(NULL, "compositional_systematic_training_begin", 0.0f);
    return 0;
}

int compositional_systematic_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "compositional_systematic_training_end: NULL argument");
        return -1;
    }
    compositional_systematic_heartbeat_instance(NULL, "compositional_systematic_training_end", 1.0f);
    return 0;
}

int compositional_systematic_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "compositional_systematic_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    compositional_systematic_heartbeat_instance(NULL, "compositional_systematic_training_step", progress);
    return 0;
}
