# a script to analyze the dataset and compute the Eval to WDL Scale
import multiprocessing
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from pydantic import BaseModel
from scipy.optimize import curve_fit
from tqdm import tqdm

MIN_SCORE = -1500
MAX_SCORE = 1500
N_BUCKETS = 100
LIMIT = 8

BUCKET_SIZE = (MAX_SCORE - MIN_SCORE) // N_BUCKETS
PIECES = {"p": 1, "n": 3, "b": 3, "r": 5, "q": 9}
TOT_VAL = (
    16 * PIECES["p"]
    + 4 * PIECES["n"]
    + 4 * PIECES["b"]
    + 4 * PIECES["r"]
    + 2 * PIECES["q"]
)


class PieceStats(BaseModel):
    # per-position count
    counts: list[int] = [0 for _ in range(30)]
    materials: list[int] = [0 for _ in range(100)]

    def __iadd__(self, other: "PieceStats") -> "PieceStats":
        for i in range(30):
            self.counts[i] += other.counts[i]
        for i in range(100):
            self.materials[i] += other.materials[i]
        return self


class SideStats(BaseModel):
    w: int = 0
    b: int = 0

    def __iadd__(self, other: "SideStats") -> "SideStats":
        self.w += other.w
        self.b += other.b
        return self


class WDLStats(BaseModel):
    w: int = 0
    d: int = 0
    b: int = 0

    def __iadd__(self, other: "WDLStats") -> "WDLStats":
        self.w += other.w
        self.d += other.d
        self.b += other.b
        return self


class Stats(BaseModel):
    n_positions: int = 0
    pieces: PieceStats = PieceStats()
    side: SideStats = SideStats()
    wdls: WDLStats = WDLStats()

    buckets: list[int] = [b for b in range(MIN_SCORE, MAX_SCORE, BUCKET_SIZE)]
    scores: list[int] = [0 for _ in range(N_BUCKETS)]
    scores_wdl_sum: list[int] = [0 for _ in range(N_BUCKETS)]
    scores_w_sum: list[int] = [0 for _ in range(N_BUCKETS)]
    scores_d_sum: list[int] = [0 for _ in range(N_BUCKETS)]
    scores_l_sum: list[int] = [0 for _ in range(N_BUCKETS)]

    ms_sum: list[list[int]] = [[0 for _ in range(N_BUCKETS)] for _ in range(100)]
    ms_w_sum: list[list[int]] = [[0 for _ in range(N_BUCKETS)] for _ in range(100)]
    ms_d_sum: list[list[int]] = [[0 for _ in range(N_BUCKETS)] for _ in range(100)]

    @property
    def scores_wdl_avg(self) -> list[float]:
        return [c / s if c else 0.0 for s, c in zip(self.scores, self.scores_wdl_sum)]

    @property
    def scores_w_avg(self) -> list[float]:
        return [c / s if c else 0.0 for s, c in zip(self.scores, self.scores_w_sum)]

    @property
    def scores_d_avg(self) -> list[float]:
        return [c / s if c else 0.0 for s, c in zip(self.scores, self.scores_d_sum)]

    @property
    def scores_l_avg(self) -> list[float]:
        return [c / s if c else 0.0 for s, c in zip(self.scores, self.scores_l_sum)]

    @property
    def ms_w_avg(self) -> list[list[float]]:
        return [
            [c / s if c else 0.0 for s, c in zip(ms, ms_w)]
            for ms, ms_w in zip(self.ms_sum, self.ms_w_sum)
        ]

    @property
    def ms_d_avg(self) -> list[list[float]]:
        return [
            [c / (s + 1e-6) for s, c in zip(ms, ms_d)]
            for ms, ms_d in zip(self.ms_sum, self.ms_d_sum)
        ]

    def __iadd__(self, other: "Stats") -> "Stats":
        self.n_positions += other.n_positions
        self.pieces += other.pieces
        self.side += other.side
        self.wdls += other.wdls
        for i in range(N_BUCKETS):
            self.scores[i] += other.scores[i]
            self.scores_wdl_sum[i] += other.scores_wdl_sum[i]
            self.scores_w_sum[i] += other.scores_w_sum[i]
            self.scores_d_sum[i] += other.scores_d_sum[i]
            self.scores_l_sum[i] += other.scores_l_sum[i]
            for m in range(100):
                self.ms_sum[m][i] += other.ms_sum[m][i]
                self.ms_w_sum[m][i] += other.ms_w_sum[m][i]
                self.ms_d_sum[m][i] += other.ms_d_sum[m][i]
        return self


def get_stats(filepath: Path) -> Stats:
    stats = Stats()

    with filepath.open() as f:
        # skip headers
        next(f)

        for row in f:
            stats.n_positions += 1
            fen, wdl, eval_rel, *_ = row.strip().split(",", 3)
            wdl = float(wdl)
            eval_rel = int(eval_rel)

            if wdl == 1.0:
                stats.wdls.w += 1
            elif wdl == 0.0:
                stats.wdls.b += 1
            else:
                stats.wdls.d += 1

            pieces, side, _ = fen.split(" ", 2)
            stats.side.__dict__[side] += 1

            n_pieces = 0
            material = 0
            for p in pieces.lower():
                if p in PIECES:
                    n_pieces += 1
                    material += PIECES[p]
            stats.pieces.counts[n_pieces - 1] += 1
            stats.pieces.materials[material] += 1

            score_bucket = (eval_rel - MIN_SCORE) // BUCKET_SIZE
            score_bucket = np.clip(score_bucket, 0, N_BUCKETS - 1)
            stats.scores[score_bucket] += 1
            stats.scores_wdl_sum[score_bucket] += wdl if side == "w" else 1.0 - wdl
            stats.scores_w_sum[score_bucket] += wdl == int(side == "w")
            stats.scores_d_sum[score_bucket] += wdl == 0.5
            stats.scores_l_sum[score_bucket] += wdl == int(side == "b")

            stats.ms_sum[material][score_bucket] += 1
            stats.ms_w_sum[material][score_bucket] += wdl == int(side == "w")
            stats.ms_d_sum[material][score_bucket] += wdl == 0.5

    return stats


def sigmoid_k(eval, k):
    return 1 / (1 + np.exp(-eval / k))


if __name__ == "__main__":
    indir = Path(input("Dataset directory: "))

    # gather stats
    print("Gathering dataset statistics...")
    stats = Stats()
    with multiprocessing.Pool(8) as pool:
        paths = [path for path in indir.iterdir() if path.is_file()][:LIMIT]
        for res in tqdm(pool.imap_unordered(get_stats, paths), total=len(paths)):
            stats += res

    # fit sigmoid to scores vs wdl
    params, _ = curve_fit(sigmoid_k, stats.buckets, stats.scores_wdl_avg, p0=1000.0)
    wdl_scale = params[0]

    # plot
    print("Plotting...")
    fig = plt.figure(figsize=(18, 8))
    gs = fig.add_gridspec(2, 4)

    # Piece counts
    ax = fig.add_subplot(gs[0, 0])
    ax.bar(list(range(1, 31)), stats.pieces.counts)
    ax.set_ylabel("No. Positions")
    ax.set_title("Piece Counts")

    # Material counts
    ax = fig.add_subplot(gs[0, 1])
    ax.bar(list(range(0, 100)), stats.pieces.materials, width=1)
    ax.set_xlim(0, TOT_VAL + 1)
    ax.set_ylabel("No. Positions")
    ax.set_title("Material (1,3,3,5,9)")

    # Side-to-move
    ax = fig.add_subplot(gs[0, 2])
    ax.bar(
        ["white", "black"],
        np.array([stats.side.w, stats.side.b]) * 100.0 / stats.n_positions,
    )
    ax.set_ylabel("% Positions")
    ax.set_title("Side")

    # WDL
    ax = fig.add_subplot(gs[0, 3])
    ax.bar(
        ["white", "draw", "black"],
        (
            np.array([stats.wdls.w, stats.wdls.d, stats.wdls.b])
            * 100.0
            / stats.n_positions
        ),
    )
    ax.set_ylabel("% Positions")
    ax.set_title("WDL")

    # Scores
    ax = fig.add_subplot(gs[1, 0])
    ax.bar(stats.buckets, stats.scores, width=BUCKET_SIZE)
    ax.set_xlabel("Eval")
    ax.set_ylabel("No. Positions")
    ax.set_title("Scores")

    # Score vs WDL
    ax = fig.add_subplot(gs[1, 1])
    ax.scatter(
        stats.buckets,
        stats.scores_wdl_avg,
        c="blue",
        marker="x",
        s=10,
        label="Measured WDL",
    )
    ax.scatter(
        stats.buckets,
        stats.scores_w_avg,
        c="green",
        marker="x",
        s=10,
        label="Measured Winrate",
    )
    ax.scatter(
        stats.buckets,
        stats.scores_d_avg,
        c="orange",
        marker="x",
        s=10,
        label="Measured drawrate",
    )
    ax.scatter(
        stats.buckets,
        stats.scores_l_avg,
        c="red",
        marker="x",
        s=10,
        label="Measured lossrate",
    )
    ax.plot(
        stats.buckets,
        sigmoid_k(np.array(stats.buckets), wdl_scale),
        "b-",
        linewidth=0.7,
        label=f"WDL fit {wdl_scale:.2f}",
    )
    ax.legend(loc="center left", prop={"size": 8})
    ax.set_xlabel("Eval")
    ax.set_ylabel("WDL")
    ax.set_title("Eval vs WDL")

    # Score vs Material vs Winrate
    ax = fig.add_subplot(gs[1, 2])
    ax.contourf(
        stats.buckets,
        list(range(100)),
        stats.ms_w_avg,
        levels=[0.0, 0.2, 0.4, 0.6, 0.8, 1.0],
    )
    ax.set_xlim(MIN_SCORE // 5, 4 * MAX_SCORE // 5)
    ax.set_ylim(0, TOT_VAL + 1)
    ax.set_xlabel("Eval")
    ax.set_ylabel("Material (1,3,3,5,9)")
    ax.set_title("Winrate")

    # Score vs Material vs Drawrate
    ax = fig.add_subplot(gs[1, 3])
    ax.contourf(
        stats.buckets,
        list(range(100)),
        stats.ms_d_avg,
        levels=[0.0, 0.2, 0.4, 0.6, 0.8, 1.0],
    )
    ax.set_xlim(MIN_SCORE // 2, MAX_SCORE // 2)
    ax.set_ylim(0, TOT_VAL + 1)
    ax.set_xlabel("Eval")
    ax.set_ylabel("Material (1,3,3,5,9)")
    ax.set_title("Drawrate")

    plt.tight_layout()
    plt.show()
    fig.savefig("stats.png")
    print(f"Total dataset size: {stats.n_positions}")
    print(f"Fitted WDL Scale: {wdl_scale:.2f}")
