/**
 * @file nimcp_emergent_language_bridge.h
 * @brief Drives the emergent-language module from the brain's own
 *        decisions and training labels.
 *
 * WHAT: The brain's outputs are observed (clustered into emergent tokens)
 *       and training labels are voted onto the token each output fell into:
 *       - every labelled brain_learn_vector: the ANN's own output for the
 *         input, read before that step's weight update, gets the step's
 *         label (exact pairing; NIMCP_EMERGENT_OBSERVE_LEARN=0 turns it off);
 *       - every brain_decide: the output is observed, and labelled when the
 *         same input vector was recently trained with a label.
 * WHY:  Gives the brain its own symbols for its output space, and a
 *       measure of whether those symbols line up with human words:
 *       before each vote, does the token's current gloss already equal the
 *       label? That running hit rate is reported against a majority-label
 *       baseline.
 * HOW:  brain_learn_vector -> emergent_bridge_on_learn (hash of input +
 *       label into a small ring) and emergent_bridge_on_learn_output (one
 *       read-only ANN forward); brain_decide -> emergent_bridge_on_decide
 *       (observe_ex, then vote if the input hash is in the ring).
 *
 * Opt-in: NIMCP_EMERGENT_LANGUAGE=1 at brain creation (default OFF; no
 * bridge is created otherwise and the hooks are no-ops).
 * Persistence: NIMCP_EMERGENT_LANGUAGE_PATH=<file> loads at creation if
 * the file exists, saves every 5000 observations, every
 * NIMCP_EMERGENT_SAVE_SECS (default 300; 0 = off) and at destroy.
 * NIMCP_EMERGENT_DISCOVERY=<d> overrides the new-token cosine distance
 * (default 0.3: a new token when the nearest is below cosine 0.7).
 * Every 1000 observations a progress line is logged: hit rates overall and
 * on never-seen inputs, and the nearest-token cosine histogram.
 */
#ifndef NIMCP_EMERGENT_LANGUAGE_BRIDGE_H
#define NIMCP_EMERGENT_LANGUAGE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct emergent_bridge emergent_bridge_t;

typedef struct {
    uint32_t vocab_size;        /* emergent tokens */
    uint32_t active_tokens;     /* matched in the last 1000 observations */
    uint32_t labeled_tokens;    /* tokens with at least one human vote */
    float    purity;            /* vote-weighted share of each token's top word */
    uint64_t observations;      /* outputs observed (decide + learn) */
    uint64_t labeled_decides;   /* observations that carried a training label */
    uint64_t predictions;       /* labelled observations whose token already had a gloss */
    uint64_t hits;              /* ... where the gloss equalled the label */
    uint64_t majority_hits;     /* baseline: always guess the most frequent label so far */
    uint32_t distinct_labels;   /* labels seen (capped at the tracking table size) */
    uint64_t decide_observations; /* of observations: from brain_decide */
    uint64_t learn_observations;  /* of observations: from labelled learn steps */
    /* Generalisation: the same scoring, restricted to inputs never observed
     * before (replays of a seen item excluded). */
    uint64_t novel_labeled;
    uint64_t novel_predictions;
    uint64_t novel_hits;
    uint64_t novel_majority_hits;
    uint64_t new_tokens;          /* observations that created a token */
    uint64_t nearest_sim_hist[11];/* nearest-token cosine at observe: [none, <.1, .1-.2, ..., >=.9] */
} emergent_bridge_stats_t;

/** Create from environment. Returns NULL unless NIMCP_EMERGENT_LANGUAGE=1. */
emergent_bridge_t* emergent_bridge_create_from_env(void);

/** Bridge held in *slot (a brain field), created on first call when the
 *  flag is on; NULL when off. Safe under concurrent callers. */
emergent_bridge_t* emergent_bridge_for(void** slot);

/** The bridge already held in *slot, or NULL. Never creates. */
emergent_bridge_t* emergent_bridge_get(void* const* slot);
void emergent_bridge_destroy(emergent_bridge_t* b);

/** Record a training example's input + label (label may be NULL/empty: ignored). */
void emergent_bridge_on_learn(emergent_bridge_t* b, const float* features,
                              uint32_t num_features, const char* label);

/** Observe the brain's own output for a labelled training input (read
 *  before the step's weight update) and vote the label onto its token. */
void emergent_bridge_on_learn_output(emergent_bridge_t* b, const float* features,
                                     uint32_t num_features, const float* output,
                                     uint32_t output_size, const char* label);

/** NIMCP_EMERGENT_OBSERVE_LEARN != "0" (default on, read once). */
bool emergent_bridge_observe_learn_enabled(void);

/** Observe a decision output for the given input. */
void emergent_bridge_on_decide(emergent_bridge_t* b, const float* features,
                               uint32_t num_features, const float* output,
                               uint32_t output_size);

int emergent_bridge_get_stats(emergent_bridge_t* b, emergent_bridge_stats_t* out);

/** Symbols for an output vector (e.g. "α3 ◊1"); returns length, or <0. */
int emergent_bridge_express(emergent_bridge_t* b, const float* output,
                            uint32_t output_size, char* text, uint32_t max_len);

#ifdef __cplusplus
}
#endif
#endif
