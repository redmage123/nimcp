/**
 * @file nimcp_emotion_tensor_substrate_bridge.h
 * @brief Bridge between Emotion Tensor system and neural substrate
 *
 * WHAT: Links tensor-based emotional representation to metabolic/energy state
 * WHY: Emotional processing requires significant metabolic resources
 * HOW: Monitors ATP/fatigue; modulates emotional intensity, valence resolution, complexity
 *
 * BIOLOGICAL BASIS:
 * - Emotional processing involves limbic structures with high metabolic demand
 * - ATP depletion leads to emotional blunting and reduced valence discrimination
 * - Fatigue increases emotional reactivity and reduces regulation capacity
 * - Metabolic stress simplifies emotional representation (reduced tensor rank)
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTION_TENSOR_SUBSTRATE_BRIDGE_H
#define NIMCP_EMOTION_TENSOR_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_EMOTION_TENSOR 0x1302

typedef struct {
    float intensity_capacity;      /* Maximum emotional intensity [0-1] */
    float valence_resolution;      /* Valence discrimination ability [0-1] */
    float tensor_complexity;       /* Tensor rank/complexity capacity [0-1] */
    float regulation_capacity;     /* Emotional regulation ability [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} emotion_tensor_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} emotion_tensor_substrate_config_t;

typedef struct emotion_tensor_substrate_bridge emotion_tensor_substrate_bridge_t;

emotion_tensor_substrate_config_t emotion_tensor_substrate_default_config(void);
emotion_tensor_substrate_bridge_t* emotion_tensor_substrate_bridge_create(void* emotion_tensor, neural_substrate_t* substrate, const emotion_tensor_substrate_config_t* config);
void emotion_tensor_substrate_bridge_destroy(emotion_tensor_substrate_bridge_t* bridge);
int emotion_tensor_substrate_bridge_update(emotion_tensor_substrate_bridge_t* bridge);
int emotion_tensor_substrate_bridge_get_effects(const emotion_tensor_substrate_bridge_t* bridge, emotion_tensor_substrate_effects_t* effects);
int emotion_tensor_substrate_bridge_apply_effects(emotion_tensor_substrate_bridge_t* bridge);
int emotion_tensor_substrate_bridge_register_bio_async(emotion_tensor_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_TENSOR_SUBSTRATE_BRIDGE_H */
