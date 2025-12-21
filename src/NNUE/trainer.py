import argparse
import shutil
from datetime import datetime
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
import torch.optim.lr_scheduler as lr_scheduler
import yaml
from dataloader import get_dataloader
from net import NNUE, NNUEOptimizer, load_model
from tqdm import tqdm


def train(
    outfolder: Path,
    model: NNUE,
    optimizer: NNUEOptimizer,
    scheduler: lr_scheduler._LRScheduler,
    criterion: nn.modules.loss._Loss,
    device: str,
    epochs: int,
    patience: int,
    batch_size: int,
    save_freq: int,
    train_path: Path,
    test_path: Path,
    checkpoint: dict[str, Any] | None,
):
    # NOTE: epoch and superbatch used interchangeably here
    start_epoch = 0
    best_loss = float("inf")
    epochs_without_improvement = 0
    train_losses: list[float] = []
    test_losses: list[float] = []
    if patience == 0:
        patience = float("inf")

    # load checkpoint
    if checkpoint is not None:
        model.load_state_dict(checkpoint["model_state_dict"])
        optimizer.load_state_dict(checkpoint["optim_state_dict"])
        scheduler.load_state_dict(checkpoint["sched_state_dict"])
        start_epoch = checkpoint["epoch"]
        best_loss = checkpoint["loss"]

        # load from loss.csv if available
        loss_path = outfolder / "loss.csv"
        if loss_path.exists():
            df = pd.read_csv(loss_path)
            train_losses = df["Train Loss"].tolist()
            test_losses = df["Test Loss"].tolist()

    print(f"Starting training on device {device}. Output in {outfolder}/")

    # set up real-time plot
    plt.ion()
    fig, ax = plt.subplots()
    ax.set_title("Train & Test Loss")
    ax.set_xlabel("Superbatch")
    ax.set_ylabel("Accuracy")

    # training loop
    for epoch, train_loader in enumerate(
        get_dataloader(
            train_path,
            model,
            batch_size,
            shuffle=True,
            repeat=True,
            start_superbatch=start_epoch,
        ),
        start_epoch,
    ):
        if epoch >= epochs:
            break

        print(f"\n==== Superbatch {epoch + 1}/{epochs} ====")
        model.train()
        total_loss = 0.0

        for (wdata, bdata, side), labels in tqdm(train_loader, desc="Training"):
            # move device
            wdata = wdata.to(device)
            bdata = bdata.to(device)
            side = side.to(device)
            labels = labels.to(device)

            # training step
            optimizer.zero_grad()
            outputs = model(wdata, bdata, side)
            outputs = torch.sigmoid((outputs / model.WDL_SCALE).double())
            loss = criterion(outputs, labels.unsqueeze(1))
            total_loss += loss.item()
            loss.backward()
            optimizer.step()
        scheduler.step()

        train_loss = total_loss / len(train_loader)
        train_losses.append(train_loss)

        if epoch % save_freq != 0:
            test_loss = float("nan")
            test_losses.append(test_loss)
            print(f"Train: {train_loss:.6f}")
        else:
            test_loss = test(model, criterion, test_path, device)
            test_losses.append(test_loss)
            print(f"Train: {train_loss:.6f} | Test: {test_loss:.6f}")

        # save model
        checkpoint = {
            "epoch": epoch + 1,
            "model_state_dict": model.state_dict(),
            "optim_state_dict": optimizer.state_dict(),
            "sched_state_dict": scheduler.state_dict(),
            "loss": test_loss,
        }
        torch.save(checkpoint, f"{outfolder}/latest.pth")
        model.export(f"{outfolder}/latest.nnue")
        if epoch % save_freq == 0:
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
        ax.plot(
            range(1, epoch + 2, save_freq), test_losses[::save_freq], "g-", label="Test"
        )
        ymin = 0.8 * min(*train_losses, *test_losses)
        ymax = 1.2 * max(*train_losses, *test_losses)
        ax.set_ylim(ymin, ymax)
        ax.set_yscale("log")
        ax.set_yticks(np.logspace(np.log10(ymin), np.log10(ymax), 5))
        ax.yaxis.set_major_formatter("{x:.6f}")
        ax.legend()
        plt.draw()
        plt.pause(0.1)

        # save figure and table
        fig.savefig(f"{outfolder}/loss.png")
        pd.DataFrame(
            {
                "Superbatch": range(1, len(train_losses) + 1),
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
            "Superbatch": range(1, len(train_losses) + 1),
            "Train Loss": train_losses,
            "Test Loss": test_losses,
        }
    ).to_csv(f"{outfolder}/loss.csv", sep=",", index=False)


def test(
    model: NNUE,
    criterion: nn.modules.loss._Loss,
    test_path: Path,
    device: str,
) -> float:
    model.eval()
    total_loss = 0.0
    total_count = 0

    with torch.no_grad():
        for test_loader in get_dataloader(
            test_path, model, 16384, shuffle=False, repeat=False
        ):
            total_count += len(test_loader)
            for (wdata, bdata, side), labels in tqdm(test_loader, desc="Testing "):
                # move device
                wdata = wdata.to(device)
                bdata = bdata.to(device)
                side = side.to(device)
                labels = labels.to(device)

                # compute loss
                outputs = model(wdata, bdata, side)
                outputs = torch.sigmoid((outputs / model.WDL_SCALE).double())
                loss = criterion(outputs, labels.unsqueeze(1))
                total_loss += loss.item()

    return total_loss / total_count


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "config", type=str, help=("Path to config file or checkpoint directory")
    )
    args = parser.parse_args()
    config_path = Path(args.config)

    # create a new training folder or load checkpoint
    if config_path.is_file():
        outfolder = Path(datetime.now().strftime("train-%Y-%m-%d-%H-%M-%S"))
        outfolder.mkdir(exist_ok=True)
        shutil.copy(config_path, outfolder / "config.yaml")
        checkpoint = None
    elif config_path.is_dir():
        print(f"Loading checkpoint {config_path}")
        outfolder = config_path
        config_path = config_path / "config.yaml"
        checkpoint = torch.load(outfolder / "latest.pth")
    else:
        raise FileNotFoundError(
            f"The specified config file or checkpoint dir does not exist"
        )

    # load config
    with config_path.open("r") as f:
        config: dict[str, Any] = list(yaml.load_all(f, yaml.SafeLoader))[0]
        assert config["config_version"] >= 1.0
        model_config: dict[str, Any] = config["model"]
        train_config: dict[str, Any] = config["training_options"]

    # set up trainer args
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = load_model(model_config)
    model.to(device)
    optimizer = NNUEOptimizer(model.parameters(), lr=train_config["lr_init"])
    scheduler = lr_scheduler.CosineAnnealingLR(
        optimizer, train_config["superbatches"], train_config["lr_final"]
    )
    criterion = nn.MSELoss()

    train(
        outfolder,
        model,
        optimizer,
        scheduler,
        criterion,
        device,
        train_config["superbatches"],
        train_config["patience"],
        train_config["batch_size"],
        train_config["save_freq"],
        Path(train_config["train_path"]),
        Path(train_config["test_path"]),
        checkpoint,
    )
