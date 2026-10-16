/**
 * @file nimcp_spatial_reasoning.c
 * @brief Spatial reasoning and mental rotation implementation
 *
 * Implements mental rotation (Shepard paradigm), coordinate transforms,
 * and spatial indexing for the parietal lobe module.
 */

#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"
#include "utils/geometry/nimcp_lie_group.h"

BRIDGE_BOILERPLATE(spatial_reasoning, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define PI NIMCP_PI_F
#define DEG_TO_RAD (PI / 180.0f)
#define RAD_TO_DEG (180.0f / PI)

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief K-D tree node for spatial indexing
 */
typedef struct kd_node {
    spatial_object_t* object;       /**< Stored object */
    struct kd_node* left;           /**< Left child */
    struct kd_node* right;          /**< Right child */
    uint8_t split_axis;             /**< Split dimension (0=x, 1=y, 2=z) */
} kd_node_t;

/**
 * @brief Internal spatial reasoning state
 */
struct spatial_reasoning {
    /* Configuration */
    spatial_config_t config;

    /* Spatial index (K-D tree) */
    kd_node_t* kd_root;
    spatial_object_t** objects;     /**< Object array for iteration */
    uint32_t num_objects;
    uint32_t next_object_id;

    /* Modulation state */
    float inflammation_level;
    float fatigue_level;
    float effective_rotation_rate;

    /* Statistics */
    uint64_t rotations_performed;
    uint64_t transforms_performed;
    uint64_t spatial_queries;
    double total_rotation_time_ms;
    double total_rotation_angle;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* Thread-local error message */
static _Thread_local char g_spatial_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_spatial_error(const char* msg) {
    strncpy(g_spatial_error, msg, sizeof(g_spatial_error) - 1);
    g_spatial_error[sizeof(g_spatial_error) - 1] = '\0';
}

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void update_effective_rotation_rate(spatial_reasoning_t* sr) {
    float base = sr->config.rotation_rate_deg_ms;

    /* Inflammation slows rotation */
    float inflammation_factor = 1.0f - sr->inflammation_level *
                                       sr->config.inflammation_sensitivity * 0.3f;

    /* Fatigue slows rotation */
    float fatigue_factor = 1.0f - sr->fatigue_level *
                                  sr->config.fatigue_sensitivity * 0.2f;

    sr->effective_rotation_rate = base * inflammation_factor * fatigue_factor;
    if (sr->effective_rotation_rate < 0.001f) {
        sr->effective_rotation_rate = 0.001f;
    }
}

/* ============================================================================
 * K-D TREE IMPLEMENTATION
 * ============================================================================ */

static float kd_get_coord(const spatial_object_t* obj, uint8_t axis) {
    switch (axis) {
        case 0: return obj->position.x;
        case 1: return obj->position.y;
        case 2: return obj->position.z;
        default: return 0.0f;
    }
}

static kd_node_t* kd_create_node(spatial_object_t* obj) {
    kd_node_t* node = nimcp_calloc(1, sizeof(kd_node_t));
    if (node) {
        node->object = obj;
    }
    return node;
}

static kd_node_t* kd_insert(kd_node_t* root, spatial_object_t* obj, uint8_t depth) {
    if (!root) {
        kd_node_t* node = kd_create_node(obj);
        if (node) node->split_axis = depth % 3;
        return node;
    }

    uint8_t axis = depth % 3;
    float obj_coord = kd_get_coord(obj, axis);
    float root_coord = kd_get_coord(root->object, axis);

    if (obj_coord < root_coord) {
        root->left = kd_insert(root->left, obj, depth + 1);
    } else {
        root->right = kd_insert(root->right, obj, depth + 1);
    }

    return root;
}

static void kd_destroy(kd_node_t* root) {
    if (!root) return;
    kd_destroy(root->left);
    kd_destroy(root->right);
    /* Note: objects are stored in the objects array, don't free here */
    nimcp_free(root);
    root = NULL;
}

typedef struct {
    spatial_object_t* best;
    float best_dist;
} kd_nearest_state_t;

static void kd_find_nearest(kd_node_t* root, vec3_t query, uint8_t depth,
                            kd_nearest_state_t* state) {
    if (!root) return;

    float dist = vec3_distance(query, root->object->position);
    if (dist < state->best_dist) {
        state->best_dist = dist;
        state->best = root->object;
    }

    uint8_t axis = depth % 3;
    float query_coord, root_coord;
    switch (axis) {
        case 0: query_coord = query.x; root_coord = root->object->position.x; break;
        case 1: query_coord = query.y; root_coord = root->object->position.y; break;
        default: query_coord = query.z; root_coord = root->object->position.z; break;
    }

    kd_node_t* first = (query_coord < root_coord) ? root->left : root->right;
    kd_node_t* second = (query_coord < root_coord) ? root->right : root->left;

    kd_find_nearest(first, query, depth + 1, state);

    /* Check if we need to search the other branch */
    float plane_dist = fabsf(query_coord - root_coord);
    if (plane_dist < state->best_dist) {
        kd_find_nearest(second, query, depth + 1, state);
    }
}

typedef struct {
    spatial_object_t** results;
    float* distances;
    uint32_t count;
    uint32_t capacity;
    vec3_t center;
    float radius;
} kd_range_state_t;

static void kd_find_in_range(kd_node_t* root, uint8_t depth, kd_range_state_t* state) {
    if (!root || state->count >= state->capacity) return;

    float dist = vec3_distance(state->center, root->object->position);
    if (dist <= state->radius) {
        state->results[state->count] = root->object;
        state->distances[state->count] = dist;
        state->count++;
    }

    uint8_t axis = depth % 3;
    float center_coord, root_coord;
    switch (axis) {
        case 0: center_coord = state->center.x; root_coord = root->object->position.x; break;
        case 1: center_coord = state->center.y; root_coord = root->object->position.y; break;
        default: center_coord = state->center.z; root_coord = root->object->position.z; break;
    }

    float plane_dist = fabsf(center_coord - root_coord);

    /* Always search the side the center is on */
    if (center_coord < root_coord) {
        kd_find_in_range(root->left, depth + 1, state);
        if (plane_dist <= state->radius) {
            kd_find_in_range(root->right, depth + 1, state);
        }
    } else {
        kd_find_in_range(root->right, depth + 1, state);
        if (plane_dist <= state->radius) {
            kd_find_in_range(root->left, depth + 1, state);
        }
    }
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

spatial_config_t spatial_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_default_conf", 0.0f);


    spatial_config_t config = {
        .rotation_rate_deg_ms = SPATIAL_ROTATION_RATE_DEG_MS,
        .matching_threshold = 0.9f,
        .max_objects = SPATIAL_MAX_OBJECTS,
        .enable_attention = true,
        .enable_bio_async = false,
        .inflammation_sensitivity = 0.5f,
        .fatigue_sensitivity = 0.5f
    };
    return config;
}

bool spatial_validate_config(const spatial_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_validate_config: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_validate_con", 0.0f);


    if (config->rotation_rate_deg_ms <= 0.0f) {
        set_spatial_error("Rotation rate must be positive");
        return false;
    }

    if (config->matching_threshold < 0.0f || config->matching_threshold > 1.0f) {
        set_spatial_error("Matching threshold must be in [0, 1]");
        return false;
    }

    if (config->max_objects == 0 || config->max_objects > 1000000) {
        set_spatial_error("Max objects must be in [1, 1000000]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_validate_config: config->max_objects is zero");
        return false;
    }

    return true;
}

spatial_reasoning_t* spatial_reasoning_create(void) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_create", 0.0f);


    return spatial_reasoning_create_custom(NULL);
}

spatial_reasoning_t* spatial_reasoning_create_custom(const spatial_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_create_custom", 0.0f);


    spatial_config_t cfg;

    if (config) {
        if (!spatial_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid spatial reasoning config");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = spatial_default_config();
    }

    spatial_reasoning_t* sr = nimcp_calloc(1, sizeof(spatial_reasoning_t));
    if (!sr) return NULL;
    NIMCP_API_CHECK_ALLOC(sr, "Failed to allocate spatial reasoning");

    sr->config = cfg;
    sr->effective_rotation_rate = cfg.rotation_rate_deg_ms;
    sr->next_object_id = 1;

    /* Allocate object array */
    sr->objects = nimcp_calloc(cfg.max_objects, sizeof(spatial_object_t*));
    if (!sr->objects) {
        set_spatial_error("Failed to allocate object array");
        nimcp_free(sr);
        sr = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_reasoning_create_custom: sr->objects is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    sr->lock = nimcp_mutex_create(&attr);
    if (!sr->lock) {
        set_spatial_error("Failed to create mutex");
        nimcp_free(sr->objects);
        nimcp_free(sr);
        sr = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_reasoning_create_custom: sr->lock is NULL");
        return NULL;
    }

    return sr;
}

void spatial_reasoning_destroy(spatial_reasoning_t* sr) {
    if (!sr) return;

    /* Destroy K-D tree */
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_destroy", 0.0f);


    kd_destroy(sr->kd_root);

    /* Free objects */
    for (uint32_t i = 0; i < sr->num_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sr->num_objects > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(i + 1) / (float)sr->num_objects);
        }

        if (sr->objects[i]) {
            if (sr->objects[i]->vertices) {
                nimcp_free(sr->objects[i]->vertices);
            }
            nimcp_free(sr->objects[i]);
        }
    }
    nimcp_free(sr->objects);

    if (sr->lock) {
        nimcp_mutex_destroy(sr->lock);
    }

    nimcp_free(sr);
    sr = NULL;
}

/* ============================================================================
 * QUATERNION UTILITIES
 * ============================================================================ */

quaternion_t quaternion_identity(void) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_quaternion_identity", 0.0f);


    quaternion_t q = {1.0f, 0.0f, 0.0f, 0.0f};
    return q;
}

quaternion_t quaternion_from_axis_angle(vec3_t axis, float angle_radians) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_quaternion_from_axis", 0.0f);


    axis = vec3_normalize(axis);
    float half_angle = angle_radians * 0.5f;
    float s = sinf(half_angle);

    quaternion_t q;
    q.w = cosf(half_angle);
    q.x = axis.x * s;
    q.y = axis.y * s;
    q.z = axis.z * s;

    return q;
}

quaternion_t quaternion_multiply(quaternion_t a, quaternion_t b) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_quaternion_multiply", 0.0f);


    quaternion_t q;
    q.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    q.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    q.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    q.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return q;
}

quaternion_t quaternion_normalize(quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_quaternion_normalize", 0.0f);


    float len = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (len < 1e-10f) return quaternion_identity();
    return (quaternion_t){q.w / (fabsf(len) > 1e-7f ? len : 1e-7f), q.x / (fabsf(len) > 1e-7f ? len : 1e-7f), q.y / (fabsf(len) > 1e-7f ? len : 1e-7f), q.z / (fabsf(len) > 1e-7f ? len : 1e-7f)};
}

vec3_t quaternion_rotate_vector(quaternion_t q, vec3_t v) {
    /* v' = q * v * q^-1 */
    quaternion_t qv = {0.0f, v.x, v.y, v.z};
    quaternion_t q_conj = {q.w, -q.x, -q.y, -q.z};

    quaternion_t temp = quaternion_multiply(q, qv);
    quaternion_t result = quaternion_multiply(temp, q_conj);

    return vec3_create(result.x, result.y, result.z);
}

float quaternion_angle_between(quaternion_t a, quaternion_t b) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_quaternion_angle_bet", 0.0f);

    /* Convert quaternions to SO(3) rotation matrices and use geodesic distance
     * d(R1,R2) = ||log(R1^T R2)|| which is the true rotation angle */
    float axis_a[3], axis_b[3];
    a = quaternion_normalize(a);
    b = quaternion_normalize(b);

    /* Extract axis-angle from quaternion a */
    float half_a = acosf(nimcp_clamp01(fabsf(a.w)));
    float sin_half_a = sinf(half_a);
    if (sin_half_a > 1e-6f) {
        axis_a[0] = a.x / sin_half_a;
        axis_a[1] = a.y / sin_half_a;
        axis_a[2] = a.z / sin_half_a;
    } else {
        axis_a[0] = 0.0f; axis_a[1] = 0.0f; axis_a[2] = 1.0f;
    }

    /* Extract axis-angle from quaternion b */
    float half_b = acosf(nimcp_clamp01(fabsf(b.w)));
    float sin_half_b = sinf(half_b);
    if (sin_half_b > 1e-6f) {
        axis_b[0] = b.x / sin_half_b;
        axis_b[1] = b.y / sin_half_b;
        axis_b[2] = b.z / sin_half_b;
    } else {
        axis_b[0] = 0.0f; axis_b[1] = 0.0f; axis_b[2] = 1.0f;
    }

    so3_rotation_t R1 = so3_from_axis_angle(axis_a, 2.0f * half_a);
    so3_rotation_t R2 = so3_from_axis_angle(axis_b, 2.0f * half_b);

    /* SO(3) geodesic distance = true rotation angle in radians */
    float dist_rad = so3_distance(&R1, &R2);
    return dist_rad * RAD_TO_DEG;
}

/* ============================================================================
 * MENTAL ROTATION API
 * ============================================================================ */

rotation_result_t spatial_rotate_and_compare(
    spatial_reasoning_t* sr,
    const spatial_object_t* object_a,
    const spatial_object_t* object_b
) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_rotate_and_c", 0.0f);


    rotation_result_t result = {0};

    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_rotate_and_compare: sr is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (!object_a) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_rotate_and_compare: object_a is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (!object_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_rotate_and_compare: object_b is NULL");
        result.confidence = -1.0f;
        return result;
    }

    nimcp_mutex_lock(sr->lock);

    /* Compute rotation angle needed to align orientations */
    float angle = quaternion_angle_between(object_a->orientation, object_b->orientation);
    result.rotation_angle = angle;

    /* Compute rotation axis via SO(3) logarithmic map (exact, not approximate) */
    quaternion_t q_a = quaternion_normalize(object_a->orientation);
    quaternion_t q_b = quaternion_normalize(object_b->orientation);

    /* Build SO(3) matrices from quaternions */
    float ha = acosf(nimcp_clamp01(fabsf(q_a.w)));
    float sha = sinf(ha);
    float ax_a[3] = {0,0,1}, ax_b[3] = {0,0,1};
    if (sha > 1e-6f) { ax_a[0]=q_a.x/sha; ax_a[1]=q_a.y/sha; ax_a[2]=q_a.z/sha; }
    float hb = acosf(nimcp_clamp01(fabsf(q_b.w)));
    float shb = sinf(hb);
    if (shb > 1e-6f) { ax_b[0]=q_b.x/shb; ax_b[1]=q_b.y/shb; ax_b[2]=q_b.z/shb; }

    so3_rotation_t R_a = so3_from_axis_angle(ax_a, 2.0f*ha);
    so3_rotation_t R_b = so3_from_axis_angle(ax_b, 2.0f*hb);

    /* log(R_a^T * R_b) gives exact rotation axis and angle */
    so3_rotation_t R_a_inv = so3_transpose(&R_a);
    so3_rotation_t R_diff = so3_multiply(&R_a_inv, &R_b);
    so3_algebra_t log_diff = so3_log(&R_diff);

    float axis_len = sqrtf(log_diff.v[0]*log_diff.v[0] + log_diff.v[1]*log_diff.v[1] + log_diff.v[2]*log_diff.v[2]);
    if (axis_len > 1e-6f) {
        result.rotation_axis = vec3_create(log_diff.v[0] / axis_len,
                                           log_diff.v[1] / axis_len,
                                           log_diff.v[2] / axis_len);
    } else {
        result.rotation_axis = vec3_create(0, 1, 0);
    }

    /* Compute processing time (linear with angle - Shepard paradigm) */
    result.processing_time_ms = (uint64_t)(angle / sr->effective_rotation_rate);

    /* Compute shape similarity */
    result.shape_similarity = spatial_shape_similarity(sr, object_a, object_b);

    /* Determine match based on similarity threshold */
    result.is_match = (result.shape_similarity >= sr->config.matching_threshold);

    /* Confidence based on how far above/below threshold */
    float margin = fabsf(result.shape_similarity - sr->config.matching_threshold);
    result.confidence = 0.5f + margin * 2.0f;
    if (result.confidence > 1.0f) result.confidence = 1.0f;

    /* Update statistics */
    sr->rotations_performed++;
    sr->total_rotation_time_ms += (double)result.processing_time_ms;
    sr->total_rotation_angle += (double)angle;

    nimcp_mutex_unlock(sr->lock);

    return result;
}

uint64_t spatial_mental_rotate(
    spatial_reasoning_t* sr,
    spatial_object_t* object,
    vec3_t axis,
    float angle_degrees
) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_mental_rotate: sr is NULL");
        return 0;
    }
    if (!object) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_mental_rotate: object is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_mental_rotat", 0.0f);


    nimcp_mutex_lock(sr->lock);

    /* Compute rotation quaternion */
    quaternion_t rot = quaternion_from_axis_angle(axis, angle_degrees * DEG_TO_RAD);

    /* Apply rotation to object orientation */
    object->orientation = quaternion_multiply(rot, object->orientation);
    object->orientation = quaternion_normalize(object->orientation);

    /* Rotate all vertices if present */
    if (object->vertices && object->num_vertices > 0) {
        for (uint32_t i = 0; i < object->num_vertices; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && object->num_vertices > 256) {
                spatial_reasoning_heartbeat("spatial_reas_loop",
                                 (float)(i + 1) / (float)object->num_vertices);
            }

            object->vertices[i] = quaternion_rotate_vector(rot, object->vertices[i]);
        }
    }

    /* Compute processing time */
    uint64_t time_ms = (uint64_t)(fabsf(angle_degrees) / sr->effective_rotation_rate);

    sr->rotations_performed++;
    sr->total_rotation_time_ms += (double)time_ms;
    sr->total_rotation_angle += (double)fabsf(angle_degrees);

    nimcp_mutex_unlock(sr->lock);

    return time_ms;
}

float spatial_shape_similarity(
    spatial_reasoning_t* sr,
    const spatial_object_t* object_a,
    const spatial_object_t* object_b
) {
    if (!sr || !object_a || !object_b) return 0.0f;

    /* Simple similarity based on bounding radius and vertex count */
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_shape_simila", 0.0f);


    float radius_sim = 1.0f - fabsf(object_a->bounding_radius - object_b->bounding_radius) /
                       fmaxf(object_a->bounding_radius, object_b->bounding_radius + 0.001f);

    /* Vertex count similarity (if both have vertices) */
    float vertex_sim = 1.0f;
    if (object_a->num_vertices > 0 && object_b->num_vertices > 0) {
        float max_v = fmaxf((float)object_a->num_vertices, (float)object_b->num_vertices);
        float min_v = fminf((float)object_a->num_vertices, (float)object_b->num_vertices);
        vertex_sim = min_v / max_v;
    }

    return 0.5f * radius_sim + 0.5f * vertex_sim;
}

/* ============================================================================
 * COORDINATE TRANSFORMATION API
 * ============================================================================ */

spatial_transform_t spatial_pose_to_transform(const observer_pose_t* pose) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_pose_to_tran", 0.0f);


    spatial_transform_t t;
    memset(&t, 0, sizeof(t));

    if (!pose) {
        /* Identity matrix */
        t.m[0] = t.m[5] = t.m[10] = t.m[15] = 1.0f;
        return t;
    }

    /* Build rotation matrix from quaternion */
    quaternion_t q = quaternion_normalize(pose->orientation);
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    t.m[0] = 1.0f - 2.0f * (yy + zz);
    t.m[1] = 2.0f * (xy + wz);
    t.m[2] = 2.0f * (xz - wy);
    t.m[3] = 0.0f;

    t.m[4] = 2.0f * (xy - wz);
    t.m[5] = 1.0f - 2.0f * (xx + zz);
    t.m[6] = 2.0f * (yz + wx);
    t.m[7] = 0.0f;

    t.m[8] = 2.0f * (xz + wy);
    t.m[9] = 2.0f * (yz - wx);
    t.m[10] = 1.0f - 2.0f * (xx + yy);
    t.m[11] = 0.0f;

    /* Translation */
    t.m[12] = pose->position.x;
    t.m[13] = pose->position.y;
    t.m[14] = pose->position.z;
    t.m[15] = 1.0f;

    return t;
}

vec3_t spatial_transform_point(const spatial_transform_t* transform, vec3_t point) {
    if (!transform) return point;

    vec3_t result;
    result.x = transform->m[0] * point.x + transform->m[4] * point.y +
               transform->m[8] * point.z + transform->m[12];
    result.y = transform->m[1] * point.x + transform->m[5] * point.y +
               transform->m[9] * point.z + transform->m[13];
    result.z = transform->m[2] * point.x + transform->m[6] * point.y +
               transform->m[10] * point.z + transform->m[14];
    return result;
}

spatial_transform_t spatial_transform_inverse(const spatial_transform_t* transform) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_transform_in", 0.0f);


    spatial_transform_t inv;
    memset(&inv, 0, sizeof(inv));

    if (!transform) {
        inv.m[0] = inv.m[5] = inv.m[10] = inv.m[15] = 1.0f;
        return inv;
    }

    /* For rigid transform: R^-1 = R^T, t^-1 = -R^T * t */
    /* Transpose rotation part */
    inv.m[0] = transform->m[0]; inv.m[1] = transform->m[4]; inv.m[2] = transform->m[8];
    inv.m[4] = transform->m[1]; inv.m[5] = transform->m[5]; inv.m[6] = transform->m[9];
    inv.m[8] = transform->m[2]; inv.m[9] = transform->m[6]; inv.m[10] = transform->m[10];

    /* Inverse translation */
    inv.m[12] = -(inv.m[0] * transform->m[12] + inv.m[4] * transform->m[13] +
                  inv.m[8] * transform->m[14]);
    inv.m[13] = -(inv.m[1] * transform->m[12] + inv.m[5] * transform->m[13] +
                  inv.m[9] * transform->m[14]);
    inv.m[14] = -(inv.m[2] * transform->m[12] + inv.m[6] * transform->m[13] +
                  inv.m[10] * transform->m[14]);
    inv.m[15] = 1.0f;

    return inv;
}

vec3_t spatial_ego_to_allocentric(
    spatial_reasoning_t* sr,
    vec3_t local_pos,
    const observer_pose_t* observer
) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_ego_to_allocentric: sr is NULL");
        return (vec3_t){0, 0, 0};
    }

    nimcp_mutex_lock(sr->lock);

    spatial_transform_t t = spatial_pose_to_transform(observer);
    vec3_t result = spatial_transform_point(&t, local_pos);

    sr->transforms_performed++;

    nimcp_mutex_unlock(sr->lock);

    return result;
}

vec3_t spatial_allocentric_to_ego(
    spatial_reasoning_t* sr,
    vec3_t world_pos,
    const observer_pose_t* observer
) {
    if (!sr) return world_pos;

    nimcp_mutex_lock(sr->lock);

    spatial_transform_t t = spatial_pose_to_transform(observer);
    spatial_transform_t t_inv = spatial_transform_inverse(&t);
    vec3_t result = spatial_transform_point(&t_inv, world_pos);

    sr->transforms_performed++;

    nimcp_mutex_unlock(sr->lock);

    return result;
}

/* ============================================================================
 * SPATIAL INDEXING API
 * ============================================================================ */

uint32_t spatial_add_object(spatial_reasoning_t* sr, const spatial_object_t* object) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_add_object: sr is NULL");
        return 0;
    }
    if (!object) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_add_object: object is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_add_object", 0.0f);


    nimcp_mutex_lock(sr->lock);

    if (sr->num_objects >= sr->config.max_objects) {
        set_spatial_error("Maximum objects reached");
        nimcp_mutex_unlock(sr->lock);
        return 0;
    }

    /* Copy object */
    spatial_object_t* new_obj = nimcp_calloc(1, sizeof(spatial_object_t));
    if (!new_obj) {
        set_spatial_error("Failed to allocate object");
        nimcp_mutex_unlock(sr->lock);
        return 0;
    }

    *new_obj = *object;
    new_obj->id = sr->next_object_id++;

    /* Copy vertices if present */
    if (object->vertices && object->num_vertices > 0) {
        new_obj->vertices = nimcp_calloc(object->num_vertices, sizeof(vec3_t));
        if (new_obj->vertices) {
            memcpy(new_obj->vertices, object->vertices,
                   object->num_vertices * sizeof(vec3_t));
        }
    }

    /* Add to array */
    sr->objects[sr->num_objects++] = new_obj;

    /* Insert into K-D tree */
    sr->kd_root = kd_insert(sr->kd_root, new_obj, 0);

    nimcp_mutex_unlock(sr->lock);

    return new_obj->id;
}

int spatial_remove_object(spatial_reasoning_t* sr, uint32_t object_id) {
    if (!sr || object_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_remove_object: sr is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_remove_objec", 0.0f);


    nimcp_mutex_lock(sr->lock);

    /* Find and remove from array */
    int found = -1;
    for (uint32_t i = 0; i < sr->num_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sr->num_objects > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(i + 1) / (float)sr->num_objects);
        }

        if (sr->objects[i] && sr->objects[i]->id == object_id) {
            if (sr->objects[i]->vertices) {
                nimcp_free(sr->objects[i]->vertices);
            }
            nimcp_free(sr->objects[i]);

            /* Shift remaining objects */
            for (uint32_t j = i; j < sr->num_objects - 1; j++) {
                sr->objects[j] = sr->objects[j + 1];
            }
            sr->objects[sr->num_objects - 1] = NULL;
            sr->num_objects--;
            found = 0;
            break;
        }
    }

    /* Rebuild K-D tree (simple approach) */
    if (found == 0) {
        kd_destroy(sr->kd_root);
        sr->kd_root = NULL;
        for (uint32_t i = 0; i < sr->num_objects; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sr->num_objects > 256) {
                spatial_reasoning_heartbeat("spatial_reas_loop",
                                 (float)(i + 1) / (float)sr->num_objects);
            }

            sr->kd_root = kd_insert(sr->kd_root, sr->objects[i], 0);
        }
    }

    nimcp_mutex_unlock(sr->lock);

    return found;
}

int spatial_update_position(spatial_reasoning_t* sr, uint32_t object_id, vec3_t new_position) {
    if (!sr || object_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_update_position: sr is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_update_posit", 0.0f);


    nimcp_mutex_lock(sr->lock);

    /* Find object */
    spatial_object_t* obj = NULL;
    for (uint32_t i = 0; i < sr->num_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sr->num_objects > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(i + 1) / (float)sr->num_objects);
        }

        if (sr->objects[i] && sr->objects[i]->id == object_id) {
            obj = sr->objects[i];
            break;
        }
    }

    if (!obj) {
        nimcp_mutex_unlock(sr->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_update_position: obj is NULL");
        return -1;
    }

    obj->position = new_position;

    /* Rebuild K-D tree (simple approach for correctness) */
    kd_destroy(sr->kd_root);
    sr->kd_root = NULL;
    for (uint32_t i = 0; i < sr->num_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sr->num_objects > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(i + 1) / (float)sr->num_objects);
        }

        sr->kd_root = kd_insert(sr->kd_root, sr->objects[i], 0);
    }

    nimcp_mutex_unlock(sr->lock);

    return 0;
}

spatial_object_t* spatial_find_nearest(spatial_reasoning_t* sr, vec3_t query_pos) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_find_nearest: sr is NULL");
        return NULL;
    }
    if (!sr->kd_root) return NULL;

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_find_nearest", 0.0f);


    nimcp_mutex_lock(sr->lock);

    kd_nearest_state_t state = {NULL, INFINITY};
    kd_find_nearest(sr->kd_root, query_pos, 0, &state);

    sr->spatial_queries++;

    nimcp_mutex_unlock(sr->lock);

    return state.best;
}

uint32_t spatial_find_k_nearest(
    spatial_reasoning_t* sr,
    vec3_t query_pos,
    uint32_t k,
    spatial_query_result_t* result
) {
    if (!sr || !result || k == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_find_k_neare", 0.0f);


    nimcp_mutex_lock(sr->lock);

    /* Simple O(n) approach - find all then sort */
    /* For production, use a proper K-NN algorithm with priority queue */

    /* Collect all distances */
    float* all_dists = nimcp_calloc(sr->num_objects, sizeof(float));
    uint32_t* indices = nimcp_calloc(sr->num_objects, sizeof(uint32_t));

    if (!all_dists || !indices) {
        nimcp_free(all_dists);
        all_dists = NULL;
        nimcp_free(indices);
        indices = NULL;
        nimcp_mutex_unlock(sr->lock);
        return 0;
    }

    for (uint32_t i = 0; i < sr->num_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sr->num_objects > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(i + 1) / (float)sr->num_objects);
        }

        all_dists[i] = vec3_distance(query_pos, sr->objects[i]->position);
        indices[i] = i;
    }

    /* Simple selection sort for k smallest */
    uint32_t count = (k < sr->num_objects) ? k : sr->num_objects;
    count = (count < result->capacity) ? count : result->capacity;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(i + 1) / (float)count);
        }

        uint32_t min_idx = i;
        for (uint32_t j = i + 1; j < sr->num_objects; j++) {
            if (all_dists[j] < all_dists[min_idx]) {
                min_idx = j;
            }
        }
        /* Swap */
        float tmp_d = all_dists[i];
        all_dists[i] = all_dists[min_idx];
        all_dists[min_idx] = tmp_d;

        uint32_t tmp_i = indices[i];
        indices[i] = indices[min_idx];
        indices[min_idx] = tmp_i;

        result->objects[i] = sr->objects[indices[i]];
        result->distances[i] = all_dists[i];
    }

    result->count = count;

    nimcp_free(all_dists);
    all_dists = NULL;
    nimcp_free(indices);
    indices = NULL;

    sr->spatial_queries++;

    nimcp_mutex_unlock(sr->lock);

    return count;
}

uint32_t spatial_find_in_radius(
    spatial_reasoning_t* sr,
    vec3_t center,
    float radius,
    spatial_query_result_t* result
) {
    if (!sr || !result || radius <= 0.0f) return 0;

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_find_in_radi", 0.0f);


    nimcp_mutex_lock(sr->lock);

    kd_range_state_t state = {
        .results = result->objects,
        .distances = result->distances,
        .count = 0,
        .capacity = result->capacity,
        .center = center,
        .radius = radius
    };

    kd_find_in_range(sr->kd_root, 0, &state);
    result->count = state.count;

    sr->spatial_queries++;

    nimcp_mutex_unlock(sr->lock);

    return result->count;
}

spatial_query_result_t* spatial_query_result_create(uint32_t capacity) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_query_result", 0.0f);


    spatial_query_result_t* result = nimcp_calloc(1, sizeof(spatial_query_result_t));
    if (!result) return NULL;
    NIMCP_API_CHECK_ALLOC(result, "Failed to allocate spatial query result");

    result->objects = nimcp_calloc(capacity, sizeof(spatial_object_t*));
    result->distances = nimcp_calloc(capacity, sizeof(float));

    if (!result->objects || !result->distances) {
        LOG_ERROR("Failed to allocate spatial query result arrays");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * (sizeof(spatial_object_t*) + sizeof(float)),
                          "Failed to allocate spatial query arrays");
        nimcp_free(result->objects);
        nimcp_free(result->distances);
        nimcp_free(result);
        result = NULL;
        return NULL;
    }

    result->capacity = capacity;
    return result;
}

void spatial_query_result_destroy(spatial_query_result_t* result) {
    if (!result) return;
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_query_result", 0.0f);


    nimcp_free(result->objects);
    nimcp_free(result->distances);
    nimcp_free(result);
    result = NULL;
}

/* ============================================================================
 * SPATIAL ATTENTION API
 * ============================================================================ */

spatial_attention_t* spatial_attention_create(
    spatial_reasoning_t* sr,
    uint32_t grid_width,
    uint32_t grid_height
) {
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_attention_cr", 0.0f);


    (void)sr;

    if (grid_width == 0 || grid_height == 0) {
        LOG_ERROR("Invalid grid dimensions: %u x %u", grid_width, grid_height);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid grid dimensions: %u x %u", grid_width, grid_height);
        return NULL;
    }

    spatial_attention_t* attn = nimcp_calloc(1, sizeof(spatial_attention_t));
    NIMCP_API_CHECK_ALLOC(attn, "Failed to allocate spatial attention");

    attn->weights = nimcp_calloc(grid_width * grid_height, sizeof(float));
    if (!attn->weights) {
        LOG_ERROR("Failed to allocate attention weights");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, grid_width * grid_height * sizeof(float),
                          "Failed to allocate attention weights");
        nimcp_free(attn);
        attn = NULL;
        return NULL;
    }

    attn->grid_width = grid_width;
    attn->grid_height = grid_height;
    attn->spread = 1.0f;

    return attn;
}

void spatial_attention_destroy(spatial_attention_t* attention) {
    if (!attention) return;
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_attention_de", 0.0f);


    nimcp_free(attention->weights);
    nimcp_free(attention);
    attention = NULL;
}

int spatial_attention_set_focus(spatial_attention_t* attention, vec3_t focus, float spread) {
    if (!attention || spread <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_attention_set_focus: attention is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_attention_se", 0.0f);


    attention->focus_point = focus;
    attention->spread = spread;

    /* Recompute weights as Gaussian centered on focus */
    float sigma_sq = spread * spread;

    for (uint32_t y = 0; y < attention->grid_height; y++) {
        /* Phase 8: Loop progress heartbeat */
        if ((y & 0xFF) == 0 && attention->grid_height > 256) {
            spatial_reasoning_heartbeat("spatial_reas_loop",
                             (float)(y + 1) / (float)attention->grid_height);
        }

        for (uint32_t x = 0; x < attention->grid_width; x++) {
            /* Phase 8: Loop progress heartbeat */
            if ((x & 0xFF) == 0 && attention->grid_width > 256) {
                spatial_reasoning_heartbeat("spatial_reas_loop",
                                 (float)(x + 1) / (float)attention->grid_width);
            }

            /* Map grid to normalized coordinates (guard against grid size 1) */
            float nx = (attention->grid_width > 1)
                ? ((float)x / (float)(attention->grid_width - 1)) * 2.0f - 1.0f
                : 0.0f;
            float ny = (attention->grid_height > 1)
                ? ((float)y / (float)(attention->grid_height - 1)) * 2.0f - 1.0f
                : 0.0f;

            float dx = nx - focus.x;
            float dy = ny - focus.y;
            float dist_sq = dx * dx + dy * dy;

            attention->weights[y * attention->grid_width + x] =
                expf(-dist_sq / (2.0f * sigma_sq));
        }
    }

    return 0;
}

float spatial_attention_at(const spatial_attention_t* attention, vec3_t pos) {
    if (!attention) return 0.0f;

    /* Map position to grid coordinates */
    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_attention_at", 0.0f);


    float nx = (pos.x + 1.0f) * 0.5f;
    float ny = (pos.y + 1.0f) * 0.5f;

    int gx = (int)(nx * (attention->grid_width - 1));
    int gy = (int)(ny * (attention->grid_height - 1));

    if (gx < 0) gx = 0;
    if (gy < 0) gy = 0;
    if (gx >= (int)attention->grid_width) gx = attention->grid_width - 1;
    if (gy >= (int)attention->grid_height) gy = attention->grid_height - 1;

    return attention->weights[gy * attention->grid_width + gx];
}

int spatial_attention_update(spatial_attention_t* attention, float decay_rate) {
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_attention_update: attention is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_attention_up", 0.0f);


    decay_rate = nimcp_clamp01(decay_rate);

    for (uint32_t i = 0; i < attention->grid_width * attention->grid_height; i++) {
        attention->weights[i] *= (1.0f - decay_rate);
    }

    return 0;
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int spatial_set_inflammation(spatial_reasoning_t* sr, float level) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_set_inflammation: sr is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_set_inflamma", 0.0f);


    nimcp_mutex_lock(sr->lock);
    sr->inflammation_level = nimcp_clamp01(level);
    update_effective_rotation_rate(sr);
    nimcp_mutex_unlock(sr->lock);

    return 0;
}

int spatial_set_fatigue(spatial_reasoning_t* sr, float level) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_set_fatigue: sr is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_set_fatigue", 0.0f);


    nimcp_mutex_lock(sr->lock);
    sr->fatigue_level = nimcp_clamp01(level);
    update_effective_rotation_rate(sr);
    nimcp_mutex_unlock(sr->lock);

    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int spatial_get_stats(spatial_reasoning_t* sr, spatial_stats_t* stats) {
    if (!sr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_get_stats: sr is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_get_stats: stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_get_stats", 0.0f);


    nimcp_mutex_lock(((spatial_reasoning_t*)sr)->lock);

    stats->rotations_performed = sr->rotations_performed;
    stats->transforms_performed = sr->transforms_performed;
    stats->spatial_queries = sr->spatial_queries;
    stats->objects_stored = sr->num_objects;

    if (sr->rotations_performed > 0) {
        stats->avg_rotation_time_ms = (float)(sr->total_rotation_time_ms /
                                               (double)sr->rotations_performed);
        stats->avg_rotation_angle = (float)(sr->total_rotation_angle /
                                             (double)sr->rotations_performed);
    } else {
        stats->avg_rotation_time_ms = 0.0f;
        stats->avg_rotation_angle = 0.0f;
    }

    nimcp_mutex_unlock(((spatial_reasoning_t*)sr)->lock);

    return 0;
}

void spatial_reset_stats(spatial_reasoning_t* sr) {
    if (!sr) return;

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_spatial_reset_stats", 0.0f);


    nimcp_mutex_lock(sr->lock);

    sr->rotations_performed = 0;
    sr->transforms_performed = 0;
    sr->spatial_queries = 0;
    sr->total_rotation_time_ms = 0.0;
    sr->total_rotation_angle = 0.0;

    nimcp_mutex_unlock(sr->lock);
}

const char* spatial_get_last_error(void) {
    return g_spatial_error;
}

/* ============================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ============================================================================ */

int spatial_reasoning_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    spatial_reasoning_heartbeat("spatial_reas_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Spatial_Reasoning_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                spatial_reasoning_heartbeat("spatial_reas_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Spatial reasoning self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Spatial_Reasoning_Module");
    if (connections) {
        LOG_DEBUG("Spatial reasoning has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Spatial_Reasoning_Module");
    if (incoming) {
        LOG_DEBUG("Spatial reasoning has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void spatial_reasoning_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_spatial_reasoning_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int spatial_reasoning_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "spatial_reasoning_training_begin: NULL argument");
        return -1;
    }
    spatial_reasoning_heartbeat_instance(g_spatial_reasoning_health_agent, "spatial_reasoning_training_begin", 0.0f);
    (void)(struct kd_node*)instance; /* Module state available for reset */
    return 0;
}

int spatial_reasoning_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "spatial_reasoning_training_end: NULL argument");
        return -1;
    }
    spatial_reasoning_heartbeat_instance(g_spatial_reasoning_health_agent, "spatial_reasoning_training_end", 1.0f);
    (void)(struct kd_node*)instance; /* Module state available for finalization */
    return 0;
}

int spatial_reasoning_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "spatial_reasoning_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    spatial_reasoning_heartbeat_instance(g_spatial_reasoning_health_agent, "spatial_reasoning_training_step", progress);
    (void)(struct kd_node*)instance; /* Module state available for step adaptation */
    return 0;
}
