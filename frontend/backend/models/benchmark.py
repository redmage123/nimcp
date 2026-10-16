"""Pydantic models for benchmark endpoints."""
from pydantic import BaseModel, Field
from typing import Optional


class BenchmarkRequest(BaseModel):
    benchmark_id: str = Field(
        default="iris",
        description="Benchmark to run: iris, wine, breast_cancer, mnist, xor, titanic, "
                    "fashion_mnist, mmlu, arc_easy, hellaswag, winogrande, or 'all'",
    )
    brain_size: int = Field(default=1, ge=0, le=2, description="0=tiny, 1=small, 2=medium")
    strategy: str = Field(
        default="auto",
        pattern=r'^(gradient|hebbian|hybrid|auto)$',
        description="Training strategy",
    )
    epochs: int = Field(default=10, ge=1, le=100)
    include_cognitive: bool = Field(default=True, description="Run cognitive benchmarks too")


class CognitiveMetrics(BaseModel):
    working_memory_capacity: int = 0
    working_memory_occupancy: float = 0.0
    oscillation_coherence: float = 0.0
    pac_index: float = 0.0
    workspace_broadcasts: int = 0
    workspace_avg_strength: float = 0.0
    ethics_harmful_score: float = 0.0
    ethics_beneficial_score: float = 0.0
    ethics_separation: float = 0.0
    knowledge_concepts: int = 0
    knowledge_coverage: float = 0.0


class BenchmarkResult(BaseModel):
    benchmark_id: str
    category: str
    accuracy: float = 0.0
    loss: float = 0.0
    per_class_accuracy: Optional[dict[str, float]] = None
    confusion: Optional[dict[str, dict[str, int]]] = None
    train_time_seconds: float = 0.0
    inference_time_us: float = 0.0
    memory_bytes: int = 0
    sparsity: float = 0.0
    active_neuron_ratio: float = 0.0
    steps_to_70_pct: Optional[int] = None
    strategy: str = ""
    epochs: int = 0
    brain_size: int = 0
    cognitive: Optional[CognitiveMetrics] = None
    reference_scores: dict[str, float] = {}
    nimcp_vs_best: float = 0.0


class BenchmarkSummary(BaseModel):
    results: list[BenchmarkResult] = []
    overall_ml_accuracy: float = 0.0
    overall_genai_accuracy: float = 0.0
    cognitive_health_score: float = 0.0
    timestamp: str = ""
