"""
Spectral K-Fold Cross Validation for Cognitive Training

Instead of random folds, uses spectral clustering on the data similarity
graph to create folds that respect the underlying data manifold structure.
Each fold holds out an entire cluster of semantically related items,
making it much harder to overfit than random k-fold.

Algorithm:
1. Encode all items to embeddings (sentence-transformers)
2. Build cosine similarity matrix
3. Compute graph Laplacian (L = D - S)
4. Eigendecompose, take first k non-trivial eigenvectors
5. K-means on spectral features -> fold assignments
6. Stratify by domain to ensure each fold has representation

Usage:
    splitter = SpectralKFoldSplitter(items, k=5)
    for fold_idx in range(5):
        train, test = splitter.get_fold(fold_idx)
        # train on train, evaluate on test
"""

import numpy as np
from collections import defaultdict


class SpectralKFoldSplitter:
    def __init__(self, items, k=5, seed=42, encoder=None):
        """
        Args:
            items: list of dicts with 'text', 'answer', 'label', 'domain'
            k: number of folds
            seed: random seed for reproducibility
            encoder: function that takes text -> numpy array embedding
                     If None, uses sentence-transformers
        """
        self.items = items
        self.k = k
        self.seed = seed
        self.n = len(items)

        # Get encoder
        if encoder is None:
            encoder = self._default_encoder()

        # 1. Encode all items
        texts = [f"{item['text']} {item['answer']}" for item in items]
        embeddings = np.array([encoder(t) for t in texts])

        # 2. Build cosine similarity matrix
        norms = np.linalg.norm(embeddings, axis=1, keepdims=True)
        norms = np.maximum(norms, 1e-8)
        normalized = embeddings / norms
        similarity = normalized @ normalized.T

        # Make non-negative (shift if needed)
        similarity = (similarity + 1.0) / 2.0  # Map [-1,1] to [0,1]
        np.fill_diagonal(similarity, 0)  # No self-loops

        # 3. Graph Laplacian
        degree = np.sum(similarity, axis=1)
        D = np.diag(degree)
        L = D - similarity

        # Normalized Laplacian for better numerical properties
        D_inv_sqrt = np.diag(1.0 / np.maximum(np.sqrt(degree), 1e-8))
        L_norm = D_inv_sqrt @ L @ D_inv_sqrt

        # 4. Eigendecompose — take smallest k+1 eigenvectors (skip first)
        eigenvalues, eigenvectors = np.linalg.eigh(L_norm)
        spectral_features = eigenvectors[:, 1:k+1]  # Skip trivial first eigenvector

        # 5. K-means on spectral features
        self.fold_assignments = self._kmeans(spectral_features, k, seed)

        # 6. Domain-stratified rebalancing
        # Ensure each fold has items from each domain
        self._rebalance_domains()

        # Cache fold indices
        self._fold_indices = defaultdict(list)
        for i, fold in enumerate(self.fold_assignments):
            self._fold_indices[fold].append(i)

        # Cache test labels per fold for fast lookup
        self._test_label_sets = {}
        for fold_idx in range(self.k):
            self._test_label_sets[fold_idx] = set(
                self.items[i]['label'] for i in self._fold_indices[fold_idx]
            )

    def _default_encoder(self):
        """Use sentence-transformers if available, else hash-based fallback."""
        try:
            import sys
            sys.path.insert(0, '/home/bbrelin/nimcp/scripts')
            from claude_teacher import encode_text
            return encode_text
        except ImportError:
            # Fallback: hash-based pseudo-embedding (for testing)
            def hash_encoder(text):
                import hashlib
                h = hashlib.sha256(text.encode()).digest()
                return np.frombuffer(h, dtype=np.float32)[:8]
            return hash_encoder

    def _kmeans(self, X, k, seed, max_iter=100):
        """Simple k-means clustering."""
        rng = np.random.RandomState(seed)
        n = X.shape[0]

        # K-means++ initialization
        centers = [X[rng.randint(n)]]
        for _ in range(1, k):
            dists = np.array([min(np.sum((x - c)**2) for c in centers) for x in X])
            probs = dists / (dists.sum() + 1e-10)
            centers.append(X[rng.choice(n, p=probs)])
        centers = np.array(centers)

        # Iterate
        assignments = np.zeros(n, dtype=int)
        for _ in range(max_iter):
            # Assign
            dists = np.array([[np.sum((x - c)**2) for c in centers] for x in X])
            new_assignments = np.argmin(dists, axis=1)
            if np.all(new_assignments == assignments):
                break
            assignments = new_assignments
            # Update centers
            for j in range(k):
                mask = assignments == j
                if mask.any():
                    centers[j] = X[mask].mean(axis=0)

        return assignments.tolist()

    def _rebalance_domains(self):
        """Ensure each fold has at least one item from each domain."""
        domain_items = defaultdict(lambda: defaultdict(list))
        for i, item in enumerate(self.items):
            domain_items[item['domain']][self.fold_assignments[i]].append(i)

        # For any domain missing from a fold, swap one item from an over-represented fold
        all_domains = set(item['domain'] for item in self.items)
        for domain in all_domains:
            folds_with_domain = set(domain_items[domain].keys())
            folds_without = set(range(self.k)) - folds_with_domain

            for missing_fold in folds_without:
                # Find the fold with most items of this domain
                if not domain_items[domain]:
                    continue
                donor_fold = max(domain_items[domain].keys(),
                               key=lambda f: len(domain_items[domain][f]))
                if len(domain_items[domain][donor_fold]) > 1:
                    # Move one item
                    idx = domain_items[domain][donor_fold].pop()
                    self.fold_assignments[idx] = missing_fold
                    domain_items[domain][missing_fold].append(idx)

    def get_fold(self, fold_idx):
        """Returns (train_items, test_items) for this fold.

        Test = items in fold_idx. Train = everything else.
        """
        assert 0 <= fold_idx < self.k
        train = [self.items[i] for i in range(self.n) if self.fold_assignments[i] != fold_idx]
        test = [self.items[i] for i in self._fold_indices[fold_idx]]
        return train, test

    def get_train_labels(self, fold_idx):
        """Labels for training items in this fold."""
        return [self.items[i]['label'] for i in range(self.n) if self.fold_assignments[i] != fold_idx]

    def get_test_labels(self, fold_idx):
        """Labels for test items in this fold (never train on these)."""
        return [self.items[i]['label'] for i in self._fold_indices[fold_idx]]

    def is_test_item(self, label, fold_idx):
        """Fast check: should this label be skipped during training?"""
        return label in self._test_label_sets[fold_idx]

    def get_fold_stats(self):
        """Return per-fold statistics."""
        stats = {}
        for fold_idx in range(self.k):
            train, test = self.get_fold(fold_idx)
            train_domains = defaultdict(int)
            test_domains = defaultdict(int)
            for item in train:
                train_domains[item['domain']] += 1
            for item in test:
                test_domains[item['domain']] += 1
            stats[fold_idx] = {
                'train_count': len(train),
                'test_count': len(test),
                'train_domains': dict(train_domains),
                'test_domains': dict(test_domains),
            }
        return stats

    def print_summary(self):
        """Print fold statistics."""
        stats = self.get_fold_stats()
        print(f"\nSpectral {self.k}-Fold Cross Validation Summary")
        print(f"Total items: {self.n}")
        print(f"{'Fold':>6} {'Train':>8} {'Test':>8} {'Domains in Test':>20}")
        print("-" * 50)
        for fold_idx in range(self.k):
            s = stats[fold_idx]
            domains = ', '.join(sorted(s['test_domains'].keys()))
            print(f"{fold_idx:>6} {s['train_count']:>8} {s['test_count']:>8}   {domains}")


# Convenience function
def create_spectral_splitter(k=5, seed=42):
    """Create a spectral k-fold splitter from the full cognitive dataset."""
    from cognitive_training_data import get_all_cognitive_data
    items = get_all_cognitive_data()
    return SpectralKFoldSplitter(items, k=k, seed=seed)
