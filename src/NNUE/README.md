# Raphael NNUE

This document contains the documentation for the various NNUE-related scripts, as well as the specific architecture and traindata info of the different Raphael NNUE versions.

## TrainGen

TrainGen is a program written in C++ for generating the training data for Raphael NNUE. The following is the usage guide for `traingen`:

```text
Usage: traingen [OPTIONS] INPUT_DIR OUTPUT_DIR MAX_NODES

  Takes in an INPUT_DIR containing files 1.epd, 2.epd, ... with contents of the form
  "fen [wdl]" in each row to generate the NNUE training data in OUTPUT_DIR, using
  the evals at the specified MAX_NODES

Options:
  -d DEPTH Max depths to search. Defaults to 128
  -n N     Max number of soft nodes (depth at N nodes) to search. Defaults to -1 (infinite)
  -b BS BE Start and end (inclusive) epd index to generate for. Defaults to 1, -1
           I.e., go through the entire INPUT_DIR
  -c       Include checks in data. Defaults to false
  -h       Show this message and exit
```

Each file in `INPUT_DIR` must be named `*.epd`, where `*` is an integer starting from 1 and ascending.
Each row of the epd file needs to be in the format `"fen [wdl]"` (no header). For example,

```text
R7/p3k1p1/1p4b1/8/4n3/q7/5PPP/6K1 w - - 0 1 [0.0]
2k5/1b2q1p1/p2p4/bp1PpPpN/4P3/3P3Q/P4B1P/6K1 w - - 0 1 [1.0]
8/3r2pk/p3qp2/3p3P/P1p1pQp1/2P5/1P3PP1/3R2K1 b - - 0 1 [0.5]
8/1r4pk/1p2p3/p3P3/1b3K2/4B3/4R3/8 w - - 0 1 [0.0]
8/8/5B2/n6p/4K1k1/8/8/8 w - - 0 1 [0.5]
```

The output will be a list of csv files, one for each epd file, with columns `fen`, `wdl` (absolute, `1.0` is a win for white), `eval` (in relative centipawns), `flag`, and `bm` (bestmove). These headers will be added the first time the output file is created, and will not be added when appending to an existing output file.

The current flags are:

- 0: None
- 1: Checks
- 2: Captures
- 4: Promotions

Note that multiple flags can be enabled at the same time via binary OR. E.g., if the flag = 3, it is equivalent to a capture with a check.

The `-b` argument is used to generate for a subset of epd files inside the `INPUT_DIR`, which can be useful for resuming generation or when generating across multiple devices.

Here is how the search depth is determined:

- the engine will attempt to search up to `MAX_NODES`
- if at the end of a depth, the current depth exceeds `DEPTH` or the number of nodes explored exceeds `N`, the search will stop there

There are additional logics in place to skip certain positions or data points from being added:

```python
for (fen, wdl) in epd_file:
    board = Board(fen)
    if not options.include_check and board.inCheck():
        return
    score = engine.eval(board)
    if score == MATE_SCORE:
        return
    output_dataset.append((fen, wdl, score))
```

## Trainer

The trainer is written in Python and it is used to train the NNUE model.
The following is the usage guide:

```text
usage: trainer.py [-h] config

positional arguments:
  config      Path to config file or checkpoint directory

options:
  -h, --help  show this help message and exit
```

The input to the trainer is a path to either a config YAML file, or a path to a directory containing the config YAML file alongside the checkpoint files.
If you provide a path to a config file, it will start a new training session using that config.
Otherwise, it will load the checkpoint and resume training from there.

The following are the contents of the config file.

```yaml
config_version: 1.0

model:
  architecture: all-1-screlu
  wdl_scale: 306.64
  # architecture-specific parameters, such as hidden_size, qa, qb, etc...

training_options:
  train_path: dataset/eval/train_clean
  test_path: dataset/eval/test_clean

  superbatches: 2000
  lr_init: 0.001
  lr_final: 0.00000243
  patience: 5
  batch_size: 16384

  save_freq: 50
```

The `model` field contains the `architecture` of the model, and the `wdl_scale` used to convert between eval and WDL. Each architecture also has its own set of parameters which must be defined to initialize the model. Note that it is very important that the architecture and parameters match the model on the CPU side.

The `training_options` field contains the various settings to control the model training. For instance, `training_path` and `test_path` both contain the path to the dataset directory, containing files of the form `1.csv`, `2.csv`, ... with the fen, wdl, and eval. Each superbatch will load one of these files and train the model on them. It is common for each superbatch to contain about a million different positions.

The `superbatches` field specifies how many superbatches to train for in total. If the superbatch index exceeds the number of training files, it will loop around to the first superbatch, effectively completing an epoch. The scheduler will decay the learning rate from `lr_init` to `lr_final` throughout the course of this process.

Finally, `patience` determines how many superbatches of consecutive non-improvement in the validation loss must occur before early stopping the training (with 0 meaning infinite), and `batch_size` determines the batch size during training. `save_freq` determines the number of superbatches before the validation loss is computed.

Note that training can take quite a while for the first epoch, as the dataloader must compute the input features to the model for every data point. These results will be cached in the `/cache` directory within the dataset directory, greatly speeding up the training process for future iterations.

## NNUETest

There are two versions of NNUETest, written in Python and C++. The following are the usage guide for the C++ and Python NNUETest, respectively:

```text
Usage: nnuetest [OPTIONS]

  Outputs nnue evaluation for each input fen

Options:
  PATH  NNUE file. Defaults to best.nnue from last train output
  -h    Show this message and exit
```

```text
usage: nnuetest.py [-h] [-s DATASIZE] [path]

positional arguments:
  path                  Path to trained checkpoint folder (default: lastest trainining session)

options:
  -h, --help            show this help message and exit
  -s DATASIZE, --datasize DATASIZE
                        Number of positions to use during testing. Greater values provide more coverage but is also slower (default: 1000)
```

Both programs are used as debugging tools to validate consistency between the C++ and Python NNUE, measure quantization, evaluate errors, and test other behaviors. The two scripts tests different things, some of which are only possible on one or the other.

Note that the script is made specifically for Raphael, and a lot of the variables are hardcoded as it is not intended to be used outside of debugging purposes during development.

Both programs will print out a list of possible commands upon startup and will both accept a FEN and print out the resulting evaluation.

## Versions

This section documents the specific options and dataset used to train the different versions of Raphael.

### Raphael 2.0

This is the first version of the Raphael NNUE. Here is the breakdown of the NNUE architecture, dataset, and training results:

<!-- TODO: Architecture:

1. 16 king-bucketed half-ka input features (12,288 features)
2. 256-wide accumulator with int16 weights and weight scale factor of 127, followed by a CReLU activation
3. 32-neuron layer with int8 weights and quantization level of 6, followed by a CReLU activation
4. 32-neuron layer with int8 weights and quantization level of 6, followed by a CReLU activation
5. output layer with int8 weights, quantization level of 5, and an output scale of 300

Training Procedure:

1. Gathered EPD files from [Blunder 8.1's Texel Tuning Dataset](https://talkchess.com/viewtopic.php?t=78536&start=20) and [Zurichess](https://www.reddit.com/r/chessprogramming/comments/1if8yx6/chess_dataset_for_tuning_evaluation/) with roughly two million unique positions in total
2. Gathered evaluations using `traingen` with arguments `combined-positions.epd 128 -n 1048576`
3. Used `clean_nnue_traindata.py` (inside /dataset) to remove positions with forced-mate sequences and downsample positions with large evaluations. At this point, the dataset size is roughly 1.3 million positions
4. Trained the NNUE using `trainer.py` with arguments `-i traindata_combined.csv -o traindata_combined_ff.csv -f` -->

<!--
TODO:
Training Results:

- Epochs:
- Training Loss:
- Test Loss:
- ELO-gain:
- -->