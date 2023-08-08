import chess.pgn

p1 = 0
draw = 0
p2 = 0
p1_is_white = False

with open("../../logs/compare.pgn", "r") as pgn:
    for i in range(400):
        p1_is_white = not p1_is_white
        game = chess.pgn.read_game(pgn)
        moves = game.mainline_moves()
        board = game.board()
        
        for move in moves:
            board.push(move)
        
        outcome = board.outcome()
        if outcome == None:
            draw += 1
            continue
        
        outcome = outcome.winner
        if outcome != None:
            p1 += (outcome == p1_is_white)
            p2 += (outcome == (not p1_is_white))
        else:
            draw += 1
            
print(f"{p1=}\n{draw=}\n{p2=}")