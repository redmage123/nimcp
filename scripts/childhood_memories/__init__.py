"""Synthetic childhood memory generation and implantation.

A human child spends ~12 months building the substrate (semantic concepts,
episodic memories, self-model, language primitives) that everything
downstream depends on. Digital systems can *inherit* this substrate via
implantation.

Pipeline:
    1. generator.generate_all()  — produces structured JSON per layer
    2. implanter.implant_all()   — loads JSON into brain substrates
    3. verifier.verify_retrievable() — checks each implanted item can be recalled
"""

from .generator import MemoryGenerator
from .implanter import MemoryImplanter, ImplantResult
from .verifier import verify_retrievable
from .running_implanter import RunningImplanter, TrainingEvent

__all__ = [
    "MemoryGenerator",
    "MemoryImplanter",
    "ImplantResult",
    "verify_retrievable",
    "RunningImplanter",
    "TrainingEvent",
]
