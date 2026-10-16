// nimcp_part_helpers.c - helpers functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c


//=============================================================================
// Error Handling
//=============================================================================

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}


/**
 * @brief Get or create brain module context for bio-async broadcasting
 *
 * P1-8 FIX: Uses nimcp_platform_once for thread-safe lazy initialization.
 * Previously had no synchronization, allowing concurrent threads to race
 * and potentially double-register the module.
 */
static bio_module_context_t get_brain_probe_module_ctx(void) {
    if (!bio_router_is_initialized()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_brain_probe_module_ctx: bio_router_is_initialized is NULL");
        return NULL;
    }
    nimcp_platform_once(&g_brain_probe_once, brain_probe_module_init_once);
    return g_brain_probe_module_ctx;
}


//=============================================================================
// Global Workspace API Implementation
//=============================================================================

/**
 * @brief Helper to convert public API module enum to internal enum
 */
static cognitive_module_t convert_module_enum(nimcp_cognitive_module_t public_module)
{
    // Direct mapping - enums have same values
    return (cognitive_module_t)public_module;
}

/**
 * @brief Get or create training state for a brain handle.
 *
 * SAFETY: Returns a pointer into the static g_training_states[] array after
 * releasing the mutex. This is safe under the following invariants:
 *   1. Each brain handle has at most ONE training state entry.
 *   2. Entries are only cleared by clear_training_state(), which is called
 *      exclusively from nimcp_brain_destroy() — after which no further
 *      access to that brain's training state occurs.
 *   3. The g_training_states[] array is statically allocated and never
 *      reallocated or moved.
 *
 * The mutex protects lookup/creation atomicity, not the returned pointer's
 * lifetime. Callers must not hold the returned pointer across brain destruction.
 */
static training_pipeline_state_t* get_training_state(nimcp_brain_t brain) {
    nimcp_mutex_lock(&g_training_states_mutex);
    // Find existing state
    for (int i = 0; i < MAX_TRAINING_STATES; i++) {
        if (g_training_states[i].brain == brain) {
            nimcp_mutex_unlock(&g_training_states_mutex);
            return &g_training_states[i].state;
        }
    }
    // Create new state
    for (int i = 0; i < MAX_TRAINING_STATES; i++) {
        if (g_training_states[i].brain == NULL) {
            g_training_states[i].brain = brain;
            memset(&g_training_states[i].state, 0, sizeof(training_pipeline_state_t));
            nimcp_mutex_unlock(&g_training_states_mutex);
            return &g_training_states[i].state;
        }
    }
    nimcp_mutex_unlock(&g_training_states_mutex);
    /* P2-3: Use correct error code for full table */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_training_state: training states table full");
    return NULL;  // No space
}


static void clear_training_state(nimcp_brain_t brain) {
    nimcp_mutex_lock(&g_training_states_mutex);
    for (int i = 0; i < MAX_TRAINING_STATES; i++) {
        if (g_training_states[i].brain == brain) {
            // Destroy callback manager if present
            if (g_training_states[i].state.callbacks) {
                tcb_destroy(g_training_states[i].state.callbacks);
                g_training_states[i].state.callbacks = NULL;
            }
            // Destroy backprop context if present
            if (g_training_states[i].state.backprop) {
                backprop_destroy(g_training_states[i].state.backprop);
                g_training_states[i].state.backprop = NULL;
            }
            // Free rubric validation features if present
            if (g_training_states[i].state.rubric_validation_features) {
                nimcp_free(g_training_states[i].state.rubric_validation_features);
                g_training_states[i].state.rubric_validation_features = NULL;
            }
            g_training_states[i].brain = NULL;
            memset(&g_training_states[i].state, 0, sizeof(training_pipeline_state_t));
            nimcp_mutex_unlock(&g_training_states_mutex);
            return;
        }
    }
    nimcp_mutex_unlock(&g_training_states_mutex);
}

static tcb_action_t callback_bridge(const tcb_event_t* event) {
    if (!event || !event->user_data) {
        return TCB_ACTION_CONTINUE;
    }

    callback_wrapper_t* wrapper = (callback_wrapper_t*)event->user_data;
    if (!wrapper->public_callback) {
        return TCB_ACTION_CONTINUE;
    }

    // Map internal event type to public event type
    nimcp_callback_event_t pub_event;
    switch (event->event_type) {
        case TCB_EVENT_STEP_COMPLETE:    pub_event = NIMCP_CB_STEP_COMPLETE; break;
        case TCB_EVENT_EPOCH_COMPLETE:   pub_event = NIMCP_CB_EPOCH_COMPLETE; break;
        case TCB_EVENT_LOSS_COMPUTED:    pub_event = NIMCP_CB_LOSS_COMPUTED; break;
        case TCB_EVENT_WEIGHTS_UPDATED:  pub_event = NIMCP_CB_WEIGHTS_UPDATED; break;
        case TCB_EVENT_LR_CHANGED:       pub_event = NIMCP_CB_LR_CHANGED; break;
        case TCB_EVENT_CONVERGENCE:      pub_event = NIMCP_CB_CONVERGENCE; break;
        case TCB_EVENT_DIVERGENCE:       pub_event = NIMCP_CB_DIVERGENCE; break;
        case TCB_EVENT_CHECKPOINT:       pub_event = NIMCP_CB_CHECKPOINT; break;
        default:                         pub_event = NIMCP_CB_STEP_COMPLETE; break;
    }

    // Convert metrics
    nimcp_callback_metrics_t pub_metrics = {
        .step = event->metrics.step,
        .epoch = event->metrics.epoch,
        .loss = event->metrics.loss,
        .loss_ema = event->metrics.loss_ema,
        .learning_rate = event->metrics.learning_rate,
        .gradient_norm = event->metrics.gradient_norm,
        .step_time_us = event->metrics.step_time_us,
        .is_converging = event->metrics.is_converging,
        .is_diverging = event->metrics.is_diverging
    };

    // Call public callback
    nimcp_callback_action_t pub_action = wrapper->public_callback(
        pub_event, &pub_metrics, wrapper->user_data);

    // Map public action to internal action
    switch (pub_action) {
        case NIMCP_CB_ACTION_CONTINUE:    return TCB_ACTION_CONTINUE;
        case NIMCP_CB_ACTION_STOP:        return TCB_ACTION_STOP_TRAINING;
        case NIMCP_CB_ACTION_SKIP:        return TCB_ACTION_SKIP_STEP;
        case NIMCP_CB_ACTION_ROLLBACK:    return TCB_ACTION_ROLLBACK;
        case NIMCP_CB_ACTION_REDUCE_LR:   return TCB_ACTION_REDUCE_LR;
        case NIMCP_CB_ACTION_INCREASE_LR: return TCB_ACTION_INCREASE_LR;
        default:                          return TCB_ACTION_CONTINUE;
    }
}


/**
 * @brief Fire callbacks for a training event (internal helper)
 *
 * @param state Training pipeline state
 * @param event_type Event type to fire
 * @param metrics Current training metrics
 * @return Action from callbacks
 */
static tcb_action_t fire_training_callback(
    training_pipeline_state_t* state,
    tcb_event_type_t event_type,
    float loss,
    float learning_rate,
    uint64_t step,
    float gradient_norm)
{
    if (!state || !state->callbacks || !state->callbacks_enabled) {
        return TCB_ACTION_CONTINUE;
    }

    // Update metrics in callback manager
    tcb_update_metrics(state->callbacks, loss, learning_rate, step, gradient_norm);

    // Fire the event
    tcb_event_t event = {
        .event_type = event_type,
        .metrics = {
            .step = step,
            .loss = loss,
            .learning_rate = learning_rate,
            .gradient_norm = gradient_norm
        },
        .user_data = NULL,
        .checkpoint_path = NULL,
        .timestamp_ns = 0
    };

    return tcb_fire(state->callbacks, &event);
}
