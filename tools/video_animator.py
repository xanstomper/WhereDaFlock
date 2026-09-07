#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import numpy as np
from PIL import Image

WIDTH = 240
HEIGHT = 135
MAX_FRAMES = 20

def process_video_to_frames(gif_path):
    frames = []
    img = Image.open(gif_path)
    
    total_frames = 0
    try:
        while True:
            img.seek(total_frames)
            total_frames += 1
    except EOFError:
        pass

    print(f"[*] Found {total_frames} frames in GIF. Sampling {MAX_FRAMES} evenly.")
    sampled_indices = [int(i) for i in np.linspace(0, total_frames - 1, min(MAX_FRAMES, total_frames))]
    
    for count, frame_idx in enumerate(sampled_indices):
        img.seek(frame_idx)
        frame = img.convert("RGB")
        if frame.size != (WIDTH, HEIGHT):
            target_ratio = WIDTH / HEIGHT
            img_ratio = frame.width / frame.height
            if img_ratio > target_ratio:
                new_width = int(frame.height * target_ratio)
                offset = (frame.width - new_width) // 2
                frame = frame.crop((offset, 0, offset + new_width, frame.height))
            else:
                new_height = int(frame.width / target_ratio)
                offset = (frame.height - new_height) // 2
                frame = frame.crop((0, offset, frame.width, offset + new_height))
            frame = frame.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        frames.append(frame)
        print(f"  -> Extracted frame {count+1}/{len(sampled_indices)} (from original frame {frame_idx})")
        
    print(f"[*] Exported {len(frames)} frames. Converting to C++ header...")
    return frames

def generate_c_header(frames, out_path):
    with open(out_path, "w") as f:
        f.write("/*\n * WhereDaFlock - M5StickC Plus Animation Header\n * Target: M5StickC Plus (ST7789 240x135 Landscape)\n */\n\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("namespace WhereDaFlockAnimation {\n\n")
        f.write(f"constexpr uint16_t DISPLAY_WIDTH = {WIDTH};\n")
        f.write(f"constexpr uint16_t DISPLAY_HEIGHT = {HEIGHT};\n")
        f.write(f"constexpr uint16_t ANIM_FRAMES = {len(frames)};\n\n")
        
        f.write(f"const uint16_t anim_frames[][DISPLAY_WIDTH * DISPLAY_HEIGHT] = {{\n")
        
        for idx, frame in enumerate(frames):
            f.write("    {\n        ")
            pixels = frame.load()
            count = 0
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    r, g, b = pixels[x, y]
                    # Swap R and B to fix color mapping on the display
                    rgb565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
                    f.write(f"0x{rgb565:04X}, ")
                    count += 1
                    if count % 16 == 0:
                        f.write("\n        ")
            f.write("    },\n")
            
        f.write("};\n\n} // namespace\n")

def main():
    repo_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    gif_path = "/home/jewboy420/Downloads/red_tui.gif"
    out_header = os.path.join(repo_dir, "firmware", "src", "m5stick_anim.h")
    
    print("[*] Processing video to frames for M5StickC Plus 1.1...")
    frames = process_video_to_frames(gif_path)
    generate_c_header(frames, out_header)
    print(f"[✓] C header successfully generated at: {out_header}")
    
if __name__ == "__main__":
    main()
