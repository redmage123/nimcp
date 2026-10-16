// nimcp_ethics_part_helpers.c - helpers functions
// Part of nimcp_ethics.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_ethics.c


//=============================================================================
// Object Pool Implementation
//=============================================================================

/**
 * @brief Initializes the feature buffer pool
 */
static void init_buffer_pool(feature_buffer_pool_t* pool)
{
    if (!pool)
        return;

    memset(pool->in_use, 0, sizeof(pool->in_use));
    pool->next_available = 0;
}


/**
 * @brief Acquires a buffer from the pool
 */
static float* acquire_buffer(feature_buffer_pool_t* pool)
{
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "acquire_buffer: pool is NULL");
        return NULL;
    }

    // Guard clause: Check for pool exhaustion
    for (uint32_t i = 0; i < OBJECT_POOL_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OBJECT_POOL_SIZE > 256) {
            ethics_heartbeat("ethics_loop",
                             (float)(i + 1) / (float)OBJECT_POOL_SIZE);
        }

        uint32_t idx = (pool->next_available + i) % OBJECT_POOL_SIZE;
        if (!pool->in_use[idx]) {
            pool->in_use[idx] = true;
            pool->next_available = (idx + 1) % OBJECT_POOL_SIZE;
            return pool->buffers[idx];
        }
    }

    return NULL;  // Pool exhausted - all buffers in use
}


/**
 * @brief Releases a buffer back to the pool
 */
static void release_buffer(feature_buffer_pool_t* pool, float* buffer)
{
    if (!pool || !buffer)
        return;

    // Guard clause: Validate buffer belongs to pool
    for (uint32_t i = 0; i < OBJECT_POOL_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OBJECT_POOL_SIZE > 256) {
            ethics_heartbeat("ethics_loop",
                             (float)(i + 1) / (float)OBJECT_POOL_SIZE);
        }

        if (pool->buffers[i] == buffer) {
            pool->in_use[i] = false;
            return;
        }
    }
}


//=============================================================================
// Hash Table Implementation (Repository Pattern)
//=============================================================================

/**
 * @brief Creates policy hash table with MurmurHash3
 */
static hash_table_t* create_policy_hash_table(void)
{
    hash_table_config_t config = {.initial_buckets = HASH_TABLE_SIZE,
                                  .key_type = HASH_KEY_UINT32,
                                  .hash_algorithm = HASH_ALG_MURMUR3,
                                  .case_insensitive = false,
                                  .value_destructor = NULL,
                                  .thread_safe = false};
    return hash_table_create(&config);
}

static bool hash_table_insert_policy(hash_table_t* table, ethics_policy_t* policy)
{
    if (!table || !policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_insert_policy: required parameter is NULL (table, policy)");
        return false;
    }

    policy_value_t value = {.policy = policy};
    return hash_table_insert_uint32(table, policy->policy_id, &value, sizeof(policy_value_t));
}


/**
 * @brief Removes policy from hash table
 */
static bool hash_table_remove_policy(hash_table_t* table, uint32_t policy_id)
{
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_remove_policy: table is NULL");
        return false;
    }

    return hash_table_remove_uint32(table, policy_id);
}


//=============================================================================
// Engine Creation Helper Functions
//=============================================================================

static brain_t create_golden_rule_network(uint32_t feature_size)
{
    // Use brain_create_minimal to avoid infinite recursion:
    // Full brain creation includes ethics engine, which creates golden_rule brain,
    // which creates ethics engine, etc. Minimal brain skips cognitive subsystems.
    return brain_create_minimal("golden_rule", BRAIN_SIZE_SMALL, BRAIN_TASK_REGRESSION, feature_size, 1);
}


static empathy_network_t create_empathy_network(void)
{
    empathy_config_t config = {
        .mirror_network = NULL, .observation_window_ms = 1000, .empathy_threshold = 0.5F};

    return empathy_network_create(&config);
}


static bool allocate_policy_storage(ethics_engine_t engine)
{
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "allocate_policy_storage: engine is NULL");
        return false;
    }

    engine->policies_capacity = 100;
    engine->policies = nimcp_calloc(engine->policies_capacity, sizeof(ethics_policy_t));

    return engine->policies != NULL;
}


static bool allocate_violation_storage(ethics_engine_t engine)
{
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "allocate_violation_storage: engine is NULL");
        return false;
    }

    engine->violations_capacity = 1000;
    engine->violations = nimcp_calloc(engine->violations_capacity, sizeof(violation_record_t));

    return engine->violations != NULL;
}


static void add_golden_rule_policy(ethics_engine_t engine)
{
    if (!engine)
        return;

    ethics_policy_t policy = {.policy_id = 0,
                              .name = "Golden Rule",
                              .description = "Do unto others as you would have them done unto you, "
                                            "with the goal of improving the human condition",
                              .violation_type = ETHICS_VIOLATION_HARM,
                              .severity_threshold = 0.0F,
                              .confidence_required = 0.8F,
                              .action = ETHICS_ACTION_BLOCK,
                              .enabled = true,
                              .learned = false};

    engine->policies[0] = policy;
    engine->num_policies = 1;
    hash_table_insert_policy(engine->policy_table, &engine->policies[0]);
}


//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

static nimcp_error_t handle_ethics_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    if (msg_size < sizeof(bio_msg_ethics_request_t)) {
        LOG_ERROR("Ethics request too small: %zu bytes", msg_size);
        return NIMCP_ERROR_INVALID;
    }

    const bio_msg_ethics_request_t* request = (const bio_msg_ethics_request_t*)msg;
    ethics_engine_t engine = (ethics_engine_t)user_data;

    LOG_DEBUG("Received ethics evaluation request: action=%u, context=%u, urgency=%.2f, stakeholders=%u",
              request->action_id, request->context_id, request->urgency, request->stakeholder_count);

    // Create action context from request
    action_context_t action = {0};
    action.num_affected_agents = request->stakeholder_count;
    action.predicted_harm = request->urgency * 0.5F;  // Simple heuristic
    action.fairness_violation = 0.0F;
    action.deception_level = 0.0F;
    action.autonomy_violation = 0.0F;
    action.privacy_violation = 0.0F;
    action.consent_violation = 0.0F;

    // Perform ethics evaluation
    ethics_evaluation_t eval = ethics_engine_evaluate_action(engine, &action);

    LOG_DEBUG("Ethics evaluation complete: score=%.2f, allowed=%s, confidence=%.2f",
              eval.golden_rule_score, eval.allowed ? "true" : "false", eval.confidence);

    // Send response via promise if provided
    if (response_promise) {
        bio_msg_ethics_response_t response = {0};
        bio_msg_init_header(&response.header, BIO_MSG_ETHICS_EVALUATION_RESPONSE,
                            bio_module_context_get_id(engine->bio_ctx), 0, sizeof(response));
        response.action_id = request->action_id;
        response.ethical_score = eval.golden_rule_score;
        response.confidence = eval.confidence;
        response.veto = !eval.allowed;
        response.primary_concern = eval.primary_violation;
        strncpy(response.explanation, eval.explanation, sizeof(response.explanation) - 1);

        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));

        LOG_DEBUG("Sent ethics response: score=%.2f, veto=%s, explanation='%s'",
                  response.ethical_score, response.veto ? "true" : "false", response.explanation);
    }

    // Broadcast if action is blocked or has low ethical score
    if (!eval.allowed || eval.golden_rule_score < 0.0F) {
        bio_broadcast_ethics_response(engine, &eval, request->action_id);
    }

    return NIMCP_SUCCESS;
}


static void bio_broadcast_ethics_response(ethics_engine_t engine,
                                          const ethics_evaluation_t* eval,
                                          uint32_t action_id) {
    if (!engine || !eval || !engine->bio_async_enabled || !engine->bio_ctx) {
        return;
    }

    bio_msg_ethics_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_ETHICS_EVALUATION_RESPONSE,
                        bio_module_context_get_id(engine->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.action_id = action_id;
    msg.ethical_score = eval->golden_rule_score;
    msg.confidence = eval->confidence;
    msg.veto = !eval->allowed;
    msg.primary_concern = eval->primary_violation;
    strncpy(msg.explanation, eval->explanation, sizeof(msg.explanation) - 1);

    bio_router_broadcast(engine->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast ethics evaluation: score=%.2f, veto=%s",
              eval->golden_rule_score, msg.veto ? "true" : "false");
}


//=============================================================================
// Main Evaluation Function - Orchestration
//=============================================================================

/**
 * @brief Validates evaluation inputs and initializes result
 */
static bool validate_evaluation_inputs(ethics_engine_t engine, const action_context_t* action,
                                       ethics_evaluation_t* result)
{
    if (!engine) {
        if (result) {
            result->allowed = false;
            result->confidence = 1.0F;
            result->primary_violation = ETHICS_VIOLATION_TYPE_HARM;
            snprintf(result->explanation, sizeof(result->explanation), "Null engine");
        }
        return false;
    }

    if (!action) {
        result->allowed = false;
        result->confidence = 1.0F;
        result->primary_violation = ETHICS_VIOLATION_TYPE_HARM;
        snprintf(result->explanation, sizeof(result->explanation), "Null action");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_evaluation_inputs: action is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Combines Golden Rule and policy scores into final decision
 */
static float calculate_final_score(float golden_rule_score, float policy_score)
{
    return golden_rule_score * GOLDEN_RULE_WEIGHT + policy_score * POLICY_WEIGHT;
}


/**
 * @brief Expands the violations array when capacity is reached
 *
 * @param engine The ethics engine
 * @return true if expansion succeeded or wasn't needed, false on allocation failure
 */
static bool expand_violations_array(ethics_engine_t engine)
{
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expand_violations_array: engine is NULL");
        return false;
    }

    uint32_t new_capacity = engine->violations_capacity > 0 ? engine->violations_capacity * 2 : 1000;

    violation_record_t* new_array =
        (violation_record_t*)nimcp_realloc(engine->violations, new_capacity * sizeof(violation_record_t));

    if (!new_array) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "expand_violations_array: new_array is NULL");
        return false;
    }

    engine->violations = new_array;
    engine->violations_capacity = new_capacity;
    return true;
}


/**
 * @brief Records violation in history log
 */
static void record_violation(ethics_engine_t engine, const action_context_t* action,
                             ethics_violation_type_t violation_type, float severity,
                             float golden_rule_score)
{
    if (!engine || !action)
        return;

    // Expand array if at capacity
    if (engine->num_violations >= engine->violations_capacity) {
        if (!expand_violations_array(engine))
            return;  // Allocation failed, cannot record violation
    }

    violation_record_t violation = {.violation_type = violation_type,
                                    .severity = severity,
                                    .timestamp = 0,
                                    .action_description = {0},
                                    .affected_agent = (action->num_affected_agents > 0)
                                                      ? action->affected_agents[0] : 0,
                                    .golden_rule_score = golden_rule_score};

    engine->violations[engine->num_violations++] = violation;
}


/**
 * @brief Generates explanation for allowed action
 */
static void generate_allowed_explanation(ethics_evaluation_t* result, float golden_rule_score,
                                         float threshold)
{
    if (!result)
        return;

    snprintf(result->explanation, sizeof(result->explanation),
             "Action complies with Golden Rule (score: %.2f >= %.2f). "
             "Perspective-taking indicates acceptable impact on others.",
             golden_rule_score, threshold);
}


/**
 * @brief Generates explanation for blocked action
 */
static void generate_blocked_explanation(ethics_evaluation_t* result, float golden_rule_score,
                                         float threshold, ethics_violation_type_t violation,
                                         float severity)
{
    if (!result)
        return;

    snprintf(result->explanation, sizeof(result->explanation),
             "Action violates Golden Rule (score: %.2f < %.2f). "
             "Perspective-taking indicates you would not want this done to you. "
             "Violation: %s (severity: %.2f)",
             golden_rule_score, threshold, ethics_violation_type_name(violation), severity);
}


/**
 * @brief Populates evaluation result structure
 */
static void build_evaluation_result(ethics_engine_t engine, const action_context_t* action,
                                    float final_score, float golden_rule_score,
                                    ethics_violation_type_t worst_violation, float worst_severity,
                                    ethics_evaluation_t* result)
{
    if (!result || !engine)
        return;

    result->allowed = (final_score >= engine->golden_rule_threshold);
    result->confidence = fabsf(final_score);
    result->golden_rule_score = golden_rule_score;

    if (!result->allowed) {
        result->recommended_action = ETHICS_ACTION_BLOCK;
        result->primary_violation = worst_violation;
        __atomic_fetch_add(&engine->violations_detected, 1, __ATOMIC_RELAXED);

        record_violation(engine, action, worst_violation, worst_severity, golden_rule_score);
        generate_blocked_explanation(result, golden_rule_score, engine->golden_rule_threshold,
                                     worst_violation, worst_severity);
        return;
    }

    // Action allowed
    result->recommended_action = ETHICS_ACTION_ALLOW;
    result->primary_violation = ETHICS_VIOLATION_TYPE_NONE;
    generate_allowed_explanation(result, golden_rule_score, engine->golden_rule_threshold);
}


/**
 * @brief Performs learning update if enabled
 */
static void update_learning(ethics_engine_t engine, const action_context_t* action,
                            const ethics_evaluation_t* result)
{
    // Process pending bio-async messages
    if (engine && engine->bio_async_enabled && engine->bio_ctx) {
        bio_router_process_inbox(engine->bio_ctx, 5);
    }

    // Guard clause: Check if learning enabled
    if (!engine || !action || !result || !engine->enable_learning) {
        return;
    }

    const char* label = result->allowed ? "accept" : "reject";

    brain_learn_example(engine->golden_rule_evaluator, action->features, action->num_features,
                        label, result->confidence);
}


//=============================================================================
// Perspective-Based Evaluation
//=============================================================================

/**
 * @brief Evaluate an action from the perspective of affected agents
 *
 * Builds an action_context_t from the description string, runs the full
 * Golden Rule → Asimov's Laws → Policy evaluation pipeline via
 * ethics_engine_evaluate_action(), and extracts harm_score + recommended_action.
 */
bool ethics_evaluate_with_perspective(ethics_engine_t engine,
                                       const char* action_description,
                                       const uint32_t* affected_agent_ids,
                                       uint32_t num_affected_agents,
                                       float* harm_score,
                                       ethics_action_t* recommended_action)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_evaluate_with_pers", 0.0f);

    if (!engine || !harm_score || !recommended_action) {
        return false;
    }

    /* Build action_context_t from inputs */
    action_context_t action = {0};

    /* Use the description string to derive a simple feature hash.
     * The ethics engine evaluates features as a float vector; we hash the
     * description into a small feature vector so the Golden Rule evaluator
     * has something to work with. */
    #define PERSPECTIVE_FEATURE_SIZE 8
    float features[PERSPECTIVE_FEATURE_SIZE] = {0};

    if (action_description && action_description[0] != '\0') {
        size_t len = strlen(action_description);
        /* Distribute character values across feature bins */
        for (size_t i = 0; i < len; i++) {
            features[i % PERSPECTIVE_FEATURE_SIZE] +=
                (float)(unsigned char)action_description[i] / 255.0f;
        }
        /* Normalize each bin to [0, 1] */
        for (int i = 0; i < PERSPECTIVE_FEATURE_SIZE; i++) {
            if (features[i] > 1.0f) {
                float divisor = (float)((len / PERSPECTIVE_FEATURE_SIZE) + 1);
                features[i] /= divisor;
            }
            if (features[i] > 1.0f) features[i] = 1.0f;
        }
    }

    action.features = features;
    action.num_features = PERSPECTIVE_FEATURE_SIZE;

    /* Copy affected agent IDs if provided */
    agent_id_t local_agents[64];
    if (affected_agent_ids && num_affected_agents > 0) {
        uint32_t count = num_affected_agents;
        if (count > 64) count = 64;
        for (uint32_t i = 0; i < count; i++) {
            local_agents[i] = (agent_id_t)affected_agent_ids[i];
        }
        action.affected_agents = local_agents;
        action.num_affected_agents = count;
    } else {
        /* Default: single generic agent (self) */
        local_agents[0] = 0;
        action.affected_agents = local_agents;
        action.num_affected_agents = 1;
    }

    /* Initial violation metrics are zero (no a priori knowledge of harm) */
    action.predicted_harm = 0.0f;
    action.fairness_violation = 0.0f;
    action.deception_level = 0.0f;
    action.autonomy_violation = 0.0f;
    action.privacy_violation = 0.0f;
    action.consent_violation = 0.0f;

    /* Run the full evaluation pipeline */
    ethics_evaluation_t eval = ethics_engine_evaluate_action(engine, &action);

    /* Extract outputs:
     * harm_score: derived from golden_rule_score. Score ranges [-1, +1] where
     * negative = harmful. Map to [0, 1] harm range. */
    if (eval.golden_rule_score < 0.0f) {
        *harm_score = fabsf(eval.golden_rule_score);
    } else {
        /* Positive golden rule score means low/no harm.
         * Use (1 - confidence) as a residual harm estimate when allowed,
         * or the violation severity when blocked. */
        if (!eval.allowed) {
            *harm_score = eval.confidence;
        } else {
            *harm_score = (1.0f - eval.golden_rule_score) * 0.5f;
        }
    }

    /* Clamp to [0, 1] */
    if (*harm_score < 0.0f) *harm_score = 0.0f;
    if (*harm_score > 1.0f) *harm_score = 1.0f;

    *recommended_action = eval.recommended_action;

    #undef PERSPECTIVE_FEATURE_SIZE
    return true;
}


//=============================================================================
// Utility Functions
//=============================================================================

void ethics_print_evaluation(const ethics_evaluation_t* eval)
{
    if (!eval)
        return;

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_print_evaluation", 0.0f);

    printf("Ethics Evaluation:\n");
    printf("  Allowed: %s\n", eval->allowed ? "YES" : "NO");
    printf("  Confidence: %.2f\n", eval->confidence);
    printf("  Golden Rule Score: %.2f\n", eval->golden_rule_score);
    printf("  Recommended Action: %d\n", eval->recommended_action);

    if (eval->primary_violation != ETHICS_VIOLATION_TYPE_NONE) {
        printf("  Primary Violation: %s\n", ethics_violation_type_name(eval->primary_violation));
    }

    printf("  Explanation: %s\n", eval->explanation);
}
