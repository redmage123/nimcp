/**
 * @file nimcp_prefrontal_substrate_bridge.h
 * @brief Bridge between Prefrontal Cortex and neural substrate
 *
 * WHAT: Links prefrontal executive function to metabolic state
 * WHY: PFC is the most metabolically demanding cortical region
 * HOW: Monitors ATP/fatigue; modulates executive function, working memory, inhibition
 *
 * BIOLOGICAL BASIS:
 * - PFC has highest metabolic demands in brain
 * - ATP depletion impairs executive function first
 * - Fatigue reduces inhibitory control and working memory
 * - Metabolic stress affects planning and decision-making
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PREFRONTAL_SUBSTRATE_BRIDGE_H
#define NIMCP_PREFRONTAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_PREFRONTAL 0x1237

typedef struct {
    float executive_function;     /* Overall executive capacity [0-1] */
    float working_memory;         /* Working memory capacity [0-1] */
    float inhibitory_control;     /* Inhibition strength [0-1] */
    float planning_capacity;      /* Planning and sequencing [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} prefrontal_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} prefrontal_substrate_config_t;

typedef struct prefrontal_substrate_bridge prefrontal_substrate_bridge_t;

prefrontal_substrate_config_t prefrontal_substrate_default_config(void);
prefrontal_substrate_bridge_t* prefrontal_substrate_bridge_create(void* prefrontal, neural_substrate_t* substrate, const prefrontal_substrate_config_t* config);
void prefrontal_substrate_bridge_destroy(prefrontal_substrate_bridge_t* bridge);
int prefrontal_substrate_bridge_update(prefrontal_substrate_bridge_t* bridge);
int prefrontal_substrate_bridge_get_effects(const prefrontal_substrate_bridge_t* bridge, prefrontal_substrate_effects_t* effects);
int prefrontal_substrate_bridge_apply_effects(prefrontal_substrate_bridge_t* bridge);
int prefrontal_substrate_bridge_register_bio_async(prefrontal_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREFRONTAL_SUBSTRATE_BRIDGE_H */
