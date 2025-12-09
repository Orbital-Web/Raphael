# Raphael NNUE

This document contains the documentation for the various NNUE-related scripts, as well as the specific architecture and traindata info of the different Raphael NNUE versions.

## TrainGen

TrainGen is a program written in C++ for generating the training data for Raphael NNUE. The following is the usage guide for `traingen`:

```text
Usage: traingen [OPTIONS] INPUT_FILE DEPTH

  Takes in a INPUT_FILE with rows "fen [wdl]" and generates the NNUE traindata using the evals at the specified DEPTH

Options:
  -o PATH  Output file. Defaults to dataset/traindata.csv
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

You should also set `-n` to a reasonable upper-bound so that the generator doesn't stall on complex positions trying to reach the specified `depth`. You can also set `depth` to an upper-bound, such as 128, and set `-n` to generate a dataset based on the number of nodes instead of the depth.

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

In general, you would want `-e` to be a fairly small value and `-c` to not be set (don't include check) to ensure the dataset is not noisy. However, `-e` should not be set too small either or else the NNUE may get worse at estimating the long-term evaluation of the board.

## Trainer

The trainer is written in Python and it, as you can probably guess, trains the NNUE model.
Before using the trainer, you should check that the NNUE model and parameters defined in `net.py` is correct and aligns with what's in `Raphael/nnue.h`.

### net.py: NNUEParams

This defines the parameters for the NNUE model, and most of it should be the same as what's defined in `Raphael/nnue.h`.

`N_INPUTS`, `N_INPUTS_FACTORIZED`, and `get_features()` must be implemented correctly for the trainer to function.
`WDL_SCALE` and `FEATURE_FACTORIZE` should be left intact. The WDL scale is not so important as this will be recalculated anyways to fit the dataset.

The remaining parameters can be freely modified/added/removed to fit the network architecture, though it should be inline with the C++ implementation. Some parameters may have certain constraints, such as being a multiple of 32, depending on the SIMD implementation of the model.

### net.py: NNUE

This defines the NNUE model. `forward()`, `parameters()`, `get_quantized_parameters()`, and `export()` must properly be implemented for the trainer to function. Again, these functions should be inline with the C++ implementation.

Things such as the `export_options`, parameter values (some values may have implicit constraints), and weight initialization can be modified relatively safely to adjust the model while keeping its overall architecture. Note that these changes will have to be reflected in the C++ NNUE too.

Parameters such as the qlevel denote the log2 quantization levels at the respective layers. A larger qlevel will allow the model to represent weights with higher precision but will decrease the range of values the weights can take.

The output scale, defined in the parameters, also affect quantization. A larger output scale will increase precision but decrease the range of values for the final layer's weights. This output scale is only used during training and it is implicitly added to the model when exported. It enables the model to output larger values (evals in centipawns can get quite big) without requiring larger weights.

### net.py: NNUEOptimizer

The optimizer is used to ensure the weights are clamped during training so that they do not exceed the allowed range for quantization.

### dataloader.py: NNUEDataSet

This class is used to load the dataset for training. The expected input and output files will be described later below.
The main portion you would want to change in this file is the `get_cost` function inside `optimize()`, which dictates how the dataset will be sampled to make it more suitable for training. The current implementation is roughly based on [this paper](https://arxiv.org/abs/2412.17948v1) which states the ideal characteristics of an NNUE dataset. For instance, it emphasizes the proportions of evaluation scores between and outside the range of -100 to 100.

This class is also used to calculate the optimum WDL_SCALE as well as the scaled evaluation scores using the provided data file.

### trainer.py

This is the main script that is used to train the NNUE model. The following is the usage guide:

```text
usage: trainer.py [-h] [-o OUT_FILENAME] [-d | --data_optimize | --no-data_optimize] [-e EPOCH] [-p PATIENCE] [-f | --feature_factorize | --no-feature_factorize]
                  [-c CHECKPOINT]
                  in_filename

positional arguments:
  in_filename           Path to input dataset csv with columns fen, wdl (absolute), and eval (relative), optionally with the feature columns (leads to faster loading)

options:
  -h, --help            show this help message and exit
  -o OUT_FILENAME, --out_filename OUT_FILENAME
                        Path to save processed dataset with the feature columns (default: None)
  -d, --data_optimize, --no-data_optimize
                        Whether to optimize the dataset for potentially faster and better training (default: False)
  -e EPOCH, --epoch EPOCH
                        Epoch number to stop training on. May stop training earlier if patience > 0 (default: 50)
  -p PATIENCE, --patience PATIENCE
                        Number of consecutive epochs without improvement to stop training. Will be ignored if 0 (default: 5)
  -f, --feature_factorize, --no-feature_factorize
                        Whether to enable feature factorization. May lead to faster training (default: False)
  -c CHECKPOINT, --checkpoint CHECKPOINT
                        Checkpoint (path to a .pth file) to resume from (default: )
```

The input file should be a `csv` file with the rows `fen`, `wdl` (absolute, with 1.0 as white win), and `eval` (relative centipawns from the side to move). The data loader will call `NNUEParams.get_features()` to populate the `side`, `widx`, and `bidx` columns. If the input file already contains these columns, this step will be skipped.

If the `-d` flag is provided, the input dataset will be optimizied by removing outliers and ensuring a balanced dataset.

If an output path is provided, it will save this dataset with columns `fen`, `wdl`, `eval`, `side`, `widx`, and `bidx` to the path.

The other command line arguments are pretty self explanatory if you know how training a neural network goes. Lastly, the `-f` flag is used to enable feature factorization, which is explained in a lot more detail in [this document](https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#feature-factorization) by the Official Stockfish.

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
usage: nnuetest.py [-h] [-f | --feature_factorize | --no-feature_factorize] [-s DATASIZE] [path]

positional arguments:
  path                  Path to trained pth file. (default: best.pth from the lastest trainining session)

options:
  -h, --help            show this help message and exit
  -f, --feature_factorize, --no-feature_factorize
                        Whether to the trained model uses feature factorization (default: False)
  -s DATASIZE, --datasize DATASIZE
                        Number of positions to use during testing. Greater values provide more coverage but is also slower
                        (default: 1000)
```

Both programs are used as debugging tools to validate consistency between the C++ and Python NNUE, measure quantization, evaluate errors, and test other behaviors. The two scripts tests different things, some of which are only possible on one or the other.

Note that the script is made specifically for Raphael, and a lot of the variables are hardcoded as it is not intended to be used outside of debugging purposes during development.

Both programs will print out a list of possible commands upon startup and will both accept a FEN and print out the resulting evaluation.

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

Training Procedure:

1. Gathered EPD files from [Blunder 8.1's Texel Tuning Dataset](https://talkchess.com/viewtopic.php?t=78536&start=20) and [Zurichess](https://www.reddit.com/r/chessprogramming/comments/1if8yx6/chess_dataset_for_tuning_evaluation/) with roughly two million unique positions in total
2. Gathered evaluations using `traingen` with arguments `combined-positions.epd 128 -n 1048576`
3. Used `clean_nnue_traindata.py` (inside /dataset) to remove positions with forced-mate sequences and downsample positions with large evaluations. At this point, the dataset size is roughly 1.3 million positions
4. Trained the NNUE using `trainer.py` with arguments `-i traindata_combined.csv -o traindata_combined_ff.csv -f`

<!--
TODO:
Training Results:

- Epochs:
- Training Loss:
- Test Loss:
- ELO-gain:
- -->