/**
 * @file nimcp_autobiographical_fep_bridge.h
 * @brief Free Energy Principle - Autobiographical Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and autobiographical memory
 * WHY:  Episodic memories are training data for generative models; memorable experiences
 *       are those with high prediction error (surprise)
 * HOW:  High-PE episodes stored as autobiographical memories; memories update FEP
 *       generative model priors
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EPISODIC MEMORY AS GENERATIVE MODEL TRAINING:
 * --------------------------------------------
 * 1. Memorable Events as High-Surprise Episodes:
 *    - High prediction error → hippocampal encoding
 *    - Surprise enhances memory consolidation
 *    - Reference: Lisman & Grace (2005) "The hippocampal-VTA loop"
 *
 * 2. Memory Replay Updates Priors:
 *    - Sleep replay → generative model refinement
 *    - Episodic memories provide training examples
 *    - Reference: Friston & Buzsaki (2016) "The functional anatomy of time"
 *
 * 3. Autobiographical Self as Generative Model:
 *    - Personal history shapes prior beliefs
 *    - Identity-defining memories structure model
 *    - Reference: Conway (2005) "Memory and the self"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AUTOBIOGRAPHICAL_FEP_BRIDGE_H
#define NIMCP_AUTOBIOGRAPHICAL_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/nimcp_autobiographical_memory.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants & Structures
 * ============================================================================ */

#define SURPRISE_MEMORY_THRESHOLD         5.0f
#define MEMORY_MODEL_UPDATE_RATE          0.1f

typedef struct autobiographical_fep_bridge autobiographical_fep_bridge_t;

typedef struct {
    float surprise_memory_threshold;
    float memory_importance_weight;
    float model_update_rate;
    float prior_influence_rate;
    bool enable_surprise_encoding;
    bool enable_memory_replay;
    bool enable_prior_updates;
} autobiographical_fep_config_t;

typedef struct {
    float memory_encoding_boost;
    float model_prior_adjustment;
    float replay_frequency;
} autobiographical_fep_effects_t;

typedef struct {
    float current_surprise_level;
    uint32_t memories_encoded;
    uint32_t model_updates_from_memory;
    float avg_memory_importance;
} autobiographical_fep_state_t;

typedef struct {
    uint64_t total_surprise_encodings;
    uint64_t total_memory_replays;
    uint64_t total_prior_updates;
    float avg_encoding_surprise;
} autobiographical_fep_stats_t;

struct autobiographical_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    autobiographical_fep_config_t config;
    fep_system_t* fep_system;
    autobiographical_memory_t autobio_system;
    autobiographical_fep_effects_t effects;
    autobiographical_fep_state_t state;
    autobiographical_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int autobiographical_fep_bridge_default_config(autobiographical_fep_config_t* config);
autobiographical_fep_bridge_t* autobiographical_fep_bridge_create(const autobiographical_fep_config_t* config);
void autobiographical_fep_bridge_destroy(autobiographical_fep_bridge_t* bridge);

int autobiographical_fep_bridge_connect_fep(autobiographical_fep_bridge_t* bridge, fep_system_t* fep);
int autobiographical_fep_bridge_connect_autobiographical(autobiographical_fep_bridge_t* bridge, autobiographical_memory_t autobio);

int autobiographical_fep_encode_surprising_episode(autobiographical_fep_bridge_t* bridge);
int autobiographical_fep_replay_memories(autobiographical_fep_bridge_t* bridge);
int autobiographical_fep_update_priors_from_memory(autobiographical_fep_bridge_t* bridge);

int autobiographical_fep_bridge_update(autobiographical_fep_bridge_t* bridge, uint64_t delta_ms);
int autobiographical_fep_bridge_get_state(const autobiographical_fep_bridge_t* bridge, autobiographical_fep_state_t* state);
int autobiographical_fep_bridge_get_stats(const autobiographical_fep_bridge_t* bridge, autobiographical_fep_stats_t* stats);

int autobiographical_fep_bridge_connect_bio_async(autobiographical_fep_bridge_t* bridge);
int autobiographical_fep_bridge_disconnect_bio_async(autobiographical_fep_bridge_t* bridge);
bool autobiographical_fep_bridge_is_bio_async_connected(const autobiographical_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIOGRAPHICAL_FEP_BRIDGE_H */
