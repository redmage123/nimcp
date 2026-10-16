// nimcp_distributed_cognition_part_helpers.c - helpers functions
// Part of nimcp_distributed_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_distributed_cognition.c


//=============================================================================
// Worker Threads
//=============================================================================

/**
 * @brief Neuromodulator synchronization worker thread
 */
static void* neuromod_sync_worker(void* arg)
{
    distrib_cognition_t dc = (distrib_cognition_t)arg;

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Neuromodulator sync worker started");

    while (!dc->shutdown_requested) {
        if (dc->config.enable_neuromod_sync) {
            // Read lock for iteration
            nimcp_rwlock_rdlock(&dc->rwlock);
            size_t pool_count = dc->neuromod_pool_count;
            nimcp_rwlock_unlock(&dc->rwlock);

            // Sync each registered pool
            for (size_t i = 0; i < pool_count; i++) {
                nimcp_rwlock_rdlock(&dc->rwlock);
                registered_neuromod_t* reg = &dc->neuromod_pools[i];
                uint64_t now = nimcp_time_get_us();
                uint64_t elapsed_ms = (now - reg->last_sync_time) / NIMCP_US_PER_MS;
                nimcp_rwlock_unlock(&dc->rwlock);

                if (elapsed_ms >= dc->config.neuromod_broadcast_interval_ms) {
                    // Broadcast neuromodulator levels
                    // NOTE: Would query pool->concentrations here

                    nimcp_rwlock_wrlock(&dc->rwlock);
                    reg->last_sync_time = now;
                    nimcp_rwlock_unlock(&dc->rwlock);
                }
            }
        }

        // Sleep for interval
        nimcp_time_sleep_ms(dc->config.neuromod_broadcast_interval_ms);
    }

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Neuromodulator sync worker stopped");

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_sync_worker: operation failed");
    return NULL;
}


/**
 * @brief Glial coordination worker thread
 */
static void* glial_sync_worker(void* arg)
{
    distrib_cognition_t dc = (distrib_cognition_t)arg;

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Glial sync worker started");

    while (!dc->shutdown_requested) {
        if (dc->config.enable_glial_sync) {
            nimcp_rwlock_rdlock(&dc->rwlock);
            size_t system_count = dc->glial_system_count;
            nimcp_rwlock_unlock(&dc->rwlock);

            // Sync each registered glial system
            for (size_t i = 0; i < system_count; i++) {
                nimcp_rwlock_rdlock(&dc->rwlock);
                registered_glial_t* reg = &dc->glial_systems[i];
                uint64_t now = nimcp_time_get_us();
                uint64_t elapsed_ms = (now - reg->last_sync_time) / NIMCP_US_PER_MS;
                nimcp_rwlock_unlock(&dc->rwlock);

                if (elapsed_ms >= dc->config.glial_sync_interval_ms) {
                    // Coordinate glial activities
                    // NOTE: Would query glial system state here

                    nimcp_rwlock_wrlock(&dc->rwlock);
                    reg->last_sync_time = now;
                    nimcp_rwlock_unlock(&dc->rwlock);
                }
            }
        }

        // Sleep for interval
        nimcp_time_sleep_ms(dc->config.glial_sync_interval_ms);
    }

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Glial sync worker stopped");

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_sync_worker: operation failed");
    return NULL;
}


/**
 * @brief Brain region synchronization worker thread
 */
static void* region_sync_worker(void* arg)
{
    distrib_cognition_t dc = (distrib_cognition_t)arg;

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Region sync worker started");

    while (!dc->shutdown_requested) {
        if (dc->config.enable_region_sync) {
            nimcp_rwlock_rdlock(&dc->rwlock);
            size_t region_count = dc->brain_region_count;
            nimcp_rwlock_unlock(&dc->rwlock);

            // Sync each registered region
            for (size_t i = 0; i < region_count; i++) {
                nimcp_rwlock_rdlock(&dc->rwlock);
                registered_region_t* reg = &dc->brain_regions[i];
                uint64_t now = nimcp_time_get_us();
                uint64_t elapsed_ms = (now - reg->last_sync_time) / NIMCP_US_PER_MS;
                nimcp_rwlock_unlock(&dc->rwlock);

                if (elapsed_ms >= dc->config.region_sync_interval_ms) {
                    // Broadcast region activity
                    // NOTE: Would query region statistics here

                    nimcp_rwlock_wrlock(&dc->rwlock);
                    reg->last_sync_time = now;
                    nimcp_rwlock_unlock(&dc->rwlock);
                }
            }
        }

        // Sleep for interval
        nimcp_time_sleep_ms(dc->config.region_sync_interval_ms);
    }

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Region sync worker stopped");

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "region_sync_worker: operation failed");
    return NULL;
}


//=============================================================================
// Message Handlers (Incoming Network Messages)
//=============================================================================

static void handle_neuromod_level_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len)
{
    if (payload_len != sizeof(neuromod_level_payload_t)) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid neuromod level payload size: %zu", payload_len);
        return;
    }

    const neuromod_level_payload_t* msg = (const neuromod_level_payload_t*)payload;

    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.neuromod_updates_received++;
    dc->stats.messages_received++;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Received neuromod level: type=%d concentration=%.3f",
              msg->neuromod_type, msg->concentration);

    // Apply diffusion to local pools
    nimcp_rwlock_rdlock(&dc->rwlock);
    for (size_t i = 0; i < dc->neuromod_pool_count; i++) {
        registered_neuromod_t* reg = &dc->neuromod_pools[i];
        // NOTE: Would apply diffusion: local = local * (1-rate) + remote * rate
        // pool_update_concentration(reg->pool, msg->neuromod_type,
        //                           msg->concentration, dc->config.neuromod_diffusion_rate);
    }
    nimcp_rwlock_unlock(&dc->rwlock);
}


static void handle_glial_pruning_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len)
{
    if (payload_len != sizeof(glial_pruning_payload_t)) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid glial pruning payload size: %zu", payload_len);
        return;
    }

    const glial_pruning_payload_t* msg = (const glial_pruning_payload_t*)payload;

    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.glial_pruning_coordinations++;
    dc->stats.messages_received++;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Received pruning coordination: synapse=%u->%u action=%d",
              msg->source_neuron_id, msg->target_neuron_id, msg->pruning_action);
}


static void handle_glial_calcium_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len)
{
    if (payload_len != sizeof(glial_calcium_payload_t)) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid glial calcium payload size: %zu", payload_len);
        return;
    }

    const glial_calcium_payload_t* msg = (const glial_calcium_payload_t*)payload;

    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.glial_calcium_propagations++;
    dc->stats.messages_received++;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Received calcium wave: astrocyte=%u level=%.3f",
              msg->astrocyte_id, msg->calcium_level);
}


static void handle_region_activity_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len)
{
    if (payload_len != sizeof(region_activity_payload_t)) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid region activity payload size: %zu", payload_len);
        return;
    }

    const region_activity_payload_t* msg = (const region_activity_payload_t*)payload;

    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.region_state_syncs++;
    dc->stats.messages_received++;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Received region activity: type=%u activity=%.3f rate=%.1fHz",
              msg->region_type, msg->avg_activity, msg->spike_rate);
}

static void* async_neuromod_broadcast_worker(void* arg)
{
    async_neuromod_broadcast_ctx_t* ctx = (async_neuromod_broadcast_ctx_t*)arg;

    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");


        return NULL;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Async neuromod broadcast starting: type=%d concentration=%.3f",
                     ctx->type, ctx->concentration);

    // Perform the synchronous broadcast operation
    bool result = distrib_cognition_broadcast_neuromod(
        ctx->dc,
        ctx->type,
        ctx->concentration
    );

    // Set promise result using correct API
    if (result) {
        bool success = true;
        nimcp_promise_complete(ctx->promise, &success);
        LOG_MODULE_DEBUG(MODULE_NAME, "Async neuromod broadcast completed successfully");
    } else {
        nimcp_promise_fail(ctx->promise, NIMCP_ERROR_OPERATION_FAILED);
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast failed");
    }

    // Free context
    nimcp_free(ctx);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_neuromod_broadcast_worker: operation failed");
    return NULL;
}


/**
 * @brief Worker thread for async calcium wave propagation
 */
static void* async_calcium_wave_worker(void* arg)
{
    async_calcium_wave_ctx_t* ctx = (async_calcium_wave_ctx_t*)arg;

    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");


        return NULL;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Async calcium wave starting: astrocyte=%u level=%.3f velocity=%.1f",
                     ctx->astrocyte_id, ctx->calcium_level, ctx->wave_velocity);

    // Perform the synchronous calcium wave operation
    bool result = distrib_cognition_propagate_calcium_wave(
        ctx->dc,
        ctx->astrocyte_id,
        ctx->calcium_level,
        ctx->wave_velocity
    );

    // Set promise result using correct API
    if (result) {
        bool success = true;
        nimcp_promise_complete(ctx->promise, &success);
        LOG_MODULE_DEBUG(MODULE_NAME, "Async calcium wave completed successfully");
    } else {
        nimcp_promise_fail(ctx->promise, NIMCP_ERROR_OPERATION_FAILED);
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave failed");
    }

    // Free context
    nimcp_free(ctx);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_calcium_wave_worker: operation failed");
    return NULL;
}


/**
 * @brief Worker thread for async pruning coordination
 */
static void* async_pruning_worker(void* arg)
{
    async_pruning_ctx_t* ctx = (async_pruning_ctx_t*)arg;

    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");


        return NULL;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Async pruning coordination starting: synapse=%u->%u score=%.3f action=%d",
                     ctx->source_neuron_id, ctx->target_neuron_id, ctx->activity_score, ctx->action);

    // Perform the synchronous pruning coordination
    bool result = distrib_cognition_coordinate_pruning(
        ctx->dc,
        ctx->source_neuron_id,
        ctx->target_neuron_id,
        ctx->activity_score,
        ctx->action
    );

    // Set promise result using correct API
    if (result) {
        bool success = true;
        nimcp_promise_complete(ctx->promise, &success);
        LOG_MODULE_DEBUG(MODULE_NAME, "Async pruning coordination completed successfully");
    } else {
        nimcp_promise_fail(ctx->promise, NIMCP_ERROR_OPERATION_FAILED);
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination failed");
    }

    // Free context
    nimcp_free(ctx);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_pruning_worker: operation failed");
    return NULL;
}
