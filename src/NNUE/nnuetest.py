import numpy as np
import pandas as pd
import torch
from net import NNUE, NNUEParams


def get_features(
    params: NNUEParams, fen: str
) -> tuple[torch.Tensor, torch.Tensor, bool]:
    side, widx, bidx = params.get_features(fen)
    wf = torch.zeros(params.N_INPUTS_FACTORIZED)
    bf = torch.zeros(params.N_INPUTS_FACTORIZED)
    wf[widx] = 1
    bf[bidx] = 1
    return wf, bf, side


def get_model_output(
    params: NNUEParams,
    model: NNUE,
    wf: torch.Tensor,
    bf: torch.Tensor,
    side: bool,
) -> float:
    with torch.no_grad():
        out = model(wf.unsqueeze(0), bf.unsqueeze(0), torch.tensor([side]))
        out = params.WDL_SCALE * torch.log(out / (1 - out))  # undo sigmoid
        return float(out[0, 0])


def get_quantized_model_params(model: NNUE) -> list[np.ndarray]:
    def round_convert(var: torch.Tensor, dtype: np.dtype) -> np.ndarray:
        var: np.ndarray = var.detach().numpy()
        var = np.clip(
            var.round(),
            np.iinfo(dtype).min,
            np.iinfo(dtype).max,
        )
        return var.astype(dtype)

    w0 = round_convert(model.ft.weight * 127, np.int16)
    b0 = round_convert(model.ft.bias * 127, np.int16)
    w1 = round_convert(model.l1.weight * 64, np.int8)
    b1 = round_convert(model.l1.bias * 64 * 127, np.int32)
    w2 = round_convert(model.l2.weight * 64, np.int8)
    b2 = round_convert(model.l2.bias * 64 * 127, np.int32)
    w3 = round_convert(model.l3.weight * 300 * 32 / 127, np.int8)
    b3 = round_convert(model.l3.bias * 300 * 32, np.int32)

    return [w0, b0, w1, b1, w2, b2, w3, b3]


def get_quantized_model_output(
    params: NNUEParams,
    quantized_params: list[np.ndarray],
    wf: torch.Tensor,
    bf: torch.Tensor,
    side: bool,
) -> float:
    w0, b0, w1, b1, w2, b2, w3, b3 = quantized_params

    w = w0 @ wf.detach().numpy().astype(np.int16) + b0
    b = w0 @ bf.detach().numpy().astype(np.int16) + b0
    accumulator = np.expand_dims(
        np.concatenate([w, b]) if side else np.concatenate([b, w]), 0
    )
    out_q = np.clip(accumulator, 0, 127).astype(np.int32)
    out_q = np.clip((out_q @ w1.T + b1) >> params.QLEVEL1, 0, 127).astype(np.int32)
    out_q = np.clip((out_q @ w2.T + b2) >> params.QLEVEL2, 0, 127).astype(np.int32)
    out_q = (out_q @ w3.T + b3) >> params.QLEVEL3

    return float(out_q[0, 0])


def evaluate_nnue(params: NNUEParams, model: NNUE, quantized_params: list[np.ndarray]):
    data = pd.read_csv("traindata_processed.csv")
    sum_eval_errors = 0
    sum_eval_percent_errors = 0
    sum_q_errors = 0
    sum_q_percent_errors = 0

    for i, row in data.iterrows():
        # get outputs
        wf, bf, side = get_features(params, row["fen"])
        out = get_model_output(params, model, wf, bf, side)
        out_q = get_quantized_model_output(params, quantized_params, wf, bf, side)

        eval_error = abs(out - row["eval"])
        sum_eval_errors += eval_error
        sum_eval_percent_errors += eval_error * 100 / (row["eval"] + 1e-2)

        q_error = abs(out - out_q)
        sum_q_errors += q_error
        sum_q_percent_errors += q_error * 100 / (out + 1e-2)

        if i > 1000:
            break

    sum_eval_errors /= i + 1
    sum_eval_percent_errors /= i + 1
    sum_q_errors /= i + 1
    sum_q_percent_errors /= i + 1

    print(
        f"Average Eval Error: {sum_eval_errors:.4f} "
        f"({sum_eval_percent_errors:.4f}%)\n"
        f"Average Quantization Error: {sum_q_errors:.4f} "
        f"({sum_q_percent_errors:.4f})%"
    )


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
    quantized_params = get_quantized_model_params(model)

    while True:
        command = input("")
        if command == "q":
            break
        elif command == "e":
            evaluate_nnue(params, model, quantized_params)
            break

        # get outputs
        wf, bf, side = get_features(params, command)
        out = get_model_output(params, model, wf, bf, side)
        out_q = get_quantized_model_output(params, quantized_params, wf, bf, side)

        # compute error
        q_error = abs(out - out_q)

        print(
            f"  eval: {out}\n  quantized: {out_q}\n"
            f"  quantization error: {q_error:.4f}"
        )
