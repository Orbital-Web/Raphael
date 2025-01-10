import json
from random import uniform

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import torch
from net import NNUEParams
from scipy.optimize import curve_fit
from scipy.stats import trim_mean
from torch.utils.data import Dataset


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
                (in centipawns, relative to the side to move).
            out_filepath (str, optional): If in_filepath is provided, the loaded data
                will be saved to this path. If only out_filepath is provided, it will
                directly set self.data to the contents of this csv file. The file should
                be a csv file with columns fen, wdl, eval, side (1 white, 0 black), widx
                (white feature index), and bidx (black feature index).
            optimize (bool, optional): Whether to optimize the training set based on the
                following criteria. May reduce dataset size.
                    - side should average to around 0.5
                    - eval should average to around 0.0
                    - ratio of eval >= 50 to eval <= 50 should be roughly equal
                    - at least 50% of evaluations should be between -100 and 100
                    - at least 40% of evaluations should be outside -100 and 100
                Defaults to True.
        """
        if in_filepath is None and out_filepath is None:
            raise ValueError("Either in_filepath or out_filepath must be provided")

        self.params = params
        self.stats = {}

        # load data
        self.data: pd.DataFrame = None  # cols: fen, wdl, eval, side, widx, bidx
        if in_filepath is not None:
            print(f"Loading dataset from {in_filepath}...")
            self.data = pd.read_csv(in_filepath)
            if len(diff := {"fen", "wdl", "eval"} - set(self.data.columns)):
                print(f"{in_filepath} is missing columns: {diff}")
            self.data[["side", "widx", "bidx"]] = (
                self.data["fen"].apply(params.get_features).apply(pd.Series)
            )
        else:
            print(f"Loading processed dataset from {out_filepath}...")
            self.data = pd.read_csv(out_filepath)
            if len(
                diff := {"fen", "wdl", "eval", "side", "widx", "bidx"}
                - set(self.data.columns)
            ):
                print(f"{out_filepath} is missing columns: {diff}")
            self.data["widx"] = self.data["widx"].apply(lambda x: json.loads(x))
            self.data["bidx"] = self.data["bidx"].apply(lambda x: json.loads(x))

        # optimize dataset
        if optimize:
            old_datasize = len(self.data)
            self.optimize()
            new_datasize = len(self.data)
            if new_datasize < old_datasize:
                print(
                    f"Note: Data optimization reduced dataset size from {old_datasize} to {new_datasize}"
                )

        if out_filepath is not None:
            print(f"Saving processed dataset to {out_filepath}...")
            self.data.to_csv(out_filepath, sep=",", index=False)

        # output data statistics
        self.stats = self.compute_stats(self.data)
        self.print_stats()

        # convert labels to WDL scale
        self.compute_wdl()
        self.data["eval"] = self.sigmoid_k(self.data["eval"], self.params.WDL_SCALE)

    def optimize(self):
        print("Optimizing dataset...")

        def get_cost(stats) -> float:
            return (
                abs(stats["eval_mean"])
                + 2 * abs(stats["eval_median"])
                + 2000 * abs(stats["side"] - 0.5)  # 0 - 1000
                + 100 * abs(stats["eval_skew"])  # 0 - 1000
                + 100 * max(0, 50 - stats["eval_low"])  # 0 - 5000
                + 100 * max(0, 40 - stats["eval_high"])  # 0 - 4000
                + 1000 * (len(self.data) - stats["size"]) / len(self.data)  # 0 - 1000
            )

        best_data = self.data
        best_stats = self.stats or self.compute_stats(self.data)
        best_cost = get_cost(best_stats)

        for _ in range(100):
            data = self.data.sample(frac=uniform(0.8, 1.0))
            stats = self.compute_stats(data)
            cost = get_cost(stats)

            if cost < best_cost:
                best_data = data
                best_stats = stats
                best_cost = cost

        self.data = best_data.reset_index(drop=True)
        self.stats = best_stats

    def compute_stats(self, data: pd.DataFrame) -> dict:
        return {
            "size": len(data),
            "side": np.mean(data["side"]),
            "eval_mean": trim_mean(data["eval"], 0.1),
            "eval_median": np.median(data["eval"]),
            "eval_skew": (np.sum(data["eval"] >= 50) - np.sum(data["eval"] <= -50))
            * 100
            / len(data),
            "eval_low": np.sum(np.abs(data["eval"]) <= 100) * 100 / len(data),
            "eval_high": np.sum(np.abs(data["eval"]) > 100) * 100 / len(data),
        }

    def print_stats(self):
        if not self.stats:
            self.stats = self.compute_stats(self.data)

        print("Loaded dataset with:")
        print(f"    Dataset size: {self.stats['size']}")
        print(f"    Average side to move: {self.stats['side']:.2f}")
        print(f"    Trimmed mean eval: {self.stats['eval_mean']:.2f}")
        print(f"    Median eval: {self.stats['eval_median']:.2f}")
        print(f"    Eval +:- skew: {self.stats['eval_skew']:.2f}%")
        print(f"    Perentage of eval between ±100: {self.stats['eval_low']:.2f}%")
        print(f"    Percentage of eval outside ±100: {self.stats['eval_high']:.2f}%")
        print("")

    def compute_wdl(self):
        data = self.data[["eval", "wdl", "side"]].copy()
        data.loc[data["side"] == 0, "eval"] *= -1  # make perspective absolute
        data["eval_bucket"] = (data["eval"] // 50) * 50
        data = data.groupby("eval_bucket")["wdl"].mean().reset_index()

        x = data["eval_bucket"]
        y = data["wdl"]

        params, _ = curve_fit(self.sigmoid_k, x, y, p0=self.params.WDL_SCALE)
        self.params.WDL_SCALE = params[0]
        print(f"Found optimum WDL_SCALE: {self.params.WDL_SCALE:.2f}")

        # save graph of fit
        x_fit = np.linspace(x.min(), x.max(), 500)
        y_fit = self.sigmoid_k(x_fit, self.params.WDL_SCALE)
        plt.scatter(x, y, c="blue", label="Train Data")
        plt.plot(
            x_fit, y_fit, "r-", label=f"Fitted with scale={self.params.WDL_SCALE:.2f}"
        )
        plt.xlabel("Eval")
        plt.ylabel("WDL")
        plt.legend()
        plt.title("Eval vs WDL")
        plt.savefig("wdl_fit.png")
        plt.close()

    @staticmethod
    def sigmoid_k(eval, k):
        return 1 / (1 + np.exp(-eval / k))

    def __getitem__(self, index: int):
        """Returns the features and labels

        Args:
            index (int): dataset index

        Returns:
            tuple: ((white_features, black_features, side), label)
        """
        wf = torch.zeros(self.params.N_INPUTS_FACTORIZED)
        bf = torch.zeros(self.params.N_INPUTS_FACTORIZED)
        wf[self.data["widx"][index]] = 1
        bf[self.data["bidx"][index]] = 1
        return (wf, bf, self.data["side"][index]), self.data["eval"][index]

    def __len__(self):
        return len(self.data)
