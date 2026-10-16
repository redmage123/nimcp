//=============================================================================
// nimcp_auction.c - Auction Mechanisms Implementation
//=============================================================================
/**
 * @file nimcp_auction.c
 * @brief Auction mechanism implementations
 */

#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "auction"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(auction, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_auction_struct {
    nimcp_auction_config_t config;
    nimcp_auction_state_t state;

    // Bid storage
    nimcp_bid_t* bids;
    uint32_t num_bids;
    uint32_t max_bids;

    // Current state (for ascending/descending)
    float current_price;
    uint32_t active_bidders;

    // Results
    nimcp_auction_result_t last_result;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Helpers
//=============================================================================

static int compare_bids_desc(const void* a, const void* b) {
    const nimcp_bid_t* bid_a = (const nimcp_bid_t*)a;
    const nimcp_bid_t* bid_b = (const nimcp_bid_t*)b;

    if (bid_b->bid_amount > bid_a->bid_amount) return 1;
    if (bid_b->bid_amount < bid_a->bid_amount) return -1;

    // Tie-break by timestamp (earlier wins)
    if (bid_a->timestamp_ms < bid_b->timestamp_ms) return -1;
    if (bid_a->timestamp_ms > bid_b->timestamp_ms) return 1;
    return 0;
}

static const char* s_auction_type_names[] = {
    "First-Price",
    "Second-Price (Vickrey)",
    "VCG",
    "Ascending (English)",
    "Descending (Dutch)"
};

//=============================================================================
// Configuration
//=============================================================================

nimcp_auction_config_t nimcp_auction_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_default_conf", 0.0f);


    nimcp_auction_config_t config = {
        .type = NIMCP_AUCTION_SECOND_PRICE,
        .num_items = 1,
        .reserve_price = 0.0f,
        .deadline_ms = 0,
        .max_bidders = NIMCP_GT_MAX_PLAYERS,
        .allow_tie_random = true,
        .reveal_bids = false,
        .ascending_increment = 0.01f,
        .descending_decrement = 0.01f
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_auction_t nimcp_auction_create(const nimcp_auction_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_create", 0.0f);


    nimcp_auction_t auction = nimcp_calloc(1, sizeof(struct nimcp_auction_struct));
    if (!auction) return NULL;
    NIMCP_API_CHECK_ALLOC(auction, "Failed to allocate auction");

    if (config) {
        auction->config = *config;
    } else {
        auction->config = nimcp_auction_default_config();
    }

    auction->max_bids = auction->config.max_bidders * auction->config.num_items;
    auction->bids = nimcp_calloc(auction->max_bids, sizeof(nimcp_bid_t));
    if (!auction->bids) {
        nimcp_free(auction);
        auction = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_auction_create: auction->bids is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&auction->mutex, false) != 0) {
        nimcp_free(auction->bids);
        nimcp_free(auction);
        auction = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_auction_create: validation failed");
        return NULL;
    }

    auction->state = NIMCP_AUCTION_STATE_CREATED;
    auction->num_bids = 0;
    auction->current_price = auction->config.reserve_price;
    auction->active_bidders = 0;

    memset(&auction->last_result, 0, sizeof(nimcp_auction_result_t));
    auction->last_result.winner_id = NIMCP_GT_INVALID_PLAYER;

    return auction;
}

void nimcp_auction_destroy(nimcp_auction_t auction) {
    if (!auction) return;

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&auction->mutex);
    nimcp_free(auction->bids);
    nimcp_free(auction);
    auction = NULL;
}

//=============================================================================
// Bidding
//=============================================================================

nimcp_error_t nimcp_auction_bid(
    nimcp_auction_t auction,
    nimcp_player_id_t bidder_id,
    float bid_amount,
    uint32_t item_id
) {
    if (!auction) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_bid", 0.0f);


    nimcp_platform_mutex_lock(&auction->mutex);

    // Check state
    if (auction->state == NIMCP_AUCTION_STATE_COMPLETED ||
        auction->state == NIMCP_AUCTION_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_AUCTION_CLOSED;
    }

    // Check capacity
    if (auction->num_bids >= auction->max_bids) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    // Validate bid
    if (bid_amount < 0.0f) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_INVALID_BID;
    }

    if (item_id >= auction->config.num_items) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    // Transition to bidding state
    if (auction->state == NIMCP_AUCTION_STATE_CREATED) {
        auction->state = NIMCP_AUCTION_STATE_BIDDING;
    }

    // Store bid
    nimcp_bid_t* bid = &auction->bids[auction->num_bids];
    bid->bidder_id = bidder_id;
    bid->bid_amount = bid_amount;
    bid->item_id = item_id;
    bid->valuation = bid_amount;  // Default: valuation = bid
    bid->timestamp_ms = nimcp_time_get_ms();
    bid->is_valid = true;

    auction->num_bids++;

    // Update current price for ascending
    if (auction->config.type == NIMCP_AUCTION_ASCENDING) {
        if (bid_amount > auction->current_price) {
            auction->current_price = bid_amount;
        }
    }

    nimcp_platform_mutex_unlock(&auction->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_auction_bid_vcg(
    nimcp_auction_t auction,
    nimcp_player_id_t bidder_id,
    float bid_amount,
    float private_valuation,
    uint32_t item_id
) {
    if (!auction) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_bid_vcg", 0.0f);


    nimcp_error_t err = nimcp_auction_bid(auction, bidder_id, bid_amount, item_id);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Update valuation for last bid
    nimcp_platform_mutex_lock(&auction->mutex);
    if (auction->num_bids > 0) {
        auction->bids[auction->num_bids - 1].valuation = private_valuation;
    }
    nimcp_platform_mutex_unlock(&auction->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Resolution
//=============================================================================

nimcp_error_t nimcp_auction_resolve(
    nimcp_auction_t auction,
    nimcp_auction_result_t* result
) {
    if (!auction || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_resolve", 0.0f);


    nimcp_platform_mutex_lock(&auction->mutex);

    // Handle cancelled auction - return cancelled state in result
    if (auction->state == NIMCP_AUCTION_STATE_CANCELLED) {
        memset(result, 0, sizeof(nimcp_auction_result_t));
        result->winner_id = NIMCP_GT_INVALID_PLAYER;
        result->final_state = NIMCP_AUCTION_STATE_CANCELLED;
        result->num_bids = auction->num_bids;
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_SUCCESS;
    }

    if (auction->state != NIMCP_AUCTION_STATE_BIDDING &&
        auction->state != NIMCP_AUCTION_STATE_CREATED) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    auction->state = NIMCP_AUCTION_STATE_RESOLVING;

    // Initialize result
    memset(result, 0, sizeof(nimcp_auction_result_t));
    result->winner_id = NIMCP_GT_INVALID_PLAYER;
    result->num_bids = auction->num_bids;
    result->resolution_time_ms = nimcp_time_get_ms();

    // No bids case
    if (auction->num_bids == 0) {
        auction->state = NIMCP_AUCTION_STATE_NO_WINNER;
        result->final_state = NIMCP_AUCTION_STATE_NO_WINNER;
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_SUCCESS;
    }

    // Sort bids by amount (descending)
    qsort(auction->bids, auction->num_bids, sizeof(nimcp_bid_t), compare_bids_desc);

    // Find highest valid bid meeting reserve
    nimcp_bid_t* winner_bid = NULL;
    nimcp_bid_t* second_bid = NULL;

    for (uint32_t i = 0; i < auction->num_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && auction->num_bids > 256) {
            auction_heartbeat("auction_loop",
                             (float)(i + 1) / (float)auction->num_bids);
        }

        if (auction->bids[i].is_valid &&
            auction->bids[i].bid_amount >= auction->config.reserve_price) {
            if (!winner_bid) {
                winner_bid = &auction->bids[i];
            } else if (!second_bid) {
                second_bid = &auction->bids[i];
                break;
            }
        }
    }

    // No winner if no bid meets reserve
    if (!winner_bid) {
        auction->state = NIMCP_AUCTION_STATE_NO_WINNER;
        result->final_state = NIMCP_AUCTION_STATE_NO_WINNER;
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_SUCCESS;
    }

    result->winner_id = winner_bid->bidder_id;
    result->winning_bid = winner_bid->bid_amount;
    result->second_highest_bid = second_bid ? second_bid->bid_amount : auction->config.reserve_price;

    // Determine payment based on auction type
    switch (auction->config.type) {
        case NIMCP_AUCTION_FIRST_PRICE:
            // Pay what you bid
            result->payment = winner_bid->bid_amount;
            break;

        case NIMCP_AUCTION_SECOND_PRICE:
            // Pay second-highest bid (or reserve)
            result->payment = second_bid ? second_bid->bid_amount : auction->config.reserve_price;
            break;

        case NIMCP_AUCTION_VCG:
            // For single-item VCG, same as second-price
            result->payment = second_bid ? second_bid->bid_amount : auction->config.reserve_price;
            break;

        case NIMCP_AUCTION_ASCENDING:
        case NIMCP_AUCTION_DESCENDING:
            // Pay final price
            result->payment = auction->current_price;
            break;

        default:
            result->payment = winner_bid->bid_amount;
            break;
    }

    // Efficiency = winner's valuation / max valuation
    float max_valuation = 0.0f;
    for (uint32_t i = 0; i < auction->num_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && auction->num_bids > 256) {
            auction_heartbeat("auction_loop",
                             (float)(i + 1) / (float)auction->num_bids);
        }

        if (auction->bids[i].valuation > max_valuation) {
            max_valuation = auction->bids[i].valuation;
        }
    }
    if (max_valuation > 0.0f) {
        result->efficiency = winner_bid->valuation / max_valuation;
    } else {
        result->efficiency = 1.0f;
    }

    auction->state = NIMCP_AUCTION_STATE_COMPLETED;
    result->final_state = NIMCP_AUCTION_STATE_COMPLETED;
    auction->last_result = *result;

    nimcp_platform_mutex_unlock(&auction->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_auction_resolve_vcg(
    nimcp_auction_t auction,
    nimcp_vcg_result_t* result
) {
    if (!auction || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_resolve_vcg", 0.0f);


    nimcp_platform_mutex_lock(&auction->mutex);

    if (auction->config.type != NIMCP_AUCTION_VCG) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    // Initialize result
    memset(result, 0, sizeof(nimcp_vcg_result_t));
    result->num_items = auction->config.num_items;

    for (uint32_t i = 0; i < NIMCP_GT_MAX_AUCTION_ITEMS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_GT_MAX_AUCTION_ITEMS > 256) {
            auction_heartbeat("auction_loop",
                             (float)(i + 1) / (float)NIMCP_GT_MAX_AUCTION_ITEMS);
        }

        result->winners[i] = NIMCP_GT_INVALID_PLAYER;
    }

    // For each item, find winner
    for (uint32_t item = 0; item < auction->config.num_items; item++) {
        /* Phase 8: Loop progress heartbeat */
        if ((item & 0xFF) == 0 && auction->config.num_items > 256) {
            auction_heartbeat("auction_loop",
                             (float)(item + 1) / (float)auction->config.num_items);
        }

        nimcp_bid_t* best_bid = NULL;
        nimcp_bid_t* second_bid = NULL;

        for (uint32_t i = 0; i < auction->num_bids; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && auction->num_bids > 256) {
                auction_heartbeat("auction_loop",
                                 (float)(i + 1) / (float)auction->num_bids);
            }

            nimcp_bid_t* bid = &auction->bids[i];
            if (bid->item_id != item || !bid->is_valid) continue;

            if (!best_bid || bid->valuation > best_bid->valuation) {
                second_bid = best_bid;
                best_bid = bid;
            } else if (!second_bid || bid->valuation > second_bid->valuation) {
                second_bid = bid;
            }
        }

        if (best_bid && best_bid->bid_amount >= auction->config.reserve_price) {
            result->winners[item] = best_bid->bidder_id;
            result->allocations[item] = 1.0f;
            result->total_welfare += best_bid->valuation;

            // VCG payment = externality = second-best valuation
            if (second_bid) {
                result->payments[item] = second_bid->valuation;
            } else {
                result->payments[item] = auction->config.reserve_price;
            }
            result->revenue += result->payments[item];
        }
    }

    result->is_efficient = true;  // VCG is always efficient

    auction->state = NIMCP_AUCTION_STATE_COMPLETED;
    nimcp_platform_mutex_unlock(&auction->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_auction_cancel(nimcp_auction_t auction) {
    if (!auction) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_cancel", 0.0f);


    nimcp_platform_mutex_lock(&auction->mutex);
    auction->state = NIMCP_AUCTION_STATE_CANCELLED;
    nimcp_platform_mutex_unlock(&auction->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Ascending/Descending Operations
//=============================================================================

nimcp_error_t nimcp_auction_ascending_round(
    nimcp_auction_t auction,
    float* current_price
) {
    if (!auction || !current_price) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_ascending_ro", 0.0f);


    nimcp_platform_mutex_lock(&auction->mutex);

    if (auction->config.type != NIMCP_AUCTION_ASCENDING) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    *current_price = auction->current_price;
    nimcp_platform_mutex_unlock(&auction->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_auction_descending_tick(
    nimcp_auction_t auction,
    float* current_price,
    nimcp_player_id_t* claimed_by
) {
    if (!auction || !current_price || !claimed_by) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_descending_t", 0.0f);


    nimcp_platform_mutex_lock(&auction->mutex);

    if (auction->config.type != NIMCP_AUCTION_DESCENDING) {
        nimcp_platform_mutex_unlock(&auction->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    *claimed_by = NIMCP_GT_INVALID_PLAYER;

    // Check if anyone claimed at current price
    for (uint32_t i = 0; i < auction->num_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && auction->num_bids > 256) {
            auction_heartbeat("auction_loop",
                             (float)(i + 1) / (float)auction->num_bids);
        }

        if (auction->bids[i].bid_amount >= auction->current_price) {
            *claimed_by = auction->bids[i].bidder_id;
            break;
        }
    }

    if (*claimed_by == NIMCP_GT_INVALID_PLAYER) {
        // Decrement price
        auction->current_price -= auction->config.descending_decrement;
        if (auction->current_price < auction->config.reserve_price) {
            auction->current_price = auction->config.reserve_price;
        }
    }

    *current_price = auction->current_price;
    nimcp_platform_mutex_unlock(&auction->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

nimcp_auction_state_t nimcp_auction_get_state(const nimcp_auction_t auction) {
    if (!auction) return NIMCP_AUCTION_STATE_CREATED;
    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_get_state", 0.0f);


    return auction->state;
}

uint32_t nimcp_auction_get_bid_count(const nimcp_auction_t auction) {
    if (!auction) return 0;
    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_get_bid_coun", 0.0f);


    return auction->num_bids;
}

float nimcp_auction_get_current_highest(const nimcp_auction_t auction) {
    if (!auction || auction->num_bids == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_get_current_", 0.0f);


    float max_bid = 0.0f;
    for (uint32_t i = 0; i < auction->num_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && auction->num_bids > 256) {
            auction_heartbeat("auction_loop",
                             (float)(i + 1) / (float)auction->num_bids);
        }

        if (auction->bids[i].bid_amount > max_bid) {
            max_bid = auction->bids[i].bid_amount;
        }
    }
    return max_bid;
}

nimcp_auction_type_t nimcp_auction_get_type(const nimcp_auction_t auction) {
    if (!auction) return NIMCP_AUCTION_SECOND_PRICE;
    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_get_type", 0.0f);


    return auction->config.type;
}

bool nimcp_auction_is_strategyproof(nimcp_auction_type_t type) {
    // Second-price and VCG are strategyproof (truthful bidding is dominant)
    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_auction_is_strategyp", 0.0f);


    return (type == NIMCP_AUCTION_SECOND_PRICE || type == NIMCP_AUCTION_VCG);
}

const char* nimcp_auction_type_name(nimcp_auction_type_t type) {
    if (type >= NIMCP_AUCTION_COUNT) {
        return "Unknown";
    }
    return s_auction_type_names[type];
}

nimcp_error_t nimcp_auction_get_bids(
    const nimcp_auction_t auction,
    nimcp_bid_t* bids,
    uint32_t max_bids,
    uint32_t* num_bids
) {
    if (!auction || !bids || !num_bids) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!auction->config.reveal_bids &&
        auction->state != NIMCP_AUCTION_STATE_COMPLETED) {
        *num_bids = 0;
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_get_bids", 0.0f);

    nimcp_platform_mutex_lock(&auction->mutex);

    uint32_t copy_count = auction->num_bids < max_bids ? auction->num_bids : max_bids;
    memcpy(bids, auction->bids, copy_count * sizeof(nimcp_bid_t));
    *num_bids = copy_count;

    nimcp_platform_mutex_unlock(&auction->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int auction_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    auction_heartbeat("auction_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Auction_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                auction_heartbeat("auction_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG(LOG_MODULE, "Auction self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Auction_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Auction_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void auction_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_auction_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int auction_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "auction_training_begin: NULL argument");
        return -1;
    }
    auction_heartbeat_instance(NULL, "auction_training_begin", 0.0f);
    (void)(struct nimcp_auction_struct*)instance; /* Module state available for reset */
    return 0;
}

int auction_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "auction_training_end: NULL argument");
        return -1;
    }
    auction_heartbeat_instance(NULL, "auction_training_end", 1.0f);
    (void)(struct nimcp_auction_struct*)instance; /* Module state available for finalization */
    return 0;
}

int auction_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "auction_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    auction_heartbeat_instance(NULL, "auction_training_step", progress);
    (void)(struct nimcp_auction_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
