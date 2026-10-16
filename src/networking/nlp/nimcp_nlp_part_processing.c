// nimcp_nlp_part_processing.c - processing functions
// Part of nimcp_nlp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_nlp.c


/**
 * @brief Handle incoming bio-async messages for NLP
 */
static nimcp_error_t nlp_bio_handler(const void* msg, size_t msg_size,
                                      nimcp_bio_promise_t response_promise,
                                      void* user_data) {
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_BIO_ERROR_INVALID_CHANNEL;  // Generic error for invalid input
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    nlp_node_t node = (nlp_node_t)user_data;

    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
        "Bio-async: received msg type=0x%04X from module=%u",
        header->type, header->source_module);

    switch (header->type) {
        case BIO_MSG_HEALTH_CHECK: {
            // Respond to health check
            if (response_promise && node) {
                bio_msg_health_response_t resp;
                memset(&resp, 0, sizeof(resp));
                bio_msg_init_header(&resp.header, BIO_MSG_HEALTH_RESPONSE,
                                    BIO_MODULE_NLP, header->source_module, sizeof(resp));
                resp.healthy = node->running;
                resp.active_threads = 3;  // recv, heartbeat, stealth
                resp.pending_messages = 0;  // Pending messages tracked separately
                nimcp_bio_promise_complete(response_promise, &resp);
            }
            break;
        }

        case BIO_MSG_ATTENTION_SHIFT: {
            // Attention system wants NLP to prioritize certain traffic
            const bio_msg_attention_shift_t* shift =
                (const bio_msg_attention_shift_t*)msg;
            if (node && shift->preemptive) {
                // Could adjust message processing priority
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                    "Attention shift: target=%u weight=%.2f",
                    shift->target_id, shift->attention_weight);
            }
            break;
        }

        case BIO_MSG_SECURITY_ALERT: {
            // Security system alerting about potential threat
            const bio_msg_nlp_error_t* alert =
                (const bio_msg_nlp_error_t*)msg;
            if (alert->severity >= 2) {
                NIMCP_LOGGING_WARN(NLP_MODULE_NAME,
                    "Security alert received: code=%u %s",
                    alert->error_code, alert->error_message);
                // Could trigger mode switch to Tactical
                if (node && node->config.auto_mode_switch) {
                    nlp_auto_mode_check(node);
                }
            }
            break;
        }

        case BIO_MSG_SHUTDOWN_REQUEST: {
            // Graceful shutdown request
            NIMCP_LOGGING_INFO(NLP_MODULE_NAME, "Shutdown requested via bio-async");
            if (node) {
                nlp_node_stop(node);
            }
            break;
        }

        default:
            // Unknown message type - just log
            NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                "Unhandled bio-async message type: 0x%04X", header->type);
            break;
    }

    (void)response_promise;  // May be unused in some cases
    return NIMCP_SUCCESS;
}


/**
 * @brief KG-driven wiring handler callback implementation
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int nlp_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;  /* NLP node, if needed for context */

    NIMCP_LOGGING_INFO(NLP_MODULE_NAME,
        "nlp_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_HEALTH_CHECK:
                bio_router_register_handler(ctx, message_types[i], nlp_bio_handler);
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                    "  Registered handler for BIO_MSG_HEALTH_CHECK");
                break;

            case BIO_MSG_ATTENTION_SHIFT:
                bio_router_register_handler(ctx, message_types[i], nlp_bio_handler);
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                    "  Registered handler for BIO_MSG_ATTENTION_SHIFT");
                break;

            case BIO_MSG_SECURITY_ALERT:
                bio_router_register_handler(ctx, message_types[i], nlp_bio_handler);
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                    "  Registered handler for BIO_MSG_SECURITY_ALERT");
                break;

            case BIO_MSG_SHUTDOWN_REQUEST:
                bio_router_register_handler(ctx, message_types[i], nlp_bio_handler);
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                    "  Registered handler for BIO_MSG_SHUTDOWN_REQUEST");
                break;

            default:
                NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                    "  Unknown message type 0x%04X - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}


/**
 * @brief Process incoming bio-async messages (call periodically)
 */
void nlp_process_bio_inbox(nlp_node_t node) {
    if (!g_nlp_bio_ctx) return;

    // Process up to 16 messages per call to avoid blocking
    uint32_t processed = bio_router_process_inbox(g_nlp_bio_ctx, 16);
    if (processed > 0) {
        NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
            "Processed %u bio-async messages", processed);
    }
    (void)node;
}


void nlp_update_environment(nlp_node_t node, const nlp_environment_t* env) {
    if (!bbb_check_pointer(node, "nlp_update_environment") ||
        !bbb_check_pointer(env, "nlp_update_environment")) {
        return;
    }

    nimcp_mutex_lock(&node->env_mutex);
    memcpy(&node->environment, env, sizeof(nlp_environment_t));
    nimcp_mutex_unlock(&node->env_mutex);

    // Check for automatic mode switching
    if (node->config.auto_mode_switch) {
        nlp_auto_mode_check(node);
    }
}


static int nlp_process_message(nlp_node_t node, const uint8_t* data, size_t len,
                               const struct sockaddr_in* from) {
    // Parse header
    if (len < NLP_HEADER_SIZE) {
        NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Message too short for header");
        return -EINVAL;
    }

    nlp_header_t header;
    memcpy(&header, data, sizeof(header));

    // Verify CRC
    uint16_t received_crc = ntohs(header.header_crc);
    header.header_crc = 0;
    uint16_t computed_crc = nlp_header_crc(&header);
    header.header_crc = htons(received_crc);

    if (received_crc != computed_crc) {
        NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Header CRC mismatch: 0x%04X vs 0x%04X",
                       received_crc, computed_crc);
        return -EINVAL;
    }

    // Extract fields
    uint8_t version = NLP_GET_VERSION(&header);
    nlp_mode_t mode = (nlp_mode_t)NLP_GET_MODE(&header);
    nlp_priority_t priority = (nlp_priority_t)NLP_GET_PRIORITY(&header);
    uint8_t key_slot = NLP_GET_KEY_SLOT(&header);
    uint8_t flags = NLP_GET_FLAGS(&header);

    uint32_t sender_id = ntohl(header.sender_id);
    uint32_t timestamp = ntohl(header.timestamp);
    nlp_msg_type_t msg_type = (nlp_msg_type_t)ntohs(header.msg_type);
    uint16_t payload_len = ntohs(header.payload_len);
    uint32_t dest_id = ntohl(header.dest_id);

    // Validate timestamp (replay protection)
    uint32_t now = (uint32_t)time(NULL);
    if (abs((int)(now - timestamp)) > NLP_TIMESTAMP_WINDOW) {
        NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Message timestamp outside window: %u vs %u",
                       timestamp, now);
        nimcp_mutex_lock(&node->stats_mutex);
        node->stats.replay_attacks_blocked++;
        nimcp_mutex_unlock(&node->stats_mutex);
        return -EINVAL;
    }

    // Check destination
    if (dest_id != 0 && dest_id != node->brain_id) {
        // Not for us - relay if in mesh mode
        if (node->current_mode == NLP_MODE_TACTICAL) {
            NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME, "Relaying message to 0x%08X", dest_id);
            nlp_relay(node, dest_id, msg_type, data + NLP_HEADER_SIZE,
                     len - NLP_HEADER_SIZE - NLP_TAG_SIZE);
        }
        return 0;
    }

    // Find or create peer
    uint32_t peer_id = sender_id;
    nlp_peer_t* peer = NULL;

    // Extract source address:port from incoming packet
    char from_addr_str[INET_ADDRSTRLEN] = {0};
    uint16_t from_port = 0;
    if (from) {
        inet_ntop(AF_INET, &from->sin_addr, from_addr_str, sizeof(from_addr_str));
        from_port = ntohs(from->sin_port);
    }

    nimcp_mutex_lock(&node->peer_mutex);

    // Always look up peer by address:port first for consistency
    // This works for both initiated connections (hash-based peer_id) and
    // received connections (brain_id-based peer_id)
    if (from) {
        for (uint32_t i = 0; i < node->peer_count; i++) {
            if (strcmp(node->peers[i].address, from_addr_str) == 0 &&
                node->peers[i].port == from_port) {
                peer = &node->peers[i];
                break;
            }
        }
    }

    // Fallback to peer_id lookup if address:port lookup failed
    // This handles cases where address might differ (NAT, etc.)
    if (!peer) {
        for (uint32_t i = 0; i < node->peer_count; i++) {
            if (node->peers[i].peer_id == sender_id) {
                peer = &node->peers[i];
                break;
            }
        }
    }

    // Auto-create peer for handshake requests from unknown senders
    if (!peer && msg_type == NLP_MSG_HANDSHAKE_REQ && from != NULL) {
        if (node->peer_count < node->config.max_peers) {
            // Extract address from sockaddr
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from->sin_addr, addr_str, sizeof(addr_str));
            uint16_t port = ntohs(from->sin_port);

            // Create new peer
            peer = &node->peers[node->peer_count];
            memset(peer, 0, sizeof(nlp_peer_t));

            peer->peer_id = sender_id;
            strncpy(peer->address, addr_str, sizeof(peer->address) - 1);
            peer->port = port;
            peer->session_state = NLP_SESSION_DISCONNECTED;
            peer->healthy = false;

            uint64_t now_ms = nimcp_platform_time_monotonic_ms();
            peer->last_seen_ms = now_ms;
            peer->last_sent_ms = now_ms;

            node->peer_count++;

            NIMCP_LOGGING_INFO(NLP_MODULE_NAME,
                "Auto-created peer 0x%08X from handshake request at %s:%u",
                sender_id, addr_str, port);
        } else {
            NIMCP_LOGGING_WARN(NLP_MODULE_NAME,
                "Cannot auto-create peer 0x%08X: max peers reached", sender_id);
        }
    }

    nimcp_mutex_unlock(&node->peer_mutex);

    // Decrypt payload if present
    uint8_t* decrypted = NULL;
    if (payload_len > 0 && (flags & NLP_FLAG_ENCRYPTED)) {
        decrypted = (uint8_t*)nimcp_malloc(payload_len);
        if (!decrypted) return -ENOMEM;

        // Get decryption key
        uint8_t* key = NULL;
        if (peer && peer->session_state == NLP_SESSION_ESTABLISHED) {
            key = peer->session_key;
        } else if (key_slot < NLP_KEY_SLOTS) {
            nimcp_mutex_lock(&node->key_mutex);
            if (node->psk_slots[key_slot].active) {
                key = node->psk_slots[key_slot].key;
            }
            nimcp_mutex_unlock(&node->key_mutex);
        }

        if (!key) {
            NIMCP_LOGGING_ERROR(NLP_MODULE_NAME, "No decryption key available");
            nimcp_free(decrypted);
            return -ENOKEY;
        }

        const uint8_t* encrypted = data + NLP_HEADER_SIZE;
        // Auth tag position is AFTER the encrypted payload, NOT at end of buffer
        // In STEALTH mode, messages are padded to fixed size so len > actual message size
        // The actual auth_tag is at NLP_HEADER_SIZE + payload_len
        const uint8_t* auth_tag = data + NLP_HEADER_SIZE + payload_len;

        // Decrypt with node's crypto state, using raw wire header as AAD
        // IMPORTANT: Use the original wire bytes (data), not the local header struct,
        // because the sender used the serialized header bytes for AAD
        int rc = nlp_crypto_decrypt(node, key, header.nonce,
                                    encrypted, payload_len,
                                    data, NLP_HEADER_SIZE,
                                    auth_tag,
                                    decrypted, payload_len);
        if (rc < 0) {
            NIMCP_LOGGING_WARN(NLP_MODULE_NAME, "Decryption failed");
            nimcp_mutex_lock(&node->stats_mutex);
            node->stats.decryption_errors++;
            nimcp_mutex_unlock(&node->stats_mutex);
            nimcp_free(decrypted);
            return rc;
        }
    }

    // Update peer stats
    if (peer) {
        peer->last_seen_ms = nimcp_platform_time_monotonic_ms();
        peer->messages_received++;
        peer->bytes_received += len;
        peer->missed_heartbeats = 0;
        peer->healthy = true;
    }

    // Update global stats
    nimcp_mutex_lock(&node->stats_mutex);
    node->stats.messages_received++;
    node->stats.bytes_received += len;
    nimcp_mutex_unlock(&node->stats_mutex);

    // Handle message by type
    switch (msg_type) {
        case NLP_MSG_HANDSHAKE_REQ:
            if (peer) {
                int req_rc = nlp_session_handle_handshake_req(node, peer, &header);
                if (req_rc == NIMCP_SUCCESS && node->current_mode == NLP_MODE_STANDARD) {
                    // Send handshake response
                    nlp_send(node, peer->peer_id, NLP_MSG_HANDSHAKE_RESP,
                             NULL, 0, NLP_PRIORITY_HIGH);
                    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                        "Sent handshake response to peer 0x%08X", peer->peer_id);
                }
            }
            break;

        case NLP_MSG_HANDSHAKE_RESP:
            if (peer) {
                int resp_rc = nlp_session_handle_handshake_resp(node, peer, &header);
                if (resp_rc == NIMCP_SUCCESS && node->current_mode == NLP_MODE_STANDARD) {
                    // Send final handshake to complete 3-way handshake
                    nlp_send(node, peer->peer_id, NLP_MSG_HANDSHAKE_FINAL,
                             NULL, 0, NLP_PRIORITY_HIGH);
                    NIMCP_LOGGING_DEBUG(NLP_MODULE_NAME,
                        "Sent handshake final to peer 0x%08X", peer->peer_id);

                    // Establish session on our side after sending final
                    nlp_session_establish(node, peer);
                }
            }
            break;

        case NLP_MSG_HANDSHAKE_FINAL:
            if (peer) {
                nlp_session_handle_handshake_final(node, peer, &header);
                // nlp_session_handle_handshake_final calls nlp_session_establish internally
            }
            break;

        case NLP_MSG_HEARTBEAT:
            // Just update peer stats (already done above)
            break;

        case NLP_MSG_ACK:
        case NLP_MSG_NACK:
            // Handle acknowledgments
            break;

        default:
            // Pass to user callback
            if (node->message_callback && peer) {
                nlp_message_t msg;
                memcpy(&msg.header, &header, sizeof(header));
                // Use decrypted payload if available, otherwise raw payload
                if (decrypted) {
                    msg.payload = decrypted;
                } else if (payload_len > 0) {
                    msg.payload = (uint8_t*)(data + NLP_HEADER_SIZE);
                } else {
                    msg.payload = NULL;
                }
                memcpy(msg.auth_tag, data + len - NLP_TAG_SIZE, NLP_TAG_SIZE);

                node->message_callback(node, peer, &msg, node->user_data);
            }
            break;
    }

    if (decrypted) {
        nimcp_free(decrypted);
    }

    return 0;
}
