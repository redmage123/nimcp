/**
 * @file nimcp_nlp_immune_brain.h
 * @brief Drives the NLP-immune bridge from a live brain.
 *
 * WHAT: Every NLP_IMMUNE_BRAIN_PERIOD learn steps:
 *       1. language -> immune: collects new held-out language outcomes
 *          (emergent-token gloss guesses, spoken-word recognitions) and
 *          feeds them to nlp_immune_record_outcomes, which releases IL-1
 *          when the brain does worse than its own running baseline and
 *          IL-10 when it does better;
 *       2. immune -> language: updates the bridge from current cytokine
 *          levels and pushes its capacity factor into grounded_language
 *          (comprehension confidence, utterance length).
 * Opt-in: NIMCP_NLP_IMMUNE=1. Capacity stays 1.0 unless cytokine clearance
 * is also on (NIMCP_IMMUNE_CYTOKINE_CLEARANCE=1); without it the immune
 * levels never fall.
 */
#ifndef NIMCP_NLP_IMMUNE_BRAIN_H
#define NIMCP_NLP_IMMUNE_BRAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NLP_IMMUNE_BRAIN_PERIOD 32

typedef struct nlp_immune_brain nlp_immune_brain_t;
struct brain_struct;

typedef struct {
    bool     clearance;          /* cytokine clearance on */
    float    capacity;           /* language capacity pushed to grounded_language */
    uint32_t inflammation_level; /* brain_inflammation_level_t */
    uint64_t outcomes;           /* held-out language outcomes recorded */
    uint64_t successes;
    float    last_window_error;
    float    baseline_error;
    uint32_t il1_releases;       /* worse than baseline */
    uint32_t il10_releases;      /* better than baseline */
    uint64_t ticks;
} nlp_immune_brain_stats_t;

/** Created when NIMCP_NLP_IMMUNE=1 and the brain has an immune system. */
nlp_immune_brain_t* nlp_immune_brain_for(void** slot, struct brain_struct* brain);
nlp_immune_brain_t* nlp_immune_brain_get(void* const* slot);
void nlp_immune_brain_destroy(nlp_immune_brain_t* nb);

/** Call once per learn step; does its work every NLP_IMMUNE_BRAIN_PERIOD calls. */
void nlp_immune_brain_tick(nlp_immune_brain_t* nb, struct brain_struct* brain);

int nlp_immune_brain_get_stats(nlp_immune_brain_t* nb, nlp_immune_brain_stats_t* out);

#ifdef __cplusplus
}
#endif
#endif
