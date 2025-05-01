# Raphael NNUE

This document contains the documentation for the varios NNUE-related scripts, as well as the specific architecture and traindata info of the different Raphael NNUE versions.

## TrainGen

TrainGen is a program written in C++ for generating the training data for Raphael NNUE. The following is the usage guide for `traingen`:

```text
Usage: traingen [OPTIONS] INPUT_FILE DEPTH

  Takes in a INPUT_FILE with rows "fen [wdl]" and generates the NNUE traindata of the using the evals at the specified DEPTH

Options:
  -o PATH  Output file. Defaults to traindata.csv
  -s LINE  Line of INPUT_FILE to read from. Defaults to 1
  -n N     Max number of nodes to search before moving on. Defaults to -1 (infinite)
  -e TE    Will drop data if abs(eval - static_eval) > TE. Defaults to 300
  -c       Include checks in data
  -h       Show this message and exit
```

Each row of the input file needs to be in the format `"fen [wdl]"` (no header). For example,

```text
R7/p3k1p1/1p4b1/8/4n3/q7/5PPP/6K1 w - - 0 1 [0.0]
2k5/1b2q1p1/p2p4/bp1PpPpN/4P3/3P3Q/P4B1P/6K1 w - - 0 1 [1.0]
8/3r2pk/p3qp2/3p3P/P1p1pQp1/2P5/1P3PP1/3R2K1 b - - 0 1 [0.5]
8/1r4pk/1p2p3/p3P3/1b3K2/4B3/4R3/8 w - - 0 1 [0.0]
8/8/5B2/n6p/4K1k1/8/8/8 w - - 0 1 [0.5]
```

The output will be a csv file with rows `fen`, `wdl`, and `eval` in relative centipawns from the side to move.
These headers will be added the first time the output file is created, and will not be added when appending to an existing output file.
Note that for both the input and output file, `wdl` is absolute, with 1.0 being a win for white and `0.0` being a win for black.

The starting line (`-s`) argument should be used to resume the traindata generation. For example, if the traingen was stopped at line 2000, you would want to pass in `-s 2001` to resume from after line 2000. Before doing that though, you should make sure the last line of the output file isn't incomplete, which might happen if you use CTRL + C to force-quit the program.

Before training, you will want to set `-n` to the number of nodes Raphael searches in a second (can be estimated by running the uci script) so that at most, each position takes a second to evaluate. However, you may want to adjust this depending on your desired `depth`.

The actual traindata generation is done in the following way:

```python
for (fen, wdl) in input_dataset:
    board = Board(fen)
    if options.include_check and board.inCheck():
        return
    score = engine.eval(board)
    if abs(score - engine.static_eval(board)) > options.TE:
        return
    output_dataset.append((fen, wdl, score))
```

<!-- TODO: -->
In the future, the traindata generation code may get updated to use the quiescene board before evaluating it to ensure the data is not noisy and reflects what will actually be passed into the NNUE model.

In general, you would want `-e` to be a fairly small value and `-c` to not be set (don't include check) to ensure the dataset is not noisy. However, `-e` should not be set too small or else the NNUE may get worse at estimating the long-term evaluation of the board.

## Trainer

The trainer is written in Python and it, as you can probably guess, trains the NNUE model.
Before using the trainer, you should check that the NNUE model and parameters defined in `net.py` is correct and aligns with what's in `Raphael/nnue.h`.

### net.py: NNUEParams

This defines the parameters for the NNUE model, and most of it should be the same as what's defined in `Raphael/nnue.h`. The WDL scale is not so important as this will be recalculated anyways to adjust to the dataset.

If you decide to change any of the network parameters, you will have to update the corresponding functions. You will also want to make sure the network parameter sizes are of the correct multiple, as changing them up may break the SIMD portion of the NNUE. Lastly, you will want to make sure these changes are reflected in the cpp version of the network too.

### net.py: NNUE

This defines the NNUE model, and it is a relatively simple, shallow neural network. The only portion you should change in this class is the `export_options` dictionary to change the data types of the exported weights (not recommended).

The qlevels denote the log2 quantization levels at the respective layers. A larger qlevel will allow the model to represent weights with higher precision but will decrease the range of values the weights can take.

The output scale, defined in the parameters, also affect quantization. A larger output scale will increase precision but decrease the range of values for the final layer's weights. This output scale is only used during training and it is implicitly added to the model when exported. The output scale scales the output to let it represent numbers in centipawns (which can get quite big) so that weights in the last layers do not have to be massive to scale the previous layer's outputs from 0-1 to the centipawn range.

### net.py: NNUEOptimizer

The optimizer is used to ensure the weights are clamped during training so that they do not exceed the allowed range for quantization. It also distributes the factorized weights to the individual weights if feature factorization is enabled, and this portion may need to be changed if the feature set is modified.

### dataloader.py: NNUEDataSet

This class is used to load the dataset for training. The expected input and output files will be described later below.
The main portion you would want to change in this file is the `get_cost` function inside `optimize()`, which dictates how the dataset will be sampled to make it more suitable for training. The current implementation is roughly based on [this paper](https://arxiv.org/abs/2412.17948v1) which states the ideal characteristics of a NNUE dataset. For instance, it emphasizes the proportions of evaluation scores between and outside the range of -100 to 100.

This class is also used to calculate the optimum WDL_SCALE as well as the scaled evaluation scores using the provided data file.

### trainer.py

This is the main script that is used to train the NNUE model. The following is the usage guide:

```text
usage: trainer.py [-h] [-i IN_FILENAME] [-o OUT_FILENAME] [-d | --data_optimize | --no-data_optimize] [-e EPOCH] [-p PATIENCE]
                  [-f | --feature_factorize | --no-feature_factorize] [-c CHECKPOINT]

options:
  -h, --help            show this help message and exit
  -i IN_FILENAME, --in_filename IN_FILENAME
                        Input csv with fen, wdl (absolute), and eval (relative). Will use -o as input instead if not provided (default: None)
  -o OUT_FILENAME, --out_filename OUT_FILENAME
                        Output csv of processed data. Will use as input if -i is not provided (leads to faster data loading) (default: None)
  -d, --data_optimize, --no-data_optimize
                        Whether to optimzie the dataset for potentially faster and better training (default: False)
  -e EPOCH, --epoch EPOCH
                        Number of epochs to train for. May stop training earlier if -p > 0 (default: 50)
  -p PATIENCE, --patience PATIENCE
                        Number of consecutive epochs without improvement to stop training. Will be ignored if 0 (default: 5)
  -f, --feature_factorize, --no-feature_factorize
                        Whether to enable feature factorization. May lead to faster training (default: False)
  -c CHECKPOINT, --checkpoint CHECKPOINT
                        Checkpoint (path to a .pth file) to resume from (default: )
```

The input file should be a `csv` file with the rows `fen`, `wdl` (absolute, with 1.0 as white win), and `eval` (relative centipawns from the side to move). The output file is also a `csv` file with the same rows, plus a few extra.

When the input dataset is first loaded, its fen string is parsed to retrieve the white and black features of the board (based on the functions defined in  `NNUEParams`). The dataset may also undergo optimizations if given the `-d` flag. If you don't want to run this process again in subsequent training runs, it may be a good idea to export the processed dataset using the `-o` flag, and passing in the processed file using the `-o` flag instead of the `-i` flag.

The other command line arguments are pretty self explanatory if you know how training a neural network goes. The `-f` flag is used to enable feature factorization, which is explained in a lot more detail in [this document](https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#feature-factorization) by the Official Stockfish.

## NNUERun

NNUERun is a debug program written in C++ for loading and running the NNUE. The following is the usage guide for `nnuerun`:

```text
Usage: nnuerun [OPTIONS]

  Outputs nnue evaluation for each input fen

Options:
  PATH  NNUE file. Defaults to best.nnue from last train output
  -h    Show this message and exit
```

Once loaded, you can enter a FEN and it will print the evaluation for that position. To quit, enter 'q'.

## NNUETest

NNUETest is very similar to NNUERun, but it runs on the Python side and loads from a pth file instead of an nnue file.
It is mainly used as a debugging tool to ensure consistency of the model with the C++ NNUE, and to evaluate the model's accuracy and quantization errors.
The following is the usage guide:

```text
usage: nnuetest.py [-h] [-f | --feature_factorize | --no-feature_factorize] [path]

positional arguments:
  path                  Path to trained pth file. Defaults to best.pth from the lastest trainining session (default: )

options:
  -h, --help            show this help message and exit
  -f, --feature_factorize, --no-feature_factorize
                        Whether to the trained model uses feature factorization (default: False)
```

Like NNUERun, once loaded, you can enter a FEN to get the raw and quantized evaluation, along with a quantization error.
To quit, enter 'q', and to get a statistics of the quantization and evaluation error, enter 'e'.

## Versions

This section documents the specific options and dataset used to train the different versions of Raphael.

### Raphael 2.0

This is the first version of the Raphael NNUE. Here is the breakdown of the NNUE architecture, dataset, and training results:

Architecture:

1. 16 king-bucketed half-ka input features (12,288 features)
2. 256-wide accumulator with int16 weights and weight scale factor of 127, followed by a CReLU activation
3. 32-neuron layer with int8 weights and quantization level of 6, followed by a CReLU activation
4. 32-neuron layer with int8 weights and quantization level of 6, followed by a CReLU activation
5. output layer with int8 weights, quantization level of 5, and an output scale of 300

Traindata:

- EPD file from [Blunder 8.1's Texel Tuning Dataset](https://talkchess.com/viewtopic.php?t=78536&start=20) with roughly a million positions
- `traingen` with arguments `quiet-extended.epd 8 -n 16777216` (quiet-extended is the name of the EPD file above)
- `trainer.py` with arguments `-i traindata.csv -o traindata_processed.csv -d`

<!--
TODO:
Training Results:

- Epochs:
- Training Loss:
- Test Loss:
- ELO-gain:
- -->