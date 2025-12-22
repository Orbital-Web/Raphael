import argparse
import subprocess
import time
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd
import torch
import yaml
from net import load_model


class NNUETester:
    def __init__(self, nnuepath: Path, datapath: Path, datasize: int):
        # load checkpoint
        print(f"Loading {nnuepath}")
        config_path = nnuepath / "config.yaml"
        checkpoint = torch.load(nnuepath / "latest.pth")
        with config_path.open("r") as f:
            config: dict[str, Any] = list(yaml.load_all(f, yaml.SafeLoader))[0]
            assert config["config_version"] >= 1.0
            model_config: dict[str, Any] = config["model"]

        # load model
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.model = load_model(model_config)
        self.model.to(self.device)
        self.model.load_state_dict(checkpoint["model_state_dict"])
        self.model.eval()
        self.quantized_params = self.model.get_quantized_parameters()
        self.nnuepath = "./" + (nnuepath / "latest.nnue").as_posix()

        # initialize input and output structures
        self.data = pd.read_csv(datapath).dropna().head(datasize).reset_index(drop=True)
        self.data[["side", "widx", "bidx"]] = (
            self.data["fen"].apply(self.model.get_features).apply(pd.Series)
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
            ["./nnuetest", self.nnuepath],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=0,
        )
        while True:
            if proc.poll() is not None:
                stderr = proc.stderr.read()
                raise RuntimeError(f"nnuetest exited with error:\n{stderr}")
            if "start typing commands:" in proc.stdout.readline().lower():
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
            wf = torch.zeros(self.model.N_INPUTS)
            bf = torch.zeros(self.model.N_INPUTS)
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
        w0, b0, w1, b1 = self.quantized_params
        qa, qb = self.model.QA, self.model.QB

        # get unquantized output
        with torch.no_grad():
            wfd = wf.to(self.device).unsqueeze(0)
            bfd = bf.to(self.device).unsqueeze(0)
            sided = torch.tensor([side]).to(self.device)
            out = self.model(wfd, bfd, sided)
            out = float(out[0, 0])

        # get quantized output
        w = w0.T @ wf.detach().numpy().astype(np.int32) + b0
        b = w0.T @ bf.detach().numpy().astype(np.int32) + b0
        assert np.all(np.iinfo(np.int16).min <= w)
        assert np.all(np.iinfo(np.int16).max >= w)
        assert np.all(np.iinfo(np.int16).min <= b)
        assert np.all(np.iinfo(np.int16).max >= b)

        accumulator = np.expand_dims(
            np.concatenate([w, b]) if side else np.concatenate([b, w]), 0
        )
        accumulator = np.clip(accumulator, 0, qa)
        out_q = np.multiply(w1.astype(np.int32), accumulator.astype(np.int32))
        out_q = out_q.astype(np.int32) @ accumulator.T.astype(np.int32)
        out_q = qa * b1.astype(np.int32) + out_q

        # ensure c-like division for negative values
        out_q = (out_q / qa).astype(np.int32) * self.model.OUTPUT_SCALE / (qa * qb)
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
                "Fail: inconsistent output between cpp and python NNUE evaluation "
                f"for fen {fen}: (cpp) {eval_cpp} != {eval_py} (py)"
            )
        print("  Passed\n")

    def test_quantization(self):
        """Computes and evaluates quantization error."""
        print("Testing quantization error...")
        self.eval_with_py()

        evals = np.array(self.eval_pys)
        evalqs = np.array(self.evalq_pys)
        abs_errors = np.abs(evals - evalqs)
        rpd_errors = abs_errors / (0.5 * np.abs(evals + evalqs)) * 100
        print(f"  Mean error: {abs_errors.mean():.4f}")
        print(f"  Max error:  {abs_errors.max():.4f}")
        print(f"  Mean relative percentage difference: {rpd_errors.mean():.4f}%")
        assert abs_errors.mean() < 8, (
            "Fail: quantization error too large, perhaps there's overflow, "
            "incorrect scaling, or an incorrect implementation"
        )
        print("  Passed\n")

    def test_evaluation(self):
        """Computes and evaluates evaluation error."""
        print("Testing evaluation accuracy...")
        self.eval_with_py()

        evals_true = self.data["eval"].to_numpy()
        evals_guess = np.array(self.evalq_pys)
        abs_errors = np.abs(evals_true - evals_guess)
        rpd_errors = abs_errors / (0.5 * np.abs(evals_true + evals_guess)) * 100
        print(f"  Mean error: {abs_errors.mean():.4f}")
        print(f"  Mean relative percentage difference: {rpd_errors.mean():.4f}%")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "path",
        nargs="?",
        default="lastest trainining session",
        type=str,
        help="Path to trained checkpoint folder",
    )
    parser.add_argument(
        "-s",
        "--datasize",
        default=1000,
        type=int,
        help="Number of positions to use during testing. Greater values provide more coverage but is also slower",
    )
    args = parser.parse_args()

    # find last training checkpoint
    if args.path == "lastest trainining session":
        dirs = sorted(
            [entry for entry in Path(".").iterdir() if entry.name.startswith("train-")],
            reverse=True,
        )
        if not dirs:
            raise ValueError(f"No training checkpoint found in {Path(".").as_posix()}")
        args.path = dirs[0]
    else:
        args.path = Path(args.path)

    # start tester
    tester = NNUETester(args.path, Path("dataset/eval/test/1.csv"), args.datasize)

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
            side, widx, bidx = tester.model.get_features(command)
            wf = torch.zeros(tester.model.N_INPUTS)
            bf = torch.zeros(tester.model.N_INPUTS)
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
