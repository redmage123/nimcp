// nimcp_brain_part_processing.c - processing functions
// Part of nimcp_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain.c


/**
 * @brief Publish brain processing event
 *
 * @param processing_type Type of processing (inference, learning, etc.)
 * @param confidence Processing confidence [0,1]
 */
static void brain_publish_processing_event(const char* processing_type, float confidence)
{
    // Use atomic load for thread-safe access to global state
    if (!__atomic_load_n(&g_brain_bio_initialized, __ATOMIC_ACQUIRE) ||
        !__atomic_load_n(&g_brain_bio_ctx, __ATOMIC_ACQUIRE)) {
        return;  // Graceful degradation
    }

    LOG_MODULE_DEBUG("BRAIN", "Publishing processing event: type=%s, confidence=%.3f",
                     processing_type, confidence);

    // Use predictive coding signal for processing events
    char signal_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(signal_name, sizeof(signal_name), "brain.processing.%s", processing_type);
    bio_router_publish_signal(g_brain_bio_ctx, signal_name, confidence);
}


//=============================================================================
// Brain Resize Helper (called from nimcp_brain_resize.c)
//=============================================================================

/**
 * @brief Internal helper for brain_resize - update subsystems after network swap
 *
 * WHAT: Updates glial integration and other subsystems to reference new network
 * WHY:  brain_resize.c can't access full brain struct, so needs helper with full access
 * HOW:  Destroys/recreates glial integration with new network reference
 *
 * @param brain Brain handle
 * @param new_base_network New neural network after resize
 * @param new_neuron_count New neuron count after resize
 * @return true on success
 */
bool brain_resize_update_subsystems_internal(brain_t brain, neural_network_t new_base_network, uint32_t new_neuron_count)
{
    if (!brain || !new_base_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_resize_update_subsystems_internal: required parameter is NULL (brain, new_base_network)");
        return false;
    }

    // Destroy and recreate glial integration (if glial system exists).
    // Uses the factory destroy/init pair so astrocyte/oligo/microglia
    // networks are freed + rebuilt scaled to the new neuron count.
    if (brain->glial) {
        bool enable_glial = brain->config.enable_glial;
        LOG_INFO(LOG_MODULE, "brain_resize: Destroying old glial integration system");

        // Destroy integration + all three glial networks (G1/G2 additions).
        nimcp_brain_factory_destroy_glial_subsystem(brain);

        if (enable_glial) {
            LOG_INFO(LOG_MODULE,
                     "brain_resize: Recreating glial integration for %u neurons",
                     new_neuron_count);

            // Recreate glial integration with new network (still manual because
            // factory init reads adaptive_network_get_base_network(brain->network);
            // here we have the new_base_network in hand from the caller).
            brain->glial = glial_integration_create(new_base_network, 1000);

            if (brain->glial) {
                LOG_INFO(LOG_MODULE, "brain_resize: Glial integration recreated");

                // Rebuild the three glial networks scaled to new neuron count.
                uint32_t n_astro = brain->config.num_astrocytes      > 0
                                   ? brain->config.num_astrocytes      : (new_neuron_count / 5);
                uint32_t n_oligo = brain->config.num_oligodendrocytes > 0
                                   ? brain->config.num_oligodendrocytes : (new_neuron_count / 7);
                uint32_t n_micro = brain->config.num_microglia       > 0
                                   ? brain->config.num_microglia       : (new_neuron_count / 10);
                if (n_astro > new_neuron_count) n_astro = new_neuron_count;
                if (n_oligo > new_neuron_count) n_oligo = new_neuron_count;
                if (n_micro > new_neuron_count) n_micro = new_neuron_count;
                if (n_astro < 1) n_astro = 1;
                if (n_oligo < 1) n_oligo = 1;
                if (n_micro < 1) n_micro = 1;

                brain->astrocyte_network       = astrocyte_network_create(n_astro);
                brain->oligodendrocyte_network = oligodendrocyte_network_create(n_oligo);
                brain->microglia_network       = microglia_network_create(n_micro);

                if (brain->astrocyte_network) {
                    glial_integration_set_astrocyte_network(brain->glial, brain->astrocyte_network);
                }
                if (brain->oligodendrocyte_network) {
                    glial_integration_set_oligodendrocyte_network(brain->glial, brain->oligodendrocyte_network);
                }
                if (brain->microglia_network) {
                    glial_integration_set_microglia_network(brain->glial, brain->microglia_network);
                }
                glial_integration_set_astrocyte_modulation_enabled(brain->glial, true);
                glial_integration_set_oligodendrocyte_myelination_enabled(brain->glial, true);
                glial_integration_set_microglia_pruning_enabled(brain->glial,
                    brain->config.enable_microglia_pruning);

                // Recreate spatial neuromodulator system
                bool enabled_types[NEUROMOD_COUNT] = {true, true, true, true};
                spatial_neuromod_config_t configs[NEUROMOD_COUNT];
                configs[0] = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
                configs[1] = spatial_neuromod_default_config(NEUROMOD_SEROTONIN);
                configs[2] = spatial_neuromod_default_config(NEUROMOD_ACETYLCHOLINE);
                configs[3] = spatial_neuromod_default_config(NEUROMOD_NOREPINEPHRINE);

                spatial_neuromod_system_t* new_spatial = spatial_neuromod_system_create(
                    new_base_network, enabled_types, configs);
                if (new_spatial) {
                    brain->glial->spatial_neuromod = new_spatial;
                } else {
                    LOG_WARN(LOG_MODULE,
                             "brain_resize: spatial_neuromod recreation failed");
                }

                // C1 FIX mirror: publish the new glial_integration pointer
                // onto the resized base network so the forward pass sees it.
                {
                    extern bool neural_network_set_glial_integration(
                        neural_network_t, void*);
                    (void)neural_network_set_glial_integration(
                        new_base_network, brain->glial);
                }

                // Populate lookup tables for new size.
                nimcp_brain_attach_glial(brain);
            } else {
                LOG_WARN(LOG_MODULE, "brain_resize: glial_integration_create failed");
                brain->config.enable_glial = false;
            }
        }
    }

    // Update brain oscillations network reference (if oscillations exist)
    // Brain oscillations doesn't store network directly, it queries from brain
    // No update needed

    return true;
}
