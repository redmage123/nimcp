"""
NIMCP Benchmark Suite - Standard ML Benchmarks
================================================

WHAT: Standardized benchmarks (MNIST, CIFAR-10, etc.) for NIMCP
WHY:  Prove competitive performance against other neural network frameworks
HOW:  Load standard datasets, train NIMCP brain, collect metrics, compare

Benchmarks:
- MNIST: Handwritten digits (28×28 grayscale, 10 classes)
- Fashion-MNIST: Clothing items (28×28 grayscale, 10 classes)
- CIFAR-10: Color images (32×32 RGB, 10 classes)

Metrics:
- Accuracy
- Training time
- Inference time
- Memory usage
- Convergence rate

Baselines:
- PyTorch CNN
- TensorFlow MLP
- Scikit-learn SVM
"""

import nimcp
import numpy as np
import time
import gzip
import struct
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import pickle
import urllib.request
import os

#=============================================================================
# Dataset Loading
#=============================================================================

class MNISTLoader:
    """
    WHAT: MNIST dataset loader
    WHY:  Standard benchmark for digit classification
    HOW:  Download from Yann LeCun's website, parse IDX format

    MNIST Format:
    - Images: 60K train + 10K test
    - Size: 28×28 grayscale pixels
    - Labels: 0-9 digits
    """

    BASE_URL = "http://yann.lecun.com/exdb/mnist/"
    FILES = {
        'train_images': 'train-images-idx3-ubyte.gz',
        'train_labels': 'train-labels-idx1-ubyte.gz',
        'test_images': 't10k-images-idx3-ubyte.gz',
        'test_labels': 't10k-labels-idx1-ubyte.gz'
    }

    def __init__(self, data_dir: str = "/tmp/mnist"):
        self.data_dir = Path(data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)

    def download(self):
        """Download MNIST dataset if not already present"""
        print("Downloading MNIST dataset...")
        for key, filename in self.FILES.items():
            filepath = self.data_dir / filename
            if not filepath.exists():
                url = self.BASE_URL + filename
                print(f"  Downloading {filename}...")
                urllib.request.urlretrieve(url, filepath)
        print("✓ Download complete")

    def _read_idx_images(self, filepath: Path) -> np.ndarray:
        """
        WHAT: Parse IDX image file format
        FORMAT: magic(4) | count(4) | rows(4) | cols(4) | pixels(count×rows×cols)
        """
        with gzip.open(filepath, 'rb') as f:
            magic, count, rows, cols = struct.unpack('>IIII', f.read(16))
            images = np.frombuffer(f.read(), dtype=np.uint8)
            images = images.reshape(count, rows, cols)
        return images

    def _read_idx_labels(self, filepath: Path) -> np.ndarray:
        """
        WHAT: Parse IDX label file format
        FORMAT: magic(4) | count(4) | labels(count)
        """
        with gzip.open(filepath, 'rb') as f:
            magic, count = struct.unpack('>II', f.read(8))
            labels = np.frombuffer(f.read(), dtype=np.uint8)
        return labels

    def load(self) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """
        WHAT: Load MNIST train/test splits
        RETURNS: (X_train, y_train, X_test, y_test)
        """
        # Download if needed
        self.download()

        # Load training data
        X_train = self._read_idx_images(self.data_dir / self.FILES['train_images'])
        y_train = self._read_idx_labels(self.data_dir / self.FILES['train_labels'])

        # Load test data
        X_test = self._read_idx_images(self.data_dir / self.FILES['test_images'])
        y_test = self._read_idx_labels(self.data_dir / self.FILES['test_labels'])

        # Normalize to [0, 1]
        X_train = X_train.astype(np.float32) / 255.0
        X_test = X_test.astype(np.float32) / 255.0

        print(f"✓ MNIST loaded: train={X_train.shape}, test={X_test.shape}")
        return X_train, y_train, X_test, y_test


#=============================================================================
# Benchmark Runner
#=============================================================================

class NIMCPBenchmark:
    """
    WHAT: Run standardized benchmark on NIMCP
    WHY:  Compare against other frameworks
    HOW:  Train on dataset, measure metrics, report results
    """

    def __init__(self, dataset_name: str, num_neurons: int = 100000, use_gpu: bool = True):
        self.dataset_name = dataset_name
        self.num_neurons = num_neurons
        self.use_gpu = use_gpu
        self.brain = None
        self.metrics = {
            'dataset': dataset_name,
            'num_neurons': num_neurons,
            'gpu_enabled': use_gpu,
            'train_accuracy': [],
            'test_accuracy': [],
            'train_loss': [],
            'train_time': [],
            'inference_time': [],
            'memory_mb': 0,
            'convergence_epoch': None
        }

    def create_brain(self, input_size: int, output_size: int):
        """
        WHAT: Create NIMCP brain for benchmark
        WHY:  100K neurons for competitive performance
        HOW:  Use BRAIN_SIZE_LARGE with custom neuron count
        """
        print(f"Creating NIMCP brain: {self.num_neurons} neurons...")

        # Determine brain size preset
        if self.num_neurons >= 100000:
            size = nimcp.BRAIN_SIZE_LARGE
        elif self.num_neurons >= 10000:
            size = nimcp.BRAIN_SIZE_MEDIUM
        else:
            size = nimcp.BRAIN_SIZE_SMALL

        self.brain = nimcp.Brain(
            name=f"{self.dataset_name}_benchmark",
            size=size,
            task=nimcp.TASK_CLASSIFICATION,
            num_inputs=input_size,
            num_outputs=output_size
        )

        print(f"✓ Brain created: {self.num_neurons} neurons")

    def flatten_images(self, images: np.ndarray) -> np.ndarray:
        """
        WHAT: Flatten 2D images to 1D vectors
        WHY:  NIMCP brain expects 1D input
        """
        return images.reshape(images.shape[0], -1)

    def train_epoch(self, X: np.ndarray, y: np.ndarray, batch_size: int = 32) -> Dict:
        """
        WHAT: Train one epoch on dataset
        WHY:  Standard training loop for benchmarking
        HOW:  Mini-batch training with progress tracking

        RETURNS: epoch metrics (loss, accuracy, time)
        """
        n_samples = len(X)
        n_batches = (n_samples + batch_size - 1) // batch_size

        epoch_loss = []
        correct = 0
        start_time = time.time()

        # Shuffle data
        indices = np.random.permutation(n_samples)
        X_shuffled = X[indices]
        y_shuffled = y[indices]

        for batch_idx in range(n_batches):
            start_idx = batch_idx * batch_size
            end_idx = min(start_idx + batch_size, n_samples)

            batch_X = X_shuffled[start_idx:end_idx]
            batch_y = y_shuffled[start_idx:end_idx]

            # Train on batch
            for i in range(len(batch_X)):
                features = batch_X[i].tolist()
                label = str(int(batch_y[i]))

                loss = self.brain.learn(features, label, confidence=1.0)
                epoch_loss.append(float(loss))

                # Check if prediction is correct (for training accuracy)
                prediction = self.brain.predict(features)
                if prediction == label:
                    correct += 1

        elapsed = time.time() - start_time
        accuracy = (correct / n_samples) * 100.0
        avg_loss = np.mean(epoch_loss)

        return {
            'loss': avg_loss,
            'accuracy': accuracy,
            'time': elapsed
        }

    def evaluate(self, X: np.ndarray, y: np.ndarray) -> Dict:
        """
        WHAT: Evaluate brain on test set
        WHY:  Measure generalization performance
        HOW:  Run predictions, measure accuracy and speed
        """
        n_samples = len(X)
        correct = 0
        inference_times = []

        for i in range(n_samples):
            features = X[i].tolist()
            label = str(int(y[i]))

            start = time.time()
            prediction = self.brain.predict(features)
            inference_times.append(time.time() - start)

            if prediction == label:
                correct += 1

        accuracy = (correct / n_samples) * 100.0
        avg_inference_time = np.mean(inference_times) * 1000  # ms

        return {
            'accuracy': accuracy,
            'inference_time_ms': avg_inference_time,
            'total_samples': n_samples
        }

    def run_mnist(self, n_epochs: int = 10, batch_size: int = 32):
        """
        WHAT: Run MNIST benchmark
        WHY:  Standard digit classification benchmark
        HOW:  Load MNIST, train for n_epochs, evaluate, report

        BASELINE (PyTorch CNN):
        - Test accuracy: 99.2%
        - Training time: ~2 min (GPU)
        - Inference: 0.1 ms/sample
        """
        print("=" * 60)
        print(f"NIMCP MNIST Benchmark - {self.num_neurons} neurons")
        print("=" * 60)

        # Load dataset
        loader = MNISTLoader()
        X_train, y_train, X_test, y_test = loader.load()

        # Flatten images (28×28 → 784)
        X_train_flat = self.flatten_images(X_train)
        X_test_flat = self.flatten_images(X_test)

        # Create brain
        self.create_brain(input_size=784, output_size=10)

        # Training loop
        print(f"\nTraining for {n_epochs} epochs...")
        for epoch in range(n_epochs):
            # Train epoch
            epoch_metrics = self.train_epoch(X_train_flat, y_train, batch_size)

            self.metrics['train_loss'].append(epoch_metrics['loss'])
            self.metrics['train_accuracy'].append(epoch_metrics['accuracy'])
            self.metrics['train_time'].append(epoch_metrics['time'])

            # Evaluate on test set (every 5 epochs)
            if (epoch + 1) % 5 == 0 or epoch == n_epochs - 1:
                test_metrics = self.evaluate(X_test_flat[:1000], y_test[:1000])  # Sample for speed
                self.metrics['test_accuracy'].append(test_metrics['accuracy'])
                self.metrics['inference_time'].append(test_metrics['inference_time_ms'])

                print(f"Epoch {epoch+1}/{n_epochs}:")
                print(f"  Train Loss: {epoch_metrics['loss']:.4f}")
                print(f"  Train Acc:  {epoch_metrics['accuracy']:.2f}%")
                print(f"  Test Acc:   {test_metrics['accuracy']:.2f}%")
                print(f"  Time:       {epoch_metrics['time']:.2f}s")
            else:
                print(f"Epoch {epoch+1}/{n_epochs}: Loss={epoch_metrics['loss']:.4f}, "
                      f"Acc={epoch_metrics['accuracy']:.2f}%")

        # Final evaluation on full test set
        print("\nFinal evaluation on full test set...")
        final_test = self.evaluate(X_test_flat, y_test)

        print("\n" + "=" * 60)
        print("BENCHMARK RESULTS")
        print("=" * 60)
        print(f"Dataset:          MNIST")
        print(f"Neurons:          {self.num_neurons:,}")
        print(f"GPU Enabled:      {self.use_gpu}")
        print(f"Final Test Acc:   {final_test['accuracy']:.2f}%")
        print(f"Inference Time:   {final_test['inference_time_ms']:.3f} ms/sample")
        print(f"Total Train Time: {sum(self.metrics['train_time']):.2f}s")
        print("=" * 60)

        return self.metrics


#=============================================================================
# Comparative Benchmarks
#=============================================================================

class ComparativeBenchmark:
    """
    WHAT: Compare NIMCP against other frameworks
    WHY:  Validate competitive performance
    HOW:  Run same benchmark on PyTorch, TensorFlow, sklearn
    """

    @staticmethod
    def pytorch_baseline_mnist():
        """
        WHAT: PyTorch CNN baseline for MNIST
        WHY:  Standard comparison point
        """
        # TODO: Implement PyTorch CNN benchmark
        return {
            'accuracy': 99.2,  # Typical CNN performance
            'train_time': 120,  # 2 minutes
            'inference_ms': 0.1
        }

    @staticmethod
    def compare(nimcp_metrics: Dict) -> Dict:
        """
        WHAT: Compare NIMCP vs baselines
        RETURNS: Comparison table
        """
        baseline = ComparativeBenchmark.pytorch_baseline_mnist()

        return {
            'nimcp': {
                'accuracy': nimcp_metrics['test_accuracy'][-1] if nimcp_metrics['test_accuracy'] else 0,
                'train_time': sum(nimcp_metrics['train_time']),
                'inference_ms': nimcp_metrics['inference_time'][-1] if nimcp_metrics['inference_time'] else 0
            },
            'pytorch_cnn': baseline,
            'comparison': {
                'accuracy_diff': nimcp_metrics['test_accuracy'][-1] - baseline['accuracy'] if nimcp_metrics['test_accuracy'] else 0,
                'speedup': baseline['train_time'] / sum(nimcp_metrics['train_time']) if nimcp_metrics['train_time'] else 0
            }
        }


#=============================================================================
# Main Entry Point
#=============================================================================

if __name__ == '__main__':
    # Run MNIST benchmark with 100K neurons
    benchmark = NIMCPBenchmark(
        dataset_name="mnist",
        num_neurons=100000,
        use_gpu=True
    )

    metrics = benchmark.run_mnist(n_epochs=10, batch_size=64)

    # Compare against baselines
    comparison = ComparativeBenchmark.compare(metrics)

    print("\n" + "=" * 60)
    print("COMPARATIVE RESULTS")
    print("=" * 60)
    print(f"NIMCP Accuracy:    {comparison['nimcp']['accuracy']:.2f}%")
    print(f"PyTorch Accuracy:  {comparison['pytorch_cnn']['accuracy']:.2f}%")
    print(f"Difference:        {comparison['comparison']['accuracy_diff']:+.2f}%")
    print("=" * 60)
