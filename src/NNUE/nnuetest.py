# TODO: call nnuerun executable and compare output with Raphael eval & python net
import torch
from net import NNUE, NNUEParams

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

        side, widx, bidx = params.get_features(command)
        side = torch.tensor([side])
        wf = torch.zeros(params.N_INPUTS_FACTORIZED)
        bf = torch.zeros(params.N_INPUTS_FACTORIZED)
        wf[widx] = 1
        bf[bidx] = 1

        with torch.no_grad():
            outputs = model(wf.unsqueeze(0), bf.unsqueeze(0), side)
            print(outputs)
