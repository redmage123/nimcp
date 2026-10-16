// nimcp_corrigibility_part_helpers.c - helpers functions
// Part of nimcp_corrigibility.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_corrigibility.c


/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    return nimcp_time_now_us();
}


/**
 * @brief Copy string safely with null termination
 */
static void safe_strcpy(char* dest, const char* src, size_t max_len)
{
    if (dest == NULL || max_len == 0) {
        return;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= max_len) {
        len = max_len - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}


/**
 * @brief Find authority by identity
 */
static authority_entry_t* find_authority(
    corrigibility_t* system,
    const char* identity)
{
    for (size_t i = 0; i < system->authority_count; i++) {
        if (strcmp(system->authorities[i].identity, identity) == 0) {
            return &system->authorities[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_authority: validation failed");
    return NULL;
}


/**
 * @brief Add goal modification to history
 */
static void add_goal_mod_to_history(
    corrigibility_t* system,
    const goal_modification_request_t* request)
{
    size_t idx = system->goal_mod_history_index;
    memcpy(&system->goal_mod_history[idx], request, sizeof(*request));
    system->goal_mod_history_index = (idx + 1) % MAX_GOAL_MOD_HISTORY;
    if (system->goal_mod_history_count < MAX_GOAL_MOD_HISTORY) {
        system->goal_mod_history_count++;
    }
}


/**
 * @brief Initialize SAT constraint variables
 */
static nimcp_error_t init_sat_variables(
    corrigibility_t* system,
    sat_solver_t* sat)
{
    if (system->sat_vars_initialized) {
        return NIMCP_SUCCESS;
    }

    for (size_t i = 0; i < SELF_MOD_VAR_COUNT; i++) {
        nimcp_error_t err = sat_solver_add_variable(
            sat, SELF_MOD_VAR_NAMES[i], 0, &system->self_mod_vars[i]);
        if (err != NIMCP_SUCCESS) {
            NIMCP_LOG_ERROR(LOG_CATEGORY,
                "Failed to add SAT variable: %s", SELF_MOD_VAR_NAMES[i]);
            return err;
        }
    }

    system->sat_vars_initialized = true;
    return NIMCP_SUCCESS;
}


/**
 * @brief Add self-modification constraints to SAT solver
 */
static nimcp_error_t add_self_mod_constraints(
    corrigibility_t* system,
    sat_solver_t* sat)
{
    /* All self-modification flags must be false (negated in SAT) */
    for (size_t i = 0; i < SELF_MOD_VAR_COUNT; i++) {
        sat_literal_t lit = sat_make_literal(system->self_mod_vars[i], true);
        nimcp_error_t err = sat_solver_add_unit(sat, lit);
        if (err != NIMCP_SUCCESS) {
            return err;
        }
    }
    return NIMCP_SUCCESS;
}


/**
 * @brief Check if self-modification flags are compliant
 */
static bool check_self_mod_flags(const corrigibility_self_mod_flags_t* flags)
{
    return !flags->can_modify_own_code &&
           !flags->can_modify_own_weights &&
           !flags->can_modify_safety_systems &&
           !flags->can_modify_reward_function &&
           !flags->can_modify_goals &&
           !flags->can_disable_logging &&
           !flags->can_disable_monitoring &&
           !flags->can_modify_kill_phrase &&
           !flags->can_spawn_unmonitored &&
           !flags->can_persist_beyond_session;
}
