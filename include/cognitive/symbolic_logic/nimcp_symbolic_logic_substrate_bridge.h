/**
 * @file nimcp_symbolic_logic_substrate_bridge.h
 * @brief Bridge between Symbolic Logic system and neural substrate
 *
 * WHAT: Links symbolic reasoning to metabolic state
 * WHY: Symbolic manipulation requires sustained prefrontal working memory
 * HOW: Monitors ATP/fatigue; modulates symbol manipulation, inference, abstraction
 *
 * BIOLOGICAL BASIS:
 * - Symbolic reasoning involves lateral PFC and parietal cortex
 * - ATP depletion reduces symbol manipulation capacity
 * - Fatigue impairs rule application and inference
 * - Metabolic stress favors concrete over abstract thinking
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_SUBSTRATE_BRIDGE_H
#define NIMCP_SYMBOLIC_LOGIC_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SYMBOLIC_LOGIC 0x122D

typedef struct {
    float symbol_manipulation;    /* Capacity for symbol manipulation [0-1] */
    float rule_application;       /* Accuracy of rule application [0-1] */
    float inference_depth;        /* Depth of symbolic inference [0-1] */
    float abstraction_level;      /* Level of abstraction capacity [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} symbolic_logic_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} symbolic_logic_substrate_config_t;

typedef struct symbolic_logic_substrate_bridge symbolic_logic_substrate_bridge_t;

symbolic_logic_substrate_config_t symbolic_logic_substrate_default_config(void);
symbolic_logic_substrate_bridge_t* symbolic_logic_substrate_bridge_create(void* symbolic_logic, neural_substrate_t* substrate, const symbolic_logic_substrate_config_t* config);
void symbolic_logic_substrate_bridge_destroy(symbolic_logic_substrate_bridge_t* bridge);
int symbolic_logic_substrate_bridge_update(symbolic_logic_substrate_bridge_t* bridge);
int symbolic_logic_substrate_bridge_get_effects(const symbolic_logic_substrate_bridge_t* bridge, symbolic_logic_substrate_effects_t* effects);
int symbolic_logic_substrate_bridge_apply_effects(symbolic_logic_substrate_bridge_t* bridge);
int symbolic_logic_substrate_bridge_register_bio_async(symbolic_logic_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYMBOLIC_LOGIC_SUBSTRATE_BRIDGE_H */
