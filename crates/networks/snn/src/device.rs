//! Population → GPU device assignment — Phase 10c.
//!
//! Pure policy, no CUDA: [`DeviceAssignment::resolve`] turns a config
//! into one device ordinal per population. `SnnNetwork::new` then opens
//! a context per distinct ordinal and builds each population's LIF
//! integrator — and every edge *into* that population — on it. The
//! host uploads each edge's source spikes to the edge's own device and
//! downloads its `i_syn`, which is exactly V1's host-mediated scheme.

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// How populations are spread across CUDA devices.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum DeviceAssignment {
    /// Everything on one device. The default (`ordinal = 0`) is the
    /// pre-Phase-10 behaviour.
    SingleDevice {
        /// CUDA device ordinal.
        ordinal: u32,
    },
    /// Population `i` goes to `devices[i % devices.len()]`.
    RoundRobin {
        /// Device ordinals to cycle through, in order.
        devices: Vec<u32>,
    },
    /// Explicit ordinal per population, index-aligned with
    /// `SnnConfig::populations`.
    Manual {
        /// One ordinal per population.
        per_population: Vec<u32>,
    },
    /// Greedy neuron-count balance: populations largest-first, each to
    /// the device with the fewest neurons so far (ties → earlier in
    /// `devices`). Synapse memory scales with the destination
    /// population's size, so neuron count is a fair proxy.
    MemoryBalanced {
        /// Device ordinals to balance across.
        devices: Vec<u32>,
    },
}

impl Default for DeviceAssignment {
    fn default() -> Self {
        Self::SingleDevice { ordinal: 0 }
    }
}

/// Why a [`DeviceAssignment`] could not be resolved.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum DeviceAssignError {
    /// `devices` was empty.
    #[error("device list is empty")]
    NoDevices,
    /// The same ordinal appears twice in `devices`.
    #[error("device ordinal {0} listed more than once")]
    DuplicateDevice(u32),
    /// `Manual` list length does not match the population count.
    #[error("manual assignment has {given} entries for {n_pops} populations")]
    ManualLengthMismatch {
        /// Entries supplied.
        given: usize,
        /// Populations in the config.
        n_pops: usize,
    },
}

impl DeviceAssignment {
    /// One ordinal per population, given each population's neuron count.
    pub fn resolve(&self, pop_sizes: &[u32]) -> Result<Vec<u32>, DeviceAssignError> {
        match self {
            Self::SingleDevice { ordinal } => Ok(vec![*ordinal; pop_sizes.len()]),
            Self::RoundRobin { devices } => {
                check_devices(devices)?;
                Ok((0..pop_sizes.len())
                    .map(|i| devices[i % devices.len()])
                    .collect())
            }
            Self::Manual { per_population } => {
                if per_population.len() != pop_sizes.len() {
                    return Err(DeviceAssignError::ManualLengthMismatch {
                        given: per_population.len(),
                        n_pops: pop_sizes.len(),
                    });
                }
                Ok(per_population.clone())
            }
            Self::MemoryBalanced { devices } => {
                check_devices(devices)?;
                let mut order: Vec<usize> = (0..pop_sizes.len()).collect();
                order.sort_by(|&a, &b| pop_sizes[b].cmp(&pop_sizes[a]).then(a.cmp(&b)));
                let mut load = vec![0u64; devices.len()];
                let mut out = vec![0u32; pop_sizes.len()];
                for pop in order {
                    let dev = (0..devices.len())
                        .min_by_key(|&d| (load[d], d))
                        .expect("devices non-empty");
                    load[dev] += u64::from(pop_sizes[pop]);
                    out[pop] = devices[dev];
                }
                Ok(out)
            }
        }
    }

    /// Distinct ordinals this assignment uses, ascending.
    pub fn ordinals(&self, pop_sizes: &[u32]) -> Result<Vec<u32>, DeviceAssignError> {
        let mut v = self.resolve(pop_sizes)?;
        v.sort_unstable();
        v.dedup();
        Ok(v)
    }
}

fn check_devices(devices: &[u32]) -> Result<(), DeviceAssignError> {
    if devices.is_empty() {
        return Err(DeviceAssignError::NoDevices);
    }
    for (i, &d) in devices.iter().enumerate() {
        if devices[..i].contains(&d) {
            return Err(DeviceAssignError::DuplicateDevice(d));
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_is_single_device_zero() {
        let a = DeviceAssignment::default();
        assert_eq!(a.resolve(&[10, 20, 30]).unwrap(), vec![0, 0, 0]);
        assert_eq!(a.ordinals(&[10, 20]).unwrap(), vec![0]);
    }

    #[test]
    fn single_device_other_ordinal() {
        let a = DeviceAssignment::SingleDevice { ordinal: 1 };
        assert_eq!(a.resolve(&[5, 5]).unwrap(), vec![1, 1]);
    }

    #[test]
    fn round_robin_cycles() {
        let a = DeviceAssignment::RoundRobin {
            devices: vec![0, 1],
        };
        assert_eq!(a.resolve(&[1, 1, 1, 1, 1]).unwrap(), vec![0, 1, 0, 1, 0]);
        assert_eq!(a.ordinals(&[1, 1, 1]).unwrap(), vec![0, 1]);
    }

    #[test]
    fn round_robin_rejects_empty_and_duplicates() {
        assert_eq!(
            DeviceAssignment::RoundRobin { devices: vec![] }.resolve(&[1]),
            Err(DeviceAssignError::NoDevices)
        );
        assert_eq!(
            DeviceAssignment::RoundRobin {
                devices: vec![0, 1, 0]
            }
            .resolve(&[1]),
            Err(DeviceAssignError::DuplicateDevice(0))
        );
    }

    #[test]
    fn manual_passthrough_and_length_check() {
        let a = DeviceAssignment::Manual {
            per_population: vec![1, 0, 1],
        };
        assert_eq!(a.resolve(&[1, 1, 1]).unwrap(), vec![1, 0, 1]);
        assert_eq!(
            a.resolve(&[1, 1]),
            Err(DeviceAssignError::ManualLengthMismatch {
                given: 3,
                n_pops: 2
            })
        );
    }

    #[test]
    fn memory_balanced_greedy_largest_first() {
        // Sizes 100, 10, 90, 20 over 2 devices: 100→d0, 90→d1, 20→d1
        // (load 90 < 100), 10→d0 (load 100 < 110). Loads: 110 / 110.
        let a = DeviceAssignment::MemoryBalanced {
            devices: vec![0, 1],
        };
        assert_eq!(a.resolve(&[100, 10, 90, 20]).unwrap(), vec![0, 0, 1, 1]);
    }

    #[test]
    fn memory_balanced_ties_prefer_earlier_device() {
        let a = DeviceAssignment::MemoryBalanced {
            devices: vec![3, 1],
        };
        assert_eq!(a.resolve(&[7, 7]).unwrap(), vec![3, 1]);
    }

    #[test]
    fn memory_balanced_single_device_degenerate() {
        let a = DeviceAssignment::MemoryBalanced { devices: vec![0] };
        assert_eq!(a.resolve(&[5, 6, 7]).unwrap(), vec![0, 0, 0]);
    }

    #[test]
    fn serde_round_trip_tagged() {
        let a = DeviceAssignment::RoundRobin {
            devices: vec![0, 1],
        };
        let s = serde_json::to_string(&a).unwrap();
        assert!(s.contains("\"kind\":\"round_robin\""), "{s}");
        let b: DeviceAssignment = serde_json::from_str(&s).unwrap();
        assert_eq!(a, b);
        let d: DeviceAssignment =
            serde_json::from_str(r#"{"kind":"single_device","ordinal":0}"#).unwrap();
        assert_eq!(d, DeviceAssignment::default());
    }
}
