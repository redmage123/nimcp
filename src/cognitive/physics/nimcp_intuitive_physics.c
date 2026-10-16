/**
 * @file nimcp_intuitive_physics.c
 * @brief Intuitive Physics Engine — rigid body simulation
 *
 * WHAT: Deterministic rigid-body physics with collision detection and resolution
 * WHY:  Ground truth physics prior for world model predictions
 * HOW:  Symplectic Euler integration, AABB broadphase, sphere/box narrowphase,
 *       sequential impulse contact solver, support graph extraction
 */

#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <float.h>
#include <string.h>

#define LOG_TAG "INTUITIVE_PHYSICS"
#define IP_MAX_CHAIN_DEPTH 16

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float vec3_dot(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline wm_parietal_vec3_t vec3_sub(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){ a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline wm_parietal_vec3_t vec3_add(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline wm_parietal_vec3_t vec3_scale(wm_parietal_vec3_t v, float s) {
    return (wm_parietal_vec3_t){ v.x * s, v.y * s, v.z * s };
}

static inline float vec3_len2(wm_parietal_vec3_t v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline float vec3_len(wm_parietal_vec3_t v) {
    return sqrtf(vec3_len2(v));
}

static inline wm_parietal_vec3_t vec3_normalize(wm_parietal_vec3_t v) {
    float l = vec3_len(v);
    if (l < 1e-8f) return (wm_parietal_vec3_t){0, 0, 0};
    return vec3_scale(v, 1.0f / l);
}

static inline wm_parietal_vec3_t vec3_cross(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }

/* Compute inverse inertia for basic shapes */
static wm_parietal_vec3_t compute_inv_inertia(const ip_object_t* obj) {
    if (obj->mass <= 0.0f || obj->is_static) {
        return (wm_parietal_vec3_t){0, 0, 0};
    }
    float m = obj->mass;
    float ix, iy, iz;
    switch (obj->shape.type) {
    case IP_SHAPE_SPHERE: {
        float r = obj->shape.sphere.radius;
        float I = 0.4f * m * r * r;  /* 2/5 * m * r^2 */
        ix = iy = iz = 1.0f / I;
        break;
    }
    case IP_SHAPE_BOX: {
        float hx = obj->shape.box.hx, hy = obj->shape.box.hy, hz = obj->shape.box.hz;
        ix = 1.0f / (m * (hy*hy + hz*hz) / 3.0f);
        iy = 1.0f / (m * (hx*hx + hz*hz) / 3.0f);
        iz = 1.0f / (m * (hx*hx + hy*hy) / 3.0f);
        break;
    }
    case IP_SHAPE_CYLINDER: {
        float r = obj->shape.cylinder.radius;
        float h = obj->shape.cylinder.half_height * 2.0f;
        float Ixx = m * (3.0f * r * r + h * h) / 12.0f;
        float Iyy = Ixx;
        float Izz = 0.5f * m * r * r;
        ix = 1.0f / Ixx; iy = 1.0f / Iyy; iz = 1.0f / Izz;
        break;
    }
    default:
        ix = iy = iz = 0;
        break;
    }
    return (wm_parietal_vec3_t){ix, iy, iz};
}

/* Bounding radius for broadphase */
static float shape_bounding_radius(const ip_shape_t* s) {
    switch (s->type) {
    case IP_SHAPE_SPHERE:   return s->sphere.radius;
    case IP_SHAPE_BOX:      return sqrtf(s->box.hx*s->box.hx + s->box.hy*s->box.hy + s->box.hz*s->box.hz);
    case IP_SHAPE_CYLINDER: return sqrtf(s->cylinder.radius*s->cylinder.radius + s->cylinder.half_height*s->cylinder.half_height);
    case IP_SHAPE_PLANE:    return 1e6f;  /* infinite */
    default: return 0;
    }
}

/* ============================================================================
 * Collision Detection — Narrowphase
 * ============================================================================ */

/* Sphere vs Sphere */
static bool collide_sphere_sphere(const ip_object_t* a, const ip_object_t* b,
                                   ip_contact_t* contact) {
    wm_parietal_vec3_t d = vec3_sub(b->position, a->position);
    float dist2 = vec3_len2(d);
    float r_sum = a->shape.sphere.radius + b->shape.sphere.radius;
    if (dist2 >= r_sum * r_sum) return false;

    float dist = sqrtf(dist2);
    contact->normal = dist > 1e-8f ? vec3_scale(d, 1.0f / dist)
                                    : (wm_parietal_vec3_t){0, 1, 0};
    contact->penetration = r_sum - dist;
    contact->point = vec3_add(a->position, vec3_scale(contact->normal, a->shape.sphere.radius - contact->penetration * 0.5f));
    return true;
}

/* Sphere vs Plane */
static bool collide_sphere_plane(const ip_object_t* sphere, const ip_object_t* plane,
                                  ip_contact_t* contact, bool flip) {
    wm_parietal_vec3_t n = plane->shape.plane.normal;
    float d = vec3_dot(sphere->position, n) - plane->shape.plane.offset;
    float pen = sphere->shape.sphere.radius - d;
    if (pen <= 0) return false;

    contact->normal = flip ? vec3_scale(n, -1.0f) : n;
    contact->penetration = pen;
    contact->point = vec3_sub(sphere->position, vec3_scale(n, d));
    return true;
}

/* Box vs Plane (simplified: box center distance) */
static bool collide_box_plane(const ip_object_t* box, const ip_object_t* plane,
                               ip_contact_t* contact, bool flip) {
    wm_parietal_vec3_t n = plane->shape.plane.normal;
    /* Project box half-extents onto plane normal (AABB approximation) */
    float proj = fabsf(n.x) * box->shape.box.hx +
                 fabsf(n.y) * box->shape.box.hy +
                 fabsf(n.z) * box->shape.box.hz;
    float d = vec3_dot(box->position, n) - plane->shape.plane.offset;
    float pen = proj - d;
    if (pen <= 0) return false;

    contact->normal = flip ? vec3_scale(n, -1.0f) : n;
    contact->penetration = pen;
    contact->point = vec3_sub(box->position, vec3_scale(n, d));
    return true;
}

/* Sphere vs Box (simplified: closest point on AABB) */
static bool collide_sphere_box(const ip_object_t* sphere, const ip_object_t* box,
                                ip_contact_t* contact, bool flip) {
    /* Find closest point on box to sphere center */
    wm_parietal_vec3_t local = vec3_sub(sphere->position, box->position);
    wm_parietal_vec3_t closest;
    closest.x = clampf(local.x, -box->shape.box.hx, box->shape.box.hx);
    closest.y = clampf(local.y, -box->shape.box.hy, box->shape.box.hy);
    closest.z = clampf(local.z, -box->shape.box.hz, box->shape.box.hz);

    wm_parietal_vec3_t diff = vec3_sub(local, closest);
    float dist2 = vec3_len2(diff);
    float r = sphere->shape.sphere.radius;
    if (dist2 >= r * r) return false;

    float dist = sqrtf(dist2);
    wm_parietal_vec3_t n = dist > 1e-8f ? vec3_scale(diff, 1.0f / dist)
                                         : (wm_parietal_vec3_t){0, 1, 0};
    if (flip) n = vec3_scale(n, -1.0f);

    contact->normal = n;
    contact->penetration = r - dist;
    contact->point = vec3_add(box->position, closest);
    return true;
}

/* Box vs Box (AABB overlap) */
static bool collide_box_box(const ip_object_t* a, const ip_object_t* b,
                             ip_contact_t* contact) {
    /* AABB overlap test */
    float dx = fabsf(b->position.x - a->position.x) - (a->shape.box.hx + b->shape.box.hx);
    float dy = fabsf(b->position.y - a->position.y) - (a->shape.box.hy + b->shape.box.hy);
    float dz = fabsf(b->position.z - a->position.z) - (a->shape.box.hz + b->shape.box.hz);

    if (dx > 0 || dy > 0 || dz > 0) return false;

    /* Find axis of minimum penetration */
    float pen_x = -dx, pen_y = -dy, pen_z = -dz;
    float min_pen = pen_x;
    wm_parietal_vec3_t n = {1, 0, 0};
    if (a->position.x > b->position.x) n.x = -1;

    if (pen_y < min_pen) { min_pen = pen_y; n = (wm_parietal_vec3_t){0, 1, 0}; if (a->position.y > b->position.y) n.y = -1; }
    if (pen_z < min_pen) { min_pen = pen_z; n = (wm_parietal_vec3_t){0, 0, 1}; if (a->position.z > b->position.z) n.z = -1; }

    contact->normal = n;
    contact->penetration = min_pen;
    /* Contact point at midpoint */
    contact->point = vec3_scale(vec3_add(a->position, b->position), 0.5f);
    return true;
}

/* Dispatch narrowphase collision detection */
static bool detect_collision(const ip_object_t* a, const ip_object_t* b,
                              ip_contact_t* contact) {
    ip_shape_type_t ta = a->shape.type, tb = b->shape.type;

    /* Sphere-Sphere */
    if (ta == IP_SHAPE_SPHERE && tb == IP_SHAPE_SPHERE)
        return collide_sphere_sphere(a, b, contact);

    /* Sphere-Plane
     * Convention: contact normal points from obj_a to obj_b.
     * When plane is a (index i) and sphere is b (index j), the plane's
     * outward normal already points toward b → no flip.
     * When sphere is a and plane is b, we swap args so sphere is first
     * in the function, and the plane normal points from b toward a → flip. */
    if (ta == IP_SHAPE_SPHERE && tb == IP_SHAPE_PLANE)
        return collide_sphere_plane(a, b, contact, true);
    if (ta == IP_SHAPE_PLANE && tb == IP_SHAPE_SPHERE)
        return collide_sphere_plane(b, a, contact, false);

    /* Box-Plane (same convention as sphere-plane) */
    if (ta == IP_SHAPE_BOX && tb == IP_SHAPE_PLANE)
        return collide_box_plane(a, b, contact, true);
    if (ta == IP_SHAPE_PLANE && tb == IP_SHAPE_BOX)
        return collide_box_plane(b, a, contact, false);

    /* Sphere-Box */
    if (ta == IP_SHAPE_SPHERE && tb == IP_SHAPE_BOX)
        return collide_sphere_box(a, b, contact, false);
    if (ta == IP_SHAPE_BOX && tb == IP_SHAPE_SPHERE)
        return collide_sphere_box(b, a, contact, true);

    /* Box-Box */
    if (ta == IP_SHAPE_BOX && tb == IP_SHAPE_BOX)
        return collide_box_box(a, b, contact);

    /* Cylinder and other combos: treat as sphere approximation */
    if (ta == IP_SHAPE_CYLINDER || tb == IP_SHAPE_CYLINDER) {
        /* Use bounding sphere approximation */
        ip_object_t sa = *a, sb = *b;
        if (ta == IP_SHAPE_CYLINDER) {
            sa.shape.type = IP_SHAPE_SPHERE;
            sa.shape.sphere.radius = shape_bounding_radius(&a->shape);
        }
        if (tb == IP_SHAPE_CYLINDER) {
            sb.shape.type = IP_SHAPE_SPHERE;
            sb.shape.sphere.radius = shape_bounding_radius(&b->shape);
        }
        return detect_collision(&sa, &sb, contact);
    }

    return false;
}

/* ============================================================================
 * Broadphase — simple O(n²) with bounding sphere check
 * ============================================================================ */

static void detect_all_contacts(intuitive_physics_engine_t* engine) {
    ip_scene_t* s = &engine->scene;
    s->num_contacts = 0;

    for (uint32_t i = 0; i < s->num_objects; i++) {
        if (!s->objects[i].active) continue;
        for (uint32_t j = i + 1; j < s->num_objects; j++) {
            if (!s->objects[j].active) continue;
            /* Both static? Skip */
            if (s->objects[i].is_static && s->objects[j].is_static) continue;

            /* Broadphase: bounding sphere */
            float ri = shape_bounding_radius(&s->objects[i].shape);
            float rj = shape_bounding_radius(&s->objects[j].shape);
            wm_parietal_vec3_t d = vec3_sub(s->objects[j].position, s->objects[i].position);
            float dist2 = vec3_len2(d);
            float sum_r = ri + rj + IP_CONTACT_THRESHOLD;
            if (dist2 > sum_r * sum_r) continue;

            engine->stats.collision_checks++;

            /* Narrowphase */
            ip_contact_t contact = {0};
            if (detect_collision(&s->objects[i], &s->objects[j], &contact)) {
                if (s->num_contacts < s->contacts_capacity) {
                    contact.obj_a = i;
                    contact.obj_b = j;
                    contact.normal_impulse = 0;
                    contact.tangent_impulse = 0;
                    s->contacts[s->num_contacts++] = contact;
                }
            }
        }
    }
}

/* ============================================================================
 * Contact Resolution — Sequential Impulse Solver
 * ============================================================================ */

static void resolve_contacts(intuitive_physics_engine_t* engine) {
    ip_scene_t* s = &engine->scene;

    for (uint32_t iter = 0; iter < engine->config.solver_iterations; iter++) {
        for (uint32_t c = 0; c < s->num_contacts; c++) {
            ip_contact_t* ct = &s->contacts[c];
            ip_object_t* a = &s->objects[ct->obj_a];
            ip_object_t* b = &s->objects[ct->obj_b];

            /* Relative velocity at contact point */
            wm_parietal_vec3_t va = {a->velocity.vx, a->velocity.vy, a->velocity.vz};
            wm_parietal_vec3_t vb = {b->velocity.vx, b->velocity.vy, b->velocity.vz};
            wm_parietal_vec3_t rel_vel = vec3_sub(vb, va);
            float vel_along_normal = vec3_dot(rel_vel, ct->normal);

            /* Don't resolve if separating */
            if (vel_along_normal > 0) continue;

            /* Restitution (min of pair) */
            float e = minf(a->restitution, b->restitution);

            /* Effective inverse mass */
            float inv_mass_sum = a->inv_mass + b->inv_mass;
            if (inv_mass_sum < 1e-12f) continue;  /* both infinite mass */

            /* Normal impulse magnitude */
            float j = -(1.0f + e) * vel_along_normal / inv_mass_sum;

            /* Clamp: accumulated impulse >= 0 (Erin Catto trick) */
            float new_impulse = maxf(ct->normal_impulse + j, 0.0f);
            float applied_j = new_impulse - ct->normal_impulse;
            ct->normal_impulse = new_impulse;

            /* Apply impulse */
            wm_parietal_vec3_t impulse = vec3_scale(ct->normal, applied_j);
            if (!a->is_static) {
                a->velocity.vx -= impulse.x * a->inv_mass;
                a->velocity.vy -= impulse.y * a->inv_mass;
                a->velocity.vz -= impulse.z * a->inv_mass;
            }
            if (!b->is_static) {
                b->velocity.vx += impulse.x * b->inv_mass;
                b->velocity.vy += impulse.y * b->inv_mass;
                b->velocity.vz += impulse.z * b->inv_mass;
            }

            /* Friction impulse (Coulomb model) */
            rel_vel = vec3_sub(
                (wm_parietal_vec3_t){b->velocity.vx, b->velocity.vy, b->velocity.vz},
                (wm_parietal_vec3_t){a->velocity.vx, a->velocity.vy, a->velocity.vz}
            );
            wm_parietal_vec3_t tangent = vec3_sub(rel_vel, vec3_scale(ct->normal, vec3_dot(rel_vel, ct->normal)));
            float tangent_len = vec3_len(tangent);
            if (tangent_len > 1e-8f) {
                tangent = vec3_scale(tangent, 1.0f / tangent_len);
                float jt = -vec3_dot(rel_vel, tangent) / inv_mass_sum;

                /* Coulomb clamp */
                float mu = 0.5f * (a->friction + b->friction);
                float max_friction = mu * ct->normal_impulse;
                jt = clampf(jt, -max_friction, max_friction);

                wm_parietal_vec3_t friction_impulse = vec3_scale(tangent, jt);
                if (!a->is_static) {
                    a->velocity.vx -= friction_impulse.x * a->inv_mass;
                    a->velocity.vy -= friction_impulse.y * a->inv_mass;
                    a->velocity.vz -= friction_impulse.z * a->inv_mass;
                }
                if (!b->is_static) {
                    b->velocity.vx += friction_impulse.x * b->inv_mass;
                    b->velocity.vy += friction_impulse.y * b->inv_mass;
                    b->velocity.vz += friction_impulse.z * b->inv_mass;
                }
            }

            /* Positional correction (Baumgarte stabilization) */
            float slop = 0.005f;
            float correction_pct = 0.4f;
            float correction = maxf(ct->penetration - slop, 0.0f) * correction_pct / inv_mass_sum;
            wm_parietal_vec3_t correction_vec = vec3_scale(ct->normal, correction);
            if (!a->is_static) {
                a->position.x -= correction_vec.x * a->inv_mass;
                a->position.y -= correction_vec.y * a->inv_mass;
                a->position.z -= correction_vec.z * a->inv_mass;
            }
            if (!b->is_static) {
                b->position.x += correction_vec.x * b->inv_mass;
                b->position.y += correction_vec.y * b->inv_mass;
                b->position.z += correction_vec.z * b->inv_mass;
            }

            engine->stats.contacts_resolved++;
        }
    }
}

/* ============================================================================
 * Support Graph Extraction
 * ============================================================================ */

static void rebuild_support_graph(intuitive_physics_engine_t* engine) {
    ip_scene_t* s = &engine->scene;
    s->num_support_edges = 0;

    /* Clear support relations */
    for (uint32_t i = 0; i < s->num_objects; i++) {
        if (s->objects[i].active)
            s->objects[i].supported_by = UINT32_MAX;
    }

    /* Derive support from contacts */
    for (uint32_t c = 0; c < s->num_contacts; c++) {
        ip_contact_t* ct = &s->contacts[c];
        ip_object_t* a = &s->objects[ct->obj_a];
        ip_object_t* b = &s->objects[ct->obj_b];

        /* Contact normal pointing from A to B.
         * If normal.y > threshold, B is above A → A supports B.
         * If normal.y < -threshold, A is above B → B supports A. */
        if (ct->normal.y > IP_SUPPORT_NORMAL_THRESHOLD) {
            /* A supports B */
            b->supported_by = a->id;
            if (s->num_support_edges < s->support_capacity) {
                s->support_graph[s->num_support_edges++] = (ip_support_edge_t){
                    .supporter = a->id,
                    .supported = b->id,
                    .contact_force = ct->normal_impulse
                };
            }
        } else if (ct->normal.y < -IP_SUPPORT_NORMAL_THRESHOLD) {
            /* B supports A */
            a->supported_by = b->id;
            if (s->num_support_edges < s->support_capacity) {
                s->support_graph[s->num_support_edges++] = (ip_support_edge_t){
                    .supporter = b->id,
                    .supported = a->id,
                    .contact_force = ct->normal_impulse
                };
            }
        }
    }
}

/* ============================================================================
 * Energy Computation
 * ============================================================================ */

static void compute_energy(intuitive_physics_engine_t* engine) {
    ip_scene_t* s = &engine->scene;
    float ke = 0, pe = 0;

    for (uint32_t i = 0; i < s->num_objects; i++) {
        ip_object_t* o = &s->objects[i];
        if (!o->active || o->is_static) continue;

        /* Kinetic energy: 0.5 * m * v^2 */
        float v2 = o->velocity.vx * o->velocity.vx +
                    o->velocity.vy * o->velocity.vy +
                    o->velocity.vz * o->velocity.vz;
        ke += 0.5f * o->mass * v2;

        /* Potential energy: m * g * h (height along gravity direction) */
        /* Gravity is typically (0, -g, 0), so height = position.y */
        pe += o->mass * engine->config.gravity_magnitude * o->position.y;
    }

    engine->stats.total_kinetic_energy = ke;
    engine->stats.total_potential_energy = pe;
    float total = ke + pe;

    if (engine->stats.step_count == 0) {
        engine->stats.initial_total_energy = total;
    }
    if (fabsf(engine->stats.initial_total_energy) > 1e-8f) {
        engine->stats.energy_drift = (total - engine->stats.initial_total_energy)
                                      / fabsf(engine->stats.initial_total_energy);
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

ip_config_t intuitive_physics_default_config(void) {
    return (ip_config_t){
        .max_objects = IP_MAX_OBJECTS,
        .max_contacts = IP_MAX_CONTACTS,
        .dt = IP_DEFAULT_DT,
        .gravity_magnitude = IP_DEFAULT_GRAVITY,
        .gravity_direction = {0, -1, 0},
        .solver_iterations = IP_SOLVER_ITERATIONS,
        .restitution_default = IP_RESTITUTION_DEFAULT,
        .friction_default = IP_FRICTION_DEFAULT,
    };
}

intuitive_physics_engine_t* intuitive_physics_create(const ip_config_t* config) {
    ip_config_t cfg = config ? *config : intuitive_physics_default_config();

    intuitive_physics_engine_t* engine = nimcp_calloc(1, sizeof(*engine));
    if (!engine) return NULL;

    engine->config = cfg;

    /* Allocate scene arrays */
    ip_scene_t* s = &engine->scene;
    s->capacity = cfg.max_objects;
    s->objects = nimcp_calloc(s->capacity, sizeof(ip_object_t));
    s->contacts_capacity = cfg.max_contacts;
    s->contacts = nimcp_calloc(s->contacts_capacity, sizeof(ip_contact_t));
    s->support_capacity = IP_MAX_SUPPORT_EDGES;
    s->support_graph = nimcp_calloc(s->support_capacity, sizeof(ip_support_edge_t));

    if (!s->objects || !s->contacts || !s->support_graph) {
        intuitive_physics_destroy(engine);
        return NULL;
    }

    s->gravity = vec3_scale(cfg.gravity_direction, cfg.gravity_magnitude);
    s->dt = cfg.dt;
    engine->initialized = true;

    LOG_INFO(LOG_TAG, "Intuitive physics engine created: max_objects=%u, dt=%.3f, "
             "gravity=%.1f, solver_iters=%u",
             cfg.max_objects, cfg.dt, cfg.gravity_magnitude, cfg.solver_iterations);

    return engine;
}

void intuitive_physics_destroy(intuitive_physics_engine_t* engine) {
    if (!engine) return;
    nimcp_free(engine->scene.objects);
    nimcp_free(engine->scene.contacts);
    nimcp_free(engine->scene.support_graph);
    nimcp_free(engine);
}

uint32_t intuitive_physics_add_object(intuitive_physics_engine_t* engine,
                                       const ip_object_t* obj) {
    if (!engine || !obj) return UINT32_MAX;
    ip_scene_t* s = &engine->scene;

    /* Find empty slot */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < s->capacity; i++) {
        if (!s->objects[i].active) { slot = i; break; }
    }
    if (slot == UINT32_MAX) {
        LOG_WARN(LOG_TAG, "Physics scene full (%u objects)", s->capacity);
        return UINT32_MAX;
    }

    s->objects[slot] = *obj;
    s->objects[slot].active = true;
    s->objects[slot].id = slot;
    s->objects[slot].inv_mass = (obj->mass > 0 && !obj->is_static) ? 1.0f / obj->mass : 0.0f;
    s->objects[slot].inv_inertia = compute_inv_inertia(&s->objects[slot]);
    if (obj->restitution <= 0) s->objects[slot].restitution = engine->config.restitution_default;
    if (obj->friction <= 0) s->objects[slot].friction = engine->config.friction_default;
    s->objects[slot].supported_by = UINT32_MAX;
    s->objects[slot].contained_in = UINT32_MAX;
    if (slot >= s->num_objects) s->num_objects = slot + 1;

    return slot;
}

void intuitive_physics_remove_object(intuitive_physics_engine_t* engine, uint32_t id) {
    if (!engine || id >= engine->scene.capacity) return;
    engine->scene.objects[id].active = false;
}

ip_object_t* intuitive_physics_get_object(intuitive_physics_engine_t* engine, uint32_t id) {
    if (!engine || id >= engine->scene.num_objects) return NULL;
    if (!engine->scene.objects[id].active) return NULL;
    return &engine->scene.objects[id];
}

int intuitive_physics_step(intuitive_physics_engine_t* engine, float dt) {
    if (!engine || !engine->initialized) return -1;
    ip_scene_t* s = &engine->scene;
    if (dt <= 0) dt = engine->config.dt;

    /* 1. Apply gravity (symplectic: velocity first) */
    for (uint32_t i = 0; i < s->num_objects; i++) {
        ip_object_t* o = &s->objects[i];
        if (!o->active || o->is_static) continue;

        o->velocity.vx += s->gravity.x * dt;
        o->velocity.vy += s->gravity.y * dt;
        o->velocity.vz += s->gravity.z * dt;
    }

    /* 2. Detect contacts */
    detect_all_contacts(engine);

    /* 3. Resolve contacts (impulse solver) */
    resolve_contacts(engine);

    /* 4. Integrate positions (symplectic Euler: use updated velocity) */
    uint32_t active = 0;
    for (uint32_t i = 0; i < s->num_objects; i++) {
        ip_object_t* o = &s->objects[i];
        if (!o->active) continue;
        active++;
        if (o->is_static) continue;

        o->position.x += o->velocity.vx * dt;
        o->position.y += o->velocity.vy * dt;
        o->position.z += o->velocity.vz * dt;

        /* Simple angular integration (for angular_velocity) */
        /* For now, just dampen angular velocity slightly */
        o->angular_velocity.x *= 0.99f;
        o->angular_velocity.y *= 0.99f;
        o->angular_velocity.z *= 0.99f;
    }

    /* 5. Rebuild support graph from contacts */
    rebuild_support_graph(engine);

    /* 6. Compute energy for monitoring */
    compute_energy(engine);

    s->time += dt;
    engine->stats.step_count++;
    engine->stats.active_objects = active;
    engine->stats.active_contacts = s->num_contacts;

    /* W8: aggregate emit — every 64 steps, record active obj + contact
     * counts + energy drift. Read-path checks whether a scene-graph
     * partner is present so downstream reasoners can be wired. */
    if ((engine->stats.step_count & 0x3F) == 0) {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain) {
            world_model_kg_emit_physics_step(_kg_brain,
                engine->stats.step_count,
                active, s->num_contacts,
                engine->stats.energy_drift);
            (void)world_model_kg_has_partner(_kg_brain,
                "cog_physics_scene_graph");
        }
    }

    return 0;
}

uint32_t intuitive_physics_add_ground(intuitive_physics_engine_t* engine) {
    ip_object_t ground = {
        .position = {0, 0, 0},
        .velocity = {0, 0, 0},
        .orientation = {1, 0, 0, 0},
        .mass = 0,
        .is_static = true,
        .visible = true,
        .active = true,
        .shape = {
            .type = IP_SHAPE_PLANE,
            .plane = { .normal = {0, 1, 0}, .offset = 0 }
        }
    };
    return intuitive_physics_add_object(engine, &ground);
}

int intuitive_physics_predict_trajectory(intuitive_physics_engine_t* engine,
                                          uint32_t object_id,
                                          float duration,
                                          wm_parietal_trajectory_t* out_traj) {
    if (!engine || !out_traj || object_id >= engine->scene.num_objects) return -1;
    if (!engine->scene.objects[object_id].active) return -1;

    float dt = engine->config.dt;
    uint32_t steps = (uint32_t)(duration / dt);
    if (steps == 0) steps = 1;
    if (steps > WM_PARIETAL_MAX_TRAJECTORY_HORIZON) steps = WM_PARIETAL_MAX_TRAJECTORY_HORIZON;

    /* Allocate trajectory states */
    wm_parietal_spatial_state_t* states = nimcp_calloc(steps, sizeof(*states));
    if (!states) return -1;

    /* Make a copy of the scene to simulate without modifying the real one */
    ip_scene_t copy;
    copy.capacity = engine->scene.capacity;
    copy.num_objects = engine->scene.num_objects;
    copy.objects = nimcp_calloc(copy.capacity, sizeof(ip_object_t));
    copy.contacts_capacity = engine->scene.contacts_capacity;
    copy.contacts = nimcp_calloc(copy.contacts_capacity, sizeof(ip_contact_t));
    copy.support_capacity = engine->scene.support_capacity;
    copy.support_graph = nimcp_calloc(copy.support_capacity, sizeof(ip_support_edge_t));
    copy.gravity = engine->scene.gravity;
    copy.dt = dt;
    copy.time = engine->scene.time;

    if (!copy.objects || !copy.contacts || !copy.support_graph) {
        nimcp_free(states);
        nimcp_free(copy.objects);
        nimcp_free(copy.contacts);
        nimcp_free(copy.support_graph);
        return -1;
    }

    memcpy(copy.objects, engine->scene.objects, copy.num_objects * sizeof(ip_object_t));

    /* Create a temporary engine for the copy */
    intuitive_physics_engine_t temp = *engine;
    temp.scene = copy;

    /* Simulate and record */
    for (uint32_t step = 0; step < steps; step++) {
        intuitive_physics_step(&temp, dt);
        ip_object_t* obj = &temp.scene.objects[object_id];
        states[step].object_id = object_id;
        states[step].position = obj->position;
        states[step].velocity = (wm_parietal_velocity_t){obj->velocity.vx, obj->velocity.vy, obj->velocity.vz};
        states[step].orientation = obj->orientation;
        states[step].angular_vel = obj->angular_velocity;
        states[step].mass = obj->mass;
        states[step].bounding_radius = shape_bounding_radius(&obj->shape);
        states[step].confidence = 1.0f - (float)step / (float)steps * 0.3f; /* decreasing confidence */
        states[step].timestamp = copy.time;
    }

    nimcp_free(copy.objects);
    nimcp_free(copy.contacts);
    nimcp_free(copy.support_graph);

    out_traj->object_id = object_id;
    out_traj->states = states;
    out_traj->length = steps;
    out_traj->dt = dt;
    out_traj->total_duration = duration;
    out_traj->overall_confidence = 0.9f;
    out_traj->physics_constrained = true;

    return 0;
}

bool intuitive_physics_will_collide(intuitive_physics_engine_t* engine,
                                     uint32_t obj_a, uint32_t obj_b,
                                     float within_time, float* collision_time) {
    if (!engine || obj_a >= engine->scene.num_objects || obj_b >= engine->scene.num_objects)
        return false;

    /* Quick check: are they already colliding? */
    ip_contact_t contact = {0};
    if (detect_collision(&engine->scene.objects[obj_a], &engine->scene.objects[obj_b], &contact)) {
        if (collision_time) *collision_time = 0;
        return true;
    }

    /* Simulate forward and check */
    float dt = engine->config.dt;
    uint32_t steps = (uint32_t)(within_time / dt);
    if (steps > 200) steps = 200;

    /* Copy just the two objects and simulate */
    ip_object_t a = engine->scene.objects[obj_a];
    ip_object_t b = engine->scene.objects[obj_b];
    wm_parietal_vec3_t g = engine->scene.gravity;

    for (uint32_t s = 0; s < steps; s++) {
        if (!a.is_static) {
            a.velocity.vx += g.x * dt; a.velocity.vy += g.y * dt; a.velocity.vz += g.z * dt;
            a.position.x += a.velocity.vx * dt; a.position.y += a.velocity.vy * dt; a.position.z += a.velocity.vz * dt;
        }
        if (!b.is_static) {
            b.velocity.vx += g.x * dt; b.velocity.vy += g.y * dt; b.velocity.vz += g.z * dt;
            b.position.x += b.velocity.vx * dt; b.position.y += b.velocity.vy * dt; b.position.z += b.velocity.vz * dt;
        }
        if (detect_collision(&a, &b, &contact)) {
            if (collision_time) *collision_time = (s + 1) * dt;
            return true;
        }
    }
    return false;
}

bool intuitive_physics_is_supported(const intuitive_physics_engine_t* engine, uint32_t id) {
    if (!engine || id >= engine->scene.num_objects) return false;
    const ip_object_t* obj = &engine->scene.objects[id];
    if (!obj->active) return false;
    if (obj->is_static) return true;  /* static objects are always "supported" */
    return obj->supported_by != UINT32_MAX;
}

bool intuitive_physics_is_stable(const intuitive_physics_engine_t* engine) {
    if (!engine) return false;
    const ip_scene_t* s = &engine->scene;

    /* Scene is stable if all dynamic objects are either supported or stationary */
    for (uint32_t i = 0; i < s->num_objects; i++) {
        const ip_object_t* o = &s->objects[i];
        if (!o->active || o->is_static) continue;

        float v2 = o->velocity.vx * o->velocity.vx +
                    o->velocity.vy * o->velocity.vy +
                    o->velocity.vz * o->velocity.vz;
        if (v2 > 0.01f && o->supported_by == UINT32_MAX) return false;
    }
    return true;
}

uint32_t intuitive_physics_get_support_chain(const intuitive_physics_engine_t* engine,
                                              uint32_t object_id,
                                              uint32_t* chain_buf, uint32_t buf_size) {
    if (!engine || !chain_buf || buf_size == 0) return 0;
    const ip_scene_t* s = &engine->scene;

    uint32_t count = 0;
    uint32_t current = object_id;

    /* Walk down the support chain */
    for (uint32_t depth = 0; depth < IP_MAX_CHAIN_DEPTH; depth++) {
        if (current >= s->num_objects || !s->objects[current].active) break;
        if (count < buf_size) chain_buf[count++] = current;
        uint32_t supporter = s->objects[current].supported_by;
        if (supporter == UINT32_MAX) break;
        current = supporter;
    }
    return count;
}

uint32_t intuitive_physics_predict_removal(intuitive_physics_engine_t* engine,
                                            uint32_t object_id,
                                            uint32_t* affected_buf, uint32_t buf_size) {
    if (!engine || !affected_buf) return 0;
    const ip_scene_t* s = &engine->scene;
    uint32_t count = 0;

    /* Find all objects directly or transitively supported by the removed object */
    bool visited[IP_MAX_OBJECTS] = {0};
    uint32_t queue[IP_MAX_OBJECTS];
    uint32_t qhead = 0, qtail = 0;

    /* Seed: objects directly supported by the removed object */
    for (uint32_t i = 0; i < s->num_objects; i++) {
        if (s->objects[i].active && s->objects[i].supported_by == object_id) {
            if (qtail < IP_MAX_OBJECTS) queue[qtail++] = i;
            visited[i] = true;
        }
    }

    /* BFS: follow support DAG upward */
    while (qhead < qtail) {
        uint32_t current = queue[qhead++];
        if (count < buf_size) affected_buf[count++] = current;

        /* Find objects supported by current */
        for (uint32_t i = 0; i < s->num_objects; i++) {
            if (!visited[i] && s->objects[i].active && s->objects[i].supported_by == current) {
                visited[i] = true;
                if (qtail < IP_MAX_OBJECTS) queue[qtail++] = i;
            }
        }
    }
    return count;
}

ip_stats_t intuitive_physics_get_stats(const intuitive_physics_engine_t* engine) {
    if (!engine) return (ip_stats_t){0};
    return engine->stats;
}
