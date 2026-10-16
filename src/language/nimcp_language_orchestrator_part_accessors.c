// nimcp_language_orchestrator_part_accessors.c - accessors functions
// Part of nimcp_language_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_language_orchestrator.c


//=============================================================================
// Configuration API
//=============================================================================

void language_orchestrator_default_config(language_orchestrator_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Processing mode */
    config->default_mode = LANGUAGE_MODE_DIALOGUE;

    /* Subsystem enables */
    config->enable_wernicke = true;
    config->enable_broca = true;
    config->enable_nlp_core = true;
    config->enable_spike_nlp = false;
    config->enable_multimodal = true;

    /* Bridge enables */
    config->enable_perception_bridge = true;
    config->enable_cognitive_bridge = true;
    config->enable_training_bridge = true;
    config->enable_omni_bridge = true;
    config->enable_immune_bridge = true;
    config->enable_gpu_bridge = false;  /* Opt-in */

    /* Processing settings */
    config->max_utterance_words = LANGUAGE_MAX_WORDS;
    config->phoneme_buffer_size = LANGUAGE_MAX_PHONEMES;
    config->semantic_dim = LANGUAGE_SEMANTIC_DIM;
    config->comprehension_threshold = 0.5f;
    config->production_threshold = 0.5f;

    /* Timing settings */
    config->update_interval_ms = 20;
    config->comprehension_timeout_ms = 5000;
    config->production_timeout_ms = 3000;

    /* Bio-async settings */
    config->enable_bio_async = true;
    config->message_inbox_capacity = 64;

    /* Logging */
    config->enable_logging = false;
    config->enable_stats = true;
}


int language_orchestrator_get_comprehension(
    const language_orchestrator_t* orchestrator,
    language_comprehension_result_t* result)
{
    if (!orchestrator || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_get_comprehension: NULL parameter");
        return -1;
    }

    if (!orchestrator->comprehension_valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "language_orchestrator_get_comprehension: No valid comprehension");
        return -1;
    }

    /* P1 note: Shallow copy - returned pointers (words, concepts, parse_tree,
     * semantic_vector, prosody contours) are BORROWED from the orchestrator.
     * Caller must NOT free them. Valid until next orchestrator update. */
    *result = orchestrator->current_comprehension;
    return 0;
}


int language_orchestrator_get_production_plan(
    const language_orchestrator_t* orchestrator,
    language_production_plan_t* plan)
{
    if (!orchestrator || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_get_production_plan: NULL parameter");
        return -1;
    }

    if (!orchestrator->production_valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "language_orchestrator_get_production_plan: No valid production plan");
        return -1;
    }

    /* P1 note: Shallow copy - returned pointers (semantic_input, words, phonemes,
     * motor_commands, prosody contours) are BORROWED from the orchestrator.
     * Caller must NOT free them. Valid until next orchestrator update. */
    *plan = orchestrator->current_production;
    return 0;
}


//=============================================================================
// State API
//=============================================================================

language_state_t language_orchestrator_get_state(
    const language_orchestrator_t* orchestrator)
{
    if (!orchestrator) {
        return LANGUAGE_STATE_ERROR;
    }
    return orchestrator->state;
}


language_mode_t language_orchestrator_get_mode(
    const language_orchestrator_t* orchestrator)
{
    if (!orchestrator) {
        return LANGUAGE_MODE_COMPREHENSION;
    }
    return orchestrator->mode;
}


int language_orchestrator_set_mode(
    language_orchestrator_t* orchestrator,
    language_mode_t mode)
{
    if (!orchestrator || mode >= LANGUAGE_MODE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "language_orchestrator_set_mode: Invalid parameters");
        return -1;
    }

    orchestrator->mode = mode;
    orchestrator->stats.current_mode = mode;

    return 0;
}


//=============================================================================
// Statistics API
//=============================================================================

int language_orchestrator_get_stats(
    const language_orchestrator_t* orchestrator,
    language_orchestrator_stats_t* stats)
{
    if (!orchestrator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_get_stats: NULL parameter");
        return -1;
    }

    *stats = orchestrator->stats;
    return 0;
}
