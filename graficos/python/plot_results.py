#!/usr/bin/env python3
"""Gráficos de um resultado do fsae_sim (pasta gravada por ``fsae_sim run``).

Uso:
    python graficos/python/plot_results.py resultados/autox
    python graficos/python/plot_results.py resultados/autox --lap all
    python graficos/python/plot_results.py resultados/autox --save figuras/
    python graficos/python/plot_results.py resultados/setupA --compare resultados/setupB
    python graficos/python/plot_results.py resultados/autox --log dados/privado/log_autox.csv
    python graficos/python/plot_results.py resultados/autox --oplap dados/privado/Pista_AutoX_OPLap.csv

Por padrão mostra só a última volta (como o runSimulation do MATLAB, que plota a
volta lançada); com --log/--oplap o padrão é a simulação inteira (como o
plotComparison). Requer numpy e matplotlib (``pip install numpy matplotlib``).
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
import unicodedata
from dataclasses import dataclass
from pathlib import Path

import numpy as np

# --- Paleta (instância de referência validada; ver docs) ---------------------
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
TRACK_GRAY = "#c9c8c2"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]  # azul, laranja, água (ordem fixa)
SEQUENTIAL = "Blues"  # magnitude: um só tom, claro -> escuro
SEQ_START = 0.3  # corta o trecho quase branco, que some no fundo claro


@dataclass
class Result:
    name: str
    data: dict[str, np.ndarray]
    summary: dict
    geometry: dict[str, np.ndarray] | None
    car: dict | None


def load_result(folder: Path) -> Result:
    tel = folder / "telemetry.csv"
    if not tel.is_file():
        sys.exit(f"Erro: {tel} não encontrado. Gere com: fsae_sim run --track <pista> --out {folder}")
    arr = np.genfromtxt(tel, delimiter=",", names=True)
    data = {n: np.asarray(arr[n], dtype=float) for n in arr.dtype.names}
    summary = json.loads((folder / "summary.json").read_text(encoding="utf-8"))
    geometry = None
    geo = folder / "geometry.csv"
    if geo.is_file():
        g = np.genfromtxt(geo, delimiter=",", names=True)
        geometry = {n: np.asarray(g[n], dtype=float) for n in g.dtype.names}
    car_file = folder / "car.json"
    car = json.loads(car_file.read_text(encoding="utf-8")) if car_file.is_file() else None
    return Result(folder.name, data, summary, geometry, car)


# --- Logs de comparação (loadRealLog.m / loadOPLapLog.m) ---------------------

# Prefixos aceitos para cada coluna do log real (como o loadRealLog.m). A
# comparação ignora maiúsculas e acentos, então "Distância" casa com "DISTANCIA"
# (no MATLAB o acento impedia e a distância era recalculada).
LOG_ALIASES = {
    "distance": ["distancia", "distance"],
    "gear": ["marcha", "gear"],
    "lat_g": ["forca_g_lateral", "aceleracao_lateral"],
    "long_g": ["forca_g_acelera", "aceleracao_longitudinal"],
    "rpm": ["rpm"],
    "time": ["time", "tempo"],
    "velocity": ["velocidade_de_t", "velocidade"],
}


def _norm(text: str) -> str:
    text = unicodedata.normalize("NFKD", text)
    return "".join(c for c in text if not unicodedata.combining(c)).lower().strip()


def _to_float(text: str) -> float:
    try:
        return float(text)
    except ValueError:
        return float("nan")


def load_real_log(path: Path) -> dict[str, np.ndarray]:
    """Log de aquisição (CSV com cabeçalho), no formato do history: SI, m/s e m/s²."""
    with open(path, newline="", encoding="utf-8-sig") as f:
        rows = list(csv.reader(f))
    header, body = rows[0], [r for r in rows[1:] if r]
    cols = {}
    for name, aliases in LOG_ALIASES.items():
        for j, h in enumerate(header):
            if any(_norm(h).startswith(a) for a in aliases):
                cols[name] = np.array([_to_float(r[j]) if j < len(r) else np.nan for r in body])
                print(f"   log: '{name}' <- coluna '{h}'")
                break
    n = len(body)
    if "time" not in cols or "velocity" not in cols:
        sys.exit(f"Erro: {path} precisa de colunas de tempo (TIME/TEMPO) e velocidade (VELOCIDADE...)")
    if "distance" not in cols:
        print("   log: sem coluna de distância; calculando por velocidade × tempo")
        v = cols["velocity"] / 3.6
        dt = np.diff(np.concatenate([[0.0], cols["time"]]))
        cols["distance"] = np.concatenate([[0.0], np.cumsum(v[:-1] * dt[1:])])
    zeros = np.zeros(n)
    return {
        "time": cols["time"],
        "distance": cols["distance"],
        "velocity": cols["velocity"] / 3.6,
        "gear": cols.get("gear", zeros),
        "rpm": cols.get("rpm", zeros),
        "acceleration": cols.get("long_g", zeros) * 9.81,
        "lateral_accel": cols.get("lat_g", zeros) * 9.81,
    }


def load_oplap(path: Path) -> dict[str, np.ndarray]:
    """Exportação do OptimumLap ('[Simulation Results]', separador ';')."""
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    try:
        start = next(i for i, l in enumerate(lines) if l.strip() == "[Simulation Results]")
    except StopIteration:
        sys.exit(f"Erro: {path} não tem a seção [Simulation Results] do OptimumLap")
    header = [h.strip() for h in lines[start + 1].split(";")]
    data = [l.split(";") for l in lines[start + 4:] if l.strip()]  # pula 2 linhas de unidades

    def col(prefix: str) -> np.ndarray:
        j = next((k for k, h in enumerate(header) if h.startswith(prefix)), None)
        if j is None:
            sys.exit(f"Erro: coluna '{prefix}' não encontrada no OptimumLap ({path})")
        return np.array([_to_float(r[j]) for r in data])

    t = col("elapsedTime")
    d = col("elapsedDistance")
    return {
        "time": t - t[0],
        "distance": d - d[0],
        "velocity": col("speed") / 3.6,
        "gear": col("gear"),
        "rpm": col("engineSpeed"),
        "acceleration": col("longitudinalAcc") * 9.81,
        "lateral_accel": col("lateralAccelera") * 9.81,
    }


def sync_start(d: dict[str, np.ndarray], threshold: float = 0.5) -> dict[str, np.ndarray]:
    """Corta até a primeira amostra com v > 0,5 m/s e zera o tempo (plotComparison.m)."""
    idx = np.flatnonzero(d["velocity"] > threshold)
    if idx.size == 0:
        return d
    k = int(idx[0])
    out = {n: v[k:] for n, v in d.items()}
    out["time"] = out["time"] - out["time"][0]
    out["distance"] = out["distance"] - out["distance"][0]
    return out


def fig_log_compare(plt, r: Result, sim, label, others: list[tuple[str, dict[str, np.ndarray]]]):
    """Equivalente ao plotComparison.m (mesmos 6 gráficos, sem o consumo do log)."""
    fig, ax = plt.subplots(5, 1, figsize=(10, 11), sharex=True, constrained_layout=True)
    names = ", ".join(n for n, _ in others)
    fig.suptitle(f"{r.summary['track']['name']} — simulação ({label}) vs. {names}", color=TEXT)
    series = [("Simulação", sync_start(sim))] + [(n, sync_start(o)) for n, o in others]
    panels = [
        ("Velocidade", "km/h", lambda d: d["velocity"] * 3.6),
        ("Rotação do motor", "rpm", lambda d: d["rpm"]),
        ("Aceleração longitudinal", "g", lambda d: d["acceleration"] / 9.81),
        ("Aceleração lateral", "g", lambda d: d["lateral_accel"] / 9.81),
        ("Marcha", "marcha", None),
    ]
    for a, (title, unit, f) in zip(ax, panels):
        for i, (n, d) in enumerate(series):
            if f is None:
                a.step(d["time"], d["gear"], where="post", color=SERIES[i], label=n,
                       linewidth=2 if i == 0 else 1.5)
            else:
                a.plot(d["time"], f(d), color=SERIES[i], label=n, linewidth=2 if i == 0 else 1.5)
        a.set_title(title)
        a.set_ylabel(unit)
    ax[0].legend(loc="lower right", bbox_to_anchor=(1.0, 1.0), ncol=len(series))
    ax[-1].set_xlabel("Tempo (s)")
    ax[-1].set_xlim(0, max(d["time"][-1] for _, d in series))
    for i, (n, d) in enumerate(series):
        print(f"   {n}: {d['time'][-1]:.3f} s, {d['distance'][-1]:.1f} m, "
              f"v máx {np.nanmax(d['velocity']) * 3.6:.1f} km/h")
    return fig


def select_lap(r: Result, lap: str) -> tuple[dict[str, np.ndarray], str]:
    """Recorta uma volta como o runSimulation: tempo, distância e combustível do zero."""
    d = r.data
    laps = int(d["lap"].max())
    if lap == "all":
        return d, f"{laps} volta(s)"
    k = laps if lap == "last" else int(lap)
    if not 1 <= k <= laps:
        sys.exit(f"Erro: volta {k} não existe (a simulação tem {laps}).")
    idx = d["lap"] == k
    out = {n: v[idx] for n, v in d.items()}
    if k > 1:
        out["time"] = out["time"] - out["time"][0]
        out["distance"] = out["distance"] - (k - 1) * r.summary["track"]["length_m"]
        out["fuel_consumed_g"] = out["fuel_consumed_g"] - out["fuel_consumed_g"][0]
    return out, f"volta {k} de {laps}"


def style(plt) -> None:
    plt.rcParams.update({
        "figure.facecolor": SURFACE,
        "axes.facecolor": SURFACE,
        "savefig.facecolor": SURFACE,
        "axes.edgecolor": GRID,
        "axes.labelcolor": TEXT_2,
        "axes.titlecolor": TEXT,
        "axes.titlesize": 11,
        "axes.titleweight": "bold",
        "axes.titlelocation": "left",
        "axes.grid": True,
        "grid.color": GRID,
        "grid.linewidth": 0.8,
        "axes.spines.top": False,
        "axes.spines.right": False,
        "xtick.color": TEXT_2,
        "ytick.color": TEXT_2,
        "lines.linewidth": 2,
        "legend.frameon": False,
        "legend.labelcolor": TEXT,
        "font.size": 9,
    })


def track_position(geometry: dict[str, np.ndarray], distance: np.ndarray):
    """Ponto do traçado mais próximo de cada distância (como o plotResults.m)."""
    x, y = geometry["x"], geometry["y"]
    s = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(x), np.diff(y)))])
    idx = np.clip(np.searchsorted(s, distance), 0, len(s) - 1)
    return x[idx], y[idx]


def fig_time(plt, r: Result, d, label):
    fig, ax = plt.subplots(5, 1, figsize=(10, 11), sharex=True, constrained_layout=True)
    fig.suptitle(f"{r.summary['track']['name']} — telemetria vs. tempo ({label})", color=TEXT)
    t = d["time"]

    ax[0].plot(t, d["velocity"] * 3.6, color=SERIES[0])
    ax[0].set_title("Velocidade")
    ax[0].set_ylabel("km/h")

    ax[1].plot(t, d["acceleration"] / 9.81, color=SERIES[0], label="Longitudinal")
    ax[1].plot(t, d["lateral_accel"] / 9.81, color=SERIES[1], label="Lateral")
    ax[1].set_title("Acelerações")
    ax[1].set_ylabel("g")
    ax[1].legend(loc="lower right", bbox_to_anchor=(1.0, 1.0), ncol=2)

    ax[2].plot(t, d["rpm"], color=SERIES[0])
    if r.car:
        for rpm in sorted(set(r.car["powertrain"]["upshift_rpm"])):
            ax[2].axhline(rpm, color=TEXT_2, linewidth=1, linestyle="--")
            ax[2].annotate("troca", (t[-1], rpm), color=TEXT_2, fontsize=8,
                           ha="right", va="bottom")
    ax[2].set_title("Rotação do motor")
    ax[2].set_ylabel("rpm")

    ax[3].plot(t, d["fuel_consumed_g"], color=SERIES[0])
    ax[3].set_title("Combustível acumulado")
    ax[3].set_ylabel("g")

    ax[4].step(t, d["gear"], where="post", color=SERIES[0])
    ax[4].set_title("Marcha")
    ax[4].set_ylabel("marcha")
    ax[4].set_xlabel("Tempo (s)")
    ax[4].set_yticks(range(1, int(d["gear"].max()) + 1))
    ax[4].set_xlim(t[0], t[-1])
    return fig


def fig_analysis(plt, r: Result, d, label):
    from matplotlib.colors import BoundaryNorm, ListedColormap
    from matplotlib import colormaps

    def seq(n: int = 256) -> ListedColormap:
        return ListedColormap(colormaps[SEQUENTIAL](np.linspace(SEQ_START, 1.0, n)))

    fig, ax = plt.subplots(2, 2, figsize=(14, 9), constrained_layout=True)
    fig.suptitle(f"{r.summary['track']['name']} — análise ({label})", color=TEXT)
    v = d["velocity"] * 3.6

    a = ax[0, 0]
    a.plot(d["distance"], v, color=SERIES[0])
    a.set_title("Velocidade vs. distância")
    a.set_xlabel("Distância (m)")
    a.set_ylabel("km/h")

    a = ax[0, 1]
    sc = a.scatter(d["lateral_accel"] / 9.81, d["acceleration"] / 9.81, c=v, cmap=seq(),
                   s=10, edgecolors="none")
    a.set_title("Diagrama G-G")
    a.set_xlabel("Lateral (g)")
    a.set_ylabel("Longitudinal (g)")
    a.set_aspect("equal")
    a.set_xlim(-2, 2)
    a.set_ylim(-2, 2)
    fig.colorbar(sc, ax=a, label="km/h")

    if r.geometry is not None:
        x, y = track_position(r.geometry, d["distance"])
        gears = d["gear"].astype(int)
        n_gears = int(gears.max())
        maps = [
            ("Mapa de velocidade", v, seq(), None, "km/h"),
            ("Mapa de marchas", gears, seq(n_gears),
             BoundaryNorm(np.arange(0.5, n_gears + 1.5), n_gears), "marcha"),
        ]
        for a, (title, c, cmap, norm, unit) in zip(ax[1], maps):
            a.plot(r.geometry["x"], r.geometry["y"], color=TRACK_GRAY, linewidth=6,
                   solid_capstyle="round", zorder=1)
            sc = a.scatter(x, y, c=c, cmap=cmap, norm=norm, s=12, edgecolors="none", zorder=2)
            a.set_title(title)
            a.set_aspect("equal")
            a.grid(False)
            a.set_xticks([])
            a.set_yticks([])
            cb = fig.colorbar(sc, ax=a, label=unit)
            if unit == "marcha":
                cb.set_ticks(range(1, n_gears + 1))
    return fig


def fig_brakes(plt, r: Result, d, label):
    fig, ax = plt.subplots(2, 1, figsize=(10, 6.5), sharex=True, constrained_layout=True)
    fig.suptitle(f"{r.summary['track']['name']} — freio dianteiro ({label})", color=TEXT)
    t = d["time"]
    ax[0].plot(t, d["brake_temp"], color=SERIES[0])
    ax[0].set_title("Temperatura do disco")
    ax[0].set_ylabel("°C")
    for i, (col, name) in enumerate([("P_conv", "Convecção"), ("P_rad", "Radiação"),
                                     ("P_cond", "Condução")]):
        ax[1].plot(t, d[col], color=SERIES[i], label=name)
    ax[1].set_title("Calor dissipado por disco")
    ax[1].set_ylabel("W")
    ax[1].set_xlabel("Tempo (s)")
    ax[1].legend(loc="lower right", bbox_to_anchor=(1.0, 1.0), ncol=3)
    ax[1].set_xlim(t[0], t[-1])
    return fig


def fig_compare(plt, runs: list[tuple[Result, dict, str]]):
    """Velocidade e diferença de tempo acumulada vs. distância (o 1º é a base)."""
    fig, ax = plt.subplots(2, 1, figsize=(11, 7.5), sharex=True, constrained_layout=True)
    fig.suptitle("Comparação", color=TEXT)
    base_r, base, _ = runs[0]
    for i, (r, d, label) in enumerate(runs):
        ax[0].plot(d["distance"], d["velocity"] * 3.6, color=SERIES[i],
                   label=f"{r.name} ({label}, {d['time'][-1]:.3f} s)")
    ax[0].set_title("Velocidade vs. distância")
    ax[0].set_ylabel("km/h")
    ax[0].legend(loc="lower right")

    grid = np.linspace(0, min(d["distance"][-1] for _, d, _ in runs), 2000)
    t_base = np.interp(grid, base["distance"], base["time"])
    ax[1].axhline(0, color=TEXT_2, linewidth=1)
    for i, (r, d, _) in enumerate(runs[1:], start=1):
        delta = np.interp(grid, d["distance"], d["time"]) - t_base
        ax[1].plot(grid, delta, color=SERIES[i], label=f"{r.name} − {base_r.name}")
    ax[1].set_title("Diferença de tempo acumulada (positivo = mais lento que a base)")
    ax[1].set_ylabel("s")
    ax[1].set_xlabel("Distância (m)")
    ax[1].legend(loc="upper left")
    return fig


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("result", type=Path, help="pasta gravada por 'fsae_sim run'")
    p.add_argument("--lap", default=None,
                   help="número da volta, 'last' ou 'all' (padrão: 'last'; 'all' com --log/--oplap)")
    p.add_argument("--compare", type=Path, nargs="+", default=[],
                   help="outras pastas para comparar com a primeira (até 2)")
    p.add_argument("--log", type=Path, help="log de aquisição do carro real (CSV) para comparar")
    p.add_argument("--oplap", type=Path, help="exportação do OptimumLap (CSV ';') para comparar")
    p.add_argument("--save", type=Path, help="grava PNGs nesta pasta em vez de abrir janelas")
    args = p.parse_args(argv)
    has_logs = args.log is not None or args.oplap is not None
    if args.lap is None:
        args.lap = "all" if has_logs else "last"

    import matplotlib
    if args.save:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    style(plt)

    main_r = load_result(args.result)
    d, label = select_lap(main_r, args.lap)
    s = main_r.summary["results"]
    print(f"{main_r.summary['track']['name']}: total {s['total_time_s']:.3f} s | voltas: "
          + " ".join(f"{t:.3f}" for t in s["lap_times_s"] if t is not None))

    figs = {}
    if has_logs:
        if args.compare:
            sys.exit("Erro: use --compare ou --log/--oplap, não os dois juntos.")
        others = []
        if args.log:
            print(f"Log real: {args.log}")
            others.append(("Log real", load_real_log(args.log)))
        if args.oplap:
            print(f"OptimumLap: {args.oplap}")
            others.append(("OptimumLap", load_oplap(args.oplap)))
        figs["vs_log"] = fig_log_compare(plt, main_r, d, label, others)
    elif args.compare:
        if len(args.compare) > 2:
            sys.exit("Erro: compare no máximo 3 resultados de uma vez (cores distintas garantidas).")
        runs = [(main_r, d, label)]
        for folder in args.compare:
            r = load_result(folder)
            rd, rl = select_lap(r, args.lap)
            runs.append((r, rd, rl))
        figs["comparacao"] = fig_compare(plt, runs)
    else:
        figs["telemetria"] = fig_time(plt, main_r, d, label)
        figs["analise"] = fig_analysis(plt, main_r, d, label)
        figs["freio"] = fig_brakes(plt, main_r, d, label)

    if args.save:
        args.save.mkdir(parents=True, exist_ok=True)
        for name, fig in figs.items():
            path = args.save / f"{main_r.name}_{name}.png"
            fig.savefig(path, dpi=110)
            print(f"Gravado {path}")
    else:
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
