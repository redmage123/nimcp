/**
 * @file nimcp_brain_hearing.h
 * @brief The brain's hearing: cochlea -> auditory nerve -> Wernicke predictor.
 *
 * WHAT: Owns the auditory learner (nimcp_auditory_predictor) that consumes
 *       the brain cochlea's auditory-nerve output, frame by frame.
 * WHY:  brain_feed_audio used to run the cochlea into a buffer nothing
 *       learned from (and had no callers). Hearing is the consumer.
 * HOW:  brain_hearing_feed() cuts the audio into 40 ms blocks (a remainder
 *       is carried into the next call), runs each block through the
 *       cochlea into brain->cochlea_output_buffer (so the cochlea bridges
 *       see real sound), sums each channel's spikes over the block and
 *       steps the predictor. Created at brain init with the cochlea;
 *       its learned state is saved as the checkpoint's .hear sidecar.
 *       Thread-safe: an internal mutex serialises feed/save/load/stats,
 *       independent of any brain-wide lock.
 */
#ifndef NIMCP_BRAIN_HEARING_H
#define NIMCP_BRAIN_HEARING_H

#include <stdbool.h>
#include <stdint.h>

#include "perception/nimcp_auditory_predictor.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/** The brain cochlea's sample rate (nimcp_brain_init_cochlea.c creates it). */
#define BRAIN_HEARING_SAMPLE_RATE 16000u

/** Create brain->hearing (needs brain->cochlea). 0 on success. */
int brain_hearing_create(struct brain_struct* brain);
void brain_hearing_destroy(struct brain_struct* brain);

/** Hear mono audio at the cochlea's sample rate (16 kHz). Returns the
 *  number of 40 ms frames learned from, or -1 on error. */
int brain_hearing_feed(struct brain_struct* brain, const float* audio, uint32_t num_samples);

/** false if the brain has no hearing. */
bool brain_hearing_get_stats(struct brain_struct* brain, auditory_predictor_stats_t* out);

/** Self-repair: replace the predictor with a fresh one (same channels). */
int brain_hearing_reset(struct brain_struct* brain);

/** Sidecar persistence: <checkpoint>.hear. 0 on success; load returns 1 if
 *  there is no sidecar (a fresh learner is kept). */
int brain_hearing_save(struct brain_struct* brain, const char* checkpoint_path);
int brain_hearing_load(struct brain_struct* brain, const char* checkpoint_path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_HEARING_H */
