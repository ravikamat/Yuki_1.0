#!/usr/bin/env python3
"""
Yuki LocalTransformer Fine-Tuning Script
=========================================
Uses llama.cpp LoRA fine-tuning on SFT/DPO data extracted by DistillationExtractor.
Supports both SFT (Supervised Fine-Tuning) and DPO (Direct Preference Optimization).

Usage:
    python scripts/train_local_transformer.py --sft data/distillation/sft.jsonl --dpo data/distillation/dpo.jsonl --output data/models/
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from datetime import datetime

def run_cmd(cmd, cwd=None, env=None):
    """Run command and return (success, stdout, stderr)"""
    print(f"[RUN] {cmd}")
    try:
        result = subprocess.run(
            cmd, shell=True, cwd=cwd, env=env,
            capture_output=True, text=True, timeout=3600
        )
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        print(f"[ERROR] Command timed out: {cmd}", file=sys.stderr)
        return False, "", "Timeout"
    except Exception as e:
        print(f"[ERROR] Failed to run: {cmd} - {e}", file=sys.stderr)
        return False, "", str(e)

def check_llama_cpp():
    """Check if llama.cpp is available"""
    # Check common locations
    paths = [
        "llama.cpp",
        "../llama.cpp",
        "../../llama.cpp",
        os.environ.get("LLAMA_CPP_PATH", ""),
    ]
    for p in paths:
        if p and os.path.exists(os.path.join(p, "llama-lora")):
            return p
    return None

def prepare_training_data(sft_path, dpo_path, output_dir):
    """Prepare and validate training data"""
    if not os.path.exists(sft_path):
        print(f"[ERROR] SFT file not found: {sft_path}")
        return False
    
    if not os.path.exists(dpo_path):
        print(f"[ERROR] DPO file not found: {dpo_path}")
        return False
    
    # Count lines
    with open(sft_path) as f:
        sft_count = sum(1 for _ in f)
    with open(dpo_path) as f:
        dpo_count = sum(1 for _ in f)
    
    print(f"[INFO] SFT samples: {sft_count}")
    print(f"[INFO] DPO pairs: {dpo_count}")
    
    if sft_count == 0 and dpo_count == 0:
        print("[ERROR] No training data available")
        return False
    
    return True

def train_lora_sft(llama_cpp_path, model_path, sft_path, output_dir, lora_rank=64, epochs=3, lr=1e-4):
    """Train LoRA adapter using llama.cpp on SFT data"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    lora_out = os.path.join(output_dir, f"lora_sft_{timestamp}")
    os.makedirs(lora_out, exist_ok=True)
    
    # llama.cpp LoRA training command
    # Note: llama.cpp uses llama-lora for LoRA training
    cmd = (
        f"{llama_cpp_path}/llama-lora "
        f"-m {model_path} "
        f"-t {os.cpu_count()} "
        f"--lora-out {lora_out} "
        f"--lora-r {lora_rank} "
        f"--epochs {epochs} "
        f"--learning-rate {lr} "
        f"--train-data {sft_path} "
        f"--ctx-size 4096 "
        f"--batch-size 1 "
        f"--save-every 100 "
    )
    
    print(f"[INFO] Starting SFT LoRA training...")
    print(f"[INFO] Output: {lora_out}")
    
    success, stdout, stderr = run_cmd(cmd)
    if success:
        print(f"[SUCCESS] SFT LoRA saved to {lora_out}")
        return lora_out
    else:
        print(f"[ERROR] SFT training failed")
        return None

def train_lora_dpo(llama_cpp_path, model_path, dpo_path, sft_lora_path, output_dir, lora_rank=64, epochs=1, lr=5e-5):
    """Train LoRA adapter using DPO on top of SFT LoRA"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    lora_out = os.path.join(output_dir, f"lora_dpo_{timestamp}")
    os.makedirs(lora_out, exist_ok=True)
    
    # DPO training with llama.cpp (requires SFT LoRA as base)
    cmd = (
        f"{llama_cpp_path}/llama-lora "
        f"-m {model_path} "
        f"-t {os.cpu_count()} "
        f"--lora-out {lora_out} "
        f"--lora-r {lora_rank} "
        f"--epochs {epochs} "
        f"--learning-rate {lr} "
        f"--train-data {dpo_path} "
        f"--dpo "
        f"--lora-base {sft_lora_path} "
        f"--ctx-size 4096 "
        f"--batch-size 1 "
    )
    
    print(f"[INFO] Starting DPO LoRA training...")
    print(f"[INFO] Base SFT LoRA: {sft_lora_path}")
    print(f"[INFO] Output: {lora_out}")
    
    success, stdout, stderr = run_cmd(cmd)
    if success:
        print(f"[SUCCESS] DPO LoRA saved to {lora_out}")
        return lora_out
    else:
        print(f"[ERROR] DPO training failed")
        return None

def merge_lora(llama_cpp_path, model_path, lora_path, output_model):
    """Merge LoRA adapter into base model"""
    cmd = (
        f"{llama_cpp_path}/llama-lora "
        f"-m {model_path} "
        f"--lora {lora_path} "
        f"--lora-out {output_model} "
        f"--merge "
    )
    
    print(f"[INFO] Merging LoRA into base model...")
    print(f"[INFO] Output model: {output_model}")
    
    success, stdout, stderr = run_cmd(cmd)
    if success:
        print(f"[SUCCESS] Merged model saved to {output_model}")
        return True
    else:
        print(f"[ERROR] LoRA merge failed")
        return False

def convert_to_gguf(llama_cpp_path, model_path, output_dir):
    """Convert merged model to GGUF if needed"""
    # For llama.cpp, the model is already GGUF, so this is a no-op
    # But we can quantize if needed
    pass

def main():
    parser = argparse.ArgumentParser(description="Yuki LocalTransformer Fine-Tuning")
    parser.add_argument("--sft", required=True, help="Path to SFT JSONL file")
    parser.add_argument("--dpo", required=True, help="Path to DPO JSONL file")
    parser.add_argument("--model", default="data/models/Qwen3.8-27B-UD-IQ1_S.gguf", help="Base model path")
    parser.add_argument("--output", default="data/models/", help="Output directory")
    parser.add_argument("--llama-cpp", help="Path to llama.cpp build directory")
    parser.add_argument("--lora-rank", type=int, default=64, help="LoRA rank")
    parser.add_argument("--sft-epochs", type=int, default=3, help="SFT epochs")
    parser.add_argument("--dpo-epochs", type=int, default=1, help="DPO epochs")
    parser.add_argument("--sft-lr", type=float, default=1e-4, help="SFT learning rate")
    parser.add_argument("--dpo-lr", type=float, default=5e-5, help="DPO learning rate")
    parser.add_argument("--merge", action="store_true", help="Merge LoRA into base model after training")
    parser.add_argument("--skip-sft", action="store_true", help="Skip SFT, only run DPO (requires --sft-lora)")
    parser.add_argument("--sft-lora", help="Path to existing SFT LoRA for DPO")
    
    args = parser.parse_args()
    
    # Find llama.cpp
    llama_cpp = args.llama_cpp or check_llama_cpp()
    if not llama_cpp:
        print("[ERROR] llama.cpp not found. Set --llama-cpp or LLAMA_CPP_PATH")
        print("        Build llama.cpp from https://github.com/ggerganov/llama.cpp")
        return 1
    
    print(f"[INFO] Using llama.cpp: {llama_cpp}")
    print(f"[INFO] Base model: {args.model}")
    print(f"[INFO] Output dir: {args.output}")
    
    # Prepare data
    if not prepare_training_data(args.sft, args.dpo, args.output):
        return 1
    
    os.makedirs(args.output, exist_ok=True)
    
    sft_lora = args.sft_lora
    
    # SFT Training
    if not args.skip_sft:
        sft_lora = train_lora_sft(
            llama_cpp, args.model, args.sft, args.output,
            lora_rank=args.lora_rank,
            epochs=args.sft_epochs,
            lr=args.sft_lr
        )
        if not sft_lora:
            return 1
    else:
        if not sft_lora:
            print("[ERROR] --skip-sft requires --sft-lora")
            return 1
    
    # DPO Training
    dpo_lora = train_lora_dpo(
        llama_cpp, args.model, args.dpo, sft_lora, args.output,
        lora_rank=args.lora_rank,
        epochs=args.dpo_epochs,
        lr=args.dpo_lr
    )
    if not dpo_lora:
        return 1
    
    # Merge LoRA if requested
    if args.merge:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        merged_model = os.path.join(args.output, f"Qwen3.8-27B-Yuki-{timestamp}.gguf")
        if not merge_lora(llama_cpp, args.model, dpo_lora, merged_model):
            return 1
        print(f"[SUCCESS] Final model: {merged_model}")
    
    print("[SUCCESS] Training complete!")
    print(f"  SFT LoRA: {sft_lora}")
    print(f"  DPO LoRA: {dpo_lora}")
    if args.merge:
        print(f"  Merged: {merged_model}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())