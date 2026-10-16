/**
 * @file nimcp_versioning.c
 * @brief Model versioning and compatibility checking for edge deployments.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include <string.h>

/* ============================================================================
 * FNV-1a hash constants (32-bit)
 * ============================================================================ */

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME        16777619u

/* ============================================================================
 * nimcp_version_compute_arch_hash
 * ============================================================================ */

uint32_t nimcp_version_compute_arch_hash(const uint32_t* layer_sizes, uint32_t num_layers) {
    if (!layer_sizes || num_layers == 0) {
        return 0;
    }

    uint32_t hash = FNV_OFFSET_BASIS;

    for (uint32_t i = 0; i < num_layers; i++) {
        /* Feed each byte of the layer size into FNV-1a */
        const uint8_t* bytes = (const uint8_t*)&layer_sizes[i];
        for (size_t b = 0; b < sizeof(uint32_t); b++) {
            hash ^= (uint32_t)bytes[b];
            hash *= FNV_PRIME;
        }
    }

    return hash;
}

/* ============================================================================
 * nimcp_version_create
 * ============================================================================ */

nimcp_model_version_t nimcp_version_create(
    uint32_t major, uint32_t minor, uint32_t patch,
    const uint32_t* layer_sizes, uint32_t num_layers)
{
    nimcp_model_version_t ver;
    ver.major = major;
    ver.minor = minor;
    ver.patch = patch;
    ver.arch_hash = nimcp_version_compute_arch_hash(layer_sizes, num_layers);
    return ver;
}

/* ============================================================================
 * nimcp_version_check_compatibility
 * ============================================================================ */

int nimcp_version_check_compatibility(
    const nimcp_model_version_t* device,
    const nimcp_model_version_t* master,
    nimcp_compatibility_result_t* result)
{
    if (!device || !master || !result) {
        return -1;
    }

    memset(result, 0, sizeof(*result));
    result->device_version = *device;
    result->master_version = *master;

    /* Architecturally compatible if same major version and same arch_hash */
    result->architecturally_compatible =
        (device->major == master->major) && (device->arch_hash == master->arch_hash);

    /* Delta compatible if architecturally compatible and versions are close */
    if (result->architecturally_compatible) {
        result->delta_compatible = true;
        strncpy(result->migration_path, "delta", sizeof(result->migration_path) - 1);
    } else if (device->major == master->major) {
        /* Same major but different arch — need re-distillation */
        result->delta_compatible = false;
        strncpy(result->migration_path, "re-distill", sizeof(result->migration_path) - 1);
    } else {
        /* Different major version — incompatible */
        result->delta_compatible = false;
        strncpy(result->migration_path, "re-distill", sizeof(result->migration_path) - 1);
    }

    return 0;
}
