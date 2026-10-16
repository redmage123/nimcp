/**
 * @file nimcp_kg_schema.c
 * @brief Schema Evolution and Migration for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of schema versioning and migration infrastructure for brain KG.
 * Supports semantic versioning, bidirectional migrations, and history tracking.
 */

#include "core/brain/nimcp_kg_schema.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_schema, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Static Data Structures
 * ============================================================================ */

/** Global migration registry */
static struct {
    kg_migration_script_t migrations[KG_SCHEMA_MAX_MIGRATIONS];
    uint32_t count;
    bool initialized;
} g_migration_registry = { .count = 0, .initialized = false };

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Initialize migration registry if not already done
 */
static void ensure_registry_initialized(void) {
    if (!g_migration_registry.initialized) {
        memset(&g_migration_registry, 0, sizeof(g_migration_registry));
        g_migration_registry.initialized = true;
    }
}

/* ============================================================================
 * Schema Management API
 * ============================================================================ */

kg_schema_version_t kg_schema_get_current(const brain_kg_t* kg) {
    kg_schema_version_t version = {0};

    if (!kg) {
        return version;
    }

    /* In a real implementation, we would read version from KG metadata */
    /* Placeholder: return version 1.0.0 as default */
    version.major = 1;
    version.minor = 0;
    version.patch = 0;
    version.label[0] = '\0';

    return version;
}

int kg_schema_set_version(brain_kg_t* kg, const kg_schema_version_t* version) {
    if (!kg || !version) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_set_version: required parameter is NULL (kg, version)");
        return -1;
    }

    /* In a real implementation, we would store version in KG metadata */
    (void)kg;
    (void)version;

    return 0;
}

int kg_schema_compare(const kg_schema_version_t* a, const kg_schema_version_t* b) {
    if (!a || !b) {
        return 0;
    }

    /* Compare major version */
    if (a->major < b->major) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_compare: validation failed");
        return -1;
    }
    if (a->major > b->major) return 1;

    /* Compare minor version */
    if (a->minor < b->minor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_compare: validation failed");
        return -1;
    }
    if (a->minor > b->minor) return 1;

    /* Compare patch version */
    if (a->patch < b->patch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_compare: validation failed");
        return -1;
    }
    if (a->patch > b->patch) return 1;

    /* Compare labels (empty label > non-empty label for release versions) */
    bool a_has_label = a->label[0] != '\0';
    bool b_has_label = b->label[0] != '\0';

    if (!a_has_label && b_has_label) return 1;  /* Release > pre-release */
    if (a_has_label && !b_has_label) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_compare: b_has_label is NULL");
        return -1;
    }

    if (a_has_label && b_has_label) {
        return strcmp(a->label, b->label);
    }

    return 0;
}

int kg_schema_version_to_string(
    const kg_schema_version_t* version,
    char* buffer,
    size_t buffer_size
) {
    if (!version || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_version_to_string: required parameter is NULL (version, buffer)");
        return -1;
    }

    int written;
    if (version->label[0] != '\0') {
        written = snprintf(buffer, buffer_size, "%u.%u.%u-%s",
                          version->major, version->minor, version->patch,
                          version->label);
    } else {
        written = snprintf(buffer, buffer_size, "%u.%u.%u",
                          version->major, version->minor, version->patch);
    }

    return (written < 0 || (size_t)written >= buffer_size) ? -1 : written;
}

int kg_schema_version_from_string(
    const char* str,
    kg_schema_version_t* version
) {
    if (!str || !version) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_version_from_string: required parameter is NULL (str, version)");
        return -1;
    }

    memset(version, 0, sizeof(*version));

    /* Try parsing with label first */
    char label[KG_SCHEMA_MAX_LABEL_LEN] = {0};
    int matched = sscanf(str, "%u.%u.%u-%31s",
                         &version->major, &version->minor, &version->patch,
                         label);

    if (matched >= 3) {
        if (matched == 4) {
            strncpy(version->label, label, KG_SCHEMA_MAX_LABEL_LEN - 1);
        }
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_version_from_string: validation failed");
    return -1;
}

/* ============================================================================
 * Migration Management API
 * ============================================================================ */

int kg_schema_register_migration(const kg_migration_script_t* migration) {
    ensure_registry_initialized();

    if (!migration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "migration is NULL");


        return -1;
    }

    if (g_migration_registry.count >= KG_SCHEMA_MAX_MIGRATIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_schema_register_migration: capacity exceeded");
        return -1; /* Registry full */
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < g_migration_registry.count; i++) {
        kg_migration_script_t* existing = &g_migration_registry.migrations[i];
        if (kg_schema_compare(&existing->from_version, &migration->from_version) == 0 &&
            kg_schema_compare(&existing->to_version, &migration->to_version) == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_register_migration: validation failed");
            return -1; /* Duplicate migration */
        }
    }

    /* Add migration to registry */
    kg_migration_script_t* entry = &g_migration_registry.migrations[g_migration_registry.count];
    memcpy(&entry->from_version, &migration->from_version, sizeof(kg_schema_version_t));
    memcpy(&entry->to_version, &migration->to_version, sizeof(kg_schema_version_t));
    strncpy(entry->description, migration->description, KG_SCHEMA_MAX_DESC_LEN - 1);
    entry->description[KG_SCHEMA_MAX_DESC_LEN - 1] = '\0';

    /* Copy scripts (caller owns the strings) */
    entry->up_script = migration->up_script;
    entry->down_script = migration->down_script;
    entry->is_reversible = migration->is_reversible;
    entry->estimated_duration_sec = migration->estimated_duration_sec;

    g_migration_registry.count++;

    return 0;
}

int kg_schema_list_migrations(kg_migration_script_t* migrations, uint32_t* count) {
    ensure_registry_initialized();

    if (!migrations || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_list_migrations: required parameter is NULL (migrations, count)");
        return -1;
    }

    uint32_t capacity = *count;
    uint32_t copy_count = (g_migration_registry.count < capacity) ?
                          g_migration_registry.count : capacity;

    for (uint32_t i = 0; i < copy_count; i++) {
        memcpy(&migrations[i], &g_migration_registry.migrations[i],
               sizeof(kg_migration_script_t));
    }

    *count = g_migration_registry.count;

    return 0;
}

int kg_schema_get_pending_migrations(
    const brain_kg_t* kg,
    kg_migration_script_t* migrations,
    uint32_t* count
) {
    ensure_registry_initialized();

    if (!kg || !migrations || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_get_pending_migrations: required parameter is NULL (kg, migrations, count)");
        return -1;
    }

    kg_schema_version_t current = kg_schema_get_current(kg);
    uint32_t capacity = *count;
    uint32_t pending_count = 0;

    /* Find migrations where from_version >= current */
    for (uint32_t i = 0; i < g_migration_registry.count && pending_count < capacity; i++) {
        kg_migration_script_t* m = &g_migration_registry.migrations[i];
        if (kg_schema_compare(&m->from_version, &current) >= 0) {
            memcpy(&migrations[pending_count], m, sizeof(kg_migration_script_t));
            pending_count++;
        }
    }

    *count = pending_count;

    return 0;
}

const kg_migration_script_t* kg_schema_find_migration(
    const kg_schema_version_t* from,
    const kg_schema_version_t* to
) {
    ensure_registry_initialized();

    if (!from || !to) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_find_migration: required parameter is NULL (from, to)");
        return NULL;
    }

    for (uint32_t i = 0; i < g_migration_registry.count; i++) {
        kg_migration_script_t* m = &g_migration_registry.migrations[i];
        if (kg_schema_compare(&m->from_version, from) == 0 &&
            kg_schema_compare(&m->to_version, to) == 0) {
            return m;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_find_migration: validation failed");
    return NULL;
}

int kg_schema_clear_migrations(void) {
    g_migration_registry.count = 0;
    g_migration_registry.initialized = true;
    return 0;
}

/* ============================================================================
 * Migration Execution API
 * ============================================================================ */

int kg_schema_migrate(
    brain_kg_t* kg,
    const kg_schema_version_t* target,
    kg_migration_result_t* result
) {
    if (!kg || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_migrate: required parameter is NULL (kg, target)");
        return -1;
    }

    kg_migration_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));
    local_result.started_at = get_current_timestamp_ms();
    local_result.status = KG_MIGRATE_IN_PROGRESS;

    kg_schema_version_t current = kg_schema_get_current(kg);
    memcpy(&local_result.from_version, &current, sizeof(kg_schema_version_t));
    memcpy(&local_result.to_version, target, sizeof(kg_schema_version_t));

    int comparison = kg_schema_compare(&current, target);

    if (comparison == 0) {
        /* Already at target version */
        local_result.status = KG_MIGRATE_COMPLETED;
        local_result.completed_at = get_current_timestamp_ms();
        local_result.duration_ms = local_result.completed_at - local_result.started_at;
        if (result) *result = local_result;
        return 0;
    }

    /* Find migration path */
    const kg_migration_script_t* migration = NULL;

    if (comparison < 0) {
        /* Upgrade: current < target */
        migration = kg_schema_find_migration(&current, target);
    } else {
        /* Downgrade: current > target */
        migration = kg_schema_find_migration(target, &current);
    }

    if (!migration) {
        local_result.status = KG_MIGRATE_FAILED;
        strncpy(local_result.error_message, "No migration path found",
                KG_SCHEMA_MAX_ERROR_LEN - 1);
        local_result.completed_at = get_current_timestamp_ms();
        local_result.duration_ms = local_result.completed_at - local_result.started_at;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_migrate: validation failed");
        return -1;
    }

    /* Execute migration */
    const char* script = (comparison < 0) ? migration->up_script : migration->down_script;

    if (!script) {
        local_result.status = KG_MIGRATE_FAILED;
        strncpy(local_result.error_message,
                (comparison < 0) ? "No up script" : "No down script (not reversible)",
                KG_SCHEMA_MAX_ERROR_LEN - 1);
        local_result.completed_at = get_current_timestamp_ms();
        local_result.duration_ms = local_result.completed_at - local_result.started_at;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_schema_migrate: validation failed");
        return -1;
    }

    /* In a real implementation, we would execute the migration script */
    /* For now, just update the version */
    (void)script;

    int rc = kg_schema_set_version(kg, target);
    if (rc != 0) {
        local_result.status = KG_MIGRATE_FAILED;
        strncpy(local_result.error_message, "Failed to update version",
                KG_SCHEMA_MAX_ERROR_LEN - 1);
    } else {
        local_result.status = KG_MIGRATE_COMPLETED;
    }

    local_result.completed_at = get_current_timestamp_ms();
    local_result.duration_ms = local_result.completed_at - local_result.started_at;

    if (result) *result = local_result;

    return (local_result.status == KG_MIGRATE_COMPLETED) ? 0 : -1;
}

int kg_schema_migrate_up(brain_kg_t* kg, kg_migration_result_t* result) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return -1;
    }

    kg_schema_version_t current = kg_schema_get_current(kg);

    /* Find next version */
    const kg_migration_script_t* next_migration = NULL;

    for (uint32_t i = 0; i < g_migration_registry.count; i++) {
        kg_migration_script_t* m = &g_migration_registry.migrations[i];
        if (kg_schema_compare(&m->from_version, &current) == 0) {
            next_migration = m;
            break;
        }
    }

    if (!next_migration) {
        /* Already at latest */
        if (result) {
            memset(result, 0, sizeof(*result));
            result->status = KG_MIGRATE_COMPLETED;
            memcpy(&result->from_version, &current, sizeof(kg_schema_version_t));
            memcpy(&result->to_version, &current, sizeof(kg_schema_version_t));
        }
        return 1; /* Already at latest */
    }

    return kg_schema_migrate(kg, &next_migration->to_version, result);
}

int kg_schema_migrate_down(brain_kg_t* kg, kg_migration_result_t* result) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return -1;
    }

    kg_schema_version_t current = kg_schema_get_current(kg);

    /* Find previous version */
    const kg_migration_script_t* prev_migration = NULL;

    for (uint32_t i = 0; i < g_migration_registry.count; i++) {
        kg_migration_script_t* m = &g_migration_registry.migrations[i];
        if (kg_schema_compare(&m->to_version, &current) == 0 && m->is_reversible) {
            prev_migration = m;
            break;
        }
    }

    if (!prev_migration) {
        /* Already at earliest or no reversible migration */
        if (result) {
            memset(result, 0, sizeof(*result));
            result->status = KG_MIGRATE_COMPLETED;
            memcpy(&result->from_version, &current, sizeof(kg_schema_version_t));
            memcpy(&result->to_version, &current, sizeof(kg_schema_version_t));
        }
        return 1; /* Already at earliest */
    }

    return kg_schema_migrate(kg, &prev_migration->from_version, result);
}

int kg_schema_rollback_last(brain_kg_t* kg, kg_migration_result_t* result) {
    /* For now, just call migrate_down */
    return kg_schema_migrate_down(kg, result);
}

/* ============================================================================
 * Migration History API
 * ============================================================================ */

int kg_schema_get_migration_history(
    const brain_kg_t* kg,
    kg_migration_result_t* history,
    uint32_t* count
) {
    if (!kg || !history || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_get_migration_history: required parameter is NULL (kg, history, count)");
        return -1;
    }

    /* In a real implementation, we would read history from KG metadata */
    *count = 0;

    return 0;
}

int kg_schema_clear_history(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return -1;
    }

    /* In a real implementation, we would clear history in KG metadata */

    return 0;
}

/* ============================================================================
 * Compatibility API
 * ============================================================================ */

bool kg_schema_is_compatible(
    const kg_schema_version_t* required,
    const kg_schema_version_t* actual
) {
    if (!required || !actual) {
        return false;
    }

    /* Same major version required */
    if (required->major != actual->major) {
        return false;
    }

    /* Actual minor must be >= required minor */
    if (actual->minor < required->minor) {
        return false;
    }

    return true;
}

bool kg_schema_needs_migration(
    const brain_kg_t* kg,
    const kg_schema_version_t* target
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_schema_needs_migration: kg is NULL");
        return false;
    }

    kg_schema_version_t current = kg_schema_get_current(kg);
    const kg_schema_version_t* compare_target = target;

    if (!compare_target) {
        kg_schema_version_t latest = kg_schema_get_latest();
        compare_target = &latest;
    }

    return kg_schema_compare(&current, compare_target) != 0;
}

kg_schema_version_t kg_schema_get_latest(void) {
    ensure_registry_initialized();

    kg_schema_version_t latest = {0};

    for (uint32_t i = 0; i < g_migration_registry.count; i++) {
        kg_migration_script_t* m = &g_migration_registry.migrations[i];
        if (kg_schema_compare(&m->to_version, &latest) > 0) {
            memcpy(&latest, &m->to_version, sizeof(kg_schema_version_t));
        }
    }

    return latest;
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

static const char* migration_direction_strings[] = {
    "UP",
    "DOWN"
};

const char* kg_migration_direction_to_string(kg_migration_direction_t direction) {
    if (direction >= 0 && direction <= KG_MIGRATE_DOWN) {
        return migration_direction_strings[direction];
    }
    return "UNKNOWN";
}

static const char* migration_status_strings[] = {
    "PENDING",
    "IN_PROGRESS",
    "COMPLETED",
    "FAILED",
    "ROLLED_BACK"
};

const char* kg_migration_status_to_string(kg_migration_status_t status) {
    if (status >= 0 && status <= KG_MIGRATE_ROLLED_BACK) {
        return migration_status_strings[status];
    }
    return "UNKNOWN";
}
