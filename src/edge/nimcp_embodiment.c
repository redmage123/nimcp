/**
 * @file nimcp_embodiment.c
 * @brief URDF embodiment model for robot body-schema awareness.
 *
 * Minimal hand-rolled XML parser for the URDF subset (<robot>, <link>,
 * <joint>). Extracts kinematic chain, joint limits, and link properties.
 * Forward kinematics chains rotation matrices through revolute joints.
 *
 * No libxml2 dependency -- uses strstr/sscanf for parsing.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_embodiment.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_MODULE "EMBODIMENT"

/**
 * @brief Safe float conversion from string, replacing atof().
 * Returns 0.0f if conversion fails.
 */
static float _safe_strtof(const char* str) {
    char* endptr;
    float val = strtof(str, &endptr);
    if (endptr == str) val = 0.0f; /* Conversion failed */
    return val;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_embodiment {
    char name[NIMCP_EMBODIMENT_NAME_MAX];
    nimcp_joint_desc_t joints[NIMCP_EMBODIMENT_MAX_JOINTS];
    nimcp_link_desc_t links[NIMCP_EMBODIMENT_MAX_LINKS];
    uint32_t num_joints;
    uint32_t num_links;
    uint32_t num_actuated;
    float total_mass;
    float workspace_radius;
};

/* ============================================================================
 * Joint Type Name
 * ============================================================================ */

const char* nimcp_joint_type_name(nimcp_joint_type_t type) {
    switch (type) {
        case NIMCP_JOINT_FIXED:      return "fixed";
        case NIMCP_JOINT_REVOLUTE:   return "revolute";
        case NIMCP_JOINT_CONTINUOUS: return "continuous";
        case NIMCP_JOINT_PRISMATIC:  return "prismatic";
        case NIMCP_JOINT_FLOATING:   return "floating";
        case NIMCP_JOINT_PLANAR:     return "planar";
        default:                     return "unknown";
    }
}

/* ============================================================================
 * Helpers
 * ============================================================================ */

static nimcp_joint_type_t _parse_joint_type(const char* str) {
    if (!str) return NIMCP_JOINT_FIXED;
    if (strstr(str, "revolute"))   return NIMCP_JOINT_REVOLUTE;
    if (strstr(str, "continuous")) return NIMCP_JOINT_CONTINUOUS;
    if (strstr(str, "prismatic"))  return NIMCP_JOINT_PRISMATIC;
    if (strstr(str, "floating"))   return NIMCP_JOINT_FLOATING;
    if (strstr(str, "planar"))     return NIMCP_JOINT_PLANAR;
    return NIMCP_JOINT_FIXED;
}

/**
 * @brief Extract an XML attribute value from a tag region.
 *
 * Looks for attr="value" within [start, end) and copies value to buf.
 * Returns true if found.
 */
static bool _extract_attr(const char* start, const char* end,
                           const char* attr, char* buf, size_t buf_size) {
    if (!start || !end || !attr || !buf || buf_size == 0) {
        return false;
    }

    /* Build search pattern: attr=" */
    char pattern[128];
    int n = snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        return false;
    }

    const char* p = start;
    while (p < end) {
        const char* found = strstr(p, pattern);
        if (!found || found >= end) {
            return false;
        }
        const char* val_start = found + strlen(pattern);
        const char* val_end = strchr(val_start, '"');
        if (!val_end || val_end > end) {
            return false;
        }
        size_t len = (size_t)(val_end - val_start);
        if (len >= buf_size) {
            len = buf_size - 1;
        }
        memcpy(buf, val_start, len);
        buf[len] = '\0';
        return true;
    }
    return false;
}

/**
 * @brief Parse "x y z" float triplet from a string.
 */
static bool _parse_xyz(const char* str, float* out) {
    if (!str || !out) return false;
    int matched = sscanf(str, "%f %f %f", &out[0], &out[1], &out[2]);
    return matched == 3;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_embodiment_t* nimcp_embodiment_create(const char* name) {
    nimcp_embodiment_t* em = (nimcp_embodiment_t*)nimcp_calloc(
        1, sizeof(nimcp_embodiment_t));
    if (!em) {
        LOG_ERROR("[%s] Failed to allocate embodiment", LOG_MODULE);
        return NULL;
    }

    if (name) {
        strncpy(em->name, name, NIMCP_EMBODIMENT_NAME_MAX - 1);
        em->name[NIMCP_EMBODIMENT_NAME_MAX - 1] = '\0';
    }

    LOG_INFO("[%s] Created embodiment '%s'", LOG_MODULE, em->name);
    return em;
}

void nimcp_embodiment_destroy(nimcp_embodiment_t* em) {
    if (!em) {
        return;
    }
    LOG_DEBUG("[%s] Destroyed embodiment '%s'", LOG_MODULE, em->name);
    nimcp_free(em);
}

/* ============================================================================
 * URDF Parsing
 * ============================================================================ */

/**
 * @brief Parse all <link> elements from URDF XML.
 */
static int _parse_links(nimcp_embodiment_t* em, const char* xml) {
    const char* cursor = xml;

    while (em->num_links < NIMCP_EMBODIMENT_MAX_LINKS) {
        const char* link_start = strstr(cursor, "<link");
        if (!link_start) break;

        /* Find the end of the link tag or self-closing */
        const char* link_close = strstr(link_start, "</link>");
        const char* link_self = strstr(link_start, "/>");
        const char* link_end;

        if (link_close && (!link_self || link_close < link_self)) {
            link_end = link_close + 7;
        } else if (link_self) {
            link_end = link_self + 2;
        } else {
            break;
        }

        nimcp_link_desc_t* link = &em->links[em->num_links];
        memset(link, 0, sizeof(nimcp_link_desc_t));

        /* Extract name */
        _extract_attr(link_start, link_end, "name", link->name,
                       NIMCP_EMBODIMENT_NAME_MAX);

        /* Parse <mass value="..."/> */
        const char* mass_tag = strstr(link_start, "<mass");
        if (mass_tag && mass_tag < link_end) {
            char mass_str[32] = {0};
            if (_extract_attr(mass_tag, link_end, "value", mass_str, sizeof(mass_str))) {
                link->mass = _safe_strtof(mass_str);
                em->total_mass += link->mass;
            }
        }

        /* Parse <origin xyz="..."/> inside <inertial> */
        const char* inertial = strstr(link_start, "<inertial");
        if (inertial && inertial < link_end) {
            const char* origin = strstr(inertial, "<origin");
            if (origin && origin < link_end) {
                char xyz_str[128] = {0};
                if (_extract_attr(origin, link_end, "xyz", xyz_str, sizeof(xyz_str))) {
                    _parse_xyz(xyz_str, link->com_xyz);
                }
            }
        }

        /* Parse <inertia ixx="..." ... /> */
        const char* inertia_tag = strstr(link_start, "<inertia");
        if (inertia_tag && inertia_tag < link_end) {
            char val[32];
            /* 3x3 symmetric inertia matrix (row-major):
             * [0]=ixx [1]=ixy [2]=ixz
             * [3]=ixy [4]=iyy [5]=iyz
             * [6]=ixz [7]=iyz [8]=izz
             * Map 6 unique URDF attrs to correct matrix indices */
            const char* inertia_attrs[] = {"ixx", "ixy", "ixz", "iyy", "iyz", "izz"};
            const int inertia_indices[] = {0, 1, 2, 4, 5, 8};
            for (int k = 0; k < 6; k++) {
                if (_extract_attr(inertia_tag, link_end, inertia_attrs[k], val, sizeof(val))) {
                    float fval = _safe_strtof(val);
                    link->inertia[inertia_indices[k]] = fval;
                    /* Mirror symmetric entries */
                    if (k == 1) link->inertia[3] = fval;  /* ixy → [1] and [3] */
                    if (k == 2) link->inertia[6] = fval;  /* ixz → [2] and [6] */
                    if (k == 4) link->inertia[7] = fval;  /* iyz → [5] and [7] */
                }
            }
        }

        em->num_links++;
        cursor = link_end;
    }

    return 0;
}

/**
 * @brief Parse all <joint> elements from URDF XML.
 */
static int _parse_joints(nimcp_embodiment_t* em, const char* xml) {
    const char* cursor = xml;

    while (em->num_joints < NIMCP_EMBODIMENT_MAX_JOINTS) {
        const char* joint_start = strstr(cursor, "<joint");
        if (!joint_start) break;

        const char* joint_close = strstr(joint_start, "</joint>");
        if (!joint_close) break;
        const char* joint_end = joint_close + 8;

        nimcp_joint_desc_t* jt = &em->joints[em->num_joints];
        memset(jt, 0, sizeof(nimcp_joint_desc_t));

        /* Default axis */
        jt->axis[2] = 1.0f;

        /* Name and type */
        _extract_attr(joint_start, joint_end, "name", jt->name,
                       NIMCP_EMBODIMENT_NAME_MAX);
        char type_str[32] = {0};
        if (_extract_attr(joint_start, joint_end, "type", type_str, sizeof(type_str))) {
            jt->type = _parse_joint_type(type_str);
        }

        /* Parent and child links */
        const char* parent_tag = strstr(joint_start, "<parent");
        if (parent_tag && parent_tag < joint_end) {
            _extract_attr(parent_tag, joint_end, "link", jt->parent_link,
                           NIMCP_EMBODIMENT_NAME_MAX);
        }
        const char* child_tag = strstr(joint_start, "<child");
        if (child_tag && child_tag < joint_end) {
            _extract_attr(child_tag, joint_end, "link", jt->child_link,
                           NIMCP_EMBODIMENT_NAME_MAX);
        }

        /* Origin */
        const char* origin = strstr(joint_start, "<origin");
        if (origin && origin < joint_end) {
            char xyz_str[128] = {0}, rpy_str[128] = {0};
            if (_extract_attr(origin, joint_end, "xyz", xyz_str, sizeof(xyz_str))) {
                _parse_xyz(xyz_str, jt->origin_xyz);
            }
            if (_extract_attr(origin, joint_end, "rpy", rpy_str, sizeof(rpy_str))) {
                _parse_xyz(rpy_str, jt->origin_rpy);
            }
        }

        /* Axis */
        const char* axis_tag = strstr(joint_start, "<axis");
        if (axis_tag && axis_tag < joint_end) {
            char xyz_str[128] = {0};
            if (_extract_attr(axis_tag, joint_end, "xyz", xyz_str, sizeof(xyz_str))) {
                _parse_xyz(xyz_str, jt->axis);
            }
        }

        /* Limits */
        const char* limit_tag = strstr(joint_start, "<limit");
        if (limit_tag && limit_tag < joint_end) {
            char val[32];
            if (_extract_attr(limit_tag, joint_end, "lower", val, sizeof(val)))
                jt->lower = _safe_strtof(val);
            if (_extract_attr(limit_tag, joint_end, "upper", val, sizeof(val)))
                jt->upper = _safe_strtof(val);
            if (_extract_attr(limit_tag, joint_end, "velocity", val, sizeof(val)))
                jt->max_velocity = _safe_strtof(val);
            if (_extract_attr(limit_tag, joint_end, "effort", val, sizeof(val)))
                jt->max_effort = _safe_strtof(val);
        }

        /* Dynamics */
        const char* dynamics = strstr(joint_start, "<dynamics");
        if (dynamics && dynamics < joint_end) {
            char val[32];
            if (_extract_attr(dynamics, joint_end, "damping", val, sizeof(val)))
                jt->damping = _safe_strtof(val);
            if (_extract_attr(dynamics, joint_end, "friction", val, sizeof(val)))
                jt->friction = _safe_strtof(val);
        }

        /* Count actuated joints */
        if (jt->type != NIMCP_JOINT_FIXED) {
            em->num_actuated++;
        }

        em->num_joints++;
        cursor = joint_end;
    }

    return 0;
}

int nimcp_embodiment_load_urdf(nimcp_embodiment_t* em, const char* xml) {
    if (!em || !xml) {
        return -1;
    }

    /* Reset existing data */
    em->num_joints = 0;
    em->num_links = 0;
    em->num_actuated = 0;
    em->total_mass = 0.0f;
    em->workspace_radius = 0.0f;

    /* Check for <robot> tag */
    const char* robot = strstr(xml, "<robot");
    if (!robot) {
        LOG_ERROR("[%s] No <robot> tag found in URDF", LOG_MODULE);
        return -1;
    }

    /* Extract robot name if present */
    const char* robot_close = strstr(robot, ">");
    if (robot_close) {
        char rname[NIMCP_EMBODIMENT_NAME_MAX] = {0};
        if (_extract_attr(robot, robot_close + 1, "name", rname, sizeof(rname))) {
            strncpy(em->name, rname, NIMCP_EMBODIMENT_NAME_MAX - 1);
        }
    }

    /* Parse links and joints */
    if (_parse_links(em, xml) != 0) {
        LOG_ERROR("[%s] Failed to parse links", LOG_MODULE);
        return -1;
    }
    if (_parse_joints(em, xml) != 0) {
        LOG_ERROR("[%s] Failed to parse joints", LOG_MODULE);
        return -1;
    }

    /* Estimate workspace radius as sum of link offsets */
    float radius = 0.0f;
    for (uint32_t i = 0; i < em->num_joints; i++) {
        float* xyz = em->joints[i].origin_xyz;
        radius += sqrtf(xyz[0]*xyz[0] + xyz[1]*xyz[1] + xyz[2]*xyz[2]);
    }
    em->workspace_radius = radius;

    LOG_INFO("[%s] Loaded URDF '%s': %u links, %u joints (%u actuated), "
             "total_mass=%.2f kg, workspace_radius=%.3f m",
             LOG_MODULE, em->name, em->num_links, em->num_joints,
             em->num_actuated, em->total_mass, em->workspace_radius);

    return 0;
}

int nimcp_embodiment_load_urdf_file(nimcp_embodiment_t* em, const char* path) {
    if (!em || !path) {
        return -1;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("[%s] Cannot open URDF file: %s", LOG_MODULE, path);
        return -1;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) {
        LOG_ERROR("[%s] URDF file size invalid: %ld bytes", LOG_MODULE, size);
        fclose(f);
        return -1;
    }

    char* buf = (char*)nimcp_calloc(1, (size_t)size + 1);
    if (!buf) {
        LOG_ERROR("[%s] Failed to allocate URDF buffer", LOG_MODULE);
        fclose(f);
        return -1;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read] = '\0';

    int result = nimcp_embodiment_load_urdf(em, buf);
    nimcp_free(buf);
    return result;
}

/* ============================================================================
 * Accessors
 * ============================================================================ */

const nimcp_joint_desc_t* nimcp_embodiment_get_joint(const nimcp_embodiment_t* em,
                                                      const char* name) {
    if (!em || !name) {
        return NULL;
    }
    for (uint32_t i = 0; i < em->num_joints; i++) {
        if (strcmp(em->joints[i].name, name) == 0) {
            return &em->joints[i];
        }
    }
    return NULL;
}

const nimcp_link_desc_t* nimcp_embodiment_get_link(const nimcp_embodiment_t* em,
                                                    const char* name) {
    if (!em || !name) {
        return NULL;
    }
    for (uint32_t i = 0; i < em->num_links; i++) {
        if (strcmp(em->links[i].name, name) == 0) {
            return &em->links[i];
        }
    }
    return NULL;
}

uint32_t nimcp_embodiment_get_num_actuated(const nimcp_embodiment_t* em) {
    if (!em) {
        return 0;
    }
    return em->num_actuated;
}

uint32_t nimcp_embodiment_get_joint_limits(const nimcp_embodiment_t* em,
                                           float* lower, float* upper,
                                           uint32_t max_joints) {
    if (!em || !lower || !upper) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < em->num_joints && count < max_joints; i++) {
        if (em->joints[i].type == NIMCP_JOINT_FIXED) {
            continue;
        }
        lower[count] = em->joints[i].lower;
        upper[count] = em->joints[i].upper;
        count++;
    }
    return count;
}

/* ============================================================================
 * Body Schema
 * ============================================================================ */

uint32_t nimcp_embodiment_compose_body_schema(const nimcp_embodiment_t* em,
                                              const float* joint_positions,
                                              float* schema,
                                              uint32_t schema_size) {
    if (!em || !joint_positions || !schema) {
        return 0;
    }

    uint32_t needed = em->num_actuated * 2;
    if (schema_size < needed) {
        LOG_ERROR("[%s] Schema buffer too small: %u < %u",
                  LOG_MODULE, schema_size, needed);
        return 0;
    }

    uint32_t act_idx = 0;
    for (uint32_t i = 0; i < em->num_joints; i++) {
        const nimcp_joint_desc_t* jt = &em->joints[i];
        if (jt->type == NIMCP_JOINT_FIXED) {
            continue;
        }

        float pos = joint_positions[act_idx];
        float range = jt->upper - jt->lower;

        /* Normalized position [0, 1] */
        float norm_pos = 0.5f;
        if (range > 1e-6f) {
            norm_pos = (pos - jt->lower) / range;
            if (norm_pos < 0.0f) norm_pos = 0.0f;
            if (norm_pos > 1.0f) norm_pos = 1.0f;
        }

        /* Limit proximity: 0 at center, 1 at either limit */
        float proximity = fabsf(2.0f * norm_pos - 1.0f);

        schema[act_idx * 2]     = norm_pos;
        schema[act_idx * 2 + 1] = proximity;
        act_idx++;
    }

    return act_idx * 2;
}

/* ============================================================================
 * Forward Kinematics
 * ============================================================================ */

/**
 * @brief Build a 3x3 rotation matrix from RPY angles.
 *
 * Convention: R = Rz(yaw) * Ry(pitch) * Rx(roll)
 * Matrix stored in row-major order.
 */
static void _rpy_to_rotation(float roll, float pitch, float yaw, float R[9]) {
    float cr = cosf(roll),  sr = sinf(roll);
    float cp = cosf(pitch), sp = sinf(pitch);
    float cy = cosf(yaw),   sy = sinf(yaw);

    R[0] = cy * cp;
    R[1] = cy * sp * sr - sy * cr;
    R[2] = cy * sp * cr + sy * sr;
    R[3] = sy * cp;
    R[4] = sy * sp * sr + cy * cr;
    R[5] = sy * sp * cr - cy * sr;
    R[6] = -sp;
    R[7] = cp * sr;
    R[8] = cp * cr;
}

/**
 * @brief Build a rotation matrix for rotation about an axis by an angle.
 * Rodrigues' formula.
 */
static void _axis_angle_to_rotation(const float axis[3], float angle, float R[9]) {
    float c = cosf(angle), s = sinf(angle), t = 1.0f - c;
    float x = axis[0], y = axis[1], z = axis[2];

    /* Normalize axis */
    float len = sqrtf(x*x + y*y + z*z);
    if (len > 1e-6f) {
        x /= len; y /= len; z /= len;
    }

    R[0] = t*x*x + c;     R[1] = t*x*y - s*z;   R[2] = t*x*z + s*y;
    R[3] = t*x*y + s*z;   R[4] = t*y*y + c;      R[5] = t*y*z - s*x;
    R[6] = t*x*z - s*y;   R[7] = t*y*z + s*x;    R[8] = t*z*z + c;
}

/**
 * @brief Multiply two 3x3 matrices: C = A * B.
 */
static void _mat3_mul(const float A[9], const float B[9], float C[9]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            C[i*3+j] = A[i*3+0]*B[0*3+j] + A[i*3+1]*B[1*3+j] + A[i*3+2]*B[2*3+j];
        }
    }
}

/**
 * @brief Apply rotation: v_out = R * v_in.
 */
static void _mat3_vec(const float R[9], const float v[3], float out[3]) {
    out[0] = R[0]*v[0] + R[1]*v[1] + R[2]*v[2];
    out[1] = R[3]*v[0] + R[4]*v[1] + R[5]*v[2];
    out[2] = R[6]*v[0] + R[7]*v[1] + R[8]*v[2];
}

int nimcp_embodiment_forward_kinematics(const nimcp_embodiment_t* em,
                                        const float* joint_positions,
                                        const char* end_link,
                                        float* xyz_out) {
    if (!em || !joint_positions || !xyz_out) {
        return -1;
    }
    if (em->num_joints == 0) {
        xyz_out[0] = xyz_out[1] = xyz_out[2] = 0.0f;
        return 0;
    }

    /* Accumulated transform: rotation R and translation t */
    float R[9] = {1,0,0, 0,1,0, 0,0,1};  /* Identity */
    float t[3] = {0, 0, 0};

    uint32_t act_idx = 0;

    for (uint32_t i = 0; i < em->num_joints; i++) {
        const nimcp_joint_desc_t* jt = &em->joints[i];

        /* Apply joint origin transform: t += R * origin_xyz */
        float rotated_origin[3];
        _mat3_vec(R, jt->origin_xyz, rotated_origin);
        t[0] += rotated_origin[0];
        t[1] += rotated_origin[1];
        t[2] += rotated_origin[2];

        /* Apply joint origin rotation (RPY) */
        float R_origin[9];
        _rpy_to_rotation(jt->origin_rpy[0], jt->origin_rpy[1],
                         jt->origin_rpy[2], R_origin);
        float R_new[9];
        _mat3_mul(R, R_origin, R_new);
        memcpy(R, R_new, sizeof(R));

        /* Apply joint rotation for actuated joints */
        if (jt->type == NIMCP_JOINT_REVOLUTE || jt->type == NIMCP_JOINT_CONTINUOUS) {
            float angle = joint_positions[act_idx];
            float R_joint[9];
            _axis_angle_to_rotation(jt->axis, angle, R_joint);
            _mat3_mul(R, R_joint, R_new);
            memcpy(R, R_new, sizeof(R));
            act_idx++;
        } else if (jt->type == NIMCP_JOINT_PRISMATIC) {
            /* Prismatic: translate along axis */
            float d = joint_positions[act_idx];
            float axis_scaled[3] = {jt->axis[0]*d, jt->axis[1]*d, jt->axis[2]*d};
            float rotated_axis[3];
            _mat3_vec(R, axis_scaled, rotated_axis);
            t[0] += rotated_axis[0];
            t[1] += rotated_axis[1];
            t[2] += rotated_axis[2];
            act_idx++;
        } else if (jt->type != NIMCP_JOINT_FIXED) {
            /* Floating/Planar — skip position for now */
            act_idx++;
        }

        /* Check if we've reached the end link */
        if (end_link && strcmp(jt->child_link, end_link) == 0) {
            break;
        }
    }

    xyz_out[0] = t[0];
    xyz_out[1] = t[1];
    xyz_out[2] = t[2];

    return 0;
}
