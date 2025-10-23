#!/usr/bin/env python3
"""
GIF/Video → PNG frames → LVGL C arrays (+ .h) pipeline

Features
- Select multiple GIF/video inputs (via CLI args)
- Ask once if you want to resize (scale factor)
- For videos: ask how many frames to extract; compute FPS
- Create a folder per input (named after the input base)
- Convert every frame PNG → .c via lvgl_img_conv.py
- Emit a per-input header: LV_IMAGE_DECLARE(...) for each frame + a null-terminated frame pointer array
- Optionally use --cf A1 and/or --compress LZ4 (toggles)
- Edge smoothing for transparent PNGs (anti-aliasing alpha edges)
- Black & white conversion with edge detection and smoothing
- Also writes a master "all_animations.h" that includes all per-input headers

Dependencies:
  pip install pillow opencv-python pypng lz4 tqdm numpy
  brew install potrace (macOS) or apt-get install potrace (Linux)
And have `pngquant` installed in PATH if your converter needs it for indexed modes.
"""
import os
import re
import cv2
import math
import argparse
import subprocess
import numpy as np
from pathlib import Path
from typing import List, Tuple
from tqdm import tqdm
from PIL import Image, ImageSequence, ImageFilter
import tempfile
import shutil



# ---- CONFIG: path to your converter script (the one you pasted earlier) ----
CONVERTER = "python3 lvgl_img_conv.py"   # or absolute path if you prefer
# ---------------------------------------------------------------------------

# Check if potrace is available
def has_potrace() -> bool:
    """Check if potrace command is available."""
    try:
        subprocess.run(['potrace', '--version'], 
                      stdout=subprocess.PIPE, 
                      stderr=subprocess.PIPE, 
                      check=False)
        return True
    except FileNotFoundError:
        return False

POTRACE_AVAILABLE = has_potrace()


def to_symbol_base(s: str) -> str:
    """
    Create a valid C identifier base from a filename base.
    e.g., 'nail-trans' -> 'nail_trans'
    """
    base = re.sub(r'[^0-9a-zA-Z_]', '_', s)
    if base and base[0].isdigit():
        base = "_" + base
    return base


def ask_yes_no(prompt: str, default: bool) -> bool:
    d = "Y/n" if default else "y/N"
    while True:
        ans = input(f"{prompt} ({d}): ").strip().lower()
        if not ans:
            return default
        if ans in ("y", "yes"): return True
        if ans in ("n", "no"):  return False
        print("Please answer y or n.")


def ask_scale() -> float:
    if not ask_yes_no("Resize frames?", False):
        return 1.0
    while True:
        try:
            scale = float(input("Enter scale (e.g., 0.5 for half-size): ").strip())
            if scale > 0:
                return scale
        except Exception:
            pass
        print("Please enter a positive number.")


def ask_edge_smoothing() -> Tuple[bool, int]:
    """
    Ask if user wants edge smoothing and the radius.
    Returns (enable, radius)
    """
    if not ask_yes_no("Apply edge smoothing to transparent areas?", False):
        return False, 0
    
    while True:
        try:
            radius = int(input("Enter smoothing radius (1-5, recommended: 2): ").strip() or "2")
            if 1 <= radius <= 5:
                return True, radius
        except Exception:
            pass
        print("Please enter a number between 1 and 5.")


def ask_background_removal() -> Tuple[bool, Tuple[int, int, int], int]:
    """
    Ask if user wants background color removal.
    Returns (enable, bg_color_rgb, tolerance)
    """
    if not ask_yes_no("Remove background color?", False):
        return False, (0, 0, 0), 0
    
    print("Enter background color to remove (RGB format):")
    while True:
        try:
            r = int(input("  Red (0-255): ").strip())
            g = int(input("  Green (0-255): ").strip())
            b = int(input("  Blue (0-255): ").strip())
            if all(0 <= c <= 255 for c in (r, g, b)):
                break
            print("Values must be between 0 and 255")
        except ValueError:
            print("Please enter valid numbers")
    
    print("\nColor tolerance (how similar pixels should be removed):")
    print("  0 = exact match only")
    print("  10-30 = slight variations")
    print("  50+ = more aggressive")
    while True:
        try:
            tolerance = int(input("Enter tolerance (0-100, recommended: 20): ").strip() or "20")
            if 0 <= tolerance <= 100:
                return True, (r, g, b), tolerance
        except ValueError:
            pass
        print("Please enter a number between 0 and 100")


def remove_background_color(img: Image.Image, bg_color: Tuple[int, int, int], 
                            tolerance: int = 20) -> Image.Image:
    """
    Remove a specific background color by converting it to transparent.
    
    Uses Euclidean distance in RGB space to match similar colors.
    
    Args:
        img: PIL Image (will be converted to RGBA)
        bg_color: RGB tuple (r, g, b) of color to remove
        tolerance: Color distance threshold (0-100)
                  0 = exact match only
                  20 = slight variations (recommended)
                  50+ = aggressive removal
    
    Returns:
        PIL Image in RGBA mode with background removed
    """
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    
    img_array = np.array(img)
    rgb = img_array[:, :, :3].astype(np.float32)
    alpha_original = img_array[:, :, 3].copy()
    
    # DEBUG: Sample some pixel colors to understand what we're working with
    h, w = rgb.shape[:2]
    sample_pixels = [
        ("Top-left corner", rgb[0, 0]),
        ("Top-right corner", rgb[0, -1]),
        ("Bottom-left corner", rgb[-1, 0]),
        ("Bottom-right corner", rgb[-1, -1]),
        ("Center", rgb[h//2, w//2]),
    ]
    
    print(f"  🔍 Sampling pixel colors (target background: RGB{bg_color}):")
    for location, pixel in sample_pixels:
        print(f"     {location}: RGB({int(pixel[0])}, {int(pixel[1])}, {int(pixel[2])})")
    
    # Calculate unique colors to see diversity
    unique_colors = np.unique(rgb.reshape(-1, 3), axis=0)
    print(f"  📊 Unique RGB colors in image: {len(unique_colors)}")
    
    # CRITICAL CHECK: If image is all one color, something went wrong
    if len(unique_colors) <= 2:
        print(f"  ⚠️  WARNING: Image has very few colors ({len(unique_colors)})")
        print(f"  ⚠️  This might indicate SVG rendering failed!")
        print(f"  ⚠️  SKIPPING background removal to preserve what's left")
        return img
    
    # Calculate color distance using Euclidean distance in RGB space
    # Distance = sqrt((r1-r2)² + (g1-g2)² + (b1-b2)²)
    bg_r, bg_g, bg_b = bg_color
    
    # Calculate per-channel differences
    diff_r = rgb[:, :, 0] - bg_r
    diff_g = rgb[:, :, 1] - bg_g
    diff_b = rgb[:, :, 2] - bg_b
    
    # Calculate Euclidean distance
    distance = np.sqrt(diff_r**2 + diff_g**2 + diff_b**2)
    
    # Normalize distance to 0-100 range (max distance in RGB is ~442)
    distance_normalized = (distance / 442.0) * 100
    
    # DEBUG: Show distance distribution
    min_dist = distance_normalized.min()
    max_dist = distance_normalized.max()
    mean_dist = distance_normalized.mean()
    print(f"  📊 Color distance from target: min={min_dist:.1f}, mean={mean_dist:.1f}, max={max_dist:.1f}")
    print(f"  📊 Tolerance threshold: {tolerance}")
    
    # Show histogram of distances
    within_tolerance = np.sum(distance_normalized <= tolerance)
    within_10 = np.sum(distance_normalized <= 10)
    within_20 = np.sum(distance_normalized <= 20)
    within_50 = np.sum(distance_normalized <= 50)
    total = distance_normalized.size
    
    print(f"  📊 Pixels by distance bracket:")
    print(f"     0-10:  {within_10:6d} pixels ({within_10/total*100:.1f}%)")
    print(f"     0-20:  {within_20:6d} pixels ({within_20/total*100:.1f}%)")
    print(f"     0-50:  {within_50:6d} pixels ({within_50/total*100:.1f}%)")
    print(f"     At tolerance ({tolerance}): {within_tolerance:6d} pixels ({within_tolerance/total*100:.1f}%)")
    
    # Create mask: pixels within tolerance of background color
    bg_mask = distance_normalized <= tolerance
    
    # CRITICAL: Only remove pixels that are opaque (not already transparent)
    opaque_mask = alpha_original > 10
    bg_mask = bg_mask & opaque_mask
    
    print(f"  📊 Opaque pixels: {np.sum(opaque_mask)}")
    print(f"  📊 Will remove (opaque + within tolerance): {np.sum(bg_mask)} pixels")
    
    # Safety check: Don't remove more than 95% of pixels
    removal_percent = np.sum(bg_mask) / total * 100
    if removal_percent > 95:
        print(f"  ⚠️  WARNING: Would remove {removal_percent:.1f}% of pixels!")
        print(f"  ⚠️  This is too aggressive - SKIPPING background removal")
        print(f"  💡 Suggestion: Check your tolerance ({tolerance}) or target color RGB{bg_color}")
        return img
    
    # Apply mask to alpha channel
    alpha = alpha_original.copy()
    alpha[bg_mask] = 0
    
    # Optional: create a gradient at the edges for smoother removal
    # Find edge pixels (pixels near the boundary of removed area)
    edge_zone = (distance_normalized > tolerance) & (distance_normalized <= tolerance + 10) & opaque_mask
    
    # For edge pixels, gradually reduce alpha based on distance
    if edge_zone.any():
        edge_alpha = np.clip(
            255 * (distance_normalized[edge_zone] - tolerance) / 10,
            0, 255
        ).astype(np.uint8)
        alpha[edge_zone] = np.minimum(alpha[edge_zone], edge_alpha)
        print(f"  📊 Edge feathering: {np.sum(edge_zone)} pixels")
    
    # Apply the new alpha channel
    result = img_array.copy()
    result[:, :, 3] = alpha
    
    print(f"  🎨 Removed {removal_percent:.1f}% of pixels (matched background color)")
    
    return Image.fromarray(result, mode='RGBA')


def ask_black_and_white() -> Tuple[bool, int]:
    """
    Ask if user wants black and white conversion with enhanced edge detection.
    Returns (enable, threshold)
    """
    if not ask_yes_no("Convert to black & white (1-bit with edge smoothing)?", False):
        return False, 128
    
    while True:
        try:
            threshold = int(input("Enter threshold (0-255, recommended: 128): ").strip() or "128")
            if 0 <= threshold <= 255:
                return True, threshold
        except Exception:
            pass
        print("Please enter a number between 0 and 255.")


def ask_use_svg() -> bool:
    """
    Ask if user wants to use SVG intermediate processing for sharper edges.
    Only available if potrace is installed.
    """
    if not POTRACE_AVAILABLE:
        return False
    
    return ask_yes_no("Use SVG vectorization for sharper edges? (best for simple graphics)", False)


def png_to_svg_via_potrace(png_path: Path, svg_path: Path) -> bool:
    """
    Convert PNG to SVG using potrace for vectorization.
    
    Args:
        png_path: Input PNG file
        svg_path: Output SVG file
    
    Returns:
        True if successful, False otherwise
    """
    try:
        # potrace requires BMP/PBM input, so convert PNG to BMP first
        img = Image.open(png_path)
        
        # Handle different image modes
        if img.mode == 'RGBA':
            # Extract RGB and alpha channel
            img_array = np.array(img)
            rgb = img_array[:, :, :3]
            alpha = img_array[:, :, 3]
            
            # Calculate luminance from RGB
            luminance = (0.299 * rgb[:, :, 0] + 
                        0.587 * rgb[:, :, 1] + 
                        0.114 * rgb[:, :, 2])
            
            # For potrace to work correctly with white-on-black:
            # - Potrace traces BLACK pixels as foreground
            # - We have WHITE ovals on BLACK/transparent background
            # - Solution: INVERT so white becomes black for tracing
            
            # Create mask: opaque areas
            is_opaque = alpha > 128
            
            # Invert luminance: white (255) → black (0) for tracing
            # But only for opaque pixels
            inverted = np.where(is_opaque, 255 - luminance, 255).astype(np.uint8)
            
            img_bw = Image.fromarray(inverted, mode='L')
        else:
            # For non-RGBA, invert grayscale
            gray = img.convert('L')
            inverted = np.array(gray)
            inverted = 255 - inverted
            img_bw = Image.fromarray(inverted, mode='L')
        
        # Save as BMP for potrace
        with tempfile.NamedTemporaryFile(suffix='.bmp', delete=False) as tmp_bmp:
            tmp_bmp_path = tmp_bmp.name
            img_bw.save(tmp_bmp_path)
        
        try:
            # Run potrace to convert BMP to SVG
            # Now we DON'T use --invert because we already inverted the input
            # Potrace will trace the black (formerly white) pixels
            cmd = [
                'potrace',
                '-b', 'svg',
                '-k', '0.3',  # Smoother curves
                '-O', '0.2',  # Good optimization
                '-t', '128',  # Threshold
                '-o', str(svg_path),
                tmp_bmp_path
            ]
            
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True, 
                                  check=True)
            
            # Post-process SVG to set fill color to white (original color)
            if svg_path.exists():
                with open(svg_path, 'r') as f:
                    svg_content = f.read()
                
                # Replace default black fill with white
                # Potrace outputs paths with black fill by default
                svg_content = svg_content.replace('fill="#000000"', 'fill="#FFFFFF"')
                svg_content = svg_content.replace('fill="black"', 'fill="white"')
                svg_content = svg_content.replace('fill="#000"', 'fill="#FFF"')
                
                with open(svg_path, 'w') as f:
                    f.write(svg_content)
            
            return svg_path.exists()
            
        finally:
            # Clean up temp BMP
            Path(tmp_bmp_path).unlink(missing_ok=True)
            
    except Exception as e:
        print(f"Warning: SVG conversion failed: {e}")
        return False


def svg_to_png(svg_path: Path, png_path: Path, width: int, height: int) -> bool:
    """
    Render SVG to PNG at specified dimensions using cairosvg or rsvg-convert.
    
    Args:
        svg_path: Input SVG file
        png_path: Output PNG file
        width: Target width in pixels
        height: Target height in pixels
    
    Returns:
        True if successful, False otherwise
    """
    # Try cairosvg first (Python library)
    try:
        import cairosvg
        cairosvg.svg2png(
            url=str(svg_path),
            write_to=str(png_path),
            output_width=width,
            output_height=height,
            background_color='transparent'
        )
        return True
    except ImportError:
        pass  # Try rsvg-convert instead
    except Exception as e:
        print(f"Warning: cairosvg failed: {e}")
    
    # Try rsvg-convert (command-line tool)
    try:
        cmd = [
            'rsvg-convert',
            '-w', str(width),
            '-h', str(height),
            '-b', 'transparent',
            '-o', str(png_path),
            str(svg_path)
        ]
        subprocess.run(cmd, check=True, capture_output=True)
        return png_path.exists()
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Warning: rsvg-convert failed: {e}")
    
    # Fallback: try ImageMagick
    try:
        cmd = [
            'convert',
            '-background', 'none',
            '-resize', f'{width}x{height}',
            str(svg_path),
            str(png_path)
        ]
        subprocess.run(cmd, check=True, capture_output=True)
        return png_path.exists()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    
    print("Error: No SVG renderer found. Install cairosvg, rsvg-convert, or ImageMagick")
    return False


def process_via_svg(img: Image.Image, target_width: int, target_height: int, 
                   debug_path: Path = None) -> Image.Image:
    """
    Process image via SVG intermediate for sharper edges.
    
    Pipeline: PNG → SVG (vectorize) → Scale → PNG (render)
    
    Args:
        img: PIL Image in RGBA mode
        target_width: Desired width
        target_height: Desired height
        debug_path: Optional path to save SVG for debugging
    
    Returns:
        Processed PIL Image with sharp edges
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        
        # Save current image as PNG
        png_in = tmp_path / "input.png"
        img.save(png_in)
        
        # Convert to SVG
        svg_path = tmp_path / "vector.svg"
        if not png_to_svg_via_potrace(png_in, svg_path):
            print("SVG conversion failed, using standard scaling")
            return img.resize((target_width, target_height), resample=Image.LANCZOS)
        
        # Export SVG for debugging if requested
        if debug_path:
            import shutil
            shutil.copy2(svg_path, debug_path)
            print(f"  📄 Exported SVG to: {debug_path}")
        
        # Render SVG to PNG at target size
        png_out = tmp_path / "output.png"
        if not svg_to_png(svg_path, png_out, target_width, target_height):
            print("SVG rendering failed, using standard scaling")
            return img.resize((target_width, target_height), resample=Image.LANCZOS)
        
        # Load the rendered PNG
        result = Image.open(png_out)
        
        # Ensure RGBA mode and preserve original alpha behavior
        if result.mode != 'RGBA':
            result = result.convert('RGBA')
        
        return result


def dilate_mask(mask: np.ndarray, iterations: int = 3) -> np.ndarray:
    """
    Simple binary dilation without scipy dependency.
    Expands True regions in a boolean mask.
    """
    result = mask.copy()
    for _ in range(iterations):
        # Create a padded version to handle edges
        padded = np.pad(result, 1, mode='constant', constant_values=False)
        
        # Check 8-connected neighbors
        dilated = (
            padded[:-2, 1:-1] |  # top
            padded[2:, 1:-1] |   # bottom
            padded[1:-1, :-2] |  # left
            padded[1:-1, 2:] |   # right
            padded[:-2, :-2] |   # top-left
            padded[:-2, 2:] |    # top-right
            padded[2:, :-2] |    # bottom-left
            padded[2:, 2:]       # bottom-right
        )
        result = dilated
    return result


def convert_to_bw_with_smooth_edges(img: Image.Image, threshold: int = 128, smooth_radius: int = 2) -> Image.Image:
    """
    Convert RGBA image to black & white with enhanced edge detection and smoothing.
    
    Process:
    1. Extract alpha channel for transparency
    2. Convert RGB to grayscale
    3. Apply edge detection to find boundaries
    4. Smooth edges with anti-aliasing
    5. Apply threshold to create clean B&W
    6. Preserve alpha transparency
    
    Args:
        img: PIL Image in RGBA mode
        threshold: Grayscale threshold (0-255), pixels above become white
        smooth_radius: Edge smoothing radius for anti-aliasing
    
    Returns:
        PIL Image in RGBA mode with B&W color and smooth edges
    """
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    
    # Convert to numpy for processing
    img_array = np.array(img)
    rgb = img_array[:, :, :3]
    alpha = img_array[:, :, 3]
    
    # Convert to grayscale using luminance formula
    grayscale = (0.299 * rgb[:, :, 0] + 
                 0.587 * rgb[:, :, 1] + 
                 0.114 * rgb[:, :, 2]).astype(np.float32)
    
    # Detect edges using Sobel-like filter
    # This helps us identify where to apply smoothing
    padded = np.pad(grayscale, 1, mode='edge')
    
    # Horizontal edges
    h_edge = np.abs(
        -1 * padded[:-2, 1:-1] +
        -2 * padded[1:-1, 1:-1] +
        -1 * padded[2:, 1:-1] +
         1 * padded[:-2, 1:-1] +
         2 * padded[1:-1, 1:-1] +
         1 * padded[2:, 1:-1]
    )
    
    # Vertical edges
    v_edge = np.abs(
        -1 * padded[1:-1, :-2] +
        -2 * padded[1:-1, 1:-1] +
        -1 * padded[1:-1, 2:] +
         1 * padded[1:-1, :-2] +
         2 * padded[1:-1, 1:-1] +
         1 * padded[1:-1, 2:]
    )
    
    # Combine edge detection
    edges = np.sqrt(h_edge**2 + v_edge**2)
    edge_mask = edges > (edges.mean() + edges.std() * 0.5)
    
    # Dilate edge mask to expand smoothing area
    edge_mask_dilated = dilate_mask(edge_mask, iterations=smooth_radius)
    
    # Apply Gaussian blur to grayscale for smooth anti-aliased edges
    grayscale_pil = Image.fromarray(grayscale.astype(np.uint8), mode='L')
    smoothed = np.array(grayscale_pil.filter(ImageFilter.GaussianBlur(radius=smooth_radius))).astype(np.float32)
    
    # Blend: use smoothed version at edges, original elsewhere
    processed = np.where(edge_mask_dilated, smoothed, grayscale)
    
    # Apply threshold to create B&W
    bw = (processed >= threshold).astype(np.uint8) * 255
    
    # Optional: Apply slight blur to B&W edges for even smoother anti-aliasing
    bw_pil = Image.fromarray(bw, mode='L')
    bw_smooth = np.array(bw_pil.filter(ImageFilter.GaussianBlur(radius=0.5)))
    
    # Create final RGBA image with B&W color channels
    result = np.zeros_like(img_array)
    result[:, :, 0] = bw_smooth  # R
    result[:, :, 1] = bw_smooth  # G
    result[:, :, 2] = bw_smooth  # B
    result[:, :, 3] = alpha       # Preserve original alpha
    
    return Image.fromarray(result, mode='RGBA')


def smooth_alpha_edges(img: Image.Image, radius: int = 2) -> Image.Image:
    """
    Aggressively smooth the edges of transparent areas in an RGBA image.
    
    This function uses multiple passes and techniques:
    1. Dilates the alpha to expand edges slightly
    2. Applies multiple Gaussian blur passes
    3. Creates smooth gradient transitions at edges
    4. Preserves fully opaque and fully transparent areas
    
    Args:
        img: PIL Image in RGBA mode
        radius: Smoothing radius (1-5, higher = more smoothing)
    
    Returns:
        PIL Image with heavily smoothed alpha edges
    """
    if img.mode != 'RGBA':
        return img
    
    # Convert to numpy array for processing
    img_array = np.array(img)
    rgb = img_array[:, :, :3]
    alpha = img_array[:, :, 3].astype(np.float32)
    
    # Save original for reference
    original_alpha = alpha.copy()
    
    # Identify edge regions (transition zones)
    # An edge is where alpha is between 5 and 250 (not fully transparent or opaque)
    edge_mask = (alpha > 5) & (alpha < 250)
    
    # Dilate the edge mask to expand smoothing region
    kernel_size = max(3, radius * 2)
    edge_mask_dilated = dilate_mask(edge_mask, iterations=kernel_size)
    
    # Multi-pass Gaussian blur for aggressive smoothing
    smoothed_alpha = alpha.copy()
    for i in range(3):  # Multiple passes for stronger effect
        alpha_pil = Image.fromarray(smoothed_alpha.astype(np.uint8), mode='L')
        blur_radius = radius + i * 0.5  # Increasing radius each pass
        smoothed_alpha = np.array(alpha_pil.filter(ImageFilter.GaussianBlur(radius=blur_radius))).astype(np.float32)
    
    # Apply additional feathering at edges
    # Create distance-based gradient near edges
    alpha_pil = Image.fromarray(smoothed_alpha.astype(np.uint8), mode='L')
    feathered = np.array(alpha_pil.filter(ImageFilter.GaussianBlur(radius=radius * 1.5))).astype(np.float32)
    
    # Blend based on edge proximity
    result_alpha = np.where(edge_mask_dilated, 
                            feathered,  # Use heavily smoothed version at edges
                            original_alpha)  # Keep original in solid areas
    
    # Additional pass: smooth the transition between smoothed and original
    result_alpha = np.array(Image.fromarray(result_alpha.astype(np.uint8), mode='L')
                           .filter(ImageFilter.GaussianBlur(radius=1))).astype(np.float32)
    
    # Ensure we don't accidentally make fully transparent areas visible
    result_alpha = np.where(original_alpha < 3, 0, result_alpha)
    
    # Ensure we don't reduce opacity of solid areas
    result_alpha = np.where(original_alpha > 250, original_alpha, result_alpha)
    
    # Apply the smoothed alpha back to the image
    result_array = img_array.copy()
    result_array[:, :, 3] = np.clip(result_alpha, 0, 255).astype(np.uint8)
    
    return Image.fromarray(result_array, mode='RGBA')


def evenly_spaced_indices(total: int, desired: int) -> List[int]:
    """Return 'desired' indices from range(total), evenly spaced."""
    if desired >= total:
        return list(range(total))
    step = total / desired
    return [min(total - 1, int(round(i * step))) for i in range(desired)]

def extract_gif_frames(inp: Path, out_dir: Path, scale: float, 
                       smooth_edges: bool = False, smooth_radius: int = 2,
                       use_bw: bool = False, bw_threshold: int = 128,
                       use_svg: bool = False,
                       remove_bg: bool = False, bg_color: Tuple[int, int, int] = (0, 0, 0),
                       bg_tolerance: int = 20) -> Tuple[List[Path], float]:
    """
    Extract frames from a (possibly transparent) GIF into RGBA PNGs.
    Ask how many frames to extract (evenly spaced). Returns (frame_paths, fps).
    Suggested FPS is computed as: saved_frames / total_duration_seconds
    """
    im = Image.open(str(inp))

    # Gather raw frame durations (ms) and count
    durations_ms = []
    total_frames = 0
    for _ in ImageSequence.Iterator(im):
        dur = im.info.get("duration", 100)
        durations_ms.append(dur if dur and dur > 0 else 100)
        total_frames += 1

    print(f"{inp.name}: {total_frames} GIF frames (total ≈ {sum(durations_ms)/1000:.2f}s).")
    desired_str = input("How many frames to extract? (Enter for all): ").strip()
    desired = total_frames if not desired_str else max(1, int(desired_str))

    # Choose evenly spaced frame indices
    indices = sorted(set(evenly_spaced_indices(total_frames, desired)))
    want = set(indices)

    frame_paths = []
    base = inp.stem

    # Create SVG debug folder if using SVG
    svg_debug_dir = None
    if use_svg:
        svg_debug_dir = out_dir / "svg_debug"
        svg_debug_dir.mkdir(exist_ok=True)
        print(f"  📁 SVG debug files will be saved to: {svg_debug_dir}")
    
    # Create pre-background-removal debug folder if removing background
    pre_bg_dir = None
    if remove_bg:
        pre_bg_dir = out_dir / "before_bg_removal"
        pre_bg_dir.mkdir(exist_ok=True)
        print(f"  📁 PNG files before background removal will be saved to: {pre_bg_dir}")

    # Now iterate again and save only selected frames
    for idx, frame in enumerate(ImageSequence.Iterator(im)):
        if idx not in want:
            continue
        fr = frame.convert("RGBA")
        
        # Handle scaling with optional SVG intermediate
        if scale != 1.0:
            new_w = max(1, int(fr.width * scale))
            new_h = max(1, int(fr.height * scale))
            
            if use_svg:
                # Use SVG for sharper scaling with debug export
                svg_debug_path = svg_debug_dir / f"{base}_frame_{len(frame_paths)+1:03d}.svg"
                fr = process_via_svg(fr, new_w, new_h, debug_path=svg_debug_path)
            else:
                # Standard LANCZOS scaling
                fr = fr.resize((new_w, new_h), resample=Image.LANCZOS)
        
        # Export PNG BEFORE background removal (for debugging/comparison)
        if remove_bg and pre_bg_dir:
            pre_bg_name = f"{base}_frame_{len(frame_paths)+1:03d}.png"
            pre_bg_path = pre_bg_dir / pre_bg_name
            fr.save(pre_bg_path)
            print(f"  💾 Saved pre-removal: {pre_bg_name}")
            
            # DEBUG: Verify what we just saved matches what we have in memory
            test_reload = Image.open(pre_bg_path)
            test_array = np.array(test_reload)
            test_rgb = test_array[:, :, :3]
            unique_test = len(np.unique(test_rgb.reshape(-1, 3), axis=0))
            print(f"  🔍 DEBUG: Saved file has {unique_test} unique colors")
            print(f"  🔍 DEBUG: Center pixel in saved file: RGB{tuple(test_rgb[test_array.shape[0]//2, test_array.shape[1]//2])}")
            
            # Check in-memory image
            mem_array = np.array(fr)
            mem_rgb = mem_array[:, :, :3]
            unique_mem = len(np.unique(mem_rgb.reshape(-1, 3), axis=0))
            print(f"  🔍 DEBUG: In-memory has {unique_mem} unique colors")
            print(f"  🔍 DEBUG: Center pixel in-memory: RGB{tuple(mem_rgb[mem_array.shape[0]//2, mem_array.shape[1]//2])}")
        
        # Apply background removal AFTER scaling (SVG → PNG → remove BG)
        if remove_bg:
            fr = remove_background_color(fr, bg_color, bg_tolerance)
        
        # Apply B&W conversion if requested (do this before edge smoothing)
        if use_bw:
            fr = convert_to_bw_with_smooth_edges(fr, threshold=bw_threshold, smooth_radius=max(1, smooth_radius))
        # Apply edge smoothing if requested (and not already done by B&W)
        elif smooth_edges:
            fr = smooth_alpha_edges(fr, radius=smooth_radius)

        png_name = f"{base}_frame_{len(frame_paths)+1:03d}.png"
        out_path = out_dir / png_name
        fr.save(out_path)
        frame_paths.append(out_path)

    # Suggested FPS: distribute the saved frames across the original total duration
    total_s = (sum(durations_ms) / 1000.0) if durations_ms else max(len(frame_paths), 1) / 10.0
    fps = (len(frame_paths) / total_s) if total_s > 0 else 10.0
    print(f"Extracted {len(frame_paths)} frames; suggested FPS ≈ {fps:.2f}")

    return frame_paths, fps

def extract_video_frames(inp: Path, out_dir: Path, scale: float,
                         smooth_edges: bool = False, smooth_radius: int = 2,
                         use_bw: bool = False, bw_threshold: int = 128,
                         use_svg: bool = False,
                         remove_bg: bool = False, bg_color: Tuple[int, int, int] = (0, 0, 0),
                         bg_tolerance: int = 20) -> Tuple[List[Path], float]:
    """
    Extracts evenly spaced frames from a video into RGBA PNGs.
    Ask the user how many frames; compute FPS = frames / duration(s).
    Returns (frame_paths, fps)
    """
    cap = cv2.VideoCapture(str(inp))
    if not cap.isOpened():
        raise RuntimeError(f"Failed to open video: {inp}")

    total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps_src = float(cap.get(cv2.CAP_PROP_FPS)) if cap.get(cv2.CAP_PROP_FPS) else 30.0
    duration_s = total / fps_src if fps_src > 0 and total > 0 else 1.0

    print(f"{inp.name}: ~{total} frames at {fps_src:.2f} fps (duration ~{duration_s:.2f}s).")
    desired_str = input("How many frames to extract? (Enter for all): ").strip()
    desired = total if not desired_str else max(1, int(desired_str))

    indices = sorted(set(evenly_spaced_indices(total, desired)))
    frame_paths = []

    # Create SVG debug folder if using SVG
    svg_debug_dir = None
    if use_svg:
        svg_debug_dir = out_dir / "svg_debug"
        svg_debug_dir.mkdir(exist_ok=True)
        print(f"  📁 SVG debug files will be saved to: {svg_debug_dir}")
    
    # Create pre-background-removal debug folder if removing background
    pre_bg_dir = None
    if remove_bg:
        pre_bg_dir = out_dir / "before_bg_removal"
        pre_bg_dir.mkdir(exist_ok=True)
        print(f"  📁 PNG files before background removal will be saved to: {pre_bg_dir}")

    # random access might be slow; we iterate sequentially and keep a pointer
    want = set(indices)
    next_target = 0
    pbar = tqdm(total=total, desc=f"Extracting {inp.name}", unit="f")

    idx = 0
    saved = 0
    base = inp.stem
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        if idx in want:
            # BGR -> BGRA for alpha channel (opaque)
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2BGRA)
            
            # Convert to PIL for processing
            pil_img = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGRA2RGBA))
            
            # Handle scaling with optional SVG intermediate
            if scale != 1.0:
                new_w = max(1, int(pil_img.width * scale))
                new_h = max(1, int(pil_img.height * scale))
                
                if use_svg:
                    # Use SVG for sharper scaling with debug export
                    svg_debug_path = svg_debug_dir / f"{base}_frame_{saved+1:03d}.svg"
                    pil_img = process_via_svg(pil_img, new_w, new_h, debug_path=svg_debug_path)
                else:
                    # Standard LANCZOS scaling
                    pil_img = pil_img.resize((new_w, new_h), resample=Image.LANCZOS)
            
            # Export PNG BEFORE background removal (for debugging/comparison)
            if remove_bg and pre_bg_dir:
                pre_bg_name = f"{base}_frame_{saved+1:03d}.png"
                pre_bg_path = pre_bg_dir / pre_bg_name
                pil_img.save(pre_bg_path)
                print(f"  💾 Saved pre-removal: {pre_bg_name}")
            
            # Apply background removal AFTER scaling (SVG → PNG → remove BG)
            if remove_bg:
                pil_img = remove_background_color(pil_img, bg_color, bg_tolerance)
            
            # Apply B&W conversion if requested (includes edge smoothing)
            if use_bw:
                pil_img = convert_to_bw_with_smooth_edges(pil_img, threshold=bw_threshold, smooth_radius=max(1, smooth_radius))
            # Apply edge smoothing if requested (and not already done by B&W)
            elif smooth_edges:
                pil_img = smooth_alpha_edges(pil_img, radius=smooth_radius)
            
            # Convert back to OpenCV format for saving
            frame = cv2.cvtColor(np.array(pil_img), cv2.COLOR_RGBA2BGRA)
            
            png_name = f"{base}_frame_{saved+1:03d}.png"
            out_path = out_dir / png_name
            cv2.imwrite(str(out_path), frame)
            frame_paths.append(out_path)
            saved += 1
            if saved >= len(indices):
                # collected all desired frames
                pbar.update(total - idx)
                break
        idx += 1
        pbar.update(1)

    cap.release()
    pbar.close()

    # Effective FPS if you were to play these frames evenly over original duration:
    fps = (saved / duration_s) if duration_s > 0 else fps_src
    print(f"Extracted {saved} frames; suggested FPS ≈ {fps:.2f}")
    return frame_paths, fps


def run_converter(png_path: Path, out_dir: Path, use_a1: bool, use_lz4: bool):
    """
    Call lvgl_img_conv.py to create a .c file for this PNG.
    We pass --name so the C symbol matches the PNG base (without .png).
    """
    cf = "A1" if use_a1 else "ARGB8888"
    compress = "LZ4" if use_lz4 else "NONE"

    # Output name (symbol) must be a valid C identifier – mirror the PNG base but sanitize.
    symbol = to_symbol_base(png_path.stem)

    cmd = (
        f'{CONVERTER} '
        f'--ofmt C '
        f'--cf {cf} '
        f'--compress {compress} '
        f'-o "{out_dir}" '
        f'--name "{symbol}" '
        f'"{png_path}"'
    )
    subprocess.run(cmd, shell=True, check=True)

def write_per_input_header(input_base: str, out_dir: Path, frame_count: int):
    """
    Create <base>.h inside <out_dir> that:
      - #includes each generated <base>_frame_###.c (same folder)
      - defines a NULL-terminated array of frame pointers
      - defines <BASE>_ANIM_FRAME_COUNT
    NOTE: Including .c files from a header causes *definitions* to be pulled
    into any TU that includes this header. Be sure you only include this header
    from a single C/C++ file to avoid multiple-definition link errors.
    """
    guard = f"{to_symbol_base(input_base).upper()}_ANIM_H"
    decl_prefix = to_symbol_base(input_base) + "_frame_"

    header_path = out_dir / f"{input_base}.h"
    with open(header_path, "w", encoding="utf-8") as f:
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")

        # Includes: pull in each generated .c file that defines lv_image_dsc_t symbols
        f.write(f"// Auto-included frame definitions for '{input_base}'\n")
        for i in range(1, frame_count + 1):
            name = f"{decl_prefix}{i:03d}"
            f.write(f'#include "{name}.c"\n')
        f.write("\n")

        # Build a NULL-terminated pointer array to the frames
        array_name = f"{to_symbol_base(input_base)}_anim_frames"
        f.write(f"// Array of frame pointers (NULL-terminated)\n")
        f.write(f"static const lv_image_dsc_t * {array_name}[] = {{\n")
        for i in range(1, frame_count + 1):
            name = f"{decl_prefix}{i:03d}"
            f.write(f"    &{name},\n")
        f.write("    NULL\n")
        f.write("};\n\n")

        # Frame count macro
        f.write(f"#define {to_symbol_base(input_base).upper()}_ANIM_FRAME_COUNT {frame_count}\n\n")
        f.write(f"#endif // {guard}\n")

    print(f"  • Wrote header: {header_path.name}")


def write_master_header(all_inputs: List[Tuple[str, Path]]):
    """
    Writes 'all_animations.h' that #includes each per-input header (relative paths).
    """
    out = Path("all_animations.h")
    with open(out, "w", encoding="utf-8") as f:
        f.write("// Auto-generated: aggregates all per-input animation headers\n\n")
        for base, out_dir in all_inputs:
            rel = out_dir / f"{base}.h"
            f.write(f'#include "{rel.as_posix()}"\n')
    print(f"✅ Wrote master header: {out}")


def process_one_input(inp: Path, scale: float, use_a1: bool, use_lz4: bool,
                      smooth_edges: bool = False, smooth_radius: int = 2,
                      use_bw: bool = False, bw_threshold: int = 128,
                      use_svg: bool = False,
                      remove_bg: bool = False, bg_color: Tuple[int, int, int] = (0, 0, 0),
                      bg_tolerance: int = 20) -> Tuple[str, Path]:
    """
    - make folder (./<base>)
    - extract frames into that folder (names based on input base)
    - convert each PNG to C using converter (symbol == filename base)
    - generate header for this input
    Returns (base, folder_path) for master header aggregation.
    """
    base = inp.stem
    out_dir = Path(base)
    out_dir.mkdir(exist_ok=True)

    # 1) Extract frames
    if inp.suffix.lower() == ".gif":
        frames, fps = extract_gif_frames(inp, out_dir, scale, smooth_edges, smooth_radius, 
                                        use_bw, bw_threshold, use_svg,
                                        remove_bg, bg_color, bg_tolerance)
    else:
        frames, fps = extract_video_frames(inp, out_dir, scale, smooth_edges, smooth_radius, 
                                          use_bw, bw_threshold, use_svg,
                                          remove_bg, bg_color, bg_tolerance)

    print(f"  Suggested playback FPS for {base}: {fps:.2f}")

    # 2) Convert each frame PNG → .c
    print(f"  Converting {len(frames)} frames to LVGL C arrays...")
    for png in tqdm(frames, desc=f"Converting {base}", unit="png"):
        run_converter(png_path=png, out_dir=out_dir, use_a1=use_a1, use_lz4=use_lz4)

    # 3) Write per-input header
    write_per_input_header(input_base=base, out_dir=out_dir, frame_count=len(frames))

    return base, out_dir


def main():
    parser = argparse.ArgumentParser(
        description="Batch convert transparent GIFs / videos into LVGL C arrays with per-animation headers."
    )
    parser.add_argument("inputs", nargs="+", help="Input files (GIF, MP4, MOV, etc.)")
    parser.add_argument("--use-a1", action="store_true", help="Use --cf A1 (1-bit alpha) instead of ARGB8888.")
    parser.add_argument("--use-lz4", action="store_true", help="Use --compress LZ4 (otherwise NONE).")
    args = parser.parse_args()

    # Check if potrace is available and inform user
    if not POTRACE_AVAILABLE:
        print("ℹ️  Note: potrace not found. SVG vectorization will not be available.")
        print("   Install with: brew install potrace (macOS) or apt-get install potrace (Linux)")
        print()

    # Background removal prompt (asked once, before other options)
    remove_bg, bg_color, bg_tolerance = ask_background_removal()
    if remove_bg:
        print(f"✓ Background removal enabled: RGB{bg_color} with tolerance {bg_tolerance}")
    
    # Scale prompt (asked once)
    scale = ask_scale()
    
    # SVG vectorization prompt (asked once, only if potrace available)
    use_svg = ask_use_svg()
    if use_svg:
        print(f"✓ SVG vectorization enabled for sharper edges")
    
    # Black & white conversion prompt (asked once)
    use_bw, bw_threshold = ask_black_and_white()
    if use_bw:
        print(f"✓ B&W conversion enabled with threshold {bw_threshold}")
        # B&W includes edge smoothing, so skip separate edge smoothing prompt
        smooth_edges = False
        smooth_radius = 2  # Default radius for B&W edge smoothing
    else:
        # Edge smoothing prompt (asked once, only if not using B&W)
        smooth_edges, smooth_radius = ask_edge_smoothing()
        if smooth_edges:
            print(f"✓ Edge smoothing enabled with radius {smooth_radius}")

    all_inputs = []
    for raw in args.inputs:
        p = Path(raw)
        if not p.exists():
            print(f"Skipping: {raw} (not found)")
            continue
        print(f"\n=== Processing: {p.name} ===")
        try:
            info = process_one_input(p, scale=scale, use_a1=args.use_a1, use_lz4=args.use_lz4,
                                    smooth_edges=smooth_edges, smooth_radius=smooth_radius,
                                    use_bw=use_bw, bw_threshold=bw_threshold,
                                    use_svg=use_svg,
                                    remove_bg=remove_bg, bg_color=bg_color, bg_tolerance=bg_tolerance)
            all_inputs.append(info)
        except Exception as e:
            print(f"ERROR processing {p.name}: {e}")

    if all_inputs:
        write_master_header(all_inputs)
        print("\nAll done 🎉")
    else:
        print("No valid inputs processed.")


if __name__ == "__main__":
    main()
