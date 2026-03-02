#!/usr/bin/env python3
"""
TensorRT Model Benchmark Script
Benchmark YOLOv11 models with PyTorch and TensorRT backends
Supports image and video input
"""

import argparse
import os
import sys
import time
from pathlib import Path
from typing import Optional


def benchmark_model(
    model_path: str,
    source: str = None,
    iterations: int = 100,
    warmup: int = 10,
    save: bool = False,
    output_dir: str = None,
    verbose: bool = True
):
    """
    Benchmark a model on images or video.
    
    Args:
        model_path: Path to model file (.pt, .engine, .onnx)
        source: Input source (image/video path, default: random tensor)
        iterations: Number of benchmark iterations
        warmup: Number of warmup iterations
        save: Save output video/images
        output_dir: Output directory for saved results
        verbose: Print detailed output
    
    Returns:
        Dictionary with benchmark results
    """
    from ultralytics import YOLO
    import torch
    
    model_path = Path(model_path)
    model_ext = model_path.suffix.lower()
    
    # Determine model type
    if model_ext == '.pt':
        model_type = "PyTorch"
        precision = "FP32"
    elif model_ext == '.engine':
        model_type = "TensorRT"
        precision = "FP16"
    elif model_ext == '.onnx':
        model_type = "ONNX"
        precision = "FP32"
    else:
        model_type = "Unknown"
        precision = "Unknown"
    
    print("\n" + "="*70)
    print(" MODEL BENCHMARK")
    print("="*70)
    print(f"Model:       {model_path.name}")
    print(f"Type:        {model_type} ({precision})")
    print(f"GPU:         {torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'CPU'}")
    print(f"Source:      {source if source else 'Random tensor'}")
    print(f"Iterations:  {iterations}")
    print(f"Warmup:      {warmup}")
    print("="*70)
    
    # Load model
    print("\nLoading model...")
    task = 'detect'  # Default task for YOLOv11
    model = YOLO(str(model_path), task=task)
    
    # Determine source
    if source:
        source_path = Path(source)
        if not source_path.exists():
            raise FileNotFoundError(f"Source not found: {source}")
        is_video = source_path.suffix.lower() in ['.mp4', '.avi', '.mov', '.mkv', '.webm']
    else:
        import numpy as np
        source = np.random.randint(0, 255, (640, 640, 3), dtype=np.uint8)
        is_video = False
    
    # Warmup
    print(f"\nWarming up ({warmup} iterations)...")
    for i in range(warmup):
        model.predict(source=source, verbose=False, save=False)
    
    # Benchmark
    print(f"Benchmarking ({iterations} iterations)...")
    times = []
    frame_count = 0
    
    for i in range(iterations):
        start = time.perf_counter()
        results = model.predict(source=source, verbose=False, save=False)
        end = time.perf_counter()
        times.append((end - start) * 1000)
        
        if is_video:
            frame_count = len(results) if hasattr(results, '__len__') else 1
    
    # Calculate statistics
    avg_time = sum(times) / len(times)
    min_time = min(times)
    max_time = max(times)
    std_time = (sum((t - avg_time) ** 2 for t in times) / len(times)) ** 0.5
    fps = 1000 / avg_time
    
    # Percentiles
    sorted_times = sorted(times)
    p50 = sorted_times[int(len(times) * 0.50)]
    p95 = sorted_times[int(len(times) * 0.95)]
    p99 = sorted_times[int(len(times) * 0.99)]
    
    results = {
        "model": model_path.name,
        "model_type": model_type,
        "precision": precision,
        "gpu": torch.cuda.get_device_name(0) if torch.cuda.is_available() else "CPU",
        "iterations": iterations,
        "avg_ms": avg_time,
        "min_ms": min_time,
        "max_ms": max_time,
        "std_ms": std_time,
        "p50_ms": p50,
        "p95_ms": p95,
        "p99_ms": p99,
        "fps": fps
    }
    
    # Print results
    print("\n" + "-"*70)
    print(" RESULTS")
    print("-"*70)
    print(f"{'Metric':<25} {'Value':<20}")
    print("-"*70)
    print(f"{'Average time:':<25} {avg_time:.2f} ms")
    print(f"{'Min time:':<25} {min_time:.2f} ms")
    print(f"{'Max time:':<25} {max_time:.2f} ms")
    print(f"{'Std deviation:':<25} {std_time:.2f} ms")
    print(f"{'P50 (median):':<25} {p50:.2f} ms")
    print(f"{'P95:':<25} {p95:.2f} ms")
    print(f"{'P99:':<25} {p99:.2f} ms")
    print("-"*70)
    print(f"{'Throughput:':<25} {fps:.1f} FPS")
    print("="*70)
    
    # Save video output if requested
    if save and source:
        save_dir = output_dir or "./results"
        os.makedirs(save_dir, exist_ok=True)
        print(f"\nSaving output to {save_dir}...")
        model.predict(source=source, save=True, project=save_dir, name="benchmark")
        print(f"✓ Output saved to {save_dir}/benchmark/")
    
    return results


def compare_models(
    pytorch_model: str,
    tensorrt_model: str,
    source: str = None,
    iterations: int = 50,
    warmup: int = 10
):
    """Compare PyTorch and TensorRT model performance."""
    
    print("\n" + "="*70)
    print(" MODEL COMPARISON: PyTorch vs TensorRT")
    print("="*70)
    
    # Benchmark PyTorch
    print("\n[1/2] Benchmarking PyTorch model...")
    pt_results = benchmark_model(pytorch_model, source, iterations, warmup)
    
    # Benchmark TensorRT
    print("\n[2/2] Benchmarking TensorRT model...")
    trt_results = benchmark_model(tensorrt_model, source, iterations, warmup)
    
    # Comparison
    speedup = pt_results["avg_ms"] / trt_results["avg_ms"]
    
    print("\n" + "="*70)
    print(" COMPARISON SUMMARY")
    print("="*70)
    print(f"{'Model':<30} {'Avg Time (ms)':<15} {'FPS':<10}")
    print("-"*70)
    print(f"{'PyTorch FP32':<30} {pt_results['avg_ms']:<15.2f} {pt_results['fps']:<10.1f}")
    print(f"{'TensorRT FP16':<30} {trt_results['avg_ms']:<15.2f} {trt_results['fps']:<10.1f}")
    print("-"*70)
    print(f"{'Speedup:':<30} {speedup:.2f}x faster with TensorRT")
    print("="*70)
    
    return {"pytorch": pt_results, "tensorrt": trt_results, "speedup": speedup}


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark YOLOv11 models (PyTorch/TensorRT/ONNX)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Benchmark single model with image
  python benchmark.py model.engine --source image.jpg
  
  # Benchmark with video and save output
  python benchmark.py model.engine --source video.mp4 --save
  
  # Compare PyTorch vs TensorRT
  python benchmark.py model.pt --compare model.engine --source image.jpg
  
  # Run 200 iterations
  python benchmark.py model.engine --iterations 200
        """
    )
    
    parser.add_argument('model', help='Path to model file (.pt, .engine, .onnx)')
    parser.add_argument('--source', '-s', help='Input source (image/video path)')
    parser.add_argument('--compare', '-c', help='Second model to compare against')
    parser.add_argument('--iterations', '-i', type=int, default=100, help='Benchmark iterations')
    parser.add_argument('--warmup', '-w', type=int, default=10, help='Warmup iterations')
    parser.add_argument('--save', action='store_true', help='Save output video/images')
    parser.add_argument('--output', '-o', help='Output directory for saved results')
    
    args = parser.parse_args()
    
    try:
        if args.compare:
            compare_models(
                args.model,
                args.compare,
                source=args.source,
                iterations=args.iterations,
                warmup=args.warmup
            )
        else:
            benchmark_model(
                args.model,
                source=args.source,
                iterations=args.iterations,
                warmup=args.warmup,
                save=args.save,
                output_dir=args.output
            )
    except Exception as e:
        print(f"✗ Benchmark failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
