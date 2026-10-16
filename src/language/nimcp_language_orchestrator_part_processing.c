// nimcp_language_orchestrator_part_processing.c - processing functions
// Part of nimcp_language_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_language_orchestrator.c


bool language_orchestrator_is_running(const language_orchestrator_t* orchestrator)
{
    return orchestrator && orchestrator->running;
}


//=============================================================================
// Processing API
//=============================================================================

int language_orchestrator_process_input(
    language_orchestrator_t* orchestrator,
    const void* input,
    uint32_t input_size,
    language_input_type_t input_type)
{
    if (!orchestrator || !orchestrator->running || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_process_input: NULL or not running");
        return -1;
    }

    switch (input_type) {
        case LANGUAGE_INPUT_PHONEMES:
            return language_orchestrator_process_phonemes(
                orchestrator,
                (const language_phoneme_t*)input,
                input_size
            );

        case LANGUAGE_INPUT_TEXT:
            return language_orchestrator_process_text(
                orchestrator,
                (const char*)input
            );

        case LANGUAGE_INPUT_AUDIO:
            /* Route to perception bridge for phoneme extraction */
            if (orchestrator->perception_bridge) {
                /* Audio processing would go through speech cortex */
                /* For now, just transition to listening state */
                orchestrator_transition_state(orchestrator,
                    LANGUAGE_STATE_LISTENING,
                    orchestrator->last_update_ms);
            }
            break;

        case LANGUAGE_INPUT_SEMANTIC:
            /* Direct semantic input for production */
            if (orchestrator->mode == LANGUAGE_MODE_PRODUCTION ||
                orchestrator->mode == LANGUAGE_MODE_DIALOGUE) {
                orchestrator_transition_state(orchestrator,
                    LANGUAGE_STATE_GENERATING,
                    orchestrator->last_update_ms);
            }
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_orchestrator_process_input: operation failed");
            return -1;
    }

    return 0;
}


int language_orchestrator_process_phonemes(
    language_orchestrator_t* orchestrator,
    const language_phoneme_t* phonemes,
    uint32_t count)
{
    if (!orchestrator || !orchestrator->running || !phonemes || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_process_phonemes: Invalid parameters");
        return -1;
    }

    /* Buffer phonemes */
    uint32_t space = orchestrator->input.phoneme_capacity -
                     orchestrator->input.phoneme_count;
    uint32_t to_copy = (count < space) ? count : space;

    if (to_copy > 0) {
        memcpy(&orchestrator->input.phonemes[orchestrator->input.phoneme_count],
               phonemes,
               to_copy * sizeof(language_phoneme_t));
        orchestrator->input.phoneme_count += to_copy;
        orchestrator->stats.phonemes_processed += to_copy;
    }

    /* Transition to comprehending if not already */
    if (orchestrator->state == LANGUAGE_STATE_IDLE ||
        orchestrator->state == LANGUAGE_STATE_LISTENING) {
        orchestrator_transition_state(orchestrator,
            LANGUAGE_STATE_COMPREHENDING,
            orchestrator->last_update_ms);
    }

    return (int)to_copy;
}


int language_orchestrator_process_text(
    language_orchestrator_t* orchestrator,
    const char* text)
{
    if (!orchestrator || !orchestrator->running || !text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_process_text: Invalid parameters");
        return -1;
    }

    uint32_t len = (uint32_t)strlen(text);
    uint32_t space = orchestrator->input.text_capacity -
                     orchestrator->input.text_length - 1;  /* -1 for NUL terminator */
    uint32_t to_copy = (len < space) ? len : space;

    if (to_copy > 0) {
        memcpy(&orchestrator->input.text_buffer[orchestrator->input.text_length],
               text,
               to_copy);
        orchestrator->input.text_length += to_copy;
        orchestrator->input.text_buffer[orchestrator->input.text_length] = '\0';
    }

    /* Transition to comprehending */
    if (orchestrator->state == LANGUAGE_STATE_IDLE) {
        orchestrator_transition_state(orchestrator,
            LANGUAGE_STATE_COMPREHENDING,
            orchestrator->last_update_ms);
    }

    return 0;
}


//=============================================================================
// Update Cycle API
//=============================================================================

int language_orchestrator_update(
    language_orchestrator_t* orchestrator,
    uint64_t current_time_ms)
{
    if (!orchestrator || !orchestrator->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_update: NULL or not running");
        return -1;
    }

    orchestrator->last_update_ms = current_time_ms;
    orchestrator->stats.last_update_ms = current_time_ms;

    /* Update bridges */
    orchestrator_update_bridges(orchestrator, current_time_ms);

    /* Process state machine */
    orchestrator_process_state(orchestrator, current_time_ms);

    /* Check for state timeout */
    if (orchestrator->state != LANGUAGE_STATE_IDLE &&
        orchestrator->state != LANGUAGE_STATE_ERROR) {
        uint64_t elapsed = current_time_ms - orchestrator->state_entry_time_ms;
        if (elapsed > ORCHESTRATOR_STATE_TIMEOUT_MS) {
            /* Timeout - return to idle */
            orchestrator_transition_state(orchestrator,
                LANGUAGE_STATE_IDLE,
                current_time_ms);
        }
    }

    return 0;
}


int language_orchestrator_process_messages(language_orchestrator_t* orchestrator)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_process_messages: NULL orchestrator");
        return -1;
    }

    /* Process bio-async messages */
    int count = 0;

    /* Message processing would be handled by bio-async integration */
    orchestrator->stats.bio_async_messages += (uint64_t)count;

    return count;
}


static int orchestrator_process_state(
    language_orchestrator_t* orch,
    uint64_t current_time_ms)
{
    switch (orch->state) {
        case LANGUAGE_STATE_IDLE:
            /* Check for pending input */
            if (orch->input.phoneme_count > 0 ||
                orch->input.text_length > 0) {
                orchestrator_transition_state(orch,
                    LANGUAGE_STATE_LISTENING,
                    current_time_ms);
            }
            break;

        case LANGUAGE_STATE_LISTENING:
            /* Transition to comprehending once we have sufficient input */
            if (orch->input.phoneme_count >= 3 ||
                orch->input.text_length > 0) {
                orchestrator_transition_state(orch,
                    LANGUAGE_STATE_COMPREHENDING,
                    current_time_ms);
            }
            break;

        case LANGUAGE_STATE_COMPREHENDING:
            /* Process through Wernicke if connected */
            if (orch->wernicke) {
                /* Wernicke processing would happen here */
                /* For now, simulate completion */
            }

            /* Transition to integrating */
            orchestrator_transition_state(orch,
                LANGUAGE_STATE_INTEGRATING,
                current_time_ms);
            break;

        case LANGUAGE_STATE_INTEGRATING:
            /* Integration with cognitive systems */
            if (orch->cognitive_bridge) {
                /* Cognitive integration would happen here */
            }

            /* Check mode for next state */
            if (orch->mode == LANGUAGE_MODE_PRODUCTION ||
                orch->mode == LANGUAGE_MODE_DIALOGUE) {
                /* If production needed, go to generating */
                if (orch->production_valid ||
                    orch->current_production.semantic_input) {
                    orchestrator_transition_state(orch,
                        LANGUAGE_STATE_GENERATING,
                        current_time_ms);
                } else {
                    /* Done comprehending, return to idle */
                    orchestrator_transition_state(orch,
                        LANGUAGE_STATE_IDLE,
                        current_time_ms);

                    /* Mark comprehension complete */
                    orch->comprehension_valid = true;
                    orch->stats.utterances_comprehended++;
                }
            } else {
                /* Comprehension only - done */
                orchestrator_transition_state(orch,
                    LANGUAGE_STATE_IDLE,
                    current_time_ms);
                orch->comprehension_valid = true;
                orch->stats.utterances_comprehended++;
            }
            break;

        case LANGUAGE_STATE_GENERATING:
            /* Process through Broca if connected */
            if (orch->broca) {
                /* Broca processing would happen here */
            }

            /* Transition to producing */
            orchestrator_transition_state(orch,
                LANGUAGE_STATE_PRODUCING,
                current_time_ms);
            break;

        case LANGUAGE_STATE_PRODUCING:
            /* Motor output phase */
            /* Production completion */
            orch->production_valid = true;
            orch->stats.utterances_produced++;

            /* Clear input buffers */
            orch->input.phoneme_count = 0;
            orch->input.word_count = 0;
            orch->input.text_length = 0;

            /* Return to idle */
            orchestrator_transition_state(orch,
                LANGUAGE_STATE_IDLE,
                current_time_ms);
            break;

        case LANGUAGE_STATE_ERROR:
            /* Error recovery - return to idle after delay */
            {
                uint64_t error_duration = current_time_ms -
                                          orch->state_entry_time_ms;
                if (error_duration > 1000) {  /* 1 second recovery */
                    orchestrator_transition_state(orch,
                        LANGUAGE_STATE_IDLE,
                        current_time_ms);
                }
            }
            break;

        default:
            break;
    }

    return 0;
}


static int orchestrator_update_bridges(
    language_orchestrator_t* orch,
    uint64_t current_time_ms)
{
    /* Update all connected bridges */
    if (orch->perception_bridge) {
        language_perception_bridge_update(orch->perception_bridge,
                                          current_time_ms);
    }
    if (orch->cognitive_bridge) {
        language_cognitive_bridge_update(orch->cognitive_bridge,
                                         current_time_ms);
    }
    if (orch->training_bridge) {
        language_training_bridge_update(orch->training_bridge,
                                        current_time_ms);
    }
    if (orch->omni_bridge) {
        language_omni_bridge_update(orch->omni_bridge,
                                    current_time_ms);
    }
    if (orch->immune_bridge) {
        language_immune_bridge_update(orch->immune_bridge,
                                      current_time_ms);
    }
    if (orch->gpu_bridge) {
        language_gpu_bridge_update(orch->gpu_bridge,
                                   current_time_ms);
    }

    return 0;
}
