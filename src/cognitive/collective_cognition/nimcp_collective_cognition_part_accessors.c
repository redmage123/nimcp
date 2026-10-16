// nimcp_collective_cognition_part_accessors.c - accessors functions
// Part of nimcp_collective_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_collective_cognition.c


/*=============================================================================
 * Configuration API
 *===========================================================================*/

collective_cognition_config_t collective_cognition_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_default_config", 0.0f);


    collective_cognition_config_t config = {
        .hyperscanning = hyperscanning_default_config(),
        .extended_mind = extended_mind_default_config(),
        .phi = collective_phi_default_config(),
        .intentionality = shared_intentionality_default_config(),

        .max_instances = COLLECTIVE_MAX_INSTANCES,
        .fragmentation_threshold = 0.3f,
        .overload_threshold = 1.5f,
        .enable_auto_balancing = true,
        .enable_bio_async = true,
        .update_interval_ms = 50
    };
    return config;
}


hyperscanning_config_t hyperscanning_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_hyperscanning_defaul", 0.0f);


    hyperscanning_config_t config = {
        .max_instances = COLLECTIVE_MAX_INSTANCES,
        .sync_threshold = 0.7f,
        .sample_rate_hz = 100,
        .enable_leader_detection = true,
        .enable_bio_async = true
    };
    return config;
}


extended_mind_config_t extended_mind_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_extended_mind_defaul", 0.0f);


    extended_mind_config_t config = {
        .max_extensions = COLLECTIVE_MAX_EXTENSIONS,
        .trust_decay_rate = 0.1f,
        .integration_threshold = 0.8f,
        .enable_automatic_offload = true,
        .enable_bio_async = true
    };
    return config;
}


collective_phi_config_t collective_phi_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_collective_phi_defau", 0.0f);


    collective_phi_config_t config = {
        .aggregation_method = 3,  /* SYNERGISTIC */
        .synergy_coefficient = 0.5f,
        .min_instances_for_phi = 2,
        .coherence_weight = 0.3f,
        .enable_network_topology = true
    };
    return config;
}


shared_intentionality_config_t shared_intentionality_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_shared_intentionalit", 0.0f);


    shared_intentionality_config_t config = {
        .max_shared_goals = COLLECTIVE_MAX_SHARED_GOALS,
        .max_joint_attentions = COLLECTIVE_MAX_JOINT_ATTENTIONS,
        .commitment_threshold = 0.5f,
        .we_mode_threshold = 0.6f,
        .enable_role_negotiation = true,
        .enable_bio_async = true
    };
    return config;
}


uint32_t collective_cognition_instance_count(const collective_cognition_t* cc) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_instance_count", 0.0f);


    return cc ? cc->instance_count : 0;
}


bool collective_cognition_has_instance(
    const collective_cognition_t* cc,
    uint32_t instance_id
) {
    if (!cc) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_has_instance", 0.0f);


    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active && cc->instances[i].instance_id == instance_id) {
            return true;
        }
    }
    return false;
}


/*=============================================================================
 * State Query API
 *===========================================================================*/

int collective_cognition_get_state(
    const collective_cognition_t* cc,
    collective_cognition_state_t* state
) {
    if (!cc || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_get_state: required parameter is NULL (cc, state)");
        return -1;
    }

    *state = cc->state;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_state", 0.0f);


    return 0;
}


collective_consciousness_level_t collective_cognition_get_consciousness_level(
    const collective_cognition_t* cc
) {
    if (!cc) return COLLECTIVE_CONSCIOUSNESS_NONE;

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_consciousness_le", 0.0f);


    return phi_to_level(cc->state.phi.phi_total);
}


int collective_cognition_get_hyperscan_state(
    const collective_cognition_t* cc,
    hyperscan_state_t* state
) {
    if (!cc || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_get_hyperscan_state: required parameter is NULL (cc, state)");
        return -1;
    }

    *state = cc->state.hyperscanning;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_hyperscan_state", 0.0f);


    return 0;
}


int collective_cognition_get_extended_mind_state(
    const collective_cognition_t* cc,
    extended_mind_state_t* state
) {
    if (!cc || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_get_extended_mind_state: required parameter is NULL (cc, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_extended_mind_st", 0.0f);

    /* Get fresh state directly from extended_mind subsystem if available */
    if (cc->extended_mind) {
        return extended_mind_get_state((extended_mind_t*)cc->extended_mind, state);
    }

    /* Fallback to cached state */
    *state = cc->state.extended_mind;
    return 0;
}


int collective_cognition_get_phi(
    const collective_cognition_t* cc,
    collective_phi_t* phi
) {
    if (!cc || !phi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_get_phi: required parameter is NULL (cc, phi)");
        return -1;
    }

    *phi = cc->state.phi;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_phi", 0.0f);


    return 0;
}


int collective_cognition_get_we_mode(
    const collective_cognition_t* cc,
    we_mode_state_t* state
) {
    if (!cc || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_get_we_mode: required parameter is NULL (cc, state)");
        return -1;
    }

    *state = cc->state.we_mode;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_we_mode", 0.0f);


    return 0;
}


/*=============================================================================
 * Component Access API
 *===========================================================================*/

hyperscanning_t* collective_cognition_get_hyperscanning(collective_cognition_t* cc) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_hyperscanning", 0.0f);


    return cc ? (hyperscanning_t*)cc->hyperscanning : NULL;
}


extended_mind_t* collective_cognition_get_extended_mind(collective_cognition_t* cc) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_extended_mind", 0.0f);


    return cc ? (extended_mind_t*)cc->extended_mind : NULL;
}


collective_phi_system_t* collective_cognition_get_phi_system(collective_cognition_t* cc) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_phi_system", 0.0f);


    return cc ? (collective_phi_system_t*)cc->phi_system : NULL;
}


shared_intentionality_t* collective_cognition_get_intentionality(collective_cognition_t* cc) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_intentionality", 0.0f);


    return cc ? (shared_intentionality_t*)cc->intentionality : NULL;
}


bool collective_cognition_is_bio_async_connected(const collective_cognition_t* cc) {
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_is_bio_async_connect", 0.0f);


    return cc ? cc->bio_async_connected : false;
}


/*=============================================================================
 * Statistics API
 *===========================================================================*/

int collective_cognition_get_stats(
    const collective_cognition_t* cc,
    collective_cognition_stats_t* stats
) {
    if (!cc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_get_stats: required parameter is NULL (cc, stats)");
        return -1;
    }

    *stats = cc->stats;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_get_stats", 0.0f);


    return 0;
}


/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */

void collective_cognition_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_collective_cognition_instance_health_agent = agent;
    }
}
