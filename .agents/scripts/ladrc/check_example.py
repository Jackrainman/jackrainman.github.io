#!/usr/bin/env python3
"""Compare the sampled C example trace with the independent Python simulation."""
from __future__ import annotations

import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CACHE = ROOT / ".agents/.cache/ladrc"


def read_csv(path: Path) -> list[dict[str, float]]:
    with path.open(encoding="utf-8", newline="") as f:
        return [{key: float(value) for key, value in row.items()} for row in csv.DictReader(f)]


def main() -> None:
    python_rows = read_csv(CACHE / "ladrc_nominal.csv")
    c_rows = read_csv(CACHE / "example_ladrc.csv")
    by_time = {round(row["t"], 6): row for row in python_rows}
    worst = 0.0
    compared = 0
    # C uses float while Python uses double. High-gain z3 integration is more
    # sensitive to rounding, so each signal has a scale-aware tolerance.
    relative_tolerance = {"theta": 2e-4, "omega": 2e-3, "u": 2e-3, "z3": 1e-2}
    for c_row in c_rows:
        p_row = by_time[round(c_row["t"], 6)]
        for c_key, p_key in [("theta", "y"), ("omega", "v"),
                             ("u", "u_app"), ("z3", "z3")]:
            c_value, p_value = c_row[c_key], p_row[p_key]
            assert math.isfinite(c_value) and math.isfinite(p_value)
            scale = max(1.0, abs(p_value))
            worst = max(worst, abs(c_value - p_value) / scale)
            assert abs(c_value - p_value) <= relative_tolerance[c_key] * scale, (
                f"t={c_row['t']:.3f}, {c_key}: C={c_value}, Python={p_value}")
        compared += 1
    assert compared == len(c_rows) and compared >= 20
    print(f"C/Python sampled trace checks: {compared} rows passed; worst relative error={worst:.3g}")


if __name__ == "__main__":
    main()
