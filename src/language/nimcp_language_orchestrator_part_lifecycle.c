// nimcp_language_orchestrator_part_lifecycle.c - lifecycle functions
// Part of nimcp_language_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_language_orchestrator.c


//=============================================================================
// Lifecycle API
//=============================================================================

language_orchestrator_t* language_orchestrator_create(
    const language_orchestrator_config_t* config)
{
    language_orchestrator_t* orch = nimcp_calloc(1, sizeof(language_orchestrator_t));
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_orchestrator_create: Failed to allocate orchestrator");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        orch->config = *config;
    } else {
        language_orchestrator_default_config(&orch->config);
    }

    /* Initialize state machine */
    orch->state = LANGUAGE_STATE_IDLE;
    orch->prev_state = LANGUAGE_STATE_IDLE;
    orch->mode = orch->config.default_mode;
    orch->state_entry_time_ms = 0;
    orch->last_update_ms = 0;

    /* Initialize buffers */
    if (orchestrator_init_buffers(orch) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_orchestrator_create: Failed to initialize buffers");
        nimcp_free(orch);
        return NULL;
    }

    /* Clear results */
    memset(&orch->current_comprehension, 0, sizeof(orch->current_comprehension));
    orch->comprehension_valid = false;

    memset(&orch->current_production, 0, sizeof(orch->current_production));
    orch->production_valid = false;

    /* Clear callbacks */
    memset(orch->callbacks, 0, sizeof(orch->callbacks));
    orch->num_callbacks = 0;

    /* Initialize statistics */
    memset(&orch->stats, 0, sizeof(orch->stats));
    orch->stats.current_state = LANGUAGE_STATE_IDLE;
    orch->stats.current_mode = orch->mode;

    orch->initialized = true;
    orch->running = false;
    orch->bio_async_registered = false;

    return orch;
}


void language_orchestrator_destroy(language_orchestrator_t* orchestrator)
{
    if (!orchestrator) return;

    /* Stop if running */
    if (orchestrator->running) {
        language_orchestrator_stop(orchestrator);
    }

    /* Free buffers */
    orchestrator_free_buffers(orchestrator);

    /* Free result resources */
    language_comprehension_result_free(&orchestrator->current_comprehension);
    language_production_plan_free(&orchestrator->current_production);

    nimcp_free(orchestrator);
}


int language_orchestrator_reset(language_orchestrator_t* orchestrator)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_reset: NULL orchestrator");
        return -1;
    }

    /* Clear buffers */
    orchestrator->input.phoneme_count = 0;
    orchestrator->input.word_count = 0;
    orchestrator->input.text_length = 0;

    /* Clear results */
    orchestrator->comprehension_valid = false;
    orchestrator->production_valid = false;

    /* Return to idle */
    orchestrator->state = LANGUAGE_STATE_IDLE;
    orchestrator->stats.current_state = LANGUAGE_STATE_IDLE;

    return 0;
}


void language_orchestrator_reset_stats(language_orchestrator_t* orchestrator)
{
    if (!orchestrator) return;

    /* Preserve connection status */
    bool wernicke = orchestrator->stats.wernicke_connected;
    bool broca = orchestrator->stats.broca_connected;
    bool nlp = orchestrator->stats.nlp_connected;
    bool perception = orchestrator->stats.perception_bridge_connected;
    bool cognitive = orchestrator->stats.cognitive_bridge_connected;
    bool training = orchestrator->stats.training_bridge_connected;
    bool omni = orchestrator->stats.omni_bridge_connected;
    bool immune = orchestrator->stats.immune_bridge_connected;
    bool gpu = orchestrator->stats.gpu_bridge_connected;
    bool bio_async = orchestrator->stats.bio_async_connected;

    memset(&orchestrator->stats, 0, sizeof(orchestrator->stats));

    /* Restore connection status */
    orchestrator->stats.wernicke_connected = wernicke;
    orchestrator->stats.broca_connected = broca;
    orchestrator->stats.nlp_connected = nlp;
    orchestrator->stats.perception_bridge_connected = perception;
    orchestrator->stats.cognitive_bridge_connected = cognitive;
    orchestrator->stats.training_bridge_connected = training;
    orchestrator->stats.omni_bridge_connected = omni;
    orchestrator->stats.immune_bridge_connected = immune;
    orchestrator->stats.gpu_bridge_connected = gpu;
    orchestrator->stats.bio_async_connected = bio_async;

    orchestrator->stats.current_state = orchestrator->state;
    orchestrator->stats.current_mode = orchestrator->mode;
}


//=============================================================================
// Memory Management
//=============================================================================

void language_comprehension_result_free(language_comprehension_result_t* result)
{
    if (!result) return;

    if (result->words) {
        nimcp_free(result->words);
        result->words = NULL;
    }
    if (result->concepts) {
        nimcp_free(result->concepts);
        result->concepts = NULL;
    }
    if (result->parse_tree) {
        /* Recursive tree free would go here */
        nimcp_free(result->parse_tree);
        result->parse_tree = NULL;
    }
    if (result->semantic_vector) {
        nimcp_free(result->semantic_vector);
        result->semantic_vector = NULL;
    }
    if (result->prosody.pitch_contour) {
        nimcp_free(result->prosody.pitch_contour);
        result->prosody.pitch_contour = NULL;
    }
    if (result->prosody.intensity_contour) {
        nimcp_free(result->prosody.intensity_contour);
        result->prosody.intensity_contour = NULL;
    }
}


void language_production_plan_free(language_production_plan_t* plan)
{
    if (!plan) return;

    /* Note: semantic_input is not owned by plan */
    if (plan->words) {
        nimcp_free(plan->words);
        plan->words = NULL;
    }
    if (plan->phonemes) {
        nimcp_free(plan->phonemes);
        plan->phonemes = NULL;
    }
    if (plan->motor_commands) {
        nimcp_free(plan->motor_commands);
        plan->motor_commands = NULL;
    }
    if (plan->prosody.pitch_contour) {
        nimcp_free(plan->prosody.pitch_contour);
        plan->prosody.pitch_contour = NULL;
    }
    if (plan->prosody.intensity_contour) {
        nimcp_free(plan->prosody.intensity_contour);
        plan->prosody.intensity_contour = NULL;
    }
}


//=============================================================================
// Internal Functions
//=============================================================================

static int orchestrator_init_buffers(language_orchestrator_t* orch)
{
    /* Allocate phoneme buffer */
    orch->input.phoneme_capacity = orch->config.phoneme_buffer_size;
    orch->input.phonemes = nimcp_calloc(orch->input.phoneme_capacity,
                                   sizeof(language_phoneme_t));
    if (!orch->input.phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orchestrator_init_buffers: Failed to allocate phoneme buffer");
        return -1;
    }
    orch->input.phoneme_count = 0;

    /* Allocate word buffer */
    orch->input.word_capacity = orch->config.max_utterance_words;
    orch->input.words = nimcp_calloc(orch->input.word_capacity,
                                sizeof(language_word_t));
    if (!orch->input.words) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orchestrator_init_buffers: Failed to allocate word buffer");
        nimcp_free(orch->input.phonemes);
        return -1;
    }
    orch->input.word_count = 0;

    /* Allocate text buffer */
    orch->input.text_capacity = 4096;
    orch->input.text_buffer = nimcp_calloc(orch->input.text_capacity, 1);
    if (!orch->input.text_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orchestrator_init_buffers: Failed to allocate text buffer");
        nimcp_free(orch->input.phonemes);
        nimcp_free(orch->input.words);
        return -1;
    }
    orch->input.text_length = 0;

    return 0;
}


static void orchestrator_free_buffers(language_orchestrator_t* orch)
{
    if (orch->input.phonemes) {
        nimcp_free(orch->input.phonemes);
        orch->input.phonemes = NULL;
    }
    if (orch->input.words) {
        nimcp_free(orch->input.words);
        orch->input.words = NULL;
    }
    if (orch->input.text_buffer) {
        nimcp_free(orch->input.text_buffer);
        orch->input.text_buffer = NULL;
    }
}
