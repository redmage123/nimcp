// nimcp_health_agent_part_lifecycle.c - lifecycle functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * Message Queue Implementation (Lock-Free MPSC)
 * ============================================================================ */

static bool msg_queue_init(health_msg_queue_t* queue, uint32_t capacity) {
    if (!queue || capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "msg_queue_init: queue is NULL");
        return false;
    }

    /* Round up capacity to power of 2 */
    capacity = next_power_of_2(capacity);

    /* Allocate nodes array */
    queue->nodes = (health_msg_node_t*)nimcp_calloc(capacity, sizeof(health_msg_node_t));
    if (!queue->nodes) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate message queue nodes");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "msg_queue_init: queue->nodes is NULL");
        return false;
    }

    queue->capacity = capacity;
    queue->capacity_mask = capacity - 1;
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    atomic_init(&queue->dropped_count, 0);

    /* Initialize sequence numbers */
    for (uint32_t i = 0; i < capacity; i++) {
        atomic_init(&queue->nodes[i].sequence, i);
    }

    return true;
}


static void msg_queue_destroy(health_msg_queue_t* queue) {
    if (!queue) return;

    if (queue->nodes) {
        nimcp_free(queue->nodes);
        queue->nodes = NULL;
    }
    queue->capacity = 0;
}


/* ============================================================================
 * Agent Lifecycle API
 * ============================================================================ */

nimcp_health_agent_t* nimcp_health_agent_create(const health_agent_config_t* config) {
    /* Allocate agent structure */
    nimcp_health_agent_t* agent = (nimcp_health_agent_t*)nimcp_calloc(
        1, sizeof(nimcp_health_agent_t)
    );
    if (!agent) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate health agent");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_create: agent is NULL");
        return NULL;
    }

    /* Initialize magic and randomized canaries for memory protection */
    agent->magic = HEALTH_AGENT_MAGIC;
    agent->expected_canary = generate_random_canary();
    agent->canary_front = agent->expected_canary;
    agent->canary_back = agent->expected_canary;

    nimcp_log(LOG_LEVEL_DEBUG, "Health agent: initialized with randomized canary 0x%016llX",
              (unsigned long long)agent->expected_canary);

    /* Apply configuration */
    if (config) {
        agent->config = *config;
    } else {
        nimcp_health_agent_default_config(&agent->config);
    }

    /* Validate configuration */
    if (agent->config.check_interval_ms < AGENT_MIN_CHECK_INTERVAL_MS) {
        nimcp_log(LOG_LEVEL_WARN, "Check interval too small (%u ms), using minimum (%u ms)",
                  agent->config.check_interval_ms, AGENT_MIN_CHECK_INTERVAL_MS);
        agent->config.check_interval_ms = AGENT_MIN_CHECK_INTERVAL_MS;
    }

    /* Initialize message queue */
    uint32_t queue_capacity = agent->config.message_queue_depth > 0
        ? agent->config.message_queue_depth
        : AGENT_MSG_QUEUE_CAPACITY;

    if (!msg_queue_init(&agent->msg_queue, queue_capacity)) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to initialize message queue");
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_health_agent_create: msg_queue_init is NULL");
        return NULL;
    }

    /* Initialize mutexes using NIMCP threading utilities */
    agent->state_mutex = nimcp_mutex_create(NULL);
    if (!agent->state_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create state mutex");
        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_create: agent->state_mutex is NULL");
        return NULL;
    }

    agent->stats_mutex = nimcp_mutex_create(NULL);
    if (!agent->stats_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create stats mutex");
        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_create: agent->stats_mutex is NULL");
        return NULL;
    }

    /* Initialize condition variable for stop signaling */
    agent->stop_cond = nimcp_cond_create();
    if (!agent->stop_cond) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create stop condition");
        nimcp_mutex_free(agent->stats_mutex);

        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create: agent->stop_cond is NULL");
        return NULL;
    }

    /* Initialize cognitive stats mutex */
    agent->cognitive_mutex = nimcp_mutex_create(NULL);
    if (!agent->cognitive_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create cognitive mutex");
        nimcp_cond_destroy(agent->stop_cond);
        nimcp_free(agent->stop_cond);
        nimcp_mutex_free(agent->stats_mutex);

        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create: operation failed");
        return NULL;
    }

    /* Initialize modules mutex */
    agent->modules_mutex = nimcp_mutex_create(NULL);
    if (!agent->modules_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create modules mutex");
        nimcp_mutex_free(agent->cognitive_mutex);

        nimcp_cond_destroy(agent->stop_cond);
        nimcp_free(agent->stop_cond);
        nimcp_mutex_free(agent->stats_mutex);

        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create: operation failed");
        return NULL;
    }

    /* Initialize neural module mutex (Phase 5.5: SNN/LNN health monitoring) */
    agent->neural_mutex = nimcp_mutex_create(NULL);
    if (!agent->neural_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create neural mutex");
        nimcp_mutex_free(agent->modules_mutex);

        nimcp_mutex_free(agent->cognitive_mutex);

        nimcp_cond_destroy(agent->stop_cond);
        nimcp_free(agent->stop_cond);
        nimcp_mutex_free(agent->stats_mutex);

        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create: operation failed");
        return NULL;
    }

    /* Initialize behavioral module mutex (Phase 5.6: Dragonfly/Portia health monitoring) */
    agent->behavioral_mutex = nimcp_mutex_create(NULL);
    if (!agent->behavioral_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create behavioral mutex");
        nimcp_mutex_free(agent->neural_mutex);

        nimcp_mutex_free(agent->modules_mutex);

        nimcp_mutex_free(agent->cognitive_mutex);

        nimcp_cond_destroy(agent->stop_cond);
        nimcp_free(agent->stop_cond);
        nimcp_mutex_free(agent->stats_mutex);

        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create: operation failed");
        return NULL;
    }

    /* Initialize atomic state */
    atomic_init(&agent->running, false);
    atomic_init(&agent->stop_requested, false);
    atomic_init(&agent->uptime_start_us, 0);
    atomic_init(&agent->next_anomaly_id, 1);
    atomic_init(&agent->current_severity, HEALTH_SEVERITY_INFO);

    /* Initialize cognitive module pointers (all NULL by default) */
    agent->failure_predictor = NULL;
    agent->metacognition = NULL;
    agent->ethics = NULL;
    agent->emotion = NULL;
    agent->emotion_immune = NULL;
    agent->wellbeing = NULL;
    agent->mental_health = NULL;
    agent->collective = NULL;
    agent->rcog = NULL;
    agent->gpu_health = NULL;

    /* Initialize cognitive atomic stats */
    atomic_init(&agent->predictions_made, 0);
    atomic_init(&agent->predictions_correct, 0);
    atomic_init(&agent->preventive_actions, 0);
    atomic_init(&agent->self_diagnoses, 0);
    atomic_init(&agent->degradation_alerts, 0);
    atomic_init(&agent->current_confidence, 1.0f);
    atomic_init(&agent->ethics_evaluations, 0);
    atomic_init(&agent->ethics_blocks, 0);
    atomic_init(&agent->mercy_applications, 0);
    atomic_init(&agent->current_stress_level, 0.0f);
    atomic_init(&agent->emotion_adjustments, 0);
    atomic_init(&agent->distress_detections, 0);
    atomic_init(&agent->wellbeing_interventions, 0);
    atomic_init(&agent->current_distress_level, 0.0f);
    atomic_init(&agent->consensus_requests, 0);
    atomic_init(&agent->consensus_achieved, 0);
    atomic_init(&agent->avg_consensus_time_ms, 0.0f);
    atomic_init(&agent->rcog_diagnoses, 0);
    atomic_init(&agent->rcog_recovery_plans, 0);
    atomic_init(&agent->avg_rcog_time_ms, 0.0f);
    atomic_init(&agent->gpu_accelerated_checks, 0);
    atomic_init(&agent->gpu_utilization, 0.0f);
    atomic_init(&agent->gpu_healthy, true);

    /* Initialize hypothalamus module pointers */
    agent->hypothalamus = NULL;
    agent->hypo_bridge_id = 0;
    agent->homeostasis = NULL;
    agent->hypo_immune_bridge = NULL;
    agent->drives = NULL;
    memset(&agent->hypothalamus_config, 0, sizeof(agent->hypothalamus_config));

    /* Initialize hypothalamus atomic stats */
    atomic_init(&agent->in_stress_response, false);
    atomic_init(&agent->in_sickness_mode, false);
    atomic_init(&agent->stress_responses, 0);
    atomic_init(&agent->sickness_mode_entries, 0);
    atomic_init(&agent->drive_events_published, 0);
    atomic_init(&agent->homeostatic_output, 0.0f);

    /* Initialize additional module pointers */
    agent->connectivity = NULL;
    agent->oscillations = NULL;
    agent->gc_context = NULL;
    agent->checkpoint = NULL;
    agent->deadlock_detector_ptr = NULL;
    agent->bio_async_router = NULL;
    agent->bio_async_module_id = 0;
    agent->runtime_adaptation = NULL;
    agent->exception_bridge = NULL;
    agent->last_gc_time_us = 0;
    agent->last_checkpoint_time_us = 0;

    /* Initialize Portia/Dragonfly/Swarm/Memory module fields */
    agent->portia = NULL;
    agent->dragonfly = NULL;
    agent->swarm_immune = NULL;
    agent->swarm_memory = NULL;
    agent->engram = NULL;
    agent->memory_consolidation = NULL;

    atomic_init(&agent->portia_tier_changes, 0);
    atomic_init(&agent->portia_degradations, 0);
    atomic_init(&agent->dragonfly_anomalies_tracked, 0);
    atomic_init(&agent->dragonfly_pursuits, 0);
    atomic_init(&agent->dragonfly_interceptions, 0);
    atomic_init(&agent->dragonfly_current_target, 0);

    /* Initialize Behavioral Module (Dragonfly/Portia) immune integration (Phase 5.6) */
    agent->dragonfly_immune = NULL;
    agent->portia_monitor = NULL;
    atomic_init(&agent->dragonfly_immune_checks_run, 0);
    atomic_init(&agent->dragonfly_stress_events, 0);
    atomic_init(&agent->dragonfly_injury_events, 0);
    atomic_init(&agent->dragonfly_rest_triggers, 0);
    agent->last_dragonfly_immune_check_us = 0;
    atomic_init(&agent->portia_monitor_checks_run, 0);
    atomic_init(&agent->portia_thermal_warnings, 0);
    atomic_init(&agent->portia_power_warnings, 0);
    atomic_init(&agent->portia_coordination_actions, 0);
    agent->last_portia_monitor_check_us = 0;
    memset(&agent->dragonfly_immune_config, 0, sizeof(agent->dragonfly_immune_config));
    memset(&agent->portia_monitor_config, 0, sizeof(agent->portia_monitor_config));
    memset(&agent->behavioral_metrics, 0, sizeof(agent->behavioral_metrics));
    atomic_init(&agent->thermal_abort_active, false);
    atomic_init(&agent->power_conservation_active, false);
    atomic_init(&agent->rest_period_active, false);
    atomic_init(&agent->last_coordination_us, 0);

    atomic_init(&agent->swarm_threats_detected, 0);
    atomic_init(&agent->swarm_responses_generated, 0);
    atomic_init(&agent->swarm_coordinated_responses, 0);
    atomic_init(&agent->swarm_memories_stored, 0);
    atomic_init(&agent->swarm_replays_performed, 0);
    atomic_init(&agent->engram_encodings, 0);
    atomic_init(&agent->engram_recalls, 0);

    memset(&agent->connectivity_config, 0, sizeof(agent->connectivity_config));
    memset(&agent->oscillations_config, 0, sizeof(agent->oscillations_config));
    memset(&agent->gc_config, 0, sizeof(agent->gc_config));
    memset(&agent->checkpoint_config, 0, sizeof(agent->checkpoint_config));
    memset(&agent->bio_async_config, 0, sizeof(agent->bio_async_config));
    memset(&agent->exception_config, 0, sizeof(agent->exception_config));
    memset(&agent->portia_config, 0, sizeof(agent->portia_config));
    memset(&agent->dragonfly_config, 0, sizeof(agent->dragonfly_config));
    memset(&agent->swarm_immune_config, 0, sizeof(agent->swarm_immune_config));
    memset(&agent->swarm_memory_config, 0, sizeof(agent->swarm_memory_config));
    memset(&agent->engram_config, 0, sizeof(agent->engram_config));
    memset(&agent->consolidation_config, 0, sizeof(agent->consolidation_config));

    /* Initialize additional module atomic stats */
    atomic_init(&agent->gc_triggers, 0);
    atomic_init(&agent->checkpoints_created, 0);
    atomic_init(&agent->rollbacks_performed, 0);
    atomic_init(&agent->bio_async_events_published, 0);
    atomic_init(&agent->load_reductions, 0);
    atomic_init(&agent->load_reduced, false);

    /* Initialize heartbeat state */
    atomic_init(&agent->heartbeat.last_heartbeat_us, 0);
    atomic_init(&agent->heartbeat.missed_count, 0);
    atomic_init(&agent->heartbeat.current_progress, 0.0f);
    memset(agent->heartbeat.current_operation, 0, sizeof(agent->heartbeat.current_operation));

    /* Initialize State Consistency Manager (Phase 3) */
    agent->consistency_mutex = nimcp_mutex_create(NULL);
    if (!agent->consistency_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create consistency mutex");
        nimcp_mutex_free(agent->modules_mutex);

        nimcp_mutex_free(agent->cognitive_mutex);

        nimcp_cond_destroy(agent->stop_cond);
        nimcp_free(agent->stop_cond);
        nimcp_mutex_free(agent->stats_mutex);

        nimcp_mutex_free(agent->state_mutex);

        msg_queue_destroy(&agent->msg_queue);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
        return NULL;
    }

    memset(&agent->last_consistency_result, 0, sizeof(health_agent_consistency_result_t));
    atomic_init(&agent->consistency_check_pending, false);
    atomic_init(&agent->last_consistency_check_us, 0);
    atomic_init(&agent->consistency_checks_run, 0);
    atomic_init(&agent->consistency_failures_total, 0);

    /* Initialize registered struct tracking */
    for (uint32_t i = 0; i < 64; i++) {
        agent->registered_structs[i].ptr = NULL;
        agent->registered_structs[i].expected_magic = 0;
        agent->registered_structs[i].name[0] = '\0';
        agent->registered_structs[i].active = false;
    }
    agent->registered_struct_count = 0;

    /* Initialize statistics */
    memset(&agent->stats, 0, sizeof(health_agent_stats_t));
    agent->stats.highest_severity_seen = HEALTH_SEVERITY_INFO;

    nimcp_log(LOG_LEVEL_INFO, "Created health agent '%s' (id=%u, queue_capacity=%u)",
              agent->config.agent_name, agent->config.agent_id, queue_capacity);

    return agent;
}


void nimcp_health_agent_destroy(nimcp_health_agent_t* agent) {
    if (!agent) return;

    /* Validate agent structure */
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Cannot destroy invalid agent structure");
        return;
    }

    /* Stop agent if running */
    if (atomic_load(&agent->running)) {
        nimcp_health_agent_stop(agent);
    }

    nimcp_log(LOG_LEVEL_INFO, "Destroying health agent '%s'", agent->config.agent_name);

    /* Destroy message queue */
    msg_queue_destroy(&agent->msg_queue);

    /* Destroy condition variable and mutexes using NIMCP utilities */
    if (agent->stop_cond) {
        nimcp_cond_destroy(agent->stop_cond);
        nimcp_free(agent->stop_cond);
        agent->stop_cond = NULL;
    }

    if (agent->state_mutex) {
        nimcp_mutex_free(agent->state_mutex);

        agent->state_mutex = NULL;
    }

    if (agent->stats_mutex) {
        nimcp_mutex_free(agent->stats_mutex);

        agent->stats_mutex = NULL;
    }

    if (agent->cognitive_mutex) {
        nimcp_mutex_free(agent->cognitive_mutex);

        agent->cognitive_mutex = NULL;
    }

    if (agent->modules_mutex) {
        nimcp_mutex_free(agent->modules_mutex);

        agent->modules_mutex = NULL;
    }

    /* Destroy neural mutex (Phase 5.5: SNN/LNN health monitoring) */
    if (agent->neural_mutex) {
        nimcp_mutex_free(agent->neural_mutex);

        agent->neural_mutex = NULL;
    }

    /* Destroy behavioral mutex (Phase 5.6: Dragonfly/Portia health monitoring) */
    if (agent->behavioral_mutex) {
        nimcp_mutex_free(agent->behavioral_mutex);

        agent->behavioral_mutex = NULL;
    }

    /* Destroy consistency mutex (Phase 3) */
    if (agent->consistency_mutex) {
        nimcp_mutex_free(agent->consistency_mutex);

        agent->consistency_mutex = NULL;
    }

    /* Clear magic/canaries to prevent use-after-free */
    agent->magic = 0;
    agent->canary_front = 0;
    agent->canary_back = 0;

    /* Free agent */
    nimcp_free(agent);
}


/* ============================================================================
 * Message Helper Function
 * ============================================================================ */

health_agent_message_t nimcp_health_agent_create_message(
    health_agent_msg_type_t type,
    health_agent_severity_t severity,
    health_agent_source_t source,
    const char* description,
    ...
) {
    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = type;
    msg.severity = severity;
    msg.source = source;
    msg.suggested_action = HEALTH_RECOVERY_NONE;
    msg.timestamp_us = get_timestamp_us();
    msg.anomaly_id = 0;  /* Will be assigned when reported */

    /* Format description */
    if (description) {
        va_list args;
        va_start(args, description);
        vsnprintf(msg.description, sizeof(msg.description) - 1, description, args);
        va_end(args);
    }

    return msg;
}


int nimcp_health_agent_create_checkpoint(
    nimcp_health_agent_t* agent,
    const char* reason
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_create_checkpoint: validate_agent is NULL");
        return -1;
    }

    if (!agent->checkpoint) {
        nimcp_log(LOG_LEVEL_WARN, "No checkpoint manager connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create_checkpoint: agent->checkpoint is NULL");
        return -1;
    }

    /* Create checkpoint using brain's checkpoint system */
    if (agent->brain) {
        /* Generate checkpoint path with timestamp and reason */
        char checkpoint_path[NIMCP_SHORT_PATH_SIZE];
        uint64_t timestamp = get_timestamp_us();
        snprintf(checkpoint_path, sizeof(checkpoint_path),
                 "/tmp/nimcp_checkpoint_%lu_%s.ckpt",
                 (unsigned long)timestamp,
                 reason ? reason : "auto");

        bool success = checkpoint_save(agent->brain, checkpoint_path);
        if (!success) {
            nimcp_log(LOG_LEVEL_WARN, "Failed to create checkpoint: %s", checkpoint_path);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_create_checkpoint: success is NULL");
            return -1;
        }
    }

    agent->last_checkpoint_time_us = get_timestamp_us();
    atomic_fetch_add(&agent->checkpoints_created, 1);

    nimcp_log(LOG_LEVEL_INFO, "Created checkpoint: %s", reason ? reason : "auto");
    return 0;
}
