//=============================================================================
// nimcp_brain_init_internal_kg.c - Internal Runtime Knowledge Graph Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_internal_kg.c
 * @brief Internal Runtime Knowledge Graph subsystem initialization
 *
 * INTERNAL KG vs KG READER:
 * - KG Reader: Reads from .aim/memory-nimcp.jsonl (static, external)
 * - Internal KG: In-memory CRUD graph (dynamic, runtime)
 *
 * The Internal KG provides:
 * - Real-time module mapping with CRUD operations
 * - Security integration with immune system
 * - Token-based access control
 * - Integrity verification
 * - Critical node protection
 *
 * INITIALIZATION PROCESS:
 * 1. Create internal KG with security enabled
 * 2. Generate admin token (stored in brain struct)
 * 3. Populate with all brain modules and connections
 * 4. Connect to immune system for threat reporting
 * 5. Mark critical nodes (core, ethics, immune, BBB)
 * 6. Compute initial integrity checksum
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: memory family roots */
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8: world-model family roots */
#include "cognitive/executive/nimcp_w9kg_events.h"  /* W9-kg: cognitive-control family roots */
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_internal_kg, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Forward Declarations
//=============================================================================

/* Immune system connection (avoid circular includes) */
struct brain_immune_system;

//=============================================================================
// Integration Helper Functions
//=============================================================================

/**
 * @brief Connect internal KG to immune system for threat reporting
 *
 * Security violations detected by the KG (unauthorized access, integrity
 * failures, critical node modifications) will be reported to the immune
 * system as potential threats.
 */
static void connect_internal_kg_to_immune(brain_kg_t* kg, brain_t brain) {
    if (!brain->immune_system) {
        fprintf(stderr, "[INTERNAL_KG] Immune system not available - skipping connection\n");
        return;
    }

    int result = brain_kg_connect_immune(kg, brain->immune_system);
    if (result == 0) {
        fprintf(stderr, "[INTERNAL_KG] Connected to immune system for threat reporting\n");
    } else {
        fprintf(stderr, "[INTERNAL_KG] WARNING: Failed to connect to immune system\n");
    }
}

/**
 * @brief Connect internal KG to FEP orchestrator
 *
 * The FEP orchestrator can use the KG to understand system topology
 * and optimize free energy minimization across modules.
 */
static void connect_internal_kg_to_fep(brain_kg_t* kg, brain_t brain) {
    (void)kg;  /* FEP doesn't have direct KG attachment yet */

    if (!brain->fep_orchestrator) {
        return;
    }

    fprintf(stderr, "[INTERNAL_KG] FEP orchestrator can now query module topology\n");
}

/**
 * @brief Connect internal KG to bio-async orchestrator
 *
 * The bio-async orchestrator can use the KG to optimize message routing.
 */
static void connect_internal_kg_to_bio_async(brain_kg_t* kg, brain_t brain) {
    (void)kg;  /* Bio-async doesn't have direct KG attachment yet */

    if (!brain->bio_async_orchestrator) {
        return;
    }

    fprintf(stderr, "[INTERNAL_KG] Bio-async orchestrator can now query module topology\n");
}

//=============================================================================
// Statistics Logging
//=============================================================================

/**
 * @brief Log internal KG statistics after population
 */
static void log_internal_kg_stats(brain_kg_t* kg) {
    brain_kg_stats_t stats;
    if (brain_kg_get_stats(kg, &stats) == 0) {
        fprintf(stderr, "[INTERNAL_KG] Statistics:\n");
        fprintf(stderr, "[INTERNAL_KG]   Total nodes: %u\n", stats.total_nodes);
        fprintf(stderr, "[INTERNAL_KG]   Total edges: %u\n", stats.total_edges);
        fprintf(stderr, "[INTERNAL_KG]   Node types:\n");
        fprintf(stderr, "[INTERNAL_KG]     - Core: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_CORE]);
        fprintf(stderr, "[INTERNAL_KG]     - Cortical: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_CORTICAL]);
        fprintf(stderr, "[INTERNAL_KG]     - Subcortical: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_SUBCORTICAL]);
        fprintf(stderr, "[INTERNAL_KG]     - Cognitive: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_COGNITIVE]);
        fprintf(stderr, "[INTERNAL_KG]     - Security: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_SECURITY]);
        fprintf(stderr, "[INTERNAL_KG]     - Training: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_TRAINING]);
        fprintf(stderr, "[INTERNAL_KG]     - Integration: %u\n", stats.nodes_by_type[BRAIN_KG_NODE_INTEGRATION]);
    }
}

/**
 * @brief Log security configuration
 */
static void log_security_config(brain_kg_t* kg) {
    uint32_t violations;
    uint64_t last_violation;

    brain_kg_get_security_stats(kg, &violations, &last_violation);

    fprintf(stderr, "[INTERNAL_KG] Security:\n");
    fprintf(stderr, "[INTERNAL_KG]   Access control: ENABLED\n");
    fprintf(stderr, "[INTERNAL_KG]   Integrity checks: ENABLED\n");
    fprintf(stderr, "[INTERNAL_KG]   Immune integration: ENABLED\n");
    fprintf(stderr, "[INTERNAL_KG]   Rate limiting: 1000 mutations/sec\n");
    fprintf(stderr, "[INTERNAL_KG]   Violations: %u\n", violations);
}

//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize internal knowledge graph subsystem for brain
 *
 * Creates the in-memory runtime knowledge graph with full CRUD operations
 * and security integration. The KG is populated with all brain modules
 * and their connections.
 *
 * @param brain Brain instance to initialize internal KG for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if internal KG is enabled in brain config
 * 2. Create KG with security configuration
 * 3. Generate admin token for privileged operations
 * 4. Populate with brain modules and connections
 * 5. Connect to immune system
 * 6. Verify initial integrity
 */
bool nimcp_brain_factory_init_internal_kg_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_internal_kg_subsystem: brain is NULL");

            return false;
    }

    /* Check if internal KG is disabled in config */
    if (!brain->config.enable_internal_kg) {
        brain->internal_kg = NULL;
        brain->internal_kg_enabled = false;
        brain->internal_kg_admin_token = 0;
        fprintf(stderr, "[INTERNAL_KG] Internal KG disabled by config\n");
        return true;  /* Success - internal KG is disabled by config */
    }

    fprintf(stderr, "[INTERNAL_KG] Initializing internal knowledge graph...\n");

    /* ====================================================================== */
    /* CREATE KG WITH SECURITY CONFIGURATION                                  */
    /* ====================================================================== */

    brain_kg_config_t config;
    brain_kg_default_config(&config);

    /* Security settings */
    config.enable_security = true;
    config.enable_integrity_checks = true;
    config.enable_access_control = true;
    config.enable_immune_integration = true;
    config.enable_audit_log = true;
    config.max_mutations_per_sec = 1000;
    config.integrity_check_interval_ms = 10000;

    /* Create internal KG */
    brain_kg_t* kg = brain_kg_create(&config);
    if (!kg) {
        fprintf(stderr, "[INTERNAL_KG] ERROR: Failed to create internal KG\n");
        brain->internal_kg = NULL;
        brain->internal_kg_enabled = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_internal_kg_subsystem: kg is NULL");
        return false;
    }

    /* ====================================================================== */
    /* GENERATE ADMIN TOKEN                                                   */
    /* ====================================================================== */

    uint64_t admin_token = 0;
    int token_result = brain_kg_generate_token(kg, BRAIN_KG_ACCESS_ADMIN, &admin_token);
    if (token_result != 0) {
        fprintf(stderr, "[INTERNAL_KG] ERROR: Failed to generate admin token\n");
        brain_kg_destroy(kg);
        brain->internal_kg = NULL;
        brain->internal_kg_enabled = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_init_internal_kg_subsystem: validation failed");
        return false;
    }

    /* Store admin token in brain for privileged operations */
    brain->internal_kg_admin_token = admin_token;
    fprintf(stderr, "[INTERNAL_KG] Admin token generated and stored\n");

    /* Set access level to ADMIN for population */
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, admin_token);

    /* ====================================================================== */
    /* POPULATE WITH BRAIN MODULES                                            */
    /* ====================================================================== */

    fprintf(stderr, "[INTERNAL_KG] Populating with brain modules...\n");
    int nodes_added = brain_kg_populate_from_brain(kg, brain);
    fprintf(stderr, "[INTERNAL_KG] Populated %d nodes with connections\n", nodes_added);

    /* ====================================================================== */
    /* STORE IN BRAIN                                                         */
    /* ====================================================================== */

    brain->internal_kg = kg;
    brain->internal_kg_enabled = true;

    /* W6: register memory-family root nodes + cognitive umbrella while we
     * still have ADMIN access. memory_kg_init_roots() handles its own
     * elevate/restore so it also works if called later, but doing it now
     * means the first events emitted by memory modules find their parents. */
    (void)memory_kg_init_roots(brain);

    /* W8: register world-model / imagination / FEP / predictive / salience
     * family root nodes. Idempotent and admin-elevates internally. */
    (void)world_model_kg_init_roots(brain);

    /* W9-kg: register cognitive-control family root nodes (attention,
     * working memory, executive, introspection, self_model, global_workspace,
     * self_awareness_extended, autobiographical_memory). Idempotent. */
    (void)w9kg_init_roots(brain);

    /* Log statistics */
    log_internal_kg_stats(kg);

    /* ====================================================================== */
    /* CONNECT TO RELEVANT SUBSYSTEMS                                         */
    /* ====================================================================== */

    /* 1. Immune System - Threat reporting */
    connect_internal_kg_to_immune(kg, brain);

    /* 2. FEP Orchestrator - Topology awareness */
    connect_internal_kg_to_fep(kg, brain);

    /* 3. Bio-Async Orchestrator - Message routing */
    connect_internal_kg_to_bio_async(kg, brain);

    /* ====================================================================== */
    /* INITIAL INTEGRITY CHECK                                                */
    /* ====================================================================== */

    /* brain_kg_populate_from_brain() seals an integrity checksum at its end,
     * but several subsequent init steps (memory_kg_init_roots, world_model_kg_init_roots,
     * w9kg_init_roots, connect_internal_kg_to_*) mutate the KG further. That
     * leaves a stale baseline that fires false integrity-violation reports on
     * the very first verify call. Reset the baseline so it captures the full
     * post-init state, then verify against it. */
    brain_kg_reset_integrity_baseline(kg);

    int integrity_result = brain_kg_verify_integrity(kg);
    if (integrity_result == 0) {
        fprintf(stderr, "[INTERNAL_KG] Initial integrity checksum computed\n");
    }

    /* Log security configuration */
    log_security_config(kg);

    /* Reset access level to READ for normal operation */
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    fprintf(stderr, "[INTERNAL_KG] Internal knowledge graph initialization complete\n");

    return true;
}

//=============================================================================
// Accessor Functions
//=============================================================================

/**
 * @brief Get internal knowledge graph from brain
 *
 * @param brain Brain instance
 * @return Internal KG handle or NULL if not enabled
 */
brain_kg_t* brain_get_internal_kg(brain_t brain) {
    if (!brain || !brain->internal_kg_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_internal_kg: invalid parameters");

            return NULL;
    }
    return brain->internal_kg;
}

/**
 * @brief Get admin token for internal KG operations
 *
 * Only use for privileged operations. Normal modules should use
 * READ access which doesn't require a token.
 *
 * @param brain Brain instance
 * @return Admin token or 0 if not available
 */
uint64_t brain_get_internal_kg_admin_token(brain_t brain) {
    if (!brain || !brain->internal_kg_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_internal_kg_admin_token: invalid parameters");

            return 0;
    }
    return brain->internal_kg_admin_token;
}

/**
 * @brief Refresh internal KG module states from brain
 *
 * Call this to update node states based on current module status.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_refresh_internal_kg(brain_t brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_refresh_internal_kg: invalid parameters");

            return -1;
    }

    /* Elevate to ADMIN for modifications */
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN,
                               brain->internal_kg_admin_token);

    int result = brain_kg_refresh_state(brain->internal_kg, brain);

    /* Reset to READ */
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);

    if (result == 0) {
        fprintf(stderr, "[INTERNAL_KG] Module states refreshed\n");
    }
    return result;
}

/**
 * @brief Emergency lock the internal KG
 *
 * Disables all write operations. Use in case of detected attack.
 *
 * @param brain Brain instance
 * @return 0 on success
 */
int brain_emergency_lock_internal_kg(brain_t brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_emergency_lock_internal_kg: invalid parameters");

            return -1;
    }

    return brain_kg_emergency_lock(brain->internal_kg);
}

/**
 * @brief Generate internal KG summary
 *
 * Creates a human-readable description of the module topology.
 *
 * @param brain Brain instance
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int brain_generate_internal_kg_summary(brain_t brain, char* buffer, size_t buffer_size) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_generate_internal_kg_summary: invalid parameters");

            return -1;
    }
    return brain_kg_generate_summary(brain->internal_kg, buffer, buffer_size);
}

//=============================================================================
// Destruction (called during brain_destroy)
//=============================================================================

/**
 * @brief Destroy internal KG subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_internal_kg_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    if (brain->internal_kg) {
        fprintf(stderr, "[INTERNAL_KG] Destroying internal knowledge graph\n");
        brain_kg_destroy(brain->internal_kg);
        brain->internal_kg = NULL;
    }

    brain->internal_kg_enabled = false;
    brain->internal_kg_admin_token = 0;
}
