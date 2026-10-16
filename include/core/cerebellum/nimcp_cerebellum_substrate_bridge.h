/**
 * @file nimcp_cerebellum_substrate_bridge.h
 * @brief Bridge between Cerebellum and neural substrate
 *
 * WHAT: Links cerebellar function to metabolic state
 * WHY: Cerebellum requires energy for motor coordination and timing
 * HOW: Monitors ATP/fatigue; modulates coordination, timing, motor learning
 *
 * BIOLOGICAL BASIS:
 * - Cerebellum is critical for motor timing and coordination
 * - ATP depletion impairs motor coordination
 * - Fatigue reduces timing precision
 * - Metabolic stress affects procedural learning
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CEREBELLUM_SUBSTRATE_BRIDGE_H
#define NIMCP_CEREBELLUM_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_CEREBELLUM 0x1235

typedef struct {
    float motor_coordination;     /* Movement coordination [0-1] */
    float timing_precision;       /* Temporal precision [0-1] */
    float procedural_learning;    /* Motor skill learning [0-1] */
    float error_correction;       /* Movement error correction [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} cerebellum_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} cerebellum_substrate_config_t;

typedef struct cerebellum_substrate_bridge cerebellum_substrate_bridge_t;

cerebellum_substrate_config_t cerebellum_substrate_default_config(void);
cerebellum_substrate_bridge_t* cerebellum_substrate_bridge_create(void* cerebellum, neural_substrate_t* substrate, const cerebellum_substrate_config_t* config);
void cerebellum_substrate_bridge_destroy(cerebellum_substrate_bridge_t* bridge);
int cerebellum_substrate_bridge_update(cerebellum_substrate_bridge_t* bridge);
int cerebellum_substrate_bridge_get_effects(const cerebellum_substrate_bridge_t* bridge, cerebellum_substrate_effects_t* effects);
int cerebellum_substrate_bridge_apply_effects(cerebellum_substrate_bridge_t* bridge);
int cerebellum_substrate_bridge_register_bio_async(cerebellum_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CEREBELLUM_SUBSTRATE_BRIDGE_H */
