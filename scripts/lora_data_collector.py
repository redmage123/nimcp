"""
LoRA Training Data Collector — Automatic Collection During Training

Collects (neural_output, description) pairs during training for future
QLoRA fine-tuning of Phi-3 on Athena's neural output space.

Data format (JSONL):
    {"embedding": [0.1, ...], "description": "...", "label": "...",
     "stage": 0, "step": 1850, "loss": 2179.8, "timestamp": "..."}
"""

import json
import os
import time


class LoRADataCollector:
    def __init__(self, output_path, buffer_size=100, max_samples=100000):
        self.output_path = output_path
        self.buffer = []
        self.buffer_size = buffer_size
        self.max_samples = max_samples
        self.total_collected = 0
        if os.path.exists(output_path):
            with open(output_path) as f:
                self.total_collected = sum(1 for _ in f)

    def collect(self, output_vector, description, label="",
                stage=0, step=0, loss=0.0):
        if self.total_collected >= self.max_samples:
            return
        if hasattr(output_vector, 'tolist'):
            output_vector = output_vector.tolist()
        if not description or len(description) < 10:
            return
        # Subsample: always collect first 1000, then every 10th familiar
        if self.total_collected > 1000 and loss < 100.0:
            if step % 10 != 0:
                return
        # Truncate embedding
        if len(output_vector) > 256:
            output_vector = output_vector[:256]
        self.buffer.append({
            "embedding": output_vector,
            "description": description[:512],
            "label": label[:64] if label else "",
            "stage": stage,
            "step": step,
            "loss": round(loss, 4),
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S")
        })
        if len(self.buffer) >= self.buffer_size:
            self.flush()

    def flush(self):
        if not self.buffer:
            return
        try:
            with open(self.output_path, 'a') as f:
                for s in self.buffer:
                    f.write(json.dumps(s) + '\n')
            self.total_collected += len(self.buffer)
            self.buffer.clear()
        except Exception as e:
            print(f"  [LoRA Collector] Flush failed: {e}")

    def get_stats(self):
        return {
            "total_collected": self.total_collected,
            "buffer_pending": len(self.buffer),
            "max_samples": self.max_samples,
            "file_path": self.output_path,
            "file_size_mb": (os.path.getsize(self.output_path) / (1024 * 1024)
                             if os.path.exists(self.output_path) else 0)
        }
