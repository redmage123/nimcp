#!/usr/bin/env python3
"""Neural Decoder — Translates brain output vectors back to human-readable text.

Architecture:
    EmbeddingProjector: Linear projection from brain output space → embedding space
    VocabularyBank: Nearest-neighbor lookup from embeddings → text
    NeuralDecoder: Combines both into decode_output(brain_vector) → text

The decoder is trained online: as Athena learns (output_vector, target_embedding)
pairs, the projector is periodically refitted via least-squares.
"""

import json
import logging
import os
import time
from pathlib import Path

import numpy as np

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Embedding Projector: brain output space → embedding space
# ---------------------------------------------------------------------------

class EmbeddingProjector:
    """Linear projection from brain output space (N-dim) to embedding space (D-dim).

    Learned via least-squares fitting on (output_vector, target_embedding) pairs
    collected during training.  Updated periodically (every `refit_interval` pairs).
    """

    def __init__(self, output_dim: int, embed_dim: int, refit_interval: int = 1000):
        self.output_dim = output_dim
        self.embed_dim = embed_dim
        self.refit_interval = refit_interval

        # Projection matrix W (output_dim × embed_dim) + bias (embed_dim,)
        # Initialized to truncation (identity-like) until enough data
        self.W = np.zeros((output_dim, embed_dim), dtype=np.float32)
        min_dim = min(output_dim, embed_dim)
        self.W[:min_dim, :min_dim] = np.eye(min_dim, dtype=np.float32)
        self.bias = np.zeros(embed_dim, dtype=np.float32)

        # Collected training pairs for refitting
        self._output_buf: list[np.ndarray] = []
        self._embed_buf: list[np.ndarray] = []
        self._total_pairs = 0
        self._is_fitted = False

    def project(self, output_vector: np.ndarray) -> np.ndarray:
        """Project brain output vector to embedding space."""
        v = np.asarray(output_vector, dtype=np.float32).ravel()
        if len(v) != self.output_dim:
            raise ValueError(f"Expected {self.output_dim}-dim, got {len(v)}-dim")
        return v @ self.W + self.bias

    def record_pair(self, output_vector: np.ndarray, target_embedding: np.ndarray):
        """Record an (output, embedding) pair for future refitting."""
        self._output_buf.append(np.asarray(output_vector, dtype=np.float32).ravel())
        self._embed_buf.append(np.asarray(target_embedding, dtype=np.float32).ravel())
        self._total_pairs += 1

        if len(self._output_buf) >= self.refit_interval:
            self.refit()

    def refit(self):
        """Refit projection matrix via least-squares on buffered pairs."""
        if len(self._output_buf) < 10:
            logger.debug("Too few pairs (%d) to refit projector", len(self._output_buf))
            return

        X = np.array(self._output_buf, dtype=np.float32)  # (N, output_dim)
        Y = np.array(self._embed_buf, dtype=np.float32)    # (N, embed_dim)

        # Add bias column: X_aug = [X, 1]
        ones = np.ones((X.shape[0], 1), dtype=np.float32)
        X_aug = np.hstack([X, ones])

        # Solve X_aug @ [W; bias] ≈ Y via least-squares
        try:
            result, _, _, _ = np.linalg.lstsq(X_aug, Y, rcond=None)
            self.W = result[:-1].astype(np.float32)   # (output_dim, embed_dim)
            self.bias = result[-1].astype(np.float32)  # (embed_dim,)
            self._is_fitted = True
            logger.info("Projector refit on %d pairs (total seen: %d)",
                        len(self._output_buf), self._total_pairs)
        except np.linalg.LinAlgError:
            logger.warning("lstsq failed — keeping previous projection")

        # Keep recent half for sliding window
        half = len(self._output_buf) // 2
        self._output_buf = self._output_buf[half:]
        self._embed_buf = self._embed_buf[half:]

    @property
    def is_fitted(self) -> bool:
        return self._is_fitted

    def save(self, path: str):
        np.savez(path, W=self.W, bias=self.bias,
                 output_dim=self.output_dim, embed_dim=self.embed_dim,
                 total_pairs=self._total_pairs, is_fitted=self._is_fitted)

    @classmethod
    def load(cls, path: str) -> "EmbeddingProjector":
        data = np.load(path)
        proj = cls(int(data["output_dim"]), int(data["embed_dim"]))
        proj.W = data["W"]
        proj.bias = data["bias"]
        proj._total_pairs = int(data["total_pairs"])
        proj._is_fitted = bool(data["is_fitted"])
        return proj


# ---------------------------------------------------------------------------
# Vocabulary Bank: embedding → nearest text
# ---------------------------------------------------------------------------

class VocabularyBank:
    """Stores (text, embedding) pairs and does nearest-neighbor lookup.

    Uses brute-force cosine similarity.  FAISS can be plugged in later for
    large vocabularies (>100K entries).
    """

    def __init__(self, embed_dim: int):
        self.embed_dim = embed_dim
        self._texts: list[str] = []
        self._embeddings: list[np.ndarray] = []
        self._matrix: np.ndarray | None = None  # cached (N, embed_dim)
        self._norms: np.ndarray | None = None    # cached L2 norms
        self._dirty = True

    def add(self, text: str, embedding: np.ndarray):
        """Add a (text, embedding) pair to the vocabulary."""
        e = np.asarray(embedding, dtype=np.float32).ravel()
        if len(e) != self.embed_dim:
            raise ValueError(f"Expected {self.embed_dim}-dim embedding, got {len(e)}")
        self._texts.append(text)
        self._embeddings.append(e)
        self._dirty = True

    def _rebuild_matrix(self):
        if not self._embeddings:
            self._matrix = np.zeros((0, self.embed_dim), dtype=np.float32)
            self._norms = np.zeros(0, dtype=np.float32)
        else:
            self._matrix = np.array(self._embeddings, dtype=np.float32)
            self._norms = np.linalg.norm(self._matrix, axis=1, keepdims=True)
            self._norms = np.maximum(self._norms, 1e-8)
        self._dirty = False

    def decode(self, embedding: np.ndarray, top_k: int = 1) -> list[tuple[str, float]]:
        """Find the nearest text(s) to the given embedding.

        Args:
            embedding: Query embedding vector.
            top_k: Number of results to return.

        Returns list of (text, cosine_similarity) tuples, sorted by similarity.
        """
        if not self._texts:
            return [("", 0.0)]

        if self._dirty:
            self._rebuild_matrix()

        q = np.asarray(embedding, dtype=np.float32).ravel()
        q_norm = np.linalg.norm(q)
        if q_norm < 1e-8:
            return [("", 0.0)]

        # Cosine similarity: (M @ q) / (||M|| * ||q||)
        similarities = (self._matrix @ q) / (self._norms.ravel() * q_norm)

        top_indices = np.argsort(similarities)[-top_k:][::-1]
        return [(self._texts[i], float(similarities[i])) for i in top_indices]

    def __len__(self):
        return len(self._texts)

    def save(self, path: str):
        data = {
            "embed_dim": self.embed_dim,
            "texts": self._texts,
        }
        np.savez(path,
                 metadata=json.dumps(data),
                 embeddings=np.array(self._embeddings, dtype=np.float32)
                            if self._embeddings else np.zeros((0, self.embed_dim)))

    @classmethod
    def load(cls, path: str) -> "VocabularyBank":
        data = np.load(path, allow_pickle=False)
        meta = json.loads(str(data["metadata"]))
        bank = cls(meta["embed_dim"])
        bank._texts = meta["texts"]
        embs = data["embeddings"]
        bank._embeddings = [embs[i] for i in range(embs.shape[0])]
        bank._dirty = True
        return bank


# ---------------------------------------------------------------------------
# Neural Decoder: full pipeline output_vector → text
# ---------------------------------------------------------------------------

class NeuralDecoder:
    """Combines EmbeddingProjector + VocabularyBank to decode brain outputs to text.

    Usage:
        decoder = NeuralDecoder(output_dim=2048, embed_dim=384)

        # During training — record pairs for projector fitting
        decoder.record_pair(brain_output, target_embedding, text="hello world")

        # Decode brain output to text
        text, similarity = decoder.decode_output(brain_output)
    """

    def __init__(self, output_dim: int, embed_dim: int = 384,
                 refit_interval: int = 1000):
        self.output_dim = output_dim
        self.embed_dim = embed_dim
        self.projector = EmbeddingProjector(output_dim, embed_dim, refit_interval)
        self.vocabulary = VocabularyBank(embed_dim)
        self._decode_count = 0
        self._frozen = False

    def record_pair(self, output_vector: np.ndarray, target_embedding: np.ndarray,
                    text: str | None = None):
        """Record a training pair for projector fitting + vocabulary building.

        No-op when decoder is frozen (during evaluation).

        Args:
            output_vector: Brain's raw output vector after forward pass
            target_embedding: The target embedding this output should match
            text: Optional text to add to vocabulary bank for this embedding
        """
        if self._frozen:
            return
        self.projector.record_pair(output_vector, target_embedding)
        if text:
            self.vocabulary.add(text, target_embedding)

    def freeze(self):
        """Freeze decoder — record_pair becomes a no-op."""
        self._frozen = True

    def unfreeze(self):
        """Unfreeze decoder — record_pair resumes normal operation."""
        self._frozen = False

    # Minimum projector training pairs before decode_output uses the projector.
    # Below this threshold, the projection matrix is essentially random (identity
    # initialization), so decoded text would be misleading.
    MIN_FITTED_PAIRS = 50

    def decode_output(self, output_vector: np.ndarray,
                      top_k: int = 1) -> list[tuple[str, float]]:
        """Decode a brain output vector to text.

        1. Project output_vector → embedding space
        2. Find nearest text in vocabulary bank

        Returns list of (text, similarity) tuples.
        Returns empty list if projector hasn't been fitted with enough samples.
        """
        if not self.projector._is_fitted and self.projector._total_pairs < self.MIN_FITTED_PAIRS:
            self._decode_count += 1
            return [("", 0.0)] * top_k
        embedding = self.projector.project(output_vector)
        results = self.vocabulary.decode(embedding, top_k=top_k)
        self._decode_count += 1
        return results

    def decode_output_text(self, output_vector: np.ndarray) -> str:
        """Convenience: decode to single best text string."""
        results = self.decode_output(output_vector, top_k=1)
        return results[0][0] if results else ""

    def force_refit(self):
        """Force projector refitting regardless of buffer size."""
        self.projector.refit()

    def save(self, directory: str):
        """Save decoder state (projector + vocabulary) to directory."""
        os.makedirs(directory, exist_ok=True)
        self.projector.save(os.path.join(directory, "projector.npz"))
        self.vocabulary.save(os.path.join(directory, "vocabulary.npz"))
        meta = {
            "output_dim": self.output_dim,
            "embed_dim": self.embed_dim,
            "decode_count": self._decode_count,
            "vocab_size": len(self.vocabulary),
            "projector_fitted": self.projector.is_fitted,
            "projector_pairs": self.projector._total_pairs,
        }
        with open(os.path.join(directory, "decoder_meta.json"), "w") as f:
            json.dump(meta, f, indent=2)
        logger.info("Decoder saved to %s (vocab=%d, projector_pairs=%d)",
                     directory, len(self.vocabulary), self.projector._total_pairs)

    @classmethod
    def load(cls, directory: str) -> "NeuralDecoder":
        """Load decoder state from directory."""
        with open(os.path.join(directory, "decoder_meta.json")) as f:
            meta = json.load(f)
        decoder = cls(meta["output_dim"], meta["embed_dim"])
        decoder._decode_count = meta.get("decode_count", 0)
        proj_path = os.path.join(directory, "projector.npz")
        if os.path.exists(proj_path):
            decoder.projector = EmbeddingProjector.load(proj_path)
        vocab_path = os.path.join(directory, "vocabulary.npz")
        if os.path.exists(vocab_path):
            decoder.vocabulary = VocabularyBank.load(vocab_path)
        logger.info("Decoder loaded from %s (vocab=%d, fitted=%s)",
                     directory, len(decoder.vocabulary), decoder.projector.is_fitted)
        return decoder
