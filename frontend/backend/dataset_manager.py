"""Dataset management — reuses web-demo datasets + CSV upload."""
import csv
import math
import os
from typing import Optional

import nimcp_logger
from config import MAX_CSV_UPLOAD_BYTES, UPLOAD_DIR
from validation import sanitize_dataset_id, validate_csv_bytes

_log = nimcp_logger.get("dataset_manager")

# Import web-demo datasets
import sys
from config import WEBDEMO_BACKEND
if WEBDEMO_BACKEND not in sys.path:
    sys.path.insert(0, WEBDEMO_BACKEND)
try:
    from datasets import DATASETS as _BUILTIN
except ImportError:
    _BUILTIN = {}

# Metadata for built-in datasets
_DATASET_META = {
    "iris": {"name": "Iris", "num_inputs": 4, "num_outputs": 3, "num_classes": 3,
             "description": "Classic iris flower classification (4 features, 3 classes)"},
    "mnist": {"name": "MNIST", "num_inputs": 784, "num_outputs": 10, "num_classes": 10,
              "description": "Handwritten digit classification (28x28 pixels)"},
    "titanic": {"name": "Titanic", "num_inputs": 8, "num_outputs": 2, "num_classes": 2,
                "description": "Titanic survival prediction (8 features)"},
    "patterns": {"name": "Visual Patterns", "num_inputs": 64, "num_outputs": 4, "num_classes": 4,
                 "description": "8x8 grid pattern recognition (horizontal/vertical/diagonal/circle)"},
    "xor": {"name": "Logic Gates (XOR)", "num_inputs": 2, "num_outputs": 2, "num_classes": 2,
            "description": "XOR/AND/OR logic gate learning"},
    "sinewave": {"name": "Sine Wave", "num_inputs": 20, "num_outputs": 4, "num_classes": 4,
                 "description": "Time series prediction from sine wave patterns"},
}

# Uploaded CSV datasets
_uploaded: dict[str, dict] = {}


def list_datasets() -> list[dict]:
    result = []
    for did, meta in _DATASET_META.items():
        info = {"id": did, **meta, "total_examples": 0, "is_builtin": True}
        if did in _BUILTIN:
            try:
                ds = _BUILTIN[did]()
                examples = ds.get_examples(100)
                info["total_examples"] = len(examples)
            except Exception as exc:
                _log.warning("Failed to load built-in dataset %s: %s", did, exc)
        result.append(info)
    for did, upl in _uploaded.items():
        result.append({
            "id": did, "name": upl["name"],
            "num_inputs": upl["num_inputs"], "num_outputs": upl["num_outputs"],
            "num_classes": upl["num_classes"],
            "description": f"Uploaded CSV ({upl['num_inputs']} features)",
            "total_examples": len(upl["data"]),
            "is_builtin": False,
        })
    return result


def _normalize_examples(raw: list) -> list[dict]:
    """Normalize (features, label) tuples or dicts into uniform dicts."""
    result = []
    for ex in raw:
        if isinstance(ex, dict):
            result.append(ex)
        elif isinstance(ex, (tuple, list)) and len(ex) >= 2:
            result.append({"features": list(ex[0]), "label": str(ex[1])})
        else:
            continue
    return result


def get_examples(dataset_id: str, count: int = 0) -> Optional[list[dict]]:
    if dataset_id in _BUILTIN:
        try:
            ds = _BUILTIN[dataset_id]()
            raw = ds.get_examples(count) if count > 0 else ds.get_examples()
            examples = _normalize_examples(raw)
            if count > 0:
                examples = examples[:count]
            return examples
        except Exception as exc:
            _log.warning("Failed to get examples from %s: %s", dataset_id, exc)
            return None
    if dataset_id in _uploaded:
        data = _uploaded[dataset_id]["data"]
        if count > 0:
            data = data[:count]
        return data
    return None


def get_dataset_config(dataset_id: str) -> Optional[dict]:
    if dataset_id in _DATASET_META:
        return _DATASET_META[dataset_id]
    if dataset_id in _uploaded:
        u = _uploaded[dataset_id]
        return {"num_inputs": u["num_inputs"], "num_outputs": u["num_outputs"],
                "num_classes": u["num_classes"]}
    return None


def upload_csv(name: str, content: bytes, label_column: str = "label") -> str:
    text = validate_csv_bytes(content, MAX_CSV_UPLOAD_BYTES)
    os.makedirs(UPLOAD_DIR, exist_ok=True)
    lines = text.strip().split("\n")
    reader = csv.DictReader(lines)
    rows = list(reader)
    if not rows:
        raise ValueError("Empty CSV")

    headers = list(rows[0].keys())
    if label_column not in headers:
        raise ValueError(f"Label column '{label_column}' not found. Columns: {headers}")

    feature_cols = [h for h in headers if h != label_column]
    labels = sorted(set(r[label_column] for r in rows))
    label_map = {l: i for i, l in enumerate(labels)}

    data = []
    for row_idx, r in enumerate(rows):
        features = []
        for c in feature_cols:
            val = float(r[c])
            if not math.isfinite(val):
                raise ValueError(f"Row {row_idx + 1}, column '{c}': non-finite value ({val})")
            features.append(val)
        label = label_map[r[label_column]]
        data.append({"features": features, "label": label, "label_name": r[label_column]})

    did = sanitize_dataset_id(name)
    _uploaded[did] = {
        "name": name, "data": data,
        "num_inputs": len(feature_cols), "num_outputs": len(labels),
        "num_classes": len(labels), "labels": labels,
    }
    _log.info("Uploaded CSV dataset %s (%d rows, %d features)", did, len(data), len(feature_cols))
    return did


def delete_dataset(dataset_id: str) -> bool:
    """Delete an uploaded dataset. Raises ValueError for built-ins, returns False if not found."""
    if dataset_id in _DATASET_META:
        raise ValueError(f"Cannot delete built-in dataset '{dataset_id}'")
    if dataset_id in _uploaded:
        del _uploaded[dataset_id]
        _log.info("Deleted uploaded dataset %s", dataset_id)
        return True
    return False


def delete_datasets(dataset_ids: list[str]) -> dict:
    """Batch delete uploaded datasets. Returns summary of results."""
    deleted = []
    not_found = []
    protected = []
    for did in dataset_ids:
        if did in _DATASET_META:
            protected.append(did)
        elif did in _uploaded:
            del _uploaded[did]
            deleted.append(did)
        else:
            not_found.append(did)
    if deleted:
        _log.info("Batch deleted %d datasets: %s", len(deleted), deleted)
    return {"deleted": deleted, "not_found": not_found, "protected": protected}
