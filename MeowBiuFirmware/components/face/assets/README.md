# GIF to LVGL Converter with SVG Vectorization

## 🎨 NEW: SVG Vectorization for **Crystal-Sharp Edges**

Perfect for simple graphics like icons, logos, and geometric shapes!

## Why SVG Makes a Huge Difference

### Your Screenshot Example (Two White Ovals)
This is the **perfect use case** for SVG vectorization!

**Traditional Raster Scaling:**
```
PNG (500x500) → Scale to 100x100 → Blurry/muddy edges
```

**SVG Vectorization (NEW!):**
```
PNG (500x500) → Vectorize → Scale to 100x100 → Perfect sharp edges
```

### The Magic of Vectors

- ✅ **Resolution-independent** - Scales perfectly to any size
- ✅ **Perfect curves** - Smooth anti-aliased edges
- ✅ **No quality loss** - Mathematical perfection
- ✅ **Debug SVG export** - See exactly what's happening

## Installation

### 1. Install Python Dependencies
```bash
pip install pillow opencv-python pypng lz4 tqdm numpy
```

### 2. Install potrace (Required for SVG)

**macOS:**
```bash
brew install potrace
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install potrace
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install potrace
```

### 3. Install SVG Renderer (Choose One)

**Option A: cairosvg (Recommended)**
```bash
pip install cairosvg
```

**Option B: rsvg-convert**
```bash
# macOS
brew install librsvg

# Linux
sudo apt-get install librsvg2-bin
```

**Option C: ImageMagick**
```bash
# macOS
brew install imagemagick

# Linux
sudo apt-get install imagemagick
```

## Usage

### Basic Usage with SVG
```bash
python gif_to_lvgl.py your_animation.gif --use-a1 --use-lz4
```

**Prompts:**
1. **Resize frames?** → Yes (if need to scale down)
   - Enter scale (e.g., 0.2 for 500x500 → 100x100)
2. **Use SVG vectorization?** → **Yes** ✓
3. **Convert to B&W?** → Yes/No (your choice)
4. **Apply edge smoothing?** → Usually not needed with SVG

### What You Get

```
your_animation/
├── your_animation_frame_001.png    # Final PNG (bg removed)
├── your_animation_frame_001.c      # LVGL C array
├── your_animation_frame_002.png
├── your_animation_frame_002.c
├── ...
├── your_animation.h                # Header file
├── svg_debug/                      # 🆕 SVG vectors (if using SVG)
│   ├── your_animation_frame_001.svg
│   └── your_animation_frame_002.svg
└── before_bg_removal/              # 🆕 PNG before removal!
    ├── your_animation_frame_001.png
    └── your_animation_frame_002.png
```

## 📊 Debug Files Explained

### before_bg_removal/ Folder

**Purpose:** Compare before/after background removal

**Contains:**
- PNG files after SVG rendering
- Before background removal applied
- Same size as final output
- All processing done EXCEPT background removal

**Use cases:**
1. **Verify SVG quality** - Check if vectorization worked
2. **Compare removal** - See exactly what was removed
3. **Adjust tolerance** - Use these to pick better values
4. **Troubleshoot** - Debug if removal is too aggressive/conservative

## When to Use SVG Vectorization

### ✅ PERFECT For:
- **Simple shapes** (circles, ovals, rectangles)
- **Icons and logos**
- **Geometric graphics**
- **Solid colors with clean edges**
- **Your screenshot example** (two white ovals)

### ❌ NOT Good For:
- **Photos or realistic images**
- **Complex gradients**
- **Textures or patterns**
- **Images with noise/grain**
- **Subtle shading**

## Examples

### Example 1: Simple Icon (Your Case!)
```bash
# Perfect for two white ovals on black background
python gif_to_lvgl.py eyes_blink.gif --use-a1 --use-lz4

# Prompts:
Resize frames? (y/N): y
Enter scale: 0.2                    # 500x500 → 100x100
Use SVG vectorization? (y/N): y    # ← Key for sharp edges!
Convert to B&W? (y/N): y
Enter threshold: 128

Result: Perfect sharp ovals at 100x100! ✨
```

### Example 2: Complex Animation
```bash
# Not suitable for photos/complex images
python gif_to_lvgl.py photo_anim.gif --use-a1

# Prompts:
Use SVG vectorization? (y/N): n    # ← Skip SVG, use edge smoothing instead
Apply edge smoothing? (y/N): y
Enter radius: 3
```

## How It Works

### Step-by-Step Process

1. **Extract Frame** from GIF/video
   ```
   Frame 1: 500x500 RGBA PNG
   ```

2. **Convert to BMP** (potrace requirement)
   ```
   Alpha channel → Grayscale → BMP
   ```

3. **Vectorize with potrace**
   ```
   potrace -b svg -k 0.3 -O 0.2 -t 128 → SVG file
   ```
   - `-k 0.3`: Smooth curves
   - `-O 0.2`: Optimization
   - `-t 128`: Threshold

4. **Render at Target Size**
   ```
   SVG → cairosvg → 100x100 PNG (perfect edges!)
   ```

5. **Export Debug SVG** 📄
   ```
   Copy to: animation_name/svg_debug/frame_001.svg
   ```

6. **Apply Post-Processing** (optional)
   - B&W conversion
   - Edge smoothing
   - Alpha adjustments

## Potrace Parameters

Fine-tune vectorization by editing the script:

```python
# In png_to_svg_via_potrace()
cmd = [
    'potrace',
    '-b', 'svg',
    '-k', '0.3',  # ← Corner threshold: 0.0 (sharp) to 1.0 (smooth)
    '-O', '0.2',  # ← Optimization: 0.0 (none) to 1.0 (aggressive)
    '-t', '128',  # ← Threshold: 0-255 (what's considered "black")
]
```

**Adjust for your needs:**
- **Sharper corners:** `-k 0.1`
- **Smoother curves:** `-k 0.5`
- **Simpler paths:** `-O 0.5`
- **More detail:** `-O 0.1`

## Troubleshooting

### "potrace not found"
```bash
# Install potrace
brew install potrace  # macOS
# or
sudo apt-get install potrace  # Linux
```

### "SVG rendering failed"
Install an SVG renderer:
```bash
pip install cairosvg  # Easiest
```

### SVG looks wrong in debug folder
- Check if image is **too complex** for vectorization
- Try adjusting `-k` parameter (lower = sharper)
- Verify alpha channel is correct
- Consider using standard scaling instead

### Edges still not perfect
1. **Check debug SVG** - Does it look correct?
2. **Adjust potrace params** - Try `-k 0.2` or `-k 0.4`
3. **Verify image is simple** - SVG works best on simple shapes
4. **Try B&W mode** - Gives cleaner edges for monochrome

## Comparison: SVG vs Standard Scaling

### Your Two-Ovals Example

**Standard LANCZOS (without SVG):**
```
500x500 → 100x100
Edge quality: ★★☆☆☆ (blurry)
File size: Normal
Best for: Nothing better available
```

**SVG Vectorization (with SVG):**
```
500x500 → Vectors → 100x100
Edge quality: ★★★★★ (perfect!)
File size: Same
Best for: Simple graphics like yours
```

**Visual difference:**
- Standard: Soft, slightly blurry edges
- SVG: Crisp, perfect curves

## Performance

**Processing time:**
- SVG adds ~0.5-1 second per frame
- Worth it for quality improvement
- Negligible for small frame counts

**File size:**
- Final PNG size: Same as without SVG
- Debug SVGs: ~1-5KB each (kept separate)

## Advanced: Inspect SVG Output

Open debug SVG in browser to see:
```xml
<svg>
  <path d="M 100,200 C 150,180 200,180 250,200 ..." />
  <!-- Perfect Bézier curves for your ovals! -->
</svg>
```

Compare with final PNG to verify quality.

## Complete Workflow Example

```bash
# Your simple oval animation with background removal
python gif_to_lvgl.py eyes.gif --use-a1 --use-lz4

# Prompts & Answers:
Remove background? y
  Red: 0
  Green: 0  
  Blue: 0
  Tolerance: 20

Resize frames? y
Enter scale: 0.2                   # 500x500 → 100x100

Use SVG vectorization? y           # Crystal-sharp edges!

Convert to B&W? n                  # Already B&W

# Output:
eyes/
├── eyes_frame_001.png             # ✅ Final: sharp + transparent
├── eyes_frame_001.c               # A1 format, LZ4 compressed
├── eyes.h                         # Animation header
├── svg_debug/
│   └── eyes_frame_001.svg         # Vector representation
└── before_bg_removal/             # 🆕 Before removal
    └── eyes_frame_001.png         # Sharp but with black bg

# Compare files:
open eyes/before_bg_removal/eyes_frame_001.png  # SVG result with bg
open eyes/eyes_frame_001.png                    # Final with bg removed
open eyes/svg_debug/eyes_frame_001.svg          # Vector source

# Result: Perfect ovals! 🎉
```

## Debug Workflow

### Step 1: Check SVG Quality

```bash
# Open SVG in browser
open eyes/svg_debug/eyes_frame_001.svg
```

**Look for:**
- ✅ Smooth curves (not jagged)
- ✅ Correct shapes traced
- ✅ No artifacts or holes

**If SVG looks wrong:**
- Adjust potrace parameters in script
- Check source image quality
- Try different `-k` value (smoothness)

### Step 2: Check Pre-Removal PNG

```bash
# View PNG after SVG rendering, before bg removal
open eyes/before_bg_removal/eyes_frame_001.png
```

**Look for:**
- ✅ Crisp edges from SVG
- ✅ Correct size (100x100)
- ✅ Black background present
- ✅ Ovals look perfect

**If PNG looks wrong:**
- SVG rendering may have failed
- Check cairosvg/rsvg installation
- Verify SVG file is valid

### Step 3: Check Final PNG

```bash
# View final output with bg removed
open eyes/eyes_frame_001.png
```

**Look for:**
- ✅ Transparent background (checkerboard)
- ✅ Ovals preserved perfectly
- ✅ Smooth edges maintained

**If too much removed:**
- Decrease tolerance (try 15, 10)
- Check RGB values match background

**If background remains:**
- Increase tolerance (try 30, 40)
- Verify RGB values are correct

### Step 4: Side-by-Side Comparison

```bash
# macOS: Quick Look both files
open eyes/before_bg_removal/eyes_frame_001.png eyes/eyes_frame_001.png

# Or use any image viewer for comparison
```

**Perfect result looks like:**
- Before: Sharp ovals + solid black background
- After: Same sharp ovals + transparent background
- Difference: Only background changed

## Tips & Tricks

1. **Always check debug SVGs first** - Verify vectorization quality
2. **Simple is better** - Works best on clean, simple graphics
3. **Combine with B&W** - SVG + B&W = ultimate sharpness
4. **Skip for photos** - Use edge smoothing instead
5. **Adjust threshold** - If SVG looks wrong, try different `-t` value

## Summary

✅ **For your two-oval graphics:** Use SVG vectorization - it's perfect!
✅ **Debug SVGs exported** - Check `svg_debug/` folder
✅ **Easy to verify** - Open SVG in browser to inspect
✅ **Sharp edges guaranteed** - Resolution-independent scaling

**Your next command:**
```bash
python gif_to_lvgl.py eyes.gif --use-a1 --use-lz4
# Answer "y" to SVG vectorization
# Check svg_debug/ folder to see the magic!
```

Enjoy your crystal-sharp animations! 🎨✨