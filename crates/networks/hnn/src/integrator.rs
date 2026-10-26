//! Symplectic Euler integrator.
//!
//! ```text
//! p_{n+1} = p_n - dt * dH/dq(q_n, p_n)
//! q_{n+1} = q_n + dt * dH/dp(q_n, p_{n+1})
//! ```
//!
//! Note the asymmetry: the update of `q` uses the *new* `p` (computed
//! one line up). That coupling is what makes the step symplectic and
//! gives bounded long-term energy error.
//!
//! Generic over any closure returning `(H, dH/dq, dH/dp)` so the same
//! step works for the MLP-defined Hamiltonian *and* for analytic
//! Hamiltonians used in the energy-conservation test.

use ndarray::Array1;

/// One symplectic Euler step. `hamiltonian(q, p) -> (H, dH/dq, dH/dp)`.
/// `q` and `p` are mutated in place to the new state. Returns the
/// scalar Hamiltonian evaluated at the *start* of the step (so callers
/// can monitor energy without an extra evaluation).
pub fn symplectic_euler_step<F>(
    q: &mut Array1<f32>,
    p: &mut Array1<f32>,
    dt: f32,
    hamiltonian: F,
) -> f32
where
    F: Fn(&Array1<f32>, &Array1<f32>) -> (f32, Array1<f32>, Array1<f32>),
{
    debug_assert_eq!(q.len(), p.len(), "symplectic: q/p length mismatch");

    // 1. Evaluate at (q_n, p_n) for the energy diagnostic + dH/dq.
    let (h_n, dh_dq, _dh_dp_unused) = hamiltonian(q, p);

    // 2. p_{n+1} = p_n - dt * dH/dq(q_n, p_n).
    for (pi, gi) in p.iter_mut().zip(dh_dq.iter()) {
        *pi -= dt * *gi;
    }

    // 3. Re-evaluate at (q_n, p_{n+1}) to fetch dH/dp at the new p.
    let (_h_mid, _dh_dq_unused, dh_dp_new) = hamiltonian(q, p);

    // 4. q_{n+1} = q_n + dt * dH/dp(q_n, p_{n+1}).
    for (qi, gi) in q.iter_mut().zip(dh_dp_new.iter()) {
        *qi += dt * *gi;
    }

    h_n
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Energy-conservation acceptance test from the Phase 11 plan.
    ///
    /// 2-D harmonic oscillator: `H = 0.5 * (q² + p²)`. With analytic
    /// gradients the symplectic Euler step should hold the energy
    /// drift below 1e-3 over 10000 steps at `dt = 0.01`.
    #[test]
    fn energy_conservation_harmonic_oscillator() {
        // H(q,p) = 0.5 * (q^2 + p^2). dH/dq = q, dH/dp = p.
        let h_fn = |q: &Array1<f32>, p: &Array1<f32>| {
            let h =
                0.5 * (q.iter().map(|v| v * v).sum::<f32>() + p.iter().map(|v| v * v).sum::<f32>());
            (h, q.clone(), p.clone())
        };

        // 2-D oscillator — initial unit-energy in q, zero p.
        let mut q = Array1::from_vec(vec![1.0_f32, 0.0]);
        let mut p = Array1::from_vec(vec![0.0_f32, 1.0]);
        let (h_init, _, _) = h_fn(&q, &p);

        let dt = 0.01_f32;
        for _ in 0..10_000 {
            symplectic_euler_step(&mut q, &mut p, dt, h_fn);
        }

        let (h_end, _, _) = h_fn(&q, &p);
        let drift = (h_end - h_init).abs() / h_init.abs().max(1e-9);
        assert!(
            drift < 1e-3,
            "symplectic energy drift {drift} exceeds 1e-3 (H_init={h_init}, H_end={h_end})"
        );
    }

    /// Sanity check vs a *non-symplectic* forward Euler — that one
    /// should drift visibly over the same window. Confirms the
    /// integrator's symplecticity matters (i.e. our test isn't trivially
    /// passing because the dynamics are too easy).
    #[test]
    fn forward_euler_drifts_more_than_symplectic() {
        let h_fn = |q: &Array1<f32>, p: &Array1<f32>| {
            let h =
                0.5 * (q.iter().map(|v| v * v).sum::<f32>() + p.iter().map(|v| v * v).sum::<f32>());
            (h, q.clone(), p.clone())
        };

        // Forward (non-symplectic) Euler — uses old p in q's update.
        let mut q = Array1::from_vec(vec![1.0_f32]);
        let mut p = Array1::from_vec(vec![0.0_f32]);
        let dt = 0.01_f32;
        for _ in 0..10_000 {
            let (_h, dq, dp) = h_fn(&q, &p);
            for (qi, gi) in q.iter_mut().zip(dp.iter()) {
                *qi += dt * *gi;
            }
            for (pi, gi) in p.iter_mut().zip(dq.iter()) {
                *pi -= dt * *gi;
            }
        }
        let (h_end_naive, _, _) = h_fn(&q, &p);
        let drift_naive = (h_end_naive - 0.5).abs() / 0.5;

        // Symplectic on the same problem.
        let mut q = Array1::from_vec(vec![1.0_f32]);
        let mut p = Array1::from_vec(vec![0.0_f32]);
        for _ in 0..10_000 {
            symplectic_euler_step(&mut q, &mut p, dt, h_fn);
        }
        let (h_end_symp, _, _) = h_fn(&q, &p);
        let drift_symp = (h_end_symp - 0.5).abs() / 0.5;

        assert!(
            drift_symp < drift_naive,
            "symplectic should drift less than naive: symp={drift_symp}, naive={drift_naive}"
        );
    }
}
