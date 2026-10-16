// nimcp_ethics_part_accessors.c - accessors functions
// Part of nimcp_ethics.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_ethics.c


bool ethics_get_statistics(ethics_engine_t engine, ethics_statistics_t* stats)
{
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_get_statistics: required parameter is NULL (engine, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_get_statistics", 0.0f);

    /* Thread-safe reads of atomically-updated counters */
    stats->total_evaluations = __atomic_load_n(&engine->total_evaluations, __ATOMIC_RELAXED);
    stats->violations_detected = __atomic_load_n(&engine->violations_detected, __ATOMIC_RELAXED);
    stats->actions_blocked = engine->actions_blocked;
    stats->num_policies = engine->num_policies;
    stats->num_violations_logged = engine->num_violations;
    stats->avg_golden_rule_score = 0.5F;  // Placeholder

    return true;
}


//=============================================================================
// Internal Accessor Functions
//=============================================================================

brain_t ethics_engine_get_golden_rule_net(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_golden_ru", 0.0f);


    return engine ? engine->golden_rule_evaluator : NULL;
}


empathy_network_t ethics_engine_get_empathy_net(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_empathy_n", 0.0f);


    return engine ? engine->empathy_net : NULL;
}


asimov_config_t* ethics_engine_get_asimov_config(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_asimov_co", 0.0f);


    return engine ? &engine->asimov_config : NULL;
}


float ethics_engine_get_threshold(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_threshold", 0.0f);


    return engine ? engine->golden_rule_threshold : 0.0F;
}


bool ethics_engine_is_learning_enabled(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_is_learning_e", 0.0f);


    return engine ? engine->enable_learning : false;
}


uint32_t ethics_engine_get_num_policies(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_num_polic", 0.0f);


    return engine ? engine->num_policies : 0;
}


const ethics_policy_t* ethics_engine_get_policy(ethics_engine_t engine, uint32_t index) {
    if (!engine || index >= engine->num_policies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ethics_engine_get_policy: engine is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_policy", 0.0f);

    return &engine->policies[index];
}


const policy_strategy_table_t* ethics_engine_get_strategy_table(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_strategy_", 0.0f);


    return engine ? &engine->strategy_table : NULL;
}


ethics_incident_storage_t* ethics_engine_get_incident_storage(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_get_incident_", 0.0f);


    return engine ? &engine->incident_storage : NULL;
}


bool ethics_engine_is_asimov_locked(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_is_asimov_loc", 0.0f);


    return engine ? engine->asimov_laws_locked : false;
}


void ethics_engine_set_asimov_locked(ethics_engine_t engine, bool locked) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_set_asimov_lo", 0.0f);


    if (engine) {
        engine->asimov_laws_locked = locked;
    }
}


const uint8_t* ethics_engine_get_asimov_hash(ethics_engine_t engine) {
    return engine ? engine->asimov_laws_hash : NULL;
}


void ethics_engine_set_asimov_hash(ethics_engine_t engine, const uint8_t* hash) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_set_asimov_ha", 0.0f);


    if (engine && hash) {
        memcpy(engine->asimov_laws_hash, hash, 32);
    }
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_ethics_health_agent = agent;
    }
}
