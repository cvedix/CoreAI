#!/usr/bin/env python3
"""
Export FaceNet PyTorch models to ONNX format for use with cvedix_facenet_node.

This script downloads pretrained FaceNet models from facenet-pytorch and exports them
to ONNX format compatible with OpenCV DNN backend.

Requirements:
    pip install torch torchvision facenet-pytorch onnx

Based on: https://github.com/timesler/facenet-pytorch

Usage:
    python export_facenet_to_onnx.py --dataset vggface2 --output facenet_vggface2.onnx
    python export_facenet_to_onnx.py --dataset casia-webface --output facenet_casia.onnx
"""

import argparse
import os
import sys
import torch
import torch.nn as nn
import onnx
from onnx import shape_inference

try:
    from facenet_pytorch import InceptionResnetV1
except ImportError:
    print("ERROR: facenet-pytorch not installed!")
    print("Install with: pip install facenet-pytorch")
    sys.exit(1)


def export_facenet_to_onnx(
    pretrained: str = 'vggface2',
    output_path: str = 'facenet.onnx',
    input_size: int = 160,
    simplify: bool = True,
    verify: bool = True
):
    """
    Export FaceNet InceptionResnetV1 model to ONNX format.
    
    Args:
        pretrained: Dataset used for pretraining ('vggface2' or 'casia-webface')
        output_path: Path to save ONNX model
        input_size: Input image size (default: 160 for FaceNet)
        simplify: Whether to simplify the ONNX model (requires onnx-simplifier)
        verify: Whether to verify the exported model
    """
    
    print(f"\n{'='*70}")
    print(f"Exporting FaceNet to ONNX")
    print(f"{'='*70}")
    print(f"Pretrained dataset: {pretrained}")
    print(f"Output path: {output_path}")
    print(f"Input size: {input_size}x{input_size}")
    print(f"{'='*70}\n")
    
    # Validate pretrained dataset
    if pretrained not in ['vggface2', 'casia-webface']:
        raise ValueError(f"Invalid pretrained dataset: {pretrained}. "
                        f"Must be 'vggface2' or 'casia-webface'")
    
    # Load pretrained model
    print("Loading pretrained FaceNet model...")
    model = InceptionResnetV1(pretrained=pretrained)
    model.eval()
    print(f"✓ Model loaded successfully (pretrained on {pretrained})")
    
    # Create dummy input
    dummy_input = torch.randn(1, 3, input_size, input_size)
    print(f"✓ Created dummy input: {dummy_input.shape}")
    
    # Test forward pass
    print("Testing forward pass...")
    with torch.no_grad():
        output = model(dummy_input)
    print(f"✓ Forward pass successful, output shape: {output.shape}")
    
    # Export to ONNX
    print(f"\nExporting to ONNX format...")
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=11,  # OpenCV DNN supports opset 11
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={
            'input': {0: 'batch_size'},
            'output': {0: 'batch_size'}
        }
    )
    print(f"✓ Model exported to {output_path}")
    
    # Verify ONNX model
    if verify:
        print("\nVerifying ONNX model...")
        try:
            onnx_model = onnx.load(output_path)
            onnx.checker.check_model(onnx_model)
            print("✓ ONNX model is valid")
            
            # Print model info
            print("\nModel Information:")
            print(f"  - Opset version: {onnx_model.opset_import[0].version}")
            print(f"  - Input shape: {onnx_model.graph.input[0].type.tensor_type.shape}")
            print(f"  - Output shape: {onnx_model.graph.output[0].type.tensor_type.shape}")
            
            # Infer shapes
            onnx_model = shape_inference.infer_shapes(onnx_model)
            onnx.save(onnx_model, output_path)
            print("✓ Shape inference completed")
            
        except Exception as e:
            print(f"✗ ONNX verification failed: {e}")
            return False
    
    # Simplify ONNX model (optional, requires onnxsim)
    if simplify:
        try:
            import onnxsim
            print("\nSimplifying ONNX model...")
            onnx_model = onnx.load(output_path)
            model_simp, check = onnxsim.simplify(onnx_model)
            if check:
                onnx.save(model_simp, output_path)
                print("✓ ONNX model simplified")
            else:
                print("✗ Simplification failed, keeping original model")
        except ImportError:
            print("ℹ onnx-simplifier not installed, skipping simplification")
            print("  Install with: pip install onnx-simplifier")
        except Exception as e:
            print(f"✗ Simplification failed: {e}")
    
    # Test with OpenCV DNN (if available)
    try:
        import cv2
        print("\nTesting with OpenCV DNN...")
        net = cv2.dnn.readNetFromONNX(output_path)
        
        # Test inference
        blob = cv2.dnn.blobFromImage(
            dummy_input.numpy().squeeze().transpose(1, 2, 0),
            scalefactor=1.0/255.0,
            size=(input_size, input_size),
            mean=(0.5, 0.5, 0.5),
            swapRB=False,
            crop=False
        )
        blob = blob / 0.5  # Apply std normalization
        
        net.setInput(blob)
        cv_output = net.forward()
        
        print(f"✓ OpenCV DNN inference successful")
        print(f"  - Input shape: {blob.shape}")
        print(f"  - Output shape: {cv_output.shape}")
        print(f"  - Output range: [{cv_output.min():.3f}, {cv_output.max():.3f}]")
        
        # Compare PyTorch and OpenCV outputs
        with torch.no_grad():
            torch_output = model(dummy_input).numpy()
        
        diff = abs(torch_output - cv_output).max()
        print(f"  - Max difference PyTorch vs OpenCV: {diff:.6f}")
        
        if diff < 0.001:
            print("✓ PyTorch and OpenCV outputs match!")
        else:
            print(f"⚠ Warning: Large difference between PyTorch and OpenCV outputs")
            
    except ImportError:
        print("ℹ OpenCV not installed, skipping OpenCV DNN test")
    except Exception as e:
        print(f"✗ OpenCV DNN test failed: {e}")
    
    print(f"\n{'='*70}")
    print(f"Export completed successfully!")
    print(f"{'='*70}")
    print(f"\nYou can now use this model with cvedix_facenet_node:")
    print(f"  auto facenet = std::make_shared<cvedix_nodes::cvedix_facenet_node>(")
    print(f"      \"facenet\", \"{output_path}\", 160, 160, true, \"{pretrained}\");")
    print(f"\n{'='*70}\n")
    
    return True


def export_mtcnn_to_onnx(output_dir: str = '.'):
    """
    Export MTCNN models to ONNX format.
    
    MTCNN consists of three networks: P-Net, R-Net, and O-Net.
    This function exports all three to separate ONNX files.
    
    Args:
        output_dir: Directory to save ONNX models
    """
    
    print(f"\n{'='*70}")
    print(f"Exporting MTCNN to ONNX")
    print(f"{'='*70}\n")
    
    try:
        from facenet_pytorch import MTCNN
    except ImportError:
        print("ERROR: facenet-pytorch not installed!")
        print("Install with: pip install facenet-pytorch")
        return False
    
    # Create MTCNN instance
    print("Loading MTCNN models...")
    mtcnn = MTCNN(keep_all=True)
    
    # Export P-Net
    print("\nExporting P-Net...")
    pnet_path = os.path.join(output_dir, 'mtcnn_pnet.onnx')
    dummy_pnet = torch.randn(1, 3, 12, 12)
    torch.onnx.export(
        mtcnn.pnet,
        dummy_pnet,
        pnet_path,
        opset_version=11,
        input_names=['input'],
        output_names=['prob', 'box']
    )
    print(f"✓ P-Net exported to {pnet_path}")
    
    # Export R-Net
    print("\nExporting R-Net...")
    rnet_path = os.path.join(output_dir, 'mtcnn_rnet.onnx')
    dummy_rnet = torch.randn(1, 3, 24, 24)
    torch.onnx.export(
        mtcnn.rnet,
        dummy_rnet,
        rnet_path,
        opset_version=11,
        input_names=['input'],
        output_names=['prob', 'box']
    )
    print(f"✓ R-Net exported to {rnet_path}")
    
    # Export O-Net
    print("\nExporting O-Net...")
    onet_path = os.path.join(output_dir, 'mtcnn_onet.onnx')
    dummy_onet = torch.randn(1, 3, 48, 48)
    torch.onnx.export(
        mtcnn.onet,
        dummy_onet,
        onet_path,
        opset_version=11,
        input_names=['input'],
        output_names=['prob', 'box', 'landmarks']
    )
    print(f"✓ O-Net exported to {onet_path}")
    
    print(f"\n{'='*70}")
    print(f"MTCNN export completed!")
    print(f"{'='*70}\n")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Export FaceNet PyTorch models to ONNX format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Export VGGFace2 model
  python export_facenet_to_onnx.py --dataset vggface2 --output facenet_vggface2.onnx
  
  # Export CASIA-Webface model
  python export_facenet_to_onnx.py --dataset casia-webface --output facenet_casia.onnx
  
  # Export MTCNN detector
  python export_facenet_to_onnx.py --mtcnn --output-dir ./models/
        """
    )
    
    parser.add_argument(
        '--dataset',
        type=str,
        default='vggface2',
        choices=['vggface2', 'casia-webface'],
        help='Pretrained dataset (default: vggface2)'
    )
    
    parser.add_argument(
        '--output',
        type=str,
        default='facenet.onnx',
        help='Output ONNX file path (default: facenet.onnx)'
    )
    
    parser.add_argument(
        '--input-size',
        type=int,
        default=160,
        help='Input image size (default: 160)'
    )
    
    parser.add_argument(
        '--no-simplify',
        action='store_true',
        help='Skip ONNX model simplification'
    )
    
    parser.add_argument(
        '--no-verify',
        action='store_true',
        help='Skip ONNX model verification'
    )
    
    parser.add_argument(
        '--mtcnn',
        action='store_true',
        help='Export MTCNN models instead of FaceNet'
    )
    
    parser.add_argument(
        '--output-dir',
        type=str,
        default='.',
        help='Output directory for MTCNN models (default: current directory)'
    )
    
    args = parser.parse_args()
    
    try:
        if args.mtcnn:
            success = export_mtcnn_to_onnx(args.output_dir)
        else:
            success = export_facenet_to_onnx(
                pretrained=args.dataset,
                output_path=args.output,
                input_size=args.input_size,
                simplify=not args.no_simplify,
                verify=not args.no_verify
            )
        
        sys.exit(0 if success else 1)
        
    except Exception as e:
        print(f"\n✗ Export failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()



