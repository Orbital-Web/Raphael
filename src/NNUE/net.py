import warnings
from dataclasses import dataclass, field

import chess
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim


@dataclass
class NNUEParams:
    N_BUCKETS: int = 16  # number of king buckets
    KING_BUCKETS: list[int] = field(
        default_factory=lambda: [
            0,  1,  2,  3,  3,  2,  1,  0,   # A1, B1, ...
            4,  5,  6,  7,  7,  6,  5,  4,   #
            8,  9,  10, 11, 11, 10, 9,  8,   #
            8,  9,  10, 11, 11, 10, 9,  8,   #
            12, 12, 13, 13, 13, 13, 12, 12,  #
            12, 12, 13, 13, 13, 13, 12, 12,  #
            14, 14, 15, 15, 15, 15, 14, 14,  #
            14, 14, 15, 15, 15, 15, 14, 14,  # A8, B8, ...
        ]
    )  # fmt: skip
    N_HIDDEN0: int = 256  # accumulator size
    N_HIDDEN1: int = 32
    N_HIDDEN2: int = 32
    QLEVEL1: int = 6
    QLEVEL2: int = 6
    QLEVEL3: int = 5
    OUTPUT_SCALE: int = 300  # scaling applied to output
    WDL_SCALE: float = 200  # scaling such that sigmoid(eval/WDL_SCALE) ≈ WDL
    FEATURE_FACTORIZE: bool = False

    @property
    def N_INPUTS(self) -> int:
        return self.N_BUCKETS * 12 * 64  # using bucketed half-ka

    @property
    def N_INPUTS_FACTORIZED(self) -> int:
        return self.N_INPUTS + self.FEATURE_FACTORIZE * 10 * 64

    def get_features(self, fen: str) -> tuple[bool, list[int], list[int]]:
        """Computes the white and black feature index of the current position along with
        the side to move.

        Args:
            fen (str): the position

        Returns:
            tuple[bool, list[int], list[int]]: side to move (True for white) + white and
                black feature indices
        """
        board = chess.Board(fen)
        wkb = self.KING_BUCKETS[board.king(chess.WHITE)]
        bkb = self.KING_BUCKETS[chess.square_mirror(board.king(chess.BLACK))]

        widx, bidx = [], []
        for sq, p in board.piece_map().items():
            pc = p.color
            pt = p.piece_type - 1  # 1 indexed

            # our pnbrqk, their pnbrqk
            wp = 6 * (pc == chess.BLACK) + pt
            bp = 6 * (pc == chess.WHITE) + pt

            widx.append(wkb * 12 * 64 + wp * 64 + sq)
            bidx.append(bkb * 12 * 64 + bp * 64 + chess.square_mirror(sq))

            if self.FEATURE_FACTORIZE and pt + 1 != chess.KING:
                # 5 since we don't factorize the king
                wpf = 5 * (pc == chess.BLACK) + pt
                bpf = 5 * (pc == chess.WHITE) + pt

                widx.append(self.N_BUCKETS + wpf * 64 + sq)
                bidx.append(self.N_INPUTS + bpf * 64 + chess.square_mirror(sq))
        return board.turn == chess.WHITE, widx, bidx


# https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#training-a-net-with-pytorch
class NNUE(nn.Module):
    def __init__(self, params: NNUEParams):
        super().__init__()
        self.params: NNUEParams = params
        self.qscale1 = 1 << self.params.QLEVEL1
        self.qscale2 = 1 << self.params.QLEVEL2
        self.qscale3 = 1 << self.params.QLEVEL3

        self.ft = nn.Linear(params.N_INPUTS_FACTORIZED, params.N_HIDDEN0)
        self.l1 = nn.Linear(2 * params.N_HIDDEN0, params.N_HIDDEN1)
        self.l2 = nn.Linear(params.N_HIDDEN1, params.N_HIDDEN2)
        self.l3 = nn.Linear(params.N_HIDDEN2, 1)

        # NOTE: after quantization
        # q0 = crelu[(127W0)*x + (127b0)] ≡ 127o0
        # q1 = crelu[[(qscale1*W1)*q0 + (qscale1*127b1)] / qscale1] ≡ 127o1
        # q2 = crelu[[(qscale2*W2)*q1 + (qscale2*127b1)] / qscale2] ≡ 127o2
        # q3 = [(oscale*qscale3*W3/127)*q2 + (oscale*qscale3*b1)] / qscale3 ≡ oscale*o3

        self.export_options = {
            "ft": {
                "wk": 127,
                "bk": 127,
                "wt": np.int16,
                "bt": np.int16,
                "t": True,  # transpose ft.weight to make it N_HIDDEN0 x N_INPUT
            },
            "l1": {
                "wk": self.qscale1,
                "bk": self.qscale1 * 127,
                "wt": np.int8,
                "bt": np.int32,
                "t": False,
            },
            "l2": {
                "wk": self.qscale2,
                "bk": self.qscale2 * 127,
                "wt": np.int8,
                "bt": np.int32,
                "t": False,
            },
            "l3": {
                "wk": self.params.OUTPUT_SCALE * self.qscale3 / 127,
                "bk": self.params.OUTPUT_SCALE * self.qscale3,
                "wt": np.int8,
                "bt": np.int32,
                "t": False,
            },
        }

        # initialize weights
        torch.nn.init.kaiming_normal_(self.ft.weight, nonlinearity="relu")
        torch.nn.init.normal_(self.l1.weight, mean=0, std=32 / self.qscale1)
        torch.nn.init.normal_(self.l2.weight, mean=0, std=32 / self.qscale2)
        torch.nn.init.normal_(
            self.l3.weight, mean=0, std=2048 / (self.params.OUTPUT_SCALE * self.qscale3)
        )
        torch.nn.init.normal_(self.ft.bias, mean=0, std=0.5)
        torch.nn.init.normal_(self.l1.bias, mean=0, std=0.5)
        torch.nn.init.normal_(self.l2.bias, mean=0, std=0.5)
        torch.nn.init.normal_(self.l3.bias, mean=0, std=0.5)

        # print statistics
        print("Initialized NNUE with:")
        print("  Layers:")
        for name, layer in self.named_children():
            ws1, ws2 = layer.weight.shape
            bs = layer.bias.shape[0]
            if self.export_options[name]["t"]:
                ws1, ws2 = ws2, ws1
            wt = self.export_options[name]["wt"]
            bt = self.export_options[name]["bt"]
            print(f"    {name}.weight {wt.__name__}_t[{ws1} * {ws2}]")
            print(f"    {name}.bias   {bt.__name__}_t[{bs}]")
        print("  Quantization")
        print(f"    Q1: {self.params.QLEVEL1} (±{(127 / self.qscale1):.2f})")
        print(f"    Q2: {self.params.QLEVEL2} (±{(127 / self.qscale2):.2f})")
        print(
            f"    Q3: {self.params.QLEVEL3} (±{(127 * 127 / (self.params.OUTPUT_SCALE * self.qscale3)):.2f})"
        )
        print("  Training Options:")
        print(f"    Feature Factorization: {self.params.FEATURE_FACTORIZE}")
        print(f"    Output Scale: {self.params.OUTPUT_SCALE}")
        print("")

    def forward(self, white_features, black_features, side: bool) -> list[float]:
        """Evaluates using the model

        Args:
            white_features: a batch of white features size N_INPUTS_FACTORIZED
            black_features: a batch of black features size N_INPUTS_FACTORIZED
            side (bool): True for white, False for black

        Returns:
            list[float]: a batch of evaluations from the side to move
        """
        w = self.ft(white_features)
        b = self.ft(black_features)
        accumulator = torch.cat([w, b], dim=1) * side.unsqueeze(1)
        accumulator += torch.cat([b, w], dim=1) * torch.logical_not(side).unsqueeze(1)

        # layers + crelu
        o0 = torch.clamp(accumulator, 0.0, 1.0)
        o1 = torch.clamp(self.l1(o0), 0.0, 1.0)
        o2 = torch.clamp(self.l2(o1), 0.0, 1.0)
        o3 = self.l3(o2) * self.params.OUTPUT_SCALE
        return torch.sigmoid((o3 / self.params.WDL_SCALE).double())

    def parameters(self):
        """Returns overwritten parameters with min and max clamp range to account for
        quantization
        """
        # the feature transformer and bias use int16_t/int32_t with no risk of overflow
        return [
            {"params": [self.ft.weight], "name": "ft.weight"},
            {"params": [self.ft.bias], "name": "ft.bias"},
            {  # must fit in int8_t after multiplying by qscale1
                "params": [self.l1.weight],
                "min_weight": -127 / self.qscale1,
                "max_weight": 127 / self.qscale1,
                "name": "l1.weight",
            },
            {"params": [self.l1.bias], "name": "l1.bias"},
            {  # must fit in int8_t after multiplying by qscale2
                "params": [self.l2.weight],
                "min_weight": -127 / self.qscale2,
                "max_weight": 127 / self.qscale2,
                "name": "l2.weight",
            },
            {"params": [self.l2.bias], "name": "l2.bias"},
            {  # must fit in int8_t after multiplying by OUTPUT_SCALE * qscale3 / 127
                "params": [self.l3.weight],
                "min_weight": -127 * 127 / (self.params.OUTPUT_SCALE * self.qscale3),
                "max_weight": 127 * 127 / (self.params.OUTPUT_SCALE * self.qscale3),
                "name": "l3.weight",
            },
            {"params": [self.l3.bias], "name": "l3.bias"},
        ]

    def export(self, filepath: str):
        """Exports the model weights to the filepath after applying quantization. Should
        only be used to port the model to C++, and not for saving the training state.

        Args:
            filepath (str): filepath to save model to
        """
        print(f"Exporting model to {filepath}")

        with open(filepath, "wb") as f:
            for name, layer in self.named_children():
                opt = self.export_options[name]

                # scale weight and bias
                w = layer.weight.detach().cpu().numpy() * opt["wk"]
                b = layer.bias.detach().cpu().numpy() * opt["bk"]

                # remove factorized features
                if self.params.FEATURE_FACTORIZE and name == "ft":
                    w = w[:, : self.params.N_INPUTS]

                # clamp and convert type
                wt = opt["wt"]
                bt = opt["bt"]
                wc = np.clip(w, np.iinfo(wt).min, np.iinfo(wt).max).astype(wt)
                bc = np.clip(b, np.iinfo(bt).min, np.iinfo(bt).max).astype(bt)
                wclipped = np.sum(wc != w.astype(wt))
                bclipped = np.sum(bc != b.astype(bt))
                if wclipped > 0:
                    warnings.warn(
                        f"Warning: {wclipped} overflows were clamped in {name}.weights"
                    )
                if bclipped > 0:
                    warnings.warn(
                        f"Warning: {bclipped} overflows were clamped in {name}.bias"
                    )

                # transpose weight if necessary
                if opt["t"]:
                    wc = wc.T

                # flatten and write to file
                wc.flatten().tofile(f)
                bc.flatten().tofile(f)


# https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#accounting-for-quantization-in-the-trainer
class NNUEOptimizer(optim.Adam):
    """Optimizer with weight clamping to account for quantization"""

    def __init__(self, params: NNUEParams, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.params = params

    def step(self, closure=None):
        super().step(closure)

        with torch.no_grad():
            for group in self.param_groups:
                # account for feature factorization
                if (
                    self.params.FEATURE_FACTORIZE
                    and group.get("name", None) == "ft.weight"
                ):
                    for param in group["params"]:
                        fact = self.params.N_INPUTS
                        wfact = param[:, fact : fact + 5 * 64]  # first (white) pnbrqk
                        bfact = param[:, fact + 5 * 64 :]  # second (black) pnbrqk

                        # add factorized parameter weights
                        for b in range(self.params.N_BUCKETS):
                            wf = b * 12 * 64
                            bf = b * 12 * 64 + 6 * 64
                            param[:, wf : wf + 5 * 64] += wfact
                            param[:, bf : bf + 5 * 64] += bfact

                        param[:, fact : fact + 5 * 64] = 0
                        param[:, fact + 5 * 64 :] = 0

                # clamp weights
                min_weight = group.get("min_weight", None)
                max_weight = group.get("max_weight", None)
                if min_weight is not None and max_weight is not None:
                    for param in group["params"]:
                        param.clamp_(min_weight, max_weight)
