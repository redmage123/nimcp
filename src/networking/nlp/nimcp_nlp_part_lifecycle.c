// nimcp_nlp_part_lifecycle.c - lifecycle functions
// Part of nimcp_nlp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_nlp.c


//=============================================================================
// Initialization
//=============================================================================

/* One-time init routine called via nimcp_once() */
static void nlp_global_init_once(void) {
    if (nimcp_mutex_init(&g_nlp_global_mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to init global mutex");
        return;
    }

    // Register with BBB
    bbb_register_module(NLP_MODULE_NAME, BBB_MODULE_TYPE_NETWORK);

    // Bio-router registration (will set up handlers)
    nlp_register_bio_async(NULL);

    g_nlp_initialized = true;
    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Neural Link Protocol initialized");
}


static bool nlp_global_init(void) {
    nimcp_once(&g_nlp_once, nlp_global_init_once);
    return g_nlp_initialized;
}


//=============================================================================
// Node Lifecycle
//=============================================================================

nlp_node_t nlp_node_create(const nlp_config_t* config) {
    if (!nlp_global_init()) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Global init failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_node_create: nlp_global_init is NULL");
        return NULL;
    }

    nlp_node_t node = (nlp_node_t)nimcp_calloc(1, sizeof(struct nlp_node_struct));
    if (!node) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to allocate node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    // Apply configuration
    if (config) {
        memcpy(&node->config, config, sizeof(nlp_config_t));
    } else {
        node->config = nlp_config_default();
    }

    node->magic = NLP_NODE_MAGIC;  // Set validation magic number
    node->brain_id = node->config.brain_id;
    node->is_master = node->config.is_master;
    node->current_mode = node->config.default_mode;
    node->emcon_level = node->config.initial_emcon;
    node->running = false;
    node->socket_fd = -1;

    // Copy PSK slots from config
    memcpy(node->psk_slots, node->config.psk, sizeof(node->psk_slots));

    /* BUG-19 fix: Track which mutexes have been successfully initialized so we
     * only destroy those on partial failure. Calling nlp_node_destroy on a
     * partially initialized node would destroy uninitialized mutexes (UB). */
    uint8_t mutex_init_count = 0;
    nimcp_mutex_t* mutex_order[] = {
        &node->state_mutex, &node->peer_mutex, &node->key_mutex,
        &node->env_mutex, &node->stats_mutex, &node->seq_mutex,
        &node->burst_mutex, &node->queue_mutex
    };
    for (int m = 0; m < 8; m++) {
        if (nimcp_mutex_init(mutex_order[m], NULL) != NIMCP_SUCCESS) {
            NIMCP_LOGGING_ERROR(NLP_MODULE_NAME,
                "Failed to init mutex %d of 8", m);
            /* Destroy only the mutexes we successfully initialized */
            for (int j = m - 1; j >= 0; j--) {
                nimcp_mutex_destroy(mutex_order[j]);
            }
            nimcp_free(node);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_node_create: mutex init failed");
            return NULL;
        }
        mutex_init_count++;
    }
    (void)mutex_init_count;  /* Used for documentation; all 8 initialized if we get here */

    // Allocate stealth burst buffer
    node->burst_buffer = (uint8_t*)nimcp_malloc(
        NLP_STEALTH_PACKET_SIZE * 64);  // Buffer for 64 messages
    if (!node->burst_buffer) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to allocate burst buffer");
        nlp_node_destroy(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_node_create: node->burst_buffer is NULL");
        return NULL;
    }
    node->burst_buffer_size = NLP_STEALTH_PACKET_SIZE * 64;

    // Initialize cryptographic subsystem
    if (nlp_crypto_init(node) != 0) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to initialize crypto subsystem");
        nlp_node_destroy(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nlp_node_create: validation failed");
        return NULL;
    }

    // Bio-async integration - use existing global context or register new
    if (g_nlp_bio_ctx) {
        node->bio_module_ctx = g_nlp_bio_ctx;
    } else if (bio_router_is_initialized()) {
        // Register now that we have a node context
        nlp_register_bio_async(node);
    }

    node->user_data = node->config.user_data;

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Node created: brain_id=0x%08X master=%d mode=%s",
                   node->brain_id, node->is_master, nlp_mode_name(node->current_mode));

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "node_created",
                  "brain_id=0x%08X", node->brain_id);

    return node;
}


void nlp_node_destroy(nlp_node_t node) {
    if (!node) return;

    // Stop if running
    if (node->running) {
        nlp_node_stop(node);
    }

    /* BUG-18 fix: Zero all peer session keys before destroying the node to
     * prevent sensitive key material from lingering in freed memory. */
    nimcp_mutex_lock(&node->peer_mutex);
    for (uint32_t i = 0; i < node->peer_count; i++) {
        volatile uint8_t* vp = (volatile uint8_t*)node->peers[i].session_key;
        for (size_t k = 0; k < sizeof(node->peers[i].session_key); k++) {
            vp[k] = 0;
        }
    }
    nimcp_mutex_unlock(&node->peer_mutex);

    // Clean up crypto subsystem
    nlp_crypto_shutdown(node);

    // Clean up bio-async context if any
    node->bio_module_ctx = NULL;

    // Free burst buffer
    if (node->burst_buffer) {
        nimcp_free(node->burst_buffer);
    }

    // Free pending messages
    for (uint32_t i = 0; i < 256; i++) {
        if (node->pending_messages[i]) {
            if (node->pending_messages[i]->payload) {
                nimcp_free(node->pending_messages[i]->payload);
            }
            nimcp_free(node->pending_messages[i]);
        }
    }

    // Destroy mutexes
    nimcp_mutex_destroy(&node->state_mutex);
    nimcp_mutex_destroy(&node->peer_mutex);
    nimcp_mutex_destroy(&node->key_mutex);
    nimcp_mutex_destroy(&node->env_mutex);
    nimcp_mutex_destroy(&node->stats_mutex);
    nimcp_mutex_destroy(&node->seq_mutex);
    nimcp_mutex_destroy(&node->burst_mutex);
    nimcp_mutex_destroy(&node->queue_mutex);

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "node_destroyed",
                  "brain_id=0x%08X", node->brain_id);

    nimcp_free(node);
}


void nlp_reset_stats(nlp_node_t node) {
    if (!node) return;

    nimcp_mutex_lock(&node->stats_mutex);
    memset(&node->stats, 0, sizeof(nlp_stats_t));
    node->stats.current_mode = node->current_mode;
    node->stats.current_emcon = node->emcon_level;
    nimcp_mutex_unlock(&node->stats_mutex);
}
