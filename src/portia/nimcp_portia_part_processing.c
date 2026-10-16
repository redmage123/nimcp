// nimcp_portia_part_processing.c - processing functions
// Part of nimcp_portia.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_portia.c


nimcp_error_t portia_update(void) {
    if (!portia_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia not initialized in portia_update");
        return NIMCP_PORTIA_ERROR_NOT_INITIALIZED;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    uint64_t start_time = nimcp_time_get_us();

    nimcp_mutex_lock(&ctx->lock);

    /* Update resource tracker */
    nimcp_error_t err = portia_resource_tracker_update(ctx->resource_tracker);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Resource tracker update failed: %d", err);
    }

    /* Update power monitor */
    err = portia_power_monitor_update(ctx->power_monitor);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Power monitor update failed: %d", err);
    }

    /* Update tier manager */
    err = portia_tier_manager_update(ctx->tier_manager, &ctx->resource_tracker->current_resources);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Tier manager update failed: %d", err);
    }

    /* Update sensor fusion - build status directly to avoid recursive lock */
    portia_status_t status = {
        .current_tier = ctx->tier_manager->current_tier,
        .tier_switches = ctx->tier_manager->tier_switch_count,
        .power_state = ctx->power_monitor->current_state,
        .battery_level = ctx->power_monitor->battery_level,
        .cpu_usage = ctx->resource_tracker->cpu_usage,
        .memory_usage = ctx->resource_tracker->memory_usage,
        .temperature_celsius = ctx->resource_tracker->temperature_celsius,
        .thermal_state = ctx->resource_tracker->thermal_state,
        .degradation_level = ctx->degradation_controller->current_level
    };
    err = portia_sensor_fusion_update(ctx->sensor_fusion, &status);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Sensor fusion update failed: %d", err);
    }

    /* Update degradation controller */
    err = portia_degradation_controller_update(ctx->degradation_controller, &status);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Degradation controller update failed: %d", err);
    }

    /* Update statistics */
    ctx->update_count++;
    uint64_t end_time = nimcp_time_get_us();
    float update_time_ms = (end_time - start_time) / 1000.0F;
    ctx->total_update_time_ms += update_time_ms;
    ctx->last_update_time_us = end_time;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG(LOG_MODULE, "Portia update complete (%.3f ms)", update_time_ms);
    return NIMCP_SUCCESS;
}

static int portia_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Cast to uint16_t to allow portia-specific message types */
        switch ((uint16_t)message_types[i]) {
            case (uint16_t)BIO_MSG_TYPE_PORTIA_STATUS_QUERY:
                bio_router_register_handler(ctx, message_types[i], portia_message_handler);
                registered++;
                break;
            case (uint16_t)BIO_MSG_TYPE_PORTIA_TIER_QUERY:
                bio_router_register_handler(ctx, message_types[i], portia_message_handler);
                registered++;
                break;
            default:
                LOG_DEBUG(LOG_MODULE, "Unknown message type 0x%04X in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO(LOG_MODULE, "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}


//=============================================================================
// Bio-Async Message Handler
//=============================================================================

static nimcp_error_t portia_message_handler(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)user_data;  // Suppress unused parameter warning

    if (!bbb_check_pointer(msg, "portia_message_handler")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid message pointer in portia_message_handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_DEBUG(LOG_MODULE, "Received message type 0x%04X (size=%zu)", header->type, msg_size);

    // Handle different message types
    // Note: Cast portia message types to bio_message_type_t
    if (header->type == (bio_message_type_t)BIO_MSG_TYPE_PORTIA_STATUS_QUERY) {
        // Query status
        portia_status_t status;
        nimcp_error_t err = portia_get_status(&status);
        if (err == NIMCP_SUCCESS && response_promise) {
            bio_msg_portia_status_response_t response = {0};
            portia_msg_init_header(&response.header,
                                   BIO_MSG_TYPE_PORTIA_STATUS_RESPONSE,
                                   BIO_MODULE_UNKNOWN,
                                   header->source_module,
                                   sizeof(response));
            response.status = status;
            nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
        }
        return err;
    } else if (header->type == (bio_message_type_t)BIO_MSG_TYPE_PORTIA_TIER_QUERY) {
        // Query tier
        platform_tier_t tier = portia_get_current_tier();
        if (response_promise) {
            nimcp_bio_promise_complete_sized(response_promise, &tier, sizeof(tier));
        }
        return NIMCP_SUCCESS;
    } else {
        LOG_DEBUG(LOG_MODULE, "Unhandled message type 0x%04X", header->type);
        return NIMCP_SUCCESS;
    }
}


static nimcp_error_t portia_tier_manager_update(portia_tier_manager_t* mgr, const system_resources_t* resources) {
    if (!mgr || !resources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL argument in portia_tier_manager_update");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Simplified: just detect tier based on current resources */
    platform_tier_t detected = platform_tier_detect();
    mgr->recommended_tier = detected;

    return NIMCP_SUCCESS;
}


static nimcp_error_t portia_power_monitor_update(portia_power_monitor_t* mon) {
    if (!mon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL monitor in portia_power_monitor_update");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Simplified: assume AC power for now */
    nimcp_mutex_lock(&mon->lock);
    mon->current_state = PORTIA_POWER_AC;
    mon->is_on_ac = true;
    mon->battery_level = 1.0F;
    nimcp_mutex_unlock(&mon->lock);

    return NIMCP_SUCCESS;
}


static nimcp_error_t portia_resource_tracker_update(portia_resource_tracker_t* tracker) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL tracker in portia_resource_tracker_update");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&tracker->lock);

    /* Query system resources */
    system_resources_query(&tracker->current_resources);

    /* Simple CPU/memory usage estimation */
    tracker->cpu_usage = 0.5F; /* Placeholder */
    /* Calculate memory usage from available and total */
    uint64_t total_ram = tracker->current_resources.total_ram_mb;
    uint64_t avail_ram = tracker->current_resources.available_ram_mb;
    if (total_ram > 0 && avail_ram <= total_ram) {
        uint64_t used_ram_mb = total_ram - avail_ram;
        tracker->memory_usage = (float)used_ram_mb / (float)total_ram;
    } else {
        tracker->memory_usage = 0.0F; /* Cannot compute - assume zero usage */
    }
    tracker->temperature_celsius = 50.0F; /* Placeholder */
    tracker->thermal_state = PORTIA_THERMAL_NOMINAL;

    nimcp_mutex_unlock(&tracker->lock);

    return NIMCP_SUCCESS;
}


static nimcp_error_t portia_degradation_controller_update(portia_degradation_controller_t* ctrl, const portia_status_t* status) {
    if (!ctrl || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL argument in portia_degradation_controller_update");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Simplified: maintain current degradation level */
    return NIMCP_SUCCESS;
}


static nimcp_error_t portia_sensor_fusion_update(portia_sensor_fusion_t* fusion, const portia_status_t* status) {
    if (!fusion || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL argument in portia_sensor_fusion_update");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&fusion->lock);

    /* Simplified fusion logic */
    fusion->resource_pressure = (status->cpu_usage + status->memory_usage) / 2.0F;
    fusion->overall_health = 1.0F - (fusion->resource_pressure * 0.5F);
    fusion->performance_score = 1.0F - (status->degradation_level * 0.2F);
    fusion->efficiency_score = fusion->overall_health * fusion->performance_score;

    nimcp_mutex_unlock(&fusion->lock);

    return NIMCP_SUCCESS;
}
