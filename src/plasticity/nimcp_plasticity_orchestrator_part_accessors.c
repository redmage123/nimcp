// nimcp_plasticity_orchestrator_part_accessors.c - accessors functions
// Part of nimcp_plasticity_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_plasticity_orchestrator.c


/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int plasticity_orchestrator_default_config(plasticity_orchestrator_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config in default_config");
        NIMCP_LOGGING_ERROR("NULL config in default_config");
        return -1;
    }

    memset(config, 0, sizeof(plasticity_orchestrator_config_t));

    /* Enable all mechanisms by default */
    config->enabled.enable_classic_stdp = false;  /* Use triplet STDP instead */
    config->enabled.enable_triplet_stdp = true;
    config->enabled.enable_bcm = true;
    config->enabled.enable_homeostatic = true;
    config->enabled.enable_eligibility = true;
    config->enabled.enable_stp = false;  /* Optional */
    config->enabled.enable_dendritic = false;  /* Optional */
    config->enabled.enable_predictive = false;  /* Optional */
    config->enabled.enable_structural = true;
    config->enabled.enable_heterosynaptic = true;
    config->enabled.enable_calcium = true;
    config->enabled.enable_astrocyte = true;
    config->enabled.enable_protein_synthesis = true;
    config->enabled.enable_metaplasticity = true;
    config->enabled.enable_metabolic = true;

    /* Global modulation */
    config->global_learning_rate = 1.0f;
    config->sleep_modulation = 1.0f;
    config->immune_modulation = 1.0f;

    /* Timing */
    config->update_interval_ms = 1;
    config->consolidation_interval_ms = 60000;  /* 1 minute */
    config->homeostatic_interval_ms = 1000;     /* 1 second */

    /* Integration */
    config->connect_sleep_bridges = true;
    config->connect_immune_bridges = true;
    config->connect_bio_async = false;

    config->log_level = 2;  /* Warnings */

    return 0;
}


/* ============================================================================
 * Module Access Functions
 * ============================================================================ */

float plasticity_orchestrator_get_weight(
    const plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id
) {
    if (!orchestrator) return NAN;

    for (size_t i = 0; i < orchestrator->num_synapses; i++) {
        if (orchestrator->synapses[i].id == synapse_id &&
            orchestrator->synapses[i].active) {
            return orchestrator->synapses[i].weight;
        }
    }
    return NAN;
}


int plasticity_orchestrator_set_weight(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    float weight
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Orchestrator is NULL in set_weight");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    synapse_entry_t* syn = get_or_create_synapse(orchestrator, synapse_id);
    if (!syn) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_orchestrator_set_weight: syn is NULL");
        return -1;
    }

    syn->weight = fmaxf(syn->w_min, fminf(syn->w_max, weight));
    if (syn->triplet_stdp) {
        syn->triplet_stdp->weight = syn->weight;
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}


float plasticity_orchestrator_get_atp_level(
    const plasticity_orchestrator_t* orchestrator
) {
    if (!orchestrator) return -1.0f;

    if (orchestrator->metabolic) {
        return metabolic_plasticity_get_atp_level(orchestrator->metabolic);
    }
    return 100.0f;  /* Default full ATP */
}


float plasticity_orchestrator_get_calcium(
    const plasticity_orchestrator_t* orchestrator,
    uint32_t compartment_id
) {
    if (!orchestrator) return -1.0f;
    (void)compartment_id;

    if (orchestrator->calcium) {
        return calcium_get_concentration(orchestrator->calcium);
    }
    return 0.1f;  /* Default resting calcium (μM) */
}


float plasticity_orchestrator_get_threshold(
    const plasticity_orchestrator_t* orchestrator,
    uint32_t neuron_id
) {
    if (!orchestrator) return -1.0f;

    for (size_t i = 0; i < orchestrator->num_neurons; i++) {
        if (orchestrator->neurons[i].id == neuron_id &&
            orchestrator->neurons[i].active) {
            return orchestrator->neurons[i].bcm_threshold;
        }
    }
    return 0.5f;  /* Default threshold */
}


/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

int plasticity_orchestrator_get_stats(
    const plasticity_orchestrator_t* orchestrator,
    plasticity_stats_t* stats
) {
    if (!orchestrator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in get_stats");
        return -1;
    }

    *stats = orchestrator->stats;
    return 0;
}
