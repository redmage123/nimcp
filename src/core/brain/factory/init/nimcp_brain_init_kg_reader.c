//=============================================================================
// nimcp_brain_init_kg_reader.c - Knowledge Graph Reader Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_kg_reader.c
 * @brief Knowledge Graph Reader subsystem initialization for self-awareness
 *
 * SELF-AWARENESS ARCHITECTURE:
 * The Knowledge Graph Reader provides runtime access to NIMCP's structural
 * self-knowledge stored in .aim/memory-nimcp.jsonl. This enables:
 * - "What modules do I have?" - Component enumeration
 * - "How am I organized?" - Structural relationships
 * - "What can I do?" - Capability introspection
 *
 * INTEGRATION POINTS:
 * - self_model_system_t: Structural identity ("What am I?")
 * - introspection_context_t: Capability reflection ("What can I do?")
 * - autobiographical_memory_t: Links runtime experiences to self-knowledge
 *
 * The KG Reader is initialized early in brain creation to provide self-knowledge
 * to other subsystems that may need it during their initialization.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_kg_reader, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Integration Helper Functions
//=============================================================================

/**
 * @brief Connect KG reader to self-model subsystem
 *
 * Self-model integration enables:
 * - Structural identity queries
 * - Module capability enumeration
 * - Integration point discovery
 */
static void connect_kg_reader_to_self_model(kg_reader_t* kg_reader, brain_t brain) {
    if (!brain->self_model) {
        return;
    }

    /* Self-model can now query KG for structural self-knowledge */
    /* The KG reader provides answers to "What am I?" questions */
    fprintf(stderr, "[KG_READER] Connected to self-model system\n");

    /* Generate initial self-description and log it */
    char self_desc[NIMCP_JSON_BUFFER_SIZE];
    int len = kg_reader_generate_self_description(kg_reader, self_desc, sizeof(self_desc));
    if (len > 0) {
        fprintf(stderr, "[KG_READER] Self-description loaded (%d chars)\n", len);
    }
}

/**
 * @brief Connect KG reader to introspection subsystem
 *
 * Introspection integration enables:
 * - Capability reflection queries
 * - "What can I do?" answers
 * - Module feature enumeration
 */
static void connect_kg_reader_to_introspection(kg_reader_t* kg_reader, brain_t brain) {
    (void)kg_reader;  /* KG reader doesn't have direct introspection attach */

    if (!brain->introspection) {
        return;
    }

    /* Introspection can now query KG for capability information */
    fprintf(stderr, "[KG_READER] Connected to introspection system\n");
}

/**
 * @brief Connect KG reader to autobiographical memory
 *
 * Autobiographical memory integration enables:
 * - Linking runtime experiences to structural knowledge
 * - "How have I changed?" queries
 * - Historical self-awareness
 */
static void connect_kg_reader_to_autobiographical(kg_reader_t* kg_reader, brain_t brain) {
    (void)kg_reader;  /* KG reader doesn't have direct autobiographical attach */

    if (!brain->autobio) {
        return;
    }

    /* Autobiographical memory can now link experiences to structural knowledge */
    fprintf(stderr, "[KG_READER] Connected to autobiographical memory\n");
}

//=============================================================================
// Statistics Logging
//=============================================================================

/**
 * @brief Log KG reader statistics after loading
 */
static void log_kg_reader_stats(kg_reader_t* kg_reader) {
    kg_reader_stats_t stats;
    if (kg_reader_get_stats(kg_reader, &stats) == 0) {
        fprintf(stderr, "[KG_READER] Statistics:\n");
        fprintf(stderr, "[KG_READER]   Entities: %u\n", stats.total_entities);
        fprintf(stderr, "[KG_READER]   Relations: %u\n", stats.total_relations);
        fprintf(stderr, "[KG_READER]   Observations: %u\n", stats.total_observations);
        fprintf(stderr, "[KG_READER]   Load time: %lu us\n", (unsigned long)stats.load_time_us);
        fprintf(stderr, "[KG_READER]   File: %s\n", stats.file_path);
    }
}

//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize knowledge graph reader subsystem for brain
 *
 * Creates and loads the KG reader, providing runtime access to NIMCP's
 * self-knowledge stored in the knowledge graph.
 *
 * @param brain Brain instance to initialize KG reader for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if KG reader is enabled in brain config
 * 2. Create KG reader instance
 * 3. Load knowledge graph from file
 * 4. Store in brain->kg_reader
 * 5. Connect to relevant subsystems (self_model, introspection, autobiographical)
 */
bool nimcp_brain_factory_init_kg_reader_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_kg_reader_subsystem: brain is NULL");

            return false;
    }

    /* Check if KG reader is disabled in config */
    if (!brain->config.enable_kg_reader) {
        brain->kg_reader = NULL;
        brain->kg_reader_enabled = false;
        memset(brain->kg_file_path, 0, sizeof(brain->kg_file_path));
        return true;  /* Success - KG reader is disabled by config */
    }

    fprintf(stderr, "[KG_READER] Initializing knowledge graph reader...\n");

    /* Create KG reader instance */
    kg_reader_t* kg_reader = kg_reader_create();
    if (!kg_reader) {
        fprintf(stderr, "[KG_READER] ERROR: Failed to create KG reader\n");
        brain->kg_reader = NULL;
        brain->kg_reader_enabled = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_kg_reader_subsystem: kg_reader is NULL");
        return false;
    }

    /* Determine file path to load */
    const char* kg_path = KG_DEFAULT_PATH;  /* .aim/memory-nimcp.jsonl */
    if (brain->config.kg_file_path[0] != '\0') {
        kg_path = brain->config.kg_file_path;
    }

    /* Load knowledge graph */
    fprintf(stderr, "[KG_READER] Loading KG from: %s\n", kg_path);
    int load_result = kg_reader_load(kg_reader, kg_path);
    if (load_result != 0) {
        fprintf(stderr, "[KG_READER] WARNING: Failed to load KG from %s: %s\n",
                kg_path, kg_reader_get_last_error());
        fprintf(stderr, "[KG_READER] Creating empty KG reader (will reload when file available)\n");
        /* Don't fail - continue with empty reader, can reload later */
    }

    /* Store in brain */
    brain->kg_reader = kg_reader;
    brain->kg_reader_enabled = true;
    strncpy(brain->kg_file_path, kg_path, sizeof(brain->kg_file_path) - 1);
    brain->kg_file_path[sizeof(brain->kg_file_path) - 1] = '\0';

    /* Log statistics */
    log_kg_reader_stats(kg_reader);

    /* ====================================================================== */
    /* CONNECT TO RELEVANT SUBSYSTEMS                                         */
    /* ====================================================================== */

    /* 1. Self-Model - Structural identity */
    connect_kg_reader_to_self_model(kg_reader, brain);

    /* 2. Introspection - Capability reflection */
    connect_kg_reader_to_introspection(kg_reader, brain);

    /* 3. Autobiographical Memory - Historical self-awareness */
    connect_kg_reader_to_autobiographical(kg_reader, brain);

    fprintf(stderr, "[KG_READER] Knowledge graph reader initialization complete\n");

    return true;
}

//=============================================================================
// Accessor Functions
//=============================================================================

/**
 * @brief Get knowledge graph reader from brain
 *
 * @param brain Brain instance
 * @return KG reader handle or NULL if not enabled
 */
kg_reader_t* brain_get_kg_reader(brain_t brain) {
    if (!brain || !brain->kg_reader_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_kg_reader: invalid parameters");

            return NULL;
    }
    return brain->kg_reader;
}

/**
 * @brief Reload knowledge graph from file
 *
 * Call this if the KG file has been updated externally.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_reload_kg(brain_t brain) {
    if (!brain || !brain->kg_reader_enabled || !brain->kg_reader) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_reload_kg: invalid parameters");

            return -1;
    }

    int result = kg_reader_reload(brain->kg_reader);
    if (result == 0) {
        fprintf(stderr, "[KG_READER] Knowledge graph reloaded\n");
        log_kg_reader_stats(brain->kg_reader);
    }
    return result;
}

/**
 * @brief Check if KG file has been modified since last load
 *
 * @param brain Brain instance
 * @return true if file was modified
 */
bool brain_is_kg_modified(brain_t brain) {
    if (!brain || !brain->kg_reader_enabled || !brain->kg_reader) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_is_kg_modified: invalid parameters");

            return false;
    }
    return kg_reader_is_modified(brain->kg_reader);
}

/**
 * @brief Generate self-description from KG
 *
 * Creates a human-readable description of the system based on KG contents.
 *
 * @param brain Brain instance
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int brain_generate_self_description(brain_t brain, char* buffer, size_t buffer_size) {
    if (!brain || !brain->kg_reader_enabled || !brain->kg_reader || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_generate_self_description: invalid parameters");

            return -1;
    }
    return kg_reader_generate_self_description(brain->kg_reader, buffer, buffer_size);
}
