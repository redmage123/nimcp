// nimcp_knowledge_part_stats.c - stats functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c


/**
 * @brief Update domain statistics incrementally
 *
 * Updates stats after learning without full recalculation.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param domain Domain to update
 *
 * Why: Incremental update is O(1) vs O(n) full recalculation.
 * Maintains statistics efficiently as knowledge grows.
 */
static void update_domain_stats(knowledge_system_t system, knowledge_domain_t domain)
{
    if (!system)
        return;

    domain_knowledge_t* stats = &system->domain_stats[domain];
    stats->avg_confidence = calculate_domain_confidence(system, domain);
    stats->coverage_percentage = (stats->estimated_total > 0) ?
        ((float) stats->concepts_known / stats->estimated_total * 100.0F) : 0.0f;
}


//=============================================================================
// Knowledge System Creation/Destruction
//=============================================================================

// REMOVED: create_domain_brain() - Dead code, brains were never used

/**
 * @brief Initialize domain statistics structure
 *
 * Sets initial values for domain tracking.
 *
 * @param stats Statistics structure to initialize (must be non-NULL)
 * @param domain Domain type
 *
 * Why: Centralized initialization ensures consistent state.
 * Provides reasonable defaults for tracking.
 */
static void initialize_domain_stats(domain_knowledge_t* stats, knowledge_domain_t domain)
{
    if (!stats)
        return;

    stats->domain = domain;
    stats->concepts_known = 0;
    stats->estimated_total = 1000;
    stats->coverage_percentage = 0.0F;
    stats->avg_confidence = 0.0F;
    stats->num_gaps = 0;
}
