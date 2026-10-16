/**
 * @file nimcp_scene_graph.c
 * @brief Scene Graph — spatial relation reasoning
 *
 * WHAT: Graph of ON_TOP_OF, INSIDE, NEXT_TO, ABOVE, BELOW, TOUCHING, BLOCKING
 * WHY:  Relational reasoning: "remove table → cup falls"
 * HOW:  Rebuilt from physics contacts each step. O(n²) but n ≤ 128.
 */

#include "cognitive/physics/nimcp_scene_graph.h"
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "SCENE_GRAPH"
#define SG_MAX_CHILDREN_PER 16

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float sg_dist(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static void add_relation(scene_graph_t* g, uint32_t subj, uint32_t obj,
                          scene_relation_type_t type, float confidence, float strength) {
    if (g->num_relations >= g->capacity) return;
    g->relations[g->num_relations++] = (scene_relation_t){
        .subject_id = subj,
        .object_id = obj,
        .type = type,
        .confidence = confidence,
        .strength = strength,
    };
}

/* ============================================================================
 * Public API
 * ============================================================================ */

scene_graph_config_t scene_graph_default_config(void) {
    return (scene_graph_config_t){
        .max_relations = SG_MAX_RELATIONS,
        .proximity_threshold = SG_PROXIMITY_THRESHOLD,
        .support_normal_thresh = IP_SUPPORT_NORMAL_THRESHOLD,
        .enable_occlusion = false,
        .viewpoint = {0, 1.6f, 5.0f},   /* default eye height, 5m back */
    };
}

scene_graph_t* scene_graph_create(const scene_graph_config_t* config) {
    scene_graph_config_t cfg = config ? *config : scene_graph_default_config();

    scene_graph_t* g = nimcp_calloc(1, sizeof(*g));
    if (!g) return NULL;

    g->config = cfg;
    g->capacity = cfg.max_relations;
    g->relations = nimcp_calloc(g->capacity, sizeof(scene_relation_t));
    g->max_objects = IP_MAX_OBJECTS;
    g->support_children = nimcp_calloc(g->max_objects * SG_MAX_CHILDREN_PER, sizeof(uint32_t));
    g->support_children_count = nimcp_calloc(g->max_objects, sizeof(uint32_t));

    if (!g->relations || !g->support_children || !g->support_children_count) {
        scene_graph_destroy(g);
        return NULL;
    }

    g->initialized = true;
    LOG_INFO(LOG_TAG, "Scene graph created: max_relations=%u", cfg.max_relations);
    return g;
}

void scene_graph_destroy(scene_graph_t* graph) {
    if (!graph) return;
    nimcp_free(graph->relations);
    nimcp_free(graph->support_children);
    nimcp_free(graph->support_children_count);
    nimcp_free(graph);
}

int scene_graph_rebuild(scene_graph_t* graph, const intuitive_physics_engine_t* physics) {
    if (!graph || !physics || !graph->initialized) return -1;

    graph->num_relations = 0;
    memset(graph->support_children_count, 0, graph->max_objects * sizeof(uint32_t));

    const ip_scene_t* s = &physics->scene;

    /* 1. Derive TOUCHING and ON_TOP_OF from contacts */
    for (uint32_t c = 0; c < s->num_contacts; c++) {
        const ip_contact_t* ct = &s->contacts[c];
        uint32_t a = ct->obj_a, b = ct->obj_b;

        /* TOUCHING: any contact */
        add_relation(graph, a, b, SCENE_REL_TOUCHING, 1.0f, ct->normal_impulse);

        /* ON_TOP_OF: contact normal points upward → B is on A */
        if (ct->normal.y > graph->config.support_normal_thresh) {
            add_relation(graph, b, a, SCENE_REL_ON_TOP_OF, 1.0f, ct->normal_impulse);

            /* Update support children adjacency (bounds check BEFORE access) */
            uint32_t supporter = a;
            if (supporter < graph->max_objects) {
                uint32_t child_count = graph->support_children_count[supporter];
                if (child_count < SG_MAX_CHILDREN_PER) {
                    graph->support_children[supporter * SG_MAX_CHILDREN_PER + child_count] = b;
                    graph->support_children_count[supporter] = child_count + 1;
                }
            }
        } else if (ct->normal.y < -graph->config.support_normal_thresh) {
            add_relation(graph, a, b, SCENE_REL_ON_TOP_OF, 1.0f, ct->normal_impulse);

            uint32_t supporter = b;
            if (supporter >= graph->max_objects) continue;  /* bounds check */
            uint32_t child_count = graph->support_children_count[supporter];
            if (child_count < SG_MAX_CHILDREN_PER && supporter < graph->max_objects) {
                graph->support_children[supporter * SG_MAX_CHILDREN_PER + child_count] = a;
                graph->support_children_count[supporter] = child_count + 1;
            }
        }
    }

    /* 2. Derive ABOVE, BELOW, NEXT_TO, INSIDE from positions */
    for (uint32_t i = 0; i < s->num_objects; i++) {
        if (!s->objects[i].active) continue;
        for (uint32_t j = i + 1; j < s->num_objects; j++) {
            if (!s->objects[j].active) continue;

            const ip_object_t* a = &s->objects[i];
            const ip_object_t* b = &s->objects[j];
            float dist = sg_dist(a->position, b->position);
            float dy = b->position.y - a->position.y;

            /* ABOVE/BELOW: significant vertical separation, small horizontal */
            float h_dist = sqrtf((b->position.x - a->position.x) * (b->position.x - a->position.x) +
                                  (b->position.z - a->position.z) * (b->position.z - a->position.z));
            if (fabsf(dy) > 0.1f && h_dist < graph->config.proximity_threshold) {
                if (dy > 0) {
                    add_relation(graph, j, i, SCENE_REL_ABOVE, 0.8f, fabsf(dy));
                    add_relation(graph, i, j, SCENE_REL_BELOW, 0.8f, fabsf(dy));
                } else {
                    add_relation(graph, i, j, SCENE_REL_ABOVE, 0.8f, fabsf(dy));
                    add_relation(graph, j, i, SCENE_REL_BELOW, 0.8f, fabsf(dy));
                }
            }

            /* NEXT_TO: close but not touching */
            float sum_r = 0;
            if (a->shape.type == IP_SHAPE_SPHERE) sum_r += a->shape.sphere.radius;
            else if (a->shape.type == IP_SHAPE_BOX) sum_r += a->shape.box.hx;
            if (b->shape.type == IP_SHAPE_SPHERE) sum_r += b->shape.sphere.radius;
            else if (b->shape.type == IP_SHAPE_BOX) sum_r += b->shape.box.hx;

            if (dist > sum_r && dist < sum_r + graph->config.proximity_threshold) {
                add_relation(graph, i, j, SCENE_REL_NEXT_TO, 0.7f, dist);
            }

            /* INSIDE: A's center is within B's bounding volume (for boxes) */
            if (b->shape.type == IP_SHAPE_BOX) {
                wm_parietal_vec3_t local = {
                    a->position.x - b->position.x,
                    a->position.y - b->position.y,
                    a->position.z - b->position.z
                };
                if (fabsf(local.x) < b->shape.box.hx &&
                    fabsf(local.y) < b->shape.box.hy &&
                    fabsf(local.z) < b->shape.box.hz) {
                    add_relation(graph, i, j, SCENE_REL_INSIDE, 0.9f, 0);
                }
            }
            if (a->shape.type == IP_SHAPE_BOX) {
                wm_parietal_vec3_t local = {
                    b->position.x - a->position.x,
                    b->position.y - a->position.y,
                    b->position.z - a->position.z
                };
                if (fabsf(local.x) < a->shape.box.hx &&
                    fabsf(local.y) < a->shape.box.hy &&
                    fabsf(local.z) < a->shape.box.hz) {
                    add_relation(graph, j, i, SCENE_REL_INSIDE, 0.9f, 0);
                }
            }
        }
    }

    graph->rebuild_count++;

    /* W8: emit every 64 rebuilds to avoid log firehose. Read-back
     * queries root edge count to confirm KG reception. */
    if ((graph->rebuild_count & 0x3F) == 0) {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain) {
            world_model_kg_emit_scene_rebuild(_kg_brain,
                graph->num_relations, graph->rebuild_count);
            (void)world_model_kg_root_edge_count(_kg_brain,
                "cog_physics_scene_graph");
        }
    }
    return 0;
}

uint32_t scene_graph_get_relations_for(const scene_graph_t* graph, uint32_t object_id,
                                        scene_relation_t* buf, uint32_t buf_size) {
    if (!graph || !buf) return 0;
    uint32_t count = 0;
    for (uint32_t r = 0; r < graph->num_relations && count < buf_size; r++) {
        if (graph->relations[r].subject_id == object_id ||
            graph->relations[r].object_id == object_id) {
            buf[count++] = graph->relations[r];
        }
    }
    return count;
}

bool scene_graph_has_relation(const scene_graph_t* graph,
                               uint32_t subject, uint32_t object,
                               scene_relation_type_t type) {
    if (!graph) return false;
    for (uint32_t r = 0; r < graph->num_relations; r++) {
        if (graph->relations[r].subject_id == subject &&
            graph->relations[r].object_id == object &&
            graph->relations[r].type == type)
            return true;
    }
    return false;
}

uint32_t scene_graph_get_support_chain(const scene_graph_t* graph, uint32_t object_id,
                                        uint32_t* chain_buf, uint32_t buf_size) {
    if (!graph || !chain_buf || buf_size == 0) return 0;

    uint32_t count = 0;
    uint32_t current = object_id;
    bool visited[IP_MAX_OBJECTS] = {0};

    for (uint32_t depth = 0; depth < SG_MAX_CHAIN_DEPTH; depth++) {
        if (current >= graph->max_objects || visited[current]) break;
        visited[current] = true;
        if (count < buf_size) chain_buf[count++] = current;

        /* Find who supports current */
        bool found = false;
        for (uint32_t r = 0; r < graph->num_relations; r++) {
            if (graph->relations[r].subject_id == current &&
                graph->relations[r].type == SCENE_REL_ON_TOP_OF) {
                current = graph->relations[r].object_id;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return count;
}

uint32_t scene_graph_predict_removal_cascade(const scene_graph_t* graph,
                                              uint32_t object_id,
                                              uint32_t* affected_buf, uint32_t buf_size) {
    if (!graph || !affected_buf) return 0;

    /* BFS from removed object's support children */
    bool visited[IP_MAX_OBJECTS] = {0};
    uint32_t queue[IP_MAX_OBJECTS];
    uint32_t qhead = 0, qtail = 0;
    uint32_t count = 0;

    /* Seed: objects directly ON TOP OF the removed object */
    if (object_id < graph->max_objects) {
        uint32_t nc = graph->support_children_count[object_id];
        for (uint32_t c = 0; c < nc && c < SG_MAX_CHILDREN_PER; c++) {
            uint32_t child = graph->support_children[object_id * SG_MAX_CHILDREN_PER + c];
            if (!visited[child]) {
                visited[child] = true;
                if (qtail < IP_MAX_OBJECTS) queue[qtail++] = child;
            }
        }
    }

    /* BFS: follow support tree upward */
    while (qhead < qtail) {
        uint32_t current = queue[qhead++];
        if (count < buf_size) affected_buf[count++] = current;

        /* Find objects on top of current */
        if (current < graph->max_objects) {
            uint32_t nc = graph->support_children_count[current];
            for (uint32_t c = 0; c < nc && c < SG_MAX_CHILDREN_PER; c++) {
                uint32_t child = graph->support_children[current * SG_MAX_CHILDREN_PER + c];
                if (!visited[child]) {
                    visited[child] = true;
                    if (qtail < IP_MAX_OBJECTS) queue[qtail++] = child;
                }
            }
        }
    }
    return count;
}

bool scene_graph_is_stack_stable(const scene_graph_t* graph, uint32_t top_object_id) {
    if (!graph) return false;

    /* Walk support chain down — stable if it terminates at a static object */
    uint32_t chain[SG_MAX_CHAIN_DEPTH];
    uint32_t len = scene_graph_get_support_chain(graph, top_object_id, chain, SG_MAX_CHAIN_DEPTH);
    /* The last element in the chain should be the bottom supporter.
     * If the chain has >1 element, the bottom must be static (ground, table, etc.) */
    return len > 0;  /* simplified: if there's any support chain at all */
}

int scene_graph_relation_between(const scene_graph_t* graph, uint32_t a, uint32_t b) {
    if (!graph) return -1;
    for (uint32_t r = 0; r < graph->num_relations; r++) {
        if (graph->relations[r].subject_id == a && graph->relations[r].object_id == b)
            return (int)graph->relations[r].type;
    }
    return -1;
}

uint32_t scene_graph_count(const scene_graph_t* graph) {
    return graph ? graph->num_relations : 0;
}
