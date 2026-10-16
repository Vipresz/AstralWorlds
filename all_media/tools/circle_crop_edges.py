#!/usr/bin/env python3
"""Mask everything outside detected edges and export tight PNG(s).

Pipeline:
  1. Detect outer edges (Canny on opaque images; alpha/silhouette as fallback)
  2. Close edge loops and fill regions; ignore nested inner loops and tiny specks
  3. Export one PNG per top-level region (separate windows stay separate)
  4. Fit/smooth the traced boundary (--fit), strip the outer rim (-t), crop
  5. Use --min-items / watershed to split touching sprite-sheet cells further

Region rules:
  - Nested edges (one inside another) -> keep only the outer boundary
  - Overlapping separate shapes -> outer boundary of each
  - Non-touching shapes -> one output file per shape

Examples:
  python circle_crop_edges.py ../menu/character/char_3_light.png
  python circle_crop_edges.py ../menu/character/
  python circle_crop_edges.py input.png --preview
  python circle_crop_edges.py input.png -t 6
  python circle_crop_edges.py input.png --fit 8
  python circle_crop_edges.py input.png --sensitivity 7
  python circle_crop_edges.py input.png --width 512
  python circle_crop_edges.py spritesheet.png --min-item-area 200
  python circle_crop_edges.py spritesheet.png --max-items 4
  python circle_crop_edges.py spritesheet.png --min-items 4 --max-items 4
  python circle_crop_edges.py input.png --single
  python circle_crop_edges.py input_folder/ --recursive
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tif", ".tiff"}
FIT_MAX = 10
FIT_DEFAULT = 5
SENSITIVITY_MAX = 10
SENSITIVITY_DEFAULT = 5
EDGE_SOURCES = ("auto", "alpha", "silhouette", "canny")
ALPHA_THRESHOLD = 128
# Silhouette mode uses border flood-fill; this is the flood lo/up diff at --sensitivity 5.
BG_TOLERANCE_DEFAULT = 8
FLOOD_TOLERANCE_MIN = 2
FLOOD_TOLERANCE_MAX = 14
CANNY_LOW_DEFAULT = 40
CANNY_HIGH_DEFAULT = 120
MIN_ITEM_AREA_DEFAULT = 64
MAX_ITEMS_DEFAULT = 0
MIN_ITEMS_DEFAULT = 0
WATERSHED_DIST_FRAC = 0.35
EDGE_CLOSE_ITERATIONS = 2
MIN_BBOX_DIMENSION_FLOOR = 20
MIN_BBOX_DIMENSION_FRAC = 0.02


def image_has_transparency(rgba: np.ndarray) -> bool:
    return bool(np.any(rgba[..., 3] < 255))


def resolve_edge_source(edge_source: str, rgba: np.ndarray) -> str:
    source = edge_source.lower()
    if source not in EDGE_SOURCES:
        raise ValueError(f"Unsupported edge source: {edge_source}")
    if source == "auto":
        return "alpha" if image_has_transparency(rgba) else "silhouette"
    return source


def resolve_contour_smooth_window(fit: int) -> int:
    """Gentler smoothing for silhouette/contour extraction."""
    fit = max(0, min(FIT_MAX, int(fit)))
    if fit == 0:
        return 0
    return max(3, min(15, (fit * 2 - 1) | 1))


def resolve_fit_parameters(fit: int) -> tuple[int, int]:
    """Map unified fit level to (polar_smooth_window, harmonic_poly_degree).

    0       — raw traced boundary, no fitting
    1–5     — increasing polar radius smoothing (removes jagged edge noise)
    6–10    — smoothing plus harmonic circle fit (stronger circular shape)
    """
    fit = max(0, min(FIT_MAX, int(fit)))
    if fit == 0:
        return 0, 0

    smooth_window = max(3, min(31, (fit * 3 - 1) | 1))
    poly_degree = 0 if fit < 6 else min(10, (fit - 5) * 2)
    return smooth_window, poly_degree


def resolve_sensitivity_parameters(sensitivity: int) -> tuple[int, int, int]:
    """Map unified sensitivity to (canny_low, canny_high, flood_tolerance).

    Higher values keep softer near-background fringe.
    Lower values flood further from the borders to eat drop shadows.
    Level 5 uses canny 40/120 and flood tolerance 8.
    """
    sensitivity = max(0, min(SENSITIVITY_MAX, int(sensitivity)))
    canny_low_floor = 180
    canny_high_floor = 350
    # Flood tolerance is inverted vs "sensitivity": low sens -> high flood.
    flood_floor = FLOOD_TOLERANCE_MAX
    flood_ceil = FLOOD_TOLERANCE_MIN
    if sensitivity <= SENSITIVITY_DEFAULT:
        t = sensitivity / float(SENSITIVITY_DEFAULT)
        canny_low = int(round(canny_low_floor + (CANNY_LOW_DEFAULT - canny_low_floor) * t))
        canny_high = int(round(canny_high_floor + (CANNY_HIGH_DEFAULT - canny_high_floor) * t))
        flood_tolerance = int(round(flood_floor + (BG_TOLERANCE_DEFAULT - flood_floor) * t))
    else:
        t = (sensitivity - SENSITIVITY_DEFAULT) / float(SENSITIVITY_MAX - SENSITIVITY_DEFAULT)
        canny_low = int(round(CANNY_LOW_DEFAULT + (10 - CANNY_LOW_DEFAULT) * t))
        canny_high = int(round(CANNY_HIGH_DEFAULT + (35 - CANNY_HIGH_DEFAULT) * t))
        flood_tolerance = int(round(BG_TOLERANCE_DEFAULT + (flood_ceil - BG_TOLERANCE_DEFAULT) * t))
    return canny_low, canny_high, max(1, flood_tolerance)


def estimate_background_color(rgba: np.ndarray) -> np.ndarray:
    height, width = rgba.shape[:2]
    sample_points = [
        (0, 0),
        (0, width - 1),
        (height - 1, 0),
        (height - 1, width - 1),
        (0, width // 2),
        (height - 1, width // 2),
        (height // 2, 0),
        (height // 2, width - 1),
    ]
    samples = np.array([rgba[y, x, :3] for y, x in sample_points], dtype=np.float32)
    return np.median(samples, axis=0)


def build_silhouette_mask_flood(rgba: np.ndarray, *, flood_tolerance: int) -> np.ndarray:
    """Mark background by flooding inward from the image borders.

    Soft drop shadows stay connected to the solid backdrop, so they are
    consumed as background. Opaque icon pixels stop the flood.
    """
    height, width = rgba.shape[:2]
    image = rgba[..., :3].copy()
    flood_mask = np.zeros((height + 2, width + 2), dtype=np.uint8)
    tolerance = max(1, int(flood_tolerance))
    lo_diff = (tolerance, tolerance, tolerance)
    up_diff = (tolerance, tolerance, tolerance)
    flags = int(cv2.FLOODFILL_MASK_ONLY) | (255 << 8)
    seeds = [
        (0, 0),
        (width - 1, 0),
        (0, height - 1),
        (width - 1, height - 1),
        (width // 2, 0),
        (width // 2, height - 1),
        (0, height // 2),
        (width - 1, height // 2),
    ]
    for seed_x, seed_y in seeds:
        cv2.floodFill(
            image,
            flood_mask,
            (seed_x, seed_y),
            (0, 0, 0),
            loDiff=lo_diff,
            upDiff=up_diff,
            flags=flags,
        )

    background = flood_mask[1:-1, 1:-1]
    return np.where(background > 0, 0, 255).astype(np.uint8)


def build_silhouette_mask_color_distance(rgba: np.ndarray, *, bg_tolerance: int) -> np.ndarray:
    background = estimate_background_color(rgba)
    rgb = rgba[..., :3].astype(np.float32)
    difference = np.linalg.norm(rgb - background, axis=2)
    return (difference > float(bg_tolerance)).astype(np.uint8) * 255


def build_foreground_mask(rgba: np.ndarray, *, bg_tolerance: int) -> np.ndarray:
    if image_has_transparency(rgba):
        mask = (rgba[..., 3] >= ALPHA_THRESHOLD).astype(np.uint8) * 255
    else:
        mask = build_silhouette_mask_flood(rgba, flood_tolerance=bg_tolerance)
        if not np.any(mask):
            # Fallback if flood ate everything (very aggressive tolerance / tiny icon).
            mask = build_silhouette_mask_color_distance(rgba, bg_tolerance=max(24, bg_tolerance))

    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
    return mask


def trace_contour_boundary(mask: np.ndarray) -> np.ndarray:
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    if not contours:
        return np.empty((0, 2), dtype=np.int32)
    best_contour = max(contours, key=cv2.contourArea)
    return best_contour.reshape(-1, 2).astype(np.int32)


def resolve_min_bbox_dimension(width: int, height: int) -> int:
    return max(MIN_BBOX_DIMENSION_FLOOR, int(min(width, height) * MIN_BBOX_DIMENSION_FRAC))


def _is_bbox_inside(
    inner: tuple[int, int, int, int],
    outer: tuple[int, int, int, int],
) -> bool:
    inner_x, inner_y, inner_w, inner_h = inner
    outer_x, outer_y, outer_w, outer_h = outer
    return (
        inner_x >= outer_x
        and inner_y >= outer_y
        and inner_x + inner_w <= outer_x + outer_w
        and inner_y + inner_h <= outer_y + outer_h
    )


def close_mask_gaps(mask: np.ndarray, width: int, height: int) -> np.ndarray:
    close_px = max(5, min(width, height) // 80) | 1
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (close_px, close_px))
    return cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)


def contour_to_mask(mask_shape: tuple[int, int], contour: np.ndarray) -> np.ndarray:
    region = np.zeros(mask_shape, dtype=np.uint8)
    cv2.drawContours(region, [contour], -1, 255, thickness=cv2.FILLED)
    return region


def pick_top_level_region_masks(mask: np.ndarray, min_area: int) -> list[np.ndarray]:
    """Return filled masks for each top-level outer region.

    Drops nested inner loops and thin decorative specks inside a larger window.
    """
    if not np.any(mask):
        return []

    height, width = mask.shape
    min_dim = resolve_min_bbox_dimension(width, height)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    candidates: list[tuple[float, np.ndarray]] = []
    for contour in contours:
        area = float(cv2.contourArea(contour))
        if area < float(min_area):
            continue
        _x, _y, box_w, box_h = cv2.boundingRect(contour)
        if min(box_w, box_h) < min_dim:
            continue
        candidates.append((area, contour))

    candidates.sort(key=lambda item: item[0], reverse=True)
    kept_contours: list[np.ndarray] = []
    for _area, contour in candidates:
        moments = cv2.moments(contour)
        if moments["m00"] <= 0.0:
            continue
        centroid = (
            float(moments["m10"] / moments["m00"]),
            float(moments["m01"] / moments["m00"]),
        )
        if any(cv2.pointPolygonTest(kept, centroid, False) >= 0 for kept in kept_contours):
            continue
        contour_box = cv2.boundingRect(contour)
        if any(_is_bbox_inside(contour_box, cv2.boundingRect(kept)) for kept in kept_contours):
            continue
        kept_contours.append(contour)

    masks = [contour_to_mask(mask.shape, contour) for contour in kept_contours]
    return sort_component_masks_by_position(masks)


def resolve_min_item_area(min_item_area: int, width: int, height: int) -> int:
    """Minimum connected-component area that counts as a separate item."""
    if min_item_area > 0:
        return int(min_item_area)
    image_area = max(1, width * height)
    return max(MIN_ITEM_AREA_DEFAULT, image_area // 4000)


def find_foreground_components(mask: np.ndarray, min_area: int) -> list[np.ndarray]:
    """Return per-item binary masks sorted by area (largest first)."""
    if not np.any(mask):
        return []

    num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(mask, connectivity=8)
    components: list[tuple[int, np.ndarray]] = []
    for label in range(1, num_labels):
        area = int(stats[label, cv2.CC_STAT_AREA])
        if area < min_area:
            continue
        component_mask = np.zeros_like(mask)
        component_mask[labels == label] = 255
        components.append((area, component_mask))

    components.sort(key=lambda item: item[0], reverse=True)
    return [component_mask for _, component_mask in components]


def sort_component_masks_by_position(masks: list[np.ndarray]) -> list[np.ndarray]:
    """Sort masks top-to-bottom, then left-to-right for stable sprite-sheet numbering."""

    def centroid_key(mask: np.ndarray) -> tuple[float, float]:
        moments = cv2.moments(mask, binaryImage=True)
        if moments["m00"] > 0.0:
            return (float(moments["m01"] / moments["m00"]), float(moments["m10"] / moments["m00"]))
        return (0.0, 0.0)

    return sorted(masks, key=centroid_key)


def split_mask_watershed(mask: np.ndarray, min_area: int) -> list[np.ndarray]:
    """Separate touching foreground blobs (e.g. sprite-sheet cells) via watershed."""
    if not np.any(mask):
        return []

    distance = cv2.distanceTransform(mask, cv2.DIST_L2, 5)
    peak = float(distance.max())
    if peak <= 0.0:
        return []

    _, sure_foreground = cv2.threshold(distance, WATERSHED_DIST_FRAC * peak, 255, 0)
    sure_foreground = np.uint8(sure_foreground)
    unknown = cv2.subtract(mask, sure_foreground)
    _, markers = cv2.connectedComponents(sure_foreground)
    markers = markers + 1
    markers[unknown == 255] = 0
    watershed_image = cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR)
    cv2.watershed(watershed_image, markers)

    components: list[np.ndarray] = []
    for marker_id in range(2, int(markers.max()) + 1):
        component_mask = np.zeros_like(mask)
        component_mask[(markers == marker_id) & (mask > 0)] = 255
        if int(np.count_nonzero(component_mask)) < min_area:
            continue
        components.append(component_mask)
    return sort_component_masks_by_position(components)


def refine_component_masks(
    mask: np.ndarray,
    min_area: int,
    min_items: int,
    *,
    initial_count: int,
) -> tuple[list[np.ndarray], str | None]:
    """Try harder splits when connected components find fewer regions than required."""
    if min_items <= 0 or initial_count >= min_items:
        return find_foreground_components(mask, min_area), None

    watershed_components = split_mask_watershed(mask, min_area)
    if len(watershed_components) >= min_items:
        return watershed_components, f"watershed split ({initial_count} -> {len(watershed_components)})"

    basic = find_foreground_components(mask, min_area)
    return basic, None


def build_foreground_mask_from_closed_edges(closed_edges: np.ndarray) -> np.ndarray:
    """Fill closed edge loops so each traced icon becomes its own component."""
    height, width = closed_edges.shape
    flood_base = np.zeros((height + 2, width + 2), dtype=np.uint8)
    filled = closed_edges.copy()
    cv2.floodFill(filled, flood_base, (0, 0), 255)
    holes = cv2.bitwise_not(filled)
    return cv2.bitwise_or(closed_edges, holes)


def estimate_mask_center(mask: np.ndarray, width: int, height: int) -> tuple[float, float]:
    moments = cv2.moments(mask, binaryImage=True)
    if moments["m00"] > 0.0:
        return float(moments["m10"] / moments["m00"]), float(moments["m01"] / moments["m00"])
    return width * 0.5, height * 0.5


def smooth_contour(boundary: np.ndarray, window: int) -> np.ndarray:
    if boundary.shape[0] < 8 or window < 3:
        return boundary

    points = boundary.astype(np.float64)
    count = points.shape[0]
    half = window // 2
    smoothed = np.zeros_like(points)
    for index in range(count):
        neighbors = points[(index + np.arange(-half, half + 1)) % count]
        smoothed[index] = neighbors.mean(axis=0)
    return smoothed.astype(np.int32)


def erode_mask(mask: np.ndarray, pixels: float) -> np.ndarray:
    if pixels <= 0.0:
        return mask
    iterations = max(1, int(round(pixels)))
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    return cv2.erode(mask, kernel, iterations=iterations)


def load_rgba(path: Path) -> np.ndarray:
    image = Image.open(path).convert("RGBA")
    return np.asarray(image)


def save_rgba(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, mode="RGBA").save(path, format="PNG")


def default_output_path(input_path: Path, suffix: str, item_index: int | None = None) -> Path:
    if item_index is None:
        return input_path.with_name(f"{input_path.stem}{suffix}{input_path.suffix}")
    return input_path.with_name(f"{input_path.stem}_{item_index:02d}{suffix}{input_path.suffix}")


def item_output_paths(base_output: Path, item_count: int) -> list[Path]:
    if item_count <= 1:
        return [base_output]
    stem = base_output.stem
    suffix = base_output.suffix or ".png"
    parent = base_output.parent
    return [parent / f"{stem}_{index:02d}{suffix}" for index in range(1, item_count + 1)]


def processed_output_dir(input_dir: Path) -> Path:
    return input_dir.parent / f"{input_dir.name}_processed"


def is_image_file(path: Path) -> bool:
    return path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS


def collect_images(input_dir: Path, *, recursive: bool) -> list[Path]:
    if recursive:
        files = [path for path in input_dir.rglob("*") if is_image_file(path)]
    else:
        files = [path for path in input_dir.iterdir() if is_image_file(path)]
    return sorted(files)


def batch_output_path(
    input_file: Path,
    input_root: Path,
    output_root: Path,
    *,
    suffix: str,
    item_index: int | None = None,
) -> Path:
    relative = input_file.relative_to(input_root)
    if item_index is not None:
        output_name = f"{relative.stem}_{item_index:02d}.png"
    elif suffix:
        output_name = f"{relative.stem}{suffix}.png"
    else:
        output_name = f"{relative.stem}.png"
    return output_root / relative.parent / output_name


def build_grayscale(rgba: np.ndarray) -> np.ndarray:
    rgb = rgba[..., :3].astype(np.float32)
    alpha = rgba[..., 3].astype(np.float32) / 255.0
    luminance = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]
    return np.clip(luminance * np.maximum(alpha, 0.15), 0, 255).astype(np.uint8)


def detect_edges(
    gray: np.ndarray,
    *,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
) -> np.ndarray:
    kernel_size = blur | 1
    blurred = cv2.GaussianBlur(gray, (kernel_size, kernel_size), 0)
    edges = cv2.Canny(blurred, canny_low, canny_high)

    if use_gradient:
        gradient_x = cv2.Sobel(blurred, cv2.CV_32F, 1, 0, ksize=3)
        gradient_y = cv2.Sobel(blurred, cv2.CV_32F, 0, 1, ksize=3)
        magnitude = cv2.magnitude(gradient_x, gradient_y)
        peak = float(magnitude.max())
        if peak > 1e-6:
            normalized = (magnitude / peak * 255.0).astype(np.uint8)
            _, gradient_edges = cv2.threshold(normalized, canny_high, 255, cv2.THRESH_BINARY)
            edges = cv2.bitwise_or(edges, gradient_edges)

    connect_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    return cv2.dilate(edges, connect_kernel, iterations=1)


def build_edge_region_mask(
    rgba: np.ndarray,
    *,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
    content_mask: np.ndarray | None = None,
    close_iterations: int = EDGE_CLOSE_ITERATIONS,
) -> np.ndarray:
    """Close detected edge loops and fill them to form separate regions."""
    gray = build_grayscale(rgba)
    edges = detect_edges(
        gray,
        canny_low=canny_low,
        canny_high=canny_high,
        blur=blur,
        use_gradient=use_gradient,
    )
    if content_mask is not None:
        edges = cv2.bitwise_and(edges, content_mask)

    close_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    closed = cv2.morphologyEx(edges, cv2.MORPH_CLOSE, close_kernel, iterations=close_iterations)
    return build_foreground_mask_from_closed_edges(closed)


def extract_region_masks(
    rgba: np.ndarray,
    *,
    edge_source: str,
    bg_tolerance: int,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
    min_area: int,
    min_items: int,
) -> tuple[list[np.ndarray], np.ndarray | None, str | None]:
    """Detect top-level outer regions from edges and/or silhouette."""
    resolved_source = resolve_edge_source(edge_source, rgba)
    height, width = rgba.shape[:2]
    content_mask = build_foreground_mask(rgba, bg_tolerance=bg_tolerance)
    shared_edges: np.ndarray | None = None
    region_masks: list[np.ndarray] = []
    split_note: str | None = None

    if edge_source in ("auto", "canny"):
        edge_regions = build_edge_region_mask(
            rgba,
            canny_low=canny_low,
            canny_high=canny_high,
            blur=blur,
            use_gradient=use_gradient,
            content_mask=content_mask if edge_source == "silhouette" else None,
        )
        gray = build_grayscale(rgba)
        shared_edges = detect_edges(
            gray,
            canny_low=canny_low,
            canny_high=canny_high,
            blur=blur,
            use_gradient=use_gradient,
        )
        region_masks = pick_top_level_region_masks(edge_regions, min_area)

    if not region_masks and resolved_source in ("auto", "alpha", "silhouette"):
        closed_content = close_mask_gaps(content_mask, width, height)
        region_masks = pick_top_level_region_masks(closed_content, min_area)

    if min_items > 0 and len(region_masks) < min_items:
        initial_count = len(region_masks) or len(find_foreground_components(content_mask, min_area))
        refined_masks, split_note = refine_component_masks(
            content_mask,
            min_area,
            min_items,
            initial_count=initial_count,
        )
        if len(refined_masks) >= min_items:
            region_masks = refined_masks

    return region_masks, shared_edges, split_note


def boundary_polar_coords(
    boundary: np.ndarray,
    center_x: float,
    center_y: float,
) -> tuple[np.ndarray, np.ndarray]:
    vectors = boundary.astype(np.float64) - np.array([center_x, center_y], dtype=np.float64)
    angles = np.arctan2(vectors[:, 1], vectors[:, 0])
    radii = np.linalg.norm(vectors, axis=1)
    order = np.argsort(angles)
    return angles[order], radii[order]


def build_harmonic_design_matrix(angles: np.ndarray, degree: int) -> np.ndarray:
    """Design matrix for r(theta) = a0 + sum_k (a_k cos(k*theta) + b_k sin(k*theta))."""
    columns = [np.ones_like(angles)]
    for harmonic in range(1, degree + 1):
        columns.append(np.cos(harmonic * angles))
        columns.append(np.sin(harmonic * angles))
    return np.column_stack(columns)


def fit_harmonic_radius(angles: np.ndarray, radii: np.ndarray, degree: int) -> np.ndarray:
    design = build_harmonic_design_matrix(angles, degree)
    coefficients, _, _, _ = np.linalg.lstsq(design, radii, rcond=None)
    return coefficients


def eval_harmonic_radius(angles: np.ndarray, coefficients: np.ndarray, degree: int) -> np.ndarray:
    radius = np.full_like(angles, coefficients[0], dtype=np.float64)
    index = 1
    for harmonic in range(1, degree + 1):
        radius += coefficients[index] * np.cos(harmonic * angles)
        radius += coefficients[index + 1] * np.sin(harmonic * angles)
        index += 2
    return radius


def smooth_boundary_polynomial(
    boundary: np.ndarray,
    center_x: float,
    center_y: float,
    *,
    degree: int,
    num_samples: int,
) -> np.ndarray:
    """Fit a trigonometric polynomial to radius(theta) and resample the boundary."""
    if degree <= 0 or boundary.shape[0] < 8:
        return boundary

    minimum_points = 2 * degree + 1
    if boundary.shape[0] < minimum_points:
        degree = max(1, boundary.shape[0] // 2 - 1)

    angles, radii = boundary_polar_coords(boundary, center_x, center_y)
    coefficients = fit_harmonic_radius(angles, radii, degree)

    sample_count = max(num_samples, minimum_points)
    sample_angles = np.linspace(-np.pi, np.pi, sample_count, endpoint=False)
    fitted_radii = np.maximum(eval_harmonic_radius(sample_angles, coefficients, degree), 1.0)

    x = center_x + fitted_radii * np.cos(sample_angles)
    y = center_y + fitted_radii * np.sin(sample_angles)
    return np.column_stack([x, y]).astype(np.int32)


def fit_boundary(
    boundary: np.ndarray,
    center_x: float,
    center_y: float,
    *,
    fit: int,
    num_samples: int,
) -> np.ndarray:
    smooth_window = resolve_contour_smooth_window(fit)
    fitted = boundary
    if smooth_window >= 3:
        fitted = smooth_contour(fitted, smooth_window)

    _smooth_window, poly_degree = resolve_fit_parameters(fit)
    if poly_degree > 0:
        fitted = smooth_boundary_polynomial(
            fitted,
            center_x,
            center_y,
            degree=poly_degree,
            num_samples=num_samples,
        )
    return fitted


@dataclass
class OuterEdgeDetection:
    edges: np.ndarray | None
    foreground_mask: np.ndarray
    raw_boundary: np.ndarray
    outer_boundary: np.ndarray
    center_x: float
    center_y: float
    edge_source: str
    item_index: int = 0
    item_count: int = 1


@dataclass
class ProcessedItem:
    cropped: np.ndarray
    detection: OuterEdgeDetection
    raw_boundary: np.ndarray
    fitted_boundary: np.ndarray
    cut_boundary: np.ndarray


def detect_outer_edge(
    rgba: np.ndarray,
    *,
    region_mask: np.ndarray,
    edge_source: str,
    edges: np.ndarray | None,
    num_rays: int,
    fit: int,
    item_index: int = 0,
    item_count: int = 1,
) -> OuterEdgeDetection:
    resolved_source = resolve_edge_source(edge_source, rgba)
    height, width = rgba.shape[:2]
    center_x, center_y = estimate_mask_center(region_mask, width, height)
    boundary = trace_contour_boundary(region_mask)
    raw_boundary = boundary.copy()
    fitted_boundary = fit_boundary(
        boundary,
        center_x,
        center_y,
        fit=fit,
        num_samples=num_rays,
    )
    return OuterEdgeDetection(
        edges=edges,
        foreground_mask=region_mask,
        raw_boundary=raw_boundary,
        outer_boundary=fitted_boundary,
        center_x=center_x,
        center_y=center_y,
        edge_source=resolved_source,
        item_index=item_index,
        item_count=item_count,
    )


def detect_all_items(
    rgba: np.ndarray,
    *,
    edge_source: str,
    bg_tolerance: int,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
    num_rays: int,
    fit: int,
    min_item_area: int,
    min_items: int,
    max_items: int,
) -> list[OuterEdgeDetection]:
    """Detect and trace the outer boundary of each top-level region."""
    height, width = rgba.shape[:2]
    min_area = resolve_min_item_area(min_item_area, width, height)
    region_masks, shared_edges, split_note = extract_region_masks(
        rgba,
        edge_source=edge_source,
        bg_tolerance=bg_tolerance,
        canny_low=canny_low,
        canny_high=canny_high,
        blur=blur,
        use_gradient=use_gradient,
        min_area=min_area,
        min_items=min_items,
    )

    if split_note:
        print(f"Refined item split: {split_note}", file=sys.stderr)

    detected_count = len(region_masks)
    if max_items > 0:
        region_masks = region_masks[:max_items]

    if min_items > 0 and detected_count < min_items:
        raise ValueError(
            f"Expected at least {min_items} item(s), detected {detected_count}"
        )

    if not region_masks:
        empty_mask = np.zeros((height, width), dtype=np.uint8)
        return [
            detect_outer_edge(
                rgba,
                region_mask=empty_mask,
                edge_source=edge_source,
                edges=shared_edges,
                num_rays=num_rays,
                fit=fit,
            )
        ]

    item_count = len(region_masks)
    return [
        detect_outer_edge(
            rgba,
            region_mask=region_mask,
            edge_source=edge_source,
            edges=shared_edges,
            num_rays=num_rays,
            fit=fit,
            item_index=index,
            item_count=item_count,
        )
        for index, region_mask in enumerate(region_masks)
    ]


def render_crop(
    rgba: np.ndarray,
    detection: OuterEdgeDetection,
    *,
    fit: int,
    strip_thickness: float,
    shrink: int,
    expand: int,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    height, width = rgba.shape[:2]

    if fit <= 0 and detection.foreground_mask is not None:
        mask = detection.foreground_mask.copy()
    elif detection.outer_boundary.shape[0] >= 3:
        mask = boundary_mask(
            (height, width),
            detection.outer_boundary,
            shrink=0,
            expand=0,
        )
    else:
        mask = detection.foreground_mask.copy()

    mask = erode_mask(mask, strip_thickness)
    if expand > 0:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.dilate(mask, kernel, iterations=expand)
    if shrink > 0:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.erode(mask, kernel, iterations=shrink)

    cut_boundary = trace_contour_boundary(mask)
    masked = apply_mask(rgba, mask)
    cropped = crop_to_opaque_bounds(masked)
    return cropped, detection.raw_boundary, detection.outer_boundary, cut_boundary


def boundary_mask(
    shape: tuple[int, int],
    boundary: np.ndarray,
    *,
    shrink: int,
    expand: int,
) -> np.ndarray:
    height, width = shape
    mask = np.zeros((height, width), dtype=np.uint8)
    if boundary.shape[0] < 3:
        return mask

    cv2.fillPoly(mask, [boundary], 255)

    if expand > 0:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.dilate(mask, kernel, iterations=expand)
    if shrink > 0:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.erode(mask, kernel, iterations=shrink)

    return mask


def apply_mask(rgba: np.ndarray, mask: np.ndarray) -> np.ndarray:
    output = rgba.copy()
    output[..., 3] = np.where(mask > 0, output[..., 3], 0).astype(np.uint8)
    return output


def crop_to_opaque_bounds(rgba: np.ndarray) -> np.ndarray:
    alpha = rgba[..., 3]
    opaque = alpha > 0
    if not np.any(opaque):
        return rgba

    ys, xs = np.where(opaque)
    top = int(ys.min())
    bottom = int(ys.max()) + 1
    left = int(xs.min())
    right = int(xs.max()) + 1
    return rgba[top:bottom, left:right]


def resize_to_width(rgba: np.ndarray, target_width: int) -> np.ndarray:
    """Resize to the target width while preserving aspect ratio."""
    if target_width <= 0:
        return rgba

    height, width = rgba.shape[:2]
    if width == target_width:
        return rgba

    target_height = max(1, int(round(height * (target_width / float(width)))))
    image = Image.fromarray(rgba, mode="RGBA")
    resized = image.resize((target_width, target_height), Image.Resampling.LANCZOS)
    return np.asarray(resized)


def process_item(
    rgba: np.ndarray,
    detection: OuterEdgeDetection,
    *,
    fit: int,
    strip_thickness: float,
    shrink: int,
    expand: int,
) -> ProcessedItem:
    source = rgba
    if detection.item_count > 1 and detection.foreground_mask is not None:
        source = rgba.copy()
        source[..., 3] = np.where(
            detection.foreground_mask > 0,
            source[..., 3],
            0,
        ).astype(np.uint8)

    cropped, raw_boundary, fitted_boundary, cut_boundary = render_crop(
        source,
        detection,
        fit=fit,
        strip_thickness=strip_thickness,
        shrink=shrink,
        expand=expand,
    )
    return ProcessedItem(
        cropped=cropped,
        detection=detection,
        raw_boundary=raw_boundary,
        fitted_boundary=fitted_boundary,
        cut_boundary=cut_boundary,
    )


def process_image(
    rgba: np.ndarray,
    *,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
    num_rays: int,
    fit: int,
    strip_thickness: float,
    shrink: int,
    expand: int,
    edge_source: str,
    bg_tolerance: int,
    min_item_area: int,
    min_items: int,
    max_items: int,
) -> tuple[list[ProcessedItem], np.ndarray | None]:
    detections = detect_all_items(
        rgba,
        edge_source=edge_source,
        bg_tolerance=bg_tolerance,
        canny_low=canny_low,
        canny_high=canny_high,
        blur=blur,
        use_gradient=use_gradient,
        num_rays=num_rays,
        fit=fit,
        min_item_area=min_item_area,
        min_items=min_items,
        max_items=max_items,
    )
    shared_edges = detections[0].edges if detections else None
    processed = [
        process_item(
            rgba,
            detection,
            fit=fit,
            strip_thickness=strip_thickness,
            shrink=shrink,
            expand=expand,
        )
        for detection in detections
    ]
    return processed, shared_edges


def resolve_item_limits(args: argparse.Namespace) -> tuple[int, int]:
    """Return (min_items, max_items) from CLI args."""
    max_items = 1 if args.single else max(0, int(args.max_items))
    min_items = max(0, int(args.min_items))
    if min_items > 0 and max_items > 0 and min_items > max_items:
        raise ValueError(
            f"--min-items ({min_items}) cannot exceed --max-items ({max_items})"
        )
    return min_items, max_items


def resolve_edge_detection_args(args: argparse.Namespace) -> tuple[int, int, int, int]:
    """Return (sensitivity, canny_low, canny_high, bg_tolerance) from CLI args."""
    sensitivity = max(0, min(SENSITIVITY_MAX, int(args.sensitivity)))
    canny_low, canny_high, bg_tolerance = resolve_sensitivity_parameters(sensitivity)
    if args.canny_low is not None:
        canny_low = int(args.canny_low)
    if args.canny_high is not None:
        canny_high = int(args.canny_high)
    if args.bg_tolerance is not None:
        bg_tolerance = max(1, int(args.bg_tolerance))
    return sensitivity, canny_low, canny_high, bg_tolerance


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Detect outer edges, mask everything outside to transparency, and crop tightly. "
            "Automatically exports one PNG per top-level region."
        )
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Source image file or folder of images",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output PNG path for a single file, or output folder when processing a directory",
    )
    parser.add_argument(
        "--suffix",
        default="_circle",
        help="Filename suffix for single-file mode (default: _circle). Ignored for folder batch output.",
    )
    parser.add_argument(
        "--recursive",
        action="store_true",
        help="When input is a folder, include images in subfolders (preserves structure in output)",
    )
    parser.add_argument(
        "--min-item-area",
        type=int,
        default=0,
        metavar="PX",
        help=(
            "Minimum pixel area for a detected region to count as a separate item "
            f"(0=auto, floor {MIN_ITEM_AREA_DEFAULT}px). "
            "Raise this to ignore specks; lower it to split tightly packed icons."
        ),
    )
    parser.add_argument(
        "--min-items",
        type=int,
        default=MIN_ITEMS_DEFAULT,
        metavar="N",
        help=(
            "Require at least N detected regions before exporting "
            f"(0=no requirement, default {MIN_ITEMS_DEFAULT}). "
            "If connected components find fewer regions, a watershed pass "
            "is used to split touching sprite-sheet cells."
        ),
    )
    parser.add_argument(
        "--max-items",
        type=int,
        default=MAX_ITEMS_DEFAULT,
        metavar="N",
        help=(
            "Maximum number of detected regions to export "
            f"(0=all detected regions, default {MAX_ITEMS_DEFAULT}). "
            "Keeps the largest regions first. Same as --single when N=1."
        ),
    )
    parser.add_argument(
        "--single",
        action="store_true",
        help="Only process the largest detected item (same as --max-items 1)",
    )
    parser.add_argument(
        "--edge-source",
        choices=EDGE_SOURCES,
        default="auto",
        help=(
            "How to detect regions. auto=Canny edge loops on opaque images and "
            "alpha/silhouette fallback; alpha/silhouette/canny force a single mode."
        ),
    )
    parser.add_argument(
        "--sensitivity",
        type=int,
        default=SENSITIVITY_DEFAULT,
        metavar="N",
        help=(
            f"Edge detection sensitivity (0-{SENSITIVITY_MAX}, default {SENSITIVITY_DEFAULT}). "
            "Lower floods more of the soft drop shadow into the background. "
            "Higher keeps softer near-bg fringe. "
            "Sets canny thresholds and --bg-tolerance unless those are given explicitly. "
            "Also adjustable in --preview."
        ),
    )
    parser.add_argument(
        "--bg-tolerance",
        type=int,
        default=None,
        help=(
            "Silhouette background flood-fill tolerance "
            f"(default from --sensitivity; typical {FLOOD_TOLERANCE_MIN}-{FLOOD_TOLERANCE_MAX}, "
            f"default level uses {BG_TOLERANCE_DEFAULT}). "
            "Higher eats more soft shadow; too high can nibble the icon edge."
        ),
    )
    parser.add_argument(
        "--canny-low",
        type=int,
        default=None,
        help=f"Canny lower threshold (default from --sensitivity; historical default {CANNY_LOW_DEFAULT})",
    )
    parser.add_argument(
        "--canny-high",
        type=int,
        default=None,
        help=f"Canny upper threshold (default from --sensitivity; historical default {CANNY_HIGH_DEFAULT})",
    )
    parser.add_argument("--blur", type=int, default=5, help="Gaussian blur kernel size before edge detect")
    parser.add_argument(
        "--no-gradient",
        action="store_true",
        help="Disable Sobel gradient edges (Canny only)",
    )
    parser.add_argument(
        "--rays",
        type=int,
        default=720,
        help="Number of boundary samples used for circular fitting (--fit 6+)",
    )
    parser.add_argument(
        "--fit",
        type=int,
        default=FIT_DEFAULT,
        metavar="N",
        help=(
            f"Boundary noise reduction and circular fit strength (0-{FIT_MAX}, default {FIT_DEFAULT}). "
            "0=raw trace, 1-5=polar smoothing only, 6-10=adds harmonic circle fit. "
            "Try 7-8 for circular icons."
        ),
    )
    parser.add_argument(
        "-t",
        "--strip-thickness",
        type=float,
        default=0.0,
        help="Pixels of detected outer edge rim to remove inward before masking",
    )
    parser.add_argument(
        "--preview",
        action="store_true",
        help=(
            "Open an interactive window with sliders for sensitivity and strip thickness "
            "(S=save, Q=quit)"
        ),
    )
    parser.add_argument(
        "--shrink",
        type=int,
        default=0,
        help="Extra erosion after strip-thickness (fine tune in pixels)",
    )
    parser.add_argument(
        "--expand",
        type=int,
        default=0,
        help="Dilate the kept region outward by this many pixels after masking",
    )
    parser.add_argument(
        "--width",
        type=int,
        default=0,
        metavar="PX",
        help="Resize output to this width in pixels (preserves aspect ratio). 0=keep cropped size.",
    )
    parser.add_argument("--debug-edges", type=Path, help="Save the edge map PNG")
    parser.add_argument(
        "--debug-boundary",
        type=Path,
        help="Save the traced outer boundary overlaid on the source image",
    )
    return parser.parse_args(argv)


def draw_boundary_ring(
    canvas: np.ndarray,
    boundary: np.ndarray,
    color: tuple[int, int, int, int],
    *,
    thickness: int = 1,
) -> None:
    if boundary.shape[0] < 2:
        return
    closed = np.vstack([boundary, boundary[:1]])
    for index in range(1, closed.shape[0]):
        x0, y0 = int(closed[index - 1, 0]), int(closed[index - 1, 1])
        x1, y1 = int(closed[index, 0]), int(closed[index, 1])
        cv2.line(canvas, (x0, y0), (x1, y1), color, thickness, cv2.LINE_AA)


def save_debug_boundary(
    path: Path,
    rgba: np.ndarray,
    processed_items: list[ProcessedItem],
) -> None:
    overlay = rgba.copy()
    colors = [
        (0, 255, 0, 255),
        (255, 180, 0, 255),
        (0, 200, 255, 255),
        (255, 80, 180, 255),
        (180, 255, 80, 255),
        (80, 180, 255, 255),
    ]
    for item_index, item in enumerate(processed_items):
        color = colors[item_index % len(colors)]
        draw_boundary_ring(overlay, item.raw_boundary, color, thickness=1)
        draw_boundary_ring(overlay, item.fitted_boundary, color, thickness=1)
        draw_boundary_ring(overlay, item.cut_boundary, color, thickness=2)
        if item.cut_boundary.shape[0] > 0:
            label_x = int(item.cut_boundary[:, 0].mean())
            label_y = int(item.cut_boundary[:, 1].min()) - 4
            cv2.putText(
                overlay,
                f"{item_index + 1}",
                (label_x, max(12, label_y)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.55,
                color,
                2,
                cv2.LINE_AA,
            )
    save_rgba(path, overlay)


def checkerboard(size: tuple[int, int], tile: int = 12) -> np.ndarray:
    height, width = size
    grid_y, grid_x = np.indices((height, width))
    checker = ((grid_x // tile) + (grid_y // tile)) % 2
    base = np.where(checker, 210, 170).astype(np.uint8)
    return np.dstack([base, base, base, np.full((height, width), 255, dtype=np.uint8)])


def composite_on_checker(rgba: np.ndarray) -> np.ndarray:
    background = checkerboard(rgba.shape[:2])
    alpha = rgba[..., 3:4].astype(np.float32) / 255.0
    blended = rgba.astype(np.float32) * alpha + background.astype(np.float32) * (1.0 - alpha)
    return blended.astype(np.uint8)


def build_preview_frame(
    rgba: np.ndarray,
    processed_items: list[ProcessedItem],
    *,
    sensitivity: int,
    strip_thickness: float,
) -> np.ndarray:
    overlay = rgba.copy()
    colors = [
        (0, 255, 0, 255),
        (255, 180, 0, 255),
        (0, 200, 255, 255),
        (255, 80, 180, 255),
        (180, 255, 80, 255),
        (80, 180, 255, 255),
    ]
    for item_index, item in enumerate(processed_items):
        color = colors[item_index % len(colors)]
        draw_boundary_ring(overlay, item.raw_boundary, color, thickness=1)
        draw_boundary_ring(overlay, item.fitted_boundary, color, thickness=1)
        draw_boundary_ring(overlay, item.cut_boundary, color, thickness=2)

    left = composite_on_checker(overlay)
    preview_item = processed_items[0]
    right = composite_on_checker(preview_item.cropped)

    gap = 12
    header = 36
    width = left.shape[1] + gap + right.shape[1]
    height = max(left.shape[0], right.shape[0]) + header
    frame = np.zeros((height, width, 3), dtype=np.uint8)
    frame[:] = (32, 32, 36)

    left_bgr = cv2.cvtColor(left, cv2.COLOR_RGBA2BGR)
    right_bgr = cv2.cvtColor(right, cv2.COLOR_RGBA2BGR)
    frame[header : header + left.shape[0], : left.shape[1]] = left_bgr
    frame[header : header + right.shape[0], left.shape[1] + gap :] = right_bgr

    label = (
        f"items={len(processed_items)}  sens={sensitivity}  strip={strip_thickness:.0f}px  "
        f"left=all traces  right=item 1 crop  |  S save  Q quit"
    )
    cv2.putText(frame, label, (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (230, 230, 230), 1, cv2.LINE_AA)
    return frame


def interactive_strip_preview(
    rgba: np.ndarray,
    *,
    edge_source: str,
    blur: int,
    use_gradient: bool,
    num_rays: int,
    fit: int,
    initial_sensitivity: int,
    initial_strip: float,
    shrink: int,
    expand: int,
    canny_low_override: int | None,
    canny_high_override: int | None,
    bg_tolerance_override: int | None,
    min_item_area: int,
    min_items: int,
    max_items: int,
) -> tuple[int, float] | None:
    window = "Edge crop preview"
    max_strip = max(40, min(rgba.shape[:2]) // 3)
    initial_sensitivity = max(0, min(SENSITIVITY_MAX, int(initial_sensitivity)))

    cv2.namedWindow(window, cv2.WINDOW_NORMAL)
    cv2.createTrackbar(
        "sensitivity",
        window,
        initial_sensitivity,
        SENSITIVITY_MAX,
        lambda _value: None,
    )
    cv2.createTrackbar(
        "strip thickness px",
        window,
        int(round(max(0.0, min(initial_strip, float(max_strip))))),
        max_strip,
        lambda _value: None,
    )

    selected_sensitivity = initial_sensitivity
    selected_strip = float(initial_strip)
    cached_sensitivity = -1
    processed_items: list[ProcessedItem] | None = None

    while True:
        sensitivity = int(cv2.getTrackbarPos("sensitivity", window))
        strip = float(cv2.getTrackbarPos("strip thickness px", window))

        if processed_items is None or sensitivity != cached_sensitivity:
            canny_low, canny_high, bg_tolerance = resolve_sensitivity_parameters(sensitivity)
            if canny_low_override is not None:
                canny_low = canny_low_override
            if canny_high_override is not None:
                canny_high = canny_high_override
            if bg_tolerance_override is not None:
                bg_tolerance = max(1, bg_tolerance_override)
            processed_items, _ = process_image(
                rgba,
                canny_low=canny_low,
                canny_high=canny_high,
                blur=blur,
                use_gradient=use_gradient,
                num_rays=num_rays,
                fit=fit,
                strip_thickness=strip,
                shrink=shrink,
                expand=expand,
                edge_source=edge_source,
                bg_tolerance=bg_tolerance,
                min_item_area=min_item_area,
                min_items=min_items,
                max_items=max_items,
            )
            cached_sensitivity = sensitivity
        else:
            detections = [item.detection for item in processed_items]
            processed_items = [
                process_item(
                    rgba,
                    detection,
                    fit=fit,
                    strip_thickness=strip,
                    shrink=shrink,
                    expand=expand,
                )
                for detection in detections
            ]

        frame = build_preview_frame(
            rgba,
            processed_items,
            sensitivity=sensitivity,
            strip_thickness=strip,
        )
        cv2.imshow(window, frame)

        key = cv2.waitKey(30) & 0xFF
        if key in (ord("q"), ord("Q"), 27):
            cv2.destroyWindow(window)
            return None
        if key in (ord("s"), ord("S"), 13):
            selected_sensitivity = sensitivity
            selected_strip = strip
            cv2.destroyWindow(window)
            return selected_sensitivity, selected_strip

    return None


def process_single_file(
    input_path: Path,
    output_path: Path,
    args: argparse.Namespace,
    *,
    fit: int,
    strip_thickness: float,
    canny_low: int,
    canny_high: int,
    bg_tolerance: int,
) -> int:
    min_items, max_items = resolve_item_limits(args)
    rgba = load_rgba(input_path)
    try:
        processed_items, shared_edges = process_image(
            rgba,
            canny_low=canny_low,
            canny_high=canny_high,
            blur=args.blur,
            use_gradient=not args.no_gradient,
            num_rays=max(64, args.rays),
            fit=fit,
            strip_thickness=strip_thickness,
            shrink=max(0, args.shrink),
            expand=max(0, args.expand),
            edge_source=args.edge_source,
            bg_tolerance=max(1, bg_tolerance),
            min_item_area=max(0, args.min_item_area),
            min_items=min_items,
            max_items=max_items,
        )
    except ValueError as error:
        print(f"Failed {input_path.name}: {error}", file=sys.stderr)
        return 1

    if args.debug_edges:
        if shared_edges is None:
            print("Debug edges skipped: current edge source does not build a Canny map.", file=sys.stderr)
        else:
            save_rgba(
                args.debug_edges.resolve(),
                np.dstack([shared_edges, shared_edges, shared_edges, np.full_like(shared_edges, 255)]),
            )
    if args.debug_boundary:
        save_debug_boundary(args.debug_boundary.resolve(), rgba, processed_items)

    output_paths = item_output_paths(output_path, len(processed_items))
    output_width = max(0, args.width)
    item_count = len(processed_items)
    edge_source = processed_items[0].detection.edge_source if processed_items else args.edge_source

    for item_index, (item, item_output_path) in enumerate(zip(processed_items, output_paths)):
        if item.raw_boundary.shape[0] < 3:
            print(
                f"Warning: outer edge boundary not found for {input_path.name} "
                f"(item {item_index + 1}/{item_count})",
                file=sys.stderr,
            )

        cropped = item.cropped
        if output_width > 0:
            cropped = resize_to_width(cropped, output_width)

        save_rgba(item_output_path, cropped)
        if item_count == 1:
            print(
                f"{input_path.name}: 1 item -> {item_output_path} "
                f"({cropped.shape[1]}x{cropped.shape[0]}, edge source: {edge_source})"
            )
        else:
            print(
                f"{input_path.name}: item {item_index + 1}/{item_count} -> {item_output_path} "
                f"({cropped.shape[1]}x{cropped.shape[0]})"
            )

    if item_count > 1:
        print(f"{input_path.name}: detected {item_count} item(s) (edge source: {edge_source})")

    return 0


def process_folder(input_dir: Path, args: argparse.Namespace) -> int:
    if args.preview:
        print("Preview mode is only supported for a single input file.", file=sys.stderr)
        return 1
    if args.debug_edges or args.debug_boundary:
        print("Debug outputs are only supported for a single input file.", file=sys.stderr)
        return 1
    if args.output and args.output.suffix:
        print("When processing a folder, --output must be a directory path.", file=sys.stderr)
        return 1

    images = collect_images(input_dir, recursive=args.recursive)
    if not images:
        print(f"No images found in {input_dir}", file=sys.stderr)
        return 1

    output_dir = args.output.resolve() if args.output else processed_output_dir(input_dir.resolve())
    output_dir.mkdir(parents=True, exist_ok=True)

    fit = max(0, min(FIT_MAX, args.fit))
    strip_thickness = max(0.0, args.strip_thickness)
    _, canny_low, canny_high, bg_tolerance = resolve_edge_detection_args(args)
    min_items, max_items = resolve_item_limits(args)

    print(f"Processing {len(images)} image(s) from {input_dir}")
    print(f"Output folder: {output_dir}")

    failures = 0
    for image_path in images:
        try:
            rgba = load_rgba(image_path)
            processed_items, _ = process_image(
                rgba,
                canny_low=canny_low,
                canny_high=canny_high,
                blur=args.blur,
                use_gradient=not args.no_gradient,
                num_rays=max(64, args.rays),
                fit=fit,
                strip_thickness=strip_thickness,
                shrink=max(0, args.shrink),
                expand=max(0, args.expand),
                edge_source=args.edge_source,
                bg_tolerance=max(1, bg_tolerance),
                min_item_area=max(0, args.min_item_area),
                min_items=min_items,
                max_items=max_items,
            )
            output_width = max(0, args.width)
            item_count = len(processed_items)
            for item_index, item in enumerate(processed_items, start=1):
                if item_count == 1:
                    output_path = batch_output_path(
                        image_path,
                        input_dir.resolve(),
                        output_dir,
                        suffix="",
                    )
                else:
                    output_path = batch_output_path(
                        image_path,
                        input_dir.resolve(),
                        output_dir,
                        suffix="",
                        item_index=item_index,
                    )
                cropped = item.cropped
                if output_width > 0:
                    cropped = resize_to_width(cropped, output_width)
                save_rgba(output_path, cropped)
                if item_count == 1:
                    print(
                        f"{image_path.name}: 1 item -> {output_path} "
                        f"({cropped.shape[1]}x{cropped.shape[0]})"
                    )
                else:
                    print(
                        f"{image_path.name}: item {item_index}/{item_count} -> {output_path} "
                        f"({cropped.shape[1]}x{cropped.shape[0]})"
                    )
            if item_count > 1:
                print(f"{image_path.name}: detected {item_count} item(s)")
        except Exception as error:  # noqa: BLE001 - batch tool should continue on failure
            failures += 1
            print(f"Failed {image_path.name}: {error}", file=sys.stderr)

    if failures:
        print(f"Done with {failures} failure(s).", file=sys.stderr)
        return 1

    print("Done.")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    try:
        min_items, max_items = resolve_item_limits(args)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    input_path = args.input.resolve()
    if not input_path.exists():
        print(f"Input not found: {input_path}", file=sys.stderr)
        return 1

    if input_path.is_dir():
        return process_folder(input_path, args)

    if not input_path.is_file():
        print(f"Input is not a file or directory: {input_path}", file=sys.stderr)
        return 1

    output_path = args.output.resolve() if args.output else default_output_path(input_path, args.suffix)
    fit = max(0, min(FIT_MAX, args.fit))
    sensitivity, canny_low, canny_high, bg_tolerance = resolve_edge_detection_args(args)

    rgba = load_rgba(input_path)
    strip_thickness = max(0.0, args.strip_thickness)
    if args.preview:
        picked = interactive_strip_preview(
            rgba,
            edge_source=args.edge_source,
            blur=args.blur,
            use_gradient=not args.no_gradient,
            num_rays=max(64, args.rays),
            fit=fit,
            initial_sensitivity=sensitivity,
            initial_strip=strip_thickness,
            shrink=max(0, args.shrink),
            expand=max(0, args.expand),
            canny_low_override=args.canny_low,
            canny_high_override=args.canny_high,
            bg_tolerance_override=args.bg_tolerance,
            min_item_area=max(0, args.min_item_area),
            min_items=min_items,
            max_items=max_items,
        )
        if picked is None:
            print("Preview cancelled.")
            return 0
        sensitivity, strip_thickness = picked
        canny_low, canny_high, bg_tolerance = resolve_sensitivity_parameters(sensitivity)
        if args.canny_low is not None:
            canny_low = int(args.canny_low)
        if args.canny_high is not None:
            canny_high = int(args.canny_high)
        if args.bg_tolerance is not None:
            bg_tolerance = max(1, int(args.bg_tolerance))
        print(f"Selected sensitivity: {sensitivity}")
        print(f"Selected strip thickness: {strip_thickness:.0f}px")

    status = process_single_file(
        input_path,
        output_path,
        args,
        fit=fit,
        strip_thickness=strip_thickness,
        canny_low=canny_low,
        canny_high=canny_high,
        bg_tolerance=bg_tolerance,
    )
    smooth_window, poly_degree = resolve_fit_parameters(fit)
    print(f"Fit: {fit} (smooth window {smooth_window}, harmonic degree {poly_degree})")
    print(
        f"Sensitivity: {sensitivity} "
        f"(canny {canny_low}/{canny_high}, flood-tolerance {bg_tolerance})"
    )
    print(f"Strip thickness: {strip_thickness:.1f}px")
    if min_items > 0:
        print(f"Min items: {min_items}")
    if max_items > 0:
        print(f"Max items: {max_items}")
    if args.width > 0:
        print(f"Output width: {args.width}px")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
