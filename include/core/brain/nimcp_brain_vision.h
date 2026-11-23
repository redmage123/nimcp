/**
 * @file nimcp_brain_vision.h
 * @brief The brain's vision: retina (rods, cones, horizontal, bipolar,
 *        amacrine, ganglion cells) -> optic nerve -> sensory hub (LGN relay,
 *        occipital lobe, columns, hippocampus, ...).
 *
 * WHAT: Owns a retina_circuit_t (nimcp_retina_circuit.h, built from the
 *       per-cell base modules) whose ganglion-cell axons form the optic
 *       nerve. brain_vision_see() presents one frame for a duration; the
 *       spikes that reach the LGN end are posted to the sensory hub.
 * WHY:  images used to go straight from pixels into a cortex CNN; there was
 *       no retina (nimcp_retina.c was empty).
 * HOW:  created at eager init; called where the trainer stages a visual
 *       frame (submit_sensory). Thread-safe (own mutex). Adaptation state
 *       and myelination are saved as the checkpoint's .retina sidecar.
 */
#ifndef NIMCP_BRAIN_VISION_H
#define NIMCP_BRAIN_VISION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

typedef struct {
    bool enabled;
    uint64_t frames;
    double seconds_seen;
    float rgc_rate_hz;          /**< mean ganglion-cell rate, last frame */
    float lgn_rate_hz;          /**< mean rate arriving at the LGN, last frame */
    float latency_ms;           /**< mean eye->LGN delay, last frame */
    float rod_activation, cone_activation;
    float mean_myelination;
    float wall_ms;              /**< cost of the last frame */
} brain_vision_stats_t;

int brain_vision_create(struct brain_struct* brain);
void brain_vision_destroy(struct brain_struct* brain);

/** See one frame (uint8 HWC, ch 1/3/4) for duration_ms (<= 0: 100 ms). */
int brain_vision_see(struct brain_struct* brain, const uint8_t* pixels, uint32_t w, uint32_t h,
                     uint32_t ch, float duration_ms);

bool brain_vision_get_stats(struct brain_struct* brain, brain_vision_stats_t* out);

/** <checkpoint>.retina sidecar. load: 1 if absent. */
int brain_vision_save(struct brain_struct* brain, const char* checkpoint_path);
int brain_vision_load(struct brain_struct* brain, const char* checkpoint_path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_VISION_H */
