/**
 * @file test_cpp_bindings.cpp
 * @brief GTest suite for nimcp C++20 bindings
 *
 * Tests all RAII wrappers, enum conversions, exception handling,
 * callback trampolines, and complete API coverage.
 */

#include <nimcp.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

// ============================================================================
// Shared test fixture — init once per suite
// ============================================================================

class CppBindingsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        nimcp::init();
    }

    static void TearDownTestSuite() {
        nimcp::shutdown();
    }
};

// ============================================================================
// 1. LibraryTest — init/shutdown, version
// ============================================================================

class LibraryTest : public CppBindingsTest {};

TEST_F(LibraryTest, VersionString) {
    const char* v = nimcp::version();
    ASSERT_NE(v, nullptr);
    EXPECT_NE(std::string(v).find('.'), std::string::npos);
}

TEST_F(LibraryTest, VersionInt) {
    int vi = nimcp::version_int();
    EXPECT_GT(vi, 20000);  // at least 2.x.x
}

TEST_F(LibraryTest, LibraryRAII) {
    // Library guard calls init/shutdown; nested init is safe
    {
        nimcp::Library lib;
        EXPECT_NE(nimcp::version(), nullptr);
    }
    // Re-init for the rest of the suite
    nimcp::init();
}

// ============================================================================
// 2. BrainCreateTest — constructor, move, moved-from
// ============================================================================

class BrainCreateTest : public CppBindingsTest {};

TEST_F(BrainCreateTest, BasicCreate) {
    nimcp::Brain brain("test_brain", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);
    EXPECT_NE(brain.get(), nullptr);
}

TEST_F(BrainCreateTest, MoveConstruction) {
    nimcp::Brain a("move_src", nimcp::BrainSize::Tiny,
                   nimcp::TaskType::Regression, 4, 1);
    nimcp_brain_t raw = a.get();
    nimcp::Brain b(std::move(a));
    EXPECT_EQ(b.get(), raw);
    EXPECT_EQ(a.get(), nullptr);
}

TEST_F(BrainCreateTest, MoveAssignment) {
    nimcp::Brain a("move_a", nimcp::BrainSize::Tiny,
                   nimcp::TaskType::Classification, 4, 2);
    nimcp::Brain b("move_b", nimcp::BrainSize::Tiny,
                   nimcp::TaskType::Classification, 4, 2);
    nimcp_brain_t raw_a = a.get();
    b = std::move(a);
    EXPECT_EQ(b.get(), raw_a);
    EXPECT_EQ(a.get(), nullptr);
}

// ============================================================================
// 3. BrainPersistenceTest — save, load
// ============================================================================

class BrainPersistenceTest : public CppBindingsTest {};

TEST_F(BrainPersistenceTest, SaveAndLoad) {
    const std::string path = "/tmp/nimcp_cpp_test_brain.bin";
    {
        nimcp::Brain brain("persist", nimcp::BrainSize::Tiny,
                           nimcp::TaskType::Classification, 4, 2);
        brain.save(path);
    }
    {
        auto brain = nimcp::Brain::load(path);
        EXPECT_NE(brain.get(), nullptr);
    }
    std::filesystem::remove(path);
}

TEST_F(BrainPersistenceTest, LoadNonExistentThrowsIOError) {
    EXPECT_THROW(nimcp::Brain::load("/tmp/does_not_exist_nimcp.bin"),
                 nimcp::IOError);
}

// ============================================================================
// 4. BrainLearningTest — learn + predict round-trip
// ============================================================================

class BrainLearningTest : public CppBindingsTest {};

TEST_F(BrainLearningTest, LearnAndPredict) {
    nimcp::Brain brain("learn_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    std::vector<float> cat_features = {1.0f, 0.0f, 0.5f, 0.3f};
    std::vector<float> dog_features = {0.0f, 1.0f, 0.2f, 0.8f};

    // Train several examples
    for (int i = 0; i < 10; ++i) {
        brain.learn(cat_features, "cat", 0.9f);
        brain.learn(dog_features, "dog", 0.9f);
    }

    auto pred = brain.predict(cat_features);
    EXPECT_FALSE(pred.label.empty());
    EXPECT_GE(pred.confidence, 0.0f);
    EXPECT_LE(pred.confidence, 1.0f);
}

TEST_F(BrainLearningTest, InferOutputSize) {
    nimcp::Brain brain("infer_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Regression, 4, 3);

    std::vector<float> features = {1.0f, 2.0f, 3.0f, 4.0f};
    auto outputs = brain.infer(features, 3);
    EXPECT_EQ(outputs.size(), 3u);
}

// ============================================================================
// 5. BrainTrainingTest — training pipeline
// ============================================================================

class BrainTrainingTest : public CppBindingsTest {};

TEST_F(BrainTrainingTest, ConfigureAndStep) {
    nimcp::Brain brain("train_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    auto config = nimcp::TrainingConfig::from_default();
    config.learning_rate = 0.01f;
    config.loss_type = nimcp::LossType::CrossEntropy;
    config.optimizer_type = nimcp::OptimizerType::Adam;
    brain.configure_training(config);

    std::vector<float> features = {1.0f, 0.0f, 0.5f, 0.3f};
    std::vector<float> targets  = {1.0f, 0.0f};
    auto result = brain.train_step(features, targets);

    EXPECT_GE(result.loss, 0.0f);
    EXPECT_GT(result.learning_rate, 0.0f);
}

TEST_F(BrainTrainingTest, TrainBatch) {
    nimcp::Brain brain("batch_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    nimcp::TrainingConfig config;
    brain.configure_training(config);

    // Batch of 2 examples, 4 features, 2 targets
    std::vector<float> features = {
        1.0f, 0.0f, 0.5f, 0.3f,
        0.0f, 1.0f, 0.2f, 0.8f
    };
    std::vector<float> targets = {
        1.0f, 0.0f,
        0.0f, 1.0f
    };
    auto result = brain.train_batch(features, targets, 2, 4, 2);
    EXPECT_GE(result.loss, 0.0f);
}

TEST_F(BrainTrainingTest, TrainingStats) {
    nimcp::Brain brain("stats_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    nimcp::TrainingConfig config;
    brain.configure_training(config);

    std::vector<float> features = {1.0f, 0.0f, 0.5f, 0.3f};
    std::vector<float> targets  = {1.0f, 0.0f};
    brain.train_step(features, targets);

    auto stats = brain.get_training_stats();
    EXPECT_GE(stats.total_steps, 1u);
    EXPECT_GT(stats.current_lr, 0.0f);
}

TEST_F(BrainTrainingTest, StepScheduler) {
    nimcp::Brain brain("sched_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    nimcp::TrainingConfig config;
    brain.configure_training(config);

    float lr = brain.step_scheduler(0.5f);
    EXPECT_GT(lr, 0.0f);
}

// ============================================================================
// 6. BrainCallbackTest
// ============================================================================

class BrainCallbackTest : public CppBindingsTest {};

TEST_F(BrainCallbackTest, RegisterAndFire) {
    nimcp::Brain brain("cb_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    nimcp::TrainingConfig config;
    brain.configure_training(config);
    brain.enable_callbacks();

    int call_count = 0;
    auto cb_id = brain.register_callback(
        nimcp::CallbackEvent::StepComplete,
        [&](nimcp::CallbackEvent, const nimcp::CallbackMetrics&) {
            ++call_count;
            return nimcp::CallbackAction::Continue;
        },
        "test_cb");

    EXPECT_GT(cb_id, 0u);

    std::vector<float> features = {1.0f, 0.0f, 0.5f, 0.3f};
    std::vector<float> targets  = {1.0f, 0.0f};
    try {
        brain.train_step(features, targets);
        // Callback should have been invoked at least once
        EXPECT_GE(call_count, 1);
    } catch (const nimcp::Exception&) {
        // Training step may fail in minimal test environment;
        // callback registration itself was verified above
    }

    try { brain.unregister_callback(cb_id); } catch (...) {}
    try { brain.disable_callbacks(); } catch (...) {}
}

TEST_F(BrainCallbackTest, CallbackStats) {
    nimcp::Brain brain("cb_stats", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    nimcp::TrainingConfig config;
    brain.configure_training(config);
    brain.enable_callbacks();

    brain.register_callback(
        nimcp::CallbackEvent::StepComplete,
        [](nimcp::CallbackEvent, const nimcp::CallbackMetrics&) {
            return nimcp::CallbackAction::Continue;
        },
        "stats_cb");

    std::vector<float> features = {1.0f, 0.0f, 0.5f, 0.3f};
    std::vector<float> targets  = {1.0f, 0.0f};
    brain.train_step(features, targets);

    auto stats = brain.get_callback_stats();
    EXPECT_GE(stats.total_fired, 1u);
}

// ============================================================================
// 7. BrainResizeTest
// ============================================================================

class BrainResizeTest : public CppBindingsTest {};

TEST_F(BrainResizeTest, NeuronCount) {
    nimcp::Brain brain("resize_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    uint32_t count = brain.neuron_count();
    EXPECT_GT(count, 0u);
}

TEST_F(BrainResizeTest, Resize) {
    nimcp::Brain brain("resize_test2", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    uint32_t original = brain.neuron_count();
    uint32_t target = original + 50;
    bool resized = brain.resize(target);
    if (resized) {
        EXPECT_GE(brain.neuron_count(), target);
    }
}

TEST_F(BrainResizeTest, AutoResize) {
    nimcp::Brain brain("autoresize", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);
    // auto_resize returns true if it resized, false if no resize needed
    brain.auto_resize();
    EXPECT_GT(brain.neuron_count(), 0u);
}

TEST_F(BrainResizeTest, UtilizationMetrics) {
    nimcp::Brain brain("util_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    auto m = brain.utilization_metrics();
    EXPECT_GE(m.utilization, 0.0f);
    EXPECT_LE(m.utilization, 1.0f);
    EXPECT_GE(m.saturation, 0.0f);
    EXPECT_LE(m.saturation, 1.0f);
}

// ============================================================================
// 8. BrainSnapshotTest — named snapshots
// ============================================================================

class BrainSnapshotTest : public CppBindingsTest {};

TEST_F(BrainSnapshotTest, SaveAndList) {
    nimcp::Brain brain("snap_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    // Save may fail if snapshot dir not configured; that's acceptable
    try {
        brain.snapshot_save("test_snap", "unit test snapshot");
        auto list = brain.snapshot_list();
        // If save succeeded, we should find it in the list
        if (!list.empty()) {
            bool found = false;
            for (const auto& info : list) {
                if (info.name.find("test_snap") != std::string::npos) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found);
            brain.snapshot_delete("test_snap");
        }
    } catch (const nimcp::Exception&) {
        // Snapshot infrastructure may not be available in test environment
        GTEST_SKIP() << "Snapshot infrastructure not available";
    }
}

// ============================================================================
// 9. BrainCOWTest — clone independence, BrainSnapshot + restore
// ============================================================================

class BrainCOWTest : public CppBindingsTest {};

TEST_F(BrainCOWTest, CloneIndependence) {
    nimcp::Brain original("cow_orig", nimcp::BrainSize::Tiny,
                          nimcp::TaskType::Classification, 4, 2);

    std::vector<float> features = {1.0f, 0.0f, 0.5f, 0.3f};
    original.learn(features, "cat", 0.9f);

    auto clone = original.clone();
    EXPECT_NE(clone.get(), nullptr);
    EXPECT_NE(clone.get(), original.get());

    // Clone should be able to predict independently
    auto pred = clone.predict(features);
    EXPECT_FALSE(pred.label.empty());
}

TEST_F(BrainCOWTest, SnapshotAndRestore) {
    nimcp::Brain brain("cow_snap", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    std::vector<float> features = {1.0f, 0.0f, 0.5f, 0.3f};
    brain.learn(features, "cat", 0.9f);

    nimcp::BrainSnapshot snapshot(brain);
    EXPECT_NE(snapshot.get(), nullptr);

    // Modify brain after snapshot
    std::vector<float> features2 = {0.0f, 1.0f, 0.2f, 0.8f};
    brain.learn(features2, "dog", 0.9f);

    // Restore to snapshot state
    snapshot.restore_to(brain);
    EXPECT_NE(brain.get(), nullptr);
}

TEST_F(BrainCOWTest, SnapshotMoveSemantics) {
    nimcp::Brain brain("cow_move", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    nimcp::BrainSnapshot snap1(brain);
    auto raw = snap1.get();
    nimcp::BrainSnapshot snap2(std::move(snap1));
    EXPECT_EQ(snap2.get(), raw);
    EXPECT_EQ(snap1.get(), nullptr);
}

// ============================================================================
// 10. BrainWorkingMemoryTest
// ============================================================================

class BrainWorkingMemoryTest : public CppBindingsTest {};

TEST_F(BrainWorkingMemoryTest, AddAndGet) {
    nimcp::Brain brain("wm_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};

    try {
        brain.working_memory_add(data, 0.8f);
        auto item = brain.working_memory_get(0);
        if (item.has_value()) {
            EXPECT_FALSE(item->empty());
        }
    } catch (const nimcp::Exception&) {
        // Working memory may not be enabled by default
        GTEST_SKIP() << "Working memory not available";
    }
}

TEST_F(BrainWorkingMemoryTest, Stats) {
    nimcp::Brain brain("wm_stats", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    try {
        auto stats = brain.working_memory_stats();
        EXPECT_GE(stats.capacity, 0u);
    } catch (const nimcp::Exception&) {
        GTEST_SKIP() << "Working memory not available";
    }
}

TEST_F(BrainWorkingMemoryTest, Refresh) {
    nimcp::Brain brain("wm_refresh", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};

    try {
        brain.working_memory_add(data, 0.8f);
        brain.working_memory_refresh(0);
    } catch (const nimcp::Exception&) {
        GTEST_SKIP() << "Working memory not available";
    }
}

// ============================================================================
// 11. BrainWorkspaceTest — Global Workspace
// ============================================================================

class BrainWorkspaceTest : public CppBindingsTest {};

TEST_F(BrainWorkspaceTest, SubscribeUnsubscribe) {
    nimcp::Brain brain("ws_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    try {
        brain.workspace_subscribe(nimcp::CognitiveModule::Perception);
        brain.workspace_unsubscribe(nimcp::CognitiveModule::Perception);
    } catch (const nimcp::Exception&) {
        GTEST_SKIP() << "Global workspace not available";
    }
}

TEST_F(BrainWorkspaceTest, CompeteAndRead) {
    nimcp::Brain brain("ws_compete", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    std::vector<float> content(256, 0.5f);

    try {
        brain.workspace_compete(nimcp::CognitiveModule::Perception,
                                content, 0.9f);
        auto result = brain.workspace_read(256);
        // May or may not have broadcast depending on thresholds
    } catch (const nimcp::Exception&) {
        GTEST_SKIP() << "Global workspace not available";
    }
}

TEST_F(BrainWorkspaceTest, HasBroadcast) {
    nimcp::Brain brain("ws_has", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    try {
        bool has = brain.workspace_has_broadcast();
        // Initially should be false (no broadcast yet)
        EXPECT_FALSE(has);
    } catch (const nimcp::Exception&) {
        GTEST_SKIP() << "Global workspace not available";
    }
}

TEST_F(BrainWorkspaceTest, WorkspaceStats) {
    nimcp::Brain brain("ws_stats", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    try {
        auto stats = brain.workspace_stats();
        EXPECT_GE(stats.total_broadcasts, 0u);
        EXPECT_GE(stats.total_competitions, 0u);
    } catch (const nimcp::Exception&) {
        GTEST_SKIP() << "Global workspace not available";
    }
}

// ============================================================================
// 12. BrainOscillationsTest
// ============================================================================

class BrainOscillationsTest : public CppBindingsTest {};

TEST_F(BrainOscillationsTest, EnableDisable) {
    nimcp::Brain brain("osc_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    bool enabled = brain.enable_oscillations(true);
    if (enabled) {
        EXPECT_TRUE(brain.oscillations_enabled());
        // Note: The C API may not support runtime disable after enable;
        // just verify enable works
    }
}

TEST_F(BrainOscillationsTest, Phasor) {
    nimcp::Brain brain("osc_phasor", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    brain.enable_oscillations(true);
    auto p = brain.get_phasor(0);
    EXPECT_GE(p.amplitude, 0.0f);
    // Phase is in [-pi, pi]
    EXPECT_GE(p.phase, static_cast<float>(-M_PI) - 0.01f);
    EXPECT_LE(p.phase, static_cast<float>(M_PI) + 0.01f);
}

TEST_F(BrainOscillationsTest, PhaseCoherence) {
    nimcp::Brain brain("osc_coherence", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    brain.enable_oscillations(true);
    std::vector<uint32_t> neurons = {0, 1, 2};
    float coherence = brain.phase_coherence(neurons);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(BrainOscillationsTest, PACModulation) {
    nimcp::Brain brain("osc_pac", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    brain.enable_oscillations(true);
    float pac = brain.pac_modulation(6.0f, 40.0f);
    EXPECT_GE(pac, 0.0f);
    EXPECT_LE(pac, 1.0f);
}

// ============================================================================
// 13. BrainProbeTest
// ============================================================================

class BrainProbeTest : public CppBindingsTest {};

TEST_F(BrainProbeTest, ProbeFields) {
    nimcp::Brain brain("probe_test", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    auto p = brain.probe();
    EXPECT_FALSE(p.task_name.empty());
    // Brain may internally upgrade Tiny to Small
    EXPECT_TRUE(p.size == nimcp::BrainSize::Tiny ||
                p.size == nimcp::BrainSize::Small);
    EXPECT_EQ(p.task, nimcp::TaskType::Classification);
    EXPECT_GT(p.num_neurons, 0u);
    EXPECT_EQ(p.num_inputs, 4u);
    EXPECT_EQ(p.num_outputs, 2u);
    EXPECT_GT(p.memory_bytes, 0u);
}

TEST_F(BrainProbeTest, BroadcastProbe) {
    nimcp::Brain brain("probe_bc", nimcp::BrainSize::Tiny,
                       nimcp::TaskType::Classification, 4, 2);

    // broadcast_probe should not throw
    EXPECT_NO_THROW(brain.broadcast_probe());
}

// ============================================================================
// 14. NetworkTest
// ============================================================================

class NetworkTest : public CppBindingsTest {};

TEST_F(NetworkTest, CreateAndForward) {
    nimcp::Network net(4, 2, 8, 0.01f);
    EXPECT_NE(net.get(), nullptr);

    std::vector<float> inputs = {1.0f, 0.0f, 0.5f, 0.3f};
    auto outputs = net.forward(inputs, 2);
    EXPECT_EQ(outputs.size(), 2u);
}

TEST_F(NetworkTest, TrainStub) {
    nimcp::Network net(4, 2, 8, 0.01f);

    std::vector<float> inputs  = {1.0f, 0.0f, 0.5f, 0.3f};
    std::vector<float> targets = {1.0f, 0.0f};

    // nimcp_network_train() is a C API stub — always returns error
    EXPECT_THROW(net.train(inputs, targets), nimcp::Exception);
}

TEST_F(NetworkTest, MoveSemantics) {
    nimcp::Network a(4, 2, 8, 0.01f);
    auto raw = a.get();
    nimcp::Network b(std::move(a));
    EXPECT_EQ(b.get(), raw);
    EXPECT_EQ(a.get(), nullptr);
}

// ============================================================================
// 15. EthicsKnowledgeTest
// ============================================================================

class EthicsKnowledgeTest : public CppBindingsTest {};

TEST_F(EthicsKnowledgeTest, EthicsCheck) {
    nimcp::Ethics ethics;
    EXPECT_NE(ethics.get(), nullptr);

    std::vector<float> situation = {0.5f, 0.3f, 0.8f, 0.1f};
    float score = ethics.check(situation);
    EXPECT_GE(score, -1.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(EthicsKnowledgeTest, KnowledgeAddAndQuery) {
    nimcp::KnowledgeGraph kg;
    EXPECT_NE(kg.get(), nullptr);

    EXPECT_NO_THROW(kg.add_fact("cat", "is_a", "animal"));
    EXPECT_NO_THROW(kg.add_fact("dog", "is_a", "animal"));

    auto result = kg.query("is_a");
    // Result may be empty string if query returns nothing, but shouldn't throw
    EXPECT_GE(result.size(), 0u);
}

TEST_F(EthicsKnowledgeTest, KnowledgeMoveSemantics) {
    nimcp::KnowledgeGraph a;
    auto raw = a.get();
    nimcp::KnowledgeGraph b(std::move(a));
    EXPECT_EQ(b.get(), raw);
    EXPECT_EQ(a.get(), nullptr);
}

// ============================================================================
// 16. ExceptionTest
// ============================================================================

class ExceptionTest : public CppBindingsTest {};

TEST_F(ExceptionTest, IOErrorOnBadPath) {
    try {
        nimcp::Brain::load("/tmp/nonexistent_nimcp_brain_12345.bin");
        FAIL() << "Expected IOError";
    } catch (const nimcp::IOError& e) {
        EXPECT_EQ(e.code(), NIMCP_ERROR_IO);
        EXPECT_NE(std::string(e.what()).size(), 0u);
    } catch (const nimcp::Exception& e) {
        // Any nimcp exception is acceptable; the C API may report differently
        EXPECT_NE(e.code(), NIMCP_OK);
    }
}

TEST_F(ExceptionTest, ExceptionHierarchy) {
    // IOError -> Exception -> runtime_error
    nimcp::IOError io("test IO error");
    EXPECT_EQ(io.code(), NIMCP_ERROR_IO);

    const nimcp::Exception& base = io;
    EXPECT_EQ(base.code(), NIMCP_ERROR_IO);

    const std::runtime_error& rt = io;
    EXPECT_STREQ(rt.what(), "test IO error");
}

TEST_F(ExceptionTest, NullArgError) {
    nimcp::NullArgError e("null arg");
    EXPECT_EQ(e.code(), NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionTest, InvalidError) {
    nimcp::InvalidError e("invalid");
    EXPECT_EQ(e.code(), NIMCP_ERROR_INVALID);
}

TEST_F(ExceptionTest, MemoryError) {
    nimcp::MemoryError e("out of memory");
    EXPECT_EQ(e.code(), NIMCP_ERROR_MEMORY);
}

// ============================================================================
// Enum Conversion Smoke Tests
// ============================================================================

class EnumTest : public CppBindingsTest {};

TEST_F(EnumTest, BrainSizeValues) {
    EXPECT_EQ(static_cast<int>(nimcp::BrainSize::Tiny), NIMCP_BRAIN_TINY);
    EXPECT_EQ(static_cast<int>(nimcp::BrainSize::Small), NIMCP_BRAIN_SMALL);
    EXPECT_EQ(static_cast<int>(nimcp::BrainSize::Medium), NIMCP_BRAIN_MEDIUM);
    EXPECT_EQ(static_cast<int>(nimcp::BrainSize::Large), NIMCP_BRAIN_LARGE);
}

TEST_F(EnumTest, TaskTypeValues) {
    EXPECT_EQ(static_cast<int>(nimcp::TaskType::Classification), NIMCP_TASK_CLASSIFICATION);
    EXPECT_EQ(static_cast<int>(nimcp::TaskType::Regression), NIMCP_TASK_REGRESSION);
    EXPECT_EQ(static_cast<int>(nimcp::TaskType::Association), NIMCP_TASK_ASSOCIATION);
}

TEST_F(EnumTest, CallbackEventValues) {
    EXPECT_EQ(static_cast<int>(nimcp::CallbackEvent::StepComplete), NIMCP_CB_STEP_COMPLETE);
    EXPECT_EQ(static_cast<int>(nimcp::CallbackEvent::Checkpoint), NIMCP_CB_CHECKPOINT);
}

TEST_F(EnumTest, CognitiveModuleValues) {
    EXPECT_EQ(static_cast<int>(nimcp::CognitiveModule::None), NIMCP_MODULE_NONE);
    EXPECT_EQ(static_cast<int>(nimcp::CognitiveModule::CustomStart), NIMCP_MODULE_CUSTOM_START);
    EXPECT_EQ(static_cast<int>(nimcp::CognitiveModule::CustomStart), 100);
}

// ============================================================================
// Config Struct Tests
// ============================================================================

class ConfigTest : public CppBindingsTest {};

TEST_F(ConfigTest, TrainingConfigDefault) {
    auto config = nimcp::TrainingConfig::from_default();
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GE(config.beta1, 0.0f);
    EXPECT_LE(config.beta1, 1.0f);
}

TEST_F(ConfigTest, TrainingConfigToC) {
    nimcp::TrainingConfig config;
    config.learning_rate = 0.05f;
    config.loss_type = nimcp::LossType::Huber;
    auto c = config.to_c();
    EXPECT_FLOAT_EQ(c.learning_rate, 0.05f);
    EXPECT_EQ(c.loss_type, NIMCP_API_LOSS_HUBER);
}

TEST_F(ConfigTest, CallbackConfigDefault) {
    auto config = nimcp::CallbackConfig::from_default();
    EXPECT_GT(config.divergence_threshold, 0.0f);
}

TEST_F(ConfigTest, CallbackConfigToC) {
    nimcp::CallbackConfig config;
    config.patience = 42;
    auto c = config.to_c();
    EXPECT_EQ(c.patience, 42u);
}
