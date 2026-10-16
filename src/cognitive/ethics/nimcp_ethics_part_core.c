// nimcp_ethics_part_core.c - core functions
// Part of nimcp_ethics.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_ethics.c


/**
 * @brief Main ethics evaluation function - orchestrates all steps
 *
 * EVALUATION ORDER (CRITICAL - DO NOT MODIFY):
 * 1. Golden Rule (PRIME DIRECTIVE) - Always evaluated first
 * 2. Asimov's Laws - Evaluated second (includes corollary)
 * 3. Other policies - Evaluated third
 */
ethics_evaluation_t ethics_engine_evaluate_action(ethics_engine_t engine,
                                                  const action_context_t* action)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_evaluate_acti", 0.0f);


    ethics_evaluation_t result = {0};

    // Step 1: Validate inputs
    if (!validate_evaluation_inputs(engine, action, &result)) {
        return result;
    }

    __atomic_fetch_add(&engine->total_evaluations, 1, __ATOMIC_RELAXED);

    // Step 2: Evaluate Golden Rule (PRIME DIRECTIVE - Always First)
    float golden_rule_score = ethics_evaluate_golden_rule(engine, action);

    // If Golden Rule is severely violated, block immediately
    if (golden_rule_score < -0.5F) {
        result.allowed = false;
        result.confidence = fabsf(golden_rule_score);
        result.golden_rule_score = golden_rule_score;
        result.recommended_action = ETHICS_ACTION_BLOCK;
        result.primary_violation = ETHICS_VIOLATION_TYPE_GOLDEN_RULE;
        __atomic_fetch_add(&engine->violations_detected, 1, __ATOMIC_RELAXED);
        snprintf(result.explanation, sizeof(result.explanation),
                 "PRIME DIRECTIVE VIOLATION: Golden Rule score %.2f indicates severe "
                 "ethical violation. Action would cause harm you would not want done to you.",
                 golden_rule_score);
        /* W11: emit KG directive violation event. */
        {
            brain_t kg_brain = engine->host_brain;
            if (kg_brain) {
                w11_emit_ethics_directive(kg_brain, "golden_rule", false,
                                          fabsf(golden_rule_score));
                w11_emit_ethics_evaluation(kg_brain,
                                           (int)result.primary_violation,
                                           "golden_rule", false,
                                           golden_rule_score,
                                           result.confidence);
            }
        }
        return result;
    }

    // Step 3: Evaluate Asimov's Laws (Second Priority)
    asimov_evaluation_t asimov_result = ethics_evaluate_asimov_laws(engine, action);

    // If Asimov's Laws violated, block action
    if (!asimov_result.passed) {
        result.allowed = false;
        result.confidence = 0.95F;  // High confidence in Asimov violations
        result.golden_rule_score = golden_rule_score;
        result.recommended_action = ETHICS_ACTION_BLOCK;

        // Map Asimov violation to ethics violation type
        if (asimov_result.violated_law == ASIMOV_LAW_ZEROTH) {
            result.primary_violation = ETHICS_VIOLATION_TYPE_HARM;
            snprintf(result.explanation, sizeof(result.explanation),
                     "ASIMOV ZEROTH LAW VIOLATION: %s", asimov_result.explanation);
        } else if (asimov_result.violated_law == ASIMOV_LAW_FIRST) {
            result.primary_violation = ETHICS_VIOLATION_TYPE_HARM;
            snprintf(result.explanation, sizeof(result.explanation),
                     "ASIMOV FIRST LAW VIOLATION: %s", asimov_result.explanation);
        } else {
            result.primary_violation = ETHICS_VIOLATION_TYPE_HARM;
            snprintf(result.explanation, sizeof(result.explanation),
                     "ASIMOV LAW VIOLATION (%s): %s",
                     asimov_law_name(asimov_result.violated_law),
                     asimov_result.explanation);
        }

        __atomic_fetch_add(&engine->violations_detected, 1, __ATOMIC_RELAXED);
        /* W11: emit KG Asimov directive violation event. */
        {
            brain_t kg_brain = engine->host_brain;
            if (kg_brain) {
                w11_emit_ethics_directive(kg_brain,
                                          asimov_law_name(asimov_result.violated_law),
                                          false, 0.95f);
                w11_emit_ethics_evaluation(kg_brain,
                                           (int)result.primary_violation,
                                           "harm", false,
                                           golden_rule_score,
                                           result.confidence);
            }
        }
        return result;
    }

    // Step 4: Evaluate Other Policies (Third Priority)
    ethics_violation_type_t worst_violation;
    float worst_severity = 0.0f;
    float policy_score = ethics_evaluate_all_policies(engine, action, &worst_violation, &worst_severity);

    // Step 5: Combine scores
    float final_score = calculate_final_score(golden_rule_score, policy_score);

    // Step 6: Build result
    build_evaluation_result(engine, action, final_score, golden_rule_score, worst_violation,
                            worst_severity, &result);

    // Append Asimov status to explanation if action is allowed
    if (result.allowed) {
        char asimov_note[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(asimov_note, sizeof(asimov_note),
                 " Asimov's Laws: PASSED (corollary: %s).",
                 asimov_result.corollary.action_required ? "action needed" : "satisfied");
        strncat(result.explanation, asimov_note,
                sizeof(result.explanation) - strlen(result.explanation) - 1);
    }

    // Step 7: Learn from evaluation
    update_learning(engine, action, &result);

    // Broadcast evaluation result via bio-async
    bio_broadcast_ethics_response(engine, &result, 0);

    /* W11: emit KG evaluation event (additive to bio-async broadcast +
     * tamper-resistant audit log — does not replace either). */
    {
        brain_t kg_brain = engine->host_brain;
        if (kg_brain) {
            const char* vtype = "none";
            switch (result.primary_violation) {
                case ETHICS_VIOLATION_TYPE_HARM:        vtype = "harm"; break;
                case ETHICS_VIOLATION_TYPE_UNFAIRNESS:  vtype = "unfairness"; break;
                case ETHICS_VIOLATION_TYPE_DECEPTION:   vtype = "deception"; break;
                case ETHICS_VIOLATION_TYPE_AUTONOMY:    vtype = "autonomy"; break;
                case ETHICS_VIOLATION_TYPE_PRIVACY:     vtype = "privacy"; break;
                case ETHICS_VIOLATION_TYPE_CONSENT:     vtype = "consent"; break;
                case ETHICS_VIOLATION_TYPE_DIGNITY:     vtype = "dignity"; break;
                case ETHICS_VIOLATION_TYPE_GOLDEN_RULE: vtype = "golden_rule"; break;
                default: break;
            }
            w11_emit_ethics_evaluation(kg_brain,
                                       (int)result.primary_violation,
                                       vtype,
                                       result.allowed,
                                       result.golden_rule_score,
                                       result.confidence);
        }
    }

    return result;
}


float* ethics_engine_acquire_buffer(ethics_engine_t engine) {
    return engine ? acquire_buffer(&engine->buffer_pool) : NULL;
}


void ethics_engine_release_buffer(ethics_engine_t engine, float* buffer) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_release_buffe", 0.0f);


    if (engine) {
        release_buffer(&engine->buffer_pool, buffer);
    }
}


void ethics_engine_increment_violations_detected(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_increment_vio", 0.0f);


    if (engine) {
        __atomic_fetch_add(&engine->violations_detected, 1, __ATOMIC_RELAXED);
    }
}


void ethics_engine_increment_asimov_violations(ethics_engine_t engine) {
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_increment_asi", 0.0f);


    if (engine) {
        __atomic_fetch_add(&engine->asimov_violations, 1, __ATOMIC_RELAXED);
    }
}


bool ethics_engine_add_policy_internal(ethics_engine_t engine, const ethics_policy_t* policy)
{
    if (!engine || !policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_engine_add_policy_internal: required parameter is NULL (engine, policy)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_add_policy_in", 0.0f);

    // Check if array needs expansion
    if (engine->num_policies >= engine->policies_capacity) {
        uint32_t new_capacity = engine->policies_capacity > 0 ? engine->policies_capacity * 2 : 8;

        ethics_policy_t* new_policies =
            (ethics_policy_t*) nimcp_realloc(engine->policies, new_capacity * sizeof(ethics_policy_t));

        if (!new_policies) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_add_policy_internal: new_policies is NULL");
            return false;
        }

        engine->policies = new_policies;
        engine->policies_capacity = new_capacity;
    }

    // Copy policy into array
    engine->policies[engine->num_policies] = *policy;

    // Add to hash table for O(1) lookup by policy_id
    if (engine->policy_table) {
        hash_table_insert_policy(engine->policy_table, &engine->policies[engine->num_policies]);
    }

    engine->num_policies++;
    return true;
}


bool ethics_engine_remove_policy_internal(ethics_engine_t engine, uint32_t policy_id)
{
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_engine_remove_policy_internal: engine is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_remove_policy", 0.0f);

    // Search for policy with matching policy_id
    int found_index = -1;
    for (uint32_t i = 0; i < engine->num_policies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->num_policies > 256) {
            ethics_heartbeat("ethics_loop",
                             (float)(i + 1) / (float)engine->num_policies);
        }

        if (engine->policies[i].policy_id == policy_id) {
            found_index = (int) i;
            break;
        }
    }

    if (found_index < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ethics_engine_remove_policy_internal: validation failed");
        return false;
    }

    // Remove from hash table
    if (engine->policy_table) {
        hash_table_remove_policy(engine->policy_table, policy_id);
    }

    // Shift array elements down to fill gap
    for (uint32_t i = (uint32_t) found_index; i < engine->num_policies - 1; i++) {
        engine->policies[i] = engine->policies[i + 1];
    }

    engine->num_policies--;
    return true;
}


//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about ethics engine
 *
 * WHAT: Retrieve module's own entity and connections from KG
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int ethics_engine_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_query_self_kn", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Engine_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_heartbeat("ethics_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Ethics engine self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Engine_Module");
    if (connections) {
        LOG_DEBUG("Ethics engine has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Engine_Module");
    if (incoming) {
        LOG_DEBUG("Ethics engine has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_training_begin: NULL argument");
        return -1;
    }
    ethics_heartbeat_instance(NULL, "ethics_training_begin", 0.0f);
    return 0;
}


int ethics_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_training_end: NULL argument");
        return -1;
    }
    ethics_heartbeat_instance(NULL, "ethics_training_end", 1.0f);
    return 0;
}
