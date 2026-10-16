// nimcp_omni_world_model_part_accessors.c - accessors functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c


void omni_world_model_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_omni_world_model_instance_health_agent = agent;
}


/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_get_default_config(omni_wm_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_default_", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_wm_config_t));

    /* Dimensionality */
    config->state_dim = 64;
    config->action_dim = 8;
    config->obs_dim = 64;
    config->hidden_dim = 128;
    config->num_levels = 3;

    /* RSSM settings */
    config->rssm_h_dim = 64;
    config->rssm_z_dim = 32;
    config->latent_dim = 64;

    /* MDN settings */
    config->mdn_components = 5;
    config->pred_type = OMNI_WM_PRED_STOCHASTIC;

    /* Learning settings */
    config->learning_rate = NIMCP_LEARNING_RATE_FINE;
    config->discount_factor = NIMCP_REWARD_DISCOUNT_DEFAULT;
    config->kl_weight = 1.0f;
    config->reward_scale = NIMCP_REWARD_SCALE_DEFAULT;
    config->learn_mode = OMNI_WM_LEARN_REPLAY;

    /* Experience replay */
    config->replay_buffer_size = 10000;
    config->batch_size = 32;
    config->priority_exponent = 0.6f;

    /* Dreaming settings */
    config->dream_horizon = 15;
    config->dream_episodes = 10;
    config->imagination_noise = 0.1f;

    /* Feature flags */
    config->enable_lateral = true;
    config->enable_hierarchical = false;
    config->enable_dreaming = true;
    config->use_symlog_rewards = true;
    config->use_rssm = true;
    config->use_mdn = false; /* Start simple */

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_set_state(omni_world_model_t* wm,
                                 const omni_wm_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_set_state", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");

    if (wm->current_state) {
        omni_wm_state_destroy(wm->current_state);
    }
    wm->current_state = omni_wm_state_clone(state);

    return wm->current_state ? NIMCP_SUCCESS : NIMCP_ERROR_NO_MEMORY;
}


const omni_wm_state_t* omni_wm_get_state(const omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_state", 0.0f);


    return wm ? wm->current_state : NULL;
}


const omni_wm_rssm_state_t* omni_wm_get_rssm_state(const omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_rssm_sta", 0.0f);


    return wm ? wm->rssm_state : NULL;
}


nimcp_error_t omni_wm_set_rssm_state(omni_world_model_t* wm,
                                      const omni_wm_rssm_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_set_rssm_sta", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "RSSM state is NULL");

    if (wm->rssm_state) {
        omni_wm_rssm_state_destroy(wm->rssm_state);
    }
    wm->rssm_state = omni_wm_rssm_state_clone(state);

    return wm->rssm_state ? NIMCP_SUCCESS : NIMCP_ERROR_NO_MEMORY;
}


uint32_t omni_wm_get_replay_size(const omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_replay_s", 0.0f);


    return wm && wm->replay_buffer ? wm->replay_buffer->size : 0;
}


nimcp_error_t omni_wm_set_learning_rate(omni_world_model_t* wm, float learning_rate) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_set_learning", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(learning_rate >= 0.0f && learning_rate <= 1.0f, NIMCP_ERROR_INVALID_PARAM,
                      "learning_rate must be between 0.0 and 1.0");
    wm->config.learning_rate = learning_rate;
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t omni_wm_get_stats(const omni_world_model_t* wm,
                                 omni_wm_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_stats", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");
    memcpy(stats, &wm->stats, sizeof(omni_wm_stats_t));
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Serialization Implementation
 * ============================================================================ */

/**
 * @brief Serialize config section
 */
static size_t serialize_config(uint8_t* buf, size_t pos, const omni_wm_config_t* cfg) {
    /* Dimensionality settings */
    pos = write_u32(buf, pos, cfg->state_dim);
    pos = write_u32(buf, pos, cfg->action_dim);
    pos = write_u32(buf, pos, cfg->obs_dim);
    pos = write_u32(buf, pos, cfg->hidden_dim);
    pos = write_u32(buf, pos, cfg->num_levels);

    /* RSSM settings */
    pos = write_u32(buf, pos, cfg->rssm_h_dim);
    pos = write_u32(buf, pos, cfg->rssm_z_dim);
    pos = write_u32(buf, pos, cfg->latent_dim);

    /* MDN settings */
    pos = write_u32(buf, pos, cfg->mdn_components);
    pos = write_u8(buf, pos, (uint8_t)cfg->pred_type);

    /* Learning settings */
    pos = write_float_be(buf, pos, cfg->learning_rate);
    pos = write_float_be(buf, pos, cfg->discount_factor);
    pos = write_float_be(buf, pos, cfg->kl_weight);
    pos = write_float_be(buf, pos, cfg->reward_scale);
    pos = write_u8(buf, pos, (uint8_t)cfg->learn_mode);

    /* Experience replay settings */
    pos = write_u32(buf, pos, cfg->replay_buffer_size);
    pos = write_u32(buf, pos, cfg->batch_size);
    pos = write_float_be(buf, pos, cfg->priority_exponent);

    /* Dreaming settings */
    pos = write_u32(buf, pos, cfg->dream_horizon);
    pos = write_u32(buf, pos, cfg->dream_episodes);
    pos = write_float_be(buf, pos, cfg->imagination_noise);

    /* Feature flags (packed as byte) */
    uint8_t flags = 0;
    if (cfg->enable_lateral) flags |= 0x01;
    if (cfg->enable_hierarchical) flags |= 0x02;
    if (cfg->enable_dreaming) flags |= 0x04;
    if (cfg->use_symlog_rewards) flags |= 0x08;
    if (cfg->use_rssm) flags |= 0x10;
    if (cfg->use_mdn) flags |= 0x20;
    pos = write_u8(buf, pos, flags);

    return pos;
}


/**
 * @brief Deserialize config section
 */
static size_t deserialize_config(const uint8_t* buf, size_t pos, omni_wm_config_t* cfg) {
    /* Dimensionality settings */
    cfg->state_dim = read_u32(buf, &pos);
    cfg->action_dim = read_u32(buf, &pos);
    cfg->obs_dim = read_u32(buf, &pos);
    cfg->hidden_dim = read_u32(buf, &pos);
    cfg->num_levels = read_u32(buf, &pos);

    /* RSSM settings */
    cfg->rssm_h_dim = read_u32(buf, &pos);
    cfg->rssm_z_dim = read_u32(buf, &pos);
    cfg->latent_dim = read_u32(buf, &pos);

    /* MDN settings */
    cfg->mdn_components = read_u32(buf, &pos);
    cfg->pred_type = (omni_wm_prediction_type_t)read_u8(buf, &pos);

    /* Learning settings */
    cfg->learning_rate = read_float_be(buf, &pos);
    cfg->discount_factor = read_float_be(buf, &pos);
    cfg->kl_weight = read_float_be(buf, &pos);
    cfg->reward_scale = read_float_be(buf, &pos);
    cfg->learn_mode = (omni_wm_learn_mode_t)read_u8(buf, &pos);

    /* Experience replay settings */
    cfg->replay_buffer_size = read_u32(buf, &pos);
    cfg->batch_size = read_u32(buf, &pos);
    cfg->priority_exponent = read_float_be(buf, &pos);

    /* Dreaming settings */
    cfg->dream_horizon = read_u32(buf, &pos);
    cfg->dream_episodes = read_u32(buf, &pos);
    cfg->imagination_noise = read_float_be(buf, &pos);

    /* Feature flags */
    uint8_t flags = read_u8(buf, &pos);
    cfg->enable_lateral = (flags & 0x01) != 0;
    cfg->enable_hierarchical = (flags & 0x02) != 0;
    cfg->enable_dreaming = (flags & 0x04) != 0;
    cfg->use_symlog_rewards = (flags & 0x08) != 0;
    cfg->use_rssm = (flags & 0x10) != 0;
    cfg->use_mdn = (flags & 0x20) != 0;

    return pos;
}


uint32_t omni_wm_get_checkpoint_count(const omni_world_model_t* wm) {
    if (!wm || !wm->checkpoint_store) return 0;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_checkpoi", 0.0f);


    return wm->checkpoint_store->count;
}
