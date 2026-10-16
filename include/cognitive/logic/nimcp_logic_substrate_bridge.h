/**
 * @file nimcp_logic_substrate_bridge.h
 * @brief Bridge between Logic systems and neural substrate
 *
 * WHAT: Links logical/symbolic reasoning to metabolic state
 * WHY: Logical inference requires sustained prefrontal-parietal activation
 * HOW: Monitors ATP/fatigue; modulates inference depth, accuracy, speed
 *
 * BIOLOGICAL BASIS:
 * - Logical reasoning activates lateral PFC and parietal cortex
 * - ATP depletion reduces inference chain depth
 * - Fatigue increases logical errors and shortcuts
 * - Working memory load affects logical capacity
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_LOGIC_SUBSTRATE_BRIDGE_H
#define NIMCP_LOGIC_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_LOGIC 0x1214

typedef struct {
    float inference_depth;        /* Maximum inference chain [0-1] */
    float logical_accuracy;       /* Correctness of operations [0-1] */
    float processing_speed;       /* Speed of logical ops [0-1] */
    float abstraction_capacity;   /* Abstract reasoning [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} logic_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} logic_substrate_config_t;

typedef struct logic_substrate_bridge logic_substrate_bridge_t;

logic_substrate_config_t logic_substrate_default_config(void);
logic_substrate_bridge_t* logic_substrate_bridge_create(void* logic, neural_substrate_t* substrate, const logic_substrate_config_t* config);
void logic_substrate_bridge_destroy(logic_substrate_bridge_t* bridge);
int logic_substrate_bridge_update(logic_substrate_bridge_t* bridge);
int logic_substrate_bridge_get_effects(const logic_substrate_bridge_t* bridge, logic_substrate_effects_t* effects);
int logic_substrate_bridge_apply_effects(logic_substrate_bridge_t* bridge);
int logic_substrate_bridge_register_bio_async(logic_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOGIC_SUBSTRATE_BRIDGE_H */
