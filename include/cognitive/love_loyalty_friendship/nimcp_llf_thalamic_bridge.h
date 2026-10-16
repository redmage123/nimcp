/**
 * @file nimcp_llf_thalamic_bridge.h
 * @brief Bridge between Love/Loyalty/Friendship system and thalamic router
 *
 * WHAT: Routes social bonding signals through attention-gated thalamic pathways
 * WHY: Social bonding requires conscious awareness via thalamic gating
 * HOW: Packages attachment signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Social bonding involves oxytocin and vasopressin systems
 * - Thalamus gates social signals for conscious processing
 * - Attention to relationships strengthens bonds
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_LLF_THALAMIC_BRIDGE_H
#define NIMCP_LLF_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLF_SIGNAL_ATTACHMENT    0x1301
#define LLF_SIGNAL_TRUST         0x1302
#define LLF_SIGNAL_LOYALTY       0x1303
#define LLF_SIGNAL_CARE          0x1304

typedef struct {
    uint32_t signal_type;
    float bond_strength;
    float trust_level;
    float care_motivation;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} llf_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_bond_strengthening;
    float min_bond_threshold;
    float trust_threshold;
} llf_thalamic_config_t;

typedef struct llf_thalamic_bridge llf_thalamic_bridge_t;

llf_thalamic_config_t llf_thalamic_default_config(void);
llf_thalamic_bridge_t* llf_thalamic_bridge_create(void* llf, thalamic_router_t* router, const llf_thalamic_config_t* config);
void llf_thalamic_bridge_destroy(llf_thalamic_bridge_t* bridge);
int llf_thalamic_bridge_reset(llf_thalamic_bridge_t* bridge);
int llf_thalamic_route_attachment(llf_thalamic_bridge_t* bridge, const llf_thalamic_signal_t* signal);
int llf_thalamic_route_care(llf_thalamic_bridge_t* bridge, const void* target, float motivation);
int llf_thalamic_set_attention(llf_thalamic_bridge_t* bridge, float attention);
int llf_thalamic_get_attention(const llf_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t attachments_routed;
    uint64_t trust_updates;
    uint64_t care_expressions;
    float avg_bond_strength;
} llf_thalamic_stats_t;

int llf_thalamic_bridge_get_stats(const llf_thalamic_bridge_t* bridge, llf_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LLF_THALAMIC_BRIDGE_H */
