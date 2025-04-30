# TODO: call nnuerun executable and compare output with Raphael eval & python net
import numpy as np
import torch
from net import NNUE, NNUEParams


def get_features(
    params: NNUEParams, fen: str
) -> tuple[torch.tensor, torch.tensor, bool]:
    side, widx, bidx = params.get_features(fen)
    wf = torch.zeros(params.N_INPUTS_FACTORIZED)
    bf = torch.zeros(params.N_INPUTS_FACTORIZED)
    wf[widx] = 1
    bf[bidx] = 1
    return wf, bf, side


def get_model_output(
    params: NNUEParams,
    model: NNUE,
    wf: torch.tensor,
    bf: torch.tensor,
    side: bool,
) -> float:
    with torch.no_grad():
        out = model(wf.unsqueeze(0), bf.unsqueeze(0), torch.tensor([side]))
        out = params.WDL_SCALE * torch.log(out / (1 - out))  # undo sigmoid
        return out[0, 0]


def get_quantized_model_output(
    params: NNUEParams,
    model: NNUE,
    wf: torch.tensor,
    bf: torch.tensor,
    side: bool,
) -> int:
    w0 = (model.ft.weight * 127).detach().numpy().astype(np.int16)
    b0 = (model.ft.bias * 127).detach().numpy().astype(np.int16)
    w = w0 @ wf.detach().numpy().astype(np.int16) + b0
    b = w0 @ bf.detach().numpy().astype(np.int16) + b0
    accumulator = np.expand_dims(
        np.concatenate([w, b]) if side else np.concatenate([b, w]), 0
    )
    out_q = np.clip(accumulator, 0, 127).astype(np.int32)

    w1 = (model.l1.weight * 64).detach().numpy().astype(np.int8)
    b1 = (model.l1.bias * 64 * 127).detach().numpy().astype(np.int32)
    out_q = np.clip((out_q @ w1.T + b1) >> params.QLEVEL1, 0, 127).astype(np.int32)

    w2 = (model.l2.weight * 64).detach().numpy().astype(np.int8)
    b2 = (model.l2.bias * 64 * 127).detach().numpy().astype(np.int32)
    out_q = np.clip((out_q @ w2.T + b2) >> params.QLEVEL2, 0, 127).astype(np.int32)

    w3 = (model.l3.weight * 300 * 32 / 127).detach().numpy().astype(np.int8)
    b3 = (model.l3.bias * 300 * 32).detach().numpy().astype(np.int32)
    out_q = (out_q @ w3.T + b3) >> params.QLEVEL3

    return out_q[0, 0]


if __name__ == "__main__":
    params = NNUEParams(FEATURE_FACTORIZE=False)
    model = NNUE(params)
    model.load_state_dict(
        torch.load(
            "train-2025-01-23-01-19-45/best.pth",
            weights_only=False,
            map_location=torch.device("cpu"),
        )["model_state_dict"]
    )

    while True:
        command = input("")
        if command == "q":
            break

        # get outputs
        wf, bf, side = get_features(params, command)
        out = get_model_output(params, model, wf, bf, side)
        out_q = get_quantized_model_output(params, model, wf, bf, side)

        # compute error
        q_error = abs(out - out_q)

        print(
            f"  eval: {out}\n  quantized: {out_q}\n"
            f"  quantization error: {q_error:.4f}"
        )
