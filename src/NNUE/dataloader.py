import warnings

import chess
import numpy as np
import pandas as pd
import torch
import torch.nn.functional as F
from scipy.optimize import curve_fit
from torch.utils.data import Dataset

from .net import NNUEParams


class NNUEDataSet(Dataset):
    def __init__(
        self,
        params: NNUEParams,
        in_filepath: str = None,
        out_filepath: str = None,
        optimize: bool = True,
    ):
        """Loads dataset from filepath and computes the optimum WDL_SCALE, storing it
        in self.params.WDL_SCALE.

        Args:
            in_filepath (str, optional). If provided, will load from this file and save
                the processed data to out_filepath (if provided). Should be a csv file
                with columns fen (position), wdl (1 white, 0.5 draw, 0 black), and eval
                (in centipawns).
            out_filepath (str, optional): If in_filepath is provided, the loaded data
                will be saved to this path. If only out_filepath is provided, it will
                directly set self.data to the contents of this csv file. The file should
                be a csv file with columns fen, wdl, eval, side (1 white, 0 black), widx
                (white feature index), and bidx (black feature index).
            optimize (bool, optional): Whether to optimize the training set based on the
                following criteria. May reduce dataset size.
                    - side should average to around 0.5
                    - eval should average to around 0 and have roughly equal +:- ratio
                    - at least 50% of evaluations should be between -100 and 100
                    - at least 40% of evaluations should be outside -100 and 100
                Defaults to True.
        """
        if in_filepath is None and out_filepath is None:
            raise ValueError("Either in_filepath or out_filepath must be provided")

        self.params = params
        self.n_inputs = params.N_INPUTS
        self.stats = {}

        # load data
        self.data: pd.DataFrame = None  # cols: fen, wdl, eval, side, widx, bidx
        if in_filepath is not None:
            print(f"Loading dataset from {in_filepath}...")
            self.data = pd.read_csv(self.filepath, names=["fen", "wdl", "eval"])
            self.data[["side", "widx", "bidx"]] = (
                self.data["fen"].apply(self._get_features).apply(pd.Series)
            )
            if out_filepath is not None:
                print(f"Saving processed dataset to {out_filepath}...")
                self.data.to_csv(out_filepath, sep=",")
        else:
            print(f"Loading processed dataset from {out_filepath}...")
            self.data = pd.read_csv(
                self.filepath, names=["fen", "wdl", "eval", "side", "widx", "bidx"]
            )
        assert set(self.data.columns) == {"fen", "wdl", "eval", "side", "widx", "bidx"}

        # optimize dataset
        if optimize:
            old_datasize = len(self.data)
            self.optimize()
            new_datasize = len(self.data)
            if new_datasize < old_datasize:
                warnings.warn(
                    f"Warning: Data optimization reduced dataset size from {old_datasize} to {new_datasize}"
                )
            if out_filepath is not None:
                print(f"Saving optimized dataset to {out_filepath}...")
                self.data.to_csv(out_filepath, sep=",")

        # output data statistics
        self.compute_stats()
        self.print_stats()

        # convert labels to WDL scale
        self.compute_wdl()
        self.data["eval"] = F.softmax(self.data["eval"] / self.params.WDL_SCALE, dim=0)

    def optimize(self):
        print("Optimizing dataset...")

        # requires stats to optimize
        if not self.stats:
            self.compute_stats()

        # do stuff
        raise NotImplementedError()

        self.compute_stats()

    def compute_stats(self):
        # compute size, side, eval, eval_ratio, eval_low, eval_high
        raise NotImplementedError()

    def print_stats(self):
        if not self.stats:
            self.compute_stats()

        print("################# Dataset statistics #################")
        print(f"Dataset size: {self.stats["size"]}")
        print(f"Average side to move: {self.stats["side"]:.2f}")
        print(f"Average eval: {self.stats["eval"]:.2f}")
        print(f"Eval +:- ratio: {self.stats["eval_ratio"]:.2f}")
        print(f"Percentage of eval between ±100: {self.stats["eval_low"]:.2f}")
        print(f"Percentage of eval outside ±100: {self.stats["eval_high"]:.2f}")
        print("")

    def compute_wdl(self):
        data = self.data[["eval", "wdl"]].copy()
        data["eval_bucket"] = (data["eval"] // 10) * 10
        data = data.groupby("eval_bucket")["wdl"].mean().reset_index()

        x = data["eval_bucket"]
        y = data["wdl"]

        def sigmoid(eval, k):
            return 1 / (1 + np.exp(-eval / k))

        params, _ = curve_fit(sigmoid, x, y, p0=self.params.WDL_SCALE)
        self.params.WDL_SCALE = params[0]
        print(f"Found optimum WDL_SCALE: {self.params.WDL_SCALE:.2f}")

        # TODO: maybe graph it

    def _get_features(self, fen: str) -> tuple[bool, list[int], list[int]]:
        """Computes the white and black feature index of the current position along with
        the side to move.

        Args:
            fen (str): the position

        Returns:
            tuple[bool, list[int], list[int]]: side to move (True for white) + white and
                black feature indices
        """
        board = chess.Board(fen)
        wkb = self.params.KING_BUCKETS[board.king(chess.WHITE)]
        bkb = self.params.KING_BUCKETS[chess.square_mirror(board.king(chess.BLACK))]

        widx, bidx = [], []
        for sq, p in board.piece_map().items():
            pc = p.color
            pt = p.piece_type
            if pt == chess.KING:
                continue

            if pc == chess.WHITE:
                pidx = 5 * (pc == chess.BLACK) + pt - 1  # PNBRQ, pnbrq (i.e., us, them)
                widx.append(wkb * 640 + pidx * 64 + sq)
                # TODO: feature factorization (self.N_INPUT + pidx * 64 + sq)
            else:
                pidx = 5 * (pc == chess.WHITE) + pt - 1  # flip color and mirror square
                bidx.append(bkb * 640 + pidx * 64 + chess.square_mirror(sq))
                # TODO: feature factorization (self.N_INPUT + pidx * 64 + sq)
        return board.turn == chess.WHITE, widx, bidx

    def __getitem__(self, index: int):
        """Returns the features and labels

        Args:
            index (int): dataset index

        Returns:
            tuple: ((white_features, black_features), label)
        """
        wf = torch.zeros(self.n_inputs)
        bf = torch.zeros(self.n_inputs)
        wf[self.data["widx"][index]] = 1
        bf[self.data["bidx"][index]] = 1
        return (wf, bf), self.data["eval"][index]

    def __len__(self):
        return len(self.data)
