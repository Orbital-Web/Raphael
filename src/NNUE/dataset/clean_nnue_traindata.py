# evaluates the position using stockfish and cleans noisy and forced-mate positions
import multiprocessing
from pathlib import Path
from random import random

import chess.engine

engine = None

SAMPLE_RATIO = {
    0: 1.0,
    100: 1.0,
    200: 1.0,
    300: 1.0,
    400: 0.9,
    500: 0.8,
    600: 0.7,
    700: 0.5,
    800: 0.25,
}
DEFAULT_RATIO = 0.1
ERROR_THRESHOLD = 1000

INPUT_FILENAME = "traindata.csv"
OUTPUT_FILENAME = "traindata_processed.csv"


def init_engine():
    global engine
    engine = chess.engine.SimpleEngine.popen_uci(
        "../../Games/stockfish/stockfish-ubuntu-x86-64-avx2"
    )


def evaluate(row: str) -> tuple[str | None, float]:
    global engine
    fen, wdl, rel_eval = row.strip().split(",")
    rel_eval = int(rel_eval)

    # pick data point by chance based on bucket
    bucket = int(abs(rel_eval)) // 100 * 100
    if random() > SAMPLE_RATIO.get(bucket, DEFAULT_RATIO):
        return None, 0.0

    # skip if stockfish can find mate quick enough
    board = chess.Board(fen)
    score = engine.analyse(board, chess.engine.Limit(time=0.05))["score"]
    true_eval = score.pov(board.turn).score()
    if true_eval is None:
        return None, 0.0

    error = abs(true_eval - rel_eval)
    return f"{fen},{wdl},{rel_eval},{error}\n", error


if __name__ == "__main__":
    with open(INPUT_FILENAME, "r") as in_f:
        inputs = in_f.readlines()[1:]

    pool = multiprocessing.Pool(
        multiprocessing.cpu_count() - 2, initializer=init_engine
    )

    # raise error if file exists to avoid overwriting it
    outpath = Path(OUTPUT_FILENAME)
    if outpath.exists():
        raise FileExistsError("The specified output file already exists")
    errorpath = Path("high_error.csv")

    sum_err = 0.0
    cnt_err = 0
    with outpath.open("w") as out_f:
        out_f.write("fen,wdl,eval,error\n")

        # separate file to store high-error data points to see what the problem is
        with errorpath.open("a") as err_f:

            for result, error in pool.imap_unordered(evaluate, inputs, chunksize=128):
                if result is None:
                    continue

                if error >= ERROR_THRESHOLD:
                    err_f.write(result)
                    continue

                out_f.write(result)
                sum_err += error
                cnt_err += 1

                if (cnt_err + 1) % 10000 == 0:
                    avg_error = sum_err / cnt_err
                    print(f"Average error: {avg_error:.2f}")

    avg_error = sum_err / cnt_err
    print(f"Average error: {avg_error:.2f}")
