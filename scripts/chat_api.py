#!/usr/bin/env python3
"""HTTP API for chatting with Athena via Phi-3 LoRA adapter."""
import os, sys, json, time, traceback
import numpy as np
# Allow HF downloads for Phi-3 base model on first load
# os.environ['HF_HUB_OFFLINE'] = '1'
# os.environ['TRANSFORMERS_OFFLINE'] = '1'

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from flask import Flask, request, jsonify
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

BRAIN_HOST = os.environ.get('BRAIN_HOST', '127.0.0.1')
BRAIN_PORT = int(os.environ.get('BRAIN_PORT', '9900'))

class BrainTCP:
    """TCP client for brain daemon via tcp_proxy."""
    def __init__(self, host=BRAIN_HOST, port=BRAIN_PORT, timeout=60):
        self.host = host
        self.port = port
        self.timeout = timeout

    def _send(self, cmd_dict):
        import socket, struct
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(self.timeout)
        try:
            s.connect((self.host, self.port))
            data = json.dumps(cmd_dict).encode()
            s.sendall(struct.pack('>I', len(data)) + data)
            hdr = b''
            while len(hdr) < 4:
                c = s.recv(4 - len(hdr))
                if not c: raise ConnectionError('EOF')
                hdr += c
            length = struct.unpack('>I', hdr)[0]
            body = b''
            while len(body) < length:
                c = s.recv(min(length - len(body), 65536))
                if not c: raise ConnectionError('EOF')
                body += c
            return json.loads(body)
        finally:
            s.close()

brain = BrainTCP()

# Lazy globals
_encoder = None
_phi3 = None

def get_encoder():
    global _encoder
    if _encoder is None:
        from sentence_transformers import SentenceTransformer
        _encoder = SentenceTransformer('BAAI/bge-large-en-v1.5')
        print('[CHAT] Sentence transformer loaded', flush=True)
    return _encoder

def get_phi3():
    global _phi3
    if _phi3 is None:
        import torch
        from transformers import AutoTokenizer, AutoModelForCausalLM
        from peft import PeftModel

        adapter_dir = os.environ.get('ADAPTER_DIR',
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         '..', 'checkpoints', 'athena', 'phi3_adapter'))
        base_model = 'microsoft/phi-3-mini-4k-instruct'

        print('[CHAT] Loading Phi-3 + LoRA...', flush=True)
        # Use base model tokenizer (adapter tokenizer has mismatched vocab)
        tokenizer = AutoTokenizer.from_pretrained(base_model)
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token

        base = AutoModelForCausalLM.from_pretrained(
            base_model, torch_dtype=torch.float32,
            device_map='auto', trust_remote_code=False)
        model = base
        model.eval()
        print('[CHAT] Using base Phi-3 as brain state translator', flush=True)

        _phi3 = {'model': model, 'tokenizer': tokenizer}
        print(f'[CHAT] Phi-3 ready (hidden={model.config.hidden_size})', flush=True)
    return _phi3


def decode_brain_output(brain_vec, prompt_text, brain_label='', max_tokens=128):
    """Decode brain output vector to text via Phi-3.

    Phi-3 acts as a translator: given the brain's raw neural pattern
    (label, top activations, norm), it produces a natural language
    interpretation. Phi-3 never generates independently — it only
    describes what Athena's brain actually produced.
    """
    import torch
    phi = get_phi3()
    model, tokenizer = phi['model'], phi['tokenizer']

    # Summarise brain state for Phi-3
    bv = np.array(brain_vec[:4096])
    norm = float(np.linalg.norm(bv))
    top_idx = np.argsort(np.abs(bv))[-10:][::-1]
    top_vals = [(int(i), float(bv[i])) for i in top_idx]
    pos_frac = float(np.mean(bv > 0))

    prompt = (
        f'You are translating the internal neural state of an artificial brain named Athena into natural language. '
        f'Athena was asked: "{prompt_text[:100]}"\n'
        f'Her brain produced this neural pattern:\n'
        f'- Activation norm: {norm:.2f}\n'
        f'- Closest learned label: "{brain_label}"\n'
        f'- Top activations: {top_vals[:5]}\n'
        f'- Positive fraction: {pos_frac:.2f}\n'
        f'Describe what Athena is expressing in 1-2 sentences. '
        f'Do not add information beyond what the brain pattern contains. '
        f'If the pattern is weak or incoherent, say so honestly.\n'
        f'Athena says:'
    )

    tokens = tokenizer(prompt, return_tensors='pt', truncation=True, max_length=512)
    input_ids = tokens['input_ids'].cuda()

    with torch.no_grad():
        out = model.generate(
            input_ids=input_ids,
            max_new_tokens=max_tokens, do_sample=True,
            temperature=0.7, top_p=0.9,
            pad_token_id=tokenizer.pad_token_id)

    text = tokenizer.decode(out[0][input_ids.shape[1]:], skip_special_tokens=True).strip()
    # Trim to first complete sentence
    for end in ['. ', '.\n', '!"', '?"']:
        idx = text.find(end)
        if idx > 10:
            text = text[:idx+1]
            break
    print(f'[CHAT] Phi-3 decoded: "{text[:100]}"', flush=True)
    return text


@app.route('/api/chat', methods=['POST'])
def chat():
    data = request.get_json()
    message = data.get('message', '').strip()
    use_phi3 = data.get('use_phi3', False)
    if not message:
        return jsonify({'error': 'Empty message'}), 400

    try:
        # Encode to brain input
        emb = get_encoder().encode(message).tolist()
        features = [0.0] * 1024
        for i in range(0, 1024, len(emb)):
            n = min(len(emb), 1024 - i)
            features[i:i+n] = emb[:n]

        # Brain inference
        r = brain._send({'cmd': 'decide_full', 'features': features})
        result = r.get('result', r)
        output_vec = result.get('output_vector', [])

        norm = float(np.linalg.norm(output_vec[:100])) if output_vec else 0.0
        response_text = ''
        decoder_used = 'none'

        if output_vec and any(abs(x) > 1e-8 for x in output_vec[:100]):
            # Method 1: Try Phi-3 LoRA decode (only if toggled on)
            if use_phi3:
                try:
                    print(f'[CHAT] Phi-3 decode requested, vec norm={norm:.2f}', flush=True)
                    brain_label = result.get('label', '')
                    response_text = decode_brain_output(output_vec, message, brain_label=brain_label)
                    if response_text:
                        decoder_used = 'phi3'
                        print(f'[CHAT] Phi-3 decoded: "{response_text[:80]}"', flush=True)
                except Exception as e:
                    import traceback
                    print(f'[CHAT] Phi-3 decode failed: {e}', flush=True)
                    traceback.print_exc()

            # Method 2: Try brain's native speak/generate_text
            if not response_text:
                try:
                    gen = brain._send({'cmd': 'generate_text',
                                       'semantic_input': output_vec[:1024]})
                    response_text = gen.get('text', '')
                    if response_text:
                        decoder_used = 'native'
                except Exception:
                    pass

            # Method 3: Try vocabulary decode via speak
            if not response_text:
                try:
                    speak = brain._send({'cmd': 'speak', 'features': features})
                    response_text = speak.get('text', speak.get('decoded', ''))
                    if response_text:
                        decoder_used = 'speak'
                except Exception:
                    pass

            # Method 4: Describe what the brain produced
            if not response_text:
                label = result.get('label', '')
                conf = result.get('confidence', 0)
                response_text = (f'[Neural pattern: "{label}" '
                                 f'(confidence={conf:.2f}, norm={norm:.1f})]')

            print(f'[CHAT] "{message[:50]}" → "{response_text[:80]}"', flush=True)

        if not response_text:
            response_text = '[No neural activity — brain may be loading]'

        return jsonify({
            'response': response_text,
            'confidence': min(norm / 10.0, 1.0),
            'output_norm': norm,
            'decoder': decoder_used,
            'timestamp': time.time()
        })
    except Exception as e:
        print(f'[CHAT] Error: {e}', flush=True)
        return jsonify({'error': str(e)}), 500


@app.route('/api/status', methods=['GET'])
def status():
    try:
        return jsonify(brain._send({'cmd': 'status'}))
    except Exception as e:
        return jsonify({'error': str(e)}), 500


if __name__ == '__main__':
    print('[CHAT] Pre-warming...', flush=True)
    get_encoder()
    try:
        get_phi3()
    except Exception as e:
        print(f'[CHAT] Phi-3 not available ({e}) — using fallback decoder', flush=True)
    print('[CHAT] Starting on port 8080', flush=True)
    app.run(host='0.0.0.0', port=8080, threaded=True)
