/**
 * @file nimcp_mirror_hippocampus_bridge.c
 * @brief Mirror Neuron - Hippocampus Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Implementation of mirror neuron to hippocampus integration
 * WHY:  Enable episodic storage of observed actions and context-based retrieval
 * HOW:  Store observed action sequences as episodes, retrieve for context/replay
 *
 * @see nimcp_mirror_hippocampus_bridge.h for API documentation
 */

#include "cognitive/mirror_neurons/nimcp_mirror_hippocampus_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(mirror_hippocampus_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "MIRROR_HIPPOCAMPUS_BRIDGE"

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_hippocampus_bridge)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MIRROR_HIPPO_LOG_TAG "mirror_hippo_bridge"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* clamp_f replaced by nimcp_myelin_clamp (B22 Upgrade) */

/**
 * @brief Compute similarity between two action feature vectors
 */
static float compute_action_similarity(const action_t* a, const action_t* b) {
    if (!a || !b) return 0.0f;

    /* Simple cosine similarity on features */
    uint32_t min_features = (a->num_features < b->num_features) ?
                           a->num_features : b->num_features;

    if (min_features == 0) {
        /* Fall back to action ID match */
        return (a->action_id == b->action_id) ? 1.0f : 0.0f;
    }

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < min_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_features > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)min_features);
        }

        dot += a->features[i] * b->features[i];
        norm_a += a->features[i] * a->features[i];
        norm_b += b->features[i] * b->features[i];
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 0.001f || norm_b < 0.001f) {
        return 0.0f;
    }

    return dot / (norm_a * norm_b);
}

/**
 * @brief Compute similarity between action and observation record
 */
static float compute_observation_similarity(const action_t* action,
                                            const action_observation_t* obs) {
    if (!action || !obs) return 0.0f;

    /* Action ID match is highest weight */
    float id_match = (action->action_id == obs->action_id) ? 0.5f : 0.0f;

    /* Feature similarity */
    uint32_t min_features = (action->num_features < obs->num_features) ?
                           action->num_features : obs->num_features;

    float feature_sim = 0.0f;
    if (min_features > 0) {
        float dot = 0.0f;
        float norm_a = 0.0f;
        float norm_b = 0.0f;

        for (uint32_t i = 0; i < min_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && min_features > 256) {
                mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                 (float)(i + 1) / (float)min_features);
            }

            dot += action->features[i] * obs->features[i];
            norm_a += action->features[i] * action->features[i];
            norm_b += obs->features[i] * obs->features[i];
        }

        norm_a = sqrtf(norm_a);
        norm_b = sqrtf(norm_b);

        if (norm_a > 0.001f && norm_b > 0.001f) {
            feature_sim = dot / (norm_a * norm_b);
        }
    }

    return id_match + feature_sim * 0.5f;
}

/**
 * @brief Compute similarity between episode and action cue
 */
static float compute_episode_similarity(const action_episode_t* episode,
                                        const action_t* cue) {
    if (!episode || !cue || episode->num_actions == 0) return 0.0f;

    /* Find best matching action in episode */
    float best_sim = 0.0f;
    for (uint32_t i = 0; i < episode->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && episode->num_actions > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)episode->num_actions);
        }

        float sim = compute_observation_similarity(cue, &episode->actions[i]);
        if (sim > best_sim) {
            best_sim = sim;
        }
    }

    /* Weight by episode strength */
    return best_sim * episode->episode_strength;
}

/**
 * @brief Find episode by ID
 */
static int32_t find_episode_by_id(const mirror_hippocampus_bridge_t* bridge,
                                   uint32_t episode_id) {
    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        if (bridge->episodes[i].episode_id == episode_id) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_action_similarity: validation failed");
    return -1;
}

/**
 * @brief Get next available episode slot
 */
static int32_t get_free_episode_slot(mirror_hippocampus_bridge_t* bridge) {
    if (bridge->num_episodes < bridge->max_episodes) {
        return (int32_t)(bridge->num_episodes++);
    }

    /* No free slots - find weakest episode to replace */
    float min_strength = 1.0f;
    int32_t min_idx = -1;

    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        if (!bridge->episodes[i].is_consolidated &&
            bridge->episodes[i].episode_strength < min_strength) {
            min_strength = bridge->episodes[i].episode_strength;
            min_idx = (int32_t)i;
        }
    }

    if (min_idx >= 0) {
        /* Free the old episode's actions */
        if (bridge->episodes[min_idx].actions) {
            nimcp_free(bridge->episodes[min_idx].actions);
            bridge->episodes[min_idx].actions = NULL;
        }
        bridge->stats.episodes_forgotten++;
    }

    return min_idx;
}

/**
 * @brief Convert action to observation record
 */
static void action_to_observation(const action_t* action,
                                  action_observation_t* obs) {
    obs->action_id = action->action_id;
    obs->agent_id = action->agent_id;
    obs->activation = action->confidence;
    obs->num_features = (action->num_features <= 32) ? action->num_features : 32;
    memcpy(obs->features, action->features, obs->num_features * sizeof(float));
    obs->timestamp_ms = action->timestamp;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int mirror_hippocampus_bridge_default_config(mirror_hippocampus_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Encoding parameters */
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_default_config", 0.0f);


    config->encoding_threshold = MIRROR_HIPPO_ENCODE_THRESHOLD;
    config->sequence_gap_ms = 2000;
    config->min_sequence_length = 2;
    config->encode_single_actions = false;

    /* Retrieval parameters */
    config->retrieval_threshold = MIRROR_HIPPO_RETRIEVAL_THRESHOLD;
    config->max_retrievals = 5;
    config->enable_auto_retrieval = true;

    /* Consolidation parameters */
    config->enable_replay = true;
    config->consolidation_threshold = 0.7f;
    config->replay_strengthening = 0.1f;

    /* Memory parameters */
    config->enable_decay = true;
    config->decay_rate = 0.001f;
    config->emotional_boost = 1.5f;

    /* Integration enables */
    config->enable_social_tagging = true;
    config->enable_goal_association = true;

    return 0;
}

mirror_hippocampus_bridge_t* mirror_hippocampus_bridge_create(
    const mirror_hippocampus_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_create", 0.0f);


    LOG_DEBUG("Creating mirror-hippocampus bridge");

    /* Allocate bridge */
    mirror_hippocampus_bridge_t* bridge = nimcp_calloc(1, sizeof(mirror_hippocampus_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate mirror-hippocampus bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_hippocampus_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_MIRROR_HIPPOCAMPUS_BRIDGE,
                         "mirror_hippocampus_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_hippocampus_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        mirror_hippocampus_bridge_default_config(&bridge->config);
    }

    /* Allocate episode storage */
    bridge->max_episodes = MIRROR_HIPPO_MAX_EPISODES;
    bridge->episodes = nimcp_calloc(bridge->max_episodes, sizeof(action_episode_t));
    if (!bridge->episodes) {
        LOG_ERROR("Failed to allocate episode storage");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_hippocampus_bridge_create: bridge->episodes is NULL");
        return NULL;
    }
    bridge->num_episodes = 0;
    bridge->next_episode_id = 1;

    /* Allocate current sequence buffer */
    bridge->current_sequence_max = MIRROR_HIPPO_MAX_SEQUENCE_LENGTH;
    bridge->current_sequence = nimcp_calloc(bridge->current_sequence_max,
                                            sizeof(action_observation_t));
    if (!bridge->current_sequence) {
        LOG_ERROR("Failed to allocate sequence buffer");
        nimcp_free(bridge->episodes);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_hippocampus_bridge_create: bridge->current_sequence is NULL");
        return NULL;
    }
    bridge->current_sequence_length = 0;
    bridge->sequence_start_ms = 0;
    bridge->last_action_ms = 0;
    bridge->current_demonstrator = 0;

    /* Initialize replay state */
    bridge->replay_active = false;
    bridge->replay_episode_id = 0;
    bridge->replay_position = 0;
    bridge->replay_speed = 1.0f;

    /* Initialize timing */
    bridge->last_update_ms = 0;

    LOG_INFO("Mirror-hippocampus bridge created (max_episodes=%u)",
             bridge->max_episodes);

    return bridge;
}

void mirror_hippocampus_bridge_destroy(mirror_hippocampus_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_hippocampus");

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_destroy", 0.0f);


    LOG_DEBUG("Destroying mirror-hippocampus bridge");

    /* Free episode action arrays */
    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        if (bridge->episodes[i].actions) {
            nimcp_free(bridge->episodes[i].actions);
        }
    }

    /* Free storage */
    if (bridge->current_sequence) {
        nimcp_free(bridge->current_sequence);
    }
    if (bridge->episodes) {
        nimcp_free(bridge->episodes);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int mirror_hippocampus_bridge_connect_mirror(
    mirror_hippocampus_bridge_t* bridge,
    mirror_neurons_t mirror
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_connect_mirror", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(mirror);

    bridge->mirror = mirror;
    bridge->base.system_a = mirror;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->base.system_b_connected;

    LOG_DEBUG("Connected mirror neurons to mirror-hippocampus bridge");
    return 0;
}

int mirror_hippocampus_bridge_connect_hippocampus(
    mirror_hippocampus_bridge_t* bridge,
    hippocampus_adapter_t* hippocampus
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_connect_hippocampus", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(hippocampus);

    bridge->hippocampus = hippocampus;
    bridge->base.system_b = hippocampus;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected;

    LOG_DEBUG("Connected hippocampus to mirror-hippocampus bridge");
    return 0;
}

/* ============================================================================
 * Episode Encoding API Implementation
 * ============================================================================ */

int mirror_hippocampus_store_action(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* action
) {
    if (!bridge || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_store_action: required parameter is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));

    /* Check encoding threshold */
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_s", 0.0f);


    if (action->confidence < bridge->config.encoding_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_store_action: validation failed");
        return -1;
    }

    uint64_t current_time = bridge->last_update_ms;

    /* Check if this continues current sequence or starts new */
    if (bridge->current_sequence_length > 0) {
        uint64_t gap = current_time - bridge->last_action_ms;
        if (gap > bridge->config.sequence_gap_ms) {
            /* Gap too large - complete current sequence and start new */
            if (bridge->current_sequence_length >= bridge->config.min_sequence_length) {
                mirror_hippocampus_complete_sequence(bridge);
            } else {
                /* Discard incomplete sequence */
                bridge->current_sequence_length = 0;
            }
        }
    }

    /* Start new sequence if needed */
    if (bridge->current_sequence_length == 0) {
        bridge->sequence_start_ms = current_time;
        bridge->current_demonstrator = action->agent_id;
    }

    /* Check sequence capacity */
    if (bridge->current_sequence_length >= bridge->current_sequence_max) {
        /* Sequence full - complete it */
        mirror_hippocampus_complete_sequence(bridge);
    }

    /* Add action to sequence */
    action_observation_t* obs = &bridge->current_sequence[bridge->current_sequence_length];
    action_to_observation(action, obs);
    obs->timestamp_ms = current_time;

    bridge->current_sequence_length++;
    bridge->last_action_ms = current_time;
    bridge->state.sequence_in_progress = true;
    bridge->state.current_sequence_length = bridge->current_sequence_length;

    bridge->stats.total_actions_stored++;
    bridge->effects.actions_stored++;

    LOG_DEBUG("Stored action %u in sequence (length=%u)",
              action->action_id, bridge->current_sequence_length);

    return 0;
}

uint32_t mirror_hippocampus_complete_sequence(
    mirror_hippocampus_bridge_t* bridge
) {
    if (!bridge || bridge->current_sequence_length == 0) {
        if (!bridge) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_complete_sequence: bridge is NULL");
        return UINT32_MAX;
    }

    /* Check minimum length */
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_c", 0.0f);


    if (bridge->current_sequence_length < bridge->config.min_sequence_length &&
        !bridge->config.encode_single_actions) {
        bridge->current_sequence_length = 0;
        bridge->state.sequence_in_progress = false;
        return UINT32_MAX;
    }

    /* Get episode slot */
    int32_t slot = get_free_episode_slot(bridge);
    if (slot < 0) {
        LOG_WARN("No episode slots available");
        bridge->current_sequence_length = 0;
        return UINT32_MAX;
    }

    /* Create episode */
    action_episode_t* episode = &bridge->episodes[slot];

    episode->episode_id = bridge->next_episode_id++;
    episode->hippocampus_memory_id = 0; /* Will be set when encoded to hippocampus */

    /* Copy actions */
    episode->max_actions = bridge->current_sequence_length;
    episode->actions = nimcp_calloc(episode->max_actions, sizeof(action_observation_t));
    if (!episode->actions) {
        LOG_ERROR("Failed to allocate episode actions");
        return UINT32_MAX;
    }
    memcpy(episode->actions, bridge->current_sequence,
           bridge->current_sequence_length * sizeof(action_observation_t));
    episode->num_actions = bridge->current_sequence_length;

    /* Set context */
    episode->demonstrator_id = bridge->current_demonstrator;
    episode->social_salience = 0.5f; /* Default */
    episode->emotional_valence = 0.0f; /* Neutral */

    /* Set metadata */
    episode->start_time_ms = bridge->sequence_start_ms;
    episode->end_time_ms = bridge->last_action_ms;
    episode->episode_strength = 1.0f; /* Full strength on creation */
    episode->is_consolidated = false;
    episode->replay_count = 0;
    episode->last_retrieval_ms = 0;
    episode->retrieval_count = 0;

    /* Encode to hippocampus if connected */
    if (bridge->hippocampus) {
        /* Create feature vector from sequence */
        float features[64];
        uint32_t num_features = 0;

        /* Average features across actions */
        for (uint32_t f = 0; f < 32 && f < episode->actions[0].num_features; f++) {
            float sum = 0.0f;
            for (uint32_t a = 0; a < episode->num_actions; a++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a & 0xFF) == 0 && episode->num_actions > 256) {
                    mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                     (float)(a + 1) / (float)episode->num_actions);
                }

                if (f < episode->actions[a].num_features) {
                    sum += episode->actions[a].features[f];
                }
            }
            features[num_features++] = sum / episode->num_actions;
        }

        /* Encode in hippocampus */
        uint32_t mem_id = hippocampus_encode_memory(
            bridge->hippocampus,
            features,
            num_features,
            NULL, /* No spatial location */
            episode->emotional_valence
        );

        if (mem_id != 0) {
            episode->hippocampus_memory_id = mem_id;
        }
    }

    /* Update statistics */
    bridge->stats.total_episodes_encoded++;
    bridge->stats.avg_sequence_length =
        (bridge->stats.avg_sequence_length * (bridge->stats.total_episodes_encoded - 1) +
         episode->num_actions) / bridge->stats.total_episodes_encoded;
    bridge->effects.episodes_encoded++;

    /* Clear current sequence */
    bridge->current_sequence_length = 0;
    bridge->state.sequence_in_progress = false;
    bridge->state.current_sequence_length = 0;

    LOG_DEBUG("Created episode %u with %u actions", episode->episode_id, episode->num_actions);

    return episode->episode_id;
}

uint32_t mirror_hippocampus_store_demonstration(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* actions,
    uint32_t num_actions,
    uint32_t demonstrator_id
) {
    if (!bridge || !actions || num_actions == 0) {
        if (!bridge || !actions) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_store_demonstration: required parameter is NULL");
        return UINT32_MAX;
    }
    BRIDGE_BBB_VALIDATE(bridge, actions, num_actions * sizeof(*actions));

    /* Store each action in sequence */
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_s", 0.0f);


    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        /* Temporarily set agent_id to demonstrator */
        action_t action = actions[i];
        action.agent_id = demonstrator_id;
        action.timestamp = bridge->last_update_ms + i * 100; /* 100ms spacing */
        action.confidence = 1.0f; /* Full confidence for demonstration */

        if (mirror_hippocampus_store_action(bridge, &action) != 0) {
            /* Clear partial sequence on failure */
            bridge->current_sequence_length = 0;
            return UINT32_MAX;
        }
    }

    /* Complete the sequence */
    return mirror_hippocampus_complete_sequence(bridge);
}

/* ============================================================================
 * Episode Retrieval API Implementation
 * ============================================================================ */

int mirror_hippocampus_retrieve_by_action(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* cue,
    episode_retrieval_result_t* result
) {
    if (!bridge || !cue || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_retrieve_by_action: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_r", 0.0f);


    memset(result, 0, sizeof(episode_retrieval_result_t));

    /* Find similar episodes */
    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        action_episode_t* episode = &bridge->episodes[i];
        if (episode->num_actions == 0) continue;

        float sim = compute_episode_similarity(episode, cue);

        if (sim >= bridge->config.retrieval_threshold) {
            /* Insert in sorted order */
            uint32_t insert_pos = result->num_retrieved;
            for (uint32_t j = 0; j < result->num_retrieved; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && result->num_retrieved > 256) {
                    mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                     (float)(j + 1) / (float)result->num_retrieved);
                }

                if (sim > result->similarities[j]) {
                    insert_pos = j;
                    break;
                }
            }

            if (insert_pos < MIRROR_HIPPO_MAX_RETRIEVALS) {
                /* Shift existing entries down */
                for (uint32_t j = MIRROR_HIPPO_MAX_RETRIEVALS - 1; j > insert_pos; j--) {
                    result->episodes[j] = result->episodes[j-1];
                    result->similarities[j] = result->similarities[j-1];
                }

                /* Insert new entry */
                result->episodes[insert_pos] = episode;
                result->similarities[insert_pos] = sim;

                if (result->num_retrieved < MIRROR_HIPPO_MAX_RETRIEVALS) {
                    result->num_retrieved++;
                }
            }
        }
    }

    /* Update retrieval metadata */
    for (uint32_t i = 0; i < result->num_retrieved; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result->num_retrieved > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)result->num_retrieved);
        }

        result->episodes[i]->last_retrieval_ms = bridge->last_update_ms;
        result->episodes[i]->retrieval_count++;

        /* Strengthen retrieved memories */
        result->episodes[i]->episode_strength = nimcp_myelin_clamp(
            result->episodes[i]->episode_strength + 0.05f, 0.0f, 1.0f
        );
    }

    result->best_similarity = (result->num_retrieved > 0) ?
                              result->similarities[0] : 0.0f;

    /* Update statistics */
    bridge->stats.total_retrievals++;
    if (result->num_retrieved > 0) {
        bridge->stats.successful_retrievals++;
        bridge->stats.avg_retrieval_similarity =
            (bridge->stats.avg_retrieval_similarity *
             (bridge->stats.total_retrievals - 1) +
             result->best_similarity) / bridge->stats.total_retrievals;
    }
    bridge->effects.retrievals_performed++;
    bridge->effects.avg_retrieval_similarity = result->best_similarity;
    bridge->effects.context_retrieved = (result->num_retrieved > 0);

    return 0;
}

int mirror_hippocampus_retrieve_by_demonstrator(
    mirror_hippocampus_bridge_t* bridge,
    uint32_t demonstrator_id,
    episode_retrieval_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_retrieve_by_demonstrator: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_r", 0.0f);


    memset(result, 0, sizeof(episode_retrieval_result_t));

    /* Find episodes by demonstrator */
    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        action_episode_t* episode = &bridge->episodes[i];
        if (episode->demonstrator_id != demonstrator_id) continue;
        if (episode->num_actions == 0) continue;

        if (result->num_retrieved < MIRROR_HIPPO_MAX_RETRIEVALS) {
            result->episodes[result->num_retrieved] = episode;
            result->similarities[result->num_retrieved] = episode->episode_strength;
            result->num_retrieved++;
        }
    }

    result->best_similarity = (result->num_retrieved > 0) ?
                              result->similarities[0] : 0.0f;

    bridge->stats.total_retrievals++;
    bridge->effects.retrievals_performed++;

    return 0;
}

int mirror_hippocampus_retrieve_by_sequence(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* actions,
    uint32_t num_actions,
    episode_retrieval_result_t* result
) {
    if (!bridge || !actions || num_actions == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_retrieve_by_sequence: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_r", 0.0f);


    memset(result, 0, sizeof(episode_retrieval_result_t));

    /* Find episodes with similar sequences */
    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        action_episode_t* episode = &bridge->episodes[i];
        if (episode->num_actions == 0) continue;

        /* Compute sequence similarity */
        float total_sim = 0.0f;
        uint32_t matches = 0;

        for (uint32_t q = 0; q < num_actions; q++) {
            /* Phase 8: Loop progress heartbeat */
            if ((q & 0xFF) == 0 && num_actions > 256) {
                mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                 (float)(q + 1) / (float)num_actions);
            }

            float best_match = 0.0f;
            for (uint32_t e = 0; e < episode->num_actions; e++) {
                /* Phase 8: Loop progress heartbeat */
                if ((e & 0xFF) == 0 && episode->num_actions > 256) {
                    mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                     (float)(e + 1) / (float)episode->num_actions);
                }

                float sim = compute_observation_similarity(&actions[q],
                                                           &episode->actions[e]);
                if (sim > best_match) {
                    best_match = sim;
                }
            }
            total_sim += best_match;
            if (best_match > 0.5f) matches++;
        }

        float avg_sim = total_sim / (fabsf(num_actions) > 1e-7f ? num_actions : 1e-7f);
        float match_ratio = (float)matches / (fabsf(num_actions) > 1e-7f ? num_actions : 1e-7f);
        float final_sim = (avg_sim + match_ratio) * 0.5f * episode->episode_strength;

        if (final_sim >= bridge->config.retrieval_threshold) {
            if (result->num_retrieved < MIRROR_HIPPO_MAX_RETRIEVALS) {
                result->episodes[result->num_retrieved] = episode;
                result->similarities[result->num_retrieved] = final_sim;
                result->num_retrieved++;
            }
        }
    }

    result->best_similarity = (result->num_retrieved > 0) ?
                              result->similarities[0] : 0.0f;

    bridge->stats.total_retrievals++;
    bridge->effects.retrievals_performed++;

    return 0;
}

int mirror_hippocampus_get_episode(
    const mirror_hippocampus_bridge_t* bridge,
    uint32_t episode_id,
    action_episode_t** episode
) {
    if (!bridge || !episode) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_get_episode: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_g", 0.0f);


    int32_t idx = find_episode_by_id(bridge, episode_id);
    if (idx < 0) {
        *episode = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_get_episode: validation failed");
        return -1;
    }

    *episode = &((mirror_hippocampus_bridge_t*)bridge)->episodes[idx];
    return 0;
}

/* ============================================================================
 * Replay API Implementation
 * ============================================================================ */

int mirror_hippocampus_request_replay(
    mirror_hippocampus_bridge_t* bridge,
    const replay_request_t* request
) {
    if (!bridge || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_request_replay: required parameter is NULL");
        return -1;
    }

    if (!bridge->config.enable_replay) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_r", 0.0f);


    uint32_t episode_id = request->episode_id;

    /* Auto-select strongest episode if not specified */
    if (episode_id == 0) {
        float best_strength = 0.0f;
        for (uint32_t i = 0; i < bridge->num_episodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
                mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                 (float)(i + 1) / (float)bridge->num_episodes);
            }

            if (bridge->episodes[i].episode_strength > best_strength &&
                bridge->episodes[i].num_actions > 0) {
                best_strength = bridge->episodes[i].episode_strength;
                episode_id = bridge->episodes[i].episode_id;
            }
        }
    }

    if (episode_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_request_replay: episode_id is zero");
        return -1; /* No episodes available */
    }

    int32_t idx = find_episode_by_id(bridge, episode_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_request_replay: validation failed");
        return -1;
    }

    /* Start replay */
    bridge->replay_active = true;
    bridge->replay_episode_id = episode_id;
    bridge->replay_position = request->reverse ?
                              bridge->episodes[idx].num_actions - 1 : 0;
    bridge->replay_speed = request->speed_factor;

    bridge->state.replay_active = true;
    bridge->effects.replay_activation = request->replay_strength;

    LOG_DEBUG("Started replay of episode %u (reverse=%d, speed=%.1f)",
              episode_id, request->reverse, request->speed_factor);

    return 0;
}

int mirror_hippocampus_step_replay(
    mirror_hippocampus_bridge_t* bridge,
    action_t* out_action
) {
    if (!bridge || !bridge->replay_active) {
        if (!bridge) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_step_replay: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_s", 0.0f);


    int32_t ep_idx = find_episode_by_id(bridge, bridge->replay_episode_id);
    if (ep_idx < 0) {
        bridge->replay_active = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_step_replay: validation failed");
        return -1;
    }

    action_episode_t* episode = &bridge->episodes[ep_idx];

    if (bridge->replay_position >= episode->num_actions) {
        /* Replay complete */
        bridge->replay_active = false;
        bridge->state.replay_active = false;
        episode->replay_count++;

        /* Strengthen episode from replay */
        episode->episode_strength = nimcp_myelin_clamp(
            episode->episode_strength + bridge->config.replay_strengthening,
            0.0f, 1.0f
        );

        bridge->stats.total_replays++;
        bridge->stats.avg_replay_strengthening =
            (bridge->stats.avg_replay_strengthening *
             (bridge->stats.total_replays - 1) +
             bridge->config.replay_strengthening) / bridge->stats.total_replays;
        bridge->effects.replays_performed++;

        return 1; /* Complete */
    }

    /* Get current action */
    action_observation_t* obs = &episode->actions[bridge->replay_position];

    /* Output action if requested */
    if (out_action) {
        out_action->action_id = obs->action_id;
        out_action->agent_id = obs->agent_id;
        out_action->confidence = obs->activation * bridge->effects.replay_activation;
        out_action->num_features = obs->num_features;
        memcpy(out_action->features, obs->features, obs->num_features * sizeof(float));
        out_action->timestamp = bridge->last_update_ms;
    }

    /* Activate mirror neurons if connected */
    if (bridge->mirror) {
        action_t replay_action = {
            .action_id = obs->action_id,
            .agent_id = 0, /* Self during replay */
            .confidence = obs->activation * bridge->effects.replay_activation
        };
        memcpy(replay_action.features, obs->features, obs->num_features * sizeof(float));
        replay_action.num_features = obs->num_features;

        /* This is conceptual - would need mirror_neurons_replay_action() or similar */
        /* For now, just track that replay occurred */
    }

    /* Advance position */
    bridge->replay_position++;

    return 0; /* Continue */
}

int mirror_hippocampus_stop_replay(
    mirror_hippocampus_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_s", 0.0f);


    bridge->replay_active = false;
    bridge->replay_episode_id = 0;
    bridge->replay_position = 0;
    bridge->state.replay_active = false;

    return 0;
}

bool mirror_hippocampus_is_replaying(
    const mirror_hippocampus_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_i", 0.0f);


    return bridge ? bridge->replay_active : false;
}

uint32_t mirror_hippocampus_consolidate(
    mirror_hippocampus_bridge_t* bridge,
    float strength_threshold
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_consolidate: bridge is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_mirror_hippocampus_c", 0.0f);


    uint32_t consolidated = 0;

    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        action_episode_t* episode = &bridge->episodes[i];

        if (episode->is_consolidated) continue;
        if (episode->episode_strength < strength_threshold) continue;

        /* Request replay for consolidation */
        if (bridge->config.enable_replay) {
            replay_request_t request = {
                .episode_id = episode->episode_id,
                .reverse = false,
                .speed_factor = 2.0f, /* Fast replay */
                .replay_strength = 0.8f
            };

            /* Step through replay */
            if (mirror_hippocampus_request_replay(bridge, &request) == 0) {
                while (mirror_hippocampus_step_replay(bridge, NULL) == 0) {
                    /* Continue stepping */
                }
            }
        }

        /* Mark as consolidated */
        episode->is_consolidated = true;
        consolidated++;
        bridge->stats.episodes_consolidated++;
    }

    return consolidated;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

int mirror_hippocampus_bridge_update(
    mirror_hippocampus_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_update", 0.0f);


    bridge->last_update_ms += delta_ms;

    /* Reset per-cycle effects */
    bridge->effects.episodes_encoded = 0;
    bridge->effects.actions_stored = 0;
    bridge->effects.retrievals_performed = 0;
    bridge->effects.replays_performed = 0;

    /* Check for sequence timeout */
    if (bridge->current_sequence_length > 0) {
        uint64_t gap = bridge->last_update_ms - bridge->last_action_ms;
        if (gap > bridge->config.sequence_gap_ms) {
            if (bridge->current_sequence_length >= bridge->config.min_sequence_length) {
                mirror_hippocampus_complete_sequence(bridge);
            } else {
                bridge->current_sequence_length = 0;
                bridge->state.sequence_in_progress = false;
            }
        }
    }

    /* Apply memory decay */
    if (bridge->config.enable_decay) {
        float decay = bridge->config.decay_rate * (delta_ms / 1000.0f);
        for (uint32_t i = 0; i < bridge->num_episodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
                mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                                 (float)(i + 1) / (float)bridge->num_episodes);
            }

            action_episode_t* episode = &bridge->episodes[i];
            if (episode->is_consolidated) continue;

            episode->episode_strength -= decay;
            if (episode->episode_strength < 0.0f) {
                episode->episode_strength = 0.0f;
            }

            /* Forget very weak episodes */
            if (episode->episode_strength < 0.1f && episode->num_actions > 0) {
                nimcp_free(episode->actions);
                episode->actions = NULL;
                episode->num_actions = 0;
                bridge->stats.episodes_forgotten++;
            }
        }
    }

    /* Update state */
    bridge->state.active_observations = bridge->current_sequence_length;
    bridge->state.total_episodes = bridge->num_episodes;

    /* Count consolidated and calculate average strength */
    uint32_t consolidated_count = 0;
    float total_strength = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < bridge->num_episodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_episodes > 256) {
            mirror_hippocampus_bridge_heartbeat("mirror_hippo_loop",
                             (float)(i + 1) / (float)bridge->num_episodes);
        }

        if (bridge->episodes[i].is_consolidated) {
            consolidated_count++;
        }
        if (bridge->episodes[i].num_actions > 0) {
            total_strength += bridge->episodes[i].episode_strength;
            active_count++;
        }
    }

    bridge->state.consolidated_episodes = consolidated_count;
    bridge->state.avg_episode_strength = (active_count > 0) ?
                                          total_strength / (active_count > 0 ? active_count : 1) : 0.0f;
    bridge->state.pending_encoding = 0; /* Not implementing queue */

    /* Update average encoding strength */
    bridge->effects.avg_encoding_strength = bridge->state.avg_episode_strength;

    /* Record update */
    bridge_base_record_update(&bridge->base);

    return 0;
}

/* ============================================================================
 * State/Stats API Implementation
 * ============================================================================ */

int mirror_hippocampus_bridge_get_state(
    const mirror_hippocampus_bridge_t* bridge,
    mirror_hippocampus_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_bridge_get_state: required parameter is NULL");
        return -1;
    }

    *state = bridge->state;
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_get_state", 0.0f);


    return 0;
}

int mirror_hippocampus_bridge_get_effects(
    const mirror_hippocampus_bridge_t* bridge,
    mirror_hippocampus_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_bridge_get_effects: required parameter is NULL");
        return -1;
    }

    *effects = bridge->effects;
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_get_effects", 0.0f);


    return 0;
}

int mirror_hippocampus_bridge_get_stats(
    const mirror_hippocampus_bridge_t* bridge,
    mirror_hippocampus_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hippocampus_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_get_stats", 0.0f);


    return 0;
}

int mirror_hippocampus_bridge_reset_stats(
    mirror_hippocampus_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hippocampus_bridge_heartbeat("mirror_hippo_reset_stats", 0.0f);


    memset(&bridge->stats, 0, sizeof(mirror_hippocampus_stats_t));
    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(mirror_hippocampus_bridge, mirror_hippocampus_bridge_t)

//=============================================================================
// Instance Health Agent Setter (B22 Upgrade)
//=============================================================================

void mirror_hippocampus_bridge_set_instance_health_agent(
    mirror_hippocampus_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B22 Upgrade)
//=============================================================================

int mirror_hippocampus_bridge_training_begin(mirror_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hippocampus_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_hippocampus_bridge_heartbeat_instance(bridge->health_agent, "mirror_hippocampus_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_hippocampus_bridge_training_end(mirror_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hippocampus_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_hippocampus_bridge_heartbeat_instance(bridge->health_agent, "mirror_hippocampus_bridge_training_end", 1.0f);
    return 0;
}

int mirror_hippocampus_bridge_training_step(mirror_hippocampus_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hippocampus_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "mirror_hippocampus_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "mirror_hippocampus_bridge_training_step");
    mirror_hippocampus_bridge_heartbeat_instance(bridge->health_agent, "mirror_hippocampus_bridge_training_step", progress);

    /* Notify coordinator of step cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
