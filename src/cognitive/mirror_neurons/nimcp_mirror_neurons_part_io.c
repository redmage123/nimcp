// nimcp_mirror_neurons_part_io.c - io functions
// Part of nimcp_mirror_neurons.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_mirror_neurons.c


//=============================================================================
// Persistence API (Save/Load) - Phase 10.11
//=============================================================================

/**
 * @brief Save mirror neuron system state to file
 *
 * WHAT: Serialize mirror neuron system to binary file
 * WHY:  Enable persistence of learned action associations and statistics
 * HOW:  Write version, config, neurons, actions, agents, and statistics
 *
 * Binary format:
 *   uint32_t version (1)
 *   mirror_neuron_config_t config
 *   uint32_t num_neurons
 *   For each neuron:
 *     mirror_neuron_unit_t neuron (excluding integration pointers)
 *   uint32_t num_actions
 *   For each action:
 *     action_mapping_t action (with neuron_indices array)
 *   uint32_t num_agents
 *   For each agent:
 *     agent_info_t agent
 *   mirror_neuron_stats_t stats
 *   uint64_t creation_time
 *   uint64_t last_update_time
 *
 * Note: Integration handles (working_memory, theory_of_mind, etc.) are not saved
 *       They must be re-established after loading
 *
 * COMPLEXITY: O(n + a + g) where n=neurons, a=actions, g=agents
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param mirror Mirror neuron system
 * @param file Open file handle for writing
 * @return true on success, false on error
 */
bool mirror_neurons_save(mirror_neurons_t mirror, FILE* file)
{
    // Guard: Validate parameters
    if (!mirror || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_save: required parameter is NULL (mirror, file)");
        return false;
    }

    // WHAT: Write version marker for backward compatibility
    // WHY:  Enable future format changes while supporting old saves
    // HOW:  Write uint32_t version = 1
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_save", 0.0f);


    uint32_t version = 1;
    if (fwrite(&version, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    // WHAT: Write configuration
    // WHY:  Restore mirror neuron behavior on load
    // HOW:  Binary write of config struct
    if (fwrite(&mirror->config, sizeof(mirror_neuron_config_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    // WHAT: Write neuron count and neurons
    // WHY:  Restore learned neuron representations
    // HOW:  Write count, then each neuron unit
    if (fwrite(&mirror->num_neurons, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        if (fwrite(&mirror->neurons[i], sizeof(mirror_neuron_unit_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
    }

    // WHAT: Write action mappings
    // WHY:  Restore action-to-neuron associations
    // HOW:  Write count, then each action with its neuron indices
    if (fwrite(&mirror->num_actions, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        action_mapping_t* action = &mirror->actions[i];

        // Write action metadata (excluding neuron_indices pointer)
        if (fwrite(&action->action_id, sizeof(uint32_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
        if (fwrite(action->action_name, sizeof(char), 64, file) != 64) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
        if (fwrite(&action->num_neurons, sizeof(uint32_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
        if (fwrite(&action->capacity, sizeof(uint32_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
        if (fwrite(&action->total_observations, sizeof(uint32_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
        if (fwrite(&action->total_executions, sizeof(uint32_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
        if (fwrite(&action->avg_similarity, sizeof(float), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }

        // Write neuron indices array
        if (action->num_neurons > 0 && action->neuron_indices) {
            if (fwrite(action->neuron_indices, sizeof(uint32_t), action->num_neurons, file) != action->num_neurons) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
                return false;
            }
        }
    }

    // WHAT: Write agent tracking data
    // WHY:  Restore multi-agent learning state
    // HOW:  Write count, then each agent info
    if (fwrite(&mirror->num_agents, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    for (uint32_t i = 0; i < mirror->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_agents > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_agents);
        }

        if (fwrite(&mirror->agents[i], sizeof(agent_info_t), 1, file) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
            return false;
        }
    }

    // WHAT: Write statistics
    // WHY:  Preserve performance metrics
    // HOW:  Binary write of stats struct
    if (fwrite(&mirror->stats, sizeof(mirror_neuron_stats_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    // WHAT: Write temporal state
    // WHY:  Restore timing information
    // HOW:  Write creation_time and last_update_time
    if (fwrite(&mirror->creation_time, sizeof(uint64_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    if (fwrite(&mirror->last_update_time, sizeof(uint64_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_save: validation failed");
        return false;
    }

    return true;
}


/**
 * @brief Load mirror neuron system state from file
 *
 * WHAT: Deserialize mirror neuron system from binary file
 * WHY:  Restore saved action associations and learning state
 * HOW:  Read version, validate, reconstruct state
 *
 * Note: Integration handles must be set separately via integration functions
 * Note: Brain reference must be set via mirror_neurons_set_brain()
 *
 * COMPLEXITY: O(n + a + g) where n=neurons, a=actions, g=agents
 * THREAD-SAFE: Yes (creates new instance)
 *
 * @param file Open file handle for reading
 * @return Mirror neuron system handle or NULL on error
 */
mirror_neurons_t mirror_neurons_load(FILE* file)
{
    // Guard: Validate parameter
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "file is NULL");

        return NULL;
    }

    // WHAT: Read and validate version
    // WHY:  Ensure format compatibility
    // HOW:  Read version, check against current version
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_load", 0.0f);


    uint32_t version = 0;
    if (fread(&version, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_load: validation failed");
        return NULL;
    }

    if (version != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_load: validation failed");
        return NULL;
    }

    // WHAT: Allocate mirror neuron system structure
    // WHY:  Need structure to hold loaded data
    // HOW:  Use nimcp_calloc for zero-initialization
    mirror_neurons_t mirror = (mirror_neurons_t)nimcp_calloc(1, sizeof(struct mirror_neurons_system));
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mirror");

        return NULL;
    }

    // WHAT: Read configuration
    // WHY:  Restore mirror neuron behavior
    // HOW:  Binary read into config struct
    if (fread(&mirror->config, sizeof(mirror_neuron_config_t), 1, file) != 1) {
        goto cleanup;
    }

    // WHAT: Read neuron count and allocate neurons
    // WHY:  Restore learned neuron representations
    // HOW:  Read count, allocate array, read each neuron
    if (fread(&mirror->num_neurons, sizeof(uint32_t), 1, file) != 1) {
        goto cleanup;
    }

    mirror->neurons = (mirror_neuron_unit_t*)nimcp_calloc(mirror->num_neurons, sizeof(mirror_neuron_unit_t));
    if (!mirror->neurons) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < mirror->num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_neurons > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_neurons);
        }

        if (fread(&mirror->neurons[i], sizeof(mirror_neuron_unit_t), 1, file) != 1) {
            goto cleanup;
        }
        /* Clear serialized pointer — substrate must be re-enabled after load */
        mirror->neurons[i].substrate = NULL;
        mirror->neurons[i].has_substrate = false;
    }

    // WHAT: Read action mappings
    // WHY:  Restore action-to-neuron associations
    // HOW:  Read count, allocate array, read each action with indices
    if (fread(&mirror->num_actions, sizeof(uint32_t), 1, file) != 1) {
        goto cleanup;
    }

    mirror->actions_capacity = mirror->num_actions;
    mirror->actions = (action_mapping_t*)nimcp_calloc(mirror->actions_capacity, sizeof(action_mapping_t));
    if (!mirror->actions) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        action_mapping_t* action = &mirror->actions[i];

        // Read action metadata
        if (fread(&action->action_id, sizeof(uint32_t), 1, file) != 1) goto cleanup;
        if (fread(action->action_name, sizeof(char), 64, file) != 64) goto cleanup;
        if (fread(&action->num_neurons, sizeof(uint32_t), 1, file) != 1) goto cleanup;
        if (fread(&action->capacity, sizeof(uint32_t), 1, file) != 1) goto cleanup;
        if (fread(&action->total_observations, sizeof(uint32_t), 1, file) != 1) goto cleanup;
        if (fread(&action->total_executions, sizeof(uint32_t), 1, file) != 1) goto cleanup;
        if (fread(&action->avg_similarity, sizeof(float), 1, file) != 1) goto cleanup;

        // Allocate and read neuron indices array
        if (action->num_neurons > 0) {
            action->neuron_indices = (uint32_t*)nimcp_calloc(action->capacity, sizeof(uint32_t));
            if (!action->neuron_indices) {
                goto cleanup;
            }

            if (fread(action->neuron_indices, sizeof(uint32_t), action->num_neurons, file) != action->num_neurons) {
                goto cleanup;
            }
        } else {
            action->neuron_indices = NULL;
        }
    }

    // WHAT: Read agent tracking data
    // WHY:  Restore multi-agent learning state
    // HOW:  Read count, allocate array, read each agent
    if (fread(&mirror->num_agents, sizeof(uint32_t), 1, file) != 1) {
        goto cleanup;
    }

    mirror->agents_capacity = mirror->num_agents;
    mirror->agents = (agent_info_t*)nimcp_calloc(mirror->agents_capacity, sizeof(agent_info_t));
    if (!mirror->agents) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < mirror->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_agents > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_agents);
        }

        if (fread(&mirror->agents[i], sizeof(agent_info_t), 1, file) != 1) {
            goto cleanup;
        }
    }

    // WHAT: Read statistics
    // WHY:  Restore performance metrics
    // HOW:  Binary read into stats struct
    if (fread(&mirror->stats, sizeof(mirror_neuron_stats_t), 1, file) != 1) {
        goto cleanup;
    }

    // WHAT: Read temporal state
    // WHY:  Restore timing information
    // HOW:  Read creation_time and last_update_time
    if (fread(&mirror->creation_time, sizeof(uint64_t), 1, file) != 1) {
        goto cleanup;
    }

    if (fread(&mirror->last_update_time, sizeof(uint64_t), 1, file) != 1) {
        goto cleanup;
    }

    // WHAT: Initialize integration handles to NULL
    // WHY:  Must be set separately by caller
    // HOW:  Zero-initialization already done by calloc
    mirror->working_memory = NULL;
    mirror->theory_of_mind = NULL;
    mirror->predictive_network = NULL;
    mirror->glial_integration = NULL;
    mirror->brain = NULL;

    // WHAT: Create mutex for thread safety (was missing - calloc zeroes it)
    // WHY:  Without mutex, any locking operation will crash (NULL deref)
    // HOW:  Same pattern as mirror_neurons_create()
    {
        mutex_attr_t mattr = {.type = MUTEX_TYPE_NORMAL};
        mirror->mutex = nimcp_mutex_create(&mattr);
        if (!mirror->mutex) {
            goto cleanup;
        }
    }

    mirror->initialized = true;

    return mirror;

cleanup:
    // WHAT: Cleanup on error
    // WHY:  Prevent memory leaks
    // HOW:  Free allocated resources
    if (mirror) {
        if (mirror->neurons) {
            nimcp_free(mirror->neurons);
        }
        if (mirror->actions) {
            for (uint32_t i = 0; i < mirror->num_actions; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
                    mirror_neurons_heartbeat("mirror_neuro_loop",
                                     (float)(i + 1) / (float)mirror->num_actions);
                }

                if (mirror->actions[i].neuron_indices) {
                    nimcp_free(mirror->actions[i].neuron_indices);
                }
            }
            nimcp_free(mirror->actions);
        }
        if (mirror->agents) {
            nimcp_free(mirror->agents);
        }
        nimcp_free(mirror);
        mirror = NULL;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_load: validation failed");
    return NULL;
}
