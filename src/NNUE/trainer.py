import argparse
import os
from datetime import datetime

import matplotlib.pyplot as plt
import pandas as pd
import torch
import torch.nn as nn
from dataloader import NNUEDataSet
from net import NNUE, NNUEOptimizer, NNUEParams
from torch.utils.data import DataLoader, random_split


def train(
    params: NNUEParams,
    model: NNUE,
    optimizer: NNUEOptimizer,
    criterion,
    epochs: int,
    patience: int,
    train_dataset,
    test_dataset,
):
    outfolder = datetime.now().strftime("train-%Y-%m-%d-%H-%M-%S")
    os.mkdir(outfolder)
    print(f"Starting Training. Output in {outfolder}/")

    train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=32, shuffle=False)

    best_loss = float("inf")
    train_losses, test_losses = [], []
    epochs_without_improvement = 0
    if patience == 0:
        patience = float("inf")

    # set up real-time plot
    plt.ion()
    fig, ax = plt.subplots()
    ax.set_title("Train & Test Loss")
    ax.set_xlabel("Epoch")
    ax.set_ylabel("Accuracy")

    for epoch in range(epochs):
        model.train()
        total_loss = 0.0

        for i, ((wdata, bdata, side), labels) in enumerate(train_loader):
            optimizer.zero_grad()
            outputs = model(wdata, bdata, side)
            loss = criterion(outputs / params.WDL_SCALE, labels.unsqueeze(1))
            total_loss += loss.item()
            loss.backward()
            optimizer.step()

        train_loss = total_loss / len(train_loader)
        train_losses.append(train_loss)
        print(f"Epoch: {epoch + 1}. Train: {train_loss:.6f}", end="")

        test_loss = test(model, criterion, test_loader)
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
        else:
            epochs_without_improvement += 1

        # update the plot
        ax.clear()
        ax.plot(range(1, epoch + 2), train_losses, "b-", label="Train")
        ax.plot(range(1, epoch + 2), test_losses, "g-", label="Test")
        ax.legend()
        plt.draw()
        plt.pause(0.001)

        if epochs_without_improvement >= patience:
            print("Stopping early")
            break

    # show final plot and save losses
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


def test(model: NNUE, criterion, test_loader) -> float:
    model.eval()
    total_loss = 0.0

    with torch.no_grad():
        for (wdata, bdata, side), labels in test_loader:
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
        help="Number of epochs to train for. May stop training earlier if -p > 0",
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
    args = parser.parse_args()

    # set up model
    params = NNUEParams(FEATURE_FACTORIZE=args.feature_factorize)
    model = NNUE(params)
    optimizer = NNUEOptimizer(params, model.parameters(), lr=0.001)
    criterion = nn.BCEWithLogitsLoss()

    # set up data loader
    dataset = NNUEDataSet(
        params, args.in_filename, args.out_filename, args.data_optimize
    )
    train_dataset, test_dataset = random_split(
        dataset, [0.8, 0.2], generator=torch.Generator().manual_seed(42)
    )

    train(
        params,
        model,
        optimizer,
        criterion,
        args.epoch,
        args.patience,
        train_dataset,
        test_dataset,
    )
