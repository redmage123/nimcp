/**
 * @file nimcp_embodiment.h
 * @brief URDF embodiment model for robot body-schema awareness.
 *
 * Parses a URDF XML string into a kinematic chain representation that the
 * brain can query for joint limits, body schema, and forward kinematics.
 * No libxml2 dependency -- uses a minimal hand-rolled parser for the URDF
 * subset (robot, link, joint).
 *
 * Usage:
 *   1. Create: nimcp_embodiment_create("my_robot")
 *   2. Load:   nimcp_embodiment_load_urdf(em, urdf_xml) or load_urdf_file(em, path)
 *   3. Query:  get_joint_by_name, get_joint_limits, compose_body_schema, forward_kinematics
 *   4. Destroy: nimcp_embodiment_destroy(em)
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_EMBODIMENT_H
#define NIMCP_EMBODIMENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Joint Types (URDF-compatible)
 * ============================================================================ */

typedef enum {
    NIMCP_JOINT_FIXED = 0,
    NIMCP_JOINT_REVOLUTE,
    NIMCP_JOINT_CONTINUOUS,
    NIMCP_JOINT_PRISMATIC,
    NIMCP_JOINT_FLOATING,
    NIMCP_JOINT_PLANAR,
} nimcp_joint_type_t;

/* ============================================================================
 * Joint Descriptor
 * ============================================================================ */

#define NIMCP_EMBODIMENT_NAME_MAX 64
#define NIMCP_EMBODIMENT_MAX_JOINTS 128
#define NIMCP_EMBODIMENT_MAX_LINKS  128

typedef struct {
    char name[NIMCP_EMBODIMENT_NAME_MAX];
    nimcp_joint_type_t type;
    char parent_link[NIMCP_EMBODIMENT_NAME_MAX];
    char child_link[NIMCP_EMBODIMENT_NAME_MAX];
    float axis[3];                 /* Joint axis (default 0,0,1) */
    float origin_xyz[3];           /* Translation from parent */
    float origin_rpy[3];           /* Rotation from parent (roll, pitch, yaw) */

    /* Limits (revolute/prismatic) */
    float lower;
    float upper;
    float max_velocity;
    float max_effort;
    float damping;
    float friction;
} nimcp_joint_desc_t;

/* ============================================================================
 * Link Descriptor
 * ============================================================================ */

typedef struct {
    char name[NIMCP_EMBODIMENT_NAME_MAX];
    float mass;
    float inertia[9];              /* 3x3 inertia tensor (row-major) */
    float com_xyz[3];              /* Center of mass */
    float bounding_box[3];         /* Approximate bounding box (x, y, z) */
} nimcp_link_desc_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_embodiment nimcp_embodiment_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Create an empty embodiment model.
 * @param name Robot name (max 63 chars).
 * @return Handle, or NULL on failure.
 */
nimcp_embodiment_t* nimcp_embodiment_create(const char* name);

/**
 * @brief Destroy an embodiment model and free all resources.
 */
void nimcp_embodiment_destroy(nimcp_embodiment_t* em);

/**
 * @brief Parse a URDF XML string and populate joints/links.
 * @param em  Handle.
 * @param xml URDF XML content (null-terminated).
 * @return 0 on success, -1 on parse error.
 */
int nimcp_embodiment_load_urdf(nimcp_embodiment_t* em, const char* xml);

/**
 * @brief Load a URDF file from disk and parse it.
 * @return 0 on success, -1 on error.
 */
int nimcp_embodiment_load_urdf_file(nimcp_embodiment_t* em, const char* path);

/**
 * @brief Get a joint descriptor by name.
 * @return Pointer to joint (owned by em), or NULL if not found.
 */
const nimcp_joint_desc_t* nimcp_embodiment_get_joint(const nimcp_embodiment_t* em,
                                                      const char* name);

/**
 * @brief Get a link descriptor by name.
 * @return Pointer to link (owned by em), or NULL if not found.
 */
const nimcp_link_desc_t* nimcp_embodiment_get_link(const nimcp_embodiment_t* em,
                                                    const char* name);

/**
 * @brief Get the number of actuated (non-fixed) joints.
 */
uint32_t nimcp_embodiment_get_num_actuated(const nimcp_embodiment_t* em);

/**
 * @brief Get joint limits for all actuated joints.
 *
 * Fills lower[i] and upper[i] for each actuated joint.
 *
 * @param em     Handle.
 * @param lower  Output array for lower limits (size >= num_actuated).
 * @param upper  Output array for upper limits (size >= num_actuated).
 * @param max_joints Size of lower/upper arrays.
 * @return Number of actuated joints filled, or 0 on error.
 */
uint32_t nimcp_embodiment_get_joint_limits(const nimcp_embodiment_t* em,
                                           float* lower, float* upper,
                                           uint32_t max_joints);

/**
 * @brief Compose a body schema vector for brain input.
 *
 * Produces a normalized representation: for each actuated joint, outputs
 * the normalized position [0,1] within its range and a limit proximity
 * signal (0=center, 1=at limit).
 *
 * @param em            Handle.
 * @param joint_positions Current joint positions (size >= num_actuated).
 * @param schema        Output array (size >= 2 * num_actuated).
 * @param schema_size   Size of schema array.
 * @return Number of elements written, or 0 on error.
 */
uint32_t nimcp_embodiment_compose_body_schema(const nimcp_embodiment_t* em,
                                              const float* joint_positions,
                                              float* schema,
                                              uint32_t schema_size);

/**
 * @brief Forward kinematics: compute end effector position.
 *
 * Chains rotation matrices through the joint chain from root to the
 * specified end link, applying joint positions.
 *
 * @param em              Handle.
 * @param joint_positions Current positions for actuated joints.
 * @param end_link        Name of end effector link (NULL = last link).
 * @param xyz_out         Output xyz position (3 floats).
 * @return 0 on success, -1 on error.
 */
int nimcp_embodiment_forward_kinematics(const nimcp_embodiment_t* em,
                                        const float* joint_positions,
                                        const char* end_link,
                                        float* xyz_out);

/**
 * @brief Get the joint type name as a string.
 */
const char* nimcp_joint_type_name(nimcp_joint_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMBODIMENT_H */
