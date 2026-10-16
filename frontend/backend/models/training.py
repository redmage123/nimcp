"""Pydantic models for training endpoints."""
from pydantic import BaseModel, Field
from typing import Optional


class TrainingConfig(BaseModel):
    loss_type: int = Field(default=0, description="0=MSE,1=CE,2=BCE,3=Huber,4=MAE,5=Focal,6=KL")
    optimizer_type: int = Field(default=3, description="0=SGD,1=Momentum,2=Adam,3=AdamW,4=RMSProp,5=Adagrad")
    scheduler_type: int = Field(default=0, description="0=Constant,1=Step,2=Exp,3=Cosine,4=WarmupCosine,5=ReducePlateau,6=Cyclic")
    learning_rate: float = 0.001
    weight_decay: float = 0.0
    momentum: float = 0.9
    beta1: float = 0.9
    beta2: float = 0.999
    epsilon: float = 1e-8
    scheduler_step_size: int = 100
    scheduler_gamma: float = 0.1
    warmup_steps: int = 0
    enable_gradient_clipping: bool = False
    gradient_clip_value: float = 1.0
    enable_biological_modulation: bool = False
    biological_blend: float = 0.5


class TrainingStart(BaseModel):
    dataset_id: str = Field(
        default="iris", max_length=128,
        pattern=r'^[a-zA-Z0-9][a-zA-Z0-9_\-]*$',
    )
    epochs: int = Field(default=10, ge=1, le=1000)
    batch_size: int = Field(default=1, ge=1, le=256)
    strategy: str = Field(
        default="hybrid",
        description="Training strategy: 'gradient' (backprop only), "
                    "'hebbian' (STDP/BCM only), 'hybrid' (gradient + STDP consolidation), "
                    "'auto' (auto-detect from data characteristics)",
        pattern=r'^(gradient|hebbian|hybrid|auto)$',
    )
    biological_blend: float = Field(
        default=0.3, ge=0.0, le=1.0,
        description="Biological modulation strength for hybrid/gradient strategies (0-1)",
    )
    stdp_confidence: float = Field(
        default=0.5, ge=0.0, le=1.0,
        description="Confidence passed to STDP consolidation step in hybrid strategy",
    )


class TrainingProgress(BaseModel):
    brain_id: int
    running: bool = False
    epoch: int = 0
    step: int = 0
    total_steps: int = 0
    loss: float = 0.0
    accuracy: float = 0.0
    learning_rate: float = 0.0
    elapsed_seconds: float = 0.0


class LearnBatch(BaseModel):
    dataset_id: str = Field(
        default="iris", max_length=128,
        pattern=r'^[a-zA-Z0-9][a-zA-Z0-9_\-]*$',
    )
    count: int = Field(default=36, ge=1, le=10000)
