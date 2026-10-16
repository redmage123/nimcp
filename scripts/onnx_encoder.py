"""ONNX-accelerated text encoder for brain daemon.

Loads BGE-large-en-v1.5 as ONNX model for fast text→embedding.
Uses CUDAExecutionProvider when available, falls back to CPU.
Thread-safe: session.run is reentrant.
"""
import os
import numpy as np

_session = None
_tokenizer = None

MODEL_DIR = os.environ.get('ONNX_MODEL_DIR',
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'models'))
ONNX_PATH = os.path.join(MODEL_DIR, 'bge-large.onnx')
TOKENIZER_PATH = os.path.join(MODEL_DIR, 'bge-tokenizer')


def _init():
    global _session, _tokenizer
    if _session is not None:
        return

    import onnxruntime as ort
    from transformers import AutoTokenizer

    providers = ort.get_available_providers()
    # Prefer CUDA > CPU
    use_providers = []
    if 'CUDAExecutionProvider' in providers:
        use_providers.append('CUDAExecutionProvider')
    use_providers.append('CPUExecutionProvider')

    _session = ort.InferenceSession(ONNX_PATH, providers=use_providers)
    _tokenizer = AutoTokenizer.from_pretrained(TOKENIZER_PATH)
    print(f'[ONNX] BGE-large loaded: {_session.get_providers()[0]}, '
          f'model={ONNX_PATH}', flush=True)


def encode_text(text: str, max_length: int = 128) -> np.ndarray:
    """Encode text to 1024-dim embedding.

    Returns normalized float32 array [1024].
    """
    _init()
    tokens = _tokenizer(text, return_tensors='np', padding='max_length',
                        max_length=max_length, truncation=True)
    feeds = {
        'input_ids': tokens['input_ids'].astype(np.int64),
        'attention_mask': tokens['attention_mask'].astype(np.int64),
    }
    # BGE ONNX model requires token_type_ids (all zeros for single-sequence)
    if 'token_type_ids' in tokens:
        feeds['token_type_ids'] = tokens['token_type_ids'].astype(np.int64)
    else:
        feeds['token_type_ids'] = np.zeros_like(tokens['input_ids'], dtype=np.int64)
    result = _session.run(None, feeds)
    # CLS token embedding (BGE convention)
    emb = result[0][0, 0, :].astype(np.float32)
    # L2 normalize (matches sentence-transformers default)
    norm = np.linalg.norm(emb)
    if norm > 0:
        emb /= norm
    return emb


def encode_batch(texts: list, max_length: int = 128) -> np.ndarray:
    """Encode multiple texts at once.

    Returns float32 array [N, 1024].
    """
    _init()
    tokens = _tokenizer(texts, return_tensors='np', padding='max_length',
                        max_length=max_length, truncation=True)
    feeds = {
        'input_ids': tokens['input_ids'].astype(np.int64),
        'attention_mask': tokens['attention_mask'].astype(np.int64),
    }
    if 'token_type_ids' in tokens:
        feeds['token_type_ids'] = tokens['token_type_ids'].astype(np.int64)
    else:
        feeds['token_type_ids'] = np.zeros_like(tokens['input_ids'], dtype=np.int64)
    result = _session.run(None, feeds)
    # CLS token for each item
    embs = result[0][:, 0, :].astype(np.float32)
    # L2 normalize each
    norms = np.linalg.norm(embs, axis=1, keepdims=True)
    norms = np.maximum(norms, 1e-8)
    embs /= norms
    return embs
