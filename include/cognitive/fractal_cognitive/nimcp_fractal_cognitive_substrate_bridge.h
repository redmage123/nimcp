/**
 * @file nimcp_fractal_cognitive_substrate_bridge.h
 * @brief Bridge between Fractal Cognitive system and neural substrate
 *
 * WHAT: Links fractal cognitive processing to metabolic/energy state
 * WHY: Scale-free processing requires significant computational resources
 * HOW: Monitors ATP/fatigue; modulates fractal depth, self-similarity, recursion
 *
 * BIOLOGICAL BASIS:
 * - Fractal patterns in neural activity require sustained metabolic support
 * - ATP depletion reduces fractal complexity and processing depth
 * - Fatigue leads to simpler, less scale-free processing patterns
 * - Metabolic stress limits recursive depth of self-similar computations
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_FRACTAL_COGNITIVE_SUBSTRATE_BRIDGE_H
#define NIMCP_FRACTAL_COGNITIVE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_FRACTAL_COGNITIVE 0x1305

typedef struct {
    float fractal_depth;           /* Maximum recursive depth [0-1] */
    float self_similarity;         /* Self-similarity preservation [0-1] */
    float scale_invariance;        /* Scale invariance maintenance [0-1] */
    float complexity_capacity;     /* Complexity generation capacity [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} fractal_cognitive_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} fractal_cognitive_substrate_config_t;

typedef struct fractal_cognitive_substrate_bridge fractal_cognitive_substrate_bridge_t;

fractal_cognitive_substrate_config_t fractal_cognitive_substrate_default_config(void);
fractal_cognitive_substrate_bridge_t* fractal_cognitive_substrate_bridge_create(void* fractal_cognitive, neural_substrate_t* substrate, const fractal_cognitive_substrate_config_t* config);
void fractal_cognitive_substrate_bridge_destroy(fractal_cognitive_substrate_bridge_t* bridge);
int fractal_cognitive_substrate_bridge_update(fractal_cognitive_substrate_bridge_t* bridge);
int fractal_cognitive_substrate_bridge_get_effects(const fractal_cognitive_substrate_bridge_t* bridge, fractal_cognitive_substrate_effects_t* effects);
int fractal_cognitive_substrate_bridge_apply_effects(fractal_cognitive_substrate_bridge_t* bridge);
int fractal_cognitive_substrate_bridge_register_bio_async(fractal_cognitive_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FRACTAL_COGNITIVE_SUBSTRATE_BRIDGE_H */
