"""Pydantic models for dataset endpoints."""
from pydantic import BaseModel


class DatasetInfo(BaseModel):
    id: str
    name: str
    num_inputs: int
    num_outputs: int
    num_classes: int
    description: str
    total_examples: int = 0
    is_builtin: bool = False


class BatchDeleteRequest(BaseModel):
    ids: list[str]
