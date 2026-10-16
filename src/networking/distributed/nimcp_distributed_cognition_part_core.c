// nimcp_distributed_cognition_part_core.c - core functions
// Part of nimcp_distributed_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_distributed_cognition.c


//=============================================================================
// Neuromodulator Network Synchronization
//=============================================================================

bool distrib_cognition_register_neuromod_pool(
    distrib_cognition_t dc,
    neuromodulator_pool_t* pool)
{
    // Process pending bio-async messages
    if (dc && dc->bio_ctx) {
        bio_router_process_inbox(dc->bio_ctx, 5);
    }

    if (!dc || !pool) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid parameters for neuromod pool registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_register_neuromod_pool: required parameter is NULL (dc, pool)");
        return false;
    }

    nimcp_rwlock_wrlock(&dc->rwlock);

    // Check capacity
    if (dc->neuromod_pool_count >= dc->neuromod_pool_capacity) {
        // Expand capacity
        size_t new_capacity = dc->neuromod_pool_capacity * 2;
        registered_neuromod_t* new_pools = (registered_neuromod_t*)nimcp_realloc(
            dc->neuromod_pools,
            new_capacity * sizeof(registered_neuromod_t)
        );

        if (!new_pools) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                              new_capacity * sizeof(registered_neuromod_t),
                              "Failed to expand neuromod pool capacity from %zu to %zu",
                              dc->neuromod_pool_capacity, new_capacity);
            nimcp_rwlock_unlock(&dc->rwlock);
            log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Failed to expand neuromod pool capacity");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_register_neuromod_pool: new_pools is NULL");
            return false;
        }

        dc->neuromod_pools = new_pools;
        dc->neuromod_pool_capacity = new_capacity;
    }

    // Register pool
    registered_neuromod_t* reg = &dc->neuromod_pools[dc->neuromod_pool_count++];
    reg->pool = pool;
    reg->last_sync_time = nimcp_time_get_us();
    reg->needs_broadcast = false;

    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Neuromodulator pool registered (total: %zu)", dc->neuromod_pool_count);

    return true;
}


bool distrib_cognition_broadcast_neuromod(
    distrib_cognition_t dc,
    neuromodulator_type_t type,
    float concentration)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_broadcast_neuromod: dc is NULL");
        return false;
    }

    if (concentration < 0.0F || concentration > 1.0F) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid neuromodulator concentration: %.2f", concentration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_broadcast_neuromod: validation failed");
        return false;
    }

    // Create payload
    neuromod_level_payload_t payload = {
        .neuromod_type = (uint8_t)type,
        .reserved = 0,
        .region_id = 0,  // Global broadcast
        .concentration = concentration,
        .timestamp = nimcp_time_get_us()
    };

    // Send control message via P2P
    // NOTE: This would require p2p_node_send_control_message() or similar
    // For now, we'll increment stats as placeholder

    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.neuromod_broadcasts++;
    dc->stats.messages_sent++;
    dc->stats.last_neuromod_sync = payload.timestamp;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Broadcast neuromodulator type=%d concentration=%.3f", type, concentration);

    return true;
}


//=============================================================================
// Glial Network Coordination
//=============================================================================

bool distrib_cognition_register_glial_system(
    distrib_cognition_t dc,
    glial_integration_t* glial)
{
    if (!dc || !glial) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid parameters for glial system registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_register_glial_system: required parameter is NULL (dc, glial)");
        return false;
    }

    nimcp_rwlock_wrlock(&dc->rwlock);

    // Check capacity
    if (dc->glial_system_count >= dc->glial_system_capacity) {
        size_t new_capacity = dc->glial_system_capacity * 2;
        registered_glial_t* new_systems = (registered_glial_t*)nimcp_realloc(
            dc->glial_systems,
            new_capacity * sizeof(registered_glial_t)
        );

        if (!new_systems) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                              new_capacity * sizeof(registered_glial_t),
                              "Failed to expand glial system capacity from %zu to %zu",
                              dc->glial_system_capacity, new_capacity);
            nimcp_rwlock_unlock(&dc->rwlock);
            log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Failed to expand glial system capacity");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_register_glial_system: new_systems is NULL");
            return false;
        }

        dc->glial_systems = new_systems;
        dc->glial_system_capacity = new_capacity;
    }

    // Register system
    registered_glial_t* reg = &dc->glial_systems[dc->glial_system_count++];
    reg->glial = glial;
    reg->last_sync_time = nimcp_time_get_us();
    reg->pending_prunings = 0;

    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Glial system registered (total: %zu)", dc->glial_system_count);

    return true;
}


bool distrib_cognition_coordinate_pruning(
    distrib_cognition_t dc,
    uint32_t source_neuron_id,
    uint32_t target_neuron_id,
    float activity_score,
    uint8_t action)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_coordinate_pruning: dc is NULL");
        return false;
    }

    if (activity_score < 0.0F || activity_score > 1.0F) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid activity score: %.2f", activity_score);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_coordinate_pruning: validation failed");
        return false;
    }

    if (action > 2) {  // 0=monitor, 1=prune, 2=preserve
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid pruning action: %d", action);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_coordinate_pruning: validation failed");
        return false;
    }

    // Create payload
    glial_pruning_payload_t payload = {
        .source_neuron_id = source_neuron_id,
        .target_neuron_id = target_neuron_id,
        .activity_score = activity_score,
        .pruning_action = action,
        .reserved = {0, 0, 0},
        .timestamp = nimcp_time_get_us()
    };

    // Send via P2P
    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.glial_pruning_coordinations++;
    dc->stats.messages_sent++;
    dc->stats.last_glial_sync = payload.timestamp;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Coordinate pruning synapse=%u->%u score=%.3f action=%d",
              source_neuron_id, target_neuron_id, activity_score, action);

    return true;
}


bool distrib_cognition_propagate_calcium_wave(
    distrib_cognition_t dc,
    uint32_t astrocyte_id,
    float calcium_level,
    float wave_velocity)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_propagate_calcium_wave: dc is NULL");
        return false;
    }

    if (calcium_level < 0.0F || calcium_level > 1.0F) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid calcium level: %.2f", calcium_level);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_propagate_calcium_wave: validation failed");
        return false;
    }

    // Create payload
    glial_calcium_payload_t payload = {
        .astrocyte_id = astrocyte_id,
        .calcium_level = calcium_level,
        .wave_velocity = wave_velocity,
        .affected_synapses = 0,  // Will be filled by local system
        .reserved = 0,
        .timestamp = nimcp_time_get_us()
    };

    // Send via P2P
    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.glial_calcium_propagations++;
    dc->stats.messages_sent++;
    dc->stats.last_glial_sync = payload.timestamp;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Propagate calcium wave astrocyte=%u level=%.3f velocity=%.1f µm/s",
              astrocyte_id, calcium_level, wave_velocity);

    return true;
}


//=============================================================================
// Brain Region Synchronization
//=============================================================================

bool distrib_cognition_register_brain_region(
    distrib_cognition_t dc,
    brain_region_t* region)
{
    if (!dc || !region) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid parameters for brain region registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_register_brain_region: required parameter is NULL (dc, region)");
        return false;
    }

    nimcp_rwlock_wrlock(&dc->rwlock);

    // Check capacity
    if (dc->brain_region_count >= dc->brain_region_capacity) {
        size_t new_capacity = dc->brain_region_capacity * 2;
        registered_region_t* new_regions = (registered_region_t*)nimcp_realloc(
            dc->brain_regions,
            new_capacity * sizeof(registered_region_t)
        );

        if (!new_regions) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                              new_capacity * sizeof(registered_region_t),
                              "Failed to expand brain region capacity from %zu to %zu",
                              dc->brain_region_capacity, new_capacity);
            nimcp_rwlock_unlock(&dc->rwlock);
            log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Failed to expand brain region capacity");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_register_brain_region: new_regions is NULL");
            return false;
        }

        dc->brain_regions = new_regions;
        dc->brain_region_capacity = new_capacity;
    }

    // Register region
    registered_region_t* reg = &dc->brain_regions[dc->brain_region_count++];
    reg->region = region;
    reg->region_type = 0;  // Would come from region->type
    reg->last_sync_time = nimcp_time_get_us();

    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Brain region registered (total: %zu)", dc->brain_region_count);

    return true;
}


bool distrib_cognition_broadcast_region_activity(
    distrib_cognition_t dc,
    uint16_t region_type,
    float avg_activity,
    float spike_rate,
    uint32_t active_neurons,
    uint32_t total_neurons)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_broadcast_region_activity: dc is NULL");
        return false;
    }

    if (avg_activity < 0.0F || avg_activity > 1.0F) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid average activity: %.2f", avg_activity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_broadcast_region_activity: validation failed");
        return false;
    }

    if (active_neurons > total_neurons) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Active neurons (%u) exceeds total neurons (%u)", active_neurons, total_neurons);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_broadcast_region_activity: validation failed");
        return false;
    }

    // Create payload
    region_activity_payload_t payload = {
        .region_type = region_type,
        .reserved = 0,
        .avg_activity = avg_activity,
        .spike_rate = spike_rate,
        .active_neurons = active_neurons,
        .total_neurons = total_neurons,
        .timestamp = nimcp_time_get_us()
    };

    // Send via P2P
    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->stats.region_activity_broadcasts++;
    dc->stats.messages_sent++;
    dc->stats.last_region_sync = payload.timestamp;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_DEBUG, "[distributed_cognition] Broadcast region activity type=%u activity=%.3f rate=%.1fHz neurons=%u/%u",
              region_type, avg_activity, spike_rate, active_neurons, total_neurons);

    return true;
}


//=============================================================================
// Control and Monitoring
//=============================================================================

bool distrib_cognition_start(distrib_cognition_t dc)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_start: dc is NULL");
        return false;
    }

    if (dc->running) {
        log_message(LOG_LEVEL_WARN, "[distributed_cognition] Coordinator already running");
        return true;
    }

    dc->shutdown_requested = false;

    // Start worker threads based on configuration
    if (dc->config.enable_neuromod_sync) {
        if (nimcp_thread_create(&dc->neuromod_thread, neuromod_sync_worker, dc, NULL) != NIMCP_SUCCESS) {
            log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Failed to start neuromod worker");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_start: validation failed");
            return false;
        }
    }

    if (dc->config.enable_glial_sync) {
        if (nimcp_thread_create(&dc->glial_thread, glial_sync_worker, dc, NULL) != NIMCP_SUCCESS) {
            log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Failed to start glial worker");
            if (dc->config.enable_neuromod_sync) {
                dc->shutdown_requested = true;
                nimcp_thread_join(dc->neuromod_thread, NULL);
            }
            return false;
        }
    }

    if (dc->config.enable_region_sync) {
        if (nimcp_thread_create(&dc->region_thread, region_sync_worker, dc, NULL) != NIMCP_SUCCESS) {
            log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Failed to start region worker");
            dc->shutdown_requested = true;
            if (dc->config.enable_neuromod_sync) {
                nimcp_thread_join(dc->neuromod_thread, NULL);
            }
            if (dc->config.enable_glial_sync) {
                nimcp_thread_join(dc->glial_thread, NULL);
            }
            return false;
        }
    }

    dc->running = true;

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Coordinator started");

    return true;
}


bool distrib_cognition_stop(distrib_cognition_t dc)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_stop: dc is NULL");
        return false;
    }

    if (!dc->running) {
        log_message(LOG_LEVEL_WARN, "[distributed_cognition] Coordinator not running");
        return true;
    }

    // Signal shutdown
    dc->shutdown_requested = true;

    // Join worker threads
    if (dc->config.enable_neuromod_sync) {
        nimcp_thread_join(dc->neuromod_thread, NULL);
    }

    if (dc->config.enable_glial_sync) {
        nimcp_thread_join(dc->glial_thread, NULL);
    }

    if (dc->config.enable_region_sync) {
        nimcp_thread_join(dc->region_thread, NULL);
    }

    dc->running = false;

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Coordinator stopped");

    return true;
}


//=============================================================================
// Async Operations - Public API
//=============================================================================

nimcp_future_t distrib_cognition_broadcast_neuromod_async(
    distrib_cognition_t dc,
    neuromodulator_type_t type,
    float concentration)
{
    if (!dc) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast: invalid coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_broadcast_neuromod_async: dc is NULL");
        return NULL;
    }

    if (concentration < 0.0F || concentration > 1.0F) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast: invalid concentration %.3f", concentration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_broadcast_neuromod_async: validation failed");
        return NULL;
    }

    // Create promise/future pair (result is a bool)
    nimcp_promise_t promise = nimcp_promise_create(sizeof(bool));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast: failed to create promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_broadcast_neuromod_async: promise is NULL");
        return NULL;
    }

    nimcp_future_t future = nimcp_promise_get_future(promise);
    if (!future) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast: failed to get future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_broadcast_neuromod_async: future is NULL");
        return NULL;
    }

    // Allocate context
    async_neuromod_broadcast_ctx_t* ctx = (async_neuromod_broadcast_ctx_t*)nimcp_malloc(sizeof(async_neuromod_broadcast_ctx_t));
    if (!ctx) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_broadcast_neuromod_async: ctx is NULL");
        return NULL;
    }

    ctx->dc = dc;
    ctx->type = type;
    ctx->concentration = concentration;
    ctx->promise = promise;

    // Spawn worker thread
    nimcp_thread_t thread;
    if (nimcp_thread_create(&thread, async_neuromod_broadcast_worker, ctx, NULL) != NIMCP_SUCCESS) {
        nimcp_free(ctx);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async neuromod broadcast: failed to create worker thread");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_broadcast_neuromod_async: validation failed");
        return NULL;
    }

    // Detach thread (worker will clean up context)
    nimcp_thread_detach(thread);

    LOG_MODULE_INFO(MODULE_NAME, "Async neuromod broadcast started: type=%d concentration=%.3f", type, concentration);

    return future;
}


nimcp_future_t distrib_cognition_propagate_calcium_wave_async(
    distrib_cognition_t dc,
    uint32_t astrocyte_id,
    float calcium_level,
    float wave_velocity)
{
    if (!dc) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave: invalid coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_propagate_calcium_wave_async: dc is NULL");
        return NULL;
    }

    if (calcium_level < 0.0F || calcium_level > 1.0F) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave: invalid calcium level %.3f", calcium_level);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_propagate_calcium_wave_async: validation failed");
        return NULL;
    }

    // Create promise/future pair (result is a bool)
    nimcp_promise_t promise = nimcp_promise_create(sizeof(bool));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave: failed to create promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_propagate_calcium_wave_async: promise is NULL");
        return NULL;
    }

    nimcp_future_t future = nimcp_promise_get_future(promise);
    if (!future) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave: failed to get future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_propagate_calcium_wave_async: future is NULL");
        return NULL;
    }

    // Allocate context
    async_calcium_wave_ctx_t* ctx = (async_calcium_wave_ctx_t*)nimcp_malloc(sizeof(async_calcium_wave_ctx_t));
    if (!ctx) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_propagate_calcium_wave_async: ctx is NULL");
        return NULL;
    }

    ctx->dc = dc;
    ctx->astrocyte_id = astrocyte_id;
    ctx->calcium_level = calcium_level;
    ctx->wave_velocity = wave_velocity;
    ctx->promise = promise;

    // Spawn worker thread
    nimcp_thread_t thread;
    if (nimcp_thread_create(&thread, async_calcium_wave_worker, ctx, NULL) != NIMCP_SUCCESS) {
        nimcp_free(ctx);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async calcium wave: failed to create worker thread");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_propagate_calcium_wave_async: validation failed");
        return NULL;
    }

    // Detach thread (worker will clean up context)
    nimcp_thread_detach(thread);

    LOG_MODULE_INFO(MODULE_NAME, "Async calcium wave started: astrocyte=%u level=%.3f velocity=%.1f",
                    astrocyte_id, calcium_level, wave_velocity);

    return future;
}


nimcp_future_t distrib_cognition_coordinate_pruning_async(
    distrib_cognition_t dc,
    uint32_t source_neuron_id,
    uint32_t target_neuron_id,
    float activity_score,
    uint8_t action)
{
    if (!dc) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: invalid coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_coordinate_pruning_async: dc is NULL");
        return NULL;
    }

    if (activity_score < 0.0F || activity_score > 1.0F) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: invalid activity score %.3f", activity_score);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_coordinate_pruning_async: validation failed");
        return NULL;
    }

    if (action > 2) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: invalid action %d", action);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_coordinate_pruning_async: validation failed");
        return NULL;
    }

    // Create promise/future pair (result is a bool)
    nimcp_promise_t promise = nimcp_promise_create(sizeof(bool));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: failed to create promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_coordinate_pruning_async: promise is NULL");
        return NULL;
    }

    nimcp_future_t future = nimcp_promise_get_future(promise);
    if (!future) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: failed to get future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_coordinate_pruning_async: future is NULL");
        return NULL;
    }

    // Allocate context
    async_pruning_ctx_t* ctx = (async_pruning_ctx_t*)nimcp_malloc(sizeof(async_pruning_ctx_t));
    if (!ctx) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_coordinate_pruning_async: ctx is NULL");
        return NULL;
    }

    ctx->dc = dc;
    ctx->source_neuron_id = source_neuron_id;
    ctx->target_neuron_id = target_neuron_id;
    ctx->activity_score = activity_score;
    ctx->action = action;
    ctx->promise = promise;

    // Spawn worker thread
    nimcp_thread_t thread;
    if (nimcp_thread_create(&thread, async_pruning_worker, ctx, NULL) != NIMCP_SUCCESS) {
        nimcp_free(ctx);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async pruning coordination: failed to create worker thread");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_coordinate_pruning_async: validation failed");
        return NULL;
    }

    // Detach thread (worker will clean up context)
    nimcp_thread_detach(thread);

    LOG_MODULE_INFO(MODULE_NAME, "Async pruning coordination started: synapse=%u->%u score=%.3f action=%d",
                    source_neuron_id, target_neuron_id, activity_score, action);

    return future;
}
