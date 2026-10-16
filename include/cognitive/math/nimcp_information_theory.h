/**
 * @file nimcp_information_theory.h
 * @brief Information theory engine for NIMCP cognitive mathematics
 *
 * Entropy (Shannon, joint, conditional), mutual information, KL divergence,
 * cross entropy, channel capacity (BSC, BEC, AWGN), rate-distortion,
 * Huffman coding, Shannon-Fano coding, data processing inequality,
 * Fano's inequality, differential entropy for Gaussian.
 */

#ifndef NIMCP_INFORMATION_THEORY_H
#define NIMCP_INFORMATION_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define IT_MAX_SYMBOLS          256     /* maximum alphabet size              */
#define IT_MAX_CHANNEL_IN       64      /* max channel input alphabet         */
#define IT_MAX_CHANNEL_OUT      64      /* max channel output alphabet        */
#define IT_EPSILON              1e-15   /* floor for log arguments            */
#define IT_LOG2_E               1.4426950408889634  /* 1/ln(2)               */

/* --------------------------------------------------------------------------
 * Enums
 * -------------------------------------------------------------------------- */

typedef enum {
    IT_CHANNEL_GENERIC = 0,
    IT_CHANNEL_BSC,             /* binary symmetric channel                */
    IT_CHANNEL_BEC,             /* binary erasure channel                  */
    IT_CHANNEL_AWGN,            /* additive white Gaussian noise           */
    IT_CHANNEL_TYPE_COUNT
} it_channel_type_t;

typedef enum {
    IT_CODE_HUFFMAN = 0,
    IT_CODE_SHANNON_FANO,
    IT_CODE_TYPE_COUNT
} it_code_type_t;

/* --------------------------------------------------------------------------
 * Structures
 * -------------------------------------------------------------------------- */

/** Discrete probability distribution */
typedef struct {
    double   prob[IT_MAX_SYMBOLS];
    uint32_t size;              /* number of symbols */
} it_distribution_t;

/** Joint distribution P(X,Y) — row-major [x][y] */
typedef struct {
    double   prob[IT_MAX_SYMBOLS][IT_MAX_SYMBOLS];
    uint32_t size_x;
    uint32_t size_y;
} it_joint_distribution_t;

/** Communication channel with transition matrix P(Y|X) */
typedef struct {
    it_channel_type_t type;
    uint32_t          n_input;   /* input alphabet size   */
    uint32_t          n_output;  /* output alphabet size  */
    double            transition[IT_MAX_CHANNEL_IN][IT_MAX_CHANNEL_OUT];
    double            param;     /* BSC: p, BEC: epsilon, AWGN: SNR */
} it_channel_t;

/** Huffman/Shannon-Fano tree node */
typedef struct it_code_node {
    uint32_t symbol;            /* leaf symbol (IT_MAX_SYMBOLS if internal) */
    double   probability;
    uint32_t code_bits;         /* binary code as integer                  */
    uint32_t code_length;       /* code length in bits                     */
    int32_t  left;              /* index into node array (-1 if leaf)      */
    int32_t  right;
} it_code_node_t;

/** Coding result */
typedef struct {
    it_code_node_t nodes[2 * IT_MAX_SYMBOLS];
    uint32_t       n_nodes;
    uint32_t       n_symbols;
    uint32_t       code_bits[IT_MAX_SYMBOLS];     /* code per symbol   */
    uint32_t       code_lengths[IT_MAX_SYMBOLS];  /* length per symbol */
    double         avg_code_length;
    double         entropy;
    double         efficiency;   /* entropy / avg_code_length */
} it_coding_result_t;

/** Rate-distortion result */
typedef struct {
    double rate;                /* R(D) in bits */
    double distortion;          /* D             */
    bool   valid;
} it_rate_distortion_t;

/** Top-level information theory engine */
typedef struct {
    it_channel_t *channel;      /* current channel (optional) */
} info_theory_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

info_theory_t *it_create(void);
void           it_destroy(info_theory_t *it);

/* --------------------------------------------------------------------------
 * Entropy measures
 * -------------------------------------------------------------------------- */

/** Shannon entropy H(X) = -sum p*log2(p), in bits */
double it_shannon_entropy(const it_distribution_t *dist);

/** Joint entropy H(X,Y) */
double it_joint_entropy(const it_joint_distribution_t *joint);

/** Conditional entropy H(X|Y) = H(X,Y) - H(Y) */
double it_conditional_entropy(const it_joint_distribution_t *joint);

/** Mutual information I(X;Y) = H(X) + H(Y) - H(X,Y) */
double it_mutual_information(const it_joint_distribution_t *joint);

/** KL divergence D_KL(P||Q) = sum p*log(p/q) in bits */
double it_kl_divergence(const it_distribution_t *p,
                        const it_distribution_t *q);

/** Cross entropy H(P,Q) = -sum p*log(q) in bits */
double it_cross_entropy(const it_distribution_t *p,
                        const it_distribution_t *q);

/** Differential entropy of Gaussian: h(X) = 0.5*log2(2*pi*e*var) */
double it_differential_entropy_gaussian(double variance);

/* --------------------------------------------------------------------------
 * Channel capacity
 * -------------------------------------------------------------------------- */

/** Binary symmetric channel capacity: C = 1 - H(p) */
double it_capacity_bsc(double p);

/** Binary erasure channel capacity: C = 1 - epsilon */
double it_capacity_bec(double epsilon);

/** AWGN channel capacity: C = 0.5 * log2(1 + SNR) */
double it_capacity_awgn(double snr);

/** Generic channel: initialize from transition matrix */
it_channel_t *it_channel_create(uint32_t n_input, uint32_t n_output);
void           it_channel_destroy(it_channel_t *ch);
void           it_channel_set_transition(it_channel_t *ch, uint32_t x,
                                         uint32_t y, double prob);

/** Compute channel capacity for the configured channel (Blahut-Arimoto) */
double it_channel_capacity(const it_channel_t *ch, uint32_t max_iter,
                           double tol);

/* --------------------------------------------------------------------------
 * Rate-distortion
 * -------------------------------------------------------------------------- */

/** Rate-distortion for binary source with Hamming distortion */
it_rate_distortion_t it_rate_distortion_binary(double p, double D);

/* --------------------------------------------------------------------------
 * Coding
 * -------------------------------------------------------------------------- */

/** Build Huffman code */
it_coding_result_t it_huffman_encode(const it_distribution_t *dist);

/** Build Shannon-Fano code */
it_coding_result_t it_shannon_fano_encode(const it_distribution_t *dist);

/** Compute coding efficiency = H(X) / L_avg */
double it_coding_efficiency(double entropy, double avg_length);

/* --------------------------------------------------------------------------
 * Inequalities and bounds
 * -------------------------------------------------------------------------- */

/** Data processing inequality check: I(X;Z) <= I(X;Y)
 *  Given joint P(X,Y) and channel P(Z|Y), verify DPI holds */
bool it_data_processing_inequality(const it_joint_distribution_t *joint_xy,
                                   const it_channel_t *channel_yz);

/** Fano's inequality: H(X|Y) <= H(P_e) + P_e * log2(|X|-1)
 *  Returns the Fano bound given error probability and alphabet size */
double it_fano_bound(double error_prob, uint32_t alphabet_size);

/* --------------------------------------------------------------------------
 * Utility
 * -------------------------------------------------------------------------- */

/** Compute marginal P(X) from joint P(X,Y) */
void it_marginal_x(const it_joint_distribution_t *joint,
                   it_distribution_t *marginal);

/** Compute marginal P(Y) from joint P(X,Y) */
void it_marginal_y(const it_joint_distribution_t *joint,
                   it_distribution_t *marginal);

/** Validate distribution (sums to 1, non-negative) */
bool it_validate_distribution(const it_distribution_t *dist);

/** Validate joint distribution */
bool it_validate_joint(const it_joint_distribution_t *joint);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFORMATION_THEORY_H */
