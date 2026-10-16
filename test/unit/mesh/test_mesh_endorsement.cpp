/**
 * @file test_mesh_endorsement.cpp
 * @brief Unit tests for mesh endorsement module
 *
 * Tests pattern-based endorser selection and collection.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_endorsement.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
}

class MeshEndorsementTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_pattern_router_t* router;
    mesh_endorsement_collector_t* collector;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);

        mesh_pattern_router_config_t router_config = {
            .default_threshold = 0.5f,
            .competition_strength = 0.3f,
            .enable_learning = true,
            .learning_rate = 0.1f,
            .max_endorsers = 16
        };
        router = mesh_pattern_router_create(&router_config);
        ASSERT_NE(router, nullptr);

        collector = nullptr;
    }

    void TearDown() override {
        if (collector) {
            mesh_endorsement_collector_destroy(collector);
            collector = nullptr;
        }
        if (router) {
            mesh_pattern_router_destroy(router);
            router = nullptr;
        }
        if (registry) {
            mesh_registry_destroy(registry);
            registry = nullptr;
        }
    }

    mesh_participant_id_t register_test_participant(const char* name) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.module_name = name;
        config.type = MESH_PARTICIPANT_MODULE;
        config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

        mesh_participant_id_t id;
        nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &id);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        return id;
    }

    void register_receptive_field(mesh_participant_id_t id, const float* pattern, size_t pattern_len, float threshold = 0.5f) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);  /* Zeros all 64 dims */
        field.threshold = threshold;
        field.sharpness = 1.0f;
        field.pattern_count = 1;

        /* Only copy provided values, rest remain 0 from init */
        for (size_t i = 0; i < pattern_len && i < MESH_PATTERN_DIM; i++) {
            field.preferred[0].vector[i] = pattern[i];
        }
        field.preferred[0].magnitude = 1.0f;
        field.preferred[0].active_dims = (uint32_t)pattern_len;

        mesh_pattern_router_register_receptive_field(router, id, &field);
    }

    mesh_pattern_t create_pattern(const float* values, size_t count) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);
        float magnitude = 0.0f;
        for (size_t i = 0; i < count && i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = values[i];
            magnitude += values[i] * values[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = (uint32_t)count;
        return pattern;
    }

    mesh_transaction_t* create_test_transaction(mesh_participant_id_t proposer) {
        return mesh_transaction_create(
            MESH_TX_BELIEF_UPDATE,
            proposer,
            MESH_CHANNEL_LEFT_HEMISPHERE
        );
    }
};

/* ============================================================================
 * Collector Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, CreateCollector) {
    collector = mesh_endorsement_collector_create(nullptr, router);
    ASSERT_NE(collector, nullptr);
}

TEST_F(MeshEndorsementTest, CreateCollectorWithConfig) {
    mesh_endorsement_collector_config_t config;
    mesh_endorsement_collector_default_config(&config);
    config.max_concurrent = 32;
    config.default_timeout_ms = 200.0f;

    collector = mesh_endorsement_collector_create(&config, router);
    ASSERT_NE(collector, nullptr);
}

TEST_F(MeshEndorsementTest, CreateCollectorWithoutRouter) {
    collector = mesh_endorsement_collector_create(nullptr, nullptr);
    ASSERT_NE(collector, nullptr);  // Should still create, but selection will fail
}

TEST_F(MeshEndorsementTest, DestroyCollectorNull) {
    mesh_endorsement_collector_destroy(nullptr);  // Should not crash
}

TEST_F(MeshEndorsementTest, DefaultConfig) {
    mesh_endorsement_collector_config_t config;
    nimcp_error_t err = mesh_endorsement_collector_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.max_concurrent, 0);
    EXPECT_GT(config.default_timeout_ms, 0.0f);
    EXPECT_EQ(config.default_quorum.required_ratio, 1.0f);
    EXPECT_TRUE(config.default_quorum.allow_veto);
}

TEST_F(MeshEndorsementTest, DefaultConfigNull) {
    nimcp_error_t err = mesh_endorsement_collector_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Role From Activation Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, RoleFromActivationRequired) {
    endorser_role_t role = mesh_endorsement_role_from_activation(0.95f);
    EXPECT_EQ(role, ENDORSER_ROLE_REQUIRED);
}

TEST_F(MeshEndorsementTest, RoleFromActivationPreferred) {
    endorser_role_t role = mesh_endorsement_role_from_activation(0.8f);
    EXPECT_EQ(role, ENDORSER_ROLE_PREFERRED);
}

TEST_F(MeshEndorsementTest, RoleFromActivationOptional) {
    endorser_role_t role = mesh_endorsement_role_from_activation(0.6f);
    EXPECT_EQ(role, ENDORSER_ROLE_OPTIONAL);
}

TEST_F(MeshEndorsementTest, RoleFromActivationVeto) {
    endorser_role_t role = mesh_endorsement_role_from_activation(-0.7f);
    EXPECT_EQ(role, ENDORSER_ROLE_VETO);
}

TEST_F(MeshEndorsementTest, RoleFromActivationBelowThreshold) {
    endorser_role_t role = mesh_endorsement_role_from_activation(0.3f);
    EXPECT_EQ(role, ENDORSER_ROLE_OPTIONAL);  // Default for in-between values
}

/* ============================================================================
 * Endorser Selection Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, SelectEndorsersNoModules) {
    collector = mesh_endorsement_collector_create(nullptr, router);
    ASSERT_NE(collector, nullptr);

    float pattern_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pattern_values, 4);

    endorser_set_t endorsers;
    nimcp_error_t err = mesh_endorsement_select_endorsers(collector, &pattern, &endorsers);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(endorsers.count, 0);  // No modules registered
}

TEST_F(MeshEndorsementTest, SelectEndorsersWithMatchingModule) {
    mesh_participant_id_t id = register_test_participant("motor_cortex");

    float rf_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    register_receptive_field(id, rf_pattern, 4);

    collector = mesh_endorsement_collector_create(nullptr, router);
    ASSERT_NE(collector, nullptr);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};  // Matches receptive field
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    endorser_set_t endorsers;
    nimcp_error_t err = mesh_endorsement_select_endorsers(collector, &pattern, &endorsers);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(endorsers.count, 1);  // Module should be selected
}

TEST_F(MeshEndorsementTest, SelectEndorsersNullCollector) {
    mesh_pattern_t pattern;
    mesh_pattern_init(&pattern);
    endorser_set_t endorsers;

    nimcp_error_t err = mesh_endorsement_select_endorsers(nullptr, &pattern, &endorsers);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshEndorsementTest, SelectEndorsersNullPattern) {
    collector = mesh_endorsement_collector_create(nullptr, router);
    endorser_set_t endorsers;

    nimcp_error_t err = mesh_endorsement_select_endorsers(collector, nullptr, &endorsers);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Collection Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, StartCollectionSuccess) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    mesh_participant_id_t endorser_id = register_test_participant("endorser");

    float rf_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    register_receptive_field(endorser_id, rf_pattern, 4);

    collector = mesh_endorsement_collector_create(nullptr, router);
    ASSERT_NE(collector, nullptr);

    mesh_transaction_t* tx = create_test_transaction(proposer);
    ASSERT_NE(tx, nullptr);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    nimcp_error_t err = mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshEndorsementTest, StartCollectionNullCollector) {
    mesh_transaction_t tx = {0};
    mesh_pattern_t pattern;
    mesh_pattern_init(&pattern);

    nimcp_error_t err = mesh_endorsement_start_collection(nullptr, &tx, &pattern, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshEndorsementTest, StartCollectionDuplicate) {
    mesh_participant_id_t proposer = register_test_participant("proposer");

    collector = mesh_endorsement_collector_create(nullptr, router);
    ASSERT_NE(collector, nullptr);

    mesh_transaction_t* tx = create_test_transaction(proposer);
    ASSERT_NE(tx, nullptr);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    nimcp_error_t err1 = mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);
    EXPECT_EQ(err1, NIMCP_SUCCESS);

    nimcp_error_t err2 = mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);
    EXPECT_EQ(err2, NIMCP_ERROR_ALREADY_EXISTS);

    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Add Endorsement Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, AddEndorsementSuccess) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    mesh_participant_id_t endorser_id = register_test_participant("endorser");

    float rf_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    register_receptive_field(endorser_id, rf_pattern, 4);

    collector = mesh_endorsement_collector_create(nullptr, router);
    mesh_transaction_t* tx = create_test_transaction(proposer);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);

    mesh_endorsement_t endorsement;
    mesh_endorsement_create(endorser_id, ENDORSEMENT_APPROVED, &endorsement);

    nimcp_error_t err = mesh_endorsement_add(collector, &tx->id, &endorsement);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshEndorsementTest, AddEndorsementNotFound) {
    collector = mesh_endorsement_collector_create(nullptr, router);

    mesh_tx_id_t fake_id = {1234, 0, 0};
    mesh_endorsement_t endorsement;
    mesh_endorsement_create(1, ENDORSEMENT_APPROVED, &endorsement);

    nimcp_error_t err = mesh_endorsement_add(collector, &fake_id, &endorsement);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Collection Status Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, IsCompleteInitiallyFalse) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    collector = mesh_endorsement_collector_create(nullptr, router);
    mesh_transaction_t* tx = create_test_transaction(proposer);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);

    EXPECT_FALSE(mesh_endorsement_is_complete(collector, &tx->id));

    mesh_transaction_destroy(tx);
}

TEST_F(MeshEndorsementTest, QuorumMetInitiallyFalse) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    collector = mesh_endorsement_collector_create(nullptr, router);
    mesh_transaction_t* tx = create_test_transaction(proposer);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);

    EXPECT_FALSE(mesh_endorsement_quorum_met(collector, &tx->id));

    mesh_transaction_destroy(tx);
}

TEST_F(MeshEndorsementTest, IsVetoedInitiallyFalse) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    collector = mesh_endorsement_collector_create(nullptr, router);
    mesh_transaction_t* tx = create_test_transaction(proposer);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);

    EXPECT_FALSE(mesh_endorsement_is_vetoed(collector, &tx->id));

    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Cancel Collection Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, CancelCollectionSuccess) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    collector = mesh_endorsement_collector_create(nullptr, router);
    mesh_transaction_t* tx = create_test_transaction(proposer);

    float tx_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(tx_pattern, 4);

    mesh_endorsement_start_collection(collector, tx, &pattern, nullptr);

    nimcp_error_t err = mesh_endorsement_cancel_collection(collector, &tx->id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Collection should no longer exist
    EXPECT_EQ(mesh_endorsement_get_selected(collector, &tx->id), nullptr);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshEndorsementTest, CancelCollectionNotFound) {
    collector = mesh_endorsement_collector_create(nullptr, router);

    mesh_tx_id_t fake_id = {9999, 0, 0};
    nimcp_error_t err = mesh_endorsement_cancel_collection(collector, &fake_id);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, CreateEndorsement) {
    mesh_endorsement_t endorsement;
    nimcp_error_t err = mesh_endorsement_create(12345, ENDORSEMENT_APPROVED, &endorsement);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(endorsement.endorser_id, 12345);
    EXPECT_EQ(endorsement.result, ENDORSEMENT_APPROVED);
    EXPECT_GT(endorsement.timestamp_ns, 0);
}

TEST_F(MeshEndorsementTest, CreateEndorsementNull) {
    nimcp_error_t err = mesh_endorsement_create(12345, ENDORSEMENT_APPROVED, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshEndorsementTest, EndorserSetInit) {
    endorser_set_t set;
    memset(&set, 0xFF, sizeof(set));  // Fill with garbage

    endorser_set_init(&set);

    EXPECT_EQ(set.count, 0);
    EXPECT_EQ(set.required_count, 0);
    EXPECT_EQ(set.preferred_count, 0);
    EXPECT_EQ(set.optional_count, 0);
    EXPECT_EQ(set.veto_count, 0);
}

TEST_F(MeshEndorsementTest, EndorserSetInitNull) {
    endorser_set_init(nullptr);  // Should not crash
}

TEST_F(MeshEndorsementTest, RoleToString) {
    EXPECT_STREQ(endorser_role_to_string(ENDORSER_ROLE_REQUIRED), "REQUIRED");
    EXPECT_STREQ(endorser_role_to_string(ENDORSER_ROLE_PREFERRED), "PREFERRED");
    EXPECT_STREQ(endorser_role_to_string(ENDORSER_ROLE_OPTIONAL), "OPTIONAL");
    EXPECT_STREQ(endorser_role_to_string(ENDORSER_ROLE_VETO), "VETO");
    EXPECT_STREQ(endorser_role_to_string((endorser_role_t)99), "UNKNOWN");
}

TEST_F(MeshEndorsementTest, VerifySignatureValid) {
    mesh_participant_id_t proposer = register_test_participant("proposer");
    mesh_transaction_t* tx = create_test_transaction(proposer);

    mesh_endorsement_t endorsement;
    mesh_endorsement_create(proposer, ENDORSEMENT_APPROVED, &endorsement);

    bool valid = mesh_endorsement_verify_signature(&endorsement, tx, registry);
    EXPECT_TRUE(valid);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshEndorsementTest, VerifySignatureNull) {
    bool valid = mesh_endorsement_verify_signature(nullptr, nullptr, nullptr);
    EXPECT_FALSE(valid);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MeshEndorsementTest, UpdateCollector) {
    collector = mesh_endorsement_collector_create(nullptr, router);

    nimcp_error_t err = mesh_endorsement_collector_update(collector, 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshEndorsementTest, UpdateCollectorNull) {
    nimcp_error_t err = mesh_endorsement_collector_update(nullptr, 100);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Print Tests (just ensure they don't crash)
 * ============================================================================ */

TEST_F(MeshEndorsementTest, PrintSelectedNull) {
    mesh_endorsement_print_selected(nullptr);  // Should not crash
}

TEST_F(MeshEndorsementTest, PrintCollectionNull) {
    mesh_endorsement_print_collection(nullptr, nullptr);  // Should not crash
}

TEST_F(MeshEndorsementTest, PrintSelected) {
    endorser_set_t set;
    endorser_set_init(&set);
    set.count = 2;
    set.endorsers[0].id = 1;
    set.endorsers[0].role = ENDORSER_ROLE_REQUIRED;
    set.endorsers[0].activation = 0.95f;
    set.endorsers[1].id = 2;
    set.endorsers[1].role = ENDORSER_ROLE_OPTIONAL;
    set.endorsers[1].activation = 0.6f;
    set.required_count = 1;
    set.optional_count = 1;

    mesh_endorsement_print_selected(&set);  // Should print without crash
}
