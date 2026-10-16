// nimcp_language_orchestrator_part_core.c - core functions
// Part of nimcp_language_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_language_orchestrator.c


int language_orchestrator_start(language_orchestrator_t* orchestrator)
{
    if (!orchestrator || !orchestrator->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_start: NULL or uninitialized orchestrator");
        return -1;
    }

    if (orchestrator->running) {
        return 0;  /* Already running */
    }

    /* Reset state */
    orchestrator->state = LANGUAGE_STATE_IDLE;
    orchestrator->state_entry_time_ms = 0;

    orchestrator->running = true;

    /* Start bridges if connected */
    if (orchestrator->perception_bridge) {
        language_perception_bridge_start(orchestrator->perception_bridge);
    }
    if (orchestrator->cognitive_bridge) {
        language_cognitive_bridge_start(orchestrator->cognitive_bridge);
    }
    if (orchestrator->training_bridge) {
        language_training_bridge_start(orchestrator->training_bridge);
    }
    if (orchestrator->omni_bridge) {
        language_omni_bridge_start(orchestrator->omni_bridge);
    }
    if (orchestrator->immune_bridge) {
        language_immune_bridge_start(orchestrator->immune_bridge);
    }
    if (orchestrator->gpu_bridge) {
        language_gpu_bridge_start(orchestrator->gpu_bridge);
    }

    return 0;
}


int language_orchestrator_stop(language_orchestrator_t* orchestrator)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_stop: NULL orchestrator");
        return -1;
    }

    if (!orchestrator->running) {
        return 0;  /* Already stopped */
    }

    /* Stop bridges */
    if (orchestrator->perception_bridge) {
        language_perception_bridge_stop(orchestrator->perception_bridge);
    }
    if (orchestrator->cognitive_bridge) {
        language_cognitive_bridge_stop(orchestrator->cognitive_bridge);
    }
    if (orchestrator->training_bridge) {
        language_training_bridge_stop(orchestrator->training_bridge);
    }
    if (orchestrator->omni_bridge) {
        language_omni_bridge_stop(orchestrator->omni_bridge);
    }
    if (orchestrator->immune_bridge) {
        language_immune_bridge_stop(orchestrator->immune_bridge);
    }
    if (orchestrator->gpu_bridge) {
        language_gpu_bridge_stop(orchestrator->gpu_bridge);
    }

    orchestrator->running = false;
    orchestrator->state = LANGUAGE_STATE_IDLE;

    return 0;
}


//=============================================================================
// Subsystem Connection API
//=============================================================================

int language_orchestrator_connect_wernicke(
    language_orchestrator_t* orchestrator,
    wernicke_adapter_t* wernicke)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_wernicke: NULL orchestrator");
        return -1;
    }

    orchestrator->wernicke = wernicke;
    orchestrator->stats.wernicke_connected = (wernicke != NULL);

    return 0;
}


int language_orchestrator_connect_broca(
    language_orchestrator_t* orchestrator,
    broca_adapter_t* broca)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_broca: NULL orchestrator");
        return -1;
    }

    orchestrator->broca = broca;
    orchestrator->stats.broca_connected = (broca != NULL);

    return 0;
}


int language_orchestrator_connect_nlp(
    language_orchestrator_t* orchestrator,
    nlp_network_t nlp)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_nlp: NULL orchestrator");
        return -1;
    }

    orchestrator->nlp_network = nlp;
    orchestrator->stats.nlp_connected = (nlp != NULL);

    return 0;
}


int language_orchestrator_connect_speech_cortex(
    language_orchestrator_t* orchestrator,
    speech_cortex_t* speech)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_speech_cortex: NULL orchestrator");
        return -1;
    }

    orchestrator->speech_cortex = speech;

    return 0;
}


int language_orchestrator_connect_multimodal(
    language_orchestrator_t* orchestrator,
    multimodal_nlp_bridge_t* multimodal)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_multimodal: NULL orchestrator");
        return -1;
    }

    orchestrator->multimodal = multimodal;

    return 0;
}


//=============================================================================
// Bridge Connection API
//=============================================================================

int language_orchestrator_connect_perception_bridge(
    language_orchestrator_t* orchestrator,
    language_perception_bridge_t* bridge)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_perception_bridge: NULL orchestrator");
        return -1;
    }

    orchestrator->perception_bridge = bridge;
    orchestrator->stats.perception_bridge_connected = (bridge != NULL);

    /* Connect bridge back to orchestrator */
    if (bridge) {
        language_perception_bridge_connect_orchestrator(bridge, orchestrator);
    }

    return 0;
}


int language_orchestrator_connect_cognitive_bridge(
    language_orchestrator_t* orchestrator,
    language_cognitive_bridge_t* bridge)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_cognitive_bridge: NULL orchestrator");
        return -1;
    }

    orchestrator->cognitive_bridge = bridge;
    orchestrator->stats.cognitive_bridge_connected = (bridge != NULL);

    if (bridge) {
        language_cognitive_bridge_connect_orchestrator(bridge, orchestrator);
    }

    return 0;
}


int language_orchestrator_connect_training_bridge(
    language_orchestrator_t* orchestrator,
    language_training_bridge_t* bridge)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_training_bridge: NULL orchestrator");
        return -1;
    }

    orchestrator->training_bridge = bridge;
    orchestrator->stats.training_bridge_connected = (bridge != NULL);

    if (bridge) {
        language_training_bridge_connect_orchestrator(bridge, orchestrator);
    }

    return 0;
}


int language_orchestrator_connect_omni_bridge(
    language_orchestrator_t* orchestrator,
    language_omni_bridge_t* bridge)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_omni_bridge: NULL orchestrator");
        return -1;
    }

    orchestrator->omni_bridge = bridge;
    orchestrator->stats.omni_bridge_connected = (bridge != NULL);

    if (bridge) {
        language_omni_bridge_connect_orchestrator(bridge, orchestrator);
    }

    return 0;
}


int language_orchestrator_connect_immune_bridge(
    language_orchestrator_t* orchestrator,
    language_immune_bridge_t* bridge)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_immune_bridge: NULL orchestrator");
        return -1;
    }

    orchestrator->immune_bridge = bridge;
    orchestrator->stats.immune_bridge_connected = (bridge != NULL);

    if (bridge) {
        language_immune_bridge_connect_orchestrator(bridge, orchestrator);
    }

    return 0;
}


int language_orchestrator_connect_gpu_bridge(
    language_orchestrator_t* orchestrator,
    language_gpu_bridge_t* bridge)
{
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_connect_gpu_bridge: NULL orchestrator");
        return -1;
    }

    orchestrator->gpu_bridge = bridge;
    orchestrator->stats.gpu_bridge_connected = (bridge != NULL);

    if (bridge) {
        language_gpu_bridge_connect_orchestrator(bridge, orchestrator);
    }

    return 0;
}


int language_orchestrator_generate_output(
    language_orchestrator_t* orchestrator,
    const float* semantic_input,
    uint32_t semantic_dim,
    void* output,
    uint32_t max_output,
    uint32_t* output_size,
    language_output_type_t output_type)
{
    if (!orchestrator || !orchestrator->running || !semantic_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_generate_output: Invalid parameters");
        return -1;
    }

    /* Store semantic input for production */
    orchestrator->current_production.semantic_input = (float*)semantic_input;
    orchestrator->current_production.semantic_dim = semantic_dim;

    /* Transition to generating state */
    orchestrator_transition_state(orchestrator,
        LANGUAGE_STATE_GENERATING,
        orchestrator->last_update_ms);

    if (output_size) {
        *output_size = 0;
    }

    /* ===== LANGUAGE PRODUCTION PIPELINE ===== */

    /* Step 1: Select lexical items from semantic vector via LPB or inline */
    enum { MAX_SPEAK_TOKENS = 32 };
    lpb_token_t tokens[MAX_SPEAK_TOKENS];
    uint32_t num_tokens = 0;

    /* Try to use Wernicke's lexicon if available */
    lexical_access_t* lexical = NULL;
    if (orchestrator->wernicke) {
        lexical = wernicke_get_lexical_access(orchestrator->wernicke);
    }

    if (lexical && lexical_get_size(lexical) > 0) {
        /* Compute semantic magnitude */
        float mag2 = 0.0F;
        for (uint32_t i = 0; i < semantic_dim && i < 256; i++) {
            mag2 += semantic_input[i] * semantic_input[i];
        }
        float magnitude = sqrtf(mag2);
        float base_act = magnitude > 0.0F ? magnitude / sqrtf((float)semantic_dim) : 0.5F;

        /* Score each word in lexicon */
        uint32_t lex_size = lexical_get_size(lexical);
        struct { uint32_t id; char word[32]; float score; } best[MAX_SPEAK_TOKENS];
        uint32_t nbest = 0;
        float min_best = 0.0F;

        for (uint32_t wid = 1; wid <= lex_size; wid++) {
            lexical_entry_t entry;
            if (!lexical_get_entry(lexical, wid, &entry)) continue;

            /* Concept-positional activation */
            float act = 0.0F;
            uint32_t fidx = (entry.concept_id * 5) % semantic_dim;
            uint32_t win = 8;
            for (uint32_t d = fidx; d < fidx + win && d < semantic_dim; d++) {
                act += fabsf(semantic_input[d]);
            }
            act /= (float)win;

            float freq_w = 0.5F + 0.5F * (entry.frequency / 7.0F);
            float score = act * freq_w * base_act;

            if (score > 0.05F) {
                if (nbest < MAX_SPEAK_TOKENS) {
                    best[nbest].id = wid;
                    strncpy(best[nbest].word, entry.orthography, 31);
                    best[nbest].word[31] = '\0';
                    best[nbest].score = score;
                    nbest++;
                    if (score < min_best || nbest == 1) min_best = score;
                } else if (score > min_best) {
                    /* Replace worst */
                    uint32_t worst = 0;
                    for (uint32_t k = 1; k < nbest; k++) {
                        if (best[k].score < best[worst].score) worst = k;
                    }
                    best[worst].id = wid;
                    strncpy(best[worst].word, entry.orthography, 31);
                    best[worst].word[31] = '\0';
                    best[worst].score = score;
                    min_best = best[0].score;
                    for (uint32_t k = 1; k < nbest; k++) {
                        if (best[k].score < min_best) min_best = best[k].score;
                    }
                }
            }
        }

        /* Sort by score descending */
        for (uint32_t i = 1; i < nbest; i++) {
            for (uint32_t j = i; j > 0 && best[j].score > best[j - 1].score; j--) {
                __typeof__(best[0]) tmp = best[j];
                best[j] = best[j - 1];
                best[j - 1] = tmp;
            }
        }

        for (uint32_t i = 0; i < nbest; i++) {
            tokens[i].token_id = best[i].id;
            strncpy(tokens[i].token_str, best[i].word, sizeof(tokens[i].token_str) - 1);
            tokens[i].token_str[sizeof(tokens[i].token_str) - 1] = '\0';
            tokens[i].activation = best[i].score;
        }
        num_tokens = nbest;
    }

    /* Fallback: feature-based heuristic */
    if (num_tokens == 0) {
        float regions[3] = {0};
        for (uint32_t i = 0; i < 32 && i < semantic_dim; i++)
            regions[0] += fabsf(semantic_input[i]);
        for (uint32_t i = 32; i < 64 && i < semantic_dim; i++)
            regions[1] += fabsf(semantic_input[i]);
        for (uint32_t i = 64; i < 96 && i < semantic_dim; i++)
            regions[2] += fabsf(semantic_input[i]);
        regions[0] /= 32.0F; regions[1] /= 32.0F; regions[2] /= 32.0F;

        const char* fallback_words[] = {"it", "does", "good"};
        for (int r = 0; r < 3 && num_tokens < MAX_SPEAK_TOKENS; r++) {
            if (regions[r] > 0.1F) {
                strncpy(tokens[num_tokens].token_str, fallback_words[r],
                        sizeof(tokens[num_tokens].token_str) - 1);
                tokens[num_tokens].token_str[sizeof(tokens[num_tokens].token_str) - 1] = '\0';
                tokens[num_tokens].activation = regions[r];
                num_tokens++;
            }
        }
    }

    if (num_tokens == 0) {
        orchestrator_transition_state(orchestrator,
            LANGUAGE_STATE_IDLE, orchestrator->last_update_ms);
        return 0;
    }

    /* Step 2: Produce through Broca if available */
    if (orchestrator->broca) {
        const char* word_ptrs[MAX_SPEAK_TOKENS];
        for (uint32_t i = 0; i < num_tokens; i++) {
            word_ptrs[i] = tokens[i].token_str;
        }
        broca_produce_from_strings(orchestrator->broca, word_ptrs, num_tokens, NULL);
    }

    /* Step 3: Concatenate token strings into output text */
    if (output && max_output > 0 && output_type == LANGUAGE_OUTPUT_TEXT) {
        char* out_text = (char*)output;
        uint32_t pos = 0;
        for (uint32_t i = 0; i < num_tokens && pos < max_output - 1; i++) {
            if (i > 0 && pos < max_output - 1) {
                out_text[pos++] = ' ';
            }
            uint32_t wlen = (uint32_t)strlen(tokens[i].token_str);
            uint32_t copy_len = (wlen < max_output - 1 - pos) ? wlen : (max_output - 1 - pos);
            memcpy(out_text + pos, tokens[i].token_str, copy_len);
            pos += copy_len;
        }
        out_text[pos] = '\0';

        if (output_size) {
            *output_size = pos;
        }
    } else if (output_size) {
        /* For non-text modes, just report token count */
        *output_size = num_tokens;
    }

    /* Update production plan */
    orchestrator->current_production.fluency_score =
        (num_tokens > 0) ? tokens[0].activation : 0.0F;
    orchestrator->current_production.valid = true;
    orchestrator->current_production.complete = true;

    /* Transition back to idle */
    orchestrator_transition_state(orchestrator,
        LANGUAGE_STATE_IDLE, orchestrator->last_update_ms);

    return 0;
}


//=============================================================================
// Event API
//=============================================================================

int language_orchestrator_register_callback(
    language_orchestrator_t* orchestrator,
    language_event_callback_t callback,
    void* user_data)
{
    if (!orchestrator || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_register_callback: NULL parameter");
        return -1;
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < ORCHESTRATOR_MAX_CALLBACKS; i++) {
        if (!orchestrator->callbacks[i].active) {
            orchestrator->callbacks[i].callback = callback;
            orchestrator->callbacks[i].user_data = user_data;
            orchestrator->callbacks[i].active = true;
            orchestrator->num_callbacks++;
            return 0;
        }
    }

    return -1;  /* No callback slots available - normal capacity behavior */
}


int language_orchestrator_unregister_callback(
    language_orchestrator_t* orchestrator,
    language_event_callback_t callback)
{
    if (!orchestrator || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_orchestrator_unregister_callback: NULL parameter");
        return -1;
    }

    for (uint32_t i = 0; i < ORCHESTRATOR_MAX_CALLBACKS; i++) {
        if (orchestrator->callbacks[i].active &&
            orchestrator->callbacks[i].callback == callback) {
            orchestrator->callbacks[i].active = false;
            orchestrator->callbacks[i].callback = NULL;
            orchestrator->callbacks[i].user_data = NULL;
            orchestrator->num_callbacks--;
            return 0;
        }
    }

    return -1;  /* Callback not found - normal lookup behavior */
}
