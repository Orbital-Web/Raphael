import argparse
from datetime import datetime
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import torch
import torch.nn as nn
from dataloader import NNUEDataSet
from net import NNUE, NNUEOptimizer, NNUEParams
from torch.utils.data import DataLoader, random_split


def train(
    model: NNUE,
    optimizer: NNUEOptimizer,
    criterion,
    device: str,
    epochs: int,
    patience: int,
    train_dataset: NNUEDataSet,
    test_dataset: NNUEDataSet,
    checkpoint: str = "",
):
    if checkpoint == "":
        outfolder = Path(datetime.now().strftime("train-%Y-%m-%d-%H-%M-%S"))
        outfolder.mkdir(exist_ok=True)
        start_epoch = 0
        best_loss = float("inf")
        train_losses, test_losses = [], []
    else:
        # load checkpoint
        checkpoint_path = Path(checkpoint)
        if not checkpoint_path.exists():
            raise FileNotFoundError(f"Could not load checkpoint {checkpoint}")
        print(f"Loading training checkpoint {checkpoint}")

        # load from pth
        outfolder = checkpoint_path.parent
        ckpt = torch.load(checkpoint)
        model.load_state_dict(ckpt["model_state_dict"])
        optimizer.load_state_dict(ckpt["optim_state_dict"])
        best_loss = ckpt["loss"]
        start_epoch = ckpt["epoch"]

        # load from loss.csv if available
        loss_path = outfolder / "loss.csv"
        if loss_path.exists():
            df = pd.read_csv(loss_path)
            if len(diff := {"Epoch", "Train Loss", "Test Loss"} - set(df.columns)):
                print(f"{loss_path} is missing columns: {diff}")
            train_losses = df["Train Loss"].tolist()
            test_losses = df["Test Loss"].tolist()

    print(f"Starting training on device {device}. Output in {outfolder}/")

    train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=32, shuffle=False)

    epochs_without_improvement = 0
    if patience == 0:
        patience = float("inf")

    # set up real-time plot
    plt.ion()
    fig, ax = plt.subplots()
    ax.set_title("Train & Test Loss")
    ax.set_xlabel("Epoch")
    ax.set_ylabel("Accuracy")

    for epoch in range(start_epoch, epochs):
        model.train()
        total_loss = 0.0

        for i, ((wdata, bdata, side), labels) in enumerate(train_loader):
            # move device
            wdata = wdata.to(device)
            bdata = bdata.to(device)
            side = side.to(device)
            labels = labels.to(device)

            # training step
            optimizer.zero_grad()
            outputs = model(wdata, bdata, side)
            loss = criterion(outputs, labels.unsqueeze(1))
            total_loss += loss.item()
            loss.backward()
            optimizer.step()

        train_loss = total_loss / len(train_loader)
        train_losses.append(train_loss)
        print(f"Epoch: {epoch + 1}. Train: {train_loss:.6f}", end="")

        test_loss = test(model, criterion, test_loader, device)
        test_losses.append(test_loss)
        print(f", Test: {test_loss:.6f}")

        # save model
        checkpoint = {
            "epoch": epoch + 1,
            "model_state_dict": model.state_dict(),
            "optim_state_dict": optimizer.state_dict(),
            "loss": test_loss,
        }
        torch.save(checkpoint, f"{outfolder}/latest.pth")
        if test_loss < best_loss:
            best_loss = test_loss
            torch.save(checkpoint, f"{outfolder}/best.pth")
            model.export(f"{outfolder}/best.nnue")
            epochs_without_improvement = 0
        else:
            epochs_without_improvement += 1

        # update the plot
        ax.clear()
        ax.plot(range(1, epoch + 2), train_losses, "b-", label="Train")
        ax.plot(range(1, epoch + 2), test_losses, "g-", label="Test")
        ax.legend()
        plt.draw()
        plt.pause(0.1)

        # save figure and table
        fig.savefig(f"{outfolder}/loss.png")
        pd.DataFrame(
            {
                "Epoch": range(1, len(train_losses) + 1),
                "Train Loss": train_losses,
                "Test Loss": test_losses,
            }
        ).to_csv(f"{outfolder}/loss.csv", sep=",", index=False)

        if epochs_without_improvement > patience:
            print("Stopping early")
            break

    # show final plot and save losses
    print("Finished training")
    plt.ioff()
    plt.show()
    fig.savefig(f"{outfolder}/loss.png")
    pd.DataFrame(
        {
            "Epoch": range(1, len(train_losses) + 1),
            "Train Loss": train_losses,
            "Test Loss": test_losses,
        }
    ).to_csv(f"{outfolder}/loss.csv", sep=",", index=False)


def test(model: NNUE, criterion, test_loader, device) -> float:
    model.eval()
    total_loss = 0.0

    with torch.no_grad():
        for (wdata, bdata, side), labels in test_loader:
            # move device
            wdata = wdata.to(device)
            bdata = bdata.to(device)
            side = side.to(device)
            labels = labels.to(device)

            # compute loss
            outputs = model(wdata, bdata, side)
            loss = criterion(outputs, labels.unsqueeze(1))
            total_loss += loss.item()

    return total_loss / len(test_loader)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "-i",
        "--in_filename",
        type=str,
        help="Input csv with fen, wdl (absolute), and eval (relative). Will use -o as input instead if not provided",
    )
    parser.add_argument(
        "-o",
        "--out_filename",
        type=str,
        help="Output csv of processed data. Will use as input if -i is not provided (leads to faster data loading)",
    )
    parser.add_argument(
        "-d",
        "--data_optimize",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Whether to optimzie the dataset for potentially faster and better training",
    )
    parser.add_argument(
        "-e",
        "--epoch",
        type=int,
        default=50,
        help="Epoch number to stop training on. May stop training earlier if patience > 0",
    )
    parser.add_argument(
        "-p",
        "--patience",
        type=int,
        default=5,
        help="Number of consecutive epochs without improvement to stop training. Will be ignored if 0",
    )
    parser.add_argument(
        "-f",
        "--feature_factorize",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Whether to enable feature factorization. May lead to faster training",
    )
    parser.add_argument(
        "-c",
        "--checkpoint",
        type=str,
        default="",
        help="Checkpoint (path to a .pth file) to resume from",
    )
    args = parser.parse_args()

    # set up model
    device = "cuda" if torch.cuda.is_available() else "cpu"
    params = NNUEParams(FEATURE_FACTORIZE=args.feature_factorize)
    model = NNUE(params)
    model.to(device)
    optimizer = NNUEOptimizer(params, model.parameters(), lr=0.001)
    criterion = nn.MSELoss()

    # set up data loader
    dataset = NNUEDataSet(
        params, args.in_filename, args.out_filename, args.data_optimize
    )
    train_dataset, test_dataset = random_split(
        dataset, [0.8, 0.2], generator=torch.Generator().manual_seed(42)
    )

    train(
        model,
        optimizer,
        criterion,
        device,
        args.epoch,
        args.patience,
        train_dataset,
        test_dataset,
        args.checkpoint,
    )
