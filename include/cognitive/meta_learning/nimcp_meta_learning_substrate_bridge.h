/**
 * @file nimcp_meta_learning_substrate_bridge.h
 * @brief Bridge between Meta-Learning system and neural substrate
 *
 * WHAT: Links meta-learning (learning to learn) to metabolic state
 * WHY: Meta-learning requires sustained prefrontal plasticity resources
 * HOW: Monitors ATP/fatigue; modulates learning rate adaptation, strategy selection
 *
 * BIOLOGICAL BASIS:
 * - Meta-learning involves prefrontal-striatal circuits
 * - ATP depletion reduces learning flexibility
 * - Fatigue impairs strategy switching
 * - Metabolic stress favors established learning patterns
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_META_LEARNING_SUBSTRATE_BRIDGE_H
#define NIMCP_META_LEARNING_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_META_LEARNING 0x1220

typedef struct {
    float learning_rate_adapt;    /* Ability to adjust learning rates [0-1] */
    float strategy_flexibility;   /* Flexibility in learning strategies [0-1] */
    float transfer_capacity;      /* Ability to transfer learning [0-1] */
    float plasticity_level;       /* Current neural plasticity [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} meta_learning_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} meta_learning_substrate_config_t;

typedef struct meta_learning_substrate_bridge meta_learning_substrate_bridge_t;

meta_learning_substrate_config_t meta_learning_substrate_default_config(void);
meta_learning_substrate_bridge_t* meta_learning_substrate_bridge_create(void* meta_learning, neural_substrate_t* substrate, const meta_learning_substrate_config_t* config);
void meta_learning_substrate_bridge_destroy(meta_learning_substrate_bridge_t* bridge);
int meta_learning_substrate_bridge_update(meta_learning_substrate_bridge_t* bridge);
int meta_learning_substrate_bridge_get_effects(const meta_learning_substrate_bridge_t* bridge, meta_learning_substrate_effects_t* effects);
int meta_learning_substrate_bridge_apply_effects(meta_learning_substrate_bridge_t* bridge);
int meta_learning_substrate_bridge_register_bio_async(meta_learning_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_LEARNING_SUBSTRATE_BRIDGE_H */
