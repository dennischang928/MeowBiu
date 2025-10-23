Here’s a clean, copy-pasteable README for your repo that explains exactly how to use script.py with lvgl_img_conv.py.

⸻

GIF/Video → LVGL C Arrays Pipeline

Convert animated GIFs or videos (MP4/MOV/…) into:
	•	Per-frame PNG images
	•	Per-frame LVGL .c image arrays (via lvgl_img_conv.py)
	•	A per-animation header <base>.h that declares every frame and exposes a null-terminated frame pointer array
	•	A project-level all_animations.h that includes every per-animation header

Tested with LVGL v9 image types in lvgl_img_conv.py.

⸻

1) Requirements
	•	Python 3.8+
	•	Python packages:

pip install pillow opencv-python pypng lz4 tqdm


	•	pngquant in your PATH (only needed if using indexed color modes inside the converter)
	•	macOS: brew install pngquant
	•	Ubuntu/Debian: sudo apt install pngquant
	•	Windows: download from https://pngquant.org and add to PATH

Videos: OpenCV needs a codec backend. If a video won’t open, install system codecs or try re-encoding with ffmpeg -i in.mp4 -vcodec libx264 -acodec aac out.mp4.

⸻

2) Files in This Repo
	•	script.py — the high-level batch pipeline (you run this)
	•	lvgl_img_conv.py — the PNG → LVGL converter (called by the script)

Make sure the constant at the top of script.py points to your converter:

CONVERTER = "python3 lvgl_img_conv.py"   # or an absolute path

On Windows you might prefer:

CONVERTER = "py lvgl_img_conv.py"


⸻

3) What the Pipeline Does

For each input file:
	1.	Create a folder named after the input base (e.g., spinner.gif → ./spinner/)
	2.	Extract frames
	•	GIF: all frames, preserves transparency
	•	Video: you’ll be asked how many frames to extract (evenly spaced)
	3.	(Optional) Resize: you’ll be asked once for a scale factor (e.g., 0.5)
	4.	Convert each PNG → .c by calling lvgl_img_conv.py
	•	Color format: default ARGB8888, or --use-a1 for A1 (1-bit alpha)
	•	Compression: default NONE, or --use-lz4 for LZ4
	5.	Write a per-animation header: <base>.h with:
	•	LV_IMAGE_DECLARE(<base>_frame_001); …
	•	static const lv_image_dsc_t * <base>_anim_frames[] = { &…, …, NULL };
	•	#define <BASE>_ANIM_FRAME_COUNT N
	6.	Write all_animations.h that #includes every <base>.h

⸻

4) Usage

Basic

python3 script.py <input1> <input2> ...

Examples:

# One GIF
python3 script.py spinner.gif

# One video
python3 script.py logo.mp4

# Multiple inputs at once
python3 script.py walk.gif jump.mp4 swirl.mov

Options
	•	--use-a1
Use LVGL A1 (1-bit alpha) instead of ARGB8888 for the generated .c frames.
	•	--use-lz4
Enable LZ4 compression in the generated .c frames.

Examples:

# 1-bit alpha + LZ4 compression
python3 script.py --use-a1 --use-lz4 spinner.gif logo.mp4

Interactive prompts (you’ll see these)
	•	Resize frames? (Y/n)
If yes, enter a scale like 0.5 (half-size) or 2.0 (double)
	•	For videos: “How many frames to extract?”
Press Enter for all frames, or enter a number (e.g., 24)

⸻

5) Outputs & Layout

For an input spinner.gif:

./spinner/
  spinner_frame_001.png
  spinner_frame_001.c
  spinner_frame_002.png
  spinner_frame_002.c
  ...
  spinner.h              # declares all frames + array + count
all_animations.h         # includes ./spinner/spinner.h (and others)

Symbols and filenames are sanitized to valid C identifiers:
	•	PNG base: <base>_frame_###.png
	•	C variable: same as PNG base, non-alnum chars → _

⸻

6) Using the Generated Headers in Your C Code (LVGL v9)

Include the master header once:

#include "all_animations.h"

Or a specific animation:

#include "spinner/spinner.h"

Typical usage:

// Frames and count from the header:
extern const lv_image_dsc_t* spinner_anim_frames[];  // null-terminated
#define SPINNER_ANIM_FRAME_COUNT  /* from spinner.h */

// Example: create an image object and change its source per frame:
void play_spinner(lv_obj_t *parent) {
    lv_obj_t *img = lv_image_create(parent);

    for (int i = 0; spinner_anim_frames[i] != NULL; ++i) {
        lv_image_set_src(img, spinner_anim_frames[i]);
        lv_timer_handler_run_in_period(33); // ~30 FPS demo; use your own timing
    }
}

The script prints a suggested FPS for each input based on original timing. Use that to time your animation in LVGL.

⸻

7) Advanced Notes & Tips
	•	Color format & compression
	•	Default is ARGB8888 with no compression for safest results.
	•	--use-a1 greatly reduces size for strictly monochrome-alpha assets (icons, masks).
	•	--use-lz4 enables LVGL’s compressed image flag; ensure your LVGL build supports it.
	•	Transparency & backgrounds
	•	GIF transparency is preserved when converting to ARGB8888.
	•	If you later choose non-alpha formats in lvgl_img_conv.py, it can pre-multiply against a background color.
	•	Video extraction
	•	Frames are evenly spaced over the full duration.
	•	If OpenCV reports 0 fps or frame count, the script still attempts extraction but the suggested FPS may be rough.
	•	Naming & C identifiers
	•	Symbols are derived from file names; leading digits receive a leading underscore to remain valid C.
	•	Where the .c files go
	•	All generated .c files are inside the per-input folder alongside the PNG frames.

⸻

8) Troubleshooting
	•	“Failed to open video”
Re-encode with FFmpeg:
ffmpeg -y -i in.mov -vcodec libx264 -pix_fmt yuv420p -an out.mp4
	•	“cannot find pngquant tool” (from lvgl_img_conv.py)
Install it and ensure it’s on PATH (see Requirements).
	•	CalledProcessError when running converter
Check CONVERTER path/command in script.py. Try absolute paths:

CONVERTER = "/usr/bin/python3 /full/path/to/lvgl_img_conv.py"


	•	LVGL include path issues in your C project
The generated .c uses #include "lvgl.h" patterns compatible with different include setups. Ensure your include paths match your LVGL integration.

⸻

9) Command Quick Reference

# Default (ARGB8888, no compression)
python3 script.py file.gif file.mp4

# Mono alpha A1 (1-bit) without compression
python3 script.py --use-a1 file.gif

# ARGB8888 + LZ4 compression
python3 script.py --use-lz4 file.mp4

# A1 + LZ4
python3 script.py --use-a1 --use-lz4 file.gif file.mov


⸻

10) License

Use at your own risk. Respect any licenses for source media, LVGL, and pngquant.

⸻

That’s it. Drop this README into your repo and you’re good to go. If you want, I can tailor a Makefile or a minimal C demo that plays a generated animation on LVGL.