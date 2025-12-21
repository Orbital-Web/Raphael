# a script to keep only the quiet positions
import multiprocessing
from enum import IntEnum
from pathlib import Path

import pandas as pd
from tqdm import tqdm


class Flags(IntEnum):
    NONE = 0
    CHECK = 1
    CAPTURE = 2
    PROMOTION = 4


# NOTE: modify this to enable/disable certain flags
EXCLUDE = Flags.CHECK | Flags.CAPTURE | Flags.PROMOTION


def clean(args: tuple[Path, Path]) -> None:
    filepath, outdir = args

    name = filepath.name
    df = pd.read_csv(filepath)

    mask = (df["flag"] & EXCLUDE) == 0
    df = df[mask][["fen", "wdl", "eval"]]

    outpath = outdir / name
    df.to_csv(outpath, sep=",", index=False)


if __name__ == "__main__":
    indir = Path(input("Dataset directory: "))
    outdir = Path(indir.as_posix() + "_clean")
    outdir.mkdir(exist_ok=True, parents=True)

    print("Cleaning...")
    with multiprocessing.Pool(8) as pool:
        paths = [(path, outdir) for path in indir.iterdir() if path.is_file()]
        for res in tqdm(pool.imap_unordered(clean, paths), total=len(paths)):
            pass
