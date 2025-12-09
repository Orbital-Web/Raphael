# FIXME: to be changed
import chess
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

WDL_SCALE = 186.17


def get_stats(row) -> tuple[float, int, int]:
    board = chess.Board(row["fen"])
    side = int(board.turn == chess.WHITE)

    n_pieces = 0
    for sq in chess.SQUARES:
        if board.piece_at(sq) is not None:
            n_pieces += 1

    eval_wdl = 1.0 / (1.0 + np.exp(-row["eval"] / WDL_SCALE))

    return eval_wdl, n_pieces, side


if __name__ == "__main__":
    filename = input("Name of dataset: ")
    outfile = filename.rsplit(".", 1)[0] + ".png"
    df = pd.read_csv(filename)
    df[["eval_wdl", "n_pieces", "side"]] = df.apply(
        get_stats, axis=1, result_type="expand"
    )

    fig, axes = plt.subplots(2, 2, figsize=(10, 8))

    axes[0][0].hist(df["eval"], bins=100)
    axes[0][0].set_title("Eval (raw)")

    axes[0][1].hist(df["eval_wdl"], bins=100)
    axes[0][1].set_title("Eval (wdl scale)")

    axes[1][0].hist(df["n_pieces"], bins=32)
    axes[1][0].set_title("No. Pieces")

    axes[1][1].hist(df["side"], bins=2)
    axes[1][1].set_title("Side to move")
    plt.savefig(outfile)
