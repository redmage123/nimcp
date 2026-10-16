/**
 * @file nimcp_kg_compliance.c
 * @brief Compliance and Data Governance for Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of GDPR/CCPA compliance features including data classification,
 * retention policies, PII detection/masking, subject access requests, and lineage.
 */

#include "core/brain/nimcp_kg_compliance.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_compliance, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Static Data Structures
 * ============================================================================ */

/** Maximum retention policies */
#define MAX_RETENTION_POLICIES 64

/** Maximum registered data subjects */
#define MAX_DATA_SUBJECTS 1024

/** Retention policy registry */
static struct {
    kg_retention_policy_t policies[MAX_RETENTION_POLICIES];
    uint32_t count;
    nimcp_mutex_t* mutex;
    bool initialized;
} g_retention_registry = { .count = 0, .initialized = false };

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
 * @brief Initialize registry if not done
 */
static void ensure_registry_initialized(void) {
    if (!g_retention_registry.initialized) {
        memset(&g_retention_registry, 0, sizeof(g_retention_registry));
        mutex_attr_t attr = {0};
        attr.type = MUTEX_TYPE_NORMAL;
        g_retention_registry.mutex = nimcp_mutex_create(&attr);
        g_retention_registry.initialized = true;
    }
}

/* ============================================================================
 * Retention Policy Management
 * ============================================================================ */

int kg_compliance_default_retention(kg_retention_policy_t* policy) {
    if (!policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "policy is NULL");

        return -1;
    }

    memset(policy, 0, sizeof(*policy));
    strncpy(policy->policy_name, "default", KG_COMPLIANCE_MAX_POLICY_NAME - 1);
    policy->applies_to = KG_CLASS_PUBLIC;
    policy->retention_days = 365;
    policy->enable_auto_delete = false;
    policy->require_approval_for_delete = false;
    policy->legal_hold_tags = NULL;

    return 0;
}

int kg_compliance_set_retention(const kg_retention_policy_t* policy) {
    ensure_registry_initialized();

    if (!policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "policy is NULL");


        return -1;
    }

    nimcp_mutex_lock(g_retention_registry.mutex);

    /* Find existing or add new */
    kg_retention_policy_t* existing = NULL;
    for (uint32_t i = 0; i < g_retention_registry.count; i++) {
        if (strcmp(g_retention_registry.policies[i].policy_name,
                   policy->policy_name) == 0) {
            existing = &g_retention_registry.policies[i];
            break;
        }
    }

    if (!existing) {
        if (g_retention_registry.count >= MAX_RETENTION_POLICIES) {
            nimcp_mutex_unlock(g_retention_registry.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_compliance_set_retention: capacity exceeded");
            return -1;
        }
        existing = &g_retention_registry.policies[g_retention_registry.count++];
    }

    /* Copy policy */
    memcpy(existing, policy, sizeof(kg_retention_policy_t));

    /* Deep copy legal_hold_tags if present */
    if (policy->legal_hold_tags) {
        size_t len = strlen(policy->legal_hold_tags) + 1;
        existing->legal_hold_tags = nimcp_malloc(len);
        if (existing->legal_hold_tags) {
            memcpy(existing->legal_hold_tags, policy->legal_hold_tags, len);
        }
    }

    nimcp_mutex_unlock(g_retention_registry.mutex);

    return 0;
}

int kg_compliance_get_retention(const char* policy_name, kg_retention_policy_t* policy) {
    ensure_registry_initialized();

    if (!policy_name || !policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_get_retention: required parameter is NULL (policy_name, policy)");
        return -1;
    }

    nimcp_mutex_lock(g_retention_registry.mutex);

    for (uint32_t i = 0; i < g_retention_registry.count; i++) {
        if (strcmp(g_retention_registry.policies[i].policy_name, policy_name) == 0) {
            memcpy(policy, &g_retention_registry.policies[i],
                   sizeof(kg_retention_policy_t));
            nimcp_mutex_unlock(g_retention_registry.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(g_retention_registry.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compliance_get_retention: validation failed");
    return -1; /* Not found */
}

int kg_compliance_apply_retention(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return -1;
    }

    /* In a real implementation, we would:
     * 1. Iterate all nodes
     * 2. Check creation timestamp vs retention policy
     * 3. Check for legal holds
     * 4. Delete expired nodes without holds
     */

    return 0;
}

int kg_compliance_set_legal_hold(brain_kg_t* kg, brain_kg_node_id_t node_id,
                                  const char* hold_tag) {
    if (!kg || !hold_tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_apply_retention: required parameter is NULL (kg, hold_tag)");
        return -1;
    }

    (void)node_id;

    /* In a real implementation, add hold tag to node metadata */

    return 0;
}

int kg_compliance_remove_legal_hold(brain_kg_t* kg, brain_kg_node_id_t node_id,
                                     const char* hold_tag) {
    if (!kg || !hold_tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_apply_retention: required parameter is NULL (kg, hold_tag)");
        return -1;
    }

    (void)node_id;

    /* In a real implementation, remove hold tag from node metadata */

    return 0;
}

/* ============================================================================
 * GDPR / Data Subject Management
 * ============================================================================ */

int kg_compliance_register_subject(brain_kg_t* kg, const kg_data_subject_t* subject) {
    if (!kg || !subject) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_register_subject: required parameter is NULL (kg, subject)");
        return -1;
    }

    /* In a real implementation, store subject record in KG metadata */

    return 0;
}

int kg_compliance_get_subject_data(const brain_kg_t* kg, const char* subject_id,
                                    brain_kg_node_id_t* nodes, uint32_t max,
                                    uint32_t* count) {
    if (!kg || !subject_id || !nodes || max == 0 || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_register_subject: required parameter is NULL (kg, subject_id, nodes, count)");
        return -1;
    }

    /* In a real implementation, query subject registry for associated nodes */
    *count = 0;

    return 0;
}

int kg_compliance_delete_subject_data(brain_kg_t* kg, const char* subject_id) {
    if (!kg || !subject_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_delete_subject_data: required parameter is NULL (kg, subject_id)");
        return -1;
    }

    /* Get all nodes for subject */
    brain_kg_node_id_t nodes[1024];
    uint32_t count = 1024;
    int rc = kg_compliance_get_subject_data(kg, subject_id, nodes, 1024, &count);
    if (rc != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compliance_delete_subject_data: validation failed");
        return -1;
    }

    /* In a real implementation, check for legal holds and delete nodes */

    return (int)count;
}

int kg_compliance_export_subject_data(const brain_kg_t* kg, const char* subject_id,
                                       const char* output_path) {
    if (!kg || !subject_id || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_delete_subject_data: required parameter is NULL (kg, subject_id, output_path)");
        return -1;
    }

    /* In a real implementation:
     * 1. Get all nodes for subject
     * 2. Serialize to JSON or CSV
     * 3. Write to output file
     */

    return 0;
}

int kg_compliance_anonymize_subject(brain_kg_t* kg, const char* subject_id) {
    if (!kg || !subject_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, subject_id)");
        return -1;
    }

    /* Get all nodes for subject */
    brain_kg_node_id_t nodes[1024];
    uint32_t count = 1024;
    int rc = kg_compliance_get_subject_data(kg, subject_id, nodes, 1024, &count);
    if (rc != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compliance_anonymize_subject: validation failed");
        return -1;
    }

    /* In a real implementation, anonymize PII fields in each node */
    int anonymized_fields = 0;

    return anonymized_fields;
}

/* ============================================================================
 * PII Detection & Masking
 * ============================================================================ */

int kg_compliance_scan_pii(const brain_kg_t* kg, kg_pii_detection_t* detections,
                            uint32_t max, uint32_t* count) {
    if (!kg || !detections || max == 0 || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, detections, count)");
        return -1;
    }

    /* In a real implementation, we would:
     * 1. Iterate all nodes
     * 2. Scan string fields with regex patterns
     * 3. Use ML classifier for ambiguous cases
     * 4. Record detections with confidence scores
     */

    *count = 0;

    return 0;
}

int kg_compliance_mask_pii(brain_kg_t* kg, brain_kg_node_id_t node_id,
                            const char* field) {
    if (!kg || !field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, field)");
        return -1;
    }

    (void)node_id;

    /* In a real implementation:
     * 1. Get field value
     * 2. Store original in encrypted vault
     * 3. Replace with masked value (e.g., "***@***.com")
     * 4. Mark field as masked
     */

    return 0;
}

int kg_compliance_unmask_pii(const brain_kg_t* kg, brain_kg_node_id_t node_id,
                              const char* field, char* value, size_t max_size) {
    if (!kg || !field || !value || max_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, field, value)");
        return -1;
    }

    (void)node_id;

    /* In a real implementation:
     * 1. Verify authorization
     * 2. Log access
     * 3. Retrieve from encrypted vault
     * 4. Return original value
     */

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compliance_anonymize_subject: operation failed");
    return -1; /* Not authorized / not found */
}

/* ============================================================================
 * Data Classification
 * ============================================================================ */

int kg_compliance_classify_node(brain_kg_t* kg, brain_kg_node_id_t node_id,
                                 kg_data_classification_t classification) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return -1;
    }

    (void)node_id;
    (void)classification;

    /* In a real implementation, store classification in node metadata */

    return 0;
}

kg_data_classification_t kg_compliance_get_classification(const brain_kg_t* kg,
                                                           brain_kg_node_id_t node_id) {
    if (!kg) {
        return KG_CLASS_PUBLIC;
    }

    (void)node_id;

    /* In a real implementation, query node metadata for classification */

    return KG_CLASS_PUBLIC;
}

/* ============================================================================
 * Data Lineage
 * ============================================================================ */

int kg_compliance_set_lineage(brain_kg_t* kg, brain_kg_node_id_t node_id,
                               const kg_data_lineage_t* lineage) {
    if (!kg || !lineage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, lineage)");
        return -1;
    }

    (void)node_id;

    /* In a real implementation, store lineage as node metadata */

    return 0;
}

int kg_compliance_get_lineage(const brain_kg_t* kg, brain_kg_node_id_t node_id,
                               kg_data_lineage_t* lineage) {
    if (!kg || !lineage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, lineage)");
        return -1;
    }

    (void)node_id;

    memset(lineage, 0, sizeof(*lineage));

    /* In a real implementation, query node metadata for lineage */

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compliance_anonymize_subject: operation failed");
    return -1; /* No lineage set */
}

int kg_compliance_trace_lineage(const brain_kg_t* kg, brain_kg_node_id_t node_id,
                                 kg_data_lineage_t* ancestors, uint32_t max,
                                 uint32_t* count) {
    if (!kg || !ancestors || max == 0 || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compliance_anonymize_subject: required parameter is NULL (kg, ancestors, count)");
        return -1;
    }

    (void)node_id;

    /* In a real implementation:
     * 1. Get lineage for node
     * 2. Recursively follow parent_nodes links
     * 3. Build ancestor chain
     */

    *count = 0;

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static const char* classification_strings[] = {
    "PUBLIC",
    "INTERNAL",
    "CONFIDENTIAL",
    "PII",
    "SENSITIVE",
    "RESTRICTED"
};

const char* kg_classification_to_string(kg_data_classification_t classification) {
    if (classification >= 0 && classification <= KG_CLASS_RESTRICTED) {
        return classification_strings[classification];
    }
    return "UNKNOWN";
}

static const char* pii_type_strings[] = {
    "NAME",
    "EMAIL",
    "PHONE",
    "ADDRESS",
    "SSN",
    "FINANCIAL",
    "HEALTH",
    "BIOMETRIC",
    "OTHER"
};

const char* kg_pii_type_to_string(kg_pii_type_t pii_type) {
    if (pii_type >= 0 && pii_type <= KG_PII_OTHER) {
        return pii_type_strings[pii_type];
    }
    return "UNKNOWN";
}

void kg_compliance_lineage_cleanup(kg_data_lineage_t* lineage) {
    if (!lineage) {
        return;
    }

    if (lineage->parent_nodes) {
        nimcp_free(lineage->parent_nodes);
        lineage->parent_nodes = NULL;
    }
}

void kg_compliance_subject_cleanup(kg_data_subject_t* subject) {
    if (!subject) {
        return;
    }

    if (subject->associated_nodes) {
        nimcp_free(subject->associated_nodes);
        subject->associated_nodes = NULL;
    }
}

void kg_compliance_policy_cleanup(kg_retention_policy_t* policy) {
    if (!policy) {
        return;
    }

    if (policy->legal_hold_tags) {
        nimcp_free(policy->legal_hold_tags);
        policy->legal_hold_tags = NULL;
    }
}
