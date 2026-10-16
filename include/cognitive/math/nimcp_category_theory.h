/**
 * @file nimcp_category_theory.h
 * @brief Category theory engine for NIMCP cognitive mathematics
 *
 * Finite categories with composition tables, functors between categories,
 * natural transformations, monads (unit + multiplication + law checking),
 * products and coproducts, representable examples (Set, partial order,
 * matrix), Yoneda embedding. All finite/discrete: max 32 objects,
 * 256 morphisms per category.
 */

#ifndef NIMCP_CATEGORY_THEORY_H
#define NIMCP_CATEGORY_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define CT_MAX_OBJECTS          32      /* max objects per category           */
#define CT_MAX_MORPHISMS        256     /* max morphisms per category         */
#define CT_MORPH_NONE           UINT16_MAX /* sentinel: no morphism          */
#define CT_MAX_MATRIX_DIM       8       /* max dimension for matrix category */

/* --------------------------------------------------------------------------
 * Enums
 * -------------------------------------------------------------------------- */

typedef enum {
    CT_CAT_GENERIC = 0,
    CT_CAT_SET,             /* finite sets with functions               */
    CT_CAT_POSET,           /* partial order (at most one morph a->b)   */
    CT_CAT_MATRIX,          /* objects = natural numbers, morphs = mats */
    CT_CAT_MONOID,          /* single-object category                   */
    CT_CAT_TYPE_COUNT
} ct_category_type_t;

typedef enum {
    CT_AXIOM_OK = 0,
    CT_AXIOM_ASSOC_FAIL,       /* (f;g);h != f;(g;h)                    */
    CT_AXIOM_LEFT_ID_FAIL,     /* id;f != f                              */
    CT_AXIOM_RIGHT_ID_FAIL,    /* f;id != f                              */
    CT_AXIOM_COMPOSITION_FAIL, /* dom/cod mismatch in composition table  */
    CT_AXIOM_ERROR_COUNT
} ct_axiom_result_t;

typedef enum {
    CT_FUNCTOR_OK = 0,
    CT_FUNCTOR_COMP_FAIL,      /* F(g.f) != F(g).F(f)                   */
    CT_FUNCTOR_ID_FAIL,        /* F(id_a) != id_F(a)                    */
    CT_FUNCTOR_ERROR_COUNT
} ct_functor_result_t;

typedef enum {
    CT_MONAD_OK = 0,
    CT_MONAD_LEFT_UNIT_FAIL,   /* mu . T(eta) != id_T                   */
    CT_MONAD_RIGHT_UNIT_FAIL,  /* mu . eta_T != id_T                    */
    CT_MONAD_ASSOC_FAIL,       /* mu . T(mu) != mu . mu_T               */
    CT_MONAD_ERROR_COUNT
} ct_monad_result_t;

/* --------------------------------------------------------------------------
 * Structures
 * -------------------------------------------------------------------------- */

/** Morphism: f: source -> target */
typedef struct {
    uint16_t id;                /* morphism index in category             */
    uint16_t source;            /* source object index                    */
    uint16_t target;            /* target object index                    */
    bool     is_identity;       /* true if this is id_source              */

    /* for matrix category: matrix data (row-major) */
    double   matrix[CT_MAX_MATRIX_DIM * CT_MAX_MATRIX_DIM];
    uint16_t matrix_rows;
    uint16_t matrix_cols;
} ct_morphism_t;

/** Category: objects + morphisms + composition table */
typedef struct {
    ct_category_type_t type;
    uint16_t           n_objects;
    uint16_t           n_morphisms;

    /* identity morphism index for each object */
    uint16_t           identity[CT_MAX_OBJECTS];

    /* morphism storage */
    ct_morphism_t      morphisms[CT_MAX_MORPHISMS];

    /* composition table: compose[f][g] = g.f (f then g)
     * CT_MORPH_NONE if not composable (codomain mismatch) */
    uint16_t           compose[CT_MAX_MORPHISMS][CT_MAX_MORPHISMS];
} category_t;

/** Functor: F: source_cat -> target_cat */
typedef struct {
    category_t *source;
    category_t *target;

    /* F maps objects: obj_map[a] = F(a) */
    uint16_t    obj_map[CT_MAX_OBJECTS];

    /* F maps morphisms: morph_map[f] = F(f) */
    uint16_t    morph_map[CT_MAX_MORPHISMS];
} ct_functor_t;

/** Natural transformation: eta: F -> G */
typedef struct {
    ct_functor_t *F;
    ct_functor_t *G;

    /* component at each object: component[a] = eta_a : F(a) -> G(a) */
    uint16_t      component[CT_MAX_OBJECTS];
} ct_natural_transform_t;

/** Monad: (T, eta, mu) on a category */
typedef struct {
    category_t           *base;   /* base category C            */
    ct_functor_t         *T;      /* endofunctor T: C -> C      */
    ct_natural_transform_t *eta;  /* unit: Id -> T              */
    ct_natural_transform_t *mu;   /* multiplication: T^2 -> T   */
} ct_monad_t;

/** Product/coproduct result */
typedef struct {
    uint16_t product_obj;           /* the product/coproduct object */
    uint16_t proj1;                 /* projection / injection 1     */
    uint16_t proj2;                 /* projection / injection 2     */
    bool     exists;
} ct_product_result_t;

/** Yoneda result: representable functor hom(a, -) evaluated at b */
typedef struct {
    uint16_t morphisms[CT_MAX_MORPHISMS]; /* indices of morphisms a->b */
    uint16_t count;
} ct_yoneda_result_t;

/** Top-level category theory engine */
typedef struct {
    category_t *categories[16]; /* cache of created categories   */
    uint16_t    n_categories;
} category_theory_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

category_theory_t *ct_create(void);
void               ct_destroy(category_theory_t *ct);

/* --------------------------------------------------------------------------
 * Category construction
 * -------------------------------------------------------------------------- */

category_t *ct_category_create(ct_category_type_t type, uint16_t n_objects);
void        ct_category_destroy(category_t *cat);

/** Add a morphism f: source -> target. Returns morphism index. */
uint16_t ct_add_morphism(category_t *cat, uint16_t source, uint16_t target);

/** Set composition: compose(f, g) = h  (meaning g . f = h) */
void ct_set_composition(category_t *cat, uint16_t f, uint16_t g, uint16_t h);

/** Build composition table from morphism data (for matrix category) */
void ct_build_composition_table(category_t *cat);

/** Check all category axioms */
ct_axiom_result_t ct_check_axioms(const category_t *cat);

/* --------------------------------------------------------------------------
 * Preset categories
 * -------------------------------------------------------------------------- */

/** Create finite Set category with n elements */
category_t *ct_create_set_category(uint16_t n_elements);

/** Create partial order category from adjacency matrix (leq[i][j]) */
category_t *ct_create_poset_category(const bool *leq, uint16_t n);

/** Create matrix category (objects=dims, morphisms=matrices, compose=mult) */
category_t *ct_create_matrix_category(uint16_t max_dim);

/* --------------------------------------------------------------------------
 * Functors
 * -------------------------------------------------------------------------- */

ct_functor_t *ct_functor_create(category_t *source, category_t *target);
void          ct_functor_destroy(ct_functor_t *F);
void          ct_functor_set_obj_map(ct_functor_t *F, uint16_t a, uint16_t Fa);
void          ct_functor_set_morph_map(ct_functor_t *F, uint16_t f, uint16_t Ff);

/** Check functor axioms (preserves composition and identity) */
ct_functor_result_t ct_functor_check(const ct_functor_t *F);

/* --------------------------------------------------------------------------
 * Natural transformations
 * -------------------------------------------------------------------------- */

ct_natural_transform_t *ct_nat_create(ct_functor_t *F, ct_functor_t *G);
void                    ct_nat_destroy(ct_natural_transform_t *eta);
void ct_nat_set_component(ct_natural_transform_t *eta, uint16_t obj,
                          uint16_t morph);

/** Check naturality: for all f:a->b, G(f).eta_a == eta_b.F(f) */
bool ct_nat_check_naturality(const ct_natural_transform_t *eta);

/* --------------------------------------------------------------------------
 * Monads
 * -------------------------------------------------------------------------- */

ct_monad_t *ct_monad_create(category_t *base, ct_functor_t *T,
                            ct_natural_transform_t *eta,
                            ct_natural_transform_t *mu);
void        ct_monad_destroy(ct_monad_t *m);

/** Check monad laws: left unit, right unit, associativity */
ct_monad_result_t ct_monad_check_laws(const ct_monad_t *m);

/* --------------------------------------------------------------------------
 * Products and coproducts
 * -------------------------------------------------------------------------- */

/** Find product of objects a and b (if it exists) */
ct_product_result_t ct_find_product(const category_t *cat,
                                    uint16_t a, uint16_t b);

/** Find coproduct of objects a and b (if it exists) */
ct_product_result_t ct_find_coproduct(const category_t *cat,
                                      uint16_t a, uint16_t b);

/* --------------------------------------------------------------------------
 * Yoneda embedding
 * -------------------------------------------------------------------------- */

/** Compute hom(a, b) — the set of morphisms from a to b */
ct_yoneda_result_t ct_hom_set(const category_t *cat, uint16_t a, uint16_t b);

/** Yoneda embedding: representable functor hom(a, -) applied to b
 *  (same as hom_set, but conceptually the Yoneda functor) */
ct_yoneda_result_t ct_yoneda_eval(const category_t *cat,
                                  uint16_t a, uint16_t b);

/** Check Yoneda lemma: Nat(hom(a,-), F) ~ F(a) for finite case
 *  Returns true if the bijection holds */
bool ct_check_yoneda_lemma(const category_t *cat, const ct_functor_t *F,
                           uint16_t a);

/* --------------------------------------------------------------------------
 * Utility
 * -------------------------------------------------------------------------- */

/** Count morphisms from a to b */
uint16_t ct_count_morphisms(const category_t *cat, uint16_t a, uint16_t b);

/** Check if morphism f is an isomorphism (has two-sided inverse) */
bool ct_is_isomorphism(const category_t *cat, uint16_t f);

/** Find inverse of morphism f (returns CT_MORPH_NONE if not iso) */
uint16_t ct_find_inverse(const category_t *cat, uint16_t f);

/** Check if two categories are equivalent (bijection on objects + full+faithful functor) */
bool ct_are_equivalent(const category_t *cat1, const category_t *cat2);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CATEGORY_THEORY_H */
