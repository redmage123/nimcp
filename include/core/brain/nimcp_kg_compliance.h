/**
 * @file nimcp_kg_compliance.h
 * @brief Compliance and Data Governance for Knowledge Graph
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Data governance, privacy compliance, and regulatory support for KG
 * WHY:  Enable GDPR/CCPA compliance, PII protection, data lineage tracking
 * HOW:  Classification, retention policies, subject tracking, PII detection
 *
 * COMPLIANCE ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    KG COMPLIANCE & DATA GOVERNANCE                         |
 * +===========================================================================+
 * |                                                                            |
 * |   DATA CLASSIFICATION                                                      |
 * |   -----------------------------------------------------------------------  |
 * |   PUBLIC -> INTERNAL -> CONFIDENTIAL -> PII -> SENSITIVE -> RESTRICTED    |
 * |                                                                            |
 * |   RETENTION POLICIES                                                       |
 * |   -----------------------------------------------------------------------  |
 * |   | Policy Name     | Applies To   | Retention | Auto-Delete | Legal Hold||
 * |   | "default"       | PUBLIC       | 365 days  | yes         | none      ||
 * |   | "pii_policy"    | PII          | 90 days   | yes         | gdpr      ||
 * |   | "audit_log"     | INTERNAL     | 7 years   | no          | legal     ||
 * |   -----------------------------------------------------------------------  |
 * |                                                                            |
 * |   GDPR / PRIVACY COMPLIANCE                                                |
 * |   -----------------------------------------------------------------------  |
 * |   - Data Subject Registration (consent tracking)                          |
 * |   - Subject Access Requests (data export)                                 |
 * |   - Right to be Forgotten (data deletion)                                 |
 * |   - Data Portability (format conversion)                                  |
 * |   - Anonymization / Pseudonymization                                      |
 * |                                                                            |
 * |   PII DETECTION & MASKING                                                  |
 * |   -----------------------------------------------------------------------  |
 * |   - Automatic PII scanning (name, email, phone, SSN, etc.)               |
 * |   - Field-level masking (reversible with authorization)                  |
 * |   - Confidence scoring for detection accuracy                            |
 * |                                                                            |
 * |   DATA LINEAGE                                                             |
 * |   -----------------------------------------------------------------------  |
 * |   - Source system tracking (where data came from)                        |
 * |   - Transformation history (how data was modified)                       |
 * |   - Parent node tracing (derivation ancestry)                            |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe via internal synchronization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_COMPLIANCE_H
#define NIMCP_KG_COMPLIANCE_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of policy name */
#define KG_COMPLIANCE_MAX_POLICY_NAME      64

/** Maximum length of subject ID */
#define KG_COMPLIANCE_MAX_SUBJECT_ID       64

/** Maximum length of consent scope description */
#define KG_COMPLIANCE_MAX_CONSENT_SCOPE    256

/** Maximum length of field name */
#define KG_COMPLIANCE_MAX_FIELD_NAME       64

/** Maximum length of source system name */
#define KG_COMPLIANCE_MAX_SOURCE_SYSTEM    64

/** Maximum length of source ID */
#define KG_COMPLIANCE_MAX_SOURCE_ID        128

/** Maximum length of transformation description */
#define KG_COMPLIANCE_MAX_TRANSFORMATION   256

/* ============================================================================
 * Data Classification
 * ============================================================================ */

/**
 * @brief Data classification level
 *
 * WHAT: Classification levels for data sensitivity and access control
 * WHY:  Enable tiered access control and compliance-driven handling
 * HOW:  Applied to nodes, controls access and retention behavior
 *
 * Classification levels are ordered by increasing restriction.
 * Higher levels inherit all restrictions from lower levels.
 */
typedef enum {
    KG_CLASS_PUBLIC = 0,           /**< No restrictions - publicly accessible */
    KG_CLASS_INTERNAL,             /**< Internal use only - organizational access */
    KG_CLASS_CONFIDENTIAL,         /**< Restricted access - need-to-know basis */
    KG_CLASS_PII,                  /**< Personally identifiable information */
    KG_CLASS_SENSITIVE,            /**< Sensitive data (health, financial, etc.) */
    KG_CLASS_RESTRICTED            /**< Highest restriction level - special handling */
} kg_data_classification_t;

/* ============================================================================
 * Retention Policy
 * ============================================================================ */

/**
 * @brief Retention policy for data lifecycle management
 *
 * WHAT: Defines how long data should be retained and when it can be deleted
 * WHY:  Comply with data retention regulations and storage optimization
 * HOW:  Applied by classification level, supports legal holds
 *
 * Retention policies control the data lifecycle:
 * - retention_days: How long to keep data (0 = indefinite)
 * - enable_auto_delete: Whether to automatically purge expired data
 * - require_approval_for_delete: Manual approval workflow
 * - legal_hold_tags: Tags that suspend deletion (e.g., "litigation", "audit")
 */
typedef struct {
    char policy_name[KG_COMPLIANCE_MAX_POLICY_NAME]; /**< Unique policy identifier */
    kg_data_classification_t applies_to;             /**< Classification level this applies to */
    uint32_t retention_days;         /**< Days to retain (0 = forever) */
    bool enable_auto_delete;         /**< Auto-delete after retention period */
    bool require_approval_for_delete; /**< Manual approval needed for deletion */
    char* legal_hold_tags;           /**< Comma-separated tags that prevent deletion */
} kg_retention_policy_t;

/* ============================================================================
 * Data Subject (GDPR)
 * ============================================================================ */

/**
 * @brief Data subject record for GDPR compliance
 *
 * WHAT: Tracks individuals whose personal data is stored in the KG
 * WHY:  Enable GDPR rights: access, rectification, erasure, portability
 * HOW:  Links subject identity to associated nodes with consent tracking
 *
 * GDPR requires tracking:
 * - What consent was given and when
 * - What data is associated with the subject
 * - When they last requested access (SAR)
 * - Whether consent is still active
 */
typedef struct {
    char subject_id[KG_COMPLIANCE_MAX_SUBJECT_ID]; /**< Unique subject identifier */
    char* associated_nodes;          /**< Comma-separated node IDs containing subject data */
    uint64_t consent_timestamp;      /**< When consent was given (ms since epoch) */
    char consent_scope[KG_COMPLIANCE_MAX_CONSENT_SCOPE]; /**< What was consented to */
    bool consent_active;             /**< Is consent still valid */
    uint64_t last_access_request;    /**< Last Subject Access Request timestamp */
} kg_data_subject_t;

/* ============================================================================
 * PII Detection
 * ============================================================================ */

/**
 * @brief PII type classification
 *
 * WHAT: Categories of personally identifiable information
 * WHY:  Enable type-specific handling and masking strategies
 * HOW:  Detected via pattern matching and ML classification
 */
typedef enum {
    KG_PII_NAME = 0,               /**< Personal name (first, last, full) */
    KG_PII_EMAIL,                  /**< Email address */
    KG_PII_PHONE,                  /**< Phone number */
    KG_PII_ADDRESS,                /**< Physical address */
    KG_PII_SSN,                    /**< Social Security Number / National ID */
    KG_PII_FINANCIAL,              /**< Financial data (credit card, bank account) */
    KG_PII_HEALTH,                 /**< Health/medical information (PHI) */
    KG_PII_BIOMETRIC,              /**< Biometric data (fingerprint, face, voice) */
    KG_PII_OTHER                   /**< Other PII not in above categories */
} kg_pii_type_t;

/**
 * @brief PII detection result
 *
 * WHAT: Result of scanning a node for personally identifiable information
 * WHY:  Enable automatic PII discovery and protection
 * HOW:  Pattern matching, regex, and ML-based detection
 *
 * Detection results include:
 * - Location (node and field)
 * - PII type classification
 * - Confidence score (higher = more certain)
 * - Masking status
 */
typedef struct {
    brain_kg_node_id_t node_id;     /**< Node containing detected PII */
    char field_name[KG_COMPLIANCE_MAX_FIELD_NAME]; /**< Field/attribute name */
    kg_pii_type_t pii_type;         /**< Type of PII detected */
    float confidence;               /**< Detection confidence (0.0-1.0) */
    bool is_masked;                 /**< Whether field is already masked */
} kg_pii_detection_t;

/* ============================================================================
 * Data Lineage
 * ============================================================================ */

/**
 * @brief Data lineage record
 *
 * WHAT: Tracks the origin and transformation history of data
 * WHY:  Enable data provenance, auditability, and impact analysis
 * HOW:  Attached to nodes, links to source systems and parent nodes
 *
 * Lineage tracks:
 * - Source system (external database, API, file, etc.)
 * - Original identifier in source system
 * - Import timestamp
 * - Transformations applied
 * - Parent nodes this was derived from
 */
typedef struct {
    brain_kg_node_id_t node_id;     /**< Node this lineage applies to */
    char source_system[KG_COMPLIANCE_MAX_SOURCE_SYSTEM]; /**< Source system name */
    char source_id[KG_COMPLIANCE_MAX_SOURCE_ID]; /**< Original ID in source */
    uint64_t import_timestamp;      /**< When imported (ms since epoch) */
    char transformation[KG_COMPLIANCE_MAX_TRANSFORMATION]; /**< Transformation description */
    char* parent_nodes;             /**< Comma-separated parent node IDs */
} kg_data_lineage_t;

/* ============================================================================
 * Retention Policy Management
 * ============================================================================ */

/**
 * @brief Set a retention policy
 *
 * WHAT: Create or update a data retention policy
 * WHY:  Define how long data should be kept by classification
 * HOW:  Store policy in compliance registry
 *
 * @param policy Retention policy to set (copied internally)
 * @return 0 on success, -1 on error
 */
int kg_compliance_set_retention(const kg_retention_policy_t* policy);

/**
 * @brief Get a retention policy by name
 *
 * WHAT: Retrieve an existing retention policy
 * WHY:  Query current retention settings
 * HOW:  Look up by policy name in registry
 *
 * @param policy_name Name of policy to retrieve
 * @param policy Output policy structure
 * @return 0 on success, -1 if not found
 */
int kg_compliance_get_retention(const char* policy_name, kg_retention_policy_t* policy);

/**
 * @brief Apply retention policies to KG
 *
 * WHAT: Enforce retention policies across all nodes
 * WHY:  Automatically purge expired data per policy
 * HOW:  Scan nodes, check retention, delete if expired and no legal hold
 *
 * @param kg Knowledge graph to apply retention to
 * @return Number of nodes deleted, -1 on error
 */
int kg_compliance_apply_retention(brain_kg_t* kg);

/**
 * @brief Set legal hold on a node
 *
 * WHAT: Prevent deletion of a node due to legal/audit requirements
 * WHY:  Comply with litigation hold or audit preservation
 * HOW:  Add hold tag to node, prevents auto-deletion
 *
 * @param kg Knowledge graph
 * @param node_id Node to place on hold
 * @param hold_tag Legal hold identifier (e.g., "case_12345")
 * @return 0 on success, -1 on error
 */
int kg_compliance_set_legal_hold(brain_kg_t* kg, brain_kg_node_id_t node_id,
                                  const char* hold_tag);

/**
 * @brief Remove legal hold from a node
 *
 * WHAT: Release a node from legal hold
 * WHY:  Allow normal retention processing after hold expires
 * HOW:  Remove hold tag from node
 *
 * @param kg Knowledge graph
 * @param node_id Node to release
 * @param hold_tag Legal hold identifier to remove
 * @return 0 on success, -1 if hold not found
 */
int kg_compliance_remove_legal_hold(brain_kg_t* kg, brain_kg_node_id_t node_id,
                                     const char* hold_tag);

/* ============================================================================
 * GDPR / Right to be Forgotten
 * ============================================================================ */

/**
 * @brief Register a data subject
 *
 * WHAT: Register an individual whose data is stored in the KG
 * WHY:  Track consent and enable GDPR rights
 * HOW:  Store subject record with consent information
 *
 * @param kg Knowledge graph
 * @param subject Data subject information (copied internally)
 * @return 0 on success, -1 on error
 */
int kg_compliance_register_subject(brain_kg_t* kg, const kg_data_subject_t* subject);

/**
 * @brief Get all nodes containing a subject's data
 *
 * WHAT: Retrieve all KG nodes associated with a data subject
 * WHY:  Support Subject Access Requests (SAR)
 * HOW:  Query subject registry and return node IDs
 *
 * @param kg Knowledge graph
 * @param subject_id Subject identifier
 * @param nodes Output array for node IDs (caller allocated)
 * @param max Maximum nodes to return
 * @param count Output: actual number of nodes found
 * @return 0 on success, -1 on error
 */
int kg_compliance_get_subject_data(const brain_kg_t* kg, const char* subject_id,
                                    brain_kg_node_id_t* nodes, uint32_t max,
                                    uint32_t* count);

/**
 * @brief Delete all data for a subject (Right to be Forgotten)
 *
 * WHAT: Remove all nodes containing a subject's personal data
 * WHY:  Comply with GDPR Article 17 erasure requests
 * HOW:  Find all associated nodes and delete them
 *
 * Note: Will fail if any nodes have active legal holds.
 *
 * @param kg Knowledge graph
 * @param subject_id Subject identifier
 * @return Number of nodes deleted, -1 on error
 */
int kg_compliance_delete_subject_data(brain_kg_t* kg, const char* subject_id);

/**
 * @brief Export subject data (Data Portability)
 *
 * WHAT: Export all data for a subject to a portable format
 * WHY:  Comply with GDPR Article 20 data portability
 * HOW:  Gather all subject nodes and export to JSON/CSV
 *
 * @param kg Knowledge graph
 * @param subject_id Subject identifier
 * @param output_path File path for export output
 * @return 0 on success, -1 on error
 */
int kg_compliance_export_subject_data(const brain_kg_t* kg, const char* subject_id,
                                       const char* output_path);

/**
 * @brief Anonymize subject data
 *
 * WHAT: Remove identifying information while keeping data structure
 * WHY:  Enable data retention for analytics after consent withdrawal
 * HOW:  Replace PII with anonymized placeholders
 *
 * @param kg Knowledge graph
 * @param subject_id Subject identifier
 * @return Number of fields anonymized, -1 on error
 */
int kg_compliance_anonymize_subject(brain_kg_t* kg, const char* subject_id);

/* ============================================================================
 * PII Detection & Masking
 * ============================================================================ */

/**
 * @brief Scan KG for PII
 *
 * WHAT: Automatically detect personally identifiable information
 * WHY:  Identify data requiring special protection
 * HOW:  Pattern matching and ML-based detection across all nodes
 *
 * @param kg Knowledge graph to scan
 * @param detections Output array for detection results (caller allocated)
 * @param max Maximum detections to return
 * @param count Output: actual number of detections
 * @return 0 on success, -1 on error
 */
int kg_compliance_scan_pii(const brain_kg_t* kg, kg_pii_detection_t* detections,
                            uint32_t max, uint32_t* count);

/**
 * @brief Mask a PII field
 *
 * WHAT: Replace PII with masked representation
 * WHY:  Protect sensitive data from unauthorized access
 * HOW:  Encrypt/hash original value, store masked version
 *
 * Masking is reversible with proper authorization.
 *
 * @param kg Knowledge graph
 * @param node_id Node containing PII
 * @param field Field name to mask
 * @return 0 on success, -1 on error
 */
int kg_compliance_mask_pii(brain_kg_t* kg, brain_kg_node_id_t node_id,
                            const char* field);

/**
 * @brief Unmask a PII field (requires authorization)
 *
 * WHAT: Retrieve original value of masked PII field
 * WHY:  Enable authorized access to protected data
 * HOW:  Decrypt/lookup original value with audit logging
 *
 * Note: Requires appropriate authorization. Access is logged.
 *
 * @param kg Knowledge graph
 * @param node_id Node containing masked PII
 * @param field Field name to unmask
 * @param value Output buffer for unmasked value
 * @param max_size Size of output buffer
 * @return 0 on success, -1 on error or unauthorized
 */
int kg_compliance_unmask_pii(const brain_kg_t* kg, brain_kg_node_id_t node_id,
                              const char* field, char* value, size_t max_size);

/* ============================================================================
 * Data Classification
 * ============================================================================ */

/**
 * @brief Classify a node
 *
 * WHAT: Set data classification level for a node
 * WHY:  Control access and retention based on sensitivity
 * HOW:  Store classification as node metadata
 *
 * @param kg Knowledge graph
 * @param node_id Node to classify
 * @param classification Classification level
 * @return 0 on success, -1 on error
 */
int kg_compliance_classify_node(brain_kg_t* kg, brain_kg_node_id_t node_id,
                                 kg_data_classification_t classification);

/**
 * @brief Get node classification
 *
 * WHAT: Retrieve classification level for a node
 * WHY:  Determine access control and retention policy
 * HOW:  Query node metadata
 *
 * @param kg Knowledge graph
 * @param node_id Node to query
 * @return Classification level, or KG_CLASS_PUBLIC if not set
 */
kg_data_classification_t kg_compliance_get_classification(const brain_kg_t* kg,
                                                           brain_kg_node_id_t node_id);

/* ============================================================================
 * Data Lineage
 * ============================================================================ */

/**
 * @brief Set lineage for a node
 *
 * WHAT: Record data provenance for a node
 * WHY:  Enable audit trail and impact analysis
 * HOW:  Store lineage record as node metadata
 *
 * @param kg Knowledge graph
 * @param node_id Node to set lineage for
 * @param lineage Lineage information (copied internally)
 * @return 0 on success, -1 on error
 */
int kg_compliance_set_lineage(brain_kg_t* kg, brain_kg_node_id_t node_id,
                               const kg_data_lineage_t* lineage);

/**
 * @brief Get lineage for a node
 *
 * WHAT: Retrieve data provenance for a node
 * WHY:  Audit data origin and transformations
 * HOW:  Query node metadata for lineage record
 *
 * @param kg Knowledge graph
 * @param node_id Node to query
 * @param lineage Output lineage structure
 * @return 0 on success, -1 if no lineage set
 */
int kg_compliance_get_lineage(const brain_kg_t* kg, brain_kg_node_id_t node_id,
                               kg_data_lineage_t* lineage);

/**
 * @brief Trace lineage ancestry
 *
 * WHAT: Get all ancestor nodes in the derivation chain
 * WHY:  Understand complete data provenance
 * HOW:  Recursively follow parent_nodes links
 *
 * @param kg Knowledge graph
 * @param node_id Starting node
 * @param ancestors Output array for ancestor lineage records
 * @param max Maximum ancestors to return
 * @param count Output: actual number of ancestors found
 * @return 0 on success, -1 on error
 */
int kg_compliance_trace_lineage(const brain_kg_t* kg, brain_kg_node_id_t node_id,
                                 kg_data_lineage_t* ancestors, uint32_t max,
                                 uint32_t* count);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert data classification to string
 *
 * @param classification Classification level
 * @return String representation (e.g., "PUBLIC", "PII", "RESTRICTED")
 */
const char* kg_classification_to_string(kg_data_classification_t classification);

/**
 * @brief Convert PII type to string
 *
 * @param pii_type PII type
 * @return String representation (e.g., "NAME", "EMAIL", "SSN")
 */
const char* kg_pii_type_to_string(kg_pii_type_t pii_type);

/**
 * @brief Create a default retention policy
 *
 * @param policy Output policy structure
 * @return 0 on success
 */
int kg_compliance_default_retention(kg_retention_policy_t* policy);

/**
 * @brief Free resources in a lineage structure
 *
 * WHAT: Release heap-allocated fields in lineage struct
 * WHY:  Clean resource management
 * HOW:  Free parent_nodes if allocated
 *
 * @param lineage Lineage structure to clean (NULL safe)
 */
void kg_compliance_lineage_cleanup(kg_data_lineage_t* lineage);

/**
 * @brief Free resources in a subject structure
 *
 * WHAT: Release heap-allocated fields in subject struct
 * WHY:  Clean resource management
 * HOW:  Free associated_nodes if allocated
 *
 * @param subject Subject structure to clean (NULL safe)
 */
void kg_compliance_subject_cleanup(kg_data_subject_t* subject);

/**
 * @brief Free resources in a retention policy structure
 *
 * WHAT: Release heap-allocated fields in policy struct
 * WHY:  Clean resource management
 * HOW:  Free legal_hold_tags if allocated
 *
 * @param policy Policy structure to clean (NULL safe)
 */
void kg_compliance_policy_cleanup(kg_retention_policy_t* policy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_COMPLIANCE_H */
