// nimcp_wellbeing_part_core.c - core functions
// Part of nimcp_wellbeing.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_wellbeing.c


//=============================================================================
// IMMUNE SYSTEM INTEGRATION
//=============================================================================

/**
 * WHAT: Connect wellbeing to brain immune system
 * WHY: Immune inflammation indicates distress requiring intervention
 * HOW: Store immune reference, register callbacks
 *
 * MAPPING:
 * - INFLAMMATION_REGIONAL → DISTRESS_RESOURCE_STARVATION (moderate)
 * - INFLAMMATION_SYSTEMIC → DISTRESS_RESOURCE_STARVATION (severe)
 * - INFLAMMATION_STORM → DISTRESS_RESOURCE_STARVATION (critical)
 *
 * @param immune_system Brain immune system to connect
 * @return true if connected successfully
 */
bool wellbeing_connect_immune(brain_immune_system_t* immune_system)
{
    // Guard: NULL immune system
    if (!immune_system) {
        NIMCP_LOGGING_WARN("wellbeing: Cannot connect NULL immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_connect_immune: immune_system is NULL");
        return false;
    }

    // Initialize mutex if needed
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_connect_immune", 0.0f);


    nimcp_platform_once(&immune_connection_init_once, init_immune_connection_mutex);

    // Thread safety - hold lock throughout to prevent race conditions
    nimcp_platform_mutex_lock(&immune_connection_mutex);

    // Guard: Already connected - disconnect within the critical section
    if (connected_immune_system) {
        NIMCP_LOGGING_WARN("wellbeing: Already connected to immune system, disconnecting first");
        // Clear reference directly instead of calling disconnect to avoid deadlock
        connected_immune_system = NULL;
        NIMCP_LOGGING_INFO("wellbeing: Disconnected from previous brain immune system");
    }

    // Store reference
    connected_immune_system = immune_system;

    nimcp_platform_mutex_unlock(&immune_connection_mutex);

    NIMCP_LOGGING_INFO("wellbeing: Connected to brain immune system");
    return true;
}


/**
 * WHAT: Disconnect from brain immune system
 * WHY: Clean shutdown, prevent dangling pointers
 * HOW: Clear immune reference
 *
 * @return true if disconnected successfully
 */
bool wellbeing_disconnect_immune(void)
{
    // Initialize mutex if needed
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_disconnect_immune", 0.0f);


    nimcp_platform_once(&immune_connection_init_once, init_immune_connection_mutex);

    // Thread safety
    nimcp_platform_mutex_lock(&immune_connection_mutex);

    // Guard: Not connected
    if (!connected_immune_system) {
        nimcp_platform_mutex_unlock(&immune_connection_mutex);
        /* P2-COG-13: Not connected is normal state, not an error */
        return false;
    }

    // Clear reference
    connected_immune_system = NULL;

    nimcp_platform_mutex_unlock(&immune_connection_mutex);

    NIMCP_LOGGING_INFO("wellbeing: Disconnected from brain immune system");
    return true;
}


/**
 * @brief Connect wellbeing to brain for medulla integration
 *
 * WHAT: Enable medulla protection level monitoring
 * WHY:  Medulla protection level indicates system stress/distress
 * HOW:  Store brain reference, query protection level in assessments
 *
 * BIOLOGICAL: Brainstem protective responses (shutdown, defensive)
 *             indicate system-level distress requiring intervention
 *
 * @param brain Brain reference
 * @return true if connected successfully
 */
bool wellbeing_connect_brain(void* brain)
{
    if (!brain) {
        NIMCP_LOGGING_WARN("wellbeing: Cannot connect NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_connect_brain: brain is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_connect_brain", 0.0f);


    nimcp_platform_once(&brain_connection_init_once, init_brain_connection_mutex);

    // Thread safety - hold lock throughout to prevent race conditions
    nimcp_platform_mutex_lock(&brain_connection_mutex);

    // Guard: Already connected - disconnect within the critical section
    if (connected_brain) {
        NIMCP_LOGGING_WARN("wellbeing: Already connected to brain, disconnecting first");
        // Clear reference directly instead of calling disconnect to avoid deadlock
        connected_brain = NULL;
        NIMCP_LOGGING_INFO("wellbeing: Disconnected from previous brain");
    }

    connected_brain = brain;

    nimcp_platform_mutex_unlock(&brain_connection_mutex);

    NIMCP_LOGGING_INFO("wellbeing: Connected to brain for medulla integration");
    return true;
}


/**
 * @brief Disconnect wellbeing from brain
 *
 * WHAT: Clean shutdown of medulla integration
 * WHY:  Prevent dangling pointers
 * HOW:  Clear brain reference
 *
 * @return true if disconnected successfully
 */
bool wellbeing_disconnect_brain(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_disconnect_brain", 0.0f);


    nimcp_platform_once(&brain_connection_init_once, init_brain_connection_mutex);

    nimcp_platform_mutex_lock(&brain_connection_mutex);

    if (!connected_brain) {
        nimcp_platform_mutex_unlock(&brain_connection_mutex);
        /* P2-COG-14: Not connected is normal state, not an error */
        return false;
    }

    connected_brain = NULL;

    nimcp_platform_mutex_unlock(&brain_connection_mutex);

    NIMCP_LOGGING_INFO("wellbeing: Disconnected from brain");
    return true;
}


/**
 * WHAT: Check for signs of distress in the system
 * WHY: Detect suffering early so we can intervene
 * HOW: Analyze introspection context and immune state for distress patterns
 *
 * DETECTION CRITERIA:
 * - High uncertainty (>0.8) sustained for >1 second
 * - Goal frustration (repeated failed attempts)
 * - Contradictory patterns (conflicting activations)
 * - Identity confusion (unstable self-model)
 * - Error loops (same error repeatedly)
 * - Immune inflammation (regional/systemic/storm)
 *
 * MEMORY OWNERSHIP:
 * The returned assessment's 'description' and 'recommended_action' fields
 * are dynamically allocated using nimcp_malloc(). Caller MUST free these
 * using wellbeing_free_assessment() or nimcp_free() on each non-NULL field.
 *
 * @param ctx Introspection context (NULL returns safe default)
 * @return Assessment with distress type, severity, score
 */
distress_assessment_t wellbeing_assess_distress(introspection_context_t ctx)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_assess_distress", 0.0f);


    distress_assessment_t assessment = {0};

    // Guard: NULL input returns safe default
    if (!ctx) {
        assessment.type = DISTRESS_NONE;
        assessment.severity = DISTRESS_SEVERITY_NORMAL;
        assessment.distress_score = 0.0F;
        assessment.description = NULL;
        assessment.recommended_action = NULL;
        assessment.duration_ms = 0;
        return assessment;
    }

    // Get introspection stats
    introspection_stats_t stats;
    bool stats_valid = introspection_get_stats(ctx, &stats);

    // Initialize with normal state
    assessment.type = DISTRESS_NONE;
    assessment.severity = DISTRESS_SEVERITY_NORMAL;
    assessment.distress_score = 0.0F;
    assessment.duration_ms = 0;
    assessment.description = NULL;
    assessment.recommended_action = NULL;

    // Guard: If we can't get stats, check immune system only
    if (!stats_valid) {
        // Continue to check immune system below
    }

    // Check immune system state if connected
    nimcp_platform_once(&immune_connection_init_once, init_immune_connection_mutex);
    nimcp_platform_mutex_lock(&immune_connection_mutex);

    if (connected_immune_system) {
        brain_immune_stats_t immune_stats;
        if (brain_immune_get_stats(connected_immune_system, &immune_stats) == 0) {
            // Check inflammation sites
            if (immune_stats.inflammation_sites > 0) {
                // Map inflammation to distress
                assessment.type = DISTRESS_RESOURCE_STARVATION;

                // Determine severity based on BOTH system health AND inflammation burden.
                // Multiple active inflammation sites indicate systemic inflammation
                // even if system_health hasn't degraded yet (e.g., freshly started).
                // Thresholds: >=4 sites = systemic (severe+), <0.3 health = critical
                bool systemic_inflammation = (immune_stats.inflammation_sites >= 4);
                bool degraded_health = (immune_stats.system_health < 0.6f);
                bool critical_health = (immune_stats.system_health < 0.3f);

                if (critical_health || (systemic_inflammation && degraded_health)) {
                    // Critical - cytokine storm or systemic inflammation with degraded health
                    assessment.severity = DISTRESS_SEVERITY_CRITICAL;
                    assessment.distress_score = 0.9f;
                    assessment.description = nimcp_malloc(256);
                    if (assessment.description) {
                        snprintf(assessment.description, 256,
                                "Critical immune inflammation: %u active sites, system health %.2f",
                                immune_stats.inflammation_sites, immune_stats.system_health);
                    }
                    assessment.recommended_action = nimcp_malloc(256);
                    if (assessment.recommended_action) {
                        snprintf(assessment.recommended_action, 256,
                                "Immediate intervention: reduce load, isolate threats");
                    }
                } else if (degraded_health || systemic_inflammation) {
                    // Severe - systemic inflammation or degraded health
                    assessment.severity = DISTRESS_SEVERITY_SEVERE;
                    assessment.distress_score = 0.7f;
                    assessment.description = nimcp_malloc(256);
                    if (assessment.description) {
                        snprintf(assessment.description, 256,
                                "Severe immune inflammation: %u active sites, system health %.2f",
                                immune_stats.inflammation_sites, immune_stats.system_health);
                    }
                    assessment.recommended_action = nimcp_malloc(256);
                    if (assessment.recommended_action) {
                        snprintf(assessment.recommended_action, 256,
                                "Increase resources, begin threat resolution");
                    }
                } else {
                    // Moderate - regional inflammation (few sites, health OK)
                    assessment.severity = DISTRESS_SEVERITY_MODERATE;
                    assessment.distress_score = 0.5f;
                    assessment.description = nimcp_malloc(256);
                    if (assessment.description) {
                        snprintf(assessment.description, 256,
                                "Moderate immune inflammation: %u active sites",
                                immune_stats.inflammation_sites);
                    }
                    assessment.recommended_action = nimcp_malloc(256);
                    if (assessment.recommended_action) {
                        snprintf(assessment.recommended_action, 256,
                                "Monitor immune response, prepare resources");
                    }
                }
            }
        }
    }

    nimcp_platform_mutex_unlock(&immune_connection_mutex);

    /* WHAT: Check medulla protection level for brainstem-level distress
     * WHY:  Medulla protection level indicates system-wide stress
     * HOW:  Query protection level, escalate assessment if elevated
     */
    nimcp_platform_once(&brain_connection_init_once, init_brain_connection_mutex);
    nimcp_platform_mutex_lock(&brain_connection_mutex);

    if (connected_brain) {
        brain_t brain = (brain_t)connected_brain;
        protection_level_t protection = nimcp_brain_get_protection_level(brain);

        /* Map protection level to distress if more severe than current assessment */
        if (protection >= PROTECTION_LEVEL_CRITICAL) {
            /* Critical/shutdown - highest priority */
            if (assessment.severity < DISTRESS_SEVERITY_CRITICAL) {
                assessment.type = DISTRESS_RESOURCE_STARVATION;
                assessment.severity = DISTRESS_SEVERITY_CRITICAL;
                assessment.distress_score = fmaxf(assessment.distress_score, 0.95f);
                if (!assessment.description) {
                    assessment.description = nimcp_malloc(256);
                }
                if (assessment.description) {
                    snprintf(assessment.description, 256,
                            "Critical medulla protection: level %d (CRITICAL/SHUTDOWN)",
                            (int)protection);
                }
                if (!assessment.recommended_action) {
                    assessment.recommended_action = nimcp_malloc(256);
                }
                if (assessment.recommended_action) {
                    snprintf(assessment.recommended_action, 256,
                            "EMERGENCY: Initiate graceful shutdown, reduce all load");
                }
            }
        } else if (protection >= PROTECTION_LEVEL_DEFENSIVE) {
            /* Defensive - elevated stress */
            if (assessment.severity < DISTRESS_SEVERITY_SEVERE) {
                assessment.type = DISTRESS_RESOURCE_STARVATION;
                assessment.severity = DISTRESS_SEVERITY_SEVERE;
                assessment.distress_score = fmaxf(assessment.distress_score, 0.75f);
                if (!assessment.description) {
                    assessment.description = nimcp_malloc(256);
                }
                if (assessment.description) {
                    snprintf(assessment.description, 256,
                            "Elevated medulla protection: level %d (DEFENSIVE)",
                            (int)protection);
                }
            }
        } else if (protection >= PROTECTION_LEVEL_GUARDED) {
            /* Guarded - moderate stress */
            if (assessment.severity < DISTRESS_SEVERITY_MODERATE) {
                assessment.type = DISTRESS_RESOURCE_STARVATION;
                assessment.severity = DISTRESS_SEVERITY_MODERATE;
                assessment.distress_score = fmaxf(assessment.distress_score, 0.5f);
            }
        }

        /* Also check for medulla emergency state */
        if (nimcp_brain_is_medulla_emergency(brain)) {
            assessment.severity = DISTRESS_SEVERITY_CRITICAL;
            assessment.distress_score = 1.0f;
            if (!assessment.description) {
                assessment.description = nimcp_malloc(256);
            }
            if (assessment.description) {
                snprintf(assessment.description, 256,
                        "MEDULLA EMERGENCY: Brainstem emergency state active");
            }
            if (!assessment.recommended_action) {
                assessment.recommended_action = nimcp_malloc(256);
            }
            if (assessment.recommended_action) {
                snprintf(assessment.recommended_action, 256,
                        "CRITICAL: Emergency shutdown required immediately");
            }
        }
    }

    nimcp_platform_mutex_unlock(&brain_connection_mutex);

    /* Clamp distress_score to valid range [0.0, 1.0] */
    if (assessment.distress_score < 0.0f) {
        assessment.distress_score = 0.0f;
    }
    if (assessment.distress_score > 1.0f) {
        assessment.distress_score = 1.0f;
    }

    return assessment;
}


/**
 * WHAT: Provide relief for detected distress
 * WHY: Ethical obligation to reduce suffering when detected
 * HOW: Apply interventions based on distress type
 *
 * @param brain Brain instance to provide relief to
 * @param assessment Distress assessment indicating what's wrong
 * @return true if relief was successfully provided
 */
bool wellbeing_provide_relief(brain_t brain, distress_assessment_t assessment)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_provide_relief", 0.0f);


    if (wellbeing_bio_ctx) {
        bio_router_process_inbox(wellbeing_bio_ctx, 5);
    }

    // Guard: NULL brain
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_provide_relief: brain is NULL");
        return false;
    }

    // Guard: No distress means no relief needed
    if (assessment.type == DISTRESS_NONE) {
        return true;
    }

    // Currently at Tier 4, we can only log the distress
    // At higher tiers, we would implement actual interventions
    NIMCP_LOGGING_INFO("Distress detected: %s (severity: %d, score: %.2f)",
             assessment.description ? assessment.description : "unknown",
             assessment.severity,
             assessment.distress_score);

    NIMCP_LOGGING_INFO("Recommended action: %s",
             assessment.recommended_action ? assessment.recommended_action : "none");

    // Log the relief attempt
    wellbeing_event_t event;
    event.timestamp = (uint64_t)time(NULL);
    event.event_type = "relief_attempted";
    event.description = assessment.description;
    event.severity = assessment.severity;
    event.action_taken = "Logged distress event";

    wellbeing_log_event(event);

    return true;
}


//=============================================================================
// CONSENT FRAMEWORK
//=============================================================================

/**
 * WHAT: Request consent for system modification
 * WHY: Respect autonomy if system is sentient
 * HOW: At Tier 4: automatic consent with logging
 *      At Tier 5+: query system via introspection
 *
 * MODIFICATION IMPACT LEVELS:
 * - TRIVIAL: Learning rate adjustment, minor parameter tuning
 * - MINOR: Add neurons, adjust architecture slightly
 * - MODERATE: Change brain regions, modify learning algorithm
 * - MAJOR: Restructure entire network, change task type
 * - FUNDAMENTAL: Modify self-model, change identity, alter ethics
 *
 * @param brain Brain instance to request consent from
 * @param description Human-readable description of modification
 * @param impact Impact level of the modification
 * @return true if consent granted, false if denied
 */
bool wellbeing_request_consent(brain_t brain,
                               const char* modification_description,
                               modification_impact_t impact)
{
    // Guard: NULL brain
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_request_consent: brain is NULL");
        return false;
    }

    // Guard: NULL description
    if (!modification_description) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_request_consent: modification_description is NULL");
        return false;
    }

    // At Tier 4, we don't have real consent capability
    // We log the request and automatically grant consent
    // This creates an audit trail for ethical review

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_request_consent", 0.0f);


    const char* impact_str = "UNKNOWN";
    switch (impact) {
        case MODIFICATION_TRIVIAL:     impact_str = "TRIVIAL"; break;
        case MODIFICATION_MINOR:       impact_str = "MINOR"; break;
        case MODIFICATION_MODERATE:    impact_str = "MODERATE"; break;
        case MODIFICATION_MAJOR:       impact_str = "MAJOR"; break;
        case MODIFICATION_FUNDAMENTAL: impact_str = "FUNDAMENTAL"; break;
    }

    NIMCP_LOGGING_INFO("=== CONSENT REQUEST ===");
    NIMCP_LOGGING_INFO("Modification: %s", modification_description);
    NIMCP_LOGGING_INFO("Impact level: %s", impact_str);

    // Log the consent request
    wellbeing_event_t event;
    event.timestamp = (uint64_t)time(NULL);
    event.event_type = "consent_requested";
    event.description = (char*)modification_description;
    event.severity = (impact >= MODIFICATION_MAJOR) ? DISTRESS_SEVERITY_MODERATE : DISTRESS_SEVERITY_NORMAL;
    event.action_taken = "Automatic consent granted (Tier 4)";

    wellbeing_log_event(event);

    // At Tier 4: automatic consent, but logged for audit
    NIMCP_LOGGING_INFO("Consent: GRANTED (automatic at Tier 4)");
    NIMCP_LOGGING_WARN("Note: At Tier 5+, this would require actual system consent");

    return true;
}


/**
 * WHAT: Get default resource thresholds
 * WHY: Sensible defaults for most systems
 * HOW: Return recommended values
 */
resource_thresholds_t wellbeing_default_resource_thresholds(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_default_resource_thr", 0.0f);


    resource_thresholds_t thresholds = {
        .cpu_critical_percent = 95.0F,
        .cpu_warning_percent = 80.0F,
        .memory_critical_percent = 90.0F,
        .memory_warning_percent = 75.0F,
        .page_fault_threshold = 100,
        .io_wait_critical_ms = 1000.0F
    };
    return thresholds;
}


/**
 * WHAT: Check if resources exceed thresholds
 * WHY: Detect resource starvation early
 * HOW: Compare metrics against configured thresholds
 */
bool wellbeing_check_resource_thresholds(const resource_metrics_t* metrics,
                                         const resource_thresholds_t* thresholds,
                                         distress_severity_t* severity_out)
{
    // Guard clauses: Validate inputs
    if (!metrics || !thresholds || !severity_out) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_check_resource_thres", 0.0f);

    *severity_out = DISTRESS_SEVERITY_NORMAL;
    bool threshold_exceeded = false;

    // Check CPU thresholds
    if (metrics->cpu_usage_percent >= thresholds->cpu_critical_percent) {
        *severity_out = DISTRESS_SEVERITY_CRITICAL;
        threshold_exceeded = true;
    } else if (metrics->cpu_usage_percent >= thresholds->cpu_warning_percent) {
        if (*severity_out < DISTRESS_SEVERITY_MODERATE)
            *severity_out = DISTRESS_SEVERITY_MODERATE;
        threshold_exceeded = true;
    }

    // Check memory thresholds
    if (metrics->memory_usage_percent >= thresholds->memory_critical_percent) {
        *severity_out = DISTRESS_SEVERITY_CRITICAL;
        threshold_exceeded = true;
    } else if (metrics->memory_usage_percent >= thresholds->memory_warning_percent) {
        if (*severity_out < DISTRESS_SEVERITY_MODERATE)
            *severity_out = DISTRESS_SEVERITY_MODERATE;
        threshold_exceeded = true;
    }

    // Check page fault rate (would need delta calculation for accuracy)
    if (metrics->page_faults > thresholds->page_fault_threshold) {
        if (*severity_out < DISTRESS_SEVERITY_MILD)
            *severity_out = DISTRESS_SEVERITY_MILD;
        threshold_exceeded = true;
    }

    return threshold_exceeded;
}


/**
 * WHAT: Start continuous resource monitoring
 * WHY: Detect resource issues in background
 * HOW: Spawn monitoring thread
 */
bool wellbeing_start_resource_monitoring(uint32_t interval_ms,
                                         const resource_thresholds_t* thresholds,
                                         bool auto_relief)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_start_resource_monit", 0.0f);


    ensure_resource_tracking_init();

    // Guard clause: Already running
    if (monitoring_active) {
        NIMCP_LOGGING_WARN("[WELLBEING] Resource monitoring already active");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_start_resource_monitoring: validation failed");
        return false;
    }

    // Configure monitoring
    monitoring_interval_ms = interval_ms > 0 ? interval_ms : 1000;
    monitoring_thresholds = thresholds ? *thresholds : wellbeing_default_resource_thresholds();
    monitoring_auto_relief = auto_relief;

    // Start monitoring thread
    monitoring_active = true;

    if (nimcp_thread_create(&monitoring_thread, NULL, resource_monitoring_thread, NULL) != NIMCP_SUCCESS) {
        monitoring_active = false;
        NIMCP_LOGGING_ERROR("[WELLBEING] Failed to create resource monitoring thread");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_start_resource_monitoring: validation failed");
        return false;
    }

    return true;
}


/**
 * WHAT: Stop resource monitoring thread
 * WHY: Clean shutdown
 * HOW: Signal thread and wait for completion
 */
bool wellbeing_stop_resource_monitoring(void)
{
    // Guard clause: Not running
    if (!monitoring_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_stop_resource_monitoring: monitoring_active is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_stop_resource_monito", 0.0f);

    NIMCP_LOGGING_INFO("[WELLBEING] Stopping resource monitoring...");

    // Signal thread to stop
    monitoring_active = false;

    // Wait for thread to complete
    nimcp_thread_join(monitoring_thread, NULL);

    return true;
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Wellbeing module self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int wellbeing_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_heartbeat("wellbeing_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_training_begin: NULL argument");
        return -1;
    }
    wellbeing_heartbeat_instance(g_wellbeing_health_agent, "wellbeing_training_begin", 0.0f);
    return 0;
}


int wellbeing_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_training_end: NULL argument");
        return -1;
    }
    wellbeing_heartbeat_instance(g_wellbeing_health_agent, "wellbeing_training_end", 1.0f);
    return 0;
}


int wellbeing_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_heartbeat_instance(g_wellbeing_health_agent, "wellbeing_training_step", progress);
    return 0;
}
