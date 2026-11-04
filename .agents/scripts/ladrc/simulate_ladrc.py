#!/usr/bin/env python3
"""Reproduce the second-order motor LADRC examples used by the MkDocs notes.

The plant and controller use the same forward-Euler convention as the tutorial:
LESO consumes the previous applied input, then the current control request is
computed, saturated, and applied to the plant. All plots are simulations, not
hardware measurements.
"""
from __future__ import annotations

import argparse
import csv
import json
from dataclasses import asdict, dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUTPUT = ROOT / "docs/zh/ControlTheory/Pictures/LADRC战队实战指南"
CACHE = ROOT / ".agents/.cache/ladrc"

BLUE = "#2a78d6"
ORANGE = "#eb6834"
AQUA = "#1baf7a"
INK = "#0b0b0b"
SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
SURFACE = "#fcfcfb"


@dataclass(frozen=True)
class Config:
    h: float = 0.001
    duration: float = 5.0
    damping: float = 2.0       # c/J, s^-1
    b_true: float = 5.0        # Kt/J, rad/(s^2 A)
    b0: float = 5.0
    wc: float = 10.0
    wo: float = 40.0
    u_max: float = 2.0
    reference: float = 1.0
    disturbance_time: float = 2.0
    load_disturbance: float = -4.0  # acceleration units, rad/s^2
    noise_std: float = 0.0
    seed: int = 20261104


def disturbance(t: np.ndarray, cfg: Config, time_varying: bool = False) -> np.ndarray:
    active = t >= cfg.disturbance_time
    if time_varying:
        return active * (cfg.load_disturbance + 1.5 * np.sin(2 * np.pi * (t - cfg.disturbance_time)))
    return active * cfg.load_disturbance


def simulate_ladrc(cfg: Config, time_varying: bool = False) -> dict[str, np.ndarray]:
    t = np.arange(0.0, cfg.duration + cfg.h, cfg.h)
    n = len(t)
    y = np.zeros(n)
    v = np.zeros(n)
    ym = np.zeros(n)
    u_req = np.zeros(n)
    u_app = np.zeros(n)
    z1 = np.zeros(n)
    z2 = np.zeros(n)
    z3 = np.zeros(n)
    total_f = np.zeros(n)
    kp, kd = cfg.wc**2, 2 * cfg.wc
    l1, l2, l3 = 3 * cfg.wo, 3 * cfg.wo**2, cfg.wo**3
    rng = np.random.default_rng(cfg.seed)
    noise = rng.normal(0.0, cfg.noise_std, n)
    d = disturbance(t, cfg, time_varying)

    for k in range(n - 1):
        ym[k] = y[k] + noise[k]
        error = ym[k] - z1[k]
        u_prev = u_app[k - 1] if k else 0.0
        dz1 = z2[k] + l1 * error
        dz2 = z3[k] + cfg.b0 * u_prev + l2 * error
        dz3 = l3 * error
        z1[k + 1] = z1[k] + cfg.h * dz1
        z2[k + 1] = z2[k] + cfg.h * dz2
        z3[k + 1] = z3[k] + cfg.h * dz3

        u0 = kp * (cfg.reference - z1[k + 1]) - kd * z2[k + 1]
        u_req[k] = (u0 - z3[k + 1]) / cfg.b0
        u_app[k] = np.clip(u_req[k], -cfg.u_max, cfg.u_max)
        total_f[k] = -cfg.damping * v[k] + (cfg.b_true - cfg.b0) * u_app[k] + d[k]
        y[k + 1] = y[k] + cfg.h * v[k]
        v[k + 1] = v[k] + cfg.h * (-cfg.damping * v[k] + cfg.b_true * u_app[k] + d[k])

    ym[-1] = y[-1] + noise[-1]
    u_req[-1] = u_req[-2]
    u_app[-1] = u_app[-2]
    total_f[-1] = -cfg.damping * v[-1] + (cfg.b_true - cfg.b0) * u_app[-1] + d[-1]
    return {"t": t, "y": y, "v": v, "ym": ym, "u_req": u_req,
            "u_app": u_app, "z1": z1, "z2": z2, "z3": z3, "f": total_f}


def simulate_pid(cfg: Config) -> dict[str, np.ndarray]:
    """PID baseline with derivative filtering and conditional integration.

    Position P/D gains target the same ideal second-order poles as LADRC before
    adding integral action. The integral gain and derivative filter are reported
    in metrics; this is one tuned example, not a universal controller ranking.
    """
    t = np.arange(0.0, cfg.duration + cfg.h, cfg.h)
    n = len(t)
    y = np.zeros(n)
    v = np.zeros(n)
    u_app = np.zeros(n)
    u_req = np.zeros(n)
    integral = 0.0
    v_filt = 0.0
    kp_pid = cfg.wc**2 / cfg.b_true
    kd_pid = (2 * cfg.wc - cfg.damping) / cfg.b_true
    ki_pid = 12.0
    derivative_tau = 0.02
    d = disturbance(t, cfg)
    rng = np.random.default_rng(cfg.seed)
    noise = rng.normal(0.0, cfg.noise_std, n)

    for k in range(n - 1):
        measurement = y[k] + noise[k]
        raw_v = (measurement - (y[k - 1] + noise[k - 1])) / cfg.h if k else 0.0
        v_filt += cfg.h / derivative_tau * (raw_v - v_filt)
        error = cfg.reference - measurement
        candidate_integral = integral + cfg.h * error
        candidate = kp_pid * error + ki_pid * candidate_integral - kd_pid * v_filt
        saturated = np.clip(candidate, -cfg.u_max, cfg.u_max)
        # Conditional integration: do not integrate farther into saturation.
        if (abs(candidate) <= cfg.u_max or
                (candidate > cfg.u_max and error < 0) or
                (candidate < -cfg.u_max and error > 0)):
            integral = candidate_integral
            u_req[k] = candidate
        else:
            u_req[k] = kp_pid * error + ki_pid * integral - kd_pid * v_filt
        u_app[k] = np.clip(u_req[k], -cfg.u_max, cfg.u_max)
        y[k + 1] = y[k] + cfg.h * v[k]
        v[k + 1] = v[k] + cfg.h * (-cfg.damping * v[k] + cfg.b_true * u_app[k] + d[k])

    u_req[-1] = u_req[-2]
    u_app[-1] = u_app[-2]
    return {"t": t, "y": y, "u_req": u_req, "u_app": u_app}


def recovery_time(result: dict[str, np.ndarray], cfg: Config) -> float | None:
    start = int(cfg.disturbance_time / cfg.h)
    hold = int(0.2 / cfg.h)
    outside = np.flatnonzero(np.abs(result["y"][start:] - cfg.reference) > 0.02)
    if len(outside) == 0:
        return 0.0
    last = start + int(outside[-1])
    if last + hold >= len(result["t"]):
        return None
    return float(result["t"][last + 1] - cfg.disturbance_time)


def metrics(result: dict[str, np.ndarray], cfg: Config) -> dict[str, float | None]:
    before = result["t"] < cfg.disturbance_time
    sat = np.abs(result["u_req"]) > cfg.u_max + 1e-9
    return {
        "overshoot_percent": float(max(0.0, np.max(result["y"][before]) - cfg.reference) * 100),
        "iae": float(np.trapezoid(np.abs(result["y"] - cfg.reference), result["t"])),
        "disturbance_recovery_s": recovery_time(result, cfg),
        "control_rms_A": float(np.sqrt(np.mean(result["u_app"] ** 2))),
        "saturation_percent": float(100 * np.mean(sat)),
        "max_abs_request_A": float(np.max(np.abs(result["u_req"]))),
    }


def style_axes(ax: plt.Axes, xlabel: bool = True) -> None:
    ax.set_facecolor(SURFACE)
    ax.grid(True, color=GRID, linewidth=0.6, linestyle="-")
    for spine in ax.spines.values():
        spine.set_color(GRID)
    ax.tick_params(colors=SECONDARY, labelsize=9)
    ax.title.set_color(INK)
    ax.yaxis.label.set_color(SECONDARY)
    ax.xaxis.label.set_color(SECONDARY)
    if xlabel:
        ax.set_xlabel("时间 (s)")


def save_figure(fig: plt.Figure, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, format="svg", facecolor=SURFACE, bbox_inches="tight")
    # Matplotlib leaves trailing spaces in multiline path data; keep generated
    # assets clean so git diff --check remains reproducible.
    path.write_text("\n".join(line.rstrip() for line in path.read_text(encoding="utf-8").splitlines()) + "\n",
                    encoding="utf-8")
    CACHE.mkdir(parents=True, exist_ok=True)
    fig.savefig(CACHE / f"{path.stem}.png", dpi=140, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def plot_disturbance(ladrc: dict, pid: dict, cfg: Config, output: Path) -> None:
    fig, axes = plt.subplots(3, 1, figsize=(9, 9), sharex=True)
    t = ladrc["t"]
    axes[0].plot(t, ladrc["y"], color=BLUE, linewidth=1.8, label="LADRC")
    axes[0].plot(t, pid["y"], color=ORANGE, linewidth=1.8, label="PID")
    axes[0].axhline(cfg.reference, color=MUTED, linewidth=1.0, linestyle="--", label="参考")
    axes[0].set_ylabel("角度 (rad)")
    axes[0].set_title("同一对象与限幅下的负载扰动响应（仿真）")
    axes[0].legend(frameon=False, ncol=3)

    axes[1].plot(t, ladrc["u_app"], color=BLUE, linewidth=1.5, label="LADRC 实际电流")
    axes[1].plot(t, pid["u_app"], color=ORANGE, linewidth=1.5, label="PID 实际电流")
    axes[1].axvline(cfg.disturbance_time, color=MUTED, linewidth=1.0)
    axes[1].set_ylabel("电流 (A)")
    axes[1].legend(frameon=False)

    axes[2].plot(t, ladrc["f"], color=BLUE, linewidth=1.8, label="实际总扰动 f")
    axes[2].plot(t, ladrc["z3"], color=ORANGE, linewidth=1.8, label="LESO 估计 $z_3$")
    axes[2].axvline(cfg.disturbance_time, color=MUTED, linewidth=1.0)
    axes[2].set_ylabel("角加速度 (rad/s²)")
    axes[2].legend(frameon=False)
    for index, ax in enumerate(axes):
        style_axes(ax, xlabel=index == len(axes) - 1)
    fig.tight_layout()
    save_figure(fig, output / "ladrc_disturbance_simulation.svg")


def plot_tuning(cfg: Config, output: Path) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(11, 8))
    colors = [AQUA, BLUE, ORANGE]

    for color, b0, label in zip(colors, [2.5, 5.0, 7.5], ["$b_0=2.5$（偏小）", "$b_0=5$（标称）", "$b_0=7.5$（偏大）"]):
        result = simulate_ladrc(Config(b0=b0))
        axes[0, 0].plot(result["t"], result["y"], color=color, linewidth=1.6, label=label)
    axes[0, 0].axhline(1.0, color=MUTED, linewidth=1.0, linestyle="--")
    axes[0, 0].set_title("输入增益估计偏差")
    axes[0, 0].set_ylabel("角度 (rad)")
    axes[0, 0].legend(frameon=False, fontsize=8)

    for color, wo, label in zip(colors, [20.0, 40.0, 80.0], ["$\\omega_o=20$", "$\\omega_o=40$", "$\\omega_o=80$"]):
        result = simulate_ladrc(Config(wo=wo, noise_std=0.002))
        axes[0, 1].plot(result["t"], result["u_app"], color=color, linewidth=1.3, label=label)
    axes[0, 1].set_title("测量噪声下的实际电流（σ=0.002 rad）")
    axes[0, 1].set_ylabel("电流 (A)")
    axes[0, 1].legend(frameon=False, fontsize=8)

    nominal = simulate_ladrc(cfg)
    axes[1, 0].plot(nominal["t"], nominal["u_req"], color=ORANGE, linewidth=1.5, label="请求电流")
    axes[1, 0].plot(nominal["t"], nominal["u_app"], color=BLUE, linewidth=1.8, label="实际电流")
    axes[1, 0].axhline(cfg.u_max, color=MUTED, linewidth=1.0, linestyle="--")
    axes[1, 0].axhline(-cfg.u_max, color=MUTED, linewidth=1.0, linestyle="--")
    axes[1, 0].set_title("执行器饱和：LESO 使用实际电流")
    axes[1, 0].set_ylabel("电流 (A)")
    axes[1, 0].legend(frameon=False, fontsize=8)

    varying = simulate_ladrc(cfg, time_varying=True)
    axes[1, 1].plot(varying["t"], varying["f"], color=BLUE, linewidth=1.7, label="实际总扰动 f")
    axes[1, 1].plot(varying["t"], varying["z3"], color=ORANGE, linewidth=1.7, label="估计 $z_3$")
    axes[1, 1].set_title("时变扰动存在估计残差")
    axes[1, 1].set_ylabel("角加速度 (rad/s²)")
    axes[1, 1].legend(frameon=False, fontsize=8)

    for ax in axes.flat:
        style_axes(ax)
    fig.tight_layout()
    save_figure(fig, output / "ladrc_tuning_simulation.svg")


def write_data(ladrc: dict, pid: dict, cfg: Config) -> dict:
    CACHE.mkdir(parents=True, exist_ok=True)
    keys = ["t", "y", "v", "ym", "u_req", "u_app", "z1", "z2", "z3", "f"]
    with (CACHE / "ladrc_nominal.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(keys)
        writer.writerows(zip(*(ladrc[key] for key in keys)))
    report = {
        "simulation_only": True,
        "config": asdict(cfg),
        "pid": {"kp": cfg.wc**2 / cfg.b_true,
                "ki": 12.0,
                "kd": (2 * cfg.wc - cfg.damping) / cfg.b_true,
                "derivative_tau_s": 0.02,
                "anti_windup": "conditional integration"},
        "ladrc_metrics": metrics(ladrc, cfg),
        "pid_metrics": metrics(pid, cfg),
    }
    (CACHE / "metrics.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    plt.rcParams.update({
        "font.sans-serif": ["Microsoft YaHei", "SimHei", "DejaVu Sans"],
        "axes.unicode_minus": False,
        "svg.fonttype": "path",
        "figure.facecolor": SURFACE,
        "savefig.facecolor": SURFACE,
    })
    cfg = Config()
    ladrc = simulate_ladrc(cfg)
    pid = simulate_pid(cfg)
    for name, result in [("LADRC", ladrc), ("PID", pid)]:
        assert all(np.all(np.isfinite(value)) for value in result.values()), name
        assert np.max(np.abs(result["u_app"])) <= cfg.u_max + 1e-9, name
    assert np.any(np.abs(ladrc["u_req"]) > cfg.u_max), "Nominal example must exercise saturation"
    assert np.any(ladrc["f"] != 0), "Nominal example must include a nonzero disturbance"
    report = write_data(ladrc, pid, cfg)
    plot_disturbance(ladrc, pid, cfg, args.output_dir)
    plot_tuning(cfg, args.output_dir)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print(f"Figures: {args.output_dir}")
    print(f"Data: {CACHE}")


if __name__ == "__main__":
    main()
