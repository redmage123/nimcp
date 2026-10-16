// nimcp_plasticity_orchestrator_part_io.c - io functions
// Part of nimcp_plasticity_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_plasticity_orchestrator.c


/* ============================================================================
 * State Persistence
 * ============================================================================ */

int plasticity_orchestrator_serialize(
    const plasticity_orchestrator_t* orchestrator,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!orchestrator || !buffer || !bytes_written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in serialize");
        return -1;
    }

    /* Header: version, num_synapses, num_neurons */
    size_t required = sizeof(uint32_t) * 3 +
                     sizeof(synapse_entry_t) * orchestrator->num_synapses +
                     sizeof(neuron_entry_t) * orchestrator->num_neurons +
                     sizeof(plasticity_stats_t);

    if (buffer_size < required) {
        *bytes_written = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_orchestrator_serialize: validation failed");
        return -1;
    }

    uint8_t* ptr = buffer;

    /* Write version */
    *(uint32_t*)ptr = 1;
    ptr += sizeof(uint32_t);

    /* Write counts */
    *(uint32_t*)ptr = (uint32_t)orchestrator->num_synapses;
    ptr += sizeof(uint32_t);
    *(uint32_t*)ptr = (uint32_t)orchestrator->num_neurons;
    ptr += sizeof(uint32_t);

    /* Write synapse weights (simplified) */
    for (size_t i = 0; i < orchestrator->num_synapses; i++) {
        *(float*)ptr = orchestrator->synapses[i].weight;
        ptr += sizeof(float);
        *(uint32_t*)ptr = orchestrator->synapses[i].id;
        ptr += sizeof(uint32_t);
    }

    /* Write stats */
    memcpy(ptr, &orchestrator->stats, sizeof(plasticity_stats_t));
    ptr += sizeof(plasticity_stats_t);

    *bytes_written = ptr - buffer;
    return 0;
}


int plasticity_orchestrator_deserialize(
    plasticity_orchestrator_t* orchestrator,
    const uint8_t* buffer,
    size_t buffer_size
) {
    if (!orchestrator || !buffer || buffer_size < sizeof(uint32_t) * 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid parameters in deserialize");
        return -1;
    }

    const uint8_t* ptr = buffer;

    /* Read version */
    uint32_t version = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);
    if (version != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_orchestrator_deserialize: validation failed");
        return -1;
    }

    /* Read counts */
    uint32_t num_synapses = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);
    uint32_t num_neurons = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);

    (void)num_neurons;  /* Not used in simplified deserialization */

    /* Read synapse weights */
    nimcp_platform_mutex_lock(orchestrator->mutex);

    for (uint32_t i = 0; i < num_synapses && i < orchestrator->synapse_capacity; i++) {
        float weight = *(float*)ptr;
        ptr += sizeof(float);
        uint32_t id = *(uint32_t*)ptr;
        ptr += sizeof(uint32_t);

        synapse_entry_t* syn = get_or_create_synapse(orchestrator, id);
        if (syn) {
            syn->weight = weight;
            if (syn->triplet_stdp) {
                syn->triplet_stdp->weight = weight;
            }
        }
    }

    /* Read stats */
    if (ptr + sizeof(plasticity_stats_t) <= buffer + buffer_size) {
        memcpy(&orchestrator->stats, ptr, sizeof(plasticity_stats_t));
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}
