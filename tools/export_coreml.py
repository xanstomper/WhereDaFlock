#!/usr/bin/env python3
"""
WhereDaFlock - YOLOv8 CoreML Model Exporter & Conversion Pipeline.

Converts a pre-trained or fine-tuned YOLOv8 surveillance detection model
into an Apple CoreML package (.mlpackage) with integrated Non-Maximum
Suppression (NMS) for real-time edge inference on iOS devices.

Usage:
    python3 tools/export_coreml.py [--weights yolov8n.pt] [--imgsz 640] [--output-dir WhereDaFlock/AI]
"""

import argparse
import os
import sys
import shutil

def main():
    parser = argparse.ArgumentParser(description="WhereDaFlock CoreML YOLOv8 Model Exporter")
    parser.add_argument("--weights", default="yolov8n.pt", help="Path or name of YOLO weights (default: yolov8n.pt)")
    parser.add_argument("--imgsz", type=int, default=640, help="Image inference size (default: 640)")
    parser.add_argument("--output-dir", default="WhereDaFlock/AI", help="Target directory for the exported CoreML model")
    args = parser.parse_args()

    print("=== WhereDaFlock CoreML Exporter ===")
    print(f"Target model: {args.weights} (imgsz={args.imgsz})")
    print(f"Output directory: {args.output_dir}")

    try:
        from ultralytics import YOLO
    except ImportError:
        print("\n[!] Error: 'ultralytics' is required. Install via:")
        print("    pip install ultralytics coremltools\n")
        sys.exit(1)

    try:
        import coremltools
    except ImportError:
        print("\n[!] Error: 'coremltools' is required. Install via:")
        print("    pip install coremltools\n")
        sys.exit(1)

    print("\n[1/3] Loading YOLO weights...")
    model = YOLO(args.weights)

    print("[2/3] Exporting to Apple CoreML format with NMS...")
    exported_path = model.export(
        format="coreml",
        nms=True,
        imgsz=args.imgsz,
        int8=False,
        half=True, # 16-bit float for optimal Apple Neural Engine (ANE) performance
    )
    print(f"Exported to: {exported_path}")

    # Destination
    os.makedirs(args.output_dir, exist_ok=True)
    dest_path = os.path.join(args.output_dir, "CameraDetector.mlpackage")

    print(f"[3/3] Installing model into {dest_path}...")
    if os.path.exists(dest_path):
        if os.path.isdir(dest_path):
            shutil.rmtree(dest_path)
        else:
            os.remove(dest_path)

    if os.path.isdir(exported_path):
        shutil.copytree(exported_path, dest_path)
    else:
        shutil.copy2(exported_path, dest_path)

    print(f"\n[✓] Successfully exported and installed: {dest_path}")
    print("    You can now build and run WhereDaFlock in Xcode with live Vision detection enabled!")

if __name__ == "__main__":
    main()
