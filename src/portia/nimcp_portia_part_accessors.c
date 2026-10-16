// nimcp_portia_part_accessors.c - accessors functions
// Part of nimcp_portia.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_portia.c


//=============================================================================
// Default Configuration
//=============================================================================

portia_config_t portia_get_default_config(void) {
    portia_config_t config = {0};

    /* Tier configuration */
    config.tier_config.enable_auto_switching = true;
    config.tier_config.switch_hysteresis_ms = 5000;
    config.tier_config.upgrade_threshold = 0.7F;
    config.tier_config.downgrade_threshold = 0.3F;
    config.tier_config.lock_tier = false;

    /* Power configuration */
    config.power_config.enable_battery_awareness = true;
    config.power_config.poll_interval_ms = 10000;
    config.power_config.low_battery_threshold = 0.2F;
    config.power_config.critical_battery_threshold = 0.05F;
    config.power_config.enable_ac_detection = true;

    /* Resource configuration */
    config.resource_config.sample_interval_ms = 1000;
    config.resource_config.history_size = 60;
    config.resource_config.cpu_threshold = 0.8F;
    config.resource_config.memory_threshold = 0.85F;
    config.resource_config.thermal_threshold = 80.0F;

    /* Degradation configuration */
    config.degradation_config.enable_graceful_degradation = true;
    config.degradation_config.max_degradation = PORTIA_DEGRADATION_SEVERE;
    config.degradation_config.recovery_delay_ms = 30000;
    config.degradation_config.recovery_threshold = 0.5F;

    /* Accelerator configuration */
    config.accelerator_config.enable_gpu_detection = true;
    config.accelerator_config.enable_npu_detection = true;
    config.accelerator_config.enable_auto_offload = true;
    config.accelerator_config.detection_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;

    /* General settings */
    config.enable_bio_async = true;
    config.update_interval_ms = 1000;
    config.enable_logging = true;
    config.enable_metrics = true;

    return config;
}


portia_context_t* portia_get_context(void) {
    return atomic_load(&g_portia_ctx);
}


nimcp_error_t portia_get_status(portia_status_t* status) {
    if (!bbb_check_pointer(status, "portia_get_status")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid status pointer in portia_get_status");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!portia_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia not initialized in portia_get_status");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);

    nimcp_mutex_lock(&ctx->lock);

    memset(status, 0, sizeof(portia_status_t));

    /* Tier info */
    status->current_tier = ctx->tier_manager->current_tier;
    status->tier_switches = ctx->tier_manager->tier_switch_count;

    /* Power info */
    status->power_state = ctx->power_monitor->current_state;
    status->battery_level = ctx->power_monitor->battery_level;

    /* Resource info */
    status->cpu_usage = ctx->resource_tracker->cpu_usage;
    status->memory_usage = ctx->resource_tracker->memory_usage;
    status->temperature_celsius = ctx->resource_tracker->temperature_celsius;
    status->thermal_state = ctx->resource_tracker->thermal_state;

    /* Degradation info */
    status->degradation_level = ctx->degradation_controller->current_level;
    status->degradations = ctx->degradation_controller->degradation_count;

    /* Accelerator info */
    status->num_accelerators = ctx->accelerator_detector->num_accelerators;
    memcpy(status->accelerator_types, ctx->accelerator_detector->accelerator_types,
           sizeof(portia_accelerator_type_t) * 8);

    /* Workload info */
    status->current_workload = ctx->target_classifier->classified_workload;

    /* Statistics */
    status->updates = ctx->update_count;
    status->avg_update_time_ms = ctx->update_count > 0 ?
        ctx->total_update_time_ms / ctx->update_count : 0.0F;

    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}


nimcp_error_t portia_set_tier(platform_tier_t tier) {
    if (!portia_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia not initialized in portia_set_tier");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }

    if (tier >= PLATFORM_TIER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid tier value %d in portia_set_tier", tier);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia context destroyed during portia_set_tier");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }
    portia_tier_manager_t* mgr = ctx->tier_manager;

    nimcp_mutex_lock(&mgr->lock);

    if (mgr->config.lock_tier) {
        nimcp_mutex_unlock(&mgr->lock);
        LOG_WARN(LOG_MODULE, "Tier switching is locked");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Tier switching is locked in portia_set_tier");
        return NIMCP_PORTIA_ERROR_TIER_LOCKED;
    }

    platform_tier_t old_tier = mgr->current_tier;
    mgr->current_tier = tier;
    mgr->tier_switch_count++;
    mgr->last_switch_time_us = nimcp_time_get_us();

    nimcp_mutex_unlock(&mgr->lock);

    LOG_INFO(LOG_MODULE, "Tier changed: %s → %s",
             platform_tier_get_name(old_tier),
             platform_tier_get_name(tier));

    /* Broadcast tier change if bio-async enabled */
    if (ctx->bio_ctx && bio_router_is_initialized()) {
        bio_msg_portia_tier_change_t msg = {
            .header = {
                .type = (bio_message_type_t)BIO_MSG_TYPE_PORTIA_TIER_CHANGE,
                .payload_size = sizeof(bio_msg_portia_tier_change_t) - sizeof(bio_message_header_t),
                .timestamp_us = nimcp_time_get_us(),
                .flags = BIO_MSG_FLAG_URGENT
            },
            .old_tier = old_tier,
            .new_tier = tier,
            .confidence = 1.0F,
            .reason = PORTIA_TIER_REASON_USER_REQUEST,
            .timestamp_us = nimcp_time_get_us()
        };
        bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    }

    /* bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "Tier changed to %s", platform_tier_get_name(tier)); */

    return NIMCP_SUCCESS;
}


platform_tier_t portia_get_current_tier(void) {
    if (!portia_is_initialized()) {
        return PLATFORM_TIER_MINIMAL;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (!ctx) {
        return PLATFORM_TIER_MINIMAL;
    }

    nimcp_mutex_lock(&ctx->lock);
    platform_tier_t tier = ctx->tier_manager->current_tier;
    nimcp_mutex_unlock(&ctx->lock);
    return tier;
}


nimcp_error_t portia_set_degradation_level(portia_degradation_level_t level) {
    if (!portia_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia not initialized in portia_set_degradation_level");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia context destroyed during portia_set_degradation_level");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }
    portia_degradation_controller_t* ctrl = ctx->degradation_controller;

    nimcp_mutex_lock(&ctrl->lock);

    portia_degradation_level_t old_level = ctrl->current_level;
    ctrl->current_level = level;
    ctrl->degradation_count++;
    ctrl->last_degradation_time_us = nimcp_time_get_us();

    nimcp_mutex_unlock(&ctrl->lock);

    LOG_INFO(LOG_MODULE, "Degradation level changed: %s → %s",
             portia_degradation_level_name(old_level),
             portia_degradation_level_name(level));

    /* Broadcast degradation event if bio-async enabled */
    if (ctx->bio_ctx && bio_router_is_initialized()) {
        bio_msg_portia_degradation_event_t msg = {
            .header = {
                .type = (bio_message_type_t)BIO_MSG_TYPE_PORTIA_DEGRADATION_EVENT,
                .payload_size = sizeof(bio_msg_portia_degradation_event_t) - sizeof(bio_message_header_t),
                .timestamp_us = nimcp_time_get_us(),
                .flags = BIO_MSG_FLAG_URGENT
            },
            .old_level = old_level,
            .new_level = level,
            .features_disabled = 0,
            .reason = PORTIA_DEGRADE_REASON_USER,
            .description = "Manual degradation level change"
        };
        bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    }

    return NIMCP_SUCCESS;
}


nimcp_error_t portia_set_auto_switching(bool enable) {
    if (!portia_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia not initialized in portia_set_auto_switching");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia context destroyed during portia_set_auto_switching");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }
    portia_tier_manager_t* mgr = ctx->tier_manager;

    nimcp_mutex_lock(&mgr->lock);
    mgr->config.enable_auto_switching = enable;
    nimcp_mutex_unlock(&mgr->lock);

    LOG_INFO(LOG_MODULE, "Automatic tier switching %s", enable ? "enabled" : "disabled");

    return NIMCP_SUCCESS;
}


nimcp_error_t portia_get_accelerators(
    portia_accelerator_type_t* out_accelerators,
    uint32_t max_accelerators,
    uint32_t* out_count)
{
    if (!bbb_check_pointer(out_accelerators, "portia_get_accelerators") ||
        !bbb_check_pointer(out_count, "portia_get_accelerators")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid pointer in portia_get_accelerators");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!portia_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia not initialized in portia_get_accelerators");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia context destroyed during portia_get_accelerators");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }
    portia_accelerator_detector_t* det = ctx->accelerator_detector;

    nimcp_mutex_lock(&det->lock);

    uint32_t count = det->num_accelerators < max_accelerators ?
                     det->num_accelerators : max_accelerators;

    memcpy(out_accelerators, det->accelerator_types,
           count * sizeof(portia_accelerator_type_t));
    *out_count = count;

    nimcp_mutex_unlock(&det->lock);

    return NIMCP_SUCCESS;
}
