// nimcp_introspection_part_processing.c - processing functions
// Part of nimcp_introspection.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_introspection.c


/**
 * @brief KG-driven wiring callback for introspection module
 */
static int introspection_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data)
{
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_INTROSPECTION_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_introspection_query);
                registered++;
                break;
            default:
                LOG_DEBUG("introspection: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO(LOG_MODULE, "Introspection: registered %d handlers via KG wiring", registered);
    return 0;
}


/**
 * WHAT: Update pattern activity in registry
 * WHY: Track pattern activation over time
 * HOW: Update existing entry or create new one
 */
static void pattern_registry_update(pattern_registry_t* registry, const char* name, float activity)
{
    if (registry == NULL || name == NULL) {
        return;
    }

    /* Guard against NaN/Inf poisoning accumulated sums */
    if (!isfinite(activity)) activity = 0.0f;

    nimcp_mutex_lock(&registry->lock);

    /* WHAT: Try to find existing entry */
    pattern_entry_t* entry = pattern_registry_lookup(registry, name);

    if (entry) {
        /* WHAT: Update existing pattern */
        entry->current_activity = activity;
        entry->activity_sum += activity;
        entry->activation_count++;
        entry->last_activated = nimcp_time_monotonic_ms();
    } else {
        /* WHAT: Create new pattern entry */
        entry = (pattern_entry_t*) nimcp_calloc(1, sizeof(pattern_entry_t));
        if (entry == NULL) {
            nimcp_mutex_unlock(&registry->lock);
            return;
        }

        entry->name = nimcp_strdup(name);
        if (entry->name == NULL) {
            nimcp_free(entry);
            nimcp_mutex_unlock(&registry->lock);
            return;
        }
        entry->current_activity = activity;
        entry->activity_sum = activity;
        entry->activation_count = 1;
        entry->pattern_strength = 0.5F; /* Initial strength */
        entry->first_learned = nimcp_time_monotonic_ms();
        entry->last_activated = entry->first_learned;

        /* WHAT: Insert into hash table */
        uint32_t bucket = hash_string(name);
        entry->next = registry->buckets[bucket];
        registry->buckets[bucket] = entry;
        registry->num_patterns++;
    }

    nimcp_mutex_unlock(&registry->lock);
}


/**
 * @brief Training step - clamp progress [0,1], adapt internal parameters
 */
int introspection_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_training_step: ctx is NULL");
        return -1;
    }
    /* Clamp progress to [0,1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    introspection_heartbeat_instance(g_introspection_instance_health_agent,
                                     "introspection_training_step", progress);
    /* Drive a sample to exercise the module at each training step */
    introspection_context_t context = (introspection_context_t)ctx;
    introspection_sample_activity(context);
    return 0;
}
