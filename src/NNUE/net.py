import warnings
from dataclasses import dataclass, field

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim


@dataclass
class NNUEParams:
    N_BUCKETS: int = 16  # number of king buckets
    KING_BUCKETS: list[int] = field(
        default_factory=lambda: [
            0,  1,  2,  3,  3,  2,  1,  0,   #
            4,  5,  6,  7,  7,  6,  5,  4,   #
            8,  9,  10, 11, 11, 10, 9,  8,   #
            8,  9,  10, 11, 11, 10, 9,  8,   #
            12, 12, 13, 13, 13, 13, 12, 12,  #
            12, 12, 13, 13, 13, 13, 12, 12,  #
            14, 14, 15, 15, 15, 15, 14, 14,  #
            14, 14, 15, 15, 15, 15, 14, 14,  #
        ]
    )  # fmt: skip
    N_HIDDEN0: int = 256  # accumulator size
    N_HIDDEN1: int = 32
    N_HIDDEN2: int = 32
    QLEVEL1: int = 6
    QLEVEL2: int = 6
    QLEVEL3: int = 5
    OUTPUT_SCALE: int = 300  # scaling applied to output
    WDL_SCALE: int = 400  # scaling such that sigmoid(eval/WDL_SCALE) ≈ WDL

    @property
    def N_INPUTS(self) -> int:
        return self.N_BUCKETS * 10 * 64  # using half-kp


# https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#training-a-net-with-pytorch
class NNUE(nn.Module):
    def __init__(self, params: NNUEParams):
        super().__init__()
        self.params: NNUEParams = params
        self.qscale1 = 1 << self.params.QLEVEL1
        self.qscale2 = 1 << self.params.QLEVEL2
        self.qscale3 = 1 << self.params.QLEVEL3

        self.ft = nn.Linear(params.N_INPUTS, params.N_HIDDEN0)
        self.l1 = nn.Linear(2 * params.N_HIDDEN0, params.N_HIDDEN1)
        self.l2 = nn.Linear(params.N_HIDDEN1, params.N_HIDDEN2)
        self.l3 = nn.Linear(params.N_HIDDEN2, 1)

        # NOTE: after quantization
        # q0 = crelu[(127W0)*x + (127b0)] ≡ 127o0
        # q1 = crelu[[(qscale1*W1)*q0 + (qscale1*127b1)] / qscale1] ≡ 127o1
        # q2 = crelu[[(qscale2*W2)*q1 + (qscale2*127b1)] / qscale2] ≡ 127o2
        # q3 = [(oscale*qscale3*W3/127)*q2 + (oscale*qscale3*b1)] / qscale3 ≡ oscale*o3

        # we need to transpose q1 and q2's weights to make them output indexed
        # don't transpose ft's weights to make them input feature indexed
        self.export_options = {
            "ft": {"wk": 127, "bk": 127, "wt": np.int16, "bt": np.int16, "t": False},
            "l1": {
                "wk": self.qscale1,
                "bk": self.qscale1 * 127,
                "wt": np.int8,
                "bt": np.int32,
                "t": True,
            },
            "l2": {
                "wk": self.qscale2,
                "bk": self.qscale2 * 127,
                "wt": np.int8,
                "bt": np.int32,
                "t": True,
            },
            "l3": {
                "wk": self.params.OUTPUT_SCALE * self.qscale3 / 127,
                "bk": self.params.OUTPUT_SCALE * self.qscale3,
                "wt": np.int8,
                "bt": np.int32,
                "t": False,  # only 1 output, makes no difference
            },
        }

    def forward(self, white_features, black_features, side: bool) -> list[float]:
        """Evaluates using the model

        Args:
            white_features: a batch of white features size N_INPUTS
            black_features: a batch of black features size N_INPUTS
            side (bool): True for white, False for black

        Returns:
            list[float]: a batch of evaluations from the side to move
        """
        w = self.ft(white_features)
        b = self.ft(black_features)
        accumulator = torch.cat([w, b], dim=1) if side else torch.cat([b, w], dim=1)

        # layers + crelu
        o0 = torch.clamp(accumulator, 0.0, 1.0)
        o1 = torch.clamp(self.l1(o0), 0.0, 1.0)
        o2 = torch.clamp(self.l2(o1), 0.0, 1.0)
        return self.l3(o2) * self.params.OUTPUT_SCALE

    def parameters(self):
        """Returns overwritten parameters with min and max clamp range to account for
        quantization
        """
        # the feature transformer and bias use int16_t/int32_t with no risk of overflow
        return [
            {"params": [self.ft.weight, self.ft.bias]},
            {  # must fit in int8_t after multiplying by qscale1
                "params": [self.l1.weight],
                "min_weight": -127 / self.qscale1,
                "max_weight": 127 / self.qscale1,
            },
            {"params": [self.l1.bias]},
            {  # must fit in int8_t after multiplying by qscale2
                "params": [self.l2.weight],
                "min_weight": -127 / self.qscale2,
                "max_weight": 127 / self.qscale2,
            },
            {"params": [self.l2.bias]},
            {  # must fit in int8_t after multiplying by OUTPUT_SCALE * qscale3 / 127
                "params": [self.l3.weight],
                "min_weight": -127 * 127 / (self.params.OUTPUT_SCALE * self.qscale3),
                "max_weight": 127 * 127 / (self.params.OUTPUT_SCALE * self.qscale3),
            },
            {"params": [self.l3.bias]},
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
                w = layer.weight.detach().numpy() * opt["wk"]
                b = layer.bias.detach().numpy() * opt["bk"]

                # clamp and convert type
                wt = opt["wt"]
                bt = opt["bt"]
                wc = np.clip(w, np.iinfo(wt).min, np.iinfo(wt).max).astype(wt)
                bc = np.clip(b, np.iinfo(bt).min, np.iinfo(bt).max).astype(bt)
                wclipped = np.sum(wc != w)
                bclipped = np.sum(bc != b)
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
                wc = wc.flatten().tofile(f)
                bc = bc.flatten().tofile(f)


# https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#accounting-for-quantization-in-the-trainer
class NNUEOptimizer(optim.adam):
    """Optimizer with weight clamping to account for quantization"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def step(self, closure=None):
        super().step(closure)

        # clamp weights
        with torch.no_grad():
            for group in self.param_groups:
                min_weight = group.get("min_weight", None)
                max_weight = group.get("max_weight", None)
                if min_weight is not None and max_weight is not None:
                    for param in group["params"]:
                        param.clamp_(min_weight, max_weight)
