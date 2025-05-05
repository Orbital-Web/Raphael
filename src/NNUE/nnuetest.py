import argparse
import subprocess
import time
from pathlib import Path

import numpy as np
import pandas as pd
import torch
from net import NNUE, NNUEParams


class NNUETester:
    def __init__(
        self,
        nnuepath: str,
        datapath: str,
        feature_factorize: bool,
        datasize: int,
    ):
        # load model
        print(
            f"Loading {nnuepath} "
            f"{'(feature factorization enabled)' if feature_factorize else ''}"
        )
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.params = NNUEParams(FEATURE_FACTORIZE=feature_factorize)
        self.model = NNUE(self.params)
        self.model.load_state_dict(
            torch.load(
                nnuepath,
                weights_only=False,
                map_location=self.device,
            )["model_state_dict"]
        )
        self.model.eval()
        self.quantized_params = self.model.get_quantized_parameters()

        # initialize input and output structures
        self.data = pd.read_csv(datapath).dropna().head(datasize).reset_index(drop=True)
        self.data[["side", "widx", "bidx"]] = (
            self.data["fen"].apply(self.params.get_features).apply(pd.Series)
        )
        self.fens = self.data["fen"].tolist()
        self.cpp_runtime: float = 0
        self.evalq_cpps: list[int] = []
        self.evalq_pys: list[int] = []
        self.eval_pys: list[float] = []

    def eval_with_cpp(self):
        """Populates evalq_cpps with the cpp NNUE evaluations of the data if evalq_cpps
        is empty and measures the runtime. Raises an error if the cpp NNUE crashes.
        """
        if self.evalq_cpps:
            return

        # start C++ nnue process
        proc = subprocess.Popen(
            ["./nnuerun"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=0,
        )
        while True:
            if "enter a fen" in proc.stdout.readline().lower():
                break

        # evaluate with C++ nnue
        inputs = "\n".join(self.fens) + "\n"
        start = time.perf_counter()
        stdout_data, stderr_data = proc.communicate(input=inputs)
        self.cpp_runtime = (time.perf_counter() - start) * 1000
        if stderr_data:
            raise RuntimeError(f"nnuerun crashed with error: {stderr_data.decode()}")
        self.evalq_cpps = [int(out[6:]) for out in stdout_data.splitlines()]

    def eval_with_py(self):
        """Populates evalq_pys and eval_pys with the quantized and unquantized python
        NNUE evaluations of the data if the two lists are empty.
        """
        if self.evalq_pys and self.eval_pys:
            return

        # evaluate with python
        for _, row in self.data.iterrows():
            wf = torch.zeros(self.params.N_INPUTS_FACTORIZED)
            bf = torch.zeros(self.params.N_INPUTS_FACTORIZED)
            wf[row["widx"]] = 1
            bf[row["bidx"]] = 1
            side = row["side"]

            out, out_q = self.eval_with_py_one(wf, bf, side)
            self.eval_pys.append(out)
            self.evalq_pys.append(out_q)

    def eval_with_py_one(
        self, wf: torch.Tensor, bf: torch.Tensor, side: bool
    ) -> tuple[float, int]:
        """Compute the unquantized and quantized python evaluation for the given input.

        Args:
            wf (torch.Tensor): white features of size N_INPUTS_FACTORIZED
            bf (torch.Tensor): black features of size N_INPUTS_FACTORIZED
            side (bool): perspective to evaluate from, True for white, False for black

        Returns:
            tuple[float, int]: the unquantized and quantized evaluations
        """
        w0, b0, w1, b1, w2, b2, w3, b3 = self.quantized_params
        q1, q2, q3 = self.params.QLEVEL1, self.params.QLEVEL2, self.params.QLEVEL3

        # get unquantized output
        out = self.model(wf.unsqueeze(0), bf.unsqueeze(0), torch.tensor([side]))
        out = self.params.WDL_SCALE * torch.log(out / (1 - out))  # undo sigmoid
        out = float(out[0, 0])

        # get quantized output
        w = w0.T @ wf[: self.params.N_INPUTS].detach().numpy().astype(np.int16) + b0
        b = w0.T @ bf[: self.params.N_INPUTS].detach().numpy().astype(np.int16) + b0
        accumulator = np.expand_dims(
            np.concatenate([w, b]) if side else np.concatenate([b, w]), 0
        )
        out_q = np.clip(accumulator, 0, 127).astype(np.int32)
        out_q = np.clip((out_q @ w1.T + b1) >> q1, 0, 127).astype(np.int32)
        out_q = np.clip((out_q @ w2.T + b2) >> q2, 0, 127).astype(np.int32)
        out_q = (out_q @ w3.T + b3) >> q3
        out_q = int(out_q[0, 0])
        return out, out_q

    def test_consistency(self):
        """Tests consistency between the cpp and python NNUE."""
        print("Testing consistency...")
        self.eval_with_cpp()
        self.eval_with_py()

        print(f"  Cpp NNUE runtime: {self.cpp_runtime:.3f}ms")
        for eval_cpp, eval_py, fen in zip(self.evalq_cpps, self.evalq_pys, self.fens):
            assert eval_cpp == eval_py, (
                "Inconsistent output between cpp and python NNUE evaluation for fen "
                f"{fen}: (cpp) {eval_cpp} != {eval_py} (py)"
            )
        print("  Passed\n")

    def test_quantization(self):
        """Computes and evaluates quantization error."""
        print("Testing quantization error...")
        self.eval_with_py()

        evals = np.array(self.eval_pys)
        evalqs = np.array(self.evalq_pys)
        abs_errors = np.abs(evals - evalqs)
        percent_errors = np.divide(abs_errors, np.abs(evals) + 1e-2) * 100
        print(f"  Mean error: {abs_errors.mean():.4f}")
        print(f"  Mean percent error: {percent_errors.mean():.4f}%")
        assert percent_errors.mean() < 8, (
            "Quantization error too large, perhaps there's overflow, "
            "incorrect scaling, or an incorrect factorization implementation"
        )
        print("  Passed\n")

    def test_evaluation(self):
        """Computes and evaluates evaluation error."""
        print("Testing evaluation accuracy...")
        self.eval_with_py()

        evals_true = self.data["eval"].to_numpy()
        evals_guess = np.array(self.evalq_pys)
        abs_errors = np.abs(evals_true - evals_guess)
        percent_errors = np.divide(abs_errors, np.abs(evals_true) + 1e-2) * 100
        print(f"  Mean error: {abs_errors.mean():.4f}")
        print(f"  Mean percent error: {percent_errors.mean():.4f}%\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "path",
        nargs="?",
        default="best.pth from the lastest trainining session",
        type=str,
        help="Path to trained pth file.",
    )
    parser.add_argument(
        "-f",
        "--feature_factorize",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Whether to the trained model uses feature factorization",
    )
    parser.add_argument(
        "-s",
        "--datasize",
        default=1000,
        type=int,
        help="Number of positions to use during testing. Greater values provide more coverage but is also slower",
    )
    args = parser.parse_args()

    # find last training file
    if args.path == "best.pth from the lastest trainining session":
        for entry in Path(".").iterdir():
            if entry.is_dir():
                file = entry / "best.pth"
                if file.exists() and str(file) > args.path:
                    args.path = file.as_posix()

    # start tester
    tester = NNUETester(
        args.path, "traindata.csv", args.feature_factorize, args.datasize
    )

    print(
        "Commands:\n"
        "  FEN: get eval, quantized eval, and quantization error\n"
        "  ta: test all\n"
        "  tc: test consistency\n"
        "  tq: test quantization\n"
        "  te: test evaluation\n"
        "  q: quit\n\n"
        "Start typing commands:"
    )
    while True:
        command = input("")
        if command == "q":
            break
        elif command == "tc":
            tester.test_consistency()
        elif command == "tq":
            tester.test_quantization()
        elif command == "te":
            tester.test_evaluation()
        elif command == "ta":
            tester.test_consistency()
            tester.test_quantization()
            tester.test_evaluation()
        else:
            side, widx, bidx = tester.params.get_features(command)
            wf = torch.zeros(tester.params.N_INPUTS_FACTORIZED)
            bf = torch.zeros(tester.params.N_INPUTS_FACTORIZED)
            wf[widx] = 1
            bf[bidx] = 1
            out, out_q = tester.eval_with_py_one(wf, bf, side)
            q_error = abs(out - out_q)
            q_error_percent = 100 * q_error / (abs(out) + 1e-2)
            print(
                f"  eval:      {out:.4f}\n"
                f"  quantized: {out_q}\n"
                f"  error: {q_error:.4f} ({q_error_percent:.4f}%)"
            )
