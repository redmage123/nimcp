"""Pydantic models for brain endpoints."""
import math
from pydantic import BaseModel, Field, field_validator
from typing import Optional


class BrainCreate(BaseModel):
    name: str = Field(
        default="brain", max_length=128,
        pattern=r'^[a-zA-Z0-9][a-zA-Z0-9_ \-]*$',
    )
    size: int = Field(default=1, ge=0, le=3, description="0=tiny,1=small,2=medium,3=large")
    task: Optional[int] = Field(default=None, ge=0, le=4, description="0=classification,1=regression,2=pattern,3=sequence,4=association")
    num_inputs: Optional[int] = Field(default=None, ge=1, le=10000)
    num_outputs: Optional[int] = Field(default=None, ge=1, le=1000)
    num_neurons: Optional[int] = Field(default=None, ge=10, le=500000, description="Custom neuron count (overrides size preset)")


class BrainResize(BaseModel):
    num_neurons: int = Field(ge=10, le=500000)


class BrainUpdate(BaseModel):
    name: str = Field(
        min_length=1, max_length=128,
        pattern=r'^[a-zA-Z0-9][a-zA-Z0-9_ \-]*$',
    )


class BrainProbe(BaseModel):
    task_name: str = ""
    size: int = 0
    task: int = 0
    num_neurons: int = 0
    num_synapses: int = 0
    num_active_synapses: int = 0
    total_inferences: int = 0
    total_learning_steps: int = 0
    avg_sparsity: float = 0.0
    avg_inference_time_us: float = 0.0
    current_learning_rate: float = 0.0
    accuracy: float = 0.0
    memory_bytes: int = 0
    num_inputs: int = 0
    num_outputs: int = 0
    utilization: float = 0.0
    last_loss: float = 0.0
    is_cow_clone: bool = False
    cow_ref_count: int = 0
    cow_shared_bytes: int = 0
    cow_private_bytes: int = 0


class BrainInfo(BaseModel):
    id: int
    name: str
    created_at: str
    dataset: Optional[str] = None
    parent_id: Optional[int] = None
    probe: Optional[BrainProbe] = None


class BrainPredict(BaseModel):
    features: list[float] = Field(min_length=1)

    @field_validator("features")
    @classmethod
    def check_features(cls, v: list[float]) -> list[float]:
        if len(v) > 10000:
            raise ValueError("Feature vector too long (max 10000)")
        for i, x in enumerate(v):
            if not math.isfinite(x):
                raise ValueError(f"features[{i}] is not finite ({x})")
        return v


class BrainLearn(BaseModel):
    features: list[float] = Field(min_length=1)
    label: str = Field(max_length=256)
    confidence: float = Field(default=1.0, ge=0.0, le=1.0)

    @field_validator("features")
    @classmethod
    def check_features(cls, v: list[float]) -> list[float]:
        if len(v) > 10000:
            raise ValueError("Feature vector too long (max 10000)")
        for i, x in enumerate(v):
            if not math.isfinite(x):
                raise ValueError(f"features[{i}] is not finite ({x})")
        return v


class SnapshotCreate(BaseModel):
    name: str = Field(
        min_length=1, max_length=64,
        pattern=r'^[a-zA-Z0-9][a-zA-Z0-9_\-]*$',
    )
