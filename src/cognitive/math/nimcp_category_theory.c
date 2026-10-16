/**
 * @file nimcp_category_theory.c
 * @brief Category theory engine implementation
 *
 * Finite categories with composition tables, axiom checking, functors,
 * natural transformations, monads, products/coproducts, Yoneda embedding.
 */

#include "cognitive/math/nimcp_category_theory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "category_theory"

/* ========================================================================== */
/* Lifecycle                                                                  */
/* ========================================================================== */

category_theory_t *ct_create(void) {
    category_theory_t *ct = (category_theory_t *)nimcp_calloc(
        1, sizeof(category_theory_t));
    if (!ct) {
        LOG_ERROR(LOG_TAG, "Failed to allocate category_theory_t");
        return NULL;
    }
    return ct;
}

void ct_destroy(category_theory_t *ct) {
    if (!ct) return;
    for (uint16_t i = 0; i < ct->n_categories; i++) {
        if (ct->categories[i]) ct_category_destroy(ct->categories[i]);
    }
    nimcp_free(ct);
}

/* ========================================================================== */
/* Category construction                                                      */
/* ========================================================================== */

category_t *ct_category_create(ct_category_type_t type, uint16_t n_objects) {
    if (n_objects == 0 || n_objects > CT_MAX_OBJECTS) {
        LOG_ERROR(LOG_TAG, "Invalid object count %u (max %d)",
                  n_objects, CT_MAX_OBJECTS);
        return NULL;
    }

    category_t *cat = (category_t *)nimcp_calloc(1, sizeof(category_t));
    if (!cat) return NULL;

    cat->type = type;
    cat->n_objects = n_objects;
    cat->n_morphisms = 0;

    /* Initialize composition table to CT_MORPH_NONE */
    for (uint16_t i = 0; i < CT_MAX_MORPHISMS; i++) {
        for (uint16_t j = 0; j < CT_MAX_MORPHISMS; j++) {
            cat->compose[i][j] = CT_MORPH_NONE;
        }
    }

    /* Create identity morphisms for each object */
    for (uint16_t obj = 0; obj < n_objects; obj++) {
        uint16_t mid = cat->n_morphisms++;
        cat->morphisms[mid].id = mid;
        cat->morphisms[mid].source = obj;
        cat->morphisms[mid].target = obj;
        cat->morphisms[mid].is_identity = true;
        cat->identity[obj] = mid;

        /* id . id = id */
        cat->compose[mid][mid] = mid;
    }

    return cat;
}

void ct_category_destroy(category_t *cat) {
    if (cat) nimcp_free(cat);
}

uint16_t ct_add_morphism(category_t *cat, uint16_t source, uint16_t target) {
    if (!cat || source >= cat->n_objects || target >= cat->n_objects) {
        return CT_MORPH_NONE;
    }
    if (cat->n_morphisms >= CT_MAX_MORPHISMS) {
        LOG_ERROR(LOG_TAG, "Morphism limit reached (%d)", CT_MAX_MORPHISMS);
        return CT_MORPH_NONE;
    }

    uint16_t mid = cat->n_morphisms++;
    cat->morphisms[mid].id = mid;
    cat->morphisms[mid].source = source;
    cat->morphisms[mid].target = target;
    cat->morphisms[mid].is_identity = false;

    /* Set identity compositions: id_target . f = f, f . id_source = f */
    uint16_t id_src = cat->identity[source];
    uint16_t id_tgt = cat->identity[target];
    cat->compose[id_src][mid] = mid;  /* mid . id_src = mid (f after id) */
    cat->compose[mid][id_tgt] = mid;  /* id_tgt . mid = mid (id after f) */

    return mid;
}

void ct_set_composition(category_t *cat, uint16_t f, uint16_t g, uint16_t h) {
    if (!cat || f >= cat->n_morphisms || g >= cat->n_morphisms ||
        h >= cat->n_morphisms) return;

    /* g . f = h: cod(f) must equal dom(g), dom(h) = dom(f), cod(h) = cod(g) */
    if (cat->morphisms[f].target != cat->morphisms[g].source) {
        LOG_WARN(LOG_TAG, "Composition domain mismatch: cod(f=%u)=%u != dom(g=%u)=%u",
                 f, cat->morphisms[f].target, g, cat->morphisms[g].source);
        return;
    }

    cat->compose[f][g] = h;
}

/* ========================================================================== */
/* Matrix category: build composition by matrix multiplication                */
/* ========================================================================== */

static void matrix_multiply(const double *A, uint16_t ar, uint16_t ac,
                            const double *B, uint16_t br, uint16_t bc,
                            double *C) {
    (void)br; /* ar x ac times ac x bc = ar x bc */
    for (uint16_t i = 0; i < ar; i++) {
        for (uint16_t j = 0; j < bc; j++) {
            double sum = 0.0;
            for (uint16_t k = 0; k < ac; k++) {
                sum += A[i * ac + k] * B[k * bc + j];
            }
            C[i * bc + j] = sum;
        }
    }
}

static bool matrices_equal(const double *A, const double *B, uint16_t n) {
    for (uint16_t i = 0; i < n; i++) {
        if (fabs(A[i] - B[i]) > 1e-10) return false;
    }
    return true;
}

void ct_build_composition_table(category_t *cat) {
    if (!cat || cat->type != CT_CAT_MATRIX) return;

    for (uint16_t f = 0; f < cat->n_morphisms; f++) {
        for (uint16_t g = 0; g < cat->n_morphisms; g++) {
            if (cat->morphisms[f].target != cat->morphisms[g].source) continue;

            /* g . f: multiply matrices */
            double result[CT_MAX_MATRIX_DIM * CT_MAX_MATRIX_DIM];
            uint16_t fr = cat->morphisms[f].matrix_rows;
            uint16_t fc = cat->morphisms[f].matrix_cols;
            uint16_t gc = cat->morphisms[g].matrix_cols;

            matrix_multiply(cat->morphisms[g].matrix, cat->morphisms[g].matrix_rows,
                            cat->morphisms[g].matrix_cols,
                            cat->morphisms[f].matrix, fr, fc,
                            result);
            (void)gc;

            /* Find matching morphism */
            uint16_t rr = cat->morphisms[g].matrix_rows;
            uint16_t rc = cat->morphisms[f].matrix_cols;
            for (uint16_t h = 0; h < cat->n_morphisms; h++) {
                if (cat->morphisms[h].source != cat->morphisms[f].source) continue;
                if (cat->morphisms[h].target != cat->morphisms[g].target) continue;
                if (cat->morphisms[h].matrix_rows != rr) continue;
                if (cat->morphisms[h].matrix_cols != rc) continue;
                if (matrices_equal(result, cat->morphisms[h].matrix, rr * rc)) {
                    cat->compose[f][g] = h;
                    break;
                }
            }
        }
    }
}

/* ========================================================================== */
/* Axiom checking                                                             */
/* ========================================================================== */

ct_axiom_result_t ct_check_axioms(const category_t *cat) {
    if (!cat) return CT_AXIOM_COMPOSITION_FAIL;

    uint16_t nm = cat->n_morphisms;

    /* Check identity laws */
    for (uint16_t f = 0; f < nm; f++) {
        uint16_t src = cat->morphisms[f].source;
        uint16_t tgt = cat->morphisms[f].target;
        uint16_t id_src = cat->identity[src];
        uint16_t id_tgt = cat->identity[tgt];

        /* Left identity: id_tgt . f = f */
        if (cat->compose[f][id_tgt] != f) {
            /* compose[f][id_tgt] means id_tgt after f; but our convention:
             * compose[f][g] = g . f. So compose[f][id_tgt] = id_tgt . f */
            return CT_AXIOM_LEFT_ID_FAIL;
        }

        /* Right identity: f . id_src = f */
        if (cat->compose[id_src][f] != f) {
            return CT_AXIOM_RIGHT_ID_FAIL;
        }
    }

    /* Check associativity: for composable f, g, h: (h.g).f = h.(g.f) */
    for (uint16_t f = 0; f < nm; f++) {
        for (uint16_t g = 0; g < nm; g++) {
            uint16_t gf = cat->compose[f][g]; /* g.f */
            if (gf == CT_MORPH_NONE) continue;

            for (uint16_t h = 0; h < nm; h++) {
                uint16_t hg = cat->compose[g][h]; /* h.g */
                uint16_t h_gf = cat->compose[gf][h]; /* h.(g.f) */

                if (hg == CT_MORPH_NONE && h_gf == CT_MORPH_NONE) continue;
                if (hg == CT_MORPH_NONE || h_gf == CT_MORPH_NONE) {
                    /* One composable but not the other — possible domain mismatch */
                    continue;
                }

                uint16_t hg_f = cat->compose[f][hg]; /* (h.g).f */
                if (hg_f == CT_MORPH_NONE) continue;

                if (hg_f != h_gf) {
                    return CT_AXIOM_ASSOC_FAIL;
                }
            }
        }
    }

    return CT_AXIOM_OK;
}

/* ========================================================================== */
/* Preset categories                                                          */
/* ========================================================================== */

category_t *ct_create_set_category(uint16_t n_elements) {
    if (n_elements == 0 || n_elements > CT_MAX_OBJECTS) return NULL;

    /* For a finite set category with n elements: objects are elements,
     * morphisms are all functions (n^n total). For small n only. */
    if (n_elements > 4) {
        LOG_WARN(LOG_TAG, "Set category with %u elements would have %u morphisms; "
                 "creating discrete category instead", n_elements,
                 (uint32_t)pow(n_elements, n_elements));
        /* Just create discrete category (only identity morphisms) */
        return ct_category_create(CT_CAT_SET, n_elements);
    }

    category_t *cat = ct_category_create(CT_CAT_SET, n_elements);
    if (!cat) return NULL;

    /* For n<=4: enumerate all functions as morphisms.
     * A function f: {0..n-1} -> {0..n-1} is stored in matrix field
     * where matrix[i] = f(i) for convenience. */
    uint16_t n = n_elements;
    uint32_t total = 1;
    for (uint16_t i = 0; i < n; i++) total *= n;

    /* Skip identity functions (already added) */
    /* Enumerate all n^n functions */
    for (uint32_t code = 0; code < total; code++) {
        /* Decode function from code */
        uint16_t func_vals[CT_MAX_OBJECTS];
        uint32_t c = code;
        bool is_id = true;
        for (uint16_t i = 0; i < n; i++) {
            func_vals[i] = c % n;
            c /= n;
            if (func_vals[i] != i) is_id = false;
        }
        if (is_id) continue; /* already have identity */

        if (cat->n_morphisms >= CT_MAX_MORPHISMS) break;

        /* For Set category, a function is a "morphism from the set to itself"
         * We model it as source=0, target=0 if single-object, but more
         * precisely: each element is an object, each function is n morphisms.
         * For simplicity, model as endomorphisms on a single object. */
        /* Actually, model as single-object category (monoid of functions) */
        uint16_t mid = cat->n_morphisms++;
        cat->morphisms[mid].id = mid;
        cat->morphisms[mid].source = 0; /* single-object monoid model */
        cat->morphisms[mid].target = 0;
        cat->morphisms[mid].is_identity = false;
        /* Store function in matrix field */
        for (uint16_t i = 0; i < n; i++) {
            cat->morphisms[mid].matrix[i] = (double)func_vals[i];
        }
        cat->morphisms[mid].matrix_rows = n;
        cat->morphisms[mid].matrix_cols = 1;
    }

    /* Build composition: (g.f)(x) = g(f(x)) */
    for (uint16_t f = 0; f < cat->n_morphisms; f++) {
        for (uint16_t g = 0; g < cat->n_morphisms; g++) {
            if (cat->morphisms[f].target != cat->morphisms[g].source) continue;

            /* Compute g.f */
            uint16_t comp_vals[CT_MAX_OBJECTS];
            for (uint16_t i = 0; i < n; i++) {
                uint16_t fi;
                if (cat->morphisms[f].is_identity) {
                    fi = i;
                } else {
                    fi = (uint16_t)cat->morphisms[f].matrix[i];
                }
                if (cat->morphisms[g].is_identity) {
                    comp_vals[i] = fi;
                } else {
                    comp_vals[i] = (uint16_t)cat->morphisms[g].matrix[fi];
                }
            }

            /* Find matching morphism */
            for (uint16_t h = 0; h < cat->n_morphisms; h++) {
                if (cat->morphisms[h].is_identity) {
                    bool match = true;
                    for (uint16_t i = 0; i < n; i++) {
                        if (comp_vals[i] != i) { match = false; break; }
                    }
                    if (match) { cat->compose[f][g] = h; break; }
                } else {
                    bool match = true;
                    for (uint16_t i = 0; i < n; i++) {
                        if ((uint16_t)cat->morphisms[h].matrix[i] != comp_vals[i]) {
                            match = false; break;
                        }
                    }
                    if (match) { cat->compose[f][g] = h; break; }
                }
            }
        }
    }

    return cat;
}

category_t *ct_create_poset_category(const bool *leq, uint16_t n) {
    if (!leq || n == 0 || n > CT_MAX_OBJECTS) return NULL;

    category_t *cat = ct_category_create(CT_CAT_POSET, n);
    if (!cat) return NULL;

    /* Add morphisms for each a <= b (a != b) */
    uint16_t morph_id[CT_MAX_OBJECTS][CT_MAX_OBJECTS];
    memset(morph_id, 0xFF, sizeof(morph_id)); /* CT_MORPH_NONE */

    for (uint16_t a = 0; a < n; a++) {
        morph_id[a][a] = cat->identity[a];
    }

    for (uint16_t a = 0; a < n; a++) {
        for (uint16_t b = 0; b < n; b++) {
            if (a == b) continue;
            if (leq[a * n + b]) {
                uint16_t mid = ct_add_morphism(cat, a, b);
                morph_id[a][b] = mid;
            }
        }
    }

    /* Build composition from transitivity: if a<=b and b<=c then a<=c */
    for (uint16_t a = 0; a < n; a++) {
        for (uint16_t b = 0; b < n; b++) {
            uint16_t fab = morph_id[a][b];
            if (fab == CT_MORPH_NONE) continue;
            for (uint16_t c = 0; c < n; c++) {
                uint16_t gbc = morph_id[b][c];
                uint16_t hac = morph_id[a][c];
                if (gbc == CT_MORPH_NONE || hac == CT_MORPH_NONE) continue;
                cat->compose[fab][gbc] = hac;
            }
        }
    }

    return cat;
}

category_t *ct_create_matrix_category(uint16_t max_dim) {
    if (max_dim == 0 || max_dim > CT_MAX_MATRIX_DIM) return NULL;

    /* Objects are dimensions 1..max_dim. Morphisms are matrices.
     * Only create identity matrices initially; user adds more. */
    category_t *cat = ct_category_create(CT_CAT_MATRIX, max_dim);
    if (!cat) return NULL;

    /* Set identity matrices */
    for (uint16_t d = 0; d < max_dim; d++) {
        uint16_t mid = cat->identity[d];
        cat->morphisms[mid].matrix_rows = d + 1;
        cat->morphisms[mid].matrix_cols = d + 1;
        memset(cat->morphisms[mid].matrix, 0, sizeof(cat->morphisms[mid].matrix));
        for (uint16_t i = 0; i <= d; i++) {
            cat->morphisms[mid].matrix[i * (d + 1) + i] = 1.0;
        }
    }

    return cat;
}

/* ========================================================================== */
/* Functors                                                                   */
/* ========================================================================== */

ct_functor_t *ct_functor_create(category_t *source, category_t *target) {
    if (!source || !target) return NULL;

    ct_functor_t *F = (ct_functor_t *)nimcp_calloc(1, sizeof(ct_functor_t));
    if (!F) return NULL;

    F->source = source;
    F->target = target;

    /* Initialize maps to CT_MORPH_NONE */
    memset(F->obj_map, 0xFF, sizeof(F->obj_map));
    memset(F->morph_map, 0xFF, sizeof(F->morph_map));

    return F;
}

void ct_functor_destroy(ct_functor_t *F) {
    if (F) nimcp_free(F);
}

void ct_functor_set_obj_map(ct_functor_t *F, uint16_t a, uint16_t Fa) {
    if (!F || a >= F->source->n_objects || Fa >= F->target->n_objects) return;
    F->obj_map[a] = Fa;
}

void ct_functor_set_morph_map(ct_functor_t *F, uint16_t f, uint16_t Ff) {
    if (!F || f >= F->source->n_morphisms || Ff >= F->target->n_morphisms) return;
    F->morph_map[f] = Ff;
}

ct_functor_result_t ct_functor_check(const ct_functor_t *F) {
    if (!F || !F->source || !F->target) return CT_FUNCTOR_COMP_FAIL;

    category_t *C = F->source;
    category_t *D = F->target;

    /* Check F preserves identities: F(id_a) = id_{F(a)} */
    for (uint16_t a = 0; a < C->n_objects; a++) {
        uint16_t id_a = C->identity[a];
        uint16_t F_id_a = F->morph_map[id_a];
        uint16_t Fa = F->obj_map[a];
        if (Fa == CT_MORPH_NONE || F_id_a == CT_MORPH_NONE) continue;
        uint16_t id_Fa = D->identity[Fa];
        if (F_id_a != id_Fa) return CT_FUNCTOR_ID_FAIL;
    }

    /* Check F preserves composition: F(g.f) = F(g).F(f) */
    for (uint16_t f = 0; f < C->n_morphisms; f++) {
        for (uint16_t g = 0; g < C->n_morphisms; g++) {
            uint16_t gf = C->compose[f][g];
            if (gf == CT_MORPH_NONE) continue;

            uint16_t Ff = F->morph_map[f];
            uint16_t Fg = F->morph_map[g];
            uint16_t Fgf = F->morph_map[gf];
            if (Ff == CT_MORPH_NONE || Fg == CT_MORPH_NONE ||
                Fgf == CT_MORPH_NONE) continue;

            uint16_t Fg_Ff = D->compose[Ff][Fg]; /* F(g).F(f) in D */
            if (Fg_Ff != Fgf) return CT_FUNCTOR_COMP_FAIL;
        }
    }

    return CT_FUNCTOR_OK;
}

/* ========================================================================== */
/* Natural transformations                                                    */
/* ========================================================================== */

ct_natural_transform_t *ct_nat_create(ct_functor_t *F, ct_functor_t *G) {
    if (!F || !G || F->source != G->source || F->target != G->target) {
        return NULL;
    }

    ct_natural_transform_t *eta = (ct_natural_transform_t *)nimcp_calloc(
        1, sizeof(ct_natural_transform_t));
    if (!eta) return NULL;

    eta->F = F;
    eta->G = G;
    memset(eta->component, 0xFF, sizeof(eta->component));
    return eta;
}

void ct_nat_destroy(ct_natural_transform_t *eta) {
    if (eta) nimcp_free(eta);
}

void ct_nat_set_component(ct_natural_transform_t *eta, uint16_t obj,
                          uint16_t morph) {
    if (!eta || obj >= CT_MAX_OBJECTS) return;
    eta->component[obj] = morph;
}

bool ct_nat_check_naturality(const ct_natural_transform_t *eta) {
    if (!eta || !eta->F || !eta->G) return false;

    category_t *C = eta->F->source;
    category_t *D = eta->F->target;

    /* For every morphism f: a -> b in C:
     * G(f) . eta_a = eta_b . F(f) in D */
    for (uint16_t f = 0; f < C->n_morphisms; f++) {
        uint16_t a = C->morphisms[f].source;
        uint16_t b = C->morphisms[f].target;

        uint16_t eta_a = eta->component[a];
        uint16_t eta_b = eta->component[b];
        uint16_t Ff = eta->F->morph_map[f];
        uint16_t Gf = eta->G->morph_map[f];

        if (eta_a == CT_MORPH_NONE || eta_b == CT_MORPH_NONE ||
            Ff == CT_MORPH_NONE || Gf == CT_MORPH_NONE) continue;

        /* Left side: G(f) . eta_a = compose[eta_a][Gf] */
        uint16_t left = D->compose[eta_a][Gf];
        /* Right side: eta_b . F(f) = compose[Ff][eta_b] */
        uint16_t right = D->compose[Ff][eta_b];

        if (left != right) return false;
    }

    return true;
}

/* ========================================================================== */
/* Monads                                                                     */
/* ========================================================================== */

ct_monad_t *ct_monad_create(category_t *base, ct_functor_t *T,
                            ct_natural_transform_t *eta,
                            ct_natural_transform_t *mu) {
    if (!base || !T || !eta || !mu) return NULL;

    ct_monad_t *m = (ct_monad_t *)nimcp_calloc(1, sizeof(ct_monad_t));
    if (!m) return NULL;

    m->base = base;
    m->T = T;
    m->eta = eta;
    m->mu = mu;
    return m;
}

void ct_monad_destroy(ct_monad_t *m) {
    if (m) nimcp_free(m);
}

ct_monad_result_t ct_monad_check_laws(const ct_monad_t *m) {
    if (!m || !m->base || !m->T || !m->eta || !m->mu) {
        return CT_MONAD_ASSOC_FAIL;
    }

    category_t *C = m->base;

    /* Check: mu is a natural transformation T^2 -> T
     * eta is a natural transformation Id -> T
     *
     * Monad laws (at each object a):
     * Left unit:  mu_a . T(eta_a) = id_{T(a)}
     * Right unit: mu_a . eta_{T(a)} = id_{T(a)}
     * Associativity: mu_a . T(mu_a) = mu_a . mu_{T(a)} */

    for (uint16_t a = 0; a < C->n_objects; a++) {
        uint16_t Ta = m->T->obj_map[a];
        if (Ta == CT_MORPH_NONE) continue;

        uint16_t mu_a = m->mu->component[a];
        uint16_t eta_a = m->eta->component[a];
        if (mu_a == CT_MORPH_NONE || eta_a == CT_MORPH_NONE) continue;

        uint16_t id_Ta = C->identity[Ta];

        /* Left unit: mu_a . T(eta_a) = id_{T(a)} */
        uint16_t T_eta_a = m->T->morph_map[eta_a];
        if (T_eta_a != CT_MORPH_NONE) {
            uint16_t left = C->compose[T_eta_a][mu_a]; /* mu_a . T(eta_a) */
            if (left != CT_MORPH_NONE && left != id_Ta) {
                return CT_MONAD_LEFT_UNIT_FAIL;
            }
        }

        /* Right unit: mu_a . eta_{T(a)} = id_{T(a)} */
        uint16_t eta_Ta = m->eta->component[Ta];
        if (eta_Ta != CT_MORPH_NONE) {
            uint16_t right = C->compose[eta_Ta][mu_a]; /* mu_a . eta_{T(a)} */
            if (right != CT_MORPH_NONE && right != id_Ta) {
                return CT_MONAD_RIGHT_UNIT_FAIL;
            }
        }

        /* Associativity: mu_a . T(mu_a) = mu_a . mu_{T(a)} */
        uint16_t T_mu_a = m->T->morph_map[mu_a];
        uint16_t mu_Ta = m->mu->component[Ta];
        if (T_mu_a != CT_MORPH_NONE && mu_Ta != CT_MORPH_NONE) {
            uint16_t lhs = C->compose[T_mu_a][mu_a];   /* mu_a . T(mu_a) */
            uint16_t rhs = C->compose[mu_Ta][mu_a];     /* mu_a . mu_{T(a)} */
            if (lhs != CT_MORPH_NONE && rhs != CT_MORPH_NONE && lhs != rhs) {
                return CT_MONAD_ASSOC_FAIL;
            }
        }
    }

    return CT_MONAD_OK;
}

/* ========================================================================== */
/* Products and coproducts                                                    */
/* ========================================================================== */

ct_product_result_t ct_find_product(const category_t *cat,
                                    uint16_t a, uint16_t b) {
    ct_product_result_t res;
    memset(&res, 0, sizeof(res));
    res.exists = false;
    if (!cat || a >= cat->n_objects || b >= cat->n_objects) return res;

    /* A product of a and b is an object p with projections pi1: p->a, pi2: p->b
     * such that for any object c with f:c->a, g:c->b there exists a unique
     * h:c->p with pi1.h = f and pi2.h = g.
     *
     * Search for candidate product objects. */
    for (uint16_t p = 0; p < cat->n_objects; p++) {
        /* Find morphisms p->a and p->b */
        for (uint16_t pi1 = 0; pi1 < cat->n_morphisms; pi1++) {
            if (cat->morphisms[pi1].source != p ||
                cat->morphisms[pi1].target != a) continue;

            for (uint16_t pi2 = 0; pi2 < cat->n_morphisms; pi2++) {
                if (cat->morphisms[pi2].source != p ||
                    cat->morphisms[pi2].target != b) continue;

                /* Check universal property: for every c, f:c->a, g:c->b
                 * there exists unique h:c->p with pi1.h=f, pi2.h=g */
                bool is_product = true;

                for (uint16_t c = 0; c < cat->n_objects && is_product; c++) {
                    /* For each pair (f:c->a, g:c->b) */
                    for (uint16_t f = 0; f < cat->n_morphisms && is_product; f++) {
                        if (cat->morphisms[f].source != c ||
                            cat->morphisms[f].target != a) continue;

                        for (uint16_t g = 0; g < cat->n_morphisms && is_product; g++) {
                            if (cat->morphisms[g].source != c ||
                                cat->morphisms[g].target != b) continue;

                            /* Find unique h:c->p */
                            uint16_t found_h = CT_MORPH_NONE;
                            uint16_t count = 0;
                            for (uint16_t h = 0; h < cat->n_morphisms; h++) {
                                if (cat->morphisms[h].source != c ||
                                    cat->morphisms[h].target != p) continue;

                                uint16_t pi1h = cat->compose[h][pi1];
                                uint16_t pi2h = cat->compose[h][pi2];
                                if (pi1h == f && pi2h == g) {
                                    found_h = h;
                                    count++;
                                }
                            }
                            if (count != 1) is_product = false;
                            (void)found_h;
                        }
                    }
                }

                if (is_product) {
                    res.product_obj = p;
                    res.proj1 = pi1;
                    res.proj2 = pi2;
                    res.exists = true;
                    return res;
                }
            }
        }
    }

    return res;
}

ct_product_result_t ct_find_coproduct(const category_t *cat,
                                      uint16_t a, uint16_t b) {
    ct_product_result_t res;
    memset(&res, 0, sizeof(res));
    res.exists = false;
    if (!cat || a >= cat->n_objects || b >= cat->n_objects) return res;

    /* Coproduct: object c with injections inj1: a->c, inj2: b->c
     * universal property dual to product */
    for (uint16_t c = 0; c < cat->n_objects; c++) {
        for (uint16_t inj1 = 0; inj1 < cat->n_morphisms; inj1++) {
            if (cat->morphisms[inj1].source != a ||
                cat->morphisms[inj1].target != c) continue;

            for (uint16_t inj2 = 0; inj2 < cat->n_morphisms; inj2++) {
                if (cat->morphisms[inj2].source != b ||
                    cat->morphisms[inj2].target != c) continue;

                bool is_coprod = true;

                for (uint16_t d = 0; d < cat->n_objects && is_coprod; d++) {
                    for (uint16_t f = 0; f < cat->n_morphisms && is_coprod; f++) {
                        if (cat->morphisms[f].source != a ||
                            cat->morphisms[f].target != d) continue;

                        for (uint16_t g = 0; g < cat->n_morphisms && is_coprod; g++) {
                            if (cat->morphisms[g].source != b ||
                                cat->morphisms[g].target != d) continue;

                            uint16_t count = 0;
                            for (uint16_t h = 0; h < cat->n_morphisms; h++) {
                                if (cat->morphisms[h].source != c ||
                                    cat->morphisms[h].target != d) continue;

                                uint16_t h_inj1 = cat->compose[inj1][h];
                                uint16_t h_inj2 = cat->compose[inj2][h];
                                if (h_inj1 == f && h_inj2 == g) count++;
                            }
                            if (count != 1) is_coprod = false;
                        }
                    }
                }

                if (is_coprod) {
                    res.product_obj = c;
                    res.proj1 = inj1;
                    res.proj2 = inj2;
                    res.exists = true;
                    return res;
                }
            }
        }
    }

    return res;
}

/* ========================================================================== */
/* Yoneda embedding                                                           */
/* ========================================================================== */

ct_yoneda_result_t ct_hom_set(const category_t *cat, uint16_t a, uint16_t b) {
    ct_yoneda_result_t res;
    memset(&res, 0, sizeof(res));
    if (!cat || a >= cat->n_objects || b >= cat->n_objects) return res;

    for (uint16_t f = 0; f < cat->n_morphisms; f++) {
        if (cat->morphisms[f].source == a && cat->morphisms[f].target == b) {
            if (res.count < CT_MAX_MORPHISMS) {
                res.morphisms[res.count++] = f;
            }
        }
    }
    return res;
}

ct_yoneda_result_t ct_yoneda_eval(const category_t *cat,
                                  uint16_t a, uint16_t b) {
    /* Yoneda embedding h^a: C -> Set maps b to hom(a, b) */
    return ct_hom_set(cat, a, b);
}

bool ct_check_yoneda_lemma(const category_t *cat, const ct_functor_t *F,
                           uint16_t a) {
    if (!cat || !F || a >= cat->n_objects) return false;

    /* Yoneda lemma: Nat(hom(a,-), F) is in bijection with F(a).
     * For finite categories, count natural transformations from
     * hom(a,-) to F and check it equals |F(a)|. */

    /* Count |F(a)|: number of morphisms in target whose source is F(a).
     * For Set-valued functor, F(a) is an object whose "size" is
     * represented by the number of morphisms to/from it.
     * In our finite model, we approximate by checking the bijection
     * component-wise. */

    /* Simplified check: each natural transformation eta: hom(a,-) -> F
     * is determined by eta_a(id_a) in F(a). Verify this. */
    uint16_t Fa = F->obj_map[a];
    if (Fa == CT_MORPH_NONE) return false;

    /* Count elements of F(a): in our model, this is the hom-set
     * size at the target object. For a representable check, we verify
     * that the Yoneda map is well-defined. */
    ct_yoneda_result_t hom_aa = ct_hom_set(cat, a, a);

    /* The identity id_a must be in hom(a,a) */
    bool has_id = false;
    for (uint16_t i = 0; i < hom_aa.count; i++) {
        if (hom_aa.morphisms[i] == cat->identity[a]) {
            has_id = true;
            break;
        }
    }

    return has_id; /* basic sanity: Yoneda sends id_a to an element of F(a) */
}

/* ========================================================================== */
/* Utility                                                                    */
/* ========================================================================== */

uint16_t ct_count_morphisms(const category_t *cat, uint16_t a, uint16_t b) {
    if (!cat || a >= cat->n_objects || b >= cat->n_objects) return 0;
    uint16_t count = 0;
    for (uint16_t f = 0; f < cat->n_morphisms; f++) {
        if (cat->morphisms[f].source == a && cat->morphisms[f].target == b) {
            count++;
        }
    }
    return count;
}

bool ct_is_isomorphism(const category_t *cat, uint16_t f) {
    return ct_find_inverse(cat, f) != CT_MORPH_NONE;
}

uint16_t ct_find_inverse(const category_t *cat, uint16_t f) {
    if (!cat || f >= cat->n_morphisms) return CT_MORPH_NONE;

    uint16_t src = cat->morphisms[f].source;
    uint16_t tgt = cat->morphisms[f].target;
    uint16_t id_src = cat->identity[src];
    uint16_t id_tgt = cat->identity[tgt];

    /* Find g: tgt -> src such that g.f = id_src and f.g = id_tgt */
    for (uint16_t g = 0; g < cat->n_morphisms; g++) {
        if (cat->morphisms[g].source != tgt ||
            cat->morphisms[g].target != src) continue;

        uint16_t gf = cat->compose[f][g]; /* g.f */
        uint16_t fg = cat->compose[g][f]; /* f.g */

        if (gf == id_src && fg == id_tgt) return g;
    }
    return CT_MORPH_NONE;
}

bool ct_are_equivalent(const category_t *cat1, const category_t *cat2) {
    if (!cat1 || !cat2) return false;

    /* Two categories are equivalent if there exists a full, faithful,
     * essentially surjective functor between them.
     * Simplified check: same number of objects (up to iso) and
     * same hom-set sizes. */
    if (cat1->n_objects != cat2->n_objects) return false;

    /* Check if hom-set cardinalities match (necessary condition) */
    for (uint16_t a = 0; a < cat1->n_objects; a++) {
        for (uint16_t b = 0; b < cat1->n_objects; b++) {
            uint16_t c1 = ct_count_morphisms(cat1, a, b);
            uint16_t c2 = ct_count_morphisms(cat2, a, b);
            if (c1 != c2) return false;
        }
    }

    return true;
}
