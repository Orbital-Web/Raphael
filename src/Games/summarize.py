import chess.pgn

# [p2, p1]
results = {
    "white wins": [0, 0],
    "black wins": [0, 0],
    "timeout wins": [0, 0],
    "stalemate draws": 0,
    "repetition draws": 0,
    "other draws": 0,
}
p1_is_white = False

with open("../../logs/compare.pgn", "r") as pgn:
    while True:
        p1_is_white = not p1_is_white
        if (game := chess.pgn.read_game(pgn)) is None:
            break
        moves = game.mainline_moves()
        board = game.board()

        for move in moves:
            board.push(move)

        outcome = board.outcome(claim_draw=True)
        if outcome is None:  # no outcome, must be timeout (player on turn loses)
            results["timeout wins"][board.turn != p1_is_white] += 1
            continue

        if outcome.winner is not None:
            if outcome.winner:  # white won, raise score of white player
                results["white wins"][p1_is_white] += 1
            else:  # black won, raise score of black player
                results["black wins"][~p1_is_white] += 1
        else:
            if outcome.termination == chess.Termination.THREEFOLD_REPETITION:
                results["repetition draws"] += 1
            elif outcome.termination == chess.Termination.STALEMATE:
                results["stalemate draws"] += 1
            else:
                results["other draws"] += 1

p1_wins = (
    results["white wins"][1] + results["black wins"][1] + results["timeout wins"][1]
)
p2_wins = (
    results["white wins"][0] + results["black wins"][0] + results["timeout wins"][0]
)
draws = (
    results["stalemate draws"] + results["repetition draws"] + results["other draws"]
)

print(
    f"p1: {p1_wins} [white: {results['white wins'][1]}, black: {results['black wins'][1]}, timeout: {results['timeout wins'][1]}]"
)
print(
    f"p2: {p2_wins} [white: {results['white wins'][0]}, black: {results['black wins'][0]}, timeout: {results['timeout wins'][0]}]"
)
print(
    f"Dr: {draws} [rep: {results['repetition draws']}, stale: {results['stalemate draws']}, other: {results['other draws']}]"
)
