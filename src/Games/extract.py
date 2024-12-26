# modified version of Sebastian Lague's code from https://youtu.be/_vqlIPDR2TU
# extacts game positions to start with for engine performance comparison
from random import random

import chess.engine
import chess.pgn

OUTFILE = "randomGamesBig.txt"
# N_GAMES = 0  # number of games to store
# MAX_ADV = 0  # maximum advantage (centipawn)


if OUTFILE == "randomGames.txt":
    N_GAMES = 400
    MAX_ADV = 200
elif OUTFILE == "randomGamesBig.txt":
    N_GAMES = 120_000
    MAX_ADV = 500

# download stockfish from https://stockfishchess.org/download/ and add it to the current directory
engine = chess.engine.SimpleEngine.popen_uci(
    "./stockfish/stockfish-windows-x86-64-avx2.exe"
)


with open(OUTFILE, "a") as out:
    with open("lichess_db_standard_rated_2015-07.pgn") as pgn:
        n_saved = 0
        avg_eval = 0

        while n_saved < N_GAMES:
            if (game := chess.pgn.read_game(pgn)) is None:
                break
            moves = game.mainline_moves()
            board = game.board()

            # anywhere from 16 to 36 moves in
            plyToPlay = int(16 + 20 * random())
            plyPlayed = 0
            store = False

            for move in moves:
                board.push(move)
                plyPlayed += 1
                if plyPlayed == plyToPlay:
                    fen = board.fen()
                    # must be >= 10 important pieces
                    if sum([fen.lower().count(piece) for piece in "rnbq"]) < 10:
                        break
                    eval = engine.analyse(board, chess.engine.Limit(time=0.1))["score"]
                    # must be <= MAX_ADV point advantage
                    if eval.is_mate() or abs(eval.white().score()) > MAX_ADV:
                        break
                    store = True

            # continued for 20 more turns
            if store and plyPlayed > plyToPlay + 40:
                n_saved += 1
                avg_eval += eval.white().score()
                progress = int(50 * n_saved / N_GAMES)
                print(
                    "Progress: ["
                    + "█" * progress
                    + "." * (50 - progress)
                    + f"] ({n_saved}/{N_GAMES})",
                    end="\r",
                )
                out.write(fen + "\n")

print(f"\nAverage eval: {avg_eval/N_GAMES/100}")
engine.close()
