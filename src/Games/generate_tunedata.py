# extracts the positions and outcomes from a pgn file for tuning the eval weights
import random
import time

import chess.pgn

EXCLUDE_COUNT = 15  # last EXCLUDE_COUNT moves to exclude

print("Processing pgn...")
epnlines = []
with open("../../logs/results.pgn", "r") as pgn:
    while True:
        if (game := chess.pgn.read_game(pgn)) is None:
            break

        moves = list(game.mainline_moves())
        board = game.board()
        outcome = game.headers["Result"]

        for move in moves[:-EXCLUDE_COUNT]:
            fen = board.fen()
            epnlines.append(fen + "; " + outcome + "\n")
            board.push(move)

print("Shuffling...")
random.shuffle(epnlines)

outfile = time.strftime("%Y-%m-%d.epd")
print(f"Writing to {outfile}")
with open(f"tunedata_{outfile}", "w") as outf:
    outf.writelines(epnlines)
