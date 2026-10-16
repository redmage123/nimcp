// nimcp_part_io.c - io functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c


nimcp_status_t nimcp_brain_save(nimcp_brain_t brain, const char* filepath) {
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    API_CHECK_THROW(filepath, NIMCP_ERROR_NULL_ARG, "Filepath is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Call internal brain API
    bool success = brain_save(brain->internal_brain, filepath);

    if (!success) {
        API_CHECK_THROW(success, NIMCP_ERROR_IO, "Failed to save brain");
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_brain_t nimcp_brain_load(const char* filepath) {
    if (!filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_ARG, "nimcp_brain_load: filepath is NULL");
        return NULL;
    }

    nimcp_brain_t handle = NULL;

    // Allocate handle (calloc to zero-init last_loss/last_gradient_norm)
    handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_load: handle allocation failed");
        goto cleanup;
    }

    // Load internal brain (auto-detect unified vs legacy format)
    {
        extern brain_t brain_load_auto(const char* filepath);
        handle->internal_brain = brain_load_auto(filepath);
    }
    if (!handle->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "nimcp_brain_load: brain_load failed for given filepath");
        goto cleanup;
    }

    // Re-initialize GPU subsystem (GPU state is not serialized in checkpoint)
    {
        extern bool nimcp_brain_factory_init_gpu_subsystem(brain_t brain);
        nimcp_brain_factory_init_gpu_subsystem(handle->internal_brain);
    }

    // NOTE: GPU inference (weight cache + adaptive network) is initialized
    // via nimcp_brain_eager_init_cognitive() AFTER full brain restoration,
    // because adaptive_network_get_config() may crash on partially-restored state.

    // Initialize sparse coding on loaded brain (not serialized in checkpoint)
    {
        brain_t b = handle->internal_brain;
        if (!b->sparse_coding_system) {
            sparse_coding_config_t sc_config;
            cortical_sparse_default_config(&sc_config);
            sc_config.sparsity_method = SPARSITY_METHOD_K_WTA;
            sc_config.target_sparsity = 0.05f;
            sc_config.num_columns = b->config.num_outputs > 0 ? b->config.num_outputs : 4096;
            sc_config.k_winners = (uint32_t)(sc_config.num_columns * sc_config.target_sparsity);
            if (sc_config.k_winners < 8) sc_config.k_winners = 8;
            sc_config.enable_homeostasis = true;
            sc_config.adaptation_rate = 0.002f;
            sc_config.enable_bio_async = false;
            sc_config.enable_lateral_inhibition = false;
            b->sparse_coding_system = cortical_sparse_create(&sc_config);
            b->enable_sparse_coding = (b->sparse_coding_system != NULL);
        }
    }

    /* Walkthrough-2 (Option-1 rebuild, 2026-05-19) — recreate concept_registry
     * on resume. nimcp_brain_load skips the parallel init waves and only
     * touches state-restoration paths, so without this hook brain->concept_registry
     * stays NULL after every checkpoint load. The bind hook in
     * brain_learn_vector then no-ops silently and no cross-modal binding
     * ever happens. Same pattern as the GPU + sparse_coding re-init above.
     *
     * Note: brain_load_post_init_sidecars (which fills the registry from
     * the .concept_registry sidecar if one exists) is called downstream by
     * nimcp_brain_eager_init_cognitive — no need to call it here. */
    {
        brain_t b = handle->internal_brain;
        if (!b->concept_registry) {
            extern bool nimcp_brain_factory_init_concept_registry_subsystem(brain_t);
            nimcp_brain_factory_init_concept_registry_subsystem(b);
        }
        /* Wire the registry into grounded_language so its mirror_binding_to_bridge
         * path can intern text into the registry on every grounding event.
         * The same wiring is done in nimcp_brain_init_language.c on fresh
         * init; the load path skips that wave, so we re-do it here. */
        if (b->concept_registry && b->grounded_lang) {
            extern void grounded_language_set_concept_registry(
                struct grounded_language* gl, void* registry);
            grounded_language_set_concept_registry(b->grounded_lang,
                                                    b->concept_registry);
        }
    }

    /* Toxicity classifier — same load-path wiring (2026-05-21). The
     * checkpoint does not serialize the classifier; it must be re-created
     * here, then plumbed into grounded_language. NULL-safe on failure
     * (classify becomes a no-op; the produce path still works). */
    /* Round-4 cleanup: replaced earlier extern-void* declarations with
     * proper includes via nimcp_part_core.c (#include'd before this part
     * in nimcp.c). Now using real typed APIs from
     * security/nimcp_toxicity_response.h + nimcp_toxicity_ml.h +
     * language/nimcp_grounded_language.h. The setter and registration
     * still need an extern declaration since they're defined in C files
     * that don't have public headers, but the *_create functions do. */
    {
        brain_t b = handle->internal_brain;
        if (!b->toxicity_classifier) {
            b->toxicity_classifier =
                toxicity_classifier_create("data/safety/toxicity_rules.tsv", 0);
        }
        if (b->toxicity_classifier && b->grounded_lang) {
            grounded_language_set_toxicity_classifier(b->grounded_lang,
                                                      b->toxicity_classifier);
        }
        /* Phase 3a/3c: response engine + ML head on resume path too. */
        if (!b->toxicity_response) {
            b->toxicity_response = toxicity_response_create(
                "data/safety/toxicity_counterclaims.tsv",
                "data/safety/toxicity_antiframes.tsv");
        }
        if (b->toxicity_response && b->grounded_lang) {
            grounded_language_set_toxicity_response(b->grounded_lang,
                                                    b->toxicity_response);
        }
        if (!b->toxicity_ml) {
            b->toxicity_ml = toxicity_ml_create("data/safety/toxicity_ml.bin");
        }
        /* Round-3 fix: re-register the toxicity cycle on --resume so the
         * 1Hz tick (which runs ML training + valence decay + heartbeat KG
         * emit) fires after a checkpoint reload. Without this, the cycle
         * was only registered on fresh init. */
        if (b->toxicity_classifier) {
            extern int nimcp_brain_factory_register_toxicity_cycle(brain_t);
            (void)nimcp_brain_factory_register_toxicity_cycle(b);
        }
    }

    set_error("No error");
    return handle;

cleanup:
    set_error("Failed to load brain from file");
    nimcp_free(handle);
    return NULL;
}


//=============================================================================
// Brain Snapshot API Implementation
//=============================================================================

nimcp_status_t nimcp_brain_snapshot_save(
    nimcp_brain_t brain,
    const char* name,
    const char* description)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    API_CHECK_THROW(name, NIMCP_ERROR_NULL_ARG, "Snapshot name is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Call internal brain snapshot API
    bool success = brain_save_snapshot(
        brain->internal_brain,
        name,
        description ? description : ""
    );

    if (!success) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_IO, "Failed to save snapshot");
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_workspace_read(
    nimcp_brain_t brain,
    float* content,
    uint32_t max_dim,
    uint32_t* actual_dim,
    nimcp_cognitive_module_t* source_module)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_read");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Validate parameters FIRST (before checking subsystem availability)
    NIMCP_CHECK_THROW(content && actual_dim && source_module, NIMCP_ERROR_NULL_ARG,
                      "NULL output parameter provided to workspace_read");
    NIMCP_CHECK_THROW(max_dim != 0, NIMCP_ERROR_INVALID, "Invalid max_dim (0)");

    // Guard: Check if global workspace enabled (after parameter validation)
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Read broadcast
    cognitive_module_t internal_source;
    bool success = global_workspace_read_broadcast(
        workspace, content, max_dim, actual_dim, &internal_source
    );

    if (success) {
        *source_module = (nimcp_cognitive_module_t)internal_source;
        set_error("No error");
        return NIMCP_OK;
    } else {
        set_error("No broadcast available or buffer too small");
        return NIMCP_ERROR;
    }
}
