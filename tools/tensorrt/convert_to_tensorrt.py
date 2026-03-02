#!/usr/bin/env python3
"""
TensorRT Model Conversion Script
Convert PyTorch YOLOv11 models to TensorRT engine format
Supports:
- TensorRT 10.x
- CUDA 12.x
- FP16/FP32 precision
"""

import argparse
import os
import sys
from pathlib import Path

def check_dependencies():
    """Check if required packages are installed."""
    try:
        from ultralytics import YOLO
        import torch
        print(f"✓ ultralytics installed")
        print(f"✓ torch {torch.__version__}")
        print(f"✓ CUDA available: {torch.cuda.is_available()}")
        if torch.cuda.is_available():
            print(f"  GPU: {torch.cuda.get_device_name(0)}")
        return True
    except ImportError as e:
        print(f"✗ Missing dependency: {e}")
        print("Install with: pip install ultralytics")
        return False

def convert_to_tensorrt(
    model_path: str,
    output_dir: str = None,
    imgsz: int = 640,
    half: bool = True,
    device: int = 0,
    batch: int = 1,
    workspace: int = 4,
    verbose: bool = True
):
    """
    Convert PyTorch model to TensorRT engine.
    
    Args:
        model_path: Path to .pt model file
        output_dir: Output directory (default: same as input)
        imgsz: Input image size
        half: Use FP16 precision
        device: CUDA device ID
        batch: Batch size
        workspace: Workspace size in GB
        verbose: Print detailed output
    
    Returns:
        Path to exported engine file
    """
    from ultralytics import YOLO
    import torch
    
    model_path = Path(model_path)
    if not model_path.exists():
        raise FileNotFoundError(f"Model not found: {model_path}")
    
    if output_dir:
        output_path = Path(output_dir) / model_path.with_suffix('.engine').name
    else:
        output_path = model_path.with_suffix('.engine')
    
    print("="*60)
    print(" TensorRT Model Conversion")
    print("="*60)
    print(f"Input model:  {model_path}")
    print(f"Output:       {output_path}")
    print(f"Image size:   {imgsz}x{imgsz}")
    print(f"Precision:    {'FP16' if half else 'FP32'}")
    print(f"Device:       cuda:{device} ({torch.cuda.get_device_name(device)})")
    print(f"Batch size:   {batch}")
    print("="*60)
    
    # Load and export model
    print("\nLoading model...")
    model = YOLO(str(model_path))
    
    print("Exporting to TensorRT...")
    export_path = model.export(
        format='engine',
        imgsz=imgsz,
        half=half,
        device=device,
        batch=batch,
        workspace=workspace,
        verbose=verbose
    )
    
    # Move to output directory if specified
    if output_dir and export_path != str(output_path):
        import shutil
        os.makedirs(output_dir, exist_ok=True)
        shutil.move(export_path, str(output_path))
        export_path = str(output_path)
    
    print("\n" + "="*60)
    print(f"✓ Export successful!")
    print(f"  Engine file: {export_path}")
    print(f"  Size: {os.path.getsize(export_path) / 1024 / 1024:.1f} MB")
    print("="*60)
    
    return export_path


def main():
    parser = argparse.ArgumentParser(
        description="Convert YOLOv11 PyTorch model to TensorRT engine",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic conversion with FP16
  python convert_to_tensorrt.py model.pt
  
  # Specify output directory and image size
  python convert_to_tensorrt.py model.pt -o ./engines/ --imgsz 640
  
  # Use FP32 precision
  python convert_to_tensorrt.py model.pt --no-half
        """
    )
    
    parser.add_argument('model', help='Path to PyTorch model (.pt)')
    parser.add_argument('-o', '--output', help='Output directory')
    parser.add_argument('--imgsz', type=int, default=640, help='Input image size')
    parser.add_argument('--half', action='store_true', default=True, help='Use FP16 (default)')
    parser.add_argument('--no-half', action='store_true', help='Use FP32')
    parser.add_argument('--device', type=int, default=0, help='CUDA device ID')
    parser.add_argument('--batch', type=int, default=1, help='Batch size')
    parser.add_argument('--workspace', type=int, default=4, help='Workspace size in GB')
    parser.add_argument('--check', action='store_true', help='Check dependencies only')
    
    args = parser.parse_args()
    
    if args.check:
        sys.exit(0 if check_dependencies() else 1)
    
    if not check_dependencies():
        sys.exit(1)
    
    half = not args.no_half
    
    try:
        convert_to_tensorrt(
            model_path=args.model,
            output_dir=args.output,
            imgsz=args.imgsz,
            half=half,
            device=args.device,
            batch=args.batch,
            workspace=args.workspace
        )
    except Exception as e:
        print(f"✗ Conversion failed: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
