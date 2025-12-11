# a script to downsample positions with extreme evaluations
import multiprocessing
from pathlib import Path

import numpy as np
import pandas as pd
from tqdm import tqdm

WDL_SCALE = 258.0


def sample(eval_rel) -> bool:
    wdl = 1 / (1 + np.exp(-eval_rel / WDL_SCALE))
    weight = 16 * np.power(wdl - 0.5, 4)
    return np.random.random() >= weight


def clean(args: tuple[Path, Path]) -> None:
    filepath, outdir = args

    name = filepath.name
    df = pd.read_csv(filepath)

    mask = df["eval"].apply(sample)
    df = df[mask]

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
