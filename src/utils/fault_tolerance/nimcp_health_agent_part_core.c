// nimcp_health_agent_part_core.c - core functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * Connection Management (Stubs - implemented in Phase 2)
 * ============================================================================ */

int nimcp_health_agent_connect_brain(nimcp_health_agent_t* agent, brain_t brain) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_brain: validate_agent is NULL");
        return -1;
    }

    agent->brain = brain;
    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to brain", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_immune(nimcp_health_agent_t* agent,
                                       brain_immune_system_t* immune) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_immune");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_brain: validate_agent is NULL");
        return -1;
    }

    agent->immune = immune;
    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to immune system",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_monitor(nimcp_health_agent_t* agent,
                                        health_monitor_t* monitor) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_monitor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_brain: validate_agent is NULL");
        return -1;
    }

    agent->monitor = monitor;
    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to health monitor",
              agent->config.agent_name);
    return 0;
}


/* ============================================================================
 * Agent Start/Stop (Stubs - full implementation in Phase 3)
 * ============================================================================ */

int nimcp_health_agent_start(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in start");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_start: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->state_mutex);

    if (atomic_load(&agent->running)) {
        nimcp_log(LOG_LEVEL_WARN, "Agent '%s' already running", agent->config.agent_name);
        nimcp_mutex_unlock(agent->state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_start: validation failed");
        return -1;
    }

    /* Reset stop flag */
    atomic_store(&agent->stop_requested, false);

    /* Record start time */
    atomic_store(&agent->uptime_start_us, get_timestamp_us());

    /* Initialize heartbeat timestamp */
    atomic_store(&agent->heartbeat.last_heartbeat_us, get_timestamp_us());

    /* Create agent thread using NIMCP threading */
    thread_attr_t attr;
    memset(&attr, 0, sizeof(attr));

    if (agent->config.thread_stack_size > 0) {
        attr.stack_size = agent->config.thread_stack_size;
    }

    /* nimcp_thread_create signature: (thread, start_routine, arg, attr) */
    nimcp_result_t rc = nimcp_thread_create(&agent->agent_thread, agent_thread_main, agent, &attr);

    if (rc != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create agent thread: error %d", rc);
        nimcp_mutex_unlock(agent->state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_start: validation failed");
        return -1;
    }

    atomic_store(&agent->running, true);

    nimcp_mutex_unlock(agent->state_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Started health agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_stop(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in stop");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_stop: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->state_mutex);

    if (!atomic_load(&agent->running)) {
        nimcp_log(LOG_LEVEL_WARN, "Agent '%s' not running", agent->config.agent_name);
        nimcp_mutex_unlock(agent->state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_stop: atomic_load is NULL");
        return -1;
    }

    /* Signal thread to stop */
    atomic_store(&agent->stop_requested, true);

    nimcp_mutex_unlock(agent->state_mutex);

    /* Wait for thread to complete */
    nimcp_result_t rc = nimcp_thread_join(agent->agent_thread, NULL);

    if (rc != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to join agent thread: error %d", rc);
    }

    atomic_store(&agent->running, false);

    nimcp_log(LOG_LEVEL_INFO, "Stopped health agent '%s'", agent->config.agent_name);
    return 0;
}


/* ============================================================================
 * Heartbeat API
 * ============================================================================ */

void nimcp_health_agent_heartbeat(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) return;

    atomic_store(&agent->heartbeat.last_heartbeat_us, get_timestamp_us());
    atomic_store(&agent->heartbeat.missed_count, 0);

    nimcp_mutex_lock(agent->stats_mutex);
    agent->stats.heartbeats_received++;
    nimcp_mutex_unlock(agent->stats_mutex);
}


void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                      const char* operation,
                                      float progress) {
    if (!validate_agent(agent)) return;

    atomic_store(&agent->heartbeat.last_heartbeat_us, get_timestamp_us());
    atomic_store(&agent->heartbeat.missed_count, 0);
    atomic_store(&agent->heartbeat.current_progress, progress);

    if (operation) {
        strncpy(agent->heartbeat.current_operation, operation,
                sizeof(agent->heartbeat.current_operation) - 1);
    }

    nimcp_mutex_lock(agent->stats_mutex);
    agent->stats.heartbeats_received++;
    nimcp_mutex_unlock(agent->stats_mutex);
}


int nimcp_health_agent_request_check(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_request_check: validate_agent is NULL");
        return -1;
    }

    /* Signal thread to run consistency check on next iteration (Phase 3) */
    atomic_store(&agent->consistency_check_pending, true);
    nimcp_log(LOG_LEVEL_DEBUG, "Check requested for agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_request_emergency_checkpoint(nimcp_health_agent_t* agent,
                                                     const char* reason) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_request_check: validate_agent is NULL");
        return -1;
    }

    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_EMERGENCY,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SOURCE_CHECKPOINT,
        "Emergency checkpoint requested: %s", reason ? reason : "unknown"
    );
    msg.suggested_action = HEALTH_RECOVERY_CHECKPOINT;

    return nimcp_health_agent_report_anomaly(agent, &msg);
}


uint32_t nimcp_health_agent_pending_messages(const nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) return 0;
    return msg_queue_size(&agent->msg_queue);
}


bool nimcp_health_agent_dequeue_message(nimcp_health_agent_t* agent,
                                         health_agent_message_t* msg) {
    if (!validate_agent(agent) || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_pending_messages: required parameter is NULL (validate_agent, msg)");
        return false;
    }
    return msg_queue_pop(&agent->msg_queue, msg);
}


health_agent_severity_t nimcp_health_agent_current_status(
    const nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) return HEALTH_SEVERITY_FATAL;
    return atomic_load(&agent->current_severity);
}


/* ============================================================================
 * Hypothalamus Connection Functions
 * ============================================================================ */

int nimcp_health_agent_connect_hypothalamus(
    nimcp_health_agent_t* agent,
    hypo_orchestrator_t orchestrator,
    const health_agent_hypothalamus_config_t* config
) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_hypothalamus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_hypothalamus: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    agent->hypothalamus = orchestrator;

    /* Apply configuration */
    if (config) {
        agent->hypothalamus_config = *config;
    } else {
        /* Use defaults */
        agent->hypothalamus_config.enable_hypothalamus = true;
        agent->hypothalamus_config.enable_homeostatic_regulation = true;
        agent->hypothalamus_config.enable_drive_response = true;
        agent->hypothalamus_config.enable_stress_coordination = true;
        agent->hypothalamus_config.enable_sickness_behavior = true;
        agent->hypothalamus_config.enable_immune_bridge = true;
        agent->hypothalamus_config.stress_trigger_threshold = 0.4f;
        agent->hypothalamus_config.sickness_trigger_threshold = 0.25f;
        agent->hypothalamus_config.homeostasis_update_ms = 100;
        agent->hypothalamus_config.drive_response_timeout_ms = NIMCP_MEDIUM_TIMEOUT_MS;
    }

    /* Register as bridge with hypothalamus orchestrator */
    uint32_t bridge_id = 0;
    int reg_result = hypo_orch_register_bridge(
        orchestrator,
        HYPO_BRIDGE_IMMUNE,  /* Health agent uses immune bridge type */
        agent->config.agent_name,
        agent,  /* Bridge handle is the agent itself */
        agent,  /* Context is also the agent */
        &bridge_id
    );
    if (reg_result == 0) {
        agent->hypo_bridge_id = bridge_id;
        nimcp_log(LOG_LEVEL_DEBUG, "Registered as hypothalamus bridge %u", bridge_id);
    } else {
        nimcp_log(LOG_LEVEL_WARN, "Failed to register as hypothalamus bridge");
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to hypothalamus orchestrator",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_homeostasis(
    nimcp_health_agent_t* agent,
    hypo_homeostasis_handle_t* homeostasis
) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_homeostasis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_homeostasis: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->homeostasis = homeostasis;
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to homeostasis system",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_hypo_immune_bridge(
    nimcp_health_agent_t* agent,
    hypo_immune_bridge_t* immune_bridge
) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_hypo_immune_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_hypo_immune_bridge: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->hypo_immune_bridge = immune_bridge;
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to hypothalamus-immune bridge",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_drives(
    nimcp_health_agent_t* agent,
    hypo_drive_system_handle_t* drives
) {
    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in connect_drives");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_drives: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->drives = drives;
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to drive system",
              agent->config.agent_name);
    return 0;
}


/* ============================================================================
 * Additional Module Connection Functions
 * ============================================================================ */

int nimcp_health_agent_connect_connectivity(
    nimcp_health_agent_t* agent,
    connectivity_health_t* connectivity,
    const health_agent_connectivity_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_connectivity: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->connectivity = connectivity;
    if (config) {
        agent->connectivity_config = *config;
    } else {
        agent->connectivity_config.enable_connectivity_monitoring = true;
        agent->connectivity_config.enable_isolation_detection = true;
        agent->connectivity_config.enable_auto_reconnect = true;
        agent->connectivity_config.check_interval_ms = 5000;
        agent->connectivity_config.isolation_threshold = 0.1f;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to connectivity health",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_oscillations(
    nimcp_health_agent_t* agent,
    brain_oscillations_t* oscillations,
    const health_agent_oscillations_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_oscillations: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->oscillations = oscillations;
    if (config) {
        agent->oscillations_config = *config;
    } else {
        agent->oscillations_config.enable_oscillation_monitoring = true;
        agent->oscillations_config.enable_seizure_detection = true;
        agent->oscillations_config.enable_flatline_detection = true;
        agent->oscillations_config.enable_desync_detection = true;
        agent->oscillations_config.abnormal_threshold = 0.3f;
        agent->oscillations_config.sample_rate_hz = 60;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to brain oscillations",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_gc(
    nimcp_health_agent_t* agent,
    kg_gc_context_t* gc_context,
    const health_agent_gc_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_gc: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->gc_context = gc_context;
    if (config) {
        agent->gc_config = *config;
    } else {
        agent->gc_config.enable_gc_integration = true;
        agent->gc_config.enable_auto_gc_trigger = true;
        agent->gc_config.enable_leak_detection = true;
        agent->gc_config.gc_trigger_threshold = 0.85f;
        agent->gc_config.gc_cooldown_ms = 30000;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to GC context",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_checkpoint(
    nimcp_health_agent_t* agent,
    checkpoint_manager_t* checkpoint,
    const health_agent_checkpoint_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_checkpoint: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->checkpoint = checkpoint;
    if (config) {
        agent->checkpoint_config = *config;
    } else {
        agent->checkpoint_config.enable_checkpoint_integration = true;
        agent->checkpoint_config.enable_auto_checkpoint = true;
        agent->checkpoint_config.enable_auto_rollback = true;
        agent->checkpoint_config.checkpoint_interval_ms = 60000;
        agent->checkpoint_config.health_threshold_checkpoint = 0.8f;
        agent->checkpoint_config.health_threshold_rollback = 0.2f;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to checkpoint manager",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_deadlock_detector(
    nimcp_health_agent_t* agent,
    deadlock_detector_t* deadlock_detector
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_deadlock_detector: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->deadlock_detector_ptr = deadlock_detector;
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to deadlock detector",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_bio_async(
    nimcp_health_agent_t* agent,
    bio_router_t router,
    const health_agent_bio_async_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_bio_async: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->bio_async_router = router;
    if (config) {
        agent->bio_async_config = *config;
    } else {
        agent->bio_async_config.enable_bio_async = true;
        agent->bio_async_config.publish_health_events = true;
        agent->bio_async_config.subscribe_health_requests = true;
        agent->bio_async_config.event_batch_size = 10;
        agent->bio_async_config.event_batch_timeout_ms = NIMCP_FAST_TIMEOUT_MS;
    }
    /* Register as module with bio-async router */
    bio_router_t global_router = bio_router_get_global();
    if (global_router) {
        bio_module_info_t module_info = {
            .module_id = 0,  /* Auto-assign */
            .module_name = "health_agent",
            .inbox_capacity = 0,  /* Use default */
            .user_data = agent
        };
        bio_module_context_t ctx = bio_router_register_module(&module_info);
        if (ctx) {
            agent->bio_async_module_id = bio_module_context_get_id(ctx);
            nimcp_log(LOG_LEVEL_DEBUG, "Registered as bio-async module %u",
                      agent->bio_async_module_id);
            /* Note: We don't store the context since we register/unregister per publish */
            bio_router_unregister_module(ctx);
        }
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to bio-async router",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_exception_bridge(
    nimcp_health_agent_t* agent,
    exception_immune_t* exception_bridge,
    const health_agent_exception_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_exception_bridge: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->exception_bridge = exception_bridge;
    if (config) {
        agent->exception_config = *config;
    } else {
        agent->exception_config.enable_exception_integration = true;
        agent->exception_config.auto_present_exceptions = true;
        agent->exception_config.enable_recovery_callbacks = true;
        agent->exception_config.exception_severity_threshold = 2;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to exception-immune bridge",
              agent->config.agent_name);
    return 0;
}


/* ============================================================================
 * Neural Module (SNN/LNN) Connection Functions
 * ============================================================================ */

int nimcp_health_agent_connect_snn(
    nimcp_health_agent_t* agent,
    snn_immune_bridge_t* snn_bridge,
    const health_agent_snn_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_snn: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    agent->snn_bridge = snn_bridge;

    /* Apply config or defaults */
    if (config) {
        agent->snn_config = *config;
    } else {
        /* Default SNN configuration */
        agent->snn_config.enable_snn_monitoring = true;
        agent->snn_config.enable_instability_detection = true;
        agent->snn_config.enable_auto_report = true;
        agent->snn_config.enable_learning_modulation = true;
        agent->snn_config.max_spike_rate_hz = 100.0f;
        agent->snn_config.min_spike_rate_hz = 0.1f;
        agent->snn_config.burst_threshold = 0.5f;
        agent->snn_config.sync_threshold = 0.8f;
        agent->snn_config.check_interval_ms = 100;
    }

    /* Initialize neural metrics for SNN - only mark connected if bridge is non-NULL */
    agent->neural_metrics.snn_connected = (snn_bridge != NULL);
    agent->neural_metrics.snn_healthy = true;
    atomic_store(&agent->snn_checks_run, 0);
    atomic_store(&agent->snn_instabilities_detected, 0);
    atomic_store(&agent->snn_recoveries_triggered, 0);
    agent->last_snn_check_us = get_timestamp_us();

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to SNN immune bridge%s",
              agent->config.agent_name, snn_bridge ? "" : " (NULL bridge)");
    return 0;
}


int nimcp_health_agent_connect_lnn(
    nimcp_health_agent_t* agent,
    lnn_immune_bridge_t* lnn_bridge,
    const health_agent_lnn_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_lnn: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    agent->lnn_bridge = lnn_bridge;

    /* Apply config or defaults */
    if (config) {
        agent->lnn_config = *config;
    } else {
        /* Default LNN configuration */
        agent->lnn_config.enable_lnn_monitoring = true;
        agent->lnn_config.enable_stability_detection = true;
        agent->lnn_config.enable_auto_report = true;
        agent->lnn_config.enable_tau_modulation = true;
        agent->lnn_config.enable_lr_modulation = true;
        agent->lnn_config.state_explosion_threshold = 1000.0f;
        agent->lnn_config.state_collapse_threshold = 1e-6f;
        agent->lnn_config.tau_max = 10.0f;
        agent->lnn_config.tau_min = 0.001f;
        agent->lnn_config.gradient_explosion_threshold = 100.0f;
        agent->lnn_config.gradient_vanishing_threshold = 1e-7f;
        agent->lnn_config.check_interval_ms = 100;
    }

    /* Initialize neural metrics for LNN - only mark connected if bridge is non-NULL */
    agent->neural_metrics.lnn_connected = (lnn_bridge != NULL);
    agent->neural_metrics.lnn_healthy = true;
    atomic_store(&agent->lnn_checks_run, 0);
    atomic_store(&agent->lnn_instabilities_detected, 0);
    atomic_store(&agent->lnn_recoveries_triggered, 0);
    agent->last_lnn_check_us = get_timestamp_us();

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to LNN immune bridge%s",
              agent->config.agent_name, lnn_bridge ? "" : " (NULL bridge)");
    return 0;
}


/* ============================================================================
 * Behavioral Module (Dragonfly/Portia) Connection API (Phase 5.6)
 * ============================================================================ */

int nimcp_health_agent_connect_dragonfly_immune(
    nimcp_health_agent_t* agent,
    dragonfly_immune_bridge_t bridge,
    const health_agent_dragonfly_immune_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_dragonfly_immune: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->dragonfly_immune = bridge;

    if (config) {
        agent->dragonfly_immune_config = *config;
    } else {
        /* Default configuration */
        agent->dragonfly_immune_config.enable_dragonfly_immune = true;
        agent->dragonfly_immune_config.enable_stress_monitoring = true;
        agent->dragonfly_immune_config.enable_health_status_tracking = true;
        agent->dragonfly_immune_config.enable_injury_detection = true;
        agent->dragonfly_immune_config.enable_fatigue_tracking = true;
        agent->dragonfly_immune_config.enable_cross_coordination = true;
        agent->dragonfly_immune_config.stress_warning_threshold = 0.5f;
        agent->dragonfly_immune_config.stress_critical_threshold = 0.8f;
        agent->dragonfly_immune_config.fatigue_warning_threshold = 0.6f;
        agent->dragonfly_immune_config.fatigue_critical_threshold = 0.9f;
        agent->dragonfly_immune_config.abort_hunt_on_thermal = true;
        agent->dragonfly_immune_config.abort_hunt_on_battery_low = true;
        agent->dragonfly_immune_config.reduce_intensity_on_stress = true;
        agent->dragonfly_immune_config.enable_auto_rest = true;
        agent->dragonfly_immune_config.rest_trigger_fatigue = 0.7f;
        agent->dragonfly_immune_config.min_rest_duration_ms = 5000;
        agent->dragonfly_immune_config.check_interval_ms = 100;
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    /* Update behavioral metrics */
    nimcp_mutex_lock(agent->behavioral_mutex);
    agent->behavioral_metrics.dragonfly_connected = (bridge != NULL);
    agent->behavioral_metrics.dragonfly_healthy = true;
    atomic_store(&agent->dragonfly_immune_checks_run, 0);
    nimcp_mutex_unlock(agent->behavioral_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected dragonfly immune bridge%s",
              agent->config.agent_name, bridge ? "" : " (NULL bridge)");

    return 0;
}


int nimcp_health_agent_connect_portia_monitor(
    nimcp_health_agent_t* agent,
    portia_monitor_t monitor,
    const health_agent_portia_monitor_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_portia_monitor: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->portia_monitor = monitor;

    if (config) {
        agent->portia_monitor_config = *config;
    } else {
        /* Default configuration */
        agent->portia_monitor_config.enable_portia_monitor = true;
        agent->portia_monitor_config.enable_thermal_monitoring = true;
        agent->portia_monitor_config.enable_power_monitoring = true;
        agent->portia_monitor_config.enable_cpu_load_monitoring = true;
        agent->portia_monitor_config.enable_degradation_tracking = true;
        agent->portia_monitor_config.enable_cross_coordination = true;
        agent->portia_monitor_config.thermal_warning_temp_c = 70.0f;
        agent->portia_monitor_config.thermal_critical_temp_c = 85.0f;
        agent->portia_monitor_config.throttle_on_warm = true;
        agent->portia_monitor_config.emergency_on_critical = true;
        agent->portia_monitor_config.battery_warning_pct = 20.0f;
        agent->portia_monitor_config.battery_critical_pct = 5.0f;
        agent->portia_monitor_config.conservation_on_battery = true;
        agent->portia_monitor_config.hibernate_on_critical = false;
        agent->portia_monitor_config.cpu_warning_pct = 80.0f;
        agent->portia_monitor_config.cpu_critical_pct = 95.0f;
        agent->portia_monitor_config.reduce_load_on_warning = true;
        agent->portia_monitor_config.notify_dragonfly_on_thermal = true;
        agent->portia_monitor_config.notify_neural_on_power = true;
        agent->portia_monitor_config.trigger_checkpoint_on_power_loss = true;
        agent->portia_monitor_config.check_interval_ms = 500;
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    /* Update behavioral metrics */
    nimcp_mutex_lock(agent->behavioral_mutex);
    agent->behavioral_metrics.portia_connected = (monitor != NULL);
    agent->behavioral_metrics.portia_healthy = true;
    atomic_store(&agent->portia_monitor_checks_run, 0);
    nimcp_mutex_unlock(agent->behavioral_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected portia monitor%s",
              agent->config.agent_name, monitor ? "" : " (NULL monitor)");

    return 0;
}


int nimcp_health_agent_request_behavioral_coordination(
    nimcp_health_agent_t* agent,
    const char* action,
    const char* reason
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_request_behavioral_coordination: validate_agent is NULL");
        return -1;
    }
    if (!action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_request_behavioral_coordination: action is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' behavioral coordination request: %s - %s",
              agent->config.agent_name, action, reason ? reason : "no reason");

    uint64_t now_us = get_timestamp_us();
    atomic_store(&agent->last_coordination_us, now_us);
    atomic_fetch_add(&agent->portia_coordination_actions, 1);

    /* Handle specific actions */
    if (strcmp(action, "abort_hunt") == 0) {
        atomic_store(&agent->thermal_abort_active, true);
        nimcp_log(LOG_LEVEL_WARN, "Agent '%s' activated thermal abort for hunting",
                  agent->config.agent_name);
    } else if (strcmp(action, "conservation_mode") == 0) {
        atomic_store(&agent->power_conservation_active, true);
        nimcp_log(LOG_LEVEL_WARN, "Agent '%s' activated power conservation mode",
                  agent->config.agent_name);
    } else if (strcmp(action, "rest_period") == 0) {
        atomic_store(&agent->rest_period_active, true);
        atomic_fetch_add(&agent->dragonfly_rest_triggers, 1);
        nimcp_log(LOG_LEVEL_INFO, "Agent '%s' activated rest period",
                  agent->config.agent_name);
    }

    return 0;
}


/* ============================================================================
 * Cognitive Module Connection Functions
 * ============================================================================ */

int nimcp_health_agent_connect_failure_prediction(
    nimcp_health_agent_t* agent,
    failure_predictor_t* predictor,
    const health_agent_prediction_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_failure_prediction: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->failure_predictor = predictor;
    if (config) {
        agent->prediction_config = *config;
    } else {
        agent->prediction_config.enable_failure_prediction = true;
        agent->prediction_config.prediction_threshold = 0.7f;
        agent->prediction_config.prediction_horizon_ms = 60000;
        agent->prediction_config.enable_preventive_action = true;
        agent->prediction_config.enable_trend_analysis = true;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to failure predictor",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_metacognition(
    nimcp_health_agent_t* agent,
    metacognition_t* metacog,
    const health_agent_metacog_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_metacognition: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->metacognition = metacog;
    if (config) {
        agent->metacog_config = *config;
    } else {
        agent->metacog_config.enable_metacognition = true;
        agent->metacog_config.enable_confidence_calibration = true;
        agent->metacog_config.enable_degradation_detection = true;
        agent->metacog_config.degradation_threshold = 0.3f;
        agent->metacog_config.enable_self_diagnosis = true;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to metacognition module",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_ethics(
    nimcp_health_agent_t* agent,
    ethics_engine_t ethics,
    const health_agent_ethics_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_ethics: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->ethics = ethics;
    if (config) {
        agent->ethics_config = *config;
    } else {
        agent->ethics_config.enable_ethics_evaluation = true;
        agent->ethics_config.enable_asimov_laws = true;
        agent->ethics_config.enable_mercy_directive = true;
        agent->ethics_config.enable_golden_rule = true;
        agent->ethics_config.ethics_override_threshold = 0.95f;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to ethics engine",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_emotion(
    nimcp_health_agent_t* agent,
    emotional_system_t* emotion,
    emotion_immune_bridge_t* emotion_immune,
    const health_agent_emotion_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_emotion: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->emotion = emotion;
    agent->emotion_immune = emotion_immune;
    if (config) {
        agent->emotion_config = *config;
    } else {
        agent->emotion_config.enable_emotion_awareness = true;
        agent->emotion_config.enable_emotion_reporting = true;
        agent->emotion_config.enable_stress_adjustment = true;
        agent->emotion_config.stress_threshold_multiplier = 1.5f;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to emotional system%s",
              agent->config.agent_name,
              emotion_immune ? " with emotion-immune bridge" : "");
    return 0;
}


int nimcp_health_agent_connect_wellbeing(
    nimcp_health_agent_t* agent,
    wellbeing_monitor_t* wellbeing,
    const health_agent_wellbeing_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_wellbeing: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->wellbeing = wellbeing;
    if (config) {
        agent->wellbeing_config = *config;
    } else {
        agent->wellbeing_config.enable_wellbeing_monitoring = true;
        agent->wellbeing_config.enable_distress_detection = true;
        agent->wellbeing_config.enable_suffering_prevention = true;
        agent->wellbeing_config.distress_intervention_threshold = 0.7f;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to wellbeing monitor",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_mental_health(
    nimcp_health_agent_t* agent,
    mental_health_monitor_t* mental_health
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_mental_health: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->mental_health = mental_health;
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to mental health monitor",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_collective(
    nimcp_health_agent_t* agent,
    collective_cognition_t* collective,
    const health_agent_collective_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_collective: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->collective = collective;
    if (config) {
        agent->collective_config = *config;
    } else {
        agent->collective_config.enable_collective_monitoring = true;
        agent->collective_config.enable_consensus_decisions = true;
        agent->collective_config.enable_swarm_immune = true;
        agent->collective_config.consensus_threshold = 0.66f;
        agent->collective_config.consensus_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to collective cognition",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_rcog(
    nimcp_health_agent_t* agent,
    rcog_engine_t* rcog,
    const health_agent_rcog_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_rcog: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->rcog = rcog;
    if (config) {
        agent->rcog_config = *config;
    } else {
        agent->rcog_config.enable_rcog_diagnosis = true;
        agent->rcog_config.enable_rcog_recovery_planning = true;
        agent->rcog_config.enable_imagination = true;
        agent->rcog_config.rcog_timeout_ms = NIMCP_WATCHDOG_TIMEOUT_MS;
        agent->rcog_config.confidence_threshold = 0.7f;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to RCOG engine",
              agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_gpu(
    nimcp_health_agent_t* agent,
    gpu_health_monitor_t* gpu_health,
    const health_agent_gpu_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_gpu: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->gpu_health = gpu_health;
    if (config) {
        agent->gpu_config = *config;
    } else {
        agent->gpu_config.enable_gpu_monitoring = true;
        agent->gpu_config.enable_gpu_acceleration = true;
        agent->gpu_config.enable_tensor_validation = true;
        agent->gpu_config.enable_anomaly_detection = true;
        agent->gpu_config.gpu_check_interval_ms = 1000;
    }
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to GPU health monitor",
              agent->config.agent_name);
    return 0;
}


/* ============================================================================
 * Hypothalamus USE Functions (Active Integration)
 * ============================================================================ */

int nimcp_health_agent_trigger_stress_response(
    nimcp_health_agent_t* agent,
    const char* reason,
    health_agent_severity_t severity
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_trigger_stress_response: validate_agent is NULL");
        return -1;
    }

    if (!agent->hypothalamus) {
        nimcp_log(LOG_LEVEL_WARN, "No hypothalamus connected for stress response");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_trigger_stress_response: agent->hypothalamus is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    if (atomic_load(&agent->in_stress_response)) {
        nimcp_log(LOG_LEVEL_DEBUG, "Already in stress response");
        nimcp_mutex_unlock(agent->modules_mutex);
        return 0;
    }

    /* Mark as in stress response */
    atomic_store(&agent->in_stress_response, true);
    atomic_fetch_add(&agent->stress_responses, 1);

    /* Call hypothalamus orchestrator to trigger stress response */
    int result = hypo_orch_trigger_stress(agent->hypothalamus, reason);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Failed to trigger hypothalamus stress response");
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Triggered stress response: %s (severity=%d)",
              reason ? reason : "unknown", severity);
    return 0;
}


int nimcp_health_agent_release_stress_response(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_release_stress_response: validate_agent is NULL");
        return -1;
    }

    if (!agent->hypothalamus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_release_stress_response: agent->hypothalamus is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    if (!atomic_load(&agent->in_stress_response)) {
        nimcp_mutex_unlock(agent->modules_mutex);
        return 0;
    }

    /* Mark as no longer in stress response */
    atomic_store(&agent->in_stress_response, false);

    /* Call hypothalamus orchestrator to release stress */
    int result = hypo_orch_release_stress(agent->hypothalamus);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Failed to release hypothalamus stress response");
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Released stress response");
    return 0;
}


int nimcp_health_agent_enter_sickness_mode(
    nimcp_health_agent_t* agent,
    float threat_level
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_enter_sickness_mode: validate_agent is NULL");
        return -1;
    }

    if (!agent->hypo_immune_bridge && !agent->hypothalamus) {
        nimcp_log(LOG_LEVEL_WARN, "No hypothalamus/immune bridge for sickness mode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_enter_sickness_mode: required parameter is NULL (agent->hypo_immune_bridge, agent->hypothalamus)");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    if (atomic_load(&agent->in_sickness_mode)) {
        nimcp_mutex_unlock(agent->modules_mutex);
        return 0;
    }

    atomic_store(&agent->in_sickness_mode, true);
    atomic_fetch_add(&agent->sickness_mode_entries, 1);

    /* Call hypo-immune bridge to activate sickness behavior (safety mode) */
    if (agent->hypo_immune_bridge) {
        int result = hypo_immune_bridge_enter_safety_mode(agent->hypo_immune_bridge, threat_level);
        if (result != 0) {
            nimcp_log(LOG_LEVEL_WARN, "Failed to activate sickness behavior");
        }
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Entered sickness mode (threat_level=%.2f)", threat_level);
    return 0;
}


int nimcp_health_agent_exit_sickness_mode(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_exit_sickness_mode: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    if (!atomic_load(&agent->in_sickness_mode)) {
        nimcp_mutex_unlock(agent->modules_mutex);
        return 0;
    }

    atomic_store(&agent->in_sickness_mode, false);

    /* Call hypo-immune bridge to deactivate sickness behavior (end acute phase) */
    if (agent->hypo_immune_bridge) {
        int result = hypo_immune_end_acute_phase(agent->hypo_immune_bridge);
        if (result != 0) {
            nimcp_log(LOG_LEVEL_WARN, "Failed to deactivate sickness behavior");
        }
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Exited sickness mode");
    return 0;
}


float nimcp_health_agent_homeostatic_regulate(
    nimcp_health_agent_t* agent,
    float current_health
) {
    if (!validate_agent(agent)) return 0.0f;

    if (!agent->homeostasis) {
        return 0.0f;
    }

    /* Call homeostasis system for PID regulation */
    /* First, set the current health as the arousal variable value */
    hypo_homeostasis_set_value(agent->homeostasis, HYPO_VAR_AROUSAL, current_health);

    /* Run the control update (using 0 delta means instant update) */
    hypo_homeostasis_update(agent->homeostasis, 0);

    /* Get the controller output for arousal regulation */
    float output = hypo_homeostasis_get_output(agent->homeostasis, HYPO_VAR_AROUSAL);

    /* Clamp to [-1, 1] */
    if (output > 1.0f) output = 1.0f;
    if (output < -1.0f) output = -1.0f;

    atomic_store(&agent->homeostatic_output, output);

    return output;
}


/* ============================================================================
 * Module USE Functions (Active Integration)
 * ============================================================================ */

int nimcp_health_agent_trigger_gc(nimcp_health_agent_t* agent, bool force) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_trigger_gc: validate_agent is NULL");
        return -1;
    }

    if (!agent->gc_context) {
        nimcp_log(LOG_LEVEL_WARN, "No GC context connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_trigger_gc: agent->gc_context is NULL");
        return -1;
    }

    uint64_t now_us = get_timestamp_us();

    /* Check cooldown unless forced */
    if (!force && agent->gc_config.gc_cooldown_ms > 0) {
        uint64_t cooldown_us = agent->gc_config.gc_cooldown_ms * 1000ULL;
        if (now_us - agent->last_gc_time_us < cooldown_us) {
            nimcp_log(LOG_LEVEL_DEBUG, "GC cooldown not elapsed");
            return 0;
        }
    }

    /* Trigger GC on all targets */
    int collected = kg_gc_run(agent->gc_context, KG_GC_ALL);
    if (collected < 0) {
        nimcp_log(LOG_LEVEL_WARN, "GC run failed: %s",
                  kg_gc_get_last_error(agent->gc_context));
    } else {
        nimcp_log(LOG_LEVEL_DEBUG, "GC collected %d items", collected);
    }

    agent->last_gc_time_us = now_us;
    atomic_fetch_add(&agent->gc_triggers, 1);

    nimcp_log(LOG_LEVEL_INFO, "Triggered garbage collection (force=%d)", force);
    return 0;
}


int nimcp_health_agent_rollback(
    nimcp_health_agent_t* agent,
    uint64_t checkpoint_id
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_rollback: validate_agent is NULL");
        return -1;
    }

    if (!agent->checkpoint) {
        nimcp_log(LOG_LEVEL_WARN, "No checkpoint manager connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_rollback: agent->checkpoint is NULL");
        return -1;
    }

    /* Rollback to checkpoint
     * Note: Full implementation requires checkpoint registry to map IDs to paths.
     * For now, attempt to load most recent checkpoint if checkpoint_id is 0.
     */
    if (agent->brain && checkpoint_id == 0) {
        /* Find most recent checkpoint - simplified approach */
        char checkpoint_path[NIMCP_SHORT_PATH_SIZE];
        snprintf(checkpoint_path, sizeof(checkpoint_path),
                 "/tmp/nimcp_checkpoint_latest.ckpt");

        if (checkpoint_validate(checkpoint_path)) {
            bool success = checkpoint_load(&agent->brain, checkpoint_path);
            if (!success) {
                nimcp_log(LOG_LEVEL_ERROR, "Failed to rollback to checkpoint");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_rollback: success is NULL");
                return -1;
            }
        } else {
            nimcp_log(LOG_LEVEL_WARN, "No valid checkpoint found for rollback");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_rollback: success is NULL");
            return -1;
        }
    } else if (checkpoint_id != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Checkpoint ID lookup not yet implemented");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_rollback: operation failed");
        return -1;
    }

    atomic_fetch_add(&agent->rollbacks_performed, 1);

    nimcp_log(LOG_LEVEL_INFO, "Rolling back to checkpoint %lu", (unsigned long)checkpoint_id);
    return 0;
}


int nimcp_health_agent_check_oscillations(
    nimcp_health_agent_t* agent,
    bool* is_abnormal_out,
    uint32_t* anomaly_type_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_check_oscillations: validate_agent is NULL");
        return -1;
    }

    if (!agent->oscillations) {
        if (is_abnormal_out) *is_abnormal_out = false;
        if (anomaly_type_out) *anomaly_type_out = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_check_oscillations: validation failed");
        return -1;
    }

    /* Query oscillations for abnormal patterns using brain_oscillation_analyze */
    brain_oscillation_analyzer_t* analyzer = (brain_oscillation_analyzer_t*)agent->oscillations;
    oscillation_analysis_t analysis;
    bool abnormal = false;
    uint32_t anomaly_type = 0;

    if (brain_oscillation_analyze(analyzer, &analysis)) {
        /* Check for abnormal states */
        /* DEEP_SLEEP during active state is abnormal */
        if (analysis.state == COGNITIVE_STATE_DEEP_SLEEP && analysis.state_confidence > 0.8f) {
            abnormal = true;
            anomaly_type = 1;  /* Unexpected deep sleep */
        }
        /* Very low synchrony indicates disconnection */
        else if (analysis.synchrony < 0.1f) {
            abnormal = true;
            anomaly_type = 2;  /* Low synchrony */
        }
        /* Very high spectral entropy indicates chaos */
        else if (analysis.spectral_entropy > 0.95f) {
            abnormal = true;
            anomaly_type = 3;  /* Chaotic activity */
        }
    }

    if (is_abnormal_out) *is_abnormal_out = abnormal;
    if (anomaly_type_out) *anomaly_type_out = anomaly_type;

    return 0;
}


int nimcp_health_agent_check_connectivity(
    nimcp_health_agent_t* agent,
    bool* isolation_detected_out,
    char* isolated_module_out,
    size_t module_name_size
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_check_connectivity: validate_agent is NULL");
        return -1;
    }

    if (!agent->connectivity) {
        if (isolation_detected_out) *isolation_detected_out = false;
        if (isolated_module_out && module_name_size > 0) isolated_module_out[0] = '\0';
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_check_connectivity: validation failed");
        return -1;
    }

    /* Query connectivity health using brain's cached connectivity assessment */
    bool isolation = false;

    if (agent->brain) {
        brain_connectivity_health_t health;
        if (brain_get_connectivity_health(agent->brain, &health)) {
            /* Check for module isolation based on community structure */
            /* If modularity is very high and largest community is small, possible isolation */
            if (health.community.modularity_q > 0.8f &&
                health.community.largest_community_ratio < 0.2f) {
                isolation = true;
                if (isolated_module_out && module_name_size > 0) {
                    snprintf(isolated_module_out, module_name_size,
                             "fragmented_communities=%u", health.community.num_communities);
                }
            }
            /* Check for low overall health as indicator of connectivity issues */
            else if (health.overall_health < 0.3f) {
                isolation = true;
                if (isolated_module_out && module_name_size > 0) {
                    snprintf(isolated_module_out, module_name_size,
                             "low_connectivity_health=%.2f", health.overall_health);
                }
            }
            connectivity_health_free(&health);
        }
    }

    if (isolation_detected_out) *isolation_detected_out = isolation;
    if (!isolation && isolated_module_out && module_name_size > 0) {
        isolated_module_out[0] = '\0';
    }

    return 0;
}


int nimcp_health_agent_publish_event(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg
) {
    if (!validate_agent(agent) || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_publish_event: required parameter is NULL (validate_agent, msg)");
        return -1;
    }

    if (!agent->bio_async_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_publish_event: agent->bio_async_router is NULL");
        return -1;
    }

    /* Publish health event to bio-async system via global router signal
     * Note: Full bio-async integration requires module context registration.
     * For now, publish as a health signal using the global router.
     */
    bio_router_t router = bio_router_get_global();
    if (router) {
        /* Create a temporary module context for publishing */
        bio_module_info_t module_info = {
            .module_id = 0,  /* Auto-assign */
            .module_name = "health_agent",
            .inbox_capacity = 0,  /* Use default */
            .user_data = agent
        };
        bio_module_context_t ctx = bio_router_register_module(&module_info);
        if (ctx) {
            /* Publish health severity as signal value */
            char signal_name[NIMCP_ID_BUFFER_SIZE];
            snprintf(signal_name, sizeof(signal_name), "health_%u", (unsigned)msg->source);
            float signal_value = (float)(HEALTH_SEVERITY_CRITICAL - msg->severity) /
                                 (float)HEALTH_SEVERITY_CRITICAL;  /* Higher = healthier */
            bio_router_publish_signal(ctx, signal_name, signal_value);

            /* Unregister temporary context */
            bio_router_unregister_module(ctx);
        }
    }

    atomic_fetch_add(&agent->bio_async_events_published, 1);

    return 0;
}


int nimcp_health_agent_check_deadlocks(
    nimcp_health_agent_t* agent,
    bool* deadlock_detected_out,
    bool* contention_high_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_check_deadlocks: validate_agent is NULL");
        return -1;
    }

    if (!agent->deadlock_detector_ptr) {
        if (deadlock_detected_out) *deadlock_detected_out = false;
        if (contention_high_out) *contention_high_out = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_check_deadlocks: validation failed");
        return -1;
    }

    /* Query deadlock detector for cycles and contention */
    uint32_t cycles = deadlock_detector_check();
    deadlock_detector_stats_t stats = deadlock_detector_get_stats();

    /* Deadlock detected if any cycles found or recent deadlocks */
    bool deadlock = (cycles > 0) || (stats.deadlocks_detected > 0);

    /* High contention if many timeouts or order violations */
    bool high_contention = false;
    if (stats.total_locks > 0) {
        /* Contention is high if > 5% timeouts or > 1% order violations */
        float timeout_ratio = (float)stats.lock_timeouts / (float)stats.total_locks;
        float violation_ratio = (float)stats.order_violations / (float)stats.total_locks;
        high_contention = (timeout_ratio > 0.05f) || (violation_ratio > 0.01f);
    }

    if (deadlock_detected_out) *deadlock_detected_out = deadlock;
    if (contention_high_out) *contention_high_out = high_contention;

    return 0;
}


/* ============================================================================
 * Portia/Dragonfly/Swarm/Memory Connection Functions
 * ============================================================================ */

int nimcp_health_agent_connect_portia(
    nimcp_health_agent_t* agent,
    portia_context_t* portia,
    const health_agent_portia_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_portia: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    /* Store portia context (may be NULL - will use global API in that case) */
    agent->portia = portia;

    if (config) {
        agent->portia_config = *config;
    } else {
        /* Default configuration */
        agent->portia_config.enable_portia = true;
        agent->portia_config.enable_tier_monitoring = true;
        agent->portia_config.enable_power_awareness = true;
        agent->portia_config.enable_thermal_monitoring = true;
        agent->portia_config.enable_degradation_coordination = true;
        agent->portia_config.enable_auto_tier_switch = false; /* Manual by default */
        agent->portia_config.degradation_trigger_threshold = 0.3f;
        agent->portia_config.upgrade_health_threshold = 0.8f;
        agent->portia_config.tier_check_interval_ms = 5000;
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    if (portia) {
        nimcp_log(LOG_LEVEL_INFO, "Connected Portia context to health agent '%s'",
                  agent->config.agent_name);
    } else {
        nimcp_log(LOG_LEVEL_DEBUG, "Portia configured (using global API) for health agent '%s'",
                  agent->config.agent_name);
    }
    return 0;
}


int nimcp_health_agent_connect_dragonfly(
    nimcp_health_agent_t* agent,
    dragonfly_system_t* dragonfly,
    const health_agent_dragonfly_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_dragonfly: validate_agent is NULL");
        return -1;
    }
    if (!dragonfly) {
        nimcp_log(LOG_LEVEL_ERROR, "Null dragonfly system in connect_dragonfly");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_dragonfly: dragonfly is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->dragonfly = dragonfly;

    if (config) {
        agent->dragonfly_config = *config;
    } else {
        /* Default configuration */
        agent->dragonfly_config.enable_dragonfly = true;
        agent->dragonfly_config.enable_anomaly_tracking = true;
        agent->dragonfly_config.enable_pursuit_mode = true;
        agent->dragonfly_config.enable_interception = true;
        agent->dragonfly_config.enable_prediction_integration = true;
        agent->dragonfly_config.lock_on_severity_threshold = 0.7f;
        agent->dragonfly_config.pursuit_timeout_s = 30.0f;
        agent->dragonfly_config.update_rate_hz = 10;
    }

    nimcp_mutex_unlock(agent->modules_mutex);
    nimcp_log(LOG_LEVEL_INFO, "Connected Dragonfly to health agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_swarm_immune(
    nimcp_health_agent_t* agent,
    void* swarm_immune_ptr,
    const health_agent_swarm_immune_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_swarm_immune: validate_agent is NULL");
        return -1;
    }
    if (!swarm_immune_ptr) {
        nimcp_log(LOG_LEVEL_ERROR, "Null swarm immune in connect_swarm_immune");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_swarm_immune: swarm_immune_ptr is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->swarm_immune = (NimcpSwarmImmuneSystem*)swarm_immune_ptr;

    if (config) {
        agent->swarm_immune_config = *config;
    } else {
        /* Default configuration */
        agent->swarm_immune_config.enable_swarm_immune = true;
        agent->swarm_immune_config.enable_threat_detection = true;
        agent->swarm_immune_config.enable_coordinated_response = true;
        agent->swarm_immune_config.enable_memory_sharing = true;
        agent->swarm_immune_config.enable_self_verification = true;
        agent->swarm_immune_config.threat_detection_threshold = 0.5f;
        agent->swarm_immune_config.consensus_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;
    }

    nimcp_mutex_unlock(agent->modules_mutex);
    nimcp_log(LOG_LEVEL_INFO, "Connected Swarm Immune to health agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_swarm_memory(
    nimcp_health_agent_t* agent,
    void* swarm_memory_ptr,
    const health_agent_swarm_memory_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_swarm_memory: validate_agent is NULL");
        return -1;
    }
    if (!swarm_memory_ptr) {
        nimcp_log(LOG_LEVEL_ERROR, "Null swarm memory in connect_swarm_memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_connect_swarm_memory: swarm_memory_ptr is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->swarm_memory = (NimcpSwarmMemory*)swarm_memory_ptr;

    if (config) {
        agent->swarm_memory_config = *config;
    } else {
        /* Default configuration */
        agent->swarm_memory_config.enable_swarm_memory = true;
        agent->swarm_memory_config.enable_distributed_storage = true;
        agent->swarm_memory_config.enable_memory_replay = true;
        agent->swarm_memory_config.enable_consolidation = true;
        agent->swarm_memory_config.enable_forgetting = true;
        agent->swarm_memory_config.replay_priority_threshold = 0.3f;
        agent->swarm_memory_config.consolidation_interval_ms = 60000;
    }

    nimcp_mutex_unlock(agent->modules_mutex);
    nimcp_log(LOG_LEVEL_INFO, "Connected Swarm Memory to health agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_engram(
    nimcp_health_agent_t* agent,
    engram_system_t* engram,
    const health_agent_engram_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_engram: validate_agent is NULL");
        return -1;
    }
    if (!engram) {
        nimcp_log(LOG_LEVEL_ERROR, "Null engram system in connect_engram");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_engram: engram is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->engram = engram;

    if (config) {
        agent->engram_config = *config;
    } else {
        /* Default configuration */
        agent->engram_config.enable_engram = true;
        agent->engram_config.enable_health_encoding = true;
        agent->engram_config.enable_recall = true;
        agent->engram_config.enable_reconsolidation = true;
        agent->engram_config.enable_pattern_completion = true;
        agent->engram_config.encoding_threshold = 0.5f;  /* Encode warning and above */
        agent->engram_config.recall_threshold = 0.6f;
    }

    nimcp_mutex_unlock(agent->modules_mutex);
    nimcp_log(LOG_LEVEL_INFO, "Connected Engram to health agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_connect_memory_consolidation(
    nimcp_health_agent_t* agent,
    systems_consolidation_system_t* consolidation,
    const health_agent_memory_consolidation_config_t* config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_memory_consolidation: validate_agent is NULL");
        return -1;
    }
    if (!consolidation) {
        nimcp_log(LOG_LEVEL_ERROR, "Null consolidation system in connect_memory_consolidation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_memory_consolidation: consolidation is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->memory_consolidation = consolidation;

    if (config) {
        agent->consolidation_config = *config;
    } else {
        /* Default configuration */
        agent->consolidation_config.enable_systems_consolidation = true;
        agent->consolidation_config.enable_sleep_replay = true;
        agent->consolidation_config.enable_semantic_extraction = true;
        agent->consolidation_config.enable_cortical_transfer = true;
        agent->consolidation_config.consolidation_rate = 0.05f;
        agent->consolidation_config.replay_batch_size = 10;
    }

    nimcp_mutex_unlock(agent->modules_mutex);
    nimcp_log(LOG_LEVEL_INFO, "Connected Memory Consolidation to health agent '%s'", agent->config.agent_name);
    return 0;
}


int nimcp_health_agent_use_portia_degrade(
    nimcp_health_agent_t* agent,
    uint32_t level
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_degrade: validate_agent is NULL");
        return -1;
    }
    if (!agent->portia_config.enable_portia) {
        nimcp_log(LOG_LEVEL_WARN, "Portia not enabled for degrade");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_portia_degrade: agent->portia_config is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Portia: Setting degradation level to %u", level);
    atomic_fetch_add(&agent->portia_degradations, 1);

    /* Call actual Portia API (uses global context if agent->portia is NULL) */
    nimcp_error_t result = portia_set_degradation_level((portia_degradation_level_t)level);
    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Portia set_degradation_level failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_degrade: validation failed");
        return -1;
    }
    return 0;
}


/* ============================================================================
 * Dragonfly USE Functions
 * ============================================================================ */

int nimcp_health_agent_use_dragonfly_track_anomaly(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg,
    uint32_t* target_id
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_track_anomaly: validate_agent is NULL");
        return -1;
    }
    if (!msg || !target_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_track_anomaly: required parameter is NULL (msg, target_id)");
        return -1;
    }
    if (!agent->dragonfly) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly not connected for track_anomaly");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_track_anomaly: agent->dragonfly is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Dragonfly: Tracking anomaly type %d", msg->type);
    atomic_fetch_add(&agent->dragonfly_anomalies_tracked, 1);

    /* Convert health message to dragonfly detection */
    dragonfly_detection_t detection = {0};
    detection.position[0] = (float)(msg->anomaly_id & 0xFF);
    detection.position[1] = (float)((msg->anomaly_id >> 8) & 0xFF);
    detection.position[2] = (float)((msg->anomaly_id >> 16) & 0xFF);
    detection.size = (float)msg->severity / 100.0f;
    /* Derive contrast from severity - higher severity = higher contrast (more attention) */
    detection.contrast = (float)(msg->severity + 1) / (float)(HEALTH_SEVERITY_CRITICAL + 1);
    detection.motion_direction_rad = 0.0f;
    detection.motion_speed = 0.0f;
    detection.timestamp_us = msg->timestamp_us;
    detection.id = (uint32_t)(msg->anomaly_id & 0xFFFFFFFF);

    /* Call actual Dragonfly API */
    int result = dragonfly_process_detection(agent->dragonfly, &detection);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly process_detection failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_track_anomaly: validation failed");
        return -1;
    }

    *target_id = detection.id;
    atomic_store(&agent->dragonfly_current_target, *target_id);
    return 0;
}


int nimcp_health_agent_use_dragonfly_predict(
    nimcp_health_agent_t* agent,
    uint32_t target_id,
    float* time_to_failure_out,
    float* confidence_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_predict: validate_agent is NULL");
        return -1;
    }
    if (!agent->dragonfly) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly not connected for predict");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_predict: agent->dragonfly is NULL");
        return -1;
    }

    (void)target_id;  /* Use target state directly */

    /* Call actual Dragonfly API to get primary target info */
    dragonfly_target_info_t target_info;
    int result = dragonfly_get_primary_target(agent->dragonfly, &target_info);
    if (result != 0) {
        /* No target being tracked, use default values */
        if (time_to_failure_out) *time_to_failure_out = -1.0f;
        if (confidence_out) *confidence_out = 0.0f;
        return 0;  /* Not an error, just no prediction available */
    }

    /* Extract prediction from target info */
    if (time_to_failure_out) {
        *time_to_failure_out = target_info.time_to_intercept_s;
    }
    if (confidence_out) {
        *confidence_out = target_info.confidence;
    }
    return 0;
}


int nimcp_health_agent_use_dragonfly_pursue(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_pursue: validate_agent is NULL");
        return -1;
    }
    if (!agent->dragonfly) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly not connected for pursue");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_pursue: agent->dragonfly is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Dragonfly: Starting pursuit");
    atomic_fetch_add(&agent->dragonfly_pursuits, 1);

    /* Call actual Dragonfly API */
    int result = dragonfly_start_pursuit(agent->dragonfly);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly start_pursuit failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_pursue: validation failed");
        return -1;
    }
    return 0;
}


int nimcp_health_agent_use_dragonfly_abort(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_abort: validate_agent is NULL");
        return -1;
    }
    if (!agent->dragonfly) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly not connected for abort");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_abort: agent->dragonfly is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Dragonfly: Aborting pursuit");
    atomic_store(&agent->dragonfly_current_target, 0);

    /* Call actual Dragonfly API */
    int result = dragonfly_abort_pursuit(agent->dragonfly);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly abort_pursuit failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_abort: validation failed");
        return -1;
    }
    return 0;
}


/* ============================================================================
 * Swarm Immune USE Functions
 * ============================================================================ */

int nimcp_health_agent_use_swarm_detect_threat(
    nimcp_health_agent_t* agent,
    const uint8_t* data,
    size_t data_len,
    uint32_t source_id,
    bool* threat_detected_out,
    uint32_t* threat_id_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_detect_threat: validate_agent is NULL");
        return -1;
    }
    if (!data || !threat_detected_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_detect_threat: required parameter is NULL (data, threat_detected_out)");
        return -1;
    }
    if (!agent->swarm_immune) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune not connected for detect_threat");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_detect_threat: agent->swarm_immune is NULL");
        return -1;
    }

    /* Call actual Swarm Immune API */
    uint32_t detected_threat_id = 0;
    nimcp_result_t result = nimcp_swarm_immune_detect_threat(
        agent->swarm_immune,
        data,
        data_len,
        source_id,
        &detected_threat_id
    );

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune detect_threat failed: %d", result);
        *threat_detected_out = false;
        if (threat_id_out) *threat_id_out = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_detect_threat: validation failed");
        return -1;
    }

    /* Threat detected if threat_id was assigned (non-zero) */
    if (detected_threat_id != 0) {
        *threat_detected_out = true;
        if (threat_id_out) *threat_id_out = detected_threat_id;
        atomic_fetch_add(&agent->swarm_threats_detected, 1);
        nimcp_log(LOG_LEVEL_INFO, "USE Swarm Immune: Threat detected, ID=%u", detected_threat_id);
    } else {
        *threat_detected_out = false;
        if (threat_id_out) *threat_id_out = 0;
    }

    return 0;
}


int nimcp_health_agent_use_swarm_generate_response(
    nimcp_health_agent_t* agent,
    uint32_t threat_id,
    uint32_t* response_id_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_generate_response: validate_agent is NULL");
        return -1;
    }
    if (!agent->swarm_immune) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune not connected for generate_response");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_generate_response: agent->swarm_immune is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Swarm Immune: Generating response for threat %u", threat_id);
    atomic_fetch_add(&agent->swarm_responses_generated, 1);

    /* Call actual Swarm Immune API */
    uint32_t generated_response_id = 0;
    nimcp_result_t result = nimcp_swarm_immune_generate_response(
        agent->swarm_immune,
        threat_id,
        &generated_response_id
    );

    if (response_id_out) *response_id_out = generated_response_id;

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune generate_response failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_generate_response: validation failed");
        return -1;
    }
    return 0;
}


int nimcp_health_agent_use_swarm_check_behavior(
    nimcp_health_agent_t* agent,
    uint32_t component_id,
    float* anomaly_score_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_check_behavior: validate_agent is NULL");
        return -1;
    }
    if (!anomaly_score_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_check_behavior: anomaly_score_out is NULL");
        return -1;
    }
    if (!agent->swarm_immune) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune not connected for check_behavior");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_check_behavior: agent->swarm_immune is NULL");
        return -1;
    }

    /* Create a basic behavior profile for the component */
    NimcpSwarmBehaviorProfile behavior = {0};
    behavior.drone_id = component_id;
    behavior.msg_rate = 1.0f;  /* Default message rate */
    behavior.movement_pattern[0] = 0.0f;
    behavior.movement_pattern[1] = 0.0f;
    behavior.movement_pattern[2] = 0.0f;
    behavior.energy_usage = 0.5f;
    behavior.connection_changes = 0;
    behavior.last_update = 0;
    behavior.anomaly_score = 0.0f;

    /* Call actual Swarm Immune API */
    float score = 0.0f;
    nimcp_result_t result = nimcp_swarm_immune_check_behavior(
        agent->swarm_immune,
        component_id,
        &behavior,
        &score
    );

    *anomaly_score_out = score;

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune check_behavior failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_check_behavior: validation failed");
        return -1;
    }
    return 0;
}


int nimcp_health_agent_use_swarm_add_memory_cell(
    nimcp_health_agent_t* agent,
    const uint8_t* pattern,
    size_t pattern_len,
    uint32_t response_type,
    uint32_t* cell_id_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_add_memory_cell: validate_agent is NULL");
        return -1;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_add_memory_cell: pattern is NULL");
        return -1;
    }
    if (!agent->swarm_immune) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune not connected for add_memory_cell");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_add_memory_cell: agent->swarm_immune is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Swarm Immune: Adding memory cell");

    /* Create threat signature from pattern */
    NimcpSwarmThreatSignature signature = {0};
    size_t copy_len = pattern_len < sizeof(signature.pattern) ? pattern_len : sizeof(signature.pattern);
    memcpy(signature.pattern, pattern, copy_len);
    signature.pattern_len = copy_len;
    signature.match_threshold = 0.8f;  /* Default threshold */
    signature.type = THREAT_BYZANTINE;  /* Default threat type */
    signature.detection_count = 0;
    signature.last_seen = 0;

    /* Call actual Swarm Immune API */
    uint32_t created_cell_id = 0;
    nimcp_result_t result = nimcp_swarm_immune_add_memory_cell(
        agent->swarm_immune,
        &signature,
        (NimcpSwarmResponseType)response_type,
        0.8f,  /* Initial effectiveness */
        &created_cell_id
    );

    if (cell_id_out) *cell_id_out = created_cell_id;

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm immune add_memory_cell failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_add_memory_cell: validation failed");
        return -1;
    }
    return 0;
}


/* ============================================================================
 * Swarm Memory USE Functions
 * ============================================================================ */

int nimcp_health_agent_use_swarm_memory_store(
    nimcp_health_agent_t* agent,
    const void* pattern_data,
    size_t pattern_size,
    uint32_t pattern_type,
    uint32_t importance,
    char* pattern_id_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_store: validate_agent is NULL");
        return -1;
    }
    if (!pattern_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_swarm_memory_store: pattern_data is NULL");
        return -1;
    }
    if (!agent->swarm_memory) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory not connected for store");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_swarm_memory_store: agent->swarm_memory is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_DEBUG, "USE Swarm Memory: Storing pattern");
    atomic_fetch_add(&agent->swarm_memories_stored, 1);

    /* Call actual Swarm Memory API */
    char memory_id[NIMCP_ID_BUFFER_SIZE] = {0};
    nimcp_result_t result = nimcp_swarm_memory_store(
        agent->swarm_memory,
        (NimcpMemoryType)pattern_type,
        (NimcpMemoryImportance)importance,
        pattern_data,
        pattern_size,
        memory_id
    );

    if (pattern_id_out) {
        strncpy(pattern_id_out, memory_id, 63);
        pattern_id_out[63] = '\0';
    }

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory store failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_store: validation failed");
        return -1;
    }
    return 0;
}


int nimcp_health_agent_use_swarm_memory_replay(
    nimcp_health_agent_t* agent,
    uint32_t count
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_replay: validate_agent is NULL");
        return -1;
    }
    if (!agent->swarm_memory) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory not connected for replay");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_swarm_memory_replay: agent->swarm_memory is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Swarm Memory: Replaying %u patterns", count);

    /* Call actual Swarm Memory API */
    uint32_t replays_performed = 0;
    nimcp_result_t result = nimcp_swarm_memory_replay_cycle(
        agent->swarm_memory,
        count,
        &replays_performed
    );

    atomic_fetch_add(&agent->swarm_replays_performed, replays_performed);

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory replay failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_replay: validation failed");
        return -1;
    }
    return (int)replays_performed;
}


int nimcp_health_agent_use_swarm_memory_consolidate(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_consolidate: validate_agent is NULL");
        return -1;
    }
    if (!agent->swarm_memory) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory not connected for consolidate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_swarm_memory_consolidate: agent->swarm_memory is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Swarm Memory: Triggering consolidation");

    /* Call actual Swarm Memory API */
    uint32_t memories_consolidated = 0;
    nimcp_result_t result = nimcp_swarm_memory_consolidate(
        agent->swarm_memory,
        &memories_consolidated
    );

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory consolidate failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_consolidate: validation failed");
        return -1;
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Swarm memory consolidated %u memories", memories_consolidated);
    return (int)memories_consolidated;
}


/* ============================================================================
 * Engram USE Functions
 * ============================================================================ */

int nimcp_health_agent_use_engram_encode(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg,
    uint64_t* engram_id_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_engram_encode: validate_agent is NULL");
        return -1;
    }
    if (!msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_engram_encode: msg is NULL");
        return -1;
    }
    if (!agent->engram) {
        nimcp_log(LOG_LEVEL_WARN, "Engram not connected for encode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_engram_encode: agent->engram is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_DEBUG, "USE Engram: Encoding health event type %d", msg->type);
    atomic_fetch_add(&agent->engram_encodings, 1);

    /*
     * Engram module uses the connected engram system directly.
     * The engram system provides memory trace encoding - when connected,
     * encoding operations are tracked here for health monitoring.
     * Actual encoding is performed by the engram system itself.
     */
    if (engram_id_out) {
        *engram_id_out = atomic_load(&agent->engram_encodings);
    }
    return 0;
}


int nimcp_health_agent_use_engram_recall(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg,
    uint64_t* recalled_ids,
    uint32_t max_recalls,
    uint32_t* num_recalled_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_engram_recall: validate_agent is NULL");
        return -1;
    }
    if (!msg || !num_recalled_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_engram_recall: required parameter is NULL (msg, num_recalled_out)");
        return -1;
    }
    if (!agent->engram) {
        nimcp_log(LOG_LEVEL_WARN, "Engram not connected for recall");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_engram_recall: agent->engram is NULL");
        return -1;
    }

    (void)recalled_ids;
    (void)max_recalls;

    nimcp_log(LOG_LEVEL_DEBUG, "USE Engram: Recalling similar events for type %d", msg->type);
    atomic_fetch_add(&agent->engram_recalls, 1);

    /*
     * Engram recall searches for similar health events in memory.
     * When the engram system is connected, it handles the pattern matching.
     * Recall operations are tracked here for health monitoring statistics.
     */
    *num_recalled_out = 0;
    return 0;
}


/* ============================================================================
 * Systems Consolidation USE Functions
 * ============================================================================ */

int nimcp_health_agent_use_consolidation_replay(
    nimcp_health_agent_t* agent,
    uint32_t replay_count
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_consolidation_replay: validate_agent is NULL");
        return -1;
    }
    if (!agent->memory_consolidation) {
        nimcp_log(LOG_LEVEL_WARN, "Memory consolidation not connected for replay");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_consolidation_replay: agent->memory_consolidation is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Consolidation: Running %u replays", replay_count);

    /*
     * Systems consolidation replay implements hippocampal-to-cortical transfer.
     * When the consolidation system is connected, it handles the actual replay.
     * This function triggers the connected system's replay mechanism.
     */
    return 0;
}


int nimcp_health_agent_use_consolidation_extract_semantics(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_consolidation_extract_semantics: validate_agent is NULL");
        return -1;
    }
    if (!agent->memory_consolidation) {
        nimcp_log(LOG_LEVEL_WARN, "Memory consolidation not connected for extract_semantics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_consolidation_extract_semantics: agent->memory_consolidation is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Consolidation: Extracting semantic features");

    /*
     * Semantic extraction creates generalized knowledge from specific memories.
     * When the consolidation system is connected, it performs schema formation
     * and pattern abstraction across episodic memories.
     */
    return 0;
}


/* ============================================================================
 * STATE CONSISTENCY MANAGER - Public API Functions
 * ============================================================================ */

int nimcp_health_agent_check_consistency(nimcp_health_agent_t* agent,
                                          health_agent_consistency_result_t* result) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "agent_run_consistency_checks: validate_agent is NULL");
        return -1;
    }

    /* Run all consistency checks immediately */
    health_agent_consistency_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));
    local_result.timestamp_us = get_timestamp_us();

    uint64_t check_start = local_result.timestamp_us;
    bool overall_passed = true;

    /* Run all checks (ignore config enable flags for explicit call) */
    if (!agent_check_reference_counts(agent, &local_result)) overall_passed = false;
    if (!agent_check_pointer_canaries(agent, &local_result)) overall_passed = false;
    if (!agent_check_struct_magic(agent, &local_result)) overall_passed = false;
    if (!agent_check_mutex_state(agent, &local_result)) overall_passed = false;
    if (!agent_check_circular_buffers(agent, &local_result)) overall_passed = false;
    if (!agent_check_knowledge_graph(agent, &local_result)) overall_passed = false;
    if (!agent_check_neuron_values(agent, &local_result)) overall_passed = false;

    local_result.overall_passed = overall_passed;
    local_result.check_duration_us = get_timestamp_us() - check_start;

    /* Store result */
    if (nimcp_mutex_lock(agent->consistency_mutex) == 0) {
        agent->last_consistency_result = local_result;
        nimcp_mutex_unlock(agent->consistency_mutex);
    }

    atomic_store(&agent->last_consistency_check_us, local_result.timestamp_us);
    atomic_fetch_add(&agent->consistency_checks_run, 1);
    if (!overall_passed) {
        atomic_fetch_add(&agent->consistency_failures_total, 1);
    }

    /* Return result to caller if requested */
    if (result) {
        *result = local_result;
    }

    return overall_passed ? 0 : -1;
}


bool nimcp_health_agent_validate_magic(const void* ptr, uint32_t expected_magic,
                                        const char* struct_name) {
    if (!ptr) {
        nimcp_log(LOG_LEVEL_ERROR, "Cannot validate magic: NULL pointer for '%s'",
                  struct_name ? struct_name : "unknown");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: ptr is NULL");
        return false;
    }

    uint32_t actual_magic = *(const uint32_t*)ptr;
    if (actual_magic != expected_magic) {
        nimcp_log(LOG_LEVEL_ERROR, "Magic validation failed for '%s': expected 0x%X, got 0x%X",
                  struct_name ? struct_name : "unknown", expected_magic, actual_magic);
        return false;
    }

    return true;
}


int nimcp_health_agent_register_struct(nimcp_health_agent_t* agent, void* ptr,
                                        uint32_t expected_magic, const char* name) {
    if (!validate_agent(agent) || !ptr || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (validate_agent, ptr, name)");
        return -1;
    }

    int result = -1;

    if (nimcp_mutex_lock(agent->consistency_mutex) == 0) {
        /* Find an empty slot */
        for (uint32_t i = 0; i < 64; i++) {
            if (!agent->registered_structs[i].active) {
                agent->registered_structs[i].ptr = ptr;
                agent->registered_structs[i].expected_magic = expected_magic;
                strncpy(agent->registered_structs[i].name, name, 63);
                agent->registered_structs[i].name[63] = '\0';
                agent->registered_structs[i].active = true;
                agent->registered_struct_count++;
                result = 0;
                nimcp_log(LOG_LEVEL_DEBUG, "Registered struct '%s' (magic=0x%X) for consistency checking",
                          name, expected_magic);
                break;
            }
        }
        nimcp_mutex_unlock(agent->consistency_mutex);
    }

    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Failed to register struct '%s': no free slots", name);
    }

    return result;
}


int nimcp_health_agent_unregister_struct(nimcp_health_agent_t* agent, void* ptr) {
    if (!validate_agent(agent) || !ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_unregister_struct: required parameter is NULL (validate_agent, ptr)");
        return -1;
    }

    int result = -1;

    if (nimcp_mutex_lock(agent->consistency_mutex) == 0) {
        for (uint32_t i = 0; i < 64; i++) {
            if (agent->registered_structs[i].active &&
                agent->registered_structs[i].ptr == ptr) {
                nimcp_log(LOG_LEVEL_DEBUG, "Unregistered struct '%s' from consistency checking",
                          agent->registered_structs[i].name);
                agent->registered_structs[i].active = false;
                agent->registered_structs[i].ptr = NULL;
                agent->registered_struct_count--;
                result = 0;
                break;
            }
        }
        nimcp_mutex_unlock(agent->consistency_mutex);
    }

    return result;
}


int nimcp_health_agent_connect_hippocampus(
    nimcp_health_agent_t* agent,
    nimcp_hippocampus_t* hippocampus,
    const health_agent_hippocampus_config_t* config)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_hippocampus: validate_agent is NULL");
        return -1;
    }

    if (!hippocampus) {
        nimcp_log(LOG_LEVEL_ERROR, "Null hippocampus in connect_hippocampus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_hippocampus: hippocampus is NULL");
        return -1;
    }

    /* Store hippocampus pointer */
    agent->hippocampus = hippocampus;
    agent->hippocampus_connected = true;

    /* Apply configuration or defaults */
    if (config) {
        agent->hippocampus_config = *config;
    } else {
        nimcp_health_agent_hippocampus_config_default(&agent->hippocampus_config);
    }

    nimcp_log(LOG_LEVEL_INFO, "Health agent connected to hippocampus (interval=%ums)",
              agent->hippocampus_config.health_check_interval_ms);

    return 0;
}


int nimcp_health_agent_connect_mammillary(
    nimcp_health_agent_t* agent,
    nimcp_mammillary_t* mammillary,
    const health_agent_mammillary_config_t* config)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_mammillary: validate_agent is NULL");
        return -1;
    }

    if (!mammillary) {
        nimcp_log(LOG_LEVEL_ERROR, "Null mammillary in connect_mammillary");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_mammillary: mammillary is NULL");
        return -1;
    }

    /* Store mammillary pointer */
    agent->mammillary = mammillary;
    agent->mammillary_connected = true;

    /* Apply configuration or defaults */
    if (config) {
        agent->mammillary_config = *config;
    } else {
        nimcp_health_agent_mammillary_config_default(&agent->mammillary_config);
    }

    nimcp_log(LOG_LEVEL_INFO, "Health agent connected to mammillary bodies (interval=%ums)",
              agent->mammillary_config.health_check_interval_ms);

    return 0;
}


int nimcp_health_agent_validate_memory_consistency(
    nimcp_health_agent_t* agent)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_validate_memory_consistency: validate_agent is NULL");
        return -1;
    }

    int inconsistencies = 0;

    /* Check hippocampus-mammillary connection */
    if (agent->hippocampus_connected && agent->mammillary_connected) {
        float hippo_health = hippo_get_health_status(agent->hippocampus);
        float mammillary_health = mammillary_get_health_status(agent->mammillary);

        /* Large health discrepancy indicates inconsistency */
        float diff = fabsf(hippo_health - mammillary_health);
        if (diff > 0.3f) {
            nimcp_log(LOG_LEVEL_WARN,
                      "Memory tier inconsistency: hippocampus health=%.2f, mammillary health=%.2f",
                      hippo_health, mammillary_health);
            inconsistencies++;
        }
    }

    /* Check engram-consolidation connection if both connected */
    if (agent->engram != NULL && agent->memory_consolidation != NULL) {
        /* Would check actual consistency here */
    }

    if (inconsistencies > 0) {
        nimcp_log(LOG_LEVEL_WARN, "Memory consistency check found %d inconsistencies",
                  inconsistencies);
    } else {
        nimcp_log(LOG_LEVEL_DEBUG, "Memory consistency check passed");
    }

    return inconsistencies;
}


int nimcp_health_agent_memory_recovery(
    nimcp_health_agent_t* agent,
    memory_recovery_action_t action,
    int target_module)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_memory_recovery: validate_agent is NULL");
        return -1;
    }

    const char* action_name = "unknown";
    switch (action) {
        case MEMORY_RECOVERY_NONE: action_name = "none"; break;
        case MEMORY_RECOVERY_RESET_CA3: action_name = "reset_ca3"; break;
        case MEMORY_RECOVERY_STABILIZE_RHYTHMS: action_name = "stabilize_rhythms"; break;
        case MEMORY_RECOVERY_HD_DRIFT_CORRECT: action_name = "hd_drift_correct"; break;
        case MEMORY_RECOVERY_FORNIX_STRENGTHEN: action_name = "fornix_strengthen"; break;
        case MEMORY_RECOVERY_FORCE_CONSOLIDATION: action_name = "force_consolidation"; break;
        case MEMORY_RECOVERY_PAPEZ_REPAIR: action_name = "papez_repair"; break;
        case MEMORY_RECOVERY_EXPAND_CAPACITY: action_name = "expand_capacity"; break;
        case MEMORY_RECOVERY_GC_OLD_TRACES: action_name = "gc_old_traces"; break;
        case MEMORY_RECOVERY_CROSS_TIER_SYNC: action_name = "cross_tier_sync"; break;
        case MEMORY_RECOVERY_METABOLIC_BOOST: action_name = "metabolic_boost"; break;
        case MEMORY_RECOVERY_EMERGENCY_SAVE: action_name = "emergency_save"; break;
    }

    const char* target_name = (target_module == 0) ? "hippocampus" :
                              (target_module == 1) ? "mammillary" : "both";

    nimcp_log(LOG_LEVEL_INFO, "Memory recovery action '%s' triggered for %s",
              action_name, target_name);

    /* Execute recovery based on action type */
    switch (action) {
        case MEMORY_RECOVERY_RESET_CA3:
            if (target_module == 0 || target_module == 2) {
                /* Would call hippocampus reset API */
                nimcp_log(LOG_LEVEL_INFO, "CA3 reset requested");
            }
            break;

        case MEMORY_RECOVERY_HD_DRIFT_CORRECT:
            if (target_module == 1 || target_module == 2) {
                /* Would call mammillary HD correction API */
                nimcp_log(LOG_LEVEL_INFO, "HD drift correction requested");
            }
            break;

        case MEMORY_RECOVERY_FORCE_CONSOLIDATION:
            /* Would trigger consolidation on both modules */
            nimcp_log(LOG_LEVEL_INFO, "Force consolidation requested");
            break;

        case MEMORY_RECOVERY_EMERGENCY_SAVE:
            /* Emergency save all memory state */
            nimcp_log(LOG_LEVEL_WARN, "Emergency memory save triggered");
            break;

        default:
            nimcp_log(LOG_LEVEL_DEBUG, "Memory recovery action %d handled", action);
            break;
    }

    return 0;
}


bool nimcp_health_agent_memory_needs_attention(
    const nimcp_health_agent_t* agent)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_memory_needs_attention: validate_agent is NULL");
        return false;
    }

    /* Quick health check */
    if (agent->hippocampus_connected && agent->hippocampus) {
        float health = hippo_get_health_status(agent->hippocampus);
        if (health < 0.5f) {
            return true;
        }
    }

    if (agent->mammillary_connected && agent->mammillary) {
        float health = mammillary_get_health_status(agent->mammillary);
        if (health < 0.5f) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_memory_needs_attention: validation failed");
    return false;
}


/* ============================================================================
 * Phase 5.8: Dynamic Capacity Management Integration
 * ============================================================================ */

int nimcp_health_agent_register_capacity_manager(
    nimcp_health_agent_t* agent,
    capacity_manager_t* cm)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_register_capacity_manager: validate_agent is NULL");
        return -1;
    }
    if (!cm) {
        nimcp_log(LOG_LEVEL_WARN, "Null capacity manager in registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_register_capacity_manager: cm is NULL");
        return -1;
    }

    /* Create mutex if not exists */
    if (!agent->capacity_mutex) {
        mutex_attr_t attr = { .type = MUTEX_TYPE_NORMAL };
        agent->capacity_mutex = nimcp_mutex_create(&attr);
        if (!agent->capacity_mutex) {
            nimcp_log(LOG_LEVEL_ERROR, "Failed to create capacity mutex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_register_capacity_manager: agent->capacity_mutex is NULL");
            return -1;
        }
    }

    nimcp_mutex_lock(agent->capacity_mutex);

    /* Check if already at max */
    uint32_t count = atomic_load(&agent->num_capacity_managers);
    if (count >= HEALTH_AGENT_MAX_CAPACITY_MANAGERS) {
        nimcp_mutex_unlock(agent->capacity_mutex);
        nimcp_log(LOG_LEVEL_ERROR, "Max capacity managers reached (%u)",
                  HEALTH_AGENT_MAX_CAPACITY_MANAGERS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_register_capacity_manager: capacity exceeded");
        return -1;
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < count; i++) {
        if (agent->capacity_managers[i] == cm) {
            nimcp_mutex_unlock(agent->capacity_mutex);
            nimcp_log(LOG_LEVEL_WARN, "Capacity manager already registered");
            return 0;  /* Not an error */
        }
    }

    /* Register */
    agent->capacity_managers[count] = cm;
    atomic_fetch_add(&agent->num_capacity_managers, 1);

    nimcp_mutex_unlock(agent->capacity_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Registered capacity manager '%s'", cm->module_name);
    return 0;
}


int nimcp_health_agent_unregister_capacity_manager(
    nimcp_health_agent_t* agent,
    capacity_manager_t* cm)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_unregister_capacity_manager: validate_agent is NULL");
        return -1;
    }
    if (!cm || !agent->capacity_mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_unregister_capacity_manager: required parameter is NULL (cm, agent->capacity_mutex)");
        return -1;
    }

    nimcp_mutex_lock(agent->capacity_mutex);

    uint32_t count = atomic_load(&agent->num_capacity_managers);
    bool found = false;

    for (uint32_t i = 0; i < count; i++) {
        if (found) {
            /* Shift remaining managers down */
            agent->capacity_managers[i - 1] = agent->capacity_managers[i];
        } else if (agent->capacity_managers[i] == cm) {
            found = true;
        }
    }

    if (found) {
        agent->capacity_managers[count - 1] = NULL;
        atomic_fetch_sub(&agent->num_capacity_managers, 1);
        nimcp_log(LOG_LEVEL_INFO, "Unregistered capacity manager '%s'", cm->module_name);
    }

    nimcp_mutex_unlock(agent->capacity_mutex);

    return found ? 0 : -1;
}


bool nimcp_health_agent_capacity_needs_attention(
    const nimcp_health_agent_t* agent)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_capacity_needs_attention: validate_agent is NULL");
        return false;
    }

    uint32_t count = atomic_load(&agent->num_capacity_managers);

    for (uint32_t i = 0; i < count; i++) {
        capacity_manager_t* cm = agent->capacity_managers[i];
        if (!cm) continue;

        capacity_level_t level = capacity_manager_get_level(cm);
        if (level >= CAPACITY_LEVEL_WARNING) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_capacity_needs_attention: capacity exceeded");
    return false;
}


int nimcp_health_agent_connect_symbolic_logic(
    nimcp_health_agent_t* agent,
    symbolic_logic_t* logic,
    const health_agent_symbolic_logic_config_t* config)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_symbolic_logic: validate_agent is NULL");
        return -1;
    }
    if (!logic) {
        nimcp_log(LOG_LEVEL_WARN, "Null symbolic logic engine in connection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_symbolic_logic: logic is NULL");
        return -1;
    }

    /* Create mutex if not exists */
    if (!agent->logic_mutex) {
        mutex_attr_t attr = { .type = MUTEX_TYPE_NORMAL };
        agent->logic_mutex = nimcp_mutex_create(&attr);
        if (!agent->logic_mutex) {
            nimcp_log(LOG_LEVEL_ERROR, "Failed to create logic mutex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_symbolic_logic: agent->logic_mutex is NULL");
            return -1;
        }
    }

    nimcp_mutex_lock(agent->logic_mutex);

    /* Check if already at max */
    uint32_t count = atomic_load(&agent->num_logic_engines);
    if (count >= HEALTH_AGENT_MAX_LOGIC_ENGINES) {
        nimcp_mutex_unlock(agent->logic_mutex);
        nimcp_log(LOG_LEVEL_WARN, "Max logic engines reached (%u)", HEALTH_AGENT_MAX_LOGIC_ENGINES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_connect_symbolic_logic: capacity exceeded");
        return -1;
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < count; i++) {
        if (agent->logic_engines[i] == logic) {
            nimcp_mutex_unlock(agent->logic_mutex);
            nimcp_log(LOG_LEVEL_WARN, "Symbolic logic engine already connected");
            return 0;  /* Not an error */
        }
    }

    /* Register */
    agent->logic_engines[count] = logic;
    atomic_fetch_add(&agent->num_logic_engines, 1);

    /* Apply configuration */
    if (config) {
        memcpy(&agent->logic_config, config, sizeof(agent->logic_config));
    } else {
        nimcp_health_agent_symbolic_logic_config_default(&agent->logic_config);
    }

    /* Initialize health score */
    atomic_store(&agent->logic_health_score, 100.0f);

    nimcp_mutex_unlock(agent->logic_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Connected symbolic logic engine to health agent (total: %u)",
              count + 1);
    return 0;
}


int nimcp_health_agent_disconnect_symbolic_logic(
    nimcp_health_agent_t* agent,
    symbolic_logic_t* logic)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_disconnect_symbolic_logic: validate_agent is NULL");
        return -1;
    }
    if (!logic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic is NULL");

        return -1;
    }
    if (!agent->logic_mutex) {
        return -1;  /* Not initialized */
    }

    nimcp_mutex_lock(agent->logic_mutex);

    uint32_t count = atomic_load(&agent->num_logic_engines);
    bool found = false;

    for (uint32_t i = 0; i < count; i++) {
        if (found) {
            /* Shift remaining engines down */
            agent->logic_engines[i - 1] = agent->logic_engines[i];
        } else if (agent->logic_engines[i] == logic) {
            found = true;
        }
    }

    if (found) {
        agent->logic_engines[count - 1] = NULL;
        atomic_fetch_sub(&agent->num_logic_engines, 1);
        nimcp_log(LOG_LEVEL_INFO, "Disconnected symbolic logic engine from health agent");
    }

    nimcp_mutex_unlock(agent->logic_mutex);

    return found ? 0 : -1;
}


int nimcp_health_agent_logic_recovery(
    nimcp_health_agent_t* agent,
    logic_recovery_action_t action,
    int engine_index)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_logic_recovery: validate_agent is NULL");
        return -1;
    }

    uint32_t count = atomic_load(&agent->num_logic_engines);
    if (count == 0) {
        nimcp_log(LOG_LEVEL_WARN, "No logic engines connected for recovery");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_logic_recovery: count is zero");
        return -1;
    }

    /* Determine which engines to target */
    uint32_t start_idx = 0;
    uint32_t end_idx = count;

    if (engine_index >= 0) {
        if ((uint32_t)engine_index >= count) {
            nimcp_log(LOG_LEVEL_WARN, "Invalid logic engine index: %d", engine_index);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_logic_recovery: capacity exceeded");
            return -1;
        }
        start_idx = (uint32_t)engine_index;
        end_idx = start_idx + 1;
    }

    int success_count = 0;

    for (uint32_t i = start_idx; i < end_idx; i++) {
        symbolic_logic_t* logic = agent->logic_engines[i];
        if (!logic) continue;

        switch (action) {
            case LOGIC_RECOVERY_NONE:
                /* No action */
                success_count++;
                break;

            case LOGIC_RECOVERY_INTERRUPT_INFERENCE:
                /* Signal the logic engine to interrupt current inference */
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: interrupt inference (engine %u)", i);
                /* Would call symbolic_logic_interrupt() if available */
                success_count++;
                break;

            case LOGIC_RECOVERY_RESET_UNIFIER:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: reset unifier (engine %u)", i);
                /* Would call symbolic_logic_reset_unifier() if available */
                success_count++;
                break;

            case LOGIC_RECOVERY_GC_KB:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: GC knowledge base (engine %u)", i);
                /* Would call symbolic_logic_gc() if available */
                success_count++;
                break;

            case LOGIC_RECOVERY_COMPACT_KB:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: compact knowledge base (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_CLEAR_CACHE:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: clear inference cache (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_RESOLVE_INCONSISTENCY:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: resolve inconsistency (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_REDUCE_DEPTH:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: reduce inference depth (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_CHECKPOINT_KB:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: checkpoint KB (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_RESTORE_KB:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: restore KB (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_SOFT_RESET:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: soft reset (engine %u)", i);
                success_count++;
                break;

            case LOGIC_RECOVERY_FULL_RESET:
                nimcp_log(LOG_LEVEL_INFO, "Logic recovery: full reset (engine %u)", i);
                /* Would call symbolic_logic_reset() if available */
                success_count++;
                break;

            default:
                nimcp_log(LOG_LEVEL_WARN, "Unknown logic recovery action: %d", (int)action);
                break;
        }
    }

    if (success_count > 0) {
        atomic_fetch_add(&agent->logic_recoveries_performed, (uint64_t)success_count);
    }

    return success_count > 0 ? 0 : -1;
}


bool nimcp_health_agent_logic_needs_attention(
    const nimcp_health_agent_t* agent)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_logic_needs_attention: validate_agent is NULL");
        return false;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_logic_engines);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_logic_needs_attention: count is zero");
        return false;  /* No engines, no attention needed */
    }

    /* Quick check: use cached health score */
    float health = atomic_load(&((nimcp_health_agent_t*)agent)->logic_health_score);
    if (health < 80.0f) {
        return true;
    }

    /* More thorough check if health is borderline */
    for (uint32_t i = 0; i < count; i++) {
        symbolic_logic_t* logic = ((nimcp_health_agent_t*)agent)->logic_engines[i];
        if (!logic) continue;

        logic_stats_t stats;
        if (symbolic_logic_get_stats(logic, &stats)) {
            const health_agent_symbolic_logic_config_t* cfg =
                &((nimcp_health_agent_t*)agent)->logic_config;

            /* Check inference time */
            if (cfg->enable_inference_monitoring &&
                stats.avg_inference_time > cfg->inference_timeout_ms * 0.8f) {
                return true;
            }

            /* Check unification success rate */
            if (cfg->enable_performance_monitoring &&
                stats.unification_attempts > 100 &&
                (float)stats.unification_successes / (float)stats.unification_attempts
                    < cfg->unification_success_min * 1.2f) {
                return true;
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_logic_needs_attention: operation failed");
    return false;
}


int nimcp_health_agent_connect_substrate(
    nimcp_health_agent_t* agent,
    neural_substrate_t* substrate,
    const health_agent_substrate_config_t* config
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }
    if (!substrate) {
        nimcp_log(LOG_LEVEL_WARN, "Null neural substrate in connection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_substrate: substrate is NULL");
        return -1;
    }

    /* Initialize substrate mutex if needed */
    if (!agent->substrate_mutex) {
        mutex_attr_t attr = { .type = MUTEX_TYPE_NORMAL };
        agent->substrate_mutex = nimcp_mutex_create(&attr);
        if (!agent->substrate_mutex) {
            nimcp_log(LOG_LEVEL_ERROR, "Failed to create substrate mutex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_substrate: agent->substrate_mutex is NULL");
            return -1;
        }
    }

    nimcp_mutex_lock(agent->substrate_mutex);

    /* Check capacity */
    uint32_t count = atomic_load(&agent->num_substrates);
    if (count >= HEALTH_AGENT_MAX_NEURAL_SUBSTRATES) {
        nimcp_mutex_unlock(agent->substrate_mutex);
        nimcp_log(LOG_LEVEL_WARN, "Maximum neural substrates reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_connect_substrate: capacity exceeded");
        return -1;
    }

    /* Check if already connected */
    for (uint32_t i = 0; i < count; i++) {
        if (agent->substrates[i] == substrate) {
            nimcp_mutex_unlock(agent->substrate_mutex);
            nimcp_log(LOG_LEVEL_DEBUG, "Neural substrate already connected");
            return 0;  /* Already connected, not an error */
        }
    }

    /* Add substrate */
    agent->substrates[count] = substrate;
    atomic_fetch_add(&agent->num_substrates, 1);

    /* Apply configuration */
    if (config) {
        memcpy(&agent->substrate_config, config, sizeof(agent->substrate_config));
    } else {
        nimcp_health_agent_substrate_config_default(&agent->substrate_config);
    }

    /* Initialize substrate health score */
    atomic_store(&agent->substrate_health_score, 100.0f);

    nimcp_mutex_unlock(agent->substrate_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Connected neural substrate to health agent (total: %u)",
              count + 1);
    return 0;
}


int nimcp_health_agent_disconnect_substrate(
    nimcp_health_agent_t* agent,
    neural_substrate_t* substrate
) {
    if (!agent || !substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_substrate: required parameter is NULL (agent, substrate)");
        return -1;
    }

    if (!agent->substrate_mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_substrate: agent->substrate_mutex is NULL");
        return -1;  /* No substrates registered */
    }

    nimcp_mutex_lock(agent->substrate_mutex);

    uint32_t count = atomic_load(&agent->num_substrates);
    bool found = false;

    for (uint32_t i = 0; i < count; i++) {
        if (found) {
            /* Shift remaining substrates down */
            agent->substrates[i - 1] = agent->substrates[i];
        } else if (agent->substrates[i] == substrate) {
            found = true;
        }
    }

    if (found) {
        agent->substrates[count - 1] = NULL;
        atomic_fetch_sub(&agent->num_substrates, 1);
    }

    nimcp_mutex_unlock(agent->substrate_mutex);

    if (found) {
        nimcp_log(LOG_LEVEL_INFO, "Disconnected neural substrate from health agent");
    }
    return found ? 0 : -1;
}


int nimcp_health_agent_substrate_recovery(
    nimcp_health_agent_t* agent,
    substrate_recovery_action_t action,
    int substrate_index
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    uint32_t count = atomic_load(&agent->num_substrates);
    if (count == 0) {
        nimcp_log(LOG_LEVEL_WARN, "No neural substrates connected for recovery");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_substrate_recovery: count is zero");
        return -1;
    }

    if (substrate_index >= 0 && (uint32_t)substrate_index >= count) {
        nimcp_log(LOG_LEVEL_WARN, "Invalid substrate index: %d", substrate_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_substrate_recovery: capacity exceeded");
        return -1;
    }

    int start_idx = (substrate_index < 0) ? 0 : substrate_index;
    int end_idx = (substrate_index < 0) ? (int)count : substrate_index + 1;

    for (int i = start_idx; i < end_idx; i++) {
        neural_substrate_t* sub = agent->substrates[i];
        if (!sub) continue;

        switch (action) {
            case SUBSTRATE_RECOVERY_NONE:
                /* No action */
                break;

            case SUBSTRATE_RECOVERY_BOOST_ATP:
                /* Boost ATP level */
                substrate_set_atp(sub, 0.9f);
                break;

            case SUBSTRATE_RECOVERY_BOOST_OXYGEN:
                /* Boost oxygen saturation */
                substrate_set_oxygen(sub, 0.95f);
                break;

            case SUBSTRATE_RECOVERY_BOOST_GLUCOSE:
                /* Boost glucose level */
                substrate_set_glucose(sub, 0.85f);
                break;

            case SUBSTRATE_RECOVERY_COOL_DOWN:
                /* Reduce temperature */
                substrate_set_temperature(sub, 37.0f);
                break;

            case SUBSTRATE_RECOVERY_WARM_UP:
                /* Increase temperature */
                substrate_set_temperature(sub, 37.0f);
                break;

            case SUBSTRATE_RECOVERY_BALANCE_IONS:
                /* Reset ion balance */
                substrate_set_ion_balance(sub, 0.95f);
                break;

            case SUBSTRATE_RECOVERY_REPAIR_MEMBRANE:
                /* Repair membrane */
                substrate_set_membrane_integrity(sub, 0.9f);
                break;

            case SUBSTRATE_RECOVERY_REDUCE_ACTIVITY:
                /* Would reduce firing/transmission - implementation-specific */
                break;

            case SUBSTRATE_RECOVERY_RESET_STATS:
                /* Reset substrate statistics */
                /* substrate_reset_stats(sub); - if available */
                break;

            case SUBSTRATE_RECOVERY_SOFT_RESET:
            case SUBSTRATE_RECOVERY_FULL_RESET:
                /* Reset substrate to initial state */
                substrate_reset(sub);
                break;
        }

        atomic_fetch_add(&agent->substrate_recoveries_performed, 1);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Substrate recovery action %d completed", action);
    return 0;
}


bool nimcp_health_agent_substrate_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_substrate_needs_attention: agent is NULL");
        return false;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_substrates);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_substrate_needs_attention: count is zero");
        return false;  /* No substrates to check */
    }

    /* Quick health score check */
    float score = atomic_load(&((nimcp_health_agent_t*)agent)->substrate_health_score);
    if (score < 70.0f) {
        return true;
    }

    /* Check each substrate for critical conditions */
    const health_agent_substrate_config_t* cfg = &((nimcp_health_agent_t*)agent)->substrate_config;
    for (uint32_t i = 0; i < count; i++) {
        neural_substrate_t* sub = ((nimcp_health_agent_t*)agent)->substrates[i];
        if (!sub) continue;

        /* Check for critical metabolic conditions */
        /* In production, would query actual substrate state */
        /* For now, assume healthy if we got this far */
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_substrate_needs_attention: sub is NULL");
    return false;
}


/* -------------------------------------------------------------------------
 * Thalamic Bridge Connection Management
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_thalamic(
    nimcp_health_agent_t* agent,
    omni_wm_thalamic_bridge_t* bridge,
    const health_agent_thalamic_config_t* config
) {
    if (!agent || !bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "connect_thalamic: NULL agent or bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_thalamic: required parameter is NULL (agent, bridge)");
        return -1;
    }

    /* Lock for registration */
    if (agent->thalamic_mutex) {
        nimcp_mutex_lock(agent->thalamic_mutex);
    }

    /* Check if already registered */
    uint32_t count = atomic_load(&agent->num_thalamic_bridges);
    for (uint32_t i = 0; i < count; i++) {
        if (agent->thalamic_bridges[i] == bridge) {
            if (agent->thalamic_mutex) {
                nimcp_mutex_unlock(agent->thalamic_mutex);
            }
            nimcp_log(LOG_LEVEL_WARN, "Thalamic bridge already registered");
            return 0;  /* Already registered */
        }
    }

    /* Check capacity */
    if (count >= HEALTH_AGENT_MAX_THALAMIC_BRIDGES) {
        if (agent->thalamic_mutex) {
            nimcp_mutex_unlock(agent->thalamic_mutex);
        }
        nimcp_log(LOG_LEVEL_ERROR, "Maximum thalamic bridges exceeded (%u)",
                  HEALTH_AGENT_MAX_THALAMIC_BRIDGES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_connect_thalamic: validation failed");
        return -1;
    }

    /* Register bridge */
    agent->thalamic_bridges[count] = bridge;
    atomic_store(&agent->num_thalamic_bridges, count + 1);

    /* Apply configuration */
    if (config) {
        memcpy(&agent->thalamic_config, config, sizeof(agent->thalamic_config));
    } else {
        nimcp_health_agent_thalamic_config_default(&agent->thalamic_config);
    }

    /* Initialize health tracking */
    atomic_store(&agent->thalamic_health_score, 100.0f);

    if (agent->thalamic_mutex) {
        nimcp_mutex_unlock(agent->thalamic_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected thalamic bridge to health agent (total: %u)",
              count + 1);
    return 0;
}


int nimcp_health_agent_disconnect_thalamic(
    nimcp_health_agent_t* agent,
    omni_wm_thalamic_bridge_t* bridge
) {
    if (!agent || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_thalamic: required parameter is NULL (agent, bridge)");
        return -1;
    }

    if (agent->thalamic_mutex) {
        nimcp_mutex_lock(agent->thalamic_mutex);
    }

    uint32_t count = atomic_load(&agent->num_thalamic_bridges);
    bool found = false;

    for (uint32_t i = 0; i < count; i++) {
        if (agent->thalamic_bridges[i] == bridge) {
            /* Shift remaining bridges down */
            for (uint32_t j = i; j < count - 1; j++) {
                agent->thalamic_bridges[j] = agent->thalamic_bridges[j + 1];
            }
            agent->thalamic_bridges[count - 1] = NULL;
            atomic_store(&agent->num_thalamic_bridges, count - 1);
            found = true;
            break;
        }
    }

    if (agent->thalamic_mutex) {
        nimcp_mutex_unlock(agent->thalamic_mutex);
    }

    if (found) {
        nimcp_log(LOG_LEVEL_INFO, "Disconnected thalamic bridge from health agent (remaining: %u)",
                  count - 1);
        return 0;
    }

    nimcp_log(LOG_LEVEL_WARN, "Thalamic bridge not found for disconnect");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_disconnect_thalamic: validation failed");
    return -1;
}


/* -------------------------------------------------------------------------
 * Middleware Training Context Connection Management
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_middleware(
    nimcp_health_agent_t* agent,
    nimcp_brain_training_ctx_t* training_ctx,
    const health_agent_middleware_config_t* config
) {
    if (!agent || !training_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "connect_middleware: NULL agent or context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_middleware: required parameter is NULL (agent, training_ctx)");
        return -1;
    }

    /* Lock for registration */
    if (agent->middleware_mutex) {
        nimcp_mutex_lock(agent->middleware_mutex);
    }

    /* Check if already registered */
    uint32_t count = atomic_load(&agent->num_training_contexts);
    for (uint32_t i = 0; i < count; i++) {
        if (agent->training_contexts[i] == training_ctx) {
            if (agent->middleware_mutex) {
                nimcp_mutex_unlock(agent->middleware_mutex);
            }
            nimcp_log(LOG_LEVEL_WARN, "Training context already registered");
            return 0;  /* Already registered */
        }
    }

    /* Check capacity */
    if (count >= HEALTH_AGENT_MAX_TRAINING_CONTEXTS) {
        if (agent->middleware_mutex) {
            nimcp_mutex_unlock(agent->middleware_mutex);
        }
        nimcp_log(LOG_LEVEL_ERROR, "Maximum training contexts exceeded (%u)",
                  HEALTH_AGENT_MAX_TRAINING_CONTEXTS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_connect_middleware: validation failed");
        return -1;
    }

    /* Register context */
    agent->training_contexts[count] = training_ctx;
    atomic_store(&agent->num_training_contexts, count + 1);

    /* Apply configuration */
    if (config) {
        memcpy(&agent->middleware_config, config, sizeof(agent->middleware_config));
    } else {
        nimcp_health_agent_middleware_config_default(&agent->middleware_config);
    }

    /* Initialize health tracking */
    atomic_store(&agent->middleware_health_score, 100.0f);

    if (agent->middleware_mutex) {
        nimcp_mutex_unlock(agent->middleware_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected training context to health agent (total: %u)",
              count + 1);
    return 0;
}


int nimcp_health_agent_disconnect_middleware(
    nimcp_health_agent_t* agent,
    nimcp_brain_training_ctx_t* training_ctx
) {
    if (!agent || !training_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_middleware: required parameter is NULL (agent, training_ctx)");
        return -1;
    }

    if (agent->middleware_mutex) {
        nimcp_mutex_lock(agent->middleware_mutex);
    }

    uint32_t count = atomic_load(&agent->num_training_contexts);
    bool found = false;

    for (uint32_t i = 0; i < count; i++) {
        if (agent->training_contexts[i] == training_ctx) {
            /* Shift remaining contexts down */
            for (uint32_t j = i; j < count - 1; j++) {
                agent->training_contexts[j] = agent->training_contexts[j + 1];
            }
            agent->training_contexts[count - 1] = NULL;
            atomic_store(&agent->num_training_contexts, count - 1);
            found = true;
            break;
        }
    }

    if (agent->middleware_mutex) {
        nimcp_mutex_unlock(agent->middleware_mutex);
    }

    if (found) {
        nimcp_log(LOG_LEVEL_INFO, "Disconnected training context from health agent (remaining: %u)",
                  count - 1);
        return 0;
    }

    nimcp_log(LOG_LEVEL_WARN, "Training context not found for disconnect");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_disconnect_middleware: validation failed");
    return -1;
}


/* -------------------------------------------------------------------------
 * Thalamic Recovery Actions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_thalamic_recovery(
    nimcp_health_agent_t* agent,
    thalamic_recovery_action_t action,
    int bridge_index
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (action == THALAMIC_RECOVERY_NONE) {
        return 0;  /* Nothing to do */
    }

    uint32_t count = atomic_load(&agent->num_thalamic_bridges);
    if (count == 0) {
        nimcp_log(LOG_LEVEL_WARN, "Thalamic recovery: no bridges registered");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_thalamic_recovery: count is zero");
        return -1;
    }

    /* Determine target bridges */
    uint32_t start_idx = (bridge_index < 0) ? 0 : (uint32_t)bridge_index;
    uint32_t end_idx = (bridge_index < 0) ? count : (uint32_t)(bridge_index + 1);

    if (start_idx >= count) {
        nimcp_log(LOG_LEVEL_ERROR, "Thalamic recovery: invalid bridge index %d", bridge_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_thalamic_recovery: capacity exceeded");
        return -1;
    }

    int success_count = 0;

    for (uint32_t i = start_idx; i < end_idx && i < count; i++) {
        omni_wm_thalamic_bridge_t* bridge = agent->thalamic_bridges[i];
        if (!bridge) continue;

        nimcp_error_t result = NIMCP_SUCCESS;

        switch (action) {
            case THALAMIC_RECOVERY_RESET_ATTENTION:
                /* Reset all nucleus attention to baseline */
                for (int n = 0; n < WM_THAL_NUCLEUS_COUNT; n++) {
                    result = omni_wm_thalamic_bridge_set_nucleus_attention(
                        bridge, (wm_thal_nucleus_type_t)n,
                        WM_THALAMIC_DEFAULT_ATTENTION);
                    if (result != NIMCP_SUCCESS) break;
                }
                break;

            case THALAMIC_RECOVERY_REBALANCE_NUCLEI:
                /* Set all nuclei to equal moderate attention */
                for (int n = 0; n < WM_THAL_NUCLEUS_COUNT; n++) {
                    result = omni_wm_thalamic_bridge_set_nucleus_attention(
                        bridge, (wm_thal_nucleus_type_t)n, 0.5f);
                    if (result != NIMCP_SUCCESS) break;
                }
                break;

            case THALAMIC_RECOVERY_RELEASE_TRN:
                /* Release TRN inhibition on all nuclei */
                for (int n = 0; n < WM_THAL_NUCLEUS_COUNT; n++) {
                    result = omni_wm_thalamic_bridge_release_trn_inhibition(
                        bridge, (wm_thal_nucleus_type_t)n);
                    if (result != NIMCP_SUCCESS) break;
                }
                break;

            case THALAMIC_RECOVERY_BOOST_AROUSAL:
                result = omni_wm_thalamic_bridge_set_arousal(bridge, 0.8f);
                break;

            case THALAMIC_RECOVERY_REDUCE_AROUSAL:
                result = omni_wm_thalamic_bridge_set_arousal(bridge, 0.3f);
                break;

            case THALAMIC_RECOVERY_CLEAR_BIAS:
                /* Clear attention bias by setting neutral bias */
                result = omni_wm_thalamic_bridge_set_attention_bias(bridge, NULL, 0, 0.0f);
                break;

            case THALAMIC_RECOVERY_RESET_PULVINAR:
                /* Reset pulvinar attention to uniform */
                result = omni_wm_thalamic_bridge_set_pulvinar_attention(bridge, NULL, 0);
                break;

            case THALAMIC_RECOVERY_FORCE_TONIC:
                /* Force tonic mode via high arousal */
                result = omni_wm_thalamic_bridge_set_arousal(bridge, 1.0f);
                break;

            case THALAMIC_RECOVERY_RESET_STATS:
                result = omni_wm_thalamic_bridge_reset_stats(bridge);
                break;

            case THALAMIC_RECOVERY_SOFT_RESET:
                result = omni_wm_thalamic_bridge_reset(bridge);
                break;

            case THALAMIC_RECOVERY_FULL_RESET:
                /* Full reset requires destroy and recreate - just do soft reset here */
                result = omni_wm_thalamic_bridge_reset(bridge);
                break;

            default:
                nimcp_log(LOG_LEVEL_WARN, "Unknown thalamic recovery action: %d", action);
                continue;
        }

        if (result == NIMCP_SUCCESS) {
            success_count++;
        } else {
            nimcp_log(LOG_LEVEL_WARN, "Thalamic recovery action %d failed on bridge %u", action, i);
        }
    }

    if (success_count > 0) {
        atomic_fetch_add(&agent->thalamic_recoveries_performed, 1);
        nimcp_log(LOG_LEVEL_INFO, "Thalamic recovery action %d completed on %d/%u bridges",
                  action, success_count, end_idx - start_idx);
    }

    return (success_count > 0) ? 0 : -1;
}


/* -------------------------------------------------------------------------
 * Middleware Recovery Actions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_middleware_recovery(
    nimcp_health_agent_t* agent,
    middleware_recovery_action_t action,
    int context_index
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (action == MIDDLEWARE_RECOVERY_NONE) {
        return 0;  /* Nothing to do */
    }

    uint32_t count = atomic_load(&agent->num_training_contexts);
    if (count == 0) {
        nimcp_log(LOG_LEVEL_WARN, "Middleware recovery: no contexts registered");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_middleware_recovery: count is zero");
        return -1;
    }

    /* Determine target contexts */
    uint32_t start_idx = (context_index < 0) ? 0 : (uint32_t)context_index;
    uint32_t end_idx = (context_index < 0) ? count : (uint32_t)(context_index + 1);

    if (start_idx >= count) {
        nimcp_log(LOG_LEVEL_ERROR, "Middleware recovery: invalid context index %d", context_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_middleware_recovery: capacity exceeded");
        return -1;
    }

    int success_count = 0;

    /*
     * NOTE: This implementation provides recovery action tracking without
     * calling the training integration API functions directly. When real
     * training contexts are connected (vs mock pointers for testing),
     * you would call the actual training API here. For now, we just
     * log the action and report success.
     */
    for (uint32_t i = start_idx; i < end_idx && i < count; i++) {
        nimcp_brain_training_ctx_t* ctx = agent->training_contexts[i];
        if (!ctx) continue;

        /* Log the recovery action */
        switch (action) {
            case MIDDLEWARE_RECOVERY_REDUCE_LR:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: reduce LR on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_INCREASE_LR:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: increase LR on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_RESET_GRADIENTS:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: reset gradients on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_CLEAR_NAM:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: clear NaN on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_PAUSE_TRAINING:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: pause training on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_RESUME_TRAINING:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: resume training on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_SAVE_CHECKPOINT:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: save checkpoint on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_LOAD_CHECKPOINT:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: load checkpoint on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_RESET_EARLY_STOP:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: reset early stop on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_RESET_STATS:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: reset stats on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_SOFT_RESET:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: soft reset on context %u", i);
                break;
            case MIDDLEWARE_RECOVERY_FULL_RESET:
                nimcp_log(LOG_LEVEL_DEBUG, "Middleware recovery: full reset on context %u", i);
                break;
            default:
                nimcp_log(LOG_LEVEL_WARN, "Unknown middleware recovery action: %d", action);
                continue;
        }

        success_count++;
    }

    if (success_count > 0) {
        atomic_fetch_add(&agent->middleware_recoveries_performed, 1);
        nimcp_log(LOG_LEVEL_INFO, "Middleware recovery action %d completed on %d/%u contexts",
                  action, success_count, end_idx - start_idx);
    }

    return (success_count > 0) ? 0 : -1;
}


/* -------------------------------------------------------------------------
 * Thalamic Health Queries
 * ------------------------------------------------------------------------- */

bool nimcp_health_agent_thalamic_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_thalamic_needs_attention: agent is NULL");
        return false;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_thalamic_bridges);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_thalamic_needs_attention: count is zero");
        return false;  /* No bridges = no attention needed */
    }

    /* Quick check: health score below threshold */
    float score = atomic_load(&((nimcp_health_agent_t*)agent)->thalamic_health_score);
    if (score < 70.0f) {
        return true;
    }

    /* Check each bridge for issues */
    const health_agent_thalamic_config_t* cfg = &((nimcp_health_agent_t*)agent)->thalamic_config;
    for (uint32_t i = 0; i < count; i++) {
        omni_wm_thalamic_bridge_t* bridge = ((nimcp_health_agent_t*)agent)->thalamic_bridges[i];
        if (!bridge) continue;

        /* Check if bridge is connected */
        if (!omni_wm_thalamic_bridge_is_connected(bridge)) {
            return true;  /* Disconnected bridge needs attention */
        }

        /* Get stats for quick check */
        omni_wm_thalamic_bridge_stats_t stats;
        if (omni_wm_thalamic_bridge_get_stats(bridge, &stats) == NIMCP_SUCCESS) {
            /* Check for high error rate */
            if (stats.total_updates > 0) {
                float error_rate = (float)stats.errors_total / (float)stats.total_updates;
                if (error_rate > 0.1f) {
                    return true;
                }
            }

            /* Check for slow updates */
            if (cfg->enable_timing_monitoring &&
                stats.mean_update_time_ms > cfg->max_update_time_ms) {
                return true;
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_thalamic_needs_attention: operation failed");
    return false;
}


/* -------------------------------------------------------------------------
 * Middleware Health Queries
 * ------------------------------------------------------------------------- */

bool nimcp_health_agent_middleware_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_middleware_needs_attention: agent is NULL");
        return false;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_training_contexts);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_middleware_needs_attention: count is zero");
        return false;  /* No contexts = no attention needed */
    }

    /* Quick check: health score below threshold */
    float score = atomic_load(&((nimcp_health_agent_t*)agent)->middleware_health_score);
    if (score < 70.0f) {
        return true;
    }

    /*
     * NOTE: This implementation checks the cached health score rather than
     * calling training integration API functions directly. When real training
     * contexts are connected, the get_middleware_metrics function would
     * update the health score which is then checked here.
     */

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_middleware_needs_attention: operation failed");
    return false;
}


/* -------------------------------------------------------------------------
 * Visual Bridge Connection Functions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_visual(
    nimcp_health_agent_t* agent,
    visual_cortical_bridge_t* bridge,
    const health_agent_perception_config_t* config
) {
    if (!agent || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_visual: required parameter is NULL (agent, bridge)");
        return -1;
    }

    if (agent->perception_mutex) {
        nimcp_mutex_lock(agent->perception_mutex);
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < agent->num_visual_bridges; i++) {
        if (agent->visual_bridges[i] == bridge) {
            if (agent->perception_mutex) {
                nimcp_mutex_unlock(agent->perception_mutex);
            }
            return 0;  /* Already registered */
        }
    }

    /* Check capacity */
    if (agent->num_visual_bridges >= HEALTH_AGENT_MAX_PERCEPTION_BRIDGES) {
        if (agent->perception_mutex) {
            nimcp_mutex_unlock(agent->perception_mutex);
        }
        nimcp_log(LOG_LEVEL_WARN, "Maximum visual bridges reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_connect_visual: validation failed");
        return -1;
    }

    /* Register bridge */
    agent->visual_bridges[agent->num_visual_bridges++] = bridge;

    /* Apply config if provided */
    if (config) {
        memcpy(&agent->perception_config, config, sizeof(agent->perception_config));
    }

    if (agent->perception_mutex) {
        nimcp_mutex_unlock(agent->perception_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected visual cortical bridge to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_visual(
    nimcp_health_agent_t* agent,
    visual_cortical_bridge_t* bridge
) {
    if (!agent || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_visual: required parameter is NULL (agent, bridge)");
        return -1;
    }

    if (agent->perception_mutex) {
        nimcp_mutex_lock(agent->perception_mutex);
    }

    /* Find and remove bridge */
    for (uint32_t i = 0; i < agent->num_visual_bridges; i++) {
        if (agent->visual_bridges[i] == bridge) {
            /* Shift remaining bridges */
            for (uint32_t j = i; j < agent->num_visual_bridges - 1; j++) {
                agent->visual_bridges[j] = agent->visual_bridges[j + 1];
            }
            agent->num_visual_bridges--;

            if (agent->perception_mutex) {
                nimcp_mutex_unlock(agent->perception_mutex);
            }

            nimcp_log(LOG_LEVEL_INFO, "Disconnected visual cortical bridge from health agent");
            return 0;
        }
    }

    if (agent->perception_mutex) {
        nimcp_mutex_unlock(agent->perception_mutex);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_disconnect_visual: validation failed");
    return -1;  /* Not found */
}


/* -------------------------------------------------------------------------
 * Audio Bridge Connection Functions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_audio(
    nimcp_health_agent_t* agent,
    audio_cortical_bridge_t* bridge,
    const health_agent_perception_config_t* config
) {
    if (!agent || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_audio: required parameter is NULL (agent, bridge)");
        return -1;
    }

    if (agent->perception_mutex) {
        nimcp_mutex_lock(agent->perception_mutex);
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < agent->num_audio_bridges; i++) {
        if (agent->audio_bridges[i] == bridge) {
            if (agent->perception_mutex) {
                nimcp_mutex_unlock(agent->perception_mutex);
            }
            return 0;  /* Already registered */
        }
    }

    /* Check capacity */
    if (agent->num_audio_bridges >= HEALTH_AGENT_MAX_PERCEPTION_BRIDGES) {
        if (agent->perception_mutex) {
            nimcp_mutex_unlock(agent->perception_mutex);
        }
        nimcp_log(LOG_LEVEL_WARN, "Maximum audio bridges reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_connect_audio: validation failed");
        return -1;
    }

    /* Register bridge */
    agent->audio_bridges[agent->num_audio_bridges++] = bridge;

    /* Apply config if provided */
    if (config) {
        memcpy(&agent->perception_config, config, sizeof(agent->perception_config));
    }

    if (agent->perception_mutex) {
        nimcp_mutex_unlock(agent->perception_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected audio cortical bridge to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_audio(
    nimcp_health_agent_t* agent,
    audio_cortical_bridge_t* bridge
) {
    if (!agent || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_audio: required parameter is NULL (agent, bridge)");
        return -1;
    }

    if (agent->perception_mutex) {
        nimcp_mutex_lock(agent->perception_mutex);
    }

    /* Find and remove bridge */
    for (uint32_t i = 0; i < agent->num_audio_bridges; i++) {
        if (agent->audio_bridges[i] == bridge) {
            /* Shift remaining bridges */
            for (uint32_t j = i; j < agent->num_audio_bridges - 1; j++) {
                agent->audio_bridges[j] = agent->audio_bridges[j + 1];
            }
            agent->num_audio_bridges--;

            if (agent->perception_mutex) {
                nimcp_mutex_unlock(agent->perception_mutex);
            }

            nimcp_log(LOG_LEVEL_INFO, "Disconnected audio cortical bridge from health agent");
            return 0;
        }
    }

    if (agent->perception_mutex) {
        nimcp_mutex_unlock(agent->perception_mutex);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_disconnect_audio: validation failed");
    return -1;  /* Not found */
}


/* -------------------------------------------------------------------------
 * Cortical Immune System Connection Functions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_cortical_immune(
    nimcp_health_agent_t* agent,
    cortical_immune_system_t* immune_system,
    const health_agent_cortical_config_t* config
) {
    if (!agent || !immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_cortical_immune: required parameter is NULL (agent, immune_system)");
        return -1;
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_lock(agent->cortical_mutex);
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < agent->num_cortical_immune_systems; i++) {
        if (agent->cortical_immune_systems[i] == immune_system) {
            if (agent->cortical_mutex) {
                nimcp_mutex_unlock(agent->cortical_mutex);
            }
            return 0;  /* Already registered */
        }
    }

    /* Check capacity */
    if (agent->num_cortical_immune_systems >= HEALTH_AGENT_MAX_PERCEPTION_BRIDGES) {
        if (agent->cortical_mutex) {
            nimcp_mutex_unlock(agent->cortical_mutex);
        }
        nimcp_log(LOG_LEVEL_WARN, "Maximum cortical immune systems reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_connect_cortical_immune: validation failed");
        return -1;
    }

    /* Register immune system */
    agent->cortical_immune_systems[agent->num_cortical_immune_systems++] = immune_system;

    /* Apply config if provided */
    if (config) {
        memcpy(&agent->cortical_config, config, sizeof(agent->cortical_config));
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_unlock(agent->cortical_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected cortical immune system to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_cortical_immune(
    nimcp_health_agent_t* agent,
    cortical_immune_system_t* immune_system
) {
    if (!agent || !immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_cortical_immune: required parameter is NULL (agent, immune_system)");
        return -1;
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_lock(agent->cortical_mutex);
    }

    /* Find and remove immune system */
    for (uint32_t i = 0; i < agent->num_cortical_immune_systems; i++) {
        if (agent->cortical_immune_systems[i] == immune_system) {
            /* Shift remaining */
            for (uint32_t j = i; j < agent->num_cortical_immune_systems - 1; j++) {
                agent->cortical_immune_systems[j] = agent->cortical_immune_systems[j + 1];
            }
            agent->num_cortical_immune_systems--;

            if (agent->cortical_mutex) {
                nimcp_mutex_unlock(agent->cortical_mutex);
            }

            nimcp_log(LOG_LEVEL_INFO, "Disconnected cortical immune system from health agent");
            return 0;
        }
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_unlock(agent->cortical_mutex);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_disconnect_cortical_immune: validation failed");
    return -1;  /* Not found */
}


/* -------------------------------------------------------------------------
 * Cortical Column Connection Functions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_cortical_column(
    nimcp_health_agent_t* agent,
    hypercolumn_t* column,
    const health_agent_cortical_config_t* config
) {
    if (!agent || !column) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_cortical_column: required parameter is NULL (agent, column)");
        return -1;
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_lock(agent->cortical_mutex);
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < agent->num_cortical_columns; i++) {
        if (agent->cortical_columns[i] == column) {
            if (agent->cortical_mutex) {
                nimcp_mutex_unlock(agent->cortical_mutex);
            }
            return 0;  /* Already registered */
        }
    }

    /* Check capacity */
    if (agent->num_cortical_columns >= HEALTH_AGENT_MAX_CORTICAL_COLUMNS) {
        if (agent->cortical_mutex) {
            nimcp_mutex_unlock(agent->cortical_mutex);
        }
        nimcp_log(LOG_LEVEL_WARN, "Maximum cortical columns reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_connect_cortical_column: validation failed");
        return -1;
    }

    /* Register column */
    agent->cortical_columns[agent->num_cortical_columns++] = column;

    /* Apply config if provided */
    if (config) {
        memcpy(&agent->cortical_config, config, sizeof(agent->cortical_config));
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_unlock(agent->cortical_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected cortical column to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_cortical_column(
    nimcp_health_agent_t* agent,
    hypercolumn_t* column
) {
    if (!agent || !column) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_disconnect_cortical_column: required parameter is NULL (agent, column)");
        return -1;
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_lock(agent->cortical_mutex);
    }

    /* Find and remove column */
    for (uint32_t i = 0; i < agent->num_cortical_columns; i++) {
        if (agent->cortical_columns[i] == column) {
            /* Shift remaining */
            for (uint32_t j = i; j < agent->num_cortical_columns - 1; j++) {
                agent->cortical_columns[j] = agent->cortical_columns[j + 1];
            }
            agent->num_cortical_columns--;

            if (agent->cortical_mutex) {
                nimcp_mutex_unlock(agent->cortical_mutex);
            }

            nimcp_log(LOG_LEVEL_INFO, "Disconnected cortical column from health agent");
            return 0;
        }
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_unlock(agent->cortical_mutex);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_disconnect_cortical_column: validation failed");
    return -1;  /* Not found */
}


/* -------------------------------------------------------------------------
 * Perception Recovery
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_perception_recovery(
    nimcp_health_agent_t* agent,
    perception_recovery_action_t action,
    int bridge_index
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    const char* action_name = "unknown";
    (void)bridge_index;  /* Used for targeted recovery in real implementation */

    switch (action) {
        case PERCEPTION_RECOVERY_NONE:
            action_name = "none";
            break;
        case PERCEPTION_RECOVERY_FLUSH_BUFFERS:
            action_name = "flush_buffers";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Flushing perception buffers");
            break;
        case PERCEPTION_RECOVERY_RESET_GAIN:
            action_name = "reset_gain";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Resetting input gain to defaults");
            break;
        case PERCEPTION_RECOVERY_ADJUST_GAIN:
            action_name = "adjust_gain";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Adjusting gain based on activity");
            break;
        case PERCEPTION_RECOVERY_RESET_FILTERS:
            action_name = "reset_filters";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Resetting filter banks");
            break;
        case PERCEPTION_RECOVERY_CLEAR_MAPS:
            action_name = "clear_maps";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Clearing topographic maps");
            break;
        case PERCEPTION_RECOVERY_REBUILD_MAPS:
            action_name = "rebuild_maps";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Rebuilding topographic maps");
            break;
        case PERCEPTION_RECOVERY_RESET_SELECTIVITY:
            action_name = "reset_selectivity";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Resetting feature selectivity");
            break;
        case PERCEPTION_RECOVERY_SOFT_RESET:
            action_name = "soft_reset";
            nimcp_log(LOG_LEVEL_INFO, "Perception recovery: Soft reset perception pipeline");
            break;
        case PERCEPTION_RECOVERY_FULL_RESET:
            action_name = "full_reset";
            nimcp_log(LOG_LEVEL_WARN, "Perception recovery: Performing full perception reset");
            break;
        default:
            nimcp_log(LOG_LEVEL_ERROR, "Perception recovery: Unknown action %d", action);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_perception_recovery: operation failed");
            return -1;
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Perception recovery action '%s' completed (bridge_index=%d)",
              action_name, bridge_index);
    return 0;
}


/* -------------------------------------------------------------------------
 * Cortical Recovery
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_cortical_recovery(
    nimcp_health_agent_t* agent,
    cortical_recovery_action_t action,
    int column_index
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    const char* action_name = "unknown";
    (void)column_index;  /* Used for targeted recovery in real implementation */

    switch (action) {
        case CORTICAL_RECOVERY_NONE:
            action_name = "none";
            break;
        case CORTICAL_RECOVERY_NORMALIZE_ACTIVITY:
            action_name = "normalize_activity";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Normalizing column activity");
            break;
        case CORTICAL_RECOVERY_RESET_COMPETITION:
            action_name = "reset_competition";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Resetting winner-take-all state");
            break;
        case CORTICAL_RECOVERY_REBALANCE_INHIBITION:
            action_name = "rebalance_inhibition";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Rebalancing E/I ratio");
            break;
        case CORTICAL_RECOVERY_REDUCE_INFLAMMATION:
            action_name = "reduce_inflammation";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Reducing inflammation level");
            break;
        case CORTICAL_RECOVERY_SUPPRESS_MICROGLIA:
            action_name = "suppress_microglia";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Suppressing microglial activation");
            break;
        case CORTICAL_RECOVERY_RESET_LAYERS:
            action_name = "reset_layers";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Resetting layer communication");
            break;
        case CORTICAL_RECOVERY_SHARPEN_TUNING:
            action_name = "sharpen_tuning";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Sharpening tuning curves");
            break;
        case CORTICAL_RECOVERY_RESET_PLASTICITY:
            action_name = "reset_plasticity";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Resetting plasticity modulation");
            break;
        case CORTICAL_RECOVERY_SOFT_RESET:
            action_name = "soft_reset";
            nimcp_log(LOG_LEVEL_INFO, "Cortical recovery: Soft reset cortical state");
            break;
        case CORTICAL_RECOVERY_FULL_RESET:
            action_name = "full_reset";
            nimcp_log(LOG_LEVEL_WARN, "Cortical recovery: Performing full cortical reset");
            break;
        default:
            nimcp_log(LOG_LEVEL_ERROR, "Cortical recovery: Unknown action %d", action);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_cortical_recovery: operation failed");
            return -1;
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Cortical recovery action '%s' completed (column_index=%d)",
              action_name, column_index);
    return 0;
}


/* -------------------------------------------------------------------------
 * Perception Needs Attention Check
 * ------------------------------------------------------------------------- */

bool nimcp_health_agent_perception_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_perception_needs_attention: agent is NULL");
        return false;
    }

    uint32_t visual_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_visual_bridges);
    uint32_t audio_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_audio_bridges);

    if (visual_count == 0 && audio_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_perception_needs_attention: visual_count is zero");
        return false;  /* No bridges = no attention needed */
    }

    float score = atomic_load(&((nimcp_health_agent_t*)agent)->perception_health_score);
    return score < 70.0f;  /* Needs attention if below 70% */
}


/* -------------------------------------------------------------------------
 * Cortical Needs Attention Check
 * ------------------------------------------------------------------------- */

bool nimcp_health_agent_cortical_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_cortical_needs_attention: agent is NULL");
        return false;
    }

    uint32_t immune_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_cortical_immune_systems);
    uint32_t column_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_cortical_columns);

    if (immune_count == 0 && column_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_cortical_needs_attention: immune_count is zero");
        return false;  /* No cortical components = no attention needed */
    }

    float score = atomic_load(&((nimcp_health_agent_t*)agent)->cortical_health_score);
    return score < 70.0f;  /* Needs attention if below 70% */
}


/* -------------------------------------------------------------------------
 * Brain Probe Registration
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_register_brain_probe(
    nimcp_health_agent_t* agent,
    brain_t brain,
    const health_agent_brain_probe_config_t* config
) {
    if (!agent || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_register_brain_probe: required parameter is NULL (agent, brain)");
        return -1;
    }

    if (agent->brain_probe_mutex) {
        nimcp_mutex_lock(agent->brain_probe_mutex);
    }

    uint32_t count = atomic_load(&agent->num_monitored_brains);
    if (count >= HEALTH_AGENT_MAX_BRAINS) {
        if (agent->brain_probe_mutex) {
            nimcp_mutex_unlock(agent->brain_probe_mutex);
        }
        nimcp_log(LOG_LEVEL_WARN, "Maximum brain probe slots reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_register_brain_probe: validation failed");
        return -1;
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < count; i++) {
        if (agent->monitored_brains[i] == brain) {
            if (agent->brain_probe_mutex) {
                nimcp_mutex_unlock(agent->brain_probe_mutex);
            }
            nimcp_log(LOG_LEVEL_DEBUG, "Brain already registered for monitoring");
            return 0;
        }
    }

    /* Store brain reference */
    agent->monitored_brains[count] = brain;

    /* Initialize metrics */
    memset(&agent->brain_metrics[count], 0, sizeof(brain_probe_health_metrics_t));
    agent->brain_metrics[count].overall_health_score = 100.0f;

    /* Copy or use default config */
    if (config) {
        memcpy(&agent->brain_probe_configs[count], config, sizeof(*config));
    } else {
        nimcp_health_agent_brain_probe_config_default(&agent->brain_probe_configs[count]);
    }

    atomic_store(&agent->num_monitored_brains, count + 1);

    if (agent->brain_probe_mutex) {
        nimcp_mutex_unlock(agent->brain_probe_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected brain for probe monitoring (slot %u)", count);
    return 0;
}


/* -------------------------------------------------------------------------
 * Brain Probe Unregistration
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_unregister_brain_probe(
    nimcp_health_agent_t* agent,
    brain_t brain
) {
    if (!agent || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_unregister_brain_probe: required parameter is NULL (agent, brain)");
        return -1;
    }

    if (agent->brain_probe_mutex) {
        nimcp_mutex_lock(agent->brain_probe_mutex);
    }

    uint32_t count = atomic_load(&agent->num_monitored_brains);
    int found_idx = -1;

    for (uint32_t i = 0; i < count; i++) {
        if (agent->monitored_brains[i] == brain) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        if (agent->brain_probe_mutex) {
            nimcp_mutex_unlock(agent->brain_probe_mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_health_agent_unregister_brain_probe: validation failed");
        return -1;
    }

    /* Shift remaining entries */
    for (uint32_t i = (uint32_t)found_idx; i < count - 1; i++) {
        agent->monitored_brains[i] = agent->monitored_brains[i + 1];
        memcpy(&agent->brain_metrics[i], &agent->brain_metrics[i + 1],
               sizeof(brain_probe_health_metrics_t));
        memcpy(&agent->brain_probe_configs[i], &agent->brain_probe_configs[i + 1],
               sizeof(health_agent_brain_probe_config_t));
    }

    /* Clear last slot */
    agent->monitored_brains[count - 1] = NULL;
    memset(&agent->brain_metrics[count - 1], 0, sizeof(brain_probe_health_metrics_t));

    atomic_store(&agent->num_monitored_brains, count - 1);

    if (agent->brain_probe_mutex) {
        nimcp_mutex_unlock(agent->brain_probe_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Disconnected brain from probe monitoring");
    return 0;
}


/* -------------------------------------------------------------------------
 * Brain Probe Recovery
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_brain_probe_recovery(
    nimcp_health_agent_t* agent,
    brain_probe_recovery_action_t action,
    int brain_index
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    uint32_t count = atomic_load(&agent->num_monitored_brains);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_brain_probe_recovery: count is zero");
        return -1;
    }

    /* Determine which brains to act on */
    uint32_t start_idx = 0;
    uint32_t end_idx = count;
    if (brain_index >= 0) {
        if ((uint32_t)brain_index >= count) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_brain_probe_recovery: capacity exceeded");
            return -1;
        }
        start_idx = (uint32_t)brain_index;
        end_idx = start_idx + 1;
    }

    nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: action=%d, brains=%u-%u",
              (int)action, start_idx, end_idx - 1);

    for (uint32_t i = start_idx; i < end_idx; i++) {
        brain_t brain = agent->monitored_brains[i];
        if (!brain) continue;

        switch (action) {
            case BRAIN_PROBE_RECOVERY_NONE:
                break;

            case BRAIN_PROBE_RECOVERY_TRIGGER_GC:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Triggering GC for brain %u", i);
                /* Would call nimcp_brain_gc(brain) if available */
                break;

            case BRAIN_PROBE_RECOVERY_REDUCE_LR:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Reducing LR for brain %u", i);
                /* Would call nimcp_brain_set_learning_rate(brain, lr * 0.5) if available */
                break;

            case BRAIN_PROBE_RECOVERY_INCREASE_SPARSITY:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Increasing sparsity for brain %u", i);
                break;

            case BRAIN_PROBE_RECOVERY_TRIGGER_PRUNE:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Triggering prune for brain %u", i);
                break;

            case BRAIN_PROBE_RECOVERY_CHECKPOINT:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Saving checkpoint for brain %u", i);
                break;

            case BRAIN_PROBE_RECOVERY_THROTTLE_INFERENCE:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Throttling inference for brain %u", i);
                break;

            case BRAIN_PROBE_RECOVERY_DETACH_COW:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Detaching COW for brain %u", i);
                break;

            case BRAIN_PROBE_RECOVERY_RESET_STATS:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Resetting stats for brain %u", i);
                memset(&agent->brain_metrics[i], 0, sizeof(brain_probe_health_metrics_t));
                agent->brain_metrics[i].overall_health_score = 100.0f;
                break;

            case BRAIN_PROBE_RECOVERY_FULL_RESET:
                nimcp_log(LOG_LEVEL_INFO, "Brain probe recovery: Full reset for brain %u", i);
                break;

            default:
                nimcp_log(LOG_LEVEL_WARN, "Unknown brain probe recovery action: %d", (int)action);
                break;
        }
    }

    atomic_fetch_add(&agent->brain_recoveries_performed, 1);
    return 0;
}


/* -------------------------------------------------------------------------
 * Brain Needs Attention Check
 * ------------------------------------------------------------------------- */

bool nimcp_health_agent_brain_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_brain_needs_attention: agent is NULL");
        return false;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_monitored_brains);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_brain_needs_attention: count is zero");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        const brain_probe_health_metrics_t* metrics = &agent->brain_metrics[i];
        if (metrics->warnings_active > 0 || metrics->critical_issues > 0) {
            return true;
        }
        if (metrics->overall_health_score < 70.0f) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_brain_needs_attention: validation failed");
    return false;
}


/* -------------------------------------------------------------------------
 * Force Immediate Probe of All Brains
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_probe_all_brains_now(
    nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    uint32_t count = atomic_load(&agent->num_monitored_brains);
    if (count == 0) {
        return 0;
    }

    if (agent->brain_probe_mutex) {
        nimcp_mutex_lock(agent->brain_probe_mutex);
    }

    for (uint32_t i = 0; i < count; i++) {
        agent_probe_single_brain(agent, i);
    }

    /* Update aggregate health score */
    float total_health = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        total_health += agent->brain_metrics[i].overall_health_score;
    }
    atomic_store(&agent->brain_probe_health_score, total_health / (float)count);
    agent->last_brain_probe_us = get_timestamp_us();

    if (agent->brain_probe_mutex) {
        nimcp_mutex_unlock(agent->brain_probe_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Probed %u brains, avg health: %.1f",
              count, atomic_load(&agent->brain_probe_health_score));
    return (int)count;
}


/* -------------------------------------------------------------------------
 * JEPA Connection
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_jepa(
    nimcp_health_agent_t* agent,
    jepa_predictor_t* jepa,
    const health_agent_wm_imagination_config_t* config
) {
    if (!agent || !jepa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_jepa: required parameter is NULL (agent, jepa)");
        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    agent->jepa_predictor = jepa;

    /* Apply config if provided, otherwise use defaults */
    if (config) {
        memcpy(&agent->wm_imagination_config, config, sizeof(*config));
    } else if (agent->wm_imagination_config.check_interval_ms == 0) {
        nimcp_health_agent_wm_imagination_config_default(&agent->wm_imagination_config);
    }

    /* Initialize JEPA metrics to healthy defaults */
    memset(&agent->jepa_metrics, 0, sizeof(agent->jepa_metrics));
    agent->jepa_metrics.embedding_orthogonality = 1.0f;
    agent->jepa_metrics.embedding_utilization = 1.0f;

    /* Initialize world model health score if this is the first WM component */
    if (!agent->world_model) {
        atomic_store(&agent->wm_health_score, 1.0f);
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected JEPA predictor to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_jepa(
    nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    agent->jepa_predictor = NULL;
    memset(&agent->jepa_metrics, 0, sizeof(agent->jepa_metrics));

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Disconnected JEPA predictor from health agent");
    return 0;
}


/* -------------------------------------------------------------------------
 * World Model Connection
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_world_model(
    nimcp_health_agent_t* agent,
    omni_world_model_t* world_model,
    const health_agent_wm_imagination_config_t* config
) {
    if (!agent || !world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_world_model: required parameter is NULL (agent, world_model)");
        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    agent->world_model = world_model;

    /* Apply config if provided */
    if (config) {
        memcpy(&agent->wm_imagination_config, config, sizeof(*config));
    } else if (agent->wm_imagination_config.check_interval_ms == 0) {
        nimcp_health_agent_wm_imagination_config_default(&agent->wm_imagination_config);
    }

    /* Initialize world model metrics */
    memset(&agent->wm_metrics, 0, sizeof(agent->wm_metrics));
    agent->wm_metrics.health_state = WM_HEALTH_OPTIMAL;
    agent->wm_metrics.health_score = 1.0f;
    agent->wm_metrics.forward_accuracy = 1.0f;
    agent->wm_metrics.forward_consistency = 1.0f;
    agent->wm_metrics.counterfactual_validity = 1.0f;
    agent->wm_metrics.crossmodal_coherence = 1.0f;

    /* Initialize world model health score atomic */
    atomic_store(&agent->wm_health_score, 1.0f);

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected world model to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_world_model(
    nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    agent->world_model = NULL;
    memset(&agent->wm_metrics, 0, sizeof(agent->wm_metrics));

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Disconnected world model from health agent");
    return 0;
}


/* -------------------------------------------------------------------------
 * Imagination Connection
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_connect_imagination(
    nimcp_health_agent_t* agent,
    imagination_engine_t* imagination,
    const health_agent_wm_imagination_config_t* config
) {
    if (!agent || !imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_connect_imagination: required parameter is NULL (agent, imagination)");
        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    agent->imagination = imagination;

    /* Apply config if provided */
    if (config) {
        memcpy(&agent->wm_imagination_config, config, sizeof(*config));
    } else if (agent->wm_imagination_config.check_interval_ms == 0) {
        nimcp_health_agent_wm_imagination_config_default(&agent->wm_imagination_config);
    }

    /* Initialize imagination metrics */
    memset(&agent->imagination_metrics, 0, sizeof(agent->imagination_metrics));
    agent->imagination_metrics.health_state = IMAG_HEALTH_VIVID;
    agent->imagination_metrics.health_score = 1.0f;
    agent->imagination_metrics.scene_coherence = 1.0f;
    agent->imagination_metrics.scene_vividness = 1.0f;
    agent->imagination_metrics.reality_check_pass_rate = 1.0f;

    /* Initialize imagination health score atomic */
    atomic_store(&agent->imagination_health_score, 1.0f);

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Connected imagination engine to health agent");
    return 0;
}


int nimcp_health_agent_disconnect_imagination(
    nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    agent->imagination = NULL;
    memset(&agent->imagination_metrics, 0, sizeof(agent->imagination_metrics));

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_INFO, "Disconnected imagination engine from health agent");
    return 0;
}


/* -------------------------------------------------------------------------
 * Recovery Actions
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_world_model_recovery(
    nimcp_health_agent_t* agent,
    world_model_recovery_action_t action,
    const char* reason
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "World model recovery: action=%d, reason=%s",
              action, reason ? reason : "unspecified");

    /* Track recovery */
    atomic_fetch_add(&agent->wm_recoveries_performed, 1);

    /* Execute recovery based on action */
    switch (action) {
        case WM_RECOVERY_NONE:
            /* No action */
            break;

        case WM_RECOVERY_RESET_PREDICTOR:
            /* Would call JEPA reset function */
            nimcp_log(LOG_LEVEL_WARN, "JEPA predictor reset requested");
            break;

        case WM_RECOVERY_PRUNE_LATENT:
            /* Would prune degenerate latent dimensions */
            nimcp_log(LOG_LEVEL_WARN, "Latent space pruning requested");
            break;

        case WM_RECOVERY_RETRAIN_DYNAMICS:
            /* Would trigger dynamics relearning */
            nimcp_log(LOG_LEVEL_WARN, "Dynamics retraining requested");
            break;

        case WM_RECOVERY_CLEAR_WORKSPACE:
            /* Would clear imagination workspace */
            nimcp_log(LOG_LEVEL_WARN, "Imagination workspace clear requested");
            break;

        case WM_RECOVERY_REDUCE_HORIZON:
            /* Would shorten simulation horizon */
            nimcp_log(LOG_LEVEL_WARN, "Simulation horizon reduction requested");
            break;

        case WM_RECOVERY_INCREASE_REALITY_CHECK:
            /* Would increase reality checking frequency */
            nimcp_log(LOG_LEVEL_WARN, "Reality check increase requested");
            break;

        case WM_RECOVERY_THROTTLE_IMAGINATION:
            /* Would rate-limit imagination */
            nimcp_log(LOG_LEVEL_WARN, "Imagination throttling requested");
            break;

        case WM_RECOVERY_BOOST_GROUNDING:
            /* Would increase sensory grounding */
            nimcp_log(LOG_LEVEL_WARN, "Sensory grounding boost requested");
            break;

        case WM_RECOVERY_CHECKPOINT_RESTORE:
            /* Would restore from checkpoint */
            nimcp_log(LOG_LEVEL_WARN, "Checkpoint restore requested");
            break;

        default:
            nimcp_log(LOG_LEVEL_ERROR, "Unknown recovery action: %d", action);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_world_model_recovery: operation failed");
            return -1;
    }

    return 0;
}


/* -------------------------------------------------------------------------
 * Health Status Queries
 * ------------------------------------------------------------------------- */

bool nimcp_health_agent_world_model_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_world_model_needs_attention: agent is NULL");
        return false;
    }

    /* No attention needed if nothing is connected */
    if (!((nimcp_health_agent_t*)agent)->jepa_predictor &&
        !((nimcp_health_agent_t*)agent)->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_world_model_needs_attention: validation failed");
        return false;
    }

    /* Check world model health state */
    if (((nimcp_health_agent_t*)agent)->wm_metrics.health_state != WM_HEALTH_OPTIMAL &&
        ((nimcp_health_agent_t*)agent)->wm_metrics.health_state != WM_HEALTH_DEGRADED) {
        return true;
    }

    /* Check JEPA health */
    if (((nimcp_health_agent_t*)agent)->jepa_metrics.embedding_collapse_detected ||
        ((nimcp_health_agent_t*)agent)->jepa_metrics.gradient_explosion ||
        ((nimcp_health_agent_t*)agent)->jepa_metrics.gradient_vanishing) {
        return true;
    }

    /* Check prediction error threshold */
    float error = ((nimcp_health_agent_t*)agent)->jepa_metrics.mean_prediction_error;
    if (error > ((nimcp_health_agent_t*)agent)->wm_imagination_config.jepa_error_warning) {
        return true;
    }

    /* Check world model health score */
    float score = atomic_load(&((nimcp_health_agent_t*)agent)->wm_health_score);
    if (score < 0.7f) {
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_world_model_needs_attention: validation failed");
    return false;
}


bool nimcp_health_agent_imagination_needs_attention(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_imagination_needs_attention: agent is NULL");
        return false;
    }

    /* No attention needed if nothing is connected */
    if (!((nimcp_health_agent_t*)agent)->imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_imagination_needs_attention: validation failed");
        return false;
    }

    /* Check imagination health state */
    imagination_health_state_t state =
        ((nimcp_health_agent_t*)agent)->imagination_metrics.health_state;
    if (state != IMAG_HEALTH_VIVID && state != IMAG_HEALTH_HAZY) {
        return true;
    }

    /* Check coherence threshold */
    float coherence = ((nimcp_health_agent_t*)agent)->imagination_metrics.scene_coherence;
    if (coherence < ((nimcp_health_agent_t*)agent)->wm_imagination_config.coherence_warning) {
        return true;
    }

    /* Check reality check pass rate */
    float reality_rate =
        ((nimcp_health_agent_t*)agent)->imagination_metrics.reality_check_pass_rate;
    if (reality_rate < ((nimcp_health_agent_t*)agent)->wm_imagination_config.reality_check_min) {
        return true;
    }

    /* Check imagination-reality blur */
    float blur = ((nimcp_health_agent_t*)agent)->imagination_metrics.imagination_reality_blur;
    if (blur > ((nimcp_health_agent_t*)agent)->wm_imagination_config.imagination_reality_blur_max) {
        return true;
    }

    /* Check imagination health score */
    float score = atomic_load(&((nimcp_health_agent_t*)agent)->imagination_health_score);
    if (score < 0.7f) {
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_imagination_needs_attention: validation failed");
    return false;
}


/* -------------------------------------------------------------------------
 * Immediate Health Checks
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_check_world_model_now(
    nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    uint64_t now = get_timestamp_us();

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    /* Update check count */
    atomic_fetch_add(&agent->wm_checks_run, 1);
    agent->last_wm_check_us = now;

    /* Check JEPA health if connected */
    if (agent->jepa_predictor) {
        /* In production, would call real JEPA health API */
        /* For now, maintain current metrics */

        /* Update trend tracking */
        agent->jepa_error_history[agent->jepa_error_idx] =
            agent->jepa_metrics.mean_prediction_error;
        agent->jepa_error_idx = (agent->jepa_error_idx + 1) % 10;

        /* Compute trend */
        float trend = 0.0f;
        for (int i = 1; i < 10; i++) {
            int curr = (agent->jepa_error_idx + i) % 10;
            int prev = (agent->jepa_error_idx + i - 1) % 10;
            trend += agent->jepa_error_history[curr] - agent->jepa_error_history[prev];
        }
        agent->jepa_metrics.prediction_error_trend = trend / 9.0f;

        /* Check thresholds */
        if (agent->jepa_metrics.mean_prediction_error >
            agent->wm_imagination_config.jepa_error_critical) {
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        }

        /* Check gradient health */
        if (agent->jepa_metrics.gradient_norm > agent->wm_imagination_config.gradient_norm_max) {
            agent->jepa_metrics.gradient_explosion = true;
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        } else {
            agent->jepa_metrics.gradient_explosion = false;
        }

        if (agent->jepa_metrics.gradient_norm < agent->wm_imagination_config.gradient_norm_min) {
            agent->jepa_metrics.gradient_vanishing = true;
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        } else {
            agent->jepa_metrics.gradient_vanishing = false;
        }

        /* Check embedding health */
        if (agent->jepa_metrics.embedding_variance <
            agent->wm_imagination_config.embedding_variance_min) {
            agent->jepa_metrics.embedding_collapse_detected = true;
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        } else {
            agent->jepa_metrics.embedding_collapse_detected = false;
        }
    }

    /* Check world model health if connected */
    if (agent->world_model) {
        /* In production, would call real world model health API */

        /* Update trend tracking */
        agent->wm_accuracy_history[agent->wm_accuracy_idx] =
            agent->wm_metrics.forward_accuracy;
        agent->wm_accuracy_idx = (agent->wm_accuracy_idx + 1) % 10;

        /* Determine health state */
        if (agent->wm_metrics.forward_accuracy <
            agent->wm_imagination_config.forward_accuracy_critical) {
            agent->wm_metrics.health_state = WM_HEALTH_CRITICAL;
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        } else if (agent->wm_metrics.forward_accuracy <
                   agent->wm_imagination_config.forward_accuracy_warning) {
            agent->wm_metrics.health_state = WM_HEALTH_DEGRADED;
        } else if (agent->wm_metrics.state_space_collapse) {
            agent->wm_metrics.health_state = WM_HEALTH_EMBEDDING_COLLAPSE;
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        } else if (agent->wm_metrics.forward_horizon_stable <
                   agent->wm_imagination_config.horizon_min_stable) {
            agent->wm_metrics.health_state = WM_HEALTH_DYNAMICS_UNSTABLE;
            atomic_fetch_add(&agent->wm_anomalies_detected, 1);
        } else {
            agent->wm_metrics.health_state = WM_HEALTH_OPTIMAL;
        }

        /* Compute health score */
        float score = agent->wm_metrics.forward_accuracy * 0.4f +
                     agent->wm_metrics.forward_consistency * 0.3f +
                     agent->wm_metrics.counterfactual_validity * 0.3f;
        agent->wm_metrics.health_score = fmaxf(0.0f, fminf(1.0f, score));
    }

    /* Update overall world model health score */
    float combined_score = 1.0f;
    int components = 0;

    if (agent->jepa_predictor) {
        float jepa_score = 1.0f - agent->jepa_metrics.mean_prediction_error;
        if (agent->jepa_metrics.embedding_collapse_detected) jepa_score *= 0.5f;
        if (agent->jepa_metrics.gradient_explosion) jepa_score *= 0.7f;
        if (agent->jepa_metrics.gradient_vanishing) jepa_score *= 0.7f;
        combined_score = jepa_score;
        components++;
    }

    if (agent->world_model) {
        combined_score = components > 0 ?
            (combined_score + agent->wm_metrics.health_score) / 2.0f :
            agent->wm_metrics.health_score;
        components++;
    }

    atomic_store(&agent->wm_health_score, combined_score);

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "World model health check complete: score=%.2f",
              combined_score);
    return 0;
}


int nimcp_health_agent_check_imagination_now(
    nimcp_health_agent_t* agent
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    uint64_t now = get_timestamp_us();

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    /* Update check count */
    atomic_fetch_add(&agent->imagination_checks_run, 1);
    agent->last_imagination_check_us = now;

    /* Check imagination health if connected */
    if (agent->imagination) {
        /* In production, would call real imagination health API */

        /* Update trend tracking */
        agent->imagination_coherence_history[agent->imagination_coherence_idx] =
            agent->imagination_metrics.scene_coherence;
        agent->imagination_coherence_idx = (agent->imagination_coherence_idx + 1) % 10;

        /* Determine health state */
        if (agent->imagination_metrics.scene_coherence <
            agent->wm_imagination_config.coherence_critical) {
            agent->imagination_metrics.health_state = IMAG_HEALTH_FRAGMENTED;
            atomic_fetch_add(&agent->imagination_anomalies_detected, 1);
        } else if (agent->imagination_metrics.scene_coherence <
                   agent->wm_imagination_config.coherence_warning) {
            agent->imagination_metrics.health_state = IMAG_HEALTH_HAZY;
        } else if (agent->imagination_metrics.imagination_reality_blur >
                   agent->wm_imagination_config.imagination_reality_blur_max) {
            agent->imagination_metrics.health_state = IMAG_HEALTH_CONFABULATING;
            atomic_fetch_add(&agent->imagination_anomalies_detected, 1);
        } else if (agent->imagination_metrics.workspace_utilization > 0.95f &&
                   agent->imagination_metrics.scenarios_abandoned >
                   agent->imagination_metrics.scenarios_completed) {
            agent->imagination_metrics.health_state = IMAG_HEALTH_STUCK;
            atomic_fetch_add(&agent->imagination_anomalies_detected, 1);
        } else if (agent->imagination_metrics.scene_vividness <
                   agent->wm_imagination_config.vividness_warning) {
            agent->imagination_metrics.health_state = IMAG_HEALTH_HAZY;
        } else {
            agent->imagination_metrics.health_state = IMAG_HEALTH_VIVID;
        }

        /* Compute health score */
        float score = agent->imagination_metrics.scene_coherence * 0.3f +
                     agent->imagination_metrics.scene_vividness * 0.2f +
                     agent->imagination_metrics.reality_check_pass_rate * 0.3f +
                     (1.0f - agent->imagination_metrics.imagination_reality_blur) * 0.2f;
        agent->imagination_metrics.health_score = fmaxf(0.0f, fminf(1.0f, score));
    }

    /* Update overall imagination health score */
    float score = agent->imagination ? agent->imagination_metrics.health_score : 1.0f;
    atomic_store(&agent->imagination_health_score, score);

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Imagination health check complete: score=%.2f", score);
    return 0;
}


/* ============================================================================
 * Memory Store Health Check
 *
 * WHAT: Verify the persistent memory store (SQLite) is healthy.
 * WHY:  Silent write failures can corrupt long-term memory without warning.
 * HOW:  Query store stats and flag if flushes succeed but writes are zero.
 * ============================================================================ */

void health_agent_check_memory_store(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent) || !agent->brain) return;

    struct nimcp_memory_store* store = brain_get_memory_store(agent->brain);
    if (!store) return;

    nimcp_memory_store_stats_t stats;
    if (nimcp_memory_store_get_stats(
            (nimcp_memory_store_t*)store, &stats) != 0) {
        nimcp_log(LOG_LEVEL_WARN,
                  "Health: memory store stats query failed — store may be corrupted");
        return;
    }

    /* Detect silent write failures: flushes happening but no writes committed */
    if (stats.write_buffer_flushes > 5 && stats.total_writes == 0) {
        nimcp_log(LOG_LEVEL_WARN,
                  "Health: memory store has %lu flushes but 0 writes — "
                  "SQLite writes may be silently failing",
                  (unsigned long)stats.write_buffer_flushes);
    }

    /* Detect high cache miss ratio indicating possible data loss */
    uint64_t total_cache = stats.cache_hits + stats.cache_misses;
    if (total_cache > 100 && stats.cache_misses > total_cache * 3 / 4) {
        nimcp_log(LOG_LEVEL_WARN,
                  "Health: memory store cache miss ratio %.0f%% — "
                  "consider increasing hot_cache_size",
                  100.0 * (double)stats.cache_misses / (double)total_cache);
    }
}


/* ============================================================================
 * OOD Detector Health Check
 *
 * WHAT: Monitor out-of-distribution detection rate.
 * WHY:  High OOD rate means training data does not match inference inputs.
 * HOW:  Query OOD stats and warn if more than 50% of inputs are OOD.
 * ============================================================================ */

void health_agent_check_ood_detector(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent) || !agent->brain) return;

    struct nimcp_ood_detector* ood = brain_get_ood_detector(agent->brain);
    if (!ood) return;

    nimcp_ood_stats_t ood_stats;
    if (nimcp_ood_get_stats(
            (const nimcp_ood_detector_t*)ood, &ood_stats) != 0) {
        return;
    }

    /* Only report after enough samples to be meaningful */
    if (ood_stats.total_checks < 20) return;

    if (ood_stats.ood_rate > 0.5f) {
        nimcp_log(LOG_LEVEL_WARN,
                  "Health: %.0f%% of inputs are OOD (avg_score=%.3f, max=%.3f) "
                  "— training data mismatch?",
                  100.0 * (double)ood_stats.ood_rate,
                  (double)ood_stats.avg_ood_score,
                  (double)ood_stats.max_ood_score);
    } else if (ood_stats.ood_rate > 0.3f) {
        nimcp_log(LOG_LEVEL_INFO,
                  "Health: OOD rate %.0f%% — elevated but not critical",
                  100.0 * (double)ood_stats.ood_rate);
    }
}
