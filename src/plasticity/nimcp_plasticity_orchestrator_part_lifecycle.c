// nimcp_plasticity_orchestrator_part_lifecycle.c - lifecycle functions
// Part of nimcp_plasticity_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_plasticity_orchestrator.c


static synapse_entry_t* get_or_create_synapse(plasticity_orchestrator_t* orch, uint32_t id) {
    synapse_entry_t* syn = find_synapse(orch, id);
    if (syn) return syn;

    if (orch->num_synapses >= orch->synapse_capacity) {
        NIMCP_LOGGING_WARN("Synapse capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_or_create_synapse: capacity exceeded");
        return NULL;
    }

    syn = &orch->synapses[orch->num_synapses++];
    memset(syn, 0, sizeof(synapse_entry_t));
    syn->id = id;
    syn->weight = 0.5f;
    syn->w_min = 0.0f;
    syn->w_max = 1.0f;
    syn->active = true;

    /* Initialize BCM state */
    syn->bcm_state = bcm_synapse_init(syn->weight, 0.5f);

    /* Create triplet STDP synapse if enabled */
    if (orch->config.enabled.enable_triplet_stdp) {
        syn->triplet_stdp = triplet_stdp_synapse_create(NULL, syn->weight);
    }

    /* Register with structural plasticity if enabled */
    if (orch->config.enabled.enable_structural && orch->structural) {
        uint32_t struct_id = 0;
        /* Form synapse with moderate initial activity to trigger formation event */
        if (structural_plasticity_form_synapse(orch->structural, id, id, 25.0f, &struct_id) == 0) {
            syn->structural_synapse_id = struct_id;
        }
    }

    return syn;
}


static neuron_entry_t* get_or_create_neuron(plasticity_orchestrator_t* orch, uint32_t id) {
    neuron_entry_t* neuron = find_neuron(orch, id);
    if (neuron) return neuron;

    if (orch->num_neurons >= orch->neuron_capacity) {
        NIMCP_LOGGING_WARN("Neuron capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_or_create_neuron: capacity exceeded");
        return NULL;
    }

    neuron = &orch->neurons[orch->num_neurons++];
    memset(neuron, 0, sizeof(neuron_entry_t));
    neuron->id = id;
    neuron->firing_rate = 0.0f;
    neuron->bcm_threshold = 0.5f;
    neuron->target_rate = 5.0f;  /* Default 5 Hz target */
    neuron->active = true;

    /* Initialize homeostatic states */
    neuron->scaling_state = synaptic_scaling_state_init(neuron->target_rate);
    neuron->ip_state = intrinsic_plasticity_state_init(0.5f, 1.0f);
    neuron->meta_state = metaplasticity_state_init(0.5f);

    return neuron;
}


plasticity_orchestrator_t* plasticity_orchestrator_create(
    const plasticity_orchestrator_config_t* config
) {
    plasticity_orchestrator_config_t default_config;
    if (!config) {
        plasticity_orchestrator_default_config(&default_config);
        config = &default_config;
    }

    plasticity_orchestrator_t* orchestrator = (plasticity_orchestrator_t*)nimcp_malloc(
        sizeof(plasticity_orchestrator_t)
    );

    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate plasticity orchestrator");
        NIMCP_LOGGING_ERROR("Failed to allocate plasticity orchestrator");
        return NULL;
    }

    memset(orchestrator, 0, sizeof(plasticity_orchestrator_t));
    orchestrator->config = *config;
    orchestrator->next_callback_id = 1;

    /* Create mutex */
    orchestrator->mutex = nimcp_platform_mutex_create();
    if (!orchestrator->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(orchestrator);
        return NULL;
    }

    /* Allocate synapse and neuron arrays */
    orchestrator->synapse_capacity = MAX_SYNAPSES;
    orchestrator->neuron_capacity = MAX_NEURONS;

    orchestrator->synapses = (synapse_entry_t*)nimcp_malloc(
        sizeof(synapse_entry_t) * MAX_SYNAPSES
    );
    orchestrator->neurons = (neuron_entry_t*)nimcp_malloc(
        sizeof(neuron_entry_t) * MAX_NEURONS
    );

    if (!orchestrator->synapses || !orchestrator->neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate synapse/neuron arrays");
        NIMCP_LOGGING_ERROR("Failed to allocate synapse/neuron arrays");
        if (orchestrator->synapses) nimcp_free(orchestrator->synapses);
        if (orchestrator->neurons) nimcp_free(orchestrator->neurons);
        nimcp_platform_mutex_destroy(orchestrator->mutex);
        nimcp_free(orchestrator->mutex);
        orchestrator->mutex = NULL;
        nimcp_free(orchestrator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_orchestrator_create: validation failed");
        return NULL;
    }

    memset(orchestrator->synapses, 0, sizeof(synapse_entry_t) * MAX_SYNAPSES);
    memset(orchestrator->neurons, 0, sizeof(neuron_entry_t) * MAX_NEURONS);

    /* Initialize metabolic system */
    if (config->enabled.enable_metabolic) {
        metabolic_config_t met_config;
        metabolic_plasticity_default_config(&met_config);
        orchestrator->metabolic = metabolic_plasticity_create(&met_config);
    }

    /* Initialize calcium dynamics */
    if (config->enabled.enable_calcium) {
        calcium_config_t ca_config;
        calcium_default_config(&ca_config);
        orchestrator->calcium = calcium_create(&ca_config);
    }

    /* Initialize structural plasticity */
    if (config->enabled.enable_structural) {
        structural_plasticity_config_t struct_config;
        structural_plasticity_default_config(&struct_config);
        orchestrator->structural = structural_plasticity_create(&struct_config);
        /* Register callback for spine formation/elimination events */
        if (orchestrator->structural) {
            structural_plasticity_register_callback(
                orchestrator->structural, structural_change_handler, orchestrator);
        }
    }

    /* Initialize heterosynaptic system */
    if (config->enabled.enable_heterosynaptic) {
        hetero_config_t hetero_config;
        hetero_default_config(&hetero_config);
        orchestrator->heterosynaptic = hetero_create(&hetero_config, 1000);
    }

    /* Initialize astrocyte system */
    if (config->enabled.enable_astrocyte) {
        astrocyte_config_t astro_config;
        astrocyte_plasticity_default_config(&astro_config);
        orchestrator->astrocyte = astrocyte_plasticity_create(&astro_config, MAX_ASTROCYTES);
    }

    /* Initialize protein synthesis */
    if (config->enabled.enable_protein_synthesis) {
        protein_synthesis_config_t prot_config;
        protein_synthesis_default_config(&prot_config);
        orchestrator->protein_synthesis = protein_synthesis_create(&prot_config);
    }

    /* Initialize homeostatic controller */
    if (config->enabled.enable_homeostatic) {
        homeostatic_config_t homeo_config = homeostatic_config_default();
        orchestrator->homeostatic = homeostatic_controller_create(&homeo_config, MAX_NEURONS);
    }

    /* Initialize metaplasticity controller */
    if (config->enabled.enable_metaplasticity) {
        extended_metaplasticity_config_t meta_config = metaplasticity_config_default();
        orchestrator->metaplasticity = metaplasticity_controller_create(&meta_config, MAX_SYNAPSES);
    }

    /* Initialize default modulation */
    orchestrator->current_sleep_state = SLEEP_STATE_AWAKE;
    orchestrator->sleep_modulation_factor = 1.0f;
    orchestrator->immune_modulation_factor = 1.0f;
    orchestrator->neuromod_levels.dopamine = 0.5f;
    orchestrator->neuromod_levels.norepinephrine = 0.3f;
    orchestrator->neuromod_levels.acetylcholine = 0.5f;
    orchestrator->neuromod_levels.serotonin = 0.4f;

    /* Initialize statistics */
    orchestrator->stats.mean_weight = 0.5f;
    orchestrator->stats.mean_atp = 100.0f;
    orchestrator->stats.min_atp = 100.0f;

    NIMCP_LOGGING_INFO("Plasticity orchestrator created with all modules");
    return orchestrator;
}


void plasticity_orchestrator_destroy(plasticity_orchestrator_t* orchestrator) {
    if (!orchestrator) return;

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Destroy synapse module states */
    for (size_t i = 0; i < orchestrator->num_synapses; i++) {
        if (orchestrator->synapses[i].triplet_stdp) {
            triplet_stdp_synapse_destroy(orchestrator->synapses[i].triplet_stdp);
        }
    }

    /* Destroy neuron input arrays */
    for (size_t i = 0; i < orchestrator->num_neurons; i++) {
        if (orchestrator->neurons[i].input_synapse_indices) {
            nimcp_free(orchestrator->neurons[i].input_synapse_indices);
        }
    }

    /* Destroy global modules */
    if (orchestrator->metabolic) {
        metabolic_plasticity_destroy(orchestrator->metabolic);
    }
    if (orchestrator->calcium) {
        calcium_destroy(orchestrator->calcium);
    }
    if (orchestrator->structural) {
        structural_plasticity_destroy(orchestrator->structural);
    }
    if (orchestrator->heterosynaptic) {
        hetero_destroy(orchestrator->heterosynaptic);
    }
    if (orchestrator->astrocyte) {
        astrocyte_plasticity_destroy(orchestrator->astrocyte);
    }
    if (orchestrator->protein_synthesis) {
        protein_synthesis_destroy(orchestrator->protein_synthesis);
    }
    if (orchestrator->homeostatic) {
        homeostatic_controller_destroy(orchestrator->homeostatic);
    }
    if (orchestrator->metaplasticity) {
        metaplasticity_controller_destroy(orchestrator->metaplasticity);
    }

    /* Free arrays */
    if (orchestrator->synapses) nimcp_free(orchestrator->synapses);
    if (orchestrator->neurons) nimcp_free(orchestrator->neurons);

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    nimcp_platform_mutex_destroy(orchestrator->mutex);
    nimcp_free(orchestrator->mutex);
    orchestrator->mutex = NULL;

    nimcp_free(orchestrator);

    NIMCP_LOGGING_DEBUG("Destroyed plasticity orchestrator");
}


int plasticity_orchestrator_reset_stats(plasticity_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in reset_stats");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    memset(&orchestrator->stats, 0, sizeof(plasticity_stats_t));
    orchestrator->stats.mean_weight = 0.5f;
    orchestrator->stats.mean_atp = 100.0f;
    orchestrator->stats.min_atp = 100.0f;

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}
