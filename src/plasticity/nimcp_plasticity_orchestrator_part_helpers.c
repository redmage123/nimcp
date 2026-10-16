// nimcp_plasticity_orchestrator_part_helpers.c - helpers functions
// Part of nimcp_plasticity_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_plasticity_orchestrator.c


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void emit_event(
    plasticity_orchestrator_t* orchestrator,
    plasticity_event_type_t type,
    uint32_t synapse_id,
    uint32_t neuron_id,
    float old_val,
    float new_val
) {
    if (!orchestrator) return;

    plasticity_event_t event = {
        .type = type,
        .synapse_id = synapse_id,
        .neuron_id = neuron_id,
        .old_value = old_val,
        .new_value = new_val,
        .delta = new_val - old_val,
        .timestamp_ms = orchestrator->current_time_ms,
        .context = NULL
    };

    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (orchestrator->event_callbacks[i].active &&
            orchestrator->event_callbacks[i].event_type == type) {
            orchestrator->event_callbacks[i].callback(
                &event,
                orchestrator->event_callbacks[i].user_data
            );
        }
    }

    /* Update stats */
    switch (type) {
        case PLASTICITY_EVENT_LTP:
            orchestrator->stats.ltp_count++;
            break;
        case PLASTICITY_EVENT_LTD:
            orchestrator->stats.ltd_count++;
            break;
        case PLASTICITY_EVENT_SPINE_FORMED:
            orchestrator->stats.spines_formed++;
            break;
        case PLASTICITY_EVENT_SPINE_ELIMINATED:
            orchestrator->stats.spines_eliminated++;
            break;
        case PLASTICITY_EVENT_CONSOLIDATION:
            orchestrator->stats.consolidation_events++;
            break;
        default:
            break;
    }
}


static synapse_entry_t* find_synapse(plasticity_orchestrator_t* orch, uint32_t id) {
    if (!orch || !orch->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_synapse: required parameter is NULL (orch, orch->synapses)");
        return NULL;
    }

    for (size_t i = 0; i < orch->num_synapses; i++) {
        if (orch->synapses[i].id == id && orch->synapses[i].active) {
            return &orch->synapses[i];
        }
    }
    return NULL;  /* Not found is a normal condition (e.g., sparse iteration) */
}


static neuron_entry_t* find_neuron(plasticity_orchestrator_t* orch, uint32_t id) {
    if (!orch || !orch->neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_neuron: required parameter is NULL (orch, orch->neurons)");
        return NULL;
    }

    for (size_t i = 0; i < orch->num_neurons; i++) {
        if (orch->neurons[i].id == id && orch->neurons[i].active) {
            return &orch->neurons[i];
        }
    }
    return NULL;  /* Not found is a normal condition (e.g., sparse iteration) */
}


static float get_sleep_modulation(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return 1.0f;
        case SLEEP_STATE_DROWSY:
            return 0.9f;
        case SLEEP_STATE_LIGHT_NREM:
            return 0.7f;
        case SLEEP_STATE_DEEP_NREM:
            return 0.3f;  /* Reduced learning during deep sleep */
        case SLEEP_STATE_REM:
            return 0.5f;  /* Memory consolidation mode */
        default:
            return 1.0f;
    }
}


/**
 * @brief Get immune modulation factor based on inflammation level
 *
 * WHAT: Converts inflammation level to plasticity modulation factor
 * WHY:  Higher inflammation reduces synaptic plasticity (fever model)
 * HOW:  Maps inflammation levels to [0.1, 1.0] range
 */
static float get_immune_modulation(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:
            return 1.0f;   /* Full plasticity */
        case INFLAMMATION_LOCAL:
            return 0.85f;  /* Slight reduction */
        case INFLAMMATION_REGIONAL:
            return 0.65f;  /* Moderate reduction */
        case INFLAMMATION_SYSTEMIC:
            return 0.40f;  /* Severe reduction */
        case INFLAMMATION_STORM:
            return 0.10f;  /* Emergency - minimal plasticity */
        default:
            return 1.0f;
    }
}


static void update_weight_statistics(plasticity_orchestrator_t* orch) {
    if (!orch || orch->num_synapses == 0) return;

    float sum = 0.0f;
    float sum_sq = 0.0f;
    float min_w = 1.0f;
    float max_w = 0.0f;
    size_t active_count = 0;

    for (size_t i = 0; i < orch->num_synapses; i++) {
        if (!orch->synapses[i].active) continue;
        float w = orch->synapses[i].weight;
        if (w == 0.0f) continue;  /* Skip pruned (zero-weight) synapses */
        sum += w;
        sum_sq += w * w;
        if (w < min_w) min_w = w;
        if (w > max_w) max_w = w;
        active_count++;
    }

    if (active_count == 0) {
        orch->stats.mean_weight = 0.0f;
        orch->stats.std_weight = 0.0f;
        orch->stats.min_weight = 0.0f;
        orch->stats.max_weight = 0.0f;
        return;
    }

    float n = (float)active_count;
    orch->stats.mean_weight = sum / n;
    float variance = (sum_sq / n) - (orch->stats.mean_weight * orch->stats.mean_weight);
    if (variance < 0.0f) variance = 0.0f;  /* Clamp to prevent negative from FP error */
    orch->stats.std_weight = sqrtf(variance);
    orch->stats.min_weight = min_w;
    orch->stats.max_weight = max_w;
}
