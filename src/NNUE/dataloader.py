import json
from pathlib import Path
from typing import Generator

import numpy as np
import pandas as pd
import torch
from net import NNUE
from torch.utils.data import DataLoader, Dataset


def sigmoid_k(eval, k):
    return 1 / (1 + np.exp(-eval / k))


class NNUEDataSet(Dataset):
    def __init__(self, model: NNUE, filepath: Path, cache_dir: Path | None = None):
        """Loads dataset from filepath, computes the features using the model, and
        saves the feature-computed dataset inside cache_dir.

        Args:
            model (NNUE). NNUE model, used to generate the features
            filepath (Path): path to the data file
            cache_dir (Path, optional): path to cache directory
        """
        # load data
        self.model = model
        self.data = pd.read_csv(filepath)

        # compute features and save to cache_dir
        if len({"side", "widx", "bidx"} - set(self.data.columns)):
            self.data[["side", "widx", "bidx"]] = (
                self.data["fen"].apply(model.get_features).apply(pd.Series)
            )
            self.data = self.data.dropna()
            if cache_dir is not None:
                self.data.to_csv(cache_dir / filepath.name, sep=",", index=False)
        else:
            self.data["widx"] = self.data["widx"].apply(lambda x: json.loads(x))
            self.data["bidx"] = self.data["bidx"].apply(lambda x: json.loads(x))
            self.data = self.data.dropna()

        # convert eval to WDL scale
        self.data["eval"] = sigmoid_k(self.data["eval"], model.WDL_SCALE)

    def __getitem__(self, index: int):
        """Returns the features and labels

        Args:
            index (int): dataset index

        Returns:
            tuple: ((white_features, black_features, side), label)
        """
        wf = torch.zeros(self.model.N_INPUTS)
        bf = torch.zeros(self.model.N_INPUTS)
        wf[self.data["widx"][index]] = 1
        bf[self.data["bidx"][index]] = 1
        return (wf, bf, self.data["side"][index]), self.data["eval"][index]

    def __len__(self):
        return len(self.data)


def get_dataloader(
    dataset_path: Path, model: NNUE, batch_size: int, shuffle: bool, repeat: bool
) -> Generator[DataLoader, None, None]:
    """Yields each dataloader in the dataset path.
    If repeat is True, it will restart from the first superbatch after iterating
    through the whole directory.

    Args:
        dataset_path (Path): path to the directory containing the data files
        model (NNUE): NNUE model, used to generate the features
        batch_size (int): batch size for the dataloader
        shuffle (bool): whether to shuffle the data points in each dataloader
        repeat (bool): whether to loop through the superbatches endlessly

    Yields:
        DataLoader: the dataloader for that superbatch
    """
    cache_dir = dataset_path / "cache"
    cache_dir.mkdir(exist_ok=True)

    # get number of files in directory
    count = len(list(dataset_path.glob("*.csv")))

    # iterate
    while True:
        for b in range(1, count + 1):
            filename = f"{b}.csv"
            # check for cache
            if (cache_dir / filename).exists():
                filepath = cache_dir / filename
            else:
                filepath = dataset_path / filename

            # get dataloader
            dataset = NNUEDataSet(model, filepath, cache_dir)
            dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=shuffle)
            yield dataloader

        if not repeat:
            break
