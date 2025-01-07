import os
from datetime import datetime

import matplotlib.pyplot as plt
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, random_split

from .dataloader import NNUEDataSet
from .net import NNUE, NNUEOptimizer, NNUEParams


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
    outfolder = datetime.now().strftime("%Y.%m.%d-%H:%M")
    os.mkdir(outfolder)

    train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=32, shuffle=False)

    best_loss = float("inf")
    train_losses, test_losses = [], []
    epochs_without_improvement = 0

    # set up real-time plot
    plt.ion()
    fig, ax = plt.subplots()
    ax.set_title("Train & Test Loss")
    ax.set_xlabel("Epoch")
    ax.set_ylabel("Accuracy")

    for epoch in range(epochs):
        model.train()
        total_loss = 0.0

        for i, ((wdata, bdata), labels) in enumerate(train_loader):
            optimizer.zero_grad()
            outputs = model(wdata, bdata)
            loss = criterion(outputs / params.WDL_SCALE, labels)
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

        if epochs_without_improvement >= patience:
            print("Stopping early")
            break

    # show final plot
    plt.ioff()
    plt.show()
    fig.savefig(f"{outfolder}/loss.png")


def test(model: NNUE, criterion, test_loader) -> float:
    model.eval()
    total_loss = 0.0

    with torch.no_grad():
        for (wdata, bdata), labels in test_loader:
            outputs = model(wdata, bdata)
            loss = criterion(outputs, labels)
            total_loss += loss.item()

    avg_loss = total_loss / len(test_loader)
    return avg_loss


if __name__ == "__main__":
    # TODO: ask load checkpoint
    # TODO: ask traindata filepath or use default

    params = NNUEParams()
    model = NNUE(params)

    optimizer = NNUEOptimizer(model.parameters(), lr=0.001)
    criterion = nn.BCEWithLogitsLoss()

    dataset = NNUEDataSet(params, "TODO:.csv", optimize=True)
    train_dataset, test_dataset = random_split(
        dataset, [0.8, 0.2], generator=torch.Generator().manual_seed(42)
    )
    params = dataset.params

    epochs = 20
    patience = 5
    train(
        params,
        model,
        optimizer,
        criterion,
        epochs,
        patience,
        train_dataset,
        test_dataset,
    )
