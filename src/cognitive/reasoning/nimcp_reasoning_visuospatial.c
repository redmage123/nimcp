/**
 * @file nimcp_reasoning_visuospatial.c
 * @brief Visuospatial Reasoning -- implementation
 */

#include "cognitive/reasoning/nimcp_reasoning_visuospatial.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "reasoning_visuospatial"

struct reasoning_visuospatial {
    visuospatial_config_t config;
    vs_object_t objects[VS_MAX_OBJECTS];
    bool object_active[VS_MAX_OBJECTS];
    uint32_t num_objects;
    uint32_t next_id;
    vs_relation_t relations[VS_MAX_RELATIONS];
    uint32_t num_relations;
    visuospatial_stats_t stats;
    uint64_t total_query_time_us;
};

static int find_object_index(const reasoning_visuospatial_t* vs, uint32_t id) {
    if (!vs) return -1;
    for (uint32_t i = 0; i < VS_MAX_OBJECTS; i++)
        if (vs->object_active[i] && vs->objects[i].id == id) return (int)i;
    return -1;
}

static int find_free_slot(const reasoning_visuospatial_t* vs) {
    if (!vs) return -1;
    uint32_t max = vs->config.max_objects;
    if (max > VS_MAX_OBJECTS) max = VS_MAX_OBJECTS;
    for (uint32_t i = 0; i < max; i++)
        if (!vs->object_active[i]) return (int)i;
    return -1;
}

static float point_distance(vs_point_t a, vs_point_t b, bool e3d) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = e3d ? (b.z - a.z) : 0.0f;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static bool point_in_bounds(vs_point_t p, vs_bounds_t b, bool e3d) {
    if (p.x < b.min.x || p.x > b.max.x) return false;
    if (p.y < b.min.y || p.y > b.max.y) return false;
    if (e3d && (p.z < b.min.z || p.z > b.max.z)) return false;
    return true;
}

static bool bounds_overlap(vs_bounds_t a, vs_bounds_t b, bool e3d) {
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (e3d && (a.max.z < b.min.z || a.min.z > b.max.z)) return false;
    return true;
}

static bool bounds_touching(vs_bounds_t a, vs_bounds_t b, bool e3d) {
    float eps = 1e-4f;
    bool ox = (a.max.x >= b.min.x - eps) && (a.min.x <= b.max.x + eps);
    bool oy = (a.max.y >= b.min.y - eps) && (a.min.y <= b.max.y + eps);
    bool oz = !e3d || ((a.max.z >= b.min.z - eps) && (a.min.z <= b.max.z + eps));
    if (!ox || !oy || !oz) return false;
    bool ex = (fabsf(a.max.x - b.min.x) < eps) || (fabsf(a.min.x - b.max.x) < eps);
    bool ey = (fabsf(a.max.y - b.min.y) < eps) || (fabsf(a.min.y - b.max.y) < eps);
    bool ez = e3d && ((fabsf(a.max.z - b.min.z) < eps) || (fabsf(a.min.z - b.max.z) < eps));
    return ex || ey || ez;
}

static bool bfs_path_exists(const reasoning_visuospatial_t* vs, uint32_t src_id, uint32_t dst_id) {
    if (src_id == dst_id) return true;
    uint32_t queue[VS_MAX_OBJECTS]; bool visited[VS_MAX_OBJECTS];
    memset(visited, 0, sizeof(visited));
    int si = find_object_index(vs, src_id); if (si < 0) return false;
    uint32_t head = 0, tail = 0; queue[tail++] = src_id; visited[si] = true;
    while (head < tail) {
        uint32_t cur = queue[head++];
        for (uint32_t r = 0; r < vs->num_relations; r++) {
            if (vs->relations[r].type != VS_RELATION_NEAR) continue;
            uint32_t nb = UINT32_MAX;
            if (vs->relations[r].object_a_id == cur) nb = vs->relations[r].object_b_id;
            else if (vs->relations[r].object_b_id == cur) nb = vs->relations[r].object_a_id;
            if (nb == UINT32_MAX) continue;
            if (nb == dst_id) return true;
            int ni = find_object_index(vs, nb);
            if (ni < 0 || visited[ni]) continue;
            visited[ni] = true;
            if (tail < VS_MAX_OBJECTS) queue[tail++] = nb;
        }
    }
    return false;
}

static inline void add_rel(reasoning_visuospatial_t* vs, vs_relation_type_t t,
                            uint32_t a, uint32_t b, float c, int* cnt) {
    if (vs->num_relations >= VS_MAX_RELATIONS) return;
    vs->relations[vs->num_relations] = (vs_relation_t){t, a, b, c, 0};
    vs->num_relations++;
    (*cnt)++;
}

visuospatial_config_t reasoning_visuospatial_default_config(void) {
    visuospatial_config_t c; memset(&c, 0, sizeof(c));
    c.max_objects = VS_MAX_OBJECTS;
    c.proximity_threshold = VS_DEFAULT_PROXIMITY_THRESHOLD;
    c.enable_3d = false;
    return c;
}

reasoning_visuospatial_t* reasoning_visuospatial_create(const visuospatial_config_t* config) {
    reasoning_visuospatial_t* vs = (reasoning_visuospatial_t*)nimcp_calloc(1, sizeof(*vs));
    if (!vs) return NULL;
    vs->config = config ? *config : reasoning_visuospatial_default_config();
    if (vs->config.max_objects > VS_MAX_OBJECTS) vs->config.max_objects = VS_MAX_OBJECTS;
    if (vs->config.max_objects == 0) vs->config.max_objects = VS_MAX_OBJECTS;
    if (vs->config.proximity_threshold <= 0.0f) vs->config.proximity_threshold = VS_DEFAULT_PROXIMITY_THRESHOLD;
    return vs;
}

void reasoning_visuospatial_destroy(reasoning_visuospatial_t* vs) {
    if (vs) nimcp_free(vs);
}

int reasoning_visuospatial_add_object(reasoning_visuospatial_t* vs, const char* name, vs_point_t pos) {
    if (!vs || !name) return -1;
    int s = find_free_slot(vs); if (s < 0) return -1;
    vs_object_t* o = &vs->objects[s]; memset(o, 0, sizeof(*o));
    o->id = vs->next_id++;
    strncpy(o->name, name, VS_MAX_NAME_LEN - 1); o->name[VS_MAX_NAME_LEN - 1] = '\0';
    o->position = pos; o->has_bounds = false;
    vs->object_active[s] = true; vs->num_objects++; vs->stats.num_objects = vs->num_objects;
    return (int)o->id;
}

int reasoning_visuospatial_add_object_with_bounds(reasoning_visuospatial_t* vs, const char* name, vs_point_t pos, vs_bounds_t bounds) {
    if (!vs || !name) return -1;
    int s = find_free_slot(vs); if (s < 0) return -1;
    vs_object_t* o = &vs->objects[s]; memset(o, 0, sizeof(*o));
    o->id = vs->next_id++;
    strncpy(o->name, name, VS_MAX_NAME_LEN - 1); o->name[VS_MAX_NAME_LEN - 1] = '\0';
    o->position = pos; o->bounds = bounds; o->has_bounds = true;
    vs->object_active[s] = true; vs->num_objects++; vs->stats.num_objects = vs->num_objects;
    return (int)o->id;
}

int reasoning_visuospatial_remove_object(reasoning_visuospatial_t* vs, uint32_t oid) {
    if (!vs) return -1;
    int idx = find_object_index(vs, oid); if (idx < 0) return -1;
    vs->object_active[idx] = false; vs->num_objects--; vs->stats.num_objects = vs->num_objects;
    uint32_t w = 0;
    for (uint32_t r = 0; r < vs->num_relations; r++) {
        if (vs->relations[r].object_a_id != oid && vs->relations[r].object_b_id != oid && vs->relations[r].reference_id != oid) {
            if (w != r) vs->relations[w] = vs->relations[r]; w++;
        }
    }
    vs->num_relations = w; vs->stats.num_relations = vs->num_relations;
    return 0;
}

int reasoning_visuospatial_move_object(reasoning_visuospatial_t* vs, uint32_t oid, vs_point_t np) {
    if (!vs) return -1;
    int idx = find_object_index(vs, oid); if (idx < 0) return -1;
    if (vs->objects[idx].has_bounds) {
        float dx = np.x - vs->objects[idx].position.x;
        float dy = np.y - vs->objects[idx].position.y;
        float dz = np.z - vs->objects[idx].position.z;
        vs->objects[idx].bounds.min.x += dx; vs->objects[idx].bounds.min.y += dy; vs->objects[idx].bounds.min.z += dz;
        vs->objects[idx].bounds.max.x += dx; vs->objects[idx].bounds.max.y += dy; vs->objects[idx].bounds.max.z += dz;
    }
    vs->objects[idx].position = np;
    return 0;
}

int reasoning_visuospatial_get_object(const reasoning_visuospatial_t* vs, uint32_t oid, vs_object_t* out) {
    if (!vs || !out) return -1;
    int idx = find_object_index(vs, oid); if (idx < 0) return -1;
    *out = vs->objects[idx]; return 0;
}

int reasoning_visuospatial_add_relation(reasoning_visuospatial_t* vs, vs_relation_type_t type, uint32_t a, uint32_t b, float conf) {
    if (!vs || vs->num_relations >= VS_MAX_RELATIONS) return -1;
    if (find_object_index(vs, a) < 0 || find_object_index(vs, b) < 0) return -1;
    vs->relations[vs->num_relations] = (vs_relation_t){type, a, b, conf, 0};
    vs->num_relations++; vs->stats.num_relations = vs->num_relations;
    return 0;
}

int reasoning_visuospatial_infer_relations(reasoning_visuospatial_t* vs) {
    if (!vs) return -1;
    vs->num_relations = 0;
    float th = vs->config.proximity_threshold; bool e3d = vs->config.enable_3d; int cnt = 0;
    for (uint32_t i = 0; i < VS_MAX_OBJECTS; i++) {
        if (!vs->object_active[i]) continue;
        vs_object_t* a = &vs->objects[i];
        for (uint32_t j = i + 1; j < VS_MAX_OBJECTS; j++) {
            if (!vs->object_active[j]) continue;
            vs_object_t* b = &vs->objects[j];
            float dist = point_distance(a->position, b->position, e3d);
            if (a->position.y > b->position.y + th) add_rel(vs, VS_RELATION_ABOVE, a->id, b->id, 1.0f, &cnt);
            else if (a->position.y < b->position.y - th) add_rel(vs, VS_RELATION_BELOW, a->id, b->id, 1.0f, &cnt);
            if (a->position.x < b->position.x - th) add_rel(vs, VS_RELATION_LEFT_OF, a->id, b->id, 1.0f, &cnt);
            else if (a->position.x > b->position.x + th) add_rel(vs, VS_RELATION_RIGHT_OF, a->id, b->id, 1.0f, &cnt);
            if (e3d) {
                if (a->position.z < b->position.z - th) add_rel(vs, VS_RELATION_IN_FRONT, a->id, b->id, 1.0f, &cnt);
                else if (a->position.z > b->position.z + th) add_rel(vs, VS_RELATION_BEHIND, a->id, b->id, 1.0f, &cnt);
            }
            if (dist < th) add_rel(vs, VS_RELATION_NEAR, a->id, b->id, 1.0f - (dist / th), &cnt);
            else if (dist >= 3.0f * th) add_rel(vs, VS_RELATION_FAR, a->id, b->id, 1.0f, &cnt);
            if (a->has_bounds && b->has_bounds) {
                if (point_in_bounds(a->position, b->bounds, e3d)) add_rel(vs, VS_RELATION_INSIDE, a->id, b->id, 1.0f, &cnt);
                if (point_in_bounds(b->position, a->bounds, e3d)) add_rel(vs, VS_RELATION_INSIDE, b->id, a->id, 1.0f, &cnt);
                if (bounds_overlap(a->bounds, b->bounds, e3d)) add_rel(vs, VS_RELATION_OVERLAPPING, a->id, b->id, 1.0f, &cnt);
                if (bounds_touching(a->bounds, b->bounds, e3d)) add_rel(vs, VS_RELATION_TOUCHING, a->id, b->id, 1.0f, &cnt);
            } else if (b->has_bounds && point_in_bounds(a->position, b->bounds, e3d)) {
                add_rel(vs, VS_RELATION_INSIDE, a->id, b->id, 1.0f, &cnt);
            } else if (a->has_bounds && point_in_bounds(b->position, a->bounds, e3d)) {
                add_rel(vs, VS_RELATION_INSIDE, b->id, a->id, 1.0f, &cnt);
            }
        }
    }
    vs->stats.num_relations = vs->num_relations;
    return cnt;
}

static int qr_relation(const reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    r->holds = false; r->confidence = 0.0f;
    for (uint32_t i = 0; i < vs->num_relations; i++) {
        if (vs->relations[i].type == q->relation && vs->relations[i].object_a_id == q->object_a_id && vs->relations[i].object_b_id == q->object_b_id) {
            r->holds = true; r->confidence = vs->relations[i].confidence;
            snprintf(r->explanation, VS_EXPLANATION_LEN, "Relation %s holds", reasoning_visuospatial_get_relation_name(q->relation));
            return 0;
        }
    }
    snprintf(r->explanation, VS_EXPLANATION_LEN, "Relation %s does NOT hold", reasoning_visuospatial_get_relation_name(q->relation));
    return 0;
}
static int qr_distance(const reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    float d = reasoning_visuospatial_distance(vs, q->object_a_id, q->object_b_id);
    if (isnan(d)) return -1;
    r->distance = d; r->confidence = 1.0f;
    snprintf(r->explanation, VS_EXPLANATION_LEN, "Distance: %.3f", d);
    return 0;
}
static int qr_nearest(const reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    int si = find_object_index(vs, q->object_a_id); if (si < 0) return -1;
    vs_point_t sp = vs->objects[si].position; float md = FLT_MAX; uint32_t nn = UINT32_MAX;
    for (uint32_t i = 0; i < VS_MAX_OBJECTS; i++) {
        if (!vs->object_active[i] || vs->objects[i].id == q->object_a_id) continue;
        float d = point_distance(sp, vs->objects[i].position, vs->config.enable_3d);
        if (d < md) { md = d; nn = vs->objects[i].id; }
    }
    if (nn == UINT32_MAX) { r->nearest_id = 0; r->confidence = 0.0f; return 0; }
    r->nearest_id = nn; r->distance = md; r->confidence = 1.0f;
    return 0;
}
static int qr_contains(const reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    int ia = find_object_index(vs, q->object_a_id), ib = find_object_index(vs, q->object_b_id);
    if (ia < 0 || ib < 0) return -1;
    if (!vs->objects[ib].has_bounds) { r->holds = false; r->confidence = 0.0f; return 0; }
    r->holds = point_in_bounds(vs->objects[ia].position, vs->objects[ib].bounds, vs->config.enable_3d);
    r->confidence = r->holds ? 1.0f : 0.0f;
    return 0;
}
static int qr_path(const reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    r->holds = bfs_path_exists(vs, q->object_a_id, q->object_b_id);
    r->confidence = r->holds ? 1.0f : 0.0f;
    return 0;
}
static int qr_region(const reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    r->num_objects_in_region = 0;
    for (uint32_t i = 0; i < VS_MAX_OBJECTS; i++) {
        if (!vs->object_active[i]) continue;
        if (point_in_bounds(vs->objects[i].position, q->region, vs->config.enable_3d))
            if (r->num_objects_in_region < VS_MAX_REGION_RESULTS)
                r->objects_in_region[r->num_objects_in_region++] = vs->objects[i].id;
    }
    r->confidence = 1.0f;
    return 0;
}

int reasoning_visuospatial_query(reasoning_visuospatial_t* vs, const vs_query_t* q, vs_result_t* r) {
    if (!vs || !q || !r) return -1;
    memset(r, 0, sizeof(*r));
    uint64_t t0 = nimcp_time_now_us();
    int rc = 0;
    switch (q->type) {
        case VS_QUERY_RELATION: rc = qr_relation(vs, q, r); break;
        case VS_QUERY_DISTANCE: rc = qr_distance(vs, q, r); break;
        case VS_QUERY_NEAREST: rc = qr_nearest(vs, q, r); break;
        case VS_QUERY_CONTAINS: rc = qr_contains(vs, q, r); break;
        case VS_QUERY_PATH: rc = qr_path(vs, q, r); break;
        case VS_QUERY_OBJECTS_IN_REGION: rc = qr_region(vs, q, r); break;
        default: rc = -1; break;
    }
    uint64_t dt = nimcp_time_now_us() - t0;
    vs->stats.num_queries++;
    vs->total_query_time_us += dt;
    vs->stats.avg_query_time_us = (float)vs->total_query_time_us / (float)vs->stats.num_queries;
    return rc;
}

float reasoning_visuospatial_distance(const reasoning_visuospatial_t* vs, uint32_t a, uint32_t b) {
    if (!vs) return NAN;
    int ia = find_object_index(vs, a), ib = find_object_index(vs, b);
    if (ia < 0 || ib < 0) return NAN;
    return point_distance(vs->objects[ia].position, vs->objects[ib].position, vs->config.enable_3d);
}

int reasoning_visuospatial_get_stats(const reasoning_visuospatial_t* vs, visuospatial_stats_t* s) {
    if (!vs || !s) return -1;
    *s = vs->stats; return 0;
}

const char* reasoning_visuospatial_get_relation_name(vs_relation_type_t type) {
    switch (type) {
        case VS_RELATION_ABOVE: return "ABOVE"; case VS_RELATION_BELOW: return "BELOW";
        case VS_RELATION_LEFT_OF: return "LEFT_OF"; case VS_RELATION_RIGHT_OF: return "RIGHT_OF";
        case VS_RELATION_IN_FRONT: return "IN_FRONT"; case VS_RELATION_BEHIND: return "BEHIND";
        case VS_RELATION_INSIDE: return "INSIDE"; case VS_RELATION_OUTSIDE: return "OUTSIDE";
        case VS_RELATION_NEAR: return "NEAR"; case VS_RELATION_FAR: return "FAR";
        case VS_RELATION_TOUCHING: return "TOUCHING"; case VS_RELATION_OVERLAPPING: return "OVERLAPPING";
        case VS_RELATION_BETWEEN: return "BETWEEN"; default: return "UNKNOWN";
    }
}
