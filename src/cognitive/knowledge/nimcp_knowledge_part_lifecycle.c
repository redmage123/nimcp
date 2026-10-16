// nimcp_knowledge_part_lifecycle.c - lifecycle functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c


/**
 * @brief Create hash table for concept_str indexing
 *
 * Creates empty hash table with chaining for collision resolution.
 *
 * @return Newly created hash table or NULL on allocation failure
 *
 * Why: Enables O(1) average case lookups instead of O(n) linear scans.
 * Memory: Allocates HASH_TABLE_SIZE pointers initially.
 */
static knowledge_hash_table_t* knowledge_hash_table_create(void)
{
    knowledge_hash_table_t* table = nimcp_calloc(1, sizeof(knowledge_hash_table_t));
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_hash_table_create: table is NULL");
        return NULL;
    }

    table->entries = nimcp_calloc(HASH_TABLE_SIZE, sizeof(hash_entry_t*));
    if (!table->entries) {
        nimcp_free(table);
        table = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_hash_table_create: table->entries is NULL");
        return NULL;
    }

    table->size = HASH_TABLE_SIZE;
    return table;
}


/**
 * @brief Destroy hash table and free all memory
 *
 * Frees all hash entries and their associated strings.
 *
 * @param table Hash table to destroy (can be NULL)
 *
 * Why: Prevents memory leaks from hash table chains.
 * Complexity: O(n) where n is total number of entries.
 */
static void knowledge_hash_table_destroy(knowledge_hash_table_t* table)
{
    if (!table)
        return;

    if (table->entries) {
        for (uint32_t i = 0; i < table->size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && table->size > 256) {
                knowledge_heartbeat("knowledge_loop",
                                 (float)(i + 1) / (float)table->size);
            }

            hash_entry_t* entry = table->entries[i];
            while (entry) {
                hash_entry_t* next = entry->next;
                nimcp_free(entry->concept);
                nimcp_free(entry);
                entry = NULL;
                entry = next;
            }
        }
        nimcp_free(table->entries);
    }

    nimcp_free(table);
    table = NULL;
}


//=============================================================================
// Repository Pattern Implementation
//=============================================================================

/**
 * @brief Create knowledge repository
 *
 * Initializes repository with hash table index for fast lookups.
 *
 * @param initial_capacity Initial storage capacity
 * @return New repository or NULL on allocation failure
 *
 * Why: Separates storage concerns from business logic (Repository Pattern).
 * Provides abstraction layer that can be swapped with different storage backends.
 */
static knowledge_repository_t* repository_create(uint32_t initial_capacity)
{
    knowledge_repository_t* repo = nimcp_calloc(1, sizeof(knowledge_repository_t));
    if (!repo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "repository_create: repo is NULL");
        return NULL;
    }

    repo->items = nimcp_calloc(initial_capacity, sizeof(knowledge_item_t));
    if (!repo->items) {
        nimcp_free(repo);
        repo = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "repository_create: repo->items is NULL");
        return NULL;
    }

    repo->index = knowledge_hash_table_create();
    if (!repo->index) {
        nimcp_free(repo->items);
        nimcp_free(repo);
        repo = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "repository_create: repo->index is NULL");
        return NULL;
    }

    // Create B-tree for confidence-based range queries
    repo->confidence_btree = btree_create(compare_confidence, extract_confidence_key, free_knowledge_item);
    if (!repo->confidence_btree) {
        knowledge_hash_table_destroy(repo->index);
        nimcp_free(repo->items);
        nimcp_free(repo);
        repo = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "repository_create: repo->confidence_btree is NULL");
        return NULL;
    }

    repo->capacity = initial_capacity;
    repo->num_items = 0;

    return repo;
}


/**
 * @brief Destroy repository and free all resources
 *
 * @param repo Repository to destroy (can be NULL)
 *
 * Why: Centralized cleanup ensures no memory leaks.
 * Frees: Items array, hash table, and all string arrays in items.
 */
static void repository_destroy(knowledge_repository_t* repo)
{
    if (!repo)
        return;

    if (repo->items) {
        for (uint32_t i = 0; i < repo->num_items; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && repo->num_items > 256) {
                knowledge_heartbeat("knowledge_loop",
                                 (float)(i + 1) / (float)repo->num_items);
            }

            if (repo->items[i].examples) {
                for (uint32_t j = 0; j < repo->items[i].num_examples; j++) {
                    nimcp_free(repo->items[i].examples[j]);
                }
                nimcp_free(repo->items[i].examples);
            }
            if (repo->items[i].related_concepts) {
                for (uint32_t j = 0; j < repo->items[i].num_related; j++) {
                    nimcp_free(repo->items[i].related_concepts[j]);
                }
                nimcp_free(repo->items[i].related_concepts);
            }
        }
        nimcp_free(repo->items);
    }

    knowledge_hash_table_destroy(repo->index);

    // Destroy B-tree
    if (repo->confidence_btree) {
        btree_destroy(repo->confidence_btree);
    }

    nimcp_free(repo);
    repo = NULL;
}


/**
 * @brief Create knowledge system with all components
 *
 * Initializes complete system including repository, brains, and strategies.
 * Uses guard clauses instead of nested if statements.
 *
 * @param learner_name Name for learner (must be non-NULL)
 * @return New knowledge system or NULL on failure
 *
 * Why: Complete initialization in one place with clear error handling.
 * Guard clauses improve readability vs nested ifs.
 *
 * Example:
 *   knowledge_system_t sys = knowledge_system_create("Alice");
 *   // System ready with all domains initialized
 */
knowledge_system_t knowledge_system_create(const char* learner_name)
{
    if (!learner_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_system_create: learner_name is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_system_create", 0.0f);


    knowledge_system_t system = nimcp_calloc(1, sizeof(struct knowledge_system_struct));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_system_create: failed to allocate system");
        return NULL;
    }

    strncpy(system->learner_name, learner_name, sizeof(system->learner_name) - 1);

    system->repository = repository_create(INITIAL_CAPACITY);
    if (!system->repository) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_system_create: failed to create repository");
        nimcp_free(system);
        system = NULL;
        return NULL;
    }

    system->narratives = nimcp_calloc(1000, sizeof(narrative_knowledge_t));
    if (!system->narratives) goto create_cleanup;
    system->narratives_capacity = 1000;

    system->artworks = nimcp_calloc(500, sizeof(aesthetic_knowledge_t));
    if (!system->artworks) goto create_cleanup;
    system->artworks_capacity = 500;

    system->history = nimcp_calloc(1000, sizeof(historical_knowledge_t));
    if (!system->history) goto create_cleanup;
    system->history_capacity = 1000;

    system->reading_list = nimcp_calloc(100, sizeof(reading_progress_t));
    if (!system->reading_list) goto create_cleanup;
    system->reading_capacity = 100;

    // goto target for allocation failures above — free everything allocated so far
    if (0) {
create_cleanup:
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_system_create: array allocation failed");
        nimcp_free(system->reading_list);
        nimcp_free(system->history);
        nimcp_free(system->artworks);
        nimcp_free(system->narratives);
        repository_destroy(system->repository);
        nimcp_free(system);
        return NULL;
    }

    // Initialize domain statistics (no brains needed - they were never used!)
    for (int i = 0; i < 11; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 11 > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)11);
        }

        initialize_domain_stats(&system->domain_stats[i], (knowledge_domain_t) i);
    }


    // Create unified brain for knowledge system (provides curiosity module)
    // Previously created curiosity independently - now follows "one brain, many modules" pattern
    // Use brain_create_lazy to defer heavyweight subsystem initialization until needed
    // This prevents test hangs from spawning threads that wait indefinitely
    system->knowledge_brain = brain_create_lazy(learner_name, BRAIN_SIZE_SMALL,
                                                BRAIN_TASK_CLASSIFICATION, 20, 10);
    if (!system->knowledge_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_system_create: failed to create brain");
        fprintf(stderr, "[ERROR] knowledge_system_create: failed to create brain\n"); fflush(stderr);
        knowledge_system_destroy(system);
        return NULL;
    }

    // Initialize bio-async fields
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;

    // Register with bio-async router if available
    LOG_DEBUG("knowledge: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        LOG_DEBUG("knowledge: Bio-router initialized, registering module (id=%d, inbox_capacity=32)...",
                           BIO_MODULE_KNOWLEDGE);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_KNOWLEDGE,
            .module_name = "knowledge",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            /* Try KG-driven wiring callback registration first */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_KNOWLEDGE,
                (void*)knowledge_wiring_handler_callback,
                system
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO("knowledge: KG-driven wiring callback registered");
            } else {
                /* Legacy fallback - register handlers directly */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_KNOWLEDGE_QUERY, handle_knowledge_query)
                );
                LOG_INFO("knowledge: legacy handler registration (module_id=%d)", BIO_MODULE_KNOWLEDGE);
            }
        } else {
            LOG_WARN("knowledge: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        LOG_DEBUG("knowledge: Bio-router not initialized, skipping async registration");
    }

    // Initialize SNN and Plasticity bridges
    knowledge_snn_config_t snn_config = knowledge_snn_config_default();
    system->snn_bridge = knowledge_snn_create(&snn_config);

    knowledge_plasticity_config_t plasticity_config = knowledge_plasticity_config_default();
    system->plasticity_bridge = knowledge_plasticity_create(&plasticity_config);

    system->bridges_enabled = (system->snn_bridge != NULL && system->plasticity_bridge != NULL);
    if (system->bridges_enabled) {
        LOG_INFO(LOG_MODULE, "Knowledge SNN and Plasticity bridges initialized");
    } else {
        LOG_WARN(LOG_MODULE, "Knowledge bridges partially failed - SNN:%p Plasticity:%p",
                 (void*)system->snn_bridge, (void*)system->plasticity_bridge);
    }

    return system;
}


// REMOVED: free_domain_brains() - Dead code, brains were never used

/**
 * @brief Destroy knowledge system and free all resources
 *
 * Comprehensive cleanup using guard clauses and helper functions.
 *
 * @param system System to destroy (can be NULL)
 *
 * Why: Complete cleanup prevents memory leaks.
 * Guard clauses and helpers reduce nesting and improve clarity.
 */
void knowledge_system_destroy(knowledge_system_t system)
{
    if (!system)
        return;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_system_destroy", 0.0f);

    // Unregister from bio-async router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
        LOG_INFO("Bio-async communication disabled for knowledge");
    }

    repository_destroy(system->repository);
    free_narratives(system->narratives, system->num_narratives);
    free_artworks(system->artworks, system->num_artworks);
    free_history(system->history, system->num_history);
    nimcp_free(system->reading_list);
    // REMOVED: free_domain_brains() call - brains were never used

    // Destroy unified brain (includes curiosity module cleanup)
    // Previously destroyed curiosity independently - now brain owns it
    if (system->knowledge_brain) {
        brain_destroy(system->knowledge_brain);
    }

    // Destroy SNN and Plasticity bridges
    if (system->snn_bridge) {
        knowledge_snn_destroy(system->snn_bridge);
        system->snn_bridge = NULL;
    }
    if (system->plasticity_bridge) {
        knowledge_plasticity_destroy(system->plasticity_bridge);
        system->plasticity_bridge = NULL;
    }
    system->bridges_enabled = false;

    nimcp_free(system);
    system = NULL;
}
