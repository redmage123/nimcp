/**
 * @file nimcp_pr_memory_adapter.c
 * @brief Prime Resonance Memory Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements Prime Resonance memory for Memory layer
 * WHY:  PR memory provides quaternion-based semantic encoding
 * HOW:  Models consolidation, resonance retrieval, and entanglement
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/memory/nimcp_pr_memory_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_memory_adapter)

#define PR_MAX_CONTENT_SIZE 256

typedef struct {
    float content[PR_MAX_CONTENT_SIZE];
    uint32_t content_size;
    pr_quaternion_state_t state;
    uint32_t z_tier;
    uint32_t id;        /* Memory ID for retrieval */
    bool is_valid;
} pr_memory_node_t;

struct nimcp_pr_memory_adapter_struct {
    nimcp_pr_memory_config_t config;
    nimcp_module_interface_t interface;

    pr_memory_node_t* memories;
    uint32_t memory_count;
    uint32_t next_id;

    nimcp_pr_memory_state_t state;
    nimcp_pr_memory_stats_t stats;
    bool is_initialized;
};

/* Quaternion operations */
static float quat_dot(const pr_quaternion_state_t* a, const pr_quaternion_state_t* b) {
    return a->w * b->w + a->x * b->x + a->y * b->y + a->z * b->z;
}

static float quat_magnitude(const pr_quaternion_state_t* q) {
    return sqrtf(q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
}

static void quat_normalize(pr_quaternion_state_t* q) {
    float mag = quat_magnitude(q);
    if (mag > 0.0001f) {
        q->w /= mag; q->x /= mag; q->y /= mag; q->z /= mag;
    }
}

/* SLERP for memory state interpolation */
static void quat_slerp(const pr_quaternion_state_t* a, const pr_quaternion_state_t* b,
                       float t, pr_quaternion_state_t* out) {
    float dot = quat_dot(a, b);
    if (dot < 0.0f) dot = -dot;
    if (dot > 0.9995f) {
        /* Linear interpolation for nearly identical quaternions */
        out->w = a->w + t * (b->w - a->w);
        out->x = a->x + t * (b->x - a->x);
        out->y = a->y + t * (b->y - a->y);
        out->z = a->z + t * (b->z - a->z);
    } else {
        float theta = acosf(dot);
        float sin_theta = sinf(theta);
        float wa = sinf((1.0f - t) * theta) / sin_theta;
        float wb = sinf(t * theta) / sin_theta;
        out->w = wa * a->w + wb * b->w;
        out->x = wa * a->x + wb * b->x;
        out->y = wa * a->y + wb * b->y;
        out->z = wa * a->z + wb * b->z;
    }
    quat_normalize(out);
}

/* Compute resonance between cue and memory */
static float compute_resonance(const float* cue, uint32_t cue_size,
                               const pr_memory_node_t* memory) {
    if (!memory->is_valid) return 0.0f;

    float dot = 0.0f, mag_cue = 0.0f, mag_mem = 0.0f;
    uint32_t min_size = cue_size < memory->content_size ? cue_size : memory->content_size;

    for (uint32_t i = 0; i < min_size; i++) {
        dot += cue[i] * memory->content[i];
        mag_cue += cue[i] * cue[i];
        mag_mem += memory->content[i] * memory->content[i];
    }

    if (mag_cue < 0.0001f || mag_mem < 0.0001f) return 0.0f;

    float cosine_sim = dot / (sqrtf(mag_cue) * sqrtf(mag_mem));

    /* Weight by accessibility */
    return cosine_sim * memory->state.z;
}

static nimcp_layer_error_t pr_init(void* module, void* config) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) adapter->config = *(nimcp_pr_memory_config_t*)config;

    adapter->memories = (pr_memory_node_t*)nimcp_calloc(adapter->config.max_memories, sizeof(pr_memory_node_t));
    if (!adapter->memories) return NIMCP_LAYER_ERR_NO_MEMORY;

    adapter->memory_count = 0;
    adapter->next_id = 1;  /* Start IDs at 1 so 0 can indicate "no memory" */
    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pr_shutdown(void* module) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->memories);
    adapter->memories = NULL;
    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pr_update(void* module, float dt) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    float dt_ms = dt * 1000.0f;
    float consolidation_rate = adapter->config.consolidation_rate;
    float decay_rate = adapter->config.decay_rate;

    float sum_consolidation = 0.0f;
    float sum_accessibility = 0.0f;
    uint32_t consolidated_count = 0;

    /* Update each memory */
    for (uint32_t i = 0; i < adapter->config.max_memories; i++) {
        pr_memory_node_t* mem = &adapter->memories[i];
        if (!mem->is_valid) continue;

        /* Consolidation: w increases over time */
        float target_w = 1.0f;
        mem->state.w += (target_w - mem->state.w) * consolidation_rate * dt_ms * 0.001f;

        /* Accessibility decay */
        mem->state.z *= expf(-decay_rate * dt_ms * 0.001f);
        if (mem->state.z < 0.01f) mem->state.z = 0.01f;

        sum_consolidation += mem->state.w;
        sum_accessibility += mem->state.z;
        if (mem->state.w > 0.9f) consolidated_count++;
    }

    /* Update state */
    if (adapter->memory_count > 0) {
        adapter->state.mean_consolidation = sum_consolidation / adapter->memory_count;
        adapter->state.mean_accessibility = sum_accessibility / adapter->memory_count;
    }
    adapter->state.total_memories = adapter->memory_count;
    adapter->state.consolidated_memories = consolidated_count;

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pr_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Encode incoming data as memory */
            if (msg->payload && msg->header.payload_size > 0) {
                pr_quaternion_state_t initial = {0.5f, 0.0f, 0.5f, 1.0f};
                uint32_t id;
                nimcp_pr_memory_adapter_encode(adapter, (float*)msg->payload,
                    msg->header.payload_size / sizeof(float), &initial, &id);
            }
            break;

        case NIMCP_LAYER_MSG_STATE_QUERY:
            /* Retrieval cue */
            break;

        case NIMCP_LAYER_MSG_MODULATE:
            /* Emotional modulation affects all memories */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float emotion = *(float*)msg->payload;
                for (uint32_t i = 0; i < adapter->config.max_memories; i++) {
                    if (adapter->memories[i].is_valid) {
                        /* Boost accessibility of emotionally congruent memories */
                        float congruence = 1.0f - fabsf(adapter->memories[i].state.x - emotion);
                        adapter->memories[i].state.z *= (1.0f + congruence * 0.1f);
                        if (adapter->memories[i].state.z > 1.0f) {
                            adapter->memories[i].state.z = 1.0f;
                        }
                    }
                }
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pr_get_state(void* module, void* state_out, size_t* size) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;
    if (*size < sizeof(nimcp_pr_memory_state_t)) {
        *size = sizeof(nimcp_pr_memory_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }
    *(nimcp_pr_memory_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_pr_memory_state_t);
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pr_set_state(void* module, const void* state, size_t size) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;
    if (size < sizeof(nimcp_pr_memory_state_t)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->state = *(const nimcp_pr_memory_state_t*)state;
    return NIMCP_LAYER_OK;
}

static const char* pr_get_name(void* module) {
    (void)module;
    return "Prime_Resonance_Memory_Adapter";
}

nimcp_pr_memory_config_t nimcp_pr_memory_adapter_default_config(void) {
    nimcp_pr_memory_config_t config = {
        .max_memories = 10000,
        .z_ladder_tiers = 7,
        .resonance_threshold = 0.3f,
        .consolidation_rate = 0.001f,
        .decay_rate = 0.0001f,
        .enable_entanglement = true,
        .enable_reconsolidation = true,
        .enable_logging = false
    };
    return config;
}

nimcp_pr_memory_adapter_t nimcp_pr_memory_adapter_create(const nimcp_pr_memory_config_t* config) {
    nimcp_pr_memory_adapter_t adapter = (nimcp_pr_memory_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_pr_memory_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate PR memory adapter");

    adapter->config = config ? *config : nimcp_pr_memory_adapter_default_config();

    adapter->interface.init = pr_init;
    adapter->interface.shutdown = pr_shutdown;
    adapter->interface.update = pr_update;
    adapter->interface.handle_message = pr_handle_message;
    adapter->interface.get_state = pr_get_state;
    adapter->interface.set_state = pr_set_state;
    adapter->interface.get_name = pr_get_name;

    return adapter;
}

void nimcp_pr_memory_adapter_destroy(nimcp_pr_memory_adapter_t adapter) {
    if (!adapter) return;
    if (adapter->is_initialized) pr_shutdown(adapter);
    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_pr_memory_adapter_get_interface(nimcp_pr_memory_adapter_t adapter) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_encode(
    nimcp_pr_memory_adapter_t adapter,
    const float* content,
    uint32_t content_size,
    const pr_quaternion_state_t* initial_state,
    uint32_t* memory_id_out
) {
    if (!adapter || !content || !memory_id_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (adapter->memory_count >= adapter->config.max_memories) return NIMCP_LAYER_ERR_CAPACITY;

    /* Find empty slot */
    uint32_t slot = 0;
    for (slot = 0; slot < adapter->config.max_memories; slot++) {
        if (!adapter->memories[slot].is_valid) break;
    }
    if (slot >= adapter->config.max_memories) return NIMCP_LAYER_ERR_CAPACITY;

    pr_memory_node_t* mem = &adapter->memories[slot];

    /* Copy content */
    uint32_t copy_size = content_size < PR_MAX_CONTENT_SIZE ? content_size : PR_MAX_CONTENT_SIZE;
    memcpy(mem->content, content, copy_size * sizeof(float));
    mem->content_size = copy_size;

    /* Set initial state */
    if (initial_state) {
        mem->state = *initial_state;
    } else {
        mem->state.w = 0.5f;  /* Initial consolidation */
        mem->state.x = 0.0f;  /* Neutral emotion */
        mem->state.y = 0.5f;  /* Medium salience */
        mem->state.z = 1.0f;  /* High initial accessibility */
    }

    /* Assign Z-ladder tier based on salience */
    mem->z_tier = (uint32_t)(mem->state.y * (adapter->config.z_ladder_tiers - 1));
    mem->id = adapter->next_id++;
    mem->is_valid = true;

    *memory_id_out = mem->id;
    adapter->memory_count++;
    adapter->state.total_memories = adapter->memory_count;  /* Update immediately */
    adapter->stats.memories_encoded++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_retrieve(
    nimcp_pr_memory_adapter_t adapter,
    const float* cue,
    uint32_t cue_size,
    float* content_out,
    uint32_t max_content,
    pr_quaternion_state_t* state_out
) {
    if (!adapter || !cue || !content_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    float best_resonance = 0.0f;
    uint32_t best_idx = 0;
    bool found = false;

    /* Find best matching memory by resonance */
    for (uint32_t i = 0; i < adapter->config.max_memories; i++) {
        if (!adapter->memories[i].is_valid) continue;

        float resonance = compute_resonance(cue, cue_size, &adapter->memories[i]);
        if (resonance > best_resonance && resonance > adapter->config.resonance_threshold) {
            best_resonance = resonance;
            best_idx = i;
            found = true;
        }
    }

    if (!found) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    pr_memory_node_t* mem = &adapter->memories[best_idx];

    /* Copy content */
    uint32_t copy_size = mem->content_size < max_content ? mem->content_size : max_content;
    memcpy(content_out, mem->content, copy_size * sizeof(float));

    /* Boost accessibility (retrieval strengthens memory) */
    mem->state.z = fminf(1.0f, mem->state.z + 0.1f);

    if (state_out) *state_out = mem->state;

    adapter->state.resonance_activity = best_resonance;
    adapter->stats.memories_retrieved++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_get_memory_state(
    nimcp_pr_memory_adapter_t adapter,
    uint32_t memory_id,
    pr_quaternion_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (memory_id == 0) return NIMCP_LAYER_ERR_INVALID_MODULE;  /* 0 is invalid ID */

    /* Search for memory by ID */
    for (uint32_t i = 0; i < adapter->config.max_memories; i++) {
        if (adapter->memories[i].is_valid && adapter->memories[i].id == memory_id) {
            *state_out = adapter->memories[i].state;
            return NIMCP_LAYER_OK;
        }
    }

    return NIMCP_LAYER_ERR_NOT_REGISTERED;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_consolidate(
    nimcp_pr_memory_adapter_t adapter,
    float duration_ms
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Simulate consolidation over duration */
    float steps = duration_ms / 10.0f;  /* 10ms steps */
    for (float t = 0; t < steps; t++) {
        for (uint32_t i = 0; i < adapter->config.max_memories; i++) {
            pr_memory_node_t* mem = &adapter->memories[i];
            if (!mem->is_valid) continue;

            /* Consolidation strengthening */
            mem->state.w += (1.0f - mem->state.w) * adapter->config.consolidation_rate;

            /* Reconsolidation can modify emotional valence */
            if (adapter->config.enable_reconsolidation) {
                /* Slight drift toward neutral over time */
                mem->state.x *= 0.999f;
            }
        }
    }

    adapter->stats.consolidation_events++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_get_state(
    nimcp_pr_memory_adapter_t adapter,
    nimcp_pr_memory_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_get_stats(
    nimcp_pr_memory_adapter_t adapter,
    nimcp_pr_memory_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pr_memory_adapter_reset_stats(nimcp_pr_memory_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
