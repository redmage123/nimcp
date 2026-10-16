// nimcp_health_agent_part_processing.c - processing functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * Query API
 * ============================================================================ */

bool nimcp_health_agent_is_running(const nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        return false;
    }
    return atomic_load(&agent->running);
}


/* ============================================================================
 * Configuration Update API
 * ============================================================================ */

int nimcp_health_agent_update_detector(nimcp_health_agent_t* agent,
                                        const char* detector,
                                        const health_agent_detector_config_t* config) {
    if (!validate_agent(agent) || !detector || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_queue_depth: required parameter is NULL (validate_agent, detector, config)");
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

    *target = *config;
    nimcp_log(LOG_LEVEL_INFO, "Updated detector '%s' configuration", detector);
    return 0;
}


int nimcp_health_agent_connect_runtime_adaptation(
    nimcp_health_agent_t* agent,
    runtime_adaptation_context_t ra_ctx
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_runtime_adaptation: validate_agent is NULL");
        return -1;
    }

    nimcp_mutex_lock(agent->modules_mutex);
    agent->runtime_adaptation = ra_ctx;
    nimcp_mutex_unlock(agent->modules_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Agent '%s' connected to runtime adaptation",
              agent->config.agent_name);
    return 0;
}


/* ============================================================================
 * Agent Thread Helper Functions (Internal)
 * ============================================================================ */

static void agent_run_hypothalamus_check(nimcp_health_agent_t* agent, float health_score) {
    if (!agent || !agent->hypothalamus) return;

    /* Query current hypothalamus orchestrator state */
    health_agent_drive_state_t drive_state;
    memset(&drive_state, 0, sizeof(drive_state));
    hypo_orch_get_drive_state(agent->hypothalamus, &drive_state);

    /* Get direct drive level for quick assessment */
    float drive_level = 0.0f;
    hypo_orch_get_drive_level(agent->hypothalamus, &drive_level);

    /* Query orchestrator stress state */
    bool orch_stressed = false;
    hypo_orch_is_stressed(agent->hypothalamus, &orch_stressed);

    /* Get hypothalamus orchestrator statistics */
    health_agent_hypo_stats_t hypo_stats;
    memset(&hypo_stats, 0, sizeof(hypo_stats));
    hypo_orch_get_stats(agent->hypothalamus, &hypo_stats);

    /* Log drive state for monitoring */
    nimcp_log(LOG_LEVEL_DEBUG, "Hypothalamus: health=%.2f, drive=%.2f, active_drives=%u, "
              "bridges=%u, conflicts=%llu, orch_stressed=%d",
              health_score, drive_level, drive_state.active_drives,
              hypo_stats.registered_bridges,
              (unsigned long long)hypo_stats.conflicts_detected, orch_stressed);

    /* Detect drive conflicts */
    if (hypo_stats.conflicts_detected > 0 && drive_state.active_drives > 2) {
        nimcp_log(LOG_LEVEL_INFO, "Hypothalamus drive conflict: %llu conflicts, %u active drives",
                  (unsigned long long)hypo_stats.conflicts_detected, drive_state.active_drives);

        /* Report drive conflict as health issue */
        health_agent_message_t conflict_msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Drive conflict: %u active drives, %llu conflicts",
            drive_state.active_drives, (unsigned long long)hypo_stats.conflicts_detected
        );
        conflict_msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
        nimcp_health_agent_report_anomaly(agent, &conflict_msg);
    }

    /* Check for high drive pressure combined with low health */
    bool high_drive_pressure = drive_level > 0.7f || drive_state.active_drives > 5;
    bool critical_combination = high_drive_pressure && health_score < 0.5f;

    if (critical_combination) {
        nimcp_log(LOG_LEVEL_WARN, "Critical: high drive pressure (%.2f) with low health (%.2f)",
                  drive_level, health_score);

        /* Trigger orchestrator stress if not already stressed */
        if (!orch_stressed) {
            hypo_orch_trigger_stress(agent->hypothalamus, "High drive pressure with low health");
        }
    }

    /* Check if we need to trigger stress response via health agent */
    if (health_score < agent->hypothalamus_config.stress_trigger_threshold) {
        if (!atomic_load(&agent->in_stress_response)) {
            health_agent_severity_t severity = HEALTH_SEVERITY_ERROR;

            /* Adjust severity based on drive state */
            if (health_score < 0.2f || (health_score < 0.3f && high_drive_pressure)) {
                severity = HEALTH_SEVERITY_CRITICAL;
            } else if (health_score < 0.15f) {
                severity = HEALTH_SEVERITY_FATAL;
            }

            char stress_reason[NIMCP_ERROR_BUFFER_MEDIUM];
            snprintf(stress_reason, sizeof(stress_reason),
                     "Health=%.2f, drive=%.2f, active_drives=%u",
                     health_score, drive_level, drive_state.active_drives);

            nimcp_health_agent_trigger_stress_response(agent, stress_reason, severity);

            /* Synchronize with orchestrator */
            if (!orch_stressed) {
                hypo_orch_trigger_stress(agent->hypothalamus, stress_reason);
            }
        }
    } else if (atomic_load(&agent->in_stress_response) && health_score > 0.6f) {
        /* Health recovered, release stress */
        nimcp_health_agent_release_stress_response(agent);

        /* Also release orchestrator stress if active */
        if (orch_stressed && health_score > 0.7f && drive_level < 0.5f) {
            hypo_orch_release_stress(agent->hypothalamus);
            nimcp_log(LOG_LEVEL_INFO, "Hypothalamus: stress released (health=%.2f, drive=%.2f)",
                      health_score, drive_level);
        }
    }

    /* Check if we need to enter sickness mode */
    if (health_score < agent->hypothalamus_config.sickness_trigger_threshold) {
        if (!atomic_load(&agent->in_sickness_mode) &&
            agent->hypothalamus_config.enable_sickness_behavior) {

            /* Calculate sickness severity based on health and drives */
            float sickness_severity = 1.0f - health_score;
            if (high_drive_pressure) {
                sickness_severity = fminf(1.0f, sickness_severity * 1.2f);
            }

            nimcp_health_agent_enter_sickness_mode(agent, sickness_severity);

            nimcp_log(LOG_LEVEL_INFO, "Hypothalamus: entering sickness mode (severity=%.2f, "
                      "health=%.2f, drives=%u)", sickness_severity, health_score,
                      drive_state.active_drives);
        }
    } else if (atomic_load(&agent->in_sickness_mode) && health_score > 0.5f) {
        /* Health recovered, exit sickness mode */
        /* Only exit if drive pressure is also manageable */
        if (drive_level < 0.6f) {
            nimcp_health_agent_exit_sickness_mode(agent);
            nimcp_log(LOG_LEVEL_INFO, "Hypothalamus: exiting sickness mode (health=%.2f, drive=%.2f)",
                      health_score, drive_level);
        } else {
            nimcp_log(LOG_LEVEL_DEBUG, "Hypothalamus: delaying sickness exit (drive=%.2f still elevated)",
                      drive_level);
        }
    }

    /* Track peak drive level for monitoring */
    if (drive_level > hypo_stats.peak_drive_level) {
        nimcp_log(LOG_LEVEL_DEBUG, "Hypothalamus: new peak drive level %.2f (previous %.2f)",
                  drive_level, hypo_stats.peak_drive_level);
    }
}


static void agent_run_homeostatic_regulation(nimcp_health_agent_t* agent, float health_score) {
    if (!agent || !agent->homeostasis) return;

    float output = nimcp_health_agent_homeostatic_regulate(agent, health_score);

    /* Use homeostatic output to guide actions */
    if (output > 0.5f) {
        /* Need to improve health - consider triggering GC or checkpointing */
        if (agent->gc_context && agent->gc_config.enable_auto_gc_trigger) {
            nimcp_health_agent_trigger_gc(agent, false);
        }
    } else if (output < -0.3f) {
        /* Health is good - could relax monitoring */
        /* (No action needed - normal operation) */
    }
}


/* ============================================================================
 * Cognitive Integration Stub Functions (Placeholders)
 * ============================================================================ */

static void agent_run_failure_prediction(nimcp_health_agent_t* agent) {
    if (!agent || !agent->failure_predictor) return;

    /* Query failure predictor for current prediction count and prevention need */
    uint32_t prediction_count = failure_predictor_get_prediction_count(agent->failure_predictor);
    bool needs_prevention = failure_predictor_needs_prevention(agent->failure_predictor);

    /* Update cognitive stats with prediction count */
    nimcp_mutex_lock(agent->cognitive_mutex);
    /* Store prediction count for status queries */
    nimcp_mutex_unlock(agent->cognitive_mutex);

    if (prediction_count > 0) {
        /* There are active failure predictions - log and report */
        nimcp_log(LOG_LEVEL_WARN, "Failure predictor: %u active predictions, prevention_needed=%d",
                  prediction_count, needs_prevention);

        /* Report to immune system if enabled */
        if (agent->prediction_config.enable_preventive_action) {
            /* Determine severity based on prevention urgency */
            health_agent_severity_t severity = needs_prevention ?
                HEALTH_SEVERITY_ERROR : HEALTH_SEVERITY_WARNING;

            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                severity,
                HEALTH_SOURCE_NEURAL,
                "Failure predictor: %u predictions, prevention=%s",
                prediction_count, needs_prevention ? "URGENT" : "normal"
            );

            /* Suggest action based on urgency */
            msg.suggested_action = needs_prevention ?
                HEALTH_RECOVERY_CHECKPOINT : HEALTH_RECOVERY_REDUCE_LOAD;

            nimcp_health_agent_report_anomaly(agent, &msg);

            /* Track preventive actions */
            if (needs_prevention) {
                atomic_fetch_add(&agent->preventive_actions, 1);
            }
        }
    }

    atomic_fetch_add(&agent->predictions_made, 1);
}


static void agent_run_metacognition_check(nimcp_health_agent_t* agent) {
    if (!agent || !agent->metacognition) return;

    /* Check if cognitive performance is degraded using real metacognition API
     * Default threshold is 0.7 (70% of baseline) */
    float threshold = agent->metacog_config.degradation_threshold;
    if (threshold <= 0.0f || threshold > 1.0f) {
        threshold = 0.7f;  /* Use default if invalid */
    }

    /* Get current metacognition state */
    bool is_degraded = metacognition_is_degraded(agent->metacognition, threshold);
    float confidence = metacognition_get_self_confidence(agent->metacognition);
    float uncertainty = metacognition_get_uncertainty(agent->metacognition);
    bool high_uncertainty = metacognition_has_high_uncertainty(agent->metacognition, 0.7f);

    /* Update confidence tracking */
    atomic_store(&agent->current_confidence, confidence);

    if (is_degraded) {
        /* Cognitive performance is degraded - log and report */
        atomic_fetch_add(&agent->degradation_alerts, 1);
        nimcp_log(LOG_LEVEL_WARN,
                  "Metacognition: degraded (threshold=%.2f, confidence=%.2f, uncertainty=%.2f)",
                  threshold, confidence, uncertainty);

        /* Report to health system */
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Metacognition: performance below %.0f%%, confidence=%.2f",
            threshold * 100.0f, confidence
        );
        msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
        nimcp_health_agent_report_anomaly(agent, &msg);
    }

    /* Check for high uncertainty even if not degraded */
    if (high_uncertainty && agent->metacog_config.enable_confidence_calibration) {
        nimcp_log(LOG_LEVEL_INFO,
                  "Metacognition: high uncertainty (%.2f) - may need external assistance",
                  uncertainty);

        /* Optionally request help from collective or RCOG */
        if (agent->collective && agent->collective_config.enable_collective_monitoring) {
            /* Log that we're deferring to collective for uncertain decisions */
            nimcp_log(LOG_LEVEL_DEBUG, "Deferring uncertain decisions to collective cognition");
        }
    }

    atomic_fetch_add(&agent->self_diagnoses, 1);
}


static void agent_run_wellbeing_check(nimcp_health_agent_t* agent) {
    if (!agent || !agent->wellbeing) return;

    /* Check wellbeing status by querying distress detections */
    /* The wellbeing monitor provides distress level tracking */
    float distress = atomic_load((volatile _Atomic float*)&agent->current_distress_level);

    /* If distress is high, increment detection count */
    if (distress > agent->wellbeing_config.distress_intervention_threshold) {
        atomic_fetch_add(&agent->distress_detections, 1);

        /* Log high distress */
        nimcp_log(LOG_LEVEL_WARN, "Wellbeing: high distress level (%.2f)", distress);
    }
}


/* ============================================================================
 * Neural Module (SNN/LNN) Health Check Functions
 * ============================================================================ */

/**
 * @brief Run all neural health checks (SNN and LNN)
 */
static void agent_run_neural_check(nimcp_health_agent_t* agent) {
    if (!agent) return;

    /* Check SNN health if connected */
    if (agent->snn_bridge && agent->snn_config.enable_snn_monitoring) {
        agent_check_snn_health(agent);
    }

    /* Check LNN health if connected */
    if (agent->lnn_bridge && agent->lnn_config.enable_lnn_monitoring) {
        agent_check_lnn_health(agent);
    }

    /* Update aggregated neural metrics */
    agent_update_neural_metrics(agent);
}


/**
 * @brief Update aggregated neural health metrics
 */
static void agent_update_neural_metrics(nimcp_health_agent_t* agent) {
    if (!agent) return;

    nimcp_mutex_lock(agent->neural_mutex);

    /* Compute combined health score */
    float total_score = 0.0f;
    int connected_modules = 0;

    if (agent->neural_metrics.snn_connected) {
        connected_modules++;
        /* SNN health score: 100 if healthy, 0-50 based on instability count */
        if (agent->neural_metrics.snn_healthy) {
            total_score += 100.0f;
        } else {
            uint32_t inst = agent->neural_metrics.snn_instability_count;
            total_score += fmaxf(0.0f, 50.0f - (float)inst * 10.0f);
        }
    }

    if (agent->neural_metrics.lnn_connected) {
        connected_modules++;
        /* LNN health score: 100 if healthy, 0-50 based on instability count */
        if (agent->neural_metrics.lnn_healthy) {
            total_score += 100.0f;
        } else {
            uint32_t inst = agent->neural_metrics.lnn_instability_count;
            total_score += fmaxf(0.0f, 50.0f - (float)inst * 10.0f);
        }
    }

    /* Compute average score */
    if (connected_modules > 0) {
        agent->neural_metrics.neural_health_score = total_score / (float)connected_modules;
    } else {
        agent->neural_metrics.neural_health_score = 0.0f;
    }

    /* Update combined flags */
    agent->neural_metrics.any_neural_unhealthy =
        (agent->neural_metrics.snn_connected && !agent->neural_metrics.snn_healthy) ||
        (agent->neural_metrics.lnn_connected && !agent->neural_metrics.lnn_healthy);

    agent->neural_metrics.total_instabilities =
        agent->neural_metrics.snn_instability_count +
        agent->neural_metrics.lnn_instability_count;

    agent->neural_metrics.last_check_time_us = get_timestamp_us();

    nimcp_mutex_unlock(agent->neural_mutex);
}


/* ============================================================================
 * Behavioral Module (Dragonfly/Portia) Health Check Functions (Phase 5.6)
 * ============================================================================ */

/**
 * @brief Run all behavioral module health checks
 */
static void agent_run_behavioral_check(nimcp_health_agent_t* agent) {
    if (!agent) return;

    /* Check dragonfly immune if connected */
    if (agent->dragonfly_immune && agent->dragonfly_immune_config.enable_dragonfly_immune) {
        agent_check_dragonfly_immune(agent);
    }

    /* Check portia monitor if connected */
    if (agent->portia_monitor && agent->portia_monitor_config.enable_portia_monitor) {
        agent_check_portia_monitor(agent);
    }

    /* Update aggregated behavioral metrics */
    agent_update_behavioral_metrics(agent);

    /* Run cross-module coordination if enabled AND at least one bridge is connected.
     * We only run auto-coordination when we have actual sensor data from bridges.
     * When bridges are NULL (using stub data), we don't want to auto-reset
     * coordination flags that were set manually via request_behavioral_coordination.
     * This prevents stub data (thermal_state=0) from triggering resets of
     * manually-set flags during testing or lazy connection scenarios. */
    if ((agent->dragonfly_immune_config.enable_cross_coordination ||
         agent->portia_monitor_config.enable_cross_coordination) &&
        (agent->dragonfly_immune || agent->portia_monitor)) {
        agent_run_cross_module_coordination(agent);
    }
}


/**
 * @brief Update aggregated behavioral health metrics
 */
static void agent_update_behavioral_metrics(nimcp_health_agent_t* agent) {
    if (!agent) return;

    nimcp_mutex_lock(agent->behavioral_mutex);

    /* Compute combined health score */
    float total_score = 0.0f;
    int connected_modules = 0;

    if (agent->behavioral_metrics.dragonfly_connected) {
        connected_modules++;
        /* Dragonfly health score: based on health status and fatigue */
        if (agent->behavioral_metrics.dragonfly_healthy) {
            /* Score based on fatigue: 100 at 0 fatigue, 50 at 1.0 fatigue */
            total_score += 100.0f - (agent->behavioral_metrics.fatigue_level * 50.0f);
        } else {
            total_score += fmaxf(0.0f, 30.0f);
        }
    }

    if (agent->behavioral_metrics.portia_connected) {
        connected_modules++;
        /* Portia health score: based on thermal and power states */
        if (agent->behavioral_metrics.portia_healthy) {
            float score = 100.0f;
            /* Reduce score based on thermal state */
            score -= agent->behavioral_metrics.thermal_state * 15.0f;
            /* Reduce score based on power state (if on battery) */
            if (!agent->behavioral_metrics.ac_connected) {
                score -= agent->behavioral_metrics.power_state * 10.0f;
            }
            /* Reduce score based on degradation */
            score -= agent->behavioral_metrics.degradation_level * 10.0f;
            total_score += fmaxf(0.0f, score);
        } else {
            total_score += fmaxf(0.0f, 30.0f);
        }
    }

    /* Compute average score */
    if (connected_modules > 0) {
        agent->behavioral_metrics.behavioral_health_score = total_score / (float)connected_modules;
    } else {
        agent->behavioral_metrics.behavioral_health_score = 100.0f;
    }

    /* Update combined flags */
    agent->behavioral_metrics.any_behavioral_unhealthy =
        (agent->behavioral_metrics.dragonfly_connected && !agent->behavioral_metrics.dragonfly_healthy) ||
        (agent->behavioral_metrics.portia_connected && !agent->behavioral_metrics.portia_healthy);

    /* Update coordination recommendations */
    agent->behavioral_metrics.thermal_abort_recommended =
        atomic_load(&agent->thermal_abort_active);
    agent->behavioral_metrics.power_abort_recommended =
        (agent->behavioral_metrics.power_state >= 4);  /* PORTIA_POWER_BATTERY_CRITICAL */
    agent->behavioral_metrics.conservation_mode_active =
        atomic_load(&agent->power_conservation_active);

    agent->behavioral_metrics.last_check_time_us = get_timestamp_us();

    nimcp_mutex_unlock(agent->behavioral_mutex);
}


/**
 * @brief Run cross-module coordination logic
 */
static void agent_run_cross_module_coordination(nimcp_health_agent_t* agent) {
    if (!agent) return;

    /* Thermal → Dragonfly coordination */
    if (agent->dragonfly_immune_config.abort_hunt_on_thermal &&
        agent->behavioral_metrics.thermal_state >= 4) {  /* PORTIA_THERMAL_CRITICAL */
        if (!atomic_load(&agent->thermal_abort_active)) {
            atomic_store(&agent->thermal_abort_active, true);
            atomic_fetch_add(&agent->portia_coordination_actions, 1);
            nimcp_log(LOG_LEVEL_WARN, "Cross-module: thermal critical, aborting hunt");
        }
    } else if (agent->behavioral_metrics.thermal_state <= 1) {  /* NOMINAL or WARM */
        if (atomic_load(&agent->thermal_abort_active)) {
            atomic_store(&agent->thermal_abort_active, false);
            nimcp_log(LOG_LEVEL_INFO, "Cross-module: thermal recovered, hunt allowed");
        }
    }

    /* Battery → Conservation coordination */
    if (agent->dragonfly_immune_config.abort_hunt_on_battery_low &&
        agent->behavioral_metrics.power_state >= 4) {  /* PORTIA_POWER_BATTERY_CRITICAL */
        if (!atomic_load(&agent->power_conservation_active)) {
            atomic_store(&agent->power_conservation_active, true);
            atomic_fetch_add(&agent->portia_coordination_actions, 1);
            nimcp_log(LOG_LEVEL_WARN, "Cross-module: battery critical, conservation mode");
        }
    } else if (agent->behavioral_metrics.power_state <= 2 ||
               agent->behavioral_metrics.ac_connected) {
        if (atomic_load(&agent->power_conservation_active)) {
            atomic_store(&agent->power_conservation_active, false);
            nimcp_log(LOG_LEVEL_INFO, "Cross-module: power recovered, normal mode");
        }
    }

    /* Rest period management */
    if (atomic_load(&agent->rest_period_active)) {
        /* Check if rest period should end */
        uint64_t now = get_timestamp_us();
        uint64_t rest_start = atomic_load(&agent->last_coordination_us);
        uint64_t rest_duration_us = agent->dragonfly_immune_config.min_rest_duration_ms * 1000ULL;

        if (now - rest_start >= rest_duration_us &&
            agent->behavioral_metrics.fatigue_level <
            agent->dragonfly_immune_config.rest_trigger_fatigue * 0.5f) {
            atomic_store(&agent->rest_period_active, false);
            nimcp_log(LOG_LEVEL_INFO, "Cross-module: rest period ended, fatigue recovered");
        }
    }
}


static int agent_run_rcog_diagnosis(nimcp_health_agent_t* agent,
                                     const health_agent_message_t* msg,
                                     health_agent_recovery_t* suggested_action) {
    if (!agent || !agent->rcog) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (agent, agent->rcog)");
        return -1;
    }

    /* Run RCOG (Recursive Cognition) diagnosis
     * RCOG provides meta-level reasoning about health issues and recovery strategies.
     * The RCOG engine acts as a prefrontal cortex - coordinating goals and recovery.
     */
    atomic_fetch_add(&agent->rcog_diagnoses, 1);

    /* Query RCOG engine state for intelligent diagnosis */
    health_agent_rcog_state_t engine_state = rcog_engine_get_state(agent->rcog);
    bool is_ready = rcog_engine_is_ready(agent->rcog);
    bool has_capacity = rcog_engine_has_capacity(agent->rcog);

    /* Get RCOG statistics for analysis */
    health_agent_rcog_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rcog_engine_get_stats(agent->rcog, &stats);

    /* Log RCOG engine state */
    nimcp_log(LOG_LEVEL_DEBUG, "RCOG diagnosis: state=%d, ready=%d, capacity=%d, "
              "goals_active=%u, goals_failed=%llu, confidence=%.2f",
              engine_state, is_ready, has_capacity,
              stats.active_goals, (unsigned long long)stats.goals_failed,
              stats.avg_confidence);

    /* Check for RCOG degradation indicators */
    bool rcog_overloaded = !has_capacity || stats.pending_goals > 10;
    bool rcog_degraded = engine_state == RCOG_ENGINE_DEGRADED;
    bool rcog_failing = stats.goals_failed > stats.goals_completed / 4; /* >25% failure rate */
    bool low_confidence = stats.avg_confidence < 0.5f;

    /* Determine recovery action based on RCOG state + message severity */
    health_agent_recovery_t action = HEALTH_RECOVERY_NONE;

    /* If RCOG itself needs attention, report it */
    if (rcog_degraded || !is_ready) {
        nimcp_log(LOG_LEVEL_WARN, "RCOG engine in degraded/not-ready state: state=%d",
                  engine_state);

        /* Report RCOG health issue to immune system */
        health_agent_message_t rcog_msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "RCOG engine degraded: state=%d, goals_failed=%llu",
            engine_state, (unsigned long long)stats.goals_failed
        );
        rcog_msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
        nimcp_health_agent_report_anomaly(agent, &rcog_msg);
    }

    if (rcog_overloaded) {
        nimcp_log(LOG_LEVEL_WARN, "RCOG overloaded: pending=%u, capacity=%d",
                  stats.pending_goals, has_capacity);

        /* Try to enter degraded mode to reduce load */
        rcog_engine_enter_degraded_mode(agent->rcog);
    }

    /* RCOG-style meta-reasoning for recovery action selection */
    if (msg) {
        /* Multi-factor decision based on RCOG insights */
        float severity_weight = (float)msg->severity / (float)HEALTH_SEVERITY_FATAL;
        float rcog_health = is_ready ? (has_capacity ? 1.0f : 0.7f) : 0.3f;
        float decision_confidence = stats.avg_confidence * rcog_health;

        /* Use recursive reasoning: consider action outcomes */
        switch (msg->severity) {
            case HEALTH_SEVERITY_FATAL:
                /* Fatal: RCOG recommends full reset with checkpointing */
                action = HEALTH_RECOVERY_FULL_RESET;
                atomic_fetch_add(&agent->rcog_recovery_plans, 1);
                nimcp_log(LOG_LEVEL_WARN, "RCOG: fatal severity -> full reset "
                          "(confidence=%.2f)", decision_confidence);
                break;

            case HEALTH_SEVERITY_CRITICAL:
                /* Critical: rollback if confident, else checkpoint first */
                if (decision_confidence > 0.6f) {
                    action = HEALTH_RECOVERY_ROLLBACK;
                } else {
                    action = HEALTH_RECOVERY_CHECKPOINT;
                    nimcp_log(LOG_LEVEL_INFO, "RCOG: low confidence (%.2f), "
                              "checkpoint before rollback", decision_confidence);
                }
                atomic_fetch_add(&agent->rcog_recovery_plans, 1);
                break;

            case HEALTH_SEVERITY_ERROR:
                /* Error: RCOG considers quarantine vs load reduction */
                if (rcog_failing || low_confidence) {
                    /* RCOG struggling - conservative quarantine approach */
                    action = HEALTH_RECOVERY_QUARANTINE;
                    nimcp_log(LOG_LEVEL_INFO, "RCOG: failing/low-confidence -> quarantine");
                } else {
                    action = HEALTH_RECOVERY_REDUCE_LOAD;
                }
                atomic_fetch_add(&agent->rcog_recovery_plans, 1);
                break;

            case HEALTH_SEVERITY_WARNING:
                /* Warning: trigger GC if high pending, else just monitor */
                if (stats.pending_goals > 5) {
                    action = HEALTH_RECOVERY_GC;
                } else {
                    action = HEALTH_RECOVERY_REDUCE_LOAD;
                }
                break;

            case HEALTH_SEVERITY_INFO:
                /* Info: use GC for minor cleanup */
                action = HEALTH_RECOVERY_GC;
                break;

            default:
                action = HEALTH_RECOVERY_NONE;
                break;
        }

        /* Store RCOG analysis time estimate */
        float rcog_time = (float)(stats.active_goals * 10 + stats.pending_goals * 5);
        atomic_store(&agent->avg_rcog_time_ms, rcog_time);

        nimcp_log(LOG_LEVEL_DEBUG, "RCOG diagnosis: severity=%d, confidence=%.2f "
                  "-> action=%d", msg->severity, decision_confidence, action);
    }

    /* If RCOG recovered from overload, exit degraded mode */
    if (rcog_degraded && has_capacity && stats.pending_goals < 3) {
        rcog_engine_exit_degraded_mode(agent->rcog);
        nimcp_log(LOG_LEVEL_INFO, "RCOG exiting degraded mode - capacity restored");
    }

    if (suggested_action) *suggested_action = action;
    return 0;
}


/**
 * @brief Master function to run all consistency checks
 */
static void agent_run_consistency_checks(nimcp_health_agent_t* agent) {
    if (!agent) return;

    /* Check if any consistency checking is enabled */
    if (!CONSISTENCY_CHECKS_ENABLED(agent->config.consistency) &&
        !atomic_load(&agent->consistency_check_pending)) {
        return;
    }

    /* Check if enough time has passed since last check */
    uint64_t now = get_timestamp_us();
    uint64_t last_check = atomic_load(&agent->last_consistency_check_us);
    uint32_t interval_ms = agent->config.consistency.consistency_check_interval_ms;
    if (interval_ms == 0) {
        interval_ms = 5000; /* Default 5 second interval */
    }

    if (now - last_check < (uint64_t)interval_ms * 1000) {
        return; /* Not time for a check yet */
    }

    /* Run all enabled consistency checks */
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));
    result.timestamp_us = now;

    uint64_t check_start = now;
    bool overall_passed = true;

    /* Reference count checks */
    if (agent->config.consistency.check_reference_counts) {
        if (!agent_check_reference_counts(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.refcount_check_passed = true;
    }

    /* Memory canary checks */
    if (agent->config.consistency.check_pointer_canaries) {
        if (!agent_check_pointer_canaries(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.canary_check_passed = true;
    }

    /* Magic number checks */
    if (agent->config.consistency.check_struct_magic) {
        if (!agent_check_struct_magic(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.magic_check_passed = true;
    }

    /* Mutex state checks */
    if (agent->config.consistency.check_mutex_state) {
        if (!agent_check_mutex_state(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.mutex_check_passed = true;
    }

    /* Circular buffer checks */
    if (agent->config.consistency.check_circular_buffers) {
        if (!agent_check_circular_buffers(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.buffer_check_passed = true;
    }

    /* Knowledge graph checks */
    if (agent->config.consistency.check_kg_consistency) {
        if (!agent_check_knowledge_graph(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.kg_check_passed = true;
    }

    /* Neuron value checks */
    if (agent->config.consistency.check_neuron_values) {
        if (!agent_check_neuron_values(agent, &result)) {
            overall_passed = false;
        }
    } else {
        result.neuron_check_passed = true;
    }

    result.overall_passed = overall_passed;
    result.check_duration_us = get_timestamp_us() - check_start;

    /* Store result with lock */
    if (nimcp_mutex_lock(agent->consistency_mutex) == 0) {
        agent->last_consistency_result = result;
        nimcp_mutex_unlock(agent->consistency_mutex);
    }

    /* Update atomic counters */
    atomic_store(&agent->last_consistency_check_us, now);
    atomic_fetch_add(&agent->consistency_checks_run, 1);

    if (!overall_passed) {
        atomic_fetch_add(&agent->consistency_failures_total, 1);

        /* Report anomaly if any check failed */
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_HEARTBEAT,  /* Closest existing source for consistency checks */
            "Consistency check failed: ref=%s can=%s mag=%s mtx=%s buf=%s kg=%s neu=%s",
            result.refcount_check_passed ? "OK" : "FAIL",
            result.canary_check_passed ? "OK" : "FAIL",
            result.magic_check_passed ? "OK" : "FAIL",
            result.mutex_check_passed ? "OK" : "FAIL",
            result.buffer_check_passed ? "OK" : "FAIL",
            result.kg_check_passed ? "OK" : "FAIL",
            result.neuron_check_passed ? "OK" : "FAIL"
        );
        msg_queue_push(&agent->msg_queue, &msg);
    }

    /* Clear pending flag */
    atomic_store(&agent->consistency_check_pending, false);
}


int nimcp_health_agent_update_consistency_config(nimcp_health_agent_t* agent,
                                                  const health_agent_consistency_config_t* config) {
    if (!validate_agent(agent) || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent_run_consistency_checks: required parameter is NULL (validate_agent, config)");
        return -1;
    }

    /* Update consistency config with lock */
    if (nimcp_mutex_lock(agent->state_mutex) == 0) {
        agent->config.consistency = *config;
        nimcp_mutex_unlock(agent->state_mutex);
        nimcp_log(LOG_LEVEL_INFO, "Updated consistency config: interval=%ums",
                  config->consistency_check_interval_ms);
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return -1;
}


int nimcp_health_agent_update_logic_config(
    nimcp_health_agent_t* agent,
    const health_agent_symbolic_logic_config_t* config)
{
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_update_logic_config: validate_agent is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    if (agent->logic_mutex) {
        nimcp_mutex_lock(agent->logic_mutex);
    }

    memcpy(&agent->logic_config, config, sizeof(agent->logic_config));

    if (agent->logic_mutex) {
        nimcp_mutex_unlock(agent->logic_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated symbolic logic health configuration");
    return 0;
}


int nimcp_health_agent_update_substrate_config(
    nimcp_health_agent_t* agent,
    const health_agent_substrate_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_substrate_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->substrate_mutex) {
        nimcp_mutex_lock(agent->substrate_mutex);
    }

    memcpy(&agent->substrate_config, config, sizeof(agent->substrate_config));

    if (agent->substrate_mutex) {
        nimcp_mutex_unlock(agent->substrate_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated neural substrate health configuration");
    return 0;
}


/* -------------------------------------------------------------------------
 * Configuration Updates
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_update_thalamic_config(
    nimcp_health_agent_t* agent,
    const health_agent_thalamic_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_thalamic_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->thalamic_mutex) {
        nimcp_mutex_lock(agent->thalamic_mutex);
    }

    memcpy(&agent->thalamic_config, config, sizeof(agent->thalamic_config));

    if (agent->thalamic_mutex) {
        nimcp_mutex_unlock(agent->thalamic_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated thalamic health configuration");
    return 0;
}


int nimcp_health_agent_update_middleware_config(
    nimcp_health_agent_t* agent,
    const health_agent_middleware_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_middleware_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->middleware_mutex) {
        nimcp_mutex_lock(agent->middleware_mutex);
    }

    memcpy(&agent->middleware_config, config, sizeof(agent->middleware_config));

    if (agent->middleware_mutex) {
        nimcp_mutex_unlock(agent->middleware_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated middleware health configuration");
    return 0;
}


/* -------------------------------------------------------------------------
 * Perception Configuration Update
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_update_perception_config(
    nimcp_health_agent_t* agent,
    const health_agent_perception_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_perception_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->perception_mutex) {
        nimcp_mutex_lock(agent->perception_mutex);
    }

    memcpy(&agent->perception_config, config, sizeof(agent->perception_config));

    if (agent->perception_mutex) {
        nimcp_mutex_unlock(agent->perception_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated perception health configuration");
    return 0;
}


/* -------------------------------------------------------------------------
 * Cortical Configuration Update
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_update_cortical_config(
    nimcp_health_agent_t* agent,
    const health_agent_cortical_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_cortical_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->cortical_mutex) {
        nimcp_mutex_lock(agent->cortical_mutex);
    }

    memcpy(&agent->cortical_config, config, sizeof(agent->cortical_config));

    if (agent->cortical_mutex) {
        nimcp_mutex_unlock(agent->cortical_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated cortical health configuration");
    return 0;
}


/* -------------------------------------------------------------------------
 * Update Brain Probe Configuration
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_update_brain_probe_config(
    nimcp_health_agent_t* agent,
    const health_agent_brain_probe_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_brain_probe_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->brain_probe_mutex) {
        nimcp_mutex_lock(agent->brain_probe_mutex);
    }

    /* Update default config */
    memcpy(&agent->brain_probe_config, config, sizeof(agent->brain_probe_config));

    /* Update all registered brain configs */
    uint32_t count = atomic_load(&agent->num_monitored_brains);
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&agent->brain_probe_configs[i], config, sizeof(*config));
    }

    if (agent->brain_probe_mutex) {
        nimcp_mutex_unlock(agent->brain_probe_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated brain probe configuration for %u brains", count);
    return 0;
}


/* -------------------------------------------------------------------------
 * Configuration Update
 * ------------------------------------------------------------------------- */

int nimcp_health_agent_update_wm_imagination_config(
    nimcp_health_agent_t* agent,
    const health_agent_wm_imagination_config_t* config
) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_update_wm_imagination_config: required parameter is NULL (agent, config)");
        return -1;
    }

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_lock(agent->wm_imagination_mutex);
    }

    memcpy(&agent->wm_imagination_config, config, sizeof(*config));

    if (agent->wm_imagination_mutex) {
        nimcp_mutex_unlock(agent->wm_imagination_mutex);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Updated world model/imagination health config");
    return 0;
}
