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


def sigmoid_k(eval, k):
    return 1 / (1 + np.exp(-eval / k))


class NNUEDataSet(Dataset):
    def __init__(
        self,
        params: NNUEParams,
        in_filepath: str,
        out_filepath: str = None,
        optimize: bool = False,
    ):
        """Loads dataset from in_filepath and computes the optimum WDL_SCALE, storing it
        in self.params.WDL_SCALE.

        Args:
            in_filepath (str). Path to the dataset. Should be a csv file with columns
                fen, wdl (1 white, 0.5 draw, 0 black), eval (in centipawns, relative to
                the side to move), and optionally the features side, widx, and bidx.
            out_filepath (str, optional): Path to save the processed dataset to. The
                file will be a csv file with columns fen, wdl, eval, side (1 white,
                0 black), widx (white feature index), and bidx (black feature index).
            optimize (bool, optional): Whether to optimize the training set based on the
                following criteria. May reduce dataset size.
                    - side should average to around 0.5
                    - eval should average to around 0.0
                    - ratio of eval >= 50 to eval <= 50 should be roughly equal
                    - at least 50% of evaluations should be between -100 and 100
                    - at least 40% of evaluations should be outside -100 and 100
                Defaults to False.
        """
        self.params = params
        self.stats = {}

        # load data
        print(f"Loading dataset from {in_filepath}...")
        self.data = pd.read_csv(in_filepath)  # cols: fen, wdl, eval, side, widx, bidx
        if len(diff := {"fen", "wdl", "eval"} - set(self.data.columns)):
            raise KeyError(f"{in_filepath} is missing columns: {diff}")
        if len({"side", "widx", "bidx"} - set(self.data.columns)):
            print("Computing features for dataset...")
            self.data[["side", "widx", "bidx"]] = (
                self.data["fen"].apply(params.get_features).apply(pd.Series)
            )
        else:
            print("Found features in dataset, skipping feature computation")
            self.data["widx"] = self.data["widx"].apply(lambda x: json.loads(x))
            self.data["bidx"] = self.data["bidx"].apply(lambda x: json.loads(x))
        self.data = self.data.dropna()

        # optimize dataset
        if optimize:
            old_datasize = len(self.data)
            self.optimize()
            new_datasize = len(self.data)
            if new_datasize < old_datasize:
                print(
                    "Note: Data optimization reduced dataset size from "
                    f"{old_datasize} to {new_datasize}"
                )

        if out_filepath is not None:
            print(f"Saving processed dataset to {out_filepath}...")
            self.data.to_csv(out_filepath, sep=",", index=False)

        # compute and print data WDL scale and statistics
        self.params.WDL_SCALE = self.compute_wdl(self.data, plot=True)
        self.stats = self.compute_stats(self.data)
        self.print_stats()

        # convert eval to WDL scale
        self.data["eval"] = sigmoid_k(self.data["eval"], self.params.WDL_SCALE)

    def optimize(self):
        print("Optimizing dataset...")

        def get_cost(stats) -> float:
            return (
                abs(stats["eval_mean"])
                + 2 * abs(stats["eval_median"])
                + 2000 * abs(stats["side"] - 0.5)  # 0 - 1000
                + 100 * abs(stats["eval_skew"])  # 0 - 1000
                + 400 * max(0, 50 - stats["eval_low"])  # 0 - 20000
                + 400 * max(0, 40 - stats["eval_high"])  # 0 - 16000
                + 1000 * (len(self.data) - stats["size"]) / len(self.data)  # 0 - 1000
            )

        best_data = self.data
        best_stats = self.stats or self.compute_stats(self.data, recompute_wdl=True)
        best_cost = get_cost(best_stats)
        last_improvement = 0

        for _ in range(1000):
            data = self.data.sample(frac=uniform(0.8, 1.0))
            stats = self.compute_stats(data, recompute_wdl=True)
            cost = get_cost(stats)

            if cost < best_cost:
                best_data = data
                best_stats = stats
                best_cost = cost
                last_improvement = 0

            last_improvement += 1
            if last_improvement > 10:
                break

        self.data = best_data.reset_index(drop=True)
        self.stats = best_stats

    def compute_stats(self, data: pd.DataFrame, recompute_wdl: bool = False) -> dict:
        ws = self.params.WDL_SCALE if not recompute_wdl else self.compute_wdl(data)
        n = 100 / len(data)

        return {
            "size": len(data),
            "side": np.mean(data["side"]),
            "eval_mean": trim_mean(data["eval"], 0.1),
            "eval_median": np.median(data["eval"]),
            "eval_skew": (np.sum(data["eval"] >= 50) - np.sum(data["eval"] <= -50)) * n,
            "eval_low": np.sum(np.abs(data["eval"]) <= ws) * n,
            "eval_high": np.sum(np.abs(data["eval"]) > ws) * n,
        }

    def print_stats(self):
        ws = self.params.WDL_SCALE
        print("Loaded dataset with:")
        print(f"    Dataset size: {self.stats['size']}")
        print(f"    Average side to move: {self.stats['side']:.2f}")
        print(f"    Trimmed mean eval: {self.stats['eval_mean']:.2f}")
        print(f"    Median eval: {self.stats['eval_median']:.2f}")
        print(f"    Eval +:- skew: {self.stats['eval_skew']:.2f}%")
        print(f"    Eval between ±{ws:.2f}: {self.stats['eval_low']:.2f}%")
        print(f"    Eval outside ±{ws:.2f}: {self.stats['eval_high']:.2f}%")
        print("")

    def compute_wdl(self, data: pd.DataFrame, plot: bool = False) -> float:
        data = data[["eval", "wdl", "side"]].copy()
        data.loc[data["side"] == 0, "wdl"] = 1 - data.loc[data["side"] == 0, "wdl"]
        data["eval_bucket"] = (data["eval"] // 50) * 50
        data = data.groupby("eval_bucket")["wdl"].mean().reset_index()

        x = data["eval_bucket"]
        y = data["wdl"]

        params, _ = curve_fit(sigmoid_k, x, y, p0=self.params.WDL_SCALE)
        wdl_scale = params[0]

        if plot:
            print(f"Found optimum WDL_SCALE: {wdl_scale:.2f}")
            x_fit = np.linspace(x.min(), x.max(), 500)
            y_fit = sigmoid_k(x_fit, wdl_scale)
            plt.scatter(x, y, c="blue", label="Train Data")
            plt.plot(
                x_fit,
                y_fit,
                "r-",
                label=f"Fitted with scale={wdl_scale:.2f}",
            )
            plt.xlabel("Eval")
            plt.ylabel("WDL")
            plt.legend()
            plt.title("Eval vs WDL")
            plt.savefig("wdl_fit.png")
            plt.close()

        return wdl_scale

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
