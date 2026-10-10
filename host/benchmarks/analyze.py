#!/usr/bin/env python3
"""Analyze device timestamp stability in a DRATS capture and optionally draw SVG."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from drats.protocol import ProtocolError, decode_sample_block, decode_stream  # noqa: E402


def percentile(values: list[int], percent: float) -> int | None:
    if not values:
        return None
    ordered = sorted(values)
    index = round((len(ordered) - 1) * percent / 100.0)
    return ordered[index]


def write_svg(path: Path, jitter: list[int]) -> None:
    width, height, pad = 1000, 360, 45
    if not jitter:
        points = ""
        bound = 1
    else:
        bound = max(1, max(abs(value) for value in jitter))
        points_list = []
        for index, value in enumerate(jitter):
            x = pad + index * (width - 2 * pad) / max(1, len(jitter) - 1)
            y = height / 2 - value * (height / 2 - pad) / bound
            points_list.append(f"{x:.1f},{y:.1f}")
        points = " ".join(points_list)
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
<rect width="100%" height="100%" fill="white"/>
<line x1="{pad}" y1="{height/2}" x2="{width-pad}" y2="{height/2}" stroke="#777"/>
<polyline fill="none" stroke="#1261a0" stroke-width="1.5" points="{points}"/>
<text x="{pad}" y="25" font-family="sans-serif" font-size="16">DRATS block interval jitter (device timestamp)</text>
<text x="{pad}" y="{height-10}" font-family="sans-serif" font-size="12">block index</text>
<text x="{width-pad-120}" y="25" font-family="sans-serif" font-size="12">range ±{bound} ns</text>
</svg>
"""
    path.write_text(svg, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--svg", type=Path)
    args = parser.parse_args()
    frames = list(decode_stream(args.capture.read_bytes()))
    blocks = []
    for frame in frames:
        try:
            first, period, samples = decode_sample_block(frame)
            blocks.append((frame, first, period, len(samples)))
        except ProtocolError:
            pass
    jitter: list[int] = []
    for previous, current in zip(blocks, blocks[1:]):
        expected = previous[2] * previous[3]
        jitter.append(current[0].timestamp_ns - previous[0].timestamp_ns - expected)
    report = {
        "capture": str(args.capture),
        "source_classification": "capture timestamps; not proof of hardware performance",
        "sample_blocks": len(blocks),
        "samples": sum(block[3] for block in blocks),
        "jitter_ns": {
            "min": min(jitter) if jitter else None,
            "p50": percentile(jitter, 50),
            "p95": percentile(jitter, 95),
            "p99": percentile(jitter, 99),
            "max": max(jitter) if jitter else None,
            "mean": statistics.fmean(jitter) if jitter else None,
        },
    }
    if args.svg:
        write_svg(args.svg, jitter)
        report["svg"] = str(args.svg)
    print(json.dumps(report, sort_keys=True))
    return 0 if blocks else 1


if __name__ == "__main__":
    raise SystemExit(main())
