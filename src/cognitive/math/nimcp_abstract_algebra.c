/**
 * @file nimcp_abstract_algebra.c
 * @brief Abstract algebra engine implementation
 */

#include "cognitive/math/nimcp_abstract_algebra.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "ABS_ALGEBRA"

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

static uint64_t mod_pow_gf(uint64_t base, uint64_t exp, uint64_t p) {
    if (p == 1) return 0;
    uint64_t result = 1;
    base %= p;
    while (exp > 0) {
        if (exp & 1) result = (result * base) % p;
        exp >>= 1;
        base = (base * base) % p;
    }
    return result;
}

static int64_t mod_pos(int64_t a, uint64_t p) {
    int64_t r = a % (int64_t)p;
    if (r < 0) r += (int64_t)p;
    return r;
}

static uint32_t gcd_u32(uint32_t a, uint32_t b) {
    while (b) { uint32_t t = b; b = a % b; a = t; }
    return a;
}

/* =========================================================================
 * Group lifecycle
 * ========================================================================= */

group_t *aa_group_create(uint32_t order) {
    if (order == 0 || order > AA_MAX_GROUP_ORDER) {
        LOG_ERROR("Group order %u out of range [1, %d]", order, AA_MAX_GROUP_ORDER);
        return NULL;
    }
    group_t *g = (group_t *)nimcp_calloc(1, sizeof(group_t));
    if (!g) return NULL;
    g->order = order;
    g->identity = -1;
    g->valid = false;
    /* Initialize Cayley table to -1 (undefined) */
    memset(g->cayley, 0xFF, sizeof(g->cayley));
    LOG_INFO("Group created with order %u", order);
    return g;
}

void aa_group_destroy(group_t *g) {
    if (!g) return;
    nimcp_free(g);
}

/* =========================================================================
 * Group axiom checks
 * ========================================================================= */

int32_t aa_group_find_identity(const group_t *g) {
    if (!g) return -1;
    for (uint32_t e = 0; e < g->order; e++) {
        bool is_id = true;
        for (uint32_t a = 0; a < g->order && is_id; a++) {
            if (g->cayley[e][a] != (int32_t)a || g->cayley[a][e] != (int32_t)a) {
                is_id = false;
            }
        }
        if (is_id) return (int32_t)e;
    }
    return -1;
}

group_axiom_result_t aa_group_check_axioms(const group_t *g) {
    group_axiom_result_t r;
    memset(&r, 0, sizeof(r));
    if (!g) return r;

    uint32_t n = g->order;

    /* Closure: all products in [0, n) */
    r.closure = true;
    for (uint32_t i = 0; i < n && r.closure; i++) {
        for (uint32_t j = 0; j < n && r.closure; j++) {
            int32_t prod = g->cayley[i][j];
            if (prod < 0 || prod >= (int32_t)n) r.closure = false;
        }
    }

    /* Associativity: (a*b)*c = a*(b*c) */
    r.associativity = true;
    if (r.closure) {
        for (uint32_t a = 0; a < n && r.associativity; a++) {
            for (uint32_t b = 0; b < n && r.associativity; b++) {
                for (uint32_t c = 0; c < n && r.associativity; c++) {
                    int32_t ab = g->cayley[a][b];
                    int32_t bc = g->cayley[b][c];
                    if (ab < 0 || bc < 0) { r.associativity = false; break; }
                    int32_t ab_c = g->cayley[ab][c];
                    int32_t a_bc = g->cayley[a][bc];
                    if (ab_c != a_bc) r.associativity = false;
                }
            }
        }
    }

    /* Identity */
    int32_t e = aa_group_find_identity(g);
    r.has_identity = (e >= 0);

    /* Inverses */
    r.has_inverses = true;
    if (r.has_identity && r.closure) {
        for (uint32_t a = 0; a < n; a++) {
            bool found = false;
            for (uint32_t b = 0; b < n; b++) {
                if (g->cayley[a][b] == e && g->cayley[b][a] == e) {
                    found = true;
                    break;
                }
            }
            if (!found) { r.has_inverses = false; break; }
        }
    } else {
        r.has_inverses = false;
    }

    r.is_group = r.closure && r.associativity && r.has_identity && r.has_inverses;

    /* Abelian check */
    r.is_abelian = true;
    if (r.closure) {
        for (uint32_t a = 0; a < n && r.is_abelian; a++) {
            for (uint32_t b = 0; b < n && r.is_abelian; b++) {
                if (g->cayley[a][b] != g->cayley[b][a]) r.is_abelian = false;
            }
        }
    }

    return r;
}

int32_t aa_group_inverse(const group_t *g, int32_t elem) {
    if (!g || elem < 0 || elem >= (int32_t)g->order) return -1;
    int32_t e = g->identity >= 0 ? g->identity : aa_group_find_identity(g);
    if (e < 0) return -1;
    for (uint32_t b = 0; b < g->order; b++) {
        if (g->cayley[elem][b] == e && g->cayley[b][elem] == e) {
            return (int32_t)b;
        }
    }
    return -1;
}

uint32_t aa_group_element_order(const group_t *g, int32_t elem) {
    if (!g || elem < 0 || elem >= (int32_t)g->order) return 0;
    int32_t e = g->identity >= 0 ? g->identity : aa_group_find_identity(g);
    if (e < 0) return 0;
    int32_t current = elem;
    for (uint32_t k = 1; k <= g->order; k++) {
        if (current == e) return k;
        current = g->cayley[current][elem];
        if (current < 0 || current >= (int32_t)g->order) return 0;
    }
    return 0;
}

bool aa_group_is_subgroup(const group_t *g, const int32_t *subset, uint32_t size) {
    if (!g || !subset || size == 0) return false;

    /* Check closure under group operation */
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            int32_t prod = g->cayley[subset[i]][subset[j]];
            bool in_subset = false;
            for (uint32_t k = 0; k < size; k++) {
                if (subset[k] == prod) { in_subset = true; break; }
            }
            if (!in_subset) return false;
        }
    }

    /* Check identity is in subset */
    int32_t e = g->identity >= 0 ? g->identity : aa_group_find_identity(g);
    bool has_id = false;
    for (uint32_t i = 0; i < size; i++) {
        if (subset[i] == e) { has_id = true; break; }
    }
    if (!has_id) return false;

    /* Check inverses */
    for (uint32_t i = 0; i < size; i++) {
        int32_t inv = aa_group_inverse(g, subset[i]);
        bool in_subset = false;
        for (uint32_t k = 0; k < size; k++) {
            if (subset[k] == inv) { in_subset = true; break; }
        }
        if (!in_subset) return false;
    }
    return true;
}

coset_t aa_group_left_coset(const group_t *g, int32_t elem,
                            const int32_t *subgroup, uint32_t sg_size) {
    coset_t c;
    memset(&c, 0, sizeof(c));
    if (!g || !subgroup) return c;
    for (uint32_t i = 0; i < sg_size && c.size < AA_MAX_GROUP_ORDER; i++) {
        c.elements[c.size++] = g->cayley[elem][subgroup[i]];
    }
    return c;
}

bool aa_group_verify_lagrange(const group_t *g, const int32_t *subgroup,
                              uint32_t sg_size) {
    if (!g || !subgroup || sg_size == 0) return false;
    /* Lagrange: |H| divides |G| */
    return (g->order % sg_size == 0);
}

/* =========================================================================
 * Ring
 * ========================================================================= */

ring_t *aa_ring_create(uint32_t order) {
    if (order == 0 || order > AA_MAX_GROUP_ORDER) return NULL;
    ring_t *r = (ring_t *)nimcp_calloc(1, sizeof(ring_t));
    if (!r) return NULL;
    r->order = order;
    r->zero = -1;
    r->one = -1;
    memset(r->add_table, 0xFF, sizeof(r->add_table));
    memset(r->mul_table, 0xFF, sizeof(r->mul_table));
    return r;
}

void aa_ring_destroy(ring_t *r) {
    if (r) nimcp_free(r);
}

ring_axiom_result_t aa_ring_check_axioms(const ring_t *r) {
    ring_axiom_result_t res;
    memset(&res, 0, sizeof(res));
    if (!r) return res;
    uint32_t n = r->order;

    /* Additive closure */
    res.add_closure = true;
    for (uint32_t i = 0; i < n && res.add_closure; i++)
        for (uint32_t j = 0; j < n && res.add_closure; j++)
            if (r->add_table[i][j] < 0 || r->add_table[i][j] >= (int32_t)n)
                res.add_closure = false;

    /* Additive associativity */
    res.add_associativity = true;
    if (res.add_closure) {
        for (uint32_t a = 0; a < n && res.add_associativity; a++)
            for (uint32_t b = 0; b < n && res.add_associativity; b++)
                for (uint32_t c = 0; c < n && res.add_associativity; c++) {
                    int32_t ab = r->add_table[a][b];
                    int32_t bc = r->add_table[b][c];
                    if (r->add_table[ab][c] != r->add_table[a][bc])
                        res.add_associativity = false;
                }
    }

    /* Additive commutativity */
    res.add_commutativity = true;
    if (res.add_closure) {
        for (uint32_t a = 0; a < n && res.add_commutativity; a++)
            for (uint32_t b = 0; b < n && res.add_commutativity; b++)
                if (r->add_table[a][b] != r->add_table[b][a])
                    res.add_commutativity = false;
    }

    /* Additive identity */
    res.add_identity = false;
    for (uint32_t e = 0; e < n && !res.add_identity; e++) {
        bool ok = true;
        for (uint32_t a = 0; a < n && ok; a++)
            if (r->add_table[e][a] != (int32_t)a || r->add_table[a][e] != (int32_t)a)
                ok = false;
        if (ok) res.add_identity = true;
    }

    /* Additive inverses */
    res.add_inverses = true;
    if (res.add_identity && res.add_closure) {
        int32_t zero = r->zero;
        if (zero < 0) {
            for (uint32_t e = 0; e < n; e++) {
                bool ok = true;
                for (uint32_t a = 0; a < n && ok; a++)
                    if (r->add_table[e][a] != (int32_t)a) ok = false;
                if (ok) { zero = (int32_t)e; break; }
            }
        }
        for (uint32_t a = 0; a < n; a++) {
            bool found = false;
            for (uint32_t b = 0; b < n; b++)
                if (r->add_table[a][b] == zero) { found = true; break; }
            if (!found) { res.add_inverses = false; break; }
        }
    }

    /* Multiplicative closure */
    res.mul_closure = true;
    for (uint32_t i = 0; i < n && res.mul_closure; i++)
        for (uint32_t j = 0; j < n && res.mul_closure; j++)
            if (r->mul_table[i][j] < 0 || r->mul_table[i][j] >= (int32_t)n)
                res.mul_closure = false;

    /* Multiplicative associativity */
    res.mul_associativity = true;
    if (res.mul_closure) {
        for (uint32_t a = 0; a < n && res.mul_associativity; a++)
            for (uint32_t b = 0; b < n && res.mul_associativity; b++)
                for (uint32_t c = 0; c < n && res.mul_associativity; c++) {
                    int32_t ab = r->mul_table[a][b];
                    int32_t bc = r->mul_table[b][c];
                    if (r->mul_table[ab][c] != r->mul_table[a][bc])
                        res.mul_associativity = false;
                }
    }

    /* Distributivity: a*(b+c) = a*b + a*c */
    res.distributive_left = true;
    res.distributive_right = true;
    if (res.add_closure && res.mul_closure) {
        for (uint32_t a = 0; a < n; a++) {
            for (uint32_t b = 0; b < n; b++) {
                for (uint32_t c = 0; c < n; c++) {
                    int32_t bpc = r->add_table[b][c];
                    int32_t lhs_l = r->mul_table[a][bpc];
                    int32_t rhs_l = r->add_table[r->mul_table[a][b]][r->mul_table[a][c]];
                    if (lhs_l != rhs_l) res.distributive_left = false;

                    int32_t lhs_r = r->mul_table[bpc][a];
                    int32_t rhs_r = r->add_table[r->mul_table[b][a]][r->mul_table[c][a]];
                    if (lhs_r != rhs_r) res.distributive_right = false;
                }
            }
        }
    }

    /* Unity (multiplicative identity) */
    res.has_unity = false;
    for (uint32_t e = 0; e < n && !res.has_unity; e++) {
        bool ok = true;
        for (uint32_t a = 0; a < n && ok; a++)
            if (r->mul_table[e][a] != (int32_t)a || r->mul_table[a][e] != (int32_t)a)
                ok = false;
        if (ok) res.has_unity = true;
    }

    /* Commutative ring? */
    res.is_commutative_ring = true;
    if (res.mul_closure) {
        for (uint32_t a = 0; a < n && res.is_commutative_ring; a++)
            for (uint32_t b = 0; b < n && res.is_commutative_ring; b++)
                if (r->mul_table[a][b] != r->mul_table[b][a])
                    res.is_commutative_ring = false;
    }

    res.is_ring = res.add_closure && res.add_associativity &&
                  res.add_commutativity && res.add_identity &&
                  res.add_inverses && res.mul_closure &&
                  res.mul_associativity && res.distributive_left &&
                  res.distributive_right;
    res.is_commutative_ring = res.is_ring && res.is_commutative_ring;

    return res;
}

bool aa_ring_is_ideal(const ring_t *r, const int32_t *subset, uint32_t size) {
    if (!r || !subset || size == 0) return false;
    uint32_t n = r->order;

    /* Must be an additive subgroup (closure + identity + inverses) */
    int32_t zero = r->zero;
    bool has_zero = false;
    for (uint32_t i = 0; i < size; i++)
        if (subset[i] == zero) { has_zero = true; break; }
    if (!has_zero) return false;

    /* Additive closure */
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            int32_t sum = r->add_table[subset[i]][subset[j]];
            bool in_s = false;
            for (uint32_t k = 0; k < size; k++)
                if (subset[k] == sum) { in_s = true; break; }
            if (!in_s) return false;
        }
    }

    /* Absorption: r*a in I and a*r in I for all r in R, a in I */
    for (uint32_t ri = 0; ri < n; ri++) {
        for (uint32_t i = 0; i < size; i++) {
            int32_t left = r->mul_table[ri][subset[i]];
            int32_t right = r->mul_table[subset[i]][ri];
            bool left_in = false, right_in = false;
            for (uint32_t k = 0; k < size; k++) {
                if (subset[k] == left) left_in = true;
                if (subset[k] == right) right_in = true;
            }
            if (!left_in || !right_in) return false;
        }
    }
    return true;
}

/* =========================================================================
 * GF(p)
 * ========================================================================= */

gf_field_t *aa_gf_create(uint64_t p) {
    if (p < 2) return NULL;
    gf_field_t *f = (gf_field_t *)nimcp_calloc(1, sizeof(gf_field_t));
    if (!f) return NULL;
    f->p = p;
    return f;
}

void aa_gf_destroy(gf_field_t *f) { if (f) nimcp_free(f); }

uint64_t aa_gf_add(const gf_field_t *f, uint64_t a, uint64_t b) {
    return (a % f->p + b % f->p) % f->p;
}

uint64_t aa_gf_sub(const gf_field_t *f, uint64_t a, uint64_t b) {
    return ((a % f->p) + f->p - (b % f->p)) % f->p;
}

uint64_t aa_gf_mul(const gf_field_t *f, uint64_t a, uint64_t b) {
    return ((a % f->p) * (b % f->p)) % f->p;
}

uint64_t aa_gf_inv(const gf_field_t *f, uint64_t a) {
    if (a % f->p == 0) return 0;
    return mod_pow_gf(a % f->p, f->p - 2, f->p); /* Fermat's little theorem */
}

uint64_t aa_gf_div(const gf_field_t *f, uint64_t a, uint64_t b) {
    uint64_t inv = aa_gf_inv(f, b);
    if (inv == 0 && b % f->p != 0) return 0;
    return aa_gf_mul(f, a, inv);
}

uint64_t aa_gf_pow(const gf_field_t *f, uint64_t base, uint64_t exp) {
    return mod_pow_gf(base % f->p, exp, f->p);
}

/* =========================================================================
 * Permutations
 * ========================================================================= */

permutation_t aa_perm_identity(uint32_t n) {
    permutation_t p;
    memset(&p, 0, sizeof(p));
    p.n = (n > AA_MAX_PERM_SIZE) ? AA_MAX_PERM_SIZE : n;
    for (uint32_t i = 0; i < p.n; i++) p.image[i] = i;
    return p;
}

permutation_t aa_perm_compose(const permutation_t *a, const permutation_t *b) {
    /* (a . b)(x) = a(b(x)) */
    permutation_t result;
    memset(&result, 0, sizeof(result));
    result.n = a->n < b->n ? a->n : b->n;
    for (uint32_t i = 0; i < result.n; i++) {
        result.image[i] = a->image[b->image[i]];
    }
    return result;
}

permutation_t aa_perm_inverse(const permutation_t *p) {
    permutation_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.n = p->n;
    for (uint32_t i = 0; i < p->n; i++) {
        inv.image[p->image[i]] = i;
    }
    return inv;
}

cycle_decomposition_t aa_perm_cycle_decomposition(const permutation_t *p) {
    cycle_decomposition_t dec;
    memset(&dec, 0, sizeof(dec));
    bool visited[AA_MAX_PERM_SIZE] = {false};

    for (uint32_t i = 0; i < p->n; i++) {
        if (visited[i]) continue;
        cycle_t *c = &dec.cycles[dec.num_cycles];
        uint32_t j = i;
        c->length = 0;
        do {
            visited[j] = true;
            c->cycle[c->length++] = j;
            j = p->image[j];
        } while (j != i && c->length < AA_MAX_PERM_SIZE);
        dec.num_cycles++;
        if (dec.num_cycles >= AA_MAX_CYCLES) break;
    }
    return dec;
}

bool aa_perm_is_even(const permutation_t *p) {
    cycle_decomposition_t dec = aa_perm_cycle_decomposition(p);
    uint32_t transpositions = 0;
    for (uint32_t i = 0; i < dec.num_cycles; i++) {
        transpositions += (dec.cycles[i].length - 1);
    }
    return (transpositions % 2 == 0);
}

uint32_t aa_perm_order(const permutation_t *p) {
    cycle_decomposition_t dec = aa_perm_cycle_decomposition(p);
    uint32_t order = 1;
    for (uint32_t i = 0; i < dec.num_cycles; i++) {
        uint32_t len = dec.cycles[i].length;
        /* order = lcm of all cycle lengths */
        uint32_t g = gcd_u32(order, len);
        order = (order / g) * len;
    }
    return order;
}

/* =========================================================================
 * Polynomials over GF(p)
 * ========================================================================= */

static void poly_normalize(gfp_poly_t *f) {
    while (f->degree > 0 && f->coeffs[f->degree] == 0) f->degree--;
}

gfp_poly_t aa_poly_create(uint64_t p, uint32_t degree, const int64_t *coeffs) {
    gfp_poly_t f;
    memset(&f, 0, sizeof(f));
    f.p = p;
    f.degree = (degree > AA_MAX_POLY_DEGREE) ? AA_MAX_POLY_DEGREE : degree;
    if (coeffs) {
        for (uint32_t i = 0; i <= f.degree; i++) {
            f.coeffs[i] = mod_pos(coeffs[i], p);
        }
    }
    poly_normalize(&f);
    return f;
}

gfp_poly_t aa_poly_add(const gfp_poly_t *a, const gfp_poly_t *b) {
    gfp_poly_t r;
    memset(&r, 0, sizeof(r));
    r.p = a->p;
    r.degree = (a->degree > b->degree) ? a->degree : b->degree;
    for (uint32_t i = 0; i <= r.degree; i++) {
        int64_t ca = (i <= a->degree) ? a->coeffs[i] : 0;
        int64_t cb = (i <= b->degree) ? b->coeffs[i] : 0;
        r.coeffs[i] = mod_pos(ca + cb, r.p);
    }
    poly_normalize(&r);
    return r;
}

gfp_poly_t aa_poly_mul(const gfp_poly_t *a, const gfp_poly_t *b) {
    gfp_poly_t r;
    memset(&r, 0, sizeof(r));
    r.p = a->p;
    uint32_t deg = a->degree + b->degree;
    if (deg > AA_MAX_POLY_DEGREE) deg = AA_MAX_POLY_DEGREE;
    r.degree = deg;
    for (uint32_t i = 0; i <= a->degree; i++) {
        for (uint32_t j = 0; j <= b->degree; j++) {
            if (i + j > AA_MAX_POLY_DEGREE) continue;
            r.coeffs[i + j] = mod_pos(r.coeffs[i + j] +
                                       (a->coeffs[i] * b->coeffs[j]), r.p);
        }
    }
    poly_normalize(&r);
    return r;
}

int64_t aa_poly_eval(const gfp_poly_t *f, int64_t x) {
    /* Horner's method */
    int64_t result = 0;
    int64_t xm = mod_pos(x, f->p);
    for (int32_t i = (int32_t)f->degree; i >= 0; i--) {
        result = mod_pos(result * xm + f->coeffs[i], f->p);
    }
    return result;
}

gfp_poly_t aa_poly_gcd(const gfp_poly_t *a, const gfp_poly_t *b) {
    gfp_poly_t r0 = *a;
    gfp_poly_t r1 = *b;

    while (r1.degree > 0 || r1.coeffs[0] != 0) {
        /* Polynomial division: r0 = q * r1 + rem */
        gfp_poly_t rem = r0;
        uint64_t p = r0.p;
        uint64_t inv_lead = mod_pow_gf((uint64_t)r1.coeffs[r1.degree], p - 2, p);

        while (rem.degree >= r1.degree && (rem.degree > 0 || rem.coeffs[0] != 0)) {
            if (rem.coeffs[rem.degree] == 0) {
                if (rem.degree == 0) break;
                rem.degree--;
                continue;
            }
            uint32_t shift = rem.degree - r1.degree;
            int64_t coeff = mod_pos((int64_t)(rem.coeffs[rem.degree] * inv_lead), p);
            for (uint32_t i = 0; i <= r1.degree; i++) {
                rem.coeffs[i + shift] = mod_pos(
                    rem.coeffs[i + shift] - coeff * r1.coeffs[i], p);
            }
            poly_normalize(&rem);
            if (rem.degree >= r0.degree && r0.degree > 0) break; /* safety */
        }
        r0 = r1;
        r1 = rem;
    }

    /* Make monic */
    if (r0.coeffs[r0.degree] != 0 && r0.coeffs[r0.degree] != 1) {
        uint64_t inv = mod_pow_gf((uint64_t)r0.coeffs[r0.degree], r0.p - 2, r0.p);
        for (uint32_t i = 0; i <= r0.degree; i++) {
            r0.coeffs[i] = mod_pos(r0.coeffs[i] * (int64_t)inv, r0.p);
        }
    }
    return r0;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit for abstract-algebra results
 * (group/ring/field proofs, polynomial factorizations, etc.).
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
void abstract_algebra_wave14_kg_emit(
    struct brain_struct* brain,
    const char* theorem_label,
    float confidence)
{
    if (!brain) return;
    wave14_math_emit_proof(brain, "algebra", theorem_label, confidence);
}
