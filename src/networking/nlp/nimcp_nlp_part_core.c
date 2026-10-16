// nimcp_nlp_part_core.c - core functions
// Part of nimcp_nlp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_nlp.c

void nlp_broadcast_session_event(nlp_node_t node, uint64_t peer_id,
                                  bio_message_type_t event_type,
                                  uint8_t old_state, uint8_t new_state) {
    if (!g_nlp_bio_ctx) return;

    bio_msg_nlp_session_state_change_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, event_type, BIO_MODULE_NLP,
                        BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  // Alerting channel
    msg.peer_id = peer_id;
    msg.session_id = (node ? node->brain_id : 0);
    msg.old_state = old_state;
    msg.new_state = new_state;
    msg.state_change_time_us = nimcp_time_get_us();

    bio_router_broadcast(g_nlp_bio_ctx, &msg, sizeof(msg));

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
        "Bio-async: broadcast session event peer=%lu state=%u->%u",
        (unsigned long)peer_id, old_state, new_state);
}


/**
 * @brief Broadcast NLP message received event
 */
void nlp_broadcast_message_received(nlp_node_t node, uint64_t peer_id,
                                     uint32_t msg_type, uint32_t msg_size,
                                     bool encrypted, bool compressed) {
    if (!g_nlp_bio_ctx) return;

    bio_msg_nlp_message_received_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_NLP_MESSAGE_RECEIVED,
                        BIO_MODULE_NLP, BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast signaling
    msg.peer_id = peer_id;
    msg.message_type = msg_type;
    msg.message_size = msg_size;
    msg.sequence_num = 0;  // Sequence from NLP message header, not tracked in node
    msg.encrypted = encrypted;
    msg.compressed = compressed;

    bio_router_broadcast(g_nlp_bio_ctx, &msg, sizeof(msg));
}


/**
 * @brief Broadcast NLP mode change event
 */
void nlp_broadcast_mode_change(nlp_node_t node, uint8_t old_mode,
                                uint8_t new_mode, uint32_t reason) {
    if (!g_nlp_bio_ctx) return;

    bio_msg_nlp_mode_change_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_NLP_PROTOCOL_MODE_CHANGE,
                        BIO_MODULE_NLP, BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_SEROTONIN;  // State change
    msg.old_mode = old_mode;
    msg.new_mode = new_mode;
    msg.peer_id = 0;  // Applies to all
    msg.reason = reason;

    bio_router_broadcast(g_nlp_bio_ctx, &msg, sizeof(msg));

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "mode_change_broadcast",
                  "old=%u new=%u reason=%u", old_mode, new_mode, reason);
}


/**
 * @brief Broadcast NLP error event
 */
void nlp_broadcast_error(uint64_t peer_id, uint32_t error_code,
                          uint8_t severity, const char* message) {
    if (!g_nlp_bio_ctx) return;

    bio_msg_nlp_error_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_NLP_ERROR,
                        BIO_MODULE_NLP, BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  // Alert
    msg.peer_id = peer_id;
    msg.error_code = error_code;
    msg.severity = severity;
    strncpy(msg.module_name, NLP_MODULE_NAME, sizeof(msg.module_name) - 1);
    msg.module_name[sizeof(msg.module_name) - 1] = '\0';
    if (message) {
        strncpy(msg.error_message, message, sizeof(msg.error_message) - 1);
        msg.error_message[sizeof(msg.error_message) - 1] = '\0';
    }

    bio_router_broadcast(g_nlp_bio_ctx, &msg, sizeof(msg));

    if (severity >= 2) {  // error or critical
        bbb_audit_log(BBB_AUDIT_WARNING, NLP_MODULE_NAME, "error_broadcast",
                      "peer=%lu code=%u severity=%u: %s",
                      (unsigned long)peer_id, error_code, severity,
                      message ? message : "");
    }
}


int nlp_node_start(nlp_node_t node) {
    if (!bbb_check_pointer(node, "nlp_node_start")) {
        return -EINVAL;
    }

    if (node->running) {
        NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Node already running");
        return 0;
    }

    // Create UDP socket
    node->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (node->socket_fd < 0) {
        /* BUG-30 fix: Save errno before logging macro, which may clobber it
         * via internal write/printf calls. */
        int saved_errno = errno;
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to create socket: %s", strerror(saved_errno));
        return -saved_errno;
    }

    // Set non-blocking
    /* BUG-25 fix: Check fcntl return values. If F_GETFL fails (returns -1),
     * F_SETFL would be called with -1 | O_NONBLOCK == -1 (all bits set),
     * setting garbage flags on the socket descriptor. */
    int flags = fcntl(node->socket_fd, F_GETFL, 0);
    if (flags < 0) {
        int saved_errno = errno;
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to get socket flags: %s", strerror(saved_errno));
        close(node->socket_fd);
        node->socket_fd = -1;
        return -saved_errno;
    }
    if (fcntl(node->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to set non-blocking: %s", strerror(saved_errno));
        close(node->socket_fd);
        node->socket_fd = -1;
        return -saved_errno;
    }

    // Allow address reuse
    int optval = 1;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Bind socket
    memset(&node->bind_addr, 0, sizeof(node->bind_addr));
    node->bind_addr.sin_family = AF_INET;
    node->bind_addr.sin_port = htons(node->config.port);
    inet_pton(AF_INET, node->config.bind_address, &node->bind_addr.sin_addr);

    if (bind(node->socket_fd, (struct sockaddr*)&node->bind_addr,
             sizeof(node->bind_addr)) < 0) {
        /* BUG-26 fix: Save errno before close() and logging, which can both
         * modify errno. Returning -errno after these calls would return the
         * wrong error code. */
        int saved_errno = errno;
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to bind socket: %s", strerror(saved_errno));
        close(node->socket_fd);
        node->socket_fd = -1;
        return -saved_errno;
    }

    nimcp_mutex_lock(&node->state_mutex);
    node->running = true;
    node->threads_running = true;
    nimcp_mutex_unlock(&node->state_mutex);

    // Start receive thread
    if (nimcp_thread_create(&node->recv_thread, nlp_recv_thread, node, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to create recv thread");
        nimcp_mutex_lock(&node->state_mutex);
        node->running = false;
        node->threads_running = false;
        nimcp_mutex_unlock(&node->state_mutex);
        close(node->socket_fd);
        return -ENOMEM;
    }

    // Start heartbeat thread
    if (nimcp_thread_create(&node->heartbeat_thread, networking_nlp_heartbeat_thread, node, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Failed to create heartbeat thread");
        nimcp_mutex_lock(&node->state_mutex);
        node->running = false;
        node->threads_running = false;
        nimcp_mutex_unlock(&node->state_mutex);
        nimcp_thread_join(node->recv_thread, NULL);
        close(node->socket_fd);
        return -ENOMEM;
    }

    // Start stealth thread if in stealth mode
    nimcp_mutex_lock(&node->state_mutex);
    bool start_stealth = (node->current_mode == NLP_MODE_STEALTH);
    nimcp_mutex_unlock(&node->state_mutex);
    if (start_stealth) {
        if (nimcp_thread_create(&node->stealth_thread, nlp_stealth_thread, node, NULL) != NIMCP_SUCCESS) {
            NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Failed to create stealth thread");
        }
    }

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Node started on %s:%d",
                   node->config.bind_address, node->config.port);

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "node_started",
                  "port=%d mode=%s", node->config.port, nlp_mode_name(node->current_mode));

    return 0;
}


int nlp_node_stop(nlp_node_t node) {
    if (!bbb_check_pointer(node, "nlp_node_stop")) {
        return -EINVAL;
    }

    nimcp_mutex_lock(&node->state_mutex);
    if (!node->running) {
        nimcp_mutex_unlock(&node->state_mutex);
        return 0;
    }

    node->running = false;
    node->threads_running = false;
    bool was_stealth = (node->current_mode == NLP_MODE_STEALTH);
    nimcp_mutex_unlock(&node->state_mutex);

    // Wake up threads
    if (node->socket_fd >= 0) {
        shutdown(node->socket_fd, SHUT_RDWR);
    }

    // Wait for threads
    nimcp_thread_join(node->recv_thread, NULL);
    nimcp_thread_join(node->heartbeat_thread, NULL);
    if (was_stealth) {
        nimcp_thread_join(node->stealth_thread, NULL);
    }

    // Close socket
    if (node->socket_fd >= 0) {
        close(node->socket_fd);
        node->socket_fd = -1;
    }

    // Disconnect all peers
    nimcp_mutex_lock(&node->peer_mutex);
    for (uint32_t i = 0; i < node->peer_count; i++) {
        /* BUG-18 related: Zero session key material on stop */
        volatile uint8_t* vp = (volatile uint8_t*)node->peers[i].session_key;
        for (size_t k = 0; k < sizeof(node->peers[i].session_key); k++) {
            vp[k] = 0;
        }
        node->peers[i].session_state = NLP_SESSION_DISCONNECTED;
    }
    node->peer_count = 0;
    nimcp_mutex_unlock(&node->peer_mutex);

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Node stopped");

    bbb_audit_log(BBB_AUDIT_INFO, NLP_MODULE_NAME, "node_stopped",
                  "brain_id=0x%08X", node->brain_id);

    return 0;
}


//=============================================================================
// Peer Management
//=============================================================================

uint32_t nlp_connect_peer(nlp_node_t node, const char* address, uint16_t port) {
    if (!bbb_check_pointer(node, "nlp_connect_peer") ||
        !bbb_check_string(address, 64, "nlp_connect_peer")) {
        return 0;
    }

    nimcp_mutex_lock(&node->peer_mutex);

    // Check if already connected
    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (strcmp(node->peers[i].address, address) == 0 &&
            node->peers[i].port == port) {
            nimcp_mutex_unlock(&node->peer_mutex);
            return node->peers[i].peer_id;
        }
    }

    // Check capacity
    if (node->peer_count >= node->config.max_peers) {
        NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Max peers reached");
        nimcp_mutex_unlock(&node->peer_mutex);
        return 0;
    }

    // Add new peer
    nlp_peer_t* peer = &node->peers[node->peer_count];
    memset(peer, 0, sizeof(nlp_peer_t));

    // Generate peer ID from address hash
    uint32_t hash = 5381;
    for (const char* p = address; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    hash ^= port;
    peer->peer_id = hash;

    strncpy(peer->address, address, sizeof(peer->address) - 1);
    peer->port = port;
    peer->session_state = NLP_SESSION_DISCONNECTED;
    peer->healthy = false;

    uint64_t now = nimcp_platform_time_monotonic_ms();
    peer->last_seen_ms = now;
    peer->last_sent_ms = now;

    node->peer_count++;

    /* BUG-4 fix: Copy needed peer fields to local variables BEFORE unlocking
     * peer_mutex. After unlock, the peers array can be modified by other threads
     * (disconnect, new connect causing realloc, etc.), making `peer` a dangling
     * pointer. */
    uint32_t local_peer_id = peer->peer_id;
    nlp_session_state_t local_session_state = peer->session_state;

    nimcp_mutex_unlock(&node->peer_mutex);

    // Initiate handshake based on mode
    if (node->current_mode == NLP_MODE_STANDARD) {
        // Start 3-way handshake - transition state first
        // CRIT-1 fix: Re-acquire peer_mutex and re-find peer by ID before
        // calling nlp_session_start_handshake. The `peer` pointer captured
        // above may be stale if another thread called nlp_disconnect_peer
        // (which does memmove on the peers array) between the unlock and here.
        nimcp_mutex_lock(&node->peer_mutex);
        nlp_peer_t* hs_peer = NULL;
        for (uint32_t i = 0; i < node->peer_count; i++) {
            if (node->peers[i].peer_id == local_peer_id) {
                hs_peer = &node->peers[i];
                break;
            }
        }
        if (hs_peer) {
            nlp_session_start_handshake(node, hs_peer);
        }
        nimcp_mutex_unlock(&node->peer_mutex);

        if (!hs_peer) {
            // Peer was removed before handshake could start
            NIMCP_LOGGING_WARN(NLP_MODULE_NAME,
                "Peer 0x%08X removed before handshake", local_peer_id);
            return local_peer_id;
        }

        // Actually send the handshake request message over the network
        int send_rc = nlp_send(node, local_peer_id, NLP_MSG_HANDSHAKE_REQ,
                               NULL, 0, NLP_PRIORITY_HIGH);
        if (send_rc < 0) {
            NIMCP_LOGGING_WARN(NLP_MODULE_NAME,
                "Failed to send handshake request to peer 0x%08X: %d",
                local_peer_id, send_rc);
            // Don't fail peer creation - peer will retry or timeout
        } else {
            NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                "Sent handshake request to peer 0x%08X", local_peer_id);
        }
    } else {
        // Tactical/Stealth: use PSK, mark as established immediately
        nimcp_mutex_lock(&node->peer_mutex);
        // Re-find peer under lock (it may have been removed)
        nlp_peer_t* found_peer = NULL;
        for (uint32_t i = 0; i < node->peer_count; i++) {
            if (node->peers[i].peer_id == local_peer_id) {
                found_peer = &node->peers[i];
                break;
            }
        }
        if (found_peer) {
            found_peer->session_state = NLP_SESSION_ESTABLISHED;
            local_session_state = NLP_SESSION_ESTABLISHED;

            // Select active PSK
            nimcp_mutex_lock(&node->key_mutex);
            for (int i = 0; i < NLP_KEY_SLOTS; i++) {
                if (node->psk_slots[i].active) {
                    memcpy(found_peer->session_key, node->psk_slots[i].key, NLP_KEY_SIZE);
                    break;
                }
            }
            nimcp_mutex_unlock(&node->key_mutex);

            found_peer->healthy = true;
        }
        nimcp_mutex_unlock(&node->peer_mutex);
    }

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Peer added: %s:%d id=0x%08X",
                   address, port, local_peer_id);

    // Notify callback (use local copies since peer pointer may be stale)
    if (node->peer_callback) {
        nimcp_mutex_lock(&node->peer_mutex);
        nlp_peer_t* cb_peer = NULL;
        for (uint32_t i = 0; i < node->peer_count; i++) {
            if (node->peers[i].peer_id == local_peer_id) {
                cb_peer = &node->peers[i];
                break;
            }
        }
        if (cb_peer) {
            node->peer_callback(node, cb_peer, NLP_SESSION_DISCONNECTED,
                               local_session_state, node->user_data);
        }
        nimcp_mutex_unlock(&node->peer_mutex);
    }

    return local_peer_id;
}


int nlp_disconnect_peer(nlp_node_t node, uint32_t peer_id) {
    if (!bbb_check_pointer(node, "nlp_disconnect_peer")) {
        return -EINVAL;
    }

    nimcp_mutex_lock(&node->peer_mutex);

    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (node->peers[i].peer_id == peer_id) {
            nlp_peer_t* peer = &node->peers[i];
            nlp_session_state_t old_state = peer->session_state;
            bool need_disconnect_msg = (node->current_mode == NLP_MODE_STANDARD &&
                                        peer->session_state == NLP_SESSION_ESTABLISHED);

            /* HIGH-2 fix: nlp_send() acquires peer_mutex internally for unicast
             * key lookup. Calling it while holding peer_mutex causes deadlock
             * on non-recursive mutex. Release the lock, send, then re-acquire
             * and re-find the peer by ID for the memmove removal. */

            /* BUG-7 fix: Zero session key material before removing peer */
            {
                volatile uint8_t* vp = (volatile uint8_t*)peer->session_key;
                for (size_t k = 0; k < sizeof(peer->session_key); k++) {
                    vp[k] = 0;
                }
            }

            // Notify callback while we still have the peer pointer
            if (node->peer_callback) {
                node->peer_callback(node, peer, old_state,
                                   NLP_SESSION_DISCONNECTED, node->user_data);
            }

            nimcp_mutex_unlock(&node->peer_mutex);

            // Send disconnect message OUTSIDE the lock to avoid deadlock
            if (need_disconnect_msg) {
                nlp_send(node, peer_id, NLP_MSG_DISCONNECT, NULL, 0, NLP_PRIORITY_HIGH);
            }

            // Re-acquire lock and re-find peer by ID for removal
            nimcp_mutex_lock(&node->peer_mutex);
            for (uint32_t j = 0; j < node->peer_count; j++) {
                if (node->peers[j].peer_id == peer_id) {
                    if (j < node->peer_count - 1) {
                        memmove(&node->peers[j], &node->peers[j + 1],
                               (node->peer_count - j - 1) * sizeof(nlp_peer_t));
                    }
                    node->peer_count--;
                    break;
                }
            }
            nimcp_mutex_unlock(&node->peer_mutex);

            NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Peer disconnected: 0x%08X", peer_id);
            return 0;
        }
    }

    nimcp_mutex_unlock(&node->peer_mutex);
    return -ENOENT;
}


//=============================================================================
// Messaging
//=============================================================================

int nlp_send(nlp_node_t node, uint32_t peer_id, nlp_msg_type_t msg_type,
             const void* payload, size_t payload_len, nlp_priority_t priority) {
    if (!bbb_check_pointer(node, "nlp_send")) {
        return -EINVAL;
    }

    if (payload_len > 0 && !bbb_check_pointer(payload, "nlp_send")) {
        return -EINVAL;
    }

    if (payload_len > NLP_MAX_PAYLOAD) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Payload too large: %zu", payload_len);
        return -EMSGSIZE;
    }

    // Check EMCON restrictions in stealth mode
    if (node->current_mode == NLP_MODE_STEALTH) {
        if (node->emcon_level == NLP_EMCON_RECEIVE ||
            node->emcon_level == NLP_EMCON_SILENT) {
            // Cannot transmit unless emergency
            if (node->emcon_level != NLP_EMCON_EMERGENCY &&
                priority != NLP_PRIORITY_CRITICAL) {
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "TX blocked by EMCON level %d",
                               node->emcon_level);
                return -EAGAIN;
            }
        }
    }

    // For broadcasts (peer_id == 0) in STANDARD mode, directly call nlp_broadcast
    // which will iterate over all established peers and send to each with their
    // individual session keys. We can't encrypt a single broadcast message in
    // STANDARD mode because each peer has a different session key.
    if (peer_id == 0 && node->current_mode == NLP_MODE_STANDARD) {
        return nlp_broadcast(node, msg_type, payload, payload_len, priority);
    }

    // Build message (pass NULL payload - we handle encryption separately)
    nlp_message_t* msg = nlp_message_create(msg_type, NULL, 0);
    if (!msg) {
        return -ENOMEM;
    }

    // Initialize header
    nlp_header_init(&msg->header);
    NLP_SET_VERSION(&msg->header, NLP_VERSION);
    NLP_SET_MODE(&msg->header, node->current_mode);
    NLP_SET_PRIORITY(&msg->header, priority);

    uint8_t flags = NLP_FLAG_ENCRYPTED;
    if (priority >= NLP_PRIORITY_HIGH) {
        flags |= NLP_FLAG_ACK_REQUIRED;
    }
    NLP_SET_FLAGS(&msg->header, flags);

    // Set header fields in HOST byte order
    // nlp_header_serialize will convert to network byte order
    msg->header.msg_type = msg_type;
    msg->header.sender_id = node->brain_id;
    msg->header.timestamp = (uint32_t)time(NULL);

    // Always set dest_id = 0 for now.
    // For initiated connections, peer_id is hash(address:port), which doesn't
    // match the remote brain_id, causing dest_id check failures on the receiver.
    // Unicast is determined by destination address:port, not dest_id.
    // TODO: Track remote brain_id separately and use it for dest_id in mesh mode.
    msg->header.dest_id = 0;

    // Generate nonce using node's crypto state
    nlp_crypto_generate_nonce(node, msg->header.nonce);

    // Get sequence number
    nimcp_mutex_lock(&node->seq_mutex);
    msg->header.sequence = node->tx_sequence++;
    nimcp_mutex_unlock(&node->seq_mutex);

    // Select key slot based on mode
    uint8_t key_slot = 0;
    if (node->current_mode != NLP_MODE_STANDARD) {
        nlp_session_select_psk(node, &key_slot);
    }
    NLP_SET_KEY_SLOT(&msg->header, key_slot);

    // Check if this is a handshake message (sent before session key is established)
    // Handshake messages in standard mode cannot be encrypted because
    // session keys don't exist yet - they're negotiated during handshake
    bool is_handshake_msg = (msg_type == NLP_MSG_HANDSHAKE_REQ ||
                             msg_type == NLP_MSG_HANDSHAKE_RESP ||
                             msg_type == NLP_MSG_HANDSHAKE_FINAL);

    // For standard mode handshake messages, skip encryption
    if (is_handshake_msg && node->current_mode == NLP_MODE_STANDARD) {
        // Clear encrypted flag for handshake messages
        uint8_t flags = NLP_GET_FLAGS(&msg->header);
        flags &= ~NLP_FLAG_ENCRYPTED;
        NLP_SET_FLAGS(&msg->header, flags);

        // Copy unencrypted payload if present
        if (payload && payload_len > 0) {
            msg->payload = (uint8_t*)nimcp_malloc(payload_len);
            if (!msg->payload) {
                nlp_message_destroy(msg);
                return -ENOMEM;
            }
            memcpy(msg->payload, payload, payload_len);
            msg->header.payload_len = (uint16_t)payload_len;
        } else {
            msg->header.payload_len = 0;
        }

        // Clear auth tag (no encryption = no auth tag)
        memset(msg->auth_tag, 0, NLP_TAG_SIZE);

        NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
            "Sending unencrypted handshake message type=0x%04X to peer 0x%08X",
            msg_type, peer_id);

        goto serialize_and_send;
    }

    // Encrypt payload
    if (payload && payload_len > 0) {
        msg->payload = (uint8_t*)nimcp_malloc(payload_len + NLP_TAG_SIZE);
        if (!msg->payload) {
            nlp_message_destroy(msg);
            return -ENOMEM;
        }

        // Get encryption key
        uint8_t* key = NULL;
        if (peer_id == 0) {
            // Broadcast: use PSK
            nimcp_mutex_lock(&node->key_mutex);
            if (node->psk_slots[key_slot].active) {
                key = node->psk_slots[key_slot].key;
            }
            nimcp_mutex_unlock(&node->key_mutex);
        } else {
            // Unicast: use session key
            nimcp_mutex_lock(&node->peer_mutex);
            for (uint32_t i = 0; i < node->peer_count; i++) {
                if (node->peers[i].peer_id == peer_id) {
                    key = node->peers[i].session_key;
                    break;
                }
            }
            nimcp_mutex_unlock(&node->peer_mutex);
        }

        if (!key) {
            NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "No encryption key available");
            nlp_message_destroy(msg);
            return -ENOKEY;
        }

        // Set payload length BEFORE serializing header (needed for AAD)
        msg->header.payload_len = (uint16_t)payload_len;

        // Serialize header to get wire format for AAD
        // We need the exact bytes that will be sent on the wire
        nlp_header_t wire_header;
        memcpy(&wire_header, &msg->header, sizeof(nlp_header_t));
        nlp_header_serialize(&wire_header);

        // Encrypt with node's crypto state, using SERIALIZED header as AAD
        // IMPORTANT: Must use wire format to match what receiver sees
        int rc = nlp_crypto_encrypt(node, key, msg->header.nonce,
                                    payload, payload_len,
                                    (const uint8_t*)&wire_header, sizeof(nlp_header_t),
                                    msg->payload, payload_len + NLP_TAG_SIZE,
                                    msg->auth_tag);
        if (rc < 0) {
            NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "Encryption failed: %d", rc);
            nlp_message_destroy(msg);
            return rc;
        }
    } else {
        msg->header.payload_len = 0;
    }

serialize_and_send:
    // Note: Header CRC is calculated in nlp_header_serialize, called from nlp_message_serialize

    // Serialize message
    uint8_t wire_buffer[NLP_MAX_PAYLOAD + NLP_HEADER_SIZE + NLP_TAG_SIZE];
    size_t wire_len = 0;

    int rc = nlp_message_serialize(msg, wire_buffer, sizeof(wire_buffer), &wire_len);
    if (rc < 0) {
        nlp_message_destroy(msg);
        return rc;
    }

    // In stealth mode, pad to fixed size or queue for burst
    if (node->current_mode == NLP_MODE_STEALTH) {
        // Queue for burst UNLESS emergency override or critical priority
        if (node->emcon_level >= NLP_EMCON_REDUCED &&
            node->emcon_level != NLP_EMCON_EMERGENCY &&
            priority != NLP_PRIORITY_CRITICAL) {
            // Queue for burst transmission
            nimcp_mutex_lock(&node->burst_mutex);
            if (node->burst_buffer_used + wire_len <= node->burst_buffer_size) {
                memcpy(node->burst_buffer + node->burst_buffer_used,
                       wire_buffer, wire_len);
                node->burst_buffer_used += wire_len;
            }
            nimcp_mutex_unlock(&node->burst_mutex);
            nlp_message_destroy(msg);
            return 0;  // Queued for later
        }

        // Pad to fixed size for traffic analysis resistance
        // nlp_message_pad_to_fixed_size expects message + buffer, not wire_buffer + len
        // Create temporary padded buffer
        uint8_t padded_buffer[NLP_STEALTH_PACKET_SIZE];
        if (nlp_message_pad_to_fixed_size(msg, padded_buffer) == 0) {
            memcpy(wire_buffer, padded_buffer, NLP_STEALTH_PACKET_SIZE);
            wire_len = NLP_STEALTH_PACKET_SIZE;
        }
    }

    // Send to peer(s)
    if (peer_id == 0) {
        /* BUG-13 fix: For non-standard mode broadcasts (peer_id == 0), send the
         * pre-built wire_buffer directly to each peer via nlp_send_raw instead of
         * calling nlp_broadcast (which calls nlp_send, which re-builds and
         * re-encrypts the message, causing double-send). In non-standard modes
         * all peers share the same PSK, so one encrypted message works for all. */
        nimcp_mutex_lock(&node->peer_mutex);
        rc = 0;
        for (uint32_t i = 0; i < node->peer_count; i++) {
            if (node->peers[i].session_state == NLP_SESSION_ESTABLISHED) {
                int send_rc = nlp_send_raw(node, node->peers[i].peer_id,
                                           wire_buffer, wire_len);
                if (send_rc >= 0) rc++;
            }
        }
        nimcp_mutex_unlock(&node->peer_mutex);
    } else {
        rc = nlp_send_raw(node, peer_id, wire_buffer, wire_len);
    }

    nlp_message_destroy(msg);

    // Update statistics
    if (rc >= 0) {
        nimcp_mutex_lock(&node->stats_mutex);
        node->stats.messages_sent++;
        node->stats.bytes_sent += wire_len;
        nimcp_mutex_unlock(&node->stats_mutex);
    }

    return rc;
}


int nlp_broadcast(nlp_node_t node, nlp_msg_type_t msg_type,
                  const void* payload, size_t payload_len, nlp_priority_t priority) {
    if (!bbb_check_pointer(node, "nlp_broadcast")) {
        return -EINVAL;
    }

    int sent = 0;

    // Collect peer IDs under lock, then send without lock to avoid
    // unlock-iterate-relock race where peers array can change
    nimcp_mutex_lock(&node->peer_mutex);
    uint32_t peer_count = node->peer_count;

    /* BUG-6 fix: Early return when no peers to avoid zero-size allocation.
     * nimcp_malloc(0) may return NULL or a unique pointer depending on
     * platform, leading to incorrect behavior. */
    if (peer_count == 0) {
        nimcp_mutex_unlock(&node->peer_mutex);
        return 0;
    }

    uint32_t* peer_ids = nimcp_malloc(peer_count * sizeof(uint32_t));
    uint32_t active = 0;
    if (peer_ids) {
        for (uint32_t i = 0; i < peer_count; i++) {
            if (node->peers[i].session_state == NLP_SESSION_ESTABLISHED) {
                peer_ids[active++] = node->peers[i].peer_id;
            }
        }
    }
    nimcp_mutex_unlock(&node->peer_mutex);

    for (uint32_t i = 0; i < active; i++) {
        int rc = nlp_send(node, peer_ids[i], msg_type,
                         payload, payload_len, priority);
        if (rc >= 0) sent++;
    }
    nimcp_free(peer_ids);

    return sent;
}


int nlp_relay(nlp_node_t node, uint32_t dest_id, nlp_msg_type_t msg_type,
              const void* payload, size_t payload_len) {
    if (!bbb_check_pointer(node, "nlp_relay")) {
        return -EINVAL;
    }

    // Build relay header
    struct {
        uint32_t final_dest;
        uint8_t ttl;
        uint8_t hop_count;
        uint16_t reserved;
    } relay_header;

    /* BUG-29 fix: Replace magic number with named constant */
#ifndef NLP_RELAY_MAX_TTL
#define NLP_RELAY_MAX_TTL 16  /**< Maximum relay hops before message is dropped */
#endif
    relay_header.final_dest = htonl(dest_id);
    relay_header.ttl = NLP_RELAY_MAX_TTL;
    relay_header.hop_count = 0;
    relay_header.reserved = 0;

    // Combine relay header with original payload
    size_t total_len = sizeof(relay_header) + payload_len;
    uint8_t* relay_payload = (uint8_t*)nimcp_malloc(total_len);
    if (!relay_payload) {
        return -ENOMEM;
    }

    memcpy(relay_payload, &relay_header, sizeof(relay_header));
    if (payload && payload_len > 0) {
        memcpy(relay_payload + sizeof(relay_header), payload, payload_len);
    }

    // Broadcast relay message
    int rc = nlp_broadcast(node, NLP_MSG_RELAY, relay_payload, total_len,
                          NLP_PRIORITY_NORMAL);

    nimcp_free(relay_payload);

    nimcp_mutex_lock(&node->stats_mutex);
    node->stats.messages_relayed++;
    nimcp_mutex_unlock(&node->stats_mutex);

    return rc;
}


//=============================================================================
// Neural Sync
//=============================================================================

int nlp_send_spikes(nlp_node_t node, uint32_t peer_id,
                    const uint32_t* neuron_ids, const uint16_t* spike_times,
                    uint32_t count) {
    if (!bbb_check_pointer(node, "nlp_send_spikes")) {
        return -EINVAL;
    }

    if (count == 0) return 0;

    /* BUG-23 fix: spike_count is uint16_t in the protocol header. Silently
     * truncating a uint32_t count > UINT16_MAX corrupts the batch metadata,
     * causing the receiver to misinterpret the payload layout. */
    if (count > UINT16_MAX) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME,
            "Spike count %u exceeds protocol maximum %u", count, UINT16_MAX);
        return -EMSGSIZE;
    }

    if (!bbb_check_pointer(neuron_ids, "nlp_send_spikes") ||
        !bbb_check_pointer(spike_times, "nlp_send_spikes")) {
        return -EINVAL;
    }

    // Pack spike batch
    size_t payload_len = sizeof(nlp_spike_batch_t) +
                         count * sizeof(uint32_t) +
                         count * sizeof(uint16_t);

    uint8_t* payload = (uint8_t*)nimcp_malloc(payload_len);
    if (!payload) {
        return -ENOMEM;
    }

    nlp_spike_batch_t* batch = (nlp_spike_batch_t*)payload;
    batch->batch_id = htonl((uint32_t)time(NULL));
    batch->timestamp_us = htonl(0);
    batch->spike_count = htons((uint16_t)count);
    batch->reserved = 0;

    uint32_t* ids_out = (uint32_t*)(payload + sizeof(nlp_spike_batch_t));
    uint16_t* times_out = (uint16_t*)(ids_out + count);

    for (uint32_t i = 0; i < count; i++) {
        ids_out[i] = htonl(neuron_ids[i]);
        times_out[i] = htons(spike_times[i]);
    }

    int rc = nlp_send(node, peer_id, NLP_MSG_SPIKE_BATCH, payload, payload_len,
                     NLP_PRIORITY_HIGH);

    nimcp_free(payload);
    return rc;
}


int nlp_send_weight_deltas(nlp_node_t node, uint32_t peer_id,
                           const uint32_t* synapse_ids,
                           const float* old_weights, const float* new_weights,
                           uint32_t count) {
    if (!bbb_check_pointer(node, "nlp_send_weight_deltas")) {
        return -EINVAL;
    }

    if (count == 0) return 0;

    /* BUG-24 fix: delta_count is uint16_t in the protocol header. Silently
     * truncating a uint32_t count > UINT16_MAX corrupts the header metadata,
     * causing the receiver to read wrong number of entries. */
    if (count > UINT16_MAX) {
        NIMCP_LOGGING_ERROR(NLP_MODULE_NAME,
            "Weight delta count %u exceeds protocol maximum %u", count, UINT16_MAX);
        return -EMSGSIZE;
    }

    if (!bbb_check_pointer(synapse_ids, "nlp_send_weight_deltas") ||
        !bbb_check_pointer(old_weights, "nlp_send_weight_deltas") ||
        !bbb_check_pointer(new_weights, "nlp_send_weight_deltas")) {
        return -EINVAL;
    }

    // Pack weight deltas
    size_t payload_len = sizeof(nlp_weight_delta_header_t) +
                         count * sizeof(nlp_weight_delta_entry_t);

    uint8_t* payload = (uint8_t*)nimcp_malloc(payload_len);
    if (!payload) {
        return -ENOMEM;
    }

    nlp_weight_delta_header_t* header = (nlp_weight_delta_header_t*)payload;
    header->base_version = htonl(0);  // TODO: track versions
    header->new_version = htonl(1);
    header->delta_count = htons((uint16_t)count);
    header->reserved = 0;

    nlp_weight_delta_entry_t* entries =
        (nlp_weight_delta_entry_t*)(payload + sizeof(nlp_weight_delta_header_t));

    for (uint32_t i = 0; i < count; i++) {
        entries[i].synapse_id = htonl(synapse_ids[i]);
        /* BUG-15 fix: Use memcpy instead of pointer cast to avoid strict aliasing
         * violation. The compiler may optimize away *(float*)&uint32 because it
         * violates C's type-based aliasing rules. memcpy is always safe. */
        float old_val, new_val;
        memcpy(&old_val, &old_weights[i], sizeof(float));
        memcpy(&new_val, &new_weights[i], sizeof(float));
        memcpy(&entries[i].old_weight, &old_val, sizeof(float));
        memcpy(&entries[i].new_weight, &new_val, sizeof(float));
    }

    int rc = nlp_send(node, peer_id, NLP_MSG_WEIGHT_DELTA, payload, payload_len,
                     NLP_PRIORITY_NORMAL);

    nimcp_free(payload);
    return rc;
}


int nlp_send_state(nlp_node_t node, uint32_t peer_id,
                   const void* state_data, size_t state_len) {
    if (!bbb_check_pointer(node, "nlp_send_state")) {
        return -EINVAL;
    }

    if (state_len > 0 && !bbb_check_pointer(state_data, "nlp_send_state")) {
        return -EINVAL;
    }

    return nlp_send(node, peer_id, NLP_MSG_STATE_SYNC, state_data, state_len,
                   NLP_PRIORITY_LOW);
}


int nlp_rotate_session_key(nlp_node_t node, uint32_t peer_id) {
    if (!bbb_check_pointer(node, "nlp_rotate_session_key")) {
        return -EINVAL;
    }

    // Only applicable in standard mode
    if (node->current_mode != NLP_MODE_STANDARD) {
        NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Key rotation only in standard mode");
        return -EINVAL;
    }

    // Generate new session key (using nonce generator as source of randomness)
    uint8_t new_key[NLP_KEY_SIZE];
    memset(new_key, 0, NLP_KEY_SIZE);  // Zero-init all bytes before partial fill
    nlp_crypto_generate_nonce(node, new_key);       // First 12 bytes
    nlp_crypto_generate_nonce(node, new_key + 12);  // Next 12 bytes

    /* WARNING: Key rotation protocol is insecure — key sent as plaintext.
     * TODO: Implement DH key exchange for proper forward secrecy.
     * Mitigation: encrypt the new key with the OLD session key before sending,
     * so at least an eavesdropper without the old key cannot read it. */

    // Find peer and update key
    nimcp_mutex_lock(&node->peer_mutex);
    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (node->peers[i].peer_id == peer_id) {
            /* BUG-5 fix: Encrypt the new key with the OLD session key before
             * sending it over the wire. The receiver must decrypt with the old
             * key to obtain the new key. This is NOT a proper key exchange (still
             * vulnerable if old key is compromised), but prevents passive
             * eavesdroppers from capturing the new key in cleartext. */
            uint8_t old_key[NLP_KEY_SIZE];
            memcpy(old_key, node->peers[i].session_key, NLP_KEY_SIZE);

            /* Update peer's session key to the new one */
            memcpy(node->peers[i].session_key, new_key, NLP_KEY_SIZE);
            nimcp_mutex_unlock(&node->peer_mutex);

            /* Send the new key encrypted under the OLD key.
             * nlp_send() will use the peer's current (new) session key for
             * encrypting the payload. We need to temporarily restore the old
             * key for this one send, then restore the new key.
             * Simpler approach: send via nlp_send which encrypts with session
             * key — we already updated to new_key, so we need to send with old.
             * Restore old key temporarily for the send. */
            nimcp_mutex_lock(&node->peer_mutex);
            for (uint32_t j = 0; j < node->peer_count; j++) {
                if (node->peers[j].peer_id == peer_id) {
                    memcpy(node->peers[j].session_key, old_key, NLP_KEY_SIZE);
                    break;
                }
            }
            nimcp_mutex_unlock(&node->peer_mutex);

            /* Send key rotation message — encrypted with OLD key */
            nlp_send(node, peer_id, NLP_MSG_KEY_ROTATE, new_key, NLP_KEY_SIZE,
                    NLP_PRIORITY_HIGH);

            /* Now install the new key for future messages */
            nimcp_mutex_lock(&node->peer_mutex);
            for (uint32_t j = 0; j < node->peer_count; j++) {
                if (node->peers[j].peer_id == peer_id) {
                    memcpy(node->peers[j].session_key, new_key, NLP_KEY_SIZE);
                    break;
                }
            }
            nimcp_mutex_unlock(&node->peer_mutex);

            /* Zero temporary old key copy */
            {
                volatile uint8_t* vp = (volatile uint8_t*)old_key;
                for (size_t k = 0; k < NLP_KEY_SIZE; k++) vp[k] = 0;
            }

            NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Session key rotated for peer 0x%08X", peer_id);
            return 0;
        }
    }
    nimcp_mutex_unlock(&node->peer_mutex);

    return -ENOENT;
}


//=============================================================================
// SAR/Disaster API
//=============================================================================

int nlp_send_location(nlp_node_t node, const nlp_location_t* location) {
    if (!bbb_check_pointer(node, "nlp_send_location") ||
        !bbb_check_pointer(location, "nlp_send_location")) {
        return -EINVAL;
    }

    return nlp_broadcast(node, NLP_MSG_LOCATION_UPDATE, location,
                        sizeof(nlp_location_t), NLP_PRIORITY_NORMAL);
}


int nlp_send_victim_report(nlp_node_t node, const nlp_victim_report_t* report,
                           const char* notes) {
    if (!bbb_check_pointer(node, "nlp_send_victim_report") ||
        !bbb_check_pointer(report, "nlp_send_victim_report")) {
        return -EINVAL;
    }

    size_t notes_len = notes ? strlen(notes) : 0;
    size_t payload_len = sizeof(nlp_victim_report_t) + notes_len;

    uint8_t* payload = (uint8_t*)nimcp_malloc(payload_len);
    if (!payload) {
        return -ENOMEM;
    }

    memcpy(payload, report, sizeof(nlp_victim_report_t));
    nlp_victim_report_t* r = (nlp_victim_report_t*)payload;
    r->notes_len = (uint16_t)notes_len;

    if (notes && notes_len > 0) {
        memcpy(payload + sizeof(nlp_victim_report_t), notes, notes_len);
    }

    // Victim reports are high priority
    int rc = nlp_broadcast(node, NLP_MSG_VICTIM_REPORT, payload, payload_len,
                          NLP_PRIORITY_HIGH);

    nimcp_free(payload);

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Victim report sent: id=%u triage=%d",
                   report->victim_id, report->triage);

    return rc;
}


int nlp_send_sensors(nlp_node_t node, const nlp_sensor_data_t* sensors) {
    if (!bbb_check_pointer(node, "nlp_send_sensors") ||
        !bbb_check_pointer(sensors, "nlp_send_sensors")) {
        return -EINVAL;
    }

    return nlp_broadcast(node, NLP_MSG_SENSOR_DATA, sensors,
                        sizeof(nlp_sensor_data_t), NLP_PRIORITY_LOW);
}


//=============================================================================
// Utility Functions
//=============================================================================

const char* nlp_msg_type_name(nlp_msg_type_t type) {
    switch (type) {
        case NLP_MSG_HANDSHAKE_REQ:    return "HANDSHAKE_REQ";
        case NLP_MSG_HANDSHAKE_RESP:   return "HANDSHAKE_RESP";
        case NLP_MSG_HANDSHAKE_FINAL:  return "HANDSHAKE_FINAL";
        case NLP_MSG_KEY_ROTATE:       return "KEY_ROTATE";
        case NLP_MSG_DISCONNECT:       return "DISCONNECT";
        case NLP_MSG_SESSION_RESUME:   return "SESSION_RESUME";
        case NLP_MSG_SPIKE_BATCH:      return "SPIKE_BATCH";
        case NLP_MSG_WEIGHT_DELTA:     return "WEIGHT_DELTA";
        case NLP_MSG_WEIGHT_FULL:      return "WEIGHT_FULL";
        case NLP_MSG_STATE_SYNC:       return "STATE_SYNC";
        case NLP_MSG_GRADIENT_PUSH:    return "GRADIENT_PUSH";
        case NLP_MSG_ACTIVATION_SYNC:  return "ACTIVATION_SYNC";
        case NLP_MSG_HEARTBEAT:        return "HEARTBEAT";
        case NLP_MSG_PEER_ANNOUNCE:    return "PEER_ANNOUNCE";
        case NLP_MSG_PEER_LIST:        return "PEER_LIST";
        case NLP_MSG_MASTER_ELECTION:  return "MASTER_ELECTION";
        case NLP_MSG_CONSENSUS_VOTE:   return "CONSENSUS_VOTE";
        case NLP_MSG_CONSENSUS_COMMIT: return "CONSENSUS_COMMIT";
        case NLP_MSG_ROLE_ASSIGN:      return "ROLE_ASSIGN";
        case NLP_MSG_PRIORITY_CMD:     return "PRIORITY_CMD";
        case NLP_MSG_EMERGENCY:        return "EMERGENCY";
        case NLP_MSG_RELAY:            return "RELAY";
        case NLP_MSG_ACK:              return "ACK";
        case NLP_MSG_NACK:             return "NACK";
        case NLP_MSG_RESEND_REQ:       return "RESEND_REQ";
        case NLP_MSG_BURST_SYNC:       return "BURST_SYNC";
        case NLP_MSG_CHAFF:            return "CHAFF";
        case NLP_MSG_LISTEN_WINDOW:    return "LISTEN_WINDOW";
        case NLP_MSG_EMCON_CHANGE:     return "EMCON_CHANGE";
        case NLP_MSG_LOCATION_UPDATE:  return "LOCATION_UPDATE";
        case NLP_MSG_VICTIM_REPORT:    return "VICTIM_REPORT";
        case NLP_MSG_SENSOR_DATA:      return "SENSOR_DATA";
        case NLP_MSG_HAZARD_ALERT:     return "HAZARD_ALERT";
        case NLP_MSG_RESOURCE_STATUS:  return "RESOURCE_STATUS";
        case NLP_MSG_PING:             return "PING";
        case NLP_MSG_PONG:             return "PONG";
        case NLP_MSG_STATS_REQ:        return "STATS_REQ";
        case NLP_MSG_STATS_RESP:       return "STATS_RESP";
        case NLP_MSG_DEBUG:            return "DEBUG";
        default:                       return "UNKNOWN";
    }
}


const char* nlp_mode_name(nlp_mode_t mode) {
    switch (mode) {
        case NLP_MODE_STANDARD: return "STANDARD";
        case NLP_MODE_TACTICAL: return "TACTICAL";
        case NLP_MODE_STEALTH:  return "STEALTH";
        default:                return "UNKNOWN";
    }
}


const char* nlp_emcon_name(nlp_emcon_level_t level) {
    switch (level) {
        case NLP_EMCON_NORMAL:    return "NORMAL";
        case NLP_EMCON_REDUCED:   return "REDUCED";
        case NLP_EMCON_RECEIVE:   return "RECEIVE_ONLY";
        case NLP_EMCON_SILENT:    return "SILENT";
        case NLP_EMCON_EMERGENCY: return "EMERGENCY";
        default:                  return "UNKNOWN";
    }
}


uint32_t nlp_generate_brain_id(void) {
    uint32_t id = 0;

    // Use time and random for uniqueness
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    id = (uint32_t)ts.tv_sec ^ (uint32_t)ts.tv_nsec;
    id ^= (uint32_t)getpid() << 16;
    id ^= nimcp_rand_uint(UINT32_MAX);

    // Ensure non-zero
    if (id == 0) id = 1;

    return id;
}
