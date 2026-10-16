// nimcp_collective_cognition_part_io.c - io functions
// Part of nimcp_collective_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_collective_cognition.c


/*=============================================================================
 * Load Balancing API
 *===========================================================================*/

int collective_cognition_balance_load(collective_cognition_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_balance_load: cc is NULL");
        return -1;
    }

    /* Find overloaded and underloaded instances */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_balance_load", 0.0f);


    int overloaded_idx = -1;
    int underloaded_idx = -1;
    float max_load = 0.0f;
    float min_load = 2.0f;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cc->instances[i].active) continue;

        float load = cc->instances[i].fatigue_level;
        if (load > max_load) {
            max_load = load;
            overloaded_idx = (int)i;
        }
        if (load < min_load) {
            min_load = load;
            underloaded_idx = (int)i;
        }
    }

    /* Balance if there's significant imbalance */
    if (overloaded_idx >= 0 && underloaded_idx >= 0 &&
        max_load - min_load > 0.3f) {
        /* Simulate load transfer */
        float transfer = (max_load - min_load) * 0.25f;
        cc->instances[overloaded_idx].fatigue_level -= transfer;
        cc->instances[underloaded_idx].fatigue_level += transfer;
        return 1;  /* One task redistributed */
    }

    return 0;
}


void collective_cognition_dump(const collective_cognition_t* cc) {
    if (!cc) {
        printf("Collective Cognition: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_dump", 0.0f);


    printf("=== Collective Cognition State ===\n");
    printf("Initialized: %s\n", cc->initialized ? "yes" : "no");
    printf("Instances: %u / %d\n", cc->instance_count, COLLECTIVE_MAX_INSTANCES);

    /* List instances */
    printf("\nRegistered Instances:\n");
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cc->instances[i].active) {
            printf("  [%u] ID=%u phi=%.3f atp=%.3f fatigue=%.3f\n",
                   i, cc->instances[i].instance_id,
                   cc->instances[i].local_phi,
                   cc->instances[i].atp_level,
                   cc->instances[i].fatigue_level);
        }
    }

    /* Hyperscanning state */
    printf("\nHyperscanning:\n");
    printf("  Global sync: %.3f\n", cc->state.hyperscanning.global_sync);
    printf("  Gamma binding: %.3f\n", cc->state.hyperscanning.gamma_binding);
    printf("  Theta emotional: %.3f\n", cc->state.hyperscanning.theta_emotional);
    printf("  Beta coordination: %.3f\n", cc->state.hyperscanning.beta_coordination);
    printf("  Entrained: %s\n", cc->state.hyperscanning.is_entrained ? "yes" : "no");
    printf("  Leader: %u (influence: %.3f)\n",
           cc->state.hyperscanning.leader_instance_id,
           cc->state.hyperscanning.leader_influence);

    /* Phi metrics */
    printf("\nCollective Phi (IIT):\n");
    printf("  Local phi: %.3f\n", cc->state.phi.phi_local);
    printf("  Network phi: %.3f\n", cc->state.phi.phi_network);
    printf("  Total phi: %.3f\n", cc->state.phi.phi_total);
    printf("  Consciousness level: %s\n",
           collective_consciousness_level_name(
               phi_to_level(cc->state.phi.phi_total)));

    /* We-mode */
    printf("\nWe-Mode (Shared Intentionality):\n");
    printf("  Strength: %.3f\n", cc->state.we_mode.we_mode_strength);
    printf("  Joint commitment: %.3f\n", cc->state.we_mode.joint_commitment);
    printf("  Mutual responsiveness: %.3f\n", cc->state.we_mode.mutual_responsiveness);
    printf("  Role understanding: %.3f\n", cc->state.we_mode.role_understanding);

    /* Extended mind */
    printf("\nExtended Mind:\n");
    printf("  Total capacity: %.3f\n", cc->state.extended_mind.total_cognitive_capacity);
    printf("  Active extensions: %u\n", cc->state.extended_mind.active_extensions);

    /* Aggregate state */
    printf("\nAggregate State:\n");
    printf("  Collective capacity: %.3f\n", cc->state.collective_capacity);
    printf("  Integration quality: %.3f\n", cc->state.integration_quality);
    printf("  Consciousness level: %.3f\n", cc->state.consciousness_level);
    printf("  Fragmented: %s\n", cc->state.is_fragmented ? "yes" : "no");
    printf("  Overloaded: %s\n", cc->state.is_overloaded ? "yes" : "no");

    /* Statistics */
    printf("\nStatistics:\n");
    printf("  Total updates: %lu\n", (unsigned long)cc->stats.total_updates);
    printf("  Instances joined: %lu\n", (unsigned long)cc->stats.instances_joined);
    printf("  Instances left: %lu\n", (unsigned long)cc->stats.instances_left);
    printf("  Entrainment events: %lu\n", (unsigned long)cc->stats.entrainment_events);
    printf("  Avg phi: %.3f, Max phi: %.3f\n", cc->stats.avg_phi, cc->stats.max_phi);
    printf("  Avg sync: %.3f\n", cc->stats.avg_sync);

    printf("\nBio-Async: %s\n", cc->bio_async_connected ? "connected" : "disconnected");
}
