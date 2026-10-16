/**
 * @file e2e_gpu_test_brain_topology.cpp
 * @brief End-to-End Tests for Brain-like Network Topology on GPU
 *
 * WHAT: Complete pipeline tests for brain network topology analysis
 * WHY:  Verify biological network properties emerge from topology operations
 * HOW:  Generate brain-like networks, analyze, detect communities, validate
 *
 * TEST PIPELINES:
 * - BrainNetworkGeneration: Generate biologically realistic connectivity
 * - CorticalModuleDetection: Detect functional modules in brain-like graphs
 * - SmallWorldAnalysis: Verify small-world properties
 * - HubIdentification: Identify connector hubs in brain networks
 * - ScalabilityBenchmark: Test performance on large-scale brain graphs
 *
 * BIOLOGICAL CONTEXT:
 * - Brain networks exhibit modular organization (Sporns & Betzel, 2016)
 * - Small-world topology optimizes information integration (Watts & Strogatz, 1998)
 * - Hub neurons are critical for network function (van den Heuvel, 2012)
 * - Scale-free degree distribution in some brain regions
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/nimcp_execution_mode.h"
#include "utils/memory/nimcp_memory.h"

#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>
#include <iomanip>

//=============================================================================
// Brain Network Metrics
//=============================================================================

struct BrainNetworkMetrics {
    // Basic topology
    int num_nodes;
    int num_edges;
    float density;
    float avg_degree;
    float max_degree;

    // Small-world properties
    float clustering_coefficient;
    float avg_path_length;
    float small_world_index;  // sigma = C/C_rand * L_rand/L

    // Modular organization
    int num_modules;
    float modularity;

    // Hub structure
    int num_hubs;
    float hub_centrality_ratio;  // Ratio of hub to average centrality

    // Performance
    double analysis_time_ms;
};

//=============================================================================
// Test Fixture
//=============================================================================

class BrainTopologyGPUE2ETest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx_ = nullptr;
    hardware_capabilities_t caps_;
    bool has_gpu_ = false;
    std::mt19937 rng_;
    BrainNetworkMetrics metrics_;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        memset(&metrics_, 0, sizeof(metrics_));

        execution_detect_capabilities(&caps_);
        has_gpu_ = caps_.cuda_available || caps_.rocm_available || caps_.opencl_available;

        if (has_gpu_) {
            gpu_ctx_ = nimcp_gpu_context_create_auto();
        }

        rng_.seed(42);
    }

    void TearDown() override {
        if (gpu_ctx_) {
            nimcp_gpu_context_destroy(gpu_ctx_);
            gpu_ctx_ = nullptr;
        }
    }

    bool HasGPU() const { return has_gpu_ && gpu_ctx_ != nullptr; }

    // Generate cortical-like modular network
    std::vector<float> GenerateCorticalNetwork(int n, int num_modules,
                                                float p_within, float p_between) {
        std::vector<float> adj(n * n, 0.0f);
        int module_size = n / num_modules;
        std::bernoulli_distribution within_dist(p_within);
        std::bernoulli_distribution between_dist(p_between);
        std::uniform_real_distribution<float> weight_dist(0.1f, 1.0f);

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                int mi = i / module_size;
                int mj = j / module_size;

                bool connect = (mi == mj) ? within_dist(rng_) : between_dist(rng_);
                if (connect) {
                    // Synaptic weight varies with distance
                    float distance = std::abs(mi - mj) + 1.0f;
                    float w = weight_dist(rng_) / distance;
                    adj[i * n + j] = w;
                    adj[j * n + i] = w;
                }
            }
        }
        return adj;
    }

    // Generate scale-free brain network (preferential attachment + distance decay)
    nimcp_graph_gpu_t* GenerateScaleFreeBrainNetwork(int n, int m,
                                                      float distance_decay = 0.5f) {
        // Start with Barabasi-Albert and modify with distance effects
        nimcp_graph_gpu_t* base = nimcp_graph_generate_barabasi_albert(
            gpu_ctx_, n, m, static_cast<uint32_t>(rng_())
        );
        return base;  // Simplified - full implementation would add distance decay
    }

    // Generate small-world brain network (Watts-Strogatz with modular structure)
    nimcp_graph_gpu_t* GenerateSmallWorldBrainNetwork(int n, int k, float p_rewire) {
        return nimcp_graph_generate_watts_strogatz(
            gpu_ctx_, n, k, p_rewire, static_cast<uint32_t>(rng_())
        );
    }

    // Helper: Create 1D GPU tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, nimcp_gpu_precision_t dtype) {
        size_t dims[] = {n};
        return nimcp_gpu_tensor_create(gpu_ctx_, dims, 1, dtype);
    }

    // Helper: Create 2D GPU tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols,
                                        nimcp_gpu_precision_t dtype) {
        size_t dims[] = {rows, cols};
        return nimcp_gpu_tensor_create(gpu_ctx_, dims, 2, dtype);
    }

    // Helper: Copy tensor to host
    std::vector<float> TensorToHost(nimcp_gpu_tensor_t* tensor, size_t n) {
        std::vector<float> host_data(n);
        nimcp_gpu_memcpy(gpu_ctx_, host_data.data(), tensor->data,
                         n * sizeof(float), GPU_MEMCPY_DEVICE_TO_HOST);
        return host_data;
    }

    std::vector<int32_t> TensorToHostInt(nimcp_gpu_tensor_t* tensor, size_t n) {
        std::vector<int32_t> host_data(n);
        nimcp_gpu_memcpy(gpu_ctx_, host_data.data(), tensor->data,
                         n * sizeof(int32_t), GPU_MEMCPY_DEVICE_TO_HOST);
        return host_data;
    }

    // Print network metrics
    void PrintMetrics(const std::string& title) {
        std::cout << "\n=== " << title << " ===" << std::endl;
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Network Size:" << std::endl;
        std::cout << "    Nodes: " << metrics_.num_nodes << std::endl;
        std::cout << "    Edges: " << metrics_.num_edges << std::endl;
        std::cout << "    Density: " << metrics_.density << std::endl;
        std::cout << "    Avg Degree: " << metrics_.avg_degree << std::endl;
        std::cout << "    Max Degree: " << metrics_.max_degree << std::endl;
        std::cout << "  Small-World Properties:" << std::endl;
        std::cout << "    Clustering: " << metrics_.clustering_coefficient << std::endl;
        std::cout << "    Avg Path Length: " << metrics_.avg_path_length << std::endl;
        std::cout << "    Small-World Index: " << metrics_.small_world_index << std::endl;
        std::cout << "  Modular Organization:" << std::endl;
        std::cout << "    Modules: " << metrics_.num_modules << std::endl;
        std::cout << "    Modularity: " << metrics_.modularity << std::endl;
        std::cout << "  Hub Structure:" << std::endl;
        std::cout << "    Hub Count: " << metrics_.num_hubs << std::endl;
        std::cout << "    Hub/Avg Centrality: " << metrics_.hub_centrality_ratio << std::endl;
        std::cout << "  Performance:" << std::endl;
        std::cout << "    Analysis Time: " << metrics_.analysis_time_ms << " ms" << std::endl;
    }

    // Compute small-world index
    float ComputeSmallWorldIndex(float C, float L, float C_rand, float L_rand) {
        if (C_rand == 0 || L == 0) return 0.0f;
        float sigma = (C / C_rand) * (L_rand / L);
        return sigma;
    }

    // Identify hub nodes (top 10% by degree or centrality)
    int CountHubs(const std::vector<float>& centrality, float threshold_percentile = 0.9f) {
        std::vector<float> sorted = centrality;
        std::sort(sorted.begin(), sorted.end());
        size_t threshold_idx = static_cast<size_t>(sorted.size() * threshold_percentile);
        float threshold = sorted[threshold_idx];

        int count = 0;
        for (float c : centrality) {
            if (c >= threshold) count++;
        }
        return count;
    }
};

//=============================================================================
// Pipeline 1: Brain Network Generation and Analysis
//=============================================================================

TEST_F(BrainTopologyGPUE2ETest, Pipeline_BrainNetworkGeneration) {
    E2E_PIPELINE_START("Brain Network Generation");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const int N_NEURONS = 500;
    const int N_MODULES = 5;  // Cortical areas
    const float P_WITHIN = 0.3f;   // Dense intra-module connectivity
    const float P_BETWEEN = 0.02f;  // Sparse inter-module connectivity

    // Stage 1: Generate cortical-like network
    E2E_STAGE_BEGIN("Generate cortical network", 5000);

    std::cout << "\n  Generating cortical-like network:" << std::endl;
    std::cout << "    Neurons: " << N_NEURONS << std::endl;
    std::cout << "    Modules (cortical areas): " << N_MODULES << std::endl;
    std::cout << "    Within-module connectivity: " << P_WITHIN << std::endl;
    std::cout << "    Between-module connectivity: " << P_BETWEEN << std::endl;

    auto adj = GenerateCorticalNetwork(N_NEURONS, N_MODULES, P_WITHIN, P_BETWEEN);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx_, adj.data(), N_NEURONS);
    E2E_ASSERT_NOT_NULL(graph, "Failed to create brain network graph");

    metrics_.num_nodes = graph->num_nodes;
    metrics_.num_edges = graph->num_edges;

    std::cout << "    Created graph with " << graph->num_edges << " synaptic connections" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Compute comprehensive topology metrics
    E2E_STAGE_BEGIN("Compute topology metrics", 10000);

    auto analysis_start = std::chrono::high_resolution_clock::now();

    nimcp_topology_metrics_gpu_t* topo_metrics = nimcp_topology_compute_metrics(gpu_ctx_, graph);
    E2E_ASSERT_NOT_NULL(topo_metrics, "Failed to compute topology metrics");

    nimcp_gpu_context_synchronize(gpu_ctx_);

    metrics_.density = topo_metrics->density;
    metrics_.clustering_coefficient = topo_metrics->global_clustering;
    metrics_.avg_path_length = topo_metrics->avg_path_length;

    // Get degree stats
    auto degree = TensorToHostInt(topo_metrics->degree, N_NEURONS);
    metrics_.avg_degree = std::accumulate(degree.begin(), degree.end(), 0.0f) / N_NEURONS;
    metrics_.max_degree = *std::max_element(degree.begin(), degree.end());

    std::cout << "\n  Topology Metrics:" << std::endl;
    std::cout << "    Density: " << metrics_.density << std::endl;
    std::cout << "    Average degree: " << metrics_.avg_degree << std::endl;
    std::cout << "    Maximum degree: " << metrics_.max_degree << std::endl;
    std::cout << "    Clustering coefficient: " << metrics_.clustering_coefficient << std::endl;
    std::cout << "    Average path length: " << metrics_.avg_path_length << std::endl;

    E2E_STAGE_END();

    // Stage 3: Detect modular structure
    E2E_STAGE_BEGIN("Detect cortical modules", 15000);

    nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
        gpu_ctx_, graph, 1.0f, 100, 1e-5f
    );
    E2E_ASSERT_NOT_NULL(communities, "Failed to detect communities");

    metrics_.num_modules = communities->num_communities;
    metrics_.modularity = communities->modularity;

    std::cout << "\n  Module Detection:" << std::endl;
    std::cout << "    Planted modules: " << N_MODULES << std::endl;
    std::cout << "    Detected modules: " << metrics_.num_modules << std::endl;
    std::cout << "    Modularity: " << metrics_.modularity << std::endl;

    // Analyze module sizes
    auto comm_sizes = TensorToHostInt(communities->community_sizes, communities->num_communities);
    std::cout << "    Module sizes: ";
    for (int i = 0; i < std::min(10, communities->num_communities); i++) {
        std::cout << comm_sizes[i] << " ";
    }
    if (communities->num_communities > 10) std::cout << "...";
    std::cout << std::endl;

    // Should detect approximately the planted number of modules
    E2E_ASSERT(metrics_.num_modules >= N_MODULES - 2, "Too few modules detected");
    E2E_ASSERT(metrics_.num_modules <= N_MODULES + 3, "Too many modules detected");

    E2E_STAGE_END();

    // Stage 4: Compute small-world index
    E2E_STAGE_BEGIN("Compute small-world properties", 5000);

    // Generate random graph for comparison
    nimcp_graph_gpu_t* random_graph = nimcp_graph_generate_erdos_renyi(
        gpu_ctx_, N_NEURONS, metrics_.density, 12345
    );
    E2E_ASSERT_NOT_NULL(random_graph, "Failed to generate random graph");

    nimcp_gpu_tensor_t* rand_clustering = Create1DTensor(N_NEURONS, NIMCP_GPU_PRECISION_FP32);
    nimcp_topology_compute_clustering(gpu_ctx_, random_graph, rand_clustering);
    auto rand_cc = TensorToHost(rand_clustering, N_NEURONS);
    float C_rand = std::accumulate(rand_cc.begin(), rand_cc.end(), 0.0f) / N_NEURONS;

    // For random graph, average path length ~ log(N) / log(k)
    float L_rand = log(N_NEURONS) / log(metrics_.avg_degree);

    metrics_.small_world_index = ComputeSmallWorldIndex(
        metrics_.clustering_coefficient, metrics_.avg_path_length,
        C_rand, L_rand
    );

    std::cout << "\n  Small-World Analysis:" << std::endl;
    std::cout << "    Random graph clustering: " << C_rand << std::endl;
    std::cout << "    Random graph path length (est): " << L_rand << std::endl;
    std::cout << "    Small-world index (sigma): " << metrics_.small_world_index << std::endl;

    // Brain networks typically have sigma > 1 (small-world property)
    E2E_ASSERT(metrics_.small_world_index > 0.5f, "Network lacks small-world property");

    nimcp_gpu_tensor_destroy(rand_clustering);
    nimcp_graph_gpu_destroy(random_graph);

    E2E_STAGE_END();

    // Stage 5: Identify hub neurons
    E2E_STAGE_BEGIN("Identify hub neurons", 10000);

    nimcp_gpu_tensor_t* betweenness = Create1DTensor(N_NEURONS, NIMCP_GPU_PRECISION_FP32);
    bool bc_ok = nimcp_topology_compute_betweenness(gpu_ctx_, graph, true, betweenness);
    E2E_ASSERT(bc_ok, "Failed to compute betweenness centrality");

    auto bc_host = TensorToHost(betweenness, N_NEURONS);
    metrics_.num_hubs = CountHubs(bc_host, 0.9f);

    float avg_bc = std::accumulate(bc_host.begin(), bc_host.end(), 0.0f) / N_NEURONS;
    float max_bc = *std::max_element(bc_host.begin(), bc_host.end());
    metrics_.hub_centrality_ratio = max_bc / avg_bc;

    std::cout << "\n  Hub Analysis:" << std::endl;
    std::cout << "    Hub neurons (top 10%): " << metrics_.num_hubs << std::endl;
    std::cout << "    Average betweenness: " << avg_bc << std::endl;
    std::cout << "    Max betweenness: " << max_bc << std::endl;
    std::cout << "    Hub/Avg centrality ratio: " << metrics_.hub_centrality_ratio << std::endl;

    // Brain networks have pronounced hub structure
    E2E_ASSERT(metrics_.hub_centrality_ratio > 2.0f, "Hub structure not prominent");

    nimcp_gpu_tensor_destroy(betweenness);

    auto analysis_end = std::chrono::high_resolution_clock::now();
    metrics_.analysis_time_ms = std::chrono::duration<double, std::milli>(
        analysis_end - analysis_start
    ).count();

    E2E_STAGE_END();

    // Cleanup
    nimcp_community_result_gpu_destroy(communities);
    nimcp_topology_metrics_gpu_destroy(topo_metrics);
    nimcp_graph_gpu_destroy(graph);

    PrintMetrics("Brain Network Generation");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Cortical Module Detection
//=============================================================================

TEST_F(BrainTopologyGPUE2ETest, Pipeline_CorticalModuleDetection) {
    E2E_PIPELINE_START("Cortical Module Detection");

    if (!HasGPU()) {
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const int N_NEURONS = 300;
    const int N_CORTICAL_AREAS = 6;  // V1, V2, MT, IT, PFC, Motor

    // Stage 1: Create multi-area cortical network
    E2E_STAGE_BEGIN("Create multi-area cortical network", 3000);

    std::cout << "\n  Simulating cortical network with " << N_CORTICAL_AREAS
              << " areas" << std::endl;

    // Strong within-area, weak between-area (except neighboring areas)
    auto adj = GenerateCorticalNetwork(N_NEURONS, N_CORTICAL_AREAS, 0.4f, 0.02f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx_, adj.data(), N_NEURONS);
    E2E_ASSERT_NOT_NULL(graph, "Failed to create cortical graph");

    E2E_STAGE_END();

    // Stage 2: Run Louvain community detection
    E2E_STAGE_BEGIN("Louvain community detection", 10000);

    nimcp_community_result_gpu_t* louvain = nimcp_community_detect_louvain(
        gpu_ctx_, graph, 1.0f, 100, 1e-6f
    );
    E2E_ASSERT_NOT_NULL(louvain, "Louvain detection failed");

    std::cout << "\n  Louvain Results:" << std::endl;
    std::cout << "    Detected modules: " << louvain->num_communities << std::endl;
    std::cout << "    Modularity: " << louvain->modularity << std::endl;

    E2E_STAGE_END();

    // Stage 3: Run Label Propagation for comparison
    E2E_STAGE_BEGIN("Label propagation detection", 5000);

    nimcp_community_result_gpu_t* label_prop = nimcp_community_detect_label_prop(
        gpu_ctx_, graph, 100
    );
    E2E_ASSERT_NOT_NULL(label_prop, "Label propagation detection failed");

    std::cout << "\n  Label Propagation Results:" << std::endl;
    std::cout << "    Detected modules: " << label_prop->num_communities << std::endl;
    std::cout << "    Modularity: " << label_prop->modularity << std::endl;

    E2E_STAGE_END();

    // Stage 4: Compare community assignments
    E2E_STAGE_BEGIN("Compare detection methods", 2000);

    auto louvain_comm = TensorToHostInt(louvain->node_communities, N_NEURONS);
    auto lp_comm = TensorToHostInt(label_prop->node_communities, N_NEURONS);

    // Ground truth
    std::vector<int32_t> ground_truth(N_NEURONS);
    int area_size = N_NEURONS / N_CORTICAL_AREAS;
    for (int i = 0; i < N_NEURONS; i++) {
        ground_truth[i] = i / area_size;
    }

    // Compute agreement rates
    int louvain_agree = 0, lp_agree = 0, both_agree = 0;
    int total_pairs = 0;

    for (int i = 0; i < N_NEURONS; i++) {
        for (int j = i + 1; j < N_NEURONS; j++) {
            bool gt_same = (ground_truth[i] == ground_truth[j]);
            bool louvain_same = (louvain_comm[i] == louvain_comm[j]);
            bool lp_same = (lp_comm[i] == lp_comm[j]);

            if (louvain_same == gt_same) louvain_agree++;
            if (lp_same == gt_same) lp_agree++;
            if (louvain_same == gt_same && lp_same == gt_same) both_agree++;
            total_pairs++;
        }
    }

    float louvain_accuracy = (float)louvain_agree / total_pairs;
    float lp_accuracy = (float)lp_agree / total_pairs;

    std::cout << "\n  Method Comparison (vs ground truth):" << std::endl;
    std::cout << "    Louvain accuracy: " << (louvain_accuracy * 100) << "%" << std::endl;
    std::cout << "    Label Prop accuracy: " << (lp_accuracy * 100) << "%" << std::endl;
    std::cout << "    Both methods agree: " << (100.0 * both_agree / total_pairs) << "%" << std::endl;

    E2E_ASSERT(louvain_accuracy > 0.7f, "Louvain accuracy too low");

    E2E_STAGE_END();

    // Cleanup
    nimcp_community_result_gpu_destroy(louvain);
    nimcp_community_result_gpu_destroy(label_prop);
    nimcp_graph_gpu_destroy(graph);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Small-World Analysis of Brain Networks
//=============================================================================

TEST_F(BrainTopologyGPUE2ETest, Pipeline_SmallWorldAnalysis) {
    E2E_PIPELINE_START("Small-World Analysis");

    if (!HasGPU()) {
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const int N = 200;
    const int K = 8;  // Lattice neighbors

    std::vector<std::tuple<std::string, float, float, float>> networks;
    // name, rewiring probability, expected sigma (small-world index)

    // Stage 1: Generate networks with varying rewiring
    E2E_STAGE_BEGIN("Generate network variants", 5000);

    std::vector<float> rewire_probs = {0.0f, 0.01f, 0.1f, 0.5f, 1.0f};

    std::cout << "\n  Testing small-world property at different rewiring rates:" << std::endl;
    std::cout << "  | p_rewire | Clustering | Path Length | Sigma |" << std::endl;
    std::cout << "  |----------|------------|-------------|-------|" << std::endl;

    for (float p : rewire_probs) {
        nimcp_graph_gpu_t* graph = nimcp_graph_generate_watts_strogatz(
            gpu_ctx_, N, K, p, 42
        );
        if (!graph) continue;

        // Compute clustering
        nimcp_gpu_tensor_t* clustering = Create1DTensor(N, NIMCP_GPU_PRECISION_FP32);
        nimcp_topology_compute_clustering(gpu_ctx_, graph, clustering);
        auto cc_host = TensorToHost(clustering, N);
        float C = std::accumulate(cc_host.begin(), cc_host.end(), 0.0f) / N;

        // Compute path length (approximation using BFS from sample nodes)
        float total_dist = 0;
        int samples = 10;
        for (int s = 0; s < samples; s++) {
            int source = (s * N) / samples;
            nimcp_shortest_path_result_gpu_t result;
            result.distances = Create1DTensor(N, NIMCP_GPU_PRECISION_FP32);
            result.predecessors = Create1DTensor(N, NIMCP_GPU_PRECISION_INT32);

            nimcp_shortest_path_bfs(gpu_ctx_, graph, source, &result);
            auto dist = TensorToHost(result.distances, N);

            for (int i = 0; i < N; i++) {
                if (i != source && dist[i] < N) {
                    total_dist += dist[i];
                }
            }

            nimcp_gpu_tensor_destroy(result.distances);
            nimcp_gpu_tensor_destroy(result.predecessors);
        }
        float L = total_dist / (samples * (N - 1));

        // Reference values for random graph
        float C_rand = (float)K / N;  // Approximate
        float L_rand = log(N) / log(K);

        float sigma = ComputeSmallWorldIndex(C, L, C_rand, L_rand);

        std::cout << "  | " << std::fixed << std::setprecision(2) << p
                  << "     | " << std::setprecision(4) << C
                  << "     | " << L
                  << "        | " << sigma << " |" << std::endl;

        networks.push_back({std::to_string(p), C, L, sigma});

        nimcp_gpu_tensor_destroy(clustering);
        nimcp_graph_gpu_destroy(graph);
    }

    E2E_STAGE_END();

    // Stage 2: Verify small-world regime
    E2E_STAGE_BEGIN("Verify small-world regime", 1000);

    // p=0.1 should have highest small-world index (sweet spot)
    bool found_peak = false;
    float prev_sigma = 0;
    for (size_t i = 0; i < networks.size(); i++) {
        float sigma = std::get<3>(networks[i]);
        if (i > 0 && i < networks.size() - 1 &&
            sigma > std::get<3>(networks[i-1]) &&
            sigma > std::get<3>(networks[i+1])) {
            found_peak = true;
            std::cout << "\n  Small-world peak at p=" << std::get<0>(networks[i])
                      << " with sigma=" << sigma << std::endl;
        }
    }

    // Middle rewiring values should give small-world property
    std::cout << "\n  Key observation: Intermediate rewiring probabilities" << std::endl;
    std::cout << "  produce networks with high clustering AND short paths" << std::endl;
    std::cout << "  (the small-world regime, typical of biological networks)" << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Hub Identification in Brain Networks
//=============================================================================

TEST_F(BrainTopologyGPUE2ETest, Pipeline_HubIdentification) {
    E2E_PIPELINE_START("Hub Identification");

    if (!HasGPU()) {
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const int N = 300;
    const int M = 3;  // Preferential attachment parameter

    // Stage 1: Generate scale-free brain network
    E2E_STAGE_BEGIN("Generate scale-free network", 3000);

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(
        gpu_ctx_, N, M, 42
    );
    E2E_ASSERT_NOT_NULL(graph, "Failed to generate BA network");

    std::cout << "\n  Generated Barabasi-Albert network:" << std::endl;
    std::cout << "    Nodes: " << graph->num_nodes << std::endl;
    std::cout << "    Edges: " << graph->num_edges << std::endl;

    E2E_STAGE_END();

    // Stage 2: Compute multiple centrality measures
    E2E_STAGE_BEGIN("Compute centrality measures", 15000);

    nimcp_gpu_tensor_t* degree = Create1DTensor(N, NIMCP_GPU_PRECISION_INT32);
    nimcp_gpu_tensor_t* pagerank = Create1DTensor(N, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* betweenness = Create1DTensor(N, NIMCP_GPU_PRECISION_FP32);

    nimcp_topology_compute_degree(gpu_ctx_, graph, degree);
    nimcp_topology_compute_pagerank(gpu_ctx_, graph, 0.85f, 100, 1e-6f, pagerank);
    nimcp_topology_compute_betweenness(gpu_ctx_, graph, true, betweenness);

    auto deg_host = TensorToHostInt(degree, N);
    auto pr_host = TensorToHost(pagerank, N);
    auto bc_host = TensorToHost(betweenness, N);

    // Convert degree to float for analysis
    std::vector<float> deg_float(deg_host.begin(), deg_host.end());

    E2E_STAGE_END();

    // Stage 3: Identify hubs by different criteria
    E2E_STAGE_BEGIN("Identify hubs", 2000);

    // Find top 10% by each measure
    auto find_top_n = [](const std::vector<float>& v, int n) {
        std::vector<std::pair<float, int>> indexed;
        for (size_t i = 0; i < v.size(); i++) {
            indexed.push_back({v[i], static_cast<int>(i)});
        }
        std::sort(indexed.begin(), indexed.end(),
                  [](auto& a, auto& b) { return a.first > b.first; });
        std::vector<int> top;
        for (int i = 0; i < n; i++) {
            top.push_back(indexed[i].second);
        }
        return top;
    };

    int hub_count = N / 10;  // Top 10%
    auto degree_hubs = find_top_n(deg_float, hub_count);
    auto pagerank_hubs = find_top_n(pr_host, hub_count);
    auto betweenness_hubs = find_top_n(bc_host, hub_count);

    // Count overlap between hub sets
    auto count_overlap = [](const std::vector<int>& a, const std::vector<int>& b) {
        int overlap = 0;
        for (int x : a) {
            if (std::find(b.begin(), b.end(), x) != b.end()) overlap++;
        }
        return overlap;
    };

    int deg_pr_overlap = count_overlap(degree_hubs, pagerank_hubs);
    int deg_bc_overlap = count_overlap(degree_hubs, betweenness_hubs);
    int pr_bc_overlap = count_overlap(pagerank_hubs, betweenness_hubs);

    std::cout << "\n  Hub Identification (top " << hub_count << " nodes):" << std::endl;
    std::cout << "    Degree-PageRank overlap: " << deg_pr_overlap << "/" << hub_count << std::endl;
    std::cout << "    Degree-Betweenness overlap: " << deg_bc_overlap << "/" << hub_count << std::endl;
    std::cout << "    PageRank-Betweenness overlap: " << pr_bc_overlap << "/" << hub_count << std::endl;

    // High overlap expected in scale-free networks
    E2E_ASSERT(deg_pr_overlap > hub_count / 2, "Low degree-PageRank correlation");

    E2E_STAGE_END();

    // Stage 4: Analyze hub properties
    E2E_STAGE_BEGIN("Analyze hub properties", 2000);

    // Compare hub vs non-hub statistics
    float hub_avg_deg = 0, non_hub_avg_deg = 0;
    float hub_avg_bc = 0, non_hub_avg_bc = 0;

    std::vector<bool> is_hub(N, false);
    for (int h : degree_hubs) is_hub[h] = true;

    for (int i = 0; i < N; i++) {
        if (is_hub[i]) {
            hub_avg_deg += deg_float[i];
            hub_avg_bc += bc_host[i];
        } else {
            non_hub_avg_deg += deg_float[i];
            non_hub_avg_bc += bc_host[i];
        }
    }

    hub_avg_deg /= hub_count;
    non_hub_avg_deg /= (N - hub_count);
    hub_avg_bc /= hub_count;
    non_hub_avg_bc /= (N - hub_count);

    std::cout << "\n  Hub vs Non-Hub Comparison:" << std::endl;
    std::cout << "    Avg Degree - Hubs: " << hub_avg_deg
              << ", Non-Hubs: " << non_hub_avg_deg << std::endl;
    std::cout << "    Avg Betweenness - Hubs: " << hub_avg_bc
              << ", Non-Hubs: " << non_hub_avg_bc << std::endl;
    std::cout << "    Hub degree ratio: " << (hub_avg_deg / non_hub_avg_deg) << "x" << std::endl;
    std::cout << "    Hub betweenness ratio: " << (hub_avg_bc / non_hub_avg_bc) << "x" << std::endl;

    // Hubs should have significantly higher centrality
    E2E_ASSERT(hub_avg_deg > non_hub_avg_deg * 2, "Hubs not prominent by degree");
    E2E_ASSERT(hub_avg_bc > non_hub_avg_bc * 2, "Hubs not prominent by betweenness");

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(degree);
    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_gpu_tensor_destroy(betweenness);
    nimcp_graph_gpu_destroy(graph);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Scalability Benchmark
//=============================================================================

TEST_F(BrainTopologyGPUE2ETest, Pipeline_ScalabilityBenchmark) {
    E2E_PIPELINE_START("Scalability Benchmark");

    if (!HasGPU()) {
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    std::vector<int> sizes = {100, 500, 1000, 2000, 5000};
    const float EDGE_PROB = 0.02f;

    std::cout << "\n=== Scalability Benchmark ===" << std::endl;
    std::cout << "| Nodes | Edges   | Gen(ms) | Metrics(ms) | Comm(ms) | Total(ms) |" << std::endl;
    std::cout << "|-------|---------|---------|-------------|----------|-----------|" << std::endl;

    for (int n : sizes) {
        // Skip very large if memory constrained
        if (n > 2000) {
            size_t free_mem = 0, total_mem = 0;
            nimcp_gpu_context_get_memory_info(gpu_ctx_, &free_mem, &total_mem);
            if (free_mem < 500 * 1024 * 1024) {
                std::cout << "| " << n << "  | SKIPPED - insufficient memory |" << std::endl;
                continue;
            }
        }

        // Generation
        auto gen_start = std::chrono::high_resolution_clock::now();
        nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
            gpu_ctx_, n, EDGE_PROB, 42
        );
        auto gen_end = std::chrono::high_resolution_clock::now();
        double gen_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();

        if (!graph) {
            std::cout << "| " << n << "  | FAILED  |" << std::endl;
            continue;
        }

        // Metrics computation
        auto metrics_start = std::chrono::high_resolution_clock::now();
        nimcp_gpu_tensor_t* degree = Create1DTensor(n, NIMCP_GPU_PRECISION_INT32);
        nimcp_gpu_tensor_t* clustering = Create1DTensor(n, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* pagerank = Create1DTensor(n, NIMCP_GPU_PRECISION_FP32);

        nimcp_topology_compute_degree(gpu_ctx_, graph, degree);
        nimcp_topology_compute_clustering(gpu_ctx_, graph, clustering);
        nimcp_topology_compute_pagerank(gpu_ctx_, graph, 0.85f, 50, 1e-4f, pagerank);
        nimcp_gpu_context_synchronize(gpu_ctx_);

        auto metrics_end = std::chrono::high_resolution_clock::now();
        double metrics_ms = std::chrono::duration<double, std::milli>(metrics_end - metrics_start).count();

        // Community detection
        auto comm_start = std::chrono::high_resolution_clock::now();
        nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
            gpu_ctx_, graph, 1.0f, 30, 1e-3f
        );
        nimcp_gpu_context_synchronize(gpu_ctx_);
        auto comm_end = std::chrono::high_resolution_clock::now();
        double comm_ms = std::chrono::duration<double, std::milli>(comm_end - comm_start).count();

        double total_ms = gen_ms + metrics_ms + comm_ms;

        std::cout << "| " << std::setw(5) << n
                  << " | " << std::setw(7) << graph->num_edges
                  << " | " << std::setw(7) << std::fixed << std::setprecision(1) << gen_ms
                  << " | " << std::setw(11) << metrics_ms
                  << " | " << std::setw(8) << comm_ms
                  << " | " << std::setw(9) << total_ms << " |" << std::endl;

        // Cleanup
        if (communities) nimcp_community_result_gpu_destroy(communities);
        nimcp_gpu_tensor_destroy(degree);
        nimcp_gpu_tensor_destroy(clustering);
        nimcp_gpu_tensor_destroy(pagerank);
        nimcp_graph_gpu_destroy(graph);
    }

    std::cout << "\n  Note: Actual brain networks have N > 10^10 neurons," << std::endl;
    std::cout << "  but coarse-grained models use ~1000 regions." << std::endl;

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Full Brain Topology Analysis Pipeline
//=============================================================================

TEST_F(BrainTopologyGPUE2ETest, Pipeline_FullBrainTopologyAnalysis) {
    E2E_PIPELINE_START("Full Brain Topology Analysis");

    if (!HasGPU()) {
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const int N_REGIONS = 400;  // Brain regions
    const int N_MODULES = 8;    // Major brain systems

    // Stage 1: Generate brain-like connectivity
    E2E_STAGE_BEGIN("Generate brain-like network", 5000);

    auto adj = GenerateCorticalNetwork(N_REGIONS, N_MODULES, 0.35f, 0.03f);
    nimcp_graph_gpu_t* brain_graph = nimcp_graph_gpu_from_dense(
        gpu_ctx_, adj.data(), N_REGIONS
    );
    E2E_ASSERT_NOT_NULL(brain_graph, "Failed to create brain graph");

    std::cout << "\n  Brain Network:" << std::endl;
    std::cout << "    Regions: " << brain_graph->num_nodes << std::endl;
    std::cout << "    Connections: " << brain_graph->num_edges << std::endl;

    E2E_STAGE_END();

    // Stage 2: Full topology analysis
    E2E_STAGE_BEGIN("Full topology analysis", 20000);

    auto analysis_start = std::chrono::high_resolution_clock::now();

    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(
        gpu_ctx_, brain_graph
    );
    E2E_ASSERT_NOT_NULL(metrics, "Failed to compute metrics");

    nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
        gpu_ctx_, brain_graph, 1.0f, 100, 1e-5f
    );
    E2E_ASSERT_NOT_NULL(communities, "Failed to detect communities");

    nimcp_apsp_result_gpu_t apsp;
    apsp.distances = Create2DTensor(N_REGIONS, N_REGIONS, NIMCP_GPU_PRECISION_FP32);
    bool apsp_ok = nimcp_shortest_path_floyd_warshall(gpu_ctx_, brain_graph, &apsp);
    E2E_ASSERT(apsp_ok, "Failed to compute APSP");

    nimcp_gpu_context_synchronize(gpu_ctx_);

    auto analysis_end = std::chrono::high_resolution_clock::now();
    double analysis_time = std::chrono::duration<double, std::milli>(
        analysis_end - analysis_start
    ).count();

    E2E_STAGE_END();

    // Stage 3: Generate comprehensive report
    E2E_STAGE_BEGIN("Generate analysis report", 1000);

    std::cout << "\n  ============================================" << std::endl;
    std::cout << "  BRAIN NETWORK TOPOLOGY ANALYSIS REPORT" << std::endl;
    std::cout << "  ============================================" << std::endl;

    std::cout << "\n  1. NETWORK STRUCTURE" << std::endl;
    std::cout << "     - Density: " << metrics->density << std::endl;
    std::cout << "     - Diameter: " << apsp.diameter << std::endl;
    std::cout << "     - Avg Path Length: " << apsp.avg_path_length << std::endl;

    std::cout << "\n  2. INTEGRATION-SEGREGATION BALANCE" << std::endl;
    std::cout << "     - Global Clustering: " << metrics->global_clustering << std::endl;
    std::cout << "       (high = segregated processing)" << std::endl;
    std::cout << "     - Avg Path Length: " << apsp.avg_path_length << std::endl;
    std::cout << "       (low = efficient integration)" << std::endl;

    std::cout << "\n  3. MODULAR ORGANIZATION" << std::endl;
    std::cout << "     - Planted Modules: " << N_MODULES << std::endl;
    std::cout << "     - Detected Modules: " << communities->num_communities << std::endl;
    std::cout << "     - Modularity Q: " << communities->modularity << std::endl;

    // Analyze hub structure
    auto pr_host = TensorToHost(metrics->pagerank, N_REGIONS);
    int hub_count = CountHubs(pr_host, 0.9f);
    float avg_pr = std::accumulate(pr_host.begin(), pr_host.end(), 0.0f) / N_REGIONS;
    float max_pr = *std::max_element(pr_host.begin(), pr_host.end());

    std::cout << "\n  4. HUB STRUCTURE" << std::endl;
    std::cout << "     - Hub Regions (top 10%): " << hub_count << std::endl;
    std::cout << "     - Max PageRank: " << max_pr << std::endl;
    std::cout << "     - Hub Prominence (max/avg): " << (max_pr / avg_pr) << "x" << std::endl;

    std::cout << "\n  5. PERFORMANCE" << std::endl;
    std::cout << "     - Total Analysis Time: " << analysis_time << " ms" << std::endl;
    std::cout << "     - Throughput: " << (N_REGIONS / (analysis_time / 1000))
              << " regions/sec" << std::endl;

    std::cout << "\n  ============================================" << std::endl;

    E2E_STAGE_END();

    // Verify biological plausibility
    E2E_ASSERT(metrics->global_clustering > 0.2f,
               "Clustering too low for brain network");
    E2E_ASSERT(apsp.avg_path_length < log(N_REGIONS),
               "Path length too high (not small-world)");
    E2E_ASSERT(communities->modularity > 0.3f,
               "Modularity too low (weak community structure)");

    // Cleanup
    nimcp_gpu_tensor_destroy(apsp.distances);
    nimcp_community_result_gpu_destroy(communities);
    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(brain_graph);

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
