"""Batch-safe biological stability mechanisms.

Python reference implementations that produce mathematically-equivalent
results to the sequential (batch=1) mechanisms, allowing batched training
without breaking biological fidelity.

Each module here is paired with a differential test in
`tests/unit/test_batch_safe_*.py` that proves batch≡sequential.

Current status:
  - synaptic_scaling.py      — homeostatic rate EMA + weight scaling
  - intrinsic_plasticity.py  — per-neuron threshold adaptation
  - short_term_depression.py — synaptic fatigue recurrence
  - inhibitory_plasticity.py — E/I-specific rules
  - metabolic_budget.py      — per-neuron weight cap (stateless)
  - gradient_flow.py         — cross-network gradient budget
  - rstdp.py                 — reward-modulated STDP with batched weight updates

Not production-ready: these are reference implementations. Production C
port requires deriving+porting+testing each. Feature-flag gated.
"""

from .synaptic_scaling import SynapticScaling
from .intrinsic_plasticity import IntrinsicPlasticity
from .short_term_depression import ShortTermDepression
from .inhibitory_plasticity import InhibitoryPlasticity
from .metabolic_budget import MetabolicBudget
from .gradient_flow import GlobalGradientBudget
from .rstdp import BatchRSTDP

__all__ = [
    "SynapticScaling",
    "IntrinsicPlasticity",
    "ShortTermDepression",
    "InhibitoryPlasticity",
    "MetabolicBudget",
    "GlobalGradientBudget",
    "BatchRSTDP",
]
