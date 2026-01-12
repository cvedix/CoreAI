#!/usr/bin/env python3
"""
ONNX to TensorRT Engine Converter
Converts ONNX models to TensorRT engine format with FP16 precision
"""

import argparse
import os
import sys
import time
from pathlib import Path

def convert_onnx_to_tensorrt(onnx_path, output_path=None, fp16=True, workspace_gb=4):
    """
    Convert ONNX model to TensorRT engine
    
    Args:
        onnx_path: Path to ONNX model
        output_path: Output engine path (default: same as input with .engine extension)
        fp16: Use FP16 precision (default: True)
        workspace_gb: Workspace size in GB
    
    Returns:
        Path to output engine file
    """
    import tensorrt as trt
    import numpy as np
    
    onnx_path = Path(onnx_path)
    if not onnx_path.exists():
        raise FileNotFoundError(f"ONNX file not found: {onnx_path}")
    
    if output_path is None:
        output_path = onnx_path.with_suffix('.engine')
    else:
        output_path = Path(output_path)
    
    onnx_size_mb = os.path.getsize(onnx_path) / (1024 * 1024)
    
    print(f"\n{'='*60}")
    print(f" ONNX to TensorRT Conversion")
    print(f"{'='*60}")
    print(f"Input:      {onnx_path.name} ({onnx_size_mb:.1f} MB)")
    print(f"Output:     {output_path.name}")
    print(f"Precision:  {'FP16' if fp16 else 'FP32'}")
    print(f"Workspace:  {workspace_gb} GB")
    print(f"{'='*60}")
    
    # Create logger
    TRT_LOGGER = trt.Logger(trt.Logger.WARNING)
    
    # Create builder and network
    builder = trt.Builder(TRT_LOGGER)
    network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
    parser = trt.OnnxParser(network, TRT_LOGGER)
    
    # Parse ONNX
    print("\nParsing ONNX model...")
    start_time = time.time()
    
    with open(onnx_path, 'rb') as f:
        if not parser.parse(f.read()):
            for i in range(parser.num_errors):
                print(f"  Error {i}: {parser.get_error(i)}")
            raise RuntimeError("Failed to parse ONNX model")
    
    print(f"  ✓ ONNX parsing completed ({time.time() - start_time:.1f}s)")
    
    # Print network info
    print(f"\nNetwork Info:")
    print(f"  Inputs:  {network.num_inputs}")
    for i in range(network.num_inputs):
        inp = network.get_input(i)
        print(f"    [{i}] {inp.name}: {inp.shape}")
    print(f"  Outputs: {network.num_outputs}")
    for i in range(network.num_outputs):
        out = network.get_output(i)
        print(f"    [{i}] {out.name}: {out.shape}")
    
    # Create builder config
    config = builder.create_builder_config()
    config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, workspace_gb * (1 << 30))
    
    if fp16:
        config.set_flag(trt.BuilderFlag.FP16)
        print(f"\n  ✓ FP16 precision enabled")
    
    # Add optimization profile for dynamic shapes
    profile = builder.create_optimization_profile()
    
    for i in range(network.num_inputs):
        inp = network.get_input(i)
        # Check if input has dynamic dimensions
        shape = inp.shape
        has_dynamic = any(d == -1 for d in shape)
        
        if has_dynamic:
            # Set fixed 640x640 input for YOLO models
            min_shape = (1, 3, 640, 640)
            opt_shape = (1, 3, 640, 640)  
            max_shape = (1, 3, 640, 640)
            
            profile.set_shape(inp.name, min_shape, opt_shape, max_shape)
            print(f"\n  ✓ Set optimization profile for '{inp.name}': {opt_shape}")
    
    config.add_optimization_profile(profile)
    
    # Build engine
    print(f"\nBuilding TensorRT engine...")
    print(f"  This may take a few minutes...")
    start_time = time.time()
    
    serialized_engine = builder.build_serialized_network(network, config)
    
    if serialized_engine is None:
        raise RuntimeError("Failed to build TensorRT engine")
    
    build_time = time.time() - start_time
    
    # Handle TensorRT 10.x IHostMemory object
    if hasattr(serialized_engine, 'nbytes'):
        engine_bytes = bytes(serialized_engine)
        engine_size_mb = len(engine_bytes) / (1024 * 1024)
    else:
        engine_bytes = serialized_engine
        engine_size_mb = len(engine_bytes) / (1024 * 1024)
    
    print(f"  ✓ Engine built in {build_time:.1f}s")
    print(f"  ✓ Engine size: {engine_size_mb:.1f} MB")
    
    # Save engine
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(engine_bytes)
    
    print(f"\n✓ Saved to: {output_path}")
    
    return str(output_path)


def main():
    parser = argparse.ArgumentParser(
        description='Convert ONNX model to TensorRT engine',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('onnx', help='Path to ONNX model file')
    parser.add_argument('-o', '--output', help='Output engine path')
    parser.add_argument('--fp32', action='store_true', help='Use FP32 precision (default: FP16)')
    parser.add_argument('--workspace', type=int, default=4, help='Workspace size in GB')
    
    args = parser.parse_args()
    
    try:
        engine_path = convert_onnx_to_tensorrt(
            args.onnx,
            args.output,
            fp16=not args.fp32,
            workspace_gb=args.workspace
        )
        print(f"\n✓ Conversion successful: {engine_path}")
    except Exception as e:
        print(f"\n✗ Conversion failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
