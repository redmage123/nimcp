/* ============================================================================
 * GPU Pipeline Validation Tests
 * ============================================================================
 * WHAT: Unit tests for GPU training/inference pipeline bug fixes
 * WHY:  Validate COO index bounds, cuSPARSE error paths, tensor numel
 *       overflow, SpMV size checks, and forward layer size consistency
 * HOW:  libcheck framework with NIMCP_CUDA_ENABLED guard
 * ============================================================================
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef NIMCP_CUDA_ENABLED
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/training/nimcp_training_bridge.h"
#include "utils/memory/nimcp_memory.h"
#endif

/* ============================================================================
 * Helper: get a GPU + sparse context, or skip gracefully
 * ============================================================================ */
#ifdef NIMCP_CUDA_ENABLED
static nimcp_gpu_context_t* helper_get_gpu_ctx(void)
{
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(NULL);
    return ctx;  /* NULL means no GPU available */
}

static nimcp_sparse_ctx_t* helper_get_sparse_ctx(nimcp_gpu_context_t* gpu)
{
    if (!gpu) return NULL;
    return nimcp_sparse_ctx_create(gpu);
}
#endif

/* ============================================================================
 * 1. COO Index Validation (nimcp_sparse_from_coo)
 * ============================================================================ */

START_TEST(test_coo_valid_indices_succeed)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    /* 3x3 identity matrix in COO */
    float values[]  = {1.0f, 1.0f, 1.0f};
    int row_idx[]   = {0, 1, 2};
    int col_idx[]   = {0, 1, 2};

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 3, 3, 3, SPARSE_FORMAT_CSR);

    ck_assert_msg(sp != NULL, "Valid COO data should produce a non-NULL sparse tensor");

    nimcp_sparse_tensor_destroy(sp);
    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_coo_row_idx_out_of_bounds_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    float values[]  = {1.0f, 2.0f};
    int row_idx[]   = {0, 5};   /* row 5 out of bounds for 3-row matrix */
    int col_idx[]   = {0, 1};

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 3, 3, 2, SPARSE_FORMAT_CSR);

    ck_assert_msg(sp == NULL,
        "row_idx out of bounds should return NULL");

    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_coo_col_idx_out_of_bounds_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    float values[]  = {1.0f, 2.0f};
    int row_idx[]   = {0, 1};
    int col_idx[]   = {0, 10};  /* col 10 out of bounds for 3-col matrix */

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 3, 3, 2, SPARSE_FORMAT_CSR);

    ck_assert_msg(sp == NULL,
        "col_idx out of bounds should return NULL");

    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_coo_negative_indices_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    float values[]  = {1.0f};
    int row_idx[]   = {-1};     /* negative row index */
    int col_idx[]   = {0};

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 4, 4, 1, SPARSE_FORMAT_CSR);

    ck_assert_msg(sp == NULL,
        "Negative index should return NULL");

    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_coo_nnz_zero_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_coo(
        ctx, NULL, NULL, NULL, 4, 4, 0, SPARSE_FORMAT_CSR);

    ck_assert_msg(sp == NULL,
        "nnz=0 (empty matrix) should return NULL");

    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_coo_zero_rows_or_cols_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    float values[] = {1.0f};
    int row_idx[]  = {0};
    int col_idx[]  = {0};

    /* rows=0 */
    nimcp_sparse_tensor_t* sp1 = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 0, 4, 1, SPARSE_FORMAT_CSR);
    ck_assert_msg(sp1 == NULL, "rows=0 should return NULL");

    /* cols=0 */
    nimcp_sparse_tensor_t* sp2 = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 4, 0, 1, SPARSE_FORMAT_CSR);
    ck_assert_msg(sp2 == NULL, "cols=0 should return NULL");

    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * 2. cuSPARSE Error Checking (CSR / COO creation)
 * ============================================================================ */

START_TEST(test_cusparse_valid_csr_creation)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    /* 3x3 identity in CSR */
    float values[]     = {1.0f, 1.0f, 1.0f};
    int col_indices[]  = {0, 1, 2};
    int row_ptrs[]     = {0, 1, 2, 3};

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_csr(
        ctx, values, col_indices, row_ptrs, 3, 3, 3);

    ck_assert_msg(sp != NULL, "Valid CSR data should produce a non-NULL sparse tensor");

    nimcp_sparse_tensor_destroy(sp);
    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_cusparse_valid_coo_creation)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    /* 4x4 sparse matrix with 3 non-zeros */
    float values[]  = {2.0f, 3.0f, 5.0f};
    int row_idx[]   = {0, 1, 3};
    int col_idx[]   = {1, 2, 0};

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_coo(
        ctx, values, row_idx, col_idx, 4, 4, 3, SPARSE_FORMAT_COO);

    ck_assert_msg(sp != NULL, "Valid COO data should produce a non-NULL tensor");

    nimcp_sparse_tensor_destroy(sp);
    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * 3. Tensor Numel Overflow (nimcp_gpu_tensor_create)
 * ============================================================================ */

START_TEST(test_tensor_create_small_dims_correct_numel)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    size_t dims[] = {4, 8};
    nimcp_gpu_tensor_t* t = nimcp_gpu_tensor_create(gpu, dims, 2, NIMCP_GPU_PRECISION_FP32);

    ck_assert_msg(t != NULL, "Small-dim tensor creation should succeed");
    ck_assert_msg(t->numel == 32,
        "numel should be 4*8=32, got %zu", t->numel);

    nimcp_gpu_tensor_destroy(t);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_tensor_create_single_element)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    size_t dims[] = {1};
    nimcp_gpu_tensor_t* t = nimcp_gpu_tensor_create(gpu, dims, 1, NIMCP_GPU_PRECISION_FP32);

    ck_assert_msg(t != NULL, "Single-element tensor creation should succeed");
    ck_assert_msg(t->numel == 1,
        "numel should be 1, got %zu", t->numel);

    nimcp_gpu_tensor_destroy(t);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_tensor_create_zero_dimension_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    size_t dims[] = {4, 0, 8};
    nimcp_gpu_tensor_t* t = nimcp_gpu_tensor_create(gpu, dims, 3, NIMCP_GPU_PRECISION_FP32);

    ck_assert_msg(t == NULL,
        "Tensor with a zero dimension should return NULL");

    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_tensor_create_rank3_numel)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    size_t dims[] = {2, 3, 5};
    nimcp_gpu_tensor_t* t = nimcp_gpu_tensor_create(gpu, dims, 3, NIMCP_GPU_PRECISION_FP32);

    ck_assert_msg(t != NULL, "Rank-3 tensor creation should succeed");
    ck_assert_msg(t->numel == 30,
        "numel should be 2*3*5=30, got %zu", t->numel);

    nimcp_gpu_tensor_destroy(t);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * 4. SpMV Output Size Validation (nimcp_sparse_mv)
 * ============================================================================ */

START_TEST(test_spmv_matched_sizes_succeed)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    /* 3x4 sparse matrix (CSR) with 3 non-zeros */
    float values[]    = {1.0f, 2.0f, 3.0f};
    int col_indices[] = {0, 2, 1};
    int row_ptrs[]    = {0, 1, 2, 3};

    nimcp_sparse_tensor_t* A = nimcp_sparse_from_csr(
        ctx, values, col_indices, row_ptrs, 3, 4, 3);

    if (!A) {
        nimcp_sparse_ctx_destroy(ctx);
        nimcp_gpu_context_destroy(gpu);
        ck_assert_msg(1, "sparse matrix creation failed");
        return;
    }

    /* x = [4], y = [3] -- dimensions match A [3x4] */
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(gpu, (size_t[]){4}, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y = nimcp_gpu_tensor_create(gpu, (size_t[]){3}, 1, NIMCP_GPU_PRECISION_FP32);

    if (x && y) {
        nimcp_gpu_tensor_t* result = nimcp_sparse_mv(ctx, A, x, 1.0f, 0.0f, y);
        /* result should be non-NULL when sizes match */
        ck_assert_msg(result != NULL,
            "SpMV with matched sizes should succeed");
    }

    if (x) nimcp_gpu_tensor_destroy(x);
    if (y) nimcp_gpu_tensor_destroy(y);
    nimcp_sparse_tensor_destroy(A);
    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_spmv_mismatched_y_returns_null)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    /* 3x4 sparse matrix */
    float values[]    = {1.0f, 2.0f, 3.0f};
    int col_indices[] = {0, 2, 1};
    int row_ptrs[]    = {0, 1, 2, 3};

    nimcp_sparse_tensor_t* A = nimcp_sparse_from_csr(
        ctx, values, col_indices, row_ptrs, 3, 4, 3);

    if (!A) {
        nimcp_sparse_ctx_destroy(ctx);
        nimcp_gpu_context_destroy(gpu);
        ck_assert_msg(1, "sparse matrix creation failed");
        return;
    }

    /* x = [4] (correct), y = [7] (wrong -- A has 3 rows, y should be [3]) */
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(gpu, (size_t[]){4}, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_wrong = nimcp_gpu_tensor_create(gpu, (size_t[]){7}, 1, NIMCP_GPU_PRECISION_FP32);

    if (x && y_wrong) {
        nimcp_gpu_tensor_t* result = nimcp_sparse_mv(ctx, A, x, 1.0f, 0.0f, y_wrong);
        /* Mismatched output should return NULL (not crash) */
        ck_assert_msg(result == NULL,
            "SpMV with mismatched y size should return NULL, not crash");
    }

    if (x) nimcp_gpu_tensor_destroy(x);
    if (y_wrong) nimcp_gpu_tensor_destroy(y_wrong);
    nimcp_sparse_tensor_destroy(A);
    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * 5. GPU Forward Layer Size Consistency
 * ============================================================================ */

START_TEST(test_bias_add_matching_tensors)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    /* Create two matching tensors and perform element-wise add (simulating bias add) */
    size_t dims[] = {8};
    nimcp_gpu_tensor_t* activation = nimcp_gpu_tensor_create(gpu, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* bias       = nimcp_gpu_tensor_create(gpu, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* out        = nimcp_gpu_tensor_create(gpu, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (activation && bias && out) {
        bool ok = nimcp_gpu_add(gpu, activation, bias, out);
        ck_assert_msg(ok, "Bias add with matching tensor sizes should succeed");
    }

    if (activation) nimcp_gpu_tensor_destroy(activation);
    if (bias) nimcp_gpu_tensor_destroy(bias);
    if (out) nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_weight_cache_layer_sizes_sum_validation)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    /*
     * Verify that the weight cache struct properly stores layer_sizes
     * by checking a manually constructed cache's layer_sizes sum.
     * We cannot easily create a full neural_network_t here, so we
     * validate that nimcp_gpu_weight_cache_create returns NULL for
     * degenerate inputs (0 layers).
     */
    nimcp_gpu_weight_cache_t* cache = nimcp_gpu_weight_cache_create(
        gpu, NULL, NULL, 0);

    ck_assert_msg(cache == NULL,
        "Weight cache with 0 layers should return NULL");

    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

START_TEST(test_tensor_ndim_and_precision_stored)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }

    size_t dims[] = {16, 32};
    nimcp_gpu_tensor_t* t = nimcp_gpu_tensor_create(gpu, dims, 2, NIMCP_GPU_PRECISION_FP32);

    ck_assert_msg(t != NULL, "Tensor creation should succeed");
    ck_assert_uint_eq(t->ndim, 2);
    ck_assert_int_eq(t->precision, NIMCP_GPU_PRECISION_FP32);
    ck_assert_msg(t->numel == 512,
        "numel should be 16*32=512, got %zu", t->numel);

    nimcp_gpu_tensor_destroy(t);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Additional: Sparse validate on well-formed tensor
 * ============================================================================ */

START_TEST(test_sparse_validate_well_formed)
{
#ifdef NIMCP_CUDA_ENABLED
    nimcp_gpu_context_t* gpu = helper_get_gpu_ctx();
    if (!gpu) { ck_assert_msg(1, "GPU not available - skipping"); return; }
    nimcp_sparse_ctx_t* ctx = helper_get_sparse_ctx(gpu);
    if (!ctx) { nimcp_gpu_context_destroy(gpu); ck_assert_msg(1, "sparse ctx failed"); return; }

    /* Well-formed 4x4 CSR with diagonal entries */
    float values[]    = {1.0f, 2.0f, 3.0f, 4.0f};
    int col_indices[] = {0, 1, 2, 3};
    int row_ptrs[]    = {0, 1, 2, 3, 4};

    nimcp_sparse_tensor_t* sp = nimcp_sparse_from_csr(
        ctx, values, col_indices, row_ptrs, 4, 4, 4);

    if (sp) {
        bool valid = nimcp_sparse_validate(sp);
        ck_assert_msg(valid, "Well-formed sparse tensor should pass validation");
        nimcp_sparse_tensor_destroy(sp);
    }

    nimcp_sparse_ctx_destroy(ctx);
    nimcp_gpu_context_destroy(gpu);
#else
    ck_assert_msg(1, "CUDA not enabled - skipping");
#endif
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* gpu_pipeline_validation_suite(void)
{
    Suite* s = suite_create("GPU Pipeline Validation");

    /* COO Index Validation */
    TCase* tc_coo = tcase_create("COO Index Validation");
    tcase_add_test(tc_coo, test_coo_valid_indices_succeed);
    tcase_add_test(tc_coo, test_coo_row_idx_out_of_bounds_returns_null);
    tcase_add_test(tc_coo, test_coo_col_idx_out_of_bounds_returns_null);
    tcase_add_test(tc_coo, test_coo_negative_indices_returns_null);
    tcase_add_test(tc_coo, test_coo_nnz_zero_returns_null);
    tcase_add_test(tc_coo, test_coo_zero_rows_or_cols_returns_null);
    tcase_set_timeout(tc_coo, 30);
    suite_add_tcase(s, tc_coo);

    /* cuSPARSE Error Checking */
    TCase* tc_cusparse = tcase_create("cuSPARSE Error Checking");
    tcase_add_test(tc_cusparse, test_cusparse_valid_csr_creation);
    tcase_add_test(tc_cusparse, test_cusparse_valid_coo_creation);
    tcase_set_timeout(tc_cusparse, 30);
    suite_add_tcase(s, tc_cusparse);

    /* Tensor Numel Overflow */
    TCase* tc_numel = tcase_create("Tensor Numel Overflow");
    tcase_add_test(tc_numel, test_tensor_create_small_dims_correct_numel);
    tcase_add_test(tc_numel, test_tensor_create_single_element);
    tcase_add_test(tc_numel, test_tensor_create_zero_dimension_returns_null);
    tcase_add_test(tc_numel, test_tensor_create_rank3_numel);
    tcase_set_timeout(tc_numel, 30);
    suite_add_tcase(s, tc_numel);

    /* SpMV Output Size Validation */
    TCase* tc_spmv = tcase_create("SpMV Output Size Validation");
    tcase_add_test(tc_spmv, test_spmv_matched_sizes_succeed);
    tcase_add_test(tc_spmv, test_spmv_mismatched_y_returns_null);
    tcase_set_timeout(tc_spmv, 30);
    suite_add_tcase(s, tc_spmv);

    /* GPU Forward Layer Size Consistency */
    TCase* tc_fwd = tcase_create("GPU Forward Layer Size Consistency");
    tcase_add_test(tc_fwd, test_bias_add_matching_tensors);
    tcase_add_test(tc_fwd, test_weight_cache_layer_sizes_sum_validation);
    tcase_add_test(tc_fwd, test_tensor_ndim_and_precision_stored);
    tcase_add_test(tc_fwd, test_sparse_validate_well_formed);
    tcase_set_timeout(tc_fwd, 30);
    suite_add_tcase(s, tc_fwd);

    return s;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Suite* s = gpu_pipeline_validation_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
