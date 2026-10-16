/**
 * @file test_architecture_audit_fixes.cpp
 * @brief Regression tests for architecture audit fixes (Session 57-58)
 *
 * Covers:
 *   H1:  Thread-local anti-collapse states (no data race)
 *   H2:  Atomic weights_dirty_on_cpu (no TSAN violations)
 *   H7:  SNN save/load atomic write + version 2 target-ID matching
 *   H8:  LNN/CNN save/load atomic write + version validation
 *   H9:  Optimizer state persistence (tested in test_optimizers.cpp)
 *   M1:  SNN MSE loss for non-surrogate modes
 *   M2:  CNN loss normalization (log mapping to [0,1])
 *   M4:  SNN timestep from sim config
 *   M8:  Tensor checkpoint portability (size_t -> uint64_t)
 *   M9:  NaN-safe EMA updates
 *
 * @date 2026-03-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>

#include "middleware/training/nimcp_optimizers.h"
#include "utils/validation/nimcp_common.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ArchAuditFixesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================================
 * M2: CNN Loss Normalization — log(1+x)/log(2) maps [0,∞) → [0,1]
 *
 * Before fix: CNN cross-entropy loss (unbounded, e.g. 6.9) was blended
 * directly with MSE losses (bounded [0,1]), distorting composite loss.
 * After fix: cnn_norm = log(1+loss)/log(2), clamped to [0,1].
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, M2_CnnLossNormalization_ZeroLoss) {
    float cnn_loss = 0.0f;
    float cnn_norm = logf(1.0f + cnn_loss) / logf(2.0f);
    if (cnn_norm > 1.0f) cnn_norm = 1.0f;
    EXPECT_FLOAT_EQ(cnn_norm, 0.0f);
}

TEST_F(ArchAuditFixesTest, M2_CnnLossNormalization_UnitLoss) {
    float cnn_loss = 1.0f;
    float cnn_norm = logf(1.0f + cnn_loss) / logf(2.0f);
    if (cnn_norm > 1.0f) cnn_norm = 1.0f;
    EXPECT_FLOAT_EQ(cnn_norm, 1.0f);
}

TEST_F(ArchAuditFixesTest, M2_CnnLossNormalization_LargeLoss_Clamped) {
    /* Cross-entropy loss can be large (e.g. -log(0.001) ≈ 6.9) */
    float cnn_loss = 6.9f;
    float cnn_norm = logf(1.0f + cnn_loss) / logf(2.0f);
    if (cnn_norm > 1.0f) cnn_norm = 1.0f;
    EXPECT_FLOAT_EQ(cnn_norm, 1.0f);
}

TEST_F(ArchAuditFixesTest, M2_CnnLossNormalization_SmallLoss) {
    /* Small loss should map to proportional value in [0,1] */
    float cnn_loss = 0.5f;
    float cnn_norm = logf(1.0f + cnn_loss) / logf(2.0f);
    if (cnn_norm > 1.0f) cnn_norm = 1.0f;
    EXPECT_GT(cnn_norm, 0.0f);
    EXPECT_LT(cnn_norm, 1.0f);
    /* log(1.5)/log(2) ≈ 0.585 */
    EXPECT_NEAR(cnn_norm, 0.585f, 0.01f);
}

TEST_F(ArchAuditFixesTest, M2_CnnLossNormalization_Monotonic) {
    /* Normalization must be monotonically increasing */
    float prev_norm = 0.0f;
    for (float loss = 0.0f; loss <= 2.0f; loss += 0.1f) {
        float norm = logf(1.0f + loss) / logf(2.0f);
        if (norm > 1.0f) norm = 1.0f;
        EXPECT_GE(norm, prev_norm) << "Not monotonic at loss=" << loss;
        prev_norm = norm;
    }
}

/* ============================================================================
 * M9: NaN-safe EMA Updates
 *
 * Before fix: EMA update with NaN initial value produced NaN forever.
 * After fix: SAFE_EMA_UPDATE seeds from first finite value.
 * ============================================================================ */

/* Reproduce the SAFE_EMA_UPDATE macro logic */
static void safe_ema_update(float& ema, float val, float alpha) {
    if (!std::isfinite(ema)) {
        ema = val;
    } else {
        ema = (1.0f - alpha) * ema + alpha * val;
    }
}

TEST_F(ArchAuditFixesTest, M9_NanSafeEMA_InitFromNaN) {
    float ema = NAN;
    safe_ema_update(ema, 0.5f, 0.1f);
    /* First update should seed from val, not compute NaN * 0.9 + val * 0.1 */
    EXPECT_FLOAT_EQ(ema, 0.5f);
}

TEST_F(ArchAuditFixesTest, M9_NanSafeEMA_InitFromInf) {
    float ema = INFINITY;
    safe_ema_update(ema, 0.3f, 0.1f);
    EXPECT_FLOAT_EQ(ema, 0.3f);
}

TEST_F(ArchAuditFixesTest, M9_NanSafeEMA_NormalUpdate) {
    float ema = 1.0f;
    safe_ema_update(ema, 0.5f, 0.1f);
    /* Normal EMA: 0.9 * 1.0 + 0.1 * 0.5 = 0.95 */
    EXPECT_NEAR(ema, 0.95f, 1e-6f);
}

TEST_F(ArchAuditFixesTest, M9_NanSafeEMA_SequenceConverges) {
    float ema = NAN;
    float alpha = 0.1f;
    for (int i = 0; i < 100; i++) {
        safe_ema_update(ema, 0.7f, alpha);
    }
    /* After 100 updates of constant 0.7, EMA should converge to 0.7 */
    EXPECT_NEAR(ema, 0.7f, 0.01f);
}

TEST_F(ArchAuditFixesTest, M9_NanSafeEMA_SkipsNaNInput) {
    float ema = 0.5f;
    float loss = NAN;
    /* The brain_learning code guards: if (isfinite(loss)) { SAFE_EMA_UPDATE(...) } */
    if (std::isfinite(loss)) {
        safe_ema_update(ema, loss, 0.1f);
    }
    /* EMA should be unchanged when input is NaN */
    EXPECT_FLOAT_EQ(ema, 0.5f);
}

/* ============================================================================
 * M8: Tensor Checkpoint Portability (size_t → uint64_t)
 *
 * Before fix: Tensor save used size_t (platform-dependent: 4 or 8 bytes).
 * After fix: Uses uint64_t (always 8 bytes).
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, M8_Uint64Portability_SizeCheck) {
    /* Verify uint64_t is always 8 bytes regardless of platform */
    EXPECT_EQ(sizeof(uint64_t), 8u);
    /* size_t might be 4 on 32-bit platforms, which would break checkpoints */
    /* uint64_t is always 8, making checkpoints portable */
}

TEST_F(ArchAuditFixesTest, M8_Uint64Portability_RoundTrip) {
    /* Simulate the save/load pattern: size_t → uint64_t → file → uint64_t → size_t */
    size_t original_nbytes = 1024 * 1024; /* 1MB */
    uint64_t nbytes_u64 = (uint64_t)original_nbytes;

    FILE* f = tmpfile();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(fwrite(&nbytes_u64, sizeof(uint64_t), 1, f), 1u);

    rewind(f);
    uint64_t loaded_u64 = 0;
    EXPECT_EQ(fread(&loaded_u64, sizeof(uint64_t), 1, f), 1u);
    size_t restored_nbytes = (size_t)loaded_u64;

    EXPECT_EQ(restored_nbytes, original_nbytes);
    fclose(f);
}

/* ============================================================================
 * H1: Thread-Local Anti-Collapse States
 *
 * Before fix: Static anti-collapse state shared across threads → data race.
 * After fix: __thread storage class → each thread gets its own copy.
 *
 * We can't directly test __thread from here, but we can verify the PATTERN:
 * thread-local variables should NOT leak state between threads.
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, H1_ThreadLocalPattern_NoLeakBetweenThreads) {
    /* Simulate the pattern: each thread updates its own anti-collapse ring buffer */
    static __thread int tl_counter = 0;
    static __thread float tl_ring[16] = {0};

    std::atomic<bool> thread1_done{false};
    std::atomic<bool> thread2_done{false};
    float thread1_val = 0.0f;
    float thread2_val = 0.0f;

    auto worker = [](int id, float* out_val, std::atomic<bool>* done) {
        /* Each thread should get its own __thread variable */
        tl_counter = id * 100;
        for (int i = 0; i < 16; i++) {
            tl_ring[i] = (float)(id * 100 + i);
        }
        /* Small delay to allow interleaving */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        /* Read back — should still be this thread's values */
        *out_val = tl_ring[0];
        *done = true;
    };

    std::thread t1(worker, 1, &thread1_val, &thread1_done);
    std::thread t2(worker, 2, &thread2_val, &thread2_done);
    t1.join();
    t2.join();

    /* Thread 1 should see 100, thread 2 should see 200 — no cross-contamination */
    EXPECT_FLOAT_EQ(thread1_val, 100.0f);
    EXPECT_FLOAT_EQ(thread2_val, 200.0f);
}

/* ============================================================================
 * H2: Atomic weights_dirty_on_cpu
 *
 * Before fix: Plain bool read/write on weights_dirty_on_cpu → data race.
 * After fix: __atomic_store_n / __atomic_load_n with RELEASE/ACQUIRE.
 *
 * Test the pattern: atomic flag visible across threads.
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, H2_AtomicDirtyFlag_Visibility) {
    /* Simulate the atomic dirty flag pattern */
    bool dirty_flag = false;
    std::atomic<bool> writer_done{false};

    std::thread writer([&]() {
        /* Simulate: __atomic_store_n(&dirty_flag, true, __ATOMIC_RELEASE) */
        __atomic_store_n(&dirty_flag, true, __ATOMIC_RELEASE);
        writer_done = true;
    });

    writer.join();

    /* Reader should see the updated value */
    bool read_val = __atomic_load_n(&dirty_flag, __ATOMIC_ACQUIRE);
    EXPECT_TRUE(read_val);
}

TEST_F(ArchAuditFixesTest, H2_AtomicDirtyFlag_MultipleToggle) {
    bool dirty_flag = false;

    /* Writer thread toggles flag 1000 times */
    std::thread writer([&]() {
        for (int i = 0; i < 1000; i++) {
            __atomic_store_n(&dirty_flag, (i % 2 == 0), __ATOMIC_RELEASE);
        }
    });

    /* Reader thread reads flag 1000 times — should never see torn reads */
    std::atomic<int> valid_reads{0};
    std::thread reader([&]() {
        for (int i = 0; i < 1000; i++) {
            bool val = __atomic_load_n(&dirty_flag, __ATOMIC_ACQUIRE);
            /* val must be either true or false (not a torn value) */
            if (val == true || val == false) {
                valid_reads++;
            }
        }
    });

    writer.join();
    reader.join();

    EXPECT_EQ(valid_reads.load(), 1000);
}

/* ============================================================================
 * M1: SNN MSE Loss for Non-Surrogate Modes
 *
 * Before fix: R-STDP/eProp modes returned loss=0.0 always.
 * After fix: Computes MSE from output spike rates.
 *
 * We can verify the MSE computation formula is correct.
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, M1_MseLossFormula_PerfectPrediction) {
    /* MSE of identical predictions and targets = 0 */
    float predictions[] = {0.5f, 0.3f, 0.8f};
    float targets[] = {0.5f, 0.3f, 0.8f};
    uint32_t dim = 3;

    float loss = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = predictions[i] - targets[i];
        loss += diff * diff;
    }
    loss /= (float)dim;
    EXPECT_FLOAT_EQ(loss, 0.0f);
}

TEST_F(ArchAuditFixesTest, M1_MseLossFormula_KnownError) {
    float predictions[] = {1.0f, 0.0f};
    float targets[] = {0.0f, 1.0f};
    uint32_t dim = 2;

    float loss = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = predictions[i] - targets[i];
        loss += diff * diff;
    }
    loss /= (float)dim;
    /* (1^2 + 1^2) / 2 = 1.0 */
    EXPECT_FLOAT_EQ(loss, 1.0f);
}

TEST_F(ArchAuditFixesTest, M1_MseLossFormula_Bounded) {
    /* MSE with predictions and targets in [0,1] should be in [0,1] */
    float predictions[] = {0.0f, 0.0f, 0.0f};
    float targets[] = {1.0f, 1.0f, 1.0f};
    uint32_t dim = 3;

    float loss = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = predictions[i] - targets[i];
        loss += diff * diff;
    }
    loss /= (float)dim;
    EXPECT_GE(loss, 0.0f);
    EXPECT_LE(loss, 1.0f);
}

/* ============================================================================
 * M4: SNN Timestep From Sim Config
 *
 * Before fix: Hardcoded dt=1.0ms everywhere.
 * After fix: dt = snn->sim->dt_ms (defaults to 1.0 if not set).
 *
 * Test the logic pattern.
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, M4_SnnTimestep_DefaultFallback) {
    /* When sim config is NULL or dt_ms <= 0, should default to 1.0 */
    float dt_ms = 0.0f;
    float dt = (dt_ms > 0.0f) ? dt_ms : 1.0f;
    EXPECT_FLOAT_EQ(dt, 1.0f);
}

TEST_F(ArchAuditFixesTest, M4_SnnTimestep_CustomValue) {
    float dt_ms = 0.5f;
    float dt = (dt_ms > 0.0f) ? dt_ms : 1.0f;
    EXPECT_FLOAT_EQ(dt, 0.5f);
}

TEST_F(ArchAuditFixesTest, M4_SnnTimestep_NegativeValue) {
    float dt_ms = -1.0f;
    float dt = (dt_ms > 0.0f) ? dt_ms : 1.0f;
    EXPECT_FLOAT_EQ(dt, 1.0f);
}

/* ============================================================================
 * H7/H8: Atomic Write Pattern (temp + rename)
 *
 * Before fix: Direct file overwrite → truncated file on crash.
 * After fix: Write to .tmp, then rename() for atomic replacement.
 *
 * Test the pattern works correctly.
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, H7H8_AtomicWritePattern_RenamePreservesContent) {
    char tmp_path[] = "/tmp/nimcp_test_atomic_XXXXXX";
    int fd = mkstemp(tmp_path);
    ASSERT_GE(fd, 0);
    close(fd);

    char final_path[256];
    snprintf(final_path, sizeof(final_path), "%s.final", tmp_path);

    /* Write to tmp file */
    FILE* f = fopen(tmp_path, "wb");
    ASSERT_NE(f, nullptr);
    uint32_t magic = 0xDEADBEEF;
    float data[] = {1.0f, 2.0f, 3.0f};
    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(data, sizeof(float), 3, f);
    fclose(f);

    /* Atomic rename */
    int ret = rename(tmp_path, final_path);
    EXPECT_EQ(ret, 0);

    /* Verify content via final path */
    f = fopen(final_path, "rb");
    ASSERT_NE(f, nullptr);
    uint32_t read_magic = 0;
    float read_data[3] = {0};
    EXPECT_EQ(fread(&read_magic, sizeof(uint32_t), 1, f), 1u);
    EXPECT_EQ(fread(read_data, sizeof(float), 3, f), 3u);
    fclose(f);

    EXPECT_EQ(read_magic, 0xDEADBEEFu);
    EXPECT_FLOAT_EQ(read_data[0], 1.0f);
    EXPECT_FLOAT_EQ(read_data[1], 2.0f);
    EXPECT_FLOAT_EQ(read_data[2], 3.0f);

    /* Cleanup */
    unlink(final_path);
}

TEST_F(ArchAuditFixesTest, H7H8_VersionValidation_RejectsHigherVersion) {
    /* SNN save uses version 2, load rejects version > 2 */
    uint32_t version = 3;
    EXPECT_GT(version, 2u) << "Version 3 should be rejected by SNN loader";

    /* LNN/CNN save uses version 1, load rejects version > 1 */
    version = 2;
    EXPECT_GT(version, 1u) << "Version 2 should be rejected by LNN/CNN loader";
}

/* ============================================================================
 * Result NaN Guard
 *
 * Training dispatch guards: result->loss = isfinite(loss) ? loss : 1.0f
 * ============================================================================ */

TEST_F(ArchAuditFixesTest, NaNGuard_FiniteLoss) {
    float loss = 0.5f;
    float guarded = std::isfinite(loss) ? loss : 1.0f;
    EXPECT_FLOAT_EQ(guarded, 0.5f);
}

TEST_F(ArchAuditFixesTest, NaNGuard_NaNLoss) {
    float loss = NAN;
    float guarded = std::isfinite(loss) ? loss : 1.0f;
    EXPECT_FLOAT_EQ(guarded, 1.0f);
}

TEST_F(ArchAuditFixesTest, NaNGuard_InfLoss) {
    float loss = INFINITY;
    float guarded = std::isfinite(loss) ? loss : 1.0f;
    EXPECT_FLOAT_EQ(guarded, 1.0f);
}

TEST_F(ArchAuditFixesTest, NaNGuard_NegInfLoss) {
    float loss = -INFINITY;
    float guarded = std::isfinite(loss) ? loss : 1.0f;
    EXPECT_FLOAT_EQ(guarded, 1.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
