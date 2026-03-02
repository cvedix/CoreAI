#!/usr/bin/env python3
"""
TensorRT Engine Benchmark Script
Uses TensorRT Python API for actual inference benchmarking
"""

import argparse
import os
import sys
import time
import json
from datetime import datetime
from pathlib import Path
import numpy as np

def get_gpu_info():
    """Get GPU information using nvidia-smi"""
    try:
        import subprocess
        result = subprocess.run(
            ['nvidia-smi', '--query-gpu=name,driver_version,memory.total,compute_cap', 
             '--format=csv,noheader,nounits'],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            parts = result.stdout.strip().split(', ')
            return {
                'name': parts[0] if len(parts) > 0 else 'Unknown',
                'driver': parts[1] if len(parts) > 1 else 'Unknown',
                'memory_mb': int(parts[2]) if len(parts) > 2 else 0,
                'compute_cap': parts[3] if len(parts) > 3 else 'Unknown'
            }
    except:
        pass
    return {'name': 'Unknown', 'driver': 'Unknown', 'memory_mb': 0, 'compute_cap': 'Unknown'}

def get_system_info():
    """Get system information"""
    import platform
    cuda_version = 'N/A'
    trt_version = 'N/A'
    
    try:
        import tensorrt as trt
        trt_version = trt.__version__
    except:
        pass
    
    try:
        import subprocess
        result = subprocess.run(['nvcc', '--version'], capture_output=True, text=True)
        if result.returncode == 0:
            for line in result.stdout.split('\n'):
                if 'release' in line.lower():
                    parts = line.split('release')[-1].strip().split(',')[0]
                    cuda_version = parts.strip()
                    break
    except:
        pass
    
    return {
        'os': platform.system(),
        'os_version': platform.release(),
        'python': platform.python_version(),
        'cuda': cuda_version,
        'tensorrt': trt_version
    }

def benchmark_tensorrt_engine(engine_path, iterations=100, warmup=20):
    """Benchmark TensorRT engine with actual inference"""
    import tensorrt as trt
    import pycuda.driver as cuda
    import pycuda.autoinit
    
    engine_path = Path(engine_path)
    engine_size_mb = os.path.getsize(engine_path) / (1024 * 1024)
    
    print(f"\nLoading TensorRT engine: {engine_path.name} ({engine_size_mb:.1f} MB)")
    
    # Load engine
    TRT_LOGGER = trt.Logger(trt.Logger.WARNING)
    with open(engine_path, 'rb') as f:
        engine_data = f.read()
    
    runtime = trt.Runtime(TRT_LOGGER)
    engine = runtime.deserialize_cuda_engine(engine_data)
    
    if engine is None:
        raise RuntimeError("Failed to load TensorRT engine")
    
    context = engine.create_execution_context()
    
    # Get input/output info
    input_names = []
    output_names = []
    input_shapes = []
    output_shapes = []
    
    for i in range(engine.num_io_tensors):
        name = engine.get_tensor_name(i)
        shape = engine.get_tensor_shape(name)
        mode = engine.get_tensor_mode(name)
        
        if mode == trt.TensorIOMode.INPUT:
            input_names.append(name)
            input_shapes.append(tuple(shape))
            print(f"  Input: {name} {list(shape)}")
        else:
            output_names.append(name)
            output_shapes.append(tuple(shape))
            print(f"  Output: {name} {list(shape)}")
    
    # Prepare buffers
    d_inputs = []
    d_outputs = []
    h_inputs = []
    h_outputs = []
    
    for shape in input_shapes:
        h_input = np.random.rand(*shape).astype(np.float32)
        d_input = cuda.mem_alloc(h_input.nbytes)
        cuda.memcpy_htod(d_input, h_input)
        h_inputs.append(h_input)
        d_inputs.append(d_input)
    
    for shape in output_shapes:
        h_output = np.zeros(shape, dtype=np.float32)
        d_output = cuda.mem_alloc(h_output.nbytes)
        h_outputs.append(h_output)
        d_outputs.append(d_output)
    
    stream = cuda.Stream()
    
    # Set tensor addresses
    for name, d_buf in zip(input_names, d_inputs):
        context.set_tensor_address(name, int(d_buf))
    for name, d_buf in zip(output_names, d_outputs):
        context.set_tensor_address(name, int(d_buf))
    
    # Warmup
    print(f"\nWarming up ({warmup} iterations)...")
    for _ in range(warmup):
        context.execute_async_v3(stream_handle=stream.handle)
        stream.synchronize()
    
    # Benchmark
    print(f"Benchmarking ({iterations} iterations)...")
    times = []
    
    for _ in range(iterations):
        start = time.perf_counter()
        context.execute_async_v3(stream_handle=stream.handle)
        stream.synchronize()
        end = time.perf_counter()
        times.append((end - start) * 1000)
    
    # Free buffers
    for d_buf in d_inputs + d_outputs:
        d_buf.free()
    
    # Calculate statistics
    avg_time = sum(times) / len(times)
    min_time = min(times)
    max_time = max(times)
    std_time = (sum((t - avg_time) ** 2 for t in times) / len(times)) ** 0.5
    fps = 1000 / avg_time
    
    sorted_times = sorted(times)
    p50 = sorted_times[int(len(times) * 0.50)]
    p95 = sorted_times[int(len(times) * 0.95)]
    p99 = sorted_times[int(len(times) * 0.99)]
    
    return {
        'model': engine_path.name,
        'model_size_mb': round(engine_size_mb, 2),
        'input_shape': list(input_shapes[0]) if input_shapes else [],
        'output_shape': list(output_shapes[0]) if output_shapes else [],
        'avg_ms': round(avg_time, 3),
        'min_ms': round(min_time, 3),
        'max_ms': round(max_time, 3),
        'std_ms': round(std_time, 3),
        'p50_ms': round(p50, 3),
        'p95_ms': round(p95, 3),
        'p99_ms': round(p99, 3),
        'fps': round(fps, 1),
        'iterations': iterations
    }

def print_results(results):
    """Print single benchmark result"""
    print(f"\n{'-'*60}")
    print(f"  Model:     {results['model']}")
    print(f"  Size:      {results['model_size_mb']} MB")
    print(f"  Input:     {results['input_shape']}")
    print(f"  Avg:       {results['avg_ms']:.2f} ms")
    print(f"  P95:       {results['p95_ms']:.2f} ms")
    print(f"  FPS:       {results['fps']:.1f}")
    print(f"{'-'*60}")

def benchmark_all(engine_dir, iterations=100, warmup=20, output_path=None):
    """Benchmark all engines in a directory"""
    engine_dir = Path(engine_dir)
    engines = sorted(engine_dir.glob("*.engine"))
    
    if not engines:
        print(f"No .engine files found in {engine_dir}")
        return []
    
    gpu_info = get_gpu_info()
    sys_info = get_system_info()
    
    print(f"\n{'='*60}")
    print(f" TensorRT Benchmark Suite")
    print(f"{'='*60}")
    print(f"  GPU:        {gpu_info['name']}")
    print(f"  TensorRT:   {sys_info['tensorrt']}")
    print(f"  Engines:    {len(engines)}")
    print(f"  Iterations: {iterations}")
    print(f"{'='*60}")
    
    all_results = []
    
    for engine_path in engines:
        try:
            results = benchmark_tensorrt_engine(str(engine_path), iterations, warmup)
            print_results(results)
            all_results.append(results)
        except Exception as e:
            print(f"\n✗ Failed to benchmark {engine_path.name}: {e}")
    
    # Print comparison table
    print(f"\n{'='*60}")
    print(f" COMPARISON TABLE")
    print(f"{'='*60}")
    print(f"  {'Model':<40} {'Size':<10} {'Avg':<10} {'FPS':<10}")
    print(f"  {'-'*60}")
    
    for r in all_results:
        name = r['model'].replace('license-plate-finetune-', '').replace('.engine', '')
        print(f"  {name:<40} {r['model_size_mb']:<10.1f} {r['avg_ms']:<10.2f} {r['fps']:<10.1f}")
    
    print(f"{'='*60}")
    
    # Save results
    if output_path:
        report = {
            'timestamp': datetime.now().isoformat(),
            'hardware': gpu_info,
            'software': sys_info,
            'benchmark_config': {'iterations': iterations, 'warmup': warmup},
            'results': all_results
        }
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)
        print(f"\n✓ Report saved to: {output_path}")
    
    return all_results

def main():
    parser = argparse.ArgumentParser(description='Benchmark TensorRT Engines')
    parser.add_argument('path', help='Engine file or directory')
    parser.add_argument('-i', '--iterations', type=int, default=100, help='Iterations')
    parser.add_argument('-w', '--warmup', type=int, default=20, help='Warmup iterations')
    parser.add_argument('-o', '--output', help='Output JSON report path')
    
    args = parser.parse_args()
    path = Path(args.path)
    
    if path.is_dir():
        benchmark_all(path, args.iterations, args.warmup, args.output)
    elif path.is_file():
        results = benchmark_tensorrt_engine(str(path), args.iterations, args.warmup)
        print_results(results)
        if args.output:
            gpu_info = get_gpu_info()
            sys_info = get_system_info()
            report = {
                'timestamp': datetime.now().isoformat(),
                'hardware': gpu_info,
                'software': sys_info,
                'results': [results]
            }
            with open(args.output, 'w') as f:
                json.dump(report, f, indent=2)
            print(f"\n✓ Report saved to: {args.output}")
    else:
        print(f"Error: Path not found: {path}")
        sys.exit(1)

if __name__ == '__main__':
    main()
