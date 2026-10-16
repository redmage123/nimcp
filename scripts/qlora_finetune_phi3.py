#!/usr/bin/env python3
"""
QLoRA Fine-Tuning of Phi-3 on Athena's Neural Output

Fine-tunes Phi-3 Mini using QLoRA (4-bit base + LoRA adapters)
to decode Athena's neural embeddings into natural language.

VRAM: ~2.1 GB (runs alongside 10 GB brain training)

Usage:
    python3 qlora_finetune_phi3.py --data checkpoints/athena/lora_training_data.jsonl
    python3 qlora_finetune_phi3.py --data ... --epochs 3 --lr 2e-4
    python3 qlora_finetune_phi3.py --resume checkpoints/athena/phi3_qlora_adapter/
"""

import argparse, json, os, sys, time

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default="checkpoints/athena/lora_training_data.jsonl")
    parser.add_argument("--output", default="checkpoints/athena/phi3_qlora_adapter")
    parser.add_argument("--model", default="microsoft/phi-3-mini-4k-instruct")
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--lr", type=float, default=2e-4)
    parser.add_argument("--batch_size", type=int, default=4)
    parser.add_argument("--max_samples", type=int, default=None)
    parser.add_argument("--lora_r", type=int, default=16)
    parser.add_argument("--lora_alpha", type=int, default=32)
    parser.add_argument("--max_length", type=int, default=512)
    parser.add_argument("--resume", default=None)
    parser.add_argument("--gpu_id", type=int, default=0)
    args = parser.parse_args()

    for pkg in ['torch', 'transformers', 'peft', 'bitsandbytes', 'datasets']:
        try: __import__(pkg)
        except ImportError:
            print(f"Missing: {pkg}. Install: pip install {pkg}")
            sys.exit(1)

    import torch
    from transformers import (AutoModelForCausalLM, AutoTokenizer,
        TrainingArguments, Trainer, BitsAndBytesConfig, DataCollatorForLanguageModeling)
    from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training, PeftModel
    from datasets import Dataset

    print("=" * 60)
    print("  Athena QLoRA Fine-Tuning")
    print("=" * 60)

    if not os.path.exists(args.data):
        print(f"ERROR: No data at {args.data}")
        sys.exit(1)

    # Load data
    samples = []
    with open(args.data) as f:
        for line in f:
            try:
                s = json.loads(line.strip())
                if s.get('description') and len(s['description']) >= 10:
                    samples.append(s)
            except: pass
            if args.max_samples and len(samples) >= args.max_samples:
                break
    print(f"Loaded {len(samples)} samples")
    if len(samples) < 10:
        print("Need at least 10 samples"); sys.exit(1)

    # Format as prompt/completion pairs
    def format_pair(s):
        label = s.get('label', '')
        desc = s.get('description', '')
        emb = s.get('embedding', [])
        mean_act = sum(emb) / max(len(emb), 1)
        confidence = max(0, min(1, 1.0 - s.get('loss', 0) / 10000.0))
        prompt = (f"<|system|>\nYou are Athena, a neural cognitive system.\n<|end|>\n"
                  f"<|user|>\n[Neural: '{label}', conf={confidence:.2f}, act={mean_act:.3f}]\n"
                  f"Describe: {label}\n<|end|>\n<|assistant|>\n")
        return prompt + desc.strip() + "\n<|end|>"

    texts = [format_pair(s) for s in samples]
    split = max(1, int(len(texts) * 0.9))
    train_texts, eval_texts = texts[:split], texts[split:]
    print(f"Train: {len(train_texts)}, Eval: {len(eval_texts)}")

    # Load tokenizer
    tokenizer = AutoTokenizer.from_pretrained(args.model, trust_remote_code=True)
    if not tokenizer.pad_token: tokenizer.pad_token = tokenizer.eos_token

    def tokenize(ex):
        return tokenizer(ex["text"], truncation=True, max_length=args.max_length, padding="max_length")

    train_ds = Dataset.from_list([{"text": t} for t in train_texts]).map(tokenize, batched=True, remove_columns=["text"])
    eval_ds = Dataset.from_list([{"text": t} for t in eval_texts]).map(tokenize, batched=True, remove_columns=["text"])

    # Load 4-bit model
    print(f"\nLoading {args.model} in 4-bit...")
    bnb = BitsAndBytesConfig(load_in_4bit=True, bnb_4bit_compute_dtype=torch.float16,
                              bnb_4bit_quant_type="nf4", bnb_4bit_use_double_quant=True)
    model = AutoModelForCausalLM.from_pretrained(args.model, quantization_config=bnb,
        device_map={"": args.gpu_id}, trust_remote_code=True, torch_dtype=torch.float16)
    model = prepare_model_for_kbit_training(model)

    if args.resume and os.path.exists(args.resume):
        model = PeftModel.from_pretrained(model, args.resume)
        print(f"Resumed from {args.resume}")
    else:
        lora = LoraConfig(r=args.lora_r, lora_alpha=args.lora_alpha, lora_dropout=0.05,
            bias="none", task_type="CAUSAL_LM",
            target_modules=["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"])
        model = get_peft_model(model, lora)

    model.print_trainable_parameters()
    if torch.cuda.is_available():
        print(f"VRAM: {torch.cuda.memory_allocated(args.gpu_id)/1e9:.1f} GB")

    os.makedirs(args.output, exist_ok=True)
    training_args = TrainingArguments(
        output_dir=args.output, num_train_epochs=args.epochs,
        per_device_train_batch_size=args.batch_size, per_device_eval_batch_size=args.batch_size,
        gradient_accumulation_steps=4, learning_rate=args.lr, weight_decay=0.01,
        warmup_steps=min(100, len(train_texts) // 16), logging_steps=10,
        save_steps=100, eval_strategy="steps", eval_steps=100, save_total_limit=3,
        fp16=True, optim="paged_adamw_8bit", lr_scheduler_type="cosine", report_to="none")

    t0 = time.time()
    Trainer(model=model, args=training_args, train_dataset=train_ds, eval_dataset=eval_ds,
        data_collator=DataCollatorForLanguageModeling(tokenizer, mlm=False), tokenizer=tokenizer).train()
    elapsed = time.time() - t0

    model.save_pretrained(args.output)
    tokenizer.save_pretrained(args.output)
    size_mb = sum(os.path.getsize(os.path.join(args.output, f))
                  for f in os.listdir(args.output) if os.path.isfile(os.path.join(args.output, f))) / 1e6

    print(f"\nDone in {elapsed/60:.1f} min. Adapter: {args.output} ({size_mb:.1f} MB)")

    # Quick eval
    model.eval()
    for s in samples[split:split+3]:
        prompt = (f"<|system|>\nYou are Athena.\n<|end|>\n<|user|>\nDescribe: {s.get('label','')}\n"
                  f"<|end|>\n<|assistant|>\n")
        ids = tokenizer(prompt, return_tensors="pt").input_ids.to(model.device)
        with torch.no_grad():
            out = model.generate(ids, max_new_tokens=80, temperature=0.7, do_sample=True,
                                  pad_token_id=tokenizer.pad_token_id)
        resp = tokenizer.decode(out[0][ids.shape[1]:], skip_special_tokens=True)
        for stop in ["<|end|>", "<|user|>"]:
            if stop in resp: resp = resp[:resp.index(stop)]
        print(f"\n  Label: {s.get('label')}")
        print(f"  Target: {s.get('description','')[:80]}...")
        print(f"  Athena: {resp.strip()[:80]}...")

if __name__ == "__main__":
    main()
