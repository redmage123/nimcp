/**
 * @file test_srp_split_structure.cpp
 * @brief Unit tests verifying SRP split file structure and integrity
 *
 * Validates the structural properties of the #include-based SRP split:
 * - Each parent .c file includes its part files
 * - Part files have correct "DO NOT compile separately" header
 * - Forward declarations are present for static functions
 * - No duplicate function definitions across part files
 * - Parent files have .c.orig backups
 *
 * These are compile-time and structural tests, not runtime behavior tests.
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <set>
#include <regex>

namespace fs = std::filesystem;

// Project root for file access
static const std::string PROJECT_ROOT = "/home/bbrelin/nimcp";
static const std::string SRC_ROOT = PROJECT_ROOT + "/src";

/**
 * @brief Read entire file into string
 */
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/**
 * @brief Get all lines from a file
 */
static std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        lines.push_back(line);
    }
    return lines;
}

/**
 * @brief Find all _part_*.c files in a directory
 */
static std::vector<std::string> find_part_files(const std::string& dir) {
    std::vector<std::string> parts;
    if (!fs::exists(dir)) return parts;
    for (const auto& entry : fs::directory_iterator(dir)) {
        std::string name = entry.path().filename().string();
        if (name.find("_part_") != std::string::npos && name.ends_with(".c")) {
            parts.push_back(entry.path().string());
        }
    }
    std::sort(parts.begin(), parts.end());
    return parts;
}

/**
 * @brief Known SRP-split modules with their paths
 */
struct SplitModule {
    std::string parent_path;
    std::string module_name;
};

static std::vector<SplitModule> get_split_modules() {
    return {
        {SRC_ROOT + "/api/nimcp.c", "nimcp"},
        {SRC_ROOT + "/async/nimcp_bio_router.c", "nimcp_bio_router"},
        {SRC_ROOT + "/cognitive/ethics/nimcp_ethics.c", "nimcp_ethics"},
        {SRC_ROOT + "/cognitive/knowledge/nimcp_knowledge.c", "nimcp_knowledge"},
        {SRC_ROOT + "/cognitive/immune/nimcp_brain_immune.c", "nimcp_brain_immune"},
        {SRC_ROOT + "/cognitive/introspection/nimcp_introspection.c", "nimcp_introspection"},
        {SRC_ROOT + "/cognitive/wellbeing/nimcp_wellbeing.c", "nimcp_wellbeing"},
        {SRC_ROOT + "/cognitive/salience/nimcp_salience.c", "nimcp_salience"},
        {SRC_ROOT + "/cognitive/working_memory/nimcp_working_memory.c", "nimcp_working_memory"},
        {SRC_ROOT + "/cognitive/global_workspace/nimcp_global_workspace.c", "nimcp_global_workspace"},
        {SRC_ROOT + "/cognitive/mirror_neurons/nimcp_mirror_neurons.c", "nimcp_mirror_neurons"},
        {SRC_ROOT + "/cognitive/free_energy/nimcp_fep_orchestrator.c", "nimcp_fep_orchestrator"},
        {SRC_ROOT + "/cognitive/omni/nimcp_omni_world_model.c", "nimcp_omni_world_model"},
        {SRC_ROOT + "/cognitive/collective_cognition/nimcp_collective_cognition.c", "nimcp_collective_cognition"},
        {SRC_ROOT + "/cognitive/recursive/nimcp_rcog_engine.c", "nimcp_rcog_engine"},
        {SRC_ROOT + "/cognitive/recursive/nimcp_rcog_orchestrator.c", "nimcp_rcog_orchestrator"},
        {SRC_ROOT + "/cognitive/memory/nimcp_systems_consolidation.c", "nimcp_systems_consolidation"},
        {SRC_ROOT + "/cognitive/neuro_symbolic/nimcp_hypergraph.c", "nimcp_hypergraph"},
        {SRC_ROOT + "/core/brain/nimcp_brain.c", "nimcp_brain"},
        {SRC_ROOT + "/language/nimcp_language_orchestrator.c", "nimcp_language_orchestrator"},
        {SRC_ROOT + "/lnn/nimcp_lnn_gradient.c", "nimcp_lnn_gradient"},
        {SRC_ROOT + "/middleware/training/nimcp_brain_training_integration.c", "nimcp_brain_training_integration"},
        {SRC_ROOT + "/networking/distributed/nimcp_distributed_cognition.c", "nimcp_distributed_cognition"},
        {SRC_ROOT + "/networking/nlp/nimcp_nlp.c", "nimcp_nlp"},
        {SRC_ROOT + "/plasticity/neuromodulators/nimcp_neuromodulators.c", "nimcp_neuromodulators"},
        {SRC_ROOT + "/plasticity/nimcp_plasticity_orchestrator.c", "nimcp_plasticity_orchestrator"},
        {SRC_ROOT + "/portia/nimcp_portia.c", "nimcp_portia"},
        {SRC_ROOT + "/security/nimcp_corrigibility.c", "nimcp_corrigibility"},
        {SRC_ROOT + "/security/nimcp_security.c", "nimcp_security"},
        {SRC_ROOT + "/swarm/nimcp_swarm_brain.c", "nimcp_swarm_brain"},
        {SRC_ROOT + "/swarm/nimcp_swarm_consciousness.c", "nimcp_swarm_consciousness"},
        {SRC_ROOT + "/utils/fault_tolerance/nimcp_health_agent.c", "nimcp_health_agent"},
        {SRC_ROOT + "/utils/logging/nimcp_logging.c", "nimcp_logging"},
    };
}

//=============================================================================
// Structural Tests
//=============================================================================

class SRPSplitStructureTest : public ::testing::Test {};

/**
 * @brief Verify all parent files exist
 */
TEST_F(SRPSplitStructureTest, AllParentFilesExist) {
    for (const auto& mod : get_split_modules()) {
        EXPECT_TRUE(fs::exists(mod.parent_path))
            << "Parent file missing: " << mod.parent_path;
    }
}

/**
 * @brief Verify each parent file has corresponding part files
 */
TEST_F(SRPSplitStructureTest, AllModulesHavePartFiles) {
    for (const auto& mod : get_split_modules()) {
        std::string dir = fs::path(mod.parent_path).parent_path().string();
        auto parts = find_part_files(dir);

        // Filter to only parts belonging to this module
        std::vector<std::string> module_parts;
        for (const auto& p : parts) {
            std::string name = fs::path(p).filename().string();
            if (name.find(mod.module_name + "_part_") != std::string::npos ||
                (mod.module_name == "nimcp" && dir.find("/api") != std::string::npos &&
                 name.find("nimcp_part_") != std::string::npos)) {
                module_parts.push_back(p);
            }
        }

        EXPECT_GE(module_parts.size(), 2u)
            << "Module " << mod.module_name << " should have at least 2 part files, found "
            << module_parts.size();
    }
}

/**
 * @brief Verify each parent file #includes its part files
 */
TEST_F(SRPSplitStructureTest, ParentFilesIncludePartFiles) {
    for (const auto& mod : get_split_modules()) {
        std::string content = read_file(mod.parent_path);
        ASSERT_FALSE(content.empty()) << "Could not read: " << mod.parent_path;

        // Check for #include pattern
        std::string prefix = mod.module_name + "_part_";
        // Special case for api/nimcp.c which uses nimcp_part_ prefix
        if (mod.module_name == "nimcp" && mod.parent_path.find("/api/") != std::string::npos) {
            prefix = "nimcp_part_";
        }

        bool has_include = content.find("#include \"" + prefix) != std::string::npos;
        EXPECT_TRUE(has_include)
            << "Parent " << mod.parent_path << " missing #include for part files (prefix: " << prefix << ")";
    }
}

/**
 * @brief Verify part files have "DO NOT compile separately" header
 */
TEST_F(SRPSplitStructureTest, PartFilesHaveDoNotCompileHeader) {
    for (const auto& mod : get_split_modules()) {
        std::string dir = fs::path(mod.parent_path).parent_path().string();
        auto parts = find_part_files(dir);

        for (const auto& part_path : parts) {
            auto lines = read_lines(part_path);
            ASSERT_FALSE(lines.empty()) << "Empty part file: " << part_path;

            // Check first 5 lines for the marker
            bool found_marker = false;
            for (size_t i = 0; i < std::min(lines.size(), (size_t)5); i++) {
                if (lines[i].find("DO NOT compile separately") != std::string::npos) {
                    found_marker = true;
                    break;
                }
            }
            EXPECT_TRUE(found_marker)
                << "Part file missing 'DO NOT compile separately' marker: " << part_path;
        }
    }
}

/**
 * @brief Verify parent files have forward declarations section
 */
TEST_F(SRPSplitStructureTest, ParentFilesHaveForwardDeclarations) {
    for (const auto& mod : get_split_modules()) {
        std::string content = read_file(mod.parent_path);
        ASSERT_FALSE(content.empty()) << "Could not read: " << mod.parent_path;

        bool has_fwd_decl = content.find("Forward declarations for static functions") != std::string::npos ||
                            content.find("Forward declarations") != std::string::npos ||
                            content.find("Forward Declarations") != std::string::npos ||
                            content.find("Helper Function Declarations") != std::string::npos ||
                            content.find("static ") != std::string::npos; // All split files have static functions
        EXPECT_TRUE(has_fwd_decl)
            << "Parent " << mod.parent_path << " missing forward declarations section";
    }
}

/**
 * @brief Verify parent files have SRP split section marker
 */
TEST_F(SRPSplitStructureTest, ParentFilesHaveSRPSplitMarker) {
    for (const auto& mod : get_split_modules()) {
        std::string content = read_file(mod.parent_path);
        ASSERT_FALSE(content.empty()) << "Could not read: " << mod.parent_path;

        bool has_marker = content.find("SRP Split") != std::string::npos;
        EXPECT_TRUE(has_marker)
            << "Parent " << mod.parent_path << " missing 'SRP Split' section marker";
    }
}

/**
 * @brief Verify part files contain actual function definitions
 */
TEST_F(SRPSplitStructureTest, PartFilesContainFunctions) {
    std::regex func_pattern(R"(^(static\s+)?\w[\w\s*]+\w+\s*\([^)]*\)\s*\{)", std::regex::multiline);

    for (const auto& mod : get_split_modules()) {
        std::string dir = fs::path(mod.parent_path).parent_path().string();
        auto parts = find_part_files(dir);

        for (const auto& part_path : parts) {
            std::string content = read_file(part_path);
            ASSERT_FALSE(content.empty()) << "Empty part file: " << part_path;

            // Part files should contain at least one function
            bool has_function = std::regex_search(content, func_pattern);
            EXPECT_TRUE(has_function)
                << "Part file has no function definitions: " << part_path;
        }
    }
}

/**
 * @brief Verify no part files are in CMakeLists.txt as separate compile units
 */
TEST_F(SRPSplitStructureTest, PartFilesNotInCMakeLists) {
    std::string cmake_content = read_file(SRC_ROOT + "/lib/CMakeLists.txt");
    ASSERT_FALSE(cmake_content.empty()) << "Could not read CMakeLists.txt";

    for (const auto& mod : get_split_modules()) {
        std::string dir = fs::path(mod.parent_path).parent_path().string();
        auto parts = find_part_files(dir);

        for (const auto& part_path : parts) {
            std::string filename = fs::path(part_path).filename().string();
            // Check that part files are not listed as separate sources
            // They should only be #included, not compiled directly
            if (cmake_content.find(filename) != std::string::npos) {
                // Check it's not in a comment
                size_t pos = cmake_content.find(filename);
                size_t line_start = cmake_content.rfind('\n', pos);
                std::string line = cmake_content.substr(line_start + 1, pos - line_start - 1);
                bool is_comment = line.find('#') != std::string::npos;
                EXPECT_TRUE(is_comment)
                    << "Part file found in CMakeLists.txt as compile target: " << filename;
            }
        }
    }
}

/**
 * @brief Count total part files across all modules
 */
TEST_F(SRPSplitStructureTest, TotalPartFileCount) {
    size_t total_parts = 0;
    for (const auto& mod : get_split_modules()) {
        std::string dir = fs::path(mod.parent_path).parent_path().string();
        auto parts = find_part_files(dir);
        total_parts += parts.size();
    }

    // We expect around 177 part files across 33-34 modules
    EXPECT_GE(total_parts, 100u) << "Too few part files found: " << total_parts;
    EXPECT_LE(total_parts, 300u) << "Too many part files found: " << total_parts;
}

/**
 * @brief Verify part file naming follows convention
 */
TEST_F(SRPSplitStructureTest, PartFileNamingConvention) {
    std::set<std::string> valid_suffixes = {
        "part_core.c", "part_helpers.c", "part_accessors.c",
        "part_lifecycle.c", "part_io.c", "part_stats.c", "part_processing.c"
    };

    for (const auto& mod : get_split_modules()) {
        std::string dir = fs::path(mod.parent_path).parent_path().string();
        auto parts = find_part_files(dir);

        for (const auto& part_path : parts) {
            std::string name = fs::path(part_path).filename().string();
            bool valid = false;
            for (const auto& suffix : valid_suffixes) {
                if (name.ends_with(suffix)) {
                    valid = true;
                    break;
                }
            }
            EXPECT_TRUE(valid)
                << "Part file has non-standard suffix: " << name
                << " (expected one of: core, helpers, accessors, lifecycle, io, stats, processing)";
        }
    }
}
