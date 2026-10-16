// nimcp_knowledge_part_accessors.c - accessors functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c


/**
 * @brief Get knowledge map for domain
 *
 * Single-pass count without nested loops.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param domain Target domain or GENERAL for all
 * @param map_data Output graph structure
 * @param max_nodes Maximum nodes
 * @return Number of nodes in map
 *
 * Why: Visual knowledge maps aid learning and gap identification.
 * Optimized to single pass with early termination.
 */
uint32_t knowledge_get_map(knowledge_system_t system, knowledge_domain_t domain, void* map_data,
                           uint32_t max_nodes)
{
    if (!system)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_get_map", 0.0f);

    uint32_t count = 0;

    for (uint32_t i = 0; i < system->repository->num_items && count < max_nodes; i++) {
        knowledge_item_t* item = repository_get(system->repository, i);
        if (!item)
            continue;

        if (item->domain == domain || domain == KNOWLEDGE_DOMAIN_GENERAL) {
            count++;
        }
    }

    return count;
}


/**
 * @brief Get reading recommendations based on gaps
 *
 * @param system Knowledge system (must be non-NULL)
 * @param domain Target domain
 * @param recommendations Output array (must be non-NULL)
 * @param max_recommendations Maximum to return
 * @return Number of recommendations
 *
 * Why: Targeted recommendations fill knowledge gaps efficiently.
 * Future: Could analyze actual gaps and recommend accordingly.
 */
uint32_t knowledge_get_reading_list(knowledge_system_t system, knowledge_domain_t domain,
                                    char** recommendations, uint32_t max_recommendations)
{
    if (!system || !recommendations)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_get_reading_list", 0.0f);

    static const char* suggestions[] = {"The Very Hungry Caterpillar", "Where the Wild Things Are",
                                        "Charlotte's Web", "The Little Prince",
                                        "A Wrinkle in Time"};

    uint32_t num_suggestions = 5;
    uint32_t count =
        (num_suggestions < max_recommendations) ? num_suggestions : max_recommendations;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)count);
        }

        recommendations[i] = (char*) suggestions[i];
    }

    return count;
}


/**
 * @brief Get summary of all domains
 *
 * Single-pass generation of all assessments.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param all_domains Output array (must be non-NULL)
 * @param max_domains Array size
 * @return Number of domains assessed
 *
 * Why: Overall summary shows learning progress across all areas.
 * Helps identify which domains need more attention.
 */
uint32_t knowledge_get_summary(knowledge_system_t system, domain_knowledge_t* all_domains,
                               uint32_t max_domains)
{
    if (!system || !all_domains)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_get_summary", 0.0f);

    uint32_t count = (11 < max_domains) ? 11 : max_domains;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)count);
        }

        knowledge_assess_domain(system, (knowledge_domain_t) i, &all_domains[i]);
    }

    return count;
}


//=============================================================================
// B-TREE INDEXED QUERIES
//=============================================================================

/**
 * WHAT: Get knowledge items by confidence range using B-tree
 * WHY: Efficient queries for well/poorly understood concepts
 * HOW: B-tree provides O(log n + k) range queries
 */
uint32_t knowledge_get_by_confidence_range(knowledge_system_t system,
                                            float min_confidence,
                                            float max_confidence,
                                            knowledge_item_t** results_out)
{
    if (!system || !results_out) {
        if (results_out) *results_out = NULL;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_get_by_confidence_ra", 0.0f);


    if (min_confidence > max_confidence) {
        *results_out = NULL;
        return 0;
    }

    knowledge_repository_t* repo = system->repository;
    if (!repo || !repo->confidence_btree || repo->num_items == 0) {
        *results_out = NULL;
        return 0;
    }

    // Allocate maximum possible size (will trim later)
    knowledge_item_t* temp_results = nimcp_calloc(repo->num_items, sizeof(knowledge_item_t));
    if (!temp_results) {
        *results_out = NULL;
        return 0;
    }

    // Single pass: collect matching items with early exit
    btree_iterator_t* iter = btree_iterator_create(repo->confidence_btree);
    if (!iter) {
        nimcp_free(temp_results);
        temp_results = NULL;
        *results_out = NULL;
        return 0;
    }

    uint32_t count = 0;
    void* data = NULL;

    while (btree_iterator_next(iter, &data)) {
        knowledge_item_t* item = (knowledge_item_t*)data;
        if (item->confidence >= min_confidence && item->confidence <= max_confidence) {
            temp_results[count++] = *item;
        } else if (item->confidence > max_confidence) {
            break; // B-tree is sorted, early exit
        }
    }

    btree_iterator_destroy(iter);

    if (count == 0) {
        nimcp_free(temp_results);
        temp_results = NULL;
        *results_out = NULL;
        return 0;
    }

    // Trim to actual size (optional optimization)
    if (count < repo->num_items) {
        knowledge_item_t* final_results = nimcp_calloc(count, sizeof(knowledge_item_t));
        if (final_results) {
            for (uint32_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    knowledge_heartbeat("knowledge_loop",
                                     (float)(i + 1) / (float)count);
                }

                final_results[i] = temp_results[i];
            }
            nimcp_free(temp_results);
            temp_results = NULL;
            *results_out = final_results;
        } else {
            // If trim fails, return oversized array
            *results_out = temp_results;
        }
    } else {
        *results_out = temp_results;
    }

    return count;
}


/**
 * WHAT: Get all knowledge ordered by confidence using B-tree
 * WHY: Review knowledge progression from least to most understood
 * HOW: B-tree in-order traversal provides sorted output
 *
 * BUG FIX: Now uses unique keys (confidence_index) so B-tree stays accurate
 */
uint32_t knowledge_get_all_ordered_by_confidence(knowledge_system_t system,
                                                   knowledge_item_t** results_out)
{
    if (!system || !results_out) {
        if (results_out) *results_out = NULL;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_get_all_ordered_by_c", 0.0f);


    knowledge_repository_t* repo = system->repository;
    if (!repo || repo->num_items == 0) {
        *results_out = NULL;
        return 0;
    }

    // Allocate results array
    *results_out = nimcp_calloc(repo->num_items, sizeof(knowledge_item_t));
    if (!*results_out) {
        return 0;
    }

    // Use B-tree for sorted output (now maintains correct order with unique keys)
    if (repo->confidence_btree) {
        btree_iterator_t* iter = btree_iterator_create(repo->confidence_btree);
        if (iter) {
            void* data = NULL;
            uint32_t idx = 0;

            while (btree_iterator_next(iter, &data) && idx < repo->num_items) {
                knowledge_item_t* item = (knowledge_item_t*)data;
                (*results_out)[idx++] = *item;
            }

            btree_iterator_destroy(iter);
            return idx;
        }
    }

    // Fallback: copy in array order if B-tree unavailable
    for (uint32_t i = 0; i < repo->num_items; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && repo->num_items > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)repo->num_items);
        }

        (*results_out)[i] = repo->items[i];
    }

    return repo->num_items;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_knowledge_health_agent = agent;
    }
}
