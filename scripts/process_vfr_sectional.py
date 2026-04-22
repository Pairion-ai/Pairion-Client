#!/usr/bin/env python3
"""
process_vfr_sectional.py — Download, crop, and tint the FAA VFR Sectional chart for the
ADS-B radar scene background.

Usage:
    python3 scripts/process_vfr_sectional.py

The script downloads the current Dallas-Ft Worth FAA VFR Sectional chart ZIP, crops it
to the Pairion ADS-B bounding box (8nm around home), applies a dark-blue HUD tint, and
saves the result to resources/adsb/vfr_sectional.png for embedding as a Qt resource.

FAA charts update every 56 days. Re-run this script after each cycle to keep the chart
current. Update FAA_ZIP_URL to the latest edition URL from:
    https://aeronav.faa.gov/visual/

Dependencies:
    pip3 install rasterio pillow

Bounding box (application.yml pairion.data.adsb):
    lamin: 33.681   lomin: -96.715
    lamax: 33.947   lomax: -96.449
"""

import os
import sys
import urllib.request
import zipfile
import tempfile

import rasterio
from rasterio.warp import transform_bounds, reproject, Resampling
from rasterio.transform import from_bounds
import numpy as np
from PIL import Image, ImageEnhance

# ── Configuration ──────────────────────────────────────────────────────────────

FAA_ZIP_URL  = "https://aeronav.faa.gov/visual/03-19-2026/sectional-files/Dallas-Ft_Worth.zip"
TIF_NAME     = "Dallas-Ft Worth SEC.tif"

# ADS-B bounding box — must match application.yml pairion.data.adsb
LOMIN, LAMIN = -96.715, 33.681   # west, south
LOMAX, LAMAX =  -96.449, 33.947  # east, north

OUTPUT_PATH  = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "resources", "adsb", "vfr_sectional.png"
)

# Output resolution — width in pixels (height derived from aspect ratio)
OUTPUT_WIDTH = 2048


def download_zip(url: str, dest: str) -> None:
    """Download the FAA sectional ZIP to dest, showing progress."""
    print(f"Downloading {url} ...")
    urllib.request.urlretrieve(url, dest)
    print(f"  → {os.path.getsize(dest) // (1024*1024)} MB downloaded")


def extract_tif(zip_path: str, tif_name: str, dest_dir: str) -> str:
    """Extract the named TIF from the ZIP and return its path."""
    print(f"Extracting {tif_name} ...")
    with zipfile.ZipFile(zip_path, "r") as z:
        z.extract(tif_name, dest_dir)
    return os.path.join(dest_dir, tif_name)


def warp_to_wgs84(src_path: str, dest_path: str) -> None:
    """Reproject the LCC GeoTIFF to WGS84 (EPSG:4326), cropped to the bounding box."""
    from rasterio.crs import CRS
    dst_crs = CRS.from_epsg(4326)

    with rasterio.open(src_path) as src:
        # Transform bounding box from WGS84 to source CRS for window extraction.
        src_bbox = transform_bounds(dst_crs, src.crs, LOMIN, LAMIN, LOMAX, LAMAX)
        left, bottom, right, top = src_bbox

        # Build the pixel window for the source crop.
        col_off = max(0, int((left   - src.bounds.left)   / (src.bounds.right - src.bounds.left)  * src.width))
        row_off = max(0, int((src.bounds.top  - top)      / (src.bounds.top   - src.bounds.bottom) * src.height))
        col_end = min(src.width,  int((right  - src.bounds.left)   / (src.bounds.right - src.bounds.left)  * src.width)  + 1)
        row_end = min(src.height, int((src.bounds.top - bottom)    / (src.bounds.top   - src.bounds.bottom) * src.height) + 1)

        window = rasterio.windows.Window(
            col_off=col_off, row_off=row_off,
            width=col_end - col_off, height=row_end - row_off
        )

        # Read the cropped source data.
        src_data = src.read(window=window)
        src_transform = src.window_transform(window)

        # Compute output dimensions preserving aspect ratio at OUTPUT_WIDTH.
        lon_span = LOMAX - LOMIN
        lat_span = LAMAX - LAMIN
        aspect   = lat_span / lon_span
        dst_width  = OUTPUT_WIDTH
        dst_height = int(OUTPUT_WIDTH * aspect)

        dst_transform = from_bounds(LOMIN, LAMIN, LOMAX, LAMAX, dst_width, dst_height)

        # Reproject to WGS84 with 3 output bands (expand palette to RGB).
        # The FAA sectional is palette-indexed (count=1, uint8). Expand to RGB.
        if src.count == 1 and src.colormap(1):
            colormap = src.colormap(1)
            palette  = np.zeros((256, 3), dtype=np.uint8)
            for idx, rgba in colormap.items():
                palette[idx] = rgba[:3]
            src_rgb = palette[src_data[0]]          # H×W×3
            # Transpose to 3×H×W for rasterio.
            src_bands = np.transpose(src_rgb, (2, 0, 1)).astype(np.uint8)
        else:
            src_bands = src_data

        dst_bands = np.zeros((3, dst_height, dst_width), dtype=np.uint8)

        for band_idx in range(3):
            reproject(
                source=src_bands[band_idx],
                destination=dst_bands[band_idx],
                src_transform=src_transform,
                src_crs=src.crs,
                dst_transform=dst_transform,
                dst_crs=dst_crs,
                resampling=Resampling.lanczos,
            )

        # Write intermediate WGS84 TIFF.
        profile = {
            "driver": "GTiff", "dtype": "uint8",
            "width": dst_width, "height": dst_height,
            "count": 3, "crs": dst_crs,
            "transform": dst_transform,
        }
        with rasterio.open(dest_path, "w", **profile) as dst:
            dst.write(dst_bands)

    print(f"  → Reprojected to WGS84: {dst_width}×{dst_height} px")


def apply_hud_tint(src_path: str, out_path: str) -> tuple[int, int]:
    """Apply dark-blue HUD tint to the reprojected PNG and save the result."""
    img = Image.open(src_path).convert("RGB")

    # Step 1: reduce brightness to ~35% of original.
    img = ImageEnhance.Brightness(img).enhance(0.35)

    # Step 2: desaturate partially — keeps chart features readable but muted.
    img = ImageEnhance.Color(img).enhance(0.55)

    # Step 3: blend with a deep navy overlay for the HUD tone.
    blue = Image.new("RGB", img.size, (8, 12, 38))
    img  = Image.blend(img, blue, 0.38)

    # Step 4: slight contrast boost so roads/airways remain legible.
    img = ImageEnhance.Contrast(img).enhance(1.25)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    img.save(out_path, "PNG", optimize=True)
    return img.size


def main() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        zip_path      = os.path.join(tmpdir, "sectional.zip")
        tif_path      = os.path.join(tmpdir, TIF_NAME)
        wgs84_tif     = os.path.join(tmpdir, "sectional_wgs84.tif")

        download_zip(FAA_ZIP_URL, zip_path)
        extract_tif(zip_path, TIF_NAME, tmpdir)
        warp_to_wgs84(tif_path, wgs84_tif)

        print(f"Applying HUD tint ...")
        w, h = apply_hud_tint(wgs84_tif, OUTPUT_PATH)
        print(f"  → Saved: {OUTPUT_PATH}")
        print(f"  → Dimensions: {w}×{h} px")
        print("Done.")


if __name__ == "__main__":
    main()
