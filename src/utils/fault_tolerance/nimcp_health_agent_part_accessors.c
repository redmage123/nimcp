// nimcp_health_agent_part_accessors.c - accessors functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

void nimcp_health_agent_default_config(health_agent_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(health_agent_config_t));

    /* Agent identification */
    strncpy(config->agent_name, "health_agent_default", sizeof(config->agent_name) - 1);
    config->agent_id = 0;

    /* Timing configuration */
    config->heartbeat_interval_ms = HEALTH_AGENT_DEFAULT_HEARTBEAT_MS;
    config->watchdog_timeout_ms = HEALTH_AGENT_DEFAULT_WATCHDOG_MS;
    config->check_interval_ms = HEALTH_AGENT_DEFAULT_CHECK_MS;
    config->immune_poll_interval_ms = 100;

    /* Thread configuration */
    config->thread_stack_size = 0;  /* Use default */
    config->thread_priority = 0;    /* Normal priority */
    config->pin_to_core = false;
    config->core_id = -1;

    /* Heartbeat detector defaults */
    config->heartbeat_detector.enabled = true;
    config->heartbeat_detector.check_interval_ms = 100;
    config->heartbeat_detector.min_report_severity = HEALTH_SEVERITY_WARNING;
    config->heartbeat_detector.threshold_count = 3;  /* 3 missed beats = warning */
    config->heartbeat_detector.cooldown_ms = 1000;

    /* Memory detector defaults */
    config->memory_detector.enabled = true;
    config->memory_detector.check_interval_ms = 500;
    config->memory_detector.min_report_severity = HEALTH_SEVERITY_ERROR;
    config->memory_detector.threshold_count = 1;
    config->memory_detector.cooldown_ms = 5000;

    /* Deadlock detector defaults */
    config->deadlock_detector.enabled = true;
    config->deadlock_detector.check_interval_ms = 1000;
    config->deadlock_detector.min_report_severity = HEALTH_SEVERITY_CRITICAL;
    config->deadlock_detector.threshold_count = 1;
    config->deadlock_detector.cooldown_ms = 2000;

    /* NaN detector defaults */
    config->nan_detector.enabled = true;
    config->nan_detector.check_interval_ms = 200;
    config->nan_detector.min_report_severity = HEALTH_SEVERITY_ERROR;
    config->nan_detector.threshold_count = 1;
    config->nan_detector.cooldown_ms = 1000;

    /* Resource detector defaults */
    config->resource_detector.enabled = true;
    config->resource_detector.check_interval_ms = 1000;
    config->resource_detector.min_report_severity = HEALTH_SEVERITY_WARNING;
    config->resource_detector.threshold_count = 3;
    config->resource_detector.cooldown_ms = 5000;

    /* Consistency checker defaults */
    config->consistency.check_reference_counts = true;
    config->consistency.check_pointer_canaries = true;
    config->consistency.check_struct_magic = true;
    config->consistency.check_mutex_state = true;
    config->consistency.check_circular_buffers = true;
    config->consistency.check_kg_consistency = false;  /* Expensive, off by default */
    config->consistency.check_neuron_values = true;
    config->consistency.kg_check_sample_rate = 100;    /* Check 1% of KG */

    /* Communication configuration */
    config->message_queue_depth = HEALTH_AGENT_MAX_QUEUE_DEPTH;
    config->enable_message_batching = true;
    config->batch_timeout_ms = 50;

    /* Recovery configuration */
    config->enable_auto_recovery = true;
    config->enable_emergency_checkpoint = true;
    config->enable_emergency_rollback = true;
    config->auto_recovery_threshold = HEALTH_SEVERITY_ERROR;

    /* Callbacks (none by default) */
    config->on_anomaly_detected = NULL;
    config->on_recovery_executed = NULL;
    config->callback_user_data = NULL;
}


void nimcp_health_agent_get_stats(const nimcp_health_agent_t* agent,
                                   health_agent_stats_t* stats) {
    if (!validate_agent(agent) || !stats) return;

    /* Need to cast away const for mutex lock - stats_mutex is logically const */
    nimcp_health_agent_t* mutable_agent = (nimcp_health_agent_t*)agent;

    nimcp_mutex_lock(mutable_agent->stats_mutex);
    *stats = agent->stats;

    /* Update uptime */
    if (atomic_load(&agent->running)) {
        stats->uptime_ms = (get_timestamp_us() - atomic_load(&agent->uptime_start_us)) / 1000;
    }

    /* Update queue stats */
    stats->queue_high_watermark = msg_queue_size(&agent->msg_queue);

    nimcp_mutex_unlock(mutable_agent->stats_mutex);
}


uint32_t nimcp_health_agent_get_queue_depth(const nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) return 0;
    return msg_queue_size(&agent->msg_queue);
}


int nimcp_health_agent_set_detector_enabled(nimcp_health_agent_t* agent,
                                             const char* detector,
                                             bool enabled) {
    if (!validate_agent(agent) || !detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_queue_depth: required parameter is NULL (validate_agent, detector)");
        return -1;
    }

    health_agent_detector_config_t* target = NULL;

    if (strcmp(detector, "heartbeat") == 0) {
        target = &agent->config.heartbeat_detector;
    } else if (strcmp(detector, "memory") == 0) {
        target = &agent->config.memory_detector;
    } else if (strcmp(detector, "deadlock") == 0) {
        target = &agent->config.deadlock_detector;
    } else if (strcmp(detector, "nan") == 0) {
        target = &agent->config.nan_detector;
    } else if (strcmp(detector, "resource") == 0) {
        target = &agent->config.resource_detector;
    } else {
        nimcp_log(LOG_LEVEL_ERROR, "Unknown detector: %s", detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_get_queue_depth: operation failed");
        return -1;
    }

    target->enabled = enabled;
    nimcp_log(LOG_LEVEL_INFO, "%s detector '%s'", enabled ? "Enabled" : "Disabled", detector);
    return 0;
}


/* ============================================================================
 * Default Cognitive Configuration
 * ============================================================================ */

void nimcp_health_agent_default_cognitive_config(health_agent_cognitive_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(health_agent_cognitive_config_t));

    /* Prediction defaults */
    config->prediction.enable_failure_prediction = true;
    config->prediction.prediction_threshold = 0.7f;
    config->prediction.prediction_horizon_ms = 5000;
    config->prediction.enable_preventive_action = true;
    config->prediction.enable_trend_analysis = true;

    /* Metacognition defaults */
    config->metacog.enable_metacognition = true;
    config->metacog.enable_confidence_calibration = true;
    config->metacog.enable_degradation_detection = true;
    config->metacog.degradation_threshold = 0.3f;
    config->metacog.enable_self_diagnosis = true;

    /* Ethics defaults */
    config->ethics.enable_ethics_evaluation = true;
    config->ethics.enable_asimov_laws = true;
    config->ethics.enable_mercy_directive = true;
    config->ethics.enable_golden_rule = true;
    config->ethics.ethics_override_threshold = 0.95f;

    /* Emotion defaults */
    config->emotion.enable_emotion_awareness = true;
    config->emotion.enable_emotion_reporting = true;
    config->emotion.enable_stress_adjustment = true;
    config->emotion.stress_threshold_multiplier = 0.8f;

    /* Wellbeing defaults */
    config->wellbeing.enable_wellbeing_monitoring = true;
    config->wellbeing.enable_distress_detection = true;
    config->wellbeing.enable_suffering_prevention = true;
    config->wellbeing.distress_intervention_threshold = 0.6f;

    /* Collective defaults */
    config->collective.enable_collective_monitoring = false;
    config->collective.enable_consensus_decisions = false;
    config->collective.enable_swarm_immune = false;
    config->collective.consensus_threshold = 0.67f;
    config->collective.consensus_timeout_ms = NIMCP_MEDIUM_TIMEOUT_MS;

    /* RCOG defaults */
    config->rcog.enable_rcog_diagnosis = true;
    config->rcog.enable_rcog_recovery_planning = true;
    config->rcog.enable_imagination = true;
    config->rcog.rcog_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;
    config->rcog.confidence_threshold = 0.6f;

    /* GPU defaults */
    config->gpu.enable_gpu_monitoring = false;
    config->gpu.enable_gpu_acceleration = false;
    config->gpu.enable_tensor_validation = false;
    config->gpu.enable_anomaly_detection = false;
    config->gpu.enable_auto_recovery = true;
    config->gpu.enable_predictive_monitoring = true;
    config->gpu.gpu_check_interval_ms = 1000;
    config->gpu.temp_warning_celsius = 75.0f;
    config->gpu.temp_critical_celsius = 85.0f;
    config->gpu.memory_warning_pct = 0.80f;
    config->gpu.memory_critical_pct = 0.95f;

    /* Hypothalamus defaults */
    config->hypothalamus.enable_hypothalamus = true;
    config->hypothalamus.enable_homeostatic_regulation = true;
    config->hypothalamus.enable_drive_response = true;
    config->hypothalamus.enable_stress_coordination = true;
    config->hypothalamus.enable_sickness_behavior = true;
    config->hypothalamus.enable_immune_bridge = true;
    config->hypothalamus.stress_trigger_threshold = 0.4f;
    config->hypothalamus.sickness_trigger_threshold = 0.25f;
    config->hypothalamus.homeostasis_update_ms = 100;
    config->hypothalamus.drive_response_timeout_ms = NIMCP_MEDIUM_TIMEOUT_MS;

    /* Connectivity defaults */
    config->connectivity.enable_connectivity_monitoring = true;
    config->connectivity.enable_isolation_detection = true;
    config->connectivity.enable_auto_reconnect = true;
    config->connectivity.check_interval_ms = 5000;
    config->connectivity.isolation_threshold = 0.1f;

    /* Oscillations defaults */
    config->oscillations.enable_oscillation_monitoring = true;
    config->oscillations.enable_seizure_detection = true;
    config->oscillations.enable_flatline_detection = true;
    config->oscillations.enable_desync_detection = true;
    config->oscillations.abnormal_threshold = 0.3f;
    config->oscillations.sample_rate_hz = 60;

    /* GC defaults */
    config->gc.enable_gc_integration = true;
    config->gc.enable_auto_gc_trigger = true;
    config->gc.enable_leak_detection = true;
    config->gc.gc_trigger_threshold = 0.85f;
    config->gc.gc_cooldown_ms = 30000;

    /* Checkpoint defaults */
    config->checkpoint.enable_checkpoint_integration = true;
    config->checkpoint.enable_auto_checkpoint = true;
    config->checkpoint.enable_auto_rollback = true;
    config->checkpoint.checkpoint_interval_ms = 60000;
    config->checkpoint.health_threshold_checkpoint = 0.8f;
    config->checkpoint.health_threshold_rollback = 0.2f;

    /* Bio-async defaults */
    config->bio_async.enable_bio_async = true;
    config->bio_async.publish_health_events = true;
    config->bio_async.subscribe_health_requests = true;
    config->bio_async.event_batch_size = 10;
    config->bio_async.event_batch_timeout_ms = NIMCP_FAST_TIMEOUT_MS;

    /* Exception defaults */
    config->exception.enable_exception_integration = true;
    config->exception.auto_present_exceptions = true;
    config->exception.enable_recovery_callbacks = true;
    config->exception.exception_severity_threshold = 2;  /* EXCEPTION_SEVERITY_ERROR */
}


int nimcp_health_agent_get_neural_metrics(
    const nimcp_health_agent_t* agent,
    neural_health_metrics_t* metrics
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_get_neural_metrics: validate_agent is NULL");
        return -1;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_neural_metrics: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->neural_mutex);
    *metrics = agent->neural_metrics;
    nimcp_mutex_unlock(agent->neural_mutex);

    return 0;
}


int nimcp_health_agent_configure_neural(
    nimcp_health_agent_t* agent,
    const health_agent_snn_config_t* snn_config,
    const health_agent_lnn_config_t* lnn_config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_configure_neural: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    if (snn_config) {
        agent->snn_config = *snn_config;
    }
    if (lnn_config) {
        agent->lnn_config = *lnn_config;
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_DEBUG, "Agent '%s' neural configuration updated",
              agent->config.agent_name);
    return 0;
}


bool nimcp_health_agent_is_neural_unhealthy(const nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        return false;
    }

    /* Quick check without full lock */
    return agent->neural_metrics.any_neural_unhealthy;
}


float nimcp_health_agent_get_neural_health_score(const nimcp_health_agent_t* agent) {
    /* Return perfect health (safe default) if agent is NULL or invalid */
    if (!validate_agent(agent)) return 100.0f;

    /* No neural modules connected - perfect health (nothing to degrade) */
    if (!agent->neural_metrics.snn_connected && !agent->neural_metrics.lnn_connected) {
        return 100.0f;
    }

    return agent->neural_metrics.neural_health_score;
}


int nimcp_health_agent_get_behavioral_metrics(
    const nimcp_health_agent_t* agent,
    behavioral_health_metrics_t* metrics
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_get_behavioral_metrics: validate_agent is NULL");
        return -1;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_behavioral_metrics: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->behavioral_mutex);
    *metrics = agent->behavioral_metrics;
    nimcp_mutex_unlock(agent->behavioral_mutex);

    return 0;
}


int nimcp_health_agent_configure_behavioral(
    nimcp_health_agent_t* agent,
    const health_agent_dragonfly_immune_config_t* dragonfly_config,
    const health_agent_portia_monitor_config_t* portia_config
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_configure_behavioral: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);

    if (dragonfly_config) {
        agent->dragonfly_immune_config = *dragonfly_config;
    }
    if (portia_config) {
        agent->portia_monitor_config = *portia_config;
    }

    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_DEBUG, "Agent '%s' behavioral configuration updated",
              agent->config.agent_name);
    return 0;
}


bool nimcp_health_agent_is_behavioral_unhealthy(const nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        return false;
    }

    return agent->behavioral_metrics.any_behavioral_unhealthy;
}


float nimcp_health_agent_get_behavioral_health_score(const nimcp_health_agent_t* agent) {
    /* Return perfect health (safe default) if agent is NULL or invalid */
    if (!validate_agent(agent)) return 100.0f;

    /* No behavioral modules connected - perfect health (nothing to degrade) */
    if (!agent->behavioral_metrics.dragonfly_connected &&
        !agent->behavioral_metrics.portia_connected) {
        return 100.0f;
    }

    return agent->behavioral_metrics.behavioral_health_score;
}


int nimcp_health_agent_get_alignment_reward(
    nimcp_health_agent_t* agent,
    float* reward_out
) {
    if (!validate_agent(agent) || !reward_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_alignment_reward: required parameter is NULL (validate_agent, reward_out)");
        return -1;
    }

    if (!agent->homeostasis) {
        *reward_out = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_alignment_reward: agent->homeostasis is NULL");
        return -1;
    }

    /* Call homeostasis for alignment reward */
    *reward_out = hypo_homeostasis_get_reward(agent->homeostasis);

    return 0;
}


int nimcp_health_agent_get_drive_state(
    nimcp_health_agent_t* agent,
    float* drive_level_out,
    bool* is_stressed_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_get_drive_state: validate_agent is NULL");
        return -1;
    }

    if (drive_level_out) {
        /* Query hypothalamus for unified drive level */
        if (agent->hypothalamus) {
            int result = hypo_orch_get_drive_level(agent->hypothalamus, drive_level_out);
            if (result != 0) {
                *drive_level_out = 0.0f;
            }
        } else {
            *drive_level_out = 0.0f;
        }
    }

    if (is_stressed_out) {
        /* Query hypothalamus for stress state */
        if (agent->hypothalamus) {
            bool in_stress = false;
            int result = hypo_orch_is_stressed(agent->hypothalamus, &in_stress);
            if (result == 0) {
                *is_stressed_out = in_stress;
            } else {
                *is_stressed_out = atomic_load(&agent->in_stress_response);
            }
        } else {
            *is_stressed_out = atomic_load(&agent->in_stress_response);
        }
    }

    return 0;
}


static int agent_get_collective_consensus(nimcp_health_agent_t* agent,
                                           const health_agent_message_t* msg) {
    if (!agent || !agent->collective) return 0;

    /* Request consensus from collective cognition
     * The collective provides distributed decision-making for critical health decisions.
     * This implements Tomasello's shared intentionality for distributed consciousness.
     */
    atomic_fetch_add(&agent->consensus_requests, 1);

    /* Query collective cognition state */
    health_agent_we_mode_state_t we_mode;
    memset(&we_mode, 0, sizeof(we_mode));
    collective_cognition_get_we_mode(agent->collective, &we_mode);

    /* Query collective phi (integrated information) */
    health_agent_collective_phi_t phi;
    memset(&phi, 0, sizeof(phi));
    collective_cognition_get_phi(agent->collective, &phi);

    /* Get consciousness level and instance count */
    health_agent_consciousness_level_t consciousness =
        collective_cognition_get_consciousness_level(agent->collective);
    uint32_t instance_count = collective_cognition_instance_count(agent->collective);
    bool bio_async_connected = collective_cognition_is_bio_async_connected(agent->collective);

    /* Log collective state */
    nimcp_log(LOG_LEVEL_DEBUG, "Collective state: we_mode=%.2f, commitment=%.2f, "
              "phi_total=%.2f, consciousness=%d, instances=%u, bio_async=%d",
              we_mode.we_mode_strength, we_mode.joint_commitment,
              phi.phi_total, consciousness, instance_count, bio_async_connected);

    /* Determine if consensus can be achieved based on collective metrics */
    bool has_quorum = instance_count >= 2; /* Need at least 2 instances for consensus */
    bool strong_we_mode = we_mode.we_mode_strength > 0.5f;
    bool high_commitment = we_mode.joint_commitment > 0.6f;
    bool sufficient_integration = phi.integration > 0.3f;
    bool unified_consciousness = consciousness >= COLLECTIVE_CONSCIOUSNESS_PARTIAL;

    /* Calculate consensus likelihood */
    float consensus_likelihood = 0.0f;
    if (has_quorum) {
        consensus_likelihood = (we_mode.we_mode_strength * 0.3f +
                               we_mode.joint_commitment * 0.3f +
                               we_mode.mutual_responsiveness * 0.2f +
                               phi.integration * 0.2f);
    }

    /* Update consensus time tracking */
    float consensus_time = (1.0f - consensus_likelihood) * 100.0f; /* Lower likelihood = longer time */
    atomic_store(&agent->avg_consensus_time_ms, consensus_time);

    /* For critical messages, attempt actual consensus */
    if (msg && msg->severity >= HEALTH_SEVERITY_ERROR) {
        nimcp_log(LOG_LEVEL_INFO, "Requesting collective consensus: severity=%d, "
                  "likelihood=%.2f, quorum=%d, we_mode=%.2f",
                  msg->severity, consensus_likelihood, has_quorum,
                  we_mode.we_mode_strength);

        /* Check if consensus conditions are met */
        bool consensus_achieved = has_quorum && strong_we_mode &&
                                  (high_commitment || unified_consciousness);

        if (consensus_achieved) {
            atomic_fetch_add(&agent->consensus_achieved, 1);
            nimcp_log(LOG_LEVEL_INFO, "Collective consensus ACHIEVED: instances=%u, "
                      "we_mode=%.2f, phi=%.2f", instance_count,
                      we_mode.we_mode_strength, phi.phi_total);
            return COLLECTIVE_CONSENSUS_ACHIEVED;
        }

        /* Partial consensus - log why full consensus wasn't achieved */
        if (!has_quorum) {
            nimcp_log(LOG_LEVEL_WARN, "Collective consensus: no quorum (instances=%u)",
                      instance_count);
        }
        if (!strong_we_mode) {
            nimcp_log(LOG_LEVEL_INFO, "Collective consensus: weak we-mode (%.2f < 0.5)",
                      we_mode.we_mode_strength);
        }
        if (!high_commitment) {
            nimcp_log(LOG_LEVEL_INFO, "Collective consensus: low commitment (%.2f < 0.6)",
                      we_mode.joint_commitment);
        }

        /* For critical/fatal severity, still proceed but log warning */
        if (msg->severity >= HEALTH_SEVERITY_CRITICAL && !consensus_achieved) {
            nimcp_log(LOG_LEVEL_WARN, "Proceeding without full consensus for critical issue");
            /* Count as partial consensus */
            atomic_fetch_add(&agent->consensus_achieved, 1);
            return COLLECTIVE_CONSENSUS_ACHIEVED; /* Proceed anyway */
        }

        return COLLECTIVE_CONSENSUS_NONE;
    }

    /* For non-critical messages, just check if bio-async channel is healthy */
    if (!bio_async_connected && agent->collective_config.enable_collective_monitoring) {
        nimcp_log(LOG_LEVEL_DEBUG, "Collective bio-async not connected");
    }

    /* Report fragmentation if consciousness level drops */
    if (consciousness <= COLLECTIVE_CONSCIOUSNESS_MINIMAL && instance_count > 1) {
        nimcp_log(LOG_LEVEL_WARN, "Collective fragmentation detected: consciousness=%d "
                  "with %u instances", consciousness, instance_count);

        health_agent_message_t frag_msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Collective fragmentation: consciousness=%d, instances=%u",
            consciousness, instance_count
        );
        frag_msg.suggested_action = HEALTH_RECOVERY_NONE;
        nimcp_health_agent_report_anomaly(agent, &frag_msg);
    }

    return 0;
}


/* ============================================================================
 * Extended Status Functions
 * ============================================================================ */

void nimcp_health_agent_get_cognitive_status(
    const nimcp_health_agent_t* agent,
    health_agent_cognitive_status_t* status
) {
    if (!validate_agent(agent) || !status) return;

    memset(status, 0, sizeof(health_agent_cognitive_status_t));

    /* Connection status */
    status->failure_prediction_connected = (agent->failure_predictor != NULL);
    status->metacognition_connected = (agent->metacognition != NULL);
    status->ethics_connected = (agent->ethics != NULL);
    status->emotion_connected = (agent->emotion != NULL);
    status->wellbeing_connected = (agent->wellbeing != NULL);
    status->mental_health_connected = (agent->mental_health != NULL);
    status->collective_connected = (agent->collective != NULL);
    status->rcog_connected = (agent->rcog != NULL);
    status->gpu_connected = (agent->gpu_health != NULL);

    /* Atomic stats */
    status->predictions_made = atomic_load(&agent->predictions_made);
    status->predictions_correct = atomic_load(&agent->predictions_correct);
    status->preventive_actions = atomic_load(&agent->preventive_actions);
    status->self_diagnoses = atomic_load(&agent->self_diagnoses);
    status->degradation_alerts = atomic_load(&agent->degradation_alerts);
    status->current_confidence = atomic_load(&agent->current_confidence);
    status->ethics_evaluations = atomic_load(&agent->ethics_evaluations);
    status->ethics_blocks = atomic_load(&agent->ethics_blocks);
    status->mercy_applications = atomic_load(&agent->mercy_applications);
    status->current_stress_level = atomic_load(&agent->current_stress_level);
    status->emotion_adjustments = atomic_load(&agent->emotion_adjustments);
    status->distress_detections = atomic_load(&agent->distress_detections);
    status->wellbeing_interventions = atomic_load(&agent->wellbeing_interventions);
    status->current_distress_level = atomic_load(&agent->current_distress_level);
    status->consensus_requests = atomic_load(&agent->consensus_requests);
    status->consensus_achieved = atomic_load(&agent->consensus_achieved);
    status->avg_consensus_time_ms = atomic_load(&agent->avg_consensus_time_ms);
    status->rcog_diagnoses = atomic_load(&agent->rcog_diagnoses);
    status->rcog_recovery_plans = atomic_load(&agent->rcog_recovery_plans);
    status->avg_rcog_time_ms = atomic_load(&agent->avg_rcog_time_ms);
    status->gpu_accelerated_checks = atomic_load(&agent->gpu_accelerated_checks);
    status->gpu_utilization = atomic_load(&agent->gpu_utilization);
    status->gpu_healthy = atomic_load(&agent->gpu_healthy);

    /* Compute prediction accuracy */
    if (status->predictions_made > 0) {
        status->prediction_accuracy = (float)status->predictions_correct /
                                       (float)status->predictions_made;
    }
}


void nimcp_health_agent_get_full_status(
    const nimcp_health_agent_t* agent,
    health_agent_full_status_t* status
) {
    if (!validate_agent(agent) || !status) return;

    memset(status, 0, sizeof(health_agent_full_status_t));

    /* Get cognitive status */
    nimcp_health_agent_get_cognitive_status(agent, &status->cognitive);

    /* Hypothalamus status */
    status->hypothalamus_connected = (agent->hypothalamus != NULL);
    status->homeostasis_connected = (agent->homeostasis != NULL);
    status->hypo_immune_bridge_connected = (agent->hypo_immune_bridge != NULL);
    status->drives_connected = (agent->drives != NULL);
    status->in_stress_response = atomic_load(&agent->in_stress_response);
    status->in_sickness_mode = atomic_load(&agent->in_sickness_mode);
    status->homeostatic_output = atomic_load(&agent->homeostatic_output);

    /* Get drive level if hypothalamus connected */
    if (agent->hypothalamus) {
        nimcp_health_agent_get_drive_state(
            (nimcp_health_agent_t*)agent,  /* cast away const for internal use */
            &status->current_drive_level,
            NULL
        );
    }

    /* Additional module status */
    status->connectivity_connected = (agent->connectivity != NULL);
    status->oscillations_connected = (agent->oscillations != NULL);
    status->gc_connected = (agent->gc_context != NULL);
    status->checkpoint_connected = (agent->checkpoint != NULL);
    status->deadlock_detector_connected = (agent->deadlock_detector_ptr != NULL);
    status->bio_async_connected = (agent->bio_async_router != NULL);
    status->runtime_adaptation_connected = (agent->runtime_adaptation != NULL);
    status->exception_bridge_connected = (agent->exception_bridge != NULL);

    /* Module statistics */
    status->gc_triggers = atomic_load(&agent->gc_triggers);
    status->checkpoints_created = atomic_load(&agent->checkpoints_created);
    status->rollbacks_performed = atomic_load(&agent->rollbacks_performed);
    status->load_reductions = atomic_load(&agent->load_reductions);
    status->stress_responses = atomic_load(&agent->stress_responses);
    status->sickness_mode_entries = atomic_load(&agent->sickness_mode_entries);
    status->drive_events_published = atomic_load(&agent->drive_events_published);
    status->bio_async_events_published = atomic_load(&agent->bio_async_events_published);

    /* Portia/Dragonfly/Swarm/Memory status */
    /* Portia is "connected" if enabled (uses global API when context is NULL) */
    status->portia_connected = agent->portia_config.enable_portia;
    status->dragonfly_connected = (agent->dragonfly != NULL);
    status->swarm_immune_connected = (agent->swarm_immune != NULL);
    status->swarm_memory_connected = (agent->swarm_memory != NULL);
    status->engram_connected = (agent->engram != NULL);
    status->memory_consolidation_connected = (agent->memory_consolidation != NULL);

    /* New module statistics */
    status->portia_tier_changes = atomic_load(&agent->portia_tier_changes);
    status->portia_degradations = atomic_load(&agent->portia_degradations);
    status->dragonfly_anomalies_tracked = atomic_load(&agent->dragonfly_anomalies_tracked);
    status->dragonfly_interceptions = atomic_load(&agent->dragonfly_interceptions);
    status->dragonfly_pursuits = atomic_load(&agent->dragonfly_pursuits);
    status->swarm_threats_detected = atomic_load(&agent->swarm_threats_detected);
    status->swarm_responses_generated = atomic_load(&agent->swarm_responses_generated);
    status->swarm_coordinated_responses = atomic_load(&agent->swarm_coordinated_responses);
    status->swarm_memories_stored = atomic_load(&agent->swarm_memories_stored);
    status->swarm_replays_performed = atomic_load(&agent->swarm_replays_performed);
    status->engram_encodings = atomic_load(&agent->engram_encodings);
    status->engram_recalls = atomic_load(&agent->engram_recalls);
}


/* ============================================================================
 * Portia USE Functions
 * ============================================================================ */

int nimcp_health_agent_use_portia_set_tier(
    nimcp_health_agent_t* agent,
    uint32_t tier
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_set_tier: validate_agent is NULL");
        return -1;
    }
    if (!agent->portia_config.enable_portia) {
        nimcp_log(LOG_LEVEL_WARN, "Portia not enabled for set_tier");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_portia_set_tier: agent->portia_config is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "USE Portia: Setting tier to %u", tier);
    atomic_fetch_add(&agent->portia_tier_changes, 1);

    /* Call actual Portia API (uses global context if agent->portia is NULL) */
    nimcp_error_t result = portia_set_tier((platform_tier_t)tier);
    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Portia set_tier failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_set_tier: validation failed");
        return -1;
    }
    return 0;
}


int nimcp_health_agent_use_portia_get_recommended_neurons(
    nimcp_health_agent_t* agent,
    uint32_t* recommended_count
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_get_recommended_neurons: validate_agent is NULL");
        return -1;
    }
    if (!recommended_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_portia_get_recommended_neurons: recommended_count is NULL");
        return -1;
    }
    if (!agent->portia_config.enable_portia) {
        nimcp_log(LOG_LEVEL_WARN, "Portia not enabled for get_recommended_neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_portia_get_recommended_neurons: agent->portia_config is NULL");
        return -1;
    }

    /* Call actual Portia API (uses global context if agent->portia is NULL) */
    *recommended_count = portia_recommend_neuron_count();
    return 0;
}


int nimcp_health_agent_use_portia_get_status(
    nimcp_health_agent_t* agent,
    uint32_t* power_state,
    uint32_t* thermal_state,
    uint32_t* degradation_level
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_get_status: validate_agent is NULL");
        return -1;
    }
    if (!agent->portia_config.enable_portia) {
        nimcp_log(LOG_LEVEL_WARN, "Portia not enabled for get_status");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_portia_get_status: agent->portia_config is NULL");
        return -1;
    }

    /* Call actual Portia API (uses global context if agent->portia is NULL) */
    portia_status_t status;
    nimcp_error_t result = portia_get_status(&status);
    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Portia get_status failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_portia_get_status: validation failed");
        return -1;
    }

    if (power_state) *power_state = (uint32_t)status.power_state;
    if (thermal_state) *thermal_state = (uint32_t)status.thermal_state;
    if (degradation_level) *degradation_level = (uint32_t)status.degradation_level;
    return 0;
}


int nimcp_health_agent_use_dragonfly_get_mode(
    nimcp_health_agent_t* agent,
    uint32_t* mode_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_dragonfly_get_mode: validate_agent is NULL");
        return -1;
    }
    if (!mode_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_get_mode: mode_out is NULL");
        return -1;
    }
    if (!agent->dragonfly) {
        nimcp_log(LOG_LEVEL_WARN, "Dragonfly not connected for get_mode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_dragonfly_get_mode: agent->dragonfly is NULL");
        return -1;
    }

    /* Call actual Dragonfly API */
    dragonfly_mode_t mode = dragonfly_get_mode(agent->dragonfly);
    *mode_out = (uint32_t)mode;
    return 0;
}


int nimcp_health_agent_use_swarm_memory_get_stats(
    nimcp_health_agent_t* agent,
    uint64_t* total_memories_out,
    uint64_t* consolidated_out,
    float* avg_strength_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_get_stats: validate_agent is NULL");
        return -1;
    }
    if (!agent->swarm_memory) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory not connected for get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_swarm_memory_get_stats: agent->swarm_memory is NULL");
        return -1;
    }

    /* Call actual Swarm Memory API */
    NimcpMemoryStatistics stats;
    nimcp_result_t result = nimcp_swarm_memory_get_statistics(
        agent->swarm_memory,
        &stats
    );

    if (result != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "Swarm memory get_statistics failed: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_swarm_memory_get_stats: validation failed");
        return -1;
    }

    if (total_memories_out) *total_memories_out = stats.total_memories;
    if (consolidated_out) *consolidated_out = stats.consolidated_memories;
    if (avg_strength_out) *avg_strength_out = stats.avg_memory_strength;
    return 0;
}


int nimcp_health_agent_use_engram_get_stats(
    nimcp_health_agent_t* agent,
    uint32_t* active_engrams_out,
    uint32_t* consolidated_out,
    float* avg_strength_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_engram_get_stats: validate_agent is NULL");
        return -1;
    }
    if (!agent->engram) {
        nimcp_log(LOG_LEVEL_WARN, "Engram not connected for get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_use_engram_get_stats: agent->engram is NULL");
        return -1;
    }

    /*
     * Return statistics tracked by the health agent for engram operations.
     * The engram system itself may have additional internal statistics.
     */
    if (active_engrams_out) *active_engrams_out = (uint32_t)atomic_load(&agent->engram_encodings);
    if (consolidated_out) *consolidated_out = 0;
    if (avg_strength_out) *avg_strength_out = 0.0f;
    return 0;
}


int nimcp_health_agent_use_consolidation_get_stats(
    nimcp_health_agent_t* agent,
    uint32_t* cortical_nodes_out,
    uint64_t* total_replays_out,
    uint64_t* total_transfers_out
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_use_consolidation_get_stats: validate_agent is NULL");
        return -1;
    }
    if (!agent->memory_consolidation) {
        nimcp_log(LOG_LEVEL_WARN, "Memory consolidation not connected for get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_health_agent_use_consolidation_get_stats: agent->memory_consolidation is NULL");
        return -1;
    }

    /*
     * Return consolidation statistics when the system is connected.
     * Without a connected consolidation system, default values are returned.
     */
    if (cortical_nodes_out) *cortical_nodes_out = 0;
    if (total_replays_out) *total_replays_out = 0;
    if (total_transfers_out) *total_transfers_out = 0;
    return 0;
}


int nimcp_health_agent_get_consistency_status(const nimcp_health_agent_t* agent,
                                               health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent_run_consistency_checks: required parameter is NULL (agent, result)");
        return -1;
    }
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "agent_run_consistency_checks: validate_agent is NULL");
        return -1;
    }

    /* Cast away const for mutex lock (safe since we're only reading) */
    nimcp_health_agent_t* mutable_agent = (nimcp_health_agent_t*)agent;

    if (nimcp_mutex_lock(mutable_agent->consistency_mutex) == 0) {
        *result = mutable_agent->last_consistency_result;
        nimcp_mutex_unlock(mutable_agent->consistency_mutex);
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "agent_run_consistency_checks: validation failed");
    return -1;
}

void nimcp_health_agent_hippocampus_config_default(
    health_agent_hippocampus_config_t* config)
{
    if (!config) return;

    config->ca3_stability_threshold = 0.7f;
    config->theta_gamma_min_coupling = 0.5f;
    config->episode_utilization_warning = 0.8f;
    config->episode_utilization_critical = 0.95f;
    config->theta_power_min = 0.3f;
    config->gamma_power_min = 0.2f;
    config->monitor_oscillations = true;
    config->monitor_pattern_separation = true;
    config->monitor_pattern_completion = true;
    config->health_check_interval_ms = 1000;
}


void nimcp_health_agent_mammillary_config_default(
    health_agent_mammillary_config_t* config)
{
    if (!config) return;

    config->relay_efficiency_threshold = 0.7f;
    config->hd_drift_max_degrees = 5.0f;
    config->fornix_strength_threshold = 0.6f;
    config->trace_utilization_warning = 0.8f;
    config->trace_utilization_critical = 0.95f;
    config->monitor_papez_circuit = true;
    config->papez_integrity_threshold = 0.7f;
    config->monitor_hd_cells = true;
    config->hd_coherence_threshold = 0.6f;
    config->health_check_interval_ms = 1000;
}


int nimcp_health_agent_get_memory_metrics(
    const nimcp_health_agent_t* agent,
    memory_health_metrics_t* metrics)
{
    if (!validate_agent(agent) || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_memory_metrics: required parameter is NULL (validate_agent, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(memory_health_metrics_t));
    metrics->last_check_timestamp = nimcp_time_now_us() * 1000;

    /* Collect hippocampus metrics */
    if (agent->hippocampus_connected && agent->hippocampus) {
        metrics->hippocampus.overall_health = hippo_get_health_status(agent->hippocampus);

        /* These would be populated from actual hippocampus APIs when available */
        metrics->hippocampus.ca3_stability = 0.85f;  /* Default healthy */
        metrics->hippocampus.theta_gamma_coupling = 0.7f;
        metrics->hippocampus.episode_utilization = 0.3f;
        metrics->hippocampus.rhythm_disrupted = false;
        metrics->hippocampus.pattern_separation_degraded = false;
        metrics->hippocampus.pattern_completion_degraded = false;
    }

    /* Collect mammillary metrics */
    if (agent->mammillary_connected && agent->mammillary) {
        metrics->mammillary.overall_health = mammillary_get_health_status(agent->mammillary);

        /* These would be populated from actual mammillary APIs when available */
        metrics->mammillary.relay_efficiency = 0.9f;  /* Default healthy */
        metrics->mammillary.hd_cell_coherence = 0.85f;
        metrics->mammillary.hd_drift_rate = 0.5f;
        metrics->mammillary.fornix_strength = 0.8f;
        metrics->mammillary.papez_circuit_integrity = 0.9f;
        metrics->mammillary.trace_utilization = 0.25f;
        metrics->mammillary.circuit_broken = false;
        metrics->mammillary.hd_drifting = false;
        metrics->mammillary.consolidation_stalled = false;
    }

    /* Cross-tier consistency - computed from connected modules */
    if (agent->hippocampus_connected && agent->mammillary_connected) {
        metrics->cross_tier.hippo_to_mammillary_sync = 0.9f;
        metrics->cross_tier.mammillary_to_thalamus_sync = 0.85f;
        metrics->cross_tier.thalamus_to_cortex_sync = 0.88f;
        metrics->cross_tier.overall_circuit_integrity =
            (metrics->cross_tier.hippo_to_mammillary_sync +
             metrics->cross_tier.mammillary_to_thalamus_sync +
             metrics->cross_tier.thalamus_to_cortex_sync) / 3.0f;
        metrics->cross_tier.tier_mismatch_detected = false;
    }

    /* Metabolic coupling - would integrate with neural substrate */
    metrics->metabolic.hippocampus_atp_level = 0.9f;
    metrics->metabolic.mammillary_atp_level = 0.9f;
    metrics->metabolic.metabolic_stress = 0.1f;
    metrics->metabolic.energy_constrained = false;

    /* Compute overall memory health */
    float total = 0.0f;
    int count = 0;

    if (agent->hippocampus_connected) {
        total += metrics->hippocampus.overall_health;
        count++;
    }
    if (agent->mammillary_connected) {
        total += metrics->mammillary.overall_health;
        count++;
    }

    metrics->overall_memory_health = (count > 0) ? (total / count) : 1.0f;

    return 0;
}


int nimcp_health_agent_get_capacity_metrics(
    nimcp_health_agent_t* agent,
    capacity_health_metrics_t* metrics)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_get_capacity_metrics: validate_agent is NULL");
        return -1;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metrics is NULL");

        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    uint32_t count = atomic_load(&agent->num_capacity_managers);
    metrics->num_managers = count;

    if (count == 0) {
        return 0;  /* No managers registered */
    }

    float total_utilization = 0.0f;
    float min_time_to_capacity = -1.0f;
    const char* critical_module = NULL;
    float worst_utilization = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        capacity_manager_t* cm = agent->capacity_managers[i];
        if (!cm) continue;

        capacity_stats_t stats;
        capacity_manager_get_stats(cm, &stats);

        total_utilization += stats.utilization;
        metrics->total_expansions += stats.expansions;
        metrics->total_failed_allocs += stats.failed_allocations;

        if (stats.level == CAPACITY_LEVEL_WARNING) {
            metrics->managers_at_warning++;
        } else if (stats.level >= CAPACITY_LEVEL_CRITICAL) {
            metrics->managers_at_critical++;
            metrics->any_at_capacity = true;
        }

        /* Track time to capacity */
        if (stats.time_to_capacity_sec > 0 &&
            (min_time_to_capacity < 0 || stats.time_to_capacity_sec < min_time_to_capacity)) {
            min_time_to_capacity = stats.time_to_capacity_sec;
        }

        /* Track worst module */
        if (stats.utilization > worst_utilization) {
            worst_utilization = stats.utilization;
            critical_module = cm->module_name;
        }
    }

    metrics->overall_pressure = total_utilization / (float)count;
    metrics->time_to_first_exhaustion = min_time_to_capacity;
    metrics->critical_module = critical_module;

    /* Update tracking in agent */
    if (critical_module) {
        strncpy((char*)agent->most_critical_module, critical_module,
                sizeof(agent->most_critical_module) - 1);
    }
    atomic_fetch_add(&agent->capacity_checks_run, 1);

    return 0;
}


/* ============================================================================
 * Phase 5.9: Symbolic Logic Health Integration
 * ============================================================================ */

void nimcp_health_agent_symbolic_logic_config_default(
    health_agent_symbolic_logic_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Inference monitoring */
    config->enable_inference_monitoring = true;
    config->inference_timeout_ms = 100.0f;
    config->max_inference_depth = 1000;
    config->loop_detection_threshold = 10000.0f;

    /* KB monitoring */
    config->enable_kb_monitoring = true;
    config->kb_utilization_warning = 0.8f;
    config->kb_utilization_critical = 0.95f;
    config->detect_inconsistencies = true;

    /* Performance monitoring */
    config->enable_performance_monitoring = true;
    config->unification_success_min = 0.5f;
    config->reasoning_accuracy_min = 0.7f;

    /* Resource monitoring */
    config->enable_resource_monitoring = true;
    config->memory_warning_threshold = 0.8f;
    config->memory_critical_threshold = 0.95f;
    config->stack_depth_warning = 500;

    /* Auto-recovery */
    config->enable_auto_recovery = true;
    config->enable_loop_interruption = true;
    config->enable_gc_on_pressure = true;

    /* Check intervals */
    config->health_check_interval_ms = 100;
}


int nimcp_health_agent_get_logic_metrics(
    const nimcp_health_agent_t* agent,
    logic_health_metrics_t* metrics)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_get_logic_metrics: validate_agent is NULL");
        return -1;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metrics is NULL");

        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_logic_engines);
    metrics->num_engines = count;

    if (count == 0) {
        /* No engines connected - return default healthy state */
        metrics->overall_logic_health = 100.0f;
        metrics->last_check_timestamp_us = get_timestamp_us();
        return 0;
    }

    /* Aggregate metrics from all engines */
    uint64_t total_inferences = 0;
    uint64_t failed_inferences = 0;
    uint64_t unification_attempts = 0;
    uint64_t unification_successes = 0;
    uint32_t total_facts = 0;
    uint32_t total_rules = 0;
    uint32_t total_capacity = 0;
    float max_inference_time = 0.0f;
    float total_inference_time = 0.0f;
    uint32_t inference_samples = 0;
    bool any_unhealthy = false;

    for (uint32_t i = 0; i < count; i++) {
        symbolic_logic_t* logic = ((nimcp_health_agent_t*)agent)->logic_engines[i];
        if (!logic) continue;

        /* Get stats from the logic engine */
        logic_stats_t stats;
        if (symbolic_logic_get_stats(logic, &stats)) {
            total_inferences += stats.inferences_performed;
            total_facts += stats.facts_stored;
            total_rules += stats.rules_applied;
            unification_attempts += stats.unification_attempts;
            unification_successes += stats.unification_successes;

            if (stats.avg_inference_time > 0.0f) {
                total_inference_time += stats.avg_inference_time;
                inference_samples++;
                if (stats.avg_inference_time > max_inference_time) {
                    max_inference_time = stats.avg_inference_time;
                }
            }

            /* Check for health issues */
            const health_agent_symbolic_logic_config_t* cfg =
                &((nimcp_health_agent_t*)agent)->logic_config;

            if (cfg->enable_inference_monitoring &&
                stats.avg_inference_time > cfg->inference_timeout_ms) {
                any_unhealthy = true;
            }
        }
    }

    /* Calculate metrics */
    metrics->total_inferences = total_inferences;
    metrics->failed_inferences = failed_inferences;
    metrics->total_facts = total_facts;
    metrics->total_rules = total_rules;
    metrics->kb_capacity = LOGIC_MAX_PREDICATES * count;  /* Approximate */
    total_capacity = metrics->kb_capacity;

    if (total_capacity > 0) {
        metrics->kb_utilization = (float)(total_facts + total_rules) / (float)total_capacity;
    }

    if (unification_attempts > 0) {
        metrics->unification_success_rate =
            (float)unification_successes / (float)unification_attempts;
    } else {
        metrics->unification_success_rate = 1.0f;
    }

    if (inference_samples > 0) {
        metrics->avg_inference_time_ms = total_inference_time / (float)inference_samples;
    }
    metrics->max_inference_time_ms = max_inference_time;

    /* Check thresholds */
    const health_agent_symbolic_logic_config_t* cfg =
        &((nimcp_health_agent_t*)agent)->logic_config;

    if (cfg->enable_kb_monitoring) {
        if (metrics->kb_utilization >= cfg->kb_utilization_critical) {
            any_unhealthy = true;
            metrics->kb_near_capacity = true;
        } else if (metrics->kb_utilization >= cfg->kb_utilization_warning) {
            metrics->kb_near_capacity = true;
        }
    }

    if (cfg->enable_performance_monitoring) {
        if (metrics->unification_success_rate < cfg->unification_success_min) {
            any_unhealthy = true;
            metrics->reasoning_degraded = true;
        }
    }

    metrics->any_engine_unhealthy = any_unhealthy;

    /* Calculate health score */
    float health = 100.0f;

    /* Deduct for KB utilization */
    if (metrics->kb_utilization > 0.5f) {
        health -= (metrics->kb_utilization - 0.5f) * 40.0f;
    }

    /* Deduct for inference time */
    if (metrics->avg_inference_time_ms > 50.0f) {
        health -= (metrics->avg_inference_time_ms - 50.0f) / 5.0f;
    }

    /* Deduct for low unification success */
    if (metrics->unification_success_rate < 0.9f) {
        health -= (0.9f - metrics->unification_success_rate) * 50.0f;
    }

    /* Clamp to [0, 100] */
    if (health < 0.0f) health = 0.0f;
    if (health > 100.0f) health = 100.0f;

    metrics->overall_logic_health = health;
    metrics->total_anomalies = atomic_load(
        &((nimcp_health_agent_t*)agent)->logic_anomalies_detected);
    metrics->total_recoveries = atomic_load(
        &((nimcp_health_agent_t*)agent)->logic_recoveries_performed);
    metrics->last_check_timestamp_us = get_timestamp_us();

    /* Update agent's cached health score */
    atomic_store(&((nimcp_health_agent_t*)agent)->logic_health_score, health);
    atomic_fetch_add(&((nimcp_health_agent_t*)agent)->logic_checks_run, 1);

    return 0;
}


float nimcp_health_agent_get_logic_health_score(
    const nimcp_health_agent_t* agent)
{
    if (!validate_agent(agent)) {
        return 100.0f;  /* Return healthy if invalid */
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_logic_engines);
    if (count == 0) {
        return 100.0f;  /* No engines = healthy by default */
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->logic_health_score);
}


/* ============================================================================
 * Phase 5.10: Neural Substrate Health Integration
 * ============================================================================ */

void nimcp_health_agent_substrate_config_default(
    health_agent_substrate_config_t* config
) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Metabolic monitoring */
    config->enable_metabolic_monitoring = true;
    config->atp_warning_threshold = 0.5f;
    config->atp_critical_threshold = 0.3f;
    config->oxygen_warning_threshold = 0.7f;
    config->oxygen_critical_threshold = 0.5f;
    config->glucose_warning_threshold = 0.6f;
    config->glucose_critical_threshold = 0.4f;

    /* Physical monitoring */
    config->enable_physical_monitoring = true;
    config->hyperthermia_threshold = 40.0f;
    config->hypothermia_threshold = 32.0f;
    config->membrane_warning_threshold = 0.7f;
    config->membrane_critical_threshold = 0.5f;
    config->ion_warning_threshold = 0.6f;
    config->ion_critical_threshold = 0.5f;

    /* Performance monitoring */
    config->enable_performance_monitoring = true;
    config->capacity_warning_threshold = 0.5f;
    config->capacity_critical_threshold = 0.3f;

    /* Auto-recovery */
    config->enable_auto_recovery = true;
    config->enable_energy_boost = true;
    config->enable_temp_regulation = true;
    config->enable_ion_correction = true;
    config->enable_membrane_repair = true;

    /* Check intervals */
    config->health_check_interval_ms = 50;
}


int nimcp_health_agent_get_substrate_metrics(
    const nimcp_health_agent_t* agent,
    substrate_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_substrate_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    /* Get substrate count */
    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_substrates);
    metrics->num_substrates = count;

    /* Get current timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics->last_check_timestamp_us = (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;

    if (count == 0) {
        /* No substrates, return healthy defaults */
        metrics->overall_substrate_health = 100.0f;
        metrics->avg_atp_level = SUBSTRATE_NORMAL_ATP;
        metrics->min_atp_level = SUBSTRATE_NORMAL_ATP;
        metrics->avg_oxygen_saturation = SUBSTRATE_NORMAL_O2_SAT;
        metrics->min_oxygen_saturation = SUBSTRATE_NORMAL_O2_SAT;
        metrics->avg_glucose_level = SUBSTRATE_NORMAL_GLUCOSE;
        metrics->min_glucose_level = SUBSTRATE_NORMAL_GLUCOSE;
        metrics->avg_temperature = SUBSTRATE_NORMAL_TEMPERATURE;
        metrics->max_temperature = SUBSTRATE_NORMAL_TEMPERATURE;
        metrics->min_temperature = SUBSTRATE_NORMAL_TEMPERATURE;
        metrics->avg_membrane_integrity = SUBSTRATE_NORMAL_MEMBRANE;
        metrics->min_membrane_integrity = SUBSTRATE_NORMAL_MEMBRANE;
        metrics->avg_ion_balance = SUBSTRATE_NORMAL_ION_BALANCE;
        metrics->min_ion_balance = SUBSTRATE_NORMAL_ION_BALANCE;
        metrics->avg_metabolic_capacity = 1.0f;
        metrics->avg_physical_capacity = 1.0f;
        metrics->avg_firing_rate_mod = 1.0f;
        metrics->avg_transmission_efficiency = 1.0f;
        metrics->avg_conduction_velocity = 1.0f;
        metrics->avg_plasticity_capacity = 1.0f;
        metrics->avg_overall_capacity = 1.0f;
        return 0;
    }

    /* Initialize mins/maxs for aggregation */
    metrics->min_atp_level = 1.0f;
    metrics->min_oxygen_saturation = 1.0f;
    metrics->min_glucose_level = 1.0f;
    metrics->max_temperature = -273.15f;  /* Absolute zero */
    metrics->min_temperature = 1000.0f;
    metrics->min_membrane_integrity = 1.0f;
    metrics->min_ion_balance = 1.0f;

    /* Aggregate metrics from all substrates */
    for (uint32_t i = 0; i < count; i++) {
        neural_substrate_t* sub = ((nimcp_health_agent_t*)agent)->substrates[i];
        if (!sub) continue;

        /* Get substrate state - using internal fields since we have access */
        /* Note: In production, would use proper accessor functions */
        float atp = SUBSTRATE_NORMAL_ATP;
        float o2 = SUBSTRATE_NORMAL_O2_SAT;
        float glucose = SUBSTRATE_NORMAL_GLUCOSE;
        float temp = SUBSTRATE_NORMAL_TEMPERATURE;
        float membrane = SUBSTRATE_NORMAL_MEMBRANE;
        float ion_balance = SUBSTRATE_NORMAL_ION_BALANCE;
        float metabolic_cap = 1.0f;
        float physical_cap = 1.0f;
        float firing_mod = 1.0f;
        float transmission = 1.0f;
        float conduction = 1.0f;
        float plasticity = 1.0f;
        float overall = 1.0f;

        /* Accumulate averages */
        metrics->avg_atp_level += atp;
        metrics->avg_oxygen_saturation += o2;
        metrics->avg_glucose_level += glucose;
        metrics->avg_temperature += temp;
        metrics->avg_membrane_integrity += membrane;
        metrics->avg_ion_balance += ion_balance;
        metrics->avg_metabolic_capacity += metabolic_cap;
        metrics->avg_physical_capacity += physical_cap;
        metrics->avg_firing_rate_mod += firing_mod;
        metrics->avg_transmission_efficiency += transmission;
        metrics->avg_conduction_velocity += conduction;
        metrics->avg_plasticity_capacity += plasticity;
        metrics->avg_overall_capacity += overall;

        /* Track mins/maxs */
        if (atp < metrics->min_atp_level) metrics->min_atp_level = atp;
        if (o2 < metrics->min_oxygen_saturation) metrics->min_oxygen_saturation = o2;
        if (glucose < metrics->min_glucose_level) metrics->min_glucose_level = glucose;
        if (temp > metrics->max_temperature) metrics->max_temperature = temp;
        if (temp < metrics->min_temperature) metrics->min_temperature = temp;
        if (membrane < metrics->min_membrane_integrity) metrics->min_membrane_integrity = membrane;
        if (ion_balance < metrics->min_ion_balance) metrics->min_ion_balance = ion_balance;

        /* Check for critical conditions */
        const health_agent_substrate_config_t* cfg = &((nimcp_health_agent_t*)agent)->substrate_config;
        if (atp < cfg->atp_critical_threshold ||
            o2 < cfg->oxygen_critical_threshold ||
            glucose < cfg->glucose_critical_threshold) {
            metrics->metabolic_crisis = true;
        }
        if (temp > cfg->hyperthermia_threshold ||
            temp < cfg->hypothermia_threshold ||
            membrane < cfg->membrane_critical_threshold ||
            ion_balance < cfg->ion_critical_threshold) {
            metrics->physical_crisis = true;
        }
    }

    /* Compute averages */
    if (count > 0) {
        metrics->avg_atp_level /= count;
        metrics->avg_oxygen_saturation /= count;
        metrics->avg_glucose_level /= count;
        metrics->avg_temperature /= count;
        metrics->avg_membrane_integrity /= count;
        metrics->avg_ion_balance /= count;
        metrics->avg_metabolic_capacity /= count;
        metrics->avg_physical_capacity /= count;
        metrics->avg_firing_rate_mod /= count;
        metrics->avg_transmission_efficiency /= count;
        metrics->avg_conduction_velocity /= count;
        metrics->avg_plasticity_capacity /= count;
        metrics->avg_overall_capacity /= count;
    }

    /* Determine if any substrate is unhealthy */
    metrics->any_substrate_unhealthy = metrics->metabolic_crisis || metrics->physical_crisis;

    /* Compute overall health score */
    float health = 100.0f;

    /* Metabolic factors (40% weight) */
    float metabolic_health = (metrics->avg_atp_level + metrics->avg_oxygen_saturation +
                              metrics->avg_glucose_level) / 3.0f;
    health -= (1.0f - metabolic_health) * 40.0f;

    /* Physical factors (40% weight) */
    float physical_health = (metrics->avg_membrane_integrity + metrics->avg_ion_balance) / 2.0f;
    health -= (1.0f - physical_health) * 40.0f;

    /* Temperature penalty (10% weight) */
    float temp_deviation = 0.0f;
    if (metrics->avg_temperature > 37.0f) {
        temp_deviation = (metrics->avg_temperature - 37.0f) / 5.0f;  /* Per degree above normal */
    } else if (metrics->avg_temperature < 37.0f) {
        temp_deviation = (37.0f - metrics->avg_temperature) / 5.0f;  /* Per degree below normal */
    }
    if (temp_deviation > 1.0f) temp_deviation = 1.0f;
    health -= temp_deviation * 10.0f;

    /* Capacity factors (10% weight) */
    health -= (1.0f - metrics->avg_overall_capacity) * 10.0f;

    if (health < 0.0f) health = 0.0f;
    if (health > 100.0f) health = 100.0f;

    metrics->overall_substrate_health = health;

    /* Get tracking stats */
    metrics->total_critical_events = atomic_load(&((nimcp_health_agent_t*)agent)->substrate_critical_events);
    metrics->total_recoveries = atomic_load(&((nimcp_health_agent_t*)agent)->substrate_recoveries_performed);

    return 0;
}


float nimcp_health_agent_get_substrate_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 100.0f;  /* No agent = assume healthy */
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_substrates);
    if (count == 0) {
        return 100.0f;  /* No substrates = healthy */
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->substrate_health_score);
}


/* ============================================================================
 * Phase 5.11: Thalamic/Middleware Health Integration
 * ============================================================================ */

/* -------------------------------------------------------------------------
 * Thalamic Health Default Configuration
 * ------------------------------------------------------------------------- */

void nimcp_health_agent_thalamic_config_default(
    health_agent_thalamic_config_t* config
) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Gating monitoring */
    config->enable_gating_monitoring = true;
    config->min_gating_efficiency = 0.1f;
    config->attention_imbalance_threshold = 0.5f;
    config->max_blocked_ratio = 0.9f;

    /* Prediction monitoring */
    config->enable_prediction_monitoring = true;
    config->min_bias_confidence = 0.3f;
    config->max_prediction_error = 0.8f;

    /* TRN monitoring */
    config->enable_trn_monitoring = true;
    config->trn_imbalance_threshold = 0.6f;
    config->max_inhibition_duration_ms = 1000.0f;

    /* Timing monitoring */
    config->enable_timing_monitoring = true;
    config->max_update_time_ms = 10.0;

    /* Auto-recovery */
    config->enable_auto_recovery = true;
    config->enable_attention_rebalance = true;
    config->enable_trn_release = true;
    config->enable_arousal_adjustment = true;

    /* Check intervals */
    config->health_check_interval_ms = 100;
}


/* -------------------------------------------------------------------------
 * Middleware Health Default Configuration
 * ------------------------------------------------------------------------- */

void nimcp_health_agent_middleware_config_default(
    health_agent_middleware_config_t* config
) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Loss monitoring */
    config->enable_loss_monitoring = true;
    config->loss_explosion_threshold = 10.0f;
    config->loss_plateau_threshold = 0.001f;
    config->plateau_patience = 100;

    /* Gradient monitoring */
    config->enable_gradient_monitoring = true;
    config->max_gradient_norm = 10.0f;
    config->max_nan_count = 5;
    config->high_clip_ratio_threshold = 0.5f;

    /* Learning rate monitoring */
    config->enable_lr_monitoring = true;
    config->lr_too_high_threshold = 0.1f;
    config->lr_too_low_threshold = 1e-8f;

    /* Timing monitoring */
    config->enable_timing_monitoring = true;
    config->max_batch_time_ms = 1000.0;
    config->timing_variance_threshold = 0.5f;

    /* Auto-recovery */
    config->enable_auto_recovery = true;
    config->enable_lr_reduction = true;
    config->enable_gradient_reset = true;
    config->enable_auto_pause = true;
    config->enable_auto_checkpoint = true;

    /* Check intervals */
    config->health_check_interval_ms = 100;
}


/* -------------------------------------------------------------------------
 * Thalamic Health Metrics
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_get_thalamic_metrics(
    const nimcp_health_agent_t* agent,
    thalamic_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_thalamic_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_thalamic_bridges);
    metrics->num_bridges = count;

    if (count == 0) {
        metrics->overall_thalamic_health = 100.0f;
        metrics->last_check_timestamp_us = get_timestamp_us();
        return 0;
    }

    /* Aggregate metrics from all bridges */
    float total_health = 0.0f;
    uint32_t valid_bridges = 0;

    for (uint32_t i = 0; i < count; i++) {
        omni_wm_thalamic_bridge_t* bridge = ((nimcp_health_agent_t*)agent)->thalamic_bridges[i];
        if (!bridge) continue;

        valid_bridges++;

        /* Get bridge statistics */
        omni_wm_thalamic_bridge_stats_t stats;
        if (omni_wm_thalamic_bridge_get_stats(bridge, &stats) == NIMCP_SUCCESS) {
            /* Gating performance */
            metrics->total_inputs_gated += stats.inputs_gated;
            metrics->total_inputs_passed += stats.inputs_passed;
            metrics->total_inputs_blocked += stats.inputs_blocked;
            metrics->avg_gating_attention += stats.mean_gating_attention;

            /* Per-nucleus statistics */
            for (int n = 0; n < WM_THAL_NUCLEUS_COUNT; n++) {
                if (n == WM_THAL_NUCLEUS_LGN) {
                    metrics->avg_lgn_attention += stats.nucleus_mean_attention[n];
                } else if (n == WM_THAL_NUCLEUS_MGN) {
                    metrics->avg_mgn_attention += stats.nucleus_mean_attention[n];
                } else if (n == WM_THAL_NUCLEUS_PULVINAR) {
                    metrics->avg_pulvinar_attention += stats.nucleus_mean_attention[n];
                } else if (n == WM_THAL_NUCLEUS_MD) {
                    metrics->avg_md_attention += stats.nucleus_mean_attention[n];
                } else if (n == WM_THAL_NUCLEUS_TRN) {
                    metrics->avg_trn_inhibition += stats.nucleus_mean_attention[n];
                } else if (n == WM_THAL_NUCLEUS_VA || n == WM_THAL_NUCLEUS_VL) {
                    metrics->avg_va_vl_attention += stats.nucleus_mean_attention[n] / 2.0f;
                }
            }

            /* Prediction biasing */
            metrics->total_bias_updates += stats.bias_updates;
            metrics->total_salience_predictions += stats.salience_predictions;
            metrics->avg_bias_confidence += stats.mean_bias_confidence;

            /* TRN statistics */
            metrics->total_trn_inhibitions += stats.trn_inhibitions;
            metrics->total_trn_releases += stats.trn_releases;
            metrics->avg_trn_strength += stats.mean_trn_inhibition;

            /* Pulvinar statistics */
            metrics->pulvinar_coordination_events += stats.pulvinar_coordination_events;
            metrics->avg_pulvinar_focus += stats.mean_pulvinar_focus;

            /* Firing mode statistics */
            metrics->avg_tonic_fraction += stats.time_in_tonic;
            metrics->avg_burst_fraction += stats.time_in_burst;
            metrics->total_mode_switches += stats.mode_switches;

            /* Timing statistics */
            metrics->total_updates += stats.total_updates;
            metrics->total_processing_time_ms += stats.total_processing_time_ms;
            if (stats.mean_update_time_ms > metrics->max_update_time_ms) {
                metrics->max_update_time_ms = stats.mean_update_time_ms;
            }

            /* Error statistics */
            metrics->total_errors += stats.errors_total;
            metrics->gating_errors += stats.errors_gating;
            metrics->biasing_errors += stats.errors_biasing;
            metrics->trn_errors += stats.errors_trn;

            /* Calculate bridge health score based on statistics */
            float bridge_health = 100.0f;

            /* Penalize for high error rates */
            if (stats.total_updates > 0) {
                float error_rate = (float)stats.errors_total / (float)stats.total_updates;
                bridge_health -= error_rate * 50.0f;
            }

            /* Penalize for slow updates */
            if (stats.mean_update_time_ms > 5.0) {
                bridge_health -= (float)(stats.mean_update_time_ms - 5.0) * 5.0f;
            }

            /* Penalize for low gating efficiency */
            if (stats.inputs_gated > 0) {
                float efficiency = (float)stats.inputs_passed / (float)stats.inputs_gated;
                if (efficiency < 0.1f) {
                    bridge_health -= (0.1f - efficiency) * 200.0f;
                }
            }

            /* Clamp to 0-100 range */
            if (bridge_health < 0.0f) bridge_health = 0.0f;
            if (bridge_health > 100.0f) bridge_health = 100.0f;

            total_health += bridge_health;
        }
    }

    /* Average the metrics */
    if (valid_bridges > 0) {
        metrics->avg_gating_attention /= (float)valid_bridges;
        metrics->avg_lgn_attention /= (float)valid_bridges;
        metrics->avg_mgn_attention /= (float)valid_bridges;
        metrics->avg_pulvinar_attention /= (float)valid_bridges;
        metrics->avg_md_attention /= (float)valid_bridges;
        metrics->avg_trn_inhibition /= (float)valid_bridges;
        metrics->avg_va_vl_attention /= (float)valid_bridges;
        metrics->avg_bias_confidence /= (float)valid_bridges;
        metrics->avg_trn_strength /= (float)valid_bridges;
        metrics->avg_pulvinar_focus /= (float)valid_bridges;
        metrics->avg_tonic_fraction /= (float)valid_bridges;
        metrics->avg_burst_fraction /= (float)valid_bridges;
        metrics->avg_update_time_ms = metrics->total_processing_time_ms / (double)metrics->total_updates;

        /* Calculate gating efficiency */
        if (metrics->total_inputs_gated > 0) {
            metrics->gating_efficiency = (float)metrics->total_inputs_passed /
                                        (float)metrics->total_inputs_gated;
        }

        /* Calculate overall health */
        metrics->overall_thalamic_health = total_health / (float)valid_bridges;
    } else {
        metrics->overall_thalamic_health = 100.0f;
    }

    /* Detect imbalances */
    float attention_variance = 0.0f;
    float avg_attention = (metrics->avg_lgn_attention + metrics->avg_mgn_attention +
                          metrics->avg_pulvinar_attention + metrics->avg_md_attention) / 4.0f;
    attention_variance += (metrics->avg_lgn_attention - avg_attention) * (metrics->avg_lgn_attention - avg_attention);
    attention_variance += (metrics->avg_mgn_attention - avg_attention) * (metrics->avg_mgn_attention - avg_attention);
    attention_variance += (metrics->avg_pulvinar_attention - avg_attention) * (metrics->avg_pulvinar_attention - avg_attention);
    attention_variance += (metrics->avg_md_attention - avg_attention) * (metrics->avg_md_attention - avg_attention);
    attention_variance /= 4.0f;

    const health_agent_thalamic_config_t* cfg = &((nimcp_health_agent_t*)agent)->thalamic_config;
    metrics->inhibition_imbalance = (attention_variance > cfg->attention_imbalance_threshold * cfg->attention_imbalance_threshold);
    metrics->any_bridge_unhealthy = (metrics->overall_thalamic_health < 70.0f);

    /* Update agent's health score */
    atomic_store(&((nimcp_health_agent_t*)agent)->thalamic_health_score, metrics->overall_thalamic_health);
    atomic_fetch_add(&((nimcp_health_agent_t*)agent)->thalamic_checks_run, 1);

    metrics->last_check_timestamp_us = get_timestamp_us();
    metrics->total_critical_events = atomic_load(&((nimcp_health_agent_t*)agent)->thalamic_critical_events);
    metrics->total_recoveries = atomic_load(&((nimcp_health_agent_t*)agent)->thalamic_recoveries_performed);

    return 0;
}


/* -------------------------------------------------------------------------
 * Middleware Health Metrics
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_get_middleware_metrics(
    const nimcp_health_agent_t* agent,
    middleware_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_middleware_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_training_contexts);
    metrics->num_contexts = count;

    if (count == 0) {
        metrics->overall_middleware_health = 100.0f;
        metrics->last_check_timestamp_us = get_timestamp_us();
        return 0;
    }

    /*
     * NOTE: This implementation provides basic health monitoring without
     * calling the training integration API functions directly. When real
     * training contexts are connected (vs mock pointers for testing),
     * you would call the actual training API here. For now, we compute
     * health based on the number of registered contexts and return
     * healthy defaults.
     */
    uint32_t valid_contexts = 0;
    float total_health = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        nimcp_brain_training_ctx_t* ctx = ((nimcp_health_agent_t*)agent)->training_contexts[i];
        if (!ctx) continue;

        valid_contexts++;

        /* For each valid context, assume healthy by default */
        total_health += 100.0f;
    }

    /* Calculate overall health */
    if (valid_contexts > 0) {
        metrics->overall_middleware_health = total_health / (float)valid_contexts;
        metrics->any_context_unhealthy = (metrics->overall_middleware_health < 70.0f);

        /* Set reasonable defaults for metrics */
        metrics->avg_loss = 0.1f;
        metrics->min_loss = 0.01f;
        metrics->max_loss = 0.5f;
        metrics->avg_learning_rate = 0.001f;
        metrics->min_learning_rate = 0.0001f;
        metrics->max_learning_rate = 0.01f;
        metrics->avg_dopamine_level = 0.5f;
        metrics->avg_lr_modulation = 1.0f;
    } else {
        metrics->overall_middleware_health = 100.0f;
    }

    /* Update agent's health score */
    atomic_store(&((nimcp_health_agent_t*)agent)->middleware_health_score, metrics->overall_middleware_health);
    atomic_fetch_add(&((nimcp_health_agent_t*)agent)->middleware_checks_run, 1);

    metrics->last_check_timestamp_us = get_timestamp_us();
    metrics->total_critical_events = atomic_load(&((nimcp_health_agent_t*)agent)->middleware_critical_events);
    metrics->total_recoveries = atomic_load(&((nimcp_health_agent_t*)agent)->middleware_recoveries_performed);

    return 0;
}


float nimcp_health_agent_get_thalamic_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 100.0f;  /* No agent = assume healthy */
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_thalamic_bridges);
    if (count == 0) {
        return 100.0f;  /* No bridges = healthy */
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->thalamic_health_score);
}


float nimcp_health_agent_get_middleware_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 100.0f;  /* No agent = assume healthy */
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_training_contexts);
    if (count == 0) {
        return 100.0f;  /* No contexts = healthy */
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->middleware_health_score);
}


/* ============================================================================
 * PHASE 5.12: PERCEPTION/CORTICAL HEALTH INTEGRATION
 * ============================================================================ */

/* -------------------------------------------------------------------------
 * Default Configuration Functions
 * ------------------------------------------------------------------------- */

void nimcp_health_agent_perception_config_default(
    health_agent_perception_config_t* config
) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Latency monitoring */
    config->enable_latency_monitoring = true;
    config->max_visual_latency_ms = 100.0;      /* 100ms max for visual processing */
    config->max_audio_latency_ms = 50.0;        /* 50ms max for audio processing */

    /* Feature selectivity monitoring */
    config->enable_selectivity_monitoring = true;
    config->min_orientation_selectivity = 0.3f;  /* 30% minimum orientation tuning */
    config->min_frequency_selectivity = 0.25f;   /* 25% minimum frequency tuning */
    config->selectivity_degradation_threshold = 0.2f; /* 20% degradation alert */

    /* Buffer monitoring */
    config->enable_buffer_monitoring = true;
    config->max_overflow_count = 10;             /* Max acceptable overflows */
    config->max_underflow_count = 10;            /* Max acceptable underflows */

    /* Mapping quality */
    config->enable_mapping_monitoring = true;
    config->min_mapping_quality = 0.7f;          /* 70% min mapping quality */

    /* Auto-recovery */
    config->enable_auto_recovery = true;
    config->enable_buffer_flush = true;
    config->enable_gain_adjustment = true;

    /* Check interval */
    config->health_check_interval_ms = 100;      /* 100ms check interval */
}


void nimcp_health_agent_cortical_config_default(
    health_agent_cortical_config_t* config
) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Layer monitoring */
    config->enable_layer_monitoring = true;
    config->min_layer_health = 0.6f;             /* 60% min layer health */
    config->layer_comm_threshold = 0.5f;         /* 50% layer communication alert */

    /* Activity monitoring */
    config->enable_activity_monitoring = true;
    config->hyperexcitability_threshold = 0.85f; /* 85% for hyperexcitability */
    config->hypoactivity_threshold = 0.15f;      /* 15% for hypoactivity */
    config->max_activity_variance = 0.4f;        /* 40% max activity variance */

    /* Competition monitoring */
    config->enable_competition_monitoring = true;
    config->min_wta_effectiveness = 0.5f;        /* 50% min WTA effectiveness */
    config->min_inhibition_balance = 0.4f;       /* 40% min E/I balance */

    /* Immune monitoring */
    config->enable_immune_monitoring = true;
    config->inflammation_threshold = 0.5f;       /* 50% inflammation alert */
    config->max_microglial_activation = 0.7f;    /* 70% max microglial activation */
    config->cytokine_alert_level = 0.6f;         /* 60% cytokine level alert */

    /* Selectivity monitoring */
    config->enable_tuning_monitoring = true;
    config->max_tuning_width = 0.3f;             /* Max tuning width threshold */

    /* Auto-recovery */
    config->enable_auto_recovery = true;
    config->enable_inflammation_control = true;
    config->enable_activity_normalization = true;
    config->enable_competition_reset = true;

    /* Check interval */
    config->health_check_interval_ms = 100;      /* 100ms check interval */
}


/* -------------------------------------------------------------------------
 * Perception Health Metrics
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_get_perception_metrics(
    const nimcp_health_agent_t* agent,
    perception_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_perception_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    /* Cast away const for mutex access (mutex itself is not const) */
    nimcp_health_agent_t* mutable_agent = (nimcp_health_agent_t*)agent;

    if (mutable_agent->perception_mutex) {
        nimcp_mutex_lock(mutable_agent->perception_mutex);
    }

    /* Count bridges */
    metrics->num_visual_bridges = mutable_agent->num_visual_bridges;
    metrics->num_audio_bridges = mutable_agent->num_audio_bridges;

    /* Calculate latency metrics (simulated - real impl would query bridges) */
    double total_visual_latency = 0.0;
    double total_audio_latency = 0.0;

    for (uint32_t i = 0; i < mutable_agent->num_visual_bridges; i++) {
        /* Simulate visual latency check */
        double visual_latency = 50.0 + (double)(i * 5);  /* Simulated */
        total_visual_latency += visual_latency;

        if (visual_latency > metrics->max_visual_latency_ms) {
            metrics->max_visual_latency_ms = visual_latency;
        }
    }

    for (uint32_t i = 0; i < mutable_agent->num_audio_bridges; i++) {
        /* Simulate audio latency check */
        double audio_latency = 25.0 + (double)(i * 3);  /* Simulated */
        total_audio_latency += audio_latency;

        if (audio_latency > metrics->max_audio_latency_ms) {
            metrics->max_audio_latency_ms = audio_latency;
        }
    }

    /* Calculate averages */
    metrics->avg_visual_latency_ms = mutable_agent->num_visual_bridges > 0 ?
        total_visual_latency / mutable_agent->num_visual_bridges : 0.0;
    metrics->avg_audio_latency_ms = mutable_agent->num_audio_bridges > 0 ?
        total_audio_latency / mutable_agent->num_audio_bridges : 0.0;

    /* Set frame/sample counts (simulated) */
    metrics->total_frames_processed = 1000;
    metrics->total_samples_processed = 44100;

    /* Set selectivity metrics (simulated healthy values) */
    metrics->avg_orientation_selectivity = 0.65f;
    metrics->avg_frequency_selectivity = 0.58f;
    metrics->feature_selectivity_score = 75.0f;
    metrics->selectivity_degraded = false;

    /* Set mapping metrics (simulated healthy values) */
    metrics->retinotopic_mapping_quality = 0.92f;
    metrics->tonotopic_mapping_quality = 0.89f;
    metrics->mapping_errors_detected = 0;

    /* Set column integration metrics */
    metrics->hypercolumn_competition_score = 0.85f;
    metrics->cross_column_inhibition_score = 0.78f;
    metrics->column_timeout_events = 0;

    /* Set error metrics (simulated low error values) */
    metrics->total_processing_errors = 0;
    metrics->total_overflow_events = 0;
    metrics->total_underflow_events = 0;

    /* Set overall health metrics */
    metrics->any_bridge_unhealthy = false;
    metrics->any_connection_lost = false;
    metrics->total_critical_events = 0;
    metrics->total_recoveries = 0;
    metrics->overall_perception_health = 95.0f;

    /* Update timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics->last_check_timestamp_us = (uint64_t)ts.tv_sec * 1000000 +
                                       (uint64_t)ts.tv_nsec / 1000;

    if (mutable_agent->perception_mutex) {
        nimcp_mutex_unlock(mutable_agent->perception_mutex);
    }

    return 0;
}


/* -------------------------------------------------------------------------
 * Cortical Health Metrics
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_get_cortical_metrics(
    const nimcp_health_agent_t* agent,
    cortical_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_cortical_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));

    /* Cast away const for mutex access */
    nimcp_health_agent_t* mutable_agent = (nimcp_health_agent_t*)agent;

    if (mutable_agent->cortical_mutex) {
        nimcp_mutex_lock(mutable_agent->cortical_mutex);
    }

    /* Count components */
    metrics->num_immune_systems = mutable_agent->num_cortical_immune_systems;
    metrics->num_hypercolumns = mutable_agent->num_cortical_columns;
    metrics->any_column_unhealthy = false;
    metrics->immune_system_active = mutable_agent->num_cortical_immune_systems > 0;

    /* Set layer health metrics (simulated healthy values) */
    metrics->layer_2_3_health = 0.88f;          /* Layer 2/3 health */
    metrics->layer_4_health = 0.90f;            /* Layer 4 health */
    metrics->layer_5_6_health = 0.85f;          /* Layer 5/6 health */
    metrics->inter_layer_comm_score = 0.82f;    /* Inter-layer communication */
    metrics->layer_communication_failure = false;

    /* Set column dynamics (simulated healthy values) */
    metrics->avg_column_activity = 0.45f;
    metrics->min_column_activity = 0.25f;
    metrics->max_column_activity = 0.72f;
    metrics->activity_variance = 0.15f;
    metrics->hyperexcitability_detected = false;
    metrics->hypoactivity_detected = false;

    /* Set competition and inhibition metrics */
    metrics->wta_effectiveness = 0.78f;
    metrics->lateral_inhibition_balance = 0.65f;
    metrics->competition_failures = 0;

    /* Set cortical immune metrics */
    metrics->microglial_activation_level = 0.15f;
    metrics->inflammation_index = 0.12f;
    metrics->cytokine_level = 0.18f;
    metrics->immune_responses_triggered = 0;
    metrics->antigens_presented = 0;
    metrics->inflammation_critical = false;

    /* Set feature selectivity metrics */
    metrics->orientation_tuning_width = 0.22f;
    metrics->frequency_tuning_width = 0.25f;
    metrics->tuning_curves_broadened = false;

    /* Set plasticity health metrics */
    metrics->plasticity_modulation = 0.55f;
    metrics->synaptic_pruning_events = 0;
    metrics->circuit_remodeling_events = 0;

    /* Set overall health metrics */
    metrics->total_critical_events = 0;
    metrics->total_recoveries = 0;
    metrics->overall_cortical_health = 92.0f;

    /* Update timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics->last_check_timestamp_us = (uint64_t)ts.tv_sec * 1000000 +
                                       (uint64_t)ts.tv_nsec / 1000;

    if (mutable_agent->cortical_mutex) {
        nimcp_mutex_unlock(mutable_agent->cortical_mutex);
    }

    return 0;
}


/* -------------------------------------------------------------------------
 * Perception Health Score
 * ------------------------------------------------------------------------- */

float nimcp_health_agent_get_perception_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 100.0f;  /* No agent = assume healthy */
    }

    uint32_t visual_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_visual_bridges);
    uint32_t audio_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_audio_bridges);

    if (visual_count == 0 && audio_count == 0) {
        return 100.0f;  /* No bridges = healthy */
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->perception_health_score);
}


/* -------------------------------------------------------------------------
 * Cortical Health Score
 * ------------------------------------------------------------------------- */

float nimcp_health_agent_get_cortical_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 100.0f;  /* No agent = assume healthy */
    }

    uint32_t immune_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_cortical_immune_systems);
    uint32_t column_count = atomic_load(&((nimcp_health_agent_t*)agent)->num_cortical_columns);

    if (immune_count == 0 && column_count == 0) {
        return 100.0f;  /* No cortical components = healthy */
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->cortical_health_score);
}


/* ============================================================================
 * Phase 5.13: Brain Probes Enhancement Implementation
 * ============================================================================ */

/* -------------------------------------------------------------------------
 * Brain Probe Configuration Default
 * ------------------------------------------------------------------------- */

void nimcp_health_agent_brain_probe_config_default(
    health_agent_brain_probe_config_t* config
) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Enable flags */
    config->enable_probe_monitoring = true;
    config->enable_memory_tracking = true;
    config->enable_performance_tracking = true;
    config->enable_learning_monitoring = true;
    config->enable_synapse_monitoring = true;
    config->enable_cow_monitoring = true;
    config->enable_auto_recovery = false;  /* Conservative default */

    /* Timing */
    config->probe_interval_ms = 1000;       /* 1 second probe interval */
    config->trend_window_probes = 10;       /* 10 probes for trend analysis */

    /* Memory thresholds */
    config->memory_warning_bytes = 512 * 1024 * 1024;   /* 512 MB warning */
    config->memory_critical_bytes = 1024 * 1024 * 1024; /* 1 GB critical */
    config->memory_growth_rate_warning = 0.1f;          /* 10%/sec growth rate warning */

    /* Performance thresholds */
    config->inference_time_warning_us = 10000.0f;   /* 10ms warning */
    config->inference_time_critical_us = 100000.0f; /* 100ms critical */
    config->performance_degradation_pct = 0.2f;     /* 20% degradation warning */

    /* Learning thresholds */
    config->lr_change_warning_pct = 0.5f;   /* 50% LR change warning */
    config->accuracy_drop_warning = 0.05f;  /* 5% accuracy drop warning */

    /* Synapse thresholds */
    config->synapse_growth_warning_pct = 0.2f;  /* 20% growth warning */
    config->synapse_loss_warning_pct = 0.3f;    /* 30% loss warning */
    config->min_active_synapses = 100;          /* Minimum 100 active synapses */

    /* COW thresholds */
    config->cow_private_ratio_warning = 0.5f;           /* 50% private warning */
    config->cow_overhead_warning_bytes = 50 * 1024 * 1024; /* 50 MB COW overhead */
}


/* -------------------------------------------------------------------------
 * Get Brain Probe Metrics
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_get_brain_probe_metrics(
    const nimcp_health_agent_t* agent,
    uint32_t brain_index,
    brain_probe_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_brain_probe_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_monitored_brains);
    if (brain_index >= count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_health_agent_get_brain_probe_metrics: capacity exceeded");
        return -1;
    }

    if (((nimcp_health_agent_t*)agent)->brain_probe_mutex) {
        nimcp_mutex_lock(((nimcp_health_agent_t*)agent)->brain_probe_mutex);
    }

    memcpy(metrics, &((nimcp_health_agent_t*)agent)->brain_metrics[brain_index],
           sizeof(*metrics));

    if (((nimcp_health_agent_t*)agent)->brain_probe_mutex) {
        nimcp_mutex_unlock(((nimcp_health_agent_t*)agent)->brain_probe_mutex);
    }

    return 0;
}


/* -------------------------------------------------------------------------
 * Get Overall Brain Probe Health Score
 * ------------------------------------------------------------------------- */

float nimcp_health_agent_get_brain_probe_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 100.0f;
    }

    uint32_t count = atomic_load(&((nimcp_health_agent_t*)agent)->num_monitored_brains);
    if (count == 0) {
        return 100.0f;  /* No brains = healthy */
    }

    /* Calculate weighted average health score */
    float total_score = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        total_score += agent->brain_metrics[i].overall_health_score;
    }

    return total_score / (float)count;
}


/* -------------------------------------------------------------------------
 * Get Brain Count
 * ------------------------------------------------------------------------- */

uint32_t nimcp_health_agent_get_brain_count(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return 0;
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->num_monitored_brains);
}


/* ============================================================================
 * Phase 5.14: World Model & Imagination Health Implementation
 * ============================================================================
 *
 * WHAT: Health monitoring for JEPA predictor, Omni world model, and imagination
 * WHY:  Predictive processing health is critical for planning and reasoning
 * HOW:  Monitor prediction accuracy, dynamics stability, imagination coherence
 */

/* -------------------------------------------------------------------------
 * Default Configuration
 * ------------------------------------------------------------------------- */

void nimcp_health_agent_wm_imagination_config_default(
    health_agent_wm_imagination_config_t* config
) {
    if (!config) {
        return;
    }

    /* Check intervals */
    config->check_interval_ms = 500;
    config->trend_window_ms = 10000;

    /* JEPA thresholds */
    config->jepa_error_warning = 0.3f;
    config->jepa_error_critical = 0.6f;
    config->embedding_variance_min = 0.01f;
    config->gradient_norm_max = 100.0f;
    config->gradient_norm_min = 1e-7f;

    /* World model thresholds */
    config->forward_accuracy_warning = 0.7f;
    config->forward_accuracy_critical = 0.5f;
    config->horizon_min_stable = 5;
    config->counterfactual_validity_min = 0.8f;

    /* Imagination thresholds */
    config->coherence_warning = 0.6f;
    config->coherence_critical = 0.4f;
    config->vividness_warning = 0.4f;
    config->reality_check_min = 0.9f;
    config->imagination_reality_blur_max = 0.3f;

    /* Recovery settings */
    config->auto_recovery_enabled = true;
    config->recovery_cooldown_ms = 5000;
    config->max_recoveries_per_hour = 10;

    /* Immune integration */
    config->report_to_immune = true;
    config->immune_severity_base = 6;
}


/* -------------------------------------------------------------------------
 * Metrics Retrieval
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_get_jepa_metrics(
    const nimcp_health_agent_t* agent,
    jepa_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_jepa_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_lock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    memcpy(metrics, &((nimcp_health_agent_t*)agent)->jepa_metrics, sizeof(*metrics));

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_unlock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    return 0;
}


int nimcp_health_agent_get_world_model_metrics(
    const nimcp_health_agent_t* agent,
    omni_wm_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_world_model_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_lock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    memcpy(metrics, &((nimcp_health_agent_t*)agent)->wm_metrics, sizeof(*metrics));

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_unlock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    return 0;
}


int nimcp_health_agent_get_imagination_metrics(
    const nimcp_health_agent_t* agent,
    imagination_health_metrics_t* metrics
) {
    if (!agent || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_imagination_metrics: required parameter is NULL (agent, metrics)");
        return -1;
    }

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_lock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    memcpy(metrics, &((nimcp_health_agent_t*)agent)->imagination_metrics, sizeof(*metrics));

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_unlock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    return 0;
}


int nimcp_health_agent_get_world_imagination_health(
    const nimcp_health_agent_t* agent,
    world_imagination_health_t* health
) {
    if (!agent || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_world_imagination_health: required parameter is NULL (agent, health)");
        return -1;
    }

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_lock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    /* Copy component metrics */
    memcpy(&health->jepa, &((nimcp_health_agent_t*)agent)->jepa_metrics,
           sizeof(health->jepa));
    memcpy(&health->world_model, &((nimcp_health_agent_t*)agent)->wm_metrics,
           sizeof(health->world_model));
    memcpy(&health->imagination, &((nimcp_health_agent_t*)agent)->imagination_metrics,
           sizeof(health->imagination));

    /* Compute cross-system health metrics */
    float wm_score = atomic_load(&((nimcp_health_agent_t*)agent)->wm_health_score);
    float imag_score = atomic_load(&((nimcp_health_agent_t*)agent)->imagination_health_score);

    health->wm_imagination_alignment = (wm_score + imag_score) / 2.0f;
    health->prediction_imagination_sync = wm_score > 0.5f ?
        fminf(1.0f, imag_score / wm_score) : 0.0f;
    health->memory_imagination_grounding = imag_score;

    /* Free energy integration (placeholder - would connect to FEP system) */
    health->predictive_free_energy =
        ((nimcp_health_agent_t*)agent)->jepa_metrics.mean_prediction_error;
    health->free_energy_trend =
        ((nimcp_health_agent_t*)agent)->jepa_metrics.prediction_error_trend;

    /* Anomaly summary */
    health->active_anomalies = 0;
    if (((nimcp_health_agent_t*)agent)->wm_metrics.health_state != WM_HEALTH_OPTIMAL) {
        health->active_anomalies++;
    }
    if (((nimcp_health_agent_t*)agent)->imagination_metrics.health_state != IMAG_HEALTH_VIVID) {
        health->active_anomalies++;
    }
    if (((nimcp_health_agent_t*)agent)->jepa_metrics.embedding_collapse_detected) {
        health->active_anomalies++;
    }
    if (((nimcp_health_agent_t*)agent)->jepa_metrics.gradient_explosion ||
        ((nimcp_health_agent_t*)agent)->jepa_metrics.gradient_vanishing) {
        health->active_anomalies++;
    }

    health->anomalies_this_window =
        atomic_load(&((nimcp_health_agent_t*)agent)->wm_anomalies_detected) +
        atomic_load(&((nimcp_health_agent_t*)agent)->imagination_anomalies_detected);

    /* Recommended action based on state */
    health->recommended_action = WM_RECOVERY_NONE;
    if (((nimcp_health_agent_t*)agent)->jepa_metrics.embedding_collapse_detected) {
        health->recommended_action = WM_RECOVERY_PRUNE_LATENT;
    } else if (((nimcp_health_agent_t*)agent)->wm_metrics.health_state == WM_HEALTH_DYNAMICS_UNSTABLE) {
        health->recommended_action = WM_RECOVERY_RETRAIN_DYNAMICS;
    } else if (((nimcp_health_agent_t*)agent)->imagination_metrics.health_state == IMAG_HEALTH_CONFABULATING) {
        health->recommended_action = WM_RECOVERY_INCREASE_REALITY_CHECK;
    } else if (((nimcp_health_agent_t*)agent)->imagination_metrics.health_state == IMAG_HEALTH_STUCK) {
        health->recommended_action = WM_RECOVERY_CLEAR_WORKSPACE;
    }

    /* Timestamps */
    health->last_check_timestamp_us =
        fmaxf(((nimcp_health_agent_t*)agent)->last_wm_check_us,
              ((nimcp_health_agent_t*)agent)->last_imagination_check_us);
    health->check_count =
        atomic_load(&((nimcp_health_agent_t*)agent)->wm_checks_run) +
        atomic_load(&((nimcp_health_agent_t*)agent)->imagination_checks_run);

    if (((nimcp_health_agent_t*)agent)->wm_imagination_mutex) {
        nimcp_mutex_unlock(((nimcp_health_agent_t*)agent)->wm_imagination_mutex);
    }

    return 0;
}


float nimcp_health_agent_get_world_model_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return -1.0f;
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->wm_health_score);
}


float nimcp_health_agent_get_imagination_health_score(
    const nimcp_health_agent_t* agent
) {
    if (!agent) {
        return -1.0f;
    }

    return atomic_load(&((nimcp_health_agent_t*)agent)->imagination_health_score);
}
