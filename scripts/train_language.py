#!/usr/bin/env python3
"""
Language Training Script for Athena/NIMCP

Trains the LNN-based language generator using text corpora.
Supports:
  - Simple text file training (line-by-line next-sentence prediction)
  - Conversational Q&A pairs (JSON format)
  - HuggingFace datasets
  - Incremental vocabulary expansion

Usage:
    python train_language.py --corpus data/corpus.txt --epochs 10
    python train_language.py --dataset wikitext --split train --epochs 5
    python train_language.py --pairs data/qa_pairs.json --epochs 20
"""

import argparse
import json
import os
import sys
import time
import signal
import random

# Add build dir to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'build', 'lib', 'python'))

import nimcp


def load_corpus_lines(path, max_lines=None):
    """Load text corpus as list of lines."""
    lines = []
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for i, line in enumerate(f):
            line = line.strip()
            if len(line) > 10:  # Skip very short lines
                lines.append(line)
            if max_lines and i >= max_lines:
                break
    return lines


def load_qa_pairs(path):
    """Load Q&A pairs from JSON file. Format: [{"input": ..., "target": ...}, ...]"""
    with open(path, 'r') as f:
        data = json.load(f)
    pairs = []
    for item in data:
        inp = item.get('input', item.get('question', item.get('prompt', '')))
        tgt = item.get('target', item.get('answer', item.get('response', '')))
        if inp and tgt:
            pairs.append((inp, tgt))
    return pairs


def load_hf_dataset(name, split='train', max_samples=10000):
    """Load a HuggingFace dataset."""
    try:
        from datasets import load_dataset
        ds = load_dataset(name, split=split)
        lines = []
        text_key = None
        for key in ['text', 'content', 'sentence', 'passage']:
            if key in ds.column_names:
                text_key = key
                break
        if not text_key:
            text_key = ds.column_names[0]

        for i, item in enumerate(ds):
            text = item[text_key]
            if isinstance(text, str) and len(text) > 10:
                lines.append(text)
            if i >= max_samples:
                break
        return lines
    except ImportError:
        print("ERROR: datasets library not installed. Run: pip install datasets")
        sys.exit(1)


def create_sentence_pairs(lines):
    """Create (input, target) pairs from consecutive lines."""
    pairs = []
    for i in range(len(lines) - 1):
        pairs.append((lines[i], lines[i + 1]))
    return pairs


def create_sliding_window_pairs(lines, window_size=3):
    """Create overlapping training pairs from text."""
    pairs = []
    for line in lines:
        words = line.split()
        if len(words) < window_size * 2:
            # Short line: use first half -> second half
            mid = len(words) // 2
            if mid > 0:
                pairs.append((' '.join(words[:mid]), ' '.join(words[mid:])))
        else:
            # Sliding window
            for i in range(0, len(words) - window_size * 2 + 1, window_size):
                inp = ' '.join(words[i:i + window_size])
                tgt = ' '.join(words[i + window_size:i + window_size * 2])
                pairs.append((inp, tgt))
    return pairs


class LanguageTrainer:
    def __init__(self, neurons=50000, init_mode='fast', lr=0.001):
        print(f"Creating brain with {neurons} neurons (init_mode={init_mode})...")
        t0 = time.time()
        self.brain = nimcp.Brain('language_trainer', neuron_count=neurons, init_mode=init_mode)
        print(f"  Brain created in {time.time() - t0:.1f}s")

        self.lr = lr
        self.losses = []
        self.running = True

        signal.signal(signal.SIGINT, self._handle_sigint)

    def _handle_sigint(self, sig, frame):
        print("\n\nInterrupted! Finishing current batch...")
        self.running = False

    def train_pairs(self, pairs, epochs=1, batch_report=100, grounded=False):
        """Train on (input, target) text pairs.

        If grounded=True, uses the grounded language system (Hebbian word-concept binding).
        Otherwise, uses the LNN autoregressive decoder.
        """
        total = len(pairs)
        mode = "grounded" if grounded else "LNN"
        print(f"\nTraining ({mode}) on {total} pairs for {epochs} epochs (lr={self.lr})")
        print("-" * 60)

        for epoch in range(epochs):
            if not self.running:
                break

            random.shuffle(pairs)
            epoch_loss = 0.0
            epoch_count = 0

            t_epoch = time.time()

            for i, (inp, tgt) in enumerate(pairs):
                if not self.running:
                    break

                if grounded:
                    # Grounded learning: distributional + pair binding
                    result = self.brain.learn_language_pair(
                        input_text=inp, target_text=tgt, learning_rate=self.lr)
                else:
                    # LNN decoder training
                    result = self.brain.train_language(inp, tgt, learning_rate=self.lr)

                loss = result.get('loss', -1.0)

                if loss >= 0:
                    epoch_loss += loss
                    epoch_count += 1
                    self.losses.append(loss)

                if (i + 1) % batch_report == 0:
                    avg = epoch_loss / max(epoch_count, 1)
                    elapsed = time.time() - t_epoch
                    rate = (i + 1) / elapsed
                    print(f"  Epoch {epoch+1}/{epochs} | "
                          f"Step {i+1}/{total} | "
                          f"Loss: {avg:.4f} | "
                          f"Rate: {rate:.1f} pairs/s")

            avg_loss = epoch_loss / max(epoch_count, 1)
            elapsed = time.time() - t_epoch
            print(f"  Epoch {epoch+1} complete: avg_loss={avg_loss:.4f}, "
                  f"time={elapsed:.1f}s, "
                  f"pairs={epoch_count}")

        return self.losses

    def learn_corpus(self, lines, epochs=1):
        """Learn from raw text lines using grounded distributional learning."""
        print(f"\nGrounded corpus learning: {len(lines)} lines, {epochs} epochs")
        print("-" * 60)

        for epoch in range(epochs):
            if not self.running:
                break
            t0 = time.time()
            for i, line in enumerate(lines):
                if not self.running:
                    break
                self.brain.learn_language(text=line)
            elapsed = time.time() - t0
            print(f"  Epoch {epoch+1}: {len(lines)} lines in {elapsed:.1f}s "
                  f"({len(lines)/elapsed:.0f} lines/s)")

    def generate_sample(self, prompt=None, semantic=None):
        """Generate a sample to check progress."""
        if prompt:
            result = self.brain.generate(prompt=prompt)
        elif semantic:
            result = self.brain.generate(semantic_input=semantic)
        else:
            # Use a simple semantic vector
            result = self.brain.generate(semantic_input=[0.5] * 128)
        return result

    def test_generation(self, prompts=None, grounded=False):
        """Test generation with some sample prompts."""
        if prompts is None:
            prompts = [
                "The brain is",
                "Neural networks can",
                "Language processing involves",
                "The world is",
            ]

        print("\n" + "=" * 60)
        mode = "GROUNDED" if grounded else "LNN"
        print(f"{mode} GENERATION SAMPLES")
        print("=" * 60)

        for prompt in prompts:
            if grounded:
                result = self.brain.grounded_respond(text=prompt)
                text = result.get('response', '')
                conf = result.get('confidence', 0)
                print(f"\n  Input: '{prompt}'")
                print(f"  Response: '{text}'")
                print(f"  Confidence: {conf:.3f}")
            else:
                result = self.generate_sample(prompt=prompt)
                text = result.get('text', '')
                conf = result.get('confidence', 0)
                perp = result.get('perplexity', 0)
                print(f"\n  Prompt: '{prompt}'")
                print(f"  Output: '{text}'")
                print(f"  Confidence: {conf:.3f}, Perplexity: {perp:.1f}")


def main():
    parser = argparse.ArgumentParser(description='Train NIMCP language generator')
    parser.add_argument('--corpus', type=str, help='Path to text corpus file')
    parser.add_argument('--pairs', type=str, help='Path to JSON Q&A pairs file')
    parser.add_argument('--dataset', type=str, help='HuggingFace dataset name')
    parser.add_argument('--split', type=str, default='train', help='Dataset split')
    parser.add_argument('--epochs', type=int, default=5, help='Number of training epochs')
    parser.add_argument('--lr', type=float, default=0.001, help='Learning rate')
    parser.add_argument('--neurons', type=int, default=50000, help='Number of neurons')
    parser.add_argument('--max-lines', type=int, default=50000, help='Max lines from corpus')
    parser.add_argument('--window', type=int, default=5, help='Sliding window size for word pairs')
    parser.add_argument('--init-mode', type=str, default='fast', help='Brain init mode (fast/full)')
    parser.add_argument('--test-every', type=int, default=1, help='Test generation every N epochs')
    parser.add_argument('--batch-report', type=int, default=100, help='Report every N pairs')
    parser.add_argument('--grounded', action='store_true',
                        help='Use grounded language system (Hebbian) instead of LNN decoder')

    args = parser.parse_args()

    # Load training data
    pairs = []

    if args.corpus:
        print(f"Loading corpus: {args.corpus}")
        lines = load_corpus_lines(args.corpus, max_lines=args.max_lines)
        print(f"  Loaded {len(lines)} lines")

        # Create training pairs
        sent_pairs = create_sentence_pairs(lines)
        window_pairs = create_sliding_window_pairs(lines, window_size=args.window)
        pairs = sent_pairs + window_pairs
        print(f"  Created {len(sent_pairs)} sentence pairs + {len(window_pairs)} window pairs")

    elif args.pairs:
        print(f"Loading Q&A pairs: {args.pairs}")
        pairs = load_qa_pairs(args.pairs)
        print(f"  Loaded {len(pairs)} pairs")

    elif args.dataset:
        print(f"Loading HuggingFace dataset: {args.dataset}")
        lines = load_hf_dataset(args.dataset, split=args.split,
                                max_samples=args.max_lines)
        print(f"  Loaded {len(lines)} samples")
        pairs = create_sentence_pairs(lines) + \
                create_sliding_window_pairs(lines, window_size=args.window)
        print(f"  Created {len(pairs)} training pairs")

    else:
        # Default: use some built-in training sentences
        print("No corpus specified. Using built-in training data.")
        builtin_pairs = [
            ("the brain processes", "information through neural networks"),
            ("language is a", "complex cognitive ability"),
            ("neural networks learn", "patterns from data"),
            ("the visual cortex", "processes visual information"),
            ("speech production requires", "coordination of many brain regions"),
            ("memory consolidation happens", "during sleep cycles"),
            ("attention is controlled", "by the prefrontal cortex"),
            ("emotions influence", "decision making processes"),
            ("learning requires", "repeated practice and exposure"),
            ("the hippocampus is", "important for memory formation"),
            ("synaptic plasticity is", "the basis of learning"),
            ("neurons communicate through", "electrical and chemical signals"),
            ("the cerebellum coordinates", "movement and balance"),
            ("consciousness arises from", "complex neural interactions"),
            ("perception involves", "interpreting sensory signals"),
        ]
        pairs = builtin_pairs * 50  # Repeat for more training
        print(f"  Using {len(pairs)} built-in training pairs")

    if not pairs:
        print("ERROR: No training pairs found!")
        sys.exit(1)

    # Create trainer and train
    trainer = LanguageTrainer(
        neurons=args.neurons,
        init_mode=args.init_mode,
        lr=args.lr
    )

    # Initial generation test
    trainer.test_generation(grounded=args.grounded)

    if args.grounded and args.corpus:
        # For grounded mode with corpus, do distributional learning first
        lines = load_corpus_lines(args.corpus, max_lines=args.max_lines)
        trainer.learn_corpus(lines, epochs=min(args.epochs, 3))

    # Train on pairs
    losses = trainer.train_pairs(
        pairs,
        epochs=args.epochs,
        batch_report=args.batch_report,
        grounded=args.grounded
    )

    # Final generation test
    trainer.test_generation(grounded=args.grounded)

    # Summary
    if losses:
        print(f"\n{'=' * 60}")
        print(f"TRAINING SUMMARY")
        print(f"{'=' * 60}")
        print(f"  Total steps: {len(losses)}")
        print(f"  Initial loss: {losses[0]:.4f}")
        print(f"  Final loss: {losses[-1]:.4f}")
        if len(losses) > 100:
            first_100 = sum(losses[:100]) / 100
            last_100 = sum(losses[-100:]) / 100
            print(f"  First 100 avg: {first_100:.4f}")
            print(f"  Last 100 avg: {last_100:.4f}")
            print(f"  Improvement: {(first_100 - last_100) / first_100 * 100:.1f}%")


if __name__ == '__main__':
    main()
