# a script to analyze the dataset and compute the Eval to WDL Scale
import multiprocessing
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from pydantic import BaseModel
from scipy.optimize import curve_fit
from tqdm import tqdm

MIN_SCORE = -1000
MAX_SCORE = 1000
N_BUCKETS = 50
LIMIT = 8

BUCKET_SIZE = (MAX_SCORE - MIN_SCORE) // N_BUCKETS
PIECES = set("pnbrq")


class PieceStats(BaseModel):
    # per-position count
    total: list[int] = [0 for _ in range(30)]

    # per-piece count
    p: int = 0
    n: int = 0
    b: int = 0
    r: int = 0
    q: int = 0

    def avg_count(self, n_positions) -> list[float]:
        return [
            self.p / n_positions,
            self.n / n_positions,
            self.b / n_positions,
            self.r / n_positions,
            self.q / n_positions,
        ]

    def __iadd__(self, other: "PieceStats") -> "PieceStats":
        for i in range(30):
            self.total[i] += other.total[i]
        self.p += other.p
        self.n += other.n
        self.b += other.b
        self.r += other.r
        self.q += other.q
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
    scores_wdl_sum: list[float] = [0.0 for _ in range(N_BUCKETS)]

    @property
    def scores_wdl_avg(self) -> list[float]:
        return [w / (s + 1e-6) for s, w in zip(self.scores, self.scores_wdl_sum)]

    def __iadd__(self, other: "Stats") -> "Stats":
        self.n_positions += other.n_positions
        self.pieces += other.pieces
        self.side += other.side
        self.wdls += other.wdls
        for i in range(N_BUCKETS):
            self.scores[i] += other.scores[i]
            self.scores_wdl_sum[i] += other.scores_wdl_sum[i]
        return self


def get_stats(filepath: Path) -> Stats:
    stats = Stats()

    with filepath.open() as f:
        # skip headers
        next(f)

        for row in f:
            stats.n_positions += 1
            fen, wdl, eval_rel = row.strip().split(",")
            wdl = float(wdl)
            eval_rel = int(eval_rel)

            if wdl == 1.0:
                stats.wdls.w += 1
            elif wdl == 0.0:
                stats.wdls.b += 1
            else:
                stats.wdls.d += 1

            pieces, side, _ = fen.split(" ", 2)
            n_pieces = 0
            for p in pieces.lower():
                if p in PIECES:
                    n_pieces += 1
                    stats.pieces.__dict__[p] += 1
            stats.pieces.total[n_pieces - 1] += 1
            stats.side.__dict__[side] += 1

            score_bucket = (eval_rel - MIN_SCORE) // BUCKET_SIZE
            score_bucket = np.clip(score_bucket, 0, N_BUCKETS - 1)
            stats.scores[score_bucket] += 1
            stats.scores_wdl_sum[score_bucket] += wdl if side == "w" else 1.0 - wdl

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
    params, _ = curve_fit(sigmoid_k, stats.buckets, stats.scores_wdl_avg, p0=200.0)
    wdl_scale = params[0]

    # plot
    print("Plotting...")
    fig = plt.figure(figsize=(9, 9))
    gs = fig.add_gridspec(3, 2)

    # Piece counts
    ax = fig.add_subplot(gs[0, 0])
    ax.bar(list(range(1, 31)), stats.pieces.total)
    ax.set_ylabel("No. Positions")
    ax.set_title("Piece Counts")

    # Average count per piece
    ax = fig.add_subplot(gs[0, 1])
    ax.bar(
        ["pawn", "knight", "bishop", "rook", "queen"],
        stats.pieces.avg_count(stats.n_positions),
        zorder=2,
    )
    ax.grid(axis="y", zorder=1)
    ax.set_yticks([i for i in range(11)])
    ax.set_ylabel("Count")
    ax.set_title("Average Piece Count")

    # Side-to-move
    ax = fig.add_subplot(gs[1, 0])
    ax.bar(
        ["white", "black"],
        np.array([stats.side.w, stats.side.b]) * 100.0 / stats.n_positions,
    )
    ax.set_ylabel("% Positions")
    ax.set_title("Side")

    # WDL
    ax = fig.add_subplot(gs[1, 1])
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
    ax = fig.add_subplot(gs[2, 0])
    ax.bar(stats.buckets, stats.scores, width=BUCKET_SIZE)
    ax.set_xlabel("Eval")
    ax.set_ylabel("No. Positions")
    ax.set_title("Scores")

    # Score vs WDL
    ax = fig.add_subplot(gs[2, 1])
    ax.scatter(
        stats.buckets,
        stats.scores_wdl_avg,
        c="blue",
        marker="x",
        s=10,
        label="Datapoints",
    )
    ax.plot(
        stats.buckets,
        sigmoid_k(np.array(stats.buckets), wdl_scale),
        "r-",
        label=f"Fitted with scale={wdl_scale:.2f}",
    )
    ax.legend(loc="upper left")
    ax.set_xlabel("Eval")
    ax.set_ylabel("WDL")
    ax.set_title("Eval vs WDL")

    plt.tight_layout()
    plt.show()
    fig.savefig("stats.png")
    print(f"Total dataset size: {stats.n_positions}")
    print(f"Fitted WDL Scale: {wdl_scale:.2f}")
