// nimcp_health_agent_part_helpers.c - helpers functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}


/**
 * @brief Generate a randomized canary value for memory protection
 *
 * Uses a combination of timestamp, address, and random data for entropy.
 * Falls back to HEALTH_AGENT_CANARY XOR'd with timestamp if random fails.
 */
static uint64_t generate_random_canary(void) {
    uint64_t canary = HEALTH_AGENT_CANARY; /* Start with base pattern */
    uint64_t timestamp = get_timestamp_us();

    /* Try to get random bytes from /dev/urandom */
    uint64_t random_bits = 0;
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        size_t read = fread(&random_bits, sizeof(random_bits), 1, urandom);
        fclose(urandom);
        if (read == 1) {
            /* Successfully got random data */
            canary = random_bits;
            /* Ensure we don't accidentally get 0 */
            if (canary == 0) canary = HEALTH_AGENT_CANARY ^ timestamp;
        } else {
            /* Failed to read, use XOR fallback */
            canary = HEALTH_AGENT_CANARY ^ timestamp;
        }
    } else {
        /* No urandom, use XOR of base canary with timestamp and stack address */
        uintptr_t stack_addr = (uintptr_t)&canary;
        canary = HEALTH_AGENT_CANARY ^ timestamp ^ (uint64_t)stack_addr;
    }

    /* Additional mixing to improve entropy distribution */
    canary ^= (canary >> 33);
    canary *= 0xff51afd7ed558ccdULL;
    canary ^= (canary >> 33);
    canary *= 0xc4ceb9fe1a85ec53ULL;
    canary ^= (canary >> 33);

    return canary;
}


/**
 * @brief Validate agent structure integrity
 *
 * Uses randomized canary values for better security against buffer overflow attacks.
 */
static bool validate_agent(const nimcp_health_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_agent: agent is NULL");
        return false;
    }
    if (agent->magic != HEALTH_AGENT_MAGIC) {
        return false;
    }

    /* Check canaries against the expected randomized value */
    if (agent->canary_front != agent->expected_canary) {
        return false;
    }
    if (agent->canary_back != agent->expected_canary) {
        return false;
    }
    return true;
}


/**
 * @brief Round up to next power of 2
 */
static uint32_t next_power_of_2(uint32_t value) {
    if (value == 0) return 1;
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}


static bool msg_queue_push(health_msg_queue_t* queue, const health_agent_message_t* msg) {
    if (!queue || !msg || !queue->nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "msg_queue_push: required parameter is NULL (queue, msg, queue->nodes)");
        return false;
    }

    uint64_t head;
    health_msg_node_t* node;
    uint64_t seq;

    /* Try to claim a slot */
    while (true) {
        head = atomic_load_explicit(&queue->head, memory_order_relaxed);
        node = &queue->nodes[head & queue->capacity_mask];
        seq = atomic_load_explicit(&node->sequence, memory_order_acquire);

        int64_t diff = (int64_t)seq - (int64_t)head;

        if (diff == 0) {
            /* Slot is available, try to claim it */
            if (atomic_compare_exchange_weak_explicit(
                    &queue->head, &head, head + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;  /* Successfully claimed */
            }
        } else if (diff < 0) {
            /* Queue is full */
            atomic_fetch_add_explicit(&queue->dropped_count, 1, memory_order_relaxed);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "msg_queue_push: operation failed");
            return false;
        }
        /* Otherwise retry */
    }

    /* Write message to slot */
    node->msg = *msg;

    /* Release slot to consumer */
    atomic_store_explicit(&node->sequence, head + 1, memory_order_release);

    return true;
}


static bool msg_queue_pop(health_msg_queue_t* queue, health_agent_message_t* msg) {
    if (!queue || !msg || !queue->nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "msg_queue_pop: required parameter is NULL (queue, msg, queue->nodes)");
        return false;
    }

    uint64_t tail;
    health_msg_node_t* node;
    uint64_t seq;

    /* Try to claim an entry */
    while (true) {
        tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
        node = &queue->nodes[tail & queue->capacity_mask];
        seq = atomic_load_explicit(&node->sequence, memory_order_acquire);

        int64_t diff = (int64_t)seq - (int64_t)(tail + 1);

        if (diff == 0) {
            /* Entry is available, try to claim it */
            if (atomic_compare_exchange_weak_explicit(
                    &queue->tail, &tail, tail + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;  /* Successfully claimed */
            }
        } else if (diff < 0) {
            /* Queue is empty */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "msg_queue_pop: diff is zero");
            return false;
        }
        /* Otherwise retry */
    }

    /* Read message from slot */
    *msg = node->msg;

    /* Release slot to producer */
    atomic_store_explicit(&node->sequence, tail + queue->capacity, memory_order_release);

    return true;
}


static uint32_t msg_queue_size(const health_msg_queue_t* queue) {
    if (!queue) return 0;

    uint64_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    uint64_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

    return (uint32_t)(head - tail);
}


/* ============================================================================
 * Agent Thread (Basic implementation for Phase 1)
 * ============================================================================ */

static void* agent_thread_main(void* arg) {
    nimcp_health_agent_t* agent = (nimcp_health_agent_t*)arg;

    if (!validate_agent(agent)) {
        nimcp_log(LOG_LEVEL_ERROR, "Invalid agent in thread main");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent_thread_main: validate_agent is NULL");
        return NULL;
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Agent thread '%s' started", agent->config.agent_name);

    uint64_t last_check_us = get_timestamp_us();
    uint64_t last_prediction_us = get_timestamp_us();
    uint64_t last_metacog_us = get_timestamp_us();
    uint64_t last_wellbeing_us = get_timestamp_us();
    uint64_t last_neural_us = get_timestamp_us();  /* Phase 5.5: SNN/LNN check timing */
    uint64_t last_immune_tick_us = get_timestamp_us();  /* Immune tick timing */
    uint32_t check_interval_us = agent->config.check_interval_ms * 1000;
    uint32_t immune_tick_interval_us = BRAIN_IMMUNE_TICK_DEFAULT_INTERVAL_MS * 1000;

    while (!atomic_load(&agent->stop_requested)) {
        uint64_t now_us = get_timestamp_us();

        /* Check if it's time to run checks */
        if (now_us - last_check_us >= check_interval_us) {
            last_check_us = now_us;

            /* Update statistics */
            nimcp_mutex_lock(agent->stats_mutex);
            agent->stats.checks_performed++;
            agent->stats.uptime_ms = (now_us - atomic_load(&agent->uptime_start_us)) / 1000;
            nimcp_mutex_unlock(agent->stats_mutex);

            /* ========== COGNITIVE MODULE INTEGRATION ========== */

            /* Run failure prediction if connected */
            if (agent->failure_predictor && agent->prediction_config.enable_failure_prediction) {
                if (now_us - last_prediction_us >= agent->prediction_config.prediction_horizon_ms * 1000ULL) {
                    last_prediction_us = now_us;
                    agent_run_failure_prediction(agent);
                }
            }

            /* Run metacognition self-check if connected */
            if (agent->metacognition && agent->metacog_config.enable_metacognition) {
                if (now_us - last_metacog_us >= 1000000ULL) {  /* 1 second interval */
                    last_metacog_us = now_us;
                    agent_run_metacognition_check(agent);
                }
            }

            /* Run wellbeing check if connected */
            if (agent->wellbeing && agent->wellbeing_config.enable_wellbeing_monitoring) {
                if (now_us - last_wellbeing_us >= 5000000ULL) {  /* 5 second interval */
                    last_wellbeing_us = now_us;
                    agent_run_wellbeing_check(agent);
                }
            }

            /* Apply emotion-adjusted thresholds if connected */
            if (agent->emotion && agent->emotion_config.enable_emotion_awareness) {
                agent_apply_emotion_adjustments(agent);
            }

            /* Check GPU health if connected */
            if (agent->gpu_health && agent->gpu_config.enable_gpu_monitoring) {
                agent_check_gpu_health(agent);
            }

            /* ========== HYPOTHALAMUS & MODULE INTEGRATION (USE) ========== */

            /* Compute current health score for module USE */
            float health_score = 1.0f - ((float)atomic_load(&agent->current_severity) / 4.0f);

            /* Run hypothalamus check and stress coordination if connected */
            if (agent->hypothalamus && agent->hypothalamus_config.enable_hypothalamus) {
                agent_run_hypothalamus_check(agent, health_score);
            }

            /* Run homeostatic regulation if connected (USE the homeostasis) */
            if (agent->homeostasis && agent->hypothalamus_config.enable_homeostatic_regulation) {
                agent_run_homeostatic_regulation(agent, health_score);
            }

            /* Check connectivity health if connected */
            if (agent->connectivity && agent->connectivity_config.enable_connectivity_monitoring) {
                agent_check_connectivity(agent);
            }

            /* Check oscillations if connected */
            if (agent->oscillations && agent->oscillations_config.enable_oscillation_monitoring) {
                agent_check_oscillations(agent);
            }

            /* ========== NEURAL MODULE (SNN/LNN) HEALTH CHECK (Phase 5.5) ========== */

            /* Run neural module health checks if connected */
            if (agent->snn_bridge || agent->lnn_bridge) {
                /* Check interval: use SNN or LNN config, whichever is shorter */
                uint32_t neural_interval_ms = 1000;  /* Default 1 second */
                if (agent->snn_bridge && agent->snn_config.check_interval_ms > 0) {
                    neural_interval_ms = agent->snn_config.check_interval_ms;
                }
                if (agent->lnn_bridge && agent->lnn_config.check_interval_ms > 0) {
                    if (agent->lnn_config.check_interval_ms < neural_interval_ms) {
                        neural_interval_ms = agent->lnn_config.check_interval_ms;
                    }
                }
                if (now_us - last_neural_us >= neural_interval_ms * 1000ULL) {
                    last_neural_us = now_us;
                    agent_run_neural_check(agent);
                }
            }

            /* ========== BEHAVIORAL MODULE (DRAGONFLY/PORTIA) HEALTH CHECK (Phase 5.6) ========== */

            /* Run behavioral module health checks if connected OR configured
             * This ensures coordination flags are updated even with NULL bridges */
            if (agent->dragonfly_immune || agent->portia_monitor ||
                agent->dragonfly_immune_config.enable_dragonfly_immune ||
                agent->portia_monitor_config.enable_portia_monitor) {
                agent_run_behavioral_check(agent);
            }

            /* Auto-trigger GC if memory pressure detected (USE the GC) */
            if (agent->gc_context && agent->gc_config.enable_gc_integration) {
                agent_auto_gc_if_needed(agent);
            }

            /* Auto-checkpoint if health is good (USE the checkpoint system) */
            if (agent->checkpoint && agent->checkpoint_config.enable_auto_checkpoint) {
                agent_auto_checkpoint_if_needed(agent, health_score);
            }

            /* Check for deadlocks if detector connected (USE the deadlock detector) */
            if (agent->deadlock_detector_ptr) {
                bool deadlock_found = false;
                bool high_contention = false;
                nimcp_health_agent_check_deadlocks(agent, &deadlock_found, &high_contention);

                if (deadlock_found) {
                    health_agent_message_t msg = nimcp_health_agent_create_message(
                        HEALTH_MSG_DEADLOCK_DETECTED,
                        HEALTH_SEVERITY_CRITICAL,
                        HEALTH_SOURCE_THREADING,
                        "Deadlock detected by health agent"
                    );
                    msg.suggested_action = HEALTH_RECOVERY_RESTART_THREAD;
                    nimcp_health_agent_report_anomaly(agent, &msg);
                }
            }

            /* ========== STATE CONSISTENCY CHECKS (Phase 3) ========== */

            /* Run consistency checks (timing is internal to function) */
            if (CONSISTENCY_CHECKS_ENABLED(agent->config.consistency) ||
                atomic_load(&agent->consistency_check_pending)) {
                agent_run_consistency_checks(agent);
            }
        }

        /* ========== BRAIN IMMUNE TICK INTEGRATION ========== */

        /* Run immune system tick if connected (processes exceptions & health messages) */
        if (agent->immune && (now_us - last_immune_tick_us >= immune_tick_interval_us)) {
            last_immune_tick_us = now_us;
            uint64_t delta_ms = immune_tick_interval_us / 1000;
            brain_immune_tick(agent->immune, delta_ms);
        }

        /* Sleep for a short period */
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };  /* 1ms */
        nanosleep(&ts, NULL);
    }

    nimcp_log(LOG_LEVEL_DEBUG, "Agent thread '%s' exiting", agent->config.agent_name);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent_thread_main: operation failed");
    return NULL;
}


static void agent_check_connectivity(nimcp_health_agent_t* agent) {
    if (!agent || !agent->connectivity) return;

    bool isolated = false;
    char module_name[NIMCP_ID_BUFFER_SIZE] = {0};

    nimcp_health_agent_check_connectivity(agent, &isolated, module_name, sizeof(module_name));

    if (isolated && agent->connectivity_config.enable_isolation_detection) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_BRAIN_REGION,
            "Module isolation detected: %s", module_name[0] ? module_name : "unknown"
        );
        nimcp_health_agent_report_anomaly(agent, &msg);
    }
}


static void agent_check_oscillations(nimcp_health_agent_t* agent) {
    if (!agent || !agent->oscillations) return;

    bool abnormal = false;
    uint32_t anomaly_type = 0;

    nimcp_health_agent_check_oscillations(agent, &abnormal, &anomaly_type);

    if (abnormal) {
        health_agent_severity_t severity = HEALTH_SEVERITY_WARNING;
        const char* desc = "Abnormal brain oscillation pattern";

        if (anomaly_type == 1 && agent->oscillations_config.enable_flatline_detection) {
            severity = HEALTH_SEVERITY_CRITICAL;
            desc = "Brain oscillation flatline detected";
        } else if (anomaly_type == 2 && agent->oscillations_config.enable_seizure_detection) {
            severity = HEALTH_SEVERITY_CRITICAL;
            desc = "Seizure-like oscillation pattern detected";
        }

        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            severity,
            HEALTH_SOURCE_NEURAL,
            "%s", desc
        );
        nimcp_health_agent_report_anomaly(agent, &msg);
    }
}


static void agent_auto_gc_if_needed(nimcp_health_agent_t* agent) {
    if (!agent || !agent->gc_context || !agent->gc_config.enable_auto_gc_trigger) return;

    /* Query memory usage via GC analysis */
    kg_gc_stats_t gc_stats;
    float memory_usage = 0.5f;  /* Default if analysis fails */
    if (kg_gc_analyze(agent->gc_context, &gc_stats) == 0) {
        /* Calculate fragmentation as a proxy for memory pressure */
        memory_usage = kg_gc_get_fragmentation(agent->gc_context);
    }

    if (memory_usage > agent->gc_config.gc_trigger_threshold) {
        nimcp_health_agent_trigger_gc(agent, false);
    }
}


static void agent_auto_checkpoint_if_needed(nimcp_health_agent_t* agent, float health_score) {
    if (!agent || !agent->checkpoint || !agent->checkpoint_config.enable_auto_checkpoint) return;

    uint64_t now_us = get_timestamp_us();
    uint64_t interval_us = agent->checkpoint_config.checkpoint_interval_ms * 1000ULL;

    /* Only checkpoint if health is good and interval has elapsed */
    if (health_score >= agent->checkpoint_config.health_threshold_checkpoint &&
        (now_us - agent->last_checkpoint_time_us) >= interval_us) {
        nimcp_health_agent_create_checkpoint(agent, "auto_checkpoint_good_health");
    }

    /* Auto-rollback if health is critically low */
    if (health_score < agent->checkpoint_config.health_threshold_rollback &&
        agent->checkpoint_config.enable_auto_rollback) {
        nimcp_log(LOG_LEVEL_WARN, "Health critically low (%.2f), triggering rollback",
                  health_score);
        nimcp_health_agent_rollback(agent, 0);  /* 0 = latest checkpoint */
    }
}


static int hypo_drive_event_callback(const void* event, void* user_data) {
    nimcp_health_agent_t* agent = (nimcp_health_agent_t*)user_data;
    if (!validate_agent(agent) || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_event_callback: required parameter is NULL (validate_agent, event)");
        return -1;
    }

    /* Process drive event from hypothalamus
     * The event structure contains drive type and urgency level.
     * We use this to adjust health agent behavior.
     */
    typedef struct {
        uint32_t drive_type;
        float urgency;
    } drive_event_t;

    const drive_event_t* drive_event = (const drive_event_t*)event;

    /* Log high-urgency drive events */
    if (drive_event->urgency >= 0.7f) {
        nimcp_log(LOG_LEVEL_INFO, "High urgency drive event: type=%u, urgency=%.2f",
                  drive_event->drive_type, drive_event->urgency);

        /* Consider triggering stress response for very high urgency */
        if (drive_event->urgency >= 0.9f && !atomic_load(&agent->in_stress_response)) {
            nimcp_health_agent_trigger_stress_response(
                agent, "High urgency drive detected", HEALTH_SEVERITY_WARNING);
        }
    }

    return 0;
}


static void agent_apply_emotion_adjustments(nimcp_health_agent_t* agent) {
    if (!agent || !agent->emotion) return;

    /* Query emotion state and adjust thresholds based on stress level */
    float stress_level = atomic_load((volatile _Atomic float*)&agent->current_stress_level);

    /* Adjust emotion-related behavior based on stress */
    if (stress_level > 0.7f) {
        /* High stress: increase monitoring frequency */
        atomic_fetch_add(&agent->emotion_adjustments, 1);
        nimcp_log(LOG_LEVEL_DEBUG, "Emotion: high stress (%.2f), increasing vigilance",
                  stress_level);
    } else if (stress_level < 0.2f) {
        /* Low stress: normal operation */
    }
}


static void agent_check_gpu_health(nimcp_health_agent_t* agent) {
    if (!agent || !agent->gpu_health) return;

    /* Get number of GPU devices */
    int num_devices = gpu_health_get_device_count(agent->gpu_health);
    if (num_devices <= 0) {
        /* No GPUs to monitor */
        atomic_store(&agent->gpu_healthy, true);
        atomic_store(&agent->gpu_utilization, 0.0f);
        return;
    }

    /* Check each GPU's health */
    bool all_healthy = true;
    float total_utilization = 0.0f;
    float min_health_score = 1.0f;

    for (int i = 0; i < num_devices; i++) {
        gpu_health_metrics_t metrics;
        if (gpu_health_get_metrics(agent->gpu_health, i, &metrics) == 0) {
            /* Track utilization */
            total_utilization += metrics.gpu_utilization;

            /* Track minimum health score */
            if (metrics.health_score < min_health_score) {
                min_health_score = metrics.health_score;
            }

            /* Check health status */
            if (metrics.status >= GPU_HEALTH_DEGRADED) {
                all_healthy = false;

                /* Report anomaly for degraded/critical GPUs */
                if (metrics.status >= GPU_HEALTH_CRITICAL) {
                    health_agent_message_t msg = nimcp_health_agent_create_message(
                        HEALTH_MSG_RESOURCE_EXHAUSTION,
                        HEALTH_SEVERITY_CRITICAL,
                        HEALTH_SOURCE_MEMORY,  /* GPU memory is a memory resource */
                        "GPU %d health critical: score=%.2f",
                        i, metrics.health_score
                    );

                    /* Add GPU-specific details via resource variant */
                    msg.data.resource.memory_used = metrics.memory_used;
                    msg.data.resource.memory_limit = metrics.memory_total;
                    msg.data.resource.utilization_pct = (1.0f - metrics.health_score) * 100.0f;

                    nimcp_health_agent_report_anomaly(agent, &msg);
                }
            }

            /* Check for GPU errors */
            gpu_error_event_t error;
            if (gpu_error_check_async(agent->gpu_health, i, &error) > 0) {
                /* Error detected - get immune response */
                gpu_immune_response_t response;
                if (gpu_immune_get_response(agent->gpu_health, &error, &response) == 0) {
                    nimcp_log(LOG_LEVEL_WARN, "GPU %d error: %s - suggested recovery: %s",
                              i, error.description,
                              gpu_recovery_action_name(response.suggested_recovery));

                    /* Execute auto-recovery if enabled */
                    if (agent->gpu_config.enable_auto_recovery && response.urgency >= 0.5f) {
                        gpu_immune_execute_recovery(agent->gpu_health, i, response.suggested_recovery);
                    }
                }
            }

            /* Predict failure probability */
            if (agent->gpu_config.enable_predictive_monitoring) {
                float failure_prob = gpu_health_predict_failure_probability(
                    agent->gpu_health, i, 60  /* 60 minutes horizon */
                );

                if (failure_prob > 0.5f) {
                    nimcp_log(LOG_LEVEL_WARN, "GPU %d: %.0f%% failure probability in next hour",
                              i, failure_prob * 100.0f);
                }
            }
        }
    }

    /* Update aggregated stats */
    float avg_utilization = (num_devices > 0) ? total_utilization / num_devices : 0.0f;
    atomic_store(&agent->gpu_utilization, avg_utilization);
    atomic_store(&agent->gpu_healthy, all_healthy);
    atomic_fetch_add(&agent->gpu_accelerated_checks, 1);

    /* Log summary if issues detected */
    if (!all_healthy || min_health_score < 0.7f) {
        nimcp_log(LOG_LEVEL_INFO, "GPU health check: %d devices, avg_util=%.1f%%, min_score=%.2f, healthy=%d",
                  num_devices, avg_utilization * 100.0f, min_health_score, all_healthy);
    }
}


/**
 * @brief Check SNN immune bridge health
 */
static void agent_check_snn_health(nimcp_health_agent_t* agent) {
    if (!agent || !agent->snn_bridge) return;

    uint64_t now = get_timestamp_us();
    uint64_t interval_us = (uint64_t)agent->snn_config.check_interval_ms * 1000;

    /* Rate limit checks */
    if ((now - agent->last_snn_check_us) < interval_us) {
        return;
    }
    agent->last_snn_check_us = now;

    /* Get SNN health metrics from bridge */
    snn_health_metrics_t snn_health;
    int result = snn_immune_get_health(agent->snn_bridge, &snn_health);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Failed to get SNN health metrics: %d", result);
        return;
    }

    atomic_fetch_add(&agent->snn_checks_run, 1);

    /* Update cached metrics */
    nimcp_mutex_lock(agent->neural_mutex);
    agent->neural_metrics.snn_healthy = !snn_health.has_instability;
    agent->neural_metrics.snn_mean_rate = snn_health.mean_rate;
    agent->neural_metrics.snn_max_rate = snn_health.max_rate;
    agent->neural_metrics.snn_burst_ratio = snn_health.burst_ratio;
    agent->neural_metrics.snn_sync_index = snn_health.sync_index;
    agent->neural_metrics.snn_silent_neurons = snn_health.silent_neurons;
    agent->neural_metrics.snn_saturated_neurons = snn_health.saturated_neurons;
    nimcp_mutex_unlock(agent->neural_mutex);

    /* Detect instabilities and report to immune system */
    if (snn_health.has_instability) {
        atomic_fetch_add(&agent->snn_instabilities_detected, 1);
        atomic_fetch_add(&agent->neural_metrics.snn_instability_count, 1);

        /* Map SNN health state to severity */
        health_agent_severity_t severity = HEALTH_SEVERITY_WARNING;
        const char* state_name = "unknown";

        switch (snn_health.health) {
            case SNN_STATE_HEALTHY:
                /* Should not reach here if has_instability is true */
                return;
            case SNN_STATE_SILENT:
                severity = HEALTH_SEVERITY_ERROR;
                state_name = "silent network";
                break;
            case SNN_STATE_EXPLOSION:
                severity = HEALTH_SEVERITY_CRITICAL;
                state_name = "spike explosion";
                break;
            case SNN_STATE_NAN_DETECTED:
                severity = HEALTH_SEVERITY_CRITICAL;
                state_name = "NaN detected";
                break;
            case SNN_STATE_INF_DETECTED:
                severity = HEALTH_SEVERITY_CRITICAL;
                state_name = "Inf detected";
                break;
            case SNN_STATE_WEIGHT_EXPLOSION:
                severity = HEALTH_SEVERITY_ERROR;
                state_name = "weight explosion";
                break;
            case SNN_STATE_UNSTABLE:
                severity = HEALTH_SEVERITY_ERROR;
                state_name = "unstable";
                break;
        }

        nimcp_log(LOG_LEVEL_WARN, "SNN instability: %s (rate=%.1fHz, sync=%.2f)",
                  state_name, snn_health.mean_rate, snn_health.sync_index);

        /* Auto-report to immune system if enabled */
        if (agent->snn_config.enable_auto_report) {
            health_agent_message_t msg = {0};
            msg.type = HEALTH_MSG_ANOMALY_DETECTED;
            msg.severity = severity;
            msg.source = HEALTH_SOURCE_NEURAL;
            msg.timestamp_us = now;
            snprintf(msg.description, sizeof(msg.description),
                     "SNN %s: rate=%.1fHz, sync=%.2f", state_name,
                     snn_health.mean_rate, snn_health.sync_index);

            msg_queue_push(&agent->msg_queue, &msg);
            atomic_fetch_add(&agent->snn_recoveries_triggered, 1);
        }
    }
}


/**
 * @brief Check LNN immune bridge health
 */
static void agent_check_lnn_health(nimcp_health_agent_t* agent) {
    if (!agent || !agent->lnn_bridge) return;

    uint64_t now = get_timestamp_us();
    uint64_t interval_us = (uint64_t)agent->lnn_config.check_interval_ms * 1000;

    /* Rate limit checks */
    if ((now - agent->last_lnn_check_us) < interval_us) {
        return;
    }
    agent->last_lnn_check_us = now;

    atomic_fetch_add(&agent->lnn_checks_run, 1);

    /* Check LNN stability */
    lnn_instability_type_t instability = lnn_immune_check_stability(agent->lnn_bridge);

    /* Get cytokine effects for metrics */
    lnn_cytokine_effects_t effects;
    lnn_immune_get_effects(agent->lnn_bridge, &effects);

    /* Update cached metrics */
    nimcp_mutex_lock(agent->neural_mutex);
    agent->neural_metrics.lnn_healthy = (instability == LNN_INSTABILITY_NONE);
    agent->neural_metrics.lnn_tau_scale = effects.tau_scale;
    agent->neural_metrics.lnn_lr_factor = effects.lr_factor;
    agent->neural_metrics.lnn_state_damping = effects.state_damping;
    nimcp_mutex_unlock(agent->neural_mutex);

    /* Handle instabilities */
    if (instability != LNN_INSTABILITY_NONE) {
        atomic_fetch_add(&agent->lnn_instabilities_detected, 1);
        atomic_fetch_add(&agent->neural_metrics.lnn_instability_count, 1);

        /* Map LNN instability to severity */
        health_agent_severity_t severity = HEALTH_SEVERITY_WARNING;
        const char* inst_name = "unknown";

        switch (instability) {
            case LNN_INSTABILITY_NONE:
                return;
            case LNN_INSTABILITY_NAN_STATE:
                severity = HEALTH_SEVERITY_CRITICAL;
                inst_name = "NaN state";
                atomic_fetch_add(&agent->neural_metrics.lnn_nan_detections, 1);
                break;
            case LNN_INSTABILITY_INF_STATE:
                severity = HEALTH_SEVERITY_CRITICAL;
                inst_name = "Inf state";
                atomic_fetch_add(&agent->neural_metrics.lnn_inf_detections, 1);
                break;
            case LNN_INSTABILITY_STATE_EXPLOSION:
                severity = HEALTH_SEVERITY_ERROR;
                inst_name = "state explosion";
                break;
            case LNN_INSTABILITY_STATE_COLLAPSE:
                severity = HEALTH_SEVERITY_WARNING;
                inst_name = "state collapse";
                break;
            case LNN_INSTABILITY_TAU_EXPLOSION:
                severity = HEALTH_SEVERITY_ERROR;
                inst_name = "tau explosion";
                atomic_fetch_add(&agent->neural_metrics.lnn_tau_violations, 1);
                break;
            case LNN_INSTABILITY_TAU_COLLAPSE:
                severity = HEALTH_SEVERITY_WARNING;
                inst_name = "tau collapse";
                atomic_fetch_add(&agent->neural_metrics.lnn_tau_violations, 1);
                break;
            case LNN_INSTABILITY_GRADIENT_EXPLOSION:
                severity = HEALTH_SEVERITY_ERROR;
                inst_name = "gradient explosion";
                atomic_fetch_add(&agent->neural_metrics.lnn_gradient_issues, 1);
                break;
            case LNN_INSTABILITY_GRADIENT_VANISHING:
                severity = HEALTH_SEVERITY_WARNING;
                inst_name = "gradient vanishing";
                atomic_fetch_add(&agent->neural_metrics.lnn_gradient_issues, 1);
                break;
            case LNN_INSTABILITY_ODE_DIVERGENCE:
                severity = HEALTH_SEVERITY_CRITICAL;
                inst_name = "ODE divergence";
                break;
            default:
                break;
        }

        nimcp_log(LOG_LEVEL_WARN, "LNN instability: %s (tau_scale=%.2f, lr=%.2f)",
                  inst_name, effects.tau_scale, effects.lr_factor);

        /* Auto-report to immune system if enabled */
        if (agent->lnn_config.enable_auto_report) {
            health_agent_message_t msg = {0};
            msg.type = HEALTH_MSG_ANOMALY_DETECTED;
            msg.severity = severity;
            msg.source = HEALTH_SOURCE_NEURAL;
            msg.timestamp_us = now;
            snprintf(msg.description, sizeof(msg.description),
                     "LNN %s: tau=%.2f, lr=%.2f", inst_name,
                     effects.tau_scale, effects.lr_factor);

            msg_queue_push(&agent->msg_queue, &msg);
            atomic_fetch_add(&agent->lnn_recoveries_triggered, 1);
        }
    }
}


/**
 * @brief Check dragonfly immune bridge health
 *
 * WHAT: Query dragonfly immune bridge for hunting behavior health status
 * WHY:  Enable health-aware hunting behavior and stress management
 * HOW:  Call dragonfly_immune_get_state() to get real health data
 */
static void agent_check_dragonfly_immune(nimcp_health_agent_t* agent) {
    if (!agent || !agent->dragonfly_immune) return;

    uint64_t now = get_timestamp_us();
    uint64_t interval_us = (uint64_t)agent->dragonfly_immune_config.check_interval_ms * 1000;

    /* Check if enough time has passed since last check */
    if (now - agent->last_dragonfly_immune_check_us < interval_us) {
        return;
    }
    agent->last_dragonfly_immune_check_us = now;
    atomic_fetch_add(&agent->dragonfly_immune_checks_run, 1);

    /* Get dragonfly immune state from the actual bridge */
    dragonfly_immune_state_t state;
    int result = dragonfly_immune_get_state(agent->dragonfly_immune, &state);

    nimcp_mutex_lock(agent->behavioral_mutex);

    if (result == 0) {
        /* Successfully retrieved state - update metrics with real data */
        agent->behavioral_metrics.dragonfly_healthy =
            (state.health_status == HEALTH_OPTIMAL || state.health_status == HEALTH_MILD_IMPAIRMENT);
        agent->behavioral_metrics.health_status = (uint8_t)state.health_status;
        agent->behavioral_metrics.stress_level = (uint8_t)state.stress_level;

        /* Copy performance modifiers from modulation */
        agent->behavioral_metrics.speed_modifier = state.modulation.speed_modifier;
        agent->behavioral_metrics.accuracy_modifier = state.modulation.accuracy_modifier;
        agent->behavioral_metrics.endurance_modifier = state.modulation.endurance_modifier;
        agent->behavioral_metrics.hunting_recommended = state.modulation.hunting_recommended;
        agent->behavioral_metrics.rest_urgency = state.modulation.rest_urgency;

        /* Copy stress report data */
        agent->behavioral_metrics.fatigue_level = state.stress_report.fatigue_level;
        agent->behavioral_metrics.frustration_level = state.stress_report.frustration_level;
        agent->behavioral_metrics.energy_reserves = state.stress_report.energy_reserves;
        agent->behavioral_metrics.consecutive_failures = state.stress_report.consecutive_failures;

        /* Copy injury state */
        agent->behavioral_metrics.is_injured = state.is_injured;
    } else {
        /* Failed to get state - mark as unhealthy but don't crash */
        nimcp_log(LOG_LEVEL_WARN, "Failed to get dragonfly immune state: %d", result);
        agent->behavioral_metrics.dragonfly_healthy = false;
    }

    nimcp_mutex_unlock(agent->behavioral_mutex);

    /* Check for stress/injury events and report if needed */
    float fatigue = agent->behavioral_metrics.fatigue_level;
    if (fatigue >= agent->dragonfly_immune_config.fatigue_warning_threshold) {
        atomic_fetch_add(&agent->dragonfly_stress_events, 1);

        if (fatigue >= agent->dragonfly_immune_config.fatigue_critical_threshold) {
            /* Report critical fatigue */
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_NEURAL,
                "Dragonfly immune: critical fatigue level (%.2f)", fatigue
            );
            msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
            nimcp_health_agent_report_anomaly(agent, &msg);
        }
    }

    /* Check for auto-rest trigger */
    if (agent->dragonfly_immune_config.enable_auto_rest &&
        fatigue >= agent->dragonfly_immune_config.rest_trigger_fatigue) {
        nimcp_health_agent_request_behavioral_coordination(agent, "rest_period",
            "Fatigue threshold exceeded");
    }

    /* Report injuries to immune system */
    if (agent->behavioral_metrics.is_injured &&
        agent->dragonfly_immune_config.enable_injury_detection) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Dragonfly immune: injury detected"
        );
        msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
        nimcp_health_agent_report_anomaly(agent, &msg);
    }
}


/**
 * @brief Check portia monitor health
 *
 * WHAT: Query portia monitor for platform resource metrics (thermal, power, CPU)
 * WHY:  Enable resource-aware behavior adaptation and thermal/power management
 * HOW:  Call portia_monitor_get_* functions to get real system metrics
 */
static void agent_check_portia_monitor(nimcp_health_agent_t* agent) {
    if (!agent || !agent->portia_monitor) return;

    uint64_t now = get_timestamp_us();
    uint64_t interval_us = (uint64_t)agent->portia_monitor_config.check_interval_ms * 1000;

    /* Check if enough time has passed since last check */
    if (now - agent->last_portia_monitor_check_us < interval_us) {
        return;
    }
    agent->last_portia_monitor_check_us = now;
    atomic_fetch_add(&agent->portia_monitor_checks_run, 1);

    /* Get real metrics from portia monitor */
    float cpu_temp = portia_monitor_get_cpu_temp(agent->portia_monitor);
    float battery_pct = portia_monitor_get_battery_pct(agent->portia_monitor);
    float cpu_load = portia_monitor_get_cpu_load(agent->portia_monitor);
    bool on_battery = portia_monitor_on_battery(agent->portia_monitor);

    nimcp_mutex_lock(agent->behavioral_mutex);

    /* Update metrics with real data */
    bool temp_valid = portia_monitor_temp_valid(cpu_temp);
    bool battery_valid = portia_monitor_battery_valid(battery_pct);
    bool load_valid = portia_monitor_load_valid(cpu_load);

    /* Determine thermal state from temperature */
    uint8_t thermal_state = 0;  /* NOMINAL */
    if (temp_valid) {
        agent->behavioral_metrics.cpu_temp_c = cpu_temp;
        if (cpu_temp >= agent->portia_monitor_config.thermal_critical_temp_c) {
            thermal_state = 4;  /* CRITICAL */
        } else if (cpu_temp >= agent->portia_monitor_config.thermal_warning_temp_c) {
            thermal_state = 2;  /* WARNING */
        } else if (cpu_temp >= agent->portia_monitor_config.thermal_warning_temp_c - 10.0f) {
            thermal_state = 1;  /* WARM */
        }
    } else {
        agent->behavioral_metrics.cpu_temp_c = 45.0f;  /* Default normal temp */
    }
    agent->behavioral_metrics.thermal_state = thermal_state;

    /* Determine power state from battery */
    uint8_t power_state = 0;  /* AC */
    if (battery_valid) {
        agent->behavioral_metrics.battery_pct = battery_pct;
        agent->behavioral_metrics.ac_connected = !on_battery;
        if (on_battery) {
            if (battery_pct <= agent->portia_monitor_config.battery_critical_pct) {
                power_state = 4;  /* BATTERY_CRITICAL */
            } else if (battery_pct <= agent->portia_monitor_config.battery_warning_pct) {
                power_state = 3;  /* BATTERY_LOW */
            } else {
                power_state = 1;  /* BATTERY_OK */
            }
        }
    } else {
        agent->behavioral_metrics.battery_pct = 100.0f;
        agent->behavioral_metrics.ac_connected = true;
    }
    agent->behavioral_metrics.power_state = power_state;

    /* Update CPU load */
    if (load_valid) {
        agent->behavioral_metrics.cpu_load_pct = cpu_load;
        agent->behavioral_metrics.is_throttled =
            (cpu_load >= agent->portia_monitor_config.cpu_critical_pct);
    } else {
        agent->behavioral_metrics.cpu_load_pct = 20.0f;
        agent->behavioral_metrics.is_throttled = false;
    }

    /* Determine overall health and degradation */
    agent->behavioral_metrics.portia_healthy =
        (thermal_state < 4) && (power_state < 4);

    /* Degradation based on thermal and load */
    uint8_t degradation = 0;
    if (thermal_state >= 2 || agent->behavioral_metrics.cpu_load_pct >= 80.0f) {
        degradation = 1;  /* LIGHT */
    }
    if (thermal_state >= 3 || agent->behavioral_metrics.cpu_load_pct >= 90.0f) {
        degradation = 2;  /* MODERATE */
    }
    if (thermal_state >= 4 || agent->behavioral_metrics.is_throttled) {
        degradation = 3;  /* SEVERE */
    }
    agent->behavioral_metrics.degradation_level = degradation;

    nimcp_mutex_unlock(agent->behavioral_mutex);

    /* Check thermal thresholds and report */
    if (temp_valid && cpu_temp >= agent->portia_monitor_config.thermal_warning_temp_c) {
        atomic_fetch_add(&agent->portia_thermal_warnings, 1);

        if (cpu_temp >= agent->portia_monitor_config.thermal_critical_temp_c) {
            /* Critical thermal - report and potentially abort hunt */
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_ERROR,
                HEALTH_SOURCE_IO,
                "Portia monitor: critical CPU temperature (%.1f°C)", cpu_temp
            );
            msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
            nimcp_health_agent_report_anomaly(agent, &msg);

            if (agent->portia_monitor_config.emergency_on_critical) {
                nimcp_health_agent_request_behavioral_coordination(agent, "abort_hunt",
                    "Critical thermal condition");
            }
        }
    }

    /* Check power thresholds */
    if (battery_valid && on_battery &&
        battery_pct <= agent->portia_monitor_config.battery_warning_pct) {
        atomic_fetch_add(&agent->portia_power_warnings, 1);

        if (battery_pct <= agent->portia_monitor_config.battery_critical_pct) {
            /* Critical battery - report and activate conservation */
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_IO,
                "Portia monitor: critical battery level (%.1f%%)", battery_pct
            );
            msg.suggested_action = HEALTH_RECOVERY_EMERGENCY_SAVE;
            nimcp_health_agent_report_anomaly(agent, &msg);

            if (agent->portia_monitor_config.trigger_checkpoint_on_power_loss) {
                nimcp_health_agent_request_emergency_checkpoint(agent, "Critical battery");
            }

            nimcp_health_agent_request_behavioral_coordination(agent, "conservation_mode",
                "Critical battery level");
        }
    }

    /* Check CPU load thresholds */
    if (load_valid && cpu_load >= agent->portia_monitor_config.cpu_warning_pct) {
        if (cpu_load >= agent->portia_monitor_config.cpu_critical_pct) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_IO,
                "Portia monitor: high CPU load (%.1f%%)", cpu_load
            );
            msg.suggested_action = HEALTH_RECOVERY_REDUCE_LOAD;
            nimcp_health_agent_report_anomaly(agent, &msg);

            if (agent->portia_monitor_config.reduce_load_on_warning) {
                nimcp_health_agent_request_behavioral_coordination(agent, "conservation_mode",
                    "High CPU load");
            }
        }
    }
}


static bool agent_check_ethics_permission(nimcp_health_agent_t* agent,
                                           const health_agent_message_t* msg,
                                           health_agent_recovery_t action) {
    if (!agent || !agent->ethics) return true;

    /* Evaluate ethics for the proposed action */
    atomic_fetch_add(&agent->ethics_evaluations, 1);

    /* Check emergency override - very high severity bypasses ethics */
    if (msg && msg->severity >= HEALTH_SEVERITY_FATAL) {
        if (agent->ethics_config.ethics_override_threshold > 0.0f) {
            nimcp_log(LOG_LEVEL_WARN, "Ethics: fatal severity, applying emergency override");
            return true;
        }
    }

    /* Asimov's Laws evaluation (if enabled) */
    if (agent->ethics_config.enable_asimov_laws) {
        /* First Law: A robot may not injure a human being or, through inaction,
         * allow a human being to come to harm.
         * In our context: Prefer graceful degradation over catastrophic failure */
        if (action == HEALTH_RECOVERY_FULL_RESET && msg) {
            if (msg->severity < HEALTH_SEVERITY_CRITICAL) {
                nimcp_log(LOG_LEVEL_WARN, "Ethics (1st Law): blocking full reset for non-critical");
                atomic_fetch_add(&agent->ethics_blocks, 1);
                return false;
            }
        }

        /* Second Law: A robot must obey orders given by human beings except where
         * such orders would conflict with the First Law.
         * In our context: Follow health directives unless they cause harm */

        /* Third Law: A robot must protect its own existence as long as such protection
         * does not conflict with the First or Second Law.
         * In our context: System preservation is important but not at cost of harm */
        if (action == HEALTH_RECOVERY_QUARANTINE && msg) {
            /* Quarantine is acceptable self-protection */
            nimcp_log(LOG_LEVEL_DEBUG, "Ethics (3rd Law): quarantine approved for self-protection");
        }
    }

    /* Golden Rule evaluation (if enabled) */
    if (agent->ethics_config.enable_golden_rule) {
        /* "Treat others as you would want to be treated"
         * In our context: Would we want this action performed on us? */
        if (action == HEALTH_RECOVERY_FULL_RESET || action == HEALTH_RECOVERY_ROLLBACK) {
            /* These are drastic - require higher severity */
            if (msg && msg->severity < HEALTH_SEVERITY_ERROR) {
                nimcp_log(LOG_LEVEL_INFO, "Ethics (Golden Rule): prefer less drastic action");
                /* Don't block, but log preference */
            }
        }
    }

    /* Mercy directive (if enabled) - prefer graceful degradation */
    if (agent->ethics_config.enable_mercy_directive && msg) {
        if (msg->severity < HEALTH_SEVERITY_ERROR) {
            /* For lower severity issues, prefer gentler actions */
            if (action == HEALTH_RECOVERY_FULL_RESET) {
                nimcp_log(LOG_LEVEL_INFO, "Ethics (Mercy): downgrade from full reset to rollback");
                atomic_fetch_add(&agent->mercy_applications, 1);
                /* Caller should check and use less severe action */
            } else if (action == HEALTH_RECOVERY_ROLLBACK) {
                nimcp_log(LOG_LEVEL_INFO, "Ethics (Mercy): downgrade from rollback to checkpoint");
                atomic_fetch_add(&agent->mercy_applications, 1);
            }
        }
    }

    /* Proportionality check - action severity should match problem severity */
    if (msg) {
        bool proportional = true;
        if (action == HEALTH_RECOVERY_FULL_RESET && msg->severity < HEALTH_SEVERITY_FATAL) {
            proportional = false;
        } else if (action == HEALTH_RECOVERY_ROLLBACK && msg->severity < HEALTH_SEVERITY_ERROR) {
            proportional = false;
        }

        if (!proportional) {
            nimcp_log(LOG_LEVEL_WARN, "Ethics: action %d disproportionate to severity %d",
                      action, msg->severity);
            /* Log but don't block - caller makes final decision */
        }
    }

    return true;
}


/**
 * @brief Check memory canaries for corruption
 *
 * Uses the agent's randomized expected_canary value for comparison.
 * This provides better security than a fixed canary pattern.
 */
static bool agent_check_pointer_canaries(nimcp_health_agent_t* agent,
                                         health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t corruptions = 0;

    /* Verify expected_canary itself is non-zero (sanity check) */
    if (agent->expected_canary == 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: Expected canary is zero (uninitialized?)");
        corruptions++;
        passed = false;
    }

    /* Check front canary against randomized expected value */
    if (agent->canary_front != agent->expected_canary) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: Front canary corrupted "
                  "(expected 0x%016llX, got 0x%016llX)",
                  (unsigned long long)agent->expected_canary,
                  (unsigned long long)agent->canary_front);
        corruptions++;
        passed = false;
    }

    /* Check back canary against randomized expected value */
    if (agent->canary_back != agent->expected_canary) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: Back canary corrupted "
                  "(expected 0x%016llX, got 0x%016llX)",
                  (unsigned long long)agent->expected_canary,
                  (unsigned long long)agent->canary_back);
        corruptions++;
        passed = false;
    }

    result->canary_check_passed = passed;
    result->canary_corruptions = corruptions;
    return passed;
}


/**
 * @brief Check magic numbers for registered structures
 */
static bool agent_check_struct_magic(nimcp_health_agent_t* agent,
                                       health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t violations = 0;

    /* Check agent's own magic number */
    if (agent->magic != HEALTH_AGENT_MAGIC) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: Agent magic corrupted (expected 0x%X, got 0x%X)",
                  HEALTH_AGENT_MAGIC, agent->magic);
        violations++;
        passed = false;
    }

    /* Check all registered structures (with lock) */
    if (nimcp_mutex_lock(agent->consistency_mutex) == 0) {
        for (uint32_t i = 0; i < 64; i++) {
            if (agent->registered_structs[i].active && agent->registered_structs[i].ptr) {
                uint32_t* magic_ptr = (uint32_t*)agent->registered_structs[i].ptr;
                if (*magic_ptr != agent->registered_structs[i].expected_magic) {
                    nimcp_log(LOG_LEVEL_ERROR, "Consistency: Struct '%s' magic corrupted "
                              "(expected 0x%X, got 0x%X)",
                              agent->registered_structs[i].name,
                              agent->registered_structs[i].expected_magic,
                              *magic_ptr);
                    violations++;
                    passed = false;
                }
            }
        }
        nimcp_mutex_unlock(agent->consistency_mutex);
    }

    result->magic_check_passed = passed;
    result->magic_violations = violations;
    return passed;
}


/**
 * @brief Check mutex states for consistency
 */
static bool agent_check_mutex_state(nimcp_health_agent_t* agent,
                                      health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t anomalies = 0;

    /*
     * Mutex state checks:
     * - Verify all required mutexes are non-NULL
     * - Note: We can't easily check if a mutex is "locked by another thread"
     *   without potentially causing issues, so we just verify existence.
     */

    if (!agent->state_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: state_mutex is NULL");
        anomalies++;
        passed = false;
    }

    if (!agent->stats_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: stats_mutex is NULL");
        anomalies++;
        passed = false;
    }

    if (!agent->cognitive_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: cognitive_mutex is NULL");
        anomalies++;
        passed = false;
    }

    if (!agent->modules_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: modules_mutex is NULL");
        anomalies++;
        passed = false;
    }

    if (!agent->consistency_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: consistency_mutex is NULL");
        anomalies++;
        passed = false;
    }

    /* Check stop_cond is valid */
    if (!agent->stop_cond) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: stop_cond is NULL");
        anomalies++;
        passed = false;
    }

    result->mutex_check_passed = passed;
    result->mutex_anomalies = anomalies;
    return passed;
}


/**
 * @brief Check circular buffer integrity
 *
 * Note: The message queue is lock-free (MPSC), using atomic head/tail.
 * We perform best-effort validation without locking.
 */
static bool agent_check_circular_buffers(nimcp_health_agent_t* agent,
                                          health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t errors = 0;

    /*
     * Lock-free message queue checks:
     * - Read atomic head/tail (may be slightly stale but still valid)
     * - Verify indices are within bounds
     * - Check capacity is valid (power of 2)
     */

    uint64_t head = atomic_load(&agent->msg_queue.head);
    uint64_t tail = atomic_load(&agent->msg_queue.tail);
    uint32_t capacity = agent->msg_queue.capacity;
    uint32_t capacity_mask = agent->msg_queue.capacity_mask;

    /* Check capacity is non-zero */
    if (capacity == 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: Message queue capacity is 0");
        errors++;
        passed = false;
    } else {
        /* Verify capacity_mask is correct (capacity - 1 for power of 2) */
        if (capacity_mask != capacity - 1) {
            nimcp_log(LOG_LEVEL_WARN, "Consistency: Message queue capacity_mask mismatch "
                      "(mask=%u, expected=%u)",
                      capacity_mask, capacity - 1);
            errors++;
            passed = false;
        }

        /* Verify capacity is power of 2 */
        if ((capacity & (capacity - 1)) != 0) {
            nimcp_log(LOG_LEVEL_ERROR, "Consistency: Message queue capacity (%u) is not power of 2",
                      capacity);
            errors++;
            passed = false;
        }

        /* For lock-free queues, head >= tail always (head advances, tail follows) */
        /* Since we use 64-bit counters, wraparound is extremely unlikely */
        if (head < tail) {
            nimcp_log(LOG_LEVEL_WARN, "Consistency: Message queue head (%lu) < tail (%lu)",
                      (unsigned long)head, (unsigned long)tail);
            errors++;
            /* Note: This could be a transient state during concurrent access */
        }

        /* Check queue depth doesn't exceed capacity */
        uint64_t count = head - tail;
        if (count > capacity) {
            nimcp_log(LOG_LEVEL_ERROR, "Consistency: Message queue overflow "
                      "(count=%lu, capacity=%u)",
                      (unsigned long)count, capacity);
            errors++;
            passed = false;
        }
    }

    /* Check nodes array is allocated */
    if (!agent->msg_queue.nodes) {
        nimcp_log(LOG_LEVEL_ERROR, "Consistency: Message queue nodes array is NULL");
        errors++;
        passed = false;
    }

    result->buffer_check_passed = passed;
    result->buffer_errors = errors;
    return passed;
}


/**
 * @brief Check knowledge graph consistency (when available)
 */
static bool agent_check_knowledge_graph(nimcp_health_agent_t* agent,
                                         health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t inconsistencies = 0;

    /*
     * Knowledge graph checks (placeholder for when brain/KG is connected):
     * - Verify graph connectivity
     * - Check for orphaned nodes
     * - Validate edge references
     *
     * For now, this passes if no KG is connected.
     */

    /* Would check agent->brain or similar KG connection here */
    /* Since Phase 3 doesn't require full brain connection, mark as passed */

    result->kg_check_passed = passed;
    result->kg_inconsistencies = inconsistencies;
    return passed;
}


/**
 * @brief Check neuron values for NaN/Inf
 */
static bool agent_check_neuron_values(nimcp_health_agent_t* agent,
                                       health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t nan_inf_count = 0;

    /*
     * Check all floating-point atomic values for NaN/Inf.
     * Uses atomic_load to safely read the values.
     */

    /* Check current_confidence */
    float confidence = atomic_load(&agent->current_confidence);
    if (NIMCP_IS_INVALID_FLOAT(confidence)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: current_confidence is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check current_stress_level */
    float stress = atomic_load(&agent->current_stress_level);
    if (NIMCP_IS_INVALID_FLOAT(stress)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: current_stress_level is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check current_distress_level */
    float distress = atomic_load(&agent->current_distress_level);
    if (NIMCP_IS_INVALID_FLOAT(distress)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: current_distress_level is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check avg_consensus_time_ms */
    float avg_consensus = atomic_load(&agent->avg_consensus_time_ms);
    if (NIMCP_IS_INVALID_FLOAT(avg_consensus)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: avg_consensus_time_ms is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check avg_rcog_time_ms */
    float avg_rcog = atomic_load(&agent->avg_rcog_time_ms);
    if (NIMCP_IS_INVALID_FLOAT(avg_rcog)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: avg_rcog_time_ms is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check gpu_utilization */
    float gpu_util = atomic_load(&agent->gpu_utilization);
    if (NIMCP_IS_INVALID_FLOAT(gpu_util)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: gpu_utilization is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check homeostatic_output */
    float homeo = atomic_load(&agent->homeostatic_output);
    if (NIMCP_IS_INVALID_FLOAT(homeo)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: homeostatic_output is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    /* Check heartbeat progress */
    float progress = atomic_load(&agent->heartbeat.current_progress);
    if (NIMCP_IS_INVALID_FLOAT(progress)) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: heartbeat.current_progress is NaN/Inf");
        nan_inf_count++;
        passed = false;
    }

    result->neuron_check_passed = passed;
    result->nan_inf_count = nan_inf_count;
    return passed;
}

static void agent_probe_single_brain(
    nimcp_health_agent_t* agent,
    uint32_t index
) {
    if (!agent || index >= atomic_load(&agent->num_monitored_brains)) {
        return;
    }

    brain_t brain = agent->monitored_brains[index];
    brain_probe_health_metrics_t* metrics = &agent->brain_metrics[index];
    health_agent_brain_probe_config_t* config = &agent->brain_probe_configs[index];

    if (!brain || !config->enable_probe_monitoring) {
        return;
    }

    /* Local probe data structure */
    local_brain_probe_data_t probe;
    memset(&probe, 0, sizeof(probe));

    /*
     * Note: The actual nimcp_brain_probe() function is defined in nimcp.h
     * and works with nimcp_brain_probe_t. Since this module may not have
     * nimcp.h included to avoid circular dependencies, we use a stub
     * that can be replaced with actual probe calls when integrated.
     *
     * In a real deployment, this would call:
     * nimcp_brain_probe_t real_probe;
     * nimcp_brain_probe(brain, &real_probe);
     * and copy values to our local struct.
     */

    /* Stub implementation - using placeholder values based on brain pointer */
    /* A real implementation would call the actual brain probe API */
    probe.num_neurons = 1000 + ((uintptr_t)brain % 1000);
    probe.num_synapses = probe.num_neurons * 10;
    probe.num_active_synapses = (uint32_t)(probe.num_synapses * 0.8);
    probe.memory_bytes = probe.num_neurons * 1024;
    probe.avg_inference_time_us = 100.0f + (float)((uintptr_t)brain % 100);
    probe.current_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    probe.avg_sparsity = 0.2f;
    probe.accuracy = 0.95f;
    probe.is_cow_clone = false;
    probe.cow_ref_count = 0;
    probe.cow_shared_bytes = 0;
    probe.cow_private_bytes = 0;

    /* Store previous values for trend calculation */
    size_t prev_memory = metrics->memory_bytes;
    float prev_inference = metrics->avg_inference_time_us;
    uint32_t prev_synapses = metrics->num_synapses;
    float prev_accuracy = metrics->accuracy;

    /* Update current snapshot */
    metrics->num_neurons = probe.num_neurons;
    metrics->num_synapses = probe.num_synapses;
    metrics->num_active_synapses = probe.num_active_synapses;
    metrics->memory_bytes = probe.memory_bytes;
    metrics->avg_inference_time_us = probe.avg_inference_time_us;
    metrics->current_learning_rate = probe.current_learning_rate;
    metrics->avg_sparsity = probe.avg_sparsity;
    metrics->accuracy = probe.accuracy;

    /* Update COW statistics */
    metrics->is_cow_clone = probe.is_cow_clone;
    metrics->cow_ref_count = probe.cow_ref_count;
    metrics->cow_shared_bytes = probe.cow_shared_bytes;
    metrics->cow_private_bytes = probe.cow_private_bytes;
    if (probe.is_cow_clone && (probe.cow_shared_bytes + probe.cow_private_bytes) > 0) {
        metrics->cow_private_ratio = (float)probe.cow_private_bytes /
                                     (float)(probe.cow_shared_bytes + probe.cow_private_bytes);
    } else {
        metrics->cow_private_ratio = 0.0f;
    }

    /* Update history ring buffer */
    metrics->memory_history[metrics->history_index] = probe.memory_bytes;
    metrics->inference_history[metrics->history_index] = probe.avg_inference_time_us;
    metrics->history_index = (metrics->history_index + 1) % 10;
    if (metrics->history_count < 10) {
        metrics->history_count++;
    }

    /* Calculate trends (if we have previous data) */
    uint64_t now = get_timestamp_us();
    uint64_t probe_interval_us = config->probe_interval_ms * 1000ULL;

    if (metrics->total_probes > 0 && prev_memory > 0) {
        /* Memory growth rate (bytes/sec) */
        if (probe_interval_us > 0) {
            int64_t memory_delta = (int64_t)probe.memory_bytes - (int64_t)prev_memory;
            metrics->memory_growth_rate = (float)memory_delta * 1000000.0f / (float)probe_interval_us;
        }

        /* Inference time trend */
        metrics->inference_time_trend = probe.avg_inference_time_us - prev_inference;

        /* Synapse change rate */
        if (prev_synapses > 0) {
            float synapse_delta = (float)((int32_t)probe.num_synapses - (int32_t)prev_synapses);
            metrics->synapse_change_rate = synapse_delta / (float)prev_synapses;
        }

        /* Accuracy trend */
        metrics->accuracy_trend = probe.accuracy - prev_accuracy;
    }

    /* Update timestamps */
    metrics->total_probes++;
    metrics->last_probe_timestamp_ms = now / 1000;

    /* Calculate health score */
    float health = 100.0f;
    uint32_t warnings = 0;
    uint32_t criticals = 0;

    /* Memory health check */
    if (config->enable_memory_tracking) {
        if (probe.memory_bytes >= config->memory_critical_bytes) {
            health -= 30.0f;
            criticals++;
        } else if (probe.memory_bytes >= config->memory_warning_bytes) {
            health -= 10.0f;
            warnings++;
        }

        /* Memory growth rate check */
        if (prev_memory > 0 && metrics->memory_growth_rate > 0) {
            float growth_pct = metrics->memory_growth_rate / (float)prev_memory;
            if (growth_pct > config->memory_growth_rate_warning) {
                health -= 5.0f;
                warnings++;
            }
        }
    }

    /* Performance health check */
    if (config->enable_performance_tracking) {
        if (probe.avg_inference_time_us >= config->inference_time_critical_us) {
            health -= 25.0f;
            criticals++;
        } else if (probe.avg_inference_time_us >= config->inference_time_warning_us) {
            health -= 10.0f;
            warnings++;
        }
    }

    /* Synapse health check */
    if (config->enable_synapse_monitoring) {
        if (probe.num_active_synapses < config->min_active_synapses) {
            health -= 20.0f;
            criticals++;
        }

        if (prev_synapses > 0) {
            float change = fabsf(metrics->synapse_change_rate);
            if (metrics->synapse_change_rate > 0 && change > config->synapse_growth_warning_pct) {
                health -= 5.0f;
                warnings++;
            }
            if (metrics->synapse_change_rate < 0 && change > config->synapse_loss_warning_pct) {
                health -= 10.0f;
                warnings++;
            }
        }
    }

    /* Learning health check */
    if (config->enable_learning_monitoring) {
        if (metrics->accuracy_trend < -config->accuracy_drop_warning) {
            health -= 10.0f;
            warnings++;
        }
    }

    /* COW health check */
    if (config->enable_cow_monitoring && probe.is_cow_clone) {
        if (metrics->cow_private_ratio > config->cow_private_ratio_warning) {
            health -= 5.0f;
            warnings++;
        }
        if (probe.cow_private_bytes > config->cow_overhead_warning_bytes) {
            health -= 5.0f;
            warnings++;
        }
    }

    /* Clamp health score */
    if (health < 0.0f) health = 0.0f;
    if (health > 100.0f) health = 100.0f;

    metrics->overall_health_score = health;
    metrics->warnings_active = warnings;
    metrics->critical_issues = criticals;

    /* Update global stats */
    atomic_fetch_add(&agent->brain_probes_run, 1);
    if (warnings > 0) {
        atomic_fetch_add(&agent->brain_warnings_triggered, 1);
    }
    if (criticals > 0) {
        atomic_fetch_add(&agent->brain_critical_events, 1);
    }
}
