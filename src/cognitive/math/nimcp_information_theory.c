/**
 * @file nimcp_information_theory.c
 * @brief Information theory engine implementation
 *
 * Shannon entropy, joint/conditional entropy, mutual information,
 * KL divergence, cross entropy, channel capacity (BSC/BEC/AWGN),
 * Blahut-Arimoto, Huffman coding, Shannon-Fano coding, rate-distortion.
 */

#include "cognitive/math/nimcp_information_theory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "info_theory"

/* ========================================================================== */
/* Internal helpers                                                           */
/* ========================================================================== */

static double safe_log2(double x) {
    return (x > IT_EPSILON) ? log2(x) : 0.0;
}

static double binary_entropy(double p) {
    if (p <= IT_EPSILON || p >= 1.0 - IT_EPSILON) return 0.0;
    return -p * log2(p) - (1.0 - p) * log2(1.0 - p);
}

/* ========================================================================== */
/* Lifecycle                                                                  */
/* ========================================================================== */

info_theory_t *it_create(void) {
    info_theory_t *it = (info_theory_t *)nimcp_calloc(1, sizeof(info_theory_t));
    if (!it) {
        LOG_ERROR(LOG_TAG, "Failed to allocate info_theory_t");
        return NULL;
    }
    return it;
}

void it_destroy(info_theory_t *it) {
    if (!it) return;
    if (it->channel) it_channel_destroy(it->channel);
    nimcp_free(it);
}

/* ========================================================================== */
/* Validation                                                                 */
/* ========================================================================== */

bool it_validate_distribution(const it_distribution_t *dist) {
    if (!dist || dist->size == 0 || dist->size > IT_MAX_SYMBOLS) return false;
    double sum = 0.0;
    for (uint32_t i = 0; i < dist->size; i++) {
        if (dist->prob[i] < -IT_EPSILON) return false;
        sum += dist->prob[i];
    }
    return fabs(sum - 1.0) < 1e-6;
}

bool it_validate_joint(const it_joint_distribution_t *joint) {
    if (!joint || joint->size_x == 0 || joint->size_y == 0) return false;
    if (joint->size_x > IT_MAX_SYMBOLS || joint->size_y > IT_MAX_SYMBOLS) {
        return false;
    }
    double sum = 0.0;
    for (uint32_t i = 0; i < joint->size_x; i++) {
        for (uint32_t j = 0; j < joint->size_y; j++) {
            if (joint->prob[i][j] < -IT_EPSILON) return false;
            sum += joint->prob[i][j];
        }
    }
    return fabs(sum - 1.0) < 1e-6;
}

/* ========================================================================== */
/* Marginals                                                                  */
/* ========================================================================== */

void it_marginal_x(const it_joint_distribution_t *joint,
                   it_distribution_t *marginal) {
    if (!joint || !marginal) return;
    memset(marginal, 0, sizeof(*marginal));
    marginal->size = joint->size_x;
    for (uint32_t i = 0; i < joint->size_x; i++) {
        double sum = 0.0;
        for (uint32_t j = 0; j < joint->size_y; j++) {
            sum += joint->prob[i][j];
        }
        marginal->prob[i] = sum;
    }
}

void it_marginal_y(const it_joint_distribution_t *joint,
                   it_distribution_t *marginal) {
    if (!joint || !marginal) return;
    memset(marginal, 0, sizeof(*marginal));
    marginal->size = joint->size_y;
    for (uint32_t j = 0; j < joint->size_y; j++) {
        double sum = 0.0;
        for (uint32_t i = 0; i < joint->size_x; i++) {
            sum += joint->prob[i][j];
        }
        marginal->prob[j] = sum;
    }
}

/* ========================================================================== */
/* Entropy measures                                                           */
/* ========================================================================== */

double it_shannon_entropy(const it_distribution_t *dist) {
    if (!dist) return 0.0;
    double H = 0.0;
    for (uint32_t i = 0; i < dist->size; i++) {
        double p = dist->prob[i];
        if (p > IT_EPSILON) {
            H -= p * log2(p);
        }
    }
    return H;
}

double it_joint_entropy(const it_joint_distribution_t *joint) {
    if (!joint) return 0.0;
    double H = 0.0;
    for (uint32_t i = 0; i < joint->size_x; i++) {
        for (uint32_t j = 0; j < joint->size_y; j++) {
            double p = joint->prob[i][j];
            if (p > IT_EPSILON) {
                H -= p * log2(p);
            }
        }
    }
    return H;
}

double it_conditional_entropy(const it_joint_distribution_t *joint) {
    if (!joint) return 0.0;
    /* H(X|Y) = H(X,Y) - H(Y) */
    it_distribution_t marginal_y;
    it_marginal_y(joint, &marginal_y);
    return it_joint_entropy(joint) - it_shannon_entropy(&marginal_y);
}

double it_mutual_information(const it_joint_distribution_t *joint) {
    if (!joint) return 0.0;
    /* I(X;Y) = H(X) + H(Y) - H(X,Y) */
    it_distribution_t mx, my;
    it_marginal_x(joint, &mx);
    it_marginal_y(joint, &my);
    return it_shannon_entropy(&mx) + it_shannon_entropy(&my)
           - it_joint_entropy(joint);
}

double it_kl_divergence(const it_distribution_t *p,
                        const it_distribution_t *q) {
    if (!p || !q || p->size != q->size) return INFINITY;
    double D = 0.0;
    for (uint32_t i = 0; i < p->size; i++) {
        double pi = p->prob[i];
        double qi = q->prob[i];
        if (pi > IT_EPSILON) {
            if (qi <= IT_EPSILON) return INFINITY; /* support mismatch */
            D += pi * log2(pi / qi);
        }
    }
    return D;
}

double it_cross_entropy(const it_distribution_t *p,
                        const it_distribution_t *q) {
    if (!p || !q || p->size != q->size) return INFINITY;
    double H = 0.0;
    for (uint32_t i = 0; i < p->size; i++) {
        double pi = p->prob[i];
        double qi = q->prob[i];
        if (pi > IT_EPSILON) {
            if (qi <= IT_EPSILON) return INFINITY;
            H -= pi * log2(qi);
        }
    }
    return H;
}

double it_differential_entropy_gaussian(double variance) {
    if (variance <= 0.0) return -INFINITY;
    /* h(X) = 0.5 * log2(2 * pi * e * var) */
    return 0.5 * log2(2.0 * M_PI * M_E * variance);
}

/* ========================================================================== */
/* Channel capacity                                                           */
/* ========================================================================== */

double it_capacity_bsc(double p) {
    if (p < 0.0 || p > 1.0) return 0.0;
    return 1.0 - binary_entropy(p);
}

double it_capacity_bec(double epsilon) {
    if (epsilon < 0.0 || epsilon > 1.0) return 0.0;
    return 1.0 - epsilon;
}

double it_capacity_awgn(double snr) {
    if (snr < 0.0) return 0.0;
    return 0.5 * log2(1.0 + snr);
}

it_channel_t *it_channel_create(uint32_t n_input, uint32_t n_output) {
    if (n_input == 0 || n_input > IT_MAX_CHANNEL_IN ||
        n_output == 0 || n_output > IT_MAX_CHANNEL_OUT) {
        return NULL;
    }
    it_channel_t *ch = (it_channel_t *)nimcp_calloc(1, sizeof(it_channel_t));
    if (!ch) return NULL;
    ch->type = IT_CHANNEL_GENERIC;
    ch->n_input = n_input;
    ch->n_output = n_output;
    return ch;
}

void it_channel_destroy(it_channel_t *ch) {
    if (ch) nimcp_free(ch);
}

void it_channel_set_transition(it_channel_t *ch, uint32_t x,
                               uint32_t y, double prob) {
    if (!ch || x >= ch->n_input || y >= ch->n_output) return;
    ch->transition[x][y] = prob;
}

/**
 * Blahut-Arimoto algorithm for channel capacity.
 * Iteratively optimizes input distribution to maximize I(X;Y).
 */
double it_channel_capacity(const it_channel_t *ch, uint32_t max_iter,
                           double tol) {
    if (!ch || ch->n_input == 0 || ch->n_output == 0) return 0.0;

    uint32_t m = ch->n_input;
    uint32_t n = ch->n_output;

    /* Initialize uniform input distribution */
    double r[IT_MAX_CHANNEL_IN];
    for (uint32_t i = 0; i < m; i++) r[i] = 1.0 / (double)m;

    double prev_cap = -1.0;

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        /* Compute output distribution q(y) = sum_x r(x) * P(y|x) */
        double q[IT_MAX_CHANNEL_OUT];
        memset(q, 0, sizeof(q));
        for (uint32_t x = 0; x < m; x++) {
            for (uint32_t y = 0; y < n; y++) {
                q[y] += r[x] * ch->transition[x][y];
            }
        }

        /* Compute c(x) = prod_y [P(y|x)/q(y)]^{P(y|x)} */
        double c[IT_MAX_CHANNEL_IN];
        for (uint32_t x = 0; x < m; x++) {
            c[x] = 0.0; /* accumulate in log domain */
            for (uint32_t y = 0; y < n; y++) {
                double pyx = ch->transition[x][y];
                if (pyx > IT_EPSILON && q[y] > IT_EPSILON) {
                    c[x] += pyx * log(pyx / q[y]);
                }
            }
            c[x] = exp(c[x]);
        }

        /* Update input distribution: r_new(x) = r(x)*c(x) / sum_x r(x)*c(x) */
        double norm = 0.0;
        for (uint32_t x = 0; x < m; x++) {
            r[x] *= c[x];
            norm += r[x];
        }
        if (norm > IT_EPSILON) {
            for (uint32_t x = 0; x < m; x++) r[x] /= norm;
        }

        /* Compute current capacity estimate */
        double cap = log2(norm);
        /* More precisely: C = log2(sum_x r(x)*c(x)) but we already normalized */
        /* Recompute I(X;Y) with current r */
        double I = 0.0;
        memset(q, 0, sizeof(q));
        for (uint32_t x = 0; x < m; x++) {
            for (uint32_t y = 0; y < n; y++) {
                q[y] += r[x] * ch->transition[x][y];
            }
        }
        for (uint32_t x = 0; x < m; x++) {
            for (uint32_t y = 0; y < n; y++) {
                double pyx = ch->transition[x][y];
                if (pyx > IT_EPSILON && q[y] > IT_EPSILON && r[x] > IT_EPSILON) {
                    I += r[x] * pyx * log2(pyx / q[y]);
                }
            }
        }
        cap = I;

        if (fabs(cap - prev_cap) < tol) {
            return cap;
        }
        prev_cap = cap;
    }

    return prev_cap;
}

/* ========================================================================== */
/* Rate-distortion                                                            */
/* ========================================================================== */

it_rate_distortion_t it_rate_distortion_binary(double p, double D) {
    it_rate_distortion_t rd;
    memset(&rd, 0, sizeof(rd));
    rd.distortion = D;

    if (p < 0.0 || p > 1.0 || D < 0.0) {
        rd.valid = false;
        return rd;
    }

    double pmin = fmin(p, 1.0 - p);

    if (D >= pmin) {
        /* R(D) = 0 when D >= min(p, 1-p) */
        rd.rate = 0.0;
        rd.valid = true;
    } else if (D < 0.0) {
        rd.valid = false;
    } else {
        /* R(D) = H(p) - H(D) for binary source with Hamming distortion */
        rd.rate = binary_entropy(p) - binary_entropy(D);
        rd.valid = true;
    }
    return rd;
}

/* ========================================================================== */
/* Huffman coding                                                             */
/* ========================================================================== */

it_coding_result_t it_huffman_encode(const it_distribution_t *dist) {
    it_coding_result_t res;
    memset(&res, 0, sizeof(res));
    if (!dist || dist->size == 0 || dist->size > IT_MAX_SYMBOLS) return res;

    uint32_t n = dist->size;
    res.n_symbols = n;
    res.entropy = it_shannon_entropy(dist);

    if (n == 1) {
        res.code_bits[0] = 0;
        res.code_lengths[0] = 1;
        res.avg_code_length = 1.0;
        res.efficiency = (res.avg_code_length > 0.0)
            ? res.entropy / res.avg_code_length : 0.0;
        res.n_nodes = 1;
        return res;
    }

    /* Build Huffman tree using a simple priority queue (array-based) */
    /* Nodes: 0..n-1 are leaves, n..2n-2 are internal */
    uint32_t total_nodes = 2 * n - 1;
    res.n_nodes = total_nodes;

    /* Initialize leaf nodes */
    for (uint32_t i = 0; i < n; i++) {
        res.nodes[i].symbol = i;
        res.nodes[i].probability = dist->prob[i];
        res.nodes[i].left = -1;
        res.nodes[i].right = -1;
    }

    /* Active set tracking */
    bool active[2 * IT_MAX_SYMBOLS];
    memset(active, 0, sizeof(active));
    for (uint32_t i = 0; i < n; i++) active[i] = true;

    uint32_t next_internal = n;

    for (uint32_t step = 0; step < n - 1; step++) {
        /* Find two smallest active nodes */
        uint32_t min1 = UINT32_MAX, min2 = UINT32_MAX;
        double pmin1 = INFINITY, pmin2 = INFINITY;

        for (uint32_t i = 0; i < next_internal; i++) {
            if (!active[i]) continue;
            if (res.nodes[i].probability < pmin1) {
                pmin2 = pmin1; min2 = min1;
                pmin1 = res.nodes[i].probability; min1 = i;
            } else if (res.nodes[i].probability < pmin2) {
                pmin2 = res.nodes[i].probability; min2 = i;
            }
        }

        if (min1 == UINT32_MAX || min2 == UINT32_MAX) break;

        /* Create internal node */
        uint32_t ni = next_internal++;
        res.nodes[ni].symbol = IT_MAX_SYMBOLS; /* internal marker */
        res.nodes[ni].probability = pmin1 + pmin2;
        res.nodes[ni].left = (int32_t)min1;
        res.nodes[ni].right = (int32_t)min2;

        active[min1] = false;
        active[min2] = false;
        active[ni] = true;
    }

    /* Assign codes via DFS traversal */
    /* Stack-based traversal to avoid recursion */
    typedef struct { uint32_t node; uint32_t code; uint32_t depth; } stack_entry_t;
    stack_entry_t stack[2 * IT_MAX_SYMBOLS];
    int32_t sp = 0;

    uint32_t root = next_internal - 1;
    stack[sp].node = root;
    stack[sp].code = 0;
    stack[sp].depth = 0;
    sp++;

    while (sp > 0) {
        sp--;
        uint32_t node = stack[sp].node;
        uint32_t code = stack[sp].code;
        uint32_t depth = stack[sp].depth;

        if (res.nodes[node].left == -1 && res.nodes[node].right == -1) {
            /* Leaf */
            uint32_t sym = res.nodes[node].symbol;
            if (sym < n) {
                res.code_bits[sym] = code;
                res.code_lengths[sym] = (depth > 0) ? depth : 1;
                res.nodes[node].code_bits = code;
                res.nodes[node].code_length = res.code_lengths[sym];
            }
        } else {
            if (res.nodes[node].left >= 0) {
                stack[sp].node = (uint32_t)res.nodes[node].left;
                stack[sp].code = (code << 1);
                stack[sp].depth = depth + 1;
                sp++;
            }
            if (res.nodes[node].right >= 0) {
                stack[sp].node = (uint32_t)res.nodes[node].right;
                stack[sp].code = (code << 1) | 1;
                stack[sp].depth = depth + 1;
                sp++;
            }
        }
    }

    /* Compute average code length */
    double avg = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        avg += dist->prob[i] * (double)res.code_lengths[i];
    }
    res.avg_code_length = avg;
    res.efficiency = (avg > 0.0) ? res.entropy / avg : 0.0;

    return res;
}

/* ========================================================================== */
/* Shannon-Fano coding                                                        */
/* ========================================================================== */

static void shannon_fano_recursive(const uint32_t *sorted_idx,
                                   const double *probs, uint32_t start,
                                   uint32_t end, uint32_t code,
                                   uint32_t depth, it_coding_result_t *res) {
    if (start >= end) return;
    if (end - start == 1) {
        uint32_t sym = sorted_idx[start];
        res->code_bits[sym] = code;
        res->code_lengths[sym] = (depth > 0) ? depth : 1;
        return;
    }

    /* Find split point that most evenly divides total probability */
    double total = 0.0;
    for (uint32_t i = start; i < end; i++) total += probs[sorted_idx[i]];

    double running = 0.0;
    double best_diff = total;
    uint32_t split = start + 1;

    for (uint32_t i = start; i < end - 1; i++) {
        running += probs[sorted_idx[i]];
        double diff = fabs(2.0 * running - total);
        if (diff < best_diff) {
            best_diff = diff;
            split = i + 1;
        }
    }

    shannon_fano_recursive(sorted_idx, probs, start, split,
                           (code << 1), depth + 1, res);
    shannon_fano_recursive(sorted_idx, probs, split, end,
                           (code << 1) | 1, depth + 1, res);
}

it_coding_result_t it_shannon_fano_encode(const it_distribution_t *dist) {
    it_coding_result_t res;
    memset(&res, 0, sizeof(res));
    if (!dist || dist->size == 0 || dist->size > IT_MAX_SYMBOLS) return res;

    uint32_t n = dist->size;
    res.n_symbols = n;
    res.entropy = it_shannon_entropy(dist);

    /* Sort symbols by probability (descending) via insertion sort */
    uint32_t sorted[IT_MAX_SYMBOLS];
    for (uint32_t i = 0; i < n; i++) sorted[i] = i;
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = sorted[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && dist->prob[sorted[j]] < dist->prob[key]) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    shannon_fano_recursive(sorted, dist->prob, 0, n, 0, 0, &res);

    double avg = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        avg += dist->prob[i] * (double)res.code_lengths[i];
    }
    res.avg_code_length = avg;
    res.efficiency = (avg > 0.0) ? res.entropy / avg : 0.0;

    return res;
}

/* ========================================================================== */
/* Coding efficiency                                                          */
/* ========================================================================== */

double it_coding_efficiency(double entropy, double avg_length) {
    if (avg_length <= 0.0) return 0.0;
    return entropy / avg_length;
}

/* ========================================================================== */
/* Inequalities                                                               */
/* ========================================================================== */

bool it_data_processing_inequality(const it_joint_distribution_t *joint_xy,
                                   const it_channel_t *channel_yz) {
    if (!joint_xy || !channel_yz) return false;
    if (joint_xy->size_y != channel_yz->n_input) return false;

    /* Compute I(X;Y) */
    double I_xy = it_mutual_information(joint_xy);

    /* Build joint P(X,Z) by marginalizing over Y:
     * P(x,z) = sum_y P(x,y) * P(z|y) */
    it_joint_distribution_t joint_xz;
    memset(&joint_xz, 0, sizeof(joint_xz));
    joint_xz.size_x = joint_xy->size_x;
    joint_xz.size_y = channel_yz->n_output;

    for (uint32_t x = 0; x < joint_xy->size_x; x++) {
        for (uint32_t z = 0; z < channel_yz->n_output; z++) {
            double pxz = 0.0;
            for (uint32_t y = 0; y < joint_xy->size_y; y++) {
                pxz += joint_xy->prob[x][y] * channel_yz->transition[y][z];
            }
            joint_xz.prob[x][z] = pxz;
        }
    }

    double I_xz = it_mutual_information(&joint_xz);

    /* DPI: I(X;Z) <= I(X;Y) */
    return I_xz <= I_xy + 1e-10; /* small tolerance for numerics */
}

double it_fano_bound(double error_prob, uint32_t alphabet_size) {
    if (alphabet_size <= 1) return 0.0;
    if (error_prob < IT_EPSILON) return 0.0;
    if (error_prob >= 1.0) return log2((double)alphabet_size);

    /* Fano's inequality: H(X|Y) <= H(P_e) + P_e * log2(|X| - 1) */
    return binary_entropy(error_prob) +
           error_prob * log2((double)(alphabet_size - 1));
}
