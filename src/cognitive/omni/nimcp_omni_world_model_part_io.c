// nimcp_omni_world_model_part_io.c - io functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c


/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* omni_wm_direction_to_string(omni_wm_direction_t dir) {
    switch (dir) {
        case OMNI_WM_DIR_FORWARD:     return "forward";
        case OMNI_WM_DIR_BACKWARD:    return "backward";
        case OMNI_WM_DIR_LATERAL:     return "lateral";
        case OMNI_WM_DIR_HIERARCHICAL: return "hierarchical";
        default:                       return "unknown";
    }
}


const char* omni_wm_learn_mode_to_string(omni_wm_learn_mode_t mode) {
    switch (mode) {
        case OMNI_WM_LEARN_ONLINE:   return "online";
        case OMNI_WM_LEARN_BATCH:    return "batch";
        case OMNI_WM_LEARN_REPLAY:   return "replay";
        case OMNI_WM_LEARN_DREAMING: return "dreaming";
        default:                      return "unknown";
    }
}


const char* omni_wm_cf_type_to_string(omni_wm_counterfactual_type_t type) {
    switch (type) {
        case OMNI_WM_CF_ACTION:  return "action";
        case OMNI_WM_CF_STATE:   return "state";
        case OMNI_WM_CF_CONTEXT: return "context";
        case OMNI_WM_CF_GOAL:    return "goal";
        default:                 return "unknown";
    }
}


/* ============================================================================
 * Public Serialization API
 * ============================================================================ */

size_t omni_wm_serialize(const omni_world_model_t* wm,
                          uint8_t* buffer,
                          size_t buffer_size) {
    if (!wm) return 0;

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_serialize", 0.0f);

    /* Two-pass pattern: if buffer is non-NULL, first compute the required size
     * with a NULL-buffer dry run, then verify it fits before writing. */
    if (buffer) {
        size_t required = omni_wm_serialize(wm, NULL, 0);
        if (required == 0 || required > buffer_size) {
            return 0;  /* Buffer too small (or serialization error) */
        }
    }

    size_t pos = 0;

    /* Header */
    pos = write_u32(buffer, pos, OMNI_WM_SERIAL_MAGIC);
    pos = write_u8(buffer, pos, OMNI_WM_SERIAL_VERSION);

    /* Compute flags */
    uint8_t flags = OMNI_WM_SERIAL_FLAG_HAS_DYNAMICS;
    if (wm->rssm_state) flags |= OMNI_WM_SERIAL_FLAG_HAS_RSSM;
    if (wm->replay_buffer && wm->replay_buffer->size > 0) {
        flags |= OMNI_WM_SERIAL_FLAG_HAS_REPLAY;
    }
    pos = write_u8(buffer, pos, flags);

    /* Config */
    pos = serialize_config(buffer, pos, &wm->config);

    /* Current state */
    pos = serialize_state(buffer, pos, wm->current_state);

    /* RSSM state */
    if (flags & OMNI_WM_SERIAL_FLAG_HAS_RSSM) {
        pos = serialize_rssm_state(buffer, pos, wm->rssm_state);
    }

    /* Dynamics models */
    pos = serialize_dynamics_weights(buffer, pos, wm->forward_dynamics);
    pos = serialize_dynamics_weights(buffer, pos, wm->backward_dynamics);
    pos = serialize_dynamics_weights(buffer, pos, wm->lateral_dynamics);

    /* Encoder/decoder weights */
    uint32_t enc_size = wm->config.obs_dim * wm->config.latent_dim;
    pos = write_u32(buffer, pos, enc_size);
    for (uint32_t i = 0; i < enc_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && enc_size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)enc_size);
        }

        pos = write_float_be(buffer, pos, wm->encoder_W[i]);
    }
    pos = write_u32(buffer, pos, wm->config.latent_dim);
    for (uint32_t i = 0; i < wm->config.latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->config.latent_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)wm->config.latent_dim);
        }

        pos = write_float_be(buffer, pos, wm->encoder_b[i]);
    }
    for (uint32_t i = 0; i < enc_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && enc_size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)enc_size);
        }

        pos = write_float_be(buffer, pos, wm->decoder_W[i]);
    }
    pos = write_u32(buffer, pos, wm->config.obs_dim);
    for (uint32_t i = 0; i < wm->config.obs_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->config.obs_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)wm->config.obs_dim);
        }

        pos = write_float_be(buffer, pos, wm->decoder_b[i]);
    }

    /* Replay buffer (limited serialization - just size info for now) */
    if (flags & OMNI_WM_SERIAL_FLAG_HAS_REPLAY) {
        pos = write_u32(buffer, pos, wm->replay_buffer->size);
        /* Note: Full replay buffer serialization would be very large.
         * For checkpointing, we just store the count. Full serialization
         * could be added as an option in a future version. */
    }

    /* Statistics */
    pos = serialize_wm_stats(buffer, pos, &wm->stats);

    /* Random seed */
    pos = write_u32(buffer, pos, wm->rand_seed);

    /* Checksum (computed over all preceding data) */
    if (buffer) {
        uint32_t checksum = crc32_compute(buffer, pos);
        pos = write_u32(buffer, pos, checksum);
    } else {
        pos += 4; /* Reserve space for checksum */
    }

    /* Size already verified upfront for non-NULL buffer via dry-run. */
    return pos;
}


omni_world_model_t* omni_wm_deserialize(const uint8_t* buffer,
                                         size_t buffer_size) {
    if (!buffer || buffer_size < 10) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_wm_stats: buffer is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_deserialize", 0.0f);


    size_t pos = 0;

    /* Verify magic */
    uint32_t magic = read_u32(buffer, &pos);
    if (magic != OMNI_WM_SERIAL_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_wm_stats: validation failed");
        return NULL;
    }

    /* Verify version */
    uint8_t version = read_u8(buffer, &pos);
    if (version > OMNI_WM_SERIAL_VERSION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_wm_stats: validation failed");
        return NULL;
    }

    /* Read flags */
    uint8_t flags = read_u8(buffer, &pos);

    /* Read config */
    omni_wm_config_t config;
    pos = deserialize_config(buffer, pos, &config);

    /* Create world model with config */
    omni_world_model_t* wm = omni_wm_create(&config);
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_wm_stats: wm is NULL");
        return NULL;
    }

    /* Read current state */
    omni_wm_state_t* state = deserialize_state_from_buf(buffer, &pos);
    if (state) {
        if (wm->current_state) {
            omni_wm_state_destroy(wm->current_state);
        }
        wm->current_state = state;
    }

    /* Read RSSM state */
    if (flags & OMNI_WM_SERIAL_FLAG_HAS_RSSM) {
        omni_wm_rssm_state_t* rssm = deserialize_rssm_state_from_buf(buffer, &pos);
        if (rssm) {
            if (wm->rssm_state) {
                omni_wm_rssm_state_destroy(wm->rssm_state);
            }
            wm->rssm_state = rssm;
        }
    }

    /* Read dynamics models */
    omni_wm_dynamics_t* forward = deserialize_dynamics_from_buf(buffer, &pos);
    if (forward) {
        dynamics_destroy(wm->forward_dynamics);
        wm->forward_dynamics = forward;
    }

    omni_wm_dynamics_t* backward = deserialize_dynamics_from_buf(buffer, &pos);
    if (backward) {
        dynamics_destroy(wm->backward_dynamics);
        wm->backward_dynamics = backward;
    }

    omni_wm_dynamics_t* lateral = deserialize_dynamics_from_buf(buffer, &pos);
    if (lateral) {
        dynamics_destroy(wm->lateral_dynamics);
        wm->lateral_dynamics = lateral;
    }

    /* Read encoder/decoder weights */
    uint32_t enc_size = read_u32(buffer, &pos);
    if (enc_size == wm->config.obs_dim * wm->config.latent_dim) {
        for (uint32_t i = 0; i < enc_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && enc_size > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)enc_size);
            }

            wm->encoder_W[i] = read_float_be(buffer, &pos);
        }
    }

    uint32_t enc_b_size = read_u32(buffer, &pos);
    if (enc_b_size == wm->config.latent_dim) {
        for (uint32_t i = 0; i < enc_b_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && enc_b_size > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)enc_b_size);
            }

            wm->encoder_b[i] = read_float_be(buffer, &pos);
        }
    }

    for (uint32_t i = 0; i < enc_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && enc_size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)enc_size);
        }

        wm->decoder_W[i] = read_float_be(buffer, &pos);
    }

    uint32_t dec_b_size = read_u32(buffer, &pos);
    if (dec_b_size == wm->config.obs_dim) {
        for (uint32_t i = 0; i < dec_b_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dec_b_size > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)dec_b_size);
            }

            wm->decoder_b[i] = read_float_be(buffer, &pos);
        }
    }

    /* Read replay buffer info */
    if (flags & OMNI_WM_SERIAL_FLAG_HAS_REPLAY) {
        uint32_t replay_size = read_u32(buffer, &pos);
        (void)replay_size; /* Note: Full replay restoration not implemented */
    }

    /* Read statistics */
    pos = deserialize_wm_stats(buffer, pos, &wm->stats);

    /* Read random seed */
    wm->rand_seed = read_u32(buffer, &pos);

    /* Verify checksum */
    if (pos + 4 <= buffer_size) {
        uint32_t stored_checksum = read_u32(buffer, &pos);
        uint32_t computed_checksum = crc32_compute(buffer, pos - 4);
        if (stored_checksum != computed_checksum) {
            omni_wm_destroy(wm);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
            return NULL;
        }
    }

    return wm;
}


nimcp_error_t omni_wm_save(const omni_world_model_t* wm,
                            const char* filepath) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_save", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(filepath, NIMCP_ERROR_INVALID_PARAM, "filepath is NULL");

    /* Get required size */
    size_t required_size = omni_wm_serialize(wm, NULL, 0);
    NIMCP_CHECK_THROW(required_size > 0, NIMCP_ERROR_SERIALIZATION, "failed to compute serialization size");

    /* Allocate buffer */
    uint8_t* buffer = nimcp_malloc(required_size);
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate serialization buffer");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Serialize */
    size_t written = omni_wm_serialize(wm, buffer, required_size);
    if (written == 0) {
        nimcp_free(buffer);
        buffer = NULL;
        return NIMCP_ERROR_SERIALIZATION;
    }

    /* Write to file */
    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        nimcp_free(buffer);
        buffer = NULL;
        return NIMCP_ERROR_FILE_OPEN;
    }

    size_t bytes_written = fwrite(buffer, 1, written, fp);
    fclose(fp);
    nimcp_free(buffer);
    buffer = NULL;

    if (bytes_written != written) {
        return NIMCP_ERROR_FILE_WRITE;
    }

    return NIMCP_SUCCESS;
}


omni_world_model_t* omni_wm_load(const char* filepath) {
    if (!filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_load: filepath is NULL");
        return NULL;
    }

    /* Open file */
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_load", 0.0f);


    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_load: fp is NULL");
        return NULL;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_load: validation failed");
        return NULL;
    }

    /* Allocate buffer */
    uint8_t* buffer = nimcp_malloc((size_t)file_size);
    if (!buffer) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_load: buffer is NULL");
        return NULL;
    }

    /* Read file */
    size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    if (bytes_read != (size_t)file_size) {
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_load: validation failed");
        return NULL;
    }

    /* Deserialize */
    omni_world_model_t* wm = omni_wm_deserialize(buffer, (size_t)file_size);
    nimcp_free(buffer);
    buffer = NULL;

    return wm;
}
