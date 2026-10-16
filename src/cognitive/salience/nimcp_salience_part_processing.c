// nimcp_salience_part_processing.c - processing functions
// Part of nimcp_salience.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_salience.c


/**
 * WHAT: Update prediction with new observation
 * WHY: Learn temporal patterns
 * HOW: Exponential moving average
 *
 * BUGFIX: Added num_features parameter for bounds checking
 * WHY: Prevent buffer overflow when features array is smaller than expected
 */
static void predictor_update(predictor_t* pred, const float* features, uint32_t num_features)
{
    nimcp_mutex_lock(&pred->lock);

    // BUGFIX: Use minimum of expected and actual sizes to prevent buffer overflow
    uint32_t safe_count = (num_features < pred->num_features) ? num_features : pred->num_features;

    if (!pred->initialized) {
        /**
         * WHAT: Initialize prediction with first observation
         * WHY: Need starting point for EMA
         */
        memcpy(pred->prediction, features, safe_count * sizeof(float));
        // Zero remaining elements if features array is smaller
        if (safe_count < pred->num_features) {
            memset(pred->prediction + safe_count, 0, (pred->num_features - safe_count) * sizeof(float));
        }
        pred->initialized = true;
    } else {
        /**
         * WHAT: Update prediction using EMA
         * WHY: Smooth temporal trend
         * HOW: prediction = alpha * new + (1-alpha) * old
         */
        for (uint32_t i = 0; i < safe_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && safe_count > 256) {
                salience_heartbeat("salience_loop",
                                 (float)(i + 1) / (float)safe_count);
            }

            float updated = pred->alpha * features[i] + (1.0F - pred->alpha) * pred->prediction[i];
            if (isfinite(updated)) {
                pred->prediction[i] = updated;
            }
        }
    }

    nimcp_mutex_unlock(&pred->lock);
}


/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 */
static int salience_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_SALIENCE_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_salience_query);
                registered++;
                break;
            default:
                LOG_DEBUG("Salience: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}


int salience_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_heartbeat_instance(NULL, "salience_training_step", progress);
    return 0;
}
