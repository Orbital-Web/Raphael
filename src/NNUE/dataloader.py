from pathlib import Path
from typing import Generator

import numpy as np
import torch
from net import NNUE
from torch.utils.data import DataLoader, Dataset


def sigmoid_k(eval: int, k: float) -> float:
    return 1 / (1 + np.exp(-eval / k))


class NNUEDataSet(Dataset):
    def __init__(self, model: NNUE, filepath: Path, lmbda: float):
        """Loads dataset from filepath and computes the features using the model.

        Args:
            model (NNUE). NNUE model, used to generate the features
            filepath (Path): path to the data file
            lmbda (float): ratio of wdl to eval in training label
        """
        # load data[widx, bidx, side, eval_wdl]
        self.model = model
        self.data: list[tuple[list[int], list[int], bool, float]] = []
        with open(filepath) as file:
            header = next(file)
            if header != "fen,wdl,eval\n":
                raise KeyError(
                    "Unsupported training data format. "
                    "Input should be a csv with keys fen,wdl,eval"
                )
            for line in file:
                fen, wdl, eval_raw = line.strip().split(",")
                eval_wdl = sigmoid_k(int(eval_raw), model.WDL_SCALE)
                side, widx, bidx = model.get_features(fen)
                wdl = float(wdl) if side else 1.0 - float(wdl)
                eval_combined = lmbda * wdl + (1.0 - lmbda) * eval_wdl
                self.data.append((widx, bidx, side, eval_combined))

    def __getitem__(self, index: int):
        """Returns the features and labels

        Args:
            index (int): dataset index

        Returns:
            tuple: ((white_features, black_features, side), label)
        """
        widx, bidx, side, eval_wdl = self.data[index]
        wf = torch.zeros(self.model.N_INPUTS)
        bf = torch.zeros(self.model.N_INPUTS)
        wf[widx] = 1
        bf[bidx] = 1
        return (wf, bf, side), eval_wdl

    def __len__(self):
        return len(self.data)


def get_dataloader(
    dataset_path: Path,
    model: NNUE,
    batch_size: int,
    lmbda: float,
    shuffle: bool,
    repeat: bool,
    start_superbatch: int = 0,
) -> Generator[DataLoader, None, None]:
    """Yields each dataloader in the dataset path.
    If repeat is True, it will restart from the first superbatch after iterating
    through the whole directory.

    Args:
        dataset_path (Path): path to the directory containing the data files
        model (NNUE): NNUE model, used to generate the features
        batch_size (int): batch size for the dataloader
        lmbda (float): ratio of wdl to eval in training label
        shuffle (bool): whether to shuffle the data points in each dataloader
        repeat (bool): whether to loop through the superbatches endlessly
        start_superbatch (int): superbatch number to start from

    Yields:
        DataLoader: the dataloader for that superbatch
    """
    # get number of files in directory
    count = len(list(dataset_path.glob("*.csv")))

    # iterate
    b = start_superbatch
    while True:
        if b >= count and not repeat:
            break

        filename = f"{(b % count)+1}.csv"
        filepath = dataset_path / filename

        # get dataloader
        dataset = NNUEDataSet(model, filepath, lmbda)
        dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=shuffle)
        yield dataloader

        b += 1
