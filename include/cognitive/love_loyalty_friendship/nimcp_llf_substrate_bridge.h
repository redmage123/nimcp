/**
 * @file nimcp_llf_substrate_bridge.h
 * @brief Bridge between Love/Loyalty/Friendship system and neural substrate
 *
 * WHAT: Links social bonding emotions to metabolic state
 * WHY: Social bonding requires sustained oxytocin and emotional processing
 * HOW: Monitors ATP/fatigue; modulates attachment, trust, social investment
 *
 * BIOLOGICAL BASIS:
 * - Social bonding involves oxytocin and vasopressin systems
 * - ATP depletion reduces social engagement capacity
 * - Fatigue impairs trust and attachment behavior
 * - Metabolic stress affects relationship maintenance
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_LLF_SUBSTRATE_BRIDGE_H
#define NIMCP_LLF_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_LLF 0x1223

typedef struct {
    float attachment_strength;    /* Capacity for attachment [0-1] */
    float trust_capacity;         /* Ability to maintain trust [0-1] */
    float social_investment;      /* Energy for relationships [0-1] */
    float loyalty_maintenance;    /* Loyalty system strength [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} llf_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} llf_substrate_config_t;

typedef struct llf_substrate_bridge llf_substrate_bridge_t;

llf_substrate_config_t llf_substrate_default_config(void);
llf_substrate_bridge_t* llf_substrate_bridge_create(void* llf, neural_substrate_t* substrate, const llf_substrate_config_t* config);
void llf_substrate_bridge_destroy(llf_substrate_bridge_t* bridge);
int llf_substrate_bridge_update(llf_substrate_bridge_t* bridge);
int llf_substrate_bridge_get_effects(const llf_substrate_bridge_t* bridge, llf_substrate_effects_t* effects);
int llf_substrate_bridge_apply_effects(llf_substrate_bridge_t* bridge);
int llf_substrate_bridge_register_bio_async(llf_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LLF_SUBSTRATE_BRIDGE_H */
