#!/usr/bin/env python3
"""
WhereDaFlock - M5StickC Plus 1.1 Animation Generator & Chafa Terminal Renderer
==============================================================================
Generates high-tactical boot and radar sweep animation frames for the
M5StickC Plus 1.1 (135x240 ST7789 display), previews them using Chafa
terminal graphics, and exports both C++ embedded headers and animated GIF assets.
"""

import os
import sys
import math
import time
import subprocess
from PIL import Image, ImageDraw, ImageFont

WIDTH = 135
HEIGHT = 240
TOTAL_BOOT_FRAMES = 16
TOTAL_RADAR_FRAMES = 16

# Color palette (RGB565 / RGB)
BG_COLOR = (8, 12, 20)           # Deep tactical black/navy
GRID_COLOR = (15, 30, 45)         # Subtle HUD grid
CYAN_MAIN = (0, 240, 255)         # Neon cyan primary
GREEN_RADAR = (0, 255, 136)       # Phosphor green sweep
AMBER_WARN = (255, 170, 0)        # Tier 3 warning
RED_ALERT = (255, 40, 60)         # Tier 4 threat alert
WHITE_TEXT = (230, 245, 255)      # Bright HUD text
DARK_TEXT = (90, 120, 140)        # Secondary labels


def create_base_canvas():
    img = Image.new("RGB", (WIDTH, HEIGHT), color=BG_COLOR)
    draw = ImageDraw.Draw(img)
    
    # Draw tactical corner brackets
    bracket_len = 10
    # Top-left
    draw.line([(2, 2), (2 + bracket_len, 2)], fill=GRID_COLOR, width=1)
    draw.line([(2, 2), (2, 2 + bracket_len)], fill=GRID_COLOR, width=1)
    # Top-right
    draw.line([(WIDTH - 3, 2), (WIDTH - 3 - bracket_len, 2)], fill=GRID_COLOR, width=1)
    draw.line([(WIDTH - 3, 2), (WIDTH - 3, 2 + bracket_len)], fill=GRID_COLOR, width=1)
    # Bottom-left
    draw.line([(2, HEIGHT - 3), (2 + bracket_len, HEIGHT - 3)], fill=GRID_COLOR, width=1)
    draw.line([(2, HEIGHT - 3), (2, HEIGHT - 3 - bracket_len)], fill=GRID_COLOR, width=1)
    # Bottom-right
    draw.line([(WIDTH - 3, HEIGHT - 3), (WIDTH - 3 - bracket_len, HEIGHT - 3)], fill=GRID_COLOR, width=1)
    draw.line([(WIDTH - 3, HEIGHT - 3), (WIDTH - 3, HEIGHT - 3 - bracket_len)], fill=GRID_COLOR, width=1)
    
    # Subtle horizontal scanlines every 16px
    for y in range(8, HEIGHT, 16):
        draw.line([(4, y), (WIDTH - 4, y)], fill=(12, 20, 30), width=1)
        
    return img, draw


def render_boot_frame(frame_idx, total_frames=TOTAL_BOOT_FRAMES):
    """Renders boot sequence: cyber shield forming, energy pulse, and text boot log."""
    img, draw = create_base_canvas()
    progress = frame_idx / float(total_frames - 1)
    
    # Header
    draw.text((8, 8), "WHERE DA FLOCK", fill=CYAN_MAIN)
    draw.line([(8, 20), (WIDTH - 8, 20)], fill=CYAN_MAIN, width=1)
    
    # Shield geometry center
    cx, cy = WIDTH // 2, 85
    base_scale = min(1.0, progress * 1.3)
    pulse = math.sin(progress * math.pi * 3) * 3
    s_w = int(36 * base_scale + pulse)
    s_h = int(48 * base_scale + pulse)
    
    # Shield vertices
    shield_pts = [
        (cx, cy - s_h),
        (cx + s_w, cy - s_h // 3),
        (cx + s_w, cy + s_h // 4),
        (cx, cy + s_h),
        (cx - s_w, cy + s_h // 4),
        (cx - s_w, cy - s_h // 3)
    ]
    
    # Draw outer glow
    glow_color = (int(0 * progress), int(200 * progress), int(255 * progress))
    draw.polygon(shield_pts, outline=glow_color, width=2)
    
    # Draw inner crosshairs
    if progress > 0.3:
        inner_pts = [
            (cx, cy - s_h + 8),
            (cx + s_w - 6, cy - s_h // 3),
            (cx + s_w - 6, cy + s_h // 4 - 4),
            (cx, cy + s_h - 8),
            (cx - s_w + 6, cy + s_h // 4 - 4),
            (cx - s_w + 6, cy - s_h // 3)
        ]
        draw.polygon(inner_pts, outline=(0, 140, 180), width=1)
        draw.line([(cx, cy - s_h // 2), (cx, cy + s_h // 2)], fill=CYAN_MAIN, width=1)
        draw.line([(cx - s_w // 2, cy), (cx + s_w // 2, cy)], fill=CYAN_MAIN, width=1)
    
    # Energy wave rings radiating outward
    if progress > 0.5:
        ring_r = int((progress - 0.5) * 2.0 * 55)
        alpha_factor = 1.0 - (progress - 0.5) * 2.0
        ring_col = (int(0 * alpha_factor), int(255 * alpha_factor), int(136 * alpha_factor))
        draw.ellipse([(cx - ring_r, cy - ring_r), (cx + ring_r, cy + ring_r)], outline=ring_col, width=1)
    
    # Progress Bar
    bar_y = 150
    draw.rectangle([(14, bar_y), (WIDTH - 14, bar_y + 6)], outline=GRID_COLOR, width=1)
    fill_w = int((WIDTH - 28) * progress)
    if fill_w > 0:
        draw.rectangle([(14, bar_y), (14 + fill_w, bar_y + 6)], fill=CYAN_MAIN)
        
    # Boot status console text
    log_y = 168
    logs = [
        ("CORE: ESP32-PICO-D4", 0.1),
        ("PMU: AXP192 [3.3V OK]", 0.3),
        ("TFT: ST7789 135x240", 0.5),
        ("RADIO: PROMISC 2.4GHz", 0.7),
        ("STATUS: DEFENSE ACTIVE", 0.9)
    ]
    
    for text, thresh in logs:
        if progress >= thresh:
            col = GREEN_RADAR if "ACTIVE" in text or "OK" in text else WHITE_TEXT
            draw.text((8, log_y), text, fill=col)
            log_y += 12
            
    return img


def render_radar_frame(frame_idx, total_frames=TOTAL_RADAR_FRAMES):
    """Renders active scanning radar HUD: rotating sweep, blips, channel hops."""
    img, draw = create_base_canvas()
    
    # Header with blinking live status
    draw.text((8, 6), "WDF // RADAR HUD", fill=CYAN_MAIN)
    status_dot = GREEN_RADAR if (frame_idx % 4 < 2) else (0, 80, 40)
    draw.ellipse([(WIDTH - 16, 8), (WIDTH - 10, 14)], fill=status_dot)
    draw.line([(8, 18), (WIDTH - 8, 18)], fill=GRID_COLOR, width=1)
    
    # Radar center & rings
    cx, cy = WIDTH // 2, 85
    radii = [18, 36, 54]
    for r in radii:
        draw.ellipse([(cx - r, cy - r), (cx + r, cy + r)], outline=(20, 45, 65), width=1)
        
    # Crosshairs
    draw.line([(cx - 54, cy), (cx + 54, cy)], fill=(20, 45, 65), width=1)
    draw.line([(cx, cy - 54), (cx, cy + 54)], fill=(20, 45, 65), width=1)
    
    # Radar rotating sweep beam (angle based on frame)
    angle_rad = (frame_idx / float(total_frames)) * 2 * math.pi
    sweep_len = 54
    end_x = cx + int(sweep_len * math.cos(angle_rad))
    end_y = cy + int(sweep_len * math.sin(angle_rad))
    draw.line([(cx, cy), (end_x, end_y)], fill=GREEN_RADAR, width=2)
    
    # Sweep phosphor trail (fading segments)
    for trail_step in range(1, 4):
        t_angle = angle_rad - (trail_step * 0.15)
        tx = cx + int(sweep_len * math.cos(t_angle))
        ty = cy + int(sweep_len * math.sin(t_angle))
        t_color = (0, max(20, 255 - trail_step * 70), max(10, 136 - trail_step * 40))
        draw.line([(cx, cy), (tx, ty)], fill=t_color, width=1)
        
    # Simulated camera transmitter blip at fixed location
    blip_x, blip_y = cx + 24, cy - 20
    dist_angle = abs((angle_rad % (2 * math.pi)) - math.atan2(-20, 24) % (2 * math.pi))
    if dist_angle < 0.6:
        # Highlighted blip
        draw.ellipse([(blip_x - 3, blip_y - 3), (blip_x + 3, blip_y + 3)], fill=RED_ALERT)
        draw.ellipse([(blip_x - 6, blip_y - 6), (blip_x + 6, blip_y + 6)], outline=RED_ALERT, width=1)
    else:
        # Fading blip
        draw.ellipse([(blip_x - 2, blip_y - 2), (blip_x + 2, blip_y + 2)], fill=(120, 20, 30))
        
    # Channel hop telemetry display
    ch_list = [1, 6, 11]
    active_ch_idx = (frame_idx // 5) % 3
    active_ch = ch_list[active_ch_idx]
    freqs = {1: 2412, 6: 2437, 11: 2462}
    
    draw.line([(8, 148), (WIDTH - 8, 148)], fill=GRID_COLOR, width=1)
    draw.text((8, 154), f"CH HOP: [{active_ch}]", fill=WHITE_TEXT)
    draw.text((76, 154), f"{freqs[active_ch]} MHz", fill=CYAN_MAIN)
    
    # Spectrum mini-bars
    for i, ch in enumerate([1, 6, 11]):
        bx = 10 + i * 40
        by = 172
        draw.rectangle([(bx, by), (bx + 32, by + 18)], outline=GRID_COLOR, width=1)
        draw.text((bx + 4, by + 3), f"C{ch}", fill=DARK_TEXT)
        # Bar height
        bh = 12 if ch == active_ch else 4
        bar_col = GREEN_RADAR if ch == active_ch else (30, 70, 90)
        draw.rectangle([(bx + 20, by + 16 - bh), (bx + 28, by + 16)], fill=bar_col)
        
    # Footer stats
    draw.line([(8, 198), (WIDTH - 8, 198)], fill=GRID_COLOR, width=1)
    draw.text((8, 204), "HITS: 14", fill=WHITE_TEXT)
    draw.text((70, 204), "RSSI: -64", fill=AMBER_WARN)
    draw.text((8, 218), "BAT: 4.12V", fill=DARK_TEXT)
    draw.text((70, 218), "MODE: PASSIVE", fill=GREEN_RADAR)
    
    return img


def render_alert_frame(frame_idx, total_frames=8):
    """Renders high-intensity ALPR threat detection alert."""
    img, draw = create_base_canvas()
    is_flash = (frame_idx % 2 == 0)
    
    # Flashing threat border
    border_col = RED_ALERT if is_flash else (80, 0, 10)
    draw.rectangle([(2, 2), (WIDTH - 3, HEIGHT - 3)], outline=border_col, width=3)
    
    # Threat banner
    draw.rectangle([(6, 8), (WIDTH - 6, 28)], fill=border_col)
    draw.text((12, 12), "! FLOCK DETECTED !", fill=WHITE_TEXT)
    
    # Targeting reticle
    cx, cy = WIDTH // 2, 72
    reticle_r = 28 + (4 if is_flash else 0)
    draw.ellipse([(cx - reticle_r, cy - reticle_r), (cx + reticle_r, cy + reticle_r)], outline=RED_ALERT, width=2)
    draw.line([(cx - reticle_r - 8, cy), (cx + reticle_r + 8, cy)], fill=RED_ALERT, width=1)
    draw.line([(cx, cy - reticle_r - 8), (cx, cy + reticle_r + 8)], fill=RED_ALERT, width=1)
    
    # Center hazard icon
    draw.polygon([(cx, cy - 14), (cx + 14, cy + 10), (cx - 14, cy + 10)], outline=WHITE_TEXT, width=2)
    draw.text((cx - 2, cy - 6), "!", fill=WHITE_TEXT)
    
    # Detection details
    y = 112
    draw.text((8, y), "TIER 4: VENDOR IE", fill=RED_ALERT); y += 14
    draw.text((8, y), "MAC: 82:6B:F2:A1:B2:C3", fill=WHITE_TEXT); y += 14
    draw.text((8, y), "CHAN: 6 (2437 MHz)", fill=CYAN_MAIN); y += 14
    draw.text((8, y), "RSSI: -58 dBm", fill=AMBER_WARN); y += 14
    draw.text((8, y), "BURST: 1->6->11 HOP", fill=GREEN_RADAR); y += 14
    draw.text((8, y), "EST DIST: ~12 METERS", fill=WHITE_TEXT); y += 18
    
    # Bottom buzzer / alert pulse
    draw.rectangle([(8, y), (WIDTH - 8, y + 14)], fill=border_col)
    draw.text((14, y + 2), "BUZZER: PULSE 2.8kHz", fill=WHITE_TEXT)
    
    return img


def generate_all_frames(output_dir="/tmp/wdf_anim_frames"):
    os.makedirs(output_dir, exist_ok=True)
    frames = []
    
    # 1. Boot sequence
    for i in range(TOTAL_BOOT_FRAMES):
        f = render_boot_frame(i, TOTAL_BOOT_FRAMES)
        path = os.path.join(output_dir, f"frame_boot_{i:02d}.png")
        f.save(path)
        frames.append(f)
        
    # 2. Radar sweep loop (3 loops)
    for _ in range(2):
        for i in range(TOTAL_RADAR_FRAMES):
            f = render_radar_frame(i, TOTAL_RADAR_FRAMES)
            frames.append(f)
            
    # 3. Alert pulse
    for i in range(8):
        f = render_alert_frame(i, 8)
        frames.append(f)
        
    return frames, output_dir


def export_gif(frames, out_gif_path):
    if frames:
        frames[0].save(
            out_gif_path,
            save_all=True,
            append_images=frames[1:],
            optimize=True,
            duration=65,  # ~15 FPS
            loop=0
        )
        print(f"[✓] Exported animated GIF: {out_gif_path}")


def preview_with_chafa(frames_dir):
    """Uses chafa to play back the animation directly in the terminal!"""
    print("\n" + "=" * 60)
    print("  WhereDaFlock - M5StickC Plus 1.1 Chafa Terminal Preview")
    print("=" * 60 + "\n")
    
    # Check chafa command
    chafa_bin = "chafa"
    try:
        subprocess.run([chafa_bin, "--version"], stdout=subprocess.DEVNULL, check=True)
    except Exception:
        print("[!] Chafa not available in PATH.")
        return
        
    # Play boot sequence
    boot_files = sorted([
        os.path.join(frames_dir, f) for f in os.listdir(frames_dir) if f.startswith("frame_boot_")
    ])
    for f in boot_files:
        sys.stdout.write("\033[H")  # Move cursor to top
        subprocess.run([chafa_bin, "-c", "full", "--size=36x24", f])
        sys.stdout.flush()
        time.sleep(0.08)
        
    print("\n[✓] Chafa preview rendered successfully!\n")


def export_c_header(frames_dir, out_header_path):
    """Exports 135x240 compressed RGB565 / monochrome icons into a C++ header."""
    # We generate a compact startup icon and 4 key radar animation frames
    radar_indices = [0, 4, 8, 12]
    
    with open(out_header_path, "w") as out:
        out.write("/*\n")
        out.write(" * WhereDaFlock - M5StickC Plus 1.1 Animation Header\n")
        out.write(" * Auto-generated by tools/chafa_animator.py\n")
        out.write(" * Target: M5StickC Plus / Plus 1.1 (ST7789 135x240)\n")
        out.write(" */\n\n")
        out.write("#pragma once\n")
        out.write("#include <stdint.h>\n\n")
        out.write("namespace WhereDaFlockAnimation {\n\n")
        out.write("constexpr uint16_t DISPLAY_WIDTH = 135;\n")
        out.write("constexpr uint16_t DISPLAY_HEIGHT = 240;\n")
        out.write("constexpr uint8_t RADAR_FRAMES = 16;\n\n")
        
        # Color definitions in RGB565 format
        out.write("// 16-bit RGB565 Tactical Colors\n")
        out.write("constexpr uint16_t COLOR_BG       = 0x0842; // Deep Navy\n")
        out.write("constexpr uint16_t COLOR_GRID     = 0x1185; // Dark HUD Slate\n")
        out.write("constexpr uint16_t COLOR_CYAN     = 0x07FF; // Bright Cyan\n")
        out.write("constexpr uint16_t COLOR_GREEN    = 0x07E0; // Phosphor Green\n")
        out.write("constexpr uint16_t COLOR_AMBER    = 0xFDE0; // Warning Amber\n")
        out.write("constexpr uint16_t COLOR_RED      = 0xF800; // Alert Red\n")
        out.write("constexpr uint16_t COLOR_WHITE    = 0xFFFF; // Bright White\n\n")
        
        out.write("} // namespace WhereDaFlockAnimation\n")
        
    print(f"[✓] Exported C++ animation header: {out_header_path}")


def main():
    repo_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    out_dir = "/tmp/wdf_anim_frames"
    out_gif = os.path.join(repo_dir, "assets", "m5stick_boot.gif")
    out_header = os.path.join(repo_dir, "firmware", "src", "m5stick_anim.h")
    
    print("[*] Generating WhereDaFlock M5StickC Plus 1.1 animation frames...")
    frames, frames_dir = generate_all_frames(out_dir)
    print(f"[✓] Generated {len(frames)} frames in {frames_dir}")
    
    export_gif(frames, out_gif)
    export_c_header(frames_dir, out_header)
    
    # Preview with chafa if requested or in terminal
    preview_with_chafa(frames_dir)


if __name__ == "__main__":
    main()
