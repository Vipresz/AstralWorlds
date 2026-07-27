#!/usr/bin/env python3
"""Mask everything outside the outermost detected edge and export a tight PNG.

Pipeline:
  1. Trace the true outer silhouette (alpha channel for transparent PNGs,
     or Canny edges for opaque images)
  2. Cast rays from the image center; along each ray, walk from the border
     inward to find the outer boundary at that angle
  3. Fit the traced boundary (--fit) to smooth noise and optionally enforce
     a circular shape
  4. Move the cut line inward by --strip-thickness to remove the outer rim
     (use --preview to pick the value interactively)
  5. Fill inside the cut line, make outside transparent, crop tightly
  6. Optionally resize to a target width (--width) while preserving aspect ratio

Examples:
  python circle_crop_edges.py ../menu/character/char_3_light.png
  python circle_crop_edges.py ../menu/character/
  python circle_crop_edges.py input.png --preview
  python circle_crop_edges.py input.png -t 6
  python circle_crop_edges.py input.png --fit 8
  python circle_crop_edges.py input.png --sensitivity 7
  python circle_crop_edges.py input.png --width 512
  python circle_crop_edges.py input_folder/ --recursive
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Callable
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
BOUNDARY_MODE_CONTOUR = "contour"
BOUNDARY_MODE_POLAR = "polar"


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


def default_output_path(input_path: Path, suffix: str) -> Path:
    return input_path.with_name(f"{input_path.stem}{suffix}{input_path.suffix}")


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
) -> Path:
    relative = input_file.relative_to(input_root)
    if suffix:
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


def ray_border_distance(
    center_x: float,
    center_y: float,
    cos_angle: float,
    sin_angle: float,
    width: int,
    height: int,
) -> float:
    candidates: list[float] = []
    if abs(cos_angle) > 1e-6:
        candidates.append((0.0 - center_x) / cos_angle)
        candidates.append((width - 1.0 - center_x) / cos_angle)
    if abs(sin_angle) > 1e-6:
        candidates.append((0.0 - center_y) / sin_angle)
        candidates.append((height - 1.0 - center_y) / sin_angle)

    positive = [distance for distance in candidates if distance > 0.0]
    if not positive:
        return float(min(width, height))
    return max(positive)


def trace_boundary_rays(
    center_x: float,
    center_y: float,
    width: int,
    height: int,
    *,
    num_rays: int,
    edge_step: float,
    is_hit: Callable[[int, int], bool],
    pick_outermost: bool,
) -> np.ndarray:
    """Cast rays from the image border inward and collect boundary hits."""
    points: list[tuple[int, int]] = []

    for angle in np.linspace(0.0, 2.0 * np.pi, num_rays, endpoint=False):
        cos_angle = float(np.cos(angle))
        sin_angle = float(np.sin(angle))
        start_distance = ray_border_distance(center_x, center_y, cos_angle, sin_angle, width, height)

        hit: tuple[int, int] | None = None
        best_distance = -1.0
        distance = start_distance
        while distance > 0.0:
            sample_x = int(round(center_x + distance * cos_angle))
            sample_y = int(round(center_y + distance * sin_angle))
            if 0 <= sample_x < width and 0 <= sample_y < height and is_hit(sample_x, sample_y):
                if pick_outermost:
                    if distance > best_distance:
                        best_distance = distance
                        hit = (sample_x, sample_y)
                else:
                    hit = (sample_x, sample_y)
                    break
            distance -= edge_step

        if hit is not None:
            points.append(hit)

    if len(points) < 8:
        return np.empty((0, 2), dtype=np.int32)
    return np.asarray(points, dtype=np.int32)


def trace_alpha_boundary(
    alpha: np.ndarray,
    center_x: float,
    center_y: float,
    *,
    num_rays: int,
    edge_step: float,
    threshold: int = ALPHA_THRESHOLD,
) -> np.ndarray:
    """Trace the outer silhouette using the alpha channel."""
    height, width = alpha.shape

    def is_opaque(sample_x: int, sample_y: int) -> bool:
        return int(alpha[sample_y, sample_x]) >= threshold

    return trace_boundary_rays(
        center_x,
        center_y,
        width,
        height,
        num_rays=num_rays,
        edge_step=edge_step,
        is_hit=is_opaque,
        pick_outermost=False,
    )


def trace_outer_boundary(
    edges: np.ndarray,
    center_x: float,
    center_y: float,
    *,
    num_rays: int,
    edge_step: float,
) -> np.ndarray:
    """Return Nx2 int32 polygon vertices on the outermost detected edge."""
    height, width = edges.shape

    def is_edge(sample_x: int, sample_y: int) -> bool:
        return int(edges[sample_y, sample_x]) > 0

    return trace_boundary_rays(
        center_x,
        center_y,
        width,
        height,
        num_rays=num_rays,
        edge_step=edge_step,
        is_hit=is_edge,
        pick_outermost=True,
    )


def smooth_boundary_polar(
    boundary: np.ndarray,
    center_x: float,
    center_y: float,
    *,
    window: int,
) -> np.ndarray:
    if boundary.shape[0] < 8 or window < 3:
        return boundary

    vectors = boundary.astype(np.float64) - np.array([center_x, center_y], dtype=np.float64)
    angles = np.arctan2(vectors[:, 1], vectors[:, 0])
    radii = np.linalg.norm(vectors, axis=1)

    order = np.argsort(angles)
    angles = angles[order]
    radii = radii[order]

    # Wrap radii for circular smoothing.
    padded = np.concatenate([radii, radii, radii])
    kernel = np.ones(window, dtype=np.float64) / float(window)
    smoothed = np.convolve(padded, kernel, mode="same")[len(radii) : 2 * len(radii)]

    x = center_x + smoothed * np.cos(angles)
    y = center_y + smoothed * np.sin(angles)
    return np.column_stack([x, y]).astype(np.int32)


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
    boundary_mode: str,
) -> np.ndarray:
    if boundary_mode == BOUNDARY_MODE_CONTOUR:
        smooth_window = resolve_contour_smooth_window(fit)
        if smooth_window >= 3:
            return smooth_contour(boundary, smooth_window)
        return boundary

    smooth_window, poly_degree = resolve_fit_parameters(fit)
    fitted = boundary
    if smooth_window >= 3:
        fitted = smooth_boundary_polar(
            fitted,
            center_x,
            center_y,
            window=smooth_window,
        )
    if poly_degree > 0:
        fitted = smooth_boundary_polynomial(
            fitted,
            center_x,
            center_y,
            degree=poly_degree,
            num_samples=num_samples,
        )
    return fitted


def offset_boundary_inward(
    boundary: np.ndarray,
    center_x: float,
    center_y: float,
    pixels: float,
) -> np.ndarray:
    """Move each boundary point toward the center by `pixels` (strip outer rim)."""
    if pixels <= 0.0 or boundary.shape[0] < 3:
        return boundary

    vectors = boundary.astype(np.float64) - np.array([center_x, center_y], dtype=np.float64)
    angles = np.arctan2(vectors[:, 1], vectors[:, 0])
    radii = np.maximum(np.linalg.norm(vectors, axis=1) - pixels, 1.0)
    x = center_x + radii * np.cos(angles)
    y = center_y + radii * np.sin(angles)
    return np.column_stack([x, y]).astype(np.int32)


@dataclass
class OuterEdgeDetection:
    edges: np.ndarray | None
    foreground_mask: np.ndarray | None
    raw_boundary: np.ndarray
    outer_boundary: np.ndarray
    center_x: float
    center_y: float
    edge_source: str
    boundary_mode: str


def resolve_boundary_mode(foreground_mask: np.ndarray) -> str:
    foreground = np.argwhere(foreground_mask > 0)
    if foreground.shape[0] == 0:
        return BOUNDARY_MODE_POLAR

    ys = foreground[:, 0]
    xs = foreground[:, 1]
    width = int(xs.max() - xs.min() + 1)
    height = int(ys.max() - ys.min() + 1)
    aspect = max(width, height) / max(1, min(width, height))
    if aspect > 1.35:
        return BOUNDARY_MODE_CONTOUR
    return BOUNDARY_MODE_POLAR


def trace_raw_boundary(
    rgba: np.ndarray,
    foreground_mask: np.ndarray | None,
    edges: np.ndarray | None,
    center_x: float,
    center_y: float,
    *,
    edge_source: str,
    num_rays: int,
    edge_step: float,
    boundary_mode: str,
) -> np.ndarray:
    if edge_source in ("alpha", "silhouette") and foreground_mask is not None:
        if boundary_mode == BOUNDARY_MODE_CONTOUR:
            return trace_contour_boundary(foreground_mask)
        return trace_alpha_boundary(
            foreground_mask,
            center_x,
            center_y,
            num_rays=num_rays,
            edge_step=edge_step,
            threshold=1,
        )
    if edge_source == "alpha":
        return trace_alpha_boundary(
            rgba[..., 3],
            center_x,
            center_y,
            num_rays=num_rays,
            edge_step=edge_step,
        )
    if edges is None:
        return np.empty((0, 2), dtype=np.int32)
    return trace_outer_boundary(
        edges,
        center_x,
        center_y,
        num_rays=num_rays,
        edge_step=edge_step,
    )


def detect_outer_edge(
    rgba: np.ndarray,
    *,
    edge_source: str,
    bg_tolerance: int,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
    num_rays: int,
    edge_step: float,
    fit: int,
    refine_center: bool,
) -> OuterEdgeDetection:
    resolved_source = resolve_edge_source(edge_source, rgba)
    edges: np.ndarray | None = None
    foreground_mask: np.ndarray | None = None
    boundary_mode = BOUNDARY_MODE_POLAR

    if resolved_source in ("alpha", "silhouette"):
        foreground_mask = build_foreground_mask(rgba, bg_tolerance=bg_tolerance)
        boundary_mode = resolve_boundary_mode(foreground_mask)
        height, width = rgba.shape[:2]
    elif resolved_source == "canny":
        gray = build_grayscale(rgba)
        edges = detect_edges(
            gray,
            canny_low=canny_low,
            canny_high=canny_high,
            blur=blur,
            use_gradient=use_gradient,
        )
        height, width = gray.shape
    else:
        height, width = rgba.shape[:2]

    center_x = width * 0.5
    center_y = height * 0.5

    boundary = trace_raw_boundary(
        rgba,
        foreground_mask,
        edges,
        center_x,
        center_y,
        edge_source=resolved_source,
        num_rays=num_rays,
        edge_step=edge_step,
        boundary_mode=boundary_mode,
    )
    if boundary.shape[0] < 8 and resolved_source == "canny" and edges is not None:
        boundary = outer_boundary_from_contours(edges)

    if refine_center and boundary.shape[0] >= 8 and boundary_mode == BOUNDARY_MODE_POLAR:
        center_x, center_y = estimate_center(boundary, width, height)
        boundary = trace_raw_boundary(
            rgba,
            foreground_mask,
            edges,
            center_x,
            center_y,
            edge_source=resolved_source,
            num_rays=num_rays,
            edge_step=edge_step,
            boundary_mode=boundary_mode,
        )
        if boundary.shape[0] < 8 and resolved_source == "canny" and edges is not None:
            boundary = outer_boundary_from_contours(edges)

    raw_boundary = boundary.copy()
    fitted_boundary = fit_boundary(
        boundary,
        center_x,
        center_y,
        fit=fit,
        num_samples=num_rays,
        boundary_mode=boundary_mode,
    )
    return OuterEdgeDetection(
        edges=edges,
        foreground_mask=foreground_mask,
        raw_boundary=raw_boundary,
        outer_boundary=fitted_boundary,
        center_x=center_x,
        center_y=center_y,
        edge_source=resolved_source,
        boundary_mode=boundary_mode,
    )


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

    if detection.boundary_mode == BOUNDARY_MODE_CONTOUR:
        if fit <= 0 and detection.foreground_mask is not None:
            mask = detection.foreground_mask.copy()
        elif detection.outer_boundary.shape[0] >= 3:
            mask = boundary_mask(
                (height, width),
                detection.outer_boundary,
                shrink=0,
                expand=0,
            )
        elif detection.foreground_mask is not None:
            mask = detection.foreground_mask.copy()
        else:
            mask = np.zeros((height, width), dtype=np.uint8)

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

    cut_boundary = offset_boundary_inward(
        detection.outer_boundary,
        detection.center_x,
        detection.center_y,
        strip_thickness,
    )
    mask = boundary_mask(
        (height, width),
        cut_boundary,
        shrink=shrink,
        expand=expand,
    )
    masked = apply_mask(rgba, mask)
    cropped = crop_to_opaque_bounds(masked)
    return cropped, detection.raw_boundary, detection.outer_boundary, cut_boundary


def outer_boundary_from_contours(edges: np.ndarray) -> np.ndarray:
    """Fallback: largest external contour on the edge map."""
    closed_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    closed = cv2.morphologyEx(edges, cv2.MORPH_CLOSE, closed_kernel, iterations=2)
    contours, _ = cv2.findContours(closed, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    if not contours:
        return np.empty((0, 2), dtype=np.int32)

    height, width = edges.shape
    center = np.array([width * 0.5, height * 0.5], dtype=np.float64)
    best_contour = max(contours, key=cv2.contourArea)
    points = best_contour.reshape(-1, 2)

    # Keep the outer half of the contour (farther from center than the median).
    distances = np.linalg.norm(points.astype(np.float64) - center, axis=1)
    threshold = np.percentile(distances, 55.0)
    outer = points[distances >= threshold]
    if outer.shape[0] < 8:
        outer = points
    return outer.astype(np.int32)


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


def estimate_center(boundary: np.ndarray, width: int, height: int) -> tuple[float, float]:
    if boundary.shape[0] == 0:
        return width * 0.5, height * 0.5
    centroid = boundary.mean(axis=0)
    return float(centroid[0]), float(centroid[1])


def process_image(
    rgba: np.ndarray,
    *,
    canny_low: int,
    canny_high: int,
    blur: int,
    use_gradient: bool,
    num_rays: int,
    edge_step: float,
    fit: int,
    strip_thickness: float,
    shrink: int,
    expand: int,
    refine_center: bool,
    edge_source: str,
    bg_tolerance: int,
) -> tuple[np.ndarray, np.ndarray | None, np.ndarray, np.ndarray, np.ndarray]:
    detection = detect_outer_edge(
        rgba,
        edge_source=edge_source,
        bg_tolerance=bg_tolerance,
        canny_low=canny_low,
        canny_high=canny_high,
        blur=blur,
        use_gradient=use_gradient,
        num_rays=num_rays,
        edge_step=edge_step,
        fit=fit,
        refine_center=refine_center,
    )
    cropped, raw_boundary, fitted_boundary, cut_boundary = render_crop(
        rgba,
        detection,
        fit=fit,
        strip_thickness=strip_thickness,
        shrink=shrink,
        expand=expand,
    )
    return cropped, detection.edges, raw_boundary, fitted_boundary, cut_boundary


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
            "Detect edges, trace the outermost boundary, mask outside to transparency, "
            "and crop tightly."
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
        "--edge-source",
        choices=EDGE_SOURCES,
        default="auto",
        help=(
            "How to detect the outer boundary. auto=alpha for transparent PNGs, "
            "silhouette for opaque images on a solid background, canny for edge-map mode."
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
        help="Number of radial samples used to trace the outer edge",
    )
    parser.add_argument(
        "--edge-step",
        type=float,
        default=1.0,
        help="Pixel step when marching along each ray toward the center",
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
    parser.add_argument(
        "--no-refine-center",
        action="store_true",
        help="Skip second pass after re-centering on the traced boundary",
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
    raw_boundary: np.ndarray,
    fitted_boundary: np.ndarray,
    cut_boundary: np.ndarray,
) -> None:
    overlay = rgba.copy()
    draw_boundary_ring(overlay, raw_boundary, (0, 255, 0, 255), thickness=1)
    draw_boundary_ring(overlay, fitted_boundary, (255, 180, 0, 255), thickness=1)
    draw_boundary_ring(overlay, cut_boundary, (0, 200, 255, 255), thickness=2)
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
    cropped: np.ndarray,
    raw_boundary: np.ndarray,
    fitted_boundary: np.ndarray,
    cut_boundary: np.ndarray,
    *,
    sensitivity: int,
    strip_thickness: float,
) -> np.ndarray:
    overlay = rgba.copy()
    draw_boundary_ring(overlay, raw_boundary, (0, 255, 0, 255), thickness=1)
    draw_boundary_ring(overlay, fitted_boundary, (255, 180, 0, 255), thickness=1)
    draw_boundary_ring(overlay, cut_boundary, (0, 200, 255, 255), thickness=2)

    left = composite_on_checker(overlay)
    right = composite_on_checker(cropped)

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
        f"sens={sensitivity}  strip={strip_thickness:.0f}px  green=raw  orange=fit  "
        f"cyan=cut  |  S save  Q quit"
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
    edge_step: float,
    fit: int,
    refine_center: bool,
    initial_sensitivity: int,
    initial_strip: float,
    shrink: int,
    expand: int,
    canny_low_override: int | None,
    canny_high_override: int | None,
    bg_tolerance_override: int | None,
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
    detection: OuterEdgeDetection | None = None

    while True:
        sensitivity = int(cv2.getTrackbarPos("sensitivity", window))
        strip = float(cv2.getTrackbarPos("strip thickness px", window))

        if detection is None or sensitivity != cached_sensitivity:
            canny_low, canny_high, bg_tolerance = resolve_sensitivity_parameters(sensitivity)
            if canny_low_override is not None:
                canny_low = canny_low_override
            if canny_high_override is not None:
                canny_high = canny_high_override
            if bg_tolerance_override is not None:
                bg_tolerance = max(1, bg_tolerance_override)
            detection = detect_outer_edge(
                rgba,
                edge_source=edge_source,
                bg_tolerance=bg_tolerance,
                canny_low=canny_low,
                canny_high=canny_high,
                blur=blur,
                use_gradient=use_gradient,
                num_rays=num_rays,
                edge_step=edge_step,
                fit=fit,
                refine_center=refine_center,
            )
            cached_sensitivity = sensitivity

        cropped, raw_boundary, fitted_boundary, cut_boundary = render_crop(
            rgba,
            detection,
            fit=fit,
            strip_thickness=strip,
            shrink=shrink,
            expand=expand,
        )
        frame = build_preview_frame(
            rgba,
            cropped,
            raw_boundary,
            fitted_boundary,
            cut_boundary,
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
    rgba = load_rgba(input_path)
    detection = detect_outer_edge(
        rgba,
        edge_source=args.edge_source,
        bg_tolerance=max(1, bg_tolerance),
        canny_low=canny_low,
        canny_high=canny_high,
        blur=args.blur,
        use_gradient=not args.no_gradient,
        num_rays=max(64, args.rays),
        edge_step=max(0.25, args.edge_step),
        fit=fit,
        refine_center=not args.no_refine_center,
    )

    cropped, raw_boundary, fitted_boundary, cut_boundary = render_crop(
        rgba,
        detection,
        fit=fit,
        strip_thickness=strip_thickness,
        shrink=max(0, args.shrink),
        expand=max(0, args.expand),
    )
    edges = detection.edges

    if args.debug_edges:
        if edges is None:
            print("Debug edges skipped: current edge source does not build a Canny map.", file=sys.stderr)
        else:
            save_rgba(
                args.debug_edges.resolve(),
                np.dstack([edges, edges, edges, np.full_like(edges, 255)]),
            )
    if args.debug_boundary:
        save_debug_boundary(
            args.debug_boundary.resolve(),
            rgba,
            raw_boundary,
            fitted_boundary,
            cut_boundary,
        )

    if raw_boundary.shape[0] < 3:
        print(f"Warning: outer edge boundary not found for {input_path.name}", file=sys.stderr)

    output_width = max(0, args.width)
    if output_width > 0:
        cropped = resize_to_width(cropped, output_width)

    save_rgba(output_path, cropped)
    print(
        f"{input_path.name}: {cropped.shape[1]}x{cropped.shape[0]} -> {output_path} "
        f"(edge source: {detection.edge_source})"
    )
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

    print(f"Processing {len(images)} image(s) from {input_dir}")
    print(f"Output folder: {output_dir}")

    failures = 0
    for image_path in images:
        output_path = batch_output_path(
            image_path,
            input_dir.resolve(),
            output_dir,
            suffix="",
        )
        try:
            process_single_file(
                image_path,
                output_path,
                args,
                fit=fit,
                strip_thickness=strip_thickness,
                canny_low=canny_low,
                canny_high=canny_high,
                bg_tolerance=bg_tolerance,
            )
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
            edge_step=max(0.25, args.edge_step),
            fit=fit,
            refine_center=not args.no_refine_center,
            initial_sensitivity=sensitivity,
            initial_strip=strip_thickness,
            shrink=max(0, args.shrink),
            expand=max(0, args.expand),
            canny_low_override=args.canny_low,
            canny_high_override=args.canny_high,
            bg_tolerance_override=args.bg_tolerance,
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
    if args.width > 0:
        print(f"Output width: {args.width}px")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
