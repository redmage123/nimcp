// nimcp_nlp_part_accessors.c - accessors functions
// Part of nimcp_nlp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_nlp.c


//=============================================================================
// Configuration
//=============================================================================

nlp_config_t nlp_config_default(void) {
    nlp_config_t config;
    memset(&config, 0, sizeof(config));

    config.brain_id = nlp_generate_brain_id();
    config.is_master = false;
    config.default_mode = NLP_MODE_STANDARD;
    config.auto_mode_switch = true;

    strncpy(config.bind_address, "0.0.0.0", sizeof(config.bind_address) - 1);
    config.port = 9999;
    config.max_peers = NLP_MAX_PEERS;

    config.heartbeat_interval_ms = NLP_HEARTBEAT_INTERVAL;
    config.session_timeout_ms = NLP_SESSION_TIMEOUT;
    config.handshake_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;

    config.burst_interval_s = NLP_BURST_INTERVAL_DEFAULT;
    config.initial_emcon = NLP_EMCON_NORMAL;

    config.require_encryption = true;
    config.key_rotation_interval_s = 3600;  // 1 hour

    config.user_data = NULL;

    return config;
}


int nlp_get_peer(nlp_node_t node, uint32_t peer_id, nlp_peer_t* peer) {
    if (!bbb_check_pointer(node, "nlp_get_peer") ||
        !bbb_check_pointer(peer, "nlp_get_peer")) {
        return -EINVAL;
    }

    nimcp_mutex_lock(&node->peer_mutex);

    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (node->peers[i].peer_id == peer_id) {
            memcpy(peer, &node->peers[i], sizeof(nlp_peer_t));
            nimcp_mutex_unlock(&node->peer_mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(&node->peer_mutex);
    return -ENOENT;
}


uint32_t nlp_get_peers(nlp_node_t node, nlp_peer_t* peers, uint32_t max_peers) {
    if (!bbb_check_pointer(node, "nlp_get_peers")) {
        return 0;
    }

    nimcp_mutex_lock(&node->peer_mutex);

    uint32_t count = node->peer_count;
    if (count > max_peers) count = max_peers;

    if (peers && count > 0) {
        memcpy(peers, node->peers, count * sizeof(nlp_peer_t));
    }

    nimcp_mutex_unlock(&node->peer_mutex);
    return count;
}


//=============================================================================
// Mode Control
//=============================================================================

int nlp_set_mode(nlp_node_t node, nlp_mode_t mode) {
    if (!bbb_check_pointer(node, "nlp_set_mode")) {
        return -EINVAL;
    }

    if (mode > NLP_MODE_STEALTH) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Invalid mode: %d", mode);
        return -EINVAL;
    }

    nlp_mode_t old_mode = node->current_mode;
    if (old_mode == mode) {
        return 0;  // No change
    }

    node->current_mode = mode;

    // Mode-specific initialization
    if (mode == NLP_MODE_STEALTH) {
        // Start stealth thread if not running
        if (node->running && !node->threads_running) {
            nimcp_thread_create(&node->stealth_thread, nlp_stealth_thread, node, NULL);
        }
        node->next_burst_time = nimcp_platform_time_monotonic_ms() +
                                node->config.burst_interval_s * NIMCP_MS_PER_SEC;
    }

    // Update statistics
    nimcp_mutex_lock(&node->stats_mutex);
    node->stats.mode_switches++;
    node->stats.current_mode = mode;
    nimcp_mutex_unlock(&node->stats_mutex);

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Mode changed: %s -> %s",
                   nlp_mode_name(old_mode), nlp_mode_name(mode));

    // Notify callback
    if (node->mode_callback) {
        node->mode_callback(node, old_mode, mode, "manual", node->user_data);
    }

    // Broadcast mode change to peers
    uint8_t mode_payload[4];
    mode_payload[0] = (uint8_t)old_mode;
    mode_payload[1] = (uint8_t)mode;
    mode_payload[2] = 0;
    mode_payload[3] = 0;
    nlp_broadcast(node, NLP_MSG_EMCON_CHANGE, mode_payload, 4, NLP_PRIORITY_HIGH);

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "mode_changed",
                  "old=%s new=%s", nlp_mode_name(old_mode), nlp_mode_name(mode));

    return 0;
}


nlp_mode_t nlp_get_mode(nlp_node_t node) {
    if (!node) return NLP_MODE_STANDARD;
    return node->current_mode;
}


int nlp_set_emcon(nlp_node_t node, nlp_emcon_level_t level) {
    if (!bbb_check_pointer(node, "nlp_set_emcon")) {
        return -EINVAL;
    }

    if (level > NLP_EMCON_EMERGENCY) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Invalid EMCON level: %d", level);
        return -EINVAL;
    }

    nlp_emcon_level_t old_level = node->emcon_level;
    node->emcon_level = level;

    nimcp_mutex_lock(&node->stats_mutex);
    node->stats.current_emcon = level;
    nimcp_mutex_unlock(&node->stats_mutex);

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "EMCON level changed: %s -> %s",
                   nlp_emcon_name(old_level), nlp_emcon_name(level));

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "emcon_changed",
                  "old=%s new=%s", nlp_emcon_name(old_level), nlp_emcon_name(level));

    return 0;
}


nlp_emcon_level_t nlp_get_emcon(nlp_node_t node) {
    if (!node) return NLP_EMCON_NORMAL;
    return node->emcon_level;
}


//=============================================================================
// Key Management
//=============================================================================

int nlp_set_psk(nlp_node_t node, uint8_t slot, const uint8_t* key,
                uint32_t key_id, uint64_t valid_from, uint64_t valid_until) {
    if (!bbb_check_pointer(node, "nlp_set_psk") ||
        !bbb_check_pointer(key, "nlp_set_psk")) {
        return -EINVAL;
    }

    if (slot >= NLP_KEY_SLOTS) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Invalid key slot: %d", slot);
        return -EINVAL;
    }

    nimcp_mutex_lock(&node->key_mutex);

    nlp_key_slot_t* psk = &node->psk_slots[slot];
    memcpy(psk->key, key, NLP_KEY_SIZE);
    psk->key_id = key_id;
    psk->valid_from = valid_from;
    psk->valid_until = valid_until;
    psk->active = true;

    nimcp_mutex_unlock(&node->key_mutex);

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "PSK set in slot %d, key_id=0x%08X", slot, key_id);

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "psk_set",
                  "slot=%d key_id=0x%08X", slot, key_id);

    return 0;
}


//=============================================================================
// Callbacks
//=============================================================================

void nlp_set_message_callback(nlp_node_t node, nlp_message_callback_t callback) {
    if (node) {
        node->message_callback = callback;
    }
}


void nlp_set_peer_callback(nlp_node_t node, nlp_peer_callback_t callback) {
    if (node) {
        node->peer_callback = callback;
    }
}


void nlp_set_mode_callback(nlp_node_t node, nlp_mode_callback_t callback) {
    if (node) {
        node->mode_callback = callback;
    }
}


//=============================================================================
// Statistics
//=============================================================================

int nlp_get_stats(nlp_node_t node, nlp_stats_t* stats) {
    if (!bbb_check_pointer(node, "nlp_get_stats") ||
        !bbb_check_pointer(stats, "nlp_get_stats")) {
        return -EINVAL;
    }

    nimcp_mutex_lock(&node->stats_mutex);
    memcpy(stats, &node->stats, sizeof(nlp_stats_t));
    nimcp_mutex_unlock(&node->stats_mutex);

    // Add peer count
    nimcp_mutex_lock(&node->peer_mutex);
    stats->connected_peers = 0;
    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (node->peers[i].session_state == NLP_SESSION_ESTABLISHED) {
            stats->connected_peers++;
        }
    }
    stats->active_sessions = stats->connected_peers;
    nimcp_mutex_unlock(&node->peer_mutex);

    return 0;
}
