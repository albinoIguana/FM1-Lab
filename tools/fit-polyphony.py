#!/usr/bin/env python3
"""Fit render time = fixed callback overhead + per-voice cost.

Input CSV columns: regime,voices,renderus
Use regime values such as single and split. Collect several steady-state
samples at each voice count; do not mix FX/settings or changing workloads.
"""
import argparse
import csv
from collections import defaultdict

def fit(rows):
    n = len(rows)
    if n < 2:
        raise ValueError("need at least two samples per regime")
    xs = [x for x, _ in rows]
    ys = [y for _, y in rows]
    mx, my = sum(xs) / n, sum(ys) / n
    denom = sum((x - mx) ** 2 for x in xs)
    if denom == 0:
        raise ValueError("voice counts must vary within each regime")
    slope = sum((x - mx) * (y - my) for x, y in rows) / denom
    intercept = my - slope * mx
    return intercept, slope

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_file")
    parser.add_argument("--limit-us", type=float, default=4934.0,
                        help="85%% shedding threshold for a 256-frame/44.1kHz block")
    args = parser.parse_args()
    groups = defaultdict(list)
    with open(args.csv_file, newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            groups[row["regime"]].append((float(row["voices"]), float(row["renderus"])))
    if not groups:
        parser.error("CSV contains no measurements")
    for regime, rows in sorted(groups.items()):
        intercept, slope = fit(rows)
        capacity = (args.limit_us - intercept) / slope if slope > 0 else float("nan")
        print(f"{regime}: fixed={intercept:.1f} us, per_voice={slope:.2f} us, "
              f"estimated_limit={capacity:.1f} voices (limit={args.limit_us:.0f} us, n={len(rows)})")

if __name__ == "__main__":
    main()
