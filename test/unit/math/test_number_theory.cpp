/**
 * @file test_number_theory.cpp
 * @brief Tests for the number theory engine (Google Test)
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/math/nimcp_number_theory.h"
}

/* ---------- lifecycle ---------- */

TEST(NumberTheoryTest, CreateDestroy) {
    number_theory_t *nt = nt_create();
    ASSERT_NE(nt, nullptr);
    nt_destroy(nt);
}

/* ---------- primality (fixture) ---------- */

class NumberTheoryFixture : public ::testing::Test {
protected:
    void SetUp() override {
        nt = nt_create();
        ASSERT_NE(nt, nullptr);
    }
    void TearDown() override {
        nt_destroy(nt);
    }
    number_theory_t *nt;
};

TEST_F(NumberTheoryFixture, IsPrimeTrue) {
    EXPECT_TRUE(nt_is_prime(nt, 997));
    EXPECT_TRUE(nt_is_prime(nt, 2));
    EXPECT_TRUE(nt_is_prime(nt, 3));
    EXPECT_TRUE(nt_is_prime(nt, 7919));
}

TEST_F(NumberTheoryFixture, IsPrimeFalse) {
    EXPECT_FALSE(nt_is_prime(nt, 998));
    EXPECT_FALSE(nt_is_prime(nt, 0));
    EXPECT_FALSE(nt_is_prime(nt, 1));
    EXPECT_FALSE(nt_is_prime(nt, 100));
}

TEST(NumberTheoryTest, IsPrimeTrialDivision) {
    EXPECT_TRUE(nt_is_prime_trial(997));
    EXPECT_FALSE(nt_is_prime_trial(998));
    EXPECT_TRUE(nt_is_prime_trial(104729));
    EXPECT_FALSE(nt_is_prime_trial(104730));
}

/* ---------- GCD / LCM ---------- */

TEST(NumberTheoryTest, GcdKnownValues) {
    EXPECT_EQ(nt_gcd(12, 8), 4);
    EXPECT_EQ(nt_gcd(100, 75), 25);
    EXPECT_EQ(nt_gcd(17, 13), 1);
    EXPECT_EQ(nt_gcd(0, 5), 5);
    EXPECT_EQ(nt_gcd(48, 18), 6);
}

TEST(NumberTheoryTest, LcmKnownValues) {
    EXPECT_EQ(nt_lcm(4, 6), 12);
    EXPECT_EQ(nt_lcm(12, 8), 24);
    EXPECT_EQ(nt_lcm(7, 13), 91);
}

/* ---------- totient ---------- */

TEST_F(NumberTheoryFixture, EulerTotient) {
    EXPECT_EQ(nt_euler_totient(nt, 12), 4);
    EXPECT_EQ(nt_euler_totient(nt, 1), 1);
    EXPECT_EQ(nt_euler_totient(nt, 7), 6);   /* prime: phi(p) = p-1 */
    EXPECT_EQ(nt_euler_totient(nt, 10), 4);
}

/* ---------- modular exponentiation ---------- */

TEST(NumberTheoryTest, ModPowKnown) {
    EXPECT_EQ(nt_mod_pow(2, 10, 1000), 24);  /* 1024 mod 1000 = 24 */
    EXPECT_EQ(nt_mod_pow(3, 5, 13), 9);      /* 243 mod 13 = 9 */
    EXPECT_EQ(nt_mod_pow(2, 0, 100), 1);     /* anything^0 = 1 */
    EXPECT_EQ(nt_mod_pow(7, 1, 5), 2);       /* 7 mod 5 = 2 */
}

/* ---------- CRT ---------- */

TEST(NumberTheoryTest, CrtSolve) {
    /* x = 2 mod 3, x = 3 mod 5, x = 2 mod 7 => x = 23 mod 105 */
    crt_system_t crt;
    memset(&crt, 0, sizeof(crt));
    crt.remainders[0] = 2; crt.moduli[0] = 3;
    crt.remainders[1] = 3; crt.moduli[1] = 5;
    crt.remainders[2] = 2; crt.moduli[2] = 7;
    crt.count = 3;

    bool ok = nt_crt_solve(&crt);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(crt.valid);
    EXPECT_EQ(crt.solution, 23);
    EXPECT_EQ(crt.combined_modulus, 105);
}

/* ---------- factorization ---------- */

TEST_F(NumberTheoryFixture, Factorize60) {
    factorization_t f = nt_factorize(nt, 60);
    /* 60 = 2^2 * 3 * 5 => 3 distinct primes */
    EXPECT_EQ(f.count, 3);
    EXPECT_EQ(f.factors[0].prime, 2);
    EXPECT_EQ(f.factors[0].exponent, 2);
    EXPECT_EQ(f.factors[1].prime, 3);
    EXPECT_EQ(f.factors[1].exponent, 1);
    EXPECT_EQ(f.factors[2].prime, 5);
    EXPECT_EQ(f.factors[2].exponent, 1);
}

/* ---------- extended GCD ---------- */

TEST(NumberTheoryTest, ExtendedGcd) {
    ext_gcd_result_t r = nt_extended_gcd(35, 15);
    EXPECT_EQ(r.gcd, 5);
    /* Verify Bezout identity: 35*x + 15*y = 5 */
    EXPECT_EQ(35 * r.x + 15 * r.y, 5);
}

/* ---------- sieve ---------- */

TEST_F(NumberTheoryFixture, SievePrimeCount) {
    ASSERT_TRUE(nt_build_sieve(nt, 1000));
    /* There are 168 primes <= 1000 */
    EXPECT_EQ(nt_prime_count(nt, 1000), 168);
    EXPECT_TRUE(nt_is_prime_sieve(nt, 997));
    EXPECT_FALSE(nt_is_prime_sieve(nt, 998));
}
