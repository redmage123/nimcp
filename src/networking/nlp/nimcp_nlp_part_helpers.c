// nimcp_nlp_part_helpers.c - helpers functions
// Part of nimcp_nlp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_nlp.c


/**
 * @brief Thread-safe check if threads should continue running
 * @param node NLP node
 * @return true if threads should continue, false if shutdown requested
 */
static inline bool nlp_should_continue(nlp_node_t node) {
    nimcp_mutex_lock(&node->state_mutex);
    bool running = node->threads_running;
    nimcp_mutex_unlock(&node->state_mutex);
    return running;
}


/**
 * @brief Thread-safe check if node is in stealth mode and running
 * @param node NLP node
 * @return true if in stealth mode and running
 */
static inline bool nlp_stealth_active(nlp_node_t node) {
    nimcp_mutex_lock(&node->state_mutex);
    bool active = node->threads_running && node->current_mode == NLP_MODE_STEALTH;
    nimcp_mutex_unlock(&node->state_mutex);
    return active;
}


/**
 * @brief Register NLP with bio-router
 */
static void nlp_register_bio_async(nlp_node_t node) {
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
            "Bio-router not initialized, skipping bio-async registration");
        return;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_NLP,
        .module_name = NLP_MODULE_NAME,
        .inbox_capacity = 64,
        .user_data = node
    };

    g_nlp_bio_ctx = bio_router_register_module(&info);
    if (g_nlp_bio_ctx) {
        if (node) {
            node->bio_module_ctx = g_nlp_bio_ctx;
        }

        /* KG-Driven Wiring: Register callback for orchestrator to invoke
         * When orchestrator starts, it discovers HANDLES_MESSAGE relations
         * from the KG and invokes this callback with the message types */
        nimcp_error_t cb_result = bio_router_register_wiring_callback(
            BIO_MODULE_NLP,
            (void*)nlp_wiring_handler_callback,
            node
        );

        if (cb_result != NIMCP_SUCCESS) {
            /* Fallback: Direct registration if orchestrator not available
             * This ensures backward compatibility with non-KG systems */
            LEGACY_HANDLER_REGISTRATION(
                bio_router_register_handler(g_nlp_bio_ctx, BIO_MSG_HEALTH_CHECK, nlp_bio_handler)
            );
            LEGACY_HANDLER_REGISTRATION(
                bio_router_register_handler(g_nlp_bio_ctx, BIO_MSG_ATTENTION_SHIFT, nlp_bio_handler)
            );
            LEGACY_HANDLER_REGISTRATION(
                bio_router_register_handler(g_nlp_bio_ctx, BIO_MSG_SECURITY_ALERT, nlp_bio_handler)
            );
            LEGACY_HANDLER_REGISTRATION(
                bio_router_register_handler(g_nlp_bio_ctx, BIO_MSG_SHUTDOWN_REQUEST, nlp_bio_handler)
            );

            // Also register for NLP-specific messages
            bio_router_register_category_handler(g_nlp_bio_ctx, 0x0A00, nlp_bio_handler);

            NIMCP_LOGGING_INFO(NLP_MODULE_NAME,
                "Registered with bio-router as module 0x%04X (legacy direct registration)",
                BIO_MODULE_NLP);
        } else {
            NIMCP_LOGGING_INFO(NLP_MODULE_NAME,
                "Registered with bio-router as module 0x%04X (KG-driven wiring callback)",
                BIO_MODULE_NLP);
        }

        bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "bio_async_registered",
                      "module_id=0x%04X", BIO_MODULE_NLP);
    }
}


static void nlp_auto_mode_check(nlp_node_t node) {
    nimcp_mutex_lock(&node->env_mutex);
    nlp_environment_t env = node->environment;
    nimcp_mutex_unlock(&node->env_mutex);

    nlp_mode_t suggested_mode = node->current_mode;
    const char* reason = NULL;

    // Check for degraded conditions requiring tactical mode
    if (env.packet_loss_rate > 0.3F ||
        env.jamming_events > 0 ||
        !env.master_reachable ||
        env.master_timeout_ms > 30000) {
        suggested_mode = NLP_MODE_TACTICAL;
        reason = "degraded network conditions";
    }

    // Check for stealth requirements
    if (env.rf_anomaly_detected ||
        env.unknown_peer_contact ||
        env.replay_attempt_detected) {
        suggested_mode = NLP_MODE_STEALTH;
        reason = "security threat detected";
    }

    // Check for power conservation
    if (env.low_power_mode || env.battery_percent < 20.0F) {
        if (node->current_mode == NLP_MODE_STANDARD) {
            suggested_mode = NLP_MODE_TACTICAL;  // Less overhead
            reason = "low power mode";
        }
    }

    // Check if we can return to standard mode
    if (node->current_mode != NLP_MODE_STANDARD &&
        env.packet_loss_rate < 0.1F &&
        env.master_reachable &&
        !env.rf_anomaly_detected &&
        env.battery_percent > 50.0F) {
        suggested_mode = NLP_MODE_STANDARD;
        reason = "conditions improved";
    }

    // Apply mode change if different
    if (suggested_mode != node->current_mode) {
        nlp_mode_t old_mode = node->current_mode;
        node->current_mode = suggested_mode;

        NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Auto mode switch: %s -> %s (%s)",
                       nlp_mode_name(old_mode), nlp_mode_name(suggested_mode), reason);

        if (node->mode_callback) {
            node->mode_callback(node, old_mode, suggested_mode, reason, node->user_data);
        }
    }
}


//=============================================================================
// Thread Functions
//=============================================================================

static void* nlp_recv_thread(void* arg) {
    nlp_node_t node = (nlp_node_t)arg;
    uint8_t buffer[65536];
    struct sockaddr_in from;
    socklen_t from_len;

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Receive thread started");

    while (nlp_should_continue(node)) {
        from_len = sizeof(from);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(node->socket_fd, &read_fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        int ready = select(node->socket_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ready <= 0) continue;

        ssize_t len = recvfrom(node->socket_fd, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&from, &from_len);

        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (!nlp_should_continue(node)) break;
            NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "recvfrom error: %s", strerror(errno));
            continue;
        }

        // Minimum size is header only for unencrypted handshake messages
        // Encrypted messages need header + auth tag (NLP_MIN_MESSAGE_SIZE)
        if (len < NLP_HEADER_SIZE) {
            NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Message too short: %zd", len);
            continue;
        }

        // Process message
        nlp_process_message(node, buffer, len, &from);
    }

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Receive thread stopped");
    return NULL;
}


static void* networking_nlp_heartbeat_thread(void* arg) {
    nlp_node_t node = (nlp_node_t)arg;

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Heartbeat thread started");

    while (nlp_should_continue(node)) {
        usleep(node->config.heartbeat_interval_ms * NIMCP_US_PER_MS);

        if (!nlp_should_continue(node)) break;

        uint64_t now = nimcp_platform_time_monotonic_ms();

        nimcp_mutex_lock(&node->peer_mutex);
        for (uint32_t i = 0; i < node->peer_count; i++) {
            nlp_peer_t* peer = &node->peers[i];

            if (peer->session_state != NLP_SESSION_ESTABLISHED) continue;

            // Check for timeout
            if (now - peer->last_seen_ms > node->config.session_timeout_ms) {
                peer->missed_heartbeats++;
                if (peer->missed_heartbeats >= 3) {
                    peer->healthy = false;
                    NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Peer 0x%08X unhealthy", peer->peer_id);
                }
            }

            // Send heartbeat
            if (now - peer->last_sent_ms > node->config.heartbeat_interval_ms) {
                nimcp_mutex_unlock(&node->peer_mutex);
                nlp_send(node, peer->peer_id, NLP_MSG_HEARTBEAT, NULL, 0,
                        NLP_PRIORITY_LOW);
                nimcp_mutex_lock(&node->peer_mutex);
                peer->last_sent_ms = now;
            }
        }
        nimcp_mutex_unlock(&node->peer_mutex);

        // Bio-async processing done via external router if registered
        // (node->bio_module_ctx managed by bio-router)
    }

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Heartbeat thread stopped");
    return NULL;
}


static void* nlp_stealth_thread(void* arg) {
    nlp_node_t node = (nlp_node_t)arg;

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Stealth thread started");

    while (nlp_stealth_active(node)) {
        uint64_t now = nimcp_platform_time_monotonic_ms();

        // Check if it's time for a burst
        if (now >= node->next_burst_time) {
            nimcp_mutex_lock(&node->burst_mutex);

            if (node->burst_buffer_used > 0 &&
                node->emcon_level != NLP_EMCON_RECEIVE &&
                node->emcon_level != NLP_EMCON_SILENT) {

                // Send buffered messages as a burst
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Burst TX: %zu bytes",
                               node->burst_buffer_used);

                // Broadcast burst to all peers
                nimcp_mutex_lock(&node->peer_mutex);
                for (uint32_t i = 0; i < node->peer_count; i++) {
                    if (node->peers[i].session_state == NLP_SESSION_ESTABLISHED) {
                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(node->peers[i].port);
                        inet_pton(AF_INET, node->peers[i].address, &addr.sin_addr);

                        sendto(node->socket_fd, node->burst_buffer,
                               node->burst_buffer_used, 0,
                               (struct sockaddr*)&addr, sizeof(addr));
                    }
                }
                nimcp_mutex_unlock(&node->peer_mutex);

                node->burst_buffer_used = 0;
            }

            // Generate chaff if needed
            if (node->emcon_level == NLP_EMCON_NORMAL) {
                // Random chaff to mask traffic patterns
                nlp_message_t* chaff = nlp_message_create_chaff(node->brain_id, 0);
                if (chaff) {
                    // Optionally send chaff - don't actually send in reduced EMCON
                    nlp_message_destroy(chaff);
                }
            }

            nimcp_mutex_unlock(&node->burst_mutex);

            node->next_burst_time = now + node->config.burst_interval_s * NIMCP_MS_PER_SEC;
        }

        usleep(100000);  // 100ms
    }

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Stealth thread stopped");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_stealth_thread: operation failed");
    return NULL;
}


//=============================================================================
// Internal Functions
//=============================================================================

static int nlp_send_raw(nlp_node_t node, uint32_t peer_id,
                        const uint8_t* data, size_t len) {
    // Find peer address
    struct sockaddr_in addr;
    bool found = false;

    nimcp_mutex_lock(&node->peer_mutex);
    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (node->peers[i].peer_id == peer_id) {
            addr.sin_family = AF_INET;
            addr.sin_port = htons(node->peers[i].port);
            inet_pton(AF_INET, node->peers[i].address, &addr.sin_addr);
            found = true;
            break;
        }
    }
    nimcp_mutex_unlock(&node->peer_mutex);

    if (!found) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Peer not found: 0x%08X", peer_id);
        return -ENOENT;
    }

    ssize_t sent = sendto(node->socket_fd, data, len, 0,
                          (struct sockaddr*)&addr, sizeof(addr));

    if (sent < 0) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "sendto failed: %s", strerror(errno));
        return -errno;
    }

    return 0;
}
