// nimcp_swarm_consciousness_part_accessors.c - accessors functions
// Part of nimcp_swarm_consciousness.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_consciousness.c

static void swarm_brain_set_consciousness_ctx(swarm_brain_t* swarm, swarm_consciousness_ctx_t* ctx) {
    (void)swarm;
    atomic_store(&swarm_consciousness_ctx_storage, ctx);
}

static swarm_consciousness_ctx_t* swarm_brain_get_consciousness_ctx(const swarm_brain_t* swarm) {
    (void)swarm;
    return atomic_load(&swarm_consciousness_ctx_storage);
}

swarm_consciousness_config_t swarm_consciousness_default_config(void) {
    swarm_consciousness_config_t config = {
        .phi_aggregation_method = PHI_AGGREGATION_SYNERGISTIC,
        .integration_weight = SWARM_CONSCIOUSNESS_DEFAULT_INTEGRATION_WEIGHT,
        .coherence_weight = SWARM_CONSCIOUSNESS_DEFAULT_COHERENCE_WEIGHT,
        .update_interval_ms = SWARM_CONSCIOUSNESS_DEFAULT_UPDATE_INTERVAL_MS,
        .enable_bio_async = true,
        .enable_logging = false,
        .min_drones_for_emergence = SWARM_CONSCIOUSNESS_DEFAULT_MIN_DRONES
    };
    return config;
}


/**
 * WHAT: Get current swarm consciousness metrics
 * WHY:  Access latest consciousness state
 * HOW:  Return copy of current metrics
 */
swarm_consciousness_metrics_t* swarm_consciousness_get_metrics(
    swarm_consciousness_ctx_t* context)
{
    // Guard: Null check
    if (!bbb_check_pointer(context, "swarm_consciousness_get_metrics")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_get_metrics: bbb_check_pointer is NULL");
        return NULL;
    }

    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)context;

    // Guard: Validate magic
    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid context magic in swarm_consciousness_get_metrics");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (!ctx->current_metrics) {
        nimcp_mutex_unlock(&ctx->lock);
        return NULL;
    }

    swarm_consciousness_metrics_t* result = (swarm_consciousness_metrics_t*)
        nimcp_malloc(sizeof(swarm_consciousness_metrics_t));
    if (result) {
        memcpy(result, ctx->current_metrics, sizeof(swarm_consciousness_metrics_t));
    }

    nimcp_mutex_unlock(&ctx->lock);

    return result;
}


/**
 * WHAT: Get current collective phi from swarm brain
 * WHY:  Quick check of consciousness level without full metrics
 * HOW:  Compute phi on demand or return cached value
 */
float swarm_brain_get_collective_phi(const swarm_brain_t* swarm) {
    // Guard: Null check
    if (!swarm) {
        return 0.0f;
    }

    // Get consciousness context
    swarm_consciousness_ctx_t* ctx = swarm_brain_get_consciousness_ctx(swarm);
    if (ctx && ctx->current_metrics) {
        nimcp_mutex_lock(&ctx->lock);
        float phi = ctx->current_metrics->collective_phi;
        nimcp_mutex_unlock(&ctx->lock);
        return phi;
    }

    // No monitoring context - compute on demand
    swarm_consciousness_metrics_t* metrics = swarm_compute_collective_phi(
        (swarm_brain_t*)swarm, NULL);
    if (!metrics) {
        return 0.0f;
    }

    float phi = metrics->collective_phi;
    swarm_consciousness_metrics_free(metrics);
    return phi;
}


/**
 * WHAT: Check if swarm has achieved consciousness
 * WHY:  Simple boolean check for consciousness emergence
 * HOW:  Compare current phi to threshold
 */
bool swarm_brain_is_conscious(const swarm_brain_t* swarm, float threshold) {
    // Guard: Null check
    if (!swarm) {
        return false;
    }

    // Use default threshold if not specified
    float effective_threshold = (threshold > 0.0f) ? threshold :
                               SWARM_CONSCIOUSNESS_MIN_PHI_THRESHOLD;

    float phi = swarm_brain_get_collective_phi(swarm);
    return phi >= effective_threshold;
}


//=============================================================================
// Internal Stub Implementations
//=============================================================================

/**
 * WHAT: Get number of drones in swarm
 * WHY:  Core swarm size metric for consciousness computation
 * HOW:  Use peer count + 1 (local) as drone count
 */
static uint32_t swarm_brain_get_drone_count_internal(const swarm_brain_t* swarm) {
    if (!swarm) {
        return 0;
    }

    uint32_t peer_count = 0;
    const swarm_peer_info_t* peers = swarm_brain_get_peers(swarm, &peer_count);
    (void)peers;  // We only need the count

    // Total drones = peers + self
    return peer_count + 1;
}


/**
 * WHAT: Get brain for specific drone by index
 * WHY:  Access individual phi values for collective computation
 * HOW:  Only local brain (index 0) is accessible; remote drones not directly accessible
 */
static brain_t* swarm_brain_get_drone_brain_internal(const swarm_brain_t* swarm, uint32_t index) {
    if (!swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_get_drone_brain_internal: swarm is NULL");
        return NULL;
    }

    // Only local brain is accessible
    if (index == 0) {
        // Note: swarm_brain_get_local_brain returns brain_t (value), not pointer
        // _Thread_local prevents data race on the static cache variable
        static _Thread_local brain_t local_brain_cache = NULL;
        local_brain_cache = swarm_brain_get_local_brain((swarm_brain_t*)swarm);
        return local_brain_cache ? &local_brain_cache : NULL;
    }

    // Remote drone brains not directly accessible
    // In production, would use swarm messaging to request phi values
    return NULL;
}


/**
 * WHAT: Get phi (integrated information) from brain
 * WHY:  Core IIT metric for consciousness measurement
 * HOW:  Use introspection subsystem if available, else estimate from activity
 */
static float brain_get_phi_internal(brain_t* brain) {
    if (!brain || !*brain) {
        return 0.0f;
    }

    // Try to get introspection subsystem
    introspection_context_t introspection = brain_get_introspection(*brain);
    if (introspection) {
        // Compute phi using introspection
        consciousness_phi_result_t* phi_result = introspection_compute_phi_fast(
            introspection, NULL);  // NULL config uses defaults
        if (phi_result) {
            float phi = phi_result->phi;
            // Free result
            nimcp_free(phi_result);
            return phi;
        }
    }

    // Fallback: Estimate phi from brain activity
    // Simple heuristic: use 0.5 as baseline for active brain
    return 0.5f;
}
