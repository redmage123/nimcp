"""
Athena Phi-3 Inference — QLoRA-enhanced language generation

Loads Phi-3 with optional QLoRA adapter and generates natural
language from Athena's neural output + memory context.

Usage:
    inference = AthenaInference("checkpoints/athena/phi3_qlora_adapter")
    response = inference.generate(brain_output, "What is a cardinal?")
"""

import os, time


class AthenaInference:
    def __init__(self, adapter_path=None, device="cuda:0"):
        self.adapter_path = adapter_path
        self.device = device
        self.model = None
        self.tokenizer = None
        self._loaded = False

    def _lazy_load(self):
        if self._loaded:
            return
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig

        bnb = BitsAndBytesConfig(load_in_4bit=True, bnb_4bit_compute_dtype=torch.float16,
                                  bnb_4bit_quant_type="nf4")
        self.tokenizer = AutoTokenizer.from_pretrained(
            self.adapter_path or "microsoft/phi-3-mini-4k-instruct", trust_remote_code=True)
        if not self.tokenizer.pad_token:
            self.tokenizer.pad_token = self.tokenizer.eos_token

        self.model = AutoModelForCausalLM.from_pretrained(
            "microsoft/phi-3-mini-4k-instruct", quantization_config=bnb,
            device_map={"": self.device}, trust_remote_code=True, torch_dtype=torch.float16)

        if self.adapter_path and os.path.exists(self.adapter_path):
            from peft import PeftModel
            self.model = PeftModel.from_pretrained(self.model, self.adapter_path)
            print(f"Loaded QLoRA adapter: {self.adapter_path}")

        self.model.eval()
        self._loaded = True

    def generate(self, brain_output, user_query, memory_context=None,
                 max_tokens=200, temperature=0.7):
        self._lazy_load()
        import torch
        t0 = time.time()

        label = brain_output.get('label', '')
        confidence = brain_output.get('confidence', 0.0)

        parts = ["<|system|>",
                  "You are Athena, a neural cognitive system with biological circuits. "
                  "You learn through sensory experience and have personal memories. "
                  "Respond naturally. If uncertain, say so.",
                  "<|end|>", "<|user|>"]

        if label:
            parts.append(f"[Neural: '{label}', confidence: {confidence:.2f}]")
        if memory_context:
            c = memory_context.get('total_concepts', 0)
            e = memory_context.get('total_engrams', 0)
            if c or e:
                parts.append(f"[Knowledge: {c} concepts, {e} memories]")
            if memory_context.get('is_ood'):
                parts.append("[Unfamiliar input — be honest about uncertainty]")

        parts.extend([f"\n{user_query}", "<|end|>", "<|assistant|>"])
        prompt = "\n".join(parts)

        ids = self.tokenizer(prompt, return_tensors="pt", truncation=True,
                              max_length=512).input_ids.to(self.model.device)
        with torch.no_grad():
            out = self.model.generate(ids, max_new_tokens=max_tokens, temperature=temperature,
                do_sample=True, top_p=0.9, pad_token_id=self.tokenizer.pad_token_id,
                repetition_penalty=1.1)

        resp = self.tokenizer.decode(out[0][ids.shape[1]:], skip_special_tokens=True)
        for stop in ["<|end|>", "<|user|>", "<|system|>"]:
            if stop in resp:
                resp = resp[:resp.index(stop)]

        return {
            'text': resp.strip(),
            'tokens_generated': out.shape[1] - ids.shape[1],
            'latency_ms': (time.time() - t0) * 1000,
            'confidence': confidence,
        }

    def unload(self):
        import torch
        del self.model; del self.tokenizer
        self.model = self.tokenizer = None
        self._loaded = False
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
