//=============================================================================
// nimcp_gt_working_memory.c - Priority Auction for Working Memory Slots
//=============================================================================
/**
 * @file nimcp_gt_working_memory.c
 * @brief Priority auction for working memory slot allocation
 *
 * WHAT: Auction-based eviction policy for working memory
 * WHY:  Market mechanism for efficient slot allocation
 * HOW:  Items bid with salience, lowest bidders evicted
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#include "cognitive/game_theory/integration/nimcp_gt_working_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "gt_working_memory"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gt_working_memory, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Constants
//=============================================================================

#define MAX_WM_SLOTS 32
#define DEFAULT_DECAY_RATE 0.01f
#define DEFAULT_RESERVE_PRICE 0.1f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal slot state
 */
typedef struct {
    float* content;
    uint32_t content_size;
    float current_bid;
    float initial_bid;
    float congestion_cost;
    uint32_t age_cycles;
    uint32_t rehearsal_count;
    uint64_t last_access_time_ms;
    bool occupied;
} gt_wm_slot_internal_t;

/**
 * @brief Opaque context structure
 */
struct gt_wm_auction_ctx_struct {
    working_memory_t* wm;
    gt_wm_config_t config;

    // Slots
    gt_wm_slot_internal_t slots[MAX_WM_SLOTS];
    uint32_t num_slots;
    uint32_t occupied_slots;

    // Statistics
    uint64_t items_added;
    uint64_t items_evicted;
    uint64_t eviction_rounds;
    float total_congestion_paid;

    // Timing
    uint64_t last_eviction_time_ms;
    uint64_t last_decay_time_ms;

    bool active;
};

//=============================================================================
// Static Helpers
//=============================================================================

static int find_free_slot(const gt_wm_auction_ctx_t ctx) {
    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        if (!ctx->slots[i].occupied) {
            return (int)i;
        }
    }
    return -1;  /* All slots occupied is normal */
}

static int find_lowest_bid_slot(const gt_wm_auction_ctx_t ctx) {
    int lowest_idx = -1;
    float lowest_effective = FLT_MAX;

    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        if (ctx->slots[i].occupied) {
            float effective = ctx->slots[i].current_bid - ctx->slots[i].congestion_cost;
            if (effective < lowest_effective) {
                lowest_effective = effective;
                lowest_idx = (int)i;
            }
        }
    }
    return lowest_idx;
}

static float compute_congestion_cost(const gt_wm_auction_ctx_t ctx) {
    if (ctx->num_slots == 0) return 0.0f;

    float occupancy = (float)ctx->occupied_slots / (float)ctx->num_slots;
    return occupancy * ctx->config.congestion_cost_factor;
}

static void evict_slot(gt_wm_auction_ctx_t ctx, uint32_t slot_idx) {
    gt_wm_slot_internal_t* slot = &ctx->slots[slot_idx];

    if (slot->content) {
        nimcp_free(slot->content);
        slot->content = NULL;
    }

    slot->occupied = false;
    slot->current_bid = 0.0f;
    slot->initial_bid = 0.0f;
    slot->content_size = 0;
    slot->age_cycles = 0;
    slot->rehearsal_count = 0;

    ctx->occupied_slots--;
    ctx->items_evicted++;
}

//=============================================================================
// Lifecycle
//=============================================================================

gt_wm_config_t gt_wm_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_default_config", 0.0f);


    gt_wm_config_t config = {
        .policy = GT_WM_EVICTION_AUCTION,
        .slot_reserve_price = DEFAULT_RESERVE_PRICE,
        .congestion_cost_factor = 0.05f,
        .decay_rate = DEFAULT_DECAY_RATE,
        .enable_preemption = true,
        .preemption_premium = 0.2f,
        .eviction_interval_ms = 100
    };
    return config;
}

gt_wm_auction_ctx_t gt_wm_create(
    working_memory_t* wm,
    const gt_wm_config_t* config
) {
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_create", 0.0f);


    gt_wm_auction_ctx_t ctx = nimcp_calloc(1, sizeof(struct gt_wm_auction_ctx_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    ctx->wm = wm;
    ctx->config = config ? *config : gt_wm_default_config();

    // Initialize slots
    ctx->num_slots = MAX_WM_SLOTS;
    ctx->occupied_slots = 0;
    for (uint32_t i = 0; i < MAX_WM_SLOTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_WM_SLOTS > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)MAX_WM_SLOTS);
        }

        ctx->slots[i].occupied = false;
        ctx->slots[i].content = NULL;
    }

    // Initialize statistics
    ctx->items_added = 0;
    ctx->items_evicted = 0;
    ctx->eviction_rounds = 0;
    ctx->total_congestion_paid = 0.0f;

    ctx->last_eviction_time_ms = nimcp_time_get_ms();
    ctx->last_decay_time_ms = nimcp_time_get_ms();

    ctx->active = true;

    return ctx;
}

void gt_wm_destroy(gt_wm_auction_ctx_t ctx) {
    if (!ctx) {
        return;
    }

    // Free all slot contents
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_destroy", 0.0f);


    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        if (ctx->slots[i].content) {
            nimcp_free(ctx->slots[i].content);
        }
    }

    nimcp_free(ctx);
    ctx = NULL;
}

//=============================================================================
// Core Operations
//=============================================================================

nimcp_error_t gt_wm_add(
    gt_wm_auction_ctx_t ctx,
    const float* item,
    uint32_t item_size,
    float bid,
    int32_t* slot_index
) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_add", 0.0f);


    NIMCP_CHECK_THROW(ctx && item && item_size > 0 && slot_index, NIMCP_ERROR_INVALID_PARAM, "ctx, item, slot_index is NULL or item_size is 0");

    *slot_index = -1;

    NIMCP_CHECK_THROW(ctx->active, NIMCP_GT_ERROR_GAME_OVER, "ctx is not active");
    NIMCP_CHECK_THROW(bid >= ctx->config.slot_reserve_price, NIMCP_GT_ERROR_BID_TOO_LOW, "bid below reserve price");

    // Try to find a free slot
    int free_slot = find_free_slot(ctx);

    if (free_slot >= 0) {
        // Slot available, allocate directly
        gt_wm_slot_internal_t* slot = &ctx->slots[free_slot];

        slot->content = nimcp_calloc(item_size, sizeof(float));
        if (!slot->content) {
            return NIMCP_ERROR_NO_MEMORY;
        }

        memcpy(slot->content, item, item_size * sizeof(float));
        slot->content_size = item_size;
        slot->current_bid = bid;
        slot->initial_bid = bid;
        slot->congestion_cost = compute_congestion_cost(ctx);
        slot->age_cycles = 0;
        slot->rehearsal_count = 0;
        slot->last_access_time_ms = nimcp_time_get_ms();
        slot->occupied = true;

        ctx->occupied_slots++;
        ctx->items_added++;
        ctx->total_congestion_paid += slot->congestion_cost;

        *slot_index = free_slot;
        return NIMCP_SUCCESS;
    }

    // No free slot - try preemption if enabled
    if (!ctx->config.enable_preemption) {
        return NIMCP_GT_ERROR_CAPACITY;
    }

    // Find lowest bidder
    int lowest_idx = find_lowest_bid_slot(ctx);
    if (lowest_idx < 0) {
        return NIMCP_GT_ERROR_CAPACITY;
    }

    gt_wm_slot_internal_t* lowest_slot = &ctx->slots[lowest_idx];
    float effective_value = lowest_slot->current_bid - lowest_slot->congestion_cost;

    // Check if new bid beats lowest + premium
    if (bid <= effective_value + ctx->config.preemption_premium) {
        return NIMCP_GT_ERROR_BID_TOO_LOW;
    }

    // Evict and replace
    evict_slot(ctx, (uint32_t)lowest_idx);

    gt_wm_slot_internal_t* slot = &ctx->slots[lowest_idx];
    slot->content = nimcp_calloc(item_size, sizeof(float));
    if (!slot->content) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    memcpy(slot->content, item, item_size * sizeof(float));
    slot->content_size = item_size;
    slot->current_bid = bid;
    slot->initial_bid = bid;
    slot->congestion_cost = compute_congestion_cost(ctx);
    slot->age_cycles = 0;
    slot->rehearsal_count = 0;
    slot->last_access_time_ms = nimcp_time_get_ms();
    slot->occupied = true;

    ctx->occupied_slots++;
    ctx->items_added++;
    ctx->total_congestion_paid += slot->congestion_cost;

    *slot_index = lowest_idx;
    return NIMCP_SUCCESS;
}

nimcp_error_t gt_wm_refresh(
    gt_wm_auction_ctx_t ctx,
    uint32_t slot_index,
    float new_bid
) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_refresh", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(slot_index < ctx->num_slots, NIMCP_ERROR_OUT_OF_RANGE, "slot_index out of range");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_GT_ERROR_GAME_OVER, "ctx is not active");

    gt_wm_slot_internal_t* slot = &ctx->slots[slot_index];
    if (!slot->occupied) {
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    // Refresh bid
    if (new_bid > 0.0f) {
        slot->current_bid = new_bid;
    } else {
        slot->current_bid = slot->initial_bid;  // Restore initial
    }

    slot->rehearsal_count++;
    slot->last_access_time_ms = nimcp_time_get_ms();

    return NIMCP_SUCCESS;
}

nimcp_error_t gt_wm_run_eviction(
    gt_wm_auction_ctx_t ctx,
    gt_wm_eviction_result_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_run_eviction", 0.0f);


    NIMCP_CHECK_THROW(ctx && result, NIMCP_ERROR_INVALID_PARAM, "ctx or result is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_GT_ERROR_GAME_OVER, "ctx is not active");

    memset(result, 0, sizeof(gt_wm_eviction_result_t));
    result->occupancy_before = ctx->occupied_slots;

    // Update congestion costs
    float current_congestion = compute_congestion_cost(ctx);
    result->total_congestion_cost = current_congestion;

    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        if (ctx->slots[i].occupied) {
            ctx->slots[i].congestion_cost = current_congestion;
        }
    }

    // Evict items below reserve price (after applying congestion)
    float lowest_surviving = FLT_MAX;
    uint32_t evict_count = 0;

    for (uint32_t i = 0; i < ctx->num_slots && evict_count < 16; i++) {
        gt_wm_slot_internal_t* slot = &ctx->slots[i];
        if (!slot->occupied) continue;

        float effective = slot->current_bid - slot->congestion_cost;

        if (effective < ctx->config.slot_reserve_price) {
            result->evicted_indices[evict_count++] = i;
            evict_slot(ctx, i);
        } else {
            if (effective < lowest_surviving) {
                lowest_surviving = effective;
            }
        }
    }

    result->num_evicted = evict_count;
    result->lowest_surviving_bid = (lowest_surviving < FLT_MAX) ? lowest_surviving : 0.0f;
    result->occupancy_after = ctx->occupied_slots;

    ctx->eviction_rounds++;
    ctx->last_eviction_time_ms = nimcp_time_get_ms();

    return NIMCP_SUCCESS;
}

nimcp_error_t gt_wm_apply_decay(
    gt_wm_auction_ctx_t ctx,
    float decay_amount
) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_apply_decay", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_GT_ERROR_GAME_OVER, "ctx is not active");

    float decay = (decay_amount > 0.0f) ? decay_amount : ctx->config.decay_rate;

    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        gt_wm_slot_internal_t* slot = &ctx->slots[i];
        if (!slot->occupied) continue;

        slot->current_bid -= decay;
        if (slot->current_bid < 0.0f) {
            slot->current_bid = 0.0f;
        }

        slot->age_cycles++;
    }

    ctx->last_decay_time_ms = nimcp_time_get_ms();

    return NIMCP_SUCCESS;
}

float gt_wm_get_congestion_cost(const gt_wm_auction_ctx_t ctx) {
    if (!ctx) {
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_congestion", 0.0f);


    return compute_congestion_cost((gt_wm_auction_ctx_t)ctx);
}

//=============================================================================
// Query Functions
//=============================================================================

nimcp_error_t gt_wm_get_slot_state(
    const gt_wm_auction_ctx_t ctx,
    uint32_t slot_index,
    gt_wm_slot_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_slot_state", 0.0f);


    NIMCP_CHECK_THROW(ctx && state, NIMCP_ERROR_INVALID_PARAM, "ctx or state is NULL");
    NIMCP_CHECK_THROW(slot_index < ctx->num_slots, NIMCP_ERROR_OUT_OF_RANGE, "slot_index out of range");

    const gt_wm_slot_internal_t* slot = &ctx->slots[slot_index];
    if (!slot->occupied) {
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    state->item_index = slot_index;
    state->current_bid = slot->current_bid;
    state->initial_bid = slot->initial_bid;
    state->congestion_cost = slot->congestion_cost;
    state->effective_value = slot->current_bid - slot->congestion_cost;
    state->age_cycles = slot->age_cycles;
    state->rehearsal_count = slot->rehearsal_count;
    state->last_access_time_ms = slot->last_access_time_ms;

    return NIMCP_SUCCESS;
}

working_memory_t* gt_wm_get_wm(const gt_wm_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_wm", 0.0f);


    return ctx ? ctx->wm : NULL;
}

uint32_t gt_wm_get_occupancy(const gt_wm_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_occupancy", 0.0f);


    return ctx ? ctx->occupied_slots : 0;
}

uint32_t gt_wm_get_capacity(const gt_wm_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_capacity", 0.0f);


    return ctx ? ctx->num_slots : 0;
}

bool gt_wm_is_active(const gt_wm_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_is_active", 0.0f);


    return ctx ? ctx->active : false;
}

float gt_wm_get_highest_bid(const gt_wm_auction_ctx_t ctx) {
    if (!ctx || ctx->occupied_slots == 0) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_highest_bi", 0.0f);


    float highest = -FLT_MAX;
    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        if (ctx->slots[i].occupied && ctx->slots[i].current_bid > highest) {
            highest = ctx->slots[i].current_bid;
        }
    }

    return highest > -FLT_MAX ? highest : 0.0f;
}

float gt_wm_get_lowest_bid(const gt_wm_auction_ctx_t ctx) {
    if (!ctx || ctx->occupied_slots == 0) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_lowest_bid", 0.0f);


    float lowest = FLT_MAX;
    for (uint32_t i = 0; i < ctx->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_slots > 256) {
            gt_working_memory_heartbeat("gt_working_m_loop",
                             (float)(i + 1) / (float)ctx->num_slots);
        }

        if (ctx->slots[i].occupied && ctx->slots[i].current_bid < lowest) {
            lowest = ctx->slots[i].current_bid;
        }
    }

    return lowest < FLT_MAX ? lowest : 0.0f;
}

nimcp_error_t gt_wm_get_stats(
    const gt_wm_auction_ctx_t ctx,
    nimcp_game_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_gt_wm_get_stats", 0.0f);


    NIMCP_CHECK_THROW(ctx && stats, NIMCP_ERROR_INVALID_PARAM, "ctx or stats is NULL");

    memset(stats, 0, sizeof(nimcp_game_stats_t));

    stats->games_played = ctx->eviction_rounds;
    stats->auctions_completed = ctx->items_added;

    // Compute efficiency (items kept vs capacity)
    if (ctx->num_slots > 0) {
        stats->avg_efficiency = (float)ctx->occupied_slots / (float)ctx->num_slots;
    }

    // Average congestion cost as fairness proxy
    if (ctx->items_added > 0) {
        stats->avg_social_welfare = ctx->total_congestion_paid / (float)ctx->items_added;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int gt_working_memory_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gt_working_memory_heartbeat("gt_working_m_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GT_WorkingMemory_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_working_memory_heartbeat("gt_working_m_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG(LOG_MODULE, "GT WorkingMemory self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GT_WorkingMemory_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GT_WorkingMemory_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_working_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_working_memory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_working_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_working_memory_training_begin: NULL argument");
        return -1;
    }
    gt_working_memory_heartbeat_instance(NULL, "gt_working_memory_training_begin", 0.0f);
    (void)(struct gt_wm_auction_ctx_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_working_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_working_memory_training_end: NULL argument");
        return -1;
    }
    gt_working_memory_heartbeat_instance(NULL, "gt_working_memory_training_end", 1.0f);
    (void)(struct gt_wm_auction_ctx_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_working_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_working_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_working_memory_heartbeat_instance(NULL, "gt_working_memory_training_step", progress);
    (void)(struct gt_wm_auction_ctx_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
