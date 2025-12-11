import warnings
from abc import ABC, abstractmethod
from typing import Any

import chess
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from typing_extensions import TypedDict


# --------------------------- shared --------------------------- #
class ExportOption(TypedDict):
    name: str
    weight: torch.Tensor
    bias: torch.Tensor
    wk: int
    bk: int
    wt: type[np.integer]
    bt: type[np.integer]
    wrange: tuple[float, float]
    brange: tuple[float, float]
    transpose: bool


class NNUE(nn.Module, ABC):
    """A base class for all NNUE architectures."""

    @abstractmethod
    def __init__(self, model_config: dict[str, Any]) -> None:
        super().__init__()
        self.N_INPUTS: int = 0
        self.WDL_SCALE: float = 0.0

    @staticmethod
    @abstractmethod
    def get_features(fen: str) -> tuple[bool, list[int], list[int]]:
        """Computes the white and black feature index of the current position along with
        the side to move.

        Args:
            fen (str): the position

        Returns:
            tuple[bool, list[int], list[int]]: side to move (True for white) + white and
                black feature indices
        """
        ...

    @abstractmethod
    def forward(
        self, white_features: torch.Tensor, black_features: torch.Tensor, side: bool
    ) -> list[float]:
        """Evaluates using the model

        Args:
            white_features: white features of size N_INPUTS
            black_features: black features of size N_INPUTS
            side: perspective to evaluate from, True for white, False for black

        Returns:
            list[float]: a batch of evaluations from the side to move
        """
        ...

    @abstractmethod
    def parameters(self) -> list[dict[str, Any]]:
        """Returns overwritten parameters with min and max clamp range to account for
        quantization
        """
        ...

    @abstractmethod
    def get_quantized_parameters(self) -> list[np.ndarray]:
        """Transforms the model weights and biases as specified by the export options

        Returns:
            list[np.ndarray]: weights and biases in order of the export options
        """
        ...

    @abstractmethod
    def export(self, filepath: str) -> None:
        """Exports the model weights to the filepath after applying quantization. Should
        only be used to port the model to C++, and not for saving the training state.

        Args:
            filepath (str): filepath to save model to
        """
        ...


# https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#accounting-for-quantization-in-the-trainer
class NNUEOptimizer(optim.Adam):
    """Optimizer with weight clamping to account for quantization"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def step(self, closure=None):
        super().step(closure)

        with torch.no_grad():
            for group in self.param_groups:
                # clamp weights
                prange = group.get("range", None)
                if prange is not None:
                    for param in group["params"]:
                        param.clamp_(*prange)


# --------------------------- architectures --------------------------- #
class NNUE_All1SCReLU(NNUE):
    def __init__(self, model_config: dict[str, Any]):
        super().__init__(model_config)

        # load config
        REQUIRED_KEYS = {"hidden_size", "qa", "qb", "output_scale", "wdl_scale"}
        if REQUIRED_KEYS - model_config.keys():
            raise KeyError(f"The model config must include the keys: {REQUIRED_KEYS}")

        self.N_INPUTS = 12 * 64
        self.N_HIDDEN: int = model_config["hidden_size"]
        self.QA: int = model_config["qa"]
        self.QB: int = model_config["qb"]
        self.OUTPUT_SCALE: int = model_config["output_scale"]
        self.WDL_SCALE = model_config["wdl_scale"]

        # create layers
        self.ft = nn.Linear(self.N_INPUTS, self.N_HIDDEN)
        self.l1 = nn.Linear(2 * self.N_HIDDEN, 1)

        self.export_options: list[ExportOption] = [
            {
                "name": "ft",
                "weight": self.ft.weight,
                "bias": self.ft.bias,
                "wk": self.QA,
                "bk": self.QA,
                "wt": np.int16,
                "bt": np.int16,
                "wrange": (
                    np.iinfo(np.int16).min / self.QA,
                    np.iinfo(np.int16).max / self.QA,
                ),
                "brange": (
                    np.iinfo(np.int16).min / self.QA,
                    np.iinfo(np.int16).max / self.QA,
                ),
                "transpose": True,  # transpose ft.weight to make it N_INPUT x N_HIDDEN
            },
            {
                "name": "l1",
                "weight": self.l1.weight,
                "bias": self.l1.bias,
                "wk": self.QB,
                "bk": self.QA * self.QB,  # note: we must multiply by QA again
                "wt": np.int16,
                "bt": np.int16,
                "wrange": (
                    np.iinfo(np.int16).min / (self.QA * self.QB),
                    np.iinfo(np.int16).max / (self.QA * self.QB),
                ),
                "brange": (
                    np.iinfo(np.int16).min / (self.QA * self.QB),
                    np.iinfo(np.int16).max / (self.QA * self.QB),
                ),
                "transpose": False,
            },
        ]

        # initialize with kaiming normal
        nn.init.normal_(self.ft.weight, std=np.sqrt(2.0 / 32))
        nn.init.normal_(self.l1.weight, std=np.sqrt(2.0 / (2 * self.N_HIDDEN)))
        nn.init.constant_(self.ft.bias, 0.0)
        nn.init.constant_(self.l1.bias, 0.0)

        # print statistics
        print("Initialized NNUE with:")
        print("  Layers:")
        for layer in self.export_options:
            name = layer["name"]
            ws1, ws2 = layer["weight"].shape
            bs = layer["bias"].shape[0]
            if layer["transpose"]:
                ws1, ws2 = ws2, ws1
            wt = layer["wt"].__name__
            bt = layer["bt"].__name__
            wmin, wmax = layer["wrange"]
            bmin, bmax = layer["brange"]
            print(f"    {name}.weight {wt}_t[{ws1} * {ws2}] ({wmin:.2f}, {wmax:.2f})")
            print(f"    {name}.bias   {bt}_t[{bs}]       ({bmin:.2f}, {bmax:.2f})")
        print("  Options:")
        print(f"    QA: {self.QA}")
        print(f"    QB: {self.QB}")
        print(f"    Output Scale: {self.OUTPUT_SCALE}")
        print(f"    WDL Scale:    {self.WDL_SCALE}")
        print("")

    @staticmethod
    def get_features(fen: str) -> tuple[bool, list[int], list[int]]:
        board = chess.Board(fen)

        widx, bidx = [], []
        for sq, p in board.piece_map().items():
            pc = p.color
            pt = p.piece_type - 1  # 1 indexed

            # our pnbrqk, their pnbrqk
            wp = 6 * (pc == chess.BLACK) + pt
            bp = 6 * (pc == chess.WHITE) + pt

            widx.append(wp * 64 + sq)
            bidx.append(bp * 64 + chess.square_mirror(sq))
        return board.turn == chess.WHITE, widx, bidx

    def forward(
        self, white_features: torch.Tensor, black_features: torch.Tensor, side: bool
    ):
        w = self.ft(white_features)
        b = self.ft(black_features)
        accumulator = torch.cat([w, b], dim=1) * side.unsqueeze(1)
        accumulator += torch.cat([b, w], dim=1) * torch.logical_not(side).unsqueeze(1)

        # linear + screlu
        o0 = torch.square(torch.clamp(accumulator, 0.0, 1.0))
        o1 = self.l1(o0) * self.OUTPUT_SCALE
        return torch.sigmoid((o1 / self.WDL_SCALE).double())

    def parameters(self) -> list[dict[str, Any]]:
        params = []
        for layer in self.export_options:
            params.append(
                {
                    "params": [layer["weight"]],
                    "name": f"{layer['name']}.weight",
                    "range": layer["wrange"],
                }
            )
            params.append(
                {
                    "params": [layer["bias"]],
                    "name": f"{layer['name']}.bias",
                    "range": layer["brange"],
                }
            )
        return params

    def get_quantized_parameters(self) -> list[np.ndarray]:
        parameters = []
        for layer in self.export_options:
            name = layer["name"]
            w = layer["weight"].detach().cpu().numpy().copy()
            b = layer["bias"].detach().cpu().numpy().copy()

            # scale weight and bias and round
            w = (w * layer["wk"]).round()
            b = (b * layer["bk"]).round()

            # clamp and convert type
            wt = layer["wt"]
            bt = layer["bt"]
            wc = np.clip(w, np.iinfo(wt).min, np.iinfo(wt).max).astype(wt)
            bc = np.clip(b, np.iinfo(bt).min, np.iinfo(bt).max).astype(bt)
            if (wclipped := np.sum(wc != w)) > 0:
                warnings.warn(
                    f"Warning: {wclipped} overflows were clamped in {name}.weights"
                )
            if (bclipped := np.sum(bc != b)) > 0:
                warnings.warn(
                    f"Warning: {bclipped} overflows were clamped in {name}.bias"
                )

            # transpose weight if necessary
            if layer["transpose"]:
                wc = wc.T

            # add to parameters
            parameters.append(wc)
            parameters.append(bc)
        return parameters

    def export(self, filepath: str) -> None:
        print(f"Exporting model to {filepath}")
        parameters = self.get_quantized_parameters()

        with open(filepath, "wb") as f:
            for param in parameters:
                np.ascontiguousarray(param.flatten()).tofile(f)


# --------------------------- interface --------------------------- #

MODEL_TYPES = {
    "all-1-screlu": NNUE_All1SCReLU,
}


def load_model(model_config: dict[str, Any]) -> NNUE:
    architecture = model_config["architecture"]
    if architecture not in MODEL_TYPES:
        raise ValueError(f"Unknown architecture: {architecture}")
    return MODEL_TYPES[architecture](model_config)
