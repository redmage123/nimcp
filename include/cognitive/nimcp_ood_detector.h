#ifndef NIMCP_OOD_DETECTOR_H
#define NIMCP_OOD_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_ood_detector nimcp_ood_detector_t;

typedef struct {
    float memory_distance_weight;    /* Default: 0.3 */
    float energy_score_weight;       /* Default: 0.25 */
    float disagreement_weight;       /* Default: 0.25 */
    float reconstruction_weight;     /* Default: 0.2 */
    float ood_threshold;             /* Default: 0.7 (above = OOD) */
    float confidence_reduction;      /* How much to reduce confidence for OOD (default: 0.5) */
    uint32_t feature_dim;            /* Expected input dimension */
    bool enable_memory_check;        /* Use memory store (default: true) */
    bool enable_energy_score;        /* Use output energy (default: true) */
    bool enable_disagreement;        /* Use network disagreement (default: true) */
    bool enable_reconstruction;      /* Use VAE recon error (default: true) */
    bool enable_bloom_precheck;      /* Fast bloom filter pre-check (default: true) */
} nimcp_ood_config_t;

typedef struct {
    float ood_score;                 /* Combined OOD score [0=in-dist, 1=OOD] */
    bool is_ood;                     /* True if ood_score > threshold */
    float memory_distance;           /* Distance to nearest memory [0-1] */
    float energy_score;              /* Output energy (higher = more uncertain) */
    float disagreement_score;        /* Network disagreement [0-1] */
    float reconstruction_error;      /* VAE reconstruction error [0-1] */
    float confidence_adjustment;     /* Multiplier to apply to output confidence */
    uint64_t nearest_memory_id;      /* ID of closest engram (0 if none) */
} nimcp_ood_result_t;

typedef struct {
    uint64_t total_checks;
    uint64_t ood_detected;
    uint64_t in_distribution;
    float avg_ood_score;
    float max_ood_score;
    float ood_rate;                  /* ood_detected / total_checks */
} nimcp_ood_stats_t;

nimcp_ood_config_t nimcp_ood_config_default(void);

nimcp_ood_detector_t* nimcp_ood_detector_create(const nimcp_ood_config_t* config);
void nimcp_ood_detector_destroy(nimcp_ood_detector_t* detector);

/* Run OOD detection on input features.
 * Optionally provide output_logits (for energy), secondary_output (for disagreement),
 * and reconstruction (for VAE error). NULL = skip that signal. */
int nimcp_ood_detect(
    nimcp_ood_detector_t* detector,
    const float* features, uint32_t feature_dim,
    const float* output_logits, uint32_t output_dim,
    const float* secondary_output, uint32_t secondary_dim,
    const float* reconstruction, uint32_t recon_dim,
    void* memory_store,  /* nimcp_memory_store_t* or NULL */
    nimcp_ood_result_t* result);

/* Update running statistics after a detection */
int nimcp_ood_update_stats(nimcp_ood_detector_t* detector, const nimcp_ood_result_t* result);

int nimcp_ood_get_stats(const nimcp_ood_detector_t* detector, nimcp_ood_stats_t* stats);

/* Compute energy score from output logits: -log(sum(exp(logits))) */
float nimcp_ood_energy_score(const float* logits, uint32_t dim);

/* Compute disagreement between two output vectors (1 - cosine similarity) */
float nimcp_ood_disagreement_score(const float* output_a, const float* output_b, uint32_t dim);

/* Compute reconstruction error (normalized MSE between input and reconstruction) */
float nimcp_ood_reconstruction_error(const float* input, const float* reconstruction, uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OOD_DETECTOR_H */
