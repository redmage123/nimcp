/**
 * @file nimcp_speech_units.h
 * @brief Learned sound units: audio -> sequence of self-organised
 *        phonetic categories.
 *
 * WHAT: Frames audio (25 ms window, 10 ms hop), computes log-mel spectra
 *       with per-utterance mean normalisation, assigns each voiced frame to
 *       the nearest of K learned category centroids, and collapses runs into
 *       a unit sequence.
 * WHY:  speech_cortex_detect_phonemes() was a stub (always 0 phonemes), so
 *       nothing downstream of it ever saw speech structure. Infants learn
 *       phonetic categories from the distribution of sounds they hear
 *       (Maye, Werker & Gerken 2002); online clustering of spectral frames is
 *       the same idea, with no labels and no fixed IPA inventory.
 * HOW:  Online k-means. Centroids are seeded from the first sufficiently
 *       distinct voiced frames, then move toward assigned frames with a
 *       running-mean rate that floors at 0.002 so categories keep tracking
 *       the input. Frames more than 22 dB below the utterance peak are
 *       silence. Single-frame runs between two runs are dropped as noise.
 *
 * Unit ids are arbitrary category numbers [0, K). They are NOT IPA
 * phonemes; callers that store them in phoneme_t slots must not read them
 * as specific sounds.
 */
#ifndef NIMCP_SPEECH_UNITS_H
#define NIMCP_SPEECH_UNITS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct speech_units speech_units_t;

typedef struct {
    uint16_t unit;        /* category id [0, K) */
    uint16_t frames;      /* run length in frames (10 ms each) */
    uint32_t onset_ms;
    float    confidence;  /* exp(-mean distance to centroid) */
} speech_unit_seg_t;

/** sample_rate in Hz (8000..48000); num_units K (2..256). NULL on bad args. */
speech_units_t* speech_units_create(uint32_t sample_rate, uint32_t num_units);
void speech_units_destroy(speech_units_t* su);

/**
 * Segment audio into unit runs. learn=true also updates the centroids
 * (and seeds unseeded ones). Returns the number of segments written to out
 * (0 for silence / too short), or -1 on error.
 */
int speech_units_segment(speech_units_t* su, const float* audio, uint32_t num_samples,
                         bool learn, speech_unit_seg_t* out, uint32_t max_out);

uint32_t speech_units_num_units(const speech_units_t* su);
uint32_t speech_units_num_seeded(const speech_units_t* su);
uint64_t speech_units_frames_learned(const speech_units_t* su);

int speech_units_save(speech_units_t* su, const char* path);
/** Load centroids saved with the same sample rate, K and band count; -1 otherwise. */
int speech_units_load(speech_units_t* su, const char* path);

#ifdef __cplusplus
}
#endif
#endif
