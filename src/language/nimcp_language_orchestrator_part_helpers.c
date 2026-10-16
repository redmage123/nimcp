// nimcp_language_orchestrator_part_helpers.c - helpers functions
// Part of nimcp_language_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_language_orchestrator.c


static int orchestrator_transition_state(
    language_orchestrator_t* orch,
    language_state_t new_state,
    uint64_t current_time_ms)
{
    if (orch->state == new_state) {
        return 0;  /* No transition needed */
    }

    /* Store previous state */
    orch->prev_state = orch->state;
    orch->state = new_state;
    orch->state_entry_time_ms = current_time_ms;

    /* Update stats */
    orch->stats.current_state = new_state;
    orch->stats.state_entry_time_ms = current_time_ms;
    orch->stats.state_transitions++;

    /* Emit state change event */
    language_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = LANGUAGE_EVENT_STATE_CHANGE;
    event.timestamp_ms = current_time_ms;
    event.source_module = BIO_MODULE_LANGUAGE_LAYER;
    event.data.state_change.old_state = orch->prev_state;
    event.data.state_change.new_state = new_state;

    orchestrator_emit_event(orch, &event);

    return 0;
}


static void orchestrator_emit_event(
    language_orchestrator_t* orch,
    const language_event_t* event)
{
    for (uint32_t i = 0; i < ORCHESTRATOR_MAX_CALLBACKS; i++) {
        if (orch->callbacks[i].active && orch->callbacks[i].callback) {
            orch->callbacks[i].callback(event, orch->callbacks[i].user_data);
        }
    }
}
