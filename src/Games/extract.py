# modified version of Sebastian Lague's code from https://youtu.be/_vqlIPDR2TU
# extacts game positions to start with for engine performance comparison
import chess.pgn
from random import random


N_GAMES = 500

with open("randomGames.txt", "w") as out:
    with open("lichess_db_standard_rated_2015-07.pgn") as pgn:
        n_saved = 0
        while (n_saved < N_GAMES):
            game = chess.pgn.read_game(pgn)
            moves = game.mainline_moves()
            board = game.board()
            
            # anywhere from 16 to 36 moves in
            plyToPlay = int(16 + 20*random())
            plyPlayed = 0
            fen = ""
            
            for move in moves:
                board.push(move)
                plyPlayed += 1
                if plyPlayed == plyToPlay:
                    fen = board.fen()
            
            if fen!="":
                # more than 20 moves to play and more than 10 pieces
                if plyPlayed > plyToPlay + 40 and sum([fen.lower().count(piece) for piece in "rnbq"]) >= 10:
                    n_saved += 1
                    out.write(fen + "\n")