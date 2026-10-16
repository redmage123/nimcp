//=============================================================================
// nimcp_creative_memory_bridge.c - Creative-Memory Integration Bridge
//=============================================================================
/**
 * @file nimcp_creative_memory_bridge.c
 * @brief Bridge connecting creative system to memory/hippocampus
 *
 * WHAT: Manages storage and retrieval of artistic experiences
 * WHY:  Learning from and building upon past artistic experiences
 * HOW:  Interface to hippocampus for creative episodic and semantic memory
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/appreciation/nimcp_creative_memory_bridge.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "CREATIVE_MEM_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(creative_memory_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_creative_memory_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_creative_memory_bridge_mesh_registry = NULL;

nimcp_error_t creative_memory_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_creative_memory_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "creative_memory_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "creative_memory_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_creative_memory_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_creative_memory_bridge_mesh_registry = registry;
    return err;
}

void creative_memory_bridge_mesh_unregister(void) {
    if (g_creative_memory_bridge_mesh_registry && g_creative_memory_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_creative_memory_bridge_mesh_registry, g_creative_memory_bridge_mesh_id);
        g_creative_memory_bridge_mesh_id = 0;
        g_creative_memory_bridge_mesh_registry = NULL;
    }
}


//=============================================================================
// Config Defaults
//=============================================================================

void creative_memory_bridge_config_defaults(creative_memory_bridge_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(creative_memory_bridge_config_t));

    /* Capacity limits */
    config->max_episodic_memories = 1000;
    config->max_semantic_memories = 500;
    config->max_preferences = 256;

    /* Consolidation settings */
    config->salience_threshold = 0.3f;
    config->enable_consolidation = true;
    config->forgetting_rate = 0.01f;  /* 1% per day */

    /* Retrieval settings */
    config->max_retrieval_results = 20;
    config->relevance_threshold = 0.4f;

    /* Integration */
    config->use_hippocampus = false;
    config->use_semantic_memory = false;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static uint64_t get_current_time(void) {
    return (uint64_t)time(NULL);
}

static uint64_t generate_memory_id(creative_memory_bridge_t* bridge) {
    static uint64_t counter = 0;
    return (get_current_time() << 16) | (++counter & 0xFFFF);
}

static float compute_emotion_similarity(const aesthetic_emotional_response_t* a,
                                         const aesthetic_emotional_response_t* b) {
    if (!a || !b) return 0.0f;

    /* Compute dot product of emotion vectors */
    float dot = 0.0f;
    dot += a->joy * b->joy;
    dot += a->trust * b->trust;
    dot += a->fear * b->fear;
    dot += a->surprise * b->surprise;
    dot += a->sadness * b->sadness;
    dot += a->anger * b->anger;
    dot += a->awe * b->awe;
    dot += a->contemplation * b->contemplation;

    /* Compute magnitudes */
    float mag_a = sqrtf(a->joy * a->joy + a->trust * a->trust +
                        a->fear * a->fear + a->surprise * a->surprise +
                        a->sadness * a->sadness + a->anger * a->anger +
                        a->awe * a->awe + a->contemplation * a->contemplation);
    float mag_b = sqrtf(b->joy * b->joy + b->trust * b->trust +
                        b->fear * b->fear + b->surprise * b->surprise +
                        b->sadness * b->sadness + b->anger * b->anger +
                        b->awe * b->awe + b->contemplation * b->contemplation);

    if (mag_a < 0.001f || mag_b < 0.001f) return 0.0f;

    return dot / (mag_a * mag_b);
}

//=============================================================================
// Lifecycle API
//=============================================================================

creative_memory_bridge_t* creative_memory_bridge_create(
    const creative_memory_bridge_config_t* config) {

    creative_memory_bridge_t* bridge = nimcp_calloc(1, sizeof(creative_memory_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate memory bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_memory_id: bridge is NULL");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        creative_memory_bridge_config_defaults(&bridge->config);
    }

    /* Allocate episodic memory storage */
    bridge->episodic_capacity = bridge->config.max_episodic_memories;
    bridge->episodic_memories = nimcp_calloc(bridge->episodic_capacity,
                                              sizeof(creative_episodic_memory_t));
    if (!bridge->episodic_memories) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate episodic memories");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_memory_id: bridge->episodic_memories is NULL");
        return NULL;
    }

    /* Allocate semantic memory storage */
    bridge->semantic_capacity = bridge->config.max_semantic_memories;
    bridge->semantic_memories = nimcp_calloc(bridge->semantic_capacity,
                                              sizeof(creative_semantic_memory_t));
    if (!bridge->semantic_memories) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate semantic memories");
        nimcp_free(bridge->episodic_memories);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_memory_id: bridge->semantic_memories is NULL");
        return NULL;
    }

    /* Allocate preference storage */
    bridge->preference_capacity = bridge->config.max_preferences;
    bridge->preferences = nimcp_calloc(bridge->preference_capacity,
                                        sizeof(creative_preference_t));
    if (!bridge->preferences) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate preferences");
        nimcp_free(bridge->semantic_memories);
        nimcp_free(bridge->episodic_memories);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_memory_id: bridge->preferences is NULL");
        return NULL;
    }

    LOG_INFO(LOG_MODULE, "Creative memory bridge created (episodic=%u, semantic=%u, prefs=%u)",
             bridge->episodic_capacity, bridge->semantic_capacity, bridge->preference_capacity);

    return bridge;
}

void creative_memory_bridge_destroy(creative_memory_bridge_t* bridge) {
    if (!bridge) return;

    /* Free style embeddings in semantic memories */
    for (uint32_t i = 0; i < bridge->semantic_count; i++) {
        style_embedding_destroy(&bridge->semantic_memories[i].style);
    }

    nimcp_free(bridge->preferences);
    nimcp_free(bridge->semantic_memories);
    nimcp_free(bridge->episodic_memories);
    nimcp_free(bridge);
    bridge = NULL;

    LOG_INFO(LOG_MODULE, "Creative memory bridge destroyed");
}

//=============================================================================
// Storage API
//=============================================================================

uint64_t creative_memory_store_episodic(creative_memory_bridge_t* bridge,
                                         art_modality_t modality,
                                         const char* work_title,
                                         const char* artist_name,
                                         const aesthetic_evaluation_t* eval,
                                         const aesthetic_emotional_response_t* emotions,
                                         float salience) {
    if (!bridge) return 0;

    /* Check salience threshold */
    if (salience < bridge->config.salience_threshold) {
        LOG_DEBUG(LOG_MODULE, "Memory below salience threshold (%.2f < %.2f)",
                  salience, bridge->config.salience_threshold);
        return 0;
    }

    /* Check capacity */
    if (bridge->episodic_count >= bridge->episodic_capacity) {
        /* Find and remove lowest salience memory */
        uint32_t min_idx = 0;
        float min_salience = 1.0f;
        for (uint32_t i = 0; i < bridge->episodic_count; i++) {
            if (bridge->episodic_memories[i].salience < min_salience) {
                min_salience = bridge->episodic_memories[i].salience;
                min_idx = i;
            }
        }

        if (salience <= min_salience) {
            LOG_DEBUG(LOG_MODULE, "New memory not salient enough to displace existing");
            return 0;
        }

        /* Shift remaining memories */
        memmove(&bridge->episodic_memories[min_idx],
                &bridge->episodic_memories[min_idx + 1],
                (bridge->episodic_count - min_idx - 1) * sizeof(creative_episodic_memory_t));
        bridge->episodic_count--;
    }

    /* Create new memory */
    creative_episodic_memory_t* mem = &bridge->episodic_memories[bridge->episodic_count];
    memset(mem, 0, sizeof(creative_episodic_memory_t));

    mem->memory_id = generate_memory_id(bridge);
    mem->modality = modality;
    if (work_title) {
        strncpy(mem->work_title, work_title, sizeof(mem->work_title) - 1);
    }
    if (artist_name) {
        strncpy(mem->artist_name, artist_name, sizeof(mem->artist_name) - 1);
    }
    if (eval) {
        mem->evaluation = *eval;
    }
    if (emotions) {
        mem->emotions = *emotions;
    }
    mem->experience_time = get_current_time();
    mem->salience = salience;
    mem->personal_significance = salience;  /* Initially same as salience */
    mem->recall_count = 0;
    mem->last_recall_time = 0;

    bridge->episodic_count++;
    bridge->total_stores++;

    LOG_DEBUG(LOG_MODULE, "Stored episodic memory: %s by %s (salience=%.2f)",
              work_title ? work_title : "Unknown",
              artist_name ? artist_name : "Unknown",
              salience);

    return mem->memory_id;
}

uint64_t creative_memory_store_semantic(creative_memory_bridge_t* bridge,
                                         art_modality_t modality,
                                         const char* concept,
                                         const char* description,
                                         const style_embedding_t* style) {
    if (!bridge || !concept) return 0;

    /* Check capacity */
    if (bridge->semantic_count >= bridge->semantic_capacity) {
        LOG_WARN(LOG_MODULE, "Semantic memory full");
        return 0;
    }

    /* Check for existing concept */
    for (uint32_t i = 0; i < bridge->semantic_count; i++) {
        if (strcmp(bridge->semantic_memories[i].concept, concept) == 0 &&
            bridge->semantic_memories[i].modality == modality) {
            /* Reinforce existing */
            bridge->semantic_memories[i].reinforcement_count++;
            bridge->semantic_memories[i].last_reinforcement = get_current_time();
            bridge->semantic_memories[i].confidence =
                fminf(1.0f, bridge->semantic_memories[i].confidence + 0.1f);
            LOG_DEBUG(LOG_MODULE, "Reinforced existing semantic memory: %s", concept);
            return bridge->semantic_memories[i].memory_id;
        }
    }

    /* Create new semantic memory */
    creative_semantic_memory_t* mem = &bridge->semantic_memories[bridge->semantic_count];
    memset(mem, 0, sizeof(creative_semantic_memory_t));

    mem->memory_id = generate_memory_id(bridge);
    mem->modality = modality;
    strncpy(mem->concept, concept, sizeof(mem->concept) - 1);
    if (description) {
        strncpy(mem->description, description, sizeof(mem->description) - 1);
    }
    if (style) {
        style_embedding_clone(style, &mem->style);
    }
    mem->confidence = 0.5f;  /* Initial confidence */
    mem->acquisition_time = get_current_time();
    mem->last_reinforcement = mem->acquisition_time;
    mem->reinforcement_count = 1;

    bridge->semantic_count++;
    bridge->total_stores++;

    LOG_DEBUG(LOG_MODULE, "Stored semantic memory: %s", concept);

    return mem->memory_id;
}

int creative_memory_update_preference(creative_memory_bridge_t* bridge,
                                       art_modality_t modality,
                                       int32_t archetype_id,
                                       float preference) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_memory_bridge_destroy: bridge is NULL");
        return -1;
    }

    /* Clamp preference */
    if (preference < -1.0f) preference = -1.0f;
    if (preference > 1.0f) preference = 1.0f;

    /* Find existing preference */
    for (uint32_t i = 0; i < bridge->preference_count; i++) {
        if (bridge->preferences[i].modality == modality &&
            bridge->preferences[i].archetype_id == archetype_id) {
            /* Update with exponential moving average */
            float alpha = 0.3f;  /* Learning rate */
            creative_preference_t* pref = &bridge->preferences[i];
            pref->preference_strength = pref->preference_strength * (1.0f - alpha) +
                                        preference * alpha;
            pref->certainty = fminf(1.0f, pref->certainty + 0.05f);
            pref->sample_count++;
            pref->last_update = get_current_time();
            return 0;
        }
    }

    /* Create new preference */
    if (bridge->preference_count >= bridge->preference_capacity) {
        LOG_WARN(LOG_MODULE, "Preference storage full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "creative_memory_bridge_destroy: capacity exceeded");
        return -1;
    }

    creative_preference_t* pref = &bridge->preferences[bridge->preference_count];
    pref->modality = modality;
    pref->archetype_id = archetype_id;
    pref->preference_strength = preference;
    pref->certainty = 0.3f;  /* Low initial certainty */
    pref->sample_count = 1;
    pref->last_update = get_current_time();

    bridge->preference_count++;

    return 0;
}

//=============================================================================
// Retrieval API
//=============================================================================

uint32_t creative_memory_recall_by_style(creative_memory_bridge_t* bridge,
                                          const style_embedding_t* style,
                                          uint32_t max_results,
                                          creative_episodic_memory_t* out) {
    if (!bridge || !style || !out) return 0;

    /* For now, return most salient memories (would use style similarity with full impl) */
    uint32_t count = 0;
    uint32_t max = max_results < bridge->episodic_count ? max_results : bridge->episodic_count;

    /* Simple copy of most recent memories */
    for (uint32_t i = 0; i < max; i++) {
        out[count++] = bridge->episodic_memories[bridge->episodic_count - 1 - i];
    }

    bridge->total_retrievals++;

    return count;
}

uint32_t creative_memory_recall_by_emotion(creative_memory_bridge_t* bridge,
                                            const aesthetic_emotional_response_t* emotion,
                                            uint32_t max_results,
                                            creative_episodic_memory_t* out) {
    if (!bridge || !emotion || !out) return 0;

    /* Score all memories by emotion similarity */
    typedef struct {
        uint32_t idx;
        float score;
    } scored_memory_t;

    scored_memory_t* scores = nimcp_calloc(bridge->episodic_count, sizeof(scored_memory_t));
    if (!scores) return 0;

    for (uint32_t i = 0; i < bridge->episodic_count; i++) {
        scores[i].idx = i;
        scores[i].score = compute_emotion_similarity(emotion,
                                                      &bridge->episodic_memories[i].emotions);
    }

    /* Simple bubble sort for top results (would use better algo for large N) */
    for (uint32_t i = 0; i + 1 < bridge->episodic_count; i++) {
        for (uint32_t j = 0; j < bridge->episodic_count - i - 1; j++) {
            if (scores[j].score < scores[j + 1].score) {
                scored_memory_t tmp = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = tmp;
            }
        }
    }

    /* Copy top results */
    uint32_t count = 0;
    uint32_t max = max_results < bridge->episodic_count ? max_results : bridge->episodic_count;

    for (uint32_t i = 0; i < max; i++) {
        if (scores[i].score >= bridge->config.relevance_threshold) {
            out[count++] = bridge->episodic_memories[scores[i].idx];
        }
    }

    nimcp_free(scores);
    scores = NULL;
    bridge->total_retrievals++;

    return count;
}

uint32_t creative_memory_recall_semantic(creative_memory_bridge_t* bridge,
                                          const char* concept,
                                          uint32_t max_results,
                                          creative_semantic_memory_t* out) {
    if (!bridge || !concept || !out) return 0;

    uint32_t count = 0;

    /* Simple substring matching */
    for (uint32_t i = 0; i < bridge->semantic_count && count < max_results; i++) {
        if (strstr(bridge->semantic_memories[i].concept, concept) ||
            strstr(bridge->semantic_memories[i].description, concept)) {
            out[count] = bridge->semantic_memories[i];
            /* Deep-copy style.embedding to avoid aliasing the bridge's heap pointer */
            if (bridge->semantic_memories[i].style.embedding &&
                bridge->semantic_memories[i].style.embedding_dim > 0) {
                uint32_t dim = bridge->semantic_memories[i].style.embedding_dim;
                out[count].style.embedding = nimcp_calloc(dim, sizeof(float));
                if (out[count].style.embedding) {
                    memcpy(out[count].style.embedding,
                           bridge->semantic_memories[i].style.embedding,
                           dim * sizeof(float));
                } else {
                    out[count].style.embedding = NULL;
                    out[count].style.embedding_dim = 0;
                }
            } else {
                out[count].style.embedding = NULL;
            }
            count++;
        }
    }

    bridge->total_retrievals++;

    return count;
}

int creative_memory_get_preference(const creative_memory_bridge_t* bridge,
                                    art_modality_t modality,
                                    int32_t archetype_id,
                                    creative_preference_t* out) {
    if (!bridge || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_memory_get_preference: bridge or out is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->preference_count; i++) {
        if (bridge->preferences[i].modality == modality &&
            bridge->preferences[i].archetype_id == archetype_id) {
            *out = bridge->preferences[i];
            return 0;
        }
    }

    return -1;  /* Not found */
}

float creative_memory_familiarity(const creative_memory_bridge_t* bridge,
                                   const style_embedding_t* style) {
    if (!bridge || !style) return 0.0f;

    /* Compute familiarity based on similarity to stored semantic memories */
    float max_similarity = 0.0f;

    for (uint32_t i = 0; i < bridge->semantic_count; i++) {
        if (bridge->semantic_memories[i].style.embedding_dim > 0) {
            float sim = style_embedding_similarity(style, &bridge->semantic_memories[i].style);
            if (sim > max_similarity) {
                max_similarity = sim;
            }
        }
    }

    return max_similarity;
}

//=============================================================================
// Maintenance API
//=============================================================================

uint32_t creative_memory_consolidate(creative_memory_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_consolidation) return 0;

    uint32_t consolidated = 0;
    uint64_t now = get_current_time();

    /* Consolidate highly salient memories (increase their persistence) */
    for (uint32_t i = 0; i < bridge->episodic_count; i++) {
        creative_episodic_memory_t* mem = &bridge->episodic_memories[i];

        /* High salience + high emotional content = consolidate */
        float emotional_intensity = mem->emotions.awe + mem->emotions.joy +
                                    mem->emotions.sublime + mem->emotions.contemplation;
        if (mem->salience > 0.7f && emotional_intensity > 0.5f) {
            mem->personal_significance = fminf(1.0f, mem->personal_significance + 0.1f);
            consolidated++;
        }
    }

    bridge->last_consolidation_time = now;

    LOG_DEBUG(LOG_MODULE, "Consolidated %u memories", consolidated);

    return consolidated;
}

uint32_t creative_memory_forget(creative_memory_bridge_t* bridge,
                                 float elapsed_days) {
    if (!bridge) return 0;

    uint32_t affected = 0;
    float decay = bridge->config.forgetting_rate * elapsed_days;

    /* Apply forgetting to episodic memories */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < bridge->episodic_count; i++) {
        creative_episodic_memory_t* mem = &bridge->episodic_memories[i];

        /* Apply decay based on recency and recall frequency */
        float protection = (float)mem->recall_count * 0.1f;  /* More recalls = more protected */
        float effective_decay = decay * (1.0f - protection);

        mem->salience -= effective_decay;
        if (mem->salience < 0.0f) mem->salience = 0.0f;

        /* Keep if above threshold or highly significant */
        if (mem->salience >= bridge->config.salience_threshold ||
            mem->personal_significance > 0.8f) {
            if (write_idx != i) {
                bridge->episodic_memories[write_idx] = *mem;
            }
            write_idx++;
        } else {
            affected++;
        }
    }
    bridge->episodic_count = write_idx;

    /* Apply decay to semantic memory confidence */
    for (uint32_t i = 0; i < bridge->semantic_count; i++) {
        creative_semantic_memory_t* mem = &bridge->semantic_memories[i];
        float protection = (float)mem->reinforcement_count * 0.05f;
        float effective_decay = decay * 0.5f * (1.0f - protection);
        mem->confidence -= effective_decay;
        if (mem->confidence < 0.1f) mem->confidence = 0.1f;
    }

    LOG_DEBUG(LOG_MODULE, "Forgetting affected %u memories", affected);

    return affected;
}

int creative_memory_reinforce(creative_memory_bridge_t* bridge,
                               uint64_t memory_id,
                               creative_memory_type_t memory_type) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_memory_consolidate: bridge is NULL");
        return -1;
    }

    uint64_t now = get_current_time();

    if (memory_type == CREATIVE_MEM_EPISODIC) {
        for (uint32_t i = 0; i < bridge->episodic_count; i++) {
            if (bridge->episodic_memories[i].memory_id == memory_id) {
                bridge->episodic_memories[i].recall_count++;
                bridge->episodic_memories[i].last_recall_time = now;
                bridge->episodic_memories[i].salience =
                    fminf(1.0f, bridge->episodic_memories[i].salience + 0.05f);
                return 0;
            }
        }
    } else if (memory_type == CREATIVE_MEM_SEMANTIC) {
        for (uint32_t i = 0; i < bridge->semantic_count; i++) {
            if (bridge->semantic_memories[i].memory_id == memory_id) {
                bridge->semantic_memories[i].reinforcement_count++;
                bridge->semantic_memories[i].last_reinforcement = now;
                bridge->semantic_memories[i].confidence =
                    fminf(1.0f, bridge->semantic_memories[i].confidence + 0.1f);
                return 0;
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_memory_consolidate: operation failed");
    return -1;  /* Not found */
}

//=============================================================================
// Integration API
//=============================================================================

void creative_memory_set_hippocampus(creative_memory_bridge_t* bridge,
                                      void* hippocampus) {
    if (!bridge) return;
    bridge->hippocampus = hippocampus;
}

void creative_memory_set_semantic_system(creative_memory_bridge_t* bridge,
                                          void* semantic) {
    if (!bridge) return;
    bridge->semantic_memory_system = semantic;
}

int creative_memory_sync_external(creative_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_memory_sync_external: bridge is NULL");
        return -1;
    }

    /* In full implementation, would sync with hippocampus and semantic memory */
    /* For now, just return success */

    if (bridge->hippocampus) {
        LOG_DEBUG(LOG_MODULE, "Would sync with hippocampus");
    }

    if (bridge->semantic_memory_system) {
        LOG_DEBUG(LOG_MODULE, "Would sync with semantic memory system");
    }

    return 0;
}

//=============================================================================
// Query Result Cleanup
//=============================================================================

void creative_memory_query_result_free(creative_memory_query_result_t* result) {
    if (!result) return;

    if (result->memories) {
        nimcp_free(result->memories);
        result->memories = NULL;
    }
    result->count = 0;
}
