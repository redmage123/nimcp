/**
 * @file test_geometry_modules.c
 * @brief Tests for differential geometry, Lorentz model, and Lie group modules
 *
 * Tests: Riemannian metric operations, Christoffel symbols, curvature,
 * parallel transport, geodesics, Lorentz/hyperboloid model, SO(3), matrix exp/log.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "utils/geometry/nimcp_differential_geometry.h"
#include "utils/geometry/nimcp_lorentz.h"
#include "utils/geometry/nimcp_lie_group.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define APPROX(a, b, eps) (fabsf((a) - (b)) < (eps))

/*=============================================================================
 * Differential Geometry Tests
 *===========================================================================*/

static void test_metric_create_destroy(void) {
    TEST("diffgeo: metric create/destroy");
    riemannian_metric_t* m = riemannian_metric_create(3);
    if (!m) { FAIL("create returned NULL"); return; }
    if (m->dim != 3) { FAIL("wrong dim"); riemannian_metric_destroy(m); return; }
    riemannian_metric_destroy(m);
    PASS();
}

static void test_metric_identity(void) {
    TEST("diffgeo: identity metric inner product");
    riemannian_metric_t* m = riemannian_metric_create(3);
    float u[] = {1.0f, 2.0f, 3.0f};
    float v[] = {4.0f, 5.0f, 6.0f};
    float ip = riemannian_inner_product(m, u, v);
    /* 1*4 + 2*5 + 3*6 = 32 */
    if (!APPROX(ip, 32.0f, 1e-4f)) { FAIL("wrong inner product"); riemannian_metric_destroy(m); return; }
    riemannian_metric_destroy(m);
    PASS();
}

static void test_metric_norm(void) {
    TEST("diffgeo: metric norm");
    riemannian_metric_t* m = riemannian_metric_create(2);
    float v[] = {3.0f, 4.0f};
    float n = riemannian_norm(m, v);
    if (!APPROX(n, 5.0f, 1e-4f)) { FAIL("wrong norm"); riemannian_metric_destroy(m); return; }
    riemannian_metric_destroy(m);
    PASS();
}

static void test_metric_set_and_inverse(void) {
    TEST("diffgeo: set metric and invert");
    riemannian_metric_t* m = riemannian_metric_create(2);
    /* Set to [[4,0],[0,9]] */
    float g_data[4] = {4.0f, 0.0f, 0.0f, 9.0f};
    riemannian_metric_set(m, g_data);
    diffgeo_error_t ret = riemannian_metric_invert(m);
    if (ret != DIFFGEO_OK) { FAIL("invert failed"); riemannian_metric_destroy(m); return; }
    /* Inverse should be [[0.25,0],[0,0.111]] */
    if (!APPROX(m->g_inv[0], 0.25f, 1e-4f)) { FAIL("wrong g_inv[0,0]"); riemannian_metric_destroy(m); return; }
    if (!APPROX(m->g_inv[3], 1.0f/9.0f, 1e-4f)) { FAIL("wrong g_inv[1,1]"); riemannian_metric_destroy(m); return; }
    riemannian_metric_destroy(m);
    PASS();
}

static void test_metric_raise_lower(void) {
    TEST("diffgeo: raise/lower index");
    riemannian_metric_t* m = riemannian_metric_create(2);
    riemannian_metric_invert(m);
    float v_lower[] = {3.0f, 7.0f};
    float v_upper[2];
    riemannian_raise_index(m, v_lower, v_upper);
    if (!APPROX(v_upper[0], 3.0f, 1e-4f) || !APPROX(v_upper[1], 7.0f, 1e-4f)) {
        FAIL("raise failed"); riemannian_metric_destroy(m); return;
    }
    float v_back[2];
    riemannian_lower_index(m, v_upper, v_back);
    if (!APPROX(v_back[0], 3.0f, 1e-4f) || !APPROX(v_back[1], 7.0f, 1e-4f)) {
        FAIL("lower failed"); riemannian_metric_destroy(m); return;
    }
    riemannian_metric_destroy(m);
    PASS();
}

static void test_metric_scaled_inner_product(void) {
    TEST("diffgeo: scaled metric inner product");
    riemannian_metric_t* m = riemannian_metric_create(2);
    /* Set to [[4,0],[0,1]] */
    float g_data[4] = {4.0f, 0.0f, 0.0f, 1.0f};
    riemannian_metric_set(m, g_data);
    float u[] = {1.0f, 1.0f};
    float v[] = {1.0f, 1.0f};
    float ip = riemannian_inner_product(m, u, v);
    /* 4*1*1 + 1*1*1 = 5 */
    if (!APPROX(ip, 5.0f, 1e-4f)) { FAIL("wrong scaled ip"); riemannian_metric_destroy(m); return; }
    riemannian_metric_destroy(m);
    PASS();
}

static void test_christoffel_create_destroy(void) {
    TEST("diffgeo: Christoffel create/destroy");
    christoffel_symbols_t* c = christoffel_create(3);
    if (!c) { FAIL("create returned NULL"); return; }
    if (c->dim != 3) { FAIL("wrong dim"); christoffel_destroy(c); return; }
    christoffel_destroy(c);
    PASS();
}

static void test_christoffel_flat_space(void) {
    TEST("diffgeo: Christoffel symbols in flat space");
    riemannian_metric_t* m = riemannian_metric_create(2);
    riemannian_metric_invert(m);
    christoffel_symbols_t* c = christoffel_create(2);
    /* Zero metric derivatives = flat space */
    float dg_dx[8]; /* 2*2*2 = 8 */
    memset(dg_dx, 0, sizeof(dg_dx));
    christoffel_compute(c, m, dg_dx);
    /* Flat metric: all Christoffel symbols should be 0 */
    float g000 = christoffel_get(c, 0, 0, 0);
    float g111 = christoffel_get(c, 1, 1, 1);
    if (!APPROX(g000, 0.0f, 1e-6f) || !APPROX(g111, 0.0f, 1e-6f)) {
        FAIL("nonzero Christoffel in flat space");
        christoffel_destroy(c); riemannian_metric_destroy(m); return;
    }
    christoffel_destroy(c);
    riemannian_metric_destroy(m);
    PASS();
}

static void test_curvature_create_destroy(void) {
    TEST("diffgeo: curvature create/destroy");
    curvature_data_t* curv = curvature_create(3);
    if (!curv) { FAIL("create returned NULL"); return; }
    curvature_destroy(curv);
    PASS();
}

static void test_curvature_flat_space(void) {
    TEST("diffgeo: Ricci curvature in flat space");
    uint32_t dim = 2;
    christoffel_symbols_t* chris = christoffel_create(dim);
    riemannian_metric_t* m = riemannian_metric_create(dim);
    riemannian_metric_invert(m);
    /* Zero metric derivatives */
    float dg_dx[8];
    memset(dg_dx, 0, sizeof(dg_dx));
    christoffel_compute(chris, m, dg_dx);

    curvature_data_t* curv = curvature_create(dim);
    /* Zero Christoffel derivatives */
    float dgamma_dx[16]; /* dim^4 = 16 */
    memset(dgamma_dx, 0, sizeof(dgamma_dx));
    curvature_compute_ricci(curv, chris, dgamma_dx);

    /* Flat space: Ricci tensor should be zero */
    int ok = 1;
    for (uint32_t i = 0; i < dim*dim; i++) {
        if (!APPROX(curv->ricci[i], 0.0f, 1e-5f)) { ok = 0; break; }
    }
    if (!ok) { FAIL("nonzero Ricci in flat space"); }
    else { PASS(); }

    curvature_destroy(curv);
    christoffel_destroy(chris);
    riemannian_metric_destroy(m);
}

static void test_curvature_scalar(void) {
    TEST("diffgeo: scalar curvature in flat space");
    uint32_t dim = 2;
    curvature_data_t* curv = curvature_create(dim);
    riemannian_metric_t* m = riemannian_metric_create(dim);
    riemannian_metric_invert(m);
    /* Ricci is already zero from create (calloc) */
    curvature_compute_scalar(curv, m);
    if (!APPROX(curv->scalar_curvature, 0.0f, 1e-5f)) {
        FAIL("nonzero scalar curvature in flat space");
    } else {
        PASS();
    }
    curvature_destroy(curv);
    riemannian_metric_destroy(m);
}

static void test_exp_map_flat(void) {
    TEST("diffgeo: exp map in flat space");
    uint32_t dim = 2;
    christoffel_symbols_t* chris = christoffel_create(dim);
    /* Zero Christoffel = flat space => exp_p(v) = p + v */
    float p[] = {1.0f, 2.0f};
    float v[] = {0.5f, -0.3f};
    float result[2];
    diffgeo_error_t ret = exp_map(chris, dim, p, v, result);
    if (ret != DIFFGEO_OK) { FAIL("exp_map failed"); christoffel_destroy(chris); return; }
    /* In flat space, exp = p + v */
    if (!APPROX(result[0], 1.5f, 0.1f) || !APPROX(result[1], 1.7f, 0.1f)) {
        FAIL("wrong exp_map result");
    } else {
        PASS();
    }
    christoffel_destroy(chris);
}

static void test_geodesic_shoot_flat(void) {
    TEST("diffgeo: geodesic shooting in flat space");
    uint32_t dim = 2;
    christoffel_symbols_t* chris = christoffel_create(dim);
    float x0[] = {0.0f, 0.0f};
    float v0[] = {1.0f, 0.5f};
    uint32_t num_steps = 100;
    float* trajectory = calloc((num_steps + 1) * dim, sizeof(float));
    diffgeo_error_t ret = geodesic_shoot(chris, dim, x0, v0, 1.0f / num_steps, num_steps, trajectory);
    if (ret != DIFFGEO_OK) { FAIL("shoot failed"); free(trajectory); christoffel_destroy(chris); return; }
    /* End point should be x0 + v0*t_total = (0,0) + (1, 0.5)*1.0 */
    float end_x = trajectory[num_steps * dim];
    float end_y = trajectory[num_steps * dim + 1];
    if (!APPROX(end_x, 1.0f, 0.15f) || !APPROX(end_y, 0.5f, 0.15f)) {
        FAIL("geodesic not straight");
    } else {
        PASS();
    }
    free(trajectory);
    christoffel_destroy(chris);
}

static void test_parallel_transport_flat(void) {
    TEST("diffgeo: parallel transport in flat space");
    uint32_t dim = 2;
    christoffel_symbols_t* chris = christoffel_create(dim);
    /* 3 points along x-axis */
    float curve[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f};
    float v_init[] = {0.0f, 1.0f};
    float v_out[2];
    diffgeo_error_t ret = parallel_transport_along_curve(chris, curve, 3, dim, v_init, v_out);
    if (ret != DIFFGEO_OK) { FAIL("transport failed"); christoffel_destroy(chris); return; }
    /* Flat space: transport preserves vector */
    if (!APPROX(v_out[0], 0.0f, 0.1f) || !APPROX(v_out[1], 1.0f, 0.1f)) {
        FAIL("transport changed vector in flat space");
    } else {
        PASS();
    }
    christoffel_destroy(chris);
}

static void test_geodesic_distance_flat(void) {
    TEST("diffgeo: geodesic distance in flat space");
    uint32_t dim = 2;
    christoffel_symbols_t* chris = christoffel_create(dim);
    riemannian_metric_t* m = riemannian_metric_create(dim);
    riemannian_metric_invert(m);
    float p[] = {0.0f, 0.0f};
    float q[] = {3.0f, 4.0f};
    float d = geodesic_distance(chris, m, dim, p, q);
    if (!APPROX(d, 5.0f, 0.5f)) {
        FAIL("wrong distance");
    } else {
        PASS();
    }
    christoffel_destroy(chris);
    riemannian_metric_destroy(m);
}

static void test_log_map_flat(void) {
    TEST("diffgeo: log map in flat space");
    uint32_t dim = 2;
    christoffel_symbols_t* chris = christoffel_create(dim);
    riemannian_metric_t* m = riemannian_metric_create(dim);
    riemannian_metric_invert(m);
    float p[] = {0.0f, 0.0f};
    float q[] = {1.0f, 0.0f};
    float result[2];
    diffgeo_error_t ret = log_map(chris, m, dim, p, q, result, 20);
    if (ret != DIFFGEO_OK) { FAIL("log_map failed"); }
    else if (!APPROX(result[0], 1.0f, 0.3f) || !APPROX(result[1], 0.0f, 0.3f)) {
        FAIL("wrong log_map result");
    } else {
        PASS();
    }
    christoffel_destroy(chris);
    riemannian_metric_destroy(m);
}

/*=============================================================================
 * Lorentz Model Tests
 *===========================================================================*/

static void test_lorentz_point_create(void) {
    TEST("lorentz: point create at origin");
    lorentz_point_t* p = lorentz_point_create(3, NULL, 1.0f);
    if (!p) { FAIL("create returned NULL"); return; }
    if (!APPROX(p->coords[0], 1.0f, 1e-5f)) { FAIL("wrong x0"); lorentz_point_destroy(p); return; }
    if (!APPROX(p->coords[1], 0.0f, 1e-5f)) { FAIL("wrong x1"); lorentz_point_destroy(p); return; }
    lorentz_point_destroy(p);
    PASS();
}

static void test_lorentz_point_create_coords(void) {
    TEST("lorentz: point create with coordinates");
    float coords[] = {0.5f, 0.3f, 0.1f};
    lorentz_point_t* p = lorentz_point_create(3, coords, 1.0f);
    if (!p) { FAIL("create returned NULL"); return; }
    float expected_x0 = sqrtf(1.0f + 0.25f + 0.09f + 0.01f);
    if (!APPROX(p->coords[0], expected_x0, 1e-4f)) { FAIL("wrong x0"); lorentz_point_destroy(p); return; }
    lorentz_point_destroy(p);
    PASS();
}

static void test_lorentz_hyperboloid_constraint(void) {
    TEST("lorentz: hyperboloid constraint <p,p>_L = -1");
    float coords[] = {0.5f, 0.3f, 0.1f};
    lorentz_point_t* p = lorentz_point_create(3, coords, 1.0f);
    float ip = lorentz_inner_product(p, p);
    if (!APPROX(ip, -1.0f, 1e-4f)) { FAIL("constraint violated"); lorentz_point_destroy(p); return; }
    lorentz_point_destroy(p);
    PASS();
}

static void test_lorentz_inner_product(void) {
    TEST("lorentz: Minkowski inner product at origin");
    lorentz_point_t* p = lorentz_point_create(3, NULL, 1.0f);
    float ip = lorentz_inner_product(p, p);
    if (!APPROX(ip, -1.0f, 1e-5f)) { FAIL("wrong inner product"); lorentz_point_destroy(p); return; }
    lorentz_point_destroy(p);
    PASS();
}

static void test_lorentz_distance_same_point(void) {
    TEST("lorentz: distance to self is zero");
    lorentz_point_t* p = lorentz_point_create(3, NULL, 1.0f);
    float d = lorentz_distance(p, p);
    if (!APPROX(d, 0.0f, 1e-5f)) { FAIL("nonzero self-distance"); lorentz_point_destroy(p); return; }
    lorentz_point_destroy(p);
    PASS();
}

static void test_lorentz_distance_symmetric(void) {
    TEST("lorentz: distance is symmetric");
    float c1[] = {0.5f, 0.0f, 0.0f};
    float c2[] = {0.0f, 0.3f, 0.0f};
    lorentz_point_t* p1 = lorentz_point_create(3, c1, 1.0f);
    lorentz_point_t* p2 = lorentz_point_create(3, c2, 1.0f);
    float d12 = lorentz_distance(p1, p2);
    float d21 = lorentz_distance(p2, p1);
    if (!APPROX(d12, d21, 1e-5f)) { FAIL("not symmetric"); }
    else if (d12 <= 0.0f) { FAIL("non-positive distance"); }
    else { PASS(); }
    lorentz_point_destroy(p1);
    lorentz_point_destroy(p2);
}

static void test_lorentz_distance_positive(void) {
    TEST("lorentz: distance between distinct points > 0");
    float c1[] = {1.0f, 0.0f, 0.0f};
    float c2[] = {0.0f, 1.0f, 0.0f};
    lorentz_point_t* p1 = lorentz_point_create(3, c1, 1.0f);
    lorentz_point_t* p2 = lorentz_point_create(3, c2, 1.0f);
    float d = lorentz_distance(p1, p2);
    if (d <= 0.0f) { FAIL("non-positive distance"); }
    else { PASS(); }
    lorentz_point_destroy(p1);
    lorentz_point_destroy(p2);
}

static void test_lorentz_exp_log_roundtrip(void) {
    TEST("lorentz: exp/log map roundtrip");
    lorentz_point_t* base = lorentz_point_create(3, NULL, 1.0f);
    /* Tangent at origin: v0=0, spatial components free */
    float tangent[] = {0.0f, 0.3f, 0.2f, 0.1f};
    lorentz_point_t* target = lorentz_exp_map(base, tangent);
    if (!target) { FAIL("exp_map returned NULL"); lorentz_point_destroy(base); return; }
    float* log_v = lorentz_log_map(base, target);
    if (!log_v) { FAIL("log_map returned NULL"); lorentz_point_destroy(base); lorentz_point_destroy(target); return; }
    if (!APPROX(log_v[1], 0.3f, 0.05f) || !APPROX(log_v[2], 0.2f, 0.05f) || !APPROX(log_v[3], 0.1f, 0.05f)) {
        FAIL("roundtrip mismatch");
    } else {
        PASS();
    }
    free(log_v);
    lorentz_point_destroy(target);
    lorentz_point_destroy(base);
}

static void test_lorentz_parallel_transport(void) {
    TEST("lorentz: parallel transport preserves norm");
    float c1[] = {0.5f, 0.0f};
    lorentz_point_t* x = lorentz_point_create(2, NULL, 1.0f);
    lorentz_point_t* y = lorentz_point_create(2, c1, 1.0f);
    float v[] = {0.0f, 1.0f, 0.5f};
    float result[3];
    int ret = lorentz_parallel_transport(x, y, v, result);
    if (ret != 0) { FAIL("transport failed"); }
    else {
        float norm_v = lorentz_tangent_norm(v, 2);
        float norm_r = lorentz_tangent_norm(result, 2);
        if (!APPROX(norm_v, norm_r, 0.1f)) { FAIL("norm not preserved"); }
        else { PASS(); }
    }
    lorentz_point_destroy(x);
    lorentz_point_destroy(y);
}

static void test_lorentz_poincare_roundtrip(void) {
    TEST("lorentz: Poincare conversion roundtrip");
    float coords[] = {0.3f, 0.2f};
    lorentz_point_t* lp = lorentz_point_create(2, coords, 1.0f);
    float poincare[2];
    int ret = lorentz_to_poincare(lp, poincare);
    if (ret != 0) { FAIL("to_poincare failed"); lorentz_point_destroy(lp); return; }
    lorentz_point_t* back = lorentz_from_poincare(poincare, 2, 1.0f);
    if (!back) { FAIL("from_poincare returned NULL"); lorentz_point_destroy(lp); return; }
    if (!APPROX(lp->coords[0], back->coords[0], 1e-3f) ||
        !APPROX(lp->coords[1], back->coords[1], 1e-3f) ||
        !APPROX(lp->coords[2], back->coords[2], 1e-3f)) {
        FAIL("roundtrip mismatch");
    } else {
        PASS();
    }
    lorentz_point_destroy(lp);
    lorentz_point_destroy(back);
}

static void test_lorentz_slerp_endpoints(void) {
    TEST("lorentz: SLERP at t=0 and t=1");
    float c1[] = {0.5f, 0.0f};
    float c2[] = {0.0f, 0.5f};
    lorentz_point_t* x = lorentz_point_create(2, c1, 1.0f);
    lorentz_point_t* y = lorentz_point_create(2, c2, 1.0f);
    lorentz_point_t* s0 = lorentz_slerp(x, y, 0.0f);
    lorentz_point_t* s1 = lorentz_slerp(x, y, 1.0f);
    if (!s0 || !s1) { FAIL("slerp returned NULL"); goto slerp_cleanup; }
    if (!APPROX(s0->coords[1], x->coords[1], 0.05f) ||
        !APPROX(s0->coords[2], x->coords[2], 0.05f)) {
        FAIL("slerp(0) != start"); goto slerp_cleanup;
    }
    if (!APPROX(s1->coords[1], y->coords[1], 0.05f) ||
        !APPROX(s1->coords[2], y->coords[2], 0.05f)) {
        FAIL("slerp(1) != end"); goto slerp_cleanup;
    }
    PASS();
slerp_cleanup:
    lorentz_point_destroy(s0);
    lorentz_point_destroy(s1);
    lorentz_point_destroy(x);
    lorentz_point_destroy(y);
}

static void test_lorentz_midpoint(void) {
    TEST("lorentz: midpoint equidistant from endpoints");
    float c1[] = {0.4f, 0.0f};
    float c2[] = {0.0f, 0.4f};
    lorentz_point_t* x = lorentz_point_create(2, c1, 1.0f);
    lorentz_point_t* y = lorentz_point_create(2, c2, 1.0f);
    lorentz_point_t* mid = lorentz_midpoint(x, y);
    if (!mid) { FAIL("midpoint returned NULL"); }
    else {
        float d1 = lorentz_distance(x, mid);
        float d2 = lorentz_distance(y, mid);
        if (!APPROX(d1, d2, 0.05f)) { FAIL("not equidistant"); }
        else { PASS(); }
        lorentz_point_destroy(mid);
    }
    lorentz_point_destroy(x);
    lorentz_point_destroy(y);
}

static void test_lorentz_point_copy(void) {
    TEST("lorentz: point copy");
    float coords[] = {0.1f, 0.2f, 0.3f};
    lorentz_point_t* p = lorentz_point_create(3, coords, 1.0f);
    lorentz_point_t* cp = lorentz_point_copy(p);
    if (!cp) { FAIL("copy returned NULL"); lorentz_point_destroy(p); return; }
    if (cp->dim != p->dim || !APPROX(cp->coords[0], p->coords[0], 1e-6f)) {
        FAIL("copy mismatch");
    } else {
        PASS();
    }
    lorentz_point_destroy(p);
    lorentz_point_destroy(cp);
}

static void test_lorentz_curvature_parameter(void) {
    TEST("lorentz: curvature parameter c != 1");
    float coords[] = {0.3f};
    lorentz_point_t* p = lorentz_point_create(1, coords, 2.0f);
    if (!p) { FAIL("create returned NULL"); return; }
    /* <p,p>_L should be -1/c = -0.5 */
    float ip = lorentz_inner_product(p, p);
    if (!APPROX(ip, -0.5f, 1e-4f)) { FAIL("wrong inner product for c=2"); }
    else { PASS(); }
    lorentz_point_destroy(p);
}

/*=============================================================================
 * SO(3) Lie Group Tests
 *===========================================================================*/

static void test_so3_identity(void) {
    TEST("so3: identity rotation");
    so3_rotation_t I = so3_identity();
    if (!APPROX(I.m[0], 1.0f, 1e-6f) || !APPROX(I.m[4], 1.0f, 1e-6f) || !APPROX(I.m[8], 1.0f, 1e-6f)) {
        FAIL("wrong diagonal"); return;
    }
    if (!APPROX(I.m[1], 0.0f, 1e-6f) || !APPROX(I.m[3], 0.0f, 1e-6f)) {
        FAIL("wrong off-diagonal"); return;
    }
    PASS();
}

static void test_so3_from_axis_angle(void) {
    TEST("so3: rotation from axis-angle (90° z)");
    float axis[] = {0.0f, 0.0f, 1.0f};
    float angle = (float)M_PI / 2.0f;
    so3_rotation_t R = so3_from_axis_angle(axis, angle);
    /* 90° around z: [[0,-1,0],[1,0,0],[0,0,1]] */
    if (!APPROX(R.m[0], 0.0f, 1e-4f) || !APPROX(R.m[1], -1.0f, 1e-4f) ||
        !APPROX(R.m[3], 1.0f, 1e-4f) || !APPROX(R.m[4], 0.0f, 1e-4f) ||
        !APPROX(R.m[8], 1.0f, 1e-4f)) {
        FAIL("wrong rotation matrix"); return;
    }
    PASS();
}

static void test_so3_exp_log_roundtrip(void) {
    TEST("so3: exp/log roundtrip");
    so3_algebra_t omega = { .v = {0.5f, 0.3f, 0.1f} };
    so3_rotation_t R = so3_exp(&omega);
    so3_algebra_t recovered = so3_log(&R);
    if (!APPROX(recovered.v[0], 0.5f, 0.02f) ||
        !APPROX(recovered.v[1], 0.3f, 0.02f) ||
        !APPROX(recovered.v[2], 0.1f, 0.02f)) {
        FAIL("roundtrip mismatch"); return;
    }
    PASS();
}

static void test_so3_multiply_identity(void) {
    TEST("so3: R * I = R");
    float axis[] = {1.0f, 0.0f, 0.0f};
    so3_rotation_t R = so3_from_axis_angle(axis, 1.0f);
    so3_rotation_t I = so3_identity();
    so3_rotation_t RI = so3_multiply(&R, &I);
    int ok = 1;
    for (int i = 0; i < 9; i++) {
        if (!APPROX(RI.m[i], R.m[i], 1e-5f)) { ok = 0; break; }
    }
    if (!ok) { FAIL("R*I != R"); return; }
    PASS();
}

static void test_so3_transpose_is_inverse(void) {
    TEST("so3: R^T * R = I");
    float axis[] = {0.577f, 0.577f, 0.577f};
    so3_rotation_t R = so3_from_axis_angle(axis, 1.2f);
    so3_rotation_t Rt = so3_transpose(&R);
    so3_rotation_t product = so3_multiply(&Rt, &R);
    so3_rotation_t I = so3_identity();
    int ok = 1;
    for (int i = 0; i < 9; i++) {
        if (!APPROX(product.m[i], I.m[i], 1e-4f)) { ok = 0; break; }
    }
    if (!ok) { FAIL("R^T*R != I"); return; }
    PASS();
}

static void test_so3_rotate_vector(void) {
    TEST("so3: rotate vector 90° around z");
    float axis[] = {0.0f, 0.0f, 1.0f};
    so3_rotation_t R = so3_from_axis_angle(axis, (float)M_PI / 2.0f);
    float x[] = {1.0f, 0.0f, 0.0f};
    float y[3];
    so3_rotate_vector(&R, x, y);
    if (!APPROX(y[0], 0.0f, 1e-4f) || !APPROX(y[1], 1.0f, 1e-4f) || !APPROX(y[2], 0.0f, 1e-4f)) {
        FAIL("wrong rotated vector"); return;
    }
    PASS();
}

static void test_so3_distance_identity(void) {
    TEST("so3: distance from identity to identity = 0");
    so3_rotation_t I = so3_identity();
    float d = so3_distance(&I, &I);
    if (!APPROX(d, 0.0f, 1e-5f)) { FAIL("nonzero self-distance"); return; }
    PASS();
}

static void test_so3_distance_symmetric(void) {
    TEST("so3: distance is symmetric");
    float a1[] = {1.0f, 0.0f, 0.0f};
    float a2[] = {0.0f, 1.0f, 0.0f};
    so3_rotation_t R1 = so3_from_axis_angle(a1, 0.5f);
    so3_rotation_t R2 = so3_from_axis_angle(a2, 0.8f);
    float d12 = so3_distance(&R1, &R2);
    float d21 = so3_distance(&R2, &R1);
    if (!APPROX(d12, d21, 1e-4f)) { FAIL("not symmetric"); return; }
    PASS();
}

static void test_so3_slerp_endpoints(void) {
    TEST("so3: SLERP at t=0 and t=1");
    float a1[] = {1.0f, 0.0f, 0.0f};
    float a2[] = {0.0f, 1.0f, 0.0f};
    so3_rotation_t R1 = so3_from_axis_angle(a1, 0.5f);
    so3_rotation_t R2 = so3_from_axis_angle(a2, 0.8f);
    so3_rotation_t s0 = so3_slerp(&R1, &R2, 0.0f);
    so3_rotation_t s1 = so3_slerp(&R1, &R2, 1.0f);
    int ok0 = 1, ok1 = 1;
    for (int i = 0; i < 9; i++) {
        if (!APPROX(s0.m[i], R1.m[i], 0.02f)) ok0 = 0;
        if (!APPROX(s1.m[i], R2.m[i], 0.02f)) ok1 = 0;
    }
    if (!ok0) { FAIL("slerp(0) != R1"); return; }
    if (!ok1) { FAIL("slerp(1) != R2"); return; }
    PASS();
}

static void test_so3_project(void) {
    TEST("so3: project noisy matrix onto SO(3)");
    float noisy[9] = {1.01f, 0.02f, -0.01f,
                      -0.02f, 0.99f, 0.03f,
                      0.01f, -0.03f, 1.01f};
    so3_rotation_t R = so3_project(noisy);
    so3_rotation_t Rt = so3_transpose(&R);
    so3_rotation_t product = so3_multiply(&Rt, &R);
    int ok = 1;
    for (int i = 0; i < 9; i++) {
        float expected = (i == 0 || i == 4 || i == 8) ? 1.0f : 0.0f;
        if (!APPROX(product.m[i], expected, 0.05f)) { ok = 0; break; }
    }
    if (!ok) { FAIL("projected matrix not orthogonal"); return; }
    PASS();
}

static void test_so3_180_rotation(void) {
    TEST("so3: 180° rotation (pi boundary)");
    float axis[] = {1.0f, 0.0f, 0.0f};
    so3_rotation_t R = so3_from_axis_angle(axis, (float)M_PI);
    /* 180° around x: [[1,0,0],[0,-1,0],[0,0,-1]] */
    if (!APPROX(R.m[0], 1.0f, 1e-4f) || !APPROX(R.m[4], -1.0f, 1e-4f) || !APPROX(R.m[8], -1.0f, 1e-4f)) {
        FAIL("wrong pi rotation"); return;
    }
    PASS();
}

/*=============================================================================
 * Matrix Exp/Log Tests
 *===========================================================================*/

static void test_matrix_exp_identity(void) {
    TEST("matrix_exp: exp(0) = I");
    float A[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float result[4];
    int ret = matrix_exp(A, 2, result);
    if (ret != 0) { FAIL("matrix_exp failed"); return; }
    if (!APPROX(result[0], 1.0f, 1e-4f) || !APPROX(result[3], 1.0f, 1e-4f) ||
        !APPROX(result[1], 0.0f, 1e-4f) || !APPROX(result[2], 0.0f, 1e-4f)) {
        FAIL("exp(0) != I"); return;
    }
    PASS();
}

static void test_matrix_exp_diagonal(void) {
    TEST("matrix_exp: exp(diag(a,b)) = diag(e^a, e^b)");
    float A[4] = {1.0f, 0.0f, 0.0f, 2.0f};
    float result[4];
    int ret = matrix_exp(A, 2, result);
    if (ret != 0) { FAIL("matrix_exp failed"); return; }
    if (!APPROX(result[0], expf(1.0f), 0.1f) || !APPROX(result[3], expf(2.0f), 0.2f)) {
        FAIL("wrong diagonal exp"); return;
    }
    PASS();
}

static void test_matrix_log_identity(void) {
    TEST("matrix_log: log(I) = 0");
    float I[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float result[4];
    int ret = matrix_log(I, 2, result);
    if (ret != 0) { FAIL("matrix_log failed"); return; }
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        if (!APPROX(result[i], 0.0f, 1e-3f)) { ok = 0; break; }
    }
    if (!ok) { FAIL("log(I) != 0"); return; }
    PASS();
}

static void test_matrix_exp_log_roundtrip(void) {
    TEST("matrix_exp/log: roundtrip for small matrix");
    float A[4] = {0.1f, 0.2f, -0.1f, 0.15f};
    float expA[4], logExpA[4];
    int ret = matrix_exp(A, 2, expA);
    if (ret != 0) { FAIL("exp failed"); return; }
    ret = matrix_log(expA, 2, logExpA);
    if (ret != 0) { FAIL("log failed"); return; }
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        if (!APPROX(logExpA[i], A[i], 0.1f)) { ok = 0; break; }
    }
    if (!ok) { FAIL("roundtrip mismatch"); return; }
    PASS();
}

static void test_matrix_exp_dim_too_large(void) {
    TEST("matrix_exp: reject dim > LIE_MAX_DIM");
    float dummy[1];
    int ret = matrix_exp(dummy, LIE_MAX_DIM + 1, dummy);
    if (ret != -1) { FAIL("should reject large dim"); return; }
    PASS();
}

static void test_matrix_exp_1x1(void) {
    TEST("matrix_exp: 1x1 matrix exp(a) = e^a");
    float A[1] = {2.0f};
    float result[1];
    int ret = matrix_exp(A, 1, result);
    if (ret != 0) { FAIL("failed"); return; }
    if (!APPROX(result[0], expf(2.0f), 0.2f)) { FAIL("wrong value"); return; }
    PASS();
}

/*=============================================================================
 * NULL safety tests
 *===========================================================================*/

static void test_null_safety(void) {
    TEST("null safety: all modules handle NULL gracefully");

    riemannian_metric_destroy(NULL);
    christoffel_destroy(NULL);
    curvature_destroy(NULL);

    lorentz_point_destroy(NULL);
    if (lorentz_point_copy(NULL) != NULL) { FAIL("copy(NULL) != NULL"); return; }
    if (lorentz_inner_product(NULL, NULL) != 0.0f) { FAIL("ip(NULL) != 0"); return; }
    if (lorentz_distance(NULL, NULL) != 0.0f) { FAIL("dist(NULL) != 0"); return; }
    if (lorentz_exp_map(NULL, NULL) != NULL) { FAIL("exp(NULL) != NULL"); return; }
    if (lorentz_log_map(NULL, NULL) != NULL) { FAIL("log(NULL) != NULL"); return; }

    float zero3[3] = {0};
    float zero_y[3];
    so3_rotation_t I = so3_identity();
    so3_rotate_vector(&I, zero3, zero_y);

    PASS();
}

/*=============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n=== Geometry Modules Test Suite ===\n\n");

    printf("[Differential Geometry]\n");
    test_metric_create_destroy();
    test_metric_identity();
    test_metric_norm();
    test_metric_set_and_inverse();
    test_metric_raise_lower();
    test_metric_scaled_inner_product();
    test_christoffel_create_destroy();
    test_christoffel_flat_space();
    test_curvature_create_destroy();
    test_curvature_flat_space();
    test_curvature_scalar();
    test_exp_map_flat();
    test_geodesic_shoot_flat();
    test_parallel_transport_flat();
    test_geodesic_distance_flat();
    test_log_map_flat();

    printf("\n[Lorentz Model]\n");
    test_lorentz_point_create();
    test_lorentz_point_create_coords();
    test_lorentz_hyperboloid_constraint();
    test_lorentz_inner_product();
    test_lorentz_distance_same_point();
    test_lorentz_distance_symmetric();
    test_lorentz_distance_positive();
    test_lorentz_exp_log_roundtrip();
    test_lorentz_parallel_transport();
    test_lorentz_poincare_roundtrip();
    test_lorentz_slerp_endpoints();
    test_lorentz_midpoint();
    test_lorentz_point_copy();
    test_lorentz_curvature_parameter();

    printf("\n[SO(3) Lie Group]\n");
    test_so3_identity();
    test_so3_from_axis_angle();
    test_so3_exp_log_roundtrip();
    test_so3_multiply_identity();
    test_so3_transpose_is_inverse();
    test_so3_rotate_vector();
    test_so3_distance_identity();
    test_so3_distance_symmetric();
    test_so3_slerp_endpoints();
    test_so3_project();
    test_so3_180_rotation();

    printf("\n[Matrix Exp/Log]\n");
    test_matrix_exp_identity();
    test_matrix_exp_diagonal();
    test_matrix_log_identity();
    test_matrix_exp_log_roundtrip();
    test_matrix_exp_dim_too_large();
    test_matrix_exp_1x1();

    printf("\n[NULL Safety]\n");
    test_null_safety();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
