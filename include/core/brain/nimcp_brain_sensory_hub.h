/**
 * @file nimcp_brain_sensory_hub.h
 * @brief Sensory hub: fans what the brain hears (and sees) out to the rest
 *        of the brain.
 *
 * WHAT: Hearing (cochlea -> auditory nerve -> Wernicke predictor) and vision
 *       (retina -> optic nerve) post each frame's summary here from their
 *       own threads. brain_sensory_hub_tick(), called from the brain's tick
 *       chain (brain_learn_vector / brain_decide), drains it and drives:
 *         thalamus (MGN/LGN relay), bio-async router (broadcast + Wernicke /
 *         Broca onset messages), cortical hypercolumns, the Wernicke SNN
 *         population, hippocampus (novel sounds), working memory and the
 *         global workspace (salient onsets), medulla (arousal on loud
 *         onsets), basal ganglia (intrinsic reward = learning progress),
 *         cerebellum (mossy input + climbing-fibre prediction error), the
 *         immune system (anomalous input; only when the immune system is
 *         enabled), symbolic logic (rate-limited facts), the knowledge graph
 *         (structure registered at attach) and the health agent (heartbeat
 *         + self-repair of a diverged predictor).
 * WHY:  the brain's other modules are not thread-safe against the hearing
 *       feed thread; the tick chain is where they are normally driven.
 * HOW:  posts accumulate under a mutex; a tick processes the accumulated
 *       frames as one event. Every target is optional (NULL-safe).
 */
#ifndef NIMCP_BRAIN_SENSORY_HUB_H
#define NIMCP_BRAIN_SENSORY_HUB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

#define SENSORY_HUB_AUDIO_CHANNELS 64u
#define SENSORY_HUB_VISUAL_MAX 1024u

/** Counters: how often each target was actually driven. */
typedef struct {
    uint64_t ticks;                 /**< ticks that had something to process */
    uint64_t audio_frames;          /**< 40 ms frames received */
    uint64_t visual_frames;
    uint64_t router_broadcasts, router_sends;
    uint64_t thalamus_relays;
    uint64_t column_updates;
    uint64_t wernicke_injections;
    uint64_t hippocampus_encodes;
    uint64_t working_memory_adds, workspace_bids;
    uint64_t medulla_boosts;
    uint64_t bg_rewards;
    uint64_t cerebellum_updates;
    uint64_t immune_antigens;
    uint64_t logic_facts;
    uint64_t occipital_frames;
    uint64_t kg_nodes, kg_edges;
    uint64_t onsets;
    uint64_t self_repairs;
    float last_surprise;            /**< MSE / MSE(copy) of the last tick */
    float last_loudness;
} brain_sensory_hub_stats_t;

/** One frame of vision after the optic nerve (posted by the retina path). */
typedef struct {
    uint32_t n;                     /**< values in lgn (<= SENSORY_HUB_VISUAL_MAX) */
    float lgn[SENSORY_HUB_VISUAL_MAX];   /**< ganglion-cell rates at the LGN end, [0,1] */
    float luminance;                /**< mean photoreceptor activation */
    float on_off_contrast;          /**< mean |ON - OFF| */
    float change;                   /**< frame-to-frame change of lgn */
    uint32_t map_w, map_h;          /**< retinotopic ON/OFF maps (<= 16x16) */
    float on_map[256];              /**< ON ganglion-cell rate, [0,1] */
    float off_map[256];             /**< OFF ganglion-cell rate, [0,1] */
} brain_sensory_visual_frame_t;

/** Create brain->sensory_hub: register with the bio-async router, add the
 *  sensory structure to the knowledge graph, assert base facts, hook the
 *  health agent. Idempotent. 0 on success. */
int brain_sensory_hub_attach(struct brain_struct* brain);
void brain_sensory_hub_destroy(struct brain_struct* brain);

/** From the hearing thread, once per 40 ms frame. */
void brain_sensory_hub_post_audio(struct brain_struct* brain, const uint16_t* counts, uint32_t channels,
                                  const float* err, float mse, float copy_mse,
                                  const float* hidden, uint32_t n_hidden);

/** From the vision path, once per seen frame. */
void brain_sensory_hub_post_visual(struct brain_struct* brain, const brain_sensory_visual_frame_t* f);

/** From the brain's tick chain. */
void brain_sensory_hub_tick(struct brain_struct* brain);

bool brain_sensory_hub_get_stats(struct brain_struct* brain, brain_sensory_hub_stats_t* out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_SENSORY_HUB_H */
