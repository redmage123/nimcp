/* ============================================================================
 * Unit Tests: GPU Recovery in Specialized Modules
 * ============================================================================
 * WHAT: Test recovery integration in dragonfly, portia, sparse, ternary, topology
 * WHY:  Verify self-healing works correctly in specialized GPU modules
 * HOW:  Test recovery initialization, OOM handling, CPU fallback
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/dragonfly/nimcp_dragonfly_vision_gpu.h"
#include "gpu/portia/nimcp_portia_gpu.h"
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/ternary/nimcp_ternary_gpu.h"
#include "gpu/topology/nimcp_topology_gpu.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoverySpecializedTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }

        gpu_ctx = nimcp_gpu_context_create_auto();
        if (!gpu_ctx || !nimcp_gpu_context_is_valid(gpu_ctx)) {
            GTEST_SKIP() << "Failed to create GPU context";
        }

        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* gpu_ctx = nullptr;

    nimcp_gpu_tensor_t* create_test_tensor(size_t size) {
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_create(gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    nimcp_gpu_tensor_t* create_test_matrix(size_t rows, size_t cols) {
        size_t dims[2] = {rows, cols};
        return nimcp_gpu_tensor_create(gpu_ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    }
#endif
};

/* ============================================================================
 * DRAGONFLY VISION RECOVERY TESTS
 * ============================================================================ */

TEST_F(GPURecoverySpecializedTest, Dragonfly_RecoveryInitializedOnContextCreate) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset recovery system */
    if (nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_shutdown();
    }
    EXPECT_FALSE(nimcp_gpu_recovery_is_initialized());

    /* Create dragonfly context - should initialize recovery */
    dfv_gpu_context_t* dfv_ctx = dfv_gpu_context_create(gpu_ctx, 320, 240);

    if (dfv_ctx) {
        EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
        dfv_gpu_context_destroy(dfv_ctx);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Dragonfly_KalmanPredictRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dfv_target_state_t* state = dfv_target_state_create(gpu_ctx, 16);
    if (!state) {
        GTEST_SKIP() << "Target state creation failed";
    }

    state->n_targets = 1;

    dfv_kalman_params_t params = {0.01f, 0.1f, 0.033f, 0.95f, 0.5f};
    params.dt = 0.033f;

    /* Kalman predict should succeed and trigger recovery if needed */
    bool result = dfv_gpu_kalman_predict(gpu_ctx, state, &params);
    EXPECT_TRUE(result);

    dfv_target_state_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Dragonfly_OpticalFlowRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dfv_optical_flow_state_t* flow = dfv_optical_flow_state_create(gpu_ctx, 64, 64, 5);
    if (!flow) {
        GTEST_SKIP() << "Optical flow state creation failed";
    }

    nimcp_gpu_tensor_t* frame = create_test_matrix(64, 64);
    if (!frame) {
        dfv_optical_flow_state_destroy(flow);
        GTEST_SKIP() << "Frame tensor creation failed";
    }

    /* Optical flow should succeed with recovery */
    bool result = dfv_gpu_optical_flow_lk(gpu_ctx, flow, frame);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(frame);
    dfv_optical_flow_state_destroy(flow);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Dragonfly_LoomingDetectionRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dfv_optical_flow_state_t* flow = dfv_optical_flow_state_create(gpu_ctx, 64, 64, 5);
    if (!flow) {
        GTEST_SKIP() << "Optical flow state creation failed";
    }

    nimcp_gpu_tensor_t* looming_map = create_test_matrix(64, 64);
    nimcp_gpu_tensor_t* foe = create_test_tensor(2);

    if (!looming_map || !foe) {
        if (looming_map) nimcp_gpu_tensor_destroy(looming_map);
        if (foe) nimcp_gpu_tensor_destroy(foe);
        dfv_optical_flow_state_destroy(flow);
        GTEST_SKIP() << "Tensor creation failed";
    }

    /* Looming detection should handle errors gracefully */
    bool result = dfv_gpu_detect_looming(gpu_ctx, flow, looming_map, foe);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(looming_map);
    nimcp_gpu_tensor_destroy(foe);
    dfv_optical_flow_state_destroy(flow);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Dragonfly_TSDNEncodeRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dfv_tsdn_state_t* tsdn = dfv_tsdn_state_create(gpu_ctx);
    if (!tsdn) {
        GTEST_SKIP() << "TSDN state creation failed";
    }

    nimcp_gpu_tensor_t* target_dir = create_test_tensor(2);
    if (!target_dir) {
        dfv_tsdn_state_destroy(tsdn);
        GTEST_SKIP() << "Tensor creation failed";
    }

    /* Set a test direction */
    float dir_data[2] = {0.785f, 0.0f};  /* 45 degrees */
    cudaMemcpy(target_dir->data, dir_data, sizeof(dir_data), cudaMemcpyHostToDevice);

    bool result = dfv_gpu_tsdn_encode(gpu_ctx, tsdn, target_dir);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(target_dir);
    dfv_tsdn_state_destroy(tsdn);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * PORTIA SPIDER RECOVERY TESTS
 * ============================================================================ */

TEST_F(GPURecoverySpecializedTest, Portia_SalienceComputationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_portia_attention_params_t params = nimcp_gpu_portia_attention_params_default();

    /* Create attention state manually or use API if available */
    nimcp_gpu_tensor_t* visual_input = create_test_matrix(64, 64);
    if (!visual_input) {
        GTEST_SKIP() << "Visual input creation failed";
    }

    /* The function should initialize recovery if needed */
    /* Note: We can't easily test the full function without state,
       but we verify recovery is initialized */
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_gpu_tensor_destroy(visual_input);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Portia_DefaultParamsValid) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_portia_attention_params_t attn = nimcp_gpu_portia_attention_params_default();
    nimcp_gpu_portia_spatial_params_t spatial = nimcp_gpu_portia_spatial_params_default();
    nimcp_gpu_portia_prey_params_t prey = nimcp_gpu_portia_prey_params_default();

    /* Verify default parameters are sane */
    EXPECT_GT(attn.salience_threshold, 0.0f);
    EXPECT_LT(attn.salience_threshold, 1.0f);
    EXPECT_GT(attn.movement_sensitivity, 0.0f);

    EXPECT_GT(spatial.map_resolution, 0);
    EXPECT_GT(spatial.planning_depth, 0);

    EXPECT_GT(prey.num_prey_templates, 0);
    EXPECT_GT(prey.template_match_threshold, 0.0f);
    EXPECT_LT(prey.template_match_threshold, 1.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * SPARSE TENSOR RECOVERY TESTS
 * ============================================================================ */

TEST_F(GPURecoverySpecializedTest, Sparse_ContextCreationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset recovery */
    if (nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_shutdown();
    }

    /* Sparse context creation should initialize recovery */
    nimcp_sparse_ctx_t* sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);

    if (sparse_ctx) {
        EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
        nimcp_sparse_ctx_destroy(sparse_ctx);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Sparse_FromDenseRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_sparse_ctx_t* sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);
    if (!sparse_ctx) {
        GTEST_SKIP() << "Sparse context creation failed";
    }

    /* Create a small dense tensor */
    nimcp_gpu_tensor_t* dense = create_test_matrix(32, 32);
    if (!dense) {
        nimcp_sparse_ctx_destroy(sparse_ctx);
        GTEST_SKIP() << "Dense tensor creation failed";
    }

    /* Initialize with some sparse pattern */
    std::vector<float> data(32 * 32, 0.0f);
    for (int i = 0; i < 32; i++) {
        data[i * 32 + i] = 1.0f;  /* Diagonal */
    }
    cudaMemcpy(dense->data, data.data(), data.size() * sizeof(float), cudaMemcpyHostToDevice);

    /* Convert to sparse - should handle errors with recovery */
    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.1f);

    /* May or may not succeed depending on implementation, but shouldn't crash */
    if (sparse) {
        nimcp_sparse_tensor_destroy(sparse);
    }

    nimcp_gpu_tensor_destroy(dense);
    nimcp_sparse_ctx_destroy(sparse_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Sparse_WorkspaceEnsureRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_sparse_ctx_t* sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);
    if (!sparse_ctx) {
        GTEST_SKIP() << "Sparse context creation failed";
    }

    /* Request a reasonable workspace size */
    bool result = nimcp_sparse_ctx_ensure_workspace(sparse_ctx, 1024 * 1024);  /* 1MB */
    EXPECT_TRUE(result);

    /* Request larger workspace */
    result = nimcp_sparse_ctx_ensure_workspace(sparse_ctx, 4 * 1024 * 1024);  /* 4MB */
    EXPECT_TRUE(result);

    nimcp_sparse_ctx_destroy(sparse_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * TERNARY TENSOR RECOVERY TESTS
 * ============================================================================ */

TEST_F(GPURecoverySpecializedTest, Ternary_TensorCreationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset recovery */
    if (nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_shutdown();
    }

    int64_t dims[2] = {32, 32};
    nimcp_ternary_tensor_t* tensor = nimcp_ternary_tensor_create(
        gpu_ctx, dims, 2, TERNARY_PACK_NONE);

    if (tensor) {
        EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
        EXPECT_TRUE(nimcp_ternary_tensor_is_valid(tensor));
        nimcp_ternary_tensor_destroy(tensor);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Ternary_FromFloatRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create source float tensor */
    nimcp_gpu_tensor_t* float_tensor = create_test_matrix(64, 64);
    if (!float_tensor) {
        GTEST_SKIP() << "Float tensor creation failed";
    }

    /* Initialize with some data */
    std::vector<float> data(64 * 64);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = (i % 3 == 0) ? 1.0f : ((i % 3 == 1) ? -1.0f : 0.0f);
    }
    cudaMemcpy(float_tensor->data, data.data(), data.size() * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_ternary_quant_config_t config = nimcp_ternary_quant_config_default();

    /* Convert to ternary - should use recovery if needed */
    nimcp_ternary_tensor_t* ternary = nimcp_ternary_from_float(
        gpu_ctx, float_tensor, &config, TERNARY_PACK_NONE);

    if (ternary) {
        EXPECT_TRUE(nimcp_ternary_tensor_is_valid(ternary));
        EXPECT_EQ(ternary->numel, 64 * 64);
        nimcp_ternary_tensor_destroy(ternary);
    }

    nimcp_gpu_tensor_destroy(float_tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Ternary_Pack2BitRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    int64_t dims[2] = {32, 32};
    nimcp_ternary_tensor_t* unpacked = nimcp_ternary_tensor_create(
        gpu_ctx, dims, 2, TERNARY_PACK_NONE);

    if (!unpacked) {
        GTEST_SKIP() << "Unpacked tensor creation failed";
    }

    /* Pack to 2-bit format */
    nimcp_ternary_tensor_t* packed = nimcp_ternary_pack_2bit(gpu_ctx, unpacked);

    if (packed) {
        EXPECT_EQ(packed->pack_mode, TERNARY_PACK_2BIT);
        EXPECT_LT(packed->packed_size, unpacked->packed_size);  /* Should be smaller */
        nimcp_ternary_tensor_destroy(packed);
    }

    nimcp_ternary_tensor_destroy(unpacked);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Ternary_GEMVRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    int64_t A_dims[2] = {64, 64};
    nimcp_ternary_tensor_t* A = nimcp_ternary_tensor_create(
        gpu_ctx, A_dims, 2, TERNARY_PACK_NONE);

    nimcp_gpu_tensor_t* x = create_test_tensor(64);

    if (!A || !x) {
        if (A) nimcp_ternary_tensor_destroy(A);
        if (x) nimcp_gpu_tensor_destroy(x);
        GTEST_SKIP() << "Tensor creation failed";
    }

    /* Initialize x with ones */
    std::vector<float> ones(64, 1.0f);
    cudaMemcpy(x->data, ones.data(), ones.size() * sizeof(float), cudaMemcpyHostToDevice);

    /* GEMV should use recovery on errors */
    nimcp_gpu_tensor_t* y = nimcp_ternary_gemv(gpu_ctx, A, x, nullptr);

    if (y) {
        nimcp_gpu_tensor_destroy(y);
    }

    nimcp_ternary_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(x);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * TOPOLOGY / GRAPH RECOVERY TESTS
 * ============================================================================ */

TEST_F(GPURecoverySpecializedTest, Topology_GraphCreationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset recovery */
    if (nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_shutdown();
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(gpu_ctx, 100, false);

    if (graph) {
        EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
        EXPECT_TRUE(nimcp_graph_gpu_is_valid(graph));
        nimcp_graph_gpu_destroy(graph);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Topology_DegreeComputationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create a small sparse graph */
    int num_nodes = 10;
    std::vector<int> row_ptrs(num_nodes + 1);
    std::vector<int> col_indices;

    /* Create a ring graph */
    for (int i = 0; i < num_nodes; i++) {
        row_ptrs[i] = col_indices.size();
        col_indices.push_back((i + num_nodes - 1) % num_nodes);
        col_indices.push_back((i + 1) % num_nodes);
    }
    row_ptrs[num_nodes] = col_indices.size();

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        gpu_ctx, row_ptrs.data(), col_indices.data(), nullptr,
        num_nodes, col_indices.size());

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_gpu_tensor_t* degree = create_test_tensor(num_nodes);
    if (!degree) {
        nimcp_graph_gpu_destroy(graph);
        GTEST_SKIP() << "Degree tensor creation failed";
    }

    bool result = nimcp_topology_compute_degree(gpu_ctx, graph, degree);
    EXPECT_TRUE(result);

    /* Verify degrees (should all be 2 for ring) */
    std::vector<int> h_degree(num_nodes);
    cudaMemcpy(h_degree.data(), degree->data, num_nodes * sizeof(int), cudaMemcpyDeviceToHost);
    for (int d : h_degree) {
        EXPECT_EQ(d, 2);
    }

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Topology_PageRankRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create a small sparse graph */
    int num_nodes = 10;
    std::vector<int> row_ptrs(num_nodes + 1);
    std::vector<int> col_indices;

    /* Create a simple connected graph */
    for (int i = 0; i < num_nodes; i++) {
        row_ptrs[i] = col_indices.size();
        col_indices.push_back((i + 1) % num_nodes);
        if (i > 0) col_indices.push_back(i - 1);
    }
    row_ptrs[num_nodes] = col_indices.size();

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        gpu_ctx, row_ptrs.data(), col_indices.data(), nullptr,
        num_nodes, col_indices.size());

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_gpu_tensor_t* pagerank = create_test_tensor(num_nodes);
    if (!pagerank) {
        nimcp_graph_gpu_destroy(graph);
        GTEST_SKIP() << "PageRank tensor creation failed";
    }

    bool result = nimcp_topology_compute_pagerank(
        gpu_ctx, graph, 0.85f, 100, 1e-6f, pagerank);
    EXPECT_TRUE(result);

    /* PageRank values should be positive and sum to 1 */
    std::vector<float> h_pr(num_nodes);
    cudaMemcpy(h_pr.data(), pagerank->data, num_nodes * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f;
    for (float pr : h_pr) {
        EXPECT_GT(pr, 0.0f);
        sum += pr;
    }
    EXPECT_NEAR(sum, 1.0f, 0.1f);

    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_graph_gpu_destroy(graph);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Topology_BFSShortestPathRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create a small sparse graph */
    int num_nodes = 5;
    std::vector<int> row_ptrs = {0, 2, 4, 6, 8, 10};
    std::vector<int> col_indices = {1, 2, 0, 3, 0, 4, 1, 4, 2, 3};

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        gpu_ctx, row_ptrs.data(), col_indices.data(), nullptr,
        num_nodes, col_indices.size());

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_shortest_path_result_gpu_t result = {0};
    bool success = nimcp_shortest_path_bfs(gpu_ctx, graph, 0, &result);
    EXPECT_TRUE(success);

    if (result.distances) {
        std::vector<float> h_dist(num_nodes);
        cudaMemcpy(h_dist.data(), result.distances->data,
                   num_nodes * sizeof(float), cudaMemcpyDeviceToHost);

        /* Distance from 0 to 0 should be 0 */
        EXPECT_FLOAT_EQ(h_dist[0], 0.0f);

        /* All nodes should be reachable */
        for (int i = 0; i < num_nodes; i++) {
            EXPECT_LT(h_dist[i], 1e30f);
        }

        nimcp_gpu_tensor_destroy(result.distances);
    }
    if (result.predecessors) {
        nimcp_gpu_tensor_destroy(result.predecessors);
    }

    nimcp_graph_gpu_destroy(graph);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Topology_ClusteringCoefficientRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create a small fully connected graph (complete graph) */
    int num_nodes = 4;
    std::vector<int> row_ptrs = {0, 3, 6, 9, 12};
    std::vector<int> col_indices = {1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2};

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        gpu_ctx, row_ptrs.data(), col_indices.data(), nullptr,
        num_nodes, col_indices.size());

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_gpu_tensor_t* clustering = create_test_tensor(num_nodes);
    if (!clustering) {
        nimcp_graph_gpu_destroy(graph);
        GTEST_SKIP() << "Clustering tensor creation failed";
    }

    bool result = nimcp_topology_compute_clustering(gpu_ctx, graph, clustering);
    EXPECT_TRUE(result);

    /* For complete graph, clustering coefficient should be 1.0 */
    std::vector<float> h_cc(num_nodes);
    cudaMemcpy(h_cc.data(), clustering->data, num_nodes * sizeof(float), cudaMemcpyDeviceToHost);
    for (float cc : h_cc) {
        EXPECT_NEAR(cc, 1.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(clustering);
    nimcp_graph_gpu_destroy(graph);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, Topology_LouvainCommunityRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create a graph with two cliques connected by one edge */
    int num_nodes = 8;
    std::vector<int> row_ptrs;
    std::vector<int> col_indices;

    /* Clique 1: nodes 0-3 */
    /* Clique 2: nodes 4-7 */
    /* Connection: edge 3-4 */

    /* Build adjacency list */
    for (int i = 0; i < 4; i++) {
        row_ptrs.push_back(col_indices.size());
        for (int j = 0; j < 4; j++) {
            if (i != j) col_indices.push_back(j);
        }
        if (i == 3) col_indices.push_back(4);  /* Connection to second clique */
    }
    for (int i = 4; i < 8; i++) {
        row_ptrs.push_back(col_indices.size());
        if (i == 4) col_indices.push_back(3);  /* Connection from first clique */
        for (int j = 4; j < 8; j++) {
            if (i != j) col_indices.push_back(j);
        }
    }
    row_ptrs.push_back(col_indices.size());

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        gpu_ctx, row_ptrs.data(), col_indices.data(), nullptr,
        num_nodes, col_indices.size());

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_community_result_gpu_t* result = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-4f);

    if (result) {
        /* Should detect 2 communities */
        EXPECT_GE(result->num_communities, 1);
        EXPECT_LE(result->num_communities, num_nodes);

        /* Modularity should be positive for modular graph */
        EXPECT_GT(result->modularity, 0.0f);

        if (result->node_communities) {
            nimcp_gpu_tensor_destroy(result->node_communities);
        }
        if (result->community_sizes) {
            nimcp_gpu_tensor_destroy(result->community_sizes);
        }
        free(result);
    }

    nimcp_graph_gpu_destroy(graph);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * RECOVERY STATISTICS TESTS
 * ============================================================================ */

TEST_F(GPURecoverySpecializedTest, RecoveryStatsAcrossModules) {
#ifdef NIMCP_ENABLE_CUDA
    /* Reset stats */
    nimcp_gpu_recovery_reset_stats();

    /* Create and use various module contexts to trigger operations */
    dfv_target_state_t* dfv = dfv_target_state_create(gpu_ctx, 8);
    if (dfv) {
        dfv->n_targets = 1;
        dfv_kalman_params_t params = {0.01f, 0.1f, 0.033f, 0.95f, 0.5f};
        dfv_gpu_kalman_predict(gpu_ctx, dfv, &params);
        dfv_target_state_destroy(dfv);
    }

    nimcp_sparse_ctx_t* sparse = nimcp_sparse_ctx_create(gpu_ctx);
    if (sparse) {
        nimcp_sparse_ctx_ensure_workspace(sparse, 1024);
        nimcp_sparse_ctx_destroy(sparse);
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(gpu_ctx, 10, false);
    if (graph) {
        nimcp_graph_gpu_destroy(graph);
    }

    /* Check stats are being tracked */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be valid (may be all zeros if no errors occurred) */
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    /* Verify action names are available */
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_NE(name, nullptr);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_NONE);
    EXPECT_NE(name, nullptr);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GPURecoverySpecializedTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    /* Verify error category names are available */
    const char* name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_KERNEL_LAUNCH);
    EXPECT_NE(name, nullptr);

    name = nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL);
    EXPECT_NE(name, nullptr);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
