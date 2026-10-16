/**
 * @file nimcp_security_knowledge_graph_bridge.c
 * @brief Security-Knowledge Graph Bridge - Protection for Graph Operations
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridge for protecting knowledge graph operations from security threats
 *       including query injection, unauthorized traversal, and data corruption.
 * WHY:  Knowledge graphs contain sensitive information and structural relationships
 *       that could be exploited through malicious queries or traversal patterns.
 * HOW:  Integrate security validation with KG reader, validate queries, enforce
 *       access control, verify node integrity, and isolate private data.
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#include "security/knowledge/nimcp_security_knowledge_graph_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <ctype.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_knowledge_graph_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Static String Tables
 * ============================================================================ */

static const char* s_query_result_names[] = {
    "VALID",
    "INJECTION_DETECTED",
    "TOO_LONG",
    "MALFORMED",
    "FORBIDDEN_PATTERN",
    "RATE_LIMITED",
    "UNAUTHORIZED",
    "REJECTED"
};

static const char* s_traversal_result_names[] = {
    "ALLOWED",
    "DEPTH_EXCEEDED",
    "SCOPE_DENIED",
    "NODE_PRIVATE",
    "EDGE_FORBIDDEN",
    "CYCLE_DETECTED",
    "RATE_LIMITED",
    "DENIED"
};

static const char* s_integrity_result_names[] = {
    "VALID",
    "HASH_MISMATCH",
    "SIGNATURE_INVALID",
    "TIMESTAMP_ANOMALY",
    "RELATION_MISMATCH",
    "CORRUPTED",
    "NOT_FOUND"
};

static const char* s_consistency_result_names[] = {
    "CONSISTENT",
    "ORPHANED_NODE",
    "DANGLING_RELATION",
    "DUPLICATE_ENTRY",
    "CYCLE_VIOLATION",
    "TYPE_VIOLATION",
    "CONSTRAINT_VIOLATION"
};

static const char* s_privacy_level_names[] = {
    "PUBLIC",
    "INTERNAL",
    "RESTRICTED",
    "CONFIDENTIAL",
    "SECRET"
};

static const char* s_state_names[] = {
    "UNINITIALIZED",
    "READY",
    "PROCESSING",
    "LOCKDOWN",
    "DEGRADED",
    "ERROR"
};

/* Injection pattern markers (simplified) */
static const char* s_injection_patterns[] = {
    "--",        /* SQL comment */
    ";--",       /* SQL statement terminator + comment */
    "/*",        /* Block comment start */
    "*/",        /* Block comment end */
    "\\x00",     /* Null byte */
    "\\n",       /* Newline injection */
    "||",        /* String concatenation */
    "&&",        /* Logical AND injection */
    "$(", ")`",  /* Command substitution */
    NULL
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Check if string contains injection pattern
 */
static bool contains_injection_pattern(const char* str, size_t len) {
    if (!str || len == 0) {
        return false;
    }

    for (int i = 0; s_injection_patterns[i] != NULL; i++) {
        if (strstr(str, s_injection_patterns[i]) != NULL) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if string contains only safe characters for entity names
 */
static bool is_safe_entity_name(const char* name) {
    if (!name || name[0] == '\0') {
        return false;
    }

    for (size_t i = 0; name[i] != '\0'; i++) {
        char c = name[i];
        /* Allow alphanumeric, underscore, hyphen, period */
        if (!isalnum((unsigned char)c) && c != '_' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Find private node in registry
 */
static sec_kg_private_node_t* find_private_node(
    security_kg_bridge_t* bridge,
    const char* entity_name
) {
    for (uint32_t i = 0; i < bridge->private_node_count; i++) {
        if (strcmp(bridge->private_nodes[i].node_name, entity_name) == 0) {
            return &bridge->private_nodes[i];
        }
    }
    /* Not finding a private node is normal - entity is simply not private */
    return NULL;
}

/**
 * @brief Update bridge active state based on connections
 */
static void update_bridge_active_state(security_kg_bridge_t* bridge) {
    bridge->base.bridge_active = bridge->state.kg_reader_connected;

    if (bridge->state.operational_state == SEC_KG_STATE_UNINITIALIZED) {
        bridge->state.operational_state = SEC_KG_STATE_READY;
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int security_kg_default_config(sec_kg_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(sec_kg_config_t));

    /* Query validation settings */
    config->enable_query_validation = true;
    config->injection_threshold = SEC_KG_DEFAULT_INJECTION_THRESHOLD;
    config->max_query_length = SEC_KG_MAX_QUERY_LENGTH;
    config->enable_pattern_matching = true;
    config->enable_rate_limiting = true;
    config->queries_per_second_limit = 100;

    /* Traversal settings */
    config->enable_traversal_control = true;
    config->max_traversal_depth = SEC_KG_MAX_TRAVERSAL_DEPTH;
    config->max_nodes_per_query = SEC_KG_MAX_RESULT_NODES;
    config->enable_scope_restriction = true;
    config->enable_cycle_detection = true;

    /* Integrity settings */
    config->enable_integrity_verification = true;
    config->integrity_threshold = SEC_KG_DEFAULT_INTEGRITY_THRESHOLD;
    config->enable_hash_verification = true;
    config->enable_signature_verification = false;

    /* Consistency settings */
    config->enable_consistency_checks = true;
    config->enforce_referential_integrity = true;
    config->detect_orphaned_nodes = true;
    config->prevent_cycles = false;

    /* Privacy settings */
    config->enable_privacy_isolation = true;
    config->default_privacy = SEC_KG_PRIVACY_INTERNAL;
    config->enable_privacy_inference_protection = false;
    config->log_access_attempts = true;

    /* Integration settings */
    config->enable_bbb = true;
    config->enable_anomaly_detector = true;
    config->enable_bio_async = false;
    config->enable_logging = true;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

security_kg_bridge_t* security_kg_bridge_create(
    const sec_kg_config_t* config
) {
    /* Allocate bridge structure */
    security_kg_bridge_t* bridge = nimcp_malloc(sizeof(security_kg_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_kg_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate security_kg_bridge");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(security_kg_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SEC_KG,
                         SEC_KG_MODULE_NAME) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_kg_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_kg_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->state.operational_state = SEC_KG_STATE_READY;
    bridge->state.last_query_result = SEC_KG_QUERY_VALID;
    bridge->state.last_traversal_result = SEC_KG_TRAVERSAL_ALLOWED;
    bridge->state.last_integrity_result = SEC_KG_NODE_VALID;

    /* Initialize effects */
    bridge->sec_to_kg.current_traversal_limit = bridge->config.max_traversal_depth;
    bridge->sec_to_kg.min_privacy = SEC_KG_PRIVACY_PUBLIC;

    /* Allocate private node registry */
    bridge->private_node_capacity = 64;
    bridge->private_nodes = nimcp_malloc(
        bridge->private_node_capacity * sizeof(sec_kg_private_node_t));
    if (!bridge->private_nodes) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_kg_bridge_create: bridge->private_nodes is NULL");
        return NULL;
    }
    memset(bridge->private_nodes, 0,
           bridge->private_node_capacity * sizeof(sec_kg_private_node_t));

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        bridge_base_connect_bio_async(&bridge->base);
    }

    return bridge;
}

void security_kg_bridge_destroy(
    security_kg_bridge_t* bridge
) {
    if (!bridge) return;

    /* Free private node registry */
    if (bridge->private_nodes) {
        nimcp_free(bridge->private_nodes);
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int security_kg_connect_reader(
    security_kg_bridge_t* bridge,
    kg_reader_t* kg_reader
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(kg_reader, NIMCP_ERROR_NULL_POINTER, "kg_reader is NULL");

    BRIDGE_LOCK(bridge);

    bridge->kg_reader = kg_reader;
    bridge->state.kg_reader_connected = true;
    update_bridge_active_state(bridge);

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-KG Bridge: KG reader connected");
    }

    return 0;
}

int security_kg_connect_bbb(
    security_kg_bridge_t* bridge,
    bbb_system_t bbb
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb, NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

    BRIDGE_LOCK(bridge);

    bridge->bbb = bbb;
    bridge->state.bbb_connected = true;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-KG Bridge: BBB connected");
    }

    return 0;
}

int security_kg_connect_anomaly_detector(
    security_kg_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(detector, NIMCP_ERROR_NULL_POINTER, "detector is NULL");

    BRIDGE_LOCK(bridge);

    bridge->anomaly_detector = detector;
    bridge->state.anomaly_detector_connected = true;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-KG Bridge: Anomaly detector connected");
    }

    return 0;
}

/* ============================================================================
 * Query Validation API
 * ============================================================================ */

int security_kg_validate_query(
    security_kg_bridge_t* bridge,
    const char* query,
    size_t query_len,
    sec_kg_query_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && query && result, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_validate_query");

    BRIDGE_LOCK(bridge);

    uint64_t start_time = nimcp_time_monotonic_us();
    *result = SEC_KG_QUERY_VALID;

    /* Check lockdown before changing state */
    if (bridge->state.lockdown_active) {
        *result = SEC_KG_QUERY_REJECTED;
        bridge->stats.queries_rejected++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    bridge->state.operational_state = SEC_KG_STATE_PROCESSING;

    /* Length check */
    if (query_len > bridge->config.max_query_length) {
        *result = SEC_KG_QUERY_TOO_LONG;
        bridge->stats.queries_rejected++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Injection pattern check */
    if (bridge->config.enable_pattern_matching) {
        if (contains_injection_pattern(query, query_len)) {
            *result = SEC_KG_QUERY_INJECTION_DETECTED;
            bridge->stats.injections_detected++;
            bridge->stats.queries_rejected++;
            bridge->stats.queries_validated_total++;
            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    /* Update statistics */
    bridge->stats.queries_validated_total++;
    bridge->stats.queries_passed++;
    bridge->kg_to_sec.queries_processed++;

    /* Update timing */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    float elapsed_f = (float)elapsed;
    bridge->stats.avg_query_validation_time_us =
        (bridge->stats.avg_query_validation_time_us *
         (bridge->stats.queries_validated_total - 1) + elapsed_f) /
        bridge->stats.queries_validated_total;

    if (elapsed_f > bridge->stats.max_validation_time_us) {
        bridge->stats.max_validation_time_us = elapsed_f;
    }

    bridge->state.last_query_result = *result;
    bridge->state.last_validation_time_us = nimcp_time_get_us();
    bridge->state.operational_state = SEC_KG_STATE_READY;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_validate_entity_name(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_query_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && entity_name && result, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_validate_entity_name");

    BRIDGE_LOCK(bridge);

    *result = SEC_KG_QUERY_VALID;

    /* Check lockdown */
    if (bridge->state.lockdown_active) {
        *result = SEC_KG_QUERY_REJECTED;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Length check */
    size_t len = strlen(entity_name);
    if (len == 0 || len > KG_MAX_NAME_LENGTH) {
        *result = SEC_KG_QUERY_MALFORMED;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Character validation */
    if (!is_safe_entity_name(entity_name)) {
        *result = SEC_KG_QUERY_INJECTION_DETECTED;
        bridge->stats.injections_detected++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    bridge->stats.queries_validated_total++;
    bridge->stats.queries_passed++;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_validate_search(
    security_kg_bridge_t* bridge,
    const char* search_text,
    sec_kg_query_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && search_text && result, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_validate_search");

    size_t len = strlen(search_text);
    return security_kg_validate_query(bridge, search_text, len, result);
}

/* ============================================================================
 * Traversal Access Control API
 * ============================================================================ */

int security_kg_check_traversal_access(
    security_kg_bridge_t* bridge,
    const char* source_entity,
    const char* target_entity,
    const char* relation_type,
    uint32_t current_depth,
    sec_kg_traversal_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && result, NIMCP_ERROR_NULL_POINTER, "bridge or result is NULL");
    NIMCP_CHECK_THROW(source_entity, NIMCP_ERROR_NULL_POINTER, "source_entity is NULL");

    BRIDGE_LOCK(bridge);

    *result = SEC_KG_TRAVERSAL_ALLOWED;

    /* Check lockdown */
    if (bridge->state.lockdown_active || bridge->sec_to_kg.traversals_blocked) {
        *result = SEC_KG_TRAVERSAL_DENIED;
        bridge->stats.traversals_validated_total++;
        bridge->stats.traversals_denied++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Depth check */
    if (current_depth > bridge->sec_to_kg.current_traversal_limit) {
        *result = SEC_KG_TRAVERSAL_DEPTH_EXCEEDED;
        bridge->stats.traversals_validated_total++;
        bridge->stats.depth_limit_violations++;
        bridge->stats.traversals_denied++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Privacy check for target if specified */
    if (target_entity && bridge->config.enable_privacy_isolation) {
        sec_kg_private_node_t* priv_node = find_private_node(bridge, target_entity);
        if (priv_node && priv_node->privacy_level > bridge->sec_to_kg.min_privacy) {
            *result = SEC_KG_TRAVERSAL_NODE_PRIVATE;
            bridge->stats.traversals_validated_total++;
            bridge->stats.traversals_denied++;
            bridge->kg_to_sec.private_access_denied++;
            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    bridge->stats.traversals_validated_total++;
    bridge->stats.traversals_allowed++;
    bridge->kg_to_sec.traversals_processed++;

    if (current_depth > bridge->kg_to_sec.max_depth_reached) {
        bridge->kg_to_sec.max_depth_reached = current_depth;
    }

    bridge->state.last_traversal_result = *result;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_is_entity_accessible(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    bool* accessible
) {
    NIMCP_CHECK_THROW(bridge && entity_name && accessible, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_is_entity_accessible");

    BRIDGE_LOCK(bridge);

    *accessible = true;

    /* Check lockdown */
    if (bridge->state.lockdown_active) {
        *accessible = false;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Privacy check */
    if (bridge->config.enable_privacy_isolation) {
        sec_kg_private_node_t* priv_node = find_private_node(bridge, entity_name);
        if (priv_node && priv_node->privacy_level > bridge->sec_to_kg.min_privacy) {
            *accessible = false;
            bridge->kg_to_sec.private_access_attempts++;
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_set_max_traversal_depth(
    security_kg_bridge_t* bridge,
    uint32_t max_depth
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->sec_to_kg.current_traversal_limit = max_depth;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-KG Bridge: Max traversal depth set to %u",
                          max_depth);
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Node Integrity API
 * ============================================================================ */

int security_kg_verify_node_integrity(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_integrity_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && entity_name && result, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_verify_node_integrity");

    BRIDGE_LOCK(bridge);

    *result = SEC_KG_NODE_VALID;
    bridge->stats.integrity_checks_total++;

    /* Check if KG reader is connected */
    if (!bridge->state.kg_reader_connected || !bridge->kg_reader) {
        *result = SEC_KG_NODE_NOT_FOUND;
        bridge->stats.integrity_failed++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Verify entity exists */
    const kg_entity_t* entity = kg_reader_get_entity(bridge->kg_reader, entity_name);
    if (!entity) {
        *result = SEC_KG_NODE_NOT_FOUND;
        bridge->stats.integrity_failed++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Basic integrity check - verify name matches */
    if (strcmp(entity->name, entity_name) != 0) {
        *result = SEC_KG_NODE_CORRUPTED;
        bridge->stats.integrity_failed++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    bridge->stats.integrity_verified++;
    bridge->state.last_integrity_result = *result;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_verify_relation_integrity(
    security_kg_bridge_t* bridge,
    const char* from_entity,
    const char* to_entity,
    const char* relation_type,
    sec_kg_integrity_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && from_entity && to_entity && result, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_verify_relation_integrity");

    BRIDGE_LOCK(bridge);

    *result = SEC_KG_NODE_VALID;
    bridge->stats.integrity_checks_total++;

    /* Check if KG reader is connected */
    if (!bridge->state.kg_reader_connected || !bridge->kg_reader) {
        *result = SEC_KG_NODE_NOT_FOUND;
        bridge->stats.integrity_failed++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Verify both endpoints exist */
    const kg_entity_t* from = kg_reader_get_entity(bridge->kg_reader, from_entity);
    const kg_entity_t* to = kg_reader_get_entity(bridge->kg_reader, to_entity);

    if (!from || !to) {
        *result = SEC_KG_NODE_RELATION_MISMATCH;
        bridge->stats.integrity_failed++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* If relation type specified, verify connection exists */
    if (relation_type) {
        const char* conn = kg_reader_are_connected(bridge->kg_reader,
                                                    from_entity, to_entity);
        if (!conn) {
            *result = SEC_KG_NODE_RELATION_MISMATCH;
            bridge->stats.integrity_failed++;
            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    bridge->stats.integrity_verified++;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Consistency Enforcement API
 * ============================================================================ */

int security_kg_enforce_consistency(
    security_kg_bridge_t* bridge,
    sec_kg_consistency_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && result, NIMCP_ERROR_NULL_POINTER, "bridge or result is NULL");

    BRIDGE_LOCK(bridge);

    *result = SEC_KG_CONSISTENT;
    bridge->stats.consistency_checks_total++;

    /* Basic consistency - just verify we have a connected reader */
    if (!bridge->state.kg_reader_connected) {
        *result = SEC_KG_CONSTRAINT_VIOLATION;
        bridge->stats.consistency_violations++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    bridge->stats.consistency_passed++;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_check_orphaned_nodes(
    security_kg_bridge_t* bridge,
    uint32_t* orphan_count
) {
    NIMCP_CHECK_THROW(bridge && orphan_count, NIMCP_ERROR_NULL_POINTER, "bridge or orphan_count is NULL");

    BRIDGE_LOCK(bridge);

    *orphan_count = 0;

    if (!bridge->state.kg_reader_connected || !bridge->kg_reader) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Get all entities and check each for relations */
    kg_entity_list_t* entities = kg_reader_get_all_entities(bridge->kg_reader);
    if (!entities) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    for (uint32_t i = 0; i < entities->count; i++) {
        const kg_entity_t* entity = entities->entities[i];
        if (!entity) continue;

        /* Check for outgoing relations */
        kg_relation_list_t* out_rels = kg_reader_get_relations_from(
            bridge->kg_reader, entity->name);
        kg_relation_list_t* in_rels = kg_reader_get_relations_to(
            bridge->kg_reader, entity->name);

        bool has_relations = (out_rels && out_rels->count > 0) ||
                             (in_rels && in_rels->count > 0);

        if (out_rels) kg_relation_list_destroy(out_rels);
        if (in_rels) kg_relation_list_destroy(in_rels);

        if (!has_relations) {
            (*orphan_count)++;
        }
    }

    kg_entity_list_destroy(entities);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_check_dangling_relations(
    security_kg_bridge_t* bridge,
    uint32_t* dangling_count
) {
    NIMCP_CHECK_THROW(bridge && dangling_count, NIMCP_ERROR_NULL_POINTER, "bridge or dangling_count is NULL");

    *dangling_count = 0;
    /* Simplified implementation - would need full relation scan */

    return 0;
}

/* ============================================================================
 * Privacy Isolation API
 * ============================================================================ */

int security_kg_isolate_private_data(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_privacy_level_t privacy_level
) {
    NIMCP_CHECK_THROW(bridge && entity_name, NIMCP_ERROR_NULL_POINTER, "bridge or entity_name is NULL");

    BRIDGE_LOCK(bridge);

    /* Check if already in registry */
    sec_kg_private_node_t* existing = find_private_node(bridge, entity_name);
    if (existing) {
        existing->privacy_level = privacy_level;
        existing->isolation_timestamp = nimcp_time_get_us();
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Expand registry if needed */
    if (bridge->private_node_count >= bridge->private_node_capacity) {
        uint32_t new_capacity = bridge->private_node_capacity * 2;
        sec_kg_private_node_t* new_nodes = nimcp_malloc(
            new_capacity * sizeof(sec_kg_private_node_t));
        if (!new_nodes) {
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(new_nodes, bridge->private_nodes,
               bridge->private_node_count * sizeof(sec_kg_private_node_t));
        nimcp_free(bridge->private_nodes);
        bridge->private_nodes = new_nodes;
        bridge->private_node_capacity = new_capacity;
    }

    /* Add new entry */
    sec_kg_private_node_t* node = &bridge->private_nodes[bridge->private_node_count];
    strncpy(node->node_name, entity_name, KG_MAX_NAME_LENGTH - 1);
    node->node_name[KG_MAX_NAME_LENGTH - 1] = '\0';
    node->privacy_level = privacy_level;
    node->isolation_timestamp = nimcp_time_get_us();
    node->access_role_mask = 0;
    bridge->private_node_count++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-KG: Isolated '%s' at level %s",
                          entity_name, security_kg_privacy_level_name(privacy_level));
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_remove_isolation(
    security_kg_bridge_t* bridge,
    const char* entity_name
) {
    NIMCP_CHECK_THROW(bridge && entity_name, NIMCP_ERROR_NULL_POINTER, "bridge or entity_name is NULL");

    BRIDGE_LOCK(bridge);

    /* Find and remove from registry */
    for (uint32_t i = 0; i < bridge->private_node_count; i++) {
        if (strcmp(bridge->private_nodes[i].node_name, entity_name) == 0) {
            /* Shift remaining entries */
            if (i < bridge->private_node_count - 1) {
                memmove(&bridge->private_nodes[i],
                        &bridge->private_nodes[i + 1],
                        (bridge->private_node_count - i - 1) *
                        sizeof(sec_kg_private_node_t));
            }
            bridge->private_node_count--;

            if (bridge->config.enable_logging) {
                NIMCP_LOGGING_INFO("Security-KG: Removed isolation from '%s'",
                                  entity_name);
            }
            break;
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_get_privacy_level(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_privacy_level_t* privacy_level
) {
    NIMCP_CHECK_THROW(bridge && entity_name && privacy_level, NIMCP_ERROR_NULL_POINTER, "NULL parameter in security_kg_get_privacy_level");

    BRIDGE_LOCK(bridge);

    sec_kg_private_node_t* node = find_private_node(bridge, entity_name);
    if (node) {
        *privacy_level = node->privacy_level;
    } else {
        *privacy_level = SEC_KG_PRIVACY_PUBLIC;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_check_privacy_access(
    security_kg_bridge_t* bridge,
    sec_kg_privacy_level_t required_level,
    bool* allowed
) {
    NIMCP_CHECK_THROW(bridge && allowed, NIMCP_ERROR_NULL_POINTER, "bridge or allowed is NULL");

    BRIDGE_LOCK(bridge);

    /* Current implementation: allow if required level <= minimum allowed */
    *allowed = (required_level <= bridge->sec_to_kg.min_privacy ||
                required_level == SEC_KG_PRIVACY_PUBLIC);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Lockdown API
 * ============================================================================ */

int security_kg_enter_lockdown(
    security_kg_bridge_t* bridge,
    const char* reason
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.lockdown_active = true;
    bridge->state.lockdown_start_time = nimcp_time_get_us();
    bridge->state.lockdown_reason = reason;
    bridge->state.operational_state = SEC_KG_STATE_LOCKDOWN;

    /* Block all operations */
    bridge->sec_to_kg.queries_blocked = true;
    bridge->sec_to_kg.traversals_blocked = true;
    bridge->sec_to_kg.writes_blocked = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_WARN("Security-KG: Entering lockdown - %s",
                           reason ? reason : "unknown reason");
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_exit_lockdown(
    security_kg_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.lockdown_active = false;
    bridge->state.lockdown_reason = NULL;
    bridge->state.operational_state = SEC_KG_STATE_READY;

    /* Unblock operations */
    bridge->sec_to_kg.queries_blocked = false;
    bridge->sec_to_kg.traversals_blocked = false;
    bridge->sec_to_kg.writes_blocked = false;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-KG: Exiting lockdown");
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_is_lockdown_active(
    security_kg_bridge_t* bridge,
    bool* active
) {
    NIMCP_CHECK_THROW(bridge && active, NIMCP_ERROR_NULL_POINTER, "bridge or active is NULL");

    BRIDGE_LOCK(bridge);
    *active = bridge->state.lockdown_active;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int security_kg_update_sec_to_kg(
    security_kg_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Compute threat levels */
    float query_threat = bridge->kg_to_sec.query_anomaly_score;
    float traversal_threat = bridge->kg_to_sec.traversal_anomaly_score;

    bridge->sec_to_kg.query_threat_level = query_threat;
    bridge->sec_to_kg.traversal_threat_level = traversal_threat;
    bridge->sec_to_kg.combined_threat_level =
        (query_threat + traversal_threat) / 2.0f;

    /* Adjust traversal limit based on threat */
    if (bridge->sec_to_kg.combined_threat_level > 0.7f) {
        bridge->sec_to_kg.current_traversal_limit =
            bridge->config.max_traversal_depth / 2;
        bridge->sec_to_kg.require_enhanced_validation = true;
    } else if (bridge->sec_to_kg.combined_threat_level > 0.5f) {
        bridge->sec_to_kg.current_traversal_limit =
            (bridge->config.max_traversal_depth * 3) / 4;
        bridge->sec_to_kg.require_enhanced_validation = true;
    } else {
        bridge->sec_to_kg.current_traversal_limit =
            bridge->config.max_traversal_depth;
        bridge->sec_to_kg.require_enhanced_validation = false;
    }

    /* Update threat flags */
    bridge->state.query_threat_active = (query_threat > 0.3f);
    bridge->state.traversal_threat_active = (traversal_threat > 0.3f);
    bridge->state.current_query_threat = query_threat;
    bridge->state.current_traversal_threat = traversal_threat;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_update_kg_to_sec(
    security_kg_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Update timestamp */
    bridge->kg_to_sec.anomaly_timestamp_us = nimcp_time_get_us();

    /* Check if anomaly flag should be raised */
    bool anomaly = bridge->kg_to_sec.query_anomaly_score >
                   bridge->config.injection_threshold ||
                   bridge->kg_to_sec.traversal_anomaly_score >
                   bridge->config.integrity_threshold;

    bridge->kg_to_sec.anomaly_flag_raised = anomaly;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_kg_update(
    security_kg_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    int ret = security_kg_update_kg_to_sec(bridge);
    if (ret != 0) return ret;

    return security_kg_update_sec_to_kg(bridge);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int security_kg_get_sec_to_kg_effects(
    const security_kg_bridge_t* bridge,
    sec_to_kg_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    security_kg_bridge_t* mutable_bridge = (security_kg_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->sec_to_kg;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_kg_get_kg_to_sec_effects(
    const security_kg_bridge_t* bridge,
    kg_to_sec_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    security_kg_bridge_t* mutable_bridge = (security_kg_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->kg_to_sec;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_kg_get_state(
    const security_kg_bridge_t* bridge,
    sec_kg_bridge_state_t* state
) {
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    security_kg_bridge_t* mutable_bridge = (security_kg_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_kg_get_stats(
    const security_kg_bridge_t* bridge,
    sec_kg_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    security_kg_bridge_t* mutable_bridge = (security_kg_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

void security_kg_reset_stats(
    security_kg_bridge_t* bridge
) {
    if (!bridge) return;

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(sec_kg_stats_t));
    BRIDGE_UNLOCK(bridge);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_kg_query_result_name(
    sec_kg_query_result_t result
) {
    if (result < 0 || result > SEC_KG_QUERY_REJECTED) {
        return "UNKNOWN";
    }
    return s_query_result_names[result];
}

const char* security_kg_traversal_result_name(
    sec_kg_traversal_result_t result
) {
    if (result < 0 || result > SEC_KG_TRAVERSAL_DENIED) {
        return "UNKNOWN";
    }
    return s_traversal_result_names[result];
}

const char* security_kg_integrity_result_name(
    sec_kg_integrity_result_t result
) {
    if (result < 0 || result > SEC_KG_NODE_NOT_FOUND) {
        return "UNKNOWN";
    }
    return s_integrity_result_names[result];
}

const char* security_kg_consistency_result_name(
    sec_kg_consistency_result_t result
) {
    if (result < 0 || result > SEC_KG_CONSTRAINT_VIOLATION) {
        return "UNKNOWN";
    }
    return s_consistency_result_names[result];
}

const char* security_kg_privacy_level_name(
    sec_kg_privacy_level_t level
) {
    if (level < 0 || level > SEC_KG_PRIVACY_SECRET) {
        return "UNKNOWN";
    }
    return s_privacy_level_names[level];
}

const char* security_kg_state_name(
    sec_kg_state_t state
) {
    if (state < 0 || state > SEC_KG_STATE_ERROR) {
        return "UNKNOWN";
    }
    return s_state_names[state];
}

int security_kg_report_false_positive(
    security_kg_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->stats.false_positives_reported++;

    /* Update estimated precision */
    uint64_t total_detections = bridge->stats.injections_detected +
                                bridge->stats.traversals_denied +
                                bridge->stats.integrity_failed;

    if (total_detections > 0) {
        uint64_t true_positives = 0;
        if (total_detections > bridge->stats.false_positives_reported) {
            true_positives = total_detections -
                             bridge->stats.false_positives_reported;
        }
        bridge->stats.estimated_precision =
            (float)true_positives / (float)total_detections;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}
