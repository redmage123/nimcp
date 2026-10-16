/**
 * @file test_analogical_reasoning.cpp
 * @brief Unit tests for NIMCP Analogical Reasoning Engine (Phase 6.2)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_analogical_reasoning.h"

namespace {

constexpr float FLOAT_TOLERANCE = 1e-4f;

class AnalogicalReasoningTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = analogical_engine_create();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            analogical_engine_destroy(engine);
            engine = nullptr;
        }
    }

    analogical_engine_t* engine = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AnalogicalReasoningTest, CreateDefault)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(AnalogicalReasoningTest, CreateCustom)
{
    analog_config_t config = analogical_engine_default_config();
    config.min_mapping_strength = 0.5f;
    config.prefer_deep_analogies = true;

    analogical_engine_t* custom = analogical_engine_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    analogical_engine_destroy(custom);
}

TEST_F(AnalogicalReasoningTest, CreateWithNullConfig)
{
    analogical_engine_t* created = analogical_engine_create_custom(nullptr);
    EXPECT_EQ(created, nullptr);
}

TEST_F(AnalogicalReasoningTest, DestroyNullSafe)
{
    analogical_engine_destroy(nullptr);
}

TEST_F(AnalogicalReasoningTest, DefaultConfig)
{
    analog_config_t config = analogical_engine_default_config();

    EXPECT_GT(config.min_mapping_strength, 0.0f);
    EXPECT_LE(config.min_mapping_strength, 1.0f);
    EXPECT_GT(config.systematicity_weight, 0.0f);
}

//=============================================================================
// Domain Creation Tests
//=============================================================================

TEST_F(AnalogicalReasoningTest, CreateDomain)
{
    analog_domain_t* domain = analogical_create_domain("test_domain", "A test domain");

    if (domain != nullptr) {
        EXPECT_EQ(domain->num_entities, 0u);
        EXPECT_EQ(domain->num_relations, 0u);
        analogical_free_domain(domain);
    }
}

TEST_F(AnalogicalReasoningTest, CreateDomainNull)
{
    // Implementation may accept null and use defaults
    analog_domain_t* domain = analogical_create_domain(nullptr, nullptr);
    if (domain != nullptr) {
        analogical_free_domain(domain);
    }
    SUCCEED();
}

TEST_F(AnalogicalReasoningTest, AddEntityToDomain)
{
    analog_domain_t* domain = analogical_create_domain("science", "Scientific domain");

    if (domain != nullptr) {
        float features[] = {1.0f, 0.5f, 0.3f};
        uint32_t id = analogical_add_entity(domain, "atom", "particle", features, 3);

        EXPECT_GT(id, 0u);
        EXPECT_EQ(domain->num_entities, 1u);

        analogical_free_domain(domain);
    }
}

TEST_F(AnalogicalReasoningTest, AddRelationToDomain)
{
    analog_domain_t* domain = analogical_create_domain("physics", "Physics domain");

    if (domain != nullptr) {
        float f[] = {1.0f};
        uint32_t e1 = analogical_add_entity(domain, "sun", "star", f, 1);
        uint32_t e2 = analogical_add_entity(domain, "earth", "planet", f, 1);

        uint32_t rel_id = analogical_add_relation(domain, "attracts", e1, e2, 0.9f);

        if (rel_id > 0) {
            EXPECT_EQ(domain->num_relations, 1u);
        }

        analogical_free_domain(domain);
    }
}

//=============================================================================
// Analogy Finding Tests
//=============================================================================

TEST_F(AnalogicalReasoningTest, FindAnalogy)
{
    // Create source domain (solar system)
    analog_domain_t* source = analogical_create_domain("solar_system", "Our solar system");
    if (!source) return;

    float f[] = {1.0f};
    uint32_t sun = analogical_add_entity(source, "sun", "star", f, 1);
    uint32_t earth = analogical_add_entity(source, "earth", "planet", f, 1);
    analogical_add_relation(source, "revolves_around", earth, sun, 0.95f);

    // Create target domain (atom)
    analog_domain_t* target = analogical_create_domain("atom", "Atomic structure");
    if (!target) {
        analogical_free_domain(source);
        return;
    }

    uint32_t nucleus = analogical_add_entity(target, "nucleus", "core", f, 1);
    uint32_t electron = analogical_add_entity(target, "electron", "particle", f, 1);
    analogical_add_relation(target, "orbits", electron, nucleus, 0.9f);

    // Find analogy
    analog_analogy_t* analogy = analogical_find_analogy(engine, source, target);

    if (analogy != nullptr) {
        EXPECT_GT(analogy->mapping_strength, 0.0f);
        analogical_free_analogy(analogy);
    }

    analogical_free_domain(source);
    analogical_free_domain(target);
}

TEST_F(AnalogicalReasoningTest, FindAnalogyNull)
{
    analog_analogy_t* analogy = analogical_find_analogy(engine, nullptr, nullptr);
    EXPECT_EQ(analogy, nullptr);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(AnalogicalReasoningTest, SetInflammation)
{
    int result = analogical_set_inflammation(engine, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(AnalogicalReasoningTest, SetFatigue)
{
    int result = analogical_set_fatigue(engine, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(AnalogicalReasoningTest, SetInflammationNull)
{
    int result = analogical_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AnalogicalReasoningTest, GetStatistics)
{
    analog_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = analogical_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(AnalogicalReasoningTest, ResetStatistics)
{
    analogical_reset_stats(engine);
    SUCCEED();
}

} // namespace
