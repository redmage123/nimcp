// nimcp_part_stats.c - stats functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c


/**
 * @brief Get brain's working memory statistics
 *
 * WHAT: Wrapper for working_memory_get_stats() on brain's working memory
 * WHY:  Provide public API for monitoring working memory state
 * HOW:  Validate brain → Get working memory → Extract size/capacity
 */
nimcp_status_t nimcp_brain_working_memory_stats(
    nimcp_brain_t brain,
    uint32_t* current_size_out,
    uint32_t* capacity_out)
{
    // Guard: Validate brain
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Validate output parameters
    API_CHECK_THROW(current_size_out && capacity_out, NIMCP_ERROR_NULL_ARG, "NULL output parameters");

    // Guard: Check if working memory enabled
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    API_CHECK_THROW(wm, NIMCP_ERROR_INVALID, "Working memory not enabled");

    // Get stats
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    *current_size_out = stats.current_size;
    *capacity_out = stats.capacity;

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_workspace_stats(
    nimcp_brain_t brain,
    uint32_t* total_broadcasts,
    uint32_t* total_competitions,
    float* avg_strength)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_stats");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Guard: Validate output parameters
    NIMCP_CHECK_THROW(total_broadcasts && total_competitions && avg_strength,
                      NIMCP_ERROR_NULL_ARG, "NULL output parameter in workspace_stats");

    // Get statistics
    workspace_statistics_t stats;
    bool success = global_workspace_get_statistics(workspace, &stats);
    NIMCP_CHECK_THROW(success, NIMCP_ERROR, "Failed to get workspace statistics");

    *total_broadcasts = (uint32_t)stats.total_broadcasts;
    *total_competitions = (uint32_t)stats.total_competitions;
    // Note: avg_strength not tracked in statistics, return 0.0
    *avg_strength = 0.0F;
    set_error("No error");
    return NIMCP_OK;
}
