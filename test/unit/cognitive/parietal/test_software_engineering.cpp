/**
 * @file test_software_engineering.cpp
 * @brief Unit tests for software engineering reasoning module
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_software_engineering.h"

class SoftwareEngTest : public ::testing::Test {
protected:
    software_eng_t* se = nullptr;

    void SetUp() override {
        se = software_eng_create();
        ASSERT_NE(se, nullptr);
    }

    void TearDown() override {
        software_eng_destroy(se);
        se = nullptr;
    }

    static constexpr float FLOAT_TOLERANCE = 0.01f;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SoftwareEngTest, CreateDefault)
{
    EXPECT_NE(se, nullptr);
}

TEST_F(SoftwareEngTest, CreateCustom)
{
    software_eng_config_t config = software_eng_default_config();
    config.complexity_threshold = 20.0f;
    config.strict_mode = true;

    software_eng_t* custom = software_eng_create_custom(&config);
    ASSERT_NE(custom, nullptr);

    software_eng_destroy(custom);
}

TEST_F(SoftwareEngTest, CreateWithNullConfig)
{
    software_eng_t* s = software_eng_create_custom(nullptr);
    EXPECT_NE(s, nullptr);
    software_eng_destroy(s);
}

TEST_F(SoftwareEngTest, DestroyNullSafe)
{
    software_eng_destroy(nullptr);  // Should not crash
}

TEST_F(SoftwareEngTest, DefaultConfig)
{
    software_eng_config_t config = software_eng_default_config();
    EXPECT_NEAR(config.complexity_threshold, 10.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.coupling_threshold, 0.7f, FLOAT_TOLERANCE);
    EXPECT_EQ(config.method_length_limit, 50);
}

TEST_F(SoftwareEngTest, ValidateConfig)
{
    software_eng_config_t config = software_eng_default_config();
    EXPECT_TRUE(software_eng_validate_config(&config));

    config.complexity_threshold = 0.0f;
    EXPECT_FALSE(software_eng_validate_config(&config));

    config.complexity_threshold = 10.0f;
    config.method_length_limit = 0;
    EXPECT_FALSE(software_eng_validate_config(&config));
}

TEST_F(SoftwareEngTest, ValidateNullConfig)
{
    EXPECT_FALSE(software_eng_validate_config(nullptr));
}

//=============================================================================
// Complexity Analysis Tests
//=============================================================================

TEST_F(SoftwareEngTest, AnalyzeComplexityConstant)
{
    algorithm_traits_t traits = {};
    traits.has_loops = false;
    traits.has_recursion = false;

    complexity_result_t result;
    int ret = software_eng_analyze_complexity(se, &traits, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.time_complexity, COMPLEXITY_O_1);
    EXPECT_GT(result.confidence, 0.5f);
}

TEST_F(SoftwareEngTest, AnalyzeComplexityLinear)
{
    algorithm_traits_t traits = {};
    traits.has_loops = true;
    traits.loop_depth = 1;

    complexity_result_t result;
    int ret = software_eng_analyze_complexity(se, &traits, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.time_complexity, COMPLEXITY_O_N);
}

TEST_F(SoftwareEngTest, AnalyzeComplexityQuadratic)
{
    algorithm_traits_t traits = {};
    traits.has_loops = true;
    traits.loop_depth = 2;

    complexity_result_t result;
    int ret = software_eng_analyze_complexity(se, &traits, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.time_complexity, COMPLEXITY_O_N_SQUARED);
}

TEST_F(SoftwareEngTest, AnalyzeComplexityCubic)
{
    algorithm_traits_t traits = {};
    traits.has_loops = true;
    traits.loop_depth = 3;

    complexity_result_t result;
    int ret = software_eng_analyze_complexity(se, &traits, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.time_complexity, COMPLEXITY_O_N_CUBED);
}

TEST_F(SoftwareEngTest, AnalyzeComplexityExponential)
{
    algorithm_traits_t traits = {};
    traits.has_recursion = true;
    traits.recursive_calls = 2;
    traits.has_dynamic_programming = false;

    complexity_result_t result;
    int ret = software_eng_analyze_complexity(se, &traits, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.time_complexity, COMPLEXITY_O_2_N);
}

TEST_F(SoftwareEngTest, AnalyzeComplexityNLogN)
{
    algorithm_traits_t traits = {};
    traits.has_sorting = true;

    complexity_result_t result;
    int ret = software_eng_analyze_complexity(se, &traits, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.time_complexity, COMPLEXITY_O_N_LOG_N);
}

TEST_F(SoftwareEngTest, ComplexityToString)
{
    EXPECT_STREQ(software_eng_complexity_to_string(COMPLEXITY_O_1), "O(1)");
    EXPECT_STREQ(software_eng_complexity_to_string(COMPLEXITY_O_N), "O(n)");
    EXPECT_STREQ(software_eng_complexity_to_string(COMPLEXITY_O_N_SQUARED), "O(n^2)");
    EXPECT_STREQ(software_eng_complexity_to_string(COMPLEXITY_O_2_N), "O(2^n)");
}

TEST_F(SoftwareEngTest, CompareComplexity)
{
    EXPECT_LT(software_eng_compare_complexity(COMPLEXITY_O_1, COMPLEXITY_O_N), 0);
    EXPECT_EQ(software_eng_compare_complexity(COMPLEXITY_O_N, COMPLEXITY_O_N), 0);
    EXPECT_GT(software_eng_compare_complexity(COMPLEXITY_O_N_SQUARED, COMPLEXITY_O_N), 0);
}

TEST_F(SoftwareEngTest, EstimateRuntime)
{
    double runtime_o1 = software_eng_estimate_runtime(se, COMPLEXITY_O_1, 1.0, 1000);
    EXPECT_NEAR(runtime_o1, 1.0, FLOAT_TOLERANCE);

    double runtime_on = software_eng_estimate_runtime(se, COMPLEXITY_O_N, 1.0, 1000);
    EXPECT_NEAR(runtime_on, 1000.0, FLOAT_TOLERANCE);

    double runtime_on2 = software_eng_estimate_runtime(se, COMPLEXITY_O_N_SQUARED, 1.0, 100);
    EXPECT_NEAR(runtime_on2, 10000.0, FLOAT_TOLERANCE);
}

TEST_F(SoftwareEngTest, ComplexityNullHandling)
{
    algorithm_traits_t traits = {};
    complexity_result_t result;

    EXPECT_EQ(software_eng_analyze_complexity(nullptr, &traits, &result), -1);
    EXPECT_EQ(software_eng_analyze_complexity(se, nullptr, &result), -1);
    EXPECT_EQ(software_eng_analyze_complexity(se, &traits, nullptr), -1);
}

//=============================================================================
// Pattern Detection Tests
//=============================================================================

TEST_F(SoftwareEngTest, DetectSingleton)
{
    code_structure_t structure = {};
    structure.has_single_instance = true;
    structure.num_classes = 1;

    pattern_result_t result;
    int ret = software_eng_detect_pattern(se, &structure, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern, DESIGN_PATTERN_SINGLETON);
    EXPECT_EQ(result.category, PATTERN_CATEGORY_CREATIONAL);
    EXPECT_GT(result.confidence, 0.5f);
}

TEST_F(SoftwareEngTest, DetectFactory)
{
    code_structure_t structure = {};
    structure.has_factory_method = true;
    structure.has_abstract_base = true;

    pattern_result_t result;
    int ret = software_eng_detect_pattern(se, &structure, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern, DESIGN_PATTERN_FACTORY);
    EXPECT_EQ(result.category, PATTERN_CATEGORY_CREATIONAL);
}

TEST_F(SoftwareEngTest, DetectObserver)
{
    code_structure_t structure = {};
    structure.has_callbacks = true;
    structure.num_interfaces = 1;

    pattern_result_t result;
    int ret = software_eng_detect_pattern(se, &structure, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern, DESIGN_PATTERN_OBSERVER);
    EXPECT_EQ(result.category, PATTERN_CATEGORY_BEHAVIORAL);
}

TEST_F(SoftwareEngTest, DetectStrategy)
{
    code_structure_t structure = {};
    structure.has_strategy_interface = true;
    structure.has_composition = true;

    pattern_result_t result;
    int ret = software_eng_detect_pattern(se, &structure, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern, DESIGN_PATTERN_STRATEGY);
    EXPECT_EQ(result.category, PATTERN_CATEGORY_BEHAVIORAL);
}

TEST_F(SoftwareEngTest, DetectUnknownPattern)
{
    code_structure_t structure = {};  // Empty structure

    pattern_result_t result;
    int ret = software_eng_detect_pattern(se, &structure, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern, DESIGN_PATTERN_NONE);
}

TEST_F(SoftwareEngTest, PatternToString)
{
    EXPECT_STREQ(software_eng_pattern_to_string(DESIGN_PATTERN_SINGLETON), "Singleton");
    EXPECT_STREQ(software_eng_pattern_to_string(DESIGN_PATTERN_FACTORY), "Factory Method");
    EXPECT_STREQ(software_eng_pattern_to_string(DESIGN_PATTERN_OBSERVER), "Observer");
}

TEST_F(SoftwareEngTest, CategoryToString)
{
    EXPECT_STREQ(software_eng_category_to_string(PATTERN_CATEGORY_CREATIONAL), "Creational");
    EXPECT_STREQ(software_eng_category_to_string(PATTERN_CATEGORY_STRUCTURAL), "Structural");
    EXPECT_STREQ(software_eng_category_to_string(PATTERN_CATEGORY_BEHAVIORAL), "Behavioral");
}

TEST_F(SoftwareEngTest, PatternApplicability)
{
    code_structure_t structure = {};
    structure.has_single_instance = true;

    float app = software_eng_pattern_applicability(se, DESIGN_PATTERN_SINGLETON, &structure);
    EXPECT_GT(app, 0.5f);

    structure.has_single_instance = false;
    app = software_eng_pattern_applicability(se, DESIGN_PATTERN_SINGLETON, &structure);
    EXPECT_LT(app, 0.5f);
}

//=============================================================================
// Code Smell Detection Tests
//=============================================================================

TEST_F(SoftwareEngTest, DetectLongMethodSmell)
{
    software_metrics_t metrics = {};
    metrics.cyclomatic_complexity = 25.0f;  // Above threshold

    smell_result_t results[8];
    uint32_t count = software_eng_detect_smells(se, &metrics, results, 8);

    EXPECT_GE(count, 1);
    bool found_long_method = false;
    for (uint32_t i = 0; i < count; i++) {
        if (results[i].smell == SMELL_LONG_METHOD) {
            found_long_method = true;
            break;
        }
    }
    EXPECT_TRUE(found_long_method);
}

TEST_F(SoftwareEngTest, DetectGodClass)
{
    software_metrics_t metrics = {};
    metrics.cyclomatic_complexity = 25.0f;  // 2x threshold
    metrics.efferent_coupling = 15.0f;

    smell_result_t results[8];
    uint32_t count = software_eng_detect_smells(se, &metrics, results, 8);

    EXPECT_GE(count, 1);
    bool found_god_class = false;
    for (uint32_t i = 0; i < count; i++) {
        if (results[i].smell == SMELL_GOD_CLASS) {
            found_god_class = true;
            break;
        }
    }
    EXPECT_TRUE(found_god_class);
}

TEST_F(SoftwareEngTest, SmellToString)
{
    EXPECT_STREQ(software_eng_smell_to_string(SMELL_LONG_METHOD), "Long Method");
    EXPECT_STREQ(software_eng_smell_to_string(SMELL_LARGE_CLASS), "Large Class");
    EXPECT_STREQ(software_eng_smell_to_string(SMELL_GOD_CLASS), "God Class");
}

//=============================================================================
// Dependency Analysis Tests
//=============================================================================

TEST_F(SoftwareEngTest, InitGraph)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    EXPECT_EQ(graph.num_nodes, 0);
    EXPECT_FALSE(graph.has_cycles);
}

TEST_F(SoftwareEngTest, AddNode)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int id = software_eng_add_node(&graph, "ModuleA");
    EXPECT_EQ(id, 0);
    EXPECT_EQ(graph.num_nodes, 1);
    EXPECT_STREQ(graph.nodes[0].name, "ModuleA");
}

TEST_F(SoftwareEngTest, AddDependency)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int a = software_eng_add_node(&graph, "A");
    int b = software_eng_add_node(&graph, "B");

    int result = software_eng_add_dependency(&graph, a, b);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(graph.nodes[a].num_dependencies, 1);
    EXPECT_EQ(graph.nodes[a].dependencies[0], (uint32_t)b);
}

TEST_F(SoftwareEngTest, DetectNoCycle)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int a = software_eng_add_node(&graph, "A");
    int b = software_eng_add_node(&graph, "B");
    software_eng_add_dependency(&graph, a, b);  // A -> B

    bool has_cycle = software_eng_detect_cycles(se, &graph);
    EXPECT_FALSE(has_cycle);
}

TEST_F(SoftwareEngTest, DetectCycle)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int a = software_eng_add_node(&graph, "A");
    int b = software_eng_add_node(&graph, "B");
    software_eng_add_dependency(&graph, a, b);  // A -> B
    software_eng_add_dependency(&graph, b, a);  // B -> A (cycle!)

    bool has_cycle = software_eng_detect_cycles(se, &graph);
    EXPECT_TRUE(has_cycle);
}

TEST_F(SoftwareEngTest, AnalyzeDependencies)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int a = software_eng_add_node(&graph, "A");
    int b = software_eng_add_node(&graph, "B");
    int c = software_eng_add_node(&graph, "C");
    software_eng_add_dependency(&graph, a, b);
    software_eng_add_dependency(&graph, b, c);

    int result = software_eng_analyze_dependencies(se, &graph);
    EXPECT_EQ(result, 0);
    EXPECT_GE(graph.max_depth, 1);
}

TEST_F(SoftwareEngTest, TopologicalSort)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int a = software_eng_add_node(&graph, "A");
    int b = software_eng_add_node(&graph, "B");
    int c = software_eng_add_node(&graph, "C");
    software_eng_add_dependency(&graph, a, b);
    software_eng_add_dependency(&graph, b, c);
    software_eng_analyze_dependencies(se, &graph);

    uint32_t order[3];
    int count = software_eng_topological_sort(se, &graph, order, 3);

    EXPECT_EQ(count, 3);
}

TEST_F(SoftwareEngTest, TopologicalSortWithCycle)
{
    dep_graph_t graph;
    software_eng_init_graph(&graph);

    int a = software_eng_add_node(&graph, "A");
    int b = software_eng_add_node(&graph, "B");
    software_eng_add_dependency(&graph, a, b);
    software_eng_add_dependency(&graph, b, a);
    software_eng_detect_cycles(se, &graph);

    uint32_t order[2];
    int count = software_eng_topological_sort(se, &graph, order, 2);

    EXPECT_EQ(count, -1);  // Should fail due to cycle
}

//=============================================================================
// Metrics Tests
//=============================================================================

TEST_F(SoftwareEngTest, CyclomaticComplexity)
{
    // M = E - N + 2P
    // Simple method: 5 edges, 4 nodes, 1 component
    float cc = software_eng_cyclomatic_complexity(5, 4, 1);
    EXPECT_NEAR(cc, 3.0f, FLOAT_TOLERANCE);
}

TEST_F(SoftwareEngTest, MaintainabilityIndex)
{
    // Should return value between 0 and 100
    float mi = software_eng_maintainability_index(100.0f, 5.0f, 50);
    EXPECT_GE(mi, 0.0f);
    EXPECT_LE(mi, 100.0f);
}

TEST_F(SoftwareEngTest, Instability)
{
    // I = Ce / (Ca + Ce)
    float i = software_eng_instability(5.0f, 10.0f);
    EXPECT_NEAR(i, 0.667f, 0.01f);  // 10 / 15

    i = software_eng_instability(0.0f, 10.0f);
    EXPECT_NEAR(i, 1.0f, FLOAT_TOLERANCE);  // Maximally unstable

    i = software_eng_instability(10.0f, 0.0f);
    EXPECT_NEAR(i, 0.0f, FLOAT_TOLERANCE);  // Maximally stable
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(SoftwareEngTest, SetInflammation)
{
    EXPECT_EQ(software_eng_set_inflammation(se, 0.5f), 0);
    EXPECT_EQ(software_eng_set_inflammation(se, 1.5f), 0);  // Should clamp
}

TEST_F(SoftwareEngTest, SetSleepDeprivation)
{
    EXPECT_EQ(software_eng_set_sleep_deprivation(se, 0.5f), 0);
}

TEST_F(SoftwareEngTest, ModulationNullHandling)
{
    EXPECT_EQ(software_eng_set_inflammation(nullptr, 0.5f), -1);
    EXPECT_EQ(software_eng_set_sleep_deprivation(nullptr, 0.5f), -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SoftwareEngTest, GetStats)
{
    algorithm_traits_t traits = {};
    complexity_result_t result;
    software_eng_analyze_complexity(se, &traits, &result);

    software_eng_stats_t stats;
    int ret = software_eng_get_stats(se, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.complexity_analyses, 1);
}

TEST_F(SoftwareEngTest, ResetStats)
{
    algorithm_traits_t traits = {};
    complexity_result_t result;
    software_eng_analyze_complexity(se, &traits, &result);

    software_eng_reset_stats(se);

    software_eng_stats_t stats;
    software_eng_get_stats(se, &stats);

    EXPECT_EQ(stats.complexity_analyses, 0);
    EXPECT_EQ(stats.pattern_detections, 0);
}

TEST_F(SoftwareEngTest, StatsNullHandling)
{
    software_eng_stats_t stats;
    EXPECT_EQ(software_eng_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(software_eng_get_stats(se, nullptr), -1);
}

TEST_F(SoftwareEngTest, ResetStatsNullSafe)
{
    software_eng_reset_stats(nullptr);  // Should not crash
}

TEST_F(SoftwareEngTest, GetLastError)
{
    const char* err = software_eng_get_last_error();
    EXPECT_NE(err, nullptr);
}
